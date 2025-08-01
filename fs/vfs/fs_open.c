/****************************************************************************
 * fs/vfs/fs_open.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sched.h>
#include <errno.h>
#include <assert.h>
#include <stdarg.h>

#include <nuttx/sched.h>
#include <nuttx/cancelpt.h>
#include <nuttx/fs/fs.h>

#include "sched/sched.h"
#include "inode/inode.h"
#include "driver/driver.h"
#include "vfs.h"

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: inode_checkflags
 *
 * Description:
 *   Check if the access described by 'oflags' is supported on 'inode'
 *
 *   inode_checkflags() is an internal NuttX interface and should not be
 *   called from applications.
 *
 * Input Parameters:
 *   inode  - The inode to check
 *   oflags - open flags.
 *
 * Returned Value:
 *   Zero (OK) is returned on success.  On failure, a negated errno value is
 *   returned.
 *
 ****************************************************************************/

static int inode_checkflags(FAR struct inode *inode, int oflags)
{
  FAR const struct file_operations *ops = inode->u.i_ops;

  if (INODE_IS_PSEUDODIR(inode))
    {
      return OK;
    }

  if (ops == NULL)
    {
      return -ENXIO;
    }

  if (((oflags & O_RDOK) != 0 && !ops->readv && !ops->read && !ops->ioctl) ||
      ((oflags & O_WROK) != 0 && !ops->writev && !ops->write && !ops->ioctl))
    {
      return -EACCES;
    }
  else
    {
      return OK;
    }
}

/****************************************************************************
 * Name: file_vopen
 ****************************************************************************/

/****************************************************************************
 * Name: file_vopen
 *
 * Description:
 *   file_vopen() is similar to the standard 'open' interface except that it
 *   populates an instance of 'struct file' rather than return a file
 *   descriptor.  It also is not a cancellation point and does not modify
 *   the errno variable.
 *
 * Input Parameters:
 *   filep  - The caller provided location in which to return the 'struct
 *            file' instance.
 *   path   - The full path to the file to be opened.
 *   oflags - open flags.
 *   umask  - File mode creation mask. Overrides the open flags.
 *   ap     - Variable argument list, may include 'mode_t mode'
 *
 * Returned Value:
 *   Zero (OK) is returned on success.  On failure, a negated errno value is
 *   returned.
 *
 ****************************************************************************/

