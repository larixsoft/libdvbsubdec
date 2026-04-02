/*-----------------------------------------------------------------------------
 * lsmemory.c
 *
 * Copyright (c) Larixsoft Inc
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at your
 * option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *---------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include "lsmemory.h"
#include "lssystem.h"
/*-----------------------------------------------------------------------------
 * System Malloc Wrapper (fastest path)
 *---------------------------------------------------------------------------*/
#ifdef LS_MEM_USE_SYSTEM_MALLOC
/* When LS_USE_SYSTEM_MALLOC is defined, use system malloc/free directly */

void*
LS_Malloc(LS_MemHeap* heap, uint32_t bytes)
{
  (void)heap;  /* Heap parameter is unused but kept for API compatibility */
  return SYS_MALLOC(bytes);
}

int32_t
LS_Free(LS_MemHeap* heap, void* ptr)
{
  (void)heap;  /* Heap parameter is unused but kept for API compatibility */
  if (ptr)
  {
    SYS_FREE(ptr);
  }
  return LS_OK;
}

void*
LS_Realloc(LS_MemHeap* heap, void* ptr, uint32_t bytes)
{
  (void)heap;  /* Heap parameter is unused but kept for API compatibility */
  return SYS_REALLOC(ptr, bytes);
}

int32_t
LS_MemInit(LS_MemHeap** heap, uint32_t heap_size)
{
  (void)heap_size;  /* Size is ignored when using system malloc */

  LS_MemHeap* h = SYS_CALLOC(1, sizeof(LS_MemHeap));
  if (!h)
  {
    return LS_ERROR_SYSTEM_ERROR;
  }

  /* Initialize basic fields */
#ifdef LS_MEM_DEBUG_HEAP
  h->magic_id = LS_MEM_MAGIC_NUMBER;
  h->peak = 0;
#endif
  h->heap_units = UINT32_MAX;  /* No limit */
  h->allocated = 0;
  LS_MutexCreate(&h->mutex);

  *heap = h;
  LS_INFO("Using system malloc allocator (no heap size limit)\n");
  return LS_OK;
}

int32_t
LS_MemFinalize(LS_MemHeap* heap)
{
  if (!heap)
  {
    return LS_ERROR_GENERAL;
  }

  LS_MutexDelete(heap->mutex);
  SYS_FREE(heap);
  return LS_OK;
}

#endif /* LS_MEM_USE_SYSTEM_MALLOC */
/*-----------------------------------------------------------------------------
 * Custom Heap Implementation
 *---------------------------------------------------------------------------*/
#ifndef LS_MEM_USE_SYSTEM_MALLOC
static void*   __heap_malloc(LS_MemHeap* heap, uint32_t bytes);
static int32_t __heap_free(LS_MemHeap* heap, void* ptr);
static void*   __heap_realloc(LS_MemHeap* heap, void* ptr, uint32_t bytes);

/*--------------------------------------------------------------------
 * public function implementation
 *-------------------------------------------------------------------*/
int32_t
LS_MemInit(LS_MemHeap* * heap, uint32_t heap_size)
{
  int32_t status = LS_OK;
  int32_t ret_val = LS_OK;

  LS_ENTER(",heap=%p,heap_size=%d\n", heap, heap_size);

  do
  {
    if ((heap == NULL) ||
        (heap_size == 0))
    {
      LS_ERROR("LS_ERROR_GENERAL: Invalid parameters: heap=%p,heap_size=%d\n", heap, heap_size);
      ret_val = LS_ERROR_GENERAL;
      break;
    }

    (*heap) = SYS_MALLOC(sizeof(LS_MemHeap));

    if (*heap == NULL)
    {
      LS_ERROR("LS_ERROR_SYSTEM_ERROR: request for %d bytes failed\n", sizeof(LS_MemHeap));
      ret_val = LS_ERROR_SYSTEM_ERROR;
      break;
    }

    SYS_MEMSET((*heap), 0, sizeof(LS_MemHeap));

#ifdef LS_MEM_DEBUG_HEAP
    (*heap)->magic_id = LS_MEM_MAGIC_NUMBER;
#endif

#ifdef LS_INTERNAL_HEAP
    /* Check for integer overflow in heap_units calculation */
    if (heap_size > UINT32_MAX - LS_MEM_NODE_SIZE + 1)
    {
      LS_ERROR("Error: heap_size %u would cause integer overflow\n", heap_size);
      SYS_FREE((*heap));
      *heap = NULL;
      ret_val = LS_ERROR_GENERAL;
      break;
    }
    (*heap)->heap_units = (heap_size + LS_MEM_NODE_SIZE - 1) / LS_MEM_NODE_SIZE;

    /* Check for integer overflow in memory allocation */
    if ((*heap)->heap_units > UINT32_MAX / LS_MEM_NODE_SIZE)
    {
      LS_ERROR("Error: heap_units %u would cause integer overflow in allocation\n", (*heap)->heap_units);
      SYS_FREE((*heap));
      *heap = NULL;
      ret_val = LS_ERROR_GENERAL;
      break;
    }
#else
    (*heap)->heap_units = heap_size;
#endif
    LS_TRACE("LS_MEM_NODE_SIZE is %d\n", LS_MEM_NODE_SIZE);
    LS_INFO("Heap_size was rounded from %d bytes to %d bytes\n", heap_size, ((*heap)->heap_units) * LS_MEM_NODE_SIZE);
    status = LS_MutexCreate(&((*heap)->mutex));

    if (status != LS_OK)
    {
      LS_ERROR("LS_ERROR_SYSTEM_ERROR: create mutex failed\n");
      SYS_FREE((*heap));
      *heap = NULL;        /* Prevent dangling pointer */
      ret_val = LS_ERROR_SYSTEM_ERROR;
      break;
    }

    LS_TRACE("creat mutex %p\n", &((*heap)->mutex));

#ifdef LS_INTERNAL_HEAP
    (*heap)->memory = SYS_MALLOC((*heap)->heap_units * LS_MEM_NODE_SIZE);

    if ((*heap)->memory == NULL)
    {
      LS_ERROR("LS_ERROR_SYSTEM_ERROR: request for %d bytes failed\n", heap_size);
      LS_MutexDelete((*heap)->mutex);
      SYS_FREE((*heap));
      *heap = NULL;        /* Prevent dangling pointer */
      ret_val = LS_ERROR_SYSTEM_ERROR;
      break;
    }
    else
    {
      LS_INFO("Heap memory starts from :%p to %p\n", (*heap)->memory, (*heap)->memory + (*heap)->heap_units);
      /*set up the anchor*/
      (*heap)->anchor.next = (*heap)->memory;
      (*heap)->anchor.next->units = (*heap)->heap_units;
      (*heap)->anchor.next->next = &((*heap)->anchor);
      LS_TRACE("(*heap)->anchor.next = %p\n", (*heap)->anchor.next);
      LS_TRACE("(*heap)->anchor.next->units = %d (%d/0x%x bytes)\n",
               (*heap)->anchor.next->units,
               (*heap)->anchor.next->units * LS_MEM_NODE_SIZE,
               (*heap)->anchor.next->units * LS_MEM_NODE_SIZE);
      LS_TRACE("(*heap)->anchor.next->next = %p\n", (*heap)->anchor.next->next);
      /*everything seems good to go....*/
      ret_val = LS_OK;
      break;
    }
#endif
  }while (0);

  LS_LEAVE("ret=%d\n", ret_val);
  return ret_val;
}


