/*-----------------------------------------------------------------------------
 * lslist.h
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
/**
 * @file lslist.h
 * @brief Linked List Implementation
 *
 * This header provides a thread-safe linked list implementation with
 * support for custom allocators and various list operations.
 */

#ifndef __LS_LIST_H__
#define __LS_LIST_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include "lssubmacros.h"
#include "lssystem.h"
/*---------------------------------------------------------------------------
 * local macros
 *--------------------------------------------------------------------------*/

#ifndef NDEBUG
/** @brief Enable list debugging */
#define LS_LIST_DEBUG
/** @brief List magic number ('LIST') */
#define LS_LIST_MAGIC    MAKE_MAGIC_NUMBER('L', 'I', 'S', 'T')

extern const uint32_t LS_LIST_WATCH_DOG;
/** @brief Enable node debugging */
#define LS_LIST_NODE_DEBUG
/** @brief Node magic number ('NODE') */
#define LS_LIST_NODE_MAGIC    MAKE_MAGIC_NUMBER('N', 'O', 'D', 'E')
#else
#undef  LS_LIST_DEBUG
#undef  LS_LIST_MAGIC
#undef  LS_LIST_WATCH_DOG

#undef  LS_LIST_NODE_DEBUG
#undef  LS_LIST_NODE_MAGIC
#endif
/*---------------------------------------------------------------------------
 * typedef
 *--------------------------------------------------------------------------*/
/** @brief List node structure */
typedef struct _LS_ListNode  LS_ListNode;
/** @brief List structure */
typedef struct _LS_List      LS_List;
/**
 * @brief Destroy callback function type
 *
 * Called when destroying list nodes to free their data.
 *
 * @param data Node data to destroy
 * @param userdata User-provided context
 */
typedef void (*DestroyFunc)(void* data, void* userdata);
/**
 * @brief For-each callback function type
 *
 * Called for each node during list iteration.
 *
 * @param data Node data
 * @param userdata User-provided context
 */
typedef void (*ForEachNodeFunc)(void* data, void* userdata);
/**
 * @brief Compare callback function type
 *
 * Compares two data values.
 *
 * @return Negative if a < b, zero if equal, positive if a > b
 */
typedef int32_t (*CompareFunc)(void* a, void* b);
/**
 * @brief Allocator callback function type
 *
 * Allocates memory for list nodes.
 *
 * @param bytes Number of bytes to allocate
 * @return Pointer to allocated memory
 */
typedef void* (*Allocator)(uint32_t bytes);
/**
 * @brief Deallocator callback function type
 *
 * Frees memory allocated by the allocator.
 *
 * @param ptr Memory to free
 */
typedef void (*Deallocator)(void* ptr);

/*---------------------------------------------------------------------------
 * struct
 *--------------------------------------------------------------------------*/
struct _LS_ListNode
{
#ifdef LS_LIST_NODE_DEBUG
  uint32_t     magic_id;
#endif
  void*        data;
  LS_ListNode* next;
};


struct _LS_List
{
#ifdef LS_LIST_DEBUG
  uint32_t    magic_id;
#endif
  LS_ListNode anchor;
  uint32_t    count;
  LS_Mutex_t  mutex;
  Allocator   allocator;
  Deallocator deallocator;
};


/*---------------------------------------------------------------------------
 * API declarations
 *--------------------------------------------------------------------------*/
LS_List* LS_ListInit(Allocator, Deallocator, int32_t * errcode);

/*add a new node->data= data to the tail of list*/
int32_t LS_ListAppend(LS_List* list, void* data);

/*add a new node->data= data to the head of list*/
int32_t LS_ListPreAppend(LS_List* list, void* data);

/*remove a node->data = data from the list*/
int32_t LS_ListRemoveNode(LS_List* list, void* data);

/*check if this node is in this list. */
int32_t LS_ListFindNode(LS_List* list, void* data);

/*check if this node is in this list.If exist, return node->data,otherwise NULL*/
void* LS_ListFindUserNode(LS_List* list, void* data, CompareFunc compare_func);

/*return the node->data picked by compare_func,and remove it from list*/
void* LS_ListRemoveUserNode(LS_List* list, void* user_data, CompareFunc compare_func);

/* this only free the node, but not free node->data */
int32_t LS_ListEmpty(LS_List* list);

/* this will free the node, also free node->data using destroy_func */
int32_t LS_ListEmptyData(LS_List* list, DestroyFunc destroy_func, void* user_data);

/*return the first node->data and remove it from list, node->count -= 1  */
void* LS_ListFirstData(LS_List* list);

/*free the list only, if list->count!=0, memory will leak*/
int32_t LS_ListDestroy(LS_List* list);
int32_t LS_ListDump(LS_List* list);
int32_t LS_ListCount(LS_List* list, uint32_t* count);

/*return the nth (index from 0th) node->data,but keep it in the list*/
void*   LS_ListNthNode(LS_List* list, uint32_t nth);
int32_t LS_ListForEachNode(LS_List* list, ForEachNodeFunc foreachfunc, void* user_data);


#ifdef __cplusplus
}
#endif


#endif /*_LS_LIST_H*/
