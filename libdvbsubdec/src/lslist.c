/*-----------------------------------------------------------------------------
 * lslist.c
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

#include "lslist.h"
#include "lsmemory.h"
#include "lssubdec.h"
#include "lssystem.h"
#include <stdint.h>
#include <inttypes.h>

const uint32_t LS_LIST_WATCH_DOG = 102400;

LS_List*
LS_ListInit(Allocator alloc_func, Deallocator free_func, int32_t* errcode)
{
  LS_List* list = NULL;
  int32_t status = LS_OK;

  LS_ENTER("alloc_func=%p, free_func=%p\n", alloc_func, free_func);

  do
  {
    if ((alloc_func == NULL) ||
        (free_func == NULL))
    {
      LS_ERROR("LS_ERROR_GENERAL:Invalid Parameters\n");
      *errcode = LS_ERROR_GENERAL;
      break;
    }

    list = (LS_List*)alloc_func(sizeof(LS_List));

    if (list == NULL)
    {
      LS_ERROR("LS_ERROR_SYSTEM_BUFFER: request for %d bytes failed\n", sizeof(LS_List));
      *errcode = LS_ERROR_SYSTEM_BUFFER;
      break;
    }

    SYS_MEMSET(list, 0, sizeof(LS_List));
    status = LS_MutexCreate(&(list->mutex));

    if (status == 0)
    {
      LS_ERROR("LS_ERROR_SYSTEM_ERROR: create mutex failed\n");
      free_func(list);
      *errcode = LS_ERROR_SYSTEM_ERROR;
      list = NULL;
      break;
    }

#ifdef LS_LIST_DEBUG
    list->magic_id = LS_LIST_MAGIC;
#endif

    list->count = 0;
    list->allocator = alloc_func;
    list->deallocator = free_func;
#ifdef LS_LIST_NODE_DEBUG
    list->anchor.magic_id = LS_LIST_NODE_MAGIC;
#endif
    list->anchor.data = NULL;
    list->anchor.next = NULL;
    break;
  }while (0);

  /*all seem OK, return ... ... */
  LS_DEBUG("list %p created\n", (void*)list);
  LS_LEAVE(",ret_val=%p\n", list);
  *errcode = LS_OK;
  return list;
}


int32_t
LS_ListAppend(LS_List* list, void* data)
{
  int32_t ret_val = LS_OK;
  int32_t status = LS_OK;
  uint32_t watchdog = 0;
  LS_ListNode* current = NULL;
  LS_ListNode* new_node = NULL;

  LS_ENTER("list=%p,data=%p\n", list, data);

  if (list == NULL)
  {
    LS_ERROR("LS_ERROR_GENERAL: Invalid parameters\n");
    ret_val = LS_ERROR_GENERAL;
    LS_LEAVE("ret_val=%d\n", ret_val);
    return ret_val;
  }

#ifdef LS_LIST_DEBUG
  if (list->magic_id != LS_LIST_MAGIC)
  {
    LS_ERROR("LS_ERROR_GENERAL: list %p currupted\n", list);
    ret_val = LS_ERROR_GENERAL;
    LS_LEAVE("ret_val=%d\n", ret_val);
    return ret_val;
  }
#endif

  status = LS_MutexWait(list->mutex);

  if (status == 0)
  {
    LS_ERROR("LS_ERROR_SYSTEM_ERROR: waiting for mutex fail\n");
    ret_val = LS_ERROR_SYSTEM_ERROR;
    LS_LEAVE("ret_val=%d\n", ret_val);
    return ret_val;
  }

  do
  {
    new_node = (LS_ListNode*)(list->allocator(sizeof(LS_ListNode)));

    if (new_node == NULL)
    {
      LS_ERROR("LS_ERROR_SYSTEM_BUFFER: request for %d bytes failed\n", sizeof(LS_ListNode));
      ret_val = LS_ERROR_SYSTEM_BUFFER;
      break;
    }

    new_node->data = data;
    new_node->next = NULL;
#ifdef LS_LIST_NODE_DEBUG
    new_node->magic_id = LS_LIST_NODE_MAGIC;
#endif

    current = list->anchor.next;

    if (current == NULL)
    {
      LS_TRACE("Oh,we are the first one\n");
      list->anchor.next = new_node;
      list->count += 1;
      LS_TRACE("new node (%p,node->data=%p) added to the list (%p)," "its previous node is %p\n",
               new_node,
               new_node->data,
               list,
               &(list->anchor));
      ret_val = LS_OK;
      break;
    }

    while (current
#ifdef LS_LIST_DEBUG
           && (watchdog < LS_LIST_WATCH_DOG)
#endif
           )
    {
#ifdef LS_LIST_NODE_DEBUG
      if (current->magic_id != LS_LIST_NODE_MAGIC)
      {
        LS_ERROR("LS_ERROR_GENERAL: corrupted list,invalid magic_id number\n");
        ret_val = LS_ERROR_GENERAL;
        break;
      }
#endif

      if (current->next == NULL)                                          /* reach the tail of list?*/
      {
        LS_TRACE("adding new node to list %p,current=%p\n", list, current);
        current->next = new_node;
        list->count += 1;
        LS_TRACE("new node (%p,node->data = %p) added to the list (%p)," "its previous node is %p\n",
                 new_node,
                 new_node->data,
                 list,
                 current);
        ret_val = LS_OK;
        break;
      }
      else
      {
        current = current->next;
      }

#ifdef LS_LIST_DEBUG
      watchdog++;
#endif
    }

#ifdef LS_LIST_DEBUG
    if (watchdog == LS_LIST_WATCH_DOG)
    {
      LS_ERROR("LS_ERROR_GENERAL: Watchdog overflow!\n");
      ret_val = LS_ERROR_GENERAL;
      break;
    }
#endif
  }while (0);

  status = LS_MutexSignal((list->mutex));

  if (status == 0)
  {
    LS_ERROR("LS_ERROR_SYSTEM_ERROR: mutex signal fail\n");
    ret_val = LS_ERROR_SYSTEM_ERROR;
  }

  LS_LEAVE("ret_val=%d\n", ret_val);
  return ret_val;
}


