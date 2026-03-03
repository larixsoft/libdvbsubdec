/*-----------------------------------------------------------------------------
 * lssubpixmap.c
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

#include "lssubpixmap.h"
#include "lssubdec.h"
#include "lssubmacros.h"
#include "lssubutils.h"
#include "lssuben300743.h"
#include "lssubconverter.h"
/*---------------------------------------------------------------------------
 * local static function declaration
 * -------------------------------------------------------------------------*/
static void initCookie(LS_ODSCookie*     cookie,
                       LS_DisplayRegion* region,
                       RCSObjectInfo*    obj_info,
                       int32_t           top_field,
                       int               non_modifying_colour_flag);
static void    drawPixel(LS_ODSCookie* cookie, uint32_t length, uint8_t pixel);
static void    startNewline(LS_ODSCookie* cookie);
static uint8_t convertPixel(LS_ODSCookie* cookie, uint8_t pixel);

/*---------------------------------------------------------------------------
 * local static functions
 * -------------------------------------------------------------------------*/
static void
initCookie(LS_ODSCookie*     cookie,
           LS_DisplayRegion* region,
           RCSObjectInfo*    obj_info,
           int32_t           top_field,
           int               non_modifying_colour_flag)
{
  SYS_MEMSET(cookie, 0, sizeof(LS_ODSCookie));
  cookie->data = region->pixmap->data;
  cookie->x_max = region->pixmap->width;
  cookie->y_max = region->pixmap->height;
  cookie->x_orig = obj_info->object_horizontal_position;
  cookie->y_orig = obj_info->object_vertical_position;
  cookie->pitch = region->pixmap->bytesPerLine;
  cookie->x_offset = obj_info->object_horizontal_position;
  cookie->y_offset = obj_info->object_vertical_position;
  cookie->OSDPixmapFormat = region->OSDPixmapFormat;
  cookie->region_depth = region->region_depth;
  cookie->non_modifying_colour_flag = non_modifying_colour_flag;
  cookie->y_offset += top_field ? 0 : 1;
  LS_TRACE("cookie->data     = %p\n", (void*)cookie->data);
  LS_TRACE("cookie->x_max    = %d\n", cookie->x_max);
  LS_TRACE("cookie->y_max    = %d\n", cookie->y_max);
  LS_TRACE("cookie->x_orig   = %d\n", cookie->x_orig);
  LS_TRACE("cookie->y_orig   = %d\n", cookie->y_orig);
  LS_TRACE("cookie->OSDPixmapFormat= %d\n", cookie->OSDPixmapFormat);
  LS_TRACE("cookie->region_depth = %d\n", cookie->region_depth);
  LS_TRACE("cookie->pitch    = %d\n", cookie->pitch);
  LS_TRACE("cookie->x_offset = %d\n", cookie->x_offset);
  LS_TRACE("cookie->y_offset = %d\n", cookie->y_offset);
  /*using default map table*/
  SYS_MEMCPY(cookie->map_table.map_table_2_to_4_bit, kDefault2To4BitMapTable, 4);
  SYS_MEMCPY(cookie->map_table.map_table_2_to_8_bit, kDefault2To8BitMapTable, 4);
  SYS_MEMCPY(cookie->map_table.map_table_4_to_8_bit, kDefault4To8BitMapTable, 16);
}


static void
drawPixel(LS_ODSCookie* cookie, uint32_t length, uint8_t pixel)
{
  uint32_t i = 0;
  uint32_t pos = 0;

  LS_TRACE("%d ", length);
  LS_TRACE("x_offset start value = %d\n", cookie->x_offset);

  for (i = 0; i < length; i++)
  {
    if ((cookie->x_offset) >= cookie->x_max)
    {
      LS_WARNING("x_offset Is Out of Range: Request %d, Only Used %d.\n", length, i);
      break;
    }

    pos = (cookie->x_offset) + cookie->y_offset * cookie->pitch;

    if (pos >= (uint32_t)(cookie->y_max * cookie->pitch))
    {
      LS_WARNING("y_offset Is Out of Range: y_offset = %d. " "Request %d,Only Used %d.\n", cookie->y_offset, length, i);
      break;
    }

    if (!((cookie->non_modifying_colour_flag == 1) &&
          (pixel == 1)))
    {
      cookie->data[pos] = convertPixel(cookie, pixel);
    }

#ifdef SIMU_OBJECT_PIXMAP
    printf("%x", pixel);
#endif
    cookie->x_offset += 1;
  }

  LS_TRACE("x_offset end value = %d\n", cookie->x_offset);
}


