/*-----------------------------------------------------------------------------
 * sdl_player.h
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

#ifndef SDL_PLAYER_H
#define SDL_PLAYER_H

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <pthread.h>
#include "lssubdec.h"
#include "subtitle_renderer.h"
#include "pes_queue.h"
/*-----------------------------------------------------------------------------
 * Constants
 *---------------------------------------------------------------------------*/
/** @brief Default window width */
#define DEFAULT_WINDOW_WIDTH     720
/** @brief Default window height */
#define DEFAULT_WINDOW_HEIGHT    576
/** @brief Maximum PES buffer size (500 MB) */
#define MAX_PES_BUFFER_SIZE      (500 * 1024 * 1024)
/** @brief Default PES feeding interval (ms) */
#define DEFAULT_FEED_INTERVAL    100
/** @brief PES queue capacity */
#define PES_QUEUE_CAPACITY       64
/*-----------------------------------------------------------------------------
 * SDL Player Structure
 *---------------------------------------------------------------------------*/
/**
 * @brief Main SDL player state structure
 */
typedef struct
{
  /* SDL components */
  SDL_Window*         window;                 /**< SDL window */
  SDL_Renderer*       renderer;               /**< SDL renderer */
  SDL_TimerID         feed_timer;             /**< Timer for PES feeding */
  /* Decoder components */
  LS_ServiceID_t      service;                /**< Decoder service handle */
  /* PES data and queue */
  uint8_t*            pes_buffer;             /**< PES data buffer */
  size_t              pes_size;               /**< Size of PES data */
  size_t              processing_pos;         /**< Current processing position */
  PES_Queue_t*        pes_queue;              /**< Thread-safe PES packet queue */
  pthread_t           feed_thread;            /**< Thread that feeds packets to queue */
  /* Display state */
  int                 display_width;          /**< Current display width */
  int                 display_height;         /**< Current display height */
  int                 dds_width;              /**< DDS display width */
  int                 dds_height;             /**< DDS display height */
  bool                dds_received;           /**< DDS received flag */
  /* Subtitle renderer */
  SubtitleRenderer_t* subtitle_renderer;      /**< Subtitle renderer instance */
  /* Playback state */
  bool                is_running;             /**< Main loop running flag */
  bool                is_paused;              /**< Paused flag */
  bool                feed_complete;          /**< PES feeding complete flag */
  bool                processing_complete;    /**< PES processing complete flag */
  uint32_t            feed_interval_ms;       /**< PES feeding interval (ms) */
  uint64_t            current_pcr;            /**< Current PCR value */
} SDL_Player_t;
/*-----------------------------------------------------------------------------
 * Functions
 *---------------------------------------------------------------------------*/
/**
 * @brief Create a new SDL player
 * @param width Initial window width
 * @param height Initial window height
 * @return Pointer to new player or NULL on failure
 */
SDL_Player_t* sdl_player_create(int width, int height);

/**
 * @brief Destroy an SDL player and free all resources
 * @param player Pointer to player to destroy
 */
void sdl_player_destroy(SDL_Player_t* player);

/**
 * @brief Load a PES file for playback
 * @param player Pointer to player
 * @param filename Path to PES file
 * @return LS_OK on success, error code otherwise
 */
LS_Status sdl_player_load_pes(SDL_Player_t* player, const char* filename);

/**
 * @brief Main event loop - processes SDL events and renders subtitles
 * @param player Pointer to player
 * @return LS_OK on success, error code otherwise
 */
LS_Status sdl_player_run(SDL_Player_t* player);

/**
 * @brief Process one PES packet (called by timer)
 * @param player Pointer to player
 * @return Number of packets processed, or -1 on error
 */
int sdl_player_process_one_packet(SDL_Player_t* player);

/**
 * @brief Start PES feeding thread
 * @param player Pointer to player
 * @return LS_OK on success, error code otherwise
 */
LS_Status sdl_player_start_feeding(SDL_Player_t* player);

/**
 * @brief Stop PES feeding thread
 * @param player Pointer to player
 */
void sdl_player_stop_feeding(SDL_Player_t* player);

/**
 * @brief Clean region callback (called by decoder)
 * @param rect Region to clear
 * @param user_data Pointer to SDL_Player_t
 * @return LS_OK on success
 */
LS_Status sdl_player_clean_region_callback(LS_Rect_t rect, void* user_data);

/**
 * @brief Draw pixmap callback (called by decoder)
 * @param pixmap Pixmap to draw
 * @param palette Color palette
 * @param palette_num Palette size
 * @param user_data Pointer to SDL_Player_t
 * @return LS_OK on success
 */
LS_Status sdl_player_draw_pixmap_callback(const LS_Pixmap_t* pixmap,
                                          const uint8_t*     palette,
                                          const uint8_t      palette_num,
                                          void*              user_data);

/**
 * @brief DDS notify callback (called by decoder)
 * @param displayWidth Display width from DDS
 * @param displayHeight Display height from DDS
 * @param user_data Pointer to SDL_Player_t
 * @return LS_OK on success
 */
LS_Status sdl_player_dds_notify_callback(uint16_t displayWidth, uint16_t displayHeight, void* user_data);

/**
 * @brief Get current PCR callback (called by decoder)
 * @param current_PCR Pointer to receive PCR value
 * @param user_data Pointer to SDL_Player_t
 * @return LS_OK on success
 */
LS_Status sdl_player_get_pcr_callback(uint64_t* current_PCR, void* user_data);

#endif /* SDL_PLAYER_H */
