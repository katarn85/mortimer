/* skeleton based on version from Fedora Core 3 */

#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <dirent.h>
#include <stddef.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <elf.h>

#define verbose_exec(failcode, path...) \
  do							\
    {							\
      char *const arr[] = { path, NULL };		\
      vexec (failcode, arr);				\
    } while (0)

__attribute__((noinline)) void vexec (int failcode, char *const path[]);
__attribute__((noinline)) void says (const char *str);
__attribute__((noinline)) void sayn (long num);
__attribute__((noinline)) void message (char *const path[]);
__attribute__((noinline)) int check_elf (const char *name);

int
main (void)
{
  char initpath[256];
  struct stat root, init_root;

#ifdef REMOVE_TLS_DIRS
  const char *library[] = {"libc.so.6", "libc.so.6.1", "libm.so.6",
			   "libm.so.6.1", "librt.so.1", "librtkaio.so.1",
			   "libpthread.so.0", "libthread_db.so.1"};
  const char *remove_dir[] = {
#ifdef __i386__
	"/lib/i686/",
#endif
#ifdef __powerpc64__
#ifdef REMOVE_PPC_OPTIMIZE_POWER4
	"/lib64/power4/",
	"/lib64/ppc970/",
#endif
#ifdef REMOVE_PPC_OPTIMIZE_POWER5
	"/lib64/power5/",
	"/lib64/power5+/",
#endif
#ifdef REMOVE_PPC_OPTIMIZE_POWER6
	"/lib64/power6/",
	"/lib64/power6x/",
#endif
#ifdef REMOVE_PPC_OPTIMIZE_POWER7
	"/lib64/power7/",
#endif
#ifdef REMOVE_PPC_OPTIMIZE_CELL
	"/lib64/ppc-cell-be/",
#endif
#endif /* __powerpc64__ */
#ifdef __powerpc__
#ifdef REMOVE_PPC_OPTIMIZE_POWER4
	"/lib/power4/",
	"/lib/ppc970/",
#endif
#ifdef REMOVE_PPC_OPTIMIZE_POWER5
	"/lib/power5/",
	"/lib/power5+/",
#endif
#ifdef REMOVE_PPC_OPTIMIZE_POWER6
	"/lib/power6/",
	"/lib/power6x/",
#endif
#ifdef REMOVE_PPC_OPTIMIZE_POWER7
	"/lib/power7/",
#endif
#ifdef REMOVE_PPC_OPTIMIZE_CELL
	"/lib/ppc-cell-be/",
#endif
#endif /* __powerpc__ */
	LIBDIR"/tls/" };
  int i, j;

  for (i = 0; i < sizeof (remove_dir) / sizeof (remove_dir[0]); ++i)
    for (j = 0; j < sizeof (library) / sizeof (library[0]); j++)
      {
	char buf[strlen (remove_dir[i]) + strlen (library[j]) + 1];
	char readlink_buf[(strlen (remove_dir[i]) + strlen (library[j])) * 2 + 30];
	ssize_t len;
	char *cp;

	cp = stpcpy (buf, remove_dir[i]);
	strcpy (cp, library[j]);
	/* This file could be a symlink to library-%{version}.so, so check
	   this and don't remove only the link, but also the library itself. */
	cp = stpcpy (readlink_buf, remove_dir[i]);
	if ((len = readlink (buf, cp, (sizeof (readlink_buf)
				       - (cp - readlink_buf) - 1))) > 0)
	  {
	    cp[len] = '\0';
	    if (cp[0] != '/') cp = readlink_buf;
	    unlink (cp);
	  }
	unlink (buf);
      }
#endif

  /* If installing bi-arch glibc, rpm sometimes doesn't unpack all files
     before running one of the lib's %post scriptlet.  /sbin/ldconfig will
     then be run by the other arch's %post.  */
  if (access ("/sbin/ldconfig", X_OK) == 0)
    verbose_exec (110, "/sbin/ldconfig", "/sbin/ldconfig", "-X");

  if (utimes (GCONV_MODULES_DIR "/gconv-modules.cache", NULL) == 0)
    {
#ifndef ICONVCONFIG
#define ICONVCONFIG "/usr/sbin/iconvconfig"
#endif
      verbose_exec (113, ICONVCONFIG, "/usr/sbin/iconvconfig",
		    "-o", GCONV_MODULES_DIR"/gconv-modules.cache",
		    "--nostdlib", GCONV_MODULES_DIR);
    }

  /* Check if telinit is available and the init fifo as well.  */
  if (access ("/sbin/telinit", X_OK) || access ("/dev/initctl", F_OK))
    _exit (0);
  /* Check if we are not inside of some chroot, because we'd just
     timeout and leave /etc/initrunlvl.  */
  if (readlink ("/proc/1/exe", initpath, 256) <= 0 ||
      readlink ("/proc/1/root", initpath, 256) <= 0 ||
      stat ("/proc/1/root", &init_root) < 0 ||
      stat ("/.buildenv", &init_root) < 0 || /* XEN build */
      stat ("/", &root) < 0 ||
      init_root.st_dev != root.st_dev || init_root.st_ino != root.st_ino)
    _exit (0);

  if (check_elf ("/proc/1/exe"))
    verbose_exec (116, "/sbin/telinit", "/sbin/telinit", "u");

#if 0
  /* Check if we can safely condrestart sshd.  */
  if (access ("/sbin/service", X_OK) == 0
      && access ("/usr/sbin/sshd", X_OK) == 0
      && access ("/bin/bash", X_OK) == 0)
    {
      if (check_elf ("/usr/sbin/sshd"))
	verbose_exec (121, "/sbin/service", "/sbin/service", "sshd", "condrestart");
    }
#endif

  _exit(0);
}

