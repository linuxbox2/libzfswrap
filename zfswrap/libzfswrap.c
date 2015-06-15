#include <umem.h>
#include <libsolkerncompat.h>

#include <sys/types.h>
#include <sys/dirent.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <sys/vfs.h>
#include <sys/systm.h>
#include <libzfs.h>
#include <libzfs_impl.h>
#include <sys/dmu_objset.h>
#include <sys/dsl_dataset.h>
#include <sys/zfs_znode.h>
#include <sys/mode.h>
#include <sys/fcntl.h>

#include <limits.h>

#include "zfs_ioctl.h"
#include <ctype.h>

#include "libzfswrap.h"
#include "libzfswrap_utils.h"


extern int zfs_vfsinit(int fstype, char *name);

static int getattr_helper(vfs_t *p_vfs, creden_t *p_cred,
			  inogen_t object, struct stat *p_stat,
			  uint64_t *p_gen, int *p_type);

/**
 * Initialize the libzfswrap library
 * @return a handle to the library, NULL in case of error
 */
lzfw_handle_t *lzfw_init()
{
  // Create the cache directory if it does not exist
  mkdir(ZPOOL_CACHE_DIR, 0700);

  init_mmap();
  libsolkerncompat_init();
  zfs_vfsinit(zfstype, NULL);
  zfs_ioctl_init();
  libzfs_handle_t *p_zhd = libzfs_init();

  if(!p_zhd)
    libsolkerncompat_exit();

  return (lzfw_handle_t*)p_zhd;
}

/**
 * Uninitialize the library
 * @param p_zhd: the libzfswrap handle
 */
void lzfw_exit(lzfw_handle_t *p_zhd)
{
  libzfs_fini((libzfs_handle_t*)p_zhd);
  libsolkerncompat_exit();
}

/**
 * Create a zpool
 * @param p_zhd: the libzfswrap handle
 * @param psz_name: the name of the zpool
 * @param psz_type: type of the zpool (mirror, raidz, raidz([1,255])
 * @param ppsz_error: the error message (if any)
 * @return 0 on success, the error code otherwise
 */
int lzfw_zpool_create(lzfw_handle_t *p_zhd, const char *psz_name,
		      const char *psz_type, const char **ppsz_dev,
		      size_t i_dev, const char **ppsz_error)
{
  int i_error;
  nvlist_t *pnv_root    = NULL;
  nvlist_t *pnv_fsprops = NULL;
  nvlist_t *pnv_props   = NULL;

  const char *pool_type =
    (psz_type == "default") ? "" : psz_type;

  // Create the zpool
  if(!(pnv_root = lzwu_make_root_vdev(pool_type, ppsz_dev, i_dev,
				      ppsz_error)))
    return 1;

  i_error = libzfs_zpool_create((libzfs_handle_t*)p_zhd, psz_name,
				pnv_root, pnv_props, pnv_fsprops,
				ppsz_error);

  nvlist_free(pnv_props);
  nvlist_free(pnv_fsprops);
  nvlist_free(pnv_root);
  return i_error;
}

/**
 * Destroy the given zpool
 * @param p_zhd: the libzfswrap handle
 * @param psz_name: zpool name
 * @param b_force: force the unmount process or not
 * @param ppsz_error: the error message (if any)
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_zpool_destroy(lzfw_handle_t *p_zhd, const char *psz_name,
		       int b_force, const char **ppsz_error)
{
  zpool_handle_t *p_zpool;
  int i_error;

  /** Open the zpool */
  if((p_zpool =
      libzfs_zpool_open_canfail((libzfs_handle_t*)p_zhd,
				psz_name, ppsz_error)) == NULL) {
    /** If the name contain a '/' redirect the user to zfs_destroy */
    if(strchr(psz_name, '/') != NULL)
      *ppsz_error = "the pool name cannot contain a '/'";
    return 1;
  }

  i_error = spa_destroy((char*)psz_name);
  libzfs_zpool_close(p_zpool);

  return i_error;
}

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
		   size_t i_dev, const char **ppsz_error)
{
  zpool_handle_t *p_zpool;
  nvlist_t *pnv_root;
  int i_error;

  if(!(p_zpool = libzfs_zpool_open((libzfs_handle_t*)p_zhd, psz_zpool,
				   ppsz_error)))
    return 1;

  if(!(pnv_root = lzwu_make_root_vdev(psz_type, ppsz_dev, i_dev,
				      ppsz_error))) {
      libzfs_zpool_close(p_zpool);
      return 2;
    }

  i_error = libzfs_zpool_vdev_add(psz_zpool, pnv_root);

  nvlist_free(pnv_root);
  libzfs_zpool_close(p_zpool);

  return i_error;
}

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
		      const char **ppsz_error)
{
  zpool_handle_t *p_zpool;
  size_t i;
  int i_error;

  if(!(p_zpool = libzfs_zpool_open((libzfs_handle_t*)p_zhd, psz_zpool,
				   ppsz_error)))
    return 1;

  for(i = 0; i < i_vdevs; i++) {
    if((i_error = libzfs_zpool_vdev_remove(p_zpool, ppsz_dev[i],
					   ppsz_error)))
      break;
  }

  libzfs_zpool_close(p_zpool);

  return i_error;
}

/**
 * Attach the given device to the given vdev in the zpool
 * @param p_zhd: the libzfswrap handle
 * @param psz_zpool: the zpool name
 * @param psz_current_dev: the device to use as an attachment point
 * @param psz_new_dev: the device to attach
 * @param i_replacing: replacing the device ?
 * @param ppsz_error: the error message if any
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_zpool_attach(lzfw_handle_t *p_zhd, const char *psz_zpool,
		      const char *psz_current_dev,
		      const char *psz_new_dev, int i_replacing,
		      const char **ppsz_error)
{
  zpool_handle_t *p_zpool;
  nvlist_t *pnv_root;
  int i_error;

  if(!(p_zpool = libzfs_zpool_open((libzfs_handle_t*)p_zhd, psz_zpool,
				   ppsz_error)))
    return 1;

  if(!(pnv_root = lzwu_make_root_vdev("", &psz_new_dev, 1,
				      ppsz_error))) {
      libzfs_zpool_close(p_zpool);
      return 2;
    }

  i_error = libzfs_zpool_vdev_attach(p_zpool, psz_current_dev,
				     pnv_root, i_replacing,
				     ppsz_error);

  nvlist_free(pnv_root);
  libzfs_zpool_close(p_zpool);

  return i_error;
}

/**
 * Detach the given vdevs from the zpool
 * @param p_zhd: the libzfswrap handle
 * @param psz_zpool: the zpool name
 * @param psz_dev: the device to detach
 * @param ppsz_error: the error message if any
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_zpool_detach(lzfw_handle_t *p_zhd, const char *psz_zpool,
		      const char *psz_dev, const char **ppsz_error)
{
  zpool_handle_t *p_zpool;
  int i_error;

  if(!(p_zpool = libzfs_zpool_open((libzfs_handle_t*)p_zhd, psz_zpool,
				   ppsz_error)))
    return 1;

  i_error = libzfs_zpool_vdev_detach(p_zpool, psz_dev, ppsz_error);

  libzfs_zpool_close(p_zpool);
  return i_error;
}

/**
 * Callback called for each pool, that print the information
 * @param p_zpool: a pointer to the current zpool
 * @param p_data: a obscure data pointer (the zpool property list)
 * @return 0
 */
static int lzfw_zpool_list_callback(zpool_handle_t *p_zpool,
				    void *p_data)
{
  zprop_list_t *p_zpl = (zprop_list_t*)p_data;
  char property[ZPOOL_MAXPROPLEN];
  char *psz_prop;
  boolean_t first = B_TRUE;

  for(; p_zpl; p_zpl = p_zpl->pl_next) {
    boolean_t right_justify = B_FALSE;
    if(first)
      first = B_FALSE;
    else
      printf("  ");

    if(p_zpl->pl_prop != ZPROP_INVAL) {
      if(zpool_get_prop(p_zpool, p_zpl->pl_prop, property,
			sizeof(property), NULL))
	psz_prop = "-";
      else
	psz_prop = property;
      right_justify = zpool_prop_align_right(p_zpl->pl_prop);
    }
    else
      psz_prop = "-";

    // Print the string
    if(p_zpl->pl_next == NULL && !right_justify)
      printf("%s", psz_prop);
    else if(right_justify)
      printf("%*s", (int)p_zpl->pl_width, psz_prop);
    else
      printf("%-*s", (int)p_zpl->pl_width, psz_prop);
  }
  printf("\n");

  return 0;
}

