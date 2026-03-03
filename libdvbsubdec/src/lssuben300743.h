/*-----------------------------------------------------------------------------
 * lssuben300743.h
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
 * @file lssuben300743.h
 * @brief DVB Subtitle ETSI EN 300 743 Standard Definitions
 *
 * This header defines data structures, constants, and functions according to
 * the ETSI EN 300 743 standard for DVB subtitling systems.
 *
 * @details The standard defines:
 * - Subtitle segment types (PCS, RCS, CDS, ODS, DDS, EDS, DSS)
 * - Page and region composition
 * - Color lookup tables (CLUT)
 * - Object data encoding
 * - Display definition segments
 * - Disparity signalling for 3D
 */

#ifndef __LS_EN300743_H__
#define __LS_EN300743_H__


#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stddef.h>
#include "lssubmacros.h"
#include "lssubport.h"
#include "lslist.h"
/*---------------------------------------------------------------------------
 * Binary numbers
 *--------------------------------------------------------------------------*/
/**
 * @brief Binary number enumeration
 *
 * Defines values for 4-bit binary patterns.
 */
typedef enum
{
  kB0000 = 0x00,
  kB0001 = 0x01,
  kB0010 = 0x02,
  kB0011 = 0x03,
  kB0100 = 0x04,
  kB0101 = 0x05,
  kB0110 = 0x06,
  kB0111 = 0x07,
  kB1000 = 0x08,
  kB1001 = 0x09,
  kB1010 = 0x0A,
  kB1011 = 0x0B,
  kB1100 = 0x0C,
  kB1101 = 0x0D,
  kB1110 = 0x0E,
  kB1111 = 0x0F,
} BinaryNumber;
/**
 * @brief Standard DVB subtitle resolution
 *
 * Defines the default subtitle display resolution per EN 300 743.
 */
typedef enum
{
  kEN300743_DVB_SUBTITLE_WIDTH  = 720, /**< Standard DVB subtitle width (720 pixels)  */
  kEN300743_DVB_SUBTITLE_HEIGHT = 576, /**< Standard DVB subtitle height (576 pixels) */
} EN300743SubtitleResolution;
/**
 * @brief Buffer size constants
 *
 * Defines recommended buffer sizes per EN 300 743.
 */
typedef enum
{
  kEN300743_CODED_DATA_BUFFER_B2_SIZE = 100 * 1024,  /**< Coded data buffer size (100 KB) */
  kEN300743_PIXEL_BUFFER_B3_SIZE      = 320 * 1024,  /**< Pixel buffer size (320 KB)      */
  kEN300743_COMPOSITION_BUFFER_SIZE   = 4 * 1024,    /**< Composition buffer size (4 KB)   */
} EN300743MemorySize;
/**
 * @brief PES packet constants
 *
 * Defines offsets and identifiers for PES packet parsing.
 */
typedef enum
{
  kPES_packet_length_OFFSET       = 4,     /**< PES packet length offset from start */
  kSIZE_OF_PES_packet_length      = 2,     /**< Size of PES packet length field      */
  kPES_header_data_length_OFFSET  = 8,     /**< PES header data length offset        */
  kSIZE_OF_PES_header_data_length = 1,     /**< Size of PES header data length field */
  kPRIVATE_STREAM_1_STREAM_ID     = 0xBD,  /**< Private Stream 1 stream ID           */
  kPES_DATA_IDENTIFIER            = 0x20,  /**< PES data identifier                  */
  kPES_DATA_IDENTIFIER_ALT        = 0x0F,  /**< Alternative PES data identifier      */
  kPES_SUBTITLE_STREAM_ID         = 0x00,  /**< Subtitle stream ID                   */
  kEND_OF_PES_DATA_FIELD_MARKER   = 0xFF,  /**< End of PES data field marker         */
  kSUBTITLING_SEGMENT_SYNC_BYTE   = 0x0F,  /**< Subtitling segment sync byte         */
} PesCont;
/**
 * @brief Segment type enumeration
 *
 * Defines the different DVB subtitle segment types.
 */
typedef enum
{
  kSEGMENT_DATA_FIELD_OFFSET    = 6,       /**< Segment data field offset        */
  kPAGE_COMPOSITION_SEGMENT     = 0x10,    /**< Page Composition Segment         */
  kREGION_COMPOSITION_SEGMENT   = 0x11,    /**< Region Composition Segment       */
  kCLUT_DEFINITION_SEGMENT      = 0x12,    /**< CLUT Definition Segment          */
  kOBJECT_DATA_SEGMENT          = 0x13,    /**< Object Data Segment              */
  kDISPLAY_DEFINITION_SEGMENT   = 0x14,    /**< Display Definition Segment       */
  kDISPARITY_SIGNALLING_SEGMENT = 0x15,    /**< Disparity Signalling Segment     */
  kEND_OF_DISPLAY_SET_SEGMENT   = 0x80,    /**< End of Display Set Segment       */
} SegmentType;
/**
 * @brief ODS object coding method enumeration
 *
 * Defines how object data is encoded in ODS.
 */
