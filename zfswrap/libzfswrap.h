#ifndef LIBZFSWRAP_H
#define LIBZFSWRAP_H

#if !defined(__USE_LARGEFILE64)
#define __USE_LARGEFILE64 1 /* rlim64_t */
#endif

#define _KERNEL 1 /* XXX advisedly */

#include <stdint.h>
#include <stat.h>
#include <statvfs.h>
#include <sys/uio.h>
#include <libzfs.h>
#include <libzfs_impl.h>
#include <portable_dirent.h>
#include <zfs_znode.h>
#include <vnode.h>

/** Reprensentation of a file system object */
typedef struct
{
  /** Object inode */
  uint64_t inode;
  /** Object generation */
  uint64_t generation;
} inogen_t;

/** Representation of a directory entry */
typedef struct
{
  /** Object name */
  char psz_filename[256];
  /** Object representation */
  inogen_t object;
  /** Object type */
  int type;
  /** Obejct attributes */
  struct stat stats;
} lzfw_entry_t;

/** Representation of the user rights */
typedef struct
{
  /** User identifier */
  uid_t uid;
  /** Group identifier */
  gid_t gid;
} creden_t;

/** libzfswrap library handle */
typedef struct libzfs_handle lzfw_handle_t;

/** Object mode */
#define LZFSW_ATTR_MODE         (1 << 0)
/** Owner user identifier */
#define LZFSW_ATTR_UID          (1 << 1)
/** Group identifier */
#define LZFSW_ATTR_GID          (1 << 2)
/** Access time */
#define LZFSW_ATTR_ATIME        (1 << 3)
/** Modification time */
#define LZFSW_ATTR_MTIME        (1 << 4)

/** Op Result Flags (some ops) */
#define LZFW_OFLAG_NONE  0x0000

#define LZFW_OFLAG_OPEN_CREATED  0x0001

/**
 * Initialize the libzfswrap library
 * @return a handle to the library, NULL in case of error
 */
lzfw_handle_t *lzfw_init();

/**
 * Uninitialize the library
 * @param p_zhd: the libzfswrap handle
 */
void lzfw_exit(lzfw_handle_t *p_zhd);

/**
 * Create a zpool
 * @param p_zhd: the libzfswrap handle
 * @param psz_name: the name of the zpool
 * @param psz_type: type of the zpool (mirror, raidz, raidz([1,255])
 * @param ppsz_error: the error message (if any)
 * @return 0 on success, the error code otherwise
 */
int lzfw_zpool_create(lzfw_handle_t *p_zhd, const char *psz_name, const char *psz_type, const char **ppsz_dev, size_t i_dev, const char **ppsz_error);

/**
 * Destroy the given zpool
 * @param p_zhd: the libzfswrap handle
 * @param psz_name: zpool name
 * @param b_force: force the unmount process or not
 * @param ppsz_error: the error message (if any)
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_zpool_destroy(lzfw_handle_t *p_zhd, const char *psz_name, int b_force, const char **ppsz_error);

/**
 * Add to the given zpool the following device
 * @param p_zhd: the libzfswrap handle
 * @param psz_zpool: the zpool name
 * @param psz_type: type of the device group to add
 * @param ppsz_dev: the list of devices
 * @param i_dev: the number of devices
 * @param ppsz_error: the error message if any
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_zpool_add(lzfw_handle_t *p_zhd, const char *psz_zpool,
		   const char *psz_type, const char **ppsz_dev,
		   size_t i_dev, const char **ppsz_error);

/**
 * Remove the given vdevs from the zpool
 * @param p_zhd: the libzfswrap handle
 * @param psz_zpool: the zpool name
 * @param ppsz_vdevs: the vdevs
 * @param i_vdevs: the number of vdevs
 * @param ppsz_error: the error message if any
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_zpool_remove(lzfw_handle_t *p_zhd, const char *psz_zpool,
		      const char **ppsz_dev, size_t i_vdevs,
		      const char **ppsz_error);

/**
 * Attach the given device to the given vdev in the zpool
 * @param p_zhd: the libzfswrap handle
 * @param psz_zpool: the zpool name
 * @param psz_current_dev: the device to use as an attachment point
 * @param psz_new_dev: the device to attach
 * @param i_replacing: do we have to attach or replace ?
 * @param ppsz_error: the error message if any
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_zpool_attach(lzfw_handle_t *p_zhd, const char *psz_zpool,
		      const char *psz_current_dev, const char *psz_new_dev,
		      int i_replacing, const char **ppsz_error);

/**
 * Detach the given vdevs from the zpool
 * @param p_zhd: the libzfswrap handle
 * @param psz_zpool: the zpool name
 * @param psz_dev: the device to detach
 * @param ppsz_error: the error message if any
 * @return 0 in case of success, the error message otherwise
 */