static int file_vopen(FAR struct file *filep, FAR const char *path,
                      int oflags, mode_t umask, va_list ap)
{
  struct inode_search_s desc;
  FAR struct inode *inode;
#if !defined(CONFIG_DISABLE_MOUNTPOINT) || defined(CONFIG_PSEUDOFS_FILE)
  mode_t mode = 0666;
#endif
  int ret;

  if (path == NULL)
    {
      return -EINVAL;
    }

#ifndef CONFIG_DISABLE_MOUNTPOINT

  /* If the file is opened for creation, then get the mode bits */

  if ((oflags & O_CREAT) != 0)
    {
      mode = va_arg(ap, mode_t);
    }

  mode &= ~umask;
#endif

  /* Get an inode for this file */

  SETUP_SEARCH(&desc, path, (oflags & O_NOFOLLOW) != 0);

  ret = inode_find(&desc);
  if (ret < 0)
    {
#ifdef CONFIG_PSEUDOFS_FILE
      if ((oflags & O_CREAT) != 0)
        {
          ret = pseudofile_create(&desc.node, path, mode);
        }
#endif

      if (ret < 0)
        {
          /* "O_CREAT is not set and the named file does not exist.  Or, a
           * directory component in pathname does not exist or is a dangling
           * symbolic link."
           */

          goto errout_with_search;
        }
    }

  /* Get the search results */

  inode = desc.node;
  DEBUGASSERT(inode != NULL);

  if (desc.nofollow && INODE_IS_SOFTLINK(inode))
    {
      return -ELOOP;
    }

#if defined(CONFIG_BCH) && \
    !defined(CONFIG_DISABLE_MOUNTPOINT) && \
    !defined(CONFIG_DISABLE_PSEUDOFS_OPERATIONS)
  /* If the inode is block driver, then we may return a character driver
   * proxy for the block driver.  block_proxy() will instantiate a BCH
   * character driver wrapper around the block driver, open(), then
   * unlink() the character driver.
   *
   * NOTE: This will recurse to open the character driver proxy.
   */

  if (INODE_IS_BLOCK(inode) || INODE_IS_MTD(inode))
    {
      /* Release the inode reference */

      inode_release(inode);
      RELEASE_SEARCH(&desc);

      /* Get the file structure of the opened character driver proxy */

#ifdef CONFIG_BCH_DEVICE_READONLY
      ret = block_proxy(filep, path, O_RDOK);
#else
      ret = block_proxy(filep, path, oflags);
#endif
#ifdef CONFIG_FS_NOTIFY
      if (ret >= 0)
        {
          notify_open(path, filep->f_oflags);
        }
#endif

      return ret;
    }
#endif

  /* Make sure that the inode supports the requested access */

  ret = inode_checkflags(inode, oflags);
  if (ret < 0)
    {
      goto errout_with_inode;
    }

  /* Associate the inode with a file structure */

  filep->f_oflags = oflags;
  filep->f_inode  = inode;

  /* Perform the driver open operation.  NOTE that the open method may be
   * called many times.  The driver/mountpoint logic should handle this
   * because it may also be closed that many times.
   */

  if (oflags & O_DIRECTORY)
    {
      ret = dir_allocate(filep, desc.relpath);
    }
#ifndef CONFIG_DISABLE_MOUNTPOINT
  else if (INODE_IS_MOUNTPT(inode))
    {
      if (inode->u.i_mops->open != NULL)
        {
          ret = inode->u.i_mops->open(filep, desc.relpath, oflags, mode);
        }
    }
#endif
  else if (INODE_IS_DRIVER(inode) || INODE_IS_PIPE(inode))
    {
      if (inode->u.i_ops->open != NULL)
        {
          ret = inode->u.i_ops->open(filep);
        }
    }
  else
    {
      ret = -ENXIO;
    }

  if (ret == -EISDIR && ((oflags & O_WRONLY) == 0))
    {
      ret = dir_allocate(filep, desc.relpath);
    }

  if (ret < 0)
    {
      goto errout_with_inode;
    }

  RELEASE_SEARCH(&desc);
#ifdef CONFIG_FS_NOTIFY
  notify_open(path, filep->f_oflags);
#endif
  return OK;

errout_with_inode:
  filep->f_inode = NULL;
  inode_release(inode);

errout_with_search:
  RELEASE_SEARCH(&desc);
  return ret;
}

/****************************************************************************
 * Name: nx_vopen
 *
 * Description:
 *   nx_vopen() is similar to the standard 'open' interface except that it
 *   is not a cancellation point and it does not modify the errno variable.
 *
 *   nx_open() is an internal NuttX interface and should not be called from
 *   applications.
 *
 * Input Parameters:
 *   tcb    - Address of the task's TCB
 *   path   - The full path to the file to be opened.
 *   oflags - open flags.
 *   ap     - Variable argument list, may include 'mode_t mode'
 *
 * Returned Value:
 *   The new file descriptor is returned on success; a negated errno value is
 *   returned on any failure.
 *
 ****************************************************************************/

