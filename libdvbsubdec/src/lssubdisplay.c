/*-----------------------------------------------------------------------------
 * lssubdisplay.c
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

#include "lssubdisplay.h"
#include "lssubdec.h"
#include "lssubmacros.h"
#include "lslist.h"
#include "lssubsegdec.h"
#include "lssubconverter.h"
#include "lssubpixmap.h"
#include "lssubutils.h"
/**----------------------------------------------------------------------------
 * local static functions declarations
 *---------------------------------------------------------------------------*/
static LS_Pixmap_t* regionPixmapConvert(LS_Service* service, LS_DisplayRegion* region);

/**----------------------------------------------------------------------------
 * local static functions implementations
 *---------------------------------------------------------------------------*/
static LS_Pixmap_t*
regionPixmapConvert(LS_Service* service, LS_DisplayRegion* region)
{
  LS_Pixmap_t* pixmap = NULL;
  uint32_t src_w;
  uint32_t src_h;
  void* src_data = NULL;

  if ((region == NULL) ||
      (service == NULL) ||
      (region->pixmap == NULL))
  {
    LS_ERROR("LS_ERROR_GENERAL: Invalid parameters: " "service<%p>,region<%p>\n", (void*)service, (void*)region);
    return NULL;
  }

  src_w = region->pixmap->width;
  src_h = region->pixmap->height;
  src_data = region->pixmap->data;

  switch (service->osdRender.OSDPixmapFormat)
  {
    case LS_PIXFMT_PALETTE2BIT:
    case LS_PIXFMT_PALETTE4BIT:
    case LS_PIXFMT_PALETTE8BIT:
      pixmap = region->pixmap;
      break;

    case LS_PIXFMT_YUV420:
      pixmap = LS_NewPixmap(service, src_w, src_h, LS_PIXFMT_YUV420);

      if (pixmap == NULL)
      {
        LS_ERROR("LS_NewPixmap() failed for LS_PIXFMT_YUV420\n");
        return NULL;
      }

      pixmap->leftPos = region->pixmap->leftPos;
      pixmap->topPos = region->pixmap->topPos;
      ConvertCLUT82YUV420P(src_data, region->pixmap->width, region->pixmap->height, region->clut, pixmap->data);
      break;

    case LS_PIXFMT_YUYV:
      break;

    case LS_PIXFMT_YVYU:
      break;

    case LS_PIXFMT_UYVY:
      break;

    case LS_PIXFMT_VYUY:
      break;

    case LS_PIXFMT_RGBA15_LE:
      break;

    case LS_PIXFMT_RGBA15_BE:
      break;

    case LS_PIXFMT_BGRA15_LE:
      break;

    case LS_PIXFMT_BGRA15_BE:
      break;

    case LS_PIXFMT_ARGB15_LE:
      break;

    case LS_PIXFMT_ARGB15_BE:
      break;

    case LS_PIXFMT_ABGR15_LE:
      break;

    case LS_PIXFMT_ABGR15_BE:
      break;

    case LS_PIXFMT_RGB24:
      break;

    case LS_PIXFMT_BGR24:
      break;

    case LS_PIXFMT_RGB16_LE:
      break;

    case LS_PIXFMT_RGB16_BE:
      break;

    case LS_PIXFMT_BGR16_LE:
      break;

    case LS_PIXFMT_BGR16_BE:
      break;

    case LS_PIXFMT_ARGB32:
      pixmap = LS_NewPixmap(service, src_w, src_h, LS_PIXFMT_ARGB32);

      if (pixmap == NULL)
      {
        LS_ERROR("LS_NewPixmap() failed\n");
        return NULL;
      }

      pixmap->leftPos = region->pixmap->leftPos;
      pixmap->topPos = region->pixmap->topPos;
      ConvertCLUT82ARGB32(src_data, region->pixmap->dataSize, region->clut, pixmap->data);
      break;

    case LS_PIXFMT_BGRA32:
      pixmap = LS_NewPixmap(service, src_w, src_h, LS_PIXFMT_BGRA32);

      if (pixmap == NULL)
      {
        LS_ERROR("LS_NewPixmap() failed\n");
        return NULL;
      }

      pixmap->leftPos = region->pixmap->leftPos;
      pixmap->topPos = region->pixmap->topPos;
      ConvertCLUT82BGRA32(src_data, region->pixmap->dataSize, region->clut, pixmap->data);
      break;

    case LS_PIXFMT_ABGR32:
      break;

    default:
      LS_ERROR("LS_ERROR_STREAM_DATA: Pixel format %d is not supported\n", service->osdRender.OSDPixmapFormat);
      return NULL;
  }

  return pixmap;
}


