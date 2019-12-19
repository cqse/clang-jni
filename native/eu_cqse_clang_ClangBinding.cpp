#include "eu_cqse_clang_ClangBinding.h"
#include "Index.h"
#include <cstdlib>
#include <cstring>
#include <mutex>

#include "../../../clang/include/clang/Tooling/CompilationDatabase.h"
#include "../../../clang-tools-extra/clang-tidy/ClangTidy.h"

// general hint: see https://stackoverflow.com/a/6245964 for local references and freeing them

// Parameters with environment info passed into the visit method
struct ClangBindingVisitorParameter {
    jmethodID method;
    JNIEnv * env;
    jobject java_visitor;
};


#define CLANG_JNI_BEGIN_EXCEPTION_HANDLER try { jni_helper::initializeJniClasses()

#define CLANG_JNI_END_EXCEPTION_HANDLER(NAME) \
  } catch (const std::exception &e) { \
    std::string message = "Exception in clang-tidy integration method " NAME ": "; \
    message += e.what(); \
    env->ThrowNew (jni_helper::runtimeExceptionClass, message.c_str()); \
    return 0; \
  } catch (...) { \
    env->ThrowNew (jni_helper::runtimeExceptionClass, "Unknown low level exception in clang-tidy integration method " NAME "!"); \
    return 0; \
  }
#define CLANG_JNI_END_EXCEPTION_HANDLER_NO_RETURN(NAME) \
  } catch (const std::exception &e) { \
    std::string message = "Exception in clang-tidy integration method " NAME ": "; \
    message += e.what(); \
    env->ThrowNew (jni_helper::runtimeExceptionClass, message.c_str()); \
    return; \
  } catch (...) { \
    env->ThrowNew (jni_helper::runtimeExceptionClass, "Unknown low level exception in clang-tidy integration method " NAME "!"); \
    return; \
  }

namespace jni_helper{

    mutex initializationMutex;
    bool isInitialized = false;

    // classes and methods that we use/need all over the place
    jclass runtimeExceptionClass;
    jmethodID runtimeExceptionConstructor

    jclass clangSpellingLocationPropertiesClass;
    jmethodID clangSpellingLocationPropertiesConstructor;

    jclass arrayListClass;
    jmethodID arrayListConstructor;
    jmethodID arrayListAdd;

    jclass hashMapClass;
    jmethodID hashMapConstructor;
    jmethodID hashMapPut;

    jclass listClass;
    jmethodID listAdd;
    jmethodID listSize;
    jmethodID listGet;

    jclass clangTidyFileClass;
    jmethodID clangTidyFileGetPath;
    jmethodID clangTidyFileGetContent;

    jclass clangTidyErrorClass;
    jmethodID clangTidyErrorConstructor;
    