/**
 * List the available zpools
 * @param p_zhd: the libzfswrap handle
 * @param psz_props: the properties to retrieve
 * @param ppsz_error: the error message if any
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_zpool_list(lzfw_handle_t *p_zhd, const char *psz_props,
		    const char **ppsz_error)
{
  zprop_list_t *p_zprop_list = NULL;
  static char psz_default_props[] = "name,size,allocated,free,capacity,dedupratio,health,altroot";
  if(zprop_get_list((libzfs_handle_t*)p_zhd,
		    psz_props ? psz_props : psz_default_props,
		    &p_zprop_list, ZFS_TYPE_POOL)) {
    *ppsz_error = "unable to get the list of properties";
    return 1;
  }

  lzwu_zpool_print_list_header(p_zprop_list);
  libzfs_zpool_iter((libzfs_handle_t*)p_zhd, lzfw_zpool_list_callback,
		    p_zprop_list, ppsz_error);
  zprop_free_list(p_zprop_list);

  return 0;
}

static int lzfw_zpool_status_callback(zpool_handle_t *zhp, void *data)
{
  status_cbdata_t *cbp = data;
  nvlist_t *config, *nvroot;
  char *msgid;
  int reason;
  const char *health;
  uint_t c;
  vdev_stat_t *vs;

  config = zpool_get_config(zhp, NULL);
  reason = zpool_get_status(zhp, &msgid);
  cbp->cb_count++;

  /*
   * If we were given 'zpool status -x', only report those pools with
   * problems.
   */
  if(reason == ZPOOL_STATUS_OK && cbp->cb_explain) {
    if(!cbp->cb_allpools) {
	  printf("pool '%s' is healthy\n", zpool_get_name(zhp));
	  if(cbp->cb_first)
	    cbp->cb_first = B_FALSE;
    }
      return 0;
  }

  if (cbp->cb_first)
    cbp->cb_first = B_FALSE;
  else
    printf("\n");

  assert(nvlist_lookup_nvlist(config, ZPOOL_CONFIG_VDEV_TREE,
			      &nvroot) == 0);
  assert(nvlist_lookup_uint64_array(nvroot, ZPOOL_CONFIG_STATS,
				    (uint64_t **)&vs, &c) == 0);
  health = zpool_state_to_name(vs->vs_state, vs->vs_aux);

  printf("  pool: %s\n", zpool_get_name(zhp));
  printf(" state: %s\n", health);

  switch (reason) {
  case ZPOOL_STATUS_MISSING_DEV_R:
    printf("status: One or more devices could not be opened. "
	   "Sufficient replicas exist for\n\tthe pool to "
	   "continue functioning in a degraded state.\n");
    printf("action: Attach the missing device and "
	   "online it using 'zpool online'.\n");
    break;

  case ZPOOL_STATUS_MISSING_DEV_NR:
    printf("status: One or more devices could not "
	   "be opened.  There are insufficient\n\treplicas for the "
	   "pool to continue functioning.\n");
    printf("action: Attach the missing device and "
	   "online it using 'zpool online'.\n");
    break;

  case ZPOOL_STATUS_CORRUPT_LABEL_R:
    printf("status: One or more devices could not "
	   "be used because the label is missing or\n\tinvalid.  "
	   "Sufficient replicas exist for the pool to continue\n\t"
	   "functioning in a degraded state.\n");
    printf("action: Replace the device using 'zpool replace'.\n");
    break;

  case ZPOOL_STATUS_CORRUPT_LABEL_NR:
    printf("status: One or more devices could not "
	   "be used because the label is missing \n\tor invalid.  "
	   "There are insufficient replicas for the pool to "
	   "continue\n\tfunctioning.\n");
    zpool_explain_recover(zpool_get_handle(zhp),
			  zpool_get_name(zhp), reason, config);
    break;

  case ZPOOL_STATUS_FAILING_DEV:
    printf("status: One or more devices has "
	   "experienced an unrecoverable error.  An\n\tattempt was "
	   "made to correct the error.  Applications are "
	   "unaffected.\n");
    printf("action: Determine if the device needs "
	   "to be replaced, and clear the errors\n\tusing "
	   "'zpool clear' or replace the device with 'zpool "
	   "replace'.\n");
    break;

  case ZPOOL_STATUS_OFFLINE_DEV:
    printf("status: One or more devices has "
	   "been taken offline by the administrator.\n\tSufficient "
	   "replicas exist for the pool to continue functioning in "
	   "a\n\tdegraded state.\n");
    printf("action: Online the device using "
	   "'zpool online' or replace the device with\n\t'zpool "
	   "replace'.\n");
    break;

  case ZPOOL_STATUS_REMOVED_DEV:
    printf("status: One or more devices has "
	   "been removed by the administrator.\n\tSufficient "
	   "replicas exist for the pool to continue functioning in "
	   "a\n\tdegraded state.\n");
    printf("action: Online the device using "
	   "'zpool online' or replace the device with\n\t'zpool "
	   "replace'.\n");
    break;


  case ZPOOL_STATUS_RESILVERING:
    printf("status: One or more devices is "
	   "currently being resilvered.  The pool will\n\tcontinue "
	   "to function, possibly in a degraded state.\n");
    printf("action: Wait for the resilver to complete.\n");
    break;

  case ZPOOL_STATUS_CORRUPT_DATA:
    printf("status: One or more devices has "
	   "experienced an error resulting in data\n\tcorruption.  "
	   "Applications may be affected.\n");
    printf("action: Restore the file in question "
	   "if possible.  Otherwise restore the\n\tentire pool from "
	   "backup.\n");
    break;

  case ZPOOL_STATUS_CORRUPT_POOL:
    printf("status: The pool metadata is corrupted "
	   "and the pool cannot be opened.\n");
    zpool_explain_recover(zpool_get_handle(zhp),
			  zpool_get_name(zhp), reason, config);
    break;

  case ZPOOL_STATUS_VERSION_OLDER:
    printf("status: The pool is formatted using an "
	   "older on-disk format.  The pool can\n\tstill be used, but "
	   "some features are unavailable.\n");
    printf("action: Upgrade the pool using 'zpool "
	   "upgrade'.  Once this is done, the\n\tpool will no longer "
	   "be accessible on older software versions.\n");
    break;

  case ZPOOL_STATUS_VERSION_NEWER:
    printf("status: The pool has been upgraded to a "
	   "newer, incompatible on-disk version.\n\tThe pool cannot "
	   "be accessed on this system.\n");
    printf("action: Access the pool from a system "
	   "running more recent software, or\n\trestore the pool from "
	   "backup.\n");
    break;

  case ZPOOL_STATUS_FAULTED_DEV_R:
    printf("status: One or more devices are "
	   "faulted in response to persistent errors.\n\tSufficient "
	   "replicas exist for the pool to continue functioning "
	   "in a\n\tdegraded state.\n");
    printf("action: Replace the faulted device, "
	   "or use 'zpool clear' to mark the device\n\trepaired.\n");
    break;

  case ZPOOL_STATUS_FAULTED_DEV_NR:
    printf("status: One or more devices are "
	   "faulted in response to persistent errors.  There are "
	   "insufficient replicas for the pool to\n\tcontinue "
	   "functioning.\n");
    printf("action: Destroy and re-create the pool "
	   "from a backup source.  Manually marking the device\n"
	   "\trepaired using 'zpool clear' may allow some data "
	   "to be recovered.\n");
    break;

  case ZPOOL_STATUS_IO_FAILURE_WAIT:
  case ZPOOL_STATUS_IO_FAILURE_CONTINUE:
    printf("status: One or more devices are "
	   "faulted in response to IO failures.\n");
    printf("action: Make sure the affected devices "
	   "are connected, then run 'zpool clear'.\n");
    break;

  case ZPOOL_STATUS_BAD_LOG:
    printf("status: An intent log record "
	   "could not be read.\n"
	   "\tWaiting for adminstrator intervention to fix the "
	   "faulted pool.\n");
    printf("action: Either restore the affected "
	   "device(s) and run 'zpool online',\n"
	   "\tor ignore the intent log records by running "
	   "'zpool clear'.\n");
    break;

  default:
    /*
     * The remaining errors can't actually be generated, yet.
     */
    assert(reason == ZPOOL_STATUS_OK);
  }

  if(msgid != NULL)
    printf("   see: http://www.sun.com/msg/%s\n", msgid);

  if(config != NULL) {
    int namewidth;
    uint64_t nerr;
    nvlist_t **spares, **l2cache;
    uint_t nspares, nl2cache;


    printf(" scrub: ");
    lzwu_zpool_print_scrub_status(nvroot);

    namewidth = lzwu_zpool_max_width(cbp->p_zhd, zhp, nvroot, 0, 0);
    if(namewidth < 10)
      namewidth = 10;

    printf("config:\n\n");
    printf("\t%-*s  %-8s %5s %5s %5s\n", namewidth,
	   "NAME", "STATE", "READ", "WRITE", "CKSUM");
    lzwu_zpool_print_status_config(cbp->p_zhd, zhp,
				   zpool_get_name(zhp), nvroot,
				   namewidth, 0, B_FALSE);
    if(lzwu_num_logs(nvroot) > 0)
      lzwu_print_logs(cbp->p_zhd, zhp, nvroot, namewidth, B_TRUE);
    if(nvlist_lookup_nvlist_array(nvroot, ZPOOL_CONFIG_L2CACHE,
				  &l2cache, &nl2cache) == 0)
      lzwu_print_l2cache(cbp->p_zhd, zhp, l2cache, nl2cache,
			 namewidth);

    if(nvlist_lookup_nvlist_array(nvroot, ZPOOL_CONFIG_SPARES,
				  &spares, &nspares) == 0)
      lzwu_print_spares(cbp->p_zhd, zhp, spares, nspares, namewidth);

    if(nvlist_lookup_uint64(config, ZPOOL_CONFIG_ERRCOUNT,
			    &nerr) == 0) {
      nvlist_t *nverrlist = NULL;

      /*
       * If the approximate error count is small, get a
       * precise count by fetching the entire log and
       * uniquifying the results.
       */
      if (nerr > 0 && nerr < 100 && !cbp->cb_verbose &&
	  zpool_get_errlog(zhp, &nverrlist) == 0) {
	nvpair_t *elem;

	elem = NULL;
	nerr = 0;
	while ((elem = nvlist_next_nvpair(nverrlist,
					  elem)) != NULL) {
	  nerr++;
	}
      }
      nvlist_free(nverrlist);

      printf("\n");

      if(nerr == 0)
	printf("errors: No known data errors\n");
      else if (!cbp->cb_verbose)
	printf("errors: %llu data errors, use '-v' for a list\n",
	       (u_longlong_t)nerr);
      else
	lzwu_print_error_log(zhp);
    }

    if(cbp->cb_dedup_stats)
      lzwu_print_dedup_stats(config);
  }
  else {
    printf("config: The configuration cannot be determined.\n");
  }
  return (0);
}

/**
 * Print the status of the available zpools
 * @param p_zhd: the libzfswrap handle
 * @param ppsz_error: the error message if any
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_zpool_status(lzfw_handle_t *p_zhd, const char **ppsz_error)
{
  status_cbdata_t cb_data;
  cb_data.cb_count = 0;
  cb_data.cb_allpools = B_FALSE;
  cb_data.cb_verbose = B_FALSE;
  cb_data.cb_explain = B_FALSE;
  cb_data.cb_first = B_TRUE;
  cb_data.cb_dedup_stats = B_FALSE;
  cb_data.p_zhd = (libzfs_handle_t*)p_zhd;

  libzfs_zpool_iter((libzfs_handle_t*)p_zhd,
		    lzfw_zpool_status_callback, &cb_data, ppsz_error);

  return 0;
}

static int lzfw_zfs_list_callback(zfs_handle_t *p_zfs, void *data)
{
  zprop_list_t *pl = (zprop_list_t*)data;

  boolean_t first = B_TRUE;
  char property[ZFS_MAXPROPLEN];
  nvlist_t *userprops = zfs_get_user_props(p_zfs);
  nvlist_t *propval;
  char *propstr;
  boolean_t right_justify;
  int width;

  for(; pl != NULL; pl = pl->pl_next) {
    if(!first)
      printf("  ");
    else
      first = B_FALSE;

    if(pl->pl_prop != ZPROP_INVAL) {
      if(zfs_prop_get(p_zfs, pl->pl_prop, property,
		      sizeof (property), NULL, NULL, 0, B_FALSE) != 0)
	propstr = "-";
      else
	propstr = property;

      right_justify = zfs_prop_align_right(pl->pl_prop);
    }
    else if(zfs_prop_userquota(pl->pl_user_prop)) {
      if(zfs_prop_get_userquota(p_zfs, pl->pl_user_prop,
				property, sizeof (property), B_FALSE) != 0)
	propstr = "-";
      else
	propstr = property;
      right_justify = B_TRUE;
    }
    else {
      if(nvlist_lookup_nvlist(userprops,
			      pl->pl_user_prop, &propval) != 0)
	propstr = "-";
      else
	verify(nvlist_lookup_string(propval,
				    ZPROP_VALUE, &propstr) == 0);
      right_justify = B_FALSE;
    }

    width = pl->pl_width;

    /*
     * If this is being called in scripted mode, or if this is the
     * last column and it is left-justified, don't include a width
     * format specifier.
     */
    if((pl->pl_next == NULL && !right_justify))
      printf("%s", propstr);
    else if(right_justify)
      printf("%*s", width, propstr);
    else
      printf("%-*s", width, propstr);
  }

  printf("\n");

  return 0;
}

/**
 * Print the list of ZFS file systems and properties
 * @param p_zhd: the libzfswrap handle
 * @param psz_props: the properties to retrieve
 * @param ppsz_error: the error message if any
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_zfs_list(lzfw_handle_t *p_zhd, const char *psz_props,
		  const char **ppsz_error)
{
  zprop_list_t *p_zprop_list = NULL;
  static char psz_default_props[] =
    "name,used,available,referenced,mountpoint";
  if(zprop_get_list((libzfs_handle_t*)p_zhd,
		    psz_props ? psz_props : psz_default_props,
		    &p_zprop_list, ZFS_TYPE_DATASET)) {
    *ppsz_error = "Unable to get the list of properties";
    return 1;
  }

  lzwu_zfs_print_list_header(p_zprop_list);
  libzfs_zfs_iter((libzfs_handle_t*)p_zhd, lzfw_zfs_list_callback,
		  p_zprop_list, ppsz_error );
  zprop_free_list(p_zprop_list);

  return 0;
}

static boolean_t
dataset_name_hidden(const char *name)
{
  /*
   * Skip over datasets that are not visible in this zone,
   * internal datasets (which have a $ in their name), and
   * temporary datasets (which have a % in their name).
   */
  if (strchr(name, '$') != NULL)
    return (B_TRUE);
  if (strchr(name, '%') != NULL)
    return (B_TRUE);
  return (B_FALSE);
}

/**
 * Callback-based iteration over ZFS datasets
 * @param zhd: the libzfswrap handle
 * @param parent_ds_name: parent name (fully qualified, if not a pool/root)
 * @param func: function to call for each (non-hidden) dataset
 * @param arg: opaque argument which will be passed to func
 * @param ppsz_error: error message if any
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_datasets_iter(libzfs_handle_t *zhd,
		       const char *parent_ds_name,
		       zfs_iter_f func, void *arg,
		       const char **ppsz_error)
{
  char *p;
  objset_t *os;
  zfs_handle_t *a_zhp;
  char ds_name[MAXNAMELEN];
  int error = 0;

  strlcpy(ds_name, parent_ds_name, sizeof(ds_name));

  if (error = dmu_objset_hold(ds_name, FTAG, &os)) {
    if (error == ENOENT)
      error = ESRCH;
    return (error);
  }

  p = strrchr(ds_name, '/');
  if (p == NULL || p[1] != '\0')
    (void) strlcat(ds_name, "/", sizeof (ds_name)); /* XXX */
  p = ds_name + strlen(ds_name);

  uint64_t cookie = 0;
  int len = sizeof (ds_name) - (p - ds_name);
  while (dmu_dir_list_next(os, len, p, NULL, &cookie) == 0) {
    (void) dmu_objset_prefetch(p, NULL);
    /* ignore internal datasets (e.g., $ORIGIN) */
    if (! dataset_name_hidden(ds_name)) {
      a_zhp = libzfs_make_dataset_handle(zhd, ds_name);
      if (! a_zhp) {
	*ppsz_error = "Unable to create the zfs_handle";
	error = EINVAL;
	goto rele;
      }
      /* call supplied function */
      error = func(a_zhp, arg);
      /* release libzfs handle */
      libzfs_zfs_close(a_zhp);
    }
  }
 rele:
  dmu_objset_rele(os, FTAG);
  return error;
}

