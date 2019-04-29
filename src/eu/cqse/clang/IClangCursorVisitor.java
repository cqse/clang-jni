
package eu.cqse.clang;

/** Visitor used in {@link ClangBinding}. */
public interface IClangCursorVisitor {

	/**
	 * Must be implemented to visit cursors in
	 * {@link ClangBindingUtils#visitChildren(CXCursor, IClangCursorVisitor)}. The
	 * return value determines whether visiting is aborted after an invocation or
	 * continued (with next sibling or first child).
	 */
	public CXChildVisitResult visit(CXCursor cursor, CXCursor parent);
}