int lzfw_zpool_detach(lzfw_handle_t *p_zhd, const char *psz_zpool, const char *psz_dev, const char **ppsz_error);

/**
 * List the available zpools
 * @param p_zhd: the libzfswrap handle
 * @param psz_props: the properties to retrieve
 * @param ppsz_error: the error message if any
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_zpool_list(lzfw_handle_t *p_zhd, const char *psz_props, const char **ppsz_error);

/**
 * Print the status of the available zpools
 * @param p_zhd: the libzfswrap handle
 * @param ppsz_error: the error message if any
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_zpool_status(lzfw_handle_t *p_zhd, const char **ppsz_error);


/**
 * Print the list of ZFS file systems and properties
 * @param p_zhd: the libzfswrap handle
 * @param psz_props: the properties to retrieve
 * @param ppsz_error: the error message if any
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_zfs_list(lzfw_handle_t *p_zhd, const char *psz_props,
		  const char **ppsz_error);

/**
 * Callback-based iteration over ZFS datasets
 * @param zhd: the libzfswrap handle
 * @param parent_ds_name: parent name (fully qualified, if not a pool/root)
 * @param func: function to call for each (non-hidden) dataset
 * @param arg: opaque argument which will be passed to func
 * @param ppsz_error: error message if any
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_datasets_iter(libzfs_handle_t *zhd, const char *parent_ds_name,
		       zfs_iter_f func, void *arg, const char **ppsz_error);

/**
 * List the available snapshots for the given zfs
 * @param p_zhd: the libzfswrap handle
 * @param psz_zfs: name of the file system
 * @param ppsz_error: the error message if any
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_zfs_list_snapshot(lzfw_handle_t *p_zhd, const char *psz_zfs, const char **ppsz_error);

/**
 * Return the list of snapshots for the given zfs in an array of strings
 * @param p_zhd: the libzfswrap handle
 * @param psz_zfs: name of the file system
 * @param pppsz_snapshots: the array of snapshots names
 * @param ppsz_error: the error message if any
 * @return the number of snapshots in case of success, -1 otherwise
 */
int lzfw_zfs_get_list_snapshots(lzfw_handle_t *p_zhd, const char *psz_zfs, char ***pppsz_snapshots, const char **ppsz_error);