static int nx_vopen(FAR struct fdlist *list,
                    FAR const char *path, int oflags, va_list ap)
{
  FAR struct file *filep;
  int ret;
  int fd;

  /* Allocate a new file descriptor for the inode */

  fd = fdlist_allocate(list, oflags, 0, &filep);
  if (fd < 0)
    {
      return fd;
    }

  /* Let file_vopen() do all of the work */

  ret = file_vopen(filep, path, oflags, getumask(), ap);
  file_put(filep);
  if (ret < 0)
    {
      fdlist_close(list, fd);
      return ret;
    }

  return fd;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: file_open
 *
 * Description:
 *   file_open() is similar to the standard 'open' interface except that it
 *   populates an instance of 'struct file' rather than return a file
 *   descriptor.  It also is not a cancellation point and does not modify
 *   the errno variable.
 *
 * Input Parameters:
 *   filep  - The caller provided location in which to return the 'struct
 *            file' instance.
 *   path   - The full path to the file to be opened.
 *   oflags - open flags.
 *   ...    - Variable number of arguments, may include 'mode_t mode'
 *
 * Returned Value:
 *   Zero (OK) is returned on success.  On failure, a negated errno value is
 *   returned.
 *
 ****************************************************************************/

int file_open(FAR struct file *filep, FAR const char *path, int oflags, ...)
{
  va_list ap;
  int ret;

  memset(filep, 0, sizeof(*filep));

  va_start(ap, oflags);
  ret = file_vopen(filep, path, oflags, 0, ap);
  va_end(ap);

  return ret;
}

/****************************************************************************
 * Name: fdlist_open
 *
 * Description:
 *   fdlist_open() is similar to the standard 'open' interface except
 *   that it is not a cancellation point and it does not modify the errno
 *   variable.
 *
 *   fdlist_open() is an internal NuttX interface and should not be
 *   called from applications.
 *
 * Input Parameters:
 *   tcb    - Address of the task's TCB
 *   path   - The full path to the file to be opened.
 *   oflags - open flags.
 *   ...    - Variable number of arguments, may include 'mode_t mode'
 *
 * Returned Value:
 *   The new file descriptor is returned on success; a negated errno value is
 *   returned on any failure.
 *
 ****************************************************************************/

int fdlist_open(FAR struct fdlist *list,
                FAR const char *path, int oflags, ...)
{
  va_list ap;
  int fd;

  /* Let nx_vopen() do all of the work */

  va_start(ap, oflags);
  fd = nx_vopen(list, path, oflags, ap);
  va_end(ap);

  return fd;
}

/****************************************************************************
 * Name: nx_open
 *
 * Description:
 *   nx_open() is similar to the standard 'open' interface except that it is
 *   not a cancellation point and it does not modify the errno variable.
 *
 *   nx_open() is an internal NuttX interface and should not be called from
 *   applications.
 *
 * Input Parameters:
 *   path   - The full path to the file to be opened.
 *   oflags - open flags.
 *   ...    - Variable number of arguments, may include 'mode_t mode'
 *
 * Returned Value:
 *   The new file descriptor is returned on success; a negated errno value is
 *   returned on any failure.
 *
 ****************************************************************************/

int nx_open(FAR const char *path, int oflags, ...)
{
  va_list ap;
  int fd;

  /* Let nx_vopen() do all of the work */

  va_start(ap, oflags);
  fd = nx_vopen(nxsched_get_fdlist_from_tcb(this_task()), path, oflags, ap);
  va_end(ap);

  return fd;
}

/****************************************************************************
 * Name: open
 *
 * Description:
 *   Standard 'open' interface
 *
 * Returned Value:
 *   The new file descriptor is returned on success; -1 (ERROR) is returned
 *   on any failure with the errno value set appropriately.
 *
 ****************************************************************************/

int open(FAR const char *path, int oflags, ...)
{
  va_list ap;
  int fd;

  /* open() is a cancellation point */

  enter_cancellation_point();

  /* Let nx_vopen() do most of the work */

  va_start(ap, oflags);
  fd = nx_vopen(nxsched_get_fdlist_from_tcb(this_task()), path, oflags, ap);
  va_end(ap);

  /* Set the errno value if any errors were reported by nx_open() */

  if (fd < 0)
    {
      set_errno(-fd);
      fd = ERROR;
    }

  leave_cancellation_point();
  return fd;
}
