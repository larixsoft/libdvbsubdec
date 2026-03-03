/*-----------------------------------------------------------------------------
 * subtitle_renderer.c
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
 * @file subtitle_renderer.c
 * @brief SDL-based subtitle renderer implementation
 *
 * @section overview Overview
 *
 * This module implements the subtitle rendering layer for the DVB subtitle
 * decoder using SDL2. It handles region management, pixel format conversion,
 * and display scaling with aspect ratio preservation.
 *
 * @section pixel_formats Pixel Format Conversion
 *
 * The decoder library supports two primary pixel formats:
 *
 * @subsection argb32 LS_PIXFMT_ARGB32
 *
 * 32-bit ARGB format with premultiplied alpha:
 * - Decoder output: A-R-G-B byte order (big-endian notation)
 * - Actual memory layout (little-endian): B-G-R-A
 * - SDL matching format: SDL_PIXELFORMAT_BGRA8888
 *
 * @subsection palette8bit LS_PIXFMT_PALETTE8BIT
 *
 * 8-bit palette-indexed format requiring CLUT (Color Look-Up Table):
 * - Each pixel is a palette index (0-255)
 * - CLUT entry contains: R, G, B, A values (0-255 each)
 * - Must convert to ARGB32 for SDL rendering
 *
 * @subsection format_conversion Format Conversion Strategy
 *
 * @code
 * // ARGB32: Extract components and re-map for SDL
 * uint32_t pixel = src_pixels[i];
 * uint8_t a = (pixel >> 24) & 0xFF;
 * uint8_t r = (pixel >> 16) & 0xFF;
 * uint8_t g = (pixel >> 8) & 0xFF;
 * uint8_t b = pixel & 0xFF;
 * pixels[i] = SDL_MapRGBA(surface->format, r, g, b, a);
 *
 * // PALETTE8BIT: Look up color in CLUT
 * SDL_Color color = get_palette_color(palette, src[i]);
 * pixels[i] = SDL_MapRGBA(surface->format, color.r, color.g, color.b, color.a);
 * @endcode
 *
 * @section region_management Region Management
 *
 * @subsection clearing Region Clearing
 *
 * The decoder calls clean_region_callback to clear areas:
 *
 * @code
 * void clear_region_callback(LS_Rect_t rect) {
 *     // Remove all regions intersecting with this rectangle
 *     for (each region) {
 *         if (SDL_HasIntersection(&clear_rect, &region->rect)) {
 *             SDL_DestroyTexture(region->texture);
 *             remove_region(region);
 *         }
 *     }
 * }
 * @endcode
 *
 * @subsection adding Adding Regions
 *
 * The decoder calls draw_pixmap_callback to add subtitle regions:
 *
 * @code
 * void draw_pixmap_callback(const LS_Pixmap_t* pixmap, const uint8_t* palette) {
 *     // 1. Convert pixel format if needed
 *     // 2. Create SDL texture from pixmap data
 *     // 3. Store with original coordinates
 *     add_region(pixmap->leftPos, pixmap->topPos, texture);
 * }
 * @endcode
 *
 * @section display_scaling Display Scaling
 *
 * @subsection aspect_ratio Aspect Ratio Preservation
 *
 * Subtitles must maintain their aspect ratio when the window resizes:
 *
 * @code
 * // Calculate scale factors with letterbox/pillarbox
 * double source_aspect = dds_width / dds_height;
 * double window_aspect = window_width / window_height;
 *
 * if (window_aspect > source_aspect) {
 *     // Window is wider - pillarbox
 *     scale = window_height / dds_height;
 *     offset_x = (window_width - dds_width * scale) / 2.0;
 * } else {
 *     // Window is taller - letterbox
 *     scale = window_width / dds_width;
 *     offset_y = (window_height - dds_height * scale) / 2.0;
 * }
 *
 * display_x = original_x * scale + offset_x;
 * display_y = original_y * scale + offset_y;
 * @endcode
 *
 * @subsection dds Display Definition Segment (DDS)
 *
 * The DDS provides the source display dimensions:
 * - Display Definition Segment received via dds_notify_callback
 * - Contains original video dimensions (e.g., 720x576 for PAL)
 * - Used for aspect ratio calculation
 *
 * @section thread_safety Thread Safety
 *
 * ALL operations are protected by pthread mutex:
 *
 * @code
 * pthread_mutex_lock(&renderer->mutex);
 * // Modify regions array
 * pthread_mutex_unlock(&renderer->mutex);
 * @endcode
 *
 * This prevents race conditions between:
 * - Decoder callbacks (add_pixmap, clear_region)
 * - Main thread rendering (render)
 *
 * @section rendering_pipeline Rendering Pipeline
 *
 * 1. Decoder calls clean_region_callback (main thread)
 *    → Removes intersecting regions
 *
 * 2. Decoder calls draw_pixmap_callback (main thread)
 *    → Converts pixel format
 *    → Creates SDL texture
 *    → Adds to regions array
 *
 * 3. Main loop calls subtitle_renderer_render()
 *    → Calculates scaled positions
 *    → Renders all textures
 *    → Presents frame
 */

