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
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef PORTABLE_DIRENT_H
#define	PORTABLE_DIRENT_H

#ifdef	__cplusplus
extern "C" {
#endif

#if !defined(_SYS_DIRENT_H)

typedef __ino64_t ino64_t;
typedef __off64_t off64_t;

typedef struct zfs_dirent64 {
	ino64_t		d_ino;		/* "inode number" of entry */
	off64_t		d_off;		/* offset of disk directory entry */
	unsigned short	d_reclen;	/* length of this record */
	char		d_name[1];	/* name of file */
} dirent64_t;

#define	DIRENT64_RECLEN(namelen)	\
	((offsetof(dirent64_t, d_name[0]) + 1 + (namelen) + 7) & ~ 7)
#define	DIRENT64_NAMELEN(reclen)	\
	((reclen) - (offsetof(dirent64_t, d_name[0])))

#endif /* !defined(_SYS_DIRENT_H) */

#ifdef	__cplusplus
}
#endif

#endif	/* PORTABLE_DIRENT_H */