    void initializeJniClasses (JNIEnv *env) {
        if (isInitialized) {
            return;
        }

        // following region is under lock until the lock is released,
        // which automatically happens at method exit
        std::unique_lock<mutex> lock(initializationMutex);
        
        if (isInitialized) {
            return;
        }

        runtimeExceptionClass = env->FindClass("java/lang/RuntimeException");
        HANDLE_JNI_NULL_RESULT(runtimeExceptionClass);
        runtimeExceptionConstructor = env->GetMethodID(runtimeExceptionClass, "<init>", "(Ljava/lang/Throwable;)V");
        HANDLE_JNI_NULL_RESULT(runtimeExceptionConstructor);

        clangSpellingLocationPropertiesClass = env->FindClass("eu/cqse/clang/ClangSpellingLocationProperties");
        HANDLE_JNI_NULL_RESULT(clangSpellingLocationPropertiesClass);
        clangSpellingLocationPropertiesConstructor =
            env->GetMethodID(clangSpellingLocationPropertiesClass, "<init>", "(Ljava/lang/String;III)V");
        HANDLE_JNI_NULL_RESULT(clangSpellingLocationPropertiesConstructor);

        arrayListClass = env->FindClass("java/util/ArrayList");
        HANDLE_JNI_NULL_RESULT(arrayListClass);
        arrayListConstructor = env->GetMethodID(arrayListClass, "<init>", "()V");
        HANDLE_JNI_NULL_RESULT(arrayListConstructor);
        arrayListAdd = env->GetMethodID(arrayListClass, "add", "(Ljava/lang/Object;)Z");
        HANDLE_JNI_NULL_RESULT(arrayListAdd);

        hashMapClass = env->FindClass("java/util/HashMap");
        HANDLE_JNI_NULL_RESULT(hashMapClass);
        hashMapConstructor = env->GetMethodID(hashMapClass, "<init>", "()V");
        HANDLE_JNI_NULL_RESULT(hashMapConstructor);
        hashMapPut = env->GetMethodID(hashMapClass, "put",
				     "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
        HANDLE_JNI_NULL_RESULT(hashMapPut);

        listClass = env->FindClass("java/util/List");
        HANDLE_JNI_NULL_RESULT(listClass);
        listAdd = env->GetMethodID(listClass, "add", "(Ljava/lang/Object;)Z");
        HANDLE_JNI_NULL_RESULT(listAdd);
        listSize = env->GetMethodID(listClass, "size", "()I");
        HANDLE_JNI_NULL_RESULT(listSize);
        listGet = env->GetMethodID(listClass, "get", "(I)Ljava/lang/Object;");
        HANDLE_JNI_NULL_RESULT(listGet);

        clangTidyFileClass = env->FindClass("eu/cqse/clang/ClangTidyFile");
        HANDLE_JNI_NULL_RESULT(clangTidyFileClass);
        clangTidyFileGetPath = env->GetMethodID(clangTidyFileClass, "getPath", "()Ljava/lang/String;");
        HANDLE_JNI_NULL_RESULT(clangTidyFileGetPath);
        clangTidyFileGetContent = env->GetMethodID(clangTidyFileClass, "getContent", "()Ljava/lang/String;");
        HANDLE_JNI_NULL_RESULT(clangTidyFileGetContent);

        clangTidyErrorClass = env->FindClass("eu/cqse/clang/ClangTidyError");
        HANDLE_JNI_NULL_RESULT(clangTidyErrorClass);
        clangTidyErrorConstructor = env->GetMethodID
            (clangTidyErrorClass, "<init>", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;I)V");
        HANDLE_JNI_NULL_RESULT(clangTidyErrorConstructor);
    
        isInitialized = true;
    }
    
    void handlePossibleJniException(JNIEnv *env) {
        jthrowable exc = env->ExceptionOccurred();
        if (exc) {
            env->ExceptionClear();
            env->Throw(env->NewObject (runtimeExceptionClass, runtimeExceptionConstructor, exc));
        }
    }

#define HANDLE_JNI_NULL_RESULT(X) if(!X) handlePossibleJniException(env)    
    
    std::string obtainString (JNIEnv *env, jstring s) {
        const char *temp = env->GetStringUTFChars(s, 0);
        HANDLE_JNI_NULL_RESULT(temp);
        std::string result (temp);
        env->ReleaseStringUTFChars(s, temp);
        return result;
    }

}

JNIEXPORT void JNICALL Java_eu_cqse_clang_ClangBinding_visitChildrenImpl
  (JNIEnv * env, jclass cls, jlong cursor_pointer, jobject java_visitor) {

    CLANG_JNI_BEGIN_EXCEPTION_HANDLER;
  
    CXCursor root_cursor = **(CXCursor **)&cursor_pointer;
    
    ClangBindingVisitorParameter parameter;
    parameter.env = env;
    parameter.method = env->GetMethodID(env->GetObjectClass(java_visitor), "visit", "(JJ)I");
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
                            jni_helper::handlePossibleJniException(param->env);

                            return (CXChildVisitResult) result;
                        }, &parameter);

    CLANG_JNI_END_EXCEPTION_HANDLER_NO_RETURN("visitChildrenImpl")
}

JNIEXPORT jobject JNICALL Java_eu_cqse_clang_ClangBinding_getSpellingLocationPropertiesImpl
  (JNIEnv *env, jclass cls, jlong location_ptr) {

    CLANG_JNI_BEGIN_EXCEPTION_HANDLER;

    CXSourceLocation location = **(CXSourceLocation **)&location_ptr;

    CXFile file;
    unsigned line, column, offset;
    clang_getSpellingLocation(location, &file, &line, &column, &offset);
    
    CXString fileName = clang_getFileName (file);
    jstring javaFileName = env->NewStringUTF(clang_getCString(fileName));
    HANDLE_JNI_NULL_RESULT(javaFileName);
    clang_disposeString (fileName);

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
    
    CXString fileName = clang_getFileName (file);
    jstring javaFileName = env->NewStringUTF(clang_getCString(fileName));
    HANDLE_JNI_NULL_RESULT(javaFileName);
    clang_disposeString (fileName);

    jobject result = env->NewObject (jni_helper::clangSpellingLocationPropertiesClass,
                                     jni_helper::clangSpellingLocationPropertiesConstructor,
                                     javaFileName, line, column, offset);
    HANDLE_JNI_NULL_RESULT(result);
    return result;
    CLANG_JNI_END_EXCEPTION_HANDLER("getExpansionLocationPropertiesImpl")
}

