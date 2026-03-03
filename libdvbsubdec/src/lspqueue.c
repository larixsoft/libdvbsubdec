/*
 * Copyright 2010 Volkan Yazzici <volkan.yazici@gmail.com>
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lspqueue.h"
#include "lssubmacros.h"

#define left(i)      ((i) << 1)
#define right(i)     (((i) << 1) + 1)
#define parent(i)    ((i) >> 1)
/*-----------------------------------------------------------------------------
 * local static functions
 *---------------------------------------------------------------------------*/
static void
bubble_up(STMPqueue* q, uint32_t i)
{
  uint32_t parent_node;
  void* moving_node = q->data[i];
  STMPqPri moving_pri = q->getpri(moving_node);

  for (parent_node = parent(i);
       ((i > 1) &&
        q->cmppri(q->getpri(q->data[parent_node]), moving_pri));
       i = parent_node, parent_node = parent(i))
  {
    q->data[i] = q->data[parent_node];
    q->setpos(q->data[i], i);
  }

  q->data[i] = moving_node;
  q->setpos(moving_node, i);
}


static int32_t
maxchild(STMPqueue* q, uint32_t i)
{
  uint32_t child_node = left(i);

  if (child_node >= q->size)
  {
    return 0;
  }

  if (((child_node + 1) < q->size) &&
      q->cmppri(q->getpri(q->data[child_node]), q->getpri(q->data[child_node + 1])))
  {
    child_node++;
  }

  /* use right child instead of left */
  return child_node;
}


static void
percolate_down(STMPqueue* q, uint32_t i)
{
  uint32_t child_node;
  void* moving_node = q->data[i];
  STMPqPri moving_pri = q->getpri(moving_node);

  while ((child_node = maxchild(q, i)) &&
         q->cmppri(moving_pri, q->getpri(q->data[child_node])))
  {
    q->data[i] = q->data[child_node];
    q->setpos(q->data[i], i);
    i = child_node;
  }

  q->data[i] = moving_node;
  q->setpos(moving_node, i);
}


#if 0
static void
set_pos(void* data, uint32_t val)
{
  /* do nothing */
}


static void
set_pri(void* data, STMPqPri pri)
{
  /* do nothing */
}


#endif

static int32_t
subtree_is_valid(STMPqueue* q, int32_t pos)
{
  if (left(pos) < (int32_t)q->size)
  {
    /* has a left child */
    if (q->cmppri(q->getpri(q->data[pos]), q->getpri(q->data[left(pos)])))
    {
      return 1;
    }

    if (subtree_is_valid(q, left(pos)))
    {
      return 1;
    }
  }

  if (right(pos) < (int32_t)q->size)
  {
    /* has a right child */
    if (q->cmppri(q->getpri(q->data[pos]), q->getpri(q->data[right(pos)])))
    {
      return 1;
    }

    if (subtree_is_valid(q, right(pos)))
    {
      return 1;
    }
  }

  return 0;
}


/*-----------------------------------------------------------------------------
 * public functions
 *---------------------------------------------------------------------------*/
STMPqueue*
stm_pqueue_init(uint32_t       n,
                PqueueMallocF  allocator,
                PqueueFreeF    deallocator,
                PqueueReallocF reallocator,
                PqueueCmpPriF  cmppri,
                PqueueGetPriF  getpri,
                PqueueSetPriF  setpri,
                PqueueGetPosF  getpos,
                PqueueSetPosF  setpos)
{
  STMPqueue* q = NULL;
  int32_t status = LS_OK;

  LS_ENTER("n=%d\n", n);

  do
  {
    if ((n == 0) ||
        (allocator == NULL) ||
        (deallocator == NULL) ||
        (cmppri == NULL) ||
        (getpri == NULL) ||
        (setpri == NULL) ||
        (getpos == NULL) ||
        (setpos == NULL) ||
        (reallocator == NULL))
    {
      LS_ERROR("Invalid parameters\n");
      break;
    }

    if (!(q = allocator(sizeof(STMPqueue))))
    {
      LS_ERROR("allocator fail\n");
      break;
    }

    /* Need to allocate n+1 elements since element 0 isn't used. */
    if (!(q->data = allocator((n + 1) * sizeof(void*))))
    {
      LS_ERROR("allocator fail\n");
      deallocator(q);
      break;
    }

    if ((status = LS_MutexCreate(&(q->mutex))) == 0)
    {
      LS_ERROR("stm_mutex_creat() fail\n");
      deallocator(q->data);
      deallocator(q);
      break;
    }

#ifdef LS_PQ_DEBUG
    q->magic_id = LS_PQ_MAGIC;
#endif

    q->size = 1;
    q->avail = q->step = (n + 1);                                       /* see comment above about n+1 */
    q->allocator = allocator;
    q->deallocator = deallocator;
    q->reallocator = reallocator;
    q->cmppri = cmppri;
    q->setpri = setpri;
    q->getpri = getpri;
    q->getpos = getpos;
    q->setpos = setpos;
  }while (0);

  return q;
}