static void
startNewline(LS_ODSCookie* cookie)
{
  LS_TRACE("startNewline:cookie = %p,cookie->x_offset = %d\n", (void*)cookie, cookie->x_offset);
  cookie->y_offset += 2;
  cookie->x_offset = cookie->x_orig;

#ifdef SIMU_OBJECT_PIXMAP
  printf("\n");
#endif
}


static uint8_t
convertPixel(LS_ODSCookie* cookie, uint8_t pixel)
{
  uint8_t region_depth = 0;

  if (!cookie)
  {
    return pixel;
  }

  region_depth = cookie->region_depth;

  switch (cookie->OSDPixmapFormat)
  {
    case LS_PIXFMT_PALETTE2BIT:

      if (region_depth == 2)
      {
        return pixel;
      }
      else if (region_depth == 4)
      {
        LS_DEBUG("using kDefault4To2BitMapTable now\n");
        return kDefault4To2BitMapTable[pixel];
      }
      else if (region_depth == 8)
      {
        LS_DEBUG("using kDefault8To2BitMapTable now\n");
        return kDefault8To2BitMapTable[pixel];
      }
      else
      {
        LS_WARNING("region_depth %d is not supported\n", region_depth);
        return pixel & 0x3;
      }

      break;

    case LS_PIXFMT_PALETTE4BIT:

      if (region_depth == 2)
      {
        LS_DEBUG("using map_table_2_to_4_bit now\n");
        return cookie->map_table.map_table_2_to_4_bit[pixel];
      }
      else if (region_depth == 4)
      {
        return pixel;
      }
      else if (region_depth == 8)
      {
        LS_DEBUG("using kDefault8To4BitMapTable now\n");
        return kDefault8To4BitMapTable[pixel];
      }
      else
      {
        LS_WARNING("region_depth %d is not supported\n", region_depth);
        return pixel & 0xF;
      }

      break;

    case LS_PIXFMT_PALETTE8BIT:

      if (region_depth == 2)
      {
        LS_DEBUG("using map_table_2_to_8_bit now\n");
        return cookie->map_table.map_table_2_to_8_bit[pixel];
      }
      else if (region_depth == 4)
      {
        LS_DEBUG("using map_table_4_to_8_bit now\n");
        return cookie->map_table.map_table_4_to_8_bit[pixel];
      }
      else if (region_depth == 8)
      {
        return pixel;
      }
      else
      {
        LS_WARNING("region_depth %d is not supported\n", region_depth);
        return pixel & 0xFF;
      }

      break;

    default:
      return pixel & 0xFF;
  }
}


/*---------------------------------------------------------------------------
 * APIs
 * -------------------------------------------------------------------------*/