/**----------------------------------------------------------------------------
 * internal public functions implementations
 *---------------------------------------------------------------------------*/

/**
 * @brief Check if subtitle should be displayed based on PTS synchronization
 *
 * Compares the subtitle PTS with the current video PTS to determine if
 * the subtitle should be displayed now. This prevents subtitles from
 * appearing too early or too late relative to the video.
 *
 * @param page Display page with subtitle PTS
 * @param service Service instance with PTS callbacks
 * @return LS_TRUE if subtitle should be displayed, LS_FALSE to wait
 */
static int32_t
ShouldDisplaySubtitleByPTS(LS_DisplayPage* page, LS_Service* service)
{
  uint64_t current_pcr = 0;
  int64_t pts_diff_ms;
  int64_t tolerance_ms;
  int64_t max_early_ms;

  if ((page == NULL) || (service == NULL))
  {
    return LS_TRUE;  /* No sync info, display immediately */
  }

  /* Check if PTS sync is enabled */
  if (service->osdRender.ptsSyncEnabled == 0)
  {
    LS_DEBUG("PTS sync disabled, displaying subtitle immediately\n");
    return LS_TRUE;
  }

  /* Check if we have a PCR callback to get current video time */
  if (service->osdRender.getCurrentPCRFunc == NULL)
  {
    LS_WARNING("PTS sync enabled but getCurrentPCRFunc not set, displaying immediately\n");
    return LS_TRUE;
  }

  /* Get current PCR (video time) */
  if (service->osdRender.getCurrentPCRFunc(&current_pcr, service->osdRender.getCurrentPCRFuncData) != LS_OK)
  {
    LS_WARNING("Failed to get current PCR, displaying subtitle immediately\n");
    return LS_TRUE;
  }

  /* Convert PTS and PCR to milliseconds for comparison */
  /* Note: Both PTS and PCR are in 90kHz units for MPEG-2 */
  /* Calculate difference: (current_video_time - subtitle_presentation_time) */
  pts_diff_ms = (int64_t)(current_pcr - page->ptsValue) / 90;  /* Convert 90kHz -> ms */

  /* Get tolerance settings */
  tolerance_ms = service->osdRender.ptsSyncTolerance;
  if (tolerance_ms == 0)
  {
    tolerance_ms = 100;  /* Default tolerance: 100ms */
  }

  max_early_ms = service->osdRender.ptsMaxEarlyDisplay;
  if (max_early_ms == 0)
  {
    max_early_ms = 50;  /* Default max early display: 50ms */
  }

  /* Check if we're within tolerance window */
  /* Allow display if: -max_early_ms <= pts_diff_ms <= tolerance_ms */
  if ((pts_diff_ms >= -max_early_ms) && (pts_diff_ms <= tolerance_ms))
  {
    LS_DEBUG("PTS sync OK: subtitle PTS=%llu, current PCR=%llu, diff=%lld ms - DISPLAYING\n",
             (unsigned long long)page->ptsValue,
             (unsigned long long)current_pcr,
             pts_diff_ms);
    return LS_TRUE;
  }

  /* Subtitle is too early */
  if (pts_diff_ms < -max_early_ms)
  {
    LS_DEBUG("PTS sync: subtitle too early by %lld ms - WAITING\n", -pts_diff_ms);
    return LS_FALSE;
  }

  /* Subtitle is late but within tolerance - still display it */
  if (pts_diff_ms > tolerance_ms)
  {
    LS_WARNING("PTS sync: subtitle late by %lld ms - DISPLAYING ANYWAY\n", pts_diff_ms);
    return LS_TRUE;
  }

  return LS_TRUE;
}


