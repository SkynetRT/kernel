/****************************************************************************
4 * sched/irq/irq.h
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

#ifndef __SCHED_IRQ_IRQ_H
#define __SCHED_IRQ_IRQ_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/compiler.h>

#include <sys/types.h>
#include <stdbool.h>

#include <nuttx/arch.h>
#include <nuttx/irq.h>
#include <nuttx/spinlock.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#if defined(CONFIG_ARCH_MINIMAL_VECTORTABLE) && \
   !defined(CONFIG_ARCH_NUSER_INTERRUPTS)
#  error CONFIG_ARCH_NUSER_INTERRUPTS is not defined
#endif

#if defined(CONFIG_ARCH_MINIMAL_VECTORTABLE_DYNAMIC)
#  define IRQ_TO_NDX(irq) (g_irqmap[irq] ? g_irqmap[irq] : irq_to_ndx(irq))
#elif defined(CONFIG_ARCH_MINIMAL_VECTORTABLE)
#  define IRQ_TO_NDX(irq) \
  (g_irqmap[(irq)] < CONFIG_ARCH_NUSER_INTERRUPTS ? g_irqmap[(irq)] : -EINVAL)
#else
#  define IRQ_TO_NDX(irq) (irq)
#endif

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* This is the type of the list of interrupt handlers, one for each IRQ.
 * This type provided all of the information necessary to irq_dispatch to
 * transfer control to interrupt handlers after the occurrence of an
 * interrupt.
 */

struct irq_info_s
{
  xcpt_t handler;    /* Address of the interrupt handler */
  FAR void *arg;     /* The argument provided to the interrupt handler. */
#ifdef CONFIG_SCHED_IRQMONITOR
  clock_t start;     /* Time interrupt attached */
  clock_t time;      /* Maximum execution time on this IRQ */
  uint32_t count;    /* Number of interrupts on this IRQ */
#endif
};

#ifdef CONFIG_SCHED_IRQMONITOR
/* This is the type of the callback from irq_foreach(). */

typedef CODE int (*irq_foreach_t)(int irq, FAR struct irq_info_s *info,
                                  FAR void *arg);
#endif

/****************************************************************************
 * Public Data
 ****************************************************************************/

/* This is the list of interrupt handlers, one for each IRQ.  This is used
 * by irq_dispatch to transfer control to interrupt handlers after the
 * occurrence of an interrupt.
 */

#ifdef CONFIG_ARCH_MINIMAL_VECTORTABLE
extern struct irq_info_s g_irqvector[CONFIG_ARCH_NUSER_INTERRUPTS];
#else
extern struct irq_info_s g_irqvector[NR_IRQS];
#endif

/* This is the interrupt vector mapping table.  This must be provided by
 * architecture specific logic if CONFIG_ARCH_MINIMAL_VECTORTABLE is define
 * in the configuration.
 *
 * REVISIT: This should be declared in include/nuttx/irq.h.  The declaration
 * at that location, however, introduces a circular include dependency so the
 * declaration is here for the time being.
 */

#if defined(CONFIG_ARCH_MINIMAL_VECTORTABLE_DYNAMIC)
extern irq_mapped_t g_irqmap[NR_IRQS];
int irq_to_ndx(int irq);
#elif defined(CONFIG_ARCH_MINIMAL_VECTORTABLE)
extern const irq_mapped_t g_irqmap[NR_IRQS];
#endif

#ifdef CONFIG_SMP
/* This is the spinlock that enforces critical sections when interrupts are
 * disabled.
 */

extern volatile spinlock_t g_cpu_irqlock;

/* Used to keep track of which CPU(s) hold the IRQ lock. */

extern volatile cpu_set_t g_cpu_irqset;

/* Handles nested calls to enter_critical section from interrupt handlers */

extern volatile uint8_t g_cpu_nestcount[CONFIG_SMP_NCPUS];
#endif

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

/****************************************************************************
 * Name: irq_initialize
 *
 * Description:
 *   Configure the IRQ subsystem
 *
 ****************************************************************************/

void irq_initialize(void);

/****************************************************************************
 * Name: irq_unexpected_isr
 *
 * Description:
 *   An interrupt has been received for an IRQ that was never registered
 *   with the system.
 *
 ****************************************************************************/

int irq_unexpected_isr(int irq, FAR void *context, FAR void *arg);

/****************************************************************************
 * Name: irq_foreach
 *
 * Description:
 *   This function traverses the internal list of interrupts and provides
 *   information about each attached interrupt.
 *
 *   Some caution may be necessary:  If interrupts are disabled then the
 *   counts may change during the traversal.  If pre-emption is enabled, then
 *   the traversed sequence may be widely separated in time.
 *
 * Input Parameters:
 *   callback - This function will be called for each attached interrupt
 *              along with the IRQ number, an instance of struct irq_info_s,
 *              and the caller provided argument
 *   args     - This is an opaque argument provided with each call to the
 *              callback function.
 *
 * Returned Value:
 *   Zero (OK) is returned after callback has been invoked for all of
 *   the attached interrupts.  The callback function may terminate the
 *   traversal at any time by returning a non-zero value.  In that case,
 *   irq_foreach will return that non-zero value.
 *
 ****************************************************************************/

#ifdef CONFIG_SCHED_IRQMONITOR
int irq_foreach(irq_foreach_t callback, FAR void *arg);
#endif

#ifdef CONFIG_IRQCHAIN
void irqchain_initialize(void);
bool is_irqchain(int ndx, xcpt_t isr);
int irqchain_attach(int ndx, xcpt_t isr, FAR void *arg);
#endif

#undef EXTERN
#ifdef __cplusplus
}
#endif

#endif /* __SCHED_IRQ_IRQ_H */
