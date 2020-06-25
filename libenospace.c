/* Copyright (c) 2019-2020, Michael Santos <michael.santos@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <fcntl.h>

#include <sys/vfs.h>

#include <linux/magic.h>

void _init(void);

ssize_t (*sys_write)(int fd, const void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);

ssize_t (*sys_writev)(int fd, const struct iovec *iov, int iovcnt);
ssize_t writev(int fd, const struct iovec *iov, int iovcnt);

ssize_t (*sys_pwrite)(int fd, const void *buf, size_t count, off_t offset);
ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset);

ssize_t (*sys_pwritev)(int fd, const struct iovec *iov, int iovcnt,
                       off_t offset);
ssize_t pwritev(int fd, const struct iovec *iov, int iovcnt, off_t offset);

int quota(int fd);

enum {
  LIBENOSPACE_OPT_BYTES = 1,
  LIBENOSPACE_OPT_PERCENT = 2,
  LIBENOSPACE_OPT_MAX = 3,
};

size_t avail = (size_t)-1;
int opt = LIBENOSPACE_OPT_PERCENT;
int libenospace_errno = ENOSPC;
char *debug;

void _init(void) {
  const char *err;
  char *env_avail;
  char *env_opt;
  char *env_errno;
  char *endptr;

  debug = getenv("LIBENOSPACE_DEBUG");

  env_avail = getenv("LIBENOSPACE_AVAIL");

  if (env_avail != NULL) {
    errno = 0;
    avail = (size_t)strtoul(env_avail, &endptr, 10);

    if ((endptr == env_avail) || *endptr != '\0') {
      errno = EINVAL;
    }

    if (errno != 0) {
      if (debug)
        (void)fprintf(stderr, "libenospace:avail:%s:%s\n", env_avail,
                      strerror(errno));

      _exit(111);
    }
  }

  env_errno = getenv("LIBENOSPACE_ERRNO");

  if (env_errno != NULL) {
    errno = 0;
    libenospace_errno = (int)strtol(env_errno, &endptr, 10);

    if ((endptr == env_errno) || *endptr != '\0') {
      errno = EINVAL;
    }

    if (errno != 0) {
      if (debug)
        (void)fprintf(stderr, "libenospace:errno:%s:%s\n", env_errno,
                      strerror(errno));

      _exit(111);
    }
  }

  env_opt = getenv("LIBENOSPACE_OPT");

  if (env_opt != NULL) {
    if (!strcmp(env_opt, "bytes")) {
      opt = LIBENOSPACE_OPT_BYTES;
    } else if (!strcmp(env_opt, "percent")) {
      opt = LIBENOSPACE_OPT_PERCENT;
    } else {
      (void)fprintf(stderr, "libenospace:invalid option:%s\n", env_opt);
      opt = LIBENOSPACE_OPT_PERCENT;
    }
  }

  if (opt == LIBENOSPACE_OPT_PERCENT && avail > 100)
    avail = 100;

#pragma GCC diagnostic ignored "-Wpedantic"
  sys_write = dlsym(RTLD_NEXT, "write");
#pragma GCC diagnostic warning "-Wpedantic"
  err = dlerror();

  if (err != NULL)
    (void)fprintf(stderr, "libenospace:dlsym (write):%s\n", err);

#pragma GCC diagnostic ignored "-Wpedantic"
  sys_writev = dlsym(RTLD_NEXT, "writev");
#pragma GCC diagnostic warning "-Wpedantic"
  err = dlerror();

  if (err != NULL)
    (void)fprintf(stderr, "libenospace:dlsym (writev):%s\n", err);

#pragma GCC diagnostic ignored "-Wpedantic"
  sys_pwrite = dlsym(RTLD_NEXT, "pwrite");
#pragma GCC diagnostic warning "-Wpedantic"
  err = dlerror();

  if (err != NULL)
    (void)fprintf(stderr, "libenospace:dlsym (pwrite):%s\n", err);

#pragma GCC diagnostic ignored "-Wpedantic"
  sys_pwritev = dlsym(RTLD_NEXT, "pwritev");
#pragma GCC diagnostic warning "-Wpedantic"
  err = dlerror();

  if (err != NULL)
    (void)fprintf(stderr, "libenospace:dlsym (pwritev):%s\n", err);
}

int quota(int fd) {
  int oerrno = errno;
  struct stat st = {0};
  struct statfs fs = {0};

  if (avail == 0)
    return 0;

  if (fstat(fd, &st) < 0)
    return -1;

  if (!S_ISREG(st.st_mode))
    return 0;

  if (fstatfs(fd, &fs) < 0)
    return -1;

  switch (fs.f_type) {
  case EXT4_SUPER_MAGIC:
  case ECRYPTFS_SUPER_MAGIC:
#ifdef BTRFS_SUPER_MAGIC
  case BTRFS_SUPER_MAGIC:
#endif
    break;

  default:
    if (debug)
      (void)fprintf(stderr, "fs_type: %X\n", (unsigned int)fs.f_type);
    return 0;
  }

  switch (opt) {
  case LIBENOSPACE_OPT_BYTES:
    if (debug)
      (void)fprintf(stderr, "avail:%lu f_bsize:%ld f_bavail:%lu total:%lu\n",
                    (long unsigned)avail, (long)fs.f_bsize, fs.f_bavail,
                    (unsigned)fs.f_bsize * fs.f_bavail);

    if (avail > (unsigned)fs.f_bsize * fs.f_bavail) {
      errno = libenospace_errno;
      return -1;
    }

    break;

  case LIBENOSPACE_OPT_PERCENT: {
    size_t free = 100 * fs.f_bavail / fs.f_blocks;

    if (debug)
      (void)fprintf(stderr, "avail:%lu free:%lu f_blocks:%lu f_bsize:%ld "
                            "f_bavail:%lu total:%lu\n",
                    (long unsigned)avail, (long unsigned)free,
                    (long unsigned)fs.f_blocks, (long)fs.f_bsize, fs.f_bavail,
                    (unsigned)fs.f_bsize * fs.f_bavail);

    if (avail > free) {
      errno = libenospace_errno;
      return -1;
    }

    break;
  }

  default:
    return -1;
  }

  errno = oerrno;
  return 0;
}

ssize_t write(int fd, const void *buf, size_t count) {
  if (quota(fd) < 0)
    return -1;

  return sys_write(fd, buf, count);
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt) {
  if (quota(fd) < 0)
    return -1;

  return sys_writev(fd, iov, iovcnt);
}

ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset) {
  if (quota(fd) < 0)
    return -1;

  return sys_pwrite(fd, buf, count, offset);
}

ssize_t pwritev(int fd, const struct iovec *iov, int iovcnt, off_t offset) {
  if (quota(fd) < 0)
    return -1;

  return sys_pwritev(fd, iov, iovcnt, offset);
}