int32_t
LS_MemFinalize(LS_MemHeap* heap)
{
  int32_t status = LS_OK;

  if (heap)
  {
#ifdef LS_INTERNAL_HEAP
    if (heap->memory)
    {
      SYS_FREE(heap->memory);
    }
#endif
    status = LS_MutexDelete(heap->mutex);

    if (status != LS_OK)
    {
      LS_ERROR("LS_ERROR_SYSTEM_ERROR: delete mutex failed,ignored in release build ...\n");
    }

    heap->mutex = NULL;
    SYS_FREE(heap);
    return LS_OK;
  }
  else
  {
    LS_ERROR("LS_ERROR_GENERAL: Invalid parameter!\n");
    return LS_ERROR_GENERAL;
  }
}


void*
LS_Malloc(LS_MemHeap* heap, uint32_t bytes)
{
  void* ret_val = NULL;
  int32_t status = 1;

#if 0
  return malloc(bytes);
#endif

  LS_ENTER("heap=%p,bytes=%d\n", heap, bytes);

  do
  {
    if (heap == NULL
#ifdef LS_MEM_DEBUG_HEAP
        || (heap->magic_id != LS_MEM_MAGIC_NUMBER)
#endif
        )
    {
      LS_ERROR("LS_ERROR_GENERAL: Invalid heap %p\n", (void*)heap);
      ret_val = NULL;
      break;
    }

    if (bytes == 0)
    {
      ret_val = NULL;
      break;
    }

    /* Check for integer overflow before calculating request size */
    if (bytes > UINT32_MAX - LS_MEM_NODE_SIZE + 1)
    {
      LS_ERROR("Error: allocation size %u would cause integer overflow\n", bytes);
      ret_val = NULL;
      break;
    }

    LS_TRACE("waitting for mutex %p\n", &(heap->mutex));
    status = LS_MutexWait(heap->mutex);

    if (status == 0)
    {
      LS_ERROR("LS_ERROR_SYSTEM_ERROR: wait for mutex failed!\n");
      ret_val = NULL;
      break;
    }

    ret_val = __heap_malloc(heap, bytes);
    status = LS_MutexSignal((heap->mutex));

    if (status == 0)
    {
      LS_ERROR("LS_ERROR_SYSTEM_ERROR: signal mutex failed!\n");
      /* Only free if allocation succeeded */
      if (ret_val != NULL)
      {
        __heap_free(heap, ret_val);
      }
      ret_val = NULL;
      break;
    }
  }while (0);

  /*everything seems good to go...*/
  LS_LEAVE("ret_val=%p\n", ret_val);
  return ret_val;
}


int32_t
LS_Free(LS_MemHeap* heap, void* ptr)
{
  int32_t ret_val = 1;
  int32_t status = 1;


#if 0
  free(ptr);
  return LS_TRUE;
#endif

  LS_ENTER("heap=%p,ptr=%p\n", heap, ptr);

  do
  {
    if (heap == NULL
#ifdef LS_MEM_DEBUG_HEAP
        || (heap->magic_id != LS_MEM_MAGIC_NUMBER)
#endif
        )
    {
      LS_ERROR("LS_ERROR_GENERAL: Invalid heap %p\n", (void*)heap);
      ret_val = LS_ERROR_GENERAL;
      break;
    }

    if (ptr == NULL)
    {
      ret_val = LS_TRUE;
      break;
    }

    status = LS_MutexWait(heap->mutex);

    if (status != LS_TRUE)
    {
      LS_ERROR("LS_ERROR_SYSTEM_ERROR: wait for mutex failed!\n");
      ret_val = LS_ERROR_SYSTEM_ERROR;
      break;
    }

    ret_val = __heap_free(heap, ptr);
    status = LS_MutexSignal((heap->mutex));

    if (status != LS_TRUE)
    {
      LS_ERROR("LS_ERROR_SYSTEM_ERROR: signal mutex failed!\n");
      ret_val = LS_ERROR_SYSTEM_ERROR;
      break;
    }
  }while (0);

  /*everything seems good to go... ...*/
  LS_LEAVE("ret=%d\n", ret_val);
  return ret_val;
}


void*
LS_Realloc(LS_MemHeap* heap, void* ptr, uint32_t bytes)
{
  void* ret_val = NULL;
  int32_t status = LS_TRUE;

  LS_ENTER("heap=%p,ptr=%p,bytes=%d\n", heap, ptr, bytes);

  do
  {
    if ((heap == NULL) ||
        (bytes <= 0)
#ifdef LS_MEM_DEBUG_HEAP
        || (heap->magic_id != LS_MEM_MAGIC_NUMBER)
#endif
        )
    {
      LS_ERROR("LS_ERROR_GENERAL: Invalid heap or bytes\n");
      ret_val = NULL;
      break;
    }

    /* Handle realloc(NULL, bytes) like malloc(bytes) */
    if (ptr == NULL)
    {
      status = LS_MutexWait(heap->mutex);
      if (status != LS_TRUE)
      {
        LS_ERROR("LS_ERROR_SYSTEM_ERROR: wait for mutex failed\n");
        ret_val = NULL;
        break;
      }
      ret_val = __heap_malloc(heap, bytes);
      status = LS_MutexSignal((heap->mutex));
      if (status != LS_TRUE)
      {
        LS_ERROR("LS_ERROR_SYSTEM_ERROR: signal mutex failed\n");
        ret_val = NULL;
      }
      break;
    }

    status = LS_MutexWait(heap->mutex);

    if (status != LS_TRUE)
    {
      LS_ERROR("LS_ERROR_SYSTEM_ERROR: wait for mutex failed\n");
      ret_val = NULL;
      break;
    }

    ret_val = __heap_realloc(heap, ptr, bytes);
    status = LS_MutexSignal((heap->mutex));

    if (status != LS_TRUE)
    {
      LS_ERROR("LS_ERROR_SYSTEM_ERROR: signal mutex failed\n");
      __heap_free(heap, ret_val);
      ret_val = NULL;
      break;
    }
  }while (0);

  /*everything seems good to go ... ... */
  LS_LEAVE("ret_val=%p\n", ret_val);
  return ret_val;
}