void
vexec (int failcode, char *const path[])
{
  pid_t pid;
  int status, save_errno;

  pid = vfork ();
  if (pid == 0)
    {
      execv (path[0], path + 1);
      save_errno = errno;
      message (path);
      says (" exec failed with errno ");
      sayn (save_errno);
      says ("\n");
      _exit (failcode);
    }
  else if (pid < 0)
    {
      save_errno = errno;
      message (path);
      says (" fork failed with errno ");
      sayn (save_errno);
      says ("\n");
      _exit (failcode + 1);
    }
  if (waitpid (0, &status, 0) != pid || !WIFEXITED (status))
    {
      message (path);
      says (" child terminated abnormally\n");
      _exit (failcode + 2);
    }
  if (WEXITSTATUS (status))
    {
      message (path);
      says (" child exited with exit code ");
      sayn (WEXITSTATUS (status));
      says ("\n");
      _exit (WEXITSTATUS (status));
    }
}

void
says (const char *str)
{
  write (1, str, strlen (str));
}

void
sayn (long num)
{
  char string[sizeof (long) * 3 + 1];
  char *p = string + sizeof (string) - 1;

  *p = '\0';
  if (num == 0)
    *--p = '0';
  else
    while (num)
      {
	*--p = '0' + num % 10;
	num = num / 10;
      }

  says (p);
}

void
message (char *const path[])
{
  says ("/usr/sbin/glibc_post_upgrade: While trying to execute ");
  says (path[0]);
}

int
check_elf (const char *name)
{
  /* Play safe, if we can't open or read, assume it might be
     ELF for the current arch.  */
  int ret = 1;
  int fd = open (name, O_RDONLY);
  if (fd >= 0)
    {
      Elf32_Ehdr ehdr;
      if (read (fd, &ehdr, offsetof (Elf32_Ehdr, e_version))
	  == offsetof (Elf32_Ehdr, e_version))
	{
	  ret = 0;
	  if (ehdr.e_ident[EI_CLASS]
	      == (sizeof (long) == 8 ? ELFCLASS64 : ELFCLASS32))
	    {
#if defined __i386__
	      ret = ehdr.e_machine == EM_386;
#elif defined __x86_64__
	      ret = ehdr.e_machine == EM_X86_64;
#elif defined __ia64__
	      ret = ehdr.e_machine == EM_IA_64;
#elif defined __powerpc64__
	      ret = ehdr.e_machine == EM_PPC64;
#elif defined __powerpc__
	      ret = ehdr.e_machine == EM_PPC;
#elif defined __s390__ || defined __s390x__
	      ret = ehdr.e_machine == EM_S390;
#elif defined __x86_64__
	      ret = ehdr.e_machine == EM_X86_64;
#elif defined __sparc__
	      if (sizeof (long) == 8)
		ret = ehdr.e_machine == EM_SPARCV9;
	      else
		ret = (ehdr.e_machine == EM_SPARC
		       || ehdr.e_machine == EM_SPARC32PLUS);
#else
	      ret = 1;
#endif
	    }
	}
      close (fd);
    }
  return ret;
}

#ifdef SMALL_BINARY

int __libc_multiple_threads __attribute__((nocommon));
int __libc_enable_asynccancel (void) { return 0; }
void __libc_disable_asynccancel (int x) { }
void __libc_csu_init (void) { }
void __libc_csu_fini (void) { }
pid_t __fork (void) { return -1; }
char thr_buf[65536];

#ifndef __powerpc__
int
__libc_start_main (int (*main) (void), int argc, char **argv,
		   void (*init) (void), void (*fini) (void),
		   void (*rtld_fini) (void), void * stack_end)
#else
struct startup_info
{
  void *sda_base;
  int (*main) (int, char **, char **, void *);
  int (*init) (int, char **, char **, void *);
  void (*fini) (void);
};

int
__libc_start_main (int argc, char **ubp_av, char **ubp_ev,
		   void *auxvec, void (*rtld_fini) (void),
		   struct startup_info *stinfo,
		   char **stack_on_entry)
#endif
{
#if defined __ia64__ || defined __powerpc64__
  register void *r13 __asm ("r13") = thr_buf + 32768;
  __asm ("" : : "r" (r13));
#elif defined __sparc__
  register void *g6 __asm ("g6") = thr_buf + 32768;
# ifdef __arch64__
  __thread_self = thr_buf + 32768;
# else
  register void *__thread_self __asm ("g7") = thr_buf + 32768;
# endif
  __asm ("" : : "r" (g6), "r" (__thread_self));
#elif defined __s390__ && !defined __s390x__
  __asm ("sar %%a0,%0" : : "d" (thr_buf + 32768));
#elif defined __s390x__
  __asm ("sar %%a1,%0; srlg 0,%0,32; sar %%a0,0" : : "d" (thr_buf + 32768) : "0");
#elif defined __powerpc__ && !defined __powerpc64__
  register void *r2 __asm ("r2") = thr_buf + 32768;
  __asm ("" : : "r" (r2));
#endif
  main();
  return 0;
}

#endif