int32_t
LS_ODSDecodePixelDataSubBlock(uint8_t*          subblock,
                              uint32_t          data_length,
                              int               non_modifying_colour_flag,
                              LS_DisplayRegion* region,
                              RCSObjectInfo*    obj_info,
                              int32_t           top_field,
                              uint32_t*         processed_bytes)
{
  uint32_t bytes = 0;
  uint8_t dataType = 0;
  int32_t status = LS_OK;
  uint32_t processed = 0;
  int32_t dataError = LS_FALSE;
  LS_ODSCookie cookie;

  if ((subblock == NULL) ||
      (region == NULL))
  {
    return LS_ERROR_GENERAL;
  }

  initCookie(&cookie, region, obj_info, top_field, non_modifying_colour_flag);

  while ((bytes < data_length) &&
         (!dataError))
  {
    dataType = ReadBitStream32(subblock, 0 + bytes, 0, 8, &status);
    DEBUG_CHECK(status == LS_OK);
    bytes += 1;

    switch (dataType)
    {
      case kCODE_STRING_2BIT_PIXEL:
        status = LS_ODSDecode2BitPixelCodeString(subblock + bytes, &cookie, data_length - bytes, &processed);
        bytes += processed;
        break;

      case kCODE_STRING_4BIT_PIXEL:
        status = LS_ODSDecode4BitPixelCodeString(subblock + bytes, &cookie, data_length - bytes, &processed);
        bytes += processed;
        break;

      case kCODE_STRING_8BIT_PIXEL:
        status = LS_ODSDecode8BitPixelCodeString(subblock + bytes, &cookie, data_length - bytes, &processed);
        bytes += processed;
        break;

      case kMAP_TABLE_DATA_2_TO_4_bit:
        status = LS_ODSDecode2To4BitMapTable(subblock + bytes, &cookie);
        bytes += 2;
        break;

      case kMAP_TABLE_DATA_2_TO_8_bit:
        status = LS_ODSDecode2To8BitMapTable(subblock + bytes, &cookie);
        bytes += 4;
        break;

      case kMAP_TABLE_DATA_4_TO_8_bit:
        status = LS_ODSDecode4To8BitMapTable(subblock + bytes, &cookie);
        bytes += 16;
        break;

      case kEND_OF_OBJECT_LINE_CODE:
        startNewline(&cookie);
        break;

      case kEND_OF_STRING_SIGNAL:
        bytes += 1;
        break;

      default:
        LS_ERROR("Unsupported data type %d\n", dataType);
        dataError = LS_TRUE;
        status = LS_ERROR_STREAM_DATA;
    }
  }

  *processed_bytes = bytes;

  if (*processed_bytes != data_length)
  {
    status = LS_ERROR_STREAM_DATA;
  }

  return status;
}


int32_t
LS_ODSDecode2BitPixelCodeString(uint8_t* data, LS_ODSCookie* cookie, uint32_t max_length, uint32_t* processed_bytes)
{
  uint32_t num_bits = 0;
  uint8_t nextbits = 0;
  uint8_t switch_1 = 0;
  uint8_t switch_2 = 0;
  uint8_t switch_3 = 0;
  uint8_t run_length_3_10 = 0;
  uint8_t run_length_12_27 = 0;
  uint8_t run_length_29_284 = 0;
  uint8_t end_of_string_signal = LS_FALSE;
  uint8_t pixel_code = 0;
  int32_t status = LS_OK;

  if ((data == NULL) ||
      (processed_bytes == NULL))
  {
    return LS_ERROR_GENERAL;
  }

  while (num_bits < max_length * 8)
  {
    nextbits = ReadBitStream32(data, 0, 0 + num_bits, 2, &status);
    DEBUG_CHECK(status == LS_OK);
    num_bits += 2;

    if (nextbits != 0)
    {
      drawPixel(cookie, 1, nextbits);
    }
    else
    {
      switch_1 = ReadBitStream32(data, 0, 0 + num_bits, 1, &status);
      DEBUG_CHECK(status == LS_OK);
      num_bits += 1;

      if (switch_1 == kB0001)
      {
        run_length_3_10 = ReadBitStream32(data, 0, 0 + num_bits, 3, &status);
        DEBUG_CHECK(status == LS_OK);
        num_bits += 3;
        pixel_code = ReadBitStream32(data, 0, 0 + num_bits, 2, &status);
        DEBUG_CHECK(status == LS_OK);
        num_bits += 2;
        drawPixel(cookie, (run_length_3_10 + 3), pixel_code);
      }
      else
      {
        switch_2 = ReadBitStream32(data, 0, 0 + num_bits, 1, &status);
        DEBUG_CHECK(status == LS_OK);
        num_bits += 1;

        if (switch_2 == kB0000)
        {
          switch_3 = ReadBitStream32(data, 0, 0 + num_bits, 2, &status);
          DEBUG_CHECK(status == LS_OK);
          num_bits += 2;

          if (switch_3 == kB0000)
          {
            LS_TRACE("meet the end of 2-bit/pixel_code_string\n");
            end_of_string_signal = LS_TRUE;
            break;
          }

          if (switch_3 == kB0001)
          {
            drawPixel(cookie, 2, 0);
          }

          if (switch_3 == kB0010)
          {
            run_length_12_27 = ReadBitStream32(data, 0, 0 + num_bits, 4, &status);
            DEBUG_CHECK(status == LS_OK);
            num_bits += 4;
            pixel_code = ReadBitStream32(data, 0, 0 + num_bits, 2, &status);
            DEBUG_CHECK(status == LS_OK);
            num_bits += 2;
            drawPixel(cookie, (run_length_12_27 + 12), pixel_code);
          }

          if (switch_3 == kB0011)
          {
            run_length_29_284 = ReadBitStream32(data, 0, 0 + num_bits, 8, &status);
            DEBUG_CHECK(status == LS_OK);
            num_bits += 8;
            pixel_code = ReadBitStream32(data, 0, 0 + num_bits, 2, &status);
            DEBUG_CHECK(status == LS_OK);
            num_bits += 2;
            drawPixel(cookie, (run_length_29_284 + 29), pixel_code);
          }
        }
        else
        {
          drawPixel(cookie, 1, 0);
        }
      }
    }
  }

  *processed_bytes = (uint32_t)(num_bits + 7) / 8;
  LS_TRACE("processed %d bits (%d bytes) in LS_ODSDecode2BitPixelCodeString()\n", num_bits, *processed_bytes);
  DEBUG_CHECK(end_of_string_signal == LS_TRUE);
  return LS_OK;
}


