/*-----------------------------------------------------------------------------
 * lssubutils.h
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
 * @file lssubutils.h
 * @brief Utility Functions
 *
 * This header provides utility functions for bitstream reading,
 * color conversion, and PTS formatting.
 */

#ifndef __LS_UTILS_H__
#define __LS_UTILS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "lssubport.h"
/**
 * @brief Bit stream type enumeration
 *
 * Defines different bit stream encoding types.
 */
typedef enum
{
  DATA_BSLBF   = 0, /**< Bit string, left bit first */
  DATA_UIMSBF  = 1, /**< Unsigned integer, most significant bit first */
  DATA_TCIMSBF = 2, /**< Two's complement integer, MSB (sign) bit first */
} LS_BitsType;
/**
 * @brief Read bits from a bitstream
 *
 * Reads a specified number of bits from a data buffer.
 *
 * @param data Data buffer
 * @param bytes_offset Byte offset to start reading
 * @param bits_offset Bit offset within byte
 * @param number_bits Number of bits to read
 * @param status Pointer to receive status
 * @return Value read from bitstream
 */
uint32_t ReadBitStream32(uint8_t* data,
                         uint32_t bytes_offset,
                         uint32_t bits_offset,
                         uint32_t number_bits,
                         int32_t* status);

/**
 * @brief Convert RGB to HSV
 *
 * Converts a color from RGB to HSV color space.
 *
 * @param rgb Source RGB color
 * @param hsv Destination HSV color
 */
void RGBtoHSV(const LS_ColorRGB_t* rgb, LS_ColorHSV_t* hsv);

/**
 * @brief Convert HSV to RGB
 *
 * Converts a color from HSV to RGB color space.
 *
 * @param hsv Source HSV color
 * @param rgb Destination RGB color
 */
void HSVtoRGB(const LS_ColorHSV_t* hsv, LS_ColorRGB_t* rgb);

/**
 * @brief Convert PTS to HMS string
 *
 * Converts a PTS value to a formatted time string (HH:MM:SS).
 *
 * @param pts PTS value
 * @return Formatted time string (static buffer)
 */
char* PTStoHMS(const uint64_t pts);

#ifdef __cplusplus
}
#endif


#endif