/**
 * Create a snapshot of the given ZFS file system
 * @param p_zhd: the libzfswrap handle
 * @param psz_zfs: name of the file system
 * @param psz_snapshot: name of the snapshot
 * @param ppsz_error: the error message if any
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_zfs_snapshot(lzfw_handle_t *p_zhd, const char *psz_zfs,
		      const char *psz_snapshot,
		      const char **ppsz_error)
{
  zfs_handle_t *p_zfs;
  int i_error;

  /**@TODO: check the name of the filesystem and snapshot*/

  if(!(p_zfs = libzfs_zfs_open((libzfs_handle_t*)p_zhd, psz_zfs, ZFS_TYPE_FILESYSTEM | ZFS_TYPE_VOLUME, ppsz_error)))
    return ENOENT;

  if((i_error = dmu_objset_snapshot(p_zfs->zfs_name,
				    (char*)psz_snapshot, NULL, 0)))
    *ppsz_error = "Unable to create the snapshot";

  libzfs_zfs_close(p_zfs);
  return i_error;
}

/**
 * Destroy a snapshot of the given ZFS file system
 * @param p_zhd: the libzfswrap handle
 * @param psz_zfs: name of the file system
 * @param psz_snapshot: name of the snapshot
 * @param ppsz_error: the error message if any
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_zfs_snapshot_destroy(lzfw_handle_t *p_zhd,
			      const char *psz_zfs,
			      const char *psz_snapshot,
			      const char **ppsz_error)
{
  zpool_handle_t *p_zpool;
  int i_error;

  /** Open the zpool */
  if((p_zpool = libzfs_zpool_open_canfail((libzfs_handle_t*)p_zhd,
					  psz_zfs,
					  ppsz_error)) == NULL) {
      /** If the name contain a '/' redirect the user to zfs_destroy */
      if(strchr(psz_zfs, '/') != NULL)
	*ppsz_error = "the pool name cannot contain a '/'";
      return 1;
    }

  if((i_error = dmu_snapshots_destroy(psz_zfs, psz_snapshot, B_TRUE)))
    *ppsz_error = "Unable to destroy the snapshot";

  libzfs_zpool_close(p_zpool);
  return i_error;
}

/**
 * Dataset support
 */

/* zfs_ioctl.c */
static void
zfs_create_cb(objset_t *os, void *arg, cred_t *cr, dmu_tx_t *tx)
{
  zfs_creat_t *zct = arg;
  zfs_create_fs(os, cr, zct->zct_zplprops, tx); /* zfs_znode.h */
}

/**
 * Create a new dataset (filesystem).
 * @param p_zhd: the libzfswrap handle
 * @param psz_zfs: name of the file system
 * @param type: dataset type (e.g., ZFS_TYPE_FILESYSTEM)
 * @param ppsz_error: the error message if any
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_dataset_create(lzfw_handle_t *p_zhd, const char *psz_zfs,
			int type, const char **ppsz_error)
{
  zfs_creat_t zct;
  nvlist_t *nvprops = NULL;
  dmu_objset_type_t os_type;
  int i_error = 0;

  /* Check the zpool name */
  if (! libzfs_dataset_name_valid(psz_zfs, ppsz_error))
    return EINVAL;

  assert(type == ZFS_TYPE_FILESYSTEM); /* no ZVOLs currently */
  switch(type) {
  case ZFS_TYPE_FILESYSTEM:
  default:
    os_type = DMU_OST_ZFS; /* beware, several OST types exist */
    break;
  };

  zct.zct_zplprops = NULL;
  zct.zct_props = nvprops;

  /*
   * We have to have normalization and
   * case-folding flags correct when we do the
   * file system creation, so go figure them out
   * now.
   */
  boolean_t is_insensitive = B_FALSE;
  VERIFY(nvlist_alloc(&zct.zct_zplprops,
		      NV_UNIQUE_NAME, KM_SLEEP) == 0);
  i_error = zfs_fill_zplprops(psz_zfs, nvprops, zct.zct_zplprops,
			      &is_insensitive);
  if (i_error != 0) {
    nvlist_free(nvprops);
    nvlist_free(zct.zct_zplprops);
    return (i_error);
  }

  i_error = dmu_objset_create(psz_zfs, os_type,
			      is_insensitive ? DS_FLAG_CI_DATASET : 0,
			      zfs_create_cb, &zct);
  nvlist_free(zct.zct_zplprops);

  /*
   * It would be nice to do this atomically.
   */
  if (i_error == 0) {
    i_error = zfs_set_prop_nvlist(psz_zfs, ZPROP_SRC_LOCAL,
				  nvprops, NULL);
    if (i_error != 0)
      (void) dmu_objset_destroy(psz_zfs, B_FALSE);
  }
  nvlist_free(nvprops);

  return i_error;
}

/**
 * Destroy dataset (filesystem).
 * @param p_zhd: the libzfswrap handle
 * @param psz_zfs: name of the file system
 * @param ppsz_error: the error message if any
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_dataset_destroy(lzfw_handle_t *p_zhd, const char *psz_zfs,
			 const char **ppsz_error)
{
  int i_error = 0;

  /* XXX fail if mounted! */
  i_error = dmu_objset_destroy(psz_zfs, B_FALSE /* defer destroy */);

  return i_error;
}

/**
 * List the available snapshots for the given zfs
 * @param p_zhd: the libzfswrap handle
 * @param psz_zfs: name of the file system
 * @param ppsz_error: the error message if any
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_zfs_list_snapshot(lzfw_handle_t *p_zhd, const char *psz_zfs,
			   const char **ppsz_error)
{
  zprop_list_t *p_zprop_list = NULL;
  static char psz_default_props[] =
    "name,used,available,referenced,mountpoint";
  if(zprop_get_list((libzfs_handle_t*)p_zhd, psz_default_props,
		    &p_zprop_list, ZFS_TYPE_DATASET)) {
      *ppsz_error = "Unable to get the list of properties";
      return 1;
    }

  lzwu_zfs_print_list_header(p_zprop_list);

  return libzfs_zfs_snapshot_iter((libzfs_handle_t*)p_zhd, psz_zfs,
				  lzfw_zfs_list_callback,
				  p_zprop_list, ppsz_error);
}

typedef struct
{
  char **ppsz_names;
  size_t i_num;
}callback_data_t;

static int lzfw_zfs_get_list_snapshots_callback(zfs_handle_t *p_zfs,
						void *data)
{
  callback_data_t *p_cb = (callback_data_t*)data;
  p_cb->i_num++;
  p_cb->ppsz_names = realloc(p_cb->ppsz_names,
			     p_cb->i_num*sizeof(char*));
  p_cb->ppsz_names[p_cb->i_num-1] = strdup(p_zfs->zfs_name);
  return 0;
}

/**
 * Return the list of snapshots for the given zfs in an array of strings
 * @param p_zhd: the libzfswrap handle
 * @param psz_zfs: name of the file system
 * @param pppsz_snapshots: the array of snapshots names
 * @param ppsz_error: the error message if any
 * @return the number of snapshots in case of success, -1 otherwise
 */
int lzfw_zfs_get_list_snapshots(lzfw_handle_t *p_zhd,
				const char *psz_zfs,
				char ***pppsz_snapshots,
				const char **ppsz_error)
{
  callback_data_t cb = { .ppsz_names = NULL, .i_num = 0 };
  if(libzfs_zfs_snapshot_iter((libzfs_handle_t*)p_zhd, psz_zfs,
			      lzfw_zfs_get_list_snapshots_callback,
			      &cb, ppsz_error))
    return -1;

  *pppsz_snapshots = cb.ppsz_names;
  return cb.i_num;
}

extern vfsops_t *zfs_vfsops;
/**
 * Mount the given file system
 * @param psz_zpool: the pool to mount
 * @param psz_dir: the directory to mount
 * @param psz_options: options for the mounting point
 * @return the vitual file system
 */
vfs_t *lzfw_mount(const char *psz_zpool, const char *psz_dir,
		       const char *psz_options)
{
  vfs_t *p_vfs = calloc(1, sizeof(vfs_t));
  if(!p_vfs)
    return NULL;

  VFS_INIT(p_vfs, zfs_vfsops, 0);
  VFS_HOLD(p_vfs);

  struct mounta uap = {
    .spec = (char*)psz_zpool,
    .dir = (char*)psz_dir,
    .flags = 0 | MS_SYSSPACE,
    .fstype = "zfs-wrap",
    .dataptr = "",
    .datalen = 0,
    .optptr = (char*)psz_options,
    .optlen = strlen(psz_options)
  };

  cred_t cred = { .cr_uid = 0, .cr_gid = 0 };
  int i_error = VFS_MOUNT(p_vfs, rootdir, &uap, &cred);
  if(i_error)
    {
      free(p_vfs);
      return NULL;
    }
  return p_vfs;
}

/**
 * Get the root object of a file system
 * @param p_vfs: the virtual filesystem
 * @param p_root: return the root object
 * @return 0 on success, the error code otherwise
 */
int lzfw_getroot(vfs_t *p_vfs, inogen_t *root)
{
  zfsvfs_t *zfsvfs = p_vfs->vfs_data;
  znode_t *znode;
  int i_error;

  ZFS_ENTER(zfsvfs);
  if((i_error = zfs_zget(zfsvfs, 3, &znode, B_TRUE))) {
      ZFS_EXIT(zfsvfs);
      return i_error;
  }
  ASSERT(znode != NULL);

  // Get the generation
  root->inode = 3;
  root->generation = znode->z_phys->zp_gen;

  VN_RELE(ZTOV(znode));

  ZFS_EXIT(zfsvfs);

  return 0;
}

/**
 * Unmount the given file system
 * @param p_vfs: the virtual file system
 * @param b_force: force the unmount ?
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_umount(vfs_t *p_vfs, int b_force)
{
  int i_error;
  cred_t cred = { .cr_uid = 0, .cr_gid = 0 };

  VFS_SYNC(p_vfs, 0, &cred);
  if((i_error = VFS_UNMOUNT(p_vfs,
			    b_force ? MS_FORCE : 0, &cred))) {
    return i_error;
  }

  assert(b_force || p_vfs->vfs_count == 1);
  return 0;
}

/**
 * Get some more information about the file system
 * @param p_vfs: the virtual file system
 * @param p_stats: the statistics
 * @return 0 in case of success, -1 otherwise
 */
int lzfw_statfs(vfs_t *p_vfs, struct statvfs *p_statvfs)
{
  //FIXME: no ZFS_ENTER ??
  struct statvfs64 zfs_stats = { 0 };
  int i_error;
  if((i_error = VFS_STATVFS(p_vfs, &zfs_stats)))
    return i_error;

  p_statvfs->f_bsize = zfs_stats.f_frsize;
  p_statvfs->f_frsize = zfs_stats.f_frsize;
  p_statvfs->f_blocks = zfs_stats.f_blocks;
  p_statvfs->f_bfree = zfs_stats.f_bfree;
  p_statvfs->f_bavail = zfs_stats.f_bavail;
  p_statvfs->f_files = zfs_stats.f_files;
  p_statvfs->f_ffree = zfs_stats.f_ffree;
  p_statvfs->f_favail = zfs_stats.f_favail;
  p_statvfs->f_fsid = zfs_stats.f_fsid;
  p_statvfs->f_flag = zfs_stats.f_flag;
  p_statvfs->f_namemax = zfs_stats.f_namemax;

  return 0;
}

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
int lzfw_lookup(vfs_t *p_vfs, creden_t *p_cred, inogen_t parent,
		const char *psz_name, inogen_t *object, int *p_type)
{
  if(strlen(psz_name) >= MAXNAMELEN)
    return -1;

  zfsvfs_t *zfsvfs = p_vfs->vfs_data;
  znode_t *parent_znode;
  int i_error;

  ZFS_ENTER(zfsvfs);

  if((i_error = zfs_zget(zfsvfs, parent.inode, &parent_znode, B_TRUE))) {
    ZFS_EXIT(zfsvfs);
    return i_error;
  }

  ASSERT(parent_znode != NULL);

  // Check the parent generation
  if(parent_znode->z_phys->zp_gen != parent.generation) {
    VN_RELE(ZTOV(parent_znode));
    ZFS_EXIT(zfsvfs);
    return ENOENT;
  }

  vnode_t *parent_vnode = ZTOV(parent_znode);
  ASSERT(parent_vnode != NULL);

  vnode_t *vnode = NULL;
  if((i_error = VOP_LOOKUP(parent_vnode, (char*)psz_name, &vnode,
			   NULL, 0, NULL, (cred_t*)p_cred, NULL, NULL,
			   NULL))) {
    VN_RELE(parent_vnode);
    ZFS_EXIT(zfsvfs);
    return i_error;
  }

  object->inode = VTOZ(vnode)->z_id;
  object->generation = VTOZ(vnode)->z_phys->zp_gen;
  *p_type = VTTOIF(vnode->v_type);

  VN_RELE(vnode);
  VN_RELE(parent_vnode);

  ZFS_EXIT(zfsvfs);

  return 0;
}

