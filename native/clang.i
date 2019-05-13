%module(directors="1") Clang
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

%include "Index.h"