void
LS_DumpMem(const char* mem, const uint32_t size)
{
  /*-------------------------------------------------------------------------
   * local defines
   *-----------------------------------------------------------------------*/
#define _ascii_isprint(letter) \
        (((char)letter >= 32 && \
          (char)letter <= 126) ? 1 : 0)
  /*------------------------------------------------------------------------
   * function code
   *-----------------------------------------------------------------------*/
  uint32_t i, j;
  char string[50];
  char chars[18];
  char addr[20];
  uint32_t addr_size;
  char label[128];

  printf("start to dump 0x%08x (%d) bytes from %p\n", size, size, (void*)mem);
  snprintf(addr, sizeof(addr), "%p", mem);
  addr_size = SYS_STRLEN(addr);
  addr_size += 13;

  for (i = 0; i < addr_size; i++)
  {
    snprintf(label + i, sizeof(label) - i, "%c", ' ');
  }

  snprintf(label + addr_size, sizeof(label) - addr_size, "%s", "00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F" "     0123456789ABCDEF\n");
  printf("%s", label);
  addr_size += 66;
  SYS_MEMSET(label, '-', addr_size);
  label[addr_size] = '\0';
  printf("%s\n", label);
  SYS_MEMSET(string, 0x20, 50);
  SYS_MEMSET(chars, 0x20, 18);
  i = j = 0;

  while (i < size)
  {
    if (_ascii_isprint(mem[i]))
    {
      chars[i % 16] = mem[i];
    }
    else
    {
      chars[i % 16] = '.';
    }

    snprintf(string + (i % 16) * 3, sizeof(string) - (i % 16) * 3, "%02x ", (uint8_t)mem[i]);
    string[(i % 16) * 3 + 3] = 0x20;
    j++;
    i++;

    if ((j == 16) ||
        (i == size))
    {
      string[49] = 0;
      chars[17] = 0;
      printf("%08x (%p): %s %s\n", (i - j), mem + i - j, string, chars);
      SYS_MEMSET(string, 0x20, 50);
      SYS_MEMSET(chars, 0x20, 18);
      j = 0;
    }
  }

#undef _ascii_isprint
}


/****************************************************************/
void
LS_MemCopy(void* pDest, const void* pSrc, uint32_t bytes)
{
  memcpy(pDest, pSrc, bytes);
}


#ifdef LS_INTERNAL_HEAP

#ifdef LS_MEM_DEBUG_HEAP
static int32_t
stm_check_heap(LS_MemHeap* heap)
{
  LS_MemNode* curr;
  uint32_t total = 0;
  uint32_t max_block = 0;
  uint32_t num_blocks = 0;
  double frag;
  uint32_t max_units = heap->heap_units;

  LS_TRACE("heap range [%p-%p]\n", heap->memory, heap->memory + heap->heap_units / LS_MEM_NODE_SIZE);
  curr = heap->anchor.next;

  while (curr != &(heap->anchor))
  {
    if ((curr < heap->memory) ||
        (curr > heap->memory + max_units))
    {
      LS_ERROR("Error: curr (%p) is out of heap[%p-%p]\n", curr, heap->memory, heap->memory + max_units);
      return 0;
    }

    if (curr->units > heap->heap_units)
    {
      LS_ERROR("Error in current slot:curr=%p," "curr->units = %d (%d bytes)," "heap->heap_units = %d (%d bytes)\n",
               curr,
               curr->units,
               curr->units * LS_MEM_NODE_SIZE,
               heap->heap_units,
               heap->heap_units * LS_MEM_NODE_SIZE);
      return 0;
    }

    if ((curr + curr->units > curr->next) &&
        (curr->next != &(heap->anchor)))
    {
      LS_ERROR("The end of free block [%p-%p]," "size = %d(%d bytes)," "enters another free block [%p-%p],"
               "size = %d (%d bytes)," "overlap = %d\n",
               curr,
               curr + curr->units,
               curr->units,
               curr->units * LS_MEM_NODE_SIZE,
               curr->next,
               curr->next + curr->next->units,
               curr->next->units,
               curr->next->units * LS_MEM_NODE_SIZE,
               (curr + curr->units - curr->next) * LS_MEM_NODE_SIZE);
      return 0;
    }

    if (curr->units > max_block)
    {
      max_block = curr->units;
    }

    num_blocks++;
    total += curr->units;
    curr = curr->next;
  }

  if (max_units != heap->allocated + total)
  {
    LS_ERROR("Error in units,max_units= %d,got %d\n", max_units, heap->allocated + total);
    return 0;
  }

  frag = 1.0 - (double)max_block / (double)total;
  LS_TRACE("Total %d free blocks,%d(%dk) bytes" " and max_block is %d (%dk) bytes,fragmentation rate = %f\n",
           num_blocks,
           total * LS_MEM_NODE_SIZE,
           total * LS_MEM_NODE_SIZE / 1024,
           max_block * LS_MEM_NODE_SIZE,
           max_block * LS_MEM_NODE_SIZE / 1024,
           frag);
  LS_TRACE("The heap is good to go!\n");
  return 1;
}


