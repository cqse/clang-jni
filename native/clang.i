%module Clang
%{
#include "Index.h"
#include <cstdlib>
#include <cstring>
%}


// this is needed to make the code parseable for SWIG

#define CINDEX_LINKAGE
#define CINDEX_DEPRECATED
#define LLVM_CLANG_C_EXTERN_C_BEGIN
#define LLVM_CLANG_C_EXTERN_C_END
#define LLVM_CLANG_C_STRICT_PROTOTYPES_BEGIN
#define LLVM_CLANG_C_STRICT_PROTOTYPES_END

// Some adjustments to the calling interface
// http://www.swig.org/Doc4.0/SWIGDocumentation.html#Java_tips_techniques

%include "arrays_java.i"
JAVA_ARRAYSOFCLASSES(CXUnsavedFile)
%apply CXUnsavedFile[] { CXUnsavedFile *unsaved_files }

// Map (const char *const *command_line_args, int num_command_line_args) to String[]
// This allows passing Java String arrays directly as compiler flags (e.g. "-x c++").
// The multi-argument typemap collapses both C parameters into a single Java parameter.
%typemap(jni) (const char *const *command_line_args, int num_command_line_args) "jobjectArray"
%typemap(jtype) (const char *const *command_line_args, int num_command_line_args) "String[]"
%typemap(jstype) (const char *const *command_line_args, int num_command_line_args) "String[]"
%typemap(javain) (const char *const *command_line_args, int num_command_line_args) "$javainput"

%typemap(in) (const char *const *command_line_args, int num_command_line_args) {
    if ($input) {
        $2 = (int)jenv->GetArrayLength($input);
        char **temp = new char*[$2 + 1];
        for (int i = 0; i < $2; i++) {
            jstring jstr = (jstring)jenv->GetObjectArrayElement($input, i);
            const char *str = jenv->GetStringUTFChars(jstr, 0);
            temp[i] = strdup(str);
            jenv->ReleaseStringUTFChars(jstr, str);
            jenv->DeleteLocalRef(jstr);
        }
        temp[$2] = NULL;
        $1 = temp;
    } else {
        $1 = NULL;
        $2 = 0;
    }
}

%typemap(freearg) (const char *const *command_line_args, int num_command_line_args) {
    if ($1) {
        for (int i = 0; i < $2; i++) {
            free((char*)$1[i]);
        }
        delete[] (char**)$1;
    }
}

// we also need to expose putenv, so we can adjust the environment
// (which Java can not out of the box)
int clang_putenv(char *string);
%{
    int clang_putenv (char *string) {
        // we have to make a copy of the string as puten does not,
        // so freeing the string would affect the environment.
        // This causes a (very small) memory leak, so do not call in a loop
        char *copy = (char *)malloc(strlen(string)+1);
        strcpy(copy, string);
        return putenv (copy);
    }
%}

%include "CXString.h"
%include "CXSourceLocation.h"
%include "Index.h"

// Wrapper for clang_parseTranslationUnit2 that returns the CXTranslationUnit
// directly (like clang_parseTranslationUnit) but also stores the CXErrorCode
// for retrieval via clang_getLastParseErrorCode(). This avoids the need for
// complex output-pointer typemaps for CXTranslationUnit*.
CXTranslationUnit clang_parseTranslationUnit2_wrap(
    CXIndex CIdx, const char *source_filename,
    const char *const *command_line_args, int num_command_line_args,
    struct CXUnsavedFile *unsaved_files, unsigned num_unsaved_files,
    unsigned options);
int clang_getLastParseErrorCode(void);

%{
static thread_local int _clang_last_parse_error = 0;

CXTranslationUnit clang_parseTranslationUnit2_wrap(
    CXIndex CIdx, const char *source_filename,
    const char *const *command_line_args, int num_command_line_args,
    struct CXUnsavedFile *unsaved_files, unsigned num_unsaved_files,
    unsigned options) {
    CXTranslationUnit TU = NULL;
    _clang_last_parse_error = (int)clang_parseTranslationUnit2(
        CIdx, source_filename, command_line_args, num_command_line_args,
        unsaved_files, num_unsaved_files, options, &TU);
    return TU;
}

int clang_getLastParseErrorCode(void) {
    return _clang_last_parse_error;
}
%}