void
stm_pqueue_free(STMPqueue* q)
{
  int32_t status = LS_OK;

  LS_ENTER("q=%p\n", q);

  do
  {
    if (q == NULL)
    {
      LS_ERROR("Invalid parameter\n");
      break;
    }

#ifdef LS_PQ_DEBUG
    if (q->magic_id != LS_PQ_MAGIC)
    {
      LS_ERROR("q (%p) is crashed\n", q);
      break;
    }
#endif

    if ((status = LS_MutexWait(q->mutex)) == 0)
    {
      LS_ERROR("LS_MutexWait() fail,memory leaks!!!\n");
      break;
    }

    q->deallocator(q->data);
    q->data = NULL;

    if ((status = LS_MutexSignal((q->mutex))) == 0)
    {
      LS_ERROR("LS_MutexSignal() fail,memory leaks!!!\n");
      break;
    }

#ifdef LS_PQ_DEBUG
    q->magic_id = 0xDEADBEEF;
#endif
    q->deallocator(q);
  }while (0);

  LS_LEAVE("\n");
}


int32_t
stm_pqueue_size(STMPqueue* q)
{
  int32_t ret_val = 0;
  int32_t status = LS_OK;

  LS_ENTER("q=%p\n", q);

  do
  {
    if (q == NULL)
    {
      LS_ERROR("Invalid parameters\n");
      break;
    }

#ifdef LS_PQ_DEBUG
    if (q->magic_id != LS_PQ_MAGIC)
    {
      LS_ERROR("q (%p) is crashed\n", q);
      break;
    }
#endif

    if ((status = LS_MutexWait(q->mutex)) == 0)
    {
      LS_ERROR("LS_MutexWait() fail,memory leaks!!!\n");
      break;
    }

    /* queue element 0 exists but doesn't count since it isn't used. */
    ret_val = (q->size - 1);

    if ((status = LS_MutexSignal((q->mutex))) == 0)
    {
      LS_ERROR("LS_MutexWait() fail,memory leaks!!!\n");
      break;
    }
  }while (0);

  LS_ENTER("ret_val=%d\n", ret_val);
  return ret_val;
}


int32_t
stm_pqueue_insert(STMPqueue* q, void* data)
{
  void* tmp;
  uint32_t i;
  uint32_t newsize;
  int32_t status = LS_OK;
  int32_t ret_val = 0;

  LS_ENTER("q=%p,data=%p\n", q, data);

  do
  {
    if ((q == NULL) ||
        (data == NULL))
    {
      LS_ERROR("Invalid parameters\n");
      break;
    }

#ifdef LS_PQ_DEBUG
    if (q->magic_id != LS_PQ_MAGIC)
    {
      LS_ERROR("q (%p) is crashed\n", q);
      break;
    }
#endif

    if ((status = LS_MutexWait(q->mutex)) == 0)
    {
      LS_ERROR("LS_MutexWait() fail,memory leaks!!!\n");
      break;
    }

    /* allocate more memory if necessary */
    if (q->size >= q->avail)
    {
      newsize = q->size + q->step;

      if (!(tmp = q->reallocator(q->data, sizeof(void*) * newsize)))
      {
        LS_ERROR("q->reallocator() for %d bytes fail\n", (sizeof(void*) * newsize));
        break;
      }

      q->data = tmp;
      q->avail = newsize;
    }

    /* insert item */
    i = q->size++;
    q->data[i] = data;
    bubble_up(q, i);

    if ((status = LS_MutexSignal((q->mutex))) == 0)
    {
      LS_ERROR("LS_MutexWait() fail,memory leaks!!!\n");
      break;
    }

    ret_val = 1;
  }while (0);

  LS_LEAVE("ret_val=%d\n", ret_val);
  return ret_val;
}


