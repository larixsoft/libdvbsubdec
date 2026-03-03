/*-----------------------------------------------------------------------------
 * lssubgfx.h
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
 * @file lssubgfx.h
 * @brief Graphics Operations
 *
 * This header provides functions for basic graphics operations
 * such as filling regions with color.
 */

#ifndef __LS_GFX_H__
#define __LS_GFX_H__

#ifdef __cplusplus
extern "C" {
#endif

#include  "lssubdec.h"
#include  "lssubmacros.h"
#include  "lssubport.h"
/*----------------------------------------------------------------------------------
 * typedefines
 *---------------------------------------------------------------------------------\*/
/*----------------------------------------------------------------------------------
 * struct
 *---------------------------------------------------------------------------------\*/
/*----------------------------------------------------------------------------------
 * APIs
 *---------------------------------------------------------------------------------\*/
/**
 * @brief Fill region with ARGB32 color
 *
 * Fills a rectangular region with the specified ARGB color.
 *
 * @param width Region width
 * @param height Region height
 * @param data Data buffer to fill
 * @param color RGB color to use
 */
void LS_FillRegionColorARGB32(uint32_t width, uint32_t height, uint8_t* data, LS_ColorRGB_t* color);

/**
 * @brief Fill region with AYUV32 color
 *
 * Fills a rectangular region with the specified AYUV color.
 *
 * @param width Region width
 * @param height Region height
 * @param data Data buffer to fill
 * @param color YUV color to use
 */
void LS_FillRegionColorAYUV32(uint32_t width, uint32_t height, uint8_t* data, LS_ColorYUV_t* color);


#ifdef __cplusplus
}
#endif


#endif
