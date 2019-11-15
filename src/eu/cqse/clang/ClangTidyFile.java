package eu.cqse.clang;

/** Helper class to pass files to clang-tidy. */
public class ClangTidyFile {

	/** The path of the file. */
	private final String path;

	/** The content of the file. */
	private final String content;

	public ClangTidyFile(String path, String content) {
		this.path = path;
		this.content = content;
	}

	public String getPath() {
		return path;
	}

	public String getContent() {
		return content;
	}
}