int32_t
LS_DisplayPageOnScreen(LS_DisplayPage* page)
{
  int32_t status = LS_OK;
  int32_t i = 0;
  uint32_t regionnumber = 0;
  LS_DrawPixmapFunc drawPixmapFunc;
  LS_DisplayRegion* region = NULL;
  LS_Pixmap_t* pixmap = NULL;
  LS_Service* service = NULL;
  LS_Time_t leftms;

  if (page == NULL)
  {
    LS_DEBUG("page == NULL, return...\n");
    return LS_OK;
  }

  service = page->service;

  if (service == NULL)
  {
    LS_ERROR("LS_ERROR_GENERAL: NULL service found in display page <%p>\n", (void*)page);
    return LS_ERROR_GENERAL;
  }

  /* Check PTS synchronization before displaying subtitle */
  if (ShouldDisplaySubtitleByPTS(page, service) == LS_FALSE)
  {
    LS_DEBUG("Subtitle PTS not reached yet, delaying display\n");
    return LS_OK;  /* Return OK - subtitle will be displayed when timer fires */
  }

  if (page->suicide_timer)
  {
    status = LS_TimerStop(page->suicide_timer, &leftms);
    LS_DEBUG("Timer <%p> was stopped,suicide_timer for display page <%p>\n", (void*)(page->suicide_timer), (void*)page);
    DEBUG_CHECK(status == LS_OK);
  }

  status = LS_ListCount(page->regions, &regionnumber);
  DEBUG_CHECK(status == LS_OK);

  if (service->displayPageOnScreen)
  {
    LS_RemovePageFromScreen(service->displayPageOnScreen);
    LS_DEBUG("displayPageOnScreen<%p> (PTS:%llu,0x%08llx,%s) was removed " "from screen\n",
             (void*)(service->displayPageOnScreen),
             service->displayPageOnScreen->ptsValue,
             service->displayPageOnScreen->ptsValue,
             PTStoHMS(service->displayPageOnScreen->ptsValue));
  }

  for (i = 0; i < (int32_t)regionnumber; i++)
  {
    region = (LS_DisplayRegion*)LS_ListNthNode(page->regions, i);

    if (region &&
        region->region_visible)
    {
      /* Check if this region contains character-based subtitle data */
      /* Character data is indicated by: small pixmap (1x1) + non-zero character_code[0] */
      if (region->pixmap != NULL &&
          region->pixmap->width == 1 &&
          region->pixmap->height == 1 &&
          region->character_code[0] != 0)
      {
        /* Character-based subtitle - use drawTextFunc callback */
        if (service->osdRender.drawTextFunc)
        {
          char text_buffer[257];  /* 256 chars + null terminator */
          int32_t char_count;
          LS_DrawTextFunc drawTextFunc;
          LS_ColorRGB_t* text_color;
          int32_t x_pos, y_pos;
          LS_ENCODING text_encoding;
          int32_t has_extended_chars = 0;

          /* Find string length (search for null terminator) */
          for (char_count = 0; char_count < 256; char_count++)
          {
            if (region->character_code[char_count] == 0)
            {
              break;
            }
            /* Check for extended characters ( > 127 indicates non-ASCII) */
            if (region->character_code[char_count] > 127)
            {
              has_extended_chars = 1;
            }
          }

          /* Copy character codes to text buffer (ensure null-terminated) */
          SYS_MEMCPY(text_buffer, region->character_code, char_count);
          text_buffer[char_count] = '\0';

          /* Get text position from pixmap (stored during decode) */
          x_pos = region->pixmap->leftPos;
          y_pos = region->pixmap->topPos;

          /* Use foreground color from region */
          text_color = &region->forground_color;

          /* Determine character encoding based on content */
          /* DVB subtitles commonly use ISO-8859-X encodings */
          if (has_extended_chars)
          {
            /* Extended characters present - likely Latin-1 or similar */
            text_encoding = LS_ENCODING_LATIN1;
            LS_DEBUG("Character encoding: LATIN-1 (extended chars detected)\n");
          }
          else
          {
            /* ASCII only */
            text_encoding = LS_ENCODING_ASCII;
            LS_DEBUG("Character encoding: ASCII\n");
          }

          /* Note: If UTF-8 output is required, the renderer should convert
           * from Latin-1/ASCII to UTF-8. The encoding parameter informs
           * the renderer of the source encoding. */

          drawTextFunc = service->osdRender.drawTextFunc;
          drawTextFunc(x_pos, y_pos, text_buffer, text_color, text_encoding,
                       service->osdRender.drawTextFuncData);

          LS_INFO("DrawTextFunc called for region_id=%d at (%d,%d) with %d chars, encoding=%d\n",
                  region->region_id, x_pos, y_pos, char_count, text_encoding);

          /* Log first 64 chars of text for debugging */
          if (char_count > 0)
          {
            char debug_buf[65];
            int32_t debug_len = (char_count < 64) ? char_count : 64;
            SYS_MEMCPY(debug_buf, text_buffer, debug_len);
            debug_buf[debug_len] = '\0';
            LS_DEBUG("Text content: \"%s\"%s\n", debug_buf,
                     (char_count > 64) ? "..." : "");
          }
        }
        else
        {
          LS_WARNING("Region has character data but drawTextFunc callback not set\n");
        }
      }
      else if (region->pixmap != NULL)
      {
        /* Bitmap-based subtitle - use drawPixmapFunc callback */
        /*see if we need do pixel format convert*/
        pixmap = regionPixmapConvert(service, region);

        if (service->osdRender.drawPixmapFunc)
        {
          LS_DEBUG("Draw region(id=%d) on screen now...rect = " "{%d,%d,%d,%d}\n",
                   region->region_id,
                   region->pixmap->leftPos,
                   region->pixmap->topPos,
                   region->pixmap->leftPos + region->pixmap->width,
                   region->pixmap->topPos + region->pixmap->height);
          drawPixmapFunc = service->osdRender.drawPixmapFunc;
          LS_DEBUG("call drawPixmapFunc to draw subtitle on screen...\n");
          // Pass the region's CLUT to the draw function for proper color/alpha rendering
          // The CLUT has already been transformed by initRegionWithDefaultCLUT if needed
          drawPixmapFunc(pixmap,
                         (const uint8_t*)region->clut,          // Pass CLUT palette
                         0,
                         service->osdRender.drawPixmapFuncData);

          if (pixmap != region->pixmap)
          {
            LS_DeletePixmap(service, pixmap);
          }
        }
      }
      else
      {
        LS_DEBUG("Region id=%d has no pixmap or character data to display\n", region->region_id);
      }
    }
  }

  page->visible = LS_TRUE;
  page->status = LS_DISPLAYPAGE_STATUS_DISPLAYING;
  return LS_OK;
}