int32_t
LS_ListPreAppend(LS_List* list, void* data)
{
  int32_t ret_val = 1;
  int32_t status = LS_OK;
  LS_ListNode* current = NULL;
  LS_ListNode* new_node = NULL;

  LS_ENTER("list=%p,data=%p\n", list, data);

  if (list == NULL)
  {
    LS_ERROR("LS_ERROR_GENERAL: Invalid parameters\n");
    ret_val = LS_ERROR_GENERAL;
    LS_LEAVE("ret_val=%d\n", ret_val);
    return ret_val;
  }

#ifdef LS_LIST_DEBUG
  if (list->magic_id != LS_LIST_MAGIC)
  {
    LS_ERROR("LS_ERROR_GENERAL: list %p currupted\n", list);
    ret_val = LS_ERROR_GENERAL;
    LS_LEAVE("ret_val=%d\n", ret_val);
    return ret_val;
  }
#endif

  new_node = (LS_ListNode*)(list->allocator(sizeof(LS_ListNode)));

  if (new_node == NULL)
  {
    LS_ERROR("LS_ERROR_SYSTEM_BUFFER: request for %d bytes failed\n", sizeof(LS_ListNode));
    ret_val = LS_ERROR_SYSTEM_BUFFER;
    LS_LEAVE("ret_val=%d\n", ret_val);
    return ret_val;
  }

#ifdef LS_LIST_NODE_DEBUG
  new_node->magic_id = LS_LIST_NODE_MAGIC;
#endif
  new_node->data = data;
  status = LS_MutexWait(list->mutex);

  if (status == 0)
  {
    LS_ERROR("LS_ERROR_SYSTEM_ERROR: wait for a mutex failed\n");
    ret_val = LS_ERROR_SYSTEM_ERROR;
    LS_LEAVE("ret_val=%d\n", ret_val);
    return ret_val;
  }

  current = list->anchor.next;
  list->anchor.next = new_node;
  new_node->next = current;
  list->count += 1;
  LS_TRACE("node (%p) becomes the first node in list %p\n", new_node, list);
  status = LS_MutexSignal((list->mutex));

  if (status == 0)
  {
    LS_ERROR("LS_ERROR_SYSTEM_ERROR: signal mutex failed\n");
    ret_val = LS_ERROR_SYSTEM_ERROR;
  }

  LS_LEAVE("ret_val=%d\n", ret_val);
  return ret_val;
}


int32_t
LS_ListFindNode(LS_List* list, void* data)
{
  int32_t ret_val = LS_OK;
  int32_t status = LS_OK;
  uint32_t watchdog = 0;
  LS_ListNode* current = NULL;

  LS_ENTER("list=%p,data=%p\n", list, data);

  if (list == NULL)
  {
    LS_ERROR("LS_ERROR_GENERAL:Invalid parameters\n");
    ret_val = LS_ERROR_GENERAL;
    LS_LEAVE("ret_val=%d\n", ret_val);
    return ret_val;
  }

#ifdef LS_LIST_DEBUG
  if (list->magic_id != LS_LIST_MAGIC)
  {
    LS_ERROR("LS_ERROR_GENERAL: list %p currupted\n", list);
    ret_val = LS_ERROR_GENERAL;
    LS_LEAVE("ret_val=%d\n", ret_val);
    return ret_val;
  }
#endif

  status = LS_MutexWait(list->mutex);

  if (status == 0)
  {
    LS_ERROR("LS_ERROR_SYSTEM_ERROR: wait for mutex failed\n");
    ret_val = LS_ERROR_SYSTEM_ERROR;
    LS_LEAVE("ret_val=%d\n", ret_val);
    return ret_val;
  }

  do
  {
    current = (&(list->anchor))->next;

    while (current
#ifdef LS_LIST_DEBUG
           && watchdog < LS_LIST_WATCH_DOG
#endif
           )
    {
#ifdef LS_LIST_NODE_DEBUG
      if (current->magic_id != LS_LIST_NODE_MAGIC)
      {
        LS_ERROR("LS_ERROR_GENERAL: node corrupted: " "current=%p,current->magic_id= 0x%08x,"
                 "current->data=%p,current->next=%p\n",
                 current,
                 (uint32_t)current->magic_id,
                 current->data,
                 current->next);
        ret_val = LS_ERROR_GENERAL;
        break;
      }
#endif

      if ((uintptr_t)(current->data) == (uintptr_t)data)
      {
        LS_TRACE("data %p is found in node(%p) list %p\n", data, current, list);
        break;
      }
      else
      {
        current = current->next;
      }

      watchdog++;
    }
  }while (0);

#ifdef LS_LIST_DEBUG
  if (watchdog == LS_LIST_WATCH_DOG)
  {
    LS_ERROR("LS_ERROR_GENERAL: Watchdog overflow!\n");
    ret_val = LS_ERROR_GENERAL;
  }
#endif

  if (current == NULL)
  {
    LS_TRACE("would not find node %p in list %p\n", data, list);
  }

  /*good to go...*/
  ret_val = LS_OK;
  status = LS_MutexSignal((list->mutex));

  if (status == 0)
  {
    LS_ERROR("LS_ERROR_SYSTEM_ERROR: signal mutex failed\n");
    ret_val = LS_ERROR_SYSTEM_ERROR;
  }

  LS_LEAVE("ret_val=%d\n", ret_val);
  return ret_val;
}


