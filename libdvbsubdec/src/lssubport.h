/*-----------------------------------------------------------------------------
 * lssubport.h
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
 * @file lssubport.h
 * @brief Portability and System Function Management
 *
 * This header provides platform portability macros and functions for
 * managing system function and logger callbacks.
 */


#ifndef __LS_PORT_H
#define __LS_PORT_H

#ifdef __cplusplus
extern "C" {
#endif
/*------------------------------------------------------------------------------
 * system include files
 *----------------------------------------------------------------------------*/
#include <malloc.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
/*------------------------------------------------------------------------------
 * local include files
 *----------------------------------------------------------------------------*/
#include "lssubdec.h"
/*----------------------------------------------------------------------------
 * support __FILE__, __LINE__, __func__
 *---------------------------------------------------------------------------*/
// #if __STDC__ && defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
// #else
// #define __func__ " "
// #endif
/*-----------------------------------------------------------------------------
 * Macros for system calls - most implementors will not need to modify these
 * Be sure to include corresponding header file or add "extern" to avoid compiler
 * warnings.
 *----------------------------------------------------------------------------*/
/** @brief System memcpy wrapper */
#define SYS_MEMCPY(a, b, c)    memcpy(a, b, c)
/** @brief System memset wrapper */
#define SYS_MEMSET(a, b, c)    memset(a, b, c)
/** @brief System strlen wrapper */
#define SYS_STRLEN(a)          strlen(a)
/** @brief System malloc wrapper */
#define SYS_MALLOC(a)          malloc(a)
/** @brief System calloc wrapper */
#define SYS_CALLOC(a, b)       calloc(a, b)
/** @brief System free wrapper */
#define SYS_FREE(a)            free(a)
/** @brief System realloc wrapper */
#define SYS_REALLOC(a, b)      realloc(a, b)
/** @brief System assert wrapper */
#define SYS_ASSERT(a)          assert(a)
/*-----------------------------------------------------------------------------
 * public extern APIs
 *----------------------------------------------------------------------------*/
/**
 * @brief Update system function callbacks
 *
 * Updates the system function table with user-provided callbacks.
 *
 * @param sysFuncs System function structure
 * @return LS_OK (1) on success, error code on failure
 */
int32_t LS_UpdateSystemFuncs(const LS_SystemFuncs_t sysFuncs);

/**
 * @brief Get system function callbacks
 *
 * Returns the current system function table.
 *
 * @return Pointer to system function table
 */
LS_SystemFuncs_t* LS_GetSystemFuncs(void);

/**
 * @brief Reset system function callbacks
 *
 * Resets the system function table to defaults.
 */
void LS_ResetSystemFuncs(void);

/**
 * @brief Update system logger
 *
 * Updates the system logger with user-provided callbacks.
 *
 * @param logger Logger configuration
 * @return LS_OK (1) on success, error code on failure
 */
int32_t LS_UpdateSystemLogger(const LS_SystemLogger_t logger);

/**
 * @brief Get system logger
 *
 * Returns the current system logger configuration.
 *
 * @return Pointer to logger configuration
 */
LS_SystemLogger_t* LS_GetSystemLogger(void);

/**
 * @brief Reset system logger
 *
 * Resets the system logger to defaults.
 */
void LS_ResetSystemLogger(void);

/**
 * @brief Finalize system port resources
 *
 * Cleans up mutexes and other resources used by the system port layer.
 * Should be called during library shutdown.
 */
void LS_FinalizeSystemPort(void);


#ifdef __cplusplus
}
#endif


#endif /* LS_PORT_C */
