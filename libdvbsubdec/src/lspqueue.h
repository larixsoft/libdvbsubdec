/*
 * Copyright 2010 Volkan Yazıcı <volkan.yazici@gmail.com>
 * Copyright 2006-2010 The Apache Software Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */
/**
 * @file  pqueue.h
 * @brief Priority Queue function declarations
 *
 * @{
 */


#ifndef __LS_PQUEUE_H__
#define __LS_PQUEUE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "lssystem.h"


#ifndef NDEBUG

#define LS_PQ_DEBUG
#define LS_PQ_MAGIC    MAKE_MAGIC_NUMBER('Q', 'U', 'E', 'U')
#else

#undef LS_PQ_DEBUG
#undef LS_PQ_MAGIC

#endif
/** priority data type */
typedef double STMPqPri;
/** callback functions to malloc/free memory */
typedef void* (*PqueueMallocF)(uint32_t bytes);
typedef void (*PqueueFreeF)(void* ptr);
typedef void* (*PqueueReallocF)(void* ptr, uint32_t new_bytes);
/** callback functions to get/set/compare the priority of an element */
typedef STMPqPri (*PqueueGetPriF)(void* a);
typedef void (*PqueueSetPriF)(void* a, STMPqPri pri);
typedef int32_t (*PqueueCmpPriF)(STMPqPri next, STMPqPri curr);
/** callback functions to get/set the position of an element */
typedef int32_t (*PqueueGetPosF)(void* a);
typedef void (*PqueueSetPosF)(void* a, uint32_t pos);
/** debug callback function to print a entry */
typedef void (*PqueuePrintEntryF)(void* a);
/** the priority queue serviceID */
typedef struct stm_pqueue_t
{
#ifdef LS_PQ_DEBUG
  uint32_t       magic_id;
#endif
  uint32_t       size;
  uint32_t       avail;
  uint32_t       step;
  PqueueMallocF  allocator;
  PqueueFreeF    deallocator;
  PqueueReallocF reallocator;
  PqueueCmpPriF  cmppri;
  PqueueGetPriF  getpri;
  PqueueSetPriF  setpri;
  PqueueGetPosF  getpos;
  PqueueSetPosF  setpos;
  void* *        data;
  LS_Mutex_t     mutex;
} STMPqueue;
/**
 * initialize the queue
 *
 * @param n the initial estimate of the number of queue items for which memory
 *          should be preallocated
 * @param pri the callback function to run to assign a score to a element
 * @param get the callback function to get the current element's position
 * @param set the callback function to set the current element's position
 *
 * @Return the serviceID or NULL for insufficent memory
 */
STMPqueue* stm_pqueue_init(uint32_t       n,
                           PqueueMallocF  allocator,
                           PqueueFreeF    deallocator,
                           PqueueReallocF reallocator,
                           PqueueCmpPriF  cmppri,
                           PqueueGetPriF  getpri,
                           PqueueSetPriF  setpri,
                           PqueueGetPosF  getpos,
                           PqueueSetPosF  setpos);

/**
 * free all memory used by the queue
 * @param q the queue
 */
void stm_pqueue_free(STMPqueue* q);

/**
 * return the size of the queue.
 * @param q the queue
 */
int32_t stm_pqueue_size(STMPqueue* q);

/**
 * insert an item into the queue.
 * @param q the queue
 * @param d the item
 * @return 1 on success
 */
int32_t stm_pqueue_insert(STMPqueue* q, void* d);

/**
 * move an existing entry to a different priority
 * @param q the queue
 * @param old the old priority
 * @param d the entry
 */
void stm_pqueue_change_priority(STMPqueue* q, STMPqPri new_pri, void* d);

/**
 * pop the highest-ranking item from the queue.
 * @param p the queue
 * @param d where to copy the entry to
 * @return NULL on error, otherwise the entry
 */
void* stm_pqueue_pop(STMPqueue* q);

/**
 * remove an item from the queue.
 * @param p the queue
 * @param d the entry
 * @return 1 on success
 */
int32_t stm_pqueue_remove(STMPqueue* q, void* d);

/**
 * access highest-ranking item without removing it.
 * @param q the queue
 * @param d the entry
 * @return NULL on error, otherwise the entry
 */
void* stm_pqueue_peek(STMPqueue* q);

/**
 * dump the queue and it's internal structure
 * @internal
 * debug function only
 * @param q the queue
 * @param the callback function to print the entry
 */
void stm_pqueue_dump(STMPqueue* q, PqueuePrintEntryF print_entry);

/**
 * checks that the pq is in the right order, etc
 * @internal
 * debug function only
 * @param q the queue
 */
int32_t stm_pqueue_is_valid(STMPqueue* q);


#ifdef __cplusplus
}
#endif

#endif /* PQUEUE_H */
/** @} */
