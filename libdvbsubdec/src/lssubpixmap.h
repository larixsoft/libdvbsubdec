/*-----------------------------------------------------------------------------
 * lssubpixmap.h
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
 * @file lssubpixmap.h
 * @brief Pixmap and Object Data Segment Decoding
 *
 * This header provides functions for decoding DVB object data segments (ODS)
 * and managing pixmap/image data structures.
 */

#ifndef __LS_PIXMAP_H__
#define __LS_PIXMAP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "lssubdec.h"
#include "lssubport.h"
#include "lssubsegparser.h"
#include "lssubsegdec.h"
/**
 * @brief Decode ODS pixel data sub-block
 *
 * Decodes a pixel data sub-block from an Object Data Segment.
 *
 * @param subblock Sub-block data buffer
 * @param data_length Length of data in buffer
 * @param non_modifying_colour_flag Non-modifying color flag
 * @param region Display region to render to
 * @param obj_info Object information
 * @param top_field Top field flag
 * @param processed_bytes Pointer to receive bytes processed
 * @return LS_OK (1) on success, error code on failure
 */
int32_t LS_ODSDecodePixelDataSubBlock(uint8_t*          subblock,
                                      uint32_t          data_length,
                                      int               non_modifying_colour_flag,
                                      LS_DisplayRegion* region,
                                      RCSObjectInfo*    obj_info,
                                      int32_t           top_field,
                                      uint32_t*         processed_bytes);

/**
 * @brief Decode 2-bit pixel code string
 *
 * Decodes a run-length encoded 2-bit pixel string.
 *
 * @param data Input data buffer
 * @param cookie ODS decoding context
 * @param max_length Maximum length to decode
 * @param processed_bytes Pointer to receive bytes processed
 * @return LS_OK (1) on success, error code on failure
 */
int32_t LS_ODSDecode2BitPixelCodeString(uint8_t*      data,
                                        LS_ODSCookie* cookie,
                                        uint32_t      max_length,
                                        uint32_t*     processed_bytes);

/**
 * @brief Decode 4-bit pixel code string
 *
 * Decodes a run-length encoded 4-bit pixel string.
 *
 * @param data Input data buffer
 * @param cookie ODS decoding context
 * @param max_length Maximum length to decode
 * @param processed_bytes Pointer to receive bytes processed
 * @return LS_OK (1) on success, error code on failure
 */
int32_t LS_ODSDecode4BitPixelCodeString(uint8_t*      data,
                                        LS_ODSCookie* cookie,
                                        uint32_t      max_length,
                                        uint32_t*     processed_bytes);

/**
 * @brief Decode 8-bit pixel code string
 *
 * Decodes a run-length encoded 8-bit pixel string.
 *
 * @param data Input data buffer
 * @param cookie ODS decoding context
 * @param max_length Maximum length to decode
 * @param processed_bytes Pointer to receive bytes processed
 * @return LS_OK (1) on success, error code on failure
 */
int32_t LS_ODSDecode8BitPixelCodeString(uint8_t*      data,
                                        LS_ODSCookie* cookie,
                                        uint32_t      max_length,
                                        uint32_t*     processed_bytes);

/**
 * @brief Decode 2 to 4-bit map table
 *
 * Decodes a pixel mapping table for 2-bit to 4-bit conversion.
 *
 * @param data Input data buffer
 * @param cookie ODS decoding context
 * @return LS_OK (1) on success, error code on failure
 */
int32_t LS_ODSDecode2To4BitMapTable(uint8_t* data, LS_ODSCookie* cookie);

/**
 * @brief Decode 2 to 8-bit map table
 *
 * Decodes a pixel mapping table for 2-bit to 8-bit conversion.
 *
 * @param data Input data buffer
 * @param cookie ODS decoding context
 * @return LS_OK (1) on success, error code on failure
 */
int32_t LS_ODSDecode2To8BitMapTable(uint8_t* data, LS_ODSCookie* cookie);

/**
 * @brief Decode 4 to 8-bit map table
 *
 * Decodes a pixel mapping table for 4-bit to 8-bit conversion.
 *
 * @param data Input data buffer
 * @param cookie ODS decoding context
 * @return LS_OK (1) on success, error code on failure
 */
int32_t LS_ODSDecode4To8BitMapTable(uint8_t* data, LS_ODSCookie* cookie);

/**
 * @brief Create a new pixmap
 *
 * Allocates and initializes a new pixmap structure.
 *
 * @param service Service instance
 * @param width Pixmap width in pixels
 * @param height Pixmap height in pixels
 * @param fmt Pixel format
 * @return New pixmap or NULL on failure
 */
LS_Pixmap_t* LS_NewPixmap(LS_Service* service, uint32_t width, uint32_t height, LS_PixelFormat fmt);

/**
 * @brief Delete a pixmap
 *
 * Releases resources associated with a pixmap.
 *
 * @param service Service instance
 * @param pixmap Pixmap to delete
 */
void LS_DeletePixmap(LS_Service* service, LS_Pixmap_t* pixmap);

/**
 * @brief Fill pixmap with a color
 *
 * Fills the entire pixmap with the specified color.
 *
 * @param pixmap Pixmap to fill
 * @param color Color to fill with
 */
void LS_FillPixmapWithColor(LS_Pixmap_t* pixmap, LS_Color_t* color);

#ifdef __cplusplus
}
#endif

#endif /*__LS_PIXMAP_H__*/
