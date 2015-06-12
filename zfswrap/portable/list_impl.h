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
 * Copyright 2003 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#if !defined(_SYS_LIST_IMPL_H)

#ifndef	PORTABLE_LIST_IMPL_H
#define	PORTABLE_LIST_IMPL_H

/* #pragma ident	"%Z%%M%	%I%	%E% SMI" */

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

struct zfs_list_node {
	struct zfs_list_node *list_next;
	struct zfs_list_node *list_prev;
};

struct zfs_list {
	size_t	list_size;
	size_t	list_offset;
	struct zfs_list_node list_head;
};

#ifdef	__cplusplus
}
#endif

#endif	/* PORTABLE_LIST_IMPL_H */
#endif  /* !defined(_SYS_LIST_IMPL_H) */
