/*-----------------------------------------------------------------------------
 * lsmemory.h
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
 * @file lsmemory.h
 * @brief Memory Management System
 *
 * This header provides a custom memory heap implementation for the
 * subtitle decoder, supporting fixed-size heaps with allocation tracking.
 */

#ifndef __LS_MEMORY_H__
#define __LS_MEMORY_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "lssubport.h"
#include "lssubmacros.h"
#include "lssystem.h"
/*---------------------------------------------------------------------------
 * local macros
 *--------------------------------------------------------------------------*/
/**
 * @brief Memory allocator selection
 *
 * LS_USE_SYSTEM_MALLOC: Use system malloc/free directly (fastest, recommended)
 * LS_INTERNAL_HEAP: Use custom internal heap (for embedded/constrained systems)
 *
 * If neither is defined, defaults to OS malloc with size tracking wrapper.
 *
 * To customize, add to your build configuration:
 *   - CMake: -DLS_USE_SYSTEM_MALLOC=ON (recommended for desktop/server)
 *   - CMake: -DLS_INTERNAL_HEAP=ON (for embedded systems)
 *   - Or define in lssubport.h before including this header
 */
#ifdef LS_USE_SYSTEM_MALLOC
  /* Fast path: use system malloc directly */
  #define LS_MEM_USE_SYSTEM_MALLOC
#elif defined(LS_INTERNAL_HEAP)
  /* Custom heap implementation for constrained systems */
#else
  /* Default: OS malloc with size tracking wrapper */
  #define LS_MEM_TRACKING_WRAPPER
#endif

/** @brief Size of a memory node */
#define LS_MEM_NODE_SIZE    (sizeof(LS_MemNode))


#if 1
/** @brief Enable heap debugging */
#define LS_MEM_DEBUG_HEAP
/** @brief Heap magic number ('HEAP') */
#define LS_MEM_MAGIC_NUMBER    MAKE_MAGIC_NUMBER('H', 'E', 'A', 'P')
/** @brief Enable heap dumping */
#define LS_MEM_DUMP_HEAP
#else
#undef LS_MEM_DEBUG_HEAP
#undef LS_MEM_MAGIC_NUMBER
#undef LS_MEM_DUMP_HEAP
#endif
/*---------------------------------------------------------------------------
 * typedefs
 *--------------------------------------------------------------------------*/
/** @brief Memory node structure */
typedef struct _LS_MemNode  LS_MemNode;
/** @brief Memory heap structure */
typedef struct _LS_MemHeap  LS_MemHeap;

/*---------------------------------------------------------------------------
 * struct
 *--------------------------------------------------------------------------*/
/**
 * @brief Memory node structure
 *
 * Represents a single block in the memory heap.
 */
struct _LS_MemNode
{
#ifdef LS_MEM_DEBUG_HEAP
  uint32_t    magic_id; /**< Magic number for validation */
#endif
  LS_MemNode* next;     /**< Next node in free list */
  uint32_t    units;    /**< Size of this node in units */
};


/**
 * @brief Memory heap structure
 *
 * Represents a memory heap for allocation.
 */
struct _LS_MemHeap
{
#ifdef LS_MEM_DEBUG_HEAP
  uint32_t magic_id;    /**< Magic number for validation */
  uint32_t peak;        /**< Peak allocation           */
#endif

#ifdef LS_INTERNAL_HEAP
  LS_MemNode  anchor;     /**< Anchor node for base calculation */
  LS_MemNode* memory;     /**< Start address of heap memory */
#endif
  uint32_t    heap_units; /**< Size of heap in bytes    */
  uint32_t    allocated;  /**< Bytes currently allocated */
  LS_Mutex_t  mutex;      /**< Mutex for thread safety  */
};


/*-----------------------------------------------------------------------------
 * prototype functions
 *---------------------------------------------------------------------------*/
/**
 * @brief Initialize a memory heap
 *
 * Creates and initializes a new memory heap.
 *
 * @param heap Pointer to receive heap handle
 * @param heap_size Size of heap in bytes
 * @return LS_OK (1) on success, error code on failure
 */
int32_t LS_MemInit(LS_MemHeap* * heap, uint32_t heap_size);

/**
 * @brief Finalize a memory heap
 *
 * Releases a memory heap and all its resources.
 *
 * @param heap Heap to finalize
 * @return LS_OK (1) on success, error code on failure
 */
int32_t LS_MemFinalize(LS_MemHeap* heap);

/**
 * @brief Allocate memory from heap
 *
 * Allocates a block of memory from the specified heap.
 *
 * @param heap Heap to allocate from
 * @param bytes Number of bytes to allocate
 * @return Pointer to allocated memory or NULL on failure
 */
void* LS_Malloc(LS_MemHeap* heap, uint32_t bytes);

/**
 * @brief Free memory to heap
 *
 * Frees a previously allocated block of memory.
 *
 * @param heap Heap to free to
 * @param ptr Memory to free
 * @return LS_OK (1) on success, error code on failure
 */
int32_t LS_Free(LS_MemHeap* heap, void* ptr);

/**
 * @brief Reallocate memory
 *
 * Resizes a previously allocated memory block.
 *
 * @param heap Heap to allocate from
 * @param ptr Existing memory block or NULL
 * @param bytes New size in bytes
 * @return Pointer to resized memory or NULL on failure
 */
void* LS_Realloc(LS_MemHeap* heap, void* ptr, uint32_t bytes);

/**
 * @brief Dump memory contents
 *
 * Prints a hex dump of memory for debugging.
 *
 * @param mem Memory to dump
 * @param size Number of bytes to dump
 */
void LS_DumpMem(const char* mem, const uint32_t size);

/**
 * @brief Copy memory
 *
 * Copies bytes from source to destination.
 *
 * @param pDest Destination buffer
 * @param pSrc Source buffer
 * @param bytes Number of bytes to copy
 */
void LS_MemCopy(void* pDest, const void* pSrc, uint32_t bytes);


#ifdef __cplusplus
}
#endif


#endif /*__LS_MEMORY_H*/
