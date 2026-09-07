/* Minimal config.h for talloc 2.4.2 on Android/Bionic (aarch64, API 26+).
 * Hand-written for the Winland proot static build (waf configure is not
 * usable in this cross environment). Only macros consumed by talloc.c /
 * lib/replace/replace.h are defined; everything is present in Bionic. */

#ifndef TALLOC_ANDROID_CONFIG_H
#define TALLOC_ANDROID_CONFIG_H 1

/* Compiler / language features */
#define HAVE_CONSTRUCTOR_ATTRIBUTE 1
#define HAVE_FUNCTION_MACRO 1
#define HAVE_VA_COPY 1
#define HAVE___VA_COPY 1
#define HAVE_C99_VSNPRINTF 1
#define HAVE_FALLTHROUGH_ATTRIBUTE 1
#define HAVE_TYPEOF 1

/* Integer / stdint */
#define HAVE_INTTYPES_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDBOOL_H 1
#define HAVE_BOOL 1
#define HAVE_INTPTR_T 1
#define HAVE_UINTPTR_T 1
#define HAVE_PTRDIFF_T 1
#define HAVE_SSIZE_T 1

/* Standard C library presence */
#define HAVE_STDIO_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_STRINGS_H 1
#define HAVE_UNISTD_H 1
#define HAVE_STDARG_H 1
#define HAVE_STDDEF_H 1
#define HAVE_CTYPE_H 1
#define HAVE_ERRNO_H 1
#define HAVE_LIMITS_H 1
#define HAVE_TIME_H 1
#define HAVE_MEMORY_H 1
#define HAVE_MALLOC_H 1

/* Sys headers present in Bionic */
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_MMAN_H 1
#define HAVE_SYS_PARAM_H 1
#define HAVE_SYS_SOCKET_H 1
#define HAVE_SYS_WAIT_H 1
#define HAVE_SYS_AUXV_H 1
#define HAVE_SYS_RESOURCE_H 1
#define HAVE_NETINET_IN_H 1
#define HAVE_ARPA_INET_H 1
#define HAVE_NETDB_H 1
#define HAVE_FCNTL_H 1
#define HAVE_DIRENT_H 1
#define HAVE_GRP_H 1
#define HAVE_PWD_H 1
#define HAVE_DLFCN_H 1
#define HAVE_PTHREAD_H 1
#define HAVE_TERMIOS_H 1
#define HAVE_SYSLOG_H 1
#define HAVE_MMAP 1

/* String / stdio functions (all in Bionic) */
#define HAVE_STRDUP 1
#define HAVE_STRNDUP 1
#define HAVE_STRNLEN 1
#define HAVE_SNPRINTF 1
#define HAVE_VSNPRINTF 1
#define HAVE_ASPRINTF 1
#define HAVE_VASPRINTF 1
#define HAVE_DPRINTF 1
#define HAVE_STRLCPY 1
#define HAVE_STRLCAT 1
#define HAVE_MEMMOVE 1
#define HAVE_MEMCPY 1
#define HAVE_MEMSET 1
#define HAVE_MEMCMP 1
#define HAVE_MEMMEM 1
#define HAVE_GETLINE 1
#define HAVE_REALPATH 1
#define HAVE_STRSEP 1
#define HAVE_STRTOK_R 1
#define HAVE_STRTOLL 1
#define HAVE_STRTOULL 1
#define HAVE_STRTOLD 1

/* Unix / process functions (all in Bionic) */
#define HAVE_FORK 1
#define HAVE_WAITPID 1
#define HAVE_GETPID 1
#define HAVE_GETPPID 1
#define HAVE_GETUID 1
#define HAVE_GETEUID 1
#define HAVE_GETGID 1
#define HAVE_GETEGID 1
#define HAVE_GETGROUPS 1
#define HAVE_CHOWN 1
#define HAVE_CHMOD 1
#define HAVE_UMASK 1
#define HAVE_MKDIR 1
#define HAVE_SYMLINK 1
#define HAVE_READLINK 1
#define HAVE_STAT 1
#define HAVE_LSTAT 1
#define HAVE_FSTAT 1
#define HAVE_OPEN 1
#define HAVE_DUP 1
#define HAVE_DUP2 1
#define HAVE_PIPE 1
#define HAVE_MKSTEMP 1
#define HAVE_SETENV 1
#define HAVE_UNSETENV 1
#define HAVE_PUTENV 1
#define HAVE_CLOCK_GETTIME 1
#define HAVE_USLEEP 1
#define HAVE_GETTIMEOFDAY 1
#define HAVE_NANOSLEEP 1
#define HAVE_POLL 1
#define HAVE_SELECT 1
#define HAVE_SOCKET 1
#define HAVE_CONNECT 1
#define HAVE_SETSOCKOPT 1
#define HAVE_GETSOCKOPT 1
#define HAVE_BIND 1
#define HAVE_LISTEN 1
#define HAVE_GETADDRINFO 1
#define HAVE_FREEADDRINFO 1
#define HAVE_GAI_STRERROR 1
#define HAVE_GETHOSTBYNAME 1
#define HAVE_GETIFADDRS 1
#define HAVE_FREEIFADDRS 1
#define HAVE_DLOPEN 1
#define HAVE_DLSYM 1
#define HAVE_DLERROR 1
#define HAVE_DLCLOSE 1
#define HAVE_MMAP 1
#define HAVE_MUNMAP 1
#define HAVE_MREMAP 1
#define HAVE_GETAUXVAL 1
#define HAVE_GETPAGESIZE 1
#define HAVE_SYSCONF 1
#define HAVE_UTIMES 1
#define HAVE_FTRUNCATE 1
#define HAVE_FCNTL 1
#define HAVE_FLOCK 1
#define HAVE_PREAD 1
#define HAVE_PWRITE 1
#define HAVE_READ 1
#define HAVE_WRITE 1
#define HAVE_LSEEK 1
#define HAVE_CLOSE 1
#define HAVE_UNLINK 1
#define HAVE_RMDIR 1
#define HAVE_RENAME 1
#define HAVE_LINK 1
#define HAVE_ACCESS 1
#define HAVE_GETCWD 1
#define HAVE_CHDIR 1
#define HAVE_CHROOT 1
#define HAVE_UMASK 1
#define HAVE_EXECVE 1

/* Do NOT define: HAVE_CRYPT_H, HAVE_SHADOW_H, HAVE_SETPROCTITLE*,
 * HAVE_BSD_*, HAVE_UNIX_H, HAVE_SYS_ATOMIC_H, HAVE_VALGRIND_*,
 * HAVE_PRAGMA_INIT (constructor attribute is used instead). */

#endif /* TALLOC_ANDROID_CONFIG_H */
