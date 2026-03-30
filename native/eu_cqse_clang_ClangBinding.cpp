#include "eu_cqse_clang_ClangBinding.h"
#include "Index.h"
#include <cstdlib>
#include <cstring>
#include <mutex>

// general hint: see https://stackoverflow.com/a/6245964 for local references and freeing them

// Parameters with environment info passed into the visit method
struct ClangBindingVisitorParameter {
    jmethodID method;
    JNIEnv * env;
    jobject java_visitor;
};


#define CLANG_JNI_BEGIN_EXCEPTION_HANDLER try { jni_helper::initializeJniClasses(env)

#define CLANG_JNI_END_EXCEPTION_HANDLER(NAME) \
  } catch (jni_helper::jni_exception_occurred &e) {   \
    /* nothing to do, as the exception will be re-thrown/continue at the calling Java code */ \
    return 0; \
  } catch (const std::exception &e) { \
    std::string message = "Exception in JNI method" NAME ": "; \
    message += e.what(); \
    env->ThrowNew (jni_helper::runtimeExceptionClass, message.c_str()); \
    return 0; \
  } catch (...) { \
    env->ThrowNew (jni_helper::runtimeExceptionClass, "Unknown low level exception in JNI method" NAME "!"); \
    return 0; \
  }
#define CLANG_JNI_END_EXCEPTION_HANDLER_NO_RETURN(NAME) \
  } catch (jni_helper::jni_exception_occurred &e) {   \
    /* nothing to do, as the exception will be re-thrown/continue at the calling Java code */ \
    return; \
  } catch (const std::exception &e) { \
    std::string message = "Exception in JNI method" NAME ": "; \
    message += e.what(); \
    env->ThrowNew (jni_helper::runtimeExceptionClass, message.c_str()); \
    return; \
  } catch (...) { \
    env->ThrowNew (jni_helper::runtimeExceptionClass, "Unknown low level exception in JNI method" NAME "!"); \
    return; \
  }

namespace jni_helper{

    class jni_exception_occurred {};

    std::mutex initializationMutex;
    bool isInitialized = false;

    // classes and methods that we use/need all over the place
    jclass runtimeExceptionClass;

