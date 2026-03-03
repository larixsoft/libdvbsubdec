/*-----------------------------------------------------------------------------
 * lssubsegdec.h
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
 * @file lssubsegdec.h
 * @brief Segment Decoding Functions
 *
 * This header provides functions for decoding DVB subtitle segments
 * including PCS, RCS, CDS, ODS, and DDS segments.
 */

#ifndef __LS_SEG_DECODER_H__
#define __LS_SEG_DECODER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "lssubdecoder.h"
#include "lssubsegparser.h"
#include "lssubmacros.h"
#include "lssuben300743.h"
/*---------------------------------------------------------------------------
 * typedefines
 * -------------------------------------------------------------------------*/
/** @brief Display region structure */
typedef struct _LS_DisplayRegion  LS_DisplayRegion;
/** @brief ODS map tables structure */
typedef struct _LS_ODSMapTables   LS_ODSMapTables;
/** @brief ODS decoding cookie/context structure */
typedef struct _LS_ODSCookie      LS_ODSCookie;

/*---------------------------------------------------------------------------
 * struct
 * -------------------------------------------------------------------------*/
/**
 * @brief Display region structure
 *
 * Represents a subtitle display region with its CLUT, pixmap, and properties.
 */
struct _LS_DisplayRegion
{
  LS_DisplayPage* page;                 /**< Associated display page            */
  int32_t         region_id;            /**< Region ID                         */
  int32_t         version;              /**< Region version                    */
  int32_t         page_id;              /**< Page ID                           */
  int32_t         region_visible;       /**< Region visibility flag            */
  uint8_t         clut_id;              /**< CLUT ID                           */
  uint8_t         region_depth;         /**< Region depth (2, 4, or 8 bits)    */
  uint8_t         max_clut_entry;       /**< Maximum CLUT entry                */
  LS_ColorRGB_t   clut[256];            /**< Color lookup table (256 entries)   */
  LS_ColorRGB_t   forground_color;      /**< Foreground color                  */
  LS_ColorRGB_t   background_color;     /**< Background color                  */
  uint16_t        character_code[256];  /**< Character codes (for text subtitles) */
  LS_Pixmap_t*    pixmap;               /**< Associated pixmap                 */
  LS_PixelFormat  OSDPixmapFormat;      /**< OSD pixel format                  */
  int8_t          disparity_shift;      /**< Disparity shift for 3D subtitles  */
};


/**
 * @brief ODS map tables structure
 *
 * Contains pixel mapping tables for different bit depth conversions.
 */
struct _LS_ODSMapTables
{
  uint8_t map_table_2_to_4_bit[4];  /**< 2 to 4-bit mapping table */
  uint8_t map_table_2_to_8_bit[4];  /**< 2 to 8-bit mapping table */
  uint8_t map_table_4_to_8_bit[16]; /**< 4 to 8-bit mapping table */
};


/**
 * @brief ODS cookie structure
 *
 * Context structure used during ODS pixel data decoding.
 */
struct _LS_ODSCookie
{
  uint8_t*        data;                      /**< Pixel data buffer          */
  int32_t         x_orig;                    /**< Original X position        */
  int32_t         y_orig;                    /**< Original Y position        */
  int32_t         x_max;                     /**< Maximum X position         */
  int32_t         y_max;                     /**< Maximum Y position         */
  int32_t         pitch;                     /**< Pitch/stride               */
  int32_t         x_offset;                  /**< X offset                   */
  int32_t         y_offset;                  /**< Y offset                   */
  int32_t         depth;                     /**< Depth in bits              */
  LS_ODSMapTables map_table;                 /**< Map tables                 */
  LS_PixelFormat  OSDPixmapFormat;           /**< OSD pixel format           */
  uint8_t         region_depth;              /**< Region depth               */
  int             non_modifying_colour_flag; /**< Non-modifying color flag   */
};


/*---------------------------------------------------------------------------
 * APIs
 * --------------------------------------------------------------------------*/
