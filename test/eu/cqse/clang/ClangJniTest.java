package eu.cqse.clang;

import java.util.ArrayList;
import java.util.List;

/**
 * Integration tests for the clang-jni library, exercising the JNI bindings in
 * the same way Teamscale's custom checks use them. These tests run standalone
 * without any Teamscale dependencies.
 *
 * <p>Each test parses C/C++ code via clang_parseTranslationUnit, then traverses
 * the AST with ClangBinding.visitChildren and verifies the results.
 *
 * <p>This class intentionally does not use JUnit or any test framework. The
 * clang-jni project has no build tool and no dependency management — it is
 * built with plain javac and shell scripts. These tests must run on all three
 * build platforms (Linux, Windows, macOS) as a quick smoke test after compiling
 * the native library, without requiring any additional JARs on the classpath.
 *
 * <p>Run with: {@code java -cp build --enable-native-access=ALL-UNNAMED eu.cqse.clang.ClangJniTest}
 */
public class ClangJniTest {

    private static final int PARSE_OPTION_INCOMPLETE = 0x02;
    private static final int PARSE_OPTION_KEEP_GOING = 0x200;
    private static final int PARSE_OPTION_SINGLE_FILE_PARSE = 0x400;
    private static final int OPTIONS = PARSE_OPTION_INCOMPLETE | PARSE_OPTION_KEEP_GOING | PARSE_OPTION_SINGLE_FILE_PARSE;

    private static int passed = 0;
    private static int failed = 0;

    public static void main(String[] args) {
        System.out.println("Loading clang-jni native library...");
        ClangJniLoader.ensureLoaded();
        System.out.println("Loaded.\n");

        testParseSimpleFunction();
        testParseEmptyFile();
        testTypeResolution();
        testTypedefResolution();
        testBinaryOperatorDetection();
        testUnaryOperatorBitwiseNot();
        testTemplateWithLambda();
        testOpenMPPragmas();
        testNaNSelfComparison();
        testCompositeExpressionWidening();
        testCommandLineArgs();
        testSharedWrapperConsistency();
        testRepeatedParseDispose();

        System.out.println("\n========================================");
        System.out.println("Results: " + passed + " passed, " + failed + " failed");
        if (failed > 0) {
            System.exit(1);
        }
    }

    private static void testParseSimpleFunction() {
        String code = "int add(int a, int b) { return a + b; }";
        List<CXCursorKind> kinds = collectCursorKinds("test.c", code);
        assertContains("testParseSimpleFunction", kinds, CXCursorKind.CXCursor_FunctionDecl);
    }

    private static void testParseEmptyFile() {
        List<CXCursorKind> kinds = collectCursorKinds("empty.h", "");
        assertCondition("testParseEmptyFile", kinds.isEmpty(),
                "Expected empty AST for empty file, got " + kinds.size() + " nodes");
    }

    private static void testTypeResolution() {
        assertTypeKind("int x;", CXTypeKind.CXType_Int, "testTypeResolution(int)");
        assertTypeKind("unsigned int x;", CXTypeKind.CXType_UInt, "testTypeResolution(unsigned int)");
        assertTypeKind("float x;", CXTypeKind.CXType_Float, "testTypeResolution(float)");
        assertTypeKind("double x;", CXTypeKind.CXType_Double, "testTypeResolution(double)");
        assertTypeKind("long double x;", CXTypeKind.CXType_LongDouble, "testTypeResolution(long double)");
        assertTypeKind("char x;", CXTypeKind.CXType_Char_S, "testTypeResolution(char)");
        assertTypeKind("unsigned char x;", CXTypeKind.CXType_UChar, "testTypeResolution(unsigned char)");
        assertTypeKind("short x;", CXTypeKind.CXType_Short, "testTypeResolution(short)");
        assertTypeKind("long x;", CXTypeKind.CXType_Long, "testTypeResolution(long)");
        assertTypeKind("long long x;", CXTypeKind.CXType_LongLong, "testTypeResolution(long long)");
        assertTypeKind("_Bool x;", CXTypeKind.CXType_Bool, "testTypeResolution(_Bool)");
    }