int32_t
LS_ODSDecode4BitPixelCodeString(uint8_t* data, LS_ODSCookie* cookie, uint32_t max_length, uint32_t* processed_bytes)
{
  uint32_t num_bits = 0;
  uint8_t nextbits = 0;
  uint8_t switch_1 = 0;
  uint8_t switch_2 = 0;
  uint8_t switch_3 = 0;
  uint8_t run_length_3_9 = 0;
  uint8_t run_length_4_7 = 0;
  uint8_t run_length_9_24 = 0;
  uint8_t run_length_25_280 = 0;
  uint8_t end_of_string_signal = LS_FALSE;
  uint8_t pixel_code = 0;
  int32_t status = LS_OK;

  if ((data == NULL) ||
      (processed_bytes == NULL))
  {
    return LS_ERROR_GENERAL;
  }

#if 0
  LS_DumpMem(data, 64);
#endif

  while (num_bits < max_length * 8)
  {
    nextbits = ReadBitStream32(data, 0, 0 + num_bits, 4, &status);
    DEBUG_CHECK(status == LS_OK);
    num_bits += 4;

    if (nextbits != 0)
    {
      drawPixel(cookie, 1, nextbits);
    }
    else
    {
      switch_1 = ReadBitStream32(data, 0, 0 + num_bits, 1, &status);
      DEBUG_CHECK(status == LS_OK);
      num_bits += 1;

      if (switch_1 == kB0000)
      {
        nextbits = ReadBitStream32(data, 0, 0 + num_bits, 3, &status);
        DEBUG_CHECK(status == LS_OK);
        num_bits += 3;

        if (nextbits != kB0000)
        {
          run_length_3_9 = nextbits;
          drawPixel(cookie, run_length_3_9 + 2, 0);
        }
        else
        {
          LS_TRACE("meet the end of 4-bit/pixel_code_string\n");
          end_of_string_signal = LS_TRUE;
          break;
        }
      }
      else
      {
        switch_2 = ReadBitStream32(data, 0, 0 + num_bits, 1, &status);
        DEBUG_CHECK(status == LS_OK);
        num_bits += 1;

        if (switch_2 == kB0000)
        {
          run_length_4_7 = ReadBitStream32(data, 0, 0 + num_bits, 2, &status);
          DEBUG_CHECK(status == LS_OK);
          num_bits += 2;
          pixel_code = ReadBitStream32(data, 0, 0 + num_bits, 4, &status);
          DEBUG_CHECK(status == LS_OK);
          num_bits += 4;
          drawPixel(cookie, run_length_4_7 + 4, pixel_code);
        }
        else
        {
          switch_3 = ReadBitStream32(data, 0, 0 + num_bits, 2, &status);
          DEBUG_CHECK(status == LS_OK);
          num_bits += 2;

          if (switch_3 == kB0000)
          {
            drawPixel(cookie, 1, 0);
          }

          if (switch_3 == kB0001)
          {
            drawPixel(cookie, 2, 0);
          }

          if (switch_3 == kB0010)
          {
            run_length_9_24 = ReadBitStream32(data, 0, 0 + num_bits, 4, &status);
            DEBUG_CHECK(status == LS_OK);
            num_bits += 4;
            pixel_code = ReadBitStream32(data, 0, 0 + num_bits, 4, &status);
            DEBUG_CHECK(status == LS_OK);
            num_bits += 4;
            drawPixel(cookie, run_length_9_24 + 9, pixel_code);
          }

          if (switch_3 == kB0011)
          {
            run_length_25_280 = ReadBitStream32(data, 0, 0 + num_bits, 8, &status);
            DEBUG_CHECK(status == LS_OK);
            num_bits += 8;
            pixel_code = ReadBitStream32(data, 0, 0 + num_bits, 4, &status);
            DEBUG_CHECK(status == LS_OK);
            num_bits += 4;
            drawPixel(cookie, run_length_25_280 + 25, pixel_code);
          }
        }
      }
    }
  }

  *processed_bytes = (uint32_t)(num_bits + 7) / 8;
  LS_TRACE("processed %d bits (%d bytes) in LS_ODSDecode4BitPixelCodeString()\n", num_bits, *processed_bytes);

  if (end_of_string_signal == LS_TRUE)
  {
    LS_DEBUG("end_of_string_signal = TRUE\n");
  }

  return LS_OK;
}


