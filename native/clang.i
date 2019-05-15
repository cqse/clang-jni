%module Clang
%{
#include "Index.h"
%}


// this is needed to make the code parseable for SWIG

#define CINDEX_LINKAGE
#define CINDEX_DEPRECATED

// Some adjustments to the calling interface
// http://www.swig.org/Doc4.0/SWIGDocumentation.html#Java_tips_techniques

%include "arrays_java.i"
JAVA_ARRAYSOFCLASSES(CXUnsavedFile)
%apply CXUnsavedFile[] { CXUnsavedFile *unsaved_files }

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
%include "Index.h"