    private static void testTypedefResolution() {
        String code = "typedef unsigned long bar; typedef bar foo; foo x = 5;";
        CXTypeKind resolved = getResolvedVarTypeKind("test.c", code);
        assertCondition("testTypedefResolution", resolved == CXTypeKind.CXType_ULong,
                "Expected CXType_ULong, got " + resolved);
    }

    private static void testUnaryOperatorBitwiseNot() {
        String code = "void foo() { int x = 5; int y = ~x; }";
        List<String> unaryOps = new ArrayList<>();
        parseAndVisit("test.c", code, (cursor, parent) -> {
            if (Clang.clang_getCursorKind(cursor) == CXCursorKind.CXCursor_UnaryOperator) {
                SWIGTYPE_p_CXTranslationUnitImpl tu = Clang.clang_Cursor_getTranslationUnit(cursor);
                CXToken token = Clang.clang_getToken(tu, Clang.clang_getCursorLocation(cursor));
                if (token != null) {
                    String spelling = Clang.clang_getCString(Clang.clang_getTokenSpelling(tu, token));
                    unaryOps.add(spelling);
                }
            }
            return CXChildVisitResult.CXChildVisit_Recurse;
        });
        assertContainsString("testUnaryOperatorBitwiseNot", unaryOps, "~");
    }

    private static void testBinaryOperatorDetection() {
        String code = "void foo() { int a = 1; int b = 2; int c = a + b; }";
        List<CXCursorKind> kinds = new ArrayList<>();
        parseAndVisit("test.c", code, (cursor, parent) -> {
            if (Clang.clang_getCursorKind(cursor) == CXCursorKind.CXCursor_BinaryOperator) {
                kinds.add(CXCursorKind.CXCursor_BinaryOperator);
            }
            return CXChildVisitResult.CXChildVisit_Recurse;
        });
        assertCondition("testBinaryOperatorDetection", !kinds.isEmpty(),
                "Expected to find at least one binary operator");
    }

    private static void testNaNSelfComparison() {
        String code = "int is_nan(float f) { return f != f; }";
        List<CXTypeKind> varTypes = new ArrayList<>();
        parseAndVisit("test.c", code, (cursor, parent) -> {
            if (Clang.clang_getCursorKind(cursor) == CXCursorKind.CXCursor_DeclRefExpr) {
                CXTypeKind typeKind = Clang.clang_getCursorType(cursor).getKind();
                varTypes.add(typeKind);
            }
            return CXChildVisitResult.CXChildVisit_Recurse;
        });
        assertContains("testNaNSelfComparison", varTypes, CXTypeKind.CXType_Float);
    }

    private static void testCompositeExpressionWidening() {
        String code = ""
                + "typedef __UINT16_TYPE__ uint16_t;\n"
                + "typedef __UINT32_TYPE__ uint32_t;\n"
                + "int main() {\n"
                + "    uint16_t a = 1, b = 2;\n"
                + "    uint32_t c = a + b;\n"
                + "}\n";
        List<CXCursorKind> assignments = new ArrayList<>();
        parseAndVisit("test.c", code, (cursor, parent) -> {
            if (Clang.clang_getCursorKind(cursor) == CXCursorKind.CXCursor_BinaryOperator) {
                assignments.add(CXCursorKind.CXCursor_BinaryOperator);
            }
            return CXChildVisitResult.CXChildVisit_Recurse;
        });
        assertCondition("testCompositeExpressionWidening", !assignments.isEmpty(),
                "Expected to find binary operators in composite expression");
    }