void*
LS_ListFindUserNode(LS_List* list, void* data, CompareFunc func)
{
  void* ret_val = NULL;
  int32_t status = LS_OK;
  uint32_t watchdog = 0;
  LS_ListNode* current = NULL;

  LS_ENTER("list=%p,data=%p,func=%p\n", list, data, func);

  if ((list == NULL) ||
      (func == NULL))
  {
    LS_ERROR("LS_ERROR_GENERAL: Invalid parameters\n");
    ret_val = NULL;
    LS_LEAVE("ret_val=%p\n", ret_val);
    return ret_val;
  }

#ifdef LS_LIST_DEBUG
  if (list->magic_id != LS_LIST_MAGIC)
  {
    LS_ERROR("LS_ERROR_GENERAL: list %p currupted\n", list);
    ret_val = NULL;
    LS_LEAVE("ret_val=%p\n", ret_val);
    return ret_val;
  }
#endif

  status = LS_MutexWait(list->mutex);

  if (status == 0)
  {
    LS_ERROR("LS_ERROR_GENERAL: wait for mutex failed\n");
    ret_val = NULL;
    LS_LEAVE("ret_val=%p\n", ret_val);
    return ret_val;
  }

  do
  {
    current = (&(list->anchor))->next;

    while (current
#ifdef LS_LIST_DEBUG
           && watchdog < LS_LIST_WATCH_DOG
#endif
           )
    {
#ifdef LS_LIST_NODE_DEBUG
      if (current->magic_id != LS_LIST_NODE_MAGIC)
      {
        LS_ERROR("LS_ERROR_GENERAL: node corrupted: " "current=%p,current->magic_id= 0x%08x,"
                 "current->data=%p,current->next=%p\n",
                 current,
                 (uint32_t)current->magic_id,
                 current->data,
                 current->next);
        ret_val = NULL;
        break;
      }
#endif

      LS_TRACE("current->data = %p\n", current->data);

      if (!(func(current->data, data)))
      {
        LS_TRACE("data %p is found in node(%p) list %p\n", data, current, list);
        break;
      }
      else
      {
        current = current->next;
      }

      watchdog++;
    }
  }while (0);

#ifdef LS_LIST_DEBUG
  if (watchdog == LS_LIST_WATCH_DOG)
  {
    LS_ERROR("LS_ERROR_GENERAL: Watchdog overflow!\n");
    current = NULL;
    ret_val = NULL;
  }
#endif

  if (current == NULL)
  {
    LS_TRACE("would not find node %p in list %p\n", data, list);
    ret_val = NULL;
  }
  else
  {
    ret_val = current->data;
  }

  status = LS_MutexSignal((list->mutex));

  if (status == 0)
  {
    LS_ERROR("LS_ERROR_SYSTEM_ERROR: signal mutex failed\n");
  }

  /*good to go...*/
  LS_LEAVE("ret_val=%p\n", ret_val);
  return ret_val;
}