JNIEXPORT jobject JNICALL Java_eu_cqse_clang_ClangBinding_getAllClangTidyChecks
  (JNIEnv *env, jclass cls) {

    CLANG_JNI_BEGIN_EXCEPTION_HANDLER;

    jobject result = env->NewObject (jni_helper::arrayListClass, jni_helper::arrayListConstructor);
    HANDLE_JNI_NULL_RESULT(result);

    clang::tidy::ClangTidyOptions options = clang::tidy::ClangTidyOptions::getDefaults();
    options.Checks = "*";
    std::vector<std::string> checkNames = clang::tidy::getCheckNames(options, true);

    for (std::vector<std::string>::iterator i = checkNames.begin(), end = checkNames.end(); i != end; ++i) {
      jstring nameString = env->NewStringUTF (i->c_str());
      HANDLE_JNI_NULL_RESULT(nameString);
      env->CallObjectMethod (result, jni_helper::arrayListAdd, nameString);
      jni_helper::handlePossibleJniException(env);
      env->DeleteLocalRef(nameString);
    }
    return result;

    CLANG_JNI_END_EXCEPTION_HANDLER("getAllClangTidyChecks")
}

JNIEXPORT jobject JNICALL Java_eu_cqse_clang_ClangBinding_getAllClangTidyCheckOptions
  (JNIEnv *env, jclass cls) {

    CLANG_JNI_BEGIN_EXCEPTION_HANDLER;

    jobject result = env->NewObject (jni_helper::hashMapClass, jni_helper::hashMapConstructor);
    HANDLE_JNI_NULL_RESULT(result);

    clang::tidy::ClangTidyOptions options = clang::tidy::ClangTidyOptions::getDefaults();
    options.Checks = "*";
    clang::tidy::ClangTidyOptions::OptionMap checkOptions = clang::tidy::getCheckOptions(options, true);

    for (clang::tidy::ClangTidyOptions::OptionMap::iterator i = checkOptions.begin(),
             end = checkOptions.end(); i != end; ++i) {
        jstring key = env->NewStringUTF (i->first.c_str());
        HANDLE_JNI_NULL_RESULT(key);
        jstring value = env->NewStringUTF (i->second.c_str());
        HANDLE_JNI_NULL_RESULT(value);
        
        env->CallObjectMethod (result, jni_helper::hashMapPut, key, value);
        jni_helper::handlePossibleJniException(env);
        
        env->DeleteLocalRef(key);
        env->DeleteLocalRef(value);
    }

    return result;

    CLANG_JNI_END_EXCEPTION_HANDLER("getAllClangTidyCheckOptions")
}