#endif                                                                                        /*LS_MEM_DEBUG_HEAP*/
static void
stm_dump_heap(LS_MemHeap* heap)
{
#ifdef LS_MEM_DUMP_HEAP
  LS_MemNode* current = NULL;

  DEBUG_CHECK(heap != NULL);

  if (heap == NULL)
  {
    LS_ERROR("Invalid parameter:heap = NULL\n");
    return;
  }

  LS_INFO("LS_MemHeap* heap = %p\n", (void*)heap);
#ifdef LS_MEM_DEBUG_HEAP
  LS_INFO("magic_id             = 0x%08x\n", heap->magic_id);
  LS_INFO("peak                     = %d (%d/0x%08x bytes)\n",
          heap->peak,
          heap->peak * LS_MEM_NODE_SIZE,
          heap->peak * LS_MEM_NODE_SIZE);
#endif
  LS_INFO("anchor.next        = %p\n", (void*)(heap->anchor.next));
  LS_INFO("anchor.units     = %d (%d/0x%08x bytes)\n",
          heap->anchor.units,
          heap->anchor.units * LS_MEM_NODE_SIZE,
          heap->anchor.units * LS_MEM_NODE_SIZE);
  LS_INFO("memory                 = %p\n", (void*)(heap->memory));
  LS_INFO("memory->next     = %p\n", (void*)(heap->memory->next));
  LS_INFO("memory->units    = %d (%d/0x%08x bytes)\n",
          heap->memory->units,
          heap->memory->units * LS_MEM_NODE_SIZE,
          heap->memory->units * LS_MEM_NODE_SIZE);
  LS_INFO("heap_units         = %d (%d/0x%08x bytes)\n",
          heap->heap_units,
          heap->heap_units * LS_MEM_NODE_SIZE,
          heap->heap_units * LS_MEM_NODE_SIZE);
  LS_INFO("allocated            = %d (%d/0x%08x bytes)\n",
          heap->allocated,
          heap->allocated * LS_MEM_NODE_SIZE,
          heap->allocated * LS_MEM_NODE_SIZE);
  LS_INFO("Current heap range [%p-%p],size=%08d (%08d bytes)\n",
          heap->memory,
          heap->memory + heap->heap_units,
          heap->heap_units,
          heap->heap_units * LS_MEM_NODE_SIZE);
  current = heap->anchor.next;
  LS_INFO("Heap->anchor.next = %p\n", heap->anchor.next);
  LS_INFO("&(heap->anchor) = %p\n", &(heap->anchor));

  if (current == &(heap->anchor))
  {
    LS_INFO("[%p-%p] size=%08d (%08d/0x08%x bytes)," "status = ALLOCATED\n",
            heap->memory,
            heap->memory + heap->heap_units,
            heap->heap_units,
            heap->heap_units * LS_MEM_NODE_SIZE,
            heap->heap_units * LS_MEM_NODE_SIZE);
    return;
  }

  if (current > heap->memory)
  {
    LS_INFO("[%p-%p] size=%08d (%08d/0x%08x bytes)," "status= ALLOCATED\n",
            heap->memory,
            current,
            current - heap->memory,
            (current - heap->memory) * LS_MEM_NODE_SIZE,
            (current - heap->memory) * LS_MEM_NODE_SIZE);
  }

  while (current != &(heap->anchor))
  {
    if (current->next != &(heap->anchor))
    {
      LS_INFO("[%p-%p] size=%08d (%08d/0x%08x bytes)," "status = FREE \n",
              current,
              current + current->units,
              current->units,
              current->units * LS_MEM_NODE_SIZE,
              current->units * LS_MEM_NODE_SIZE);
      LS_INFO("[%p-%p] size=%08d (%08d/0x%08x bytes)," "status = ALLOCATED \n",
              current + current->units,
              current->next,
              current->next - (current + current->units),
              (current->next - (current + current->units)) * LS_MEM_NODE_SIZE,
              (current->next - (current + current->units)) * LS_MEM_NODE_SIZE);
    }
    else
    {
      if ((current + current->units) < (heap->memory + heap->heap_units))
      {
        LS_INFO("[%p-%p] size=%08d (%d/0x%08x bytes)," "status = FREE\n",
                current,
                current + current->units,
                current->units,
                current->units * LS_MEM_NODE_SIZE,
                current->units * LS_MEM_NODE_SIZE);
        LS_INFO("[%p-%p] size=%08d (%08d/0x%08x bytes)," "status= ALLOCATED\n",
                current + current->units,
                heap->memory + heap->heap_units,
                heap->memory + heap->heap_units - (current + current->units),
                (heap->memory + heap->heap_units - (current + current->units)) * LS_MEM_NODE_SIZE,
                (heap->memory + heap->heap_units - (current + current->units)) * LS_MEM_NODE_SIZE);
      }
      else if (current + current->units == heap->memory + heap->heap_units)
      {
        LS_INFO("[%p-%p] size=%08d (%08d/0x%08x bytes)," "status = FREE\n",
                current,
                current + current->units,
                current->units,
                current->units * LS_MEM_NODE_SIZE,
                current->units * LS_MEM_NODE_SIZE);
      }
    }

    current = current->next;
  }
#endif                                                                                    /*LS_MEM_DUMP_HEAP*/
}


/*-----------------------------------------------------------------------
 *                                                                     function implementations.
 *---------------------------------------------------------------------*/
/*
   C                 A                    F        G    D             E                B
 |-------| |----------|XXXX|--|XXXXXXX|--------|XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX|
 | anchor| | next         | S4 |S5|    S1     |    S2        |     S3                                                    |
 |-------| |----------|XXXX|--|XXXXXXX|--------|XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX|

   anchor is the base of list of free memory regions,whose size is 0. its next is
   the whole heap which will be divided into different regions based on the request
   of LS_Malloc.It starts from A. When S3 is allocated, B point will be returned
   to the caller as the new memory address.

   Before allocation:

   &anchor == C;
   anchor->next ==A;
   anchor->next->units== whole heap size;
   anchor->next->next=&anchor

   After allocation:

   &anchor == C;
   anchor->next ==A;
   anchor->next->units== whole heap size - S3;
   anchor->next->next=&anchor

   the final list will be
   anchor->next = A;
   anchor->next->next = G;
   anchor->next->next->next = E;
   anchor->next->next->next-> next =&anchor ;
 */
