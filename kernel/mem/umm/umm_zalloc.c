/****************************************************************************
 * mm/umm_heap/umm_zalloc.c
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

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <nuttx/mm/mm.h>

#include "umm_heap.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: zalloc
 *
 * Description:
 *   Allocate and zero memory from the user heap.
 *
 * Input Parameters:
 *   size - Size (in bytes) of the memory region to be allocated.
 *
 * Returned Value:
 *   The address of the allocated memory (NULL on failure to allocate)
 *
 ****************************************************************************/

#undef zalloc /* See mm/README.txt */
FAR void *zalloc(size_t size)
{
#ifdef CONFIG_ARCH_ADDRENV
  /* Use malloc() because it implements the sbrk() logic */

  FAR void *mem = malloc(size);
  if (mem)
    {
       memset(mem, 0, size);
    }

  return mem;
#else
  FAR void *ret;

  /* Use mm_zalloc() because it implements the clear */

  ret = mm_zalloc(USR_HEAP, size);
  if (ret == NULL)
    {
      set_errno(ENOMEM);
    }
  else
    {
      mm_notify_pressure(mm_heapfree(USR_HEAP),
                         mm_heapfree_largest(USR_HEAP));
    }

  return ret;
#endif
}
