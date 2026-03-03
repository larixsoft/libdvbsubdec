/*-----------------------------------------------------------------------------
 * main.c
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
 * @file main.c
 * @brief DVB Subtitle Player - SDL Demo Application
 *
 * @section overview Overview
 *
 * This demo application demonstrates how to integrate the DVB subtitle decoder
 * library into a multimedia application using SDL2 for rendering. It serves as
 * a reference implementation for developers who want to add DVB subtitle
 * support to their applications.
 *
 * @section key_features Key Features
 *
 * - SDL2-based rendering with hardware acceleration
 * - Manual timer system for thread-safe decoder integration
 * - POSIX mutex implementation for decoder synchronization
 * - PES (Packetized Elementary Stream) file playback
 * - Proper shutdown handling to prevent decoder callbacks during cleanup
 *
 * @section architecture Architecture
 *
 * The application follows this flow:
 *
 * 1. Initialization (main function)
 *    - Initialize SDL2 with video subsystem
 *    - Register POSIX system functions with decoder library
 *    - Create SDL player (window, renderer, decoder service)
 *    - Load PES file into memory
 *
 * 2. Playback (sdl_player_run)
 *    - Main event loop processes SDL events
 *    - Manual timer system checks and fires decoder timers
 *    - PES packets processed at fixed intervals (500ms)
 *    - Subtitle renderer displays active regions
 *
 * 3. Cleanup
 *    - Set shutdown flag to prevent timer callbacks
 *    - Destroy SDL player and decoder service
 *    - Finalize decoder library
 *    - Quit SDL
 *
 * @section best_practices Best Practices to Prevent Crashes
 *
 * @subsection timers_avoid_sdl_addtimer AVOID: SDL_AddTimer
 *
 * SDL_AddTimer callbacks do NOT run in the main thread. This causes crashes
 * because the decoder library is NOT thread-safe. Use a manual timer system
 * that checks timer expiry in the main event loop.
 *
 * @subsection timer_validation Timer Pointer Validation
 *
 * Use magic numbers (0xDEADBEEF) to validate timer pointers before access,
 * preventing use-after-free bugs when timers are deleted.
 *
 * @subsection callback_safety Safe Timer Callback Execution
 *
 * Save callback pointers locally before calling, in case the timer is
 * deleted during callback execution.
 *
 * @subsection shutdown_order Shutdown Order
 *
 * CRITICAL: Set shutdown flag BEFORE cleanup:
 * 1. g_shutting_down = true
 * 2. Destroy player (stops decoder)
 * 3. LS_DVBSubDecFinalize()
 *
 * @subsection pixel_format Correct Pixel Format
 *
 * Use SDL_PIXELFORMAT_BGRA8888 to match decoder's LS_PIXFMT_ARGB32 output format.
 *
 * The decoder's LS_PIXFMT_ARGB32 uses big-endian notation (A-R-G-B), which on
 * little-endian systems is stored in memory as B-G-R-A bytes. SDL_PIXELFORMAT_BGRA8888
 * matches this memory layout.
 *
 * @section thread_safety Thread Safety
 *
 * The DVB subtitle decoder library is NOT thread-safe. All decoder callbacks
 * must execute in the same thread that initialized the decoder.
 *
 * @section integration Integrating with Your Application
 *
 * 1. Include "lssubdec.h"
 * 2. Implement system functions (mutex, timer, timestamp)
 * 3. Implement OSD callbacks (cleanRegion, drawPixmap, ddsNotify, getPCR)
 * 4. Initialize: LS_DVBSubDecInit(buffer_size, system_functions)
 * 5. Create service: LS_DVBSubDecServiceNew(memCfg)
 * 6. Start: LS_DVBSubDecServiceStart(service, osdRender)
 * 7. Feed data: LS_DVBSubDecServicePlay(service, &pesData, pageId)
 * 8. Cleanup: LS_DVBSubDecServiceStop/Delete, LS_DVBSubDecFinalize()
 *
 * @section building Building
 *
 * @code
 * mkdir build && cd build
 * cmake -DBUILD_APP=ON ..
 * make dvbplayer_sdl
 * @endcode
 *
 * @section usage Usage
 *
 * @code
 * ./dvbplayer_sdl <path_to_pes_file>
 * @endcode
 *
 * @section controls Controls
 *
 * - ESC: Exit
 * - SPACE: Pause/Resume
 * - Close Window: Exit
 *
 * @section crash_prevention Crash Prevention Best Practices
 *
 * The following practices are CRITICAL to avoid crashes:
 *
 * @subsection timer_safe Thread-Safe Timer Implementation
 *
 * @b Problem: SDL_AddTimer runs callbacks in separate thread
 *
 * SDL_AddTimer implementation varies by platform:
 * - Linux: Uses timer_create() with SIGEV_THREAD → separate thread
 * - Windows: Uses multimedia timers → thread pool
 * - MacOS: Uses CFRunLoopRunInMode → timer thread
 *
 * The decoder library is NOT thread-safe. Access from multiple threads
 * causes data races and crashes.
 *
 * @b Solution: Manual timer system in main thread
 *
 * @code
 * // Store timer with target time (not SDL_AddTimer!)
 * timer_info->target_time_ms = SDL_GetTicks64() + delay;
 * timer_info->active = true;
 *
 * // In main event loop, check and fire expired timers:
 * void check_and_fire_timers(void) {
 *     if (g_shutting_down) return;  // Prevent callbacks during cleanup
 *     Uint64 now = SDL_GetTicks64();
 *     for (each timer) {
 *         if (timer->active && now >= timer->target_time_ms) {
 *             timer->active = false;
 *             saved_callback(timer->param);  // Use saved pointers
 *         }
 *     }
 * }
 * @endcode
 *
 * @subsection timer_validation Validate Timer Pointers
 *
 * Use magic numbers (0xDEADBEEF) to detect freed timers:
 *
 * @code
 * #define TIMER_MAGIC 0xDEADBEEF
 *
 * typedef struct {
 *     uint32_t magic;  // Set to TIMER_MAGIC on create, clear to 0 on delete
 *     void (*callback)(void*);
 *     void* param;
 *     bool active;
 * } TimerInfo_t;
 *
 * static bool is_timer_valid(TimerInfo_t* info) {
 *     return info && info->magic == TIMER_MAGIC;
 * }
 *
 * // Before accessing timer:
 * if (!is_timer_valid(info)) continue;
 * @endcode
 *
 * @subsection shutdown_sequence Shutdown Sequence
 *
 * CRITICAL: Set shutdown flag FIRST, before cleanup:
 *
 * @code
 * // CORRECT ORDER:
 * g_shutting_down = true;      // STEP 1: Prevent callbacks
 * sdl_player_destroy(player);  // STEP 2: Stop decoder
 * LS_DVBSubDecFinalize();      // STEP 3: Finalize library
 * SDL_Quit();                  // STEP 4: Cleanup SDL
 *
 * // WRONG: (will crash)
 * sdl_player_destroy(player);  // May trigger callbacks
 * g_shutting_down = true;         // Too late
 * @endcode
 *
 * @subsection pixel_format Pixel Format
 *
 * Use SDL_PIXELFORMAT_BGRA8888 to match decoder's LS_PIXFMT_ARGB32.
 *
 * The decoder's LS_PIXFMT_ARGB32 uses big-endian notation (A-R-G-B), which on
 * little-endian systems is stored in memory as B-G-R-A. SDL_PIXELFORMAT_BGRA8888
 * matches this memory layout.
 *
 * @code
 * // CORRECT: (matches decoder output on little-endian systems)
 * SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_BGRA8888);
 *
 * // WRONG: (colors appear wrong - byte order mismatch)
 * SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
 * @endcode
 *
 * @subsection thread_affinity Thread Affinity
 *
 * ALL decoder operations MUST run in same thread:
 *
 * @code
 * // Thread A (main): Init, feed data, callbacks, cleanup
 * LS_DVBSubDecInit(...);
 * LS_DVBSubDecServiceNew(...);
 * LS_DVBSubDecServiceStart(...);
 * // ... event loop with check_and_fire_timers() ...
 * LS_DVBSubDecServiceStop(...);
 * LS_DVBSubDecServiceDelete(...);
 * LS_DVBSubDecFinalize();
 *
 * // NEVER: Call decoder from other threads
 * // WRONG: SDL_AddTimer callback (separate thread)
 * // WRONG: Worker thread calling decoder functions
 * @endcode
 */