/**
 * @brief Create a new display page
 *
 * Allocates and initializes a new display page structure.
 *
 * @param service Service instance
 * @param errorCode Pointer to receive error code
 * @return New display page or NULL on failure
 */
LS_DisplayPage* LS_DisplayPageNew(LS_Service* service, int32_t* errorCode);

/**
 * @brief Delete a display page
 *
 * Releases resources associated with a display page.
 *
 * @param service Service instance
 * @param page Display page to delete
 */
void LS_DisplayPageDelete(LS_Service* service, LS_DisplayPage* page);

/**
 * @brief Create a new display region
 *
 * Allocates and initializes a new display region structure.
 *
 * @param service Service instance
 * @return New display region or NULL on failure
 */
LS_DisplayRegion* LS_DisplayRegionNew(LS_Service* service);

/**
 * @brief Delete a display region
 *
 * Releases resources associated with a display region.
 *
 * @param service Service instance
 * @param region Display region to delete
 */
void LS_DisplayRegionDelete(LS_Service* service, LS_DisplayRegion* region);

/**
 * @brief Decode RCS segment
 *
 * Decodes a Region Composition Segment into a display region.
 *
 * @param displayset Display set containing the RCS
 * @param region Display region to decode into
 * @param allocate_pixmap Flag to allocate pixmap
 * @return LS_OK (1) on success, error code on failure
 */
int32_t LS_DisplaysetDecodeRCS(LS_Displayset* displayset, LS_DisplayRegion* region, int32_t allocate_pixmap);

/**
 * @brief Decode ODS bitmap data
 *
 * Decodes bitmap data from an Object Data Segment.
 *
 * @param displayset Display set containing the ODS
 * @param region Display region to render to
 * @param obj_info Object information
 * @return LS_OK (1) on success, error code on failure
 */
int32_t LS_DisplaysetDecodeODSBitmap(LS_Displayset* displayset, LS_DisplayRegion* region, RCSObjectInfo* obj_info);

/**
 * @brief Decode ODS character data
 *
 * Decodes character data from an Object Data Segment.
 *
 * @param displayset Display set containing the ODS
 * @param region Display region to render to
 * @param obj_info Object information
 * @return LS_OK (1) on success, error code on failure
 */
int32_t LS_DisplaysetDecodeODSCharacters(LS_Displayset* displayset, LS_DisplayRegion* region, RCSObjectInfo* obj_info);

/**
 * @brief Decode CDS segment
 *
 * Decodes a CLUT Definition Segment into a display region.
 *
 * @param cds CDS segment to decode
 * @param region Display region to update
 * @return LS_OK (1) on success, error code on failure
 */
int32_t LS_DisplaysetDecodeCDS(LS_SegCDS* cds, LS_DisplayRegion* region);

/**
 * @brief Decode DDS segment
 *
 * Decodes a Display Definition Segment.
 *
 * @param dds DDS segment to decode
 * @param page Display page to update
 * @return LS_OK (1) on success, error code on failure
 */
int32_t LS_DisplaysetDecodeDDS(LS_SegDDS* dds, LS_DisplayPage* page);

/**
 * @brief Decode DSS segment for 3D subtitles
 *
 * Decodes a Disparity Signalling Segment and applies disparity shifts
 * to regions for stereoscopic (3D) subtitle display.
 *
 * @param displayset Display set containing the DSS
 * @param page Display page to update with disparity information
 * @return LS_OK (1) on success, error code on failure
 */
int32_t LS_DisplaysetDecodeDSS(LS_Displayset* displayset, LS_DisplayPage* page);

/**
 * @brief Find region by ID
 *
 * Searches a display page for a region with the specified ID.
 *
 * @param page Display page to search
 * @param region_id Region ID to find
 * @return Display region or NULL if not found
 */
LS_DisplayRegion* LS_DisplaypageFindRegionByID(LS_DisplayPage* page, uint16_t region_id);


#ifdef __cplusplus
}
#endif


#endif