    private static void testTemplateWithLambda() {
        String code = ""
                + "template <typename LOOP_BODY>\n"
                + "inline void forall(int Begin, int End, LOOP_BODY LoopBody) {\n"
                + "  for (int I = Begin; I < End; ++I) {\n"
                + "    LoopBody(I);\n"
                + "  }\n"
                + "}\n"
                + "\n"
                + "#define N (1000)\n"
                + "\n"
                + "int main() {\n"
                + "  double A[N], B[N], C[N];\n"
                + "  for (int I = 0; I < N; I++) {\n"
                + "    A[I] = I + 1;\n"
                + "    B[I] = -I;\n"
                + "    C[I] = -9;\n"
                + "  }\n"
                + "  forall(0, N, [&](int I) { C[I] += A[I] + B[I]; });\n"
                + "  return 0;\n"
                + "}\n";
        List<CXCursorKind> kinds = collectCursorKinds("lambda_mapping.cpp", code);
        assertContains("testTemplateWithLambda", kinds, CXCursorKind.CXCursor_FunctionTemplate);
        assertContains("testTemplateWithLambda", kinds, CXCursorKind.CXCursor_LambdaExpr);
    }

    private static void testOpenMPPragmas() {
        String code = ""
                + "void compute(int* arr, int n) {\n"
                + "#pragma omp parallel for\n"
                + "  for (int i = 0; i < n; i++) {\n"
                + "    arr[i] = i * 2;\n"
                + "  }\n"
                + "}\n";
        List<CXCursorKind> kinds = collectCursorKinds("omp_test.cpp", code);
        assertContains("testOpenMPPragmas", kinds, CXCursorKind.CXCursor_FunctionDecl);
    }

    /**
     * Tests that String[] command_line_args are passed correctly to clang.
     * Uses "-x c++" to force C++ parsing on a .c file. Without this flag,
     * clang would parse as C and not find the C++ class declaration.
     */
    private static void testCommandLineArgs() {
        // C++ code that is NOT valid C (class keyword)
        String code = "class Foo { public: int bar() { return 42; } };";
        SWIGTYPE_p_void index = Clang.clang_createIndex(0, 0);
        CXUnsavedFile file = createUnsavedFile("test.c", code);

        // Parse with -x c++ flag — should find ClassDecl
        SWIGTYPE_p_CXTranslationUnitImpl tuCpp = Clang.clang_parseTranslationUnit(
                index, "test.c", new String[]{"-x", "c++"},
                new CXUnsavedFile[] { file }, 1, OPTIONS);
        try {
            List<CXCursorKind> kindsCpp = new ArrayList<>();
            ClangBinding.visitChildren(Clang.clang_getTranslationUnitCursor(tuCpp), (cursor, parent) -> {
                kindsCpp.add(Clang.clang_getCursorKind(cursor));
                return CXChildVisitResult.CXChildVisit_Recurse;
            });
            assertContains("testCommandLineArgs(-x c++)", kindsCpp, CXCursorKind.CXCursor_ClassDecl);
        } finally {
            Clang.clang_disposeTranslationUnit(tuCpp);
        }

        // Parse WITHOUT -x c++ — .c file parsed as C, should NOT find ClassDecl
        SWIGTYPE_p_CXTranslationUnitImpl tuC = Clang.clang_parseTranslationUnit(
                index, "test.c", null,
                new CXUnsavedFile[] { file }, 1, OPTIONS);
        try {
            List<CXCursorKind> kindsC = new ArrayList<>();
            ClangBinding.visitChildren(Clang.clang_getTranslationUnitCursor(tuC), (cursor, parent) -> {
                kindsC.add(Clang.clang_getCursorKind(cursor));
                return CXChildVisitResult.CXChildVisit_Recurse;
            });
            assertCondition("testCommandLineArgs(no flag)",
                    !kindsC.contains(CXCursorKind.CXCursor_ClassDecl),
                    "Parsing .c as C should not find ClassDecl");
        } finally {
            Clang.clang_disposeTranslationUnit(tuC);
            file.delete();
            Clang.clang_disposeIndex(index);
        }
    }

