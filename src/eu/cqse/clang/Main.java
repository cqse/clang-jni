package eu.cqse.clang;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Paths;

/**
 * JAR entry point for manual testing and debugging only. This class is not used by Teamscale.
 * Teamscale uses the clang-jni library through its own ClangTranslationUnitWrapper.
 *
 * <p>Loads the native library, prints version information, and optionally parses a user-provided
 * C/C++ file and prints its AST. Useful for verifying the JAR works on a given machine without
 * needing Teamscale.
 *
 * <p>Usage:
 * <pre>
 * java -jar clang-jni.jar --enable-native-access=ALL-UNNAMED [file.cpp]
 * </pre>
 *
 * <p>Without arguments, parses a built-in test snippet.
 * With a file argument, parses that file and prints its AST.
 */
public class Main {

	public static void main(String[] args) throws IOException {
		System.out.println("Loading clang-jni native library...");
		ClangJniLoader.ensureLoaded();
		System.out.println("Loaded successfully.");
		System.out.println("Clang version: " + Clang.clang_getCString(Clang.clang_getClangVersion()));

		if (args.length > 0) {
			String filePath = args[0];
			System.out.println("\nParsing file: " + filePath);
			String content = new String(Files.readAllBytes(Paths.get(filePath)));
			parseAndPrintAst(filePath, content);
		} else {
			System.out.println("\nParsing a built-in C++ test snippet...");
			parseAndPrintAst("test.cpp", "int main() { return 0; }");
		}
	}

	private static void parseAndPrintAst(String fileName, String content) {
		SWIGTYPE_p_void index = Clang.clang_createIndex(0, 0);
		CXUnsavedFile file = new CXUnsavedFile();
		file.setFilename(fileName);
		file.setContents(content);
		file.setLength(content.length());

		try {
			SWIGTYPE_p_CXTranslationUnitImpl tu = Clang.clang_parseTranslationUnit(
					index, fileName, new String[] { "-x", "c++" }, new CXUnsavedFile[] { file }, 1, 0);

			if (tu == null) {
				System.out.println("Parse FAILED.");
				return;
			}

			System.out.println("Parse successful. AST:\n");
			CXCursor root = Clang.clang_getTranslationUnitCursor(tu);
			printAst(root, tu, 0);
			Clang.clang_disposeTranslationUnit(tu);
		} finally {
			file.delete();
			Clang.clang_disposeIndex(index);
		}
	}

	private static void printAst(CXCursor cursor, SWIGTYPE_p_CXTranslationUnitImpl tu, int depth) {
		ClangBinding.visitChildren(cursor, (child, parent) -> {
			// Only show nodes from the main file, not built-in declarations
			if (Clang.clang_Location_isFromMainFile(Clang.clang_getCursorLocation(child)) == 0) {
				return CXChildVisitResult.CXChildVisit_Continue;
			}

			for (int i = 0; i < depth; i++) {
				System.out.print("  ");
			}
			String kind = Clang.clang_getCursorKind(child).toString();
			String spelling = Clang.clang_getCString(Clang.clang_getCursorSpelling(child));
			String type = Clang.clang_getCString(Clang.clang_getTypeSpelling(Clang.clang_getCursorType(child)));

			System.out.print(kind);
			if (!spelling.isEmpty()) {
				System.out.print(" '" + spelling + "'");
			}
			if (!type.isEmpty()) {
				System.out.print(" : " + type);
			}
			System.out.println();

			printAst(child, tu, depth + 1);
			return CXChildVisitResult.CXChildVisit_Continue;
		});
	}
}
