/*-----------------------------------------------------------------------------
 * Simple CLI test for DVB subtitle decoder
 *---------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include "lssubdec.h"

static volatile int g_running = 1;

void sigint_handler(int sig) {
    g_running = 0;
}

/*-----------------------------------------------------------------------------
 * pthread mutex functions for Linux
 *---------------------------------------------------------------------------*/

static int32_t pthread_mutex_create(LS_Mutex_t* mutex)
{
    pthread_mutex_t* m = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    if (!m) {
        return LS_ERROR_SYSTEM_ERROR;
    }
    if (pthread_mutex_init(m, NULL) != 0) {
        free(m);
        return LS_ERROR_SYSTEM_ERROR;
    }
    *mutex = m;
    return LS_OK;
}

static int32_t pthread_mutex_delete(LS_Mutex_t mutex)
{
    if (!mutex) {
        return LS_ERROR_GENERAL;
    }
    pthread_mutex_t* m = (pthread_mutex_t*)mutex;
    pthread_mutex_destroy(m);
    free(m);
    return LS_OK;
}

static int32_t pthread_mutex_wait(LS_Mutex_t mutex)
{
    if (!mutex) {
        return LS_ERROR_GENERAL;
    }
    pthread_mutex_t* m = (pthread_mutex_t*)mutex;
    return (pthread_mutex_lock(m) == 0) ? LS_OK : LS_ERROR_SYSTEM_ERROR;
}

static int32_t pthread_mutex_signal(LS_Mutex_t mutex)
{
    if (!mutex) {
        return LS_ERROR_GENERAL;
    }
    pthread_mutex_t* m = (pthread_mutex_t*)mutex;
    return (pthread_mutex_unlock(m) == 0) ? LS_OK : LS_ERROR_SYSTEM_ERROR;
}

/*-----------------------------------------------------------------------------
 * Dummy timer functions for CLI test (immediate callback execution)
 *---------------------------------------------------------------------------*/

// Timer callback info for immediate execution
struct TimerCallbackInfo {
    void (*callback)(void* param);
    void* param;
    uint32_t delay_ms;
    int active;
    int executing;  // Re-entry guard
    int id;  // For tracking
};

static int g_timer_id_counter = 0;

static TimerCallbackInfo g_timer_callbacks[100];
static int g_timer_callback_count = 0;
static int g_in_timer_start = 0;  // Global re-entry guard

static int32_t dummy_timer_create(LS_Timer_t* timer_id, void (*callback_func)(void*), void* param)
{
    printf("[TIMER] dummy_timer_create called - callback=%p, param=%p\n", callback_func, param);
    // Store callback info for immediate execution
    if (g_timer_callback_count < 100) {
        g_timer_callbacks[g_timer_callback_count].callback = callback_func;
        g_timer_callbacks[g_timer_callback_count].param = param;
        g_timer_callbacks[g_timer_callback_count].delay_ms = 0;
        g_timer_callbacks[g_timer_callback_count].active = 1;
        g_timer_callbacks[g_timer_callback_count].executing = 0;
        g_timer_callbacks[g_timer_callback_count].id = g_timer_id_counter++;
        *timer_id = reinterpret_cast<LS_Timer_t>(&g_timer_callbacks[g_timer_callback_count]);
        printf("[TIMER] Created timer ID %d (count=%d)\n", g_timer_callbacks[g_timer_callback_count].id, g_timer_callback_count);
        g_timer_callback_count++;
    } else {
        fprintf(stderr, "ERROR: Timer array full (max 100)!\n");
        *timer_id = NULL;
        return LS_ERROR_GENERAL;
    }
    return LS_OK;
}

static int32_t dummy_timer_delete(LS_Timer_t timer_id)
{
    TimerCallbackInfo* info = reinterpret_cast<TimerCallbackInfo*>(timer_id);
    printf("[TIMER] dummy_timer_delete called - timer=%p\n", info);
    if (info) {
        info->active = 0;
    }
    return LS_OK;
}

static int32_t dummy_timer_start(LS_Timer_t timer_id, uint32_t time_in_millisec)
{
    TimerCallbackInfo* info = reinterpret_cast<TimerCallbackInfo*>(timer_id);
    printf("[TIMER] dummy_timer_start called - %u ms, timer=%p (ID %d), callback=%p, param=%p\n",
           time_in_millisec, info, info ? info->id : -1,
           info ? info->callback : nullptr, info ? info->param : nullptr);
    if (info) {
        info->delay_ms = time_in_millisec;
        // Don't execute immediately - store for later execution in correct order
        printf("[TIMER] Timer %d scheduled for %u ms (will execute after processing)\n", info->id, time_in_millisec);
    }
    return LS_OK;
}

static int32_t dummy_timer_stop(LS_Timer_t timer_id, LS_Time_t* time_left)
{
    TimerCallbackInfo* info = reinterpret_cast<TimerCallbackInfo*>(timer_id);
    printf("[TIMER] dummy_timer_stop called - timer=%p\n", info);
    if (info && time_left) {
        time_left->seconds = 0;
        time_left->milliseconds = info->delay_ms;
    }
    return LS_OK;
}

