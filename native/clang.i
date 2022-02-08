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
/*
// This is from the Swig docs (Section 26.10.4 Converting Java String arrays to char **).
// We need it to pass a Java String (like "-std=c99" to the main function of the clang parser).

// This tells SWIG to treat char ** as a special case when used as a parameter  in a function call
%typemap(in) char ** (jint size) {
  int i = 0;
  size = (*jenv)->GetArrayLength(jenv, $input);
  $1 = (char **) malloc((size+1)*sizeof(char *));
  // make a copy of each string
  for (i = 0; i<size; i++) {
    jstring j_string = (jstring)(*jenv)->GetObjectArrayElement(jenv, $input, i);
    const char * c_string = (*jenv)->GetStringUTFChars(jenv, j_string, 0);
    $1[i] = malloc((strlen(c_string)+1)*sizeof(char));
    strcpy($1[i], c_string);
    (*jenv)->ReleaseStringUTFChars(jenv, j_string, c_string);
    (*jenv)->DeleteLocalRef(jenv, j_string);
  }
  $1[i] = 0;
}

// This cleans up the memory we malloc'd before the function call
%typemap(freearg) char ** {
  int i;
  for (i=0; i<size$argnum-1; i++)
    free($1[i]);
  free($1);
}

// omitted the unnecessary typemap(out) part to keep complexity and error potential low

// These 3 typemaps tell SWIG what JNI and Java types to use
%typemap(jni) char ** "jobjectArray"
%typemap(jtype) char ** "String[]"
%typemap(jstype) char ** "String[]"

// These 2 typemaps handle the conversion of the jtype to jstype typemap type and vice versa
%typemap(javain) char ** "$javainput"
%typemap(javaout) char ** {
    return $jnicall;
  }
*/
