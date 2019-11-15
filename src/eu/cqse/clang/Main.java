package eu.cqse.clang;

import java.io.IOException;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/**
 * Simple main program that can be used to test the clang binding on various
 * machines.
 */
public class Main {

	private static final String CODE = "" + //
			"#include \"my-header.h\"\n" + //
			"\n" + //
			"int Foo::add (int a, int b) {\n" + //
			"  for (int i = 0; i < 10; ++i);\n" + //
			"    printf (\"Hello world\\n\");\n" + //
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

	public static void main(String[] args) throws IOException {
		ClangJniLoader.ensureLoaded();
		parseCode();
		runClangTidy();
	}

	private static void parseCode() {
		System.out.println("## Test code parsing");

		CXUnsavedFile codeFile = new CXUnsavedFile();
		codeFile.setFilename("/foo/code.cpp");
		codeFile.setContents(CODE);
		codeFile.setLength(CODE.length());

		CXUnsavedFile headerFile = new CXUnsavedFile();
		headerFile.setFilename("/foo/my-header.h");
		headerFile.setContents(HEADER);
		headerFile.setLength(HEADER.length());

		parseFiles(codeFile, headerFile);
	}

	/** Parses the first file, all remaining files are referenced headers. */
	private static void parseFiles(CXUnsavedFile... files) {
		SWIGTYPE_p_void index = Clang.clang_createIndex(0, 0);
		try {
			SWIGTYPE_p_CXTranslationUnitImpl translationUnit = Clang.clang_parseTranslationUnit(index,
					files[0].getFilename(), null, 0, files, files.length, ClangJNI.CXTranslationUnit_KeepGoing_get());
			try {
				CXCursor cursor = Clang.clang_getTranslationUnitCursor(translationUnit);
				try {
					ClangBinding.visitChildren(cursor, new PrintVisitor(translationUnit));
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

			ClangSpellingLocationProperties locationProperties = ClangBinding.getSpellingLocationProperties(location);
			long line = locationProperties.getLine();
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

			System.out.println(locationProperties.getFile() + ":" + line + " name:" + name + " kind:" + kind + " type:"
					+ type + "/" + typeKind + " spelling:" + spelling + " tokenSpelling: " + tokenSpelling);
		}

	}

	private static void runClangTidy() {
		System.out.println("\n## All clang-tidy checks found: ");
		ClangBinding.getAllClangTidyChecks().stream().map(s -> "  - " + s).forEach(System.out::println);

		System.out.println("\n## All clang-tidy options found: ");
		ClangBinding.getAllClangTidyCheckOptions()
				.forEach((key, value) -> System.out.println("  " + key + " -> " + value));

		System.out.println("\n## clang-tidy test run: ");
		List<ClangTidyFile> files = Arrays.asList(new ClangTidyFile("code.cc", CODE),
				new ClangTidyFile("my-header.h", HEADER));
		List<ClangTidyError> errors = ClangBinding.runClangTidy(files, "*", ClangBinding.getAllClangTidyCheckOptions());
		for (ClangTidyError error : errors) {
			System.out.println(
					error.getPath() + ":" + error.getOffset() + ":" + error.getCheckName() + ":" + error.getMessage());
		}
	}

}
