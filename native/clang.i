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
// (which Java can not out pf the box)
int putenv(char *string);

%include "CXString.h"
%include "Index.h"