int32_t
LS_ListRemoveNode(LS_List* list, void* data)
{
  int32_t ret_val = LS_ERROR_GENERAL;
  int32_t status = LS_OK;
  uint32_t watchdog = 0;
  LS_ListNode* current = NULL;
  LS_ListNode* next = NULL;

  LS_ENTER("list=%p,data=%p\n", list, data);

  if (list == NULL)
  {
    LS_ERROR("LS_ERROR_GENERAL: Invalid parameters\n");
    ret_val = LS_ERROR_GENERAL;
    LS_LEAVE("ret_val=%d\n", ret_val);
    return ret_val;
  }

#ifdef LS_LIST_DEBUG
  if (list->magic_id != LS_LIST_MAGIC)
  {
    LS_ERROR("LS_ERROR_GENERAL: list %p currupted\n", list);
    ret_val = LS_ERROR_GENERAL;
    LS_LEAVE("ret_val=%d\n", ret_val);
    return ret_val;
  }
#endif

  status = LS_MutexWait(list->mutex);

  if (status == 0)
  {
    LS_ERROR("LS_ERROR_SYSTEM_ERROR: waiting for mutex failed\n");
    ret_val = LS_ERROR_SYSTEM_ERROR;
    LS_LEAVE("ret_val=%d\n", ret_val);
    return ret_val;
  }

  do
  {
    current = &(list->anchor);

    while (current
#ifdef LS_LIST_DEBUG
           && watchdog < LS_LIST_WATCH_DOG
#endif
           )
    {
      next = current->next;

#ifdef LS_LIST_NODE_DEBUG
      if ((current->magic_id != LS_LIST_NODE_MAGIC) ||
          ((next != NULL) &&
           (next->magic_id != LS_LIST_NODE_MAGIC)))
      {
        LS_ERROR("LS_ERROR_GENERAL: node corrupted: " "current=%p,current->magic_id= 0x%08x,"
                 "next=%p,next->magic_id=0x%08x\n",
                 current,
                 (uint32_t)current->magic_id,
                 next,
                 (uint32_t)next->magic_id);
        ret_val = LS_ERROR_GENERAL;
        break;
      }
#endif

      if ((next != NULL) &&
          (next->data == data))
      {
        LS_TRACE("data %p is found in node(%p) list %p\n", data, next, list);
        current->next = next->next;
        list->deallocator(next);
        LS_TRACE("node %p released\n", next);
        list->count -= 1;
        /*good to go....*/
        ret_val = LS_OK;
        break;
      }
      else
      {
        current = current->next;
      }

      watchdog++;
    }
  }while (0);

#ifdef LS_LIST_DEBUG
  if (watchdog == LS_LIST_WATCH_DOG)
  {
    LS_ERROR("LS_ERROR_GENERAL: Watchdog overflow!\n");
    ret_val = LS_ERROR_GENERAL;
  }
#endif

  if (next == NULL)
  {
    ret_val = LS_OK;
  }

  status = LS_MutexSignal((list->mutex));

  if (status == 0)
  {
    LS_ERROR("LS_ERROR_SYSTEM_ERROR: signal mutex failed\n");
    ret_val = LS_ERROR_SYSTEM_ERROR;
  }

  LS_LEAVE("ret_val=%d\n", ret_val);
  return ret_val;
}


void*
LS_ListRemoveUserNode(LS_List* list, void* user_data, CompareFunc func)
{
  void* ret_val = NULL;
  int32_t status = LS_OK;
  uint32_t watchdog = 0;
  LS_ListNode* current = NULL;
  LS_ListNode* previous = NULL;

  LS_ENTER("list=%p,data=%p\n", list, (void*)user_data);

  if ((list == NULL) ||
      (func == NULL))
  {
    LS_ERROR("LS_ERROR_GENERAL: Invalid parameters\n");
    ret_val = NULL;
    LS_LEAVE("ret_val=%p\n", ret_val);
    return NULL;
  }

#ifdef LS_LIST_DEBUG
  if (list->magic_id != LS_LIST_MAGIC)
  {
    LS_ERROR("LS_ERROR_GENERAL: list %p currupted\n", list);
    ret_val = NULL;
    LS_LEAVE("ret_val=%p\n", ret_val);
    return ret_val;
  }
#endif

  status = LS_MutexWait(list->mutex);

  if (status == 0)
  {
    LS_ERROR("LS_ERROR_SYSTEM_ERROR: wait for mutex failed\n");
    ret_val = NULL;
    LS_LEAVE("ret_val=%p\n", ret_val);
    return ret_val;
  }

  do
  {
    current = list->anchor.next;
    previous = NULL;

    while (current
#ifdef LS_LIST_DEBUG
           && watchdog < LS_LIST_WATCH_DOG
#endif
           )
    {
#ifdef LS_LIST_NODE_DEBUG
      if (current->magic_id != LS_LIST_NODE_MAGIC)
      {
        LS_ERROR("LS_ERROR_GENERAL: node corrupted: " "current=%p,current->magic_id= 0x%08x,"
                 "current->next=%p,current->data=%p\n",
                 current,
                 (uint32_t)current->magic_id,
                 current->next,
                 current->data);
        ret_val = NULL;
        break;
      }
#endif

      if ((func(current->data,
                user_data)) == (ptrdiff_t)(0))
      {
        LS_TRACE("current(%p)->data (%p) matched" " in list %p\n", current, current->data, list);
        ret_val = current->data;

        if (previous == NULL)
        {
          list->anchor.next = current->next;
        }
        else
        {
          previous->next = current->next;
        }

#ifdef LS_LIST_NODE_DEBUG
        current->magic_id = 0xDEADBEEF;
#endif
        current->data = NULL;
        current->next = NULL;
        list->deallocator(current);
        LS_TRACE("node %p released\n", current);
        list->count -= 1;
        LS_TRACE("%d nodes in the list\n", list->count);
        break;
      }
      else
      {
        previous = current;
        current = current->next;
      }

      watchdog++;
    }
  }while (0);

#ifdef LS_LIST_DEBUG
  if (watchdog == LS_LIST_WATCH_DOG)
  {
    LS_ERROR("Watchdog overflow!\n");
    ret_val = NULL;
  }
#endif

  status = LS_MutexSignal((list->mutex));

  if (status == 0)
  {
    LS_ERROR("LS_ERROR_SYSTEM_ERROR: signal mutex failed\n");
    ret_val = NULL;
  }

  LS_LEAVE("ret_val=%p\n", ret_val);
  return ret_val;
}