int32_t
LS_RemovePageFromScreen(LS_DisplayPage* page)
{
  int32_t status = LS_OK;
  int32_t i;
  uint32_t regionnumber;
  LS_Color_t fill_color;
  LS_ColorRGB_t fill_rgb;
  LS_ColorYUV_t fill_yuv;
  LS_DrawPixmapFunc drawPixmapFunc;
  void* cb_draw_data = NULL;
  LS_Rect_t fill_rect;
  LS_Pixmap_t* fill_pixmap = NULL;
  LS_CleanRegionFunc clean_region_cb;
  void* clean_region_cb_data = NULL;
  LS_DisplayRegion* region = NULL;
  LS_Service* service = NULL;
  LS_Time_t leftms;

  LS_ENTER("page<%p>\n", page);

  do
  {
    if (page == NULL)
    {
      LS_DEBUG("page == NULL, return...\n");
      break;
    }

    service = page->service;

    if (service == NULL)
    {
      LS_ERROR("LS_ERROR_GENERAL: NULL service found in display page %p\n", (void*)page);
      status = LS_ERROR_GENERAL;
      break;
    }

    if ((page->visible == LS_FALSE) ||
        (page->status != LS_DISPLAYPAGE_STATUS_DISPLAYING))
    {
      LS_DEBUG("Display page %p (PTS:%llu,0x%08llx,%s) was not visible or " "status is %d.\n",
               (void*)page,
               page->ptsValue,
               page->ptsValue,
               PTStoHMS(page->ptsValue),
               page->status);
      status = LS_OK;
      break;
    }

    status = LS_TimerStop(page->page_time_out_timer, &leftms);
    DEBUG_CHECK(status == LS_OK);

    if (status != LS_OK)
    {
      LS_WARNING("page<%p>->page_time_out_timer<%p> cannot be stopped\n", page, page->page_time_out_timer);
    }

    LS_DEBUG("Timer <%p> was was stopped,page_time_out_timer for display page<%p>" " (PTS:%llu,0x%08llx,%s).\n",
             (void*)(page->page_time_out_timer),
             (void*)page,
             page->ptsValue,
             page->ptsValue,
             PTStoHMS(page->ptsValue));
    LS_DEBUG("check the draw function...\n");
    drawPixmapFunc = service->osdRender.drawPixmapFunc;
    cb_draw_data = service->osdRender.drawPixmapFuncData;
    clean_region_cb = service->osdRender.cleanRegionFunc;
    clean_region_cb_data = service->osdRender.cleanRegionFuncData;
    fill_color = service->osdRender.backgroundColor;
    ConvertColor2RGBYUV(&fill_color, &fill_yuv, &fill_rgb);
    status = LS_ListCount(page->regions, &regionnumber);
    DEBUG_CHECK(status == LS_OK);

    for (i = 0; i < (int32_t)regionnumber; i++)
    {
      region = (LS_DisplayRegion*)LS_ListNthNode(page->regions, i);

      if (region &&
          region->region_visible &&
          region->pixmap)
      {
        fill_rect.leftPos = region->pixmap->leftPos;
        fill_rect.topPos = region->pixmap->topPos;
        fill_rect.rightPos = region->pixmap->leftPos + region->pixmap->width;
        fill_rect.bottomPos = region->pixmap->topPos + region->pixmap->height;

        if (clean_region_cb)
        {
          clean_region_cb(fill_rect, clean_region_cb_data);
        }
        else if (drawPixmapFunc &&
                 region->pixmap)
        {
          fill_pixmap = LS_NewPixmap(service,
                                     region->pixmap->width,
                                     region->pixmap->height,
                                     service->osdRender.OSDPixmapFormat);

          if (fill_pixmap)
          {
            fill_pixmap->leftPos = region->pixmap->leftPos;
            fill_pixmap->topPos = region->pixmap->topPos;
            LS_FillPixmapWithColor(fill_pixmap, &(service->osdRender.backgroundColor));
            LS_DEBUG("Clean region(id=%d) on screen now...rect = " "{%d,%d,%d,%d}\n",
                     region->region_id,
                     region->pixmap->leftPos,
                     region->pixmap->topPos,
                     region->pixmap->leftPos + region->pixmap->width,
                     region->pixmap->topPos + region->pixmap->height);
            drawPixmapFunc(fill_pixmap, NULL, 0, cb_draw_data);
            LS_DeletePixmap(service, fill_pixmap);
          }
        }
      }
    }

    page->visible = LS_FALSE;
    page->status = LS_DISPLAYPAGE_STATUS_RETIRED;
    status = LS_OK;
  }while (0);

  LS_LEAVE("page<%p>\n", page);
  return status;
}
