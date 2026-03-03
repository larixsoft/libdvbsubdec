/*-----------------------------------------------------------------------------
 * pes_queue.h
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

#ifndef PES_QUEUE_H
#define PES_QUEUE_H

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
/*-----------------------------------------------------------------------------
 * PES Packet Structure
 *---------------------------------------------------------------------------*/
/**
 * @brief PES packet data structure
 */
typedef struct
{
  uint8_t* data;           /**< Pointer to packet data */
  size_t   size;           /**< Size of packet data */
} PES_Packet_t;
/*-----------------------------------------------------------------------------
 * PES Queue Structure
 *---------------------------------------------------------------------------*/
/**
 * @brief Thread-safe queue for PES packets
 */
typedef struct
{
  PES_Packet_t*   packets;  /**< Array of packets */
  size_t          capacity; /**< Maximum number of packets */
  size_t          head;     /**< Head index (for dequeue) */
  size_t          tail;     /**< Tail index (for enqueue) */
  size_t          count;    /**< Current number of packets */
  pthread_mutex_t mutex;    /**< Mutex for thread safety */
  pthread_cond_t  cond;     /**< Condition variable for signaling */
  bool            shutdown; /**< Flag to indicate queue shutdown */
} PES_Queue_t;
/*-----------------------------------------------------------------------------
 * Functions
 *---------------------------------------------------------------------------*/
/**
 * @brief Create a new PES packet queue
 * @param capacity Maximum number of packets in queue
 * @return Pointer to new queue or NULL on failure
 */
PES_Queue_t* pes_queue_create(size_t capacity);

/**
 * @brief Destroy a PES packet queue and free all resources
 * @param queue Pointer to queue to destroy
 */
void pes_queue_destroy(PES_Queue_t* queue);

/**
 * @brief Enqueue a PES packet (blocking if queue is full)
 * @param queue Pointer to queue
 * @param data Pointer to packet data (will be copied)
 * @param size Size of packet data
 * @return true on success, false on failure
 */
bool pes_queue_enqueue(PES_Queue_t* queue, const uint8_t* data, size_t size);

/**
 * @brief Dequeue a PES packet (blocking if queue is empty)
 * @param queue Pointer to queue
 * @param packet Pointer to packet structure to fill
 * @return true on success, false if queue is shutdown
 */
bool pes_queue_dequeue(PES_Queue_t* queue, PES_Packet_t* packet);

/**
 * @brief Signal queue to shutdown (wakes up waiting threads)
 * @param queue Pointer to queue
 */
void pes_queue_shutdown(PES_Queue_t* queue);

/**
 * @brief Check if queue is empty
 * @param queue Pointer to queue
 * @return true if empty, false otherwise
 */
bool pes_queue_is_empty(PES_Queue_t* queue);

/**
 * @brief Get current packet count
 * @param queue Pointer to queue
 * @return Number of packets in queue
 */
size_t pes_queue_count(PES_Queue_t* queue);

#endif /* PES_QUEUE_H */
