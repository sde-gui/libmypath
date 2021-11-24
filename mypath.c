#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#if !defined( MYPATH_DISABLE_DLADDR)
#define _GNU_SOURCE
#include <dlfcn.h>
#endif

#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "mypath.h"

/*****************************************************************************/

static void strcat_separator(char * str, char separator)
{
	size_t l = strlen(str);
	if (str[l - 1] != separator)
	{
		str[l] = separator;
		str[l + 1] = 0;
	}
}

/*****************************************************************************/

static char * getcwd_alloc(void)
{
	char * buf;
	ssize_t size = 64;

again:
	buf = calloc(size, 1);

	if (getcwd(buf, size - 1))
	{
		size_t l = strlen(buf);
		if (l == 0)
		{
			free(buf);
			return NULL;
		}

		return buf;
	}

	free(buf);

	if (errno != ERANGE)
		return NULL;

	size *= 2;
	goto again;
}

/*****************************************************************************/

/*

POSIX:
> If the buf argument is not large enough to contain the link content,
> the first bufsize bytes shall be placed in buf.
> Upon successful completion, readlink() shall return the count of bytes
> placed in the buffer.

Arrggh! To make sure a link read completely, one has to readlink() it twice
into buffers of different size and compare results.

*/

static char * readlink_alloc(const char * path)
{
	ssize_t len1, len2;
	ssize_t size = 64;
	char * buf1 = NULL;
	char * buf2 = NULL;

	buf1 = calloc(1, size);
	if (!buf1)
		goto failure;

	len1 = readlink(path, buf1, size - 2);
	if (len1 < 1)
		goto failure;

again:

	buf2 = calloc(1, size * 2);
	if (!buf2)
		goto failure;

	len2 = readlink(path, buf2, size * 2 - 2);
	if (len2 < 1)
		goto failure;

	if (len1 == len2 && strcmp(buf1, buf2) == 0)
	{
		free(buf2);
		return buf1;
	}

	free(buf1);
	buf1 = buf2;
	len1 = len2;
	size *= 2;

	goto again;

failure:
	free(buf1);
	free(buf2);
	return NULL;
}

/*****************************************************************************/

#define BUF_SIZE 64

static char * get_from_procfs_linux(void)
{
	char buf[BUF_SIZE];
	char * result = NULL;
	struct stat st;

	if (readlink("/proc/self", buf, BUF_SIZE) < 1)
		goto end;

	buf[BUF_SIZE-1] = 0;

	if (buf[0] <= '0' || buf[9] > '9')
		goto end;

	if (atol(buf) != getpid())
		goto end;

	if (lstat("/proc/self/exe", &st) < 0)
		goto end;

	if (!S_ISLNK(st.st_mode))
		goto end;

	result = readlink_alloc("/proc/self/exe");

end:
	return result;
}

static char * get_from_procfs_freebsd(void)
{
	char buf[BUF_SIZE];
	char * result = NULL;
	struct stat st;

	if (readlink("/proc/curproc", buf, BUF_SIZE) < 1)
		goto end;

	buf[BUF_SIZE-1] = 0;

	if (buf[0] <= '0' || buf[9] > '9')
		goto end;

	if (atol(buf) != getpid())
		goto end;

	if (lstat("/proc/curproc/file", &st) < 0)
		goto end;

	if (!S_ISLNK(st.st_mode))
		goto end;

	result = readlink_alloc("/proc/curproc/file");

end:
	return result;
}

static char * get_from_procfs_netbsd(void)
{
	char buf[BUF_SIZE];
	char * result = NULL;
	struct stat st;

	if (readlink("/proc/self", buf, BUF_SIZE) < 1)
		goto end;

	buf[BUF_SIZE-1] = 0;

	if (strcmp(buf, "curproc") != 0)
		goto end;

	if (readlink("/proc/curproc", buf, BUF_SIZE) < 1)
		goto end;

	buf[BUF_SIZE-1] = 0;

	if (buf[0] <= '0' || buf[9] > '9')
		goto end;

	if (atol(buf) != getpid())
		goto end;

	if (lstat("/proc/curproc/exe", &st) < 0)
		goto end;

	if (!S_ISLNK(st.st_mode))
		goto end;

	result = readlink_alloc("/proc/curproc/exe");

end:
	return result;
}


#undef BUF_SIZE

static char * get_from_procfs(void)
{
	char * result = NULL;
	struct stat st;

	if (stat("/proc/", &st) < 0)
		goto end;

	if (!S_ISDIR(st.st_mode) || st.st_uid != 0)
		goto end;

	result = get_from_procfs_linux();
	if (!result)
		result = get_from_procfs_freebsd();
	if (!result)
		result = get_from_procfs_netbsd();

end:
	return result;
}

/*****************************************************************************/