typedef enum
{
  kCODING_OF_PIXELS                = 0x00,      /**< Coded as pixels                */
  kCODED_AS_A_STRING_OF_CHARACTERS = 0x01,      /**< Coded as character string      */
} ODSObjectCodingMethod;
/**
 * @brief Pixel data sub-block type enumeration
 *
 * Defines the different types of pixel data sub-blocks.
 */
typedef enum
{
  kEND_OF_STRING_SIGNAL      = 0x00, /**< End of string signal      */
  kCODE_STRING_2BIT_PIXEL    = 0x10, /**< 2-bit pixel code string   */
  kCODE_STRING_4BIT_PIXEL    = 0x11, /**< 4-bit pixel code string   */
  kCODE_STRING_8BIT_PIXEL    = 0x12, /**< 8-bit pixel code string   */
  kMAP_TABLE_DATA_2_TO_4_bit = 0x20, /**< 2 to 4-bit map table      */
  kMAP_TABLE_DATA_2_TO_8_bit = 0x21, /**< 2 to 8-bit map table      */
  kMAP_TABLE_DATA_4_TO_8_bit = 0x22, /**< 4 to 8-bit map table      */
  kEND_OF_OBJECT_LINE_CODE   = 0xF0, /**< End of object line code   */
} PixelDataSubBlockDataType;
/**
 * @brief PCS page state enumeration
 *
 * Defines the state of the page in PCS.
 */
typedef enum
{
  kPCS_PAGE_STATE_NORMAL_CASE       = 0x00, /**< Normal state        */
  kPCS_PAGE_STATE_ACQUISITION_POINT = 0x01, /**< Acquisition point    */
  kPCS_PAGE_STATE_MODE_CHANGE       = 0x02, /**< Mode change          */
  kPCS_PAGE_STATE_RESERVED          = 0x03, /**< Reserved             */
} PCSPageState;
/**
 * @brief RCS region depth enumeration
 *
 * Defines the pixel depth of a region.
 */
typedef enum
{
  kREGION_DEPTH_2_BIT = 0x01, /**< 2-bit depth (4 colors)    */
  kREGION_DEPTH_4_BIT = 0x02, /**< 4-bit depth (16 colors)   */
  kREGION_DEPTH_8_BIT = 0x03, /**< 8-bit depth (256 colors)  */
} RCSRegionDepth;
/**
 * @brief RCS object type enumeration
 *
 * Defines the type of object in a region.
 */
typedef enum
{
  kBASIC_OBJECT_BITMAP                   = 0x00,    /**< Basic bitmap object                 */
  kBASIC_OBJECT_CHARACTER                = 0x01,    /**< Basic character object              */
  kCOMPOSITE_OBJECT_STRING_OF_CHARACTERS = 0x02,    /**< Composite string of characters      */
} RCSObjectType;

extern uint8_t kDefault2To4BitMapTable[4];
extern uint8_t kDefault2To8BitMapTable[4];
extern uint8_t kDefault4To8BitMapTable[16];
extern uint8_t kDefault4To2BitMapTable[16];
extern uint8_t kDefault8To2BitMapTable[256];
extern uint8_t kDefault8To4BitMapTable[256];
extern LS_ColorYUV_t kDefaultAYUV256CLUT[256];
extern LS_ColorRGB_t kDefaultARGB256CLUT[256];
extern LS_ColorYUV_t kDefaultAYUV16CLUT[16];
extern LS_ColorRGB_t kDefaultARGB16CLUT[16];
extern LS_ColorYUV_t kDefaultAYUV4CLUT[4];
extern LS_ColorRGB_t kDefaultARGB4CLUT[4];

/*---------------------------------------------------------------------------
 * typedefs
 *--------------------------------------------------------------------------*/
typedef struct _LS_Displayset                    LS_Displayset;
typedef struct _LS_SegHeader                     LS_SegHeader;
typedef struct _LS_SegDDS                        LS_SegDDS;
typedef struct _PCSRegionInfo                    PCSRegionInfo;
typedef struct _LS_SegPCS                        LS_SegPCS;
typedef struct _RCSObjectInfo                    RCSObjectInfo;
typedef struct _LS_SegRCS                        LS_SegRCS;
typedef struct _CDSCLUTInfo                      CDSCLUTInfo;
typedef struct _LS_SegCDS                        LS_SegCDS;
typedef struct _LS_SegODS                        LS_SegODS;
typedef struct _LS_SegEDS                        LS_SegEDS;
typedef struct _LS_SegDSS                        LS_SegDSS;
typedef struct _ODSPixelData                     ODSPixelData;
typedef struct _ODSPixelDataSubBlock             ODSPixelDataSubBlock;
typedef struct _ODSStringData                    ODSStringData;
typedef struct _DSSDisparityShiftUpdateSequence  DSSDisparityShiftUpdateSequence;
typedef struct _DSSRegionInfo                    DSSRegionInfo;
typedef struct _DSSSubRegionInfo                 DSSSubRegionInfo;
typedef struct _DSSDivisionPeriod                DSSDivisionPeriod;