int32_t
LS_ListEmpty(LS_List* list)
{
  int32_t ret_val = LS_OK;
  int32_t status = LS_OK;
  uint32_t watchdog = 0;
  LS_ListNode* current = NULL;
  LS_ListNode* next = NULL;

  LS_ENTER("list=%p\n", list);

  if (list == NULL)
  {
    LS_DEBUG("Empty list,return...\n");
    ret_val = LS_OK;
    LS_LEAVE("ret_val=%d\n", ret_val);
    return ret_val;
  }

#ifdef LS_LIST_DEBUG
  if (list->magic_id != LS_LIST_MAGIC)
  {
    LS_ERROR("LS_ERROR_GENERAL: list %p currupted\n", list);
    ret_val = LS_ERROR_GENERAL;
    LS_LEAVE("ret_val=%d\n", ret_val);
    return ret_val;
  }
#endif

  status = LS_MutexWait(list->mutex);

  if (status == 0)
  {
    LS_ERROR("LS_ERROR_SYSTEM_ERROR: mutex wait failed\n");
    ret_val = LS_ERROR_SYSTEM_ERROR;
    LS_LEAVE("ret_val=%d\n", ret_val);
    return ret_val;
  }

  do
  {
    current = &(list->anchor);

    while (current->next != NULL &&
           list->count != 0
#ifdef LS_LIST_DEBUG
           && watchdog < LS_LIST_WATCH_DOG
#endif
           )
    {
      next = current->next->next;
#ifdef LS_LIST_NODE_DEBUG
      if (current->next->magic_id != LS_LIST_NODE_MAGIC)
      {
        LS_ERROR("LS_ERROR_GENERAL: node corrupted: " "node %p,node->magic_id= 0x%08x\n",
                 current->next,
                 (uint32_t)current->next->magic_id);
        ret_val = LS_ERROR_GENERAL;
        break;
      }
      current->next->magic_id = 0xDEADBEEF;
#endif
      current->next->next = NULL;
      current->next->data = NULL;
      list->deallocator(current->next);
      LS_TRACE("node %p released\n", current->next);
      list->count -= 1;
      current->next = next;
      watchdog++;
    }
  }while (0);

  if ((list->anchor.next != NULL) ||
      (list->count != 0))
  {
    LS_ERROR("LS_ERROR_GENERAL: Fail when delete each node\n");
    ret_val = LS_ERROR_GENERAL;
  }

#ifdef LS_LIST_DEBUG
  if (watchdog == LS_LIST_WATCH_DOG)
  {
    LS_ERROR("LS_ERROR_GENERAL: Watchdog overflow!\n");
    ret_val = LS_ERROR_GENERAL;
  }
#endif

  status = LS_MutexSignal((list->mutex));

  if (status == 0)
  {
    LS_ERROR("LS_ERROR_SYSTEM_ERROR: signal mutex failed\n");
    ret_val = LS_ERROR_SYSTEM_ERROR;
  }

  LS_LEAVE("ret_val=%d\n", ret_val);
  return ret_val;
}


int32_t
LS_ListEmptyData(LS_List* list, DestroyFunc destroy_func, void* userdata)
{
  int32_t ret_val = LS_OK;
  int32_t status = LS_OK;
  uint32_t watchdog = 0;
  LS_ListNode* current = NULL;
  LS_ListNode* next = NULL;

  LS_ENTER("list=%p,free_func=%p\n", list, destroy_func);

  if (list == NULL)
  {
    LS_DEBUG("Empty list, return...\n");
    return LS_OK;
  }

  if (destroy_func == NULL)
  {
    LS_ERROR("LS_ERROR_GENERAL: Invalid parameters\n");
    ret_val = LS_ERROR_GENERAL;
    LS_LEAVE("ret_val=%d\n", ret_val);
    return ret_val;
  }

#ifdef LS_LIST_DEBUG
  if (list->magic_id != LS_LIST_MAGIC)
  {
    LS_ERROR("LS_ERROR_GENERAL: list %p currupted\n", list);
    ret_val = LS_ERROR_GENERAL;
    LS_LEAVE("ret_val=%d\n", ret_val);
    return ret_val;
  }
#endif

  status = LS_MutexWait(list->mutex);

  if (status == 0)
  {
    LS_ERROR("LS_ERROR_SYSTEM_ERROR: LS_MutexWait() fail\n");
    ret_val = 0;
    LS_LEAVE("ret_val=%d\n", ret_val);
    return ret_val;
  }

  do
  {
    current = list->anchor.next;

    while (current != NULL &&
           list->count != 0
#ifdef LS_LIST_DEBUG
           && watchdog < LS_LIST_WATCH_DOG
#endif
           )
    {
      next = current->next;
#ifdef LS_LIST_NODE_DEBUG
      if (current->magic_id != LS_LIST_NODE_MAGIC)
      {
        LS_ERROR("LS_ERROR_GENERAL: node corrupted: " "node %p,node->magic_id= 0x%08x\n",
                 current,
                 (uint32_t)current->magic_id);
        ret_val = 0;
        break;
      }
      current->magic_id = 0xDEADBEEF;
#endif
      destroy_func(current->data, userdata);
      LS_TRACE("data %p in node %p,list %p was processed\n", current->data, current, list);
      current->next = NULL;
      current->data = NULL;
      list->deallocator(current);
      LS_TRACE("node %p released\n", current);
      list->count -= 1;
      current = next;
      watchdog++;
    }
  }while (0);

  if ((current != NULL) ||
      (list->count != 0))
  {
    LS_ERROR("LS_ERROR_GENERAL: fail when delete each node\n");
    ret_val = 0;
  }

  list->anchor.next = NULL;
  list->anchor.data = NULL;

#ifdef LS_LIST_DEBUG
  if (watchdog == LS_LIST_WATCH_DOG)
  {
    LS_ERROR("LS_ERROR_GENERAL: Watchdog overflow!\n");
    ret_val = 0;
  }
#endif

  status = LS_MutexSignal((list->mutex));

  if (status == 0)
  {
    LS_ERROR("LS_ERROR_SYSTEM_ERROR: signal mutex failed\n");
    ret_val = 0;
  }

  LS_LEAVE("ret_val=%d\n", ret_val);
  return ret_val;
}