static char * get_from_path_cwd(const char * argv0)
{
	char * result = NULL;
	char * cwd = getcwd_alloc();
	size_t size;

	if (!cwd)
		goto end;

	size = strlen(cwd) + strlen(argv0) + 2;
	result = calloc(size, 1);
	if (!result)
		goto end;

	strcpy(result, cwd);
	strcat_separator(result, '/');
	if (argv0[0] == '.' && argv0[1] == '/')
		strcat(result, argv0 + 2);
	else
		strcat(result, argv0);

end:
	free(cwd);
	return result;
}

static char * get_from_path_check_dir(const char * dir, const char * argv0)
{
	size_t size;
	struct stat st;
	char * result = NULL;

	size = strlen(dir) + strlen(argv0) + 2;
	result = calloc(size, 1);
	if (!result)
		return NULL;

	strcpy(result, dir);
	strcat_separator(result, '/');
	strcat(result, dir);

	if (stat(result, &st) < 0)
		goto failed;

	if (!S_ISREG(st.st_mode))
		goto failed;

	return result;

failed:
	free(result);
	return NULL;
}

static char * get_from_path_scan(const char * argv0)
{
	char * result = NULL;
	char * envPATH = getenv("PATH");

	if (!envPATH)
		return NULL;

	envPATH = strdup(envPATH);
	if (!envPATH)
		return NULL;

	char * p_from = envPATH;
	while (1)
	{
		char * p_to = strchr(p_from, ':');
		if (p_to == p_from)
		{
			p_from++;
			continue;
		}

		if (p_to == 0)
		{
			result = get_from_path_check_dir(p_from, argv0);
			goto end;
		}

		*p_to = 0;
		result = get_from_path_check_dir(p_from, argv0);
		if (result)
			goto end;

		p_from = p_to + 1;
	}

end:
	free(envPATH);
	return result;
}

static char * get_from_path(const char * argv0)
{
	char * result = NULL;

	if (!argv0 || !*argv0)
		return NULL;

	if (argv0[0] == '/')
		result = strdup(argv0);
	else if (strchr(argv0, '/') != NULL)
		result = get_from_path_cwd(argv0);
	else
		result = get_from_path_scan(argv0);

	if (result)
	{
		/*
			realpath() is broken by design in POSIX.1-2001.
			We need POSIX.1-2008 here.
			http://pubs.opengroup.org/onlinepubs/009695399/functions/realpath.html
			http://pubs.opengroup.org/onlinepubs/9699919799/functions/realpath.html
			
		*/
		char * resolved_result = realpath(result, NULL);
		free(result);
		result = resolved_result;
	}

	return result;
}

/*****************************************************************************/

#if !defined(MYPATH_DISABLE_DLADDR)

/*

FreeBSD: man 3 dladdr

> This implementation is bug-compatible with the Solaris implementation.
> In particular, the following bugs are present:

> If addr lies in the main executable rather than in a shared library,
> the pathname returned in dli_fname may not be correct.  The pathname
> is taken directly from argv[0] of the calling process.  When execut-
> ing a program specified by its full pathname, most shells set argv[0]
> to the pathname.  But this is not required of shells or guaranteed by
> the operating system.

Unsure if other systems behave the same or not.

*/

static char * get_from_dladdr()
{
	extern int main();
	Dl_info info;

	if (dladdr(main, &info) != 0 && info.dli_fname)
	{
		return get_from_path(info.dli_fname);
	}
	return NULL;
}

#else

static char * get_from_dladdr()
{
	return NULL;
}

#endif

/*****************************************************************************/

static int application_path_initialized = 0;
static char * application_path = NULL;

const char * mypath_get_application_path(const char * argv0, unsigned int flags)
{
	if (!(flags & MY_PATH_ALLOW_ROOT)) {
		if (getuid() == 0 || geteuid() == 0)
			return NULL;
	}

	if (!application_path_initialized)
	{
		application_path = get_from_procfs();
		if (!application_path)
			application_path = get_from_dladdr();
		if (!application_path)
			application_path = get_from_path(argv0);
		application_path_initialized = 1;
	}

	return application_path;
}

/*****************************************************************************/

#ifdef MYPATH_BUILD_TEST_APP

#include <stdio.h>

int main(int argc, char **argv)
{
	char * path_from_procfs = get_from_procfs();
	char * path_from_dladdr = get_from_dladdr();
	char * path_from_argv0  = get_from_path(argv[0]);

	printf("Path detected with procfs  method: %s\n", path_from_procfs ? path_from_procfs : "(NULL)");
	printf("Path detected with dladdr  method: %s\n", path_from_dladdr ? path_from_dladdr : "(NULL)");
	printf("Path detected with argv[0] method: %s\n", path_from_argv0 ? path_from_argv0 : "(NULL)");

	free(path_from_procfs);
	free(path_from_dladdr);
	free(path_from_argv0);

	return 0;
}

#endif

/*****************************************************************************/
