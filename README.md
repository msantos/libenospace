libenospace
===========

libenospace: set disk usage limits for a process

libenospace works by intercepting calls to `write(2)` using
`LD_PRELOAD`.

Build
-----

~~~
make
~~~

Example
-------

    LD_PRELOAD=libenospace.so LIBENOSPACE_AVAIL=-1 sh -c "yes > test"

    LD_PRELOAD=libenospace.so LIBENOSPACE_OPT=bytes \
        LIBENOSPACE=$((10*1024*1024*1024)) vim


Environment Variables
---------------------

`LIBENOSPACE_OPT`
: Specify whether the minimum available free disk space is calculated
  as a percentage or in bytes:

        percent: percentage of free disk space (default)
        bytes: free disk space in bytes


`LIBENOSPACE_AVAIL`
: Set the minimum free disk space.

        0: never enforce disk space limits
        -1: always enforce disk space limits

`LIBENOSPACE_ERRNO`
: Sets the value to return on write failures (default: ENOSPC).

`LIBENOSPACE_DEBUG`
: Write errors and informational messages to stderr.

Limitations
-----------

* libenospace requires the program to be dynamically linked

  libenospace will not work with statically linked programs or programs
  that directly make syscalls.

* currently libenospace only enforces usage limits on extfs and ecryptfs

* usage limits apply to all filesystems the program accesses

  For example, a program writing to /home and /opt may have writes to
  /opt succeed while writes to /home fail, if /home does not have the
  minimum available space.

* overhead: for regular files, libenospace makes 2 additional syscalls
  per write(2)

  For all file descriptors (sockets, pipes, char devices), libenospace
  calls `fstat(2)`.

* disk usage limits do not check the number of inodes

* disk usage limits apply to the following syscalls:

        write(2)
        writev(2)
        pwrite(2)
        pwritev(2)

Rationale
---------

Disk space is a shared resource. If multiple processes share disk space,
any process can monopolize usage.

Processes may be otherwise isolated using Linux containers: running in
separate namespaces, under unique UIDs, bind mounting directories on a
common disk.

Isolating disk usage typically requires administrative intervention and
allocting dedicated space to each process.

libenospace lets processes opt into disk usage limits without requiring
any special privileges from a common pool.

Alternatives
------------

### Bind Mounts

Bind mounts do not support disk usage limits.

### Partitions

New partitions can be created for each process:

* requires root privileges

* a new parition must be allocated for each process

* depending on the underlying file system and volume management, may be
  inflexible and may require interruption of service

### Loopback Mounts

Create a sparse file on the disk and mount within the container:

* requires root privileges

* since loopback mounts are sparse files, no disk space is used

  The disk image is sparse. If it is not used, actual disk usage is
  minimal. If the disk is used at all, e.g., writing and deleting files,
  the full disk image will eventually be used.

  Since the loop device is created on an existing partition, care must
  be taken to leave appropriate space for other processes.

### Disk Quota

* requires root privileges

* quotas are per user not per process

* accounting overhead

* the quota system is designed for administrators setting limits for
  human users, not services running under a UID

### fuse

[fusequota](https://github.com/floriandejonckheere/fusequota)

### Resource Limits

Processes can restrict the maximum writable size of a file by calling
`setrlimit(RLIMIT_FSIZE)`.

If the file size exceeds the limit, the process will be sent a `SIGXFSZ`
signal. By default, the signal will cause the process to exit.

~~~ shell
#!/bin/bash

set -o errexit
set -o nounset
set -o pipefail

# ignore the signal
# write will return -1
trap '' XFSZ

# disable file writes
ulimit -f 0

# test: File too large
yes > test
~~~

* no special privileges required

* can be set per process

* only effective if the program writes to one file, e.g., a log

  The process can write many files of the maximum file size.

* if the maximum file size is set to 0, the process can still create 0
  byte files possibly truncating existing files