static void*
__heap_malloc(LS_MemHeap* heap, uint32_t bytes)
{
  uint32_t request;
  int32_t status = LS_OK;
  void* ret_val = NULL;
  uint32_t err;
  LS_MemNode* previous = NULL;
  LS_MemNode* current = NULL;
  LS_MemNode* start = NULL;
  LS_MemNode* selected_c = NULL;
  LS_MemNode* selected_p = NULL;
  uint32_t max_block = 0;

  LS_ENTER("heap=%p,bytes=%d\n", heap, bytes);

  if (heap == NULL
#ifdef LS_MEM_DEBUG_HEAP
      || heap->magic_id != LS_MEM_MAGIC_NUMBER
#endif
      )
  {
    LS_ERROR("Invalid heap!\n");
    return NULL;
  }

  LS_TRACE("heap range [%p-%p],units=%08d (%08d/0x%08x bytes)," "allocated= %d (%08d/0x%08x bytes)\n",
           heap->memory,
           heap->memory + heap->heap_units,
           heap->heap_units,
           heap->heap_units * LS_MEM_NODE_SIZE,
           (uint32_t)(heap->heap_units * LS_MEM_NODE_SIZE),
           heap->allocated,
           heap->allocated * LS_MEM_NODE_SIZE,
           (uint32_t)(heap->allocated * LS_MEM_NODE_SIZE));

  do
  {
#ifdef LS_MEM_DEBUG_HEAP
    status = stm_check_heap(heap);

    if (status == 0)
    {
      LS_ERROR("stm_check_heap() failed.\n");
      break;
    }
#endif

#if 0
    stm_dump_heap(heap);
#endif
    /*------------------------------------------------------------------------
     * we need extra one unit of LS_MemNode to save the memory information,
     * also we need align the address of LS_MemNode
     *----------------------------------------------------------------------*/
    /* Check for integer overflow before calculating request size */
    if (bytes > UINT32_MAX - LS_MEM_NODE_SIZE + 1)
    {
      LS_ERROR("Error: allocation size %u would cause integer overflow\n", bytes);
      break;
    }

    request = 1 + (bytes + LS_MEM_NODE_SIZE - 1) / LS_MEM_NODE_SIZE;
    LS_TRACE("Requested bytes %d was rounded to %d (%d units)\n", bytes, request * LS_MEM_NODE_SIZE, request);

    if (request > heap->heap_units)
    {
      LS_ERROR("Error: request is over heap_size!\n");
      break;
    }

    LS_TRACE("Searching for the best-fit block for %d units\n", request);
    current = heap->anchor.next;

    if (current == &(heap->anchor))
    {
      LS_ERROR("Error: no room inside heap!\n");
      break;
    }

    /*
     * traverse the list to found one free blog with smallest waste of memory
     */
    previous = NULL;
    current = heap->anchor.next;
    err = heap->heap_units - request;

    while (current != &(heap->anchor))
    {
      LS_TRACE("Previous=%p, current=%p,current->next=%p\n", previous, current, current->next);

      if (current->units > max_block)
      {
        max_block = current->units;
      }

      if (current->units >= request)
      {
        if (current->units - request == 0)
        {
          selected_c = current;
          selected_p = previous;
          LS_TRACE("Find one block best fit the request...\n");
          LS_TRACE("selected_p=%p, selected_c=%p," "selected_c->next=%p\n", selected_p, selected_c, selected_c->next);
          err = 0;
          LS_TRACE("Break the search now....\n");
          break;
        }

        if (current->units - request <= err)
        {
          selected_c = current;
          selected_p = previous;
          err = current->units - request;
          LS_TRACE("selected_p=%p, selected_c=%p," "selected_c->next=%p,err=%d\n",
                   selected_p,
                   selected_c,
                   selected_c->next,
                   err);
        }
      }

      previous = current;
      current = current->next;
    }

    if (selected_c == NULL)
    {
      LS_ERROR("No free block big enough(%d) for this request(%d)\n", max_block, request);
      break;
    }

    LS_TRACE("Found it,selected_c=%p,selected_c->units=%d (%d bytes),"
             "selected_c->next=%p,request=%d (%d bytes),error=%d (%d bytes)\n",
             selected_c,
             selected_c->units,
             selected_c->units * LS_MEM_NODE_SIZE,
             selected_c->next,
             request,
             request * LS_MEM_NODE_SIZE,
             err,
             err * LS_MEM_NODE_SIZE);

    if (selected_c->units == request)
    {
      /*
       * In this case, all the heap will be allocated to one client.
       *
       */
      LS_TRACE("The select block exactly fits the request.\n");

      if (selected_p == NULL)
      {
        LS_TRACE("selected_p=NULL\n");
        start = selected_c;
        start->units = selected_c->units;
        heap->anchor.next = selected_c->next;
      }
      else
      {
        LS_TRACE("selected_p=%p,selected_p->next=%p\n", selected_p, selected_p->next);
        start = selected_c;
        start->units = selected_c->units;
        selected_p->next = selected_c->next;
        LS_TRACE("reset selected_p=%p,selected_p->next=%p\n", selected_p, selected_p->next);
      }
    }
    else
    {
      /*now points to the newly allocated memory*/
      LS_TRACE("resize selected_c block [%p-%p]\n", selected_c, selected_c + selected_c->units);
      selected_c->units -= request;
      start = selected_c + selected_c->units;
      start->units = request;
      LS_TRACE("start= [%p-%p],size=%d (%d/0x%x bytes)\n",
               start,
               start + start->units,
               start->units,
               start->units * LS_MEM_NODE_SIZE,
               start->units * LS_MEM_NODE_SIZE);
      LS_TRACE("selected_c = [%p-%p],size=%d (%d/0x%x bytes)\n",
               selected_c,
               selected_c + selected_c->units,
               selected_c->units,
               selected_c->units * LS_MEM_NODE_SIZE,
               selected_c->units * LS_MEM_NODE_SIZE);
    }

    heap->allocated += request;

    if (heap->allocated > heap->peak)
    {
      heap->peak = heap->allocated;
      LS_TRACE("Heap->peak = %d (%d bytes)\n", heap->peak, heap->peak * LS_MEM_NODE_SIZE);
    }

#ifdef LS_MEM_DEBUG_HEAP
    start->magic_id = LS_MEM_MAGIC_NUMBER;
#endif
    LS_TRACE(": memory actually starts from %p\n", start);
    LS_TRACE(": Userspace actually starts from %p\n", start + 1);
    ret_val = (void*)(start + 1);
    break;
  } while (0);

  LS_LEAVE("ret_val=%p\n", ret_val);

  if (ret_val == NULL)
  {
    stm_dump_heap(heap);
  }

  return ret_val;
}