#include "subtitle_renderer.h"
#include <stdlib.h>
#include <string.h>
/*-----------------------------------------------------------------------------
 * Constants
 *---------------------------------------------------------------------------*/
/** @brief Initial region capacity */
#define INITIAL_REGION_CAPACITY    16
/** @brief Default window width */
#define DEFAULT_WINDOW_WIDTH       720
/** @brief Default window height */
#define DEFAULT_WINDOW_HEIGHT      576
/*-----------------------------------------------------------------------------
 * Helper Functions
 *---------------------------------------------------------------------------*/
/**
 * @brief Get color from CLUT palette
 */
static SDL_Color
get_palette_color(const uint8_t* palette, uint8_t index)
{
  SDL_Color color;

  /* Default to transparent black if no palette (like Qt) */
  if (!palette)
  {
    color.r = 0;
    color.g = 0;
    color.b = 0;
    color.a = 0;
    return color;
  }

  /* Get CLUT entry - exactly like Qt version:
   * Qt: QColor(r, g, b, a) where r,g,b,a come directly from CLUT */
  const LS_ColorRGB_t* clutEntry = (const LS_ColorRGB_t*)palette;

  color.r = clutEntry[index].redValue;
  color.g = clutEntry[index].greenValue;
  color.b = clutEntry[index].blueValue;
  color.a = clutEntry[index].alphaValue;
  return color;
}


/*-----------------------------------------------------------------------------
 * Public Functions
 *---------------------------------------------------------------------------*/
SubtitleRenderer_t*
subtitle_renderer_create(void)
{
  SubtitleRenderer_t* renderer = calloc(1, sizeof(SubtitleRenderer_t));

  if (!renderer)
  {
    return NULL;
  }

  renderer->regions = calloc(INITIAL_REGION_CAPACITY, sizeof(SubtitleRegion_t));

  if (!renderer->regions)
  {
    free(renderer);
    return NULL;
  }

  renderer->region_capacity = INITIAL_REGION_CAPACITY;
  renderer->region_count = 0;
  renderer->display_width = DEFAULT_WINDOW_WIDTH;
  renderer->display_height = DEFAULT_WINDOW_HEIGHT;

  if (pthread_mutex_init(&renderer->mutex, NULL) != 0)
  {
    free(renderer->regions);
    free(renderer);
    return NULL;
  }

  return renderer;
}


void
subtitle_renderer_destroy(SubtitleRenderer_t* renderer)
{
  if (!renderer)
  {
    return;
  }

  pthread_mutex_lock(&renderer->mutex);

  /* Free all textures */
  for (int i = 0; i < renderer->region_count; i++)
  {
    if (renderer->regions[i].texture)
    {
      SDL_DestroyTexture(renderer->regions[i].texture);
    }
  }

  pthread_mutex_unlock(&renderer->mutex);
  pthread_mutex_destroy(&renderer->mutex);
  free(renderer->regions);
  free(renderer);
}


void
subtitle_renderer_set_display_size(SubtitleRenderer_t* renderer, int width, int height)
{
  if (!renderer)
  {
    return;
  }

  pthread_mutex_lock(&renderer->mutex);
  renderer->display_width = width;
  renderer->display_height = height;
  pthread_mutex_unlock(&renderer->mutex);
}


void
subtitle_renderer_clear_region(SubtitleRenderer_t* renderer, int left, int top, int right, int bottom)
{
  if (!renderer)
  {
    return;
  }

  SDL_Rect clear_rect = { left, top, right - left, bottom - top };

  pthread_mutex_lock(&renderer->mutex);

  /* Remove all regions that intersect with the clear rectangle */
  for (int i = renderer->region_count - 1; i >= 0; i--)
  {
    if (SDL_HasIntersection(&clear_rect, &renderer->regions[i].original_rect))
    {
      if (renderer->regions[i].texture)
      {
        SDL_DestroyTexture(renderer->regions[i].texture);
        renderer->regions[i].texture = NULL;
      }

      /* Move last element to this position */
      if (i < renderer->region_count - 1)
      {
        renderer->regions[i] = renderer->regions[renderer->region_count - 1];
      }

      renderer->region_count--;
    }
  }

  pthread_mutex_unlock(&renderer->mutex);
}


