package eu.cqse.clang;

import java.io.File;

/**
 * Simple main program that can be used to test the clang binding on various
 * machines.
 */
public class Main {

	public static void main(String[] args) {
		if (args.length == 0) {
			System.err.println("Missing arguments! Please pass names of C/C++ files.");
			return;
		}

		ClangBinding.ensureLibraryLoaded();

		SWIGTYPE_p_void index = clang.clang_createIndex(0, 0);
		try {
			for (String arg : args) {
				System.out.println("===== " + arg + " =====");
				SWIGTYPE_p_CXTranslationUnitImpl translationUnit = clang.clang_parseTranslationUnit(index,
						new File(arg).getAbsolutePath(), null, 0, null, 0, 0);
				try {
					CXCursor cursor = clang.clang_getTranslationUnitCursor(translationUnit);
					try {
						ClangBinding.visitChildren(cursor, new PrintVisitor(translationUnit));
					} finally {
						cursor.delete();
					}
				} finally {
					clang.clang_disposeTranslationUnit(translationUnit);
				}
			}
		} finally {
			clang.clang_disposeIndex(index);
		}
	}

	private static class PrintVisitor implements IClangCursorVisitor {

		private final SWIGTYPE_p_CXTranslationUnitImpl translationUnit;
		private final int level;

		public PrintVisitor(SWIGTYPE_p_CXTranslationUnitImpl translationUnit) {
			this(translationUnit, 0);
		}

		public PrintVisitor(SWIGTYPE_p_CXTranslationUnitImpl translationUnit, int level) {
			this.translationUnit = translationUnit;
			this.level = level;
		}

		@Override
		public CXChildVisitResult visit(CXCursor cursor, CXCursor parent) {

			for (int i = 0; i < level; i++) {
				System.out.print("    ");
			}
			print(cursor);

			ClangBinding.visitChildren(cursor, new PrintVisitor(translationUnit, level + 1));
			return CXChildVisitResult.CXChildVisit_Continue;
		}

		public void print(CXCursor current) {
			CXSourceLocation location = clang.clang_getCursorLocation(current);

			long line = ClangBinding.getSpellingLocationProperties(location).getLine();

			String name = clang.clang_getCString(clang.clang_getCursorDisplayName(current));
			String kind = clang.clang_getCursorKind(current).toString();
			String typeKind = clang.clang_getCursorType(current).getKind().toString();
			String type = clang.clang_getCString(clang.clang_getTypeSpelling(clang.clang_getCursorType(current)));
			String spelling = clang.clang_getCString(clang.clang_getCursorSpelling(current));
			CXToken token = clang.clang_getToken(translationUnit, clang.clang_getCursorLocation(current));
			String tokenSpelling = "token is null";
			if (token != null) {
				tokenSpelling = clang.clang_getCString(clang.clang_getTokenSpelling(translationUnit, token));
			}
			System.out.println(line + " name:" + name + " kind:" + kind + " type:" + type + "/" + typeKind
					+ " spelling:" + spelling + " tokenSpelling: " + tokenSpelling);
		}

	}

}