    jclass clangSpellingLocationPropertiesClass;
    jmethodID clangSpellingLocationPropertiesConstructor;


#define HANDLE_JNI_NULL_RESULT(X) \
  jni_helper::handlePossibleJniException(env); \
  if (!(X)) throw std::runtime_error("Returned null for " #X )

    void handlePossibleJniException(JNIEnv *env) {
        jthrowable exc = env->ExceptionOccurred();
        if (exc) {
            // exits the JNI code and signals that a Java exception is pending
            throw jni_exception_occurred ();
        }
    }

    jclass findGlobalClass(JNIEnv *env, const char *name) {
        jclass theClass = env->FindClass(name);
	HANDLE_JNI_NULL_RESULT(theClass);
	theClass = (jclass) env->NewGlobalRef((jobject) theClass);
	HANDLE_JNI_NULL_RESULT(theClass);
	return theClass;
    }

    jmethodID getMethodId(JNIEnv *env, jclass theClass, const char *name, const char *signature) {
        jmethodID method = env->GetMethodID(theClass, name, signature);
	HANDLE_JNI_NULL_RESULT(method);
	return method;
    }

    void initializeJniClasses (JNIEnv *env) {
        if (isInitialized) {
            return;
        }

        // following region is under lock until the lock is released,
        // which automatically happens at method exit
        std::unique_lock<std::mutex> lock(initializationMutex);
        
        if (isInitialized) {
            return;
        }

        runtimeExceptionClass = findGlobalClass(env, "java/lang/RuntimeException");

        clangSpellingLocationPropertiesClass = findGlobalClass(env, "eu/cqse/clang/ClangSpellingLocationProperties");
        clangSpellingLocationPropertiesConstructor =
	    getMethodId(env, clangSpellingLocationPropertiesClass, "<init>", "(Ljava/lang/String;III)V");


        isInitialized = true;
    }
    
}

// IMPORTANT: No C++ exceptions in the visitor callback.
//
// This callback is passed to clang_visitChildren, which is part of libclang.
// All of LLVM/clang is compiled with -fno-exceptions: functions have no
// landing pads, so destructors for local objects are skipped when a C++
// exception unwinds through them. This can corrupt clang's internal state
// (leaked locks, broken refcounts, inconsistent data structures), causing
// segfaults — potentially on other threads and much later.
//
// On error, set a pending Java exception via JNI and return
// CXChildVisit_Break to stop traversal cleanly. The pending exception
// propagates to Java after clang_visitChildren returns.
//
// This rule applies to ALL code reachable from the callback — not just the
// callback body itself. Any function called from here (directly or
// indirectly) must also avoid throwing C++ exceptions.
JNIEXPORT void JNICALL Java_eu_cqse_clang_ClangBinding_visitChildrenImpl
  (JNIEnv * env, jclass cls, jlong cursor_pointer, jobject java_visitor) {

    CLANG_JNI_BEGIN_EXCEPTION_HANDLER;

    CXCursor root_cursor = **(CXCursor **)&cursor_pointer;

    ClangBindingVisitorParameter parameter;
    parameter.env = env;
    parameter.method = jni_helper::getMethodId(env, env->GetObjectClass(java_visitor), "visit", "(JJ)I");
    parameter.java_visitor = java_visitor;

    clang_visitChildren(root_cursor,
                        [](CXCursor cursor, CXCursor parent, CXClientData client_data) {
                            jlong cursor_copy = 0;
                            {
				CXCursor * cursorptr = new CXCursor();
				memmove(cursorptr, &cursor, sizeof(CXCursor));
				*(CXCursor **)&cursor_copy = cursorptr;
                            }

                            jlong parent_copy = 0;
                            {
				CXCursor * parentptr = new CXCursor();
				memmove(parentptr, &parent, sizeof(CXCursor));
				*(CXCursor **)&parent_copy = parentptr;
                            }

                            ClangBindingVisitorParameter* param = (ClangBindingVisitorParameter*)client_data;
                            jint result = param->env->CallIntMethod(param->java_visitor, param->method, cursor_copy, parent_copy);

                            if (param->env->ExceptionCheck()) {
                                return CXChildVisit_Break;
                            }

                            return (CXChildVisitResult) result;
                        }, &parameter);

    // Now that we're back from C code, check for pending Java exceptions
    jni_helper::handlePossibleJniException(env);

    CLANG_JNI_END_EXCEPTION_HANDLER_NO_RETURN("visitChildrenImpl")
}

JNIEXPORT jobject JNICALL Java_eu_cqse_clang_ClangBinding_getSpellingLocationPropertiesImpl
  (JNIEnv *env, jclass cls, jlong location_ptr) {

    CLANG_JNI_BEGIN_EXCEPTION_HANDLER;

    CXSourceLocation location = **(CXSourceLocation **)&location_ptr;

    CXFile file;
    unsigned line, column, offset;
    clang_getSpellingLocation(location, &file, &line, &column, &offset);

    // file can be NULL for built-in types and compiler-synthesized locations
    jstring javaFileName;
    if (file) {
        CXString fileName = clang_getFileName(file);
        javaFileName = env->NewStringUTF(clang_getCString(fileName));
        clang_disposeString(fileName);
    } else {
        javaFileName = env->NewStringUTF("");
    }
    HANDLE_JNI_NULL_RESULT(javaFileName);

    jobject result = env->NewObject (jni_helper::clangSpellingLocationPropertiesClass,
                                     jni_helper::clangSpellingLocationPropertiesConstructor,
                                     javaFileName, line, column, offset);
    HANDLE_JNI_NULL_RESULT(result);
    return result;
    CLANG_JNI_END_EXCEPTION_HANDLER("getSpellingLocationPropertiesImpl")
}

JNIEXPORT jobject JNICALL Java_eu_cqse_clang_ClangBinding_getExpansionLocationPropertiesImpl
  (JNIEnv *env, jclass cls, jlong location_ptr) {

    CLANG_JNI_BEGIN_EXCEPTION_HANDLER;

    CXSourceLocation location = **(CXSourceLocation **)&location_ptr;

    CXFile file;
    unsigned line, column, offset;
    clang_getExpansionLocation(location, &file, &line, &column, &offset);

    // file can be NULL for built-in types and compiler-synthesized locations
    jstring javaFileName;
    if (file) {
        CXString fileName = clang_getFileName(file);
        javaFileName = env->NewStringUTF(clang_getCString(fileName));
        clang_disposeString(fileName);
    } else {
        javaFileName = env->NewStringUTF("");
    }
    HANDLE_JNI_NULL_RESULT(javaFileName);

    jobject result = env->NewObject (jni_helper::clangSpellingLocationPropertiesClass,
                                     jni_helper::clangSpellingLocationPropertiesConstructor,
                                     javaFileName, line, column, offset);
    HANDLE_JNI_NULL_RESULT(result);
    return result;
    CLANG_JNI_END_EXCEPTION_HANDLER("getExpansionLocationPropertiesImpl")
}
