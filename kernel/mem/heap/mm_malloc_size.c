/****************************************************************************
 * mm/mm_heap/mm_malloc_size.c
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

#include <assert.h>
#include <debug.h>

#include <nuttx/mm/kasan.h>
#include <nuttx/mm/mm.h>

#include "mm.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

size_t mm_malloc_size(FAR struct mm_heap_s *heap, FAR void *mem)
{
  FAR struct mm_freenode_s *node;
  ssize_t size;
  bool flag;

  flag = kasan_bypass(true);
#ifdef CONFIG_MM_HEAP_MEMPOOL
  if (heap->mm_mpool)
    {
      size = mempool_multiple_alloc_size(heap->mm_mpool, mem);
      if (size >= 0)
        {
          kasan_bypass(flag);
          return size;
        }
    }
#endif

  /* Protect against attempts to query a NULL reference */

  if (!mem)
    {
      return 0;
    }

  /* Map the memory chunk into a free node */

  node = (FAR struct mm_freenode_s *)((FAR char *)mem - MM_SIZEOF_ALLOCNODE);

  /* Sanity check against double-frees */

  DEBUGASSERT(MM_NODE_IS_ALLOC(node));

  size = MM_SIZEOF_NODE(node) - MM_ALLOCNODE_OVERHEAD;

  kasan_bypass(flag);
  return size;
}