/**
 * Create a snapshot of the given ZFS file system
 * @param p_zhd: the libzfswrap handle
 * @param psz_zfs: name of the file system
 * @param psz_snapshot: name of the snapshot
 * @param ppsz_error: the error message if any
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_zfs_snapshot(lzfw_handle_t *p_zhd, const char *psz_zfs, const char *psz_snapshot, const char **ppsz_error);

/**
 * Destroy a snapshot of the given ZFS file system
 * @param p_zhd: the libzfswrap handle
 * @param psz_zfs: name of the file system
 * @param psz_snapshot: name of the snapshot
 * @param ppsz_error: the error message if any
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_zfs_snapshot_destroy(lzfw_handle_t *p_zhd, const char *psz_zfs, const char *psz_snapshot, const char **ppsz_error);

/**
 * Create a new dataset (filesystem).
 * @param p_zhd: the libzfswrap handle
 * @param psz_zfs: name of the file system
 * @param type: dataset type (e.g., ZFS_TYPE_FILESYSTEM)
 * @param ppsz_error: the error message if any
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_dataset_create(lzfw_handle_t *p_zhd, const char *psz_zfs, int type, const char **ppsz_error);

/**
 * Destroy dataset (filesystem).
 * @param p_zhd: the libzfswrap handle
 * @param psz_zfs: name of the file system
 * @param ppsz_error: the error message if any
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_dataset_destroy(lzfw_handle_t *p_zhd, const char *psz_zfs, const char **ppsz_error);

/**
 * Mount the given file system
 * @param psz_zpool: the pool to mount
 * @param psz_dir: the directory to mount
 * @param psz_options: options for the mounting point
 * @return the vitual file system
 */
vfs_t *lzfw_mount(const char *psz_zpool, const char *psz_dir, const char *psz_options);

/**
 * Get the root object of a file system
 * @param p_vfs: the virtual filesystem
 * @param p_root: return the root object
 * @return 0 on success, the error code otherwise
 */
int lzfw_getroot(vfs_t *p_vfs, inogen_t *p_root);


/**
 * Unmount the given file system
 * @param p_vfs: the virtual file system
 * @param b_force: force the unmount ?
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_umount(vfs_t *p_vfs, int b_force);

/**
 * Get some more informations about the file system
 * @param p_vfs: the virtual file system
 * @param p_stats: the statistics
 * @return 0 in case of success, -1 otherwise
 */
int lzfw_statfs(vfs_t *p_vfs, struct statvfs *p_stats);

/**
 * Lookup for a given file in the given directory
 * @param p_vfs: the virtual file system
 * @param p_cred: the credentials of the user
 * @param parent: the parent file object
 * @param psz_name: filename
 * @param p_object: return the object node and generation
 * @param p_type: return the object type
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_lookup(vfs_t *p_vfs, creden_t *p_cred, inogen_t parent, const char *psz_name, inogen_t *p_object, int *p_type);

/**
 * Lookup name relative to an open directory vnode
 * @param p_vfs: the virtual file system
 * @param p_cred: the credentials of the user
 * @param parent: the parent file object
 * @param psz_name: filename
 * @param p_object: return the object node and generation (XXX shouldn't this be a vnode?)
 * @param p_type: return the object type
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_lookupnameat(vfs_t *p_vfs, creden_t *p_cred,
		      vnode_t *parent, const char *psz_name,
		      inogen_t *p_object, int *p_type);

/**
 * Test the access right of the given file
 * @param p_vfs: the virtual filesystem
 * @param p_cred: the credentials of the user
 * @param object: the object
 * @param mask: the rights to check
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_access(vfs_t *p_vfs, creden_t *p_cred, inogen_t object, int mask);

/**
 * Create the given file
 * @param p_vfs: the virtual file system
 * @param p_cred: the credentials of the user
 * @param parent: the parent object
 * @param psz_filename: the file name
 * @param mode: the file mode
 * @param p_file: return the file
 * @return 0 in case of success the error code otherwise
 */
int lzfw_create(vfs_t *p_vfs, creden_t *p_cred, inogen_t parent,
		const char *psz_filename, mode_t mode,
		inogen_t *p_file);

/**
 * Create the given file
 * @param vfs: the virtual file system
 * @param cred: the credentials of the user
 * @param parent: parent vnode
 * @param psz_filename: the file name
 * @param mode: the file mode
 * @param file: return the file
 * @return 0 in case of success the error code otherwise
 */
int lzfw_createat(vfs_t *vfs, creden_t *cred, vnode_t *parent,
		  const char *psz_filename, mode_t mode, inogen_t *file);