/****************************************************************/
static int32_t
__heap_free(LS_MemHeap* heap, void* ptr)
{
  int32_t status = LS_OK;
  int32_t ret_val = 1;
  LS_MemNode* tofree, * current;

  LS_ENTER("heap=%p,ptr=%p\n", heap, ptr);

  if ((heap == NULL) ||
      ptr == NULL
#ifdef LS_MEM_DEBUG_HEAP
      || heap->magic_id != LS_MEM_MAGIC_NUMBER
#endif
      )
  {
    LS_ERROR("Invalid heap!\n");
    return 0;
  }

  LS_TRACE("heap range [%p-%p],units=%08d (%08d/0x%08x bytes)," "allocated= %d (%08d/0x%08x bytes)\n",
           heap->memory,
           heap->memory + heap->heap_units,
           heap->heap_units,
           heap->heap_units * LS_MEM_NODE_SIZE,
           (uint32_t)(heap->heap_units * LS_MEM_NODE_SIZE),
           heap->allocated,
           heap->allocated * LS_MEM_NODE_SIZE,
           (uint32_t)(heap->allocated * LS_MEM_NODE_SIZE));

  do
  {
#ifdef LS_MEM_DEBUG_HEAP
    status = stm_check_heap(heap);

    if (status == 0)
    {
      LS_ERROR("stm_check_heap() failed.\n");
      ret_val = 0;
      break;
    }
#endif

#if 0
    stm_dump_heap(heap);
#endif

    tofree = (LS_MemNode*)ptr - 1;
    LS_TRACE("Region to be free'ed is from [%p-%p],size=%d\n", tofree, tofree + tofree->units, tofree->units);

    if ((tofree < heap->memory) ||
        tofree > heap->memory + heap->heap_units
#ifdef LS_MEM_DEBUG_HEAP
        || tofree->magic_id != LS_MEM_MAGIC_NUMBER
#endif
        || (uint32_t)tofree->units > heap->allocated)
    {
      LS_ERROR("ptr = %p, tofree= %p is not a valid allocated block." "tofree->units=%d,tofree->magic_id=0x%08x,"
               "heap->memory=%p,heap->memory + heap->heap_units=%p," "heap->allocated=%d\n",
               (void*)ptr,
               (void*)tofree,
               tofree->units,
               tofree->magic_id,
               heap->memory,
               heap->memory + heap->heap_units,
               heap->allocated);
      ret_val = 0;
      break;
    }

    current = heap->anchor.next;

    /* all the heap are allocated, so just insert this new free'ed one
     * into free list
     */
    if (current == &(heap->anchor))
    {
      LS_TRACE("all heap were allocated\n");
#ifdef LS_MEM_DEBUG_HEAP
      tofree->magic_id = 0xDEADBEEF;
#endif
      heap->allocated -= tofree->units;
      heap->anchor.next = tofree;
      heap->anchor.next->units = tofree->units;
      heap->anchor.next->next = &(heap->anchor);
      ret_val = 1;
      break;
    }

    /*tofree is located between the start of heap and first of free block*/
    if ((tofree >= heap->memory) &&
        ((tofree + tofree->units) <= heap->anchor.next))
    {
      heap->allocated -= tofree->units;
      LS_TRACE("%p falls into [%p-%p]\n", tofree, heap->memory, heap->anchor.next);
#ifdef LS_MEM_DEBUG_HEAP
      tofree->magic_id = 0xDEADBEEF;
#endif
      tofree->next = heap->anchor.next;
      heap->anchor.next = tofree;
      heap->anchor.next->units = tofree->units;
      break;
    }

    while (current != &(heap->anchor))
    {
      if (current + current->units > tofree)
      {
        LS_ERROR("Error: free block [%p-%p] invades " "tofree block [%p-%p]\n",
                 current,
                 current + current->units,
                 tofree,
                 tofree + tofree->units);
        current = &(heap->anchor);
        break;
      }

      if (current->next != &(heap->anchor))
      {
        if ((tofree >= current + current->units) &&
            ((tofree + tofree->units) <= current->next))
        {
          heap->allocated -= tofree->units;
          LS_TRACE("%p falls into [%p-%p]\n", tofree, current + current->units, current->next);

#ifdef LS_MEM_DEBUG_HEAP
          tofree->magic_id = 0xDEADBEEF;
#endif
          break;
        }
      }
      else
      {
        if ((tofree >= current + current->units) &&
            (tofree <= heap->memory + heap->heap_units))
        {
          heap->allocated -= tofree->units;
          LS_TRACE("%p falls into [%p-%p]\n", tofree, current + current->units, heap->memory + heap->heap_units);
#ifdef LS_MEM_DEBUG_HEAP
          tofree->magic_id = 0xDEADBEEF;
#endif

          break;
        }
      }

      current = current->next;
    }

    if (current == &(heap->anchor))
    {
      LS_ERROR("Error: would not free %p,out of heap!", ptr);
      ret_val = 0;
      break;
    }

    LS_TRACE("trying to merge tofree into free list...\n");
    LS_TRACE("current=%p,current->units=%d\n", current, current->units);
    LS_TRACE("current->next =%p,current->next->units=%d\n", current->next, current->next->units);

    /*check tofree right bounday to see if we need merge
     * into right free block,which is current->next
     */
    if (tofree + tofree->units == current->next)
    {
      /* merge right when tofree block is adjacent to next free block*/
      LS_TRACE("try to merge current block [%p-%p] " "into right free block[%p-%p]\n",
               tofree,
               tofree + tofree->units,
               current->next,
               current->next + current->next->units);
      tofree->units += (current->next)->units;
      tofree->next = (current->next)->next;
      LS_TRACE("tofree->next=%p,tofree->units=%d\n", tofree->next, tofree->units);
    }
    else if ((uintptr_t)(tofree + tofree->units) == (uintptr_t)(heap + heap->heap_units))
    {
      LS_TRACE("tofree block is in the bottom of heap.");
      tofree->next = &(heap->anchor);
      LS_TRACE("tofree->next=%p,tofree->units=%d\n", tofree->next, tofree->units);
    }
    else
    {
      /*tofree block is adjacent to two allocated blocks,insert into the list*/
      LS_TRACE("tofree is adjacent to two allocated blocks...\n");
      tofree->next = current->next;
      LS_TRACE("tofree->next=%p,tofree->units=%d\n", tofree->next, tofree->units);
    }

    /*check tofree left bounday to see if we need merge into left free block*/
    if (current + current->units == tofree)
    {
      LS_TRACE("Merge tofree block with current block...\n");
      /* merge left */
      current->units += tofree->units;
      current->next = tofree->next;
      LS_TRACE("current->next=%p,current->units=%d\n", current->next, current->units);
    }
    else
    {
      /* insert left */
      LS_TRACE("Insert tofree block into free block list...\n");
      current->next = tofree;
      LS_TRACE("current->next=%p,current->next->units=%d\n", current->next, current->next->units);
      LS_TRACE("current->next->next=%p," "current->next->next->units=%d\n",
               current->next->next,
               current->next->next->units);
    }

#ifdef LS_MEM_DEBUG_HEAP
    LS_TRACE("After free......\n");
    status = stm_check_heap(heap);

    if (status == 0)
    {
      LS_ERROR("stm_check_heap() failed.\n");
      ret_val = 0;
      break;
    }
#endif

#if 0
    stm_dump_heap(heap);
#endif

    break;
  }while (0);

  LS_LEAVE(",ret=%d\n", ret_val);
  return ret_val;
}


