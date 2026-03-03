/*-----------------------------------------------------------------------------
 * subtitle_renderer.h
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

#ifndef SUBTITLE_RENDERER_H
#define SUBTITLE_RENDERER_H

#include <SDL2/SDL.h>
#include <pthread.h>
#include "lssubdec.h"
/*-----------------------------------------------------------------------------
 * Subtitle Region Structure
 *---------------------------------------------------------------------------*/
/**
 * @brief Single subtitle region with texture and position information
 */
typedef struct
{
  SDL_Texture* texture;            /**< SDL texture for this region */
  SDL_Rect     original_rect;      /**< Original position from decoder */
  SDL_Rect     display_rect;       /**< Scaled display position */
} SubtitleRegion_t;
/*-----------------------------------------------------------------------------
 * Subtitle Renderer Structure
 *---------------------------------------------------------------------------*/
/**
 * @brief Manages all subtitle regions and rendering state
 */
typedef struct
{
  SubtitleRegion_t* regions;         /**< Array of subtitle regions */
  int               region_count;    /**< Current number of regions */
  int               region_capacity; /**< Maximum number of regions */
  pthread_mutex_t   mutex;           /**< Mutex for thread-safe access */
  int               display_width;   /**< Current display width */
  int               display_height;  /**< Current display height */
} SubtitleRenderer_t;
/*-----------------------------------------------------------------------------
 * Functions
 *---------------------------------------------------------------------------*/
/**
 * @brief Create a new subtitle renderer
 * @return Pointer to new renderer or NULL on failure
 */
SubtitleRenderer_t* subtitle_renderer_create(void);

/**
 * @brief Destroy a subtitle renderer and free all resources
 * @param renderer Pointer to renderer to destroy
 */
void subtitle_renderer_destroy(SubtitleRenderer_t* renderer);

/**
 * @brief Set display dimensions for scaling calculations
 * @param renderer Pointer to renderer
 * @param width Display width in pixels
 * @param height Display height in pixels
 */
void subtitle_renderer_set_display_size(SubtitleRenderer_t* renderer, int width, int height);

/**
 * @brief Clear a rectangular region (removes intersecting subtitles)
 * @param renderer Pointer to renderer
 * @param left Left X coordinate
 * @param top Top Y coordinate
 * @param right Right X coordinate
 * @param bottom Bottom Y coordinate
 */
void subtitle_renderer_clear_region(SubtitleRenderer_t* renderer, int left, int top, int right, int bottom);

/**
 * @brief Add a new subtitle region from decoder pixmap
 * @param renderer Pointer to renderer
 * @param pixmap Pointer to pixmap from decoder
 * @param palette Pointer to color palette (may be NULL)
 * @param sdl_renderer SDL renderer for texture creation
 * @return LS_OK on success, error code otherwise
 */
LS_Status subtitle_renderer_add_pixmap(SubtitleRenderer_t* renderer,
                                       const LS_Pixmap_t*  pixmap,
                                       const uint8_t*      palette,
                                       SDL_Renderer*       sdl_renderer);

/**
 * @brief Render all subtitle regions to SDL renderer
 * @param renderer Pointer to renderer
 * @param sdl_renderer SDL renderer to draw to
 * @param window_width Current window width
 * @param window_height Current window height
 */
void subtitle_renderer_render(SubtitleRenderer_t* renderer,
                              SDL_Renderer*       sdl_renderer,
                              int                 window_width,
                              int                 window_height);

/**
 * @brief Calculate scaled display rectangle for letterbox/pillarbox
 * @param renderer Pointer to renderer
 * @param original_rect Original rectangle from decoder
 * @param window_width Current window width
 * @param window_height Current window height
 * @return Calculated display rectangle
 */
SDL_Rect subtitle_renderer_calculate_display_rect(const SubtitleRenderer_t* renderer,
                                                  const SDL_Rect*           original_rect,
                                                  int                       window_width,
                                                  int                       window_height);

#endif /* SUBTITLE_RENDERER_H */