    private static void testSharedWrapperConsistency() {
        String code = "void foo() { int x = 1 + 2; int y = 3; }";
        SWIGTYPE_p_void index = Clang.clang_createIndex(0, 0);
        CXUnsavedFile file = createUnsavedFile("test.c", code);
        SWIGTYPE_p_CXTranslationUnitImpl tu = Clang.clang_parseTranslationUnit(
                index, "test.c", null, new CXUnsavedFile[] { file }, 1, OPTIONS);
        try {
            CXCursor root = Clang.clang_getTranslationUnitCursor(tu);
            List<CXCursorKind> firstPass = new ArrayList<>();
            ClangBinding.visitChildren(root, (cursor, parent) -> {
                firstPass.add(Clang.clang_getCursorKind(cursor));
                return CXChildVisitResult.CXChildVisit_Recurse;
            });
            List<CXCursorKind> secondPass = new ArrayList<>();
            ClangBinding.visitChildren(root, (cursor, parent) -> {
                secondPass.add(Clang.clang_getCursorKind(cursor));
                return CXChildVisitResult.CXChildVisit_Recurse;
            });
            assertCondition("testSharedWrapperConsistency",
                    firstPass.equals(secondPass),
                    "Two traversals of the same TU should produce identical results");
        } finally {
            Clang.clang_disposeTranslationUnit(tu);
            file.delete();
            Clang.clang_disposeIndex(index);
        }
    }

    private static void testRepeatedParseDispose() {
        String[] files = {
                "int main() { return 0; }",
                "void foo(int x) { if (x > 0) foo(x - 1); }",
                "typedef unsigned long long uint64; uint64 x = 42;",
                "enum Color { RED, GREEN, BLUE }; enum Color c = RED;",
                "struct Point { int x; int y; }; struct Point p = {1, 2};"
        };
        boolean ok = true;
        for (int i = 0; i < 100; i++) {
            String code = files[i % files.length];
            SWIGTYPE_p_void index = Clang.clang_createIndex(0, 0);
            CXUnsavedFile file = createUnsavedFile("test.c", code);
            SWIGTYPE_p_CXTranslationUnitImpl tu = Clang.clang_parseTranslationUnit(
                    index, "test.c", null, new CXUnsavedFile[] { file }, 1, OPTIONS);
            if (tu != null) {
                ClangBinding.visitChildren(Clang.clang_getTranslationUnitCursor(tu), (cursor, parent) ->
                        CXChildVisitResult.CXChildVisit_Recurse);
                Clang.clang_disposeTranslationUnit(tu);
            } else {
                ok = false;
            }
            file.delete();
            Clang.clang_disposeIndex(index);
        }
        assertCondition("testRepeatedParseDispose", ok,
                "All 100 parse/dispose cycles should succeed");
    }

    // ==================== Helpers ====================

    private static CXUnsavedFile createUnsavedFile(String filename, String content) {
        CXUnsavedFile file = new CXUnsavedFile();
        file.setFilename(filename);
        file.setContents(content);
        file.setLength(content.length());
        return file;
    }

    private static List<CXCursorKind> collectCursorKinds(String filename, String code) {
        List<CXCursorKind> kinds = new ArrayList<>();
        parseAndVisit(filename, code, (cursor, parent) -> {
            kinds.add(Clang.clang_getCursorKind(cursor));
            return CXChildVisitResult.CXChildVisit_Recurse;
        });
        return kinds;
    }

    private static void parseAndVisit(String filename, String code, IClangCursorVisitor visitor) {
        SWIGTYPE_p_void index = Clang.clang_createIndex(0, 0);
        CXUnsavedFile file = createUnsavedFile(filename, code);
        try {
            SWIGTYPE_p_CXTranslationUnitImpl tu = Clang.clang_parseTranslationUnit(
                    index, filename, null, new CXUnsavedFile[] { file }, 1, OPTIONS);
            if (tu != null) {
                try {
                    CXCursor root = Clang.clang_getTranslationUnitCursor(tu);
                    ClangBinding.visitChildren(root, visitor);
                } finally {
                    Clang.clang_disposeTranslationUnit(tu);
                }
            }
        } finally {
            file.delete();
            Clang.clang_disposeIndex(index);
        }
    }

