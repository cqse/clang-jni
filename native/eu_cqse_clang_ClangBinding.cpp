#include "eu_cqse_clangBinding_ClangBindingUtils.h"
#include "Index.h"
#include <cstdlib>
#include <cstring>

// Parameters with environment info passed into the visit method
struct ClangBindingVisitorParameter {
    jmethodID method;
    JNIEnv * env;
    jobject java_visitor;
};

JNIEXPORT void JNICALL Java_eu_cqse_clangBinding_ClangBindingUtils_visitChildrenImpl
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
    jmethodID constructor = env->GetMethodID(env, result_class, "<init>", "(Ljava/lang/String;III)V");

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
    jmethodID constructor = env->GetMethodID(env, result_class, "<init>", "(Ljava/lang/String;III)V");

    CXString fileName = clang_getFileName (file);
    jstring javaFileName = env->NewStringUTF(clang_getCString(fileName));
    clang_disposeString (fileName);

    return env->NewObject (result_class, constructor, javaFileName, line, column, offset);
}