/**
 * Open the given object
 * @param p_vfs: the virtual filesystem
 * @param p_cred: the credentials of the user
 * @param object: the object to open
 * @param i_flags: the opening flags
 * @param pp_vnode: the vnode to return
 * @return 0 on success, the error code otherwise
 */
int lzfw_open(vfs_t *p_vfs, creden_t *p_cred, inogen_t object, int i_flags, vnode_t **pp_vnode);

/**
 * Open an object relative to an open directory vnode
 * @param p_vfs: the virtual file system
 * @param p_cred: the credentials of the user
 * @param parent: the parent vnode
 * @param psz_name: file name
 * @param i_flags: the opening flags
 * @param mode: desired mode, if i_flags & O_CREAT
 * @param o_flags: result flags
 * @param pp_vnode: the virtual node
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_openat(vfs_t *p_vfs, creden_t *p_cred, vnode_t *parent, const char *psz_name, unsigned int i_flags, mode_t mode,
		unsigned int *o_flags, vnode_t **pp_vnode);

/**
 * Close the given vnode
 * @param p_vfs: the virtual file system
 * @param p_cred: the credentials of the user
 * @param p_vnode: the vnode to close
 * @param i_flags: the flags given when opening
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_close(vfs_t *p_vfs, creden_t *p_cred, vnode_t *p_vnode, int i_flags);

/**
 * Read some data from the given file
 * @param p_vfs: the virtual file system
 * @param p_cred: the credentials of the user
 * @param p_vnode: the vnode
 * @param p_buffer: the buffer to write into
 * @param size: the size of the buffer
 * @param behind: do we have to read behind the file ?
 * @param offset: the offset to read
 * @return bytes read if successful, -error code otherwise (?)
 */
ssize_t lzfw_read(vfs_t *p_vfs, creden_t *p_cred, vnode_t *p_vnode, void *p_buffer, size_t size, int behind, off_t offset);

/**
 * Vectorwise read from file
 * @param p_vfs: the virtual file system
 * @param cred: the credentials of the user
 * @param vnode: the vnode
 * @param iov: array of iovec buffers to read into
 * @param iovcnt: the length of the iov array
 * @param offset: the logical file offset
 * @return bytes read if successful, -error code otherwise (?)
 */
ssize_t lzfw_preadv(vfs_t *p_vfs, creden_t *cred,
		    vnode_t *vnode,
		    struct iovec *iov, int iovcnt,
		    off_t offset);

/**
 * Write some data to the given file
 * @param p_vfs: the virtual file system
 * @param p_cred: the credentials of the user
 * @param p_vnode: the vnode
 * @param p_buffer: the buffer to write
 * @param size: the size of the buffer
 * @param behind: do we have to write behind the end of the file ?
 * @param offset: the offset to write
 * @return bytes written if successful, -error code otherwise (?)
 */
ssize_t lzfw_write(vfs_t *p_vfs, creden_t *p_cred, vnode_t *p_vnode, void *p_buffer, size_t size, int behind, off_t offset);

/**
 * Vectorwise write to file
 * @param p_vfs: the virtual file system
 * @param cred: the credentials of the user
 * @param vnode: the vnode
 * @param iov: array of iovec buffers to write
 * @param iovcnt: the length of the iov array
 * @param offset: the logical file offset
 * @return bytes written if successful, -error code otherwise (?)
 */
ssize_t lzfw_pwritev(vfs_t *p_vfs, creden_t *cred,
		     vnode_t *vnode,
		     struct iovec *iov, int iovcnt,
		     off_t offset);

/**
 * Get the stat about a file
 * @param p_vfs: the virtual file system
 * @param p_cred: the credentials of the user
 * @param p_vnode: the vnode
 * @param p_stat: the stat struct to fill in
 * @return 0 on success, the error code otherwise
 */
int lzfw_stat(vfs_t *p_vfs, creden_t *p_cred, vnode_t *p_vnode, struct stat *p_stat);