/*-----------------------------------------------------------------------------
 * OSD callbacks
 *---------------------------------------------------------------------------*/

static LS_Status clean_region_callback(LS_Rect_t rect, void* userData)
{
    printf("[OSD] Clean region: x=%d, y=%d, w=%d, h=%d\n",
           rect.leftPos, rect.topPos,
           rect.rightPos - rect.leftPos,
           rect.bottomPos - rect.topPos);
    return LS_OK;
}

static LS_Status draw_pixmap_callback(const LS_Pixmap_t* pixmap,
                                      const uint8_t* palette,
                                      const uint8_t paletteNum,
                                      void* userData)
{
    printf("[OSD] Draw pixmap: %dx%d @ (%d,%d), format=%d, paletteNum=%d\n",
           pixmap ? pixmap->width : 0,
           pixmap ? pixmap->height : 0,
           pixmap ? pixmap->leftPos : 0,
           pixmap ? pixmap->topPos : 0,
           pixmap ? pixmap->pixelFormat : 0,
           paletteNum);
    if (pixmap) {
        printf("[OSD]   pixmap=%p, pixmap->data=%p, palette=%p\n", pixmap, pixmap->data, palette);
    } else {
        printf("[OSD]   WARNING: pixmap is NULL!\n");
    }
    fflush(stdout);
    return LS_OK;
}

// Current PCR value - for file playback, PCR stays at 0
// The PTS manager normalizes PTS values relative to the first PTS,
// and adds a 2-second lead time for the first subtitle
static LS_Status get_current_pcr_callback(uint64_t* currentPCR, void* userData)
{
    *currentPCR = 0;  // For file playback, PCR is always 0
    printf("[OSD] Get current PCR: 0 (file playback mode)\n");
    return LS_OK;
}

/*-----------------------------------------------------------------------------
 * Main
 *---------------------------------------------------------------------------*/

int main(int argc, char* argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <pes_file> [--verbose]\n", argv[0]);
        return 1;
    }

    const char* pes_file = argv[1];
    int verbose = (argc > 2 && strcmp(argv[2], "--verbose") == 0);

    signal(SIGINT, sigint_handler);

    printf("=== DVB Subtitle Decoder CLI Test ===\n");
    printf("File: %s\n", pes_file);
    printf("Verbose: %s\n", verbose ? "yes" : "no");
    printf("\n");

    // Initialize system functions
    LS_SystemFuncs_t sysFuncs;
    memset(&sysFuncs, 0, sizeof(sysFuncs));
    sysFuncs.mutexCreateFunc = pthread_mutex_create;
    sysFuncs.mutexDeleteFunc = pthread_mutex_delete;
    sysFuncs.mutexWaitFunc = pthread_mutex_wait;
    sysFuncs.mutexSignalFunc = pthread_mutex_signal;
    sysFuncs.timerCreateFunc = dummy_timer_create;
    sysFuncs.timerDeleteFunc = dummy_timer_delete;
    sysFuncs.timerStartFunc = dummy_timer_start;
    sysFuncs.timerStopFunc = dummy_timer_stop;

    printf("Initializing decoder library...\n");
    LS_Status status = LS_DVBSubDecInit(1024 * 1024, sysFuncs);
    if (status != LS_OK) {
        fprintf(stderr, "ERROR: Failed to initialize decoder library: %d\n", status);
        return 1;
    }
    printf("Decoder library initialized\n");

    // Create service
    LS_ServiceMemCfg_t memCfg;
    memCfg.codedDataBufferSize = 512 * 1024;
    memCfg.pixelBufferSize = 16 * 1024 * 1024;  // Increased to handle multiple display sets
    memCfg.compositionBufferSize = 256 * 1024;

    printf("Creating subtitle service...\n");
    LS_ServiceID_t serviceId = LS_DVBSubDecServiceNew(memCfg);
    if (!serviceId) {
        fprintf(stderr, "ERROR: Failed to create subtitle service\n");
        LS_DVBSubDecFinalize();
        return 1;
    }
    printf("Subtitle service created\n");

    // Setup OSD rendering
    LS_OSDRender_t osdRender;
    memset(&osdRender, 0, sizeof(osdRender));
    osdRender.cleanRegionFunc = clean_region_callback;
    osdRender.drawPixmapFunc = draw_pixmap_callback;
    osdRender.getCurrentPCRFunc = get_current_pcr_callback;
    osdRender.cleanRegionFuncData = NULL;
    osdRender.drawPixmapFuncData = NULL;
    osdRender.getCurrentPCRFuncData = NULL;
    osdRender.OSDPixmapFormat = LS_PIXFMT_ARGB32;
    osdRender.alphaValueFullOpaque = 255;
    osdRender.alphaValueFullTransparent = 0;
    osdRender.backgroundColor.colorData.rgbColor.redValue = 0;
    osdRender.backgroundColor.colorData.rgbColor.greenValue = 0;
    osdRender.backgroundColor.colorData.rgbColor.blueValue = 0;
    osdRender.backgroundColor.colorData.rgbColor.alphaValue = 0;

    printf("Starting subtitle service...\n");
    status = LS_DVBSubDecServiceStart(serviceId, osdRender);
    if (status != LS_OK) {
        fprintf(stderr, "ERROR: Failed to start service: %d\n", status);
        LS_DVBSubDecServiceDelete(serviceId);
        LS_DVBSubDecFinalize();
        return 1;
    }
    printf("Subtitle service started\n");

    // Load PES file
    printf("\nLoading PES file: %s\n", pes_file);
    FILE* fp = fopen(pes_file, "rb");
    if (!fp) {
        fprintf(stderr, "ERROR: Cannot open file: %s\n", pes_file);
        LS_DVBSubDecServiceStop(serviceId);
        LS_DVBSubDecServiceDelete(serviceId);
        LS_DVBSubDecFinalize();
        return 1;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    printf("File size: %ld bytes\n", file_size);

    uint8_t* buffer = (uint8_t*)malloc(file_size);
    if (!buffer) {
        fprintf(stderr, "ERROR: Cannot allocate buffer\n");
        fclose(fp);
        LS_DVBSubDecServiceStop(serviceId);
        LS_DVBSubDecServiceDelete(serviceId);
        LS_DVBSubDecFinalize();
        return 1;
    }

    size_t bytes_read = fread(buffer, 1, file_size, fp);
    fclose(fp);

    printf("Read %zu bytes\n", bytes_read);

    // Process all PES packets in the file
    printf("\n=== Processing PES data ===\n");

    LS_PageId_t pageId;
    pageId.compositionPageId = 1;
    pageId.ancillaryPageId = 1;

    // Iterate through all PES packets (00 00 01 BD marker)
    size_t pos = 0;
    int packet_count = 0;
    while (pos + 6 < bytes_read) {
        // Find next PES packet start (00 00 01 BD or BE)
        while (pos + 6 < bytes_read) {
            if (buffer[pos] == 0x00 && buffer[pos+1] == 0x00 && buffer[pos+2] == 0x01 &&
                (buffer[pos+3] == 0xBD || buffer[pos+3] == 0xBE)) {
                break;
            }
            pos++;
        }

        if (pos + 6 >= bytes_read) break;

        // Extract PES packet length (big-endian at offset 4)
        uint16_t pes_len = (buffer[pos+4] << 8) | buffer[pos+5];
        size_t packet_end = pos + 6 + pes_len;

        if (packet_end > bytes_read) {
            printf("Warning: PES packet extends beyond buffer, truncating\n");
            packet_end = bytes_read;
        }

        // Only process stream_id 0xBD (Private Stream 1 - subtitles)
        if (buffer[pos+3] == 0xBD) {
            printf("\n[PES Packet %d] offset=%zu, length=%u\n", packet_count, pos, pes_len);

            LS_CodedData_t pesData;
            pesData.data = buffer + pos;
            pesData.dataSize = packet_end - pos;

            status = LS_DVBSubDecServicePlay(serviceId, &pesData, pageId);
            if (status != LS_OK) {
                printf("Warning: PES packet %d failed with status %d\n", packet_count, status);
            }
            packet_count++;
        } else {
            printf("[Skipping stream_id 0x%02X packet at offset %zu]\n", buffer[pos+3], pos);
        }

        pos = packet_end;
    }

    printf("\n=== Processing complete ===\n");
    printf("Processed %d subtitle PES packets\n", packet_count);

    // Execute timer callbacks in order (simulating timer firing)
    printf("\n=== Executing Timer Callbacks ===\n");
    printf("Total timers created: %d\n", g_timer_callback_count);
    for (int i = 0; i < g_timer_callback_count; i++) {
        TimerCallbackInfo* info = &g_timer_callbacks[i];
        printf("  Timer ID %d: delay=%u ms, callback=%p, param=%p\n",
               info->id, info->delay_ms, info->callback, info->param);
    }

    printf("\nExecuting callbacks...\n");
    for (int i = 0; i < g_timer_callback_count; i++) {
        TimerCallbackInfo* info = &g_timer_callbacks[i];
        if (info->active && info->callback && !info->executing) {
            printf("  [Timer %d] Executing callback %p with param %p\n",
                   info->id, info->callback, info->param);
            info->executing = 1;
            info->callback(info->param);
            info->executing = 0;
        }
    }
    printf("Timer callbacks executed.\n");

    // Wait for pending operations
    printf("\nWaiting for pending operations...\n");
    sleep(1);

    // Cleanup
    printf("\n=== Cleanup ===\n");
    LS_DVBSubDecServiceStop(serviceId);
    LS_DVBSubDecServiceDelete(serviceId);
    LS_DVBSubDecFinalize();

    free(buffer);

    printf("\n=== Test complete ===\n");
    return 0;
}