int32_t
LS_ODSDecode8BitPixelCodeString(uint8_t* data, LS_ODSCookie* cookie, uint32_t max_length, uint32_t* processed_bytes)
{
  uint32_t num_bits = 0;
  uint8_t nextbits = 0;
  uint8_t switch_1 = 0;
  uint8_t run_length_1_127 = 0;
  uint8_t run_length_3_127 = 0;
  uint8_t pixel_code = 0;
  int32_t status = LS_OK;
  uint8_t end_of_string_signal = LS_FALSE;

  if ((data == NULL) ||
      (processed_bytes == NULL))
  {
    return LS_ERROR_GENERAL;
  }

  while (num_bits < max_length * 8)
  {
    nextbits = ReadBitStream32(data, 0, 0 + num_bits, 8, &status);
    DEBUG_CHECK(status == LS_OK);
    num_bits += 8;

    if (nextbits != 0)
    {
      drawPixel(cookie, 1, nextbits);
    }
    else
    {
      switch_1 = ReadBitStream32(data, 0, 0 + num_bits, 1, &status);
      DEBUG_CHECK(status == LS_OK);
      num_bits += 1;

      if (switch_1 == kB0000)
      {
        nextbits = ReadBitStream32(data, 0, 0 + num_bits, 7, &status);
        DEBUG_CHECK(status == LS_OK);
        num_bits += 7;

        if (nextbits != kB0000)
        {
          run_length_1_127 = nextbits;
          drawPixel(cookie, run_length_1_127, 0);
        }
        else
        {
          LS_TRACE("Meet end_of_string_signal\n");
          end_of_string_signal = LS_TRUE;
          break;
        }
      }
      else
      {
        run_length_3_127 = ReadBitStream32(data, 0, 0 + num_bits, 7, &status);
        DEBUG_CHECK(status == LS_OK);
        num_bits += 7;
        pixel_code = ReadBitStream32(data, 0, 0 + num_bits, 8, &status);
        DEBUG_CHECK(status == LS_OK);
        num_bits += 8;
        drawPixel(cookie, run_length_3_127, pixel_code);
      }
    }
  }

  *processed_bytes = (uint32_t)(num_bits + 7) / 8;
  LS_TRACE("Processed %d bits (%d bytes) in LS_ODSDecode8BitPixelCodeString()\n", num_bits, *processed_bytes);
  DEBUG_CHECK(end_of_string_signal == LS_TRUE);
  return LS_OK;
}


