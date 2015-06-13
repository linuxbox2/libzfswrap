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
 * Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#if !defined(_SYS_ZFS_CONTEXT_H)

#ifndef PORTABLE_ZFS_CONTEXT_H
#define	PORTABLE_ZFS_CONTEXT_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <note.h>
#include <types.h>
#include <t_lock.h>
#include <atomic.h>
#include <sysmacros.h>
#include <bitmap.h>
#include <cmn_err.h>
#include <kmem.h>
#include <taskq.h>
#include <buf.h>
#include <param.h>
#include <systm.h>
#include <cpuvar.h>
#include <kobj.h>
#include <conf.h>
#include <disp.h>
#include <debug.h>
#include <random.h>
#include <byteorder.h>
#include <systm.h>
#include <list.h>
#include <uio.h>
#include <portable_dirent.h>
#include <kern_sys_time.h>
#include <seg_kmem.h>
#include <zone.h>
#include <uio.h>
#include <zfs_debug.h>
#include <sysevent.h>
#include <sysevent_eventdefs.h>
// #include <sys/sysevent/dev.h>
#include <fm_util.h>
#include <sunddi.h>

// #define	CPU_SEQID (thr_self() & (max_ncpus - 1))
/* zfs-fuse : this CPU_SEQID macro is used to enter a mutex
 * for a cpu group. Wonder if it's safe in zfs-fuse ?
 * Here it seems to assume that threads are assigned sequentially to
 * all the cpus. I prefer to return 0 for this now... */
#define CPU_SEQID 0

extern char *kmem_asprintf(const char *fmt, ...);
#define	strfree(str) kmem_free((str), strlen(str)+1)

#ifdef	__cplusplus
}
#endif

#endif	/* PORTABLE_ZFS_CONTEXT_H */
#endif  /* !defined(_SYS_ZFS_CONTEXT_H) */