void*
LS_ListFirstData(LS_List* list)
{
  void* ret_val = NULL;
  int32_t status = LS_OK;
  LS_ListNode* next = NULL;

  LS_ENTER("list=%p\n", list);

  if (list == NULL)
  {
    LS_DEBUG("Empty list, return...\n");
    ret_val = NULL;
    LS_LEAVE("ret_val=%p\n", ret_val);
    return ret_val;
  }

#ifdef LS_LIST_DEBUG
  if (list->magic_id != LS_LIST_MAGIC)
  {
    LS_ERROR("LS_ERROR_GENERAL: list %p currupted\n", list);
    ret_val = NULL;
    LS_LEAVE("ret_val=%p\n", ret_val);
    return ret_val;
  }
#endif

  status = LS_MutexWait(list->mutex);

  if (status == 0)
  {
    LS_ERROR("LS_ERROR_SYSTEM_ERROR: wait for mutex failed\n");
    ret_val = NULL;
    LS_LEAVE("ret_val=%p\n", ret_val);
    return ret_val;
  }

  next = list->anchor.next;

#ifdef LS_LIST_NODE_DEBUG
  if ((next != NULL) &&
      (next->magic_id != LS_LIST_NODE_MAGIC))
  {
    LS_ERROR("LS_ERROR_GENERAL: node %p currupted,node->magic_id=%" PRIxPTR "\n", next, (uintptr_t)next->magic_id);
    ret_val = NULL;
    LS_LEAVE("ret_val=%p\n", ret_val);
    return ret_val;
  }
#endif

  if (next != NULL)
  {
    list->anchor.next = next->next;
    list->count -= 1;
    ret_val = next->data;
    next->next = NULL;
    next->data = NULL;
#ifdef LS_LIST_NODE_DEBUG
    next->magic_id = 0xDEADBEEF;
#endif
    list->deallocator(next);
    LS_TRACE("node %p was released\n", next);
  }

  status = LS_MutexSignal((list->mutex));

  if (status == 0)
  {
    LS_ERROR("LS_ERROR_SYSTEM_ERROR: signal mutex failed\n");
    ret_val = NULL;
    LS_LEAVE("ret_val=%p\n", ret_val);
    return ret_val;
  }

  LS_LEAVE("ret_val=%p\n", ret_val);
  return ret_val;
}


int32_t
LS_ListDestroy(LS_List* list)
{
  int32_t ret_val = LS_OK;
  int32_t status = LS_OK;

  LS_ENTER("list=%p\n", list);

  if (list == NULL)
  {
    LS_DEBUG("list = nil,return...\n");
    ret_val = LS_OK;
    LS_LEAVE("ret_val=%d\n", ret_val);
    return ret_val;
  }

#ifdef LS_LIST_DEBUG
  if (list->magic_id != LS_LIST_MAGIC)
  {
    LS_ERROR("LS_ERROR_GENERAL: list %p currupted\n", list);
    ret_val = 0;
    LS_LEAVE("ret_val=%d\n", ret_val);
    return ret_val;
  }
#endif

  if (list->count != 0)
  {
    LS_ERROR("LS_ERROR_GENERAL: The list is not empty, memory leak!!!!\n ");
  }
  else
  {
    status = LS_MutexDelete(list->mutex);

    if (status != 1)
    {
      LS_ERROR("LS_ERROR_SYSTEM_ERROR: delete mutex failed\n");
      ret_val = 0;
    }

    list->mutex = NULL;
#ifdef LS_LIST_DEBUG
    list->magic_id = 0xDEADBEEF;
#endif
    list->deallocator(list);
    LS_DEBUG("list %p was freed\n", list);
  }

  LS_LEAVE("ret_val=%d\n", ret_val);
  return ret_val;
}


