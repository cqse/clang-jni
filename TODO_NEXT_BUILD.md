# Improvements for the next clang-jni rebuild

Changes that should be made the next time we rebuild the native libraries.
These don't warrant a rebuild on their own, but should be included when we
rebuild for other reasons (e.g., LLVM version upgrade).

## Add diagnostic formatting to SWIG interface

`clang_formatDiagnostic` and `clang_getDiagnosticSpelling` are not exported
in the current SWIG bindings.
This means `ClangUtils#debugPrintDiagnostics` in Teamscale can only print
opaque object references instead of readable diagnostic messages.

Add to `clang.i`:
- `clang_formatDiagnostic(CXDiagnostic, unsigned)` — formats a diagnostic
  with source location and severity into a human-readable string
- `clang_getDiagnosticSpelling(CXDiagnostic)` — returns just the message text
- `clang_disposeDiagnostic(CXDiagnostic)` — frees the diagnostic

The `CXDiagnostic` type also needs a proper SWIG typemap instead of the
current opaque `SWIGTYPE_p_CXDiagnostic`.
