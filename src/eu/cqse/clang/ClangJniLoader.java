package eu.cqse.clang;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.FileSystemNotFoundException;
import java.nio.file.FileSystems;
import java.nio.file.Files;
import java.nio.file.ProviderNotFoundException;
import java.nio.file.StandardCopyOption;

/**
 * Loader class for the clang JNI system libraries. Code is strongly inspired by
 * https://github.com/adamheinrich/native-utils/blob/master/src/main/java/cz/adamh/utils/NativeUtils.java
 */
public class ClangJniLoader {

	private static boolean loaded = false;

	/**
	 * Ensures that the clang JNI native library is loaded. This throws
	 * {@link RuntimeException}, as this might need to be embedded in static blocks.
	 */
	public static synchronized void ensureLoaded() throws UnsatisfiedLinkError, RuntimeException {
		if (loaded) {
			return;
		}

		String libraryName = determineLibraryName();

		File tempDir = createTempDirectory();
		tempDir.deleteOnExit();
		File tempFile = new File(tempDir, libraryName);
		try {
			try (InputStream stream = ClangJniLoader.class.getResourceAsStream(libraryName)) {
				Files.copy(stream, tempFile.toPath(), StandardCopyOption.REPLACE_EXISTING);
			}
		} catch (IOException e) {
			tempFile.delete();
			throw new RuntimeException("Failed to copy library to temp folder: " + e.getMessage(), e);
		}

		try {
			System.load(tempFile.getAbsolutePath());
			prepareClangEnvironment();
		} finally {
			if (isPosixCompliant()) {
				// this is ok for POSIX
				tempFile.delete();
			} else {
				tempFile.deleteOnExit();
			}
		}
	}

	/** We need to set some environment variables to make Clang work as expected. */
	private static void prepareClangEnvironment() {
		// we need to disable crash recovery, as it causes the JVM to crash on exit
		Clang.putenv("LIBCLANG_DISABLE_CRASH_RECOVERY=1");
	}

	/** Determines the library name from the OS name. */
	private static String determineLibraryName() throws UnsatisfiedLinkError {
		String operatingSystem = System.getProperty("os.name").toLowerCase();
		String libraryName;
		if (operatingSystem.contains("mac")) {
			libraryName = "libclang-mac.dylib";
		} else if (operatingSystem.contains("win")) {
			libraryName = "libclang-win.dll";
		} else if (operatingSystem.contains("linux")) {
			libraryName = "libclang-linux.so";
		} else {
			throw new RuntimeException("No library available for OS " + operatingSystem);
		}
		return libraryName;
	}

	private static boolean isPosixCompliant() {
		try {
			return FileSystems.getDefault().supportedFileAttributeViews().contains("posix");
		} catch (FileSystemNotFoundException | ProviderNotFoundException | SecurityException e) {
			return false;
		}
	}

	private static File createTempDirectory() {
		String tempDir = System.getProperty("java.io.tmpdir");
		File generatedDir = new File(tempDir, "clang-jni-" + System.nanoTime());

		if (!generatedDir.mkdir()) {
			throw new RuntimeException("Failed to create temp directory " + generatedDir.getName());
		}
		return generatedDir;
	}
}