int32_t
LS_ListDump(LS_List* list)
{
  int32_t ret_val = LS_OK;
  int32_t status = LS_OK;
  uint32_t watchdog = 0;
  LS_ListNode* current = NULL;

  LS_ENTER("list=%p\n", list);

  if (list == NULL)
  {
    LS_ERROR("LS_ERROR_GENERAL: Invalid parameters\n");
    ret_val = 0;
    LS_LEAVE("ret_val=%d\n", ret_val);
    return ret_val;
  }

#ifdef LS_LIST_DEBUG
  if (list->magic_id != LS_LIST_MAGIC)
  {
    LS_ERROR("LS_ERROR_GENERAL: list %p currupted\n", list);
    ret_val = 0;
    LS_LEAVE("ret_val=%d\n", ret_val);
    return ret_val;
  }
#endif

  status = LS_MutexWait(list->mutex);

  if (status == 0)
  {
    LS_ERROR("LS_ERROR_SYSTEM_ERROR: wait for mutex failed\n");
    ret_val = 0;
    LS_LEAVE("ret_val=%d\n", ret_val);
    return ret_val;
  }

  do
  {
    LS_TRACE("list=%p,list->count=%d\n", list, list->count);
    current = list->anchor.next;

    while (current != NULL &&
           list->count != 0
#ifdef LS_LIST_DEBUG
           && watchdog < LS_LIST_WATCH_DOG
#endif
           )
    {
#ifdef LS_LIST_NODE_DEBUG
      if (current->magic_id != LS_LIST_NODE_MAGIC)
      {
        LS_ERROR("LS_ERROR_GENERAL: node corrupted: " "node %p,node->magic_id= 0x%08x (should be 0x%08x)\n",
                 current,
                 (uint32_t)current->magic_id,
                 (uint32_t)LS_LIST_NODE_MAGIC);
        ret_val = 0;
        break;
      }
#endif
      LS_TRACE("[%d] current=%p,current->data=%p, " "current->next=%p\n",
               watchdog,
               current,
               current->data,
               current->next);
      current = current->next;
      watchdog++;
    }
  }while (0);

#ifdef LS_LIST_DEBUG
  if (watchdog == LS_LIST_WATCH_DOG)
  {
    LS_ERROR("LS_ERROR_GENERAL: Watchdog overflow!\n");
    ret_val = 0;
  }
#endif

  status = LS_MutexSignal((list->mutex));

  if (status == 0)
  {
    LS_ERROR("LS_ERROR_SYSTEM_ERROR: signal mutex failed\n");
    ret_val = 0;
  }

  LS_LEAVE("ret_val=%d\n", ret_val);
  return ret_val;
}


int32_t
LS_ListCount(LS_List* list, uint32_t* number)
{
  int32_t ret_val = LS_OK;
  int32_t status = LS_OK;

  LS_ENTER("list=%p\n", list);

  if (number == NULL)
  {
    LS_ERROR("LS_ERROR_GENERAL: Invalid parameters: list=%p,number = %p\n", (void*)list, (void*)number);
    ret_val = LS_ERROR_GENERAL;
    LS_LEAVE("ret_val=%d\n", ret_val);
    return ret_val;
  }

  if (list == NULL)
  {
    LS_DEBUG("Empty list,return...\n");
    *number = 0;
    ret_val = LS_OK;
    LS_LEAVE("ret_val=%d\n", ret_val);
    return ret_val;
  }

  *number = 0;

#ifdef LS_LIST_DEBUG
  if (list->magic_id != LS_LIST_MAGIC)
  {
    LS_ERROR("LS_ERROR_GENERAL: list %p currupted\n", list);
    ret_val = LS_ERROR_GENERAL;
    LS_LEAVE("ret_val=%d\n", ret_val);
    return ret_val;
  }
#endif

  status = LS_MutexWait(list->mutex);

  if (status == 0)
  {
    LS_ERROR("LS_ERROR_SYSTEM_ERROR: wait for mutex failed\n");
    ret_val = LS_ERROR_SYSTEM_ERROR;
    LS_LEAVE("ret_val=%d\n", ret_val);
    return ret_val;
  }

  *number = list->count;
  status = LS_MutexSignal((list->mutex));

  if (status == 0)
  {
    LS_ERROR("LS_ERROR_SYSTEM_ERROR: signal mutex failed\n");
    ret_val = LS_ERROR_SYSTEM_ERROR;
    LS_LEAVE("ret_val=%d\n", ret_val);
  }

  /*good to go now...*/
  ret_val = LS_OK;
  LS_LEAVE("ret_val=%d\n", ret_val);
  return ret_val;
}


