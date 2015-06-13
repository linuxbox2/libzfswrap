/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Copyright 2006 Ricardo Correia.
 * Use is subject to license terms.
 */

#if !defined(_SYS_KOBJ_H)

#ifndef PORTABLE_KOBJ_H
#define PORTABLE_KOBJ_H

#include <sdt.h>
#include <bootstat.h>

struct _buf {
	intptr_t _fd;
};

extern struct _buf *kobj_open_file(char *name);
extern int kobj_read_file(struct _buf *file, char *buf, unsigned size, unsigned off);
extern void kobj_close_file(struct _buf *file);
extern int kobj_get_filesize(struct _buf *file, uint64_t *size);

#endif  /* PORTABLE_KOBJ_H */
#endif  /* !defined(_SYS_KOBJ_H) */
