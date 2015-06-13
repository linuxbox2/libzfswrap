lib_LTLIBRARIES = libzfswrap.la

library_includedir=$(includedir)
library_include_HEADERS = libzfswrap.h \
	portable/acl.h \
	portable/acl_impl.h \
	portable/atomic.h \
	portable/attr.h \
	portable/avl.h \
	portable/avl_impl.h \
	portable/bitmap.h \
	portable/bootstat.h \
	portable/buf.h \
	portable/byteorder.h \
	portable/cmn_err.h \
	portable/condvar.h \
	portable/conf.h \
	portable/cpuvar.h \
	portable/cred.h \
	portable/dditypes.h \
	portable/ddt.h \
	portable/debug.h \
	portable/disp.h \
	portable/dmu.h \
	portable/dmu_zfetch.h \
	portable/dnode.h \
	portable/dsl_deleg.h \
	portable/dsl_pool.h \
	portable/feature_tests.h \
	portable/file_aux.h \
	portable/file.h \
	portable/fm_util.h \
	portable/int_types.h \
	portable/isa_defs.h \
	portable/kern_sys_time.h \
	portable/kmem.h \
	portable/kobj.h \
	portable/libuutil.h \
	portable/libzfs.h \
	portable/libzfs_impl.h \
	portable/list.h \
	portable/list_impl.h \
	portable/machlock.h \
	portable/mutex.h \
	portable/note.h \
	portable/nvpair.h \
	portable/param_aux.h \
	portable/param.h \
	portable/portable_dirent.h \
	portable/portable_ioctl.h \
	portable/portable_thread.h \
	portable/proc.h \
	portable/random.h \
	portable/rctl.h \
	portable/refcount.h \
	portable/refstr.h \
	portable/rrwlock.h \
	portable/rwlock.h \
	portable/rwstlock.h \
	portable/sdt.h \
	portable/seg_enum.h \
	portable/seg_kmem.h \
	portable/semaphore.h \
	portable/spa.h \
	portable/stat.h \
	portable/statvfs.h \
	portable/sunddi.h \
	portable/sysevent_eventdefs.h \
	portable/sysevent.h \
	portable/sysmacros.h \
	portable/systm.h \
	portable/taskq.h \
	portable/time_aux.h \
	portable/t_lock.h \
	portable/txg.h \
	portable/txg_impl.h \
	portable/types32.h \
	portable/types.h \
	portable/u8_textprep.h \
	portable/uio.h \
	portable/umem.h \
	portable/user.h \
	portable/vfs.h \
	portable/vmem.h \
	portable/vnode.h \
	portable/zfs_acl.h \
	portable/zfs_context.h \
	portable/zfs_debug.h \
	portable/zfs_fuid.h \
	portable/zfs.h \
	portable/zfs_vfsops.h \
	portable/zfs_znode.h \
	portable/zil.h \
	portable/zio.h \
	portable/zio_impl.h \
	portable/zone.h

libzfswrap_la_SOURCES = ptrace.c zfs_acl.c zfs_dir.c zfs_ioctl.c zfs_log.c zfs_replay.c zfs_rlock.c zfs_vfsops.c zfs_vnops.c zvol.c zfsfuse_socket.c libzfswrap_utils.c libzfswrap.c
libzfswrap_la_LIBADD = ${top_builddir}/lib/libatomic/libatomic.la \
                       ${top_builddir}/lib/libzpool/libzpool-kernel.la \
                       ${top_builddir}/lib/libnvpair/libnvpair-kernel.la \
                       ${top_builddir}/lib/libumem/libumem.la \
                       ${top_builddir}/lib/libsolkerncompat/libsolkerncompat.la \
                       ${top_builddir}/lib/libzfs/libzfs.la \
                       ${top_builddir}/lib/libavl/libavl.la \
                       ${top_builddir}/lib/libuutil/libuutil.la \
                       ${top_builddir}/lib/libsolcompat/libsolcompat.la \
                       ${top_builddir}/lib/libzfscommon/libzfscommon-user.la
libzfswrap_la_CFLAGS = -I${top_srcdir}/lib/libavl/include \
                       -I${top_srcdir}/lib/libnvpair/include \
                       -I${top_srcdir}/lib/libumem/include \
                       -I${top_srcdir}/lib/libzfs/include \
                       -I${top_srcdir}/lib/libuutil/include \
                       -I${top_srcdir}/lib/libzfscommon/include \
                       -I${top_srcdir}/lib/libsolkerncompat/include \
                       -I${top_srcdir}/lib/libsolcompat/include \
                       -Iportable \
                       -D_KERNEL @DEBUG_CFLAGS@
AM_LDFLAGS = -lrt -lpthread -ldl -lz -laio -lcrypto -lm

noinst_HEADERS = kmem_asprintf.h \
                 zfs_ioctl.h \
                 zfsfuse_socket.h \
                 libzfswrap_utils.h

doc:
	doxygen

.PHONY: doc