static LS_Status
convert_palette_pixmap(const LS_Pixmap_t* pixmap, const uint8_t* palette, SDL_Surface* surface)
{
  const uint8_t* src = (const uint8_t*)pixmap->data;
  /* Calculate pixel count */
  size_t total_pixels = (size_t)pixmap->width * pixmap->height;

  /* Convert pixels with bounds checking */
  for (uint32_t y = 0; y < pixmap->height; y++)
  {
    uint32_t* dst_row = (uint32_t*)((uint8_t*)surface->pixels + y * surface->pitch);

    for (uint32_t x = 0; x < pixmap->width; x++)
    {
      size_t pixel_offset = (size_t)y * pixmap->width + x;

      if (pixel_offset >= total_pixels)
      {
        return LS_ERROR_PIXEL_BUFFER;
      }

      uint8_t palette_index = src[pixel_offset];
      SDL_Color color = get_palette_color(palette, palette_index);

      /* Map to BGRA format to match Qt's Format_ARGB32
       * SDL_PIXELFORMAT_BGRA8888 expects bytes in memory: B, G, R, A
       * When stored as Uint32 (little-endian): (A << 24) | (R << 16) | (G << 8) | B */
      dst_row[x] = ((Uint32)color.a << 24) | ((Uint32)color.r << 16) |
                   ((Uint32)color.g << 8) | (Uint32)color.b;
    }
  }

  return LS_OK;
}


LS_Status
subtitle_renderer_add_pixmap(SubtitleRenderer_t* renderer,
                             const LS_Pixmap_t*  pixmap,
                             const uint8_t*      palette,
                             SDL_Renderer*       sdl_renderer)
{
  if (!renderer ||
      !pixmap ||
      !sdl_renderer)
  {
    return LS_ERROR_GENERAL;
  }

  if (!pixmap->data ||
      (pixmap->width == 0) ||
      (pixmap->height == 0))
  {
    return LS_ERROR_GENERAL;
  }

  /* Sanity check dimensions */
  if ((pixmap->width > 4096) ||
      (pixmap->height > 4096))
  {
    return LS_ERROR_PIXEL_BUFFER;
  }

  /* Calculate expected data size */
  size_t expected_size = (size_t)pixmap->width * pixmap->height * 4;
  /* Create surface with BGRA8888 format to match Qt's Format_ARGB32
   * Qt's QImage::Format_ARGB32 stores pixels in memory as BGRA (little-endian)
   * SDL_PIXELFORMAT_BGRA8888 has the same memory layout */
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, pixmap->width, pixmap->height, 32, SDL_PIXELFORMAT_BGRA8888);

  if (!surface)
  {
    return LS_ERROR_PIXEL_BUFFER;
  }

  /* Verify surface has enough space */
  size_t surface_size = (size_t)surface->pitch * surface->h;

  if (surface_size < expected_size)
  {
    SDL_FreeSurface(surface);
    return LS_ERROR_PIXEL_BUFFER;
  }

  LS_Status status = LS_OK;

  /* Convert based on pixel format */
  switch (pixmap->pixelFormat)
  {
    case LS_PIXFMT_ARGB32:
      /* Convert ARGB32 to BGRA for SDL
       * LS_ColorRGB_t struct has [A, R, G, B] in memory
       * On little-endian, uint32_t view is: (B << 24) | (G << 16) | (R << 8) | A
       * SDL_PIXELFORMAT_BGRA8888 expects: B, G, R, A in memory
       * So we need to swap byte order */
    {
      uint32_t* dst_pixels = (uint32_t*)surface->pixels;
      size_t total_pixels = (size_t)pixmap->width * pixmap->height;
      const uint32_t* src_pixels = (const uint32_t*)pixmap->data;

      for (size_t i = 0; i < total_pixels; i++)
      {
        uint32_t pixel = src_pixels[i];
        /* Swap bytes from (B,G,R,A) as uint32 to (B,G,R,A) in memory */
        uint8_t a = (pixel >> 0) & 0xFF;
        uint8_t r = (pixel >> 8) & 0xFF;
        uint8_t g = (pixel >> 16) & 0xFF;
        uint8_t b = (pixel >> 24) & 0xFF;

        /* Write as BGRA in memory */
        dst_pixels[i] = ((uint32_t)b << 0) | ((uint32_t)g << 8) |
                        ((uint32_t)r << 16) | ((uint32_t)a << 24);
      }
    }
    break;

    case LS_PIXFMT_PALETTE8BIT:
      /* Convert palette to BGRA format */
      status = convert_palette_pixmap(pixmap, palette, surface);
      break;

    default:
      status = LS_ERROR_PIXEL_BUFFER;
      break;
  }

  if (status != LS_OK)
  {
    SDL_FreeSurface(surface);
    return status;
  }

  /* Verify surface is valid before creating texture */
  if (!surface->pixels)
  {
    SDL_FreeSurface(surface);
    return LS_ERROR_PIXEL_BUFFER;
  }

  /* Create texture with same format as surface */
  Uint32 surface_format = surface->format->format;
  SDL_Texture* texture = SDL_CreateTexture(sdl_renderer,
                                           surface_format,
                                           SDL_TEXTUREACCESS_STATIC,
                                           pixmap->width,
                                           pixmap->height);

  if (!texture)
  {
    SDL_FreeSurface(surface);
    return LS_ERROR_PIXEL_BUFFER;
  }

  if (SDL_UpdateTexture(texture, NULL, surface->pixels, surface->pitch) != 0)
  {
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
    return LS_ERROR_PIXEL_BUFFER;
  }

  SDL_FreeSurface(surface);

  pthread_mutex_lock(&renderer->mutex);

  /* Expand capacity if needed */
  if (renderer->region_count >= renderer->region_capacity)
  {
    int new_capacity = renderer->region_capacity * 2;
    SubtitleRegion_t* new_regions = realloc(renderer->regions, new_capacity * sizeof(SubtitleRegion_t));

    if (!new_regions)
    {
      pthread_mutex_unlock(&renderer->mutex);
      SDL_DestroyTexture(texture);
      return LS_ERROR_SYSTEM_BUFFER;
    }

    renderer->regions = new_regions;
    renderer->region_capacity = new_capacity;
  }

  /* Add new region */
  SubtitleRegion_t* region = &renderer->regions[renderer->region_count];

  region->texture = texture;
  region->original_rect.x = pixmap->leftPos;
  region->original_rect.y = pixmap->topPos;
  region->original_rect.w = pixmap->width;
  region->original_rect.h = pixmap->height;
  renderer->region_count++;
  pthread_mutex_unlock(&renderer->mutex);
  return LS_OK;
}


