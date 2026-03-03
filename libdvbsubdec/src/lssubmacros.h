/*-----------------------------------------------------------------------------
 * lssubmacros.h
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
 * @file lssubmacros.h
 * @brief Utility Macros and Inline Functions
 *
 * This header provides general-purpose macros, inline functions, and
 * utility definitions used throughout the subtitle decoder library.
 *
 * @details Includes:
 * - Boolean value definitions
 * - Math utility macros (MIN, MAX, CLAMP, etc.)
 * - Color conversion macros (RGB <-> YCbCr)
 * - Debug and logging macros
 * - Magic number definitions
 */

#ifndef __LS_MACROS_H__
#define __LS_MACROS_H__


#ifdef __cplusplus
extern "C" {
#endif


#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "lssubdec.h"
#include "lssubport.h"
#include "lssystem.h"

/* Disable type-limits warnings for CLAMP macro (comparison of unsigned with 0 is expected) */
#pragma GCC diagnostic ignored "-Wtype-limits"

/** @brief Boolean true value */
#define LS_TRUE        (1)
/** @brief Boolean false value */
#define LS_FALSE       (0)
/** @brief Helper macro to extract filename from full path */
#define LS_FILENAME    (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 :  \
                         strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
/**
 * @brief Macro to create a magic number from 4 characters
 *
 * Creates a 32-bit magic number from four 8-bit characters for validation.
 */
#define MAKE_MAGIC_NUMBER(a, b, c, d)                                                  \
        (                                                                              \
          (((long)(a)) << 24)                                                          \
          | (((long)(b)) << 16)                                                        \
          | (((long)(c)) << 8)                                                         \
          | (((long)(d)) << 0)                                                         \
        )

#ifndef MIN
/** @brief Return the minimum of two values */
#define MIN(a, b)    (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
/** @brief Return the maximum of two values */
#define MAX(a, b)    (((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN3
/** @brief Return the minimum of three values */
#define MIN3(x, y, z)    ((y) <= (z) ?                                                 \
                          ((x) <= (y) ? (x) : (y))                                     \
                          :                                                            \
                          ((x) <= (z) ? (x) : (z)))
#endif


#ifndef MAX3
/** @brief Return the maximum of three values */
#define MAX3(x, y, z)    ((y) >= (z) ?                                                 \
                          ((x) >= (y) ? (x) : (y))                                     \
                          :                                                            \
                          ((x) >= (z) ? (x) : (z)))
#endif


#ifndef SIGN
/** @brief Return the sign of a value (-1, 0, or 1) */
#define SIGN(x)    (((x) < 0) ? -1 : (((x) > 0) ? 1 : 0))
#endif

#ifndef ABS
/** @brief Return the absolute value of a number */
#define ABS(x)    ((x) > 0 ? (x) : -(x))
#endif

#ifndef CLAMP
/** @brief Clamp a value between min and max */
#define CLAMP(x, min, max)    ((x) < (min) ? (min) : (x) > (max) ? (max) : (x))
#endif
/*-----------------------------------------------------------------------------
 * Color Conversion Macros
 *---------------------------------------------------------------------------*/
/** @brief YCbCr to RGB conversion (ITU-R standard) */
#define YCBCR2RGB(y, cb, cr, r, g, b)                                                \
        do {                                                                         \
          int32_t yVal = (y) - 16;                                                   \
          int32_t cbVal = (cb) - 128;                                                \
          int32_t crVal = (cr) - 128;                                                \
                                                                                     \
          int32_t rVal = (298 * yVal + 0 * cbVal + 409 * crVal) >> 8;                \
          int32_t gVal = (298 * yVal - 100 * cbVal - 208 * crVal) >> 8;              \
          int32_t bVal = (298 * yVal + 516 * cbVal + 0 * crVal) >> 8;                \
                                                                                     \
          (r) = CLAMP(rVal, 0, 255);                                                 \
          (g) = CLAMP(gVal, 0, 255);                                                 \
          (b) = CLAMP(bVal, 0, 255);                                                 \
        } while (0)
/** @brief RGB to YCbCr conversion (ITU-R standard) */
#define RGB2YCBCR(r, g, b, y, cb, cr)                                                \
        do {                                                                         \
          int32_t rVal = (r);                                                        \
          int32_t gVal = (g);                                                        \
          int32_t bVal = (b);                                                        \
                                                                                     \
          (y) = (66 * rVal + 129 * gVal + 25 * bVal + 16 * 256) >> 8;                \
          (cb) = (-38 * rVal - 74 * gVal + 112 * bVal + 128 * 256) >> 8;             \
          (cr) = (112 * rVal - 94 * gVal - 18 * bVal + 128 * 256) >> 8;              \
        } while (0)
/*-----------------------------------------------------------------------------
 * Debug and Logging Macros
 *---------------------------------------------------------------------------*/
