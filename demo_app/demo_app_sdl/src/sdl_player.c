/*-----------------------------------------------------------------------------
 * sdl_player.c
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
 * @file sdl_player.c
 * @brief SDL-based subtitle player implementation
 *
 * @section overview Overview
 *
 * SDL player implementation for DVB subtitle playback. Manages decoder
 * lifecycle, PES file processing, and OSD rendering callbacks.
 *
 * @section pes_processing PES Processing
 *
 * 1. Load entire PES file into memory
 * 2. Find PES packets (start code 0x000001, stream ID 0xBD/0xBE)
 * 3. Process packets at fixed interval in main thread
 * 4. Feed to decoder via LS_DVBSubDecServicePlay()
 *
 * @section callbacks Decoder Callbacks
 *
 * - clean_region_callback: Clear rectangular areas
 * - draw_pixmap_callback: Render subtitle pixmaps
 * - dds_notify_callback: Display dimension updates
 * - get_pcr_callback: Get PCR timestamp
 *
 * @section pes_efficiency PES Processing Strategies
 *
 * @subsection incremental Incremental Processing (Demo)
 *
 * @code
 * // Slow but simple: 500ms per packet
 * while (running) {
 *     check_decoder_timers();
 *     process_one_packet();
 *     render_subtitles();
 *     SDL_Delay(10);
 * }
 * @endcode
 *
 * @subsection batch Batch Processing (Production)
 *
 * @code
 * // Process multiple packets per frame
 #define MAX_PACKETS_PER_FRAME 10
 * #define TIME_BUDGET_MS 16  // Don't exceed 16ms per frame
 *
 * Uint64 start = SDL_GetTicks64();
 * int processed = 0;
 *
 * while (processed < MAX_PACKETS_PER_FRAME &&
 *        (SDL_GetTicks64() - start) < TIME_BUDGET_MS) {
 *     if (process_one_packet() <= 0) break;
 *     processed++;
 * }
 * @endcode
 *
 * @subsection timing Synchronization
 *
 * For real-time apps, synchronize with video PCR:
 *
 * @code
 * // Feed PES data synchronized with video playback
 * void on_video_frame(uint64_t video_pts) {
 *     while (current_pes_pts < video_pts) {
 *         process_one_packet();
 *     }
 *     check_decoder_timers();  // Update subtitle display
 * }
 * @endcode
 */

#include "sdl_player.h"
#include "pes_queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*-----------------------------------------------------------------------------
 * PES Stream Constants
 *---------------------------------------------------------------------------*/
#define PES_START_CODE            0x000001
#define PES_STREAM_ID_SUBTITLE    0xBD
#define PRIVATE_STREAM_1          0xBD
/*-----------------------------------------------------------------------------
 * Forward Declarations
 *---------------------------------------------------------------------------*/
static uint32_t sdl_timer_callback(uint32_t interval, void* param);

/*-----------------------------------------------------------------------------
 * Private Functions
 *---------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------
 * Helper Functions
 *---------------------------------------------------------------------------*/
/**
 * @brief Find next PES packet start code
 */
static size_t
find_pes_packet(const uint8_t* data, size_t size, size_t offset)
{
  while (offset + 3 < size)
  {
    if ((data[offset] == 0x00) &&
        (data[offset + 1] == 0x00) &&
        (data[offset + 2] == 0x01))
    {
      return offset;
    }

    offset++;
  }

  return size;   /* Not found */
}


/**
 * @brief Get PES packet size from header
 */
static size_t
get_pes_packet_size(const uint8_t* data, size_t size)
{
  if (size < 6)
  {
    return 0;
  }

  /* PES packet length is at offset 4-5 */
  size_t pes_length = (data[4] << 8) | data[5];

  /* Add 6 bytes for header (start code + length field) */
  return pes_length + 6;
}


/*-----------------------------------------------------------------------------
 * Public Functions
 *---------------------------------------------------------------------------*/
