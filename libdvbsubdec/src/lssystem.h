/*-----------------------------------------------------------------------------
 * lssystem.h
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
 * @file lssystem.h
 * @brief System Abstraction Layer
 *
 * This header provides platform-independent wrappers for system-level
 * operations including mutexes, timers, and logging.
 */

#ifndef __LS_SYSTEM_H__
#define __LS_SYSTEM_H__


#ifdef __cplusplus
extern "C" {
#endif

#include "lssubport.h"
#include "lssubdec.h"
/*-----------------------------------------------------------------------------
 * Mutex Functions
 *---------------------------------------------------------------------------*/
/**
 * @brief Create a mutex
 *
 * Creates a new mutex for thread synchronization.
 *
 * @param mutex Pointer to receive mutex handle
 * @return LS_OK (1) on success, error code on failure
 */
int32_t LS_MutexCreate(LS_Mutex_t* mutex);

/**
 * @brief Delete a mutex
 *
 * Releases a mutex created with LS_MutexCreate.
 *
 * @param mutex Mutex handle
 * @return LS_OK (1) on success, error code on failure
 */
int32_t LS_MutexDelete(LS_Mutex_t mutex);

/**
 * @brief Wait for/acquire a mutex
 *
 * Locks the mutex, blocking if already locked.
 *
 * @param mutex Mutex handle
 * @return LS_OK (1) on success, error code on failure
 */
int32_t LS_MutexWait(LS_Mutex_t mutex);

/**
 * @brief Signal/release a mutex
 *
 * Unlocks the mutex.
 *
 * @param mutex Mutex handle
 * @return LS_OK (1) on success, error code on failure
 */
int32_t LS_MutexSignal(LS_Mutex_t mutex);

/*-----------------------------------------------------------------------------
 * Timer Functions
 *---------------------------------------------------------------------------*/
/**
 * @brief Create a timer
 *
 * Creates a new timer with the specified callback.
 *
 * @param timer Pointer to receive timer handle
 * @param callbackfunc Function to call when timer expires
 * @param param User parameter to pass to callback
 * @return LS_OK (1) on success, error code on failure
 */
int32_t LS_TimerNew(LS_Timer_t* timer, void (*callbackfunc) (void* param), void* param);

/**
 * @brief Start a timer
 *
 * Starts a timer with the specified timeout.
 *
 * @param timer Timer handle
 * @param time_ms Timeout in milliseconds
 * @return LS_OK (1) on success, error code on failure
 */
int32_t LS_TimerStart(LS_Timer_t timer, uint32_t time_ms);

/**
 * @brief Stop a timer
 *
 * Stops a running timer and retrieves remaining time.
 *
 * @param timer Timer handle
 * @param left_ms Pointer to receive remaining time
 * @return LS_OK (1) on success, error code on failure
 */
int32_t LS_TimerStop(LS_Timer_t timer, LS_Time_t* left_ms);

/**
 * @brief Delete a timer
 *
 * Releases a timer created with LS_TimerNew.
 *
 * @param timer Timer handle
 * @return LS_OK (1) on success, error code on failure
 */
int32_t LS_TimerDelete(LS_Timer_t timer);

/*-----------------------------------------------------------------------------
 * Utility Functions
 *---------------------------------------------------------------------------*/
/**
 * @brief Get current timestamp string
 *
 * Returns a formatted string representing the current time.
 *
 * @return Timestamp string (static buffer)
 */
char* LS_GetTimeStamp(void);

/**
 * @brief Printf wrapper
 *
 * Logs a formatted message at the specified verbosity level.
 *
 * @param level Verbosity level
 * @param format Printf-style format string
 * @param ... Format arguments
 * @return LS_OK (1) on success, error code on failure
 */
int32_t LS_Printf(int32_t level, const char* format, ...);

#ifdef __cplusplus
}
#endif


#endif