/*---------------------------------------------------------------------------
 * struct
 *--------------------------------------------------------------------------*/
struct _LS_Displayset
{
  uint32_t   magic_id;
  LS_SegDDS* dds;
  LS_SegPCS* pcs;
  LS_List*   rcs;
  LS_List*   cds;
  LS_List*   ods;
  LS_SegEDS* eds;
  LS_SegDSS* dss;
  long long  pts;
};
struct _LS_SegHeader
{
  uint8_t  sync_byte;
  uint8_t  segment_type;
  uint16_t page_id;
  uint16_t segment_length;
  uint8_t  stuff[2];
};


/*display_definition_segment*/
struct _LS_SegDDS
{
  uint8_t  sync_byte;
  uint8_t  segment_type;
  uint16_t page_id;
  uint16_t segment_length;
  uint8_t  stuff[2];
  uint8_t  dds_version_number;
  uint8_t  display_window_flag;
  uint8_t  reserved;
  uint8_t  stuff2;
  uint16_t display_width;
  uint16_t display_height;
  uint16_t display_window_horizontal_position_minimum;
  uint16_t display_window_horizontal_position_maximum;
  uint16_t display_window_vertical_position_minimum;
  uint16_t display_window_vertical_position_maximum;
};
struct _PCSRegionInfo
{
  uint8_t  region_id;
  uint8_t  reserved;
  uint16_t region_horizontal_address;
  uint16_t region_vertical_address;
};


/*page_composition_segment*/
struct _LS_SegPCS
{
  uint8_t  sync_byte;
  uint8_t  segment_type;
  uint16_t page_id;
  uint16_t segment_length;
  uint8_t  stuff[2];
  uint8_t  page_time_out;
  uint8_t  page_version_number;
  uint8_t  page_state;
  uint8_t  reserved;
  LS_List* regioninfos;      /*list of _RegionInfo*/
};
struct _RCSObjectInfo
{
  uint16_t object_id;
  uint8_t  object_type;
  uint8_t  object_provider_flag;
  uint16_t object_horizontal_position;
  uint16_t reserved;
  uint16_t object_vertical_position;
  uint8_t  foreground_pixel_code;
  uint8_t  background_pixel_code;
};


/*region_composition_segment*/
struct _LS_SegRCS
{
  uint8_t  sync_byte;
  uint8_t  segment_type;
  uint16_t page_id;
  uint16_t segment_length;
  uint8_t  stuff[2];
  uint8_t  region_id;
  uint8_t  region_version_number;
  uint8_t  region_fill_flag;
  uint8_t  reserved1;
  uint16_t region_width;
  uint16_t region_height;
  uint8_t  region_level_of_compatibility;
  uint8_t  region_depth;
  uint8_t  reserved2;
  uint8_t  CLUT_id;
  uint8_t  region_8bit_pixel_code;
  uint8_t  region_4bit_pixel_code;
  uint8_t  region_2bit_pixel_code;
  uint8_t  reserved3;
  LS_List* objectinfo_list;      /*list of _ObjectInfo*/
};
struct _CDSCLUTInfo
{
  uint8_t  CLUT_entry_id;
  uint8_t  two_bit_entry_CLUT_flag;
  uint8_t  four_bit_entry_CLUT_flag;
  uint8_t  eight_bit_entry_CLUT_flag;
  uint16_t reserved;
  uint16_t full_range_flag;
  uint8_t  Y_value;
  uint8_t  Cr_value;
  uint8_t  Cb_value;
  uint8_t  T_value;
};


/*CLUT_definition_segment*/
struct _LS_SegCDS
{
  uint8_t  sync_byte;
  uint8_t  segment_type;
  uint16_t page_id;
  uint16_t segment_length;
  uint8_t  stuff[2];
  uint8_t  CLUT_id;
  uint8_t  CLUT_version_number;
  uint16_t reserved;
  LS_List* clutinfo_list;       /*list of _CDSCLUTInfo*/
};
struct _ODSPixelDataSubBlock
{
  uint8_t data_type;
  union
  {
    uint8_t* code_string_2bit_pixel;
    uint8_t* code_string_4bit_pixel;
    uint8_t* code_string_8bit_pixel;
    uint8_t* map_table_2_to_4;
    uint8_t* map_table_2_to_8;
    uint8_t* map_table_4_to_8;
  } data;
};
struct _ODSPixelData
{
  uint16_t top_field_data_block_length;
  uint16_t bottom_field_data_block_length;
  uint8_t* top_pixel_data_sub_block;
  uint8_t* bottom_pixel_data_sub_block;
};
struct _ODSStringData
{
  uint8_t  number_of_codes;
  uint8_t* character_code;
};


