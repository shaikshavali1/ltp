/*
 *   Copyright (c) International Business Machines  Corp., 2001
 *	07/2001 Ported by Wayne Boyer
 *
 *   This program is free software;  you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY;  without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program;  if not, write to the Free Software Foundation,
 *   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/*
 * Test Description:
 *   Verify that,
 *   1) fchown(2) returns -1 and sets errno to EPERM if the effective user id
 *	of process does not match the owner of the file and the process is
 *	not super user.
 *   2) fchown(2) returns -1 and sets errno to EBADF if the file descriptor
 *	of the specified file is not valid.
 *   3) fchown(2) returns -1 and sets errno to EROFS if the named file resides
 *	on a read-only file system.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <grp.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <linux/loop.h>
#include <sys/ioctl.h>

#include "test.h"
#include "safe_macros.h"
#include "compat_16.h"

#define DIR_MODE	(S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP| \
			 S_IXGRP|S_IROTH|S_IXOTH)

static int fd1;
static int fd2 = -1;
static int fd3;
static const char *device;
static int mount_flag;

static struct test_case_t {
	int *fd;
	int exp_errno;
} test_cases[] = {
	{&fd1, EPERM},
	{&fd2, EBADF},
	{&fd3, EROFS},
};

TCID_DEFINE(fchown04);
int TST_TOTAL = ARRAY_SIZE(test_cases);

static void setup(void);
static void fchown_verify(int);
static void cleanup(void);
char loopname[4096];

int main(int ac, char **av)
{
	int lc;
	int i;

	tst_parse_opts(ac, av, NULL, NULL);

	setup();

	for (lc = 0; TEST_LOOPING(lc); lc++) {

		tst_count = 0;

		for (i = 0; i < TST_TOTAL; i++)
			fchown_verify(i);
	}

	cleanup();
	tst_exit();
}

void acquire_device(void)
{
        int ctlloopdev, loopdev, fsimgfile;
        long devnum;
	
	// Open loop controle device file	
	ctlloopdev = open("/dev/loop-control", O_RDWR);
        if (ctlloopdev == -1)
        	tst_brkm(TBROK, cleanup, "failed to open /dev/loop-control");

	// Find the free loop device
	devnum = ioctl(ctlloopdev, LOOP_CTL_GET_FREE);
        if (devnum == -1)
        	tst_brkm(TBROK, cleanup, "failed to get free loop device");

	sprintf(loopname, "/dev/loop%ld", devnum);
	tst_resm(TINFO, "loopname = %s\n", loopname);

	loopdev = open(loopname, O_RDWR);
        if (loopdev == -1)
        	tst_brkm(TBROK, cleanup, "failed to open: loop device");

	fsimgfile = open("/ltp_tst_mnt_fs/tstfs_ext4.img", O_RDWR);
        if (fsimgfile == -1)
        	tst_brkm(TBROK, cleanup, "failed to open: file system image file");

        if (ioctl(loopdev, LOOP_SET_FD, fsimgfile) == -1)
               tst_brkm(TBROK, cleanup, "failed to set loop fd");
	
	close(fsimgfile);
    	close(loopdev);
	

}

static void setup(void)
{
	struct passwd *ltpuser;
	const char *fs_type;

	TEST_PAUSE;

	tst_require_root();

	tst_tmpdir();

// This code is disabled because it explicitly
// acquire a loop device to mount the fs.
// create a filesystem using mkfs on the loop device and mount the fs.
#if 0
	fs_type = tst_dev_fs_type();
	device = tst_acquire_device(cleanup);

	if (!device)
		tst_brkm(TCONF, cleanup, "Failed to acquire device");

	tst_sig(NOFORK, DEF_HANDLER, cleanup);

	fd1 = SAFE_OPEN(cleanup, "tfile_1", O_RDWR | O_CREAT, 0666);

	tst_mkfs(cleanup, device, fs_type, NULL, NULL);
	SAFE_MKDIR(cleanup, "mntpoint", DIR_MODE);
	SAFE_MOUNT(cleanup, device, "mntpoint", fs_type, 0, NULL);
	mount_flag = 1;
	SAFE_TOUCH(cleanup, "mntpoint/tfile_3", 0644, NULL);
	SAFE_MOUNT(cleanup, device, "mntpoint", fs_type,
		   MS_REMOUNT | MS_RDONLY, NULL);
	fd3 = SAFE_OPEN(cleanup, "mntpoint/tfile_3", O_RDONLY);
#endif
	tst_sig(NOFORK, DEF_HANDLER, cleanup);

	fd1 = SAFE_OPEN(cleanup, "tfile_1", O_RDWR | O_CREAT, 0666);

	acquire_device();

	// Create mount point and mount the loop device
	SAFE_MKDIR(cleanup, "mntpoint", DIR_MODE);
	SAFE_MOUNT(cleanup, loopname, "mntpoint", "ext4", 0, NULL);

	// create a test file and remout the filesystem in Read only mode
	mount_flag = 1;
	SAFE_TOUCH(cleanup, "mntpoint/tfile_3", 0644, NULL);
	SAFE_MOUNT(cleanup, loopname, "mntpoint", "ext4",
		   MS_REMOUNT | MS_RDONLY, NULL);
	fd3 = SAFE_OPEN(cleanup, "mntpoint/tfile_3", O_RDONLY);

	ltpuser = SAFE_GETPWNAM(cleanup, "nobody");
	SAFE_SETEUID(cleanup, ltpuser->pw_uid);
}

static void fchown_verify(int i)
{
	UID16_CHECK(geteuid(), "fchown", cleanup)
	GID16_CHECK(getegid(), "fchown", cleanup)

	TEST(FCHOWN(cleanup, *test_cases[i].fd, geteuid(), getegid()));

	if (TEST_RETURN == -1) {
		if (TEST_ERRNO == test_cases[i].exp_errno) {
			tst_resm(TPASS | TTERRNO, "fchown failed as expected");
		} else {
			tst_resm(TFAIL | TTERRNO,
				 "fchown failed unexpectedly; expected %d - %s",
				 test_cases[i].exp_errno,
				 strerror(test_cases[i].exp_errno));
		}
	} else {
		tst_resm(TFAIL, "fchown passed unexpectedly");
	}
}

static void cleanup(void)
{
	if (seteuid(0))
		tst_resm(TWARN | TERRNO, "Failet to seteuid(0) before cleanup");

	if (fd1 > 0 && close(fd1))
		tst_resm(TWARN | TERRNO, "Failed to close fd1");

// This code performs clean up of mounted filesystem and disabled sub test case.
// This code is not needed because mounting filesystem and sub test case is
// disabled. Hence, disabling this code.
#if 0
	if (fd3 > 0 && close(fd3))
		tst_resm(TWARN | TERRNO, "Failed to close fd3");

	if (mount_flag && tst_umount("mntpoint") < 0)
		tst_resm(TWARN | TERRNO, "umount device:%s failed", device);

	if (device)
		tst_release_device(device);
#endif
        if (fd3 > 0 && close(fd3))
                tst_resm(TWARN | TERRNO, "Failed to close fd3");
	if (mount_flag) {
		int devfd = open(loopname, O_RDWR);
		if (ioctl(devfd, LOOP_CLR_FD, 0) < 0) {
    			tst_resm(TWARN | TERRNO, "Failed to clear fd");
		}
		SAFE_UMOUNT(cleanup, "mntpoint");
	}

	tst_rmdir();
}
