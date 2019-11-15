package eu.cqse.clang;

/** Helper class to retrieve results from clang-tidy. */
public class ClangTidyError {

	private final String checkName;

	private final String message;

	private final String path;

	private final int offset;

	public ClangTidyError(String checkName, String message, String path, int offset) {
		this.checkName = checkName;
		this.message = message;
		this.path = path;
		this.offset = offset;
	}

	public String getCheckName() {
		return checkName;
	}

	public String getMessage() {
		return message;
	}

	public String getPath() {
		return path;
	}

	public int getOffset() {
		return offset;
	}

}