int32_t
LS_ODSDecode2To4BitMapTable(uint8_t* data, LS_ODSCookie* cookie)
{
  LS_TRACE("Found 2_to_4_bit_map_table:\n");
  cookie->map_table.map_table_2_to_4_bit[0] = data[0] >> 4;
  cookie->map_table.map_table_2_to_4_bit[1] = data[0] & 0x0F;
  cookie->map_table.map_table_2_to_4_bit[2] = data[1] >> 4;
  cookie->map_table.map_table_2_to_4_bit[3] = data[1] & 0x0F;
  return LS_OK;
}


int32_t
LS_ODSDecode2To8BitMapTable(uint8_t* data, LS_ODSCookie* cookie)
{
  LS_TRACE("Found 2_to_8_bit_map_table:\n");
  SYS_MEMCPY(cookie->map_table.map_table_2_to_8_bit, data, 4);
  return LS_OK;
}


int32_t
LS_ODSDecode4To8BitMapTable(uint8_t* data, LS_ODSCookie* cookie)
{
  LS_TRACE("Found 4_to_8_bit_map_table:\n");
  SYS_MEMCPY(cookie->map_table.map_table_4_to_8_bit, data, 16);
  return LS_OK;
}


LS_Pixmap_t*
LS_NewPixmap(LS_Service* service, uint32_t width, uint32_t height, LS_PixelFormat fmt)
{
  LS_Pixmap_t* pixmap = NULL;
  int32_t status = LS_OK;

  do
  {
    if ((fmt >= LS_PIXFMT_END) ||
        (fmt <= LS_PIXFMT_UNKNOWN))
    {
      LS_ERROR("Pixmap format %d is not supported yet!\n", fmt);
      status = LS_ERROR_GENERAL;
      break;
    }

    pixmap = (LS_Pixmap_t*)ServiceHeapMalloc(service, PIXEL_BUFFER, sizeof(LS_Pixmap_t));
    DEBUG_CHECK(pixmap != NULL);

    if (pixmap == NULL)
    {
      LS_ERROR("PIXEL_BUFFER,Memory allocation failed: %d bytes\n", sizeof(LS_Pixmap_t));
      status = LS_ERROR_PIXEL_BUFFER;
      break;
    }

    SYS_MEMSET((void*)pixmap, 0, sizeof(LS_Pixmap_t));
    pixmap->width = width;
    pixmap->height = height;
    pixmap->pixelFormat = fmt;

    switch (pixmap->pixelFormat)
    {
      case LS_PIXFMT_PALETTE2BIT:
      case LS_PIXFMT_PALETTE4BIT:
      case LS_PIXFMT_PALETTE8BIT:
        pixmap->bytesPerLine = width;
        pixmap->bytesPerPixel = 1;
        pixmap->dataSize = width * height;
        break;

      case LS_PIXFMT_YUV420:
        pixmap->bytesPerLine = 0;
        pixmap->bytesPerPixel = 3;
        pixmap->dataSize = (width * height * 3) / 2;
        break;

      case LS_PIXFMT_YUYV:
      case LS_PIXFMT_YVYU:
      case LS_PIXFMT_UYVY:
      case LS_PIXFMT_VYUY:
        pixmap->bytesPerLine = 0;
        pixmap->bytesPerPixel = 3;
        pixmap->dataSize = width * height * 2;
        break;

      case LS_PIXFMT_RGBA15_LE:
      case LS_PIXFMT_RGBA15_BE:
      case LS_PIXFMT_BGRA15_LE:
      case LS_PIXFMT_BGRA15_BE:
      case LS_PIXFMT_ARGB15_LE:
      case LS_PIXFMT_ARGB15_BE:
      case LS_PIXFMT_ABGR15_LE:
      case LS_PIXFMT_ABGR15_BE:
        pixmap->bytesPerLine = width * 2;
        pixmap->bytesPerPixel = 2;
        pixmap->dataSize = width * height * 2;
        break;

      case LS_PIXFMT_RGB24:
      case LS_PIXFMT_BGR24:
        pixmap->bytesPerLine = width * 3;
        pixmap->bytesPerPixel = 3;
        pixmap->dataSize = width * height * 3;
        break;

      case LS_PIXFMT_RGB16_LE:
      case LS_PIXFMT_RGB16_BE:
      case LS_PIXFMT_BGR16_LE:
      case LS_PIXFMT_BGR16_BE:
        pixmap->bytesPerLine = width * 2;
        pixmap->bytesPerPixel = 2;
        pixmap->dataSize = width * height * 2;
        break;

      case LS_PIXFMT_ARGB32:
      case LS_PIXFMT_RGBA32:
      case LS_PIXFMT_BGRA32:
      case LS_PIXFMT_ABGR32:
        pixmap->bytesPerLine = width * 4;
        pixmap->bytesPerPixel = 4;
        pixmap->dataSize = width * height * 4;
        break;

      default:
        LS_ERROR("Unsupported format: %d\n", fmt);
        status = LS_ERROR_GENERAL;
    }

    if (status != LS_OK)
    {
      break;
    }

    pixmap->data = ServiceHeapMalloc(service, PIXEL_BUFFER, pixmap->dataSize);
    DEBUG_CHECK(pixmap->data != NULL);

    if (pixmap->data == NULL)
    {
      LS_ERROR("PIXEL_BUFFER,Memory allocation failed: %d bytes\n", pixmap->dataSize);
      status = LS_ERROR_PIXEL_BUFFER;
      break;
    }

    SYS_MEMSET((void*)(pixmap->data), 0, pixmap->dataSize);
  }while (0);

  if ((status != LS_OK) &&
      (NULL != pixmap))
  {
    LS_DeletePixmap(service, pixmap);
    pixmap = NULL;
  }

  LS_DEBUG("pixmap<%p>,pixmap->data <%p>,pixmap->datasize<%d> created\n",
           (void*)pixmap,
           (void*)pixmap->data,
           pixmap->dataSize);
  return pixmap;
}