    private static void assertTypeKind(String declaration, CXTypeKind expectedKind, String testName) {
        CXTypeKind actual = getVarTypeKind("test.c", declaration);
        assertCondition(testName, actual == expectedKind, "Expected " + expectedKind + ", got " + actual);
    }

    private static CXTypeKind getVarTypeKind(String filename, String code) {
        CXTypeKind[] result = { null };
        parseAndVisit(filename, code, (cursor, parent) -> {
            CXCursorKind kind = Clang.clang_getCursorKind(cursor);
            if (kind == CXCursorKind.CXCursor_VarDecl || kind == CXCursorKind.CXCursor_EnumDecl) {
                result[0] = Clang.clang_getCursorType(cursor).getKind();
                return CXChildVisitResult.CXChildVisit_Break;
            }
            return CXChildVisitResult.CXChildVisit_Recurse;
        });
        return result[0];
    }

    private static CXTypeKind getResolvedVarTypeKind(String filename, String code) {
        CXTypeKind[] result = { null };
        SWIGTYPE_p_void index = Clang.clang_createIndex(0, 0);
        CXUnsavedFile file = createUnsavedFile(filename, code);
        try {
            SWIGTYPE_p_CXTranslationUnitImpl tu = Clang.clang_parseTranslationUnit(
                    index, filename, null, new CXUnsavedFile[] { file }, 1, OPTIONS);
            if (tu != null) {
                try {
                    CXCursor root = Clang.clang_getTranslationUnitCursor(tu);
                    ClangBinding.visitChildren(root, (cursor, parent) -> {
                        if (Clang.clang_getCursorKind(cursor) == CXCursorKind.CXCursor_VarDecl) {
                            // Resolve typedef chain, same as ClangUtils.resolveTypedefs.
                            // In clang 20+, Elaborated types may wrap the underlying type,
                            // so we unwrap those too via clang_Type_getNamedType.
                            CXType type = Clang.clang_getCursorType(cursor);
                            boolean changed = true;
                            while (changed) {
                                changed = false;
                                if (type.getKind() == CXTypeKind.CXType_Typedef) {
                                    type = Clang.clang_getTypedefDeclUnderlyingType(
                                            Clang.clang_getTypeDeclaration(type));
                                    changed = true;
                                }
                                if (type.getKind() == CXTypeKind.CXType_Elaborated) {
                                    type = Clang.clang_Type_getNamedType(type);
                                    changed = true;
                                }
                            }
                            result[0] = type.getKind();
                            return CXChildVisitResult.CXChildVisit_Break;
                        }
                        return CXChildVisitResult.CXChildVisit_Recurse;
                    });
                } finally {
                    Clang.clang_disposeTranslationUnit(tu);
                }
            }
        } finally {
            file.delete();
            Clang.clang_disposeIndex(index);
        }
        return result[0];
    }

    // ==================== Assertion helpers ====================

    private static <T> void assertContains(String testName, List<T> list, T expected) {
        if (list.contains(expected)) {
            System.out.println("  PASS: " + testName);
            passed++;
        } else {
            System.out.println("  FAIL: " + testName + " - expected list to contain " + expected + ", got " + list);
            failed++;
        }
    }

    private static void assertContainsString(String testName, List<String> list, String expected) {
        if (list.contains(expected)) {
            System.out.println("  PASS: " + testName);
            passed++;
        } else {
            System.out.println("  FAIL: " + testName + " - expected list to contain '" + expected + "', got " + list);
            failed++;
        }
    }

    private static void assertCondition(String testName, boolean condition, String message) {
        if (condition) {
            System.out.println("  PASS: " + testName);
            passed++;
        } else {
            System.out.println("  FAIL: " + testName + " - " + message);
            failed++;
        }
    }
}