SDL_Player_t*
sdl_player_create(int width, int height)
{
  SDL_Player_t* player = calloc(1, sizeof(SDL_Player_t));

  if (!player)
  {
    return NULL;
  }

  player->display_width = width;
  player->display_height = height;
  player->dds_width = width;
  player->dds_height = height;
  player->dds_received = false;
  player->is_running = false;
  player->is_paused = false;
  player->current_pcr = 0;
  player->feed_timer = 0;
  player->feed_complete = false;
  player->processing_complete = false;
  player->feed_interval_ms = DEFAULT_FEED_INTERVAL;
  player->pes_queue = NULL;
  player->feed_thread = 0;
  /* Create PES queue */
  player->pes_queue = pes_queue_create(PES_QUEUE_CAPACITY);

  if (!player->pes_queue)
  {
    free(player);
    return NULL;
  }

  /* Create subtitle renderer */
  player->subtitle_renderer = subtitle_renderer_create();

  if (!player->subtitle_renderer)
  {
    free(player);
    return NULL;
  }

  /* Create window with ALWAYS_ON_TOP to ensure visibility */
  player->window = SDL_CreateWindow("DVB Subtitle Player (SDL)",
                                    SDL_WINDOWPOS_CENTERED,
                                    SDL_WINDOWPOS_CENTERED,
                                    width,
                                    height,
                                    SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALWAYS_ON_TOP);

  if (!player->window)
  {
    fprintf(stderr, "Error: Failed to create window: %s\n", SDL_GetError());
    subtitle_renderer_destroy(player->subtitle_renderer);
    free(player);
    return NULL;
  }

  /* Create renderer */
  player->renderer = SDL_CreateRenderer(player->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

  if (!player->renderer)
  {
    fprintf(stderr, "Error: Failed to create renderer: %s\n", SDL_GetError());
    SDL_DestroyWindow(player->window);
    subtitle_renderer_destroy(player->subtitle_renderer);
    free(player);
    return NULL;
  }

  /* Create decoder service */
  LS_ServiceMemCfg_t memCfg =
  {
    .codedDataBufferSize = 512 * 1024, .pixelBufferSize = 16 * 1024 * 1024, .compositionBufferSize = 2 * 1024 * 1024
  };

  player->service = LS_DVBSubDecServiceNew(memCfg);

  if (!player->service)
  {
    fprintf(stderr, "Error: Failed to create decoder service\n");
    SDL_DestroyRenderer(player->renderer);
    SDL_DestroyWindow(player->window);
    subtitle_renderer_destroy(player->subtitle_renderer);
    free(player);
    return NULL;
  }

  /* Setup OSD rendering callbacks */
  LS_OSDRender_t osdRender = { 0 };

  osdRender.cleanRegionFunc = sdl_player_clean_region_callback;
  osdRender.cleanRegionFuncData = player;
  osdRender.drawPixmapFunc = sdl_player_draw_pixmap_callback;
  osdRender.drawPixmapFuncData = player;
  osdRender.ddsNotifyFunc = sdl_player_dds_notify_callback;
  osdRender.ddsNotifyFuncData = player;
  osdRender.getCurrentPCRFunc = sdl_player_get_pcr_callback;
  osdRender.getCurrentPCRFuncData = player;
  osdRender.OSDPixmapFormat = LS_PIXFMT_ARGB32;       // Use ARGB32 to match Qt demo
  osdRender.alphaValueFullTransparent = 0;
  osdRender.alphaValueFullOpaque = 255;
  /* NOTE: PTS sync disabled for demo - would require actual video playback timing */
  osdRender.ptsSyncEnabled = 0;           // Disable PTS sync (no video timeline in demo)
  osdRender.ptsSyncTolerance = 100;       // Max late tolerance: 100ms
  osdRender.ptsMaxEarlyDisplay = 50;      // Max early display: 50ms

  LS_Status status = LS_DVBSubDecServiceStart(player->service, osdRender);

  if (status != LS_OK)
  {
    fprintf(stderr, "Error: Failed to start decoder service (%d)\n", status);
    LS_DVBSubDecServiceDelete(player->service);
    SDL_DestroyRenderer(player->renderer);
    SDL_DestroyWindow(player->window);
    subtitle_renderer_destroy(player->subtitle_renderer);
    free(player);
    return NULL;
  }

  return player;
}


void
sdl_player_destroy(SDL_Player_t* player)
{
  if (!player)
  {
    return;
  }

  /* Stop PES feeding if still active */
  if (player->feed_timer ||
      player->feed_thread)
  {
    sdl_player_stop_feeding(player);
  }

  if (player->service)
  {
    LS_DVBSubDecServiceStop(player->service);
    LS_DVBSubDecServiceDelete(player->service);
  }

  if (player->pes_buffer)
  {
    free(player->pes_buffer);
  }

  /* Destroy PES queue */
  if (player->pes_queue)
  {
    pes_queue_destroy(player->pes_queue);
  }

  if (player->subtitle_renderer)
  {
    subtitle_renderer_destroy(player->subtitle_renderer);
  }

  if (player->renderer)
  {
    SDL_DestroyRenderer(player->renderer);
  }

  if (player->window)
  {
    SDL_DestroyWindow(player->window);
  }

  free(player);
}


LS_Status
sdl_player_load_pes(SDL_Player_t* player, const char* filename)
{
  if (!player ||
      !filename)
  {
    return LS_ERROR_GENERAL;
  }

  FILE* fp = fopen(filename, "rb");

  if (!fp)
  {
    fprintf(stderr, "Error: Failed to open file: %s\n", filename);
    return LS_ERROR_STREAM_DATA;
  }

  /* Get file size */
  fseek(fp, 0, SEEK_END);

  long file_size = ftell(fp);

  fseek(fp, 0, SEEK_SET);

  if ((file_size <= 0) ||
      (file_size > MAX_PES_BUFFER_SIZE))
  {
    fprintf(stderr, "Error: Invalid file size: %ld\n", file_size);
    fclose(fp);
    return LS_ERROR_STREAM_DATA;
  }

  /* Allocate buffer */
  player->pes_buffer = malloc(file_size);

  if (!player->pes_buffer)
  {
    fprintf(stderr, "Error: Failed to allocate buffer\n");
    fclose(fp);
    return LS_ERROR_SYSTEM_BUFFER;
  }

  /* Read file */
  size_t read_size = fread(player->pes_buffer, 1, file_size, fp);

  fclose(fp);

  if (read_size != (size_t)file_size)
  {
    fprintf(stderr, "Error: Failed to read complete file\n");
    free(player->pes_buffer);
    player->pes_buffer = NULL;
    return LS_ERROR_STREAM_DATA;
  }

  player->pes_size = read_size;
  player->processing_pos = 0;
  printf("  Loaded %zu bytes\n", player->pes_size);
  return LS_OK;
}


int
sdl_player_process_one_packet(SDL_Player_t* player)
{
  if (!player ||
      !player->pes_buffer)
  {
    return -1;
  }

  /* Find next PES packet */
  size_t packet_start = find_pes_packet(player->pes_buffer, player->pes_size, player->processing_pos);

  if (packet_start >= player->pes_size)
  {
    /* End of file */
    return 0;
  }

  /* Check stream ID (offset 3) */
  uint8_t stream_id = player->pes_buffer[packet_start + 3];
  /* Get packet size */
  size_t packet_size = get_pes_packet_size(player->pes_buffer + packet_start, player->pes_size - packet_start);

  if ((packet_size == 0) ||
      (packet_start + packet_size > player->pes_size))
  {
    /* Invalid packet */
    return -1;
  }

  if ((stream_id == PRIVATE_STREAM_1) ||
      (stream_id == 0xBE))
  {
    /* This is a subtitle stream, feed to decoder */
    LS_CodedData_t pesData =
    {
      .data = player->pes_buffer + packet_start, .dataSize = packet_size
    };
    LS_PageId_t pageId = { 0, 0 }; /* Wildcard for any page */
    LS_Status status = LS_DVBSubDecServicePlay(player->service, &pesData, pageId);

    (void)status;                  /* Status logged by decoder */
  }

  /* Move to next packet */
  player->processing_pos = packet_start + packet_size;
  return 1;
}


/* Manual PES processing timer state */
static uint64_t g_pes_target_time_ms = 0;
static const uint32_t PES_INTERVAL_MS = 50;

/* Check and process PES packet (call from main thread) */
static void
check_and_process_pes(SDL_Player_t* player)
{
  if (!player ||
      player->processing_complete ||
      player->is_paused)
  {
    return;
  }

  uint64_t now = SDL_GetTicks64();

  if (now >= g_pes_target_time_ms)
  {
    /* Process one packet */
    int result = sdl_player_process_one_packet(player);

    if (result < 0)
    {
      /* Error - stop processing */
      player->processing_complete = true;
    }
    else if (result == 0)
    {
      /* End of file - stop processing
       * Note: Decoder timers are still active and will fire to display subtitles
       */
      player->processing_complete = true;
    }

    /* Schedule next processing */
    g_pes_target_time_ms = now + PES_INTERVAL_MS;
  }
}


/* Forward declaration for timer checker from main.c */
extern void check_and_fire_timers(void);

LS_Status
sdl_player_run(SDL_Player_t* player)
{
  if (!player)
  {
    return LS_ERROR_GENERAL;
  }

  player->is_running = true;
  player->is_paused = false;
  player->processing_complete = false;

  /* Initialize PES processing timer */
  g_pes_target_time_ms = SDL_GetTicks64() + PES_INTERVAL_MS;

  printf("Starting playback...\n");
  printf("Press ESC or close window to exit.\n\n");
  fflush(stdout);

  /* Main event loop - Process PES packets and decoder timers in main thread */
  SDL_Event event;

  while (player->is_running)
  {
    /* Handle events */
    while (SDL_PollEvent(&event))
    {
      switch (event.type)
      {
        case SDL_QUIT:
          player->is_running = false;
          break;

        case SDL_USEREVENT:
          break;

        case SDL_KEYDOWN:

          if (event.key.keysym.sym == SDLK_ESCAPE)
          {
            player->is_running = false;
          }
          else if (event.key.keysym.sym == SDLK_SPACE)
          {
            player->is_paused = !player->is_paused;
          }

          break;

        case SDL_WINDOWEVENT:

          if (event.window.event == SDL_WINDOWEVENT_RESIZED)
          {
            player->display_width = event.window.data1;
            player->display_height = event.window.data2;
          }

          break;

        default:
          break;
      }
    }

    /* Check and fire decoder timers (thread-safe, main thread only) */
    check_and_fire_timers();

    /* Process PES packets incrementally (allows decoder timers to fire between packets) */
    check_and_process_pes(player);

    /* Clear screen with black background */
    SDL_SetRenderDrawColor(player->renderer, 0, 0, 0, 255);
    SDL_RenderClear(player->renderer);
    /* Render subtitles */
    subtitle_renderer_render(player->subtitle_renderer,
                             player->renderer,
                             player->display_width,
                             player->display_height);
    /* Present */
    SDL_RenderPresent(player->renderer);
    /* Small delay to prevent tight loop */
    SDL_Delay(10);
  }

  return LS_OK;
}


/*-----------------------------------------------------------------------------
 * Callback Functions (called by decoder)
 *---------------------------------------------------------------------------*/
LS_Status
sdl_player_clean_region_callback(LS_Rect_t rect, void* user_data)
{
  SDL_Player_t* player = (SDL_Player_t*)user_data;

  if (!player)
  {
    return LS_ERROR_GENERAL;
  }

  subtitle_renderer_clear_region(player->subtitle_renderer, rect.leftPos, rect.topPos, rect.rightPos, rect.bottomPos);
  return LS_OK;
}


LS_Status
sdl_player_draw_pixmap_callback(const LS_Pixmap_t* pixmap,
                                const uint8_t*     palette,
                                const uint8_t      palette_num,
                                void*              user_data)
{
  (void)palette_num;

  SDL_Player_t* player = (SDL_Player_t*)user_data;

  if (!player ||
      !pixmap)
  {
    return LS_ERROR_GENERAL;
  }

  /* Pass palette to renderer for PALETTE8BIT format */
  return subtitle_renderer_add_pixmap(player->subtitle_renderer, pixmap, palette, player->renderer);
}


LS_Status
sdl_player_dds_notify_callback(uint16_t displayWidth, uint16_t displayHeight, void* user_data)
{
  SDL_Player_t* player = (SDL_Player_t*)user_data;

  if (!player)
  {
    return LS_ERROR_GENERAL;
  }

  player->dds_width = displayWidth;
  player->dds_height = displayHeight;
  player->dds_received = true;
  /* Update renderer display size */
  subtitle_renderer_set_display_size(player->subtitle_renderer, displayWidth, displayHeight);
  return LS_OK;
}


LS_Status
sdl_player_get_pcr_callback(uint64_t* current_PCR, void* user_data)
{
  SDL_Player_t* player = (SDL_Player_t*)user_data;

  if (!player ||
      !current_PCR)
  {
    return LS_ERROR_GENERAL;
  }

  *current_PCR = player->current_pcr;
  return LS_OK;
}


/*-----------------------------------------------------------------------------
 * PES Feeding Thread
 *---------------------------------------------------------------------------*/
/**
 * @brief Thread function that feeds PES packets to the queue
 */
static void*
pes_feeding_thread(void* param)
{
  SDL_Player_t* player = (SDL_Player_t*)param;

  if (!player ||
      !player->pes_buffer)
  {
    return NULL;
  }

  /* Find and enqueue all PES packets */
  size_t pos = 0;

  while (pos < player->pes_size &&
         !player->feed_complete)
  {
    /* Find next PES packet */
    size_t packet_start = find_pes_packet(player->pes_buffer, player->pes_size, pos);

    if (packet_start >= player->pes_size)
    {
      break;       /* End of file */
    }

    /* Get packet size */
    size_t packet_size = get_pes_packet_size(player->pes_buffer + packet_start, player->pes_size - packet_start);

    if ((packet_size == 0) ||
        (packet_start + packet_size > player->pes_size))
    {
      break;       /* Invalid packet */
    }

    /* Check stream ID (offset 3) - only process subtitle streams */
    uint8_t stream_id = player->pes_buffer[packet_start + 3];

    if ((stream_id == PRIVATE_STREAM_1) ||
        (stream_id == 0xBE))
    {
      /* Enqueue packet */
      if (!pes_queue_enqueue(player->pes_queue, player->pes_buffer + packet_start, packet_size))
      {
        break;         /* Queue shutdown or error */
      }
    }

    /* Move to next packet */
    pos = packet_start + packet_size;
  }

  return NULL;
}


/*-----------------------------------------------------------------------------
 * PES Feeding Control Functions
 *---------------------------------------------------------------------------*/
/**
 * @brief Start PES feeding thread
 */
LS_Status
sdl_player_start_feeding(SDL_Player_t* player)
{
  if (!player ||
      !player->pes_queue)
  {
    return LS_ERROR_GENERAL;
  }

  /* Reset feed complete flag */
  player->feed_complete = false;

  /* Create feeding thread */
  if (pthread_create(&player->feed_thread, NULL, pes_feeding_thread, player) != 0)
  {
    fprintf(stderr, "Error: Failed to create feeding thread\n");
    return LS_ERROR_SYSTEM_ERROR;
  }

  /* Create timer to feed packets to decoder at specified interval */
  player->feed_timer = SDL_AddTimer(player->feed_interval_ms, sdl_timer_callback, player);

  if (!player->feed_timer)
  {
    fprintf(stderr, "Error: Failed to create feed timer\n");
    player->feed_complete = true;
    return LS_ERROR_SYSTEM_ERROR;
  }

  printf("  [feed] Started PES feeding (interval: %u ms)\n", player->feed_interval_ms);
  return LS_OK;
}


/**
 * @brief Stop PES feeding thread
 */
void
sdl_player_stop_feeding(SDL_Player_t* player)
{
  if (!player)
  {
    return;
  }

  /* Signal shutdown */
  player->feed_complete = true;

  /* Shutdown queue */
  if (player->pes_queue)
  {
    pes_queue_shutdown(player->pes_queue);
  }

  /* Stop timer */
  if (player->feed_timer)
  {
    SDL_RemoveTimer(player->feed_timer);
    player->feed_timer = 0;
  }

  /* Wait for thread to finish */
  if (player->feed_thread)
  {
    pthread_join(player->feed_thread, NULL);
    player->feed_thread = 0;
  }

  printf("  [feed] Stopped PES feeding\n");
}


/*-----------------------------------------------------------------------------
 * SDL Timer Callback for PES Feeding
 *---------------------------------------------------------------------------*/
/**
 * @brief SDL timer callback that feeds one packet from queue to decoder
 */
static uint32_t
sdl_timer_callback(uint32_t interval, void* param)
{
  (void)param;
  (void)interval;
  /* Not used in synchronous processing mode */
  return 0;
}
