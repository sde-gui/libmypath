# Libmypath

A small library that helps your application to locate the application's data files.

Libmypath attempts to locate the executable file path of the running application. On Unix systems, there's no reliable way for an application to retrieve a path to its own executable image or any other application-related files. Usually paths are either hard-coded into the application binary or passed as command-line parameters.

There are several methods to locate the application path. Libmypath tries the following:

1. If procfs is mounted at /proc, libmypath reads the path from procfs.
2. Libmypath uses dladdr() to read the file name of the mapped executable image.
3. As the last resort, libmypath attemts to guess the path based on the value of *argv\[0\]* and the *PATH* environent variable.

# Usage

Libmypath is not intended for building as a shared object and installing to `{/,/usr,/usr/local}lib`. Just drop it into your application's source code and use it directly.

# Portability issues

* `procfs` is not in POSIX.
* `dladdr()` is not in POSIX.
* `argv[0]` can generally contain an arbitrary string and not an actual command name.

# Layouts of procfs on different systems

Linux:

    /proc/self -> \[pid\]
    /proc/\[pid\]/exe -> \[path\]

FreeBSD:

    /proc/curproc -> \[pid\]
    /proc/\[pid\]/file -> \[path\]

NetBSD:

    /proc/self -> curproc
    /proc/curproc -> \[pid\]
    /proc/\[pid\]/exe -> \[path\]

Libmypath tries all the mentioned procfs layouts in order and doesn't contain any compile-time or run-time OS-detection logic.

# dladdr() issues

1. dladdr() is not in POSIX.

2. A backward-compatible bug render dladdr() useless on FreeBSD:

> This implementation is bug-compatible with the Solaris implementation.
> In particular, the following bugs are present:

> If addr lies in the main executable rather than in a shared library,
> the pathname returned in dli_fname may not be correct.  The pathname
> is taken directly from argv[0] of the calling process.  When execut-
> ing a program specified by its full pathname, most shells set argv[0]
> to the pathname.  But this is not required of shells or guaranteed by
> the operating system.

3. "In dynamically linked programs, the address of a global function will point to its program linkage table entry, rather than to the entry point of the function itself. This causes most global functions to appear to be defined within the main executable, rather than in the shared libraries where the actual code resides."

The third issue does not affect Libmypath since Libmypath does not cover cases of shared libraries. But if you will implement path detection method for shared libraries (in fact, dladdr() is the only possible method for this), be aware. Pass a pointer to a global variable into dladdr(), not to a global function. A pointer to a variable actually contains an address within the data section of your shared library, while pointer to global function may point to some trampoline code.