/**
 * Lookup name relative to an open directory vnode
 * @param p_vfs: the virtual file system
 * @param p_cred: the credentials of the user
 * @param parent: the parent file object
 * @param psz_name: filename
 * @param p_object: return the object node and generation
 * @param p_type: return the object type
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_lookupnameat(vfs_t *p_vfs, creden_t *p_cred,
		      vnode_t *parent, const char *psz_name,
		      inogen_t *object, int *p_type)
{
  zfsvfs_t *zfsvfs = p_vfs->vfs_data;
  int i_error;

  vnode_t *vnode = NULL;

  ZFS_ENTER(zfsvfs);

  i_error = VOP_LOOKUP(parent, (char*)psz_name, &vnode,
		       NULL, 0, NULL, (cred_t*)p_cred, NULL, NULL,
		       NULL);
  if (i_error) {
    ZFS_EXIT(zfsvfs);
    return i_error;
  }

  object->inode = VTOZ(vnode)->z_id;
  object->generation = VTOZ(vnode)->z_phys->zp_gen;
  *p_type = VTTOIF(vnode->v_type);

  VN_RELE(vnode);
  ZFS_EXIT(zfsvfs);

  return 0;
}

/**
 * Test the access right of the given file
 * @param p_vfs: the virtual filesystem
 * @param p_cred: the credentials of the user
 * @param object: the object
 * @param mask: the rights to check
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_access(vfs_t *p_vfs, creden_t *p_cred, inogen_t object,
		int mask)
{
  zfsvfs_t *zfsvfs = p_vfs->vfs_data;
  znode_t *p_znode;
  int i_error;

  ZFS_ENTER(zfsvfs);
  if((i_error = zfs_zget(zfsvfs, object.inode, &p_znode, B_TRUE)))
    {
      ZFS_EXIT(zfsvfs);
      return i_error;
    }
  ASSERT(p_znode);
  // Check the generation
  if(p_znode->z_phys->zp_gen != object.generation)
    {
      VN_RELE(ZTOV(p_znode));
      ZFS_EXIT(zfsvfs);
      return ENOENT;
    }

  vnode_t *p_vnode = ZTOV(p_znode);
  ASSERT(p_vnode);

  int mode = 0;
  if(mask & R_OK)
    mode |= VREAD;
  if(mask & W_OK)
    mode |= VWRITE;
  if(mask & X_OK)
    mode |= VEXEC;

  i_error = VOP_ACCESS(p_vnode, mode, 0, (cred_t*)p_cred, NULL);

  VN_RELE(p_vnode);
  ZFS_EXIT(zfsvfs);

  return i_error;
}

/**
 * Open the given object
 * @param p_vfs: the virtual file system
 * @param p_cred: the credentials of the user
 * @param object: the object to open
 * @param i_flags: the opening flags
 * @param pp_vnode: the virtual node
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_open(vfs_t *p_vfs, creden_t *p_cred, inogen_t object,
	      int i_flags, vnode_t **pp_vnode)
{
  zfsvfs_t *zfsvfs = p_vfs->vfs_data;
  int mode = 0, flags = 0, i_error;
  lzwu_flags2zfs(i_flags, &flags, &mode);

  znode_t *p_znode;

  ZFS_ENTER(zfsvfs);
  if((i_error = zfs_zget(zfsvfs, object.inode, &p_znode, B_FALSE)))
    {
      ZFS_EXIT(zfsvfs);
      return i_error;
    }
  ASSERT(p_znode);
  // Check the generation
  if(p_znode->z_phys->zp_gen != object.generation)
    {
      VN_RELE(ZTOV(p_znode));
      ZFS_EXIT(zfsvfs);
      return ENOENT;
    }

  vnode_t *p_vnode = ZTOV(p_znode);
  ASSERT(p_vnode != NULL);

  vnode_t *p_old_vnode = p_vnode;

  // Check errors
  if((i_error = VOP_OPEN(&p_vnode, flags, (cred_t*)p_cred, NULL)))
    {
      //FIXME: memleak ?
      ZFS_EXIT(zfsvfs);
      return i_error;
    }
  ASSERT(p_old_vnode == p_vnode);

  ZFS_EXIT(zfsvfs);
  *pp_vnode = p_vnode;
  return 0;
}

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
int lzfw_openat(vfs_t *p_vfs, creden_t *p_cred,
		vnode_t *parent, const char *psz_name,
		unsigned int i_flags, mode_t mode,
		unsigned int *o_flags, vnode_t **pp_vnode)
{
  zfsvfs_t *zfsvfs = p_vfs->vfs_data;
  int i_mode = 0, flags = 0, i_error;
  lzwu_flags2zfs(i_flags, &flags, &i_mode);

  vnode_t *vnode = NULL;

  ZFS_ENTER(zfsvfs);

  i_error = VOP_LOOKUP(parent, (char*)psz_name, &vnode,
		       NULL, 0, NULL, (cred_t*)p_cred, NULL, NULL,
		       NULL);
  if (i_error) {
    if ((i_error == ENOENT) && (i_flags & O_CREAT)) {
      /* try VOP_CREATE */
      vattr_t vattr = { 0 };
      vattr.va_type = VREG;
      vattr.va_mode = mode;
      vattr.va_mask = AT_TYPE | AT_MODE;

      i_error = VOP_CREATE(parent, (char*)psz_name, &vattr,
			   (i_flags & O_EXCL) ? EXCL : NONEXCL,
			   mode, &vnode, (cred_t*)p_cred,
			   0, NULL, NULL);
      if (i_error) {
	ZFS_EXIT(zfsvfs);
	return i_error;
      }
      *o_flags |= LZFW_OFLAG_OPEN_CREATED;
    } else {
      ZFS_EXIT(zfsvfs);
      return i_error;
    }
  } else {
    if (i_flags & O_EXCL) {
      return EEXIST;
    }
  }

  vnode_t *old_vnode = vnode;

  // Check errors
  i_error = VOP_OPEN(&vnode, flags, (cred_t*)p_cred, NULL);
  if (i_error) {
    //FIXME: memleak ?
    VN_RELE(vnode); // XXX added (Matt)
    ZFS_EXIT(zfsvfs);
    return i_error;
  }
  ASSERT(old_vnode == vnode);

  ZFS_EXIT(zfsvfs);
  *pp_vnode = vnode;

  return 0;
}

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
		inogen_t *p_file)
{
  zfsvfs_t *zfsvfs = p_vfs->vfs_data;
  int i_error;
  znode_t *parent_znode;

  ZFS_ENTER(zfsvfs);
  if((i_error = zfs_zget(zfsvfs, parent.inode, &parent_znode,
			 B_FALSE))) {
    ZFS_EXIT(zfsvfs);
    return i_error;
  }
  ASSERT(parent_znode);
  // Check the generation
  if(parent_znode->z_phys->zp_gen != parent.generation) {
    VN_RELE(ZTOV(parent_znode));
      ZFS_EXIT(zfsvfs);
      return ENOENT;
  }

  vnode_t *parent_vnode = ZTOV(parent_znode);
  ASSERT(parent_vnode != NULL);

  vattr_t vattr = { 0 };
  vattr.va_type = VREG;
  vattr.va_mode = mode;
  vattr.va_mask = AT_TYPE | AT_MODE;

  vnode_t *new_vnode;

  if((i_error = VOP_CREATE(parent_vnode, (char*)psz_filename, &vattr,
			   NONEXCL, mode, &new_vnode, (cred_t*)p_cred,
			   0, NULL, NULL))) {
    VN_RELE(parent_vnode);
    ZFS_EXIT(zfsvfs);
    return i_error;
  }

  p_file->inode = VTOZ(new_vnode)->z_id;
  p_file->generation = VTOZ(new_vnode)->z_phys->zp_gen;

  VN_RELE(new_vnode);
  VN_RELE(parent_vnode);
  ZFS_EXIT(zfsvfs);
  return 0;
}

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
int lzfw_createat(vfs_t *vfs, creden_t *cred,
		  vnode_t *parent, const char *psz_filename,
		  mode_t mode, inogen_t *file)
{
  zfsvfs_t *zfsvfs = ((vfs_t*)vfs)->vfs_data;
  znode_t *parent_znode;
  int error;

  ZFS_ENTER(zfsvfs);

  parent_znode = VTOZ(parent);

  vattr_t vattr = { 0 };
  vattr.va_type = VREG;
  vattr.va_mode = mode;
  vattr.va_mask = AT_TYPE | AT_MODE;

  vnode_t *new_vnode;
  if((error = VOP_CREATE(parent, (char*)psz_filename, &vattr,
			 NONEXCL, mode, &new_vnode, (cred_t*)cred, 0,
			 NULL, NULL))) {
    ZFS_EXIT(zfsvfs);
    return error;
  }

  file->inode = VTOZ(new_vnode)->z_id;
  file->generation = VTOZ(new_vnode)->z_phys->zp_gen;

  VN_RELE(new_vnode);

  ZFS_EXIT(zfsvfs);

  return 0;
}

/**
 * Open a directory
 * @param p_vfs: the virtual filesystem
 * @param p_cred: the credentials of the user
 * @param directory: the directory to open
 * @param pp_vnode: the vnode to return
 * @return 0 on success, the error code otherwise
 */
int lzfw_opendir(vfs_t *p_vfs, creden_t *p_cred,
		 inogen_t directory, vnode_t **pp_vnode)
{
  zfsvfs_t *zfsvfs = p_vfs->vfs_data;
  int i_error;
  znode_t *znode;

  ZFS_ENTER(zfsvfs);
  if((i_error = zfs_zget(zfsvfs, directory.inode, &znode, B_TRUE))) {
    ZFS_EXIT(zfsvfs);
    return i_error;
  }
  ASSERT(znode);

  // Check the generation
  if(znode->z_phys->zp_gen != directory.generation) {
    VN_RELE(ZTOV(znode));
    ZFS_EXIT(zfsvfs);
    return ENOENT;
  }

  vnode_t *vnode = ZTOV(znode);
  ASSERT(vnode != NULL);

  // Check that we have a directory
  if(vnode->v_type != VDIR) {
    VN_RELE(vnode);
    ZFS_EXIT(zfsvfs);
    return ENOTDIR;
  }

  vnode_t *old_vnode = vnode;
  if((i_error = VOP_OPEN(&vnode, FREAD, (cred_t*)p_cred, NULL))) {
    VN_RELE(old_vnode);
    ZFS_EXIT(zfsvfs);
    return i_error;
  }
  ASSERT(old_vnode == vnode);

  ZFS_EXIT(zfsvfs);
  *pp_vnode = vnode;
  return 0;
}

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
int lzfw_readdir(vfs_t *p_vfs, creden_t *p_cred,
		 vnode_t *vnode, lzfw_entry_t *p_entries,
		 size_t size, off_t *cookie)
{
  zfsvfs_t *zfsvfs = p_vfs->vfs_data;

  // Check that the vnode is a directory
  if(vnode->v_type != VDIR)
    return ENOTDIR;

  iovec_t iovec;
  uio_t uio;
  uio.uio_iov = &iovec;
  uio.uio_iovcnt = 1;
  uio.uio_segflg = UIO_SYSSPACE;
  uio.uio_fmode = 0;
  uio.uio_llimit = RLIM64_INFINITY;
 
  off_t next_entry = *cookie;
  int eofp = 0;
  union {
    char buf[DIRENT64_RECLEN(MAXNAMELEN)];
    struct dirent64 dirent;
  } entry;

  ZFS_ENTER(zfsvfs);
  size_t index = 0;
  while(index < size) {
    iovec.iov_base = entry.buf;
    iovec.iov_len = sizeof(entry.buf);
    uio.uio_resid = iovec.iov_len;
    uio.uio_loffset = next_entry;

    /* TODO: do only one call for more than one entry ? */
    if(VOP_READDIR(vnode, &uio, (cred_t*)p_cred, &eofp, NULL, 0))
      break;

    // End of directory ?
    if(iovec.iov_base == entry.buf)
      break;

    // Copy the entry name
    strcpy(p_entries[index].psz_filename, entry.dirent.d_name);
    p_entries[index].object.inode = entry.dirent.d_ino;
    getattr_helper(p_vfs, p_cred, p_entries[index].object,
		   &(p_entries[index].stats),
		   &(p_entries[index].object.generation),
		   &(p_entries[index].type));

    // Go to the next entry
    next_entry = entry.dirent.d_off;
    index++;
  }
  ZFS_EXIT(zfsvfs);

  // Set the last element to NULL if we end before size elements
  if(index < size) {
    p_entries[index].psz_filename[0] = '\0';
    *cookie = 0;
  }
  else
    *cookie = next_entry;

  return 0;
}

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