#include "sdl_player.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
/*-----------------------------------------------------------------------------
 * Global Variables
 *---------------------------------------------------------------------------*/
static SDL_Player_t* g_player = NULL;

/*-----------------------------------------------------------------------------
 * Signal Handler
 *---------------------------------------------------------------------------*/
static void
signal_handler(int sig)
{
  (void)sig;    /* Unused */

  if (g_player)
  {
    g_player->is_running = false;
  }
}


/*-----------------------------------------------------------------------------
 * Manual Timer Functions for Decoder (thread-safe main thread implementation)
 *---------------------------------------------------------------------------*/
#define MAX_TIMERS    1024

typedef struct
{
  void (*callback)(void* param);
  void*       param;
  uint32_t    id;
  uint64_t    target_time_ms;  /* Target time for timer to fire */
  bool        active;
} TimerInfo_t;

static TimerInfo_t* g_timers[MAX_TIMERS] = { 0 };
static uint32_t g_next_timer_id = 0;


static int32_t
stub_timer_create(LS_Timer_t* timer_id, void (*callback_func) (void*), void* param)
{
  if (!timer_id ||
      !callback_func)
  {
    return LS_ERROR_GENERAL;
  }

  /* Check if we've run out of timer IDs */
  if (g_next_timer_id >= MAX_TIMERS)
  {
    return LS_ERROR_SYSTEM_ERROR;
  }

  TimerInfo_t* info = calloc(1, sizeof(TimerInfo_t));

  if (!info)
  {
    return LS_ERROR_SYSTEM_ERROR;
  }

  info->callback = callback_func;
  info->param = param;
  info->id = g_next_timer_id;
  info->target_time_ms = 0;
  info->active = false;
  g_timers[g_next_timer_id++] = info;
  *timer_id = (LS_Timer_t)info;
  return LS_OK;
}


static int32_t
stub_timer_delete(LS_Timer_t timer_id)
{
  TimerInfo_t* info = (TimerInfo_t*)timer_id;

  if (!info)
  {
    return LS_ERROR_GENERAL;
  }

  /* Remove from global array */
  if (info->id < MAX_TIMERS)
  {
    g_timers[info->id] = NULL;
  }

  free(info);
  return LS_OK;
}


static int32_t
stub_timer_start(LS_Timer_t timer_id, uint32_t time_in_millisec)
{
  TimerInfo_t* info = (TimerInfo_t*)timer_id;

  if (!info ||
      !info->callback)
  {
    return LS_ERROR_GENERAL;
  }

  info->active = true;
  info->target_time_ms = SDL_GetTicks64() + time_in_millisec;
  return LS_OK;
}


static int32_t
stub_timer_stop(LS_Timer_t timer_id, LS_Time_t* time_left)
{
  (void)time_left;

  TimerInfo_t* info = (TimerInfo_t*)timer_id;

  if (!info)
  {
    return LS_ERROR_GENERAL;
  }

  info->active = false;
  return LS_OK;
}


/* Check and fire expired timers (call from main thread) */
void
check_and_fire_timers(void)
{
  uint64_t now = SDL_GetTicks64();

  for (uint32_t i = 0; i < g_next_timer_id; i++)
  {
    TimerInfo_t* info = g_timers[i];

    if (!info ||
        !info->active)
    {
      continue;
    }

    if (now >= info->target_time_ms)
    {
      /* Save callback and param before deactivating */
      void (*callback)(void*) = info->callback;
      void* param = info->param;

      /* Mark as inactive */
      info->active = false;

      /* Call the callback */
      if (callback)
      {
        callback(param);
      }
    }
  }
}


/* Reset timer callback depth (no longer needed) */
void
reset_timer_callback_depth(void)
{
  /* Nothing to do */
}


/* Execute all timer callbacks (no longer needed) */
void
execute_all_timer_callbacks(void)
{
  /* Timers are now checked and fired in check_and_fire_timers() */
}


static char*
stub_get_timestamp(void)
{
  static char timestamp[32];

  snprintf(timestamp, sizeof(timestamp), "%ld", (long)time(NULL));
  return timestamp;
}


/*-----------------------------------------------------------------------------
 * pthread Functions for Decoder
 *---------------------------------------------------------------------------*/
static int32_t
pthread_mutex_create(LS_Mutex_t* mutex)
{
  pthread_mutex_t* m = malloc(sizeof(pthread_mutex_t));

  if (!m)
  {
    return LS_ERROR_SYSTEM_ERROR;
  }

  if (pthread_mutex_init(m, NULL) != 0)
  {
    free(m);
    return LS_ERROR_SYSTEM_ERROR;
  }

  *mutex = m;
  return LS_OK;
}


static int32_t
pthread_mutex_delete(LS_Mutex_t mutex)
{
  if (!mutex)
  {
    return LS_ERROR_GENERAL;
  }

  pthread_mutex_t* m = (pthread_mutex_t*)mutex;

  pthread_mutex_destroy(m);
  free(m);
  return LS_OK;
}


static int32_t
pthread_mutex_wait(LS_Mutex_t mutex)
{
  if (!mutex)
  {
    return LS_ERROR_GENERAL;
  }

  pthread_mutex_t* m = (pthread_mutex_t*)mutex;

  return (pthread_mutex_lock(m) == 0) ? LS_OK : LS_ERROR_SYSTEM_ERROR;
}


static int32_t
pthread_mutex_signal(LS_Mutex_t mutex)
{
  if (!mutex)
  {
    return LS_ERROR_GENERAL;
  }

  pthread_mutex_t* m = (pthread_mutex_t*)mutex;

  return (pthread_mutex_unlock(m) == 0) ? LS_OK : LS_ERROR_SYSTEM_ERROR;
}


/*-----------------------------------------------------------------------------
 * Main Function
 *---------------------------------------------------------------------------*/
int
main(int argc, char* argv[])
{
  LS_Status status;
  SDL_Player_t* player = NULL;

  printf("DVB Subtitle Player (SDL)\n");
  printf("===========================\n\n");

  /* Check arguments */
  if (argc < 2)
  {
    printf("Usage: %s <pes_file>\n", argv[0]);
    printf("\nExample: %s ../samples/490000000_subtitle_pid_205.pes\n", argv[0]);
    return 1;
  }

  /* Setup signal handlers */
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  /* Initialize SDL with VIDEO support (no TIMER needed - using manual timers) */
  if (SDL_Init(SDL_INIT_VIDEO) < 0)
  {
    fprintf(stderr, "Error: Failed to initialize SDL: %s\n", SDL_GetError());
    return 1;
  }

  /* Initialize decoder library */
  printf("Initializing decoder library...\n");

  LS_SystemFuncs_t sysFuncs = { 0 };

  sysFuncs.mutexCreateFunc = pthread_mutex_create;
  sysFuncs.mutexDeleteFunc = pthread_mutex_delete;
  sysFuncs.mutexWaitFunc = pthread_mutex_wait;
  sysFuncs.mutexSignalFunc = pthread_mutex_signal;
  sysFuncs.timerCreateFunc = stub_timer_create;
  sysFuncs.timerDeleteFunc = stub_timer_delete;
  sysFuncs.timerStartFunc = stub_timer_start;
  sysFuncs.timerStopFunc = stub_timer_stop;
  sysFuncs.getTimeStampFunc = stub_get_timestamp;
  status = LS_DVBSubDecInit(1024 * 1024, sysFuncs);

  if (status != LS_OK)
  {
    fprintf(stderr, "Error: Failed to initialize decoder library (%d)\n", status);
    SDL_Quit();
    return 1;
  }

  /* Create player */
  player = sdl_player_create(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT);

  if (!player)
  {
    fprintf(stderr, "Error: Failed to create player\n");
    LS_DVBSubDecFinalize();
    SDL_Quit();
    return 1;
  }

  g_player = player;
  /* Load PES file */
  printf("Loading PES file: %s\n", argv[1]);
  status = sdl_player_load_pes(player, argv[1]);

  if (status != LS_OK)
  {
    fprintf(stderr, "Error: Failed to load PES file (%d)\n", status);
    sdl_player_destroy(player);
    LS_DVBSubDecFinalize();
    SDL_Quit();
    return 1;
  }

  /* Start playback */
  printf("Starting playback...\n");
  printf("Press ESC or close window to exit.\n\n");
  fflush(stdout);
  status = sdl_player_run(player);
  /* Cleanup */
  printf("\nShutting down...\n");
  sdl_player_destroy(player);
  LS_DVBSubDecFinalize();
  SDL_Quit();
  printf("Done.\n");
  return (status == LS_OK) ? 0 : 1;
}
