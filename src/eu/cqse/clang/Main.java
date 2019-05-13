package eu.cqse.clang;

/**
 * Simple main program that can be used to test the clang binding on various
 * machines.
 */
public class Main {

	private static final String CODE = "" + //
			"#include \"my-header.h\"\n" + //
			"\n" + //
			"int Foo::add (int a, int b) {\n" + //
			"  return a+b; \n" + //
			"}\n" + //
			"\n";

	private static final String HEADER = "" + //
			"#ifndef MY_HEADER\n" + //
			"#define MY_HEADER\n" + //
			"\n" + //
			"class Foo {\n" + //
			"public: \n" + //
			"  int add (int a, int b);\n" + //
			"};\n" + //
			"\n" + //
			"#endif // MY_HEADER\n";

	public static void main(String[] args) {
		ClangJniLoader.ensureLoaded();

		CXUnsavedFile codeFile = new CXUnsavedFile();
		codeFile.setFilename("code.cpp");
		codeFile.setContents(CODE);
		codeFile.setLength(CODE.length());

		CXUnsavedFile headerFile = new CXUnsavedFile();
		headerFile.setFilename("my-header.h");
		headerFile.setContents(HEADER);
		headerFile.setLength(HEADER.length());

		CXUnsavedFile[] files = new CXUnsavedFile[] { codeFile };

		SWIGTYPE_p_void index = Clang.clang_createIndex(0, 0);
		try {
			SWIGTYPE_p_CXTranslationUnitImpl translationUnit = Clang.clang_parseTranslationUnit(index, "code.cpp", null,
					0, files, files.length, 0);
//			SWIGTYPE_p_CXTranslationUnitImpl translationUnit = Clang.clang_parseTranslationUnit(index,
//					"native/eu_cqse_clang_ClangBinding.cpp", null, 0, new CXUnsavedFile[0], 0, 0);
			try {
				CXCursor cursor = Clang.clang_getTranslationUnitCursor(translationUnit);

				try {
					System.err.println("pre-visit");
					ClangBinding.visitChildren(cursor, new PrintVisitor(translationUnit));
					System.err.println("post-visit");
				} finally {
					cursor.delete();
				}
			} finally {
				Clang.clang_disposeTranslationUnit(translationUnit);
			}
		} finally {
			Clang.clang_disposeIndex(index);
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
			CXSourceLocation location = Clang.clang_getCursorLocation(current);

			long line = ClangBinding.getSpellingLocationProperties(location).getLine();

			String name = Clang.clang_getCString(Clang.clang_getCursorDisplayName(current));
			String kind = Clang.clang_getCursorKind(current).toString();
			String typeKind = Clang.clang_getCursorType(current).getKind().toString();
			String type = Clang.clang_getCString(Clang.clang_getTypeSpelling(Clang.clang_getCursorType(current)));
			String spelling = Clang.clang_getCString(Clang.clang_getCursorSpelling(current));
			CXToken token = Clang.clang_getToken(translationUnit, Clang.clang_getCursorLocation(current));
			String tokenSpelling = "token is null";
			if (token != null) {
				tokenSpelling = Clang.clang_getCString(Clang.clang_getTokenSpelling(translationUnit, token));
			}
			System.out.println(line + " name:" + name + " kind:" + kind + " type:" + type + "/" + typeKind
					+ " spelling:" + spelling + " tokenSpelling: " + tokenSpelling);
		}

	}

}
