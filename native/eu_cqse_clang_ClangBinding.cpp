#include "eu_cqse_clang_ClangBinding.h"
#include "Index.h"
#include <cstdlib>
#include <cstring>

#include "../../../clang/include/clang/Tooling/CompilationDatabase.h"
#include "../../../clang-tools-extra/clang-tidy/ClangTidy.h"

// Parameters with environment info passed into the visit method
struct ClangBindingVisitorParameter {
    jmethodID method;
    JNIEnv * env;
    jobject java_visitor;
};


#define CLANG_JNI_BEGIN_EXCEPTION_HANDLER try {
#define CLANG_JNI_END_EXCEPTION_HANDLER(NAME) \
  } catch (const std::exception &e) { \
    jclass exceptionClass = env->FindClass("java/lang/RuntimeException"); \
    std::string message = "Exception in clang-tidy integration method " NAME ": "; \
    message += e.what(); \
    env->ThrowNew (exceptionClass, message.c_str()); \
    return 0; \
  } catch (...) { \
    jclass exceptionClass = env->FindClass("java/lang/RuntimeException"); \
    env->ThrowNew (exceptionClass, "Unknown low level exception in clang-tidy integration method " NAME "!"); \
    return 0; \
  }
#define CLANG_JNI_END_EXCEPTION_HANDLER_NO_RETURN(NAME) \
  } catch (const std::exception &e) { \
    jclass exceptionClass = env->FindClass("java/lang/RuntimeException"); \
    std::string message = "Exception in clang-tidy integration method " NAME ": "; \
    message += e.what(); \
    env->ThrowNew (exceptionClass, message.c_str()); \
    return; \
  } catch (...) { \
    jclass exceptionClass = env->FindClass("java/lang/RuntimeException"); \
    env->ThrowNew (exceptionClass, "Unknown low level exception in clang-tidy integration method " NAME "!"); \
    return; \
  }

namespace jni_helper{

  std::string obtainString (JNIEnv *env, jstring s) {
      const char *temp = env->GetStringUTFChars(s, 0);
      std::string result (temp);
      env->ReleaseStringUTFChars(s, temp);
      return result;
  }

  void handlePossibleJniException(JNIEnv *env) {
      jthrowable exc = env->ExceptionOccurred();
      if (exc) {
          env->ExceptionDescribe();
          env->ExceptionClear();

          /* We don't do much with the exception, except that we print a
             debug message using ExceptionDescribe, clear it, and throw
             a new exception. */
          jclass newExcCls = env->FindClass("java/lang/IllegalArgumentException");
          if (newExcCls == 0) {
              // Unable to find the new exception class, give up.
              return;
          }
          env->ThrowNew(newExcCls, "Caught illegal argument exception when calling java code.");
          env->DeleteLocalRef(newExcCls);
      }
  }
}