/** @brief Debug check/assert macro */
#define DEBUG_CHECK(value)                                                         \
        do                                                                         \
        {                                                                          \
          if ((value) == 0)                                                        \
            {                                                                      \
              LS_Printf(LS_VERB_ERROR,                                             \
                        "Assert: failed in %s:%d:%s().\n",                         \
                        __FILE__,                                                  \
                        __LINE__,                                                  \
                        __func__);                                                 \
            }                                                                      \
        } while (0)

/* Disable variadic macro warnings for logging macros (GNU extension) */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvariadic-macros"

/** @brief Log error message */
#define LS_ERROR(format...)                                                       \
        do                                                                        \
        {                                                                         \
          LS_Printf(LS_VERB_ERROR,                                                \
                    "[%s][LS_ERROR]%s, line %d --> <%s>:",                        \
                    LS_GetTimeStamp(),                                            \
                    LS_FILENAME,                                                  \
                    __LINE__,                                                     \
                    __func__);                                                    \
          LS_Printf(LS_VERB_ERROR, ## format);                                    \
        } while (0)
/** @brief Log warning message */
#define LS_WARNING(format...)                                                     \
        do                                                                        \
        {                                                                         \
          LS_Printf(LS_VERB_WARNING,                                              \
                    "[%s][LS_WARNING]%s, line %d --> <%s>:",                      \
                    LS_GetTimeStamp(),                                            \
                    LS_FILENAME,                                                  \
                    __LINE__,                                                     \
                    __func__);                                                    \
          LS_Printf(LS_VERB_WARNING, ## format);                                  \
        } while (0)
/** @brief Log info message */
#define LS_INFO(format...)                                                        \
        do                                                                        \
        {                                                                         \
          LS_Printf(LS_VERB_INFO,                                                 \
                    "[%s][LS_INFO]%s, line %d --> <%s>:",                         \
                    LS_GetTimeStamp(),                                            \
                    LS_FILENAME,                                                  \
                    __LINE__,                                                     \
                    __func__);                                                    \
          LS_Printf(LS_VERB_INFO, ## format);                                     \
        } while (0)
/** @brief Log debug message */
#define LS_DEBUG(format...)                                                       \
        do                                                                        \
        {                                                                         \
          LS_Printf(LS_VERB_DEBUG,                                                \
                    "[%s][LS_DEBUG]%s, line %d --> <%s>:",                        \
                    LS_GetTimeStamp(),                                            \
                    LS_FILENAME,                                                  \
                    __LINE__,                                                     \
                    __func__);                                                    \
          LS_Printf(LS_VERB_DEBUG, ## format);                                    \
        } while (0)
/** @brief Log trace message */
#define LS_TRACE(format...)                                                       \
        do                                                                        \
        {                                                                         \
          LS_Printf(LS_VERB_TRACE,                                                \
                    "[%s][LS_TRACE]%s, line %d --> <%s>:",                        \
                    LS_GetTimeStamp(),                                            \
                    LS_FILENAME,                                                  \
                    __LINE__,                                                     \
                    __func__);                                                    \
          LS_Printf(LS_VERB_TRACE, ## format);                                    \
        } while (0)
/** @brief Log function entry */
#define LS_ENTER(format...)                                                       \
        do                                                                        \
        {                                                                         \
          LS_Printf(LS_VERB_TRACE,                                                \
                    "[%s][LS_ENTER]%s, line %d --> <%s>:",                        \
                    LS_GetTimeStamp(),                                            \
                    LS_FILENAME,                                                  \
                    __LINE__,                                                     \
                    __func__);                                                    \
          LS_Printf(LS_VERB_TRACE, ## format);                                    \
        } while (0)
/** @brief Log function exit */
#define LS_LEAVE(format...)                                                       \
        do                                                                        \
        {                                                                         \
          LS_Printf(LS_VERB_TRACE,                                                \
                    "[%s][LS_LEAVE]%s, line %d --> <%s>:",                        \
                    LS_GetTimeStamp(),                                            \
                    LS_FILENAME,                                                  \
                    __LINE__,                                                     \
                    __func__);                                                    \
          LS_Printf(LS_VERB_TRACE, ## format);                                    \
        } while (0)

/* Restore diagnostic settings */
#pragma GCC diagnostic pop

/*-----------------------------------------------------------------------------
 * Magic Number Definitions
 *---------------------------------------------------------------------------*/
/** @brief Service magic number ('SRVC') */
#define SERVICE_MAGIC_NUMBER        MAKE_MAGIC_NUMBER('S', 'R', 'V', 'C')
/** @brief Display set magic number ('DSET') */
#define DISPLAYSET_MAGIC_NUMBER     MAKE_MAGIC_NUMBER('D', 'S', 'E', 'T')
/** @brief Display page magic number ('PAGE') */
#define DISPLAYPAGE_MAGIC_NUMBER    MAKE_MAGIC_NUMBER('P', 'A', 'G', 'E')
/** @brief Validate service magic ID */
#define ValidateServiceMagicID(id)       ((id) == SERVICE_MAGIC_NUMBER)
/** @brief Validate display set magic ID */
#define ValidateDisplaysetMagicID(id)    ((id) == DISPLAYSET_MAGIC_NUMBER)

#ifdef __cplusplus
}
#endif

#endif
