

#ifndef LIBMYPATH_MYPATH_H_
#define LIBMYPATH_MYPATH_H_

#define MY_PATH_VERSION_MAJOR 0
#define MY_PATH_VERSION_MINOR 1
#define MY_PATH_VERSION_MICRO 0

#define MY_PATH_ALLOW_ROOT     (1 << 0)

/*
	Detects and returns a path to the executable image of the apllication or NULL if the detection failed.
	Returns NULL when the process is running with root UID/EUID.
	If you do insist on using relocatable application paths for root processes in your application, pass
	MY_PATH_ALLOW_ROOT in flags.
*/
const char * mypath_get_application_path(const char * argv0, unsigned int flags);

#endif
