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

JNIEXPORT void JNICALL Java_eu_cqse_clang_ClangBinding_visitChildrenImpl
  (JNIEnv * env, jclass cls, jlong cursor_pointer, jobject java_visitor) {

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
                            return (CXChildVisitResult) result;
                        }, &parameter);
}

JNIEXPORT jobject JNICALL Java_eu_cqse_clang_ClangBinding_getSpellingLocationPropertiesImpl
  (JNIEnv *env, jclass cls, jlong location_ptr) {

    CXSourceLocation location = **(CXSourceLocation **)&location_ptr;

    CXFile file;
    unsigned line, column, offset;
    clang_getSpellingLocation(location, &file, &line, &column, &offset);
    
    jclass result_class = env->FindClass("eu/cqse/clang/ClangSpellingLocationProperties");
    jmethodID constructor = env->GetMethodID(result_class, "<init>", "(Ljava/lang/String;III)V");

    CXString fileName = clang_getFileName (file);
    jstring javaFileName = env->NewStringUTF(clang_getCString(fileName));
    clang_disposeString (fileName);

    return env->NewObject (result_class, constructor, javaFileName, line, column, offset);
}

JNIEXPORT jobject JNICALL Java_eu_cqse_clang_ClangBinding_getExpansionLocationPropertiesImpl
  (JNIEnv *env, jclass cls, jlong location_ptr) {

    CXSourceLocation location = **(CXSourceLocation **)&location_ptr;

    CXFile file;
    unsigned line, column, offset;
    clang_getExpansionLocation(location, &file, &line, &column, &offset);
    
    jclass result_class = env->FindClass("eu/cqse/clang/ClangSpellingLocationProperties");
    jmethodID constructor = env->GetMethodID(result_class, "<init>", "(Ljava/lang/String;III)V");

    CXString fileName = clang_getFileName (file);
    jstring javaFileName = env->NewStringUTF(clang_getCString(fileName));
    clang_disposeString (fileName);

    return env->NewObject (result_class, constructor, javaFileName, line, column, offset);
}

JNIEXPORT jobject JNICALL Java_eu_cqse_clang_ClangBinding_getAllClangTidyChecks
  (JNIEnv *env, jclass cls) {

    jclass arrayListClass = env->FindClass("java/util/ArrayList");
    jmethodID constructor = env->GetMethodID(arrayListClass, "<init>", "()V");
    jobject result = env->NewObject (arrayListClass, constructor);

    clang::tidy::ClangTidyOptions options = clang::tidy::ClangTidyOptions::getDefaults();
    options.Checks = "*";
    std::vector<std::string> checkNames = clang::tidy::getCheckNames(options, true);

    jmethodID add = env->GetMethodID(arrayListClass, "add", "(Ljava/lang/Object;)Z");
    for (std::vector<std::string>::iterator i = checkNames.begin(), end = checkNames.end(); i != end; ++i) {
      env->CallObjectMethod (result, add, env->NewStringUTF (i->c_str()));
    }

    return result;
}

JNIEXPORT jobject JNICALL Java_eu_cqse_clang_ClangBinding_getAllClangTidyCheckOptions
  (JNIEnv *env, jclass cls) {

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
      env->CallObjectMethod (result, put, env->NewStringUTF (i->first.c_str()),
			     env->NewStringUTF (i->second.c_str()));
    }

    return result;
}


namespace jni_helper{

  std::string obtainString (JNIEnv *env, jstring s) {
      const char *temp = env->GetStringUTFChars(s, 0);
      std::string result (temp);
      env->ReleaseStringUTFChars(s, temp);
      return result;
  }

  
}

JNIEXPORT jobject JNICALL Java_eu_cqse_clang_ClangBinding_runClangTidyInternal
(JNIEnv *env, jclass cls, jobject files, jstring rules, jobject compilerSwitches,
 jobject checkOptionsKeys, jobject checkOptionsValues, jboolean codeIsCpp) {

    jclass listClass = env->FindClass("java/util/List");
    jmethodID add = env->GetMethodID(listClass, "add", "(Ljava/lang/Object;)Z");
    jmethodID size = env->GetMethodID(listClass, "size", "()I");
    jmethodID get = env->GetMethodID(listClass, "get", "(I)Ljava/lang/Object;");

    jclass arrayListClass = env->FindClass("java/util/ArrayList");
    jmethodID constructor = env->GetMethodID(arrayListClass, "<init>", "()V");

    jclass fileClass = env->FindClass("eu/cqse/clang/ClangTidyFile");
    jmethodID fileGetPath = env->GetMethodID(fileClass, "getPath", "()Ljava/lang/String;");
    jmethodID fileGetContent = env->GetMethodID(fileClass, "getContent", "()Ljava/lang/String;");

    jclass errorClass = env->FindClass("eu/cqse/clang/ClangTidyError");
    jmethodID errorConstructor = env->GetMethodID
      (errorClass, "<init>", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;I)V");

    jobject result = env->NewObject (arrayListClass, constructor);

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
    for (int i = 0; i < checkOptionsSize; ++i) {
      jobject keyObject = env->CallObjectMethod (checkOptionsKeys, get, i);
      jobject valueObject = env->CallObjectMethod (checkOptionsValues, get, i);

      options.CheckOptions.insert ({ jni_helper::obtainString(env, (jstring)keyObject),
	    jni_helper::obtainString(env, (jstring)valueObject)});
    }
    
    auto optionsProvider = llvm::make_unique<clang::tidy::DefaultOptionsProvider>(globalOptions, options);
    clang::tidy::ClangTidyContext context (std::move(optionsProvider), true);

    std::vector<const char *> argv;
    std::vector<std::string> argvBuffer;
    argv.push_back("my-compiler");
    argv.push_back("--");
    int compilerSwitchesSize = env->CallIntMethod (compilerSwitches, size);
    for (int i = 0; i < compilerSwitchesSize; ++i) {
      jobject compilerSwitch = env->CallObjectMethod (compilerSwitches, get, i);
      argvBuffer.push_back(jni_helper::obtainString(env, (jstring)compilerSwitch));
      argv.push_back (argvBuffer.back().c_str());
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
    for (int i = 0; i < filesSize; ++i) {
      jobject file = env->CallObjectMethod (files, get, i);
      jobject path = env->CallObjectMethod (file, fileGetPath);
      jobject content = env->CallObjectMethod (file, fileGetContent);

      // we use vectors to simplify memory management and scoping
      inputFiles.push_back (jni_helper::obtainString(env, (jstring)path));
      contents.push_back (jni_helper::obtainString(env, (jstring)content));

      auto buffer = llvm::MemoryBuffer::getMemBuffer(contents.back().c_str());
      inMemoryFS->addFile(inputFiles.back(), modificationTime, std::move(buffer)); 
    }

    llvm::IntrusiveRefCntPtr<llvm::vfs::OverlayFileSystem> overlayFS
      (new llvm::vfs::OverlayFileSystem(inMemoryFS));

    std::vector<clang::tidy::ClangTidyError> errors =
      runClangTidy(context, *compilations, inputFiles, overlayFS);
    
    for (std::vector<clang::tidy::ClangTidyError>::iterator i = errors.begin(),
	   end = errors.end(); i != end; ++i) {
      jobject javaError = env->NewObject (errorClass, errorConstructor,
					  env->NewStringUTF(i->DiagnosticName.c_str()),
					  env->NewStringUTF(i->Message.Message.c_str()),
					  env->NewStringUTF(i->Message.FilePath.c_str()),
					  i->Message.FileOffset);
      env->CallObjectMethod (result, add, javaError);
    }

    return result;
}