/****************************************************************/
static void*
__heap_realloc(LS_MemHeap* heap, void* ptr, uint32_t bytes)
{
  void* np;
  LS_MemNode* current, * new_allocated = 0;
  uint32_t new_units, old_units;
  int32_t status = LS_OK;
  void* ret_val = NULL;

  LS_ENTER(",heap=%p,ptr=%p,bytes=%d\n", heap, ptr, bytes);

  /* Handle NULL ptr - should have been caught by LS_Realloc wrapper */
  if ((heap == NULL)
#ifdef LS_MEM_DEBUG_HEAP
      || heap->magic_id != LS_MEM_MAGIC_NUMBER
#endif
      )
  {
    LS_ERROR("Invalid heap!\n");
    return 0;
  }

  if (ptr == NULL)
  {
    LS_ERROR("ptr is NULL - should call __heap_malloc instead!\n");
    return 0;
  }

  LS_TRACE("heap range [%p-%p],units=%08d (%08d/0x%08x bytes)," "allocated= %d (%08d/0x%08x bytes)\n",
           heap->memory,
           heap->memory + heap->heap_units,
           heap->heap_units,
           heap->heap_units * LS_MEM_NODE_SIZE,
           (uint32_t)heap->heap_units * LS_MEM_NODE_SIZE,
           heap->allocated,
           heap->allocated * LS_MEM_NODE_SIZE,
           (uint32_t)heap->allocated * LS_MEM_NODE_SIZE);

  do
  {
#ifdef LS_MEM_DEBUG_HEAP
    status = stm_check_heap(heap);

    if (status == 0)
    {
      LS_ERROR("stm_check_heap() failed.\n");
      break;
    }
#endif

#if 0
    stm_dump_heap(heap);
#endif

    current = (LS_MemNode*)ptr - 1;

    if ((current < (heap->memory)) ||
        current > ((heap->memory) + heap->heap_units)
#ifdef LS_MEM_DEBUG_HEAP
        || current->magic_id != LS_MEM_MAGIC_NUMBER
#endif
        || current->units > heap->allocated)
    {
      LS_ERROR("current=%p is not a valid allocated block." "current->units=%d," "heap->memory=%p,"
               "heap->memory + heap->heap_units=%p," "heap->allocated=%d\n",
               current,
               current->units,
               heap->memory,
               heap->memory + heap->heap_units,
               heap->allocated);
      break;
    }

    old_units = current->units;
    np = __heap_malloc(heap, bytes);

    if (np == NULL)
    {
      LS_ERROR("Not enough room in heap for reqeust %d bytes\n", bytes);
      break;
    }

    /* np is guaranteed non-NULL here - no redundant check needed */
    new_allocated = (LS_MemNode*)np - 1;
    new_units = new_allocated->units;
    bytes = LS_MEM_NODE_SIZE * MIN(old_units - 1, new_units - 1);
    LS_MemCopy(np, ptr, bytes);
    __heap_free(heap, ptr);

    ret_val = (void*)(new_allocated + 1);
    break;
  }while (0);

  LS_LEAVE(",ret_val=%p\n", ret_val);
  return ret_val;
}


/*-----------------------------------------------------------------------------
 * OS MALLOC/FREE with FREE_SIZE() supported
 *---------------------------------------------------------------------------*/
#elif defined (FREE_MEM_SIZE_SUPPORTED)

void*
LS_Malloc(LS_MemHeap* heap, uint32_t bytes)
{
  void* current = NULL;

  /* Check for integer overflow before capacity check */
  if (bytes > heap->heap_units - heap->allocated)
  {
    LS_ERROR("this request will over the heap size\n");
    return 0;
  }

  current = SYS_MALLOC(bytes);

  if (current)
  {
    heap->allocated += SYS_MEM_SIZE(current);
    LS_TRACE("current allocated bytes is %d\n", heap->allocated);
  }

  return current;
}


/****************************************************************/
int32_t
LS_Free(LS_MemHeap* heap, void* ptr)
{
  if (ptr == NULL)
  {
    LS_TRACE("freeing NULL pointer is a no-op\n");
    return 1;
  }

  heap->allocated -= SYS_MEM_SIZE(ptr);
  SYS_FREE(ptr);
  LS_TRACE("ptr %p is free'ed, allocated =%d\n", ptr, heap->allocated);
  return 1;
}


/****************************************************************/
void*
LS_Realloc(LS_MemHeap* heap, void* ptr, uint32_t bytes)
{
  uint32_t old;
  void* new_ptr;

  /* Handle realloc(NULL, bytes) like malloc(bytes) */
  if (ptr == NULL)
  {
    return LS_Malloc(heap, bytes);
  }

  old = SYS_MEM_SIZE(ptr);

  /* Check for integer overflow before capacity check */
  if (bytes > heap->heap_units - (heap->allocated - old))
  {
    LS_ERROR("this request will over the heap size\n");
    return 0;
  }

  new_ptr = SYS_REALLOC(ptr, bytes);

  if (new_ptr)
  {
    heap->allocated -= old;
    heap->allocated += SYS_MEM_SIZE(new_ptr);
  }
  /* If realloc fails, ptr is still valid and heap->allocated is unchanged */

  return new_ptr;
}