static inline
int vattr_helper(vfs_t *vfs, creden_t *cred,
		 dir_iter_cb_context_t *cb_ctx)
{
  zfsvfs_t *zfsvfs = ((vfs_t*)vfs)->vfs_data;
  znode_t *znode;
  vnode_t *vnode;
  int error;

  error = zfs_zget(zfsvfs, cb_ctx->dirent->d_ino, &znode, B_FALSE);
  if (error)
    goto out;

  cb_ctx->gen = znode->z_phys->zp_gen;
  vnode = ZTOV(znode);

  if (cb_ctx->vattr) {
    cb_ctx->vattr->va_mask = AT_ALL;
    error = VOP_GETATTR(vnode, cb_ctx->vattr, 0, (cred_t*)cred, NULL);
    VN_RELE(vnode);
    if (error)
      goto out;
  }

 out:
  return error;
}

int lzfw_dir_iter(vfs_t *vfs, creden_t *cred, vnode_t *vnode,
		  dir_iter_f func, void *arg, off_t *cookie,
		  uint32_t flags)
{
  zfsvfs_t *zfsvfs = ((vfs_t*)vfs)->vfs_data;

  if(vnode->v_type != VDIR)
    return ENOTDIR;

  off_t next_entry;
  int eofp;
  union {
    char buf[DIRENT64_RECLEN(MAXNAMELEN)];
    struct dirent64 dirent;
  } entry;

  /* iter */
  vattr_t vattr;
  vnode_t *d_vnode;
  znode_t *d_znode;
  dir_iter_cb_context_t cb_ctx;

  iovec_t iovec;
  uio_t uio;

  uio.uio_iov = &iovec;
  uio.uio_iovcnt = 1;
  uio.uio_segflg = UIO_SYSSPACE;
  uio.uio_fmode = 0;
  uio.uio_llimit = RLIM64_INFINITY;

  int error;

  ZFS_ENTER(zfsvfs);

 restart:
  next_entry = *cookie;
  eofp = 0;

  do {
    iovec.iov_base = entry.buf;
    iovec.iov_len = sizeof(entry.buf);
    uio.uio_resid = iovec.iov_len;
    uio.uio_loffset = next_entry;

    error = VOP_READDIR(vnode, &uio, (cred_t*)cred, &eofp, NULL, 0);
    if (eofp || /* unlikely */ error)
      break;

    /* set up callback */
    init_di_cb_context(&cb_ctx);
    cb_ctx.dirent = &entry.dirent;

    if (eofp)
      cb_ctx.iflags |= LZFW_DI_CB_IFLAG_EOF;

    if (flags & LZFW_DI_FLAG_GETATTR) {
      cb_ctx.iflags |= LZFW_DI_CB_IFLAG_ATTR;

      cb_ctx.vattr = &vattr;
      error = zfs_zget(zfsvfs, cb_ctx.dirent->d_ino, &d_znode,
		       B_FALSE);

      cb_ctx.gen = d_znode->z_phys->zp_gen;
      d_vnode = ZTOV(d_znode);

      cb_ctx.vnode = d_vnode;
      cb_ctx.znode = d_znode;

      vattr.va_mask = AT_ALL;
      error = VOP_GETATTR(d_vnode, &vattr, 0, (cred_t*)cred,
			  NULL);
    } else {
      d_vnode = NULL;
      d_znode = NULL;
    }

    error = func(vnode, &cb_ctx, arg);
    if (error)
      break;

    if (d_vnode)
      VN_RELE(d_vnode);

    if (cb_ctx.oflags & LZFW_DI_CB_OFLAG_INVALIDATE)
      goto restart;
 
    next_entry = entry.dirent.d_off;    
  } while (!eofp);

  ZFS_EXIT(zfsvfs);

  if (eofp)
    *cookie = 0;
  else
    *cookie = next_entry;

  return error;
}

/**
 * Close the given directory
 * @param p_vfs: the virtual filesystem
 * @param p_cred: the credentials of the user
 * @param p_vnode: the vnode
 * @return 0 on success, the error code otherwise
 */
int lzfw_closedir(vfs_t *p_vfs, creden_t *p_cred,
		  vnode_t *vnode)
{
  return lzfw_close(p_vfs, p_cred, vnode, O_RDONLY);
}

/**
 * Get the stat about a file
 * @param p_vfs: the virtual file system
 * @param p_cred: the credentials of the user
 * @param p_vnode: the vnode
 * @param p_stat: the stat struct to fill in
 * @return 0 on success, the error code otherwise
 */
int lzfw_stat(vfs_t *p_vfs, creden_t *p_cred,
	      vnode_t *vnode, struct stat *p_stat)
{
  zfsvfs_t *zfsvfs = p_vfs->vfs_data;
  vattr_t vattr;
  vattr.va_mask = AT_ALL;
  memset(p_stat, 0, sizeof(*p_stat));

  ZFS_ENTER(zfsvfs);
  int i_error = VOP_GETATTR(vnode, &vattr, 0, (cred_t*)p_cred, NULL);
  ZFS_EXIT(zfsvfs);
  if(i_error)
    return i_error;

  p_stat->st_dev = vattr.va_fsid;                      
  p_stat->st_ino = vattr.va_nodeid;
  p_stat->st_mode = VTTOIF(vattr.va_type) | vattr.va_mode;
  p_stat->st_nlink = vattr.va_nlink;
  p_stat->st_uid = vattr.va_uid;
  p_stat->st_gid = vattr.va_gid;
  p_stat->st_rdev = vattr.va_rdev;
  p_stat->st_size = vattr.va_size;
  p_stat->st_blksize = vattr.va_blksize;         
  p_stat->st_blocks = vattr.va_nblocks;
  TIMESTRUC_TO_TIME(vattr.va_atime, &p_stat->st_atime);
  TIMESTRUC_TO_TIME(vattr.va_mtime, &p_stat->st_mtime);
  TIMESTRUC_TO_TIME(vattr.va_ctime, &p_stat->st_ctime);

  return 0;
}

static int getattr_helper(vfs_t *p_vfs, creden_t *p_cred,
			  inogen_t object, struct stat *p_stat,
			  uint64_t *p_gen, int *p_type)
{
  zfsvfs_t *zfsvfs = p_vfs->vfs_data;
  int i_error;
  znode_t *znode;

  if((i_error = zfs_zget(zfsvfs, object.inode, &znode, B_FALSE)))
    return i_error;
  ASSERT(znode);

  // Check or set the generation
  if(p_gen)
    *p_gen = znode->z_phys->zp_gen;
  else if(znode->z_phys->zp_gen != object.generation)
    {
      VN_RELE(ZTOV(znode));
      return ENOENT;
    }

  vnode_t *vnode = ZTOV(znode);
  ASSERT(vnode);

  vattr_t vattr;
  vattr.va_mask = AT_ALL;
  memset(p_stat, 0, sizeof(*p_stat));

  if(p_type)
    *p_type = VTTOIF(vnode->v_type);

  if((i_error = VOP_GETATTR(vnode, &vattr, 0, (cred_t*)p_cred,
			    NULL))) {
      VN_RELE(vnode);
      return i_error;
    }
  VN_RELE(vnode);

  p_stat->st_dev = vattr.va_fsid;
  p_stat->st_ino = vattr.va_nodeid;
  p_stat->st_mode = VTTOIF(vattr.va_type) | vattr.va_mode;
  p_stat->st_nlink = vattr.va_nlink;
  p_stat->st_uid = vattr.va_uid;
  p_stat->st_gid = vattr.va_gid;
  p_stat->st_rdev = vattr.va_rdev;
  p_stat->st_size = vattr.va_size;
  p_stat->st_blksize = vattr.va_blksize;
  p_stat->st_blocks = vattr.va_nblocks;
  TIMESTRUC_TO_TIME(vattr.va_atime, &p_stat->st_atime);
  TIMESTRUC_TO_TIME(vattr.va_mtime, &p_stat->st_mtime);
  TIMESTRUC_TO_TIME(vattr.va_ctime, &p_stat->st_ctime);

  return 0;
}

/**
 * Get the attributes of an object
 * @param p_vfs: the virtual filesystem
 * @param p_cred: the credentials of the user
 * @param object: the object
 * @param p_stat: the attributes to fill
 * @param p_type: return the type of the object
 * @return 0 on success, the error code otherwise
 */
int lzfw_getattr(vfs_t *p_vfs, creden_t *p_cred, inogen_t object,
		 struct stat *p_stat, int *p_type)
{
  zfsvfs_t *zfsvfs = p_vfs->vfs_data;
  int i_error;

  ZFS_ENTER(zfsvfs);
  i_error = getattr_helper(p_vfs, p_cred, object, p_stat, NULL, p_type);
  ZFS_EXIT(zfsvfs);
  return i_error;
}

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
int lzfw_setattr(vfs_t *p_vfs, creden_t *p_cred, inogen_t object,
		 struct stat *p_stat, int flags,
		 struct stat *p_new_stat)
{
  zfsvfs_t *zfsvfs = p_vfs->vfs_data;
  int i_error;
  znode_t *znode;
  int update_time = 0;

  ZFS_ENTER(zfsvfs);
  if((i_error = zfs_zget(zfsvfs, object.inode, &znode, B_TRUE)))
    {
      ZFS_EXIT(zfsvfs);
      return i_error;
    }
  ASSERT(znode);

  // Check the generation
  if(znode->z_phys->zp_gen != object.generation) {
    VN_RELE(ZTOV(znode));
    ZFS_EXIT(zfsvfs);
    return ENOENT;
  }

  vnode_t *vnode = ZTOV(znode);
  ASSERT(vnode);

  vattr_t vattr = { 0 };
  if(flags & LZFSW_ATTR_MODE)
    {
      vattr.va_mask |= AT_MODE;
      vattr.va_mode = p_stat->st_mode;
    }
  if(flags & LZFSW_ATTR_UID)
    {
      vattr.va_mask |= AT_UID;
      vattr.va_uid = p_stat->st_uid;
    }
  if(flags & LZFSW_ATTR_GID)
    {
      vattr.va_mask |= AT_GID;
      vattr.va_gid = p_stat->st_gid;
    }
  if(flags & LZFSW_ATTR_ATIME)
    {
      vattr.va_mask |= AT_ATIME;
      TIME_TO_TIMESTRUC(p_stat->st_atime, &vattr.va_atime);
      update_time = ATTR_UTIME;
    }
  if(flags & LZFSW_ATTR_MTIME)
    {
      vattr.va_mask |= AT_MTIME;
      TIME_TO_TIMESTRUC(p_stat->st_mtime, &vattr.va_mtime);
      update_time = ATTR_UTIME;
    }

  i_error = VOP_SETATTR(vnode, &vattr, update_time, (cred_t*)p_cred,
			NULL);

  VN_RELE(vnode);
  ZFS_EXIT(zfsvfs);

  return i_error;
}

/**
 * Helper function for every function that manipulate xattrs
 * @param zfsvfs: the virtual file system root object
 * @param p_cred: the user credentials
 * @param object: the object
 * @param
 */
static int xattr_helper(zfsvfs_t *zfsvfs, creden_t *p_cred,
			inogen_t object, vnode_t **pp_vnode)
{
  znode_t *znode;
  int i_error;

  if((i_error = zfs_zget(zfsvfs, object.inode, &znode, B_TRUE)))
    return i_error;
  ASSERT(znode);

  // Check the generation
  if(znode->z_phys->zp_gen != object.generation) {
    VN_RELE(ZTOV(znode));
    return ENOENT;
  }
  vnode_t* vnode = ZTOV(znode);
  ASSERT(vnode);

  // Lookup for the xattr directory
  vnode_t *xattr_vnode;
  i_error = VOP_LOOKUP(vnode, "", &xattr_vnode, NULL,
		       LOOKUP_XATTR | CREATE_XATTR_DIR, NULL,
		       (cred_t*)p_cred, NULL, NULL, NULL);
  VN_RELE(vnode);

  if(i_error || !xattr_vnode) {
    if(xattr_vnode)
      VN_RELE(xattr_vnode);
    return i_error ? i_error : ENOSYS;
  }

  *pp_vnode = xattr_vnode;
  return 0;
}