SDL_Rect
subtitle_renderer_calculate_display_rect(const SubtitleRenderer_t* renderer,
                                         const SDL_Rect*           original_rect,
                                         int                       window_width,
                                         int                       window_height)
{
  SDL_Rect display_rect;
  double scale_x, scale_y;
  double offset_x = 0, offset_y = 0;
  /* Calculate scale factors based on display dimensions from DDS */
  double source_aspect = (double)renderer->display_width / renderer->display_height;
  double window_aspect = (double)window_width / window_height;

  if (window_aspect > source_aspect)
  {
    /* Window is wider - pillarbox */
    scale_y = (double)window_height / renderer->display_height;
    scale_x = scale_y;
    offset_x = (window_width - (renderer->display_width * scale_x)) / 2.0;
  }
  else
  {
    /* Window is taller - letterbox */
    scale_x = (double)window_width / renderer->display_width;
    scale_y = scale_x;
    offset_y = (window_height - (renderer->display_height * scale_y)) / 2.0;
  }

  display_rect.x = (int)(original_rect->x * scale_x + offset_x);
  display_rect.y = (int)(original_rect->y * scale_y + offset_y);
  display_rect.w = (int)(original_rect->w * scale_x);
  display_rect.h = (int)(original_rect->h * scale_y);
  return display_rect;
}


void
subtitle_renderer_render(SubtitleRenderer_t* renderer, SDL_Renderer* sdl_renderer, int window_width, int window_height)
{
  if (!renderer ||
      !sdl_renderer)
  {
    return;
  }

  pthread_mutex_lock(&renderer->mutex);

  /* Render all regions */
  for (int i = 0; i < renderer->region_count; i++)
  {
    SubtitleRegion_t* region = &renderer->regions[i];

    /* Skip regions with NULL textures (safety check) */
    if (!region->texture)
    {
      continue;
    }

    /* Calculate scaled display position */
    SDL_Rect display_rect = subtitle_renderer_calculate_display_rect(renderer,
                                                                     &region->original_rect,
                                                                     window_width,
                                                                     window_height);

    /* Set blend mode for proper alpha blending */
    SDL_SetTextureBlendMode(region->texture, SDL_BLENDMODE_BLEND);
    SDL_RenderCopy(sdl_renderer, region->texture, NULL, &display_rect);
  }

  pthread_mutex_unlock(&renderer->mutex);
}
