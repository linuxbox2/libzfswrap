/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2006 Ricardo Correia.  All rights reserved.
 * Use is subject to license terms.
 */

#if !defined(_SOL_STAT_H)

#ifndef PORTABLE_SOL_STAT_H
#define PORTABLE_SOL_STAT_H

#if !defined(__USE_LARGEFILE64)
#define __USE_LARGEFILE64 1
#endif
#include_next <sys/stat.h>

typedef struct stat stat64_t; /* XXX "struct stat64" should be defined,
			       * due to __USE_LARGEFILE64! */

#include <stdio.h>
#include <string.h>
#include <errno.h>

/* LINUX */
#include <sys/ioctl.h>
#include <sys/mount.h>

#if 0 /* XXX needs stat64 fix */

static inline int zfsfuse_fstat64(int fd, stat64_t *buf)
{
	if(fstat64(fd, buf) == -1)
		return -1;

	if(S_ISBLK(buf->st_mode)) {
		/* LINUX */
		uint64_t size;
		if(real_ioctl(fd, BLKGETSIZE64, &size) != 0) {
			fprintf(stderr, "failed to read device size: %s\n", strerror(errno));
			return 0;
		}
		buf->st_size = size;
	}

	return 0;
}

#define fstat64(fd, buf) zfsfuse_fstat64(fd, buf)

#endif /* 0 */

#endif  /* PORTABLE_SOL_STAT_H */
#endif  /* !defined(_SOL_STAT_H) */
