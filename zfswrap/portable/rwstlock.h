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
 * Copyright (c) 2006 Ricardo Correia.
 */

#if !defined(_SYS_RWSTLOCK_H)

#ifndef PORTABLE_RWSTLOCK_H
#define PORTABLE_RWSTLOCK_H

#include <rwlock.h>

typedef krwlock_t rwslock_t;

#define rwst_init(a,b,c,d) rw_init(a,b,c,d)
#define rwst_tryenter(a,b) rw_tryenter(a,b)
#define rwst_exit(a)       rw_exit(a)
#define rwst_destroy(a)    rw_destroy(a)

#endif	/* PORTABLE_RWSTLOCK_H */
#endif  /* !defined(_SYS_RWSTLOCK_H) */