JNIEXPORT jobject JNICALL Java_eu_cqse_clang_ClangBinding_runClangTidyInternal
(JNIEnv *env, jclass cls, jobject files, jstring rules, jobject compilerSwitches,
 jobject checkOptionsKeys, jobject checkOptionsValues, jboolean codeIsCpp) {

    CLANG_JNI_BEGIN_EXCEPTION_HANDLER;
        
    jobject result = env->NewObject (jni_helper::arrayListClass, jni_helper::arrayListConstructor);
    HANDLE_JNI_NULL_RESULT(result);

    clang::tidy::ClangTidyGlobalOptions globalOptions;
    clang::tidy::ClangTidyOptions options = clang::tidy::ClangTidyOptions::getDefaults();
    options.Checks = jni_helper::obtainString(env, rules);

    if (codeIsCpp) {
      if (!options.ExtraArgsBefore.hasValue()) {
	options.ExtraArgsBefore = std::vector<std::string>();
      }
      options.ExtraArgsBefore.getValue().push_back(std::string("-xc++")); 
    }

    // check options; we assume both options to have same size
    int checkOptionsSize = env->CallIntMethod (checkOptionsKeys, jni_helper::listSize);
    jni_helper::handlePossibleJniException(env);

    for (int i = 0; i < checkOptionsSize; ++i) {
      jobject keyObject = env->CallObjectMethod (checkOptionsKeys, jni_helper::listGet, i);
      HANDLE_JNI_NULL_RESULT(keyObject);

      jobject valueObject = env->CallObjectMethod (checkOptionsValues, jni_helper::listGet, i);
      HANDLE_JNI_NULL_RESULT(valueObject);

      options.CheckOptions.insert ({ jni_helper::obtainString(env, (jstring)keyObject),
                  jni_helper::obtainString(env, (jstring)valueObject)});
      
      env->DeleteLocalRef(keyObject);
      env->DeleteLocalRef(valueObject);
    }
    
    auto optionsProvider = llvm::make_unique<clang::tidy::DefaultOptionsProvider>(globalOptions, options);
    clang::tidy::ClangTidyContext context (std::move(optionsProvider), true);

    std::vector<const char *> argv;
    std::vector<std::string> argvBuffer;
    argv.push_back("my-compiler");
    argv.push_back("--");
    int compilerSwitchesSize = env->CallIntMethod (compilerSwitches, jni_helper::listSize);
    jni_helper::handlePossibleJniException(env);
    
    for (int i = 0; i < compilerSwitchesSize; ++i) {
        jobject compilerSwitch = env->CallObjectMethod (compilerSwitches, jni_helper::listGet, i);
        HANDLE_JNI_NULL_RESULT(compilerSwitch);
        argvBuffer.push_back(jni_helper::obtainString(env, (jstring)compilerSwitch));
        argv.push_back (argvBuffer.back().c_str());
        env->DeleteLocalRef(compilerSwitch);
    }
    argv.push_back(0);
    int argc = argv.size()-1;
    std::string errorMessage ("Error while parsing command line in ClangBinding"); 
    auto compilations = clang::tooling::FixedCompilationDatabase::loadFromCommandLine
      (argc, &argv[0], errorMessage); 

    std::vector<std::string> inputFiles;
    std::vector<std::string> contents;
    llvm::IntrusiveRefCntPtr<llvm::vfs::InMemoryFileSystem> inMemoryFS(new llvm::vfs::InMemoryFileSystem());
    time_t modificationTime = std::time(0);
    
    int filesSize = env->CallIntMethod (files, jni_helper::listSize);
    jni_helper::handlePossibleJniException(env);
    
    for (int i = 0; i < filesSize; ++i) {
        jobject file = env->CallObjectMethod (files, jni_helper::listGet, i);
        HANDLE_JNI_NULL_RESULT(file);
      
        jobject path = env->CallObjectMethod (file, jni_helper::clangTidyFileGetPath);
        HANDLE_JNI_NULL_RESULT(path);
        
        jobject content = env->CallObjectMethod (file, jni_helper::clangTidyFileGetContent);
        HANDLE_JNI_NULL_RESULT(content);
        
        // we use vectors to simplify memory management and scoping
        inputFiles.push_back (jni_helper::obtainString(env, (jstring)path));
        contents.push_back (jni_helper::obtainString(env, (jstring)content));
        
        auto buffer = llvm::MemoryBuffer::getMemBuffer(contents.back().c_str());
        inMemoryFS->addFile(inputFiles.back(), modificationTime, std::move(buffer));

        env->DeleteLocalRef(path);
        env->DeleteLocalRef(content);
        env->DeleteLocalRef(file);
    }

    llvm::IntrusiveRefCntPtr<llvm::vfs::OverlayFileSystem> overlayFS
        (new llvm::vfs::OverlayFileSystem(llvm::vfs::getRealFileSystem()));
    overlayFS->pushOverlay(inMemoryFS);
    
    std::vector<clang::tidy::ClangTidyError> errors =
        runClangTidy(context, *compilations, inputFiles, overlayFS);
    
    for (std::vector<clang::tidy::ClangTidyError>::iterator i = errors.begin(), end = errors.end(); i != end; ++i) {
        jstring diagnosticName = env->NewStringUTF(i->DiagnosticName.c_str());
        HANDLE_JNI_NULL_RESULT(diagnosticName);
        
        jstring message = env->NewStringUTF(i->Message.Message.c_str());
        HANDLE_JNI_NULL_RESULT(message);
        
        jstring filePath =  env->NewStringUTF(i->Message.FilePath.c_str());
        HANDLE_JNI_NULL_RESULT(filePath);

        jobject javaError = env->NewObject (jni_helper::clangTidyErrorClass, jni_helper::clangTidyErrorConstructor,
                                            diagnosticName, message, filePath, i->Message.FileOffset);
        HANDLE_JNI_NULL_RESULT(javaError);
        
        env->CallObjectMethod (result, jni_helper::listAdd, javaError);
        jni_helper::handlePossibleJniException(env);
        
        env->DeleteLocalRef(javaError);
        env->DeleteLocalRef(diagnosticName);
        env->DeleteLocalRef(message);
        env->DeleteLocalRef(filePath);
    }

    return result;

    CLANG_JNI_END_EXCEPTION_HANDLER("runClangTidyInternal")
}
