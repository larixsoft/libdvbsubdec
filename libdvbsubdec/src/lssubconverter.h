/*-----------------------------------------------------------------------------
 * lssubconverter.h
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
 * @file lssubconverter.h
 * @brief Color Space and Format Conversion Functions
 *
 * This header provides functions for converting between different color spaces
 * (RGB, YUV, HSV) and pixel formats used in subtitle rendering.
 */

#ifndef __LS_CONVERT_H__
#define __LS_CONVERT_H__


#ifdef __cplusplus
extern "C" {
#endif


#include "lssubmacros.h"
/**
 * @brief Convert RGB color to YUV
 *
 * Converts a color from RGB color space to YUV color space.
 *
 * @param rgb Source RGB color
 * @param yuv Destination YUV color
 */
void RGB2YUV(const LS_ColorRGB_t* rgb, LS_ColorYUV_t* yuv);

/**
 * @brief Convert YUV color to RGB
 *
 * Converts a color from YUV color space to RGB color space.
 *
 * @param yuv Source YUV color
 * @param rgb Destination RGB color
 */
void YUV2RGB(const LS_ColorYUV_t* yuv, LS_ColorRGB_t* rgb);

/**
 * @brief Convert RGB color to HSV
 *
 * Converts a color from RGB color space to HSV color space.
 *
 * @param rgb Source RGB color
 * @param hsv Destination HSV color
 */
void RGB2HSV(const LS_ColorRGB_t* rgb, LS_ColorHSV_t* hsv);

/**
 * @brief Convert HSV color to RGB
 *
 * Converts a color from HSV color space to RGB color space.
 *
 * @param hsv Source HSV color
 * @param rgb Destination RGB color
 */
void HSV2RGB(const LS_ColorHSV_t* hsv, LS_ColorRGB_t* rgb);

/**
 * @brief Convert 8-bit CLUT to ARGB32 format
 *
 * Converts palette-indexed data to 32-bit ARGB format.
 *
 * @param src_data Source palette-indexed data
 * @param data_len Length of source data
 * @param clut Color lookup table (256 entries)
 * @param dest_data Destination ARGB32 buffer
 */
void ConvertCLUT82ARGB32(const uint8_t* src_data, uint32_t data_len, const LS_ColorRGB_t* clut, uint8_t* dest_data);

/**
 * @brief Convert 8-bit CLUT to BGRA32 format
 *
 * Converts palette-indexed data to 32-bit BGRA format.
 *
 * @param src_data Source palette-indexed data
 * @param data_len Length of source data
 * @param clut Color lookup table (256 entries)
 * @param dest_data Destination BGRA32 buffer
 */
void ConvertCLUT82BGRA32(const uint8_t* src_data, uint32_t data_len, const LS_ColorRGB_t* clut, uint8_t* dest_data);

/**
 * @brief Convert generic color to RGB and YUV
 *
 * Converts a color from any format to both RGB and YUV representations.
 *
 * @param color Source color (any format)
 * @param yuv Destination YUV color
 * @param rgb Destination RGB color
 */
void ConvertColor2RGBYUV(const LS_Color_t* color, LS_ColorYUV_t* yuv, LS_ColorRGB_t* rgb);

/**
 * @brief Convert 8-bit CLUT to YUV420P format
 *
 * Converts palette-indexed data to planar YUV420 format.
 *
 * @param src_data Source palette-indexed data
 * @param width Image width
 * @param height Image height
 * @param clut Color lookup table (256 entries)
 * @param dest_data Destination YUV420P buffer
 */
void ConvertCLUT82YUV420P(const uint8_t*       src_data,
                          int32_t              width,
                          int32_t              height,
                          const LS_ColorRGB_t* clut,
                          uint8_t*             dest_data);


#ifdef __cplusplus
}
#endif


#endif /*__LS_CONVERT_H__*/