void
stm_pqueue_change_priority(STMPqueue* q, STMPqPri new_pri, void* data)
{
  uint32_t posn;
  int32_t status = LS_OK;
  STMPqPri old_pri;

  LS_ENTER("q=%p,new_pri=%f,data=%p\n", q, new_pri, data);

  do
  {
    if ((q == NULL) ||
        (data == NULL))
    {
      LS_ERROR("Invalid parameters\n");
      break;
    }

#ifdef LS_PQ_DEBUG
    if (q->magic_id != LS_PQ_MAGIC)
    {
      LS_ERROR("q (%p) is crashed\n", q);
      break;
    }
#endif

    if ((status = LS_MutexWait(q->mutex)) == 0)
    {
      LS_ERROR("LS_MutexWait() fail," "memory leaks!!!\n");
      break;
    }

    old_pri = q->getpri(data);
    q->setpri(data, new_pri);
    posn = q->getpos(data);

    if (q->cmppri(old_pri, new_pri))
    {
      bubble_up(q, posn);
    }
    else
    {
      percolate_down(q, posn);
    }

    if ((status = LS_MutexSignal((q->mutex))) == 0)
    {
      LS_ERROR("LS_MutexSignal() fail," "DEADLOCK and MEMORY LEAKS!!!\n");
      break;
    }
  }while (0);

  LS_LEAVE("\n");
}


int32_t
stm_pqueue_remove(STMPqueue* q, void* data)
{
  uint32_t posn;
  int32_t ret_val = 0;
  int32_t status = LS_OK;

  LS_ENTER("q=%p,data=%p\n", q, data);

  do
  {
    if ((q == NULL) ||
        (data == NULL))
    {
      LS_ERROR("Invalid parameters\n");
      break;
    }

#ifdef LS_PQ_DEBUG
    if (q->magic_id != LS_PQ_MAGIC)
    {
      LS_ERROR("q (%p) is crashed\n", q);
      break;
    }
#endif

    if ((status = LS_MutexWait(q->mutex)) == 0)
    {
      LS_ERROR("LS_MutexWait() fail," "memory leaks!!!\n");
      break;
    }

    posn = q->getpos(data);
    q->data[posn] = q->data[--q->size];

    if (q->cmppri(q->getpri(data), q->getpri(q->data[posn])))
    {
      bubble_up(q, posn);
    }
    else
    {
      percolate_down(q, posn);
    }

    if ((status = LS_MutexSignal((q->mutex))) == 0)
    {
      LS_ERROR("LS_MutexSignal() fail," "DEADLOCK and MEMORY LEAKS!!!\n");
      break;
    }

    ret_val = 1;
  }while (0);

  LS_LEAVE("ret_val=%d\n", ret_val);
  return ret_val;
}


void*
stm_pqueue_pop(STMPqueue* q)
{
  void* ret_val = NULL;
  int32_t status = LS_OK;

  LS_ENTER("q=%p\n", q);

  do
  {
    if (q == NULL)
    {
      LS_ERROR("Invalid parameters\n");
      break;
    }

#ifdef LS_PQ_DEBUG
    if (q->magic_id != LS_PQ_MAGIC)
    {
      LS_ERROR("q (%p) is crashed\n", q);
      break;
    }
#endif

    if ((status = LS_MutexWait(q->mutex)) == 0)
    {
      LS_ERROR("LS_MutexWait() fail," "memory leaks!!!\n");
      break;
    }

    if (q->size == 1)
    {
      LS_DEBUG("empty q (%p)\n", q);
      ret_val = NULL;
    }
    else
    {
      ret_val = q->data[1];
      q->data[1] = q->data[--q->size];
      percolate_down(q, 1);
    }

    if ((status = LS_MutexSignal((q->mutex))) == 0)
    {
      LS_ERROR("LS_MutexSignal() fail," "DEADLOCK and MEMORY LEAKS!!!\n");
      break;
    }
  }while (0);

  LS_LEAVE("ret_val=%p\n", ret_val);
  return ret_val;
}


