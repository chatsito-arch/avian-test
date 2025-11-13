/* Minimal Vita oriented shims that cover the tiny subset of POSIX calls
 * required by the interpreter-only Avian runtime.  The Vita SDK already
 * ships a newlib based libc, but a handful of Unix entry points are missing.
 * These definitions keep the Avian build from pulling in mprotect/mmap or
 * signal heavy code paths. */

#include <errno.h>
#include <sched.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifdef __vita__
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#endif

#ifndef MAP_PRIVATE
#define MAP_PRIVATE 0
#endif

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS 0
#endif

#ifndef PROT_READ
#define PROT_READ 0x1
#endif

#ifndef PROT_WRITE
#define PROT_WRITE 0x2
#endif

#ifndef PROT_EXEC
#define PROT_EXEC 0x4
#endif

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 0
#endif

void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset)
{
  (void)addr;
  (void)prot;
  (void)flags;
  (void)fd;
  (void)offset;
  void* ptr = malloc(length);
  if (!ptr) {
    errno = ENOMEM;
    return (void*)-1;
  }
  memset(ptr, 0, length);
  return ptr;
}

int munmap(void* addr, size_t length)
{
  (void)length;
  free(addr);
  return 0;
}

int mprotect(void* addr, size_t len, int prot)
{
  (void)addr;
  (void)len;
  (void)prot;
  return 0;
}

int sched_yield(void)
{
#ifdef __vita__
  sceKernelDelayThread(0);
#else
  usleep(0);
#endif
  return 0;
}

int clock_gettime(clockid_t clk_id, struct timespec* tp)
{
  (void)clk_id;
  struct timeval tv;
  if (gettimeofday(&tv, 0) != 0) {
    return -1;
  }
  tp->tv_sec = tv.tv_sec;
  tp->tv_nsec = tv.tv_usec * 1000;
  return 0;
}

int nanosleep(const struct timespec* rqtp, struct timespec* rmtp)
{
  (void)rmtp;
  const useconds_t usec = (useconds_t)(rqtp->tv_sec * 1000000
                                       + rqtp->tv_nsec / 1000);
  usleep(usec);
  return 0;
}