void
LS_DeletePixmap(LS_Service* service, LS_Pixmap_t* pixmap)
{
  if (pixmap)
  {
    if (pixmap->data)
    {
      ServiceHeapFree(service, PIXEL_BUFFER, (void*)pixmap->data);
      LS_DEBUG("Pixmap(%p)-> data(%p) free'ed\n", (void*)pixmap, pixmap->data);
      pixmap->data = NULL;        /* Prevent double-free */
      SYS_MEMSET((void*)pixmap, 0, sizeof(LS_Pixmap_t));
    }

    ServiceHeapFree(service, PIXEL_BUFFER, (void*)pixmap);
    LS_DEBUG("Pixmap (%p) free'ed\n", (void*)pixmap);
  }
}


void
LS_FillPixmapWithColor(LS_Pixmap_t* pixmap, LS_Color_t* color)
{
  LS_ColorRGB_t rgb;
  LS_ColorYUV_t yuv;
  int32_t i = 0;
  uint8_t fill_color[4];

  if ((pixmap == NULL) ||
      (color == NULL))
  {
    return;
  }

  ConvertColor2RGBYUV(color, &yuv, &rgb);

  switch (pixmap->pixelFormat)
  {
    case LS_PIXFMT_ARGB32:

      while (i < (int32_t)pixmap->dataSize)
      {
        SYS_MEMCPY((uint8_t*)(pixmap->data) + i, (void*)(&rgb), 4);
        i += 4;
      }

      break;

    case LS_PIXFMT_BGRA32:

      while (i < (int32_t)pixmap->dataSize)
      {
        fill_color[0] = rgb.blueValue;
        fill_color[1] = rgb.greenValue;
        fill_color[2] = rgb.redValue;
        fill_color[3] = rgb.alphaValue;
        SYS_MEMCPY((uint8_t*)(pixmap->data) + i, (void*)(&fill_color), 4);
        i += 4;
      }

      break;

    default:
      LS_ERROR("Unsupported pixel format:%d\n", pixmap->pixelFormat);
  }
}