/*object data segment*/
struct _LS_SegODS
{
  uint8_t  sync_byte;
  uint8_t  segment_type;
  uint16_t page_id;
  uint16_t segment_length;
  uint8_t  stuff[2];
  uint16_t object_id;
  uint8_t  object_version_number;
  uint8_t  object_coding_method;
  uint8_t  non_modifying_colour_flag;
  uint8_t  reserved;
  union
  {
    ODSPixelData*  pixeldata;
    ODSStringData* stringdata;
  } data;
};


/*end of display set segment*/
struct _LS_SegEDS
{
  uint8_t  sync_byte;
  uint8_t  segment_type;
  uint16_t page_id;
  uint16_t segment_length;
  uint8_t  stuff[2];
};
struct _DSSDivisionPeriod
{
  uint8_t interval_count;
  uint8_t disparity_shift_update_integer_part;
  uint8_t stuff[2];
};
struct _DSSDisparityShiftUpdateSequence
{
  uint32_t interval_duration;
  uint8_t  disparity_shift_update_sequence_length;
  uint8_t  division_period_count;
  uint8_t  stuff[2];
  LS_List* division_periods;  /*list of _DDSDivisionPeriod*/
};
struct _DSSSubRegionInfo
{
  uint16_t                         subregion_horizontal_position;
  uint16_t                         subregion_width;
  uint8_t                          subregion_disparity_shift_integer_part;
  uint8_t                          subregion_disparity_shift_fractional_part;
  uint8_t                          reserved;
  uint8_t                          stuff;
  DSSDisparityShiftUpdateSequence* disparity_shift_update_sequence;
};
struct _DSSRegionInfo
{
  uint8_t  region_id;
  uint8_t  disparity_shift_update_sequence_region_flag;
  uint8_t  reserved;
  uint8_t  number_of_subregions_minus_1;
  LS_List* sub_region_info;  /*list of _DSSSubRegionInfo*/
};
struct _LS_SegDSS
{
  uint8_t                          sync_byte;
  uint8_t                          segment_type;
  uint16_t                         page_id;
  uint16_t                         segment_length;
  uint8_t                          dss_version_number;
  uint8_t                          disparity_shift_update_sequence_page_flag;
  uint8_t                          reserved;
  uint8_t                          page_default_disparity_shift;
  uint8_t                          stuff[2];
  DSSDisparityShiftUpdateSequence* disparity_shift_update_sequence;
  LS_List*                         regions; /*list of _DSSRegionInfo*/
};


int32_t TranslatePixelCode4CLUT(uint8_t        pixel_code,
                                uint8_t        compatibility,
                                uint8_t        depth,
                                LS_ColorYUV_t* CLUT4,
                                uint8_t*       maptable4to2bit,
                                uint8_t*       maptable8to2bit,
                                LS_ColorYUV_t* color);
int32_t TranslatePixelCode16CLUT(uint8_t        pixel_code,
                                 uint8_t        compatibility,
                                 uint8_t        depth,
                                 LS_ColorYUV_t* CLUT16,
                                 uint8_t*       maptable2to4bit,
                                 uint8_t*       maptable8to4bit,
                                 LS_ColorYUV_t* color);
int32_t TranslatePixelCode256CLUT(uint8_t        pixel_code,
                                  uint8_t        compatibility,
                                  uint8_t        depth,
                                  LS_ColorYUV_t* CLUT256,
                                  uint8_t*       maptable2to8bit,
                                  uint8_t*       maptable4to8bit,
                                  LS_ColorYUV_t* color);
void GenerateDefault256CLUT(LS_ColorRGB_t* rgbclut, LS_ColorYUV_t* yuvclut);
void GenerateDefault16CLUT(LS_ColorRGB_t* rgbclut, LS_ColorYUV_t* yuvclut);
void GenerateDefault4CLUT(LS_ColorRGB_t* rgbclut, LS_ColorYUV_t* yuvclut);
void Map4to2bitReduction(uint8_t* map);
void Map8to2bitReduction(uint8_t* map);
void Map8to4bitReduction(uint8_t* map);


#ifdef __cplusplus
}
#endif


#endif /*__LS_EN300743_H__*/
