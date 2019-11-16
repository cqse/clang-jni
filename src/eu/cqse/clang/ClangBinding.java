
package eu.cqse.clang;

import java.util.List;
import java.util.Map;

/** Main entry point for the clang JNI binding. */
public class ClangBinding {

	static {
		ClangJniLoader.ensureLoaded();
	}

	/** Traverses all children of the given cursor with the given visitor. */
	public static void visitChildren(CXCursor cursor, IClangCursorVisitor visitor) {
		visitChildrenImpl(CXCursor.getCPtr(cursor), new NativeVisitor(visitor));
	}

	/** The native implementation part of the visitor pattern. */
	private static native void visitChildrenImpl(long cursor, NativeVisitor visitor);

	public static ClangSpellingLocationProperties getSpellingLocationProperties(CXSourceLocation location) {
		return getSpellingLocationPropertiesImpl(CXSourceLocation.getCPtr(location));
	}

	private static native ClangSpellingLocationProperties getSpellingLocationPropertiesImpl(long locationPtr);

	public static ClangSpellingLocationProperties getExpansionLocationProperties(CXSourceLocation location) {
		return getExpansionLocationPropertiesImpl(CXSourceLocation.getCPtr(location));
	}

	private static native ClangSpellingLocationProperties getExpansionLocationPropertiesImpl(long locationPtr);

	/** Wrapper class used for passing the visitor to the native code. */
	private static class NativeVisitor {

		private final IClangCursorVisitor delegate;

		public NativeVisitor(IClangCursorVisitor delegate) {
			this.delegate = delegate;
		}

		/**
		 * Visitor method the handles the conversion between native pointers and
		 * {@link CXCursor}.
		 */
		public int visit(long cursorPtr, long parentPtr) {
			return delegate.visit(new CXCursor(cursorPtr, false), new CXCursor(parentPtr, false)).swigValue();
		}
	}

	/** Lists all clang-tidy checks that are available in the library. */
	public static native List<String> getAllClangTidyChecks();

	/** Returns all options for all checks. */
	public static native Map<String, String> getAllClangTidyCheckOptions();

	/**
	 * Runs clang-tidy on the given files.
	 * 
	 * @param files            the files to analyze
	 * @param checks           the semicolon separated list of checks being executed
	 * @param compilerSwitches list of additional compiler switcher to pass on (may
	 *                         be empty)
	 * @param checkOptions     options for individual checks
	 * @param codeIsCpp        if this is true, code is treated as C++ regardless of
	 *                         file extension. This is especially useful for header
	 *                         files.
	 */
	public static native List<ClangTidyError> runClangTidy(List<ClangTidyFile> files, String checks,
			List<String> compilerSwitches, Map<String, String> checkOptions, boolean codeIsCpp);

}
