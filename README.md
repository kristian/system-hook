![Java (low-level) System Hook](https://raw.github.com/kristian/system-hook/assets/system-hook-logo.png) [![Build Status](https://ci.appveyor.com/api/projects/status/github/kristian/system-hook?branch=master&svg=true)](https://ci.appveyor.com/project/kristian/system-hook)

Java (low-level) System Hook provides a very light-weight global keyboard and mouse listener for Java. Generally keyboard and mouse events in Java only work, if the registered component is in focus. For example, in case any window looses its focus (e.g. when minimized), it stops receiving any more keyboard or mouse events. Through a low-level system-wide hook the *global keyboard / mouse hook* is able to deliver those events regardless.

The *Java (low-level) System Hook* comes bundled with native libraries (for Windows 32 & 64 bit) to register the hooks via the Java Native Interface (JNI). The libraries are load dynamically depending on the version and architecture of your operating system. The libraries to track the keyboard and mouse events can be loaded and used separately.

Using the `GlobalKeyboardHook` class a `GlobalKeyListener` or the adapter class `GlobalKeyAdapter` can be registered to listen to `keyPressed` and `keyReleased` events like so:
```java
import java.util.Map.Entry;

import lc.kra.system.keyboard.GlobalKeyboardHook;
import lc.kra.system.keyboard.event.GlobalKeyAdapter;
import lc.kra.system.keyboard.event.GlobalKeyEvent;

public class GlobalKeyboardExample {
	private static boolean run = true;
	public static void main(String[] args) {
		// might throw a UnsatisfiedLinkError if the native library fails to load or a RuntimeException if hooking fails 
		GlobalKeyboardHook keyboardHook = new GlobalKeyboardHook(true); // use false here to switch to hook instead of raw input

		System.out.println("Global keyboard hook successfully started, press [escape] key to shutdown. Connected keyboards:");
		for(Entry<Long,String> keyboard:GlobalKeyboardHook.listKeyboards().entrySet())
			System.out.format("%d: %s\n", keyboard.getKey(), keyboard.getValue());
		
		keyboardHook.addKeyListener(new GlobalKeyAdapter() {
			@Override public void keyPressed(GlobalKeyEvent event) {
				System.out.println(event);
				if(event.getVirtualKeyCode()==GlobalKeyEvent.VK_ESCAPE)
					run = false;
			}
			@Override public void keyReleased(GlobalKeyEvent event) {
				System.out.println(event); }
		});
		
		try {
			while(run) Thread.sleep(128);
		} catch(InterruptedException e) { /* nothing to do here */ }
		  finally { keyboardHook.shutdownHook(); }
	}
}
```

Using the `GlobalMouseHook` class a `GlobalMouseListener` or the adapter class `GlobalMouseAdapter` can be registered to listen to `mousePressed`, `mouseReleased`, `mouseMoved` and `mouseWheel` events like so:
```java
import java.util.Map.Entry;

import lc.kra.system.mouse.GlobalMouseHook;
import lc.kra.system.mouse.event.GlobalMouseAdapter;
import lc.kra.system.mouse.event.GlobalMouseEvent;

public class GlobalMouseExample {
	private static boolean run = true;
	public static void main(String[] args) {
		// might throw a UnsatisfiedLinkError if the native library fails to load or a RuntimeException if hooking fails 
		GlobalMouseHook mouseHook = new GlobalMouseHook(); // add true to the constructor, to switch to raw input mode

		System.out.println("Global mouse hook successfully started, press [middle] mouse button to shutdown. Connected mice:");
		for(Entry<Long,String> mouse:GlobalMouseHook.listMice().entrySet())
			System.out.format("%d: %s\n", mouse.getKey(), mouse.getValue());
		
		mouseHook.addMouseListener(new GlobalMouseAdapter() {
			@Override public void mousePressed(GlobalMouseEvent event)  {
				System.out.println(event);
				if((event.getButtons()&GlobalMouseEvent.BUTTON_LEFT)!=GlobalMouseEvent.BUTTON_NO
				&& (event.getButtons()&GlobalMouseEvent.BUTTON_RIGHT)!=GlobalMouseEvent.BUTTON_NO)
					System.out.println("Both mouse buttons are currenlty pressed!");
				if(event.getButton()==GlobalMouseEvent.BUTTON_MIDDLE)
					run = false;
			}
			@Override public void mouseReleased(GlobalMouseEvent event)  {
				System.out.println(event); }
			@Override public void mouseMoved(GlobalMouseEvent event) {
				System.out.println(event); }
			@Override public void mouseWheel(GlobalMouseEvent event) {
				System.out.println(event); }
		});
		
		try {
			while(run) Thread.sleep(128);
		} catch(InterruptedException e) { /* nothing to do here */ }
		  finally { mouseHook.shutdownHook(); }
	}
}
```

Please find some background information about this framework [on my blog](http://kra.lc/blog/2016/02/java-global-system-hook/).

Usage
-----

Feel free to use the classes `lc.kra.system.keyboard.GobalKeyboardHook` and `lc.kra.system.mouse.GlobalMouseHook` by coping the `.java` files to your project. Alternatively check the [**releases section**](https://github.com/kristian/system-hook/releases) for pre-bundled Java archives (JAR).

The `LibraryLoader` will first attempt to load the native libraries from the `java.library.path` and fall back checking the archives `/lc/kra/system/lib` package, if no libraries where found.

### Maven Dependency
You can include `system-hook` from this GitHub repository by adding this dependency to your `pom.xml`:

```xml
<dependency>
  <groupId>lc.kra.system</groupId>
  <artifactId>system-hook</artifactId>
  <version>3.2</version>
</dependency>
```

Additionally you will have to add the following repository to your `pom.xml`:

```xml
<repositories>
  <repository>
    <id>system-hook-mvn-repo</id>
    <url>https://raw.github.com/kristian/system-hook/mvn-repo/</url>
    <snapshots>
      <enabled>true</enabled>
      <updatePolicy>always</updatePolicy>
    </snapshots>
  </repository>
</repositories>
```

Build
-----

To build `system-hook` on your machine, checkout the repository, `cd` into it, and call:
```
mvn clean install
```
(A `C99` compatible compiler / linker bundle is required to build the native libraries, see [MSYS2](http://sourceforge.net/projects/msys2/)) 

License
-------

The code is available under the terms of the [MIT License](http://opensource.org/licenses/MIT).