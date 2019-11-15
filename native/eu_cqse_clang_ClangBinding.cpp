#include "eu_cqse_clang_ClangBinding.h"
#include "Index.h"
#include <cstdlib>
#include <cstring>

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

JNIEXPORT jobject JNICALL Java_eu_cqse_clang_ClangBinding_runClangTidy
  (JNIEnv *env, jclass cls, jobject files, jstring rules, jobject parameters) {

    jclass arrayListClass = env->FindClass("java/util/ArrayList");
    jmethodID constructor = env->GetMethodID(arrayListClass, "<init>", "()V");
    jobject result = env->NewObject (arrayListClass, constructor);

    clang::tidy::ClangTidyGlobalOptions globalOptions;
    clang::tidy::ClangTidyOptions options = clang::tidy::ClangTidyOptions::getDefaults();
    options.Checks = "*"; // TODO: convert rules
    // TODO: fill options.CheckOptions;

    std::unique_ptr<clang::tidy::ClangTidyOptionsProvider> optionsProvider
      (new clang::tidy::DefaultOptionsProvider(globalOptions, options));
    clang::tidy::ClangTidyContext context (optionsProvider, true);

    std::unique_ptr<tooling::FixedCompilationDatabase> compilations =
      tooling::FixedCompilationDatabase::loadFromCommandLine(1, { "gcc" }, "My error");

    ArrayRef<std::string> inputFiles; // TODO
    llvm::IntrusiveRefCntPtr<llvm::vfs::OverlayFileSystem> baseFS; // TODO

    std::vector<clang::tidy::ClangTidyError> errors =
      runClangTidy(context, *compilations, inputFiles, baseFS);

    jmethodID add = env->GetMethodID(arrayListClass, "add", "(Ljava/lang/Object;)Z");
    jclass errorClass = env->FindClass("eu/cqse/clang/ClangTidyError");
    jmethodID errorConstructor = env->GetMethodID(errorClass, "<init>",


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