JNIEXPORT void JNICALL Java_eu_cqse_clang_ClangBinding_visitChildrenImpl
  (JNIEnv * env, jclass cls, jlong cursor_pointer, jobject java_visitor) {

    CLANG_JNI_BEGIN_EXCEPTION_HANDLER  
  
    CXCursor root_cursor = **(CXCursor **)&cursor_pointer;
    
    ClangBindingVisitorParameter parameter;
    parameter.env = env;
    parameter.method = env->GetMethodID(env->GetObjectClass(java_visitor), "visit", "(JJ)I");
    parameter.java_visitor = java_visitor;
    
    clang_visitChildren(root_cursor,
                        [](CXCursor cursor, CXCursor parent, CXClientData client_data) {
                            jlong cursor_copy = 0;
                            {
				CXCursor * cursorptr = (CXCursor *) malloc(sizeof(CXCursor));
				memmove(cursorptr, &cursor, sizeof(CXCursor));
				*(CXCursor **)&cursor_copy = cursorptr;
                            }
                            
                            jlong parent_copy = 0;
                            {
				CXCursor * parentptr = (CXCursor *) malloc(sizeof(CXCursor));
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

    CLANG_JNI_BEGIN_EXCEPTION_HANDLER  

    CXSourceLocation location = **(CXSourceLocation **)&location_ptr;

    CXFile file;
    unsigned line, column, offset;
    clang_getSpellingLocation(location, &file, &line, &column, &offset);
    
    jclass result_class = env->FindClass("eu/cqse/clang/ClangSpellingLocationProperties");
    jmethodID constructor = env->GetMethodID(result_class, "<init>", "(Ljava/lang/String;III)V");

    CXString fileName = clang_getFileName (file);
    jstring javaFileName = env->NewStringUTF(clang_getCString(fileName));
    clang_disposeString (fileName);

    jobject result = env->NewObject (result_class, constructor, javaFileName, line, column, offset);
    env->DeleteLocalRef(result_class);
    return result;
    CLANG_JNI_END_EXCEPTION_HANDLER("getSpellingLocationPropertiesImpl")
}

JNIEXPORT jobject JNICALL Java_eu_cqse_clang_ClangBinding_getExpansionLocationPropertiesImpl
  (JNIEnv *env, jclass cls, jlong location_ptr) {

    CLANG_JNI_BEGIN_EXCEPTION_HANDLER  

    CXSourceLocation location = **(CXSourceLocation **)&location_ptr;

    CXFile file;
    unsigned line, column, offset;
    clang_getExpansionLocation(location, &file, &line, &column, &offset);
    
    jclass result_class = env->FindClass("eu/cqse/clang/ClangSpellingLocationProperties");
    jmethodID constructor = env->GetMethodID(result_class, "<init>", "(Ljava/lang/String;III)V");

    CXString fileName = clang_getFileName (file);
    jstring javaFileName = env->NewStringUTF(clang_getCString(fileName));
    clang_disposeString (fileName);

    jobject result = env->NewObject (result_class, constructor, javaFileName, line, column, offset);

    env->DeleteLocalRef(result_class);
    return result;
    CLANG_JNI_END_EXCEPTION_HANDLER("getExpansionLocationPropertiesImpl")
}

JNIEXPORT jobject JNICALL Java_eu_cqse_clang_ClangBinding_getAllClangTidyChecks
  (JNIEnv *env, jclass cls) {

    CLANG_JNI_BEGIN_EXCEPTION_HANDLER  

    jclass arrayListClass = env->FindClass("java/util/ArrayList");
    jmethodID constructor = env->GetMethodID(arrayListClass, "<init>", "()V");
    jobject result = env->NewObject (arrayListClass, constructor);

    clang::tidy::ClangTidyOptions options = clang::tidy::ClangTidyOptions::getDefaults();
    options.Checks = "*";
    std::vector<std::string> checkNames = clang::tidy::getCheckNames(options, true);

    jmethodID add = env->GetMethodID(arrayListClass, "add", "(Ljava/lang/Object;)Z");
    for (std::vector<std::string>::iterator i = checkNames.begin(), end = checkNames.end(); i != end; ++i) {
      jstring nameString = env->NewStringUTF (i->c_str());
      env->CallObjectMethod (result, add, nameString);
      jni_helper::handlePossibleJniException(env);
      env->DeleteLocalRef(nameString);
    }

    env->DeleteLocalRef(arrayListClass);
    return result;

    CLANG_JNI_END_EXCEPTION_HANDLER("getAllClangTidyChecks")
}

JNIEXPORT jobject JNICALL Java_eu_cqse_clang_ClangBinding_getAllClangTidyCheckOptions
  (JNIEnv *env, jclass cls) {

    CLANG_JNI_BEGIN_EXCEPTION_HANDLER  

    jclass hashMapClass = env->FindClass("java/util/HashMap");
    jmethodID constructor = env->GetMethodID(hashMapClass, "<init>", "()V");
    jobject result = env->NewObject (hashMapClass, constructor);

    clang::tidy::ClangTidyOptions options = clang::tidy::ClangTidyOptions::getDefaults();
    options.Checks = "*";
    clang::tidy::ClangTidyOptions::OptionMap checkOptions = clang::tidy::getCheckOptions(options, true);

    jmethodID put = env->GetMethodID(hashMapClass, "put",
				     "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    for (clang::tidy::ClangTidyOptions::OptionMap::iterator i = checkOptions.begin(),
	   end = checkOptions.end(); i != end; ++i) {
	   jstring key = env->NewStringUTF (i->first.c_str());
	   jstring value = env->NewStringUTF (i->second.c_str());
      env->CallObjectMethod (result, put, key, value);
	  jni_helper::handlePossibleJniException(env);
	  env->DeleteLocalRef(key);
	  env->DeleteLocalRef(value);
      }

    env->DeleteLocalRef(hashMapClass);
    return result;

    CLANG_JNI_END_EXCEPTION_HANDLER("getAllClangTidyCheckOptions")
}




JNIEXPORT jobject JNICALL Java_eu_cqse_clang_ClangBinding_runClangTidyInternal
(JNIEnv *env, jclass cls, jobject files, jstring rules, jobject compilerSwitches,
 jobject checkOptionsKeys, jobject checkOptionsValues, jboolean codeIsCpp) {

    CLANG_JNI_BEGIN_EXCEPTION_HANDLER
    
    jclass listClass = env->FindClass("java/util/List");
    jmethodID add = env->GetMethodID(listClass, "add", "(Ljava/lang/Object;)Z");
    jmethodID size = env->GetMethodID(listClass, "size", "()I");
    jmethodID get = env->GetMethodID(listClass, "get", "(I)Ljava/lang/Object;");
    env->DeleteLocalRef(listClass);

    jclass arrayListClass = env->FindClass("java/util/ArrayList");
    jmethodID constructor = env->GetMethodID(arrayListClass, "<init>", "()V");

    jclass fileClass = env->FindClass("eu/cqse/clang/ClangTidyFile");
    jmethodID fileGetPath = env->GetMethodID(fileClass, "getPath", "()Ljava/lang/String;");
    jmethodID fileGetContent = env->GetMethodID(fileClass, "getContent", "()Ljava/lang/String;");
    env->DeleteLocalRef(fileClass);

    jclass errorClass = env->FindClass("eu/cqse/clang/ClangTidyError");
    jmethodID errorConstructor = env->GetMethodID
      (errorClass, "<init>", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;I)V");

    jobject result = env->NewObject (arrayListClass, constructor);
    env->DeleteLocalRef(arrayListClass);

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
    int checkOptionsSize = env->CallIntMethod (checkOptionsKeys, size);
    jni_helper::handlePossibleJniException(env);

    for (int i = 0; i < checkOptionsSize; ++i) {
      jobject keyObject = env->CallObjectMethod (checkOptionsKeys, get, i);
      jni_helper::handlePossibleJniException(env);
      jobject valueObject = env->CallObjectMethod (checkOptionsValues, get, i);
      jni_helper::handlePossibleJniException(env);

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
    int compilerSwitchesSize = env->CallIntMethod (compilerSwitches, size);
    jni_helper::handlePossibleJniException(env);
    for (int i = 0; i < compilerSwitchesSize; ++i) {
      jobject compilerSwitch = env->CallObjectMethod (compilerSwitches, get, i);
      jni_helper::handlePossibleJniException(env);
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
    
    int filesSize = env->CallIntMethod (files, size);
    jni_helper::handlePossibleJniException(env);
    for (int i = 0; i < filesSize; ++i) {
      jobject file = env->CallObjectMethod (files, get, i);
      jni_helper::handlePossibleJniException(env);
      jobject path = env->CallObjectMethod (file, fileGetPath);
      jni_helper::handlePossibleJniException(env);
      jobject content = env->CallObjectMethod (file, fileGetContent);
      jni_helper::handlePossibleJniException(env);

      // we use vectors to simplify memory management and scoping
      inputFiles.push_back (jni_helper::obtainString(env, (jstring)path));
      contents.push_back (jni_helper::obtainString(env, (jstring)content));

      auto buffer = llvm::MemoryBuffer::getMemBuffer(contents.back().c_str());
      inMemoryFS->addFile(inputFiles.back(), modificationTime, std::move(buffer));

      env->DeleteLocalRef(file);
      env->DeleteLocalRef(path);
      env->DeleteLocalRef(content);
    }

    llvm::IntrusiveRefCntPtr<llvm::vfs::OverlayFileSystem> overlayFS
      (new llvm::vfs::OverlayFileSystem(llvm::vfs::getRealFileSystem()));
    overlayFS->pushOverlay(inMemoryFS);
    
    std::vector<clang::tidy::ClangTidyError> errors =
      runClangTidy(context, *compilations, inputFiles, overlayFS);
    
    for (std::vector<clang::tidy::ClangTidyError>::iterator i = errors.begin(), end = errors.end(); i != end; ++i) {
      jstring diagnosticName = env->NewStringUTF(i->DiagnosticName.c_str());
      jstring message = env->NewStringUTF(i->Message.Message.c_str());
      jstring filePath =  env->NewStringUTF(i->Message.FilePath.c_str());
      jobject javaError = env->NewObject (errorClass, errorConstructor, diagnosticName, message, filePath,
                                          i->Message.FileOffset);
      env->CallObjectMethod (result, add, javaError);
      jni_helper::handlePossibleJniException(env);
      env->DeleteLocalRef(diagnosticName);
      env->DeleteLocalRef(message);
      env->DeleteLocalRef(filePath);
      env->DeleteLocalRef(javaError);
    }

    env->DeleteLocalRef(errorClass);
    return result;

    CLANG_JNI_END_EXCEPTION_HANDLER("runClangTidyInternal")
}