/**
 * Get the attributes of an object
 * @param p_vfs: the virtual filesystem
 * @param p_cred: the credentials of the user
 * @param object: the object
 * @param p_stat: the attributes to fill
 * @param p_type: return the type of the object
 * @return 0 on success, the error code otherwise
 */
int lzfw_getattr(vfs_t *p_vfs, creden_t *p_cred, inogen_t object, struct stat *p_stat, int *p_type);

/**
 * Get the value for the given extended attribute.  As in Linux xattrs,
 * if value is NULL, then the stored size of the attribute identified by
 * psz_key (if any) is placed in size.
 *
 * @param p_vfs: the virtual file system
 * @param p_cred: the user credentials
 * @param object: the object
 * @param psz_key: the key
 * @param value: buffer to receive value
 * @param size: on entry, max size of value; on exit, bytes written into value
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_getxattrat(vfs_t *p_vfs, creden_t *p_cred, vnode_t *object,
		    const char *psz_key, char *value, size_t *size);

/**
 * Set the attributes of an object
 * @param p_vfs: the virtual filesystem
 * @param p_cred: the credentials of the user
 * @param object: the object
 * @param p_stat: new attributes to set
 * @param flags: bit field of attributes to set
 * @param p_new_stat: new attributes of the object
 * @return 0 on success, the error code otherwise
 */
int lzfw_setattr(vfs_t *p_vfs, creden_t *p_cred, inogen_t object, struct stat *p_stat, int flags, struct stat *p_new_stat);

/**
 * Add the given (key,value) to the extended attributes.
 * This function will change the value if the key already exists.
 * @param p_vfs: the virtual file system
 * @param p_cred: the credentials of the user
 * @param object: the object
 * @param psz_key: the key
 * @param psz_value: the value
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_setxattrat(vfs_t *p_vfs, creden_t *p_cred, vnode_t *object, const char *psz_key, const char *psz_value);

/**
 * List the extended attributes
 * @param p_vfs: the virtual file system
 * @param p_cred: the credentials of the user
 * @param object: the object
 * @param ppsz_buffer: the buffer to fill with the list of attributes
 * @param p_size: will contain the size of the buffer
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_listxattr(vfs_t *p_vfs, creden_t *p_cred, inogen_t object, char **ppsz_buffer, size_t *p_size);

typedef int (*opxattr_func)(vnode_t *vnode, inogen_t object, creden_t *cred, const char *name, void *arg);

/**
 * List extended attributes callback style
 * @param p_vfs: the virtual file system
 * @param p_cred: the credentials of the user
 * @param object: the object
 * @param cb: per-key callback
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_listxattr2(vfs_t *p_vfs, creden_t *p_cred, inogen_t object, opxattr_func cb, void *arg);

/**
 * Add the given (key,value) to the extended attributes.
 * This function will change the value if the key already exist.
 * @param p_vfs: the virtual file system
 * @param p_cred: the credentials of the user
 * @param object: the object
 * @param psz_key: the key
 * @param psz_value: the value
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_setxattr(vfs_t *p_vfs, creden_t *p_cred, inogen_t object, const char *psz_key, const char *psz_value);

/**
 * Get the value for the given extended attribute
 * @param p_vfs: the virtual file system
 * @param p_cred: the user credentials
 * @param object: the object
 * @param psz_key: the key
 * @param ppsz_value: the value
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_getxattr(vfs_t *p_vfs, creden_t *p_cred, inogen_t object, const char *psz_key, char **ppsz_value);

/**
 * Remove the given extended attribute
 * @param p_vfs: the virtual file system
 * @param p_cred: the user credentials
 * @param object: the object
 * @param psz_key: the key
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_removexattr(vfs_t *p_vfs, creden_t *p_cred, inogen_t object, const char *psz_key);

/**
 * Open a directory
 * @param p_vfs: the virtual filesystem
 * @param p_cred: the credentials of the user
 * @param directory: the directory to open
 * @param pp_vnode: the vnode to return
 * @return 0 on success, the error code otherwise
 */