/**
 * List the extended attributes
 * @param p_vfs: the virtual file system
 * @param p_cred: the credentials of the user
 * @param object: the object
 * @param ppsz_buffer: the buffer to fill with the list of attributes
 * @param p_size: will contain the size of the buffer
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_listxattr(vfs_t *p_vfs, creden_t *p_cred,
		   inogen_t object, char **ppsz_buffer, size_t *p_size)
{
  zfsvfs_t *zfsvfs = p_vfs->vfs_data;
  int i_error;
  vnode_t *vnode;

  ZFS_ENTER(zfsvfs);
  if((i_error = xattr_helper(zfsvfs, p_cred, object, &vnode))) {
    ZFS_EXIT(zfsvfs);
    return i_error;
  }

  // Open the speudo directory
  if((i_error = VOP_OPEN(&vnode, FREAD, (cred_t*)p_cred, NULL))) {
    VN_RELE(vnode);
    ZFS_EXIT(zfsvfs);
    return i_error;
  }

  char *psz_buffer = NULL;
  size_t i_size = 0;
  union {
    char buf[DIRENT64_RECLEN(MAXNAMELEN)];
    struct dirent64 dirent;
  } entry;

  iovec_t iovec;
  uio_t uio;
  uio.uio_iov = &iovec;
  uio.uio_iovcnt = 1;
  uio.uio_segflg = UIO_SYSSPACE;
  uio.uio_fmode = 0;
  uio.uio_llimit = RLIM64_INFINITY;

  int eofp = 0;
  off_t next = 0;

  while(1) {
    iovec.iov_base = entry.buf;
    iovec.iov_len = sizeof(entry.buf);
    uio.uio_resid = iovec.iov_len;
    uio.uio_loffset = next;

    if((i_error = VOP_READDIR(vnode, &uio, (cred_t*)p_cred, &eofp,
			      NULL, 0))) {
      VOP_CLOSE(vnode, FREAD, 1, (offset_t)0, (cred_t*)p_cred, NULL);
      VN_RELE(vnode);
      ZFS_EXIT(zfsvfs);
      return i_error;
    }

    if(iovec.iov_base == entry.buf)
      break;

    next = entry.dirent.d_off;
    // Skip '.' and '..'
    char *s = entry.dirent.d_name;
    if(*s == '.' && (s[1] == 0 || (s[1] == '.' && s[2] == 0)))
      continue;

    size_t length = strlen(s);
    psz_buffer = realloc(psz_buffer, i_size + length + 1);
    strcpy(&psz_buffer[i_size], s);
    i_size += length + 1;
  }

  VOP_CLOSE(vnode, FREAD, 1, (offset_t)0, (cred_t*)p_cred, NULL);
  VN_RELE(vnode);
  ZFS_EXIT(zfsvfs);

  // Return the values
  *ppsz_buffer = psz_buffer;
  *p_size = i_size;

  return 0;
}

/**
 * List extended attributes callback style
 * @param p_vfs: the virtual file system
 * @param p_cred: the credentials of the user
 * @param object: the object
 * @param cb: per-key callback
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_listxattr2(vfs_t *p_vfs, creden_t *p_cred,
		    inogen_t object, opxattr_func cb_func, void *arg)
{
  zfsvfs_t *zfsvfs = p_vfs->vfs_data;
  int i_error;
  vnode_t *vnode;

  ZFS_ENTER(zfsvfs);
  if((i_error = xattr_helper(zfsvfs, p_cred, object, &vnode))) {
    ZFS_EXIT(zfsvfs);
    return i_error;
  }

  // Open the speudo directory
  if((i_error = VOP_OPEN(&vnode, FREAD, (cred_t*)p_cred, NULL))) {
    VN_RELE(vnode);
    ZFS_EXIT(zfsvfs);
    return i_error;
  }

  union {
    char buf[DIRENT64_RECLEN(MAXNAMELEN)];
    struct dirent64 dirent;
  } entry;

  iovec_t iovec;
  uio_t uio;
  uio.uio_iov = &iovec;
  uio.uio_iovcnt = 1;
  uio.uio_segflg = UIO_SYSSPACE;
  uio.uio_fmode = 0;
  uio.uio_llimit = RLIM64_INFINITY;

  int eofp = 0;
  off_t next = 0;

  inogen_t obj;

  while(1) {
    iovec.iov_base = entry.buf;
    iovec.iov_len = sizeof(entry.buf);
    uio.uio_resid = iovec.iov_len;
    uio.uio_loffset = next;

    if((i_error = VOP_READDIR(vnode, &uio, (cred_t*)p_cred, &eofp,
			      NULL, 0))) {
      VOP_CLOSE(vnode, FREAD, 1, (offset_t)0, (cred_t*)p_cred, NULL);
      VN_RELE(vnode);
      ZFS_EXIT(zfsvfs);
      return i_error;
    }

    if(iovec.iov_base == entry.buf)
      break;

    next = entry.dirent.d_off;
    // Skip '.' and '..'
    char *s = entry.dirent.d_name;
    if(*s == '.' && (s[1] == 0 || (s[1] == '.' && s[2] == 0)))
      continue;

    obj.inode = VTOZ(vnode)->z_id;
    obj.generation = VTOZ(vnode)->z_phys->zp_gen;

    /* call w/args */
    (void) cb_func(vnode, obj, p_cred, s, arg);
  }

  VOP_CLOSE(vnode, FREAD, 1, (offset_t)0, (cred_t*)p_cred, NULL);
  VN_RELE(vnode);
  ZFS_EXIT(zfsvfs);

  return 0;
}

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
int lzfw_setxattr(vfs_t *p_vfs, creden_t *p_cred, inogen_t object,
		  const char *psz_key, const char *psz_value)
{
  zfsvfs_t *zfsvfs = p_vfs->vfs_data;
  int i_error;
  vnode_t *vnode;

  ZFS_ENTER(zfsvfs);
  if((i_error = xattr_helper(zfsvfs, p_cred, object, &vnode))) {
    ZFS_EXIT(zfsvfs);
    return i_error;
  }

  // Create a new speudo-file
  vattr_t vattr = { 0 };
  vattr.va_type = VREG;
  vattr.va_mode = 0660;
  vattr.va_mask = AT_TYPE | AT_MODE | AT_SIZE;
  vattr.va_size = 0;

  vnode_t *pseudo_vnode;
  if((i_error = VOP_CREATE(vnode, (char*)psz_key, &vattr, NONEXCL,
			   VWRITE, &pseudo_vnode, (cred_t*)p_cred, 0,
			   NULL, NULL))) {
    VN_RELE(vnode);
    ZFS_EXIT(zfsvfs);
  }
  VN_RELE(vnode);

  // Open the key-file
  vnode_t *key_vnode = pseudo_vnode;
  if((i_error = VOP_OPEN(&key_vnode, FWRITE, (cred_t*)p_cred, NULL))) {
    VN_RELE(pseudo_vnode);
    ZFS_EXIT(zfsvfs);
    return i_error;
  }

  iovec_t iovec;
  uio_t uio;
  uio.uio_iov = &iovec;
  uio.uio_iovcnt = 1;
  uio.uio_segflg = UIO_SYSSPACE;
  uio.uio_fmode = 0;
  uio.uio_llimit = RLIM64_INFINITY;

  iovec.iov_base = (void *) psz_value;
  iovec.iov_len = strlen(psz_value);
  uio.uio_resid = iovec.iov_len;
  uio.uio_loffset = 0;

  i_error = VOP_WRITE(key_vnode, &uio, FWRITE, (cred_t*)p_cred, NULL);
  VOP_CLOSE(key_vnode, FWRITE, 1, (offset_t) 0, (cred_t*)p_cred, NULL);

  VN_RELE(key_vnode);
  ZFS_EXIT(zfsvfs);
  return i_error;
}

/**
 * Add the given (key,value) to the extended attributes.
 * This function will change the value if the key already exists.
 * @param p_vfs: the virtual file system
 * @param p_cred: the credentials of the user
 * @param vnode: the object vnode
 * @param psz_key: the key
 * @param psz_value: the value
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_setxattrat(vfs_t *p_vfs, creden_t *p_cred,
		    vnode_t *vnode, const char *psz_key,
		    const char *psz_value)
{
  zfsvfs_t *zfsvfs = p_vfs->vfs_data;
  vnode_t *xattr_vnode;
  int i_error;

  ZFS_ENTER(zfsvfs);

  /* lookup xattr directory */
  i_error = VOP_LOOKUP(vnode, "", &xattr_vnode, NULL,
		       LOOKUP_XATTR | CREATE_XATTR_DIR, NULL,
		       (cred_t*)p_cred, NULL, NULL, NULL);
  if (i_error) {
    ZFS_EXIT(zfsvfs);
    return i_error;
  }

  // Create a new pseudo-file
  vattr_t vattr = { 0 };
  vattr.va_type = VREG;
  vattr.va_mode = 0660;
  vattr.va_mask = AT_TYPE | AT_MODE | AT_SIZE;
  vattr.va_size = 0;

  vnode_t *pseudo_vnode;
  i_error = VOP_CREATE(xattr_vnode, (char*)psz_key, &vattr, NONEXCL,
		       VWRITE, &pseudo_vnode, (cred_t*)p_cred, 0,
		       NULL, NULL);
  if (i_error) {
    VN_RELE(xattr_vnode);
    ZFS_EXIT(zfsvfs);
  }
  VN_RELE(xattr_vnode);

  // Open the pseudo-file
  i_error = VOP_OPEN(&pseudo_vnode, FWRITE, (cred_t*)p_cred, NULL);
  if (i_error) {
    VN_RELE(pseudo_vnode); // rele ref taken in VOP_CREATE
    ZFS_EXIT(zfsvfs);
    return i_error;
  }

  iovec_t iovec;
  uio_t uio;
  uio.uio_iov = &iovec;
  uio.uio_iovcnt = 1;
  uio.uio_segflg = UIO_SYSSPACE;
  uio.uio_fmode = 0;
  uio.uio_llimit = RLIM64_INFINITY;

  iovec.iov_base = (void *) psz_value;
  iovec.iov_len = strlen(psz_value);
  uio.uio_resid = iovec.iov_len;
  uio.uio_loffset = 0;

  i_error = VOP_WRITE(pseudo_vnode, &uio, FWRITE, (cred_t*)p_cred,
		      NULL);
  VOP_CLOSE(pseudo_vnode, FWRITE, 1, (offset_t) 0, (cred_t*)p_cred,
	    NULL);
  VN_RELE(pseudo_vnode); // rele rev taken in VOP_CREATE

  ZFS_EXIT(zfsvfs);

  return i_error;
}

/**
 * Get the value for the given extended attribute
 * @param p_vfs: the virtual file system
 * @param p_cred: the user credentials
 * @param object: the object
 * @param psz_key: the key
 * @param ppsz_value: the value
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_getxattr(vfs_t *p_vfs, creden_t *p_cred,
		  inogen_t object, const char *psz_key,
		  char **ppsz_value)
{
  zfsvfs_t *zfsvfs = p_vfs->vfs_data;
  int i_error;
  vnode_t *vnode;
  char *psz_value;

  ZFS_ENTER(zfsvfs);
  if((i_error = xattr_helper(zfsvfs, p_cred, object, &vnode))) {
    ZFS_EXIT(zfsvfs);
    return i_error;
  }

  // Lookup for the right file
  vnode_t *key_vnode;
  if((i_error = VOP_LOOKUP(vnode, (char*)psz_key, &key_vnode, NULL,
			   0, NULL, (cred_t*)p_cred, NULL, NULL,
			   NULL))) {
    VN_RELE(vnode);
    ZFS_EXIT(zfsvfs);
    return i_error;
  }
  VN_RELE(vnode);

  // Get the size of the value
  vattr_t vattr = { 0 };
  vattr.va_mask = AT_STAT | AT_NBLOCKS | AT_BLKSIZE | AT_SIZE;
  if((i_error = VOP_GETATTR(key_vnode, &vattr, 0, (cred_t*)p_cred,
			    NULL))) {
    VN_RELE(key_vnode);
    ZFS_EXIT(zfsvfs);
    return i_error;
  }

  if((i_error = VOP_OPEN(&key_vnode, FREAD, (cred_t*)p_cred, NULL))) {
    VN_RELE(key_vnode);
    ZFS_EXIT(zfsvfs);
    return i_error;
  }

  // Read the value
  psz_value = malloc(vattr.va_size + 1);
  iovec_t iovec;
  uio_t uio;
  uio.uio_iov = &iovec;
  uio.uio_iovcnt = 1;
  uio.uio_segflg = UIO_SYSSPACE;
  uio.uio_fmode = 0;
  uio.uio_llimit = RLIM64_INFINITY;

  iovec.iov_base = psz_value;
  iovec.iov_len = vattr.va_size + 1;
  uio.uio_resid = iovec.iov_len;
  uio.uio_loffset = 0;

  i_error = VOP_READ(key_vnode, &uio, FREAD, (cred_t*)p_cred, NULL);
  VOP_CLOSE(key_vnode, FREAD, 1, (offset_t)0, (cred_t*)p_cred, NULL);

  VN_RELE(key_vnode);
  ZFS_EXIT(zfsvfs);

  if(!i_error) {
    psz_value[vattr.va_size] = '\0';
    *ppsz_value = psz_value;
  }

  return i_error;
}

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
int lzfw_getxattrat(vfs_t *p_vfs, creden_t *p_cred,
		    vnode_t *vnode, const char *psz_key,
		    char *value, size_t *size)
{
  zfsvfs_t *zfsvfs = p_vfs->vfs_data;
  vnode_t *xattr_vnode;
  int i_error;

  iovec_t iovec;
  uio_t uio;
  uio.uio_iov = &iovec;
  uio.uio_iovcnt = 1;
  uio.uio_segflg = UIO_SYSSPACE;
  uio.uio_fmode = 0;
  uio.uio_llimit = RLIM64_INFINITY;
  iovec.iov_base = value;
  iovec.iov_len = *size;
  uio.uio_resid = iovec.iov_len;
  uio.uio_loffset = 0;

  ZFS_ENTER(zfsvfs);

  /* lookup xattr directory */
  i_error = VOP_LOOKUP(vnode, "", &xattr_vnode, NULL,
		       LOOKUP_XATTR | CREATE_XATTR_DIR, NULL,
		       (cred_t*)p_cred, NULL, NULL, NULL);
  if (i_error) {
    ZFS_EXIT(zfsvfs);
    return i_error;
  }

  // lookup pseudo-file
  vnode_t *pseudo_vnode;
  i_error = VOP_LOOKUP(xattr_vnode, (char*)psz_key, &pseudo_vnode,
		       NULL, 0, NULL, (cred_t*)p_cred, NULL, NULL,
		       NULL);
  if (i_error) {
    VN_RELE(xattr_vnode);
    ZFS_EXIT(zfsvfs);
    return i_error;
  }

  VN_RELE(xattr_vnode); // done with this

  /* special: return the stored size of xattr */
  if (*size == 0) {
    vattr_t vattr = { 0 };
    vattr.va_mask = AT_STAT | AT_NBLOCKS | AT_BLKSIZE | AT_SIZE;
    i_error = VOP_GETATTR(pseudo_vnode, &vattr, 0, (cred_t*)p_cred,
			  NULL);
    if (!i_error)
      *size = vattr.va_size;
    goto out;
  }

  // open xattr file
  i_error = VOP_OPEN(&pseudo_vnode, FREAD, (cred_t*)p_cred, NULL);
  if (i_error) {
    VN_RELE(pseudo_vnode);
    ZFS_EXIT(zfsvfs);
    return i_error;
  }
  
  i_error = VOP_READ(pseudo_vnode, &uio, 0, (cred_t*)p_cred, NULL);
  if (i_error)
    *size = 0;
  else
    *size = iovec.iov_len; // XXX check
  (void) VOP_CLOSE(pseudo_vnode, FREAD, 1, (offset_t)0,
		   (cred_t*)p_cred, NULL);

 out:
  VN_RELE(pseudo_vnode);
  ZFS_EXIT(zfsvfs);

  return i_error;
}