void*
LS_ListNthNode(LS_List* list, uint32_t nth)
{
  void* ret_val = NULL;
  int32_t status = LS_OK;
  uint32_t watchdog = 0;
  uint32_t count = 0;
  LS_ListNode* current = NULL;

  LS_ENTER("list=%p,nth=%d\n", list, nth);

  if (list == NULL)
  {
    LS_DEBUG("Empty list, return...\n");
    ret_val = NULL;
    LS_LEAVE("ret_val=%p\n", ret_val);
    return ret_val;
  }

#ifdef LS_LIST_DEBUG
  if (list->magic_id != LS_LIST_MAGIC)
  {
    LS_ERROR("LS_ERROR_GENERAL:list %p currupted\n", list);
    ret_val = NULL;
    LS_LEAVE("ret_val=%p\n", ret_val);
    return ret_val;
  }
#endif

  status = LS_MutexWait(list->mutex);

  if (status == 0)
  {
    LS_ERROR("LS_ERROR_SYSTEM_ERROR: wait for mutex failed\n");
    ret_val = NULL;
    LS_LEAVE("ret_val=%p\n", ret_val);
    return ret_val;
  }

  do
  {
    if (list->count - 1 < nth)
    {
      LS_ERROR("LS_ERROR_GENERAL: over range, there are only %d elements\n", list->count);
      ret_val = NULL;
      break;
    }

    current = (&(list->anchor))->next;

    while (current
#ifdef LS_LIST_DEBUG
           && watchdog < LS_LIST_WATCH_DOG
#endif
           )
    {
#ifdef LS_LIST_NODE_DEBUG
      if (current->magic_id != LS_LIST_NODE_MAGIC)
      {
        LS_ERROR("LS_ERROR_GENERAL: node corrupted: " "current=%p,current->magic_id= 0x%08x,"
                 "current->data=%p,current->next=%p\n",
                 current,
                 (uint32_t)current->magic_id,
                 current->data,
                 current->next);
        ret_val = NULL;
        break;
      }
#endif

      if (count == nth)
      {
        ret_val = current->data;
        break;
      }

      current = current->next;
      count++;
      watchdog++;
    }
  }while (0);

#ifdef LS_LIST_DEBUG
  if (watchdog == LS_LIST_WATCH_DOG)
  {
    LS_ERROR("LS_ERROR_GENERAL: Watchdog overflow!\n");
    ret_val = 0;
  }
#endif

  if (current == NULL)
  {
    // LS_ERROR("LS_ERROR_GENERAL: would not find %dth node in list %p\n",count,(void*)list);
    ret_val = NULL;
  }

  status = LS_MutexSignal((list->mutex));

  if (status == 0)
  {
    LS_ERROR("LS_ERROR_SYSTEM_ERROR: signal mutex failed\n");
    ret_val = NULL;
  }

  // LS_LEAVE("retVal=%p\n",(void*)retVal);
  return ret_val;
}


int32_t
LS_ListForEachNode(LS_List* list, ForEachNodeFunc foreachfunc, void* user_data)
{
  int32_t ret_val = LS_OK;
  int32_t status = LS_OK;
  uint32_t watchdog = 0;
  LS_ListNode* current = NULL;

  LS_ENTER("list=%p\n", list);

  if (list == NULL)
  {
    LS_ERROR("LS_ERROR_GENERAL: Invalid parameters\n");
    ret_val = 0;
    LS_LEAVE("ret_val=%d\n", ret_val);
    return ret_val;
  }

#ifdef LS_LIST_DEBUG
  if (list->magic_id != LS_LIST_MAGIC)
  {
    LS_ERROR("LS_ERROR_GENERAL: list %p currupted\n", list);
    ret_val = 0;
    LS_LEAVE("ret_val=%d\n", ret_val);
    return ret_val;
  }
#endif

  status = LS_MutexWait(list->mutex);

  if (status == 0)
  {
    LS_ERROR("LS_ERROR_SYSTEM_ERROR: wait for mutex failed\n");
    ret_val = 0;
    LS_LEAVE("ret_val=%d\n", ret_val);
    return ret_val;
  }

  do
  {
    LS_TRACE("list=%p,list->count=%d\n", list, list->count);
    current = list->anchor.next;

    while (current != NULL &&
           list->count != 0
#ifdef LS_LIST_DEBUG
           && watchdog < LS_LIST_WATCH_DOG
#endif
           )
    {
#ifdef LS_LIST_NODE_DEBUG
      if (current->magic_id != LS_LIST_NODE_MAGIC)
      {
        LS_ERROR("LS_ERROR_GENERAL: node corrupted: " "node %p,node->magic_id= 0x%08x (should be 0x%08x)\n",
                 current,
                 (uint32_t)current->magic_id,
                 (uint32_t)LS_LIST_NODE_MAGIC);
        ret_val = 0;
        break;
      }
#endif
      LS_TRACE("[%d] current=%p,current->data=%p,current->next=%p\n", watchdog, current, current->data, current->next);
      foreachfunc(current->data, user_data);
      current = current->next;
      watchdog++;
    }
  }while (0);

#ifdef LS_LIST_DEBUG
  if (watchdog == LS_LIST_WATCH_DOG)
  {
    LS_ERROR("LS_ERROR_GENERAL: Watchdog overflow!\n");
    ret_val = 0;
  }
#endif

  status = LS_MutexSignal((list->mutex));

  if (status == 0)
  {
    LS_ERROR("LS_ERROR_SYSTEM_ERROR: signal mutex failed\n");
    ret_val = 0;
  }

  LS_LEAVE("ret_val=%d\n", ret_val);
  return ret_val;
}


/* eof */
