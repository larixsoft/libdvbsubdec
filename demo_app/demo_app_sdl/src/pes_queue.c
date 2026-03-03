/*-----------------------------------------------------------------------------
 * pes_queue.c
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

#include "pes_queue.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
/*-----------------------------------------------------------------------------
 * Public Functions
 *---------------------------------------------------------------------------*/
PES_Queue_t*
pes_queue_create(size_t capacity)
{
  PES_Queue_t* queue = calloc(1, sizeof(PES_Queue_t));

  if (!queue)
  {
    return NULL;
  }

  queue->packets = calloc(capacity, sizeof(PES_Packet_t));

  if (!queue->packets)
  {
    free(queue);
    return NULL;
  }

  queue->capacity = capacity;
  queue->head = 0;
  queue->tail = 0;
  queue->count = 0;
  queue->shutdown = false;

  if (pthread_mutex_init(&queue->mutex, NULL) != 0)
  {
    free(queue->packets);
    free(queue);
    return NULL;
  }

  if (pthread_cond_init(&queue->cond, NULL) != 0)
  {
    pthread_mutex_destroy(&queue->mutex);
    free(queue->packets);
    free(queue);
    return NULL;
  }

  return queue;
}


void
pes_queue_destroy(PES_Queue_t* queue)
{
  if (!queue)
  {
    return;
  }

  /* Signal shutdown and wake up any waiting threads */
  pthread_mutex_lock(&queue->mutex);
  queue->shutdown = true;
  pthread_cond_broadcast(&queue->cond);
  pthread_mutex_unlock(&queue->mutex);
  /* Wait a bit for threads to wake up */
  usleep(10000);

  /* Free all packet data */
  for (size_t i = 0; i < queue->capacity; i++)
  {
    if (queue->packets[i].data)
    {
      free(queue->packets[i].data);
    }
  }

  pthread_mutex_destroy(&queue->mutex);
  pthread_cond_destroy(&queue->cond);
  free(queue->packets);
  free(queue);
}


bool
pes_queue_enqueue(PES_Queue_t* queue, const uint8_t* data, size_t size)
{
  if (!queue ||
      !data ||
      (size == 0))
  {
    return false;
  }

  pthread_mutex_lock(&queue->mutex);

  /* Wait while queue is full and not shutdown */
  while (queue->count >= queue->capacity &&
         !queue->shutdown)
  {
    pthread_cond_wait(&queue->cond, &queue->mutex);
  }

  if (queue->shutdown)
  {
    pthread_mutex_unlock(&queue->mutex);
    return false;
  }

  /* Allocate buffer for packet data */
  queue->packets[queue->tail].data = malloc(size);

  if (!queue->packets[queue->tail].data)
  {
    pthread_mutex_unlock(&queue->mutex);
    return false;
  }

  /* Copy packet data */
  memcpy(queue->packets[queue->tail].data, data, size);
  queue->packets[queue->tail].size = size;
  /* Move tail forward (circular buffer) */
  queue->tail = (queue->tail + 1) % queue->capacity;
  queue->count++;
  /* Signal waiting threads */
  pthread_cond_signal(&queue->cond);
  pthread_mutex_unlock(&queue->mutex);
  return true;
}


bool
pes_queue_dequeue(PES_Queue_t* queue, PES_Packet_t* packet)
{
  if (!queue ||
      !packet)
  {
    return false;
  }

  pthread_mutex_lock(&queue->mutex);

  /* Wait while queue is empty and not shutdown */
  while (queue->count == 0 &&
         !queue->shutdown)
  {
    pthread_cond_wait(&queue->cond, &queue->mutex);
  }

  if (queue->count == 0)
  {
    pthread_mutex_unlock(&queue->mutex);
    return false;     /* Queue empty and shutdown */
  }

  /* Copy packet data */
  *packet = queue->packets[queue->head];
  queue->packets[queue->head].data = NULL;   /* Clear pointer */
  /* Move head forward (circular buffer) */
  queue->head = (queue->head + 1) % queue->capacity;
  queue->count--;
  /* Signal waiting threads */
  pthread_cond_signal(&queue->cond);
  pthread_mutex_unlock(&queue->mutex);
  return true;
}


void
pes_queue_shutdown(PES_Queue_t* queue)
{
  if (!queue)
  {
    return;
  }

  pthread_mutex_lock(&queue->mutex);
  queue->shutdown = true;
  pthread_cond_broadcast(&queue->cond);
  pthread_mutex_unlock(&queue->mutex);
}


bool
pes_queue_is_empty(PES_Queue_t* queue)
{
  if (!queue)
  {
    return true;
  }

  pthread_mutex_lock(&queue->mutex);

  bool empty = (queue->count == 0);

  pthread_mutex_unlock(&queue->mutex);
  return empty;
}


size_t
pes_queue_count(PES_Queue_t* queue)
{
  if (!queue)
  {
    return 0;
  }

  pthread_mutex_lock(&queue->mutex);

  size_t count = queue->count;

  pthread_mutex_unlock(&queue->mutex);
  return count;
}