void*
stm_pqueue_peek(STMPqueue* q)
{
  void* ret_val = NULL;
  int32_t status = LS_OK;

  LS_ENTER("q=%p\n", q);

  do
  {
    if (q == NULL)
    {
      LS_ERROR("Invalid parameters\n");
      break;
    }

#ifdef LS_PQ_DEBUG
    if (q->magic_id != LS_PQ_MAGIC)
    {
      LS_ERROR("q (%p) is crashed\n", q);
      break;
    }
#endif

    if ((status = LS_MutexWait(q->mutex)) == 0)
    {
      LS_ERROR("LS_MutexWait() fail," "memory leaks!!!\n");
      break;
    }

    if (q->size == 1)
    {
      LS_DEBUG("empty q (%p)\n", q);
    }

    ret_val = q->data[1];

    if ((status = LS_MutexSignal((q->mutex))) == 0)
    {
      LS_ERROR("LS_MutexSignal() fail," "DEADLOCK and MEMORY LEAKS!!!\n");
      break;
    }
  }while (0);

  LS_LEAVE("ret_val=%p\n", ret_val);
  return ret_val;
}


void
stm_pqueue_dump(STMPqueue* q, PqueuePrintEntryF print_entry)
{
  int32_t i;
  int32_t status = LS_OK;

  LS_ENTER("q=%p\n", q);

  do
  {
    if (q == NULL)
    {
      LS_ERROR("Invalid parameters\n");
      break;
    }

#ifdef LS_PQ_DEBUG
    if (q->magic_id != LS_PQ_MAGIC)
    {
      LS_ERROR("q (%p) is crashed\n", q);
      break;
    }
#endif

    if ((status = LS_MutexWait(q->mutex)) == 0)
    {
      LS_ERROR("LS_MutexWait() fail," "memory leaks!!!\n");
      break;
    }

    fprintf(stdout, "posn\tleft\tright\tparent\tmaxchild\t...\n");

    for (i = 1; i < (int32_t)q->size; i++)
    {
      fprintf(stdout, "%d\t%d\t%d\t%d\t%d\t", i, left(i), right(i), parent(i), maxchild(q, i));
      print_entry(q->data[i]);
    }

    if ((status = LS_MutexSignal((q->mutex))) == 0)
    {
      LS_ERROR("LS_MutexSignal() fail," "DEADLOCK and MEMORY LEAKS!!!\n");
      break;
    }
  }while (0);

  LS_LEAVE("\n");
}


int32_t
stm_pqueue_is_valid(STMPqueue* q)
{
  int32_t ret_val = 0;
  int32_t status = LS_OK;

  LS_ENTER("q=%p\n", q);

  do
  {
    if (q == NULL)
    {
      LS_ERROR("Invalid parameters\n");
      break;
    }

#ifdef LS_PQ_DEBUG
    if (q->magic_id != LS_PQ_MAGIC)
    {
      LS_ERROR("q (%p) is crashed\n", q);
      break;
    }
#endif

    if ((status = LS_MutexWait(q->mutex)) == 0)
    {
      LS_ERROR("LS_MutexWait() fail," "memory leaks!!!\n");
      break;
    }

    ret_val = subtree_is_valid(q, 1);

    if ((status = LS_MutexSignal((q->mutex))) == 0)
    {
      LS_ERROR("LS_MutexWait() fail," "memory leaks!!!\n");
      break;
    }
  }while (0);

  LS_LEAVE("ret_val=%d\n", ret_val);
  return ret_val;
}