/**
 * Remove the given extended attribute
 * @param p_vfs: the virtual file system
 * @param p_cred: the user credentials
 * @param object: the object
 * @param psz_key: the key
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_removexattr(vfs_t *p_vfs, creden_t *p_cred,
		     inogen_t object, const char *psz_key)
{
  zfsvfs_t *zfsvfs = p_vfs->vfs_data;
  int i_error;
  vnode_t *vnode;

  ZFS_ENTER(zfsvfs);
  if((i_error = xattr_helper(zfsvfs, p_cred, object, &vnode))) {
    ZFS_EXIT(zfsvfs);
    return i_error;
  }

  i_error = VOP_REMOVE(vnode, (char*)psz_key, (cred_t*)p_cred, NULL,
		       0);
  VN_RELE(vnode);
  ZFS_EXIT(zfsvfs);

  return i_error;
}

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
ssize_t lzfw_read(vfs_t *p_vfs, creden_t *p_cred,
		  vnode_t *vnode, void *p_buffer, size_t size,
		  int behind, off_t offset)
{
  zfsvfs_t *zfsvfs = p_vfs->vfs_data;
  iovec_t iovec;
  uio_t uio;
  uio.uio_iov = &iovec;
  uio.uio_iovcnt = 1;
  uio.uio_segflg = UIO_SYSSPACE;
  uio.uio_fmode = 0; // TODO: Do we have to give the same flags ?
  uio.uio_llimit = RLIM64_INFINITY;

  iovec.iov_base = p_buffer;
  iovec.iov_len = size;
  uio.uio_resid = iovec.iov_len;
  uio.uio_loffset = offset;
  if(behind)
    uio.uio_loffset += VTOZ(vnode)->z_phys->zp_size;

  ZFS_ENTER(zfsvfs);
  ssize_t error = VOP_READ(vnode, &uio, 0, (cred_t*)p_cred, NULL);
  ZFS_EXIT(zfsvfs);

  /* XXXX return from VOP_READ is always discarded? */
  if(offset == uio.uio_loffset)
    return 0;
  else
    return size;
  return error;
}

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
		    off_t offset)
{
  zfsvfs_t *zfsvfs = p_vfs->vfs_data;
  uio_t uio;
  ssize_t resid, error;
  int ix;

  uio.uio_iov = iov;
  uio.uio_iovcnt = iovcnt;
  uio.uio_segflg = UIO_SYSSPACE;
  uio.uio_fmode = 0; // TODO: Do we have to give the same flags ?
  uio.uio_llimit = RLIM64_INFINITY;
  uio.uio_loffset = offset;
  resid = 0;
  for (ix = 0; ix < iovcnt; ++ix) {
    iovec_t *t_iov = &iov[ix];
    resid += t_iov->iov_len;
  }
  uio.uio_resid = resid;

  ZFS_ENTER(zfsvfs);

  error = VOP_READ(vnode, &uio, 0, (cred_t*)cred, NULL);
  /* return count of bytes actually read */
  if (!error)
    error = resid - uio.uio_resid;

  ZFS_EXIT(zfsvfs);

  return error;
}

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
ssize_t lzfw_write(vfs_t *p_vfs, creden_t *p_cred,
		   vnode_t *vnode, void *p_buffer, size_t size,
		   int behind, off_t offset)
{
  zfsvfs_t *zfsvfs = p_vfs->vfs_data;
  iovec_t iovec;
  uio_t uio;
  uio.uio_iov = &iovec;
  uio.uio_iovcnt = 1;
  uio.uio_segflg = UIO_SYSSPACE;
  uio.uio_fmode = 0; // TODO: Do we have to give the same flags ?
  uio.uio_llimit = RLIM64_INFINITY;

  iovec.iov_base = p_buffer;
  iovec.iov_len = size;
  uio.uio_resid = iovec.iov_len;
  uio.uio_loffset = offset;
  if(behind)
    uio.uio_loffset += VTOZ(vnode)->z_phys->zp_size;

  ZFS_ENTER(zfsvfs);
  ssize_t error = VOP_WRITE(vnode, &uio, 0, (cred_t*)p_cred, NULL);
  ZFS_EXIT(zfsvfs);

  return error;
}

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
		     vnode_t *vnode, struct iovec *iov, int iovcnt,
		     off_t offset)
{
  zfsvfs_t *zfsvfs = p_vfs->vfs_data;
  uio_t uio;
  ssize_t resid, error;
  int ix;

  uio.uio_iov = iov;
  uio.uio_iovcnt = iovcnt;
  uio.uio_segflg = UIO_SYSSPACE;
  uio.uio_fmode = 0; // TODO: Do we have to give the same flags ?
  uio.uio_llimit = RLIM64_INFINITY;
  uio.uio_loffset = offset;
  resid = 0;
  for (ix = 0; ix < iovcnt; ++ix) {
    iovec_t *t_iov = &iov[ix];
    resid += t_iov->iov_len;
  }
  uio.uio_resid = resid;

  ZFS_ENTER(zfsvfs);

  error = VOP_WRITE(vnode, &uio, 0, (cred_t*)cred, NULL);

  /* return count of bytes actually written */
  if (!error)
    error = resid - uio.uio_resid;

  ZFS_EXIT(zfsvfs);

  return error;
}

/**
 * Close the given vnode
 * @param p_vfs: the virtual file system
 * @param p_cred: the credentials of the user
 * @param p_vnode: the vnode to close
 * @param i_flags: the flags given when opening
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_close(vfs_t *p_vfs, creden_t *p_cred,
	       vnode_t *vnode, int i_flags)
{
  zfsvfs_t *zfsvfs = p_vfs->vfs_data;

  int mode, flags, i_error;
  lzwu_flags2zfs(i_flags, &flags, &mode);

  ZFS_ENTER(zfsvfs);
  i_error = VOP_CLOSE(vnode, flags, 1, (offset_t)0, (cred_t*)p_cred,
		      NULL);
  VN_RELE(vnode);
  ZFS_EXIT(zfsvfs);
  return i_error;
}

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
int lzfw_mkdir(vfs_t *p_vfs, creden_t *p_cred, inogen_t parent,
	       const char *psz_name, mode_t mode,
	       inogen_t *p_directory)
{
  zfsvfs_t *zfsvfs = p_vfs->vfs_data;
  int i_error;
  znode_t *parent_znode;

  ZFS_ENTER(zfsvfs);
  if((i_error = zfs_zget(zfsvfs, parent.inode, &parent_znode,
			 B_FALSE))) {
    ZFS_EXIT(zfsvfs);
    return i_error;
  }
  ASSERT(parent_znode != NULL);

  // Check the generation
  if(parent_znode->z_phys->zp_gen != parent.generation) {
    VN_RELE(ZTOV(parent_znode));
    ZFS_EXIT(zfsvfs);
    return ENOENT;
  }

  vnode_t *parent_vnode = ZTOV(parent_znode);
  ASSERT(parent_vnode != NULL);
  vnode_t *vnode = NULL;

  vattr_t vattr = { 0 };
  vattr.va_type = VDIR;
  vattr.va_mode = mode & PERMMASK;
  vattr.va_mask = AT_TYPE | AT_MODE;

  if((i_error = VOP_MKDIR(parent_vnode, (char*)psz_name, &vattr,
			  &vnode, (cred_t*)p_cred, NULL, 0, NULL))) {
    VN_RELE(parent_vnode);
    ZFS_EXIT(zfsvfs);
    return i_error;
  }

  p_directory->inode = VTOZ(vnode)->z_id;
  p_directory->generation = VTOZ(vnode)->z_phys->zp_gen;

  VN_RELE(vnode);
  VN_RELE(parent_vnode);
  ZFS_EXIT(zfsvfs);

  return 0;
}

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
int lzfw_mkdirat(vfs_t *p_vfs, creden_t *p_cred,
		 vnode_t *parent, const char *psz_name,
		 mode_t mode, inogen_t *p_directory)
{
  zfsvfs_t *zfsvfs = p_vfs->vfs_data;
  int i_error;

  ASSERT(parent);

  ZFS_ENTER(zfsvfs);

  vnode_t *vnode = NULL;

  vattr_t vattr = { 0 };
  vattr.va_type = VDIR;
  vattr.va_mode = mode & PERMMASK;
  vattr.va_mask = AT_TYPE | AT_MODE;

  i_error = VOP_MKDIR(parent, (char*)psz_name, &vattr, &vnode,
		      (cred_t*)p_cred, NULL, 0, NULL);
  if (i_error) {
    ZFS_EXIT(zfsvfs);
    return i_error;
  }

  p_directory->inode = VTOZ(vnode)->z_id;
  p_directory->generation = VTOZ(vnode)->z_phys->zp_gen;

  VN_RELE(vnode);
  ZFS_EXIT(zfsvfs);

  return 0;
}

/**
 * Remove the given directory
 * @param p_vfs: the virtual filesystem
 * @param p_cred: the credentials of the user
 * @param parent: the parent directory
 * @param psz_filename: name of the file to unlink
 * @return 0 on success, the error code otherwise
 */
