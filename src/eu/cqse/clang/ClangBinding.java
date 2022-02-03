
package eu.cqse.clang;

import java.util.ArrayList;
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
			return delegate.visit(new CXCursor(cursorPtr, true), new CXCursor(parentPtr, true)).swigValue();
		}
	}
}
