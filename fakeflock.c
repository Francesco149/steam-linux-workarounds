/*
 * works around flock issues and disk space overflow when running steam or
 * other programs on zfs over nfs and other similar setups
 *
 * sudo gcc -shared flock_to_setlk.c -ldl -o /usr/lib/libfakeflock.so
 * sudo gcc -m32 -shared flock_to_setlk.c -ldl -o /usr/lib32/libfakeflock.so
 * LD_PRELOAD="/usr/lib/libfakeflock.so /usr/lib32/libfakeflock.so" steam
 *
 * also works with uplay and other things running under wine
 *
 * this is free and unencumbered software released into the public domain,
 * see the attached UNLICENSE or https://unlicense.org/
 */

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <dlfcn.h>
#include <sys/statvfs.h>
#include <sys/statfs.h>
#include <stdint.h>

/*
 * fixes programs that try to flock over nfs such as steam
 * https://github.com/ValveSoftware/steam-for-linux/issues/5788
 * credits to https://gist.github.com/DataBeaver/0aa46844c8e1788207fc882fc2a221f6
 */

int flock(int fd, int operation) {
  char *op_str = 0, *req_str = 0, *type_str = 0, *mode_str = 0;
  int nonblock, open_mode, req, err;
  struct flock lock;

  /* translate flock to fcntl */
  nonblock = operation & LOCK_NB;
  operation &= ~LOCK_NB;
  switch (operation) {
   case LOCK_SH: lock.l_type = F_RDLCK; break;
    case LOCK_EX: lock.l_type = F_WRLCK; break;
    case LOCK_UN: lock.l_type = F_UNLCK; break;
    default:
      errno = EINVAL;
      return -1;
  }
  lock.l_whence = SEEK_SET;
  lock.l_start = lock.l_len = lock.l_pid = 0;
  open_mode = fcntl(fd, F_GETFL, NULL) & O_ACCMODE;
  if (lock.l_type == F_RDLCK && open_mode == O_WRONLY) {
    lock.l_type = F_WRLCK;
  } else if (lock.l_type==F_WRLCK && open_mode==O_RDONLY) {
    lock.l_type = F_RDLCK;
  }
  req = (nonblock ? F_OFD_SETLK : F_OFD_SETLKW);

  /* print debug info */
  #define c(x, y) case y: x= #y; break;
  switch (operation) {
    c(op_str, LOCK_SH) c(op_str, LOCK_EX) c(op_str, LOCK_UN)
  }
  switch (req) {
    c(req_str, F_OFD_SETLK) c(req_str, F_OFD_SETLKW)
  }
  switch (lock.l_type) {
    c(type_str, F_RDLCK) c(type_str, F_WRLCK) c(type_str, F_UNLCK)
  }
  switch (open_mode) {
    c(mode_str, O_RDONLY) c(mode_str, O_WRONLY) c(mode_str, O_RDWR)
  }
  #undef c
  printf("Translated flock(%d, %s%s) with open mode %s to "
    "fcntl(%d, %s, { .l_type = %s })\n", fd, op_str,
    (nonblock ? "|LOCK_NB" : ""), mode_str, fd, req_str, type_str);

  /* execute translated call */
  err = fcntl(fd, req, &lock);
  if (err<0) {
    err = errno;
    perror("fcntl");
    if (nonblock && (err == EAGAIN || err == EACCES)) {
      errno = EWOULDBLOCK;
    }
    return -1;
  }
  return 0;
}

/*
 * fix 32-bit programs seeing no free space for >2TB
 *
 * block size is hardcoded to 4096 because sometimes the stat calls return
 * invalid block sizes for some reason
 *
 * credits:
 * ryao: https://github.com/ValveSoftware/steam-for-linux/issues/3226#issuecomment-422869718
 * nonchip: https://github.com/nonchip/steam_workaround_fsoverflow
 */

#define LIMITSIZE \
  int ret = f(arg, buf); \
  if (errno != 75 && ret) { \
    perror(__func__); \
    return ret; \
  } \
  buf->f_frsize = 4096; \
  buf->f_bfree = buf->f_bavail = buf->f_blocks = \
    ((int64_t)INT_MAX << 9) / buf->f_frsize; \
  printf("Translated %s call to report " \
    "%d,%d/%d * %d\n", __func__, \
    (int)buf->f_bfree, (int)buf->f_bavail, (int)buf->f_blocks, \
    (int)buf->f_frsize); \
  errno = 0; \
  return 0;

int statvfs(char const* arg, struct statvfs* buf) {
  int (*f)(char const*, struct statvfs*) = dlsym(RTLD_NEXT, __func__);
  LIMITSIZE
}

int statvfs64(char const* arg, struct statvfs64* buf) {
  int (*f)(char const*, struct statvfs64*) = dlsym(RTLD_NEXT, __func__);
  LIMITSIZE
}

int fstatvfs64(int arg, struct statvfs64* buf) {
  int (*f)(int, struct statvfs64*) = dlsym(RTLD_NEXT, __func__);
  LIMITSIZE
}

int statfs(char const* arg, struct statfs* buf) {
  int (*f)(char const*, struct statfs*) = dlsym(RTLD_NEXT, __func__);
  LIMITSIZE
}

int statfs64(char const* arg, struct statfs64* buf) {
  int (*f)(char const*, struct statfs64*) = dlsym(RTLD_NEXT, __func__);
  LIMITSIZE
}

int fstatfs64(int arg, struct statfs64* buf) {
  int (*f)(int, struct statfs64*) = dlsym(RTLD_NEXT, __func__);
  LIMITSIZE
}