int lzfw_rmdir(vfs_t *p_vfs, creden_t *p_cred, inogen_t parent,
	       const char *psz_filename)
{
  zfsvfs_t *zfsvfs = p_vfs->vfs_data;
  int i_error;
  znode_t *parent_znode;

  ZFS_ENTER(zfsvfs);
  if((i_error = zfs_zget(zfsvfs, parent.inode, &parent_znode,
			 B_FALSE))) {
    ZFS_EXIT(zfsvfs);
    return i_error;
  }
  ASSERT(parent_znode);

  // Check the generation
  if(parent_znode->z_phys->zp_gen != parent.generation) {
    VN_RELE(ZTOV(parent_znode));
    ZFS_EXIT(zfsvfs);
    return ENOENT;
  }

  vnode_t *parent_vnode = ZTOV(parent_znode);
  ASSERT(parent_vnode);

  i_error = VOP_RMDIR(parent_vnode, (char*)psz_filename, NULL,
		      (cred_t*)p_cred, NULL, 0);

  VN_RELE(parent_vnode);
  ZFS_EXIT(zfsvfs);

  return i_error == EEXIST ? ENOTEMPTY : i_error;
}

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
int lzfw_symlink(vfs_t *p_vfs, creden_t *p_cred, inogen_t parent,
		 const char *psz_name, const char *psz_link,
		 inogen_t *p_symlink)
{
  zfsvfs_t *zfsvfs = p_vfs->vfs_data;
  int i_error;
  znode_t *parent_znode;

  ZFS_ENTER(zfsvfs);
  if((i_error = zfs_zget(zfsvfs, parent.inode, &parent_znode,
			 B_FALSE))) {
    ZFS_EXIT(zfsvfs);
    return i_error;
  }
  ASSERT(parent_znode != NULL);

  // Check generation
  if(parent_znode->z_phys->zp_gen != parent.generation) {
    VN_RELE(ZTOV(parent_znode));
    ZFS_EXIT(zfsvfs);
    return ENOENT;
  }

  vnode_t *parent_vnode = ZTOV(parent_znode);
  ASSERT(parent_vnode != NULL);

  vattr_t vattr = { 0 };
  vattr.va_type = VLNK;
  vattr.va_mode = 0777;
  vattr.va_mask = AT_TYPE | AT_MODE;

  if((i_error = VOP_SYMLINK(parent_vnode, (char*)psz_name, &vattr,
			    (char*) psz_link, (cred_t*)p_cred, NULL,
			    0))) {
    VN_RELE(parent_vnode);
    ZFS_EXIT(zfsvfs);
    return i_error;
  }

  vnode_t *vnode;
  if((i_error = VOP_LOOKUP(parent_vnode, (char*) psz_name, &vnode,
			   NULL, 0, NULL, (cred_t*)p_cred, NULL, NULL,
			   NULL))) {
    VN_RELE(parent_vnode);
    ZFS_EXIT(zfsvfs);
    return i_error;
  }

  ASSERT(vnode != NULL);
  p_symlink->inode = VTOZ(vnode)->z_id;
  p_symlink->generation = VTOZ(vnode)->z_phys->zp_gen;

  VN_RELE(vnode);
  VN_RELE(parent_vnode);
  ZFS_EXIT(zfsvfs);
  return 0;
}

/**
 * Read the content of a symbolic link
 * @param p_vfs: the virtual file system
 * @param p_cred: the credentials of the user
 * @param symlink: the symlink to read
 * @param psz_content: return the content of the symlink
 * @param content_size: size of the buffer
 * @return 0 on success, the error code otherwise
 */
int lzfw_readlink(vfs_t *p_vfs, creden_t *p_cred,
		  inogen_t symlink, char *psz_content,
		  size_t content_size)
{
  zfsvfs_t *zfsvfs = p_vfs->vfs_data;
  int i_error;
  znode_t *znode;

  ZFS_ENTER(zfsvfs);

  if((i_error = zfs_zget(zfsvfs, symlink.inode, &znode, B_FALSE))) {
    ZFS_EXIT(zfsvfs);
    return i_error;
  }
  ASSERT(znode != NULL);

  // Check generation
  if(znode->z_phys->zp_gen != symlink.generation) {
    VN_RELE(ZTOV(znode));
    ZFS_EXIT(zfsvfs);
    return ENOENT;
  }

  vnode_t *vnode = ZTOV(znode);
  ASSERT(vnode != NULL);

  iovec_t iovec;
  uio_t uio;
  uio.uio_iov = &iovec;
  uio.uio_iovcnt = 1;
  uio.uio_segflg = UIO_SYSSPACE;
  uio.uio_fmode = 0;
  uio.uio_llimit = RLIM64_INFINITY;
  iovec.iov_base = psz_content;
  iovec.iov_len = content_size;
  uio.uio_resid = iovec.iov_len;
  uio.uio_loffset = 0;

  i_error = VOP_READLINK(vnode, &uio, (cred_t*)p_cred, NULL);
  VN_RELE(vnode);
  ZFS_EXIT(zfsvfs);

  if(!i_error)
    psz_content[uio.uio_loffset] = '\0';
  else
    psz_content[0] = '\0';

  return i_error;
}

/**
 * Create a hard link
 * @param p_vfs: the virtual file system
 * @param p_cred: the credentials of the user
 * @param parent: the parent directory
 * @param target: the target object
 * @param psz_name: name of the link
 * @return 0 on success, the error code otherwise
 */
int lzfw_link(vfs_t *p_vfs, creden_t *p_cred, inogen_t parent,
	      inogen_t target, const char *psz_name)
{
  zfsvfs_t *zfsvfs = p_vfs->vfs_data;
  int i_error;
  znode_t *parent_znode, *target_znode;

  ZFS_ENTER(zfsvfs);
  if((i_error = zfs_zget(zfsvfs, parent.inode, &parent_znode,
			 B_FALSE))) {
    ZFS_EXIT(zfsvfs);
    return i_error;
  }
  ASSERT(parent_znode);

  // Check the generation
  if(parent_znode->z_phys->zp_gen != parent.generation) {
    VN_RELE(ZTOV(parent_znode));
    ZFS_EXIT(zfsvfs);
    return ENOENT;
  }

  if((i_error = zfs_zget(zfsvfs, target.inode, &target_znode,
			 B_FALSE))) {
    VN_RELE((ZTOV(parent_znode)));
    ZFS_EXIT(zfsvfs);
    return i_error;
  }
  ASSERT(target_znode);

  // Check the generation
  if(target_znode->z_phys->zp_gen != target.generation) {
    VN_RELE(ZTOV(target_znode));
    VN_RELE(ZTOV(parent_znode));
    ZFS_EXIT(zfsvfs);
    return ENOENT;
  }

  vnode_t *parent_vnode = ZTOV(parent_znode);
  vnode_t *target_vnode = ZTOV(target_znode);

  i_error = VOP_LINK(parent_vnode, target_vnode, (char*)psz_name,
		     (cred_t*)p_cred, NULL, 0);

  VN_RELE(target_vnode);
  VN_RELE(parent_vnode);

  ZFS_EXIT(zfsvfs);

  return i_error;
}

/**
 * Unlink the given file
 * @param p_vfs: the virtual filesystem
 * @param p_cred: the credentials of the user
 * @param parent: the parent directory
 * @param psz_filename: name of the file to unlink
 * @return 0 on success, the error code otherwise
 */
int lzfw_unlink(vfs_t *p_vfs, creden_t *p_cred, inogen_t parent,
		const char *psz_filename)
{
  zfsvfs_t *zfsvfs = p_vfs->vfs_data;
  int i_error;
  znode_t *parent_znode;

  ZFS_ENTER(zfsvfs);

  if((i_error = zfs_zget(zfsvfs, parent.inode, &parent_znode,
			 B_FALSE))) {
    ZFS_EXIT(zfsvfs);
    return i_error;
  }
  ASSERT(parent_znode);

  // Check the generation
  if(parent_znode->z_phys->zp_gen != parent.generation) {
    VN_RELE(ZTOV(parent_znode));
    ZFS_EXIT(zfsvfs);
    return ENOENT;
  }

  vnode_t *parent_vnode = ZTOV(parent_znode);
  ASSERT(parent_vnode);

  i_error = VOP_REMOVE(parent_vnode, (char*)psz_filename,
		       (cred_t*)p_cred, NULL, 0);

  VN_RELE(parent_vnode);
  ZFS_EXIT(zfsvfs);

  return i_error;
}

/**
 * Unlink the given file w/parent vnode
 * @param p_vfs: the virtual filesystem
 * @param p_cred: the credentials of the user
 * @param parent: the parent directory
 * @param psz_filename: name of the file to unlink
 * @param flags: flags
 * @return 0 on success, the error code otherwise
 */
int lzfw_unlinkat(vfs_t *p_vfs, creden_t *p_cred,
		  vnode_t* parent, const char *psz_filename,
		  int flags)
{
  zfsvfs_t *zfsvfs = p_vfs->vfs_data;
  int i_error;

  ASSERT(parent);

  ZFS_ENTER(zfsvfs);

  i_error = VOP_REMOVE(parent, (char*)psz_filename, (cred_t*)p_cred,
		       NULL, 0);

  ZFS_EXIT(zfsvfs);

  return i_error;
}

/**
 * Rename the given file
 * @param p_vfs: the virtual filesystem
 * @param p_cred: the credentials of the user
 * @param parent: the current parent directory
 * @param psz_filename: current name of the file
 * @param new_parent: the new parents directory
 * @param psz_new_filename: new file name
 * @return 0 on success, the error code otherwise
 */
int lzfw_rename(vfs_t *p_vfs, creden_t *p_cred, inogen_t parent,
		const char *psz_filename, inogen_t new_parent,
		const char *psz_new_filename)
{
  zfsvfs_t *zfsvfs = p_vfs->vfs_data;
  znode_t *parent_znode, *new_parent_znode;
  int i_error;

  ZFS_ENTER(zfsvfs);

  if((i_error = zfs_zget(zfsvfs, parent.inode, &parent_znode,
			 B_FALSE))) {
    ZFS_EXIT(zfsvfs);
    return i_error;
  }
  ASSERT(parent_znode);

  // Check the generation
  if(parent_znode->z_phys->zp_gen != parent.generation) {
    VN_RELE(ZTOV(parent_znode));
    ZFS_EXIT(zfsvfs);
    return ENOENT;
  }

  if((i_error = zfs_zget(zfsvfs, new_parent.inode,
			 &new_parent_znode, B_FALSE))) {
    VN_RELE(ZTOV(parent_znode));
    ZFS_EXIT(zfsvfs);
    return i_error;
  }
  ASSERT(new_parent_znode);

  // Check the generation
  if(new_parent_znode->z_phys->zp_gen != new_parent.generation) {
    VN_RELE(ZTOV(new_parent_znode));
    VN_RELE(ZTOV(parent_znode));
    ZFS_EXIT(zfsvfs);
    return ENOENT;
  }

  vnode_t *parent_vnode = ZTOV(parent_znode);
  vnode_t *new_parent_vnode = ZTOV(new_parent_znode);
  ASSERT(parent_vnode);
  ASSERT(new_parent_vnode);

  i_error = VOP_RENAME(parent_vnode, (char*)psz_filename,
		       new_parent_vnode, (char*)psz_new_filename,
		       (cred_t*)p_cred, NULL, 0);

  VN_RELE(new_parent_vnode);
  VN_RELE(parent_vnode);

  ZFS_EXIT(zfsvfs);

  return i_error;
}

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
int lzfw_renameat(vfs_t *p_vfs, creden_t *p_cred,
		  vnode_t *parent, const char *psz_name, 
		  vnode_t *new_parent,
		  const char *psz_newname)
{
  zfsvfs_t *zfsvfs = p_vfs->vfs_data;
  int i_error;

  ZFS_ENTER(zfsvfs);

  ASSERT(parent);
  ASSERT(new_parent);

  i_error = VOP_RENAME(parent, (char*)psz_name, new_parent,
		       (char*)psz_newname, (cred_t*)p_cred, NULL, 0);

  ZFS_EXIT(zfsvfs);

  return i_error;
}

/**
 * Set the size of the given file
 * @param p_vfs: the virtual filesystem
 * @param p_cred: the credentials of the user
 * @param file: the file to truncate
 * @param size: the new size
 * @return 0 in case of success, the error code otherwise
 */
int lzfw_truncate(vfs_t *p_vfs, creden_t *p_cred, inogen_t file,
		  size_t size)
{
  zfsvfs_t *zfsvfs = p_vfs->vfs_data;
  znode_t *znode;
  int i_error;

  ZFS_ENTER(zfsvfs);
  if((i_error = zfs_zget(zfsvfs, file.inode, &znode, B_TRUE))) {
    ZFS_EXIT(zfsvfs);
    return i_error;
  }
  ASSERT(znode);

  // Check the generation
  if(znode->z_phys->zp_gen != file.generation) {
    VN_RELE(ZTOV(znode));
    ZFS_EXIT(zfsvfs);
    return ENOENT;
  }

  vnode_t *vnode = ZTOV(znode);
  ASSERT(vnode);

  flock64_t fl;
  fl.l_whence = 0;        // beginning of the file
  fl.l_start = size;
  fl.l_type = F_WRLCK;
  fl.l_len = (off_t)0;

  i_error = VOP_SPACE(vnode, F_FREESP, &fl, FWRITE, 0, (cred_t*)p_cred,
		      NULL);
  VN_RELE(vnode);

  ZFS_EXIT(zfsvfs);

  return i_error;
}

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
	      off_t offset, size_t length)
{
  zfsvfs_t *zfsvfs = p_vfs->vfs_data;
  flock64_t fl;

  fl.l_type = F_WRLCK;
  fl.l_whence = 0; /* offset is from BOF */
  fl.l_start = offset;
  fl.l_len = length;

  ZFS_ENTER(zfsvfs);

  int error = VOP_SPACE(vnode, F_FREESP, &fl, FWRITE|FOFFMAX,
			offset /* XXX check */, (cred_t*)cred, NULL);

  ZFS_EXIT(zfsvfs);

  return error;
}