int lzfw_opendir(vfs_t *p_vfs, creden_t *p_cred, inogen_t directory, vnode_t **pp_vnode);

/**
 * Read the given directory
 * @param p_vfs: the virtual filesystem
 * @param p_cred: the credentials of the user
 * @param p_vnode: the vnode
 * @param p_entries: the array of entries to fill
 * @param size: the array size
 * @param cookie: the offset to read in the directory
 * @return 0 on success, the error code otherwise
 */
int lzfw_readdir(vfs_t *p_vfs, creden_t *p_cred, vnode_t *p_vnode, lzfw_entry_t *p_entries, size_t size, off_t *cookie);

/**
 * Callback-based iteration over ZFS directories
 * @param vfs: the virtual filesystem
 * @param cred: the credentials of the user
 * @param vnode: an open directory vnode
 * @param dir_iter_f: function to call for each entry
 * @param arg: opaque argument which will be passed to func
 * @param cookie: offset from which to iterate (0 for beginning)
 * @return 0 on success, the error code otherwise
 */
#define LZFW_DI_CB_IFLAG_NONE   0x0000
#define LZFW_DI_CB_IFLAG_EOF    0x0001
#define LZFW_DI_CB_IFLAG_ATTR   0x0002

#define LZFW_DI_CB_OFLAG_NONE        0x0000
#define LZFW_DI_CB_OFLAG_INVALIDATE  0x0001 /* iteration invalidated */

typedef struct dir_iter_cb_context
{
  dirent64_t *dirent;
  vattr_t *vattr;
  vnode_t *vnode;
  znode_t *znode;
  uint64_t gen;
  uint32_t iflags; /* caller flags */
  uint32_t oflags; /* called-function flags */
} dir_iter_cb_context_t;

#define init_di_cb_context(ctx)			\
  do {						\
    (ctx)->vattr = NULL;			\
    (ctx)->vnode = NULL;			\
    (ctx)->znode = NULL;			\
    (ctx)->iflags = LZFW_DI_CB_IFLAG_NONE;	\
    (ctx)->oflags = LZFW_DI_CB_OFLAG_NONE;	\
  } while (0)

typedef int (*dir_iter_f)(vnode_t *, dir_iter_cb_context_t *, void *);

#define LZFW_DI_FLAG_NONE     0x0000
#define LZFW_DI_FLAG_GEN      0x0001
#define LZFW_DI_FLAG_GETATTR  0x0002

int lzfw_dir_iter(vfs_t *vfs, creden_t *cred, vnode_t *vnode,
		  dir_iter_f func, void *arg, off_t *cookie, uint32_t flags);

/**
 * Close the given directory
 * @param p_vfs: the virtual filesystem
 * @param p_cred: the credentials of the user
 * @param p_vnode: the vnode
 * @return 0 on success, the error code otherwise
 */
int lzfw_closedir(vfs_t *p_vfs, creden_t *p_cred, vnode_t *p_vnode);

/**
 * Create the given directory
 * @param p_vfs: the virtual file system
 * @param p_cred: the credentials of the user
 * @param parent: the parent directory
 * @param psz_name: the name of the directory
 * @param mode: the mode for the directory
 * @param p_directory: return the new directory
 * @return 0 on success, the error code otherwise
 */
int lzfw_mkdir(vfs_t *p_vfs, creden_t *p_cred, inogen_t parent, const char *psz_name, mode_t mode, inogen_t *p_directory);

/**
 * Create directory at vnode
 * @param p_vfs: the virtual file system
 * @param p_cred: the credentials of the user
 * @param parent: the parent directory
 * @param psz_name: the name of the directory
 * @param mode: the mode for the directory
 * @param p_directory: return the new directory
 * @return 0 on success, the error code otherwise
 */
