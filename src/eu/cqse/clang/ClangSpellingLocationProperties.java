package eu.cqse.clang;

public class ClangSpellingLocationProperties {

	private final String file;
	private final int line;
	private final int column;
	private final int offset;

	public ClangSpellingLocationProperties(String file, int line, int column, int offset) {
		this.file = file;
		this.line = line;
		this.column = column;
		this.offset = offset;
	}

	public String getFile() {
		return file;
	}

	public int getLine() {
		return line;
	}

	public int getColumn() {
		return column;
	}

	public int getOffset() {
		return offset;
	}
}