/*
 * based on OS functions.
 */
#else
/* This struct is used to keep the 'highwater mark' variable <allocated>
 * when the OS doesn't have a call to tell us the size of a pointer.
 * So we need to allocate some extra space to hold the size field.
 * But can't just be a uint32_t, since this screws up alignments on
 * machines like the ALPHA which want pointers to be on 8 byte
 * boundaries.    So we use a union -- let the compiler do the math!
 */
typedef struct
{
#ifdef LS_MEM_DEBUG_HEAP
  uint32_t magic_id;
#endif
  union
  {
    uint32_t* ptr;                                                                        /* just for alignment purposes */
    uint32_t  units;
  } u;                                                                                     /* named union member for C99 compliance */
} LS_MemHeader;
/****************************************************************/
static void*
__heap_malloc(LS_MemHeap* heap, uint32_t bytes)
{
  LS_MemHeader* record = NULL;
  uint32_t request;
  uint32_t cond = 1;

  LS_ENTER("heap=%p,bytes=%d\n", heap, bytes);

  while (cond)
  {
    request = bytes + sizeof(LS_MemHeader);

    /* Check for integer overflow before capacity check */
    if (request > heap->heap_units - heap->allocated)
    {
      LS_ERROR("this request will over the heap size\n");
      cond = 0;
      record = NULL;
      break;
    }

    record = (LS_MemHeader*)SYS_MALLOC(request);
    LS_TRACE("Raw memory starts from %p\n", record);

    if (record == NULL)
    {
      LS_ERROR("Memory error: request for %d bytes failed\n", request);
      cond = 0;
      record = NULL;
      break;
    }

    record->u.units = request;
#ifdef LS_MEM_DEBUG_HEAP
    record->magic_id = LS_MEM_MAGIC_NUMBER;
#endif
    heap->allocated += request;
    record++;
    LS_TRACE("request %d bytes fulfilled with return %p\n", bytes, record);
    LS_TRACE("heap->allocated = %d,heap->heap_units=%d\n", heap->allocated, heap->heap_units);
    break;
  }

  LS_LEAVE("ret=%p\n", record);
  return (void*)record;
}


/****************************************************************/
static int32_t
__heap_free(LS_MemHeap* heap, void* ptr)
{
  LS_MemHeader* record = NULL;
  uint32_t cond = 1;
  int32_t ret_val = 1;

  LS_ENTER("heap=%p,ptr=%p\n", heap, ptr);

  while (cond)
  {
    if (ptr == NULL)
    {
      cond = 0;
      ret_val = 1;
      break;
    }

    record = (LS_MemHeader*)ptr;
    record--;

#ifdef LS_MEM_DEBUG_HEAP
    if (record->magic_id != LS_MEM_MAGIC_NUMBER)
    {
      LS_ERROR("%p is not valid pointer\n", ptr);
      cond = 0;
      ret_val = 0;
      break;
    }
#endif

    heap->allocated -= record->u.units;

#ifdef LS_MEM_DEBUG_HEAP
    record->magic_id = 0xDEADBEEF;
#endif
    SYS_FREE((void*)record);
    LS_TRACE("%p was free'ed.\n", ptr);
    LS_TRACE("heap->allocated = %d,heap->heap_units=%d\n", heap->allocated, heap->heap_units);
    cond = 0;
    ret_val = 1;
    break;
  }

  LS_LEAVE("ret=%d\n", ret_val);
  return ret_val;
}


/****************************************************************/
static void*
__heap_realloc(LS_MemHeap* heap, void* ptr, uint32_t bytes)
{
  uint32_t request;
  LS_MemHeader* record = NULL;
  uint32_t old_size;
  void* ret_val = NULL;
  uint32_t cond = 1;

  LS_ENTER(",heap=%p,ptr=%p,bytes=%d\n", heap, ptr, bytes);

  while (cond)
  {
    if ((ptr == NULL) ||
        (bytes <= 0))
    {
      cond = 0;
      ret_val = NULL;
      break;
    }

    record = (LS_MemHeader*)ptr;
    record--;                                                                             /* at old header */
#ifdef LS_MEM_DEBUG_HEAP
    if (record->magic_id != LS_MEM_MAGIC_NUMBER)
    {
      LS_ERROR("%p (%p) is not valid pointer\n", ptr, record);
      cond = 0;
      ret_val = NULL;
      break;
    }
#endif

    old_size = record->u.units;
    request = bytes + sizeof(LS_MemHeader);

    /* Check for integer overflow/underflow: (heap->allocated - old_size + request) > heap->heap_units */
    /* This avoids: (1) overflow in (heap->allocated + request), (2) underflow in (request - old_size) */
    if (request > heap->heap_units - (heap->allocated - old_size))
    {
      LS_ERROR("this request will over the heap size\n");
      cond = 0;
      ret_val = NULL;
      break;
    }

#ifdef LS_MEM_DEBUG_HEAP
    record->magic_id = 0xDEADBEEF;
#endif
    record = (LS_MemHeader*)SYS_REALLOC((void*)record, request);

    if (record)
    {
#ifdef LS_MEM_DEBUG_HEAP
      record->magic_id = LS_MEM_MAGIC_NUMBER;
#endif
      record->u.units = request;
      heap->allocated -= old_size;
      heap->allocated += request;
      record++;
    }
    else
    {
      LS_ERROR("SYS_REALLOC fail\n");
      cond = 0;
      ret_val = NULL;
      break;
    }

    LS_TRACE("%p request %d bytes fulfilled " "with return %p,raw=%p\n", ptr, bytes, record, record - 1);
    LS_TRACE("heap->allocated = %d,heap->heap_units=%d\n", heap->allocated, heap->heap_units);
    cond = 0;
    ret_val = (void*)record;
    break;
  }

  LS_LEAVE("ret=%p\n", ret_val);
  return ret_val;
}


#endif                                                                                        /* LS_INTERNAL_HEAP */
#endif /* LS_MEM_USE_SYSTEM_MALLOC */