int lzfw_mkdirat(vfs_t *p_vfs, creden_t *p_cred, vnode_t *parent, const char *psz_name, mode_t mode, inogen_t *p_directory);

/**
 * Remove the given directory
 * @param p_vfs: the virtual filesystem
 * @param p_cred: the credentials of the user
 * @param parent: the parent directory
 * @param psz_filename: name of the file to unlink
 * @return 0 on success, the error code otherwise
 */
int lzfw_rmdir(vfs_t *p_vfs, creden_t *p_cred, inogen_t parent, const char *psz_filename);

/**
 * Create a symbolic link
 * @param p_vfs: the virtual file system
 * @param p_cred: the credentials of the user
 * @param parent: the parent directory
 * @param psz_name: the symbolic name
 * @param psz_link: the link content
 * @param p_symlink: the new symlink
 * @return 0 on success, the error code otherwise
 */
int lzfw_symlink(vfs_t *p_vfs, creden_t *p_cred, inogen_t parent, const char *psz_name, const char *psz_link, inogen_t *p_symlink);

/**
 * Read the content of a symbolic link
 * @param p_vfs: the virtual file system
 * @param p_cred: the credentials of the user
 * @param symlink: the symlink to read
 * @param psz_content: return the content of the symlink
 * @param content_size: size of the buffer
 * @return 0 on success, the error code otherwise
 */
int lzfw_readlink(vfs_t *p_vfs, creden_t *p_cred, inogen_t symlink, char *psz_content, size_t content_size);

/**
 * Create a hard link
 * @param p_vfs: the virtual file system
 * @param p_cred: the credentials of the user
 * @param parent: the parent directory
 * @param target: the target object
 * @param psz_name: name of the link
 * @return 0 on success, the error code otherwise
 */
int lzfw_link(vfs_t *p_vfs, creden_t *p_cred, inogen_t parent, inogen_t target, const char *psz_name);

/**
 * Unlink the given file
 * @param p_vfs: the virtual filesystem
 * @param p_cred: the credentials of the user
 * @param parent: the parent directory
 * @param psz_filename: name of the file to unlink
 * @return 0 on success, the error code otherwise
 */
int lzfw_unlink(vfs_t *p_vfs, creden_t *p_cred, inogen_t parent, const char *psz_filename);

/**
 * Unlink the given file w/parent vnode
 * @param p_vfs: the virtual filesystem
 * @param p_cred: the credentials of the user
 * @param parent: the parent directory
 * @param psz_filename: name of the file to unlink
 * @param flags: flags
 * @return 0 on success, the error code otherwise
 */
int lzfw_unlinkat(vfs_t *p_vfs, creden_t *p_cred, vnode_t* parent, const char *psz_filename, int flags);

/**
 * Move name from parent (directory) to new_parent (directory)
 * @param p_vfs: the virtual filesystem
 * @param p_cred: the credentials of the user
 * @param parent: the current parent directory
 * @param psz_name: current name of the file
 * @param new_parent: the new parents directory
 * @param psz_newname: new file name
 * @return 0 on success, the error code otherwise
 */
int lzfw_renameat(vfs_t *p_vfs, creden_t *p_cred, vnode_t *parent, const char *psz_name, vnode_t *new_parent, const char *psz_newname);

/**
 * Set the size of the given file
 * @param p_vfs: the virtual filesystem
 * @param p_cred: the credentials of the user
 * @param file: the file to truncate
 * @param size: the new size
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_truncate(vfs_t *p_vfs, creden_t *p_cred, inogen_t file, size_t size);

/**
 * Zero a region of a file (convert to--or extend from EOF--a hole
 * using VOP_SPACE)
 * @param p_vfs: the virtual filesystem
 * @param p_cred: the credentials of the user
 * @param vnode: the file to truncate
 * @param length: length of the region
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_zero(vfs_t *p_vfs, creden_t *cred, vnode_t *vnode,
	      off_t offset, size_t length);

#endif /* LIBZFSWRAP_H */
