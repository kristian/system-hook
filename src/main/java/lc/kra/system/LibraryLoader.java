/**
 * Copyright (c) 2016 Kristian Kraljic
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
package lc.kra.system;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Locale;
import java.util.zip.CRC32;
import java.util.zip.Checksum;

public class LibraryLoader {
	private static final String LIBRARY_NAME = "systemhook";
	
	private static boolean libraryLoad;
	
	/**
	 * Tries to laod the library with the given name
	 * 
	 * @param name The name of the library to load
	 * @throws UnsatisfiedLinkError Thrown in case loading the library fails
	 */
	public static synchronized void loadLibrary() throws UnsatisfiedLinkError {
		if(libraryLoad) return; //library already loaded
		String libName = System.getProperty(LIBRARY_NAME+".lib.name", System.mapLibraryName(LIBRARY_NAME).replaceAll("\\.jnilib$", "\\.dylib")),
			libPath = System.getProperty(LIBRARY_NAME+".lib.path");
		
		try {
			if(libPath==null)
			     System.loadLibrary(libName);
			else System.load(new File(libPath, libName).getAbsolutePath());
			libraryLoad = true;
			return;
		} catch(UnsatisfiedLinkError e) { /* do nothing, try next */ }
		
		libName = System.mapLibraryName(LIBRARY_NAME+'-'+getOperatingSystemName()+'-'+getOperatingSystemArchitecture())
			.replaceAll("\\.jnilib$", "\\.dylib"); // for JDK < 1.7
		String libNameExtension = libName.substring(libName.lastIndexOf('.')),
			libResourcePath = LibraryLoader.class.getPackage().getName().replace('.', '/')+"/lib/"+libName;
		
		InputStream inputStream = null; OutputStream outputStream = null;
		try {
			if((inputStream=LibraryLoader.class.getClassLoader().getResourceAsStream(libResourcePath))==null)
				throw new FileNotFoundException("lib: "+libName+" not found in lib directory");
			File tempFile = File.createTempFile(LIBRARY_NAME+"-", libNameExtension);
			
			Checksum checksum = new CRC32();
			outputStream = new FileOutputStream(tempFile);
			int read; byte[] buffer = new byte[1024];
			while((read=inputStream.read(buffer))!=-1) {
				outputStream.write(buffer, 0, read);
				checksum.update(buffer, 0, read);
			}
			outputStream.close();
			
			File libFile = new File(tempFile.getParentFile(), LIBRARY_NAME+"+"+checksum.getValue()+libNameExtension);
			if(!libFile.exists())
			     tempFile.renameTo(libFile);
			else tempFile.delete();
			
			System.load(libFile.getAbsolutePath());
			libraryLoad = true;
		} catch(IOException e) {
			throw new UnsatisfiedLinkError(e.getMessage());
		} finally {
			if(inputStream!=null) {
				try { inputStream.close(); }
				catch (IOException e) { /* nothing to do here */ }
			}
			if(outputStream!=null) {
				try { outputStream.close(); }
				catch (IOException e) { /* nothing to do here */ }
			}
		}
	}
	
	private static String getOperatingSystemName() {
		String osName = System.getProperty("os.name").toLowerCase(Locale.ROOT);
		if(osName.startsWith("windows"))
			return "windows";
		else if(osName.startsWith("linux"))
			return "linux";
		else if(osName.startsWith("mac os"))
			return "darwin";
		else if(osName.startsWith("sunos")||osName.startsWith("solaris"))
			return "solaris";
		else return osName;
	}
	private static String getOperatingSystemArchitecture() {
		String osArch = System.getProperty("os.arch").toLowerCase(Locale.ROOT);
		if((osArch.startsWith("i")||osArch.startsWith("x"))&&osArch.endsWith("86"))
			return "x86";
		else if((osArch.equals("i86")||osArch.startsWith("amd"))&&osArch.endsWith("64"))
			return "amd64";
		else if(osArch.startsWith("arm"))
			return "arm";
		else if(osArch.startsWith("sparc"))
			return !osArch.endsWith("64")?"sparc":"sparc64";
		else if(osArch.startsWith("ppc"))
			return !osArch.endsWith("64")?"ppc":"ppc64";
		else return osArch;
	}
}