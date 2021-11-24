# Libmypath v0.1.0

A small library that helps your application to locate the application's data files.

Libmypath attempts to locate the executable file path of the running application. On Unix systems, there's no reliable way for an application to retrieve a path to its own executable image or any other application-related files. Usually paths are either hard-coded into the application binary or passed as command-line parameters.

There are several methods to locate the application path. Libmypath tries the following:

1. If procfs is mounted at /proc, libmypath reads the path from procfs.
2. Libmypath uses dladdr() to read the file name of the mapped executable image.
3. As the last resort, libmypath attemts to guess the path based on the value of *argv\[0\]* and the *PATH* environent variable.

# Supported operating systems and environments

The library targets:

* Linux
* FreeBSD
* NetBSD

May work for other Unix-like systems as well.

More systems will probably be tested in the future.

# Usage

Libmypath is not intended for building as a shared object and installing into `{/,/usr,/usr/local}lib`. Just drop it into your application's source code and use it directly.

On Linux, provide `-ldl` option for the linker in order to make `dladdr()` symbol available. Refer to OS-specific manuals on other systems.

When linking the application statically or on systems where `dladdr()` is unavailable, `#define MYPATH_DISABLE_DLADDR` to disable linking to that symbol. (Normally the option looks like `-DMYPATH_DISABLE_DLADDR` for gcc, clang or another gcc-compatible compiler.)

# Test app

See `build_test_app.sh`. Edit it, if necessary, to make the app building under your OS. Build the app and make sure the detection logic works as expected. Report issues, if any.

# Portability issues

* `procfs` is not in POSIX.
* `dladdr()` is not in POSIX.
* `argv[0]` can generally contain an arbitrary string and not an actual command name.

# Layouts of procfs on different systems

Linux:

```
    /proc/self -> [pid]
    /proc/[pid]/exe -> [path]
```

FreeBSD:

```
    /proc/curproc -> [pid]
    /proc/[pid]/file -> [path]
```

NetBSD:

```
    /proc/self -> curproc
    /proc/curproc -> [pid]
    /proc/[pid]/exe -> [path]
```

Libmypath tries all the mentioned procfs layouts in order and doesn't apply any compile-time or run-time OS-detection logic.

# dladdr() issues

1. `man 3 dladdr` under FreeBSD reports:

> This implementation is bug-compatible with the Solaris implementation.
> In particular, the following bugs are present:

> If addr lies in the main executable rather than in a shared library,
> the pathname returned in dli_fname may not be correct.  The pathname
> is taken directly from argv\[0\] of the calling process.  When execut-
> ing a program specified by its full pathname, most shells set argv\[0\]
> to the pathname.  But this is not required of shells or guaranteed by
> the operating system.

I've not tested if other systems behave the same or not. If they do, `dladdr()` is just a verbose way to say `argv[0]`, and `argv[0]` isn't guaranteed to containg a valid command name. If they don't, `dladdr()` is more reliable way to get the path. Anyway, there MAY be some systems now or in the future that return the real path on `dladdr()`.

2. "In dynamically linked programs, the address of a global function will point to its program linkage table entry, rather than to the entry point of the function itself. This causes most global functions to appear to be defined within the main executable, rather than in the shared libraries where the actual code resides."

The third issue does not affect Libmypath since Libmypath does not cover cases of shared libraries. But if you will implement path detection method for shared libraries (in fact, dladdr() is the only possible method for this), be aware. Pass a pointer to a global variable into dladdr(), not to a global function. A pointer to a variable actually contains an address within the data section of your shared library, while pointer to global function may point to some trampoline code.

