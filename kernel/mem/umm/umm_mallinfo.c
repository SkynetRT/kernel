/****************************************************************************
 * mm/umm_heap/umm_mallinfo.c
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

#include <malloc.h>

#include <nuttx/mm/mm.h>

#include "umm_heap.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: mallinfo
 *
 * Description:
 *   mallinfo returns a copy of updated current heap information for the
 *   user heap.
 *
 ****************************************************************************/

struct mallinfo mallinfo(void)
{
  return mm_mallinfo(USR_HEAP);
}

/****************************************************************************
 * Name: mallinfo_task
 *
 * Description:
 *   mallinfo_task returns a copy of updated current heap information of
 *   task with specified pid for the user heap.
 *
 ****************************************************************************/

struct mallinfo_task mallinfo_task(FAR const struct malltask *task)
{
  return mm_mallinfo_task(USR_HEAP, task);
}
