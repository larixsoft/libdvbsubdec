/*-----------------------------------------------------------------------------
 * lssubdec.h
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
 * @file lssubdec.h
 * @brief libdvbsubdec - DVB Subtitle Decoder Public API Header
 *
 * This header provides the public API for the libdvbsubdec library.
 * It defines the core data structures, types, enumerations, and function
 * declarations needed to integrate DVB subtitle decoding into applications.
 *
 * The decoder supports DVB subtitles according to ETSI EN 300 743 standard,
 * handling subtitle composition, rendering, and display synchronization.
 *
 * @details Key Features:
 * - Multiple subtitle service management
 * - Support for various pixel formats and color spaces
 * - Flexible callback-based rendering interface
 * - PTS-based subtitle synchronization
 * - Configurable memory management
 * - Support for display definition segments (DDS)
 *
 * @section Thread Safety
 *
 * This library is thread-safe with the following guarantees:
 *
 * - Different service instances can be used concurrently from multiple threads
 * - Each service instance has its own mutex for internal state protection
 * - System functions are protected by global mutexes
 * - Memory heaps are protected by per-heap mutexes
 *
 * @subsection Thread Safety Rules
 *
 * 1. Multiple threads MAY:
 *    - Create/delete different service instances concurrently
 *    - Call LS_DVBSubDecService*() functions on different service instances
 *    - Read configuration from the same service
 *
 * 2. Multiple threads MUST NOT:
 *    - Call LS_DVBSubDecServiceDelete() while other operations on the same service
 *    - Call LS_DVBSubDecFinalize() while any service is active
 *
 * 3. Callback thread safety:
 *    - Callbacks (drawPixmapFunc, cleanRegionFunc, etc.) are invoked with
 *      service mutex held, so they MUST NOT call back into the library
 *      to avoid deadlock
 *    - Callbacks should be non-blocking to avoid holding locks for extended periods
 *
 * @note This library requires initialization before use and cleanup after use.
 */

#ifndef __LS_SUBDEC_H__
#define __LS_SUBDEC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
/*-----------------------------------------------------------------------------
 * private data types
 *---------------------------------------------------------------------------*/
/**
 * @brief Opaque handle to a DVB subtitle service instance
 *
 * This type represents a unique subtitle service that can decode and display
 * subtitles for a specific program or stream.
 */
typedef void* LS_ServiceID_t;
/**
 * @brief Opaque handle to a Packet Identifier
 *
 * Used for identifying different PES packets in the MPEG-TS stream.
 */
typedef void* LS_PID_t;
/**
 * @brief Opaque handle to a semaphore primitive
 *
 * Used for thread synchronization in multi-threaded environments.
 */
typedef void* LS_Semaphore_t;
/**
 * @brief Opaque handle to a mutex primitive
 *
 * Used for mutual exclusion in multi-threaded environments.
 */
typedef void* LS_Mutex_t;
/**
 * @brief Opaque handle to a timer primitive
 *
 * Used for timeout and timing operations.
 */
typedef void* LS_Timer_t;
/*-----------------------------------------------------------------------------
 * const definitions
 *---------------------------------------------------------------------------*/
/** @brief Default verbosity level for logging */
#define LS_DEFAULT_VERB_LEVEL    LS_VERB_WARNING
/*-----------------------------------------------------------------------------
 * enum data types
 *---------------------------------------------------------------------------*/
/**
 * @brief System heap size configuration
 *
 * Defines the default system heap size for the subtitle decoder.
 */
typedef enum
{
  kLS_SYS_HEAP_SIZE = 64 * 1024, /**< Default system heap size in bytes (64 KB) */
} LS_SysHeapSize;
/**
 * @brief Service state enumeration
 *
 * Represents the current state of a subtitle decoder service.
 */
typedef enum
{
  LS_SERVICE_STATE_VOID    = 0, /**< Service is in void/invalid state */
  LS_SERVICE_STATE_NULL    = 1, /**< Service is null/not initialized */
  LS_SERVICE_STATE_READY   = 2, /**< Service is ready but not started */
  LS_SERVICE_STATE_PAUSED  = 3, /**< Service is paused */
  LS_SERVICE_STATE_PLAYING = 4  /**< Service is actively playing/decoding */
} LS_ServiceState;
/**
 * @brief Verbosity level enumeration for logging
 *
 * Defines different verbosity levels for diagnostic and debug output.
 * Multiple levels can be combined using bitwise OR operations.
 */
typedef enum
{
  LS_VERB_ERROR   = 1 << 0,  /**< Error messages only */
  LS_VERB_WARNING = 1 << 1,  /**< Warning and error messages */
  LS_VERB_INFO    = 1 << 2,  /**< Information, warning, and error messages */
  LS_VERB_DEBUG   = 1 << 3,  /**< Debug messages and above */
  LS_VERB_TRACE   = 1 << 4   /**< Detailed trace messages and above */
} LS_VerbLevel;
/**
 * @brief Status and error codes
 *
 * Return values for API functions indicating success or specific error conditions.
 */
typedef enum
{
  LS_OK                       = 1,  /**< Operation completed successfully */
  LS_ERROR_GENERAL            = 0,  /**< General error occurred */
  LS_ERROR_CODED_DATA_BUFFER  = -1, /**< Coded data buffer error */
  LS_ERROR_PIXEL_BUFFER       = -2, /**< Pixel buffer error */
  LS_ERROR_COMPOSITION_BUFFER = -3, /**< Composition buffer error */
  LS_ERROR_SYSTEM_BUFFER      = -4, /**< System buffer error */
  LS_ERROR_SYSTEM_ERROR       = -5, /**< System-level error */
  LS_ERROR_STREAM_DATA        = -6, /**< Stream data error */
  LS_ERROR_WRONG_STATE        = -7  /**< Service in wrong state for requested operation */
} LS_Status;
/**
 * @brief Pixel format enumeration
 *
 * Defines various pixel formats supported by the decoder for subtitle rendering.
 */
typedef enum
{
  LS_PIXFMT_UNKNOWN     = 0,  /**< Unknown pixel format */
  LS_PIXFMT_PALETTE2BIT = 1,  /**< 2-bit palette indexed color */
  LS_PIXFMT_PALETTE4BIT = 2,  /**< 4-bit palette indexed color */
  LS_PIXFMT_PALETTE8BIT = 3,  /**< 8-bit palette indexed color */
  LS_PIXFMT_YUV420      = 4,  /**< YUV 4:2:0 planar format */
  LS_PIXFMT_YUYV        = 5,  /**< YUYV packed format */
  LS_PIXFMT_YVYU        = 6,  /**< YVYU packed format */
  LS_PIXFMT_UYVY        = 7,  /**< UYVY packed format */
  LS_PIXFMT_VYUY        = 8,  /**< VYUY packed format */
  LS_PIXFMT_RGBA15_LE   = 9,  /**< RGBA 15-bit little-endian */
  LS_PIXFMT_RGBA15_BE   = 10, /**< RGBA 15-bit big-endian */
  LS_PIXFMT_BGRA15_LE   = 11, /**< BGRA 15-bit little-endian */
  LS_PIXFMT_BGRA15_BE   = 12, /**< BGRA 15-bit big-endian */
  LS_PIXFMT_ARGB15_LE   = 13, /**< ARGB 15-bit little-endian */
  LS_PIXFMT_ARGB15_BE   = 14, /**< ARGB 15-bit big-endian */
  LS_PIXFMT_ABGR15_LE   = 15, /**< ABGR 15-bit little-endian */
  LS_PIXFMT_ABGR15_BE   = 16, /**< ABGR 15-bit big-endian */
  LS_PIXFMT_RGB24       = 17, /**< RGB 24-bit */
  LS_PIXFMT_BGR24       = 18, /**< BGR 24-bit */
  LS_PIXFMT_RGB16_LE    = 19, /**< RGB 16-bit little-endian */
  LS_PIXFMT_RGB16_BE    = 20, /**< RGB 16-bit big-endian */
  LS_PIXFMT_BGR16_LE    = 21, /**< BGR 16-bit little-endian */
  LS_PIXFMT_BGR16_BE    = 22, /**< BGR 16-bit big-endian */
  LS_PIXFMT_ARGB32      = 23, /**< ARGB 32-bit */
  LS_PIXFMT_RGBA32      = 24, /**< RGBA 32-bit */
  LS_PIXFMT_BGRA32      = 25, /**< BGRA 32-bit */
  LS_PIXFMT_ABGR32      = 26, /**< ABGR 32-bit */
  LS_PIXFMT_END               /**< End of pixel format marker */
} LS_PixelFormat;
/**
 * @brief Text encoding enumeration
 *
 * Defines supported text encodings for subtitle text.
 */
typedef enum
{
  LS_ENCODING_DEFAULT = 0,  /**< Default encoding (auto-detected) */
  LS_UTF_8 = 1,             /**< UTF-8 text encoding */
  LS_ENCODING_LATIN1 = 2,   /**< ISO-8859-1 (Latin-1) encoding */
  LS_ENCODING_ASCII = 3,    /**< ASCII encoding */
} LS_ENCODING;
/**
 * @brief Palette type enumeration
 *
 * Defines the number of colors in a palette.
 */
typedef enum
{
  LS_PALETTE_COLORS_2BITS,  /**< 2-bit palette (4 colors) */
  LS_PALETTE_COLORS_4BITS,  /**< 4-bit palette (16 colors) */
  LS_PALETTE_COLORS_8BITS,  /**< 8-bit palette (256 colors) */
} LS_PaletteType;
/**
 * @brief Data type enumeration for PES packets
 *
 * Defines how PES packet data should be interpreted.
 */
typedef enum
{
  LS_COMPLETE_PES_PACKET = 0,    /**< Complete PES packet including header */
  LS_PES_PAYLOAD_ONLY    = 1,    /**< PES payload only (header stripped) */
} LS_DataType;
/**
 * @brief CRC32 algorithm type enumeration
 *
 * Defines different CRC32 calculation algorithms.
 */
typedef enum
{
  LS_CRC32_STANDARD,     /**< Standard CRC32 */
  LS_CRC32_C,            /**< CRC32-C */
  LS_CRC32_MPEG,         /**< MPEG CRC32 */
  LS_CRC32_BZIP2,        /**< BZIP2 CRC32 */
  LS_CRC32_POSIX,        /**< POSIX CRC32 */
  LS_CRC32_JAM,          /**< JAM CRC32 */
  LS_CRC32_XFER,         /**< XFER CRC32 */
  LS_CRC32_USER_DEFINED  /**< User-defined CRC32 */
} LS_CRCType;
/**
 * @brief Synchronization mode enumeration
 *
 * Defines methods for synchronizing subtitle display.
 */
typedef enum
{
  LS_SYNC_BY_SUBDEC_TIMER = 0, /**< Sync using decoder internal timer */
  LS_SYNC_BY_PTS_CALLBACK = 1, /**< Sync using PTS callback from application */
} LS_SyncMode;
/**
 * @brief Color mode enumeration
 *
 * Defines different color space representations.
 */
typedef enum
{
  LS_COLOR_RGB     = 0,  /**< RGB color space */
  LS_COLOR_YUV     = 1,  /**< YUV color space */
  LS_COLOR_HSV     = 2,  /**< HSV color space */
  LS_COLOR_PALETTE = 3,  /**< Palette-indexed color */
} LS_ColorMode;
/*-----------------------------------------------------------------------------
 * struct data types
 *---------------------------------------------------------------------------*/
/** @brief Service memory configuration structure */
typedef struct _LS_ServiceMemCfg  LS_ServiceMemCfg_t;
/** @brief System functions callback structure */
typedef struct _LS_SystemFuncs    LS_SystemFuncs_t;
/** @brief System logger configuration structure */
typedef struct _LS_SystemLogger   LS_SystemLogger_t;
/** @brief Time representation structure */
typedef struct _LS_Time           LS_Time_t;
/** @brief DVB page identifier structure */
typedef struct _LS_PageId         LS_PageId_t;
/** @brief Rectangle definition structure */
typedef struct _LS_Rect           LS_Rect_t;
/** @brief RGB color structure */
typedef struct _LS_ColorRGB       LS_ColorRGB_t;
/** @brief YUV color structure */
typedef struct _LS_ColorYUV       LS_ColorYUV_t;
/** @brief HSV color structure */
typedef struct _LS_ColorHSV       LS_ColorHSV_t;
/** @brief Generic color structure (union of color spaces) */
typedef struct _LS_Color          LS_Color_t;
/** @brief Pixmap/image data structure */
typedef struct _LS_Pixmap         LS_Pixmap_t;
/** @brief OSD rendering callback structure */
typedef struct _LS_OSDRender      LS_OSDRender_t;
/** @brief Coded data container structure */
typedef struct _LS_CodedData      LS_CodedData_t;
/** @brief Customer DDS (Display Definition Segment) structure */
typedef struct _LS_CustomerDDS    LS_CustomerDDS_t;
/**
 * @brief Callback function for cleaning a display region
 *
 * This callback is invoked to clear a rectangular region of the display
 * before rendering new subtitle content.
 *
 * @param rect The rectangle region to clear
 * @param user_data User-provided data pointer
 * @return LS_OK on success, error code otherwise
 */
typedef LS_Status (*LS_CleanRegionFunc)(LS_Rect_t rect, void* user_data);
/**
 * @brief Callback function for drawing a pixmap
 *
 * This callback is invoked to render a subtitle pixmap (image) to the display.
 *
 * @param pixmap The pixmap to draw
 * @param palette The color palette to use
 * @param palette_num The palette size/number
 * @param user_data User-provided data pointer
 * @return LS_OK on success, error code otherwise
 */
typedef LS_Status (*LS_DrawPixmapFunc)(const LS_Pixmap_t* pixmap,
                                       const uint8_t*     palette,
                                       const uint8_t      palette_num,
                                       void*              user_data);
/**
 * @brief Callback function for drawing text
 *
 * This callback is invoked to render subtitle text to the display.
 *
 * @param x_pos X coordinate for text position
 * @param y_pos Y coordinate for text position
 * @param text The text string to draw
 * @param color The color to use for text
 * @param encoding The text encoding (e.g., UTF-8)
 * @param user_data User-provided data pointer
 * @return LS_OK on success, error code otherwise
 */
typedef LS_Status (*LS_DrawTextFunc)(const int32_t     x_pos,
                                     const int32_t     y_pos,
                                     const char*       text,
                                     const void*       color,
                                     const LS_ENCODING encoding,
                                     void*             user_data);
/**
 * @brief Callback function for getting text metrics
 *
 * This callback is invoked to query the dimensions needed to render text.
 *
 * @param text The text string to measure
 * @param width Pointer to receive the text width
 * @param height Pointer to receive the text height
 * @param user_data User-provided data pointer
 * @return LS_OK on success, error code otherwise
 */
typedef LS_Status (*LS_GetTextMetricsFunc)(const char* text, uint32_t* width, uint32_t* height, void* user_data);
/**
 * @brief Callback function for getting current PCR value
 *
 * This callback is invoked to retrieve the current Program Clock Reference
 * value from the transport stream for synchronization.
 *
 * @param current_PCR Pointer to receive the current PCR value
 * @param user_data User-provided data pointer
 * @return LS_OK on success, error code otherwise
 */
typedef LS_Status (*LS_GetgetCurrentPCRFunc)(uint64_t* current_PCR, void* user_data);
/**
 * @brief Callback function for CRC calculation
 *
 * This callback is invoked to calculate CRC values for data verification.
 *
 * @param data Pointer to data buffer (implementation-specific)
 * @param crc Pointer to receive the calculated CRC
 * @param user_data User-provided data pointer
 * @return LS_OK on success, error code otherwise
 */
typedef LS_Status (*LS_CRCFunc)(const char*, uint32_t* crc, void* user_data);
/**
 * Callback for notification of Display Definition Segment (DDS)
 * This callback is invoked when a DDS is processed, providing the application
 * with the actual display dimensions to use for subtitle scaling.
 *
 * @param displayWidth  The display width from DDS (in pixels)
 * @param displayHeight The display height from DDS (in pixels)
 * @param user_data     User-provided data pointer
 * @return LS_OK on success, error code otherwise
 */
typedef LS_Status (*LS_DDSNotifyFunc)(uint16_t displayWidth, uint16_t displayHeight, void* user_data);

/**
 * @brief Service memory configuration structure
 *
 * Defines the memory buffer sizes for a subtitle decoder service.
 */
struct _LS_ServiceMemCfg
{
  uint32_t codedDataBufferSize;      /**< Size of the coded data buffer in bytes */
  uint32_t pixelBufferSize;          /**< Size of the pixel buffer in bytes       */
  uint32_t compositionBufferSize;    /**< Size of the composition buffer in bytes */
};


/**
 * @brief System functions callback structure
 *
 * Provides callbacks for system-level operations like mutex and timer management.
 * These allow the decoder to be portable across different platforms.
 */
struct _LS_SystemFuncs
{
  int32_t (*mutexCreateFunc)(LS_Mutex_t* mutex_id);                                                   /**< Callback to create a mutex             */
  int32_t (*mutexDeleteFunc)(LS_Mutex_t mutex_id);                                                    /**< Callback to delete a mutex             */
  int32_t (*mutexWaitFunc)(LS_Mutex_t mutex_id);                                                      /**< Callback to wait/acquire a mutex       */
  int32_t (*mutexSignalFunc)(LS_Mutex_t mutex_id);                                                    /**< Callback to signal/release a mutex     */
  int32_t (*timerCreateFunc)(LS_Timer_t* timer_id, void (*callback_func) (void* param), void* param); /**< Callback to create a timer             */
  int32_t (*timerDeleteFunc)(LS_Timer_t timer_id);                                                    /**< Callback to delete a timer             */
  int32_t (*timerStartFunc)(LS_Timer_t timer_id, uint32_t time_in_millisec);                          /**< Callback to start a timer              */
  int32_t (*timerStopFunc)(LS_Timer_t timer_id, LS_Time_t* time_left);                                /**< Callback to stop a timer               */
  char*   (*getTimeStampFunc)(void);                                                                  /**< Callback to get current timestamp string */
};


typedef int32_t (*loggerfunc)(void* user_data, const char* format, va_list arg);

/**
 * @brief System logger configuration structure
 *
 * Configures the logging system for the decoder.
 */
struct _LS_SystemLogger
{
  LS_VerbLevel level;        /**< Minimum verbosity level to log */
  loggerfunc   func;         /**< Logger callback function      */
  void*        user_data;    /**< User-provided data for logger  */
};


/**
 * @brief Time representation structure
 *
 * Represents a time value with second and millisecond precision.
 */
struct _LS_Time
{
  uint32_t seconds;        /**< Seconds component     */
  uint32_t milliseconds;   /**< Milliseconds component */
};


/**
 * @brief DVB page identifier structure
 *
 * Identifies the composition and ancillary pages for DVB subtitles.
 */
struct _LS_PageId
{
  uint16_t compositionPageId;   /**< Composition page ID (main subtitle page)    */
  uint16_t ancillaryPageId;     /**< Ancillary page ID (additional subtitle data) */
};


/**
 * @brief Rectangle definition structure
 *
 * Defines a rectangular region using screen coordinates.
 */
struct _LS_Rect
{
  int32_t leftPos;     /**< Left X coordinate  */
  int32_t topPos;      /**< Top Y coordinate   */
  int32_t rightPos;    /**< Right X coordinate */
  int32_t bottomPos;   /**< Bottom Y coordinate */
};


/**
 * @brief RGB color structure
 *
 * Represents a color in RGB color space with alpha channel.
 */
struct _LS_ColorRGB
{
  uint8_t alphaValue;   /**< Alpha/transparency value (0-255) */
  uint8_t redValue;     /**< Red component (0-255)             */
  uint8_t greenValue;   /**< Green component (0-255)           */
  uint8_t blueValue;    /**< Blue component (0-255)            */
};


/**
 * @brief YUV color structure
 *
 * Represents a color in YUV color space with alpha channel.
 */
struct _LS_ColorYUV
{
  uint8_t alphaValue;   /**< Alpha/transparency value (0-255) */
  uint8_t yValue;       /**< Luma (Y) component (0-255)       */
  uint8_t uValue;       /**< U (chroma) component (0-255)     */
  uint8_t vValue;       /**< V (chroma) component (0-255)     */
};


/**
 * @brief HSV color structure
 *
 * Represents a color in HSV color space with alpha channel.
 */
struct _LS_ColorHSV
{
  uint8_t hValue;       /**< Hue degree (0-255)                              */
  uint8_t sValue;       /**< Saturation (0=gray, 255=fully saturated)        */
  uint8_t vValue;       /**< Value/brightness (0=black, 255=full brightness) */
  uint8_t alphaValue;   /**< Transparency value (0=opaque, 255=fully transparent) */
};


/**
 * @brief Generic color structure
 *
 * Union structure that can represent colors in different color spaces.
 */
struct _LS_Color
{
  LS_ColorMode colorMode;        /**< Color mode (RGB, YUV, HSV, or palette) */
  union
  {
    LS_ColorRGB_t rgbColor;      /**< RGB color data */
    LS_ColorYUV_t yuvColor;      /**< YUV color data */
    LS_ColorHSV_t hsvColor;      /**< HSV color data */
    uint8_t       colorIndex;    /**< Palette index  */
  } colorData;
};


/**
 * @brief Pixmap/image data structure
 *
 * Represents an image or pixmap for subtitle rendering.
 */
struct _LS_Pixmap
{
  LS_PixelFormat pixelFormat;     /**< Pixel format of the pixmap data */
  uint32_t       bytesPerLine;    /**< Bytes per line (stride)       */
  uint8_t        bytesPerPixel;   /**< Bytes per pixel               */
  int32_t        leftPos;         /**< Left X position on screen     */
  int32_t        topPos;          /**< Top Y position on screen      */
  uint32_t       width;           /**< Width in pixels               */
  uint32_t       height;          /**< Height in pixels              */
  void*          data;            /**< Pointer to pixel data         */
  uint32_t       dataSize;        /**< Size of data buffer in bytes  */
};


/**
 * @brief OSD rendering callback structure
 *
 * Contains all the callback functions and configuration needed for
 * on-screen display rendering of subtitles.
 */
struct _LS_OSDRender
{
  LS_CleanRegionFunc      cleanRegionFunc;           /**< Callback to clean/clear a region    */
  void*                   cleanRegionFuncData;       /**< User data for clean region callback */
  LS_DrawPixmapFunc       drawPixmapFunc;            /**< Callback to draw a pixmap/image     */
  void*                   drawPixmapFuncData;        /**< User data for draw pixmap callback  */
  LS_DrawTextFunc         drawTextFunc;              /**< Callback to draw text               */
  void*                   drawTextFuncData;          /**< User data for draw text callback     */
  LS_GetTextMetricsFunc   getTextMetricsFunc;        /**< Callback to get text metrics        */
  void*                   getTextMetricsFuncData;    /**< User data for text metrics callback */
  LS_GetgetCurrentPCRFunc getCurrentPCRFunc;         /**< Callback to get current PCR value   */
  void*                   getCurrentPCRFuncData;     /**< User data for PCR callback         */
  LS_DDSNotifyFunc        ddsNotifyFunc;             /**< Callback for DDS notification       */
  void*                   ddsNotifyFuncData;         /**< User data for DDS notification callback */
  LS_Color_t              backgroundColor;           /**< Background color for subtitle display */
  LS_PixelFormat          OSDPixmapFormat;           /**< Pixel format for OSD pixmaps         */
  uint8_t                 alphaValueFullTransparent; /**< Alpha value for full transparency  */
  uint8_t                 alphaValueFullOpaque;      /**< Alpha value for full opacity        */
  uint8_t                 ptsSyncEnabled;            /**< Enable PTS synchronization (0=off, 1=on) */
  int64_t                 ptsSyncTolerance;          /**< PTS tolerance in milliseconds (default: 100ms) */
  int64_t                 ptsMaxEarlyDisplay;        /**< Max early display time in ms (default: 50ms) */
};


/**
 * @brief Coded data container structure
 *
 * Holds encoded subtitle data for processing.
 */
struct _LS_CodedData
{
  void*    data;               /**< Pointer to data buffer */
  uint32_t dataSize;           /**< Size of data in bytes */
};


/**
 * @brief Customer DDS (Display Definition Segment) structure
 *
 * Allows the application to specify custom display dimensions
 * instead of using the DDS from the stream.
 */
struct _LS_CustomerDDS
{
  uint32_t  displayWidth;          /**< Display width in pixels    */
  uint32_t  displayHeight;         /**< Display height in pixels   */
  uint32_t  displayWindowFlag;     /**< Display window flag       */
  LS_Rect_t displayWindow;         /**< Display window rectangle  */
};


/*-----------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------*/
/**
 * @brief Setup the logger for the decoder
 *
 * Configures the logging system with user-provided logger callbacks.
 *
 * @param loggerFunc Pointer to logger configuration structure
 * @return LS_OK on success, error code otherwise
 */
LS_Status LS_DVBSubDecSetupLogger(const LS_SystemLogger_t* loggerFunc);

/**
 * @brief Initialize the DVB subtitle decoder
 *
 * Initializes the global decoder instance with specified heap size and
 * system functions. Must be called before any other decoder operations.
 *
 * @param heap_size Size of the system heap in bytes
 * @param sysFcns Pointer to system function callbacks
 * @return LS_OK on success, error code otherwise
 */
LS_Status LS_DVBSubDecInit(const uint32_t heap_size, const LS_SystemFuncs_t sysFcns);

/**
 * @brief Finalize and cleanup the decoder
 *
 * Releases all resources and shuts down the decoder. No further
 * decoder operations should be called after this function.
 *
 * @return LS_OK on success, error code otherwise
 */
LS_Status LS_DVBSubDecFinalize(void);

/**
 * @brief Create a new subtitle decoder service
 *
 * Creates and initializes a new subtitle service instance with
 * the specified memory configuration.
 *
 * @param memCfg Memory configuration for the service
 * @return Service ID handle on success, NULL on failure
 */
LS_ServiceID_t LS_DVBSubDecServiceNew(const LS_ServiceMemCfg_t memCfg);

/**
 * @brief Delete a subtitle decoder service
 *
 * Releases all resources associated with the specified service.
 *
 * @param serviceID Service ID to delete
 * @return LS_OK on success, error code otherwise
 */
LS_Status LS_DVBSubDecServiceDelete(const LS_ServiceID_t serviceID);

/**
 * @brief Start a subtitle service
 *
 * Starts the specified service with the given OSD rendering configuration.
 * The service will begin processing subtitle data.
 *
 * @param serviceID Service ID to start
 * @param osdRender OSD rendering callbacks and configuration
 * @return LS_OK on success, error code otherwise
 */
LS_Status LS_DVBSubDecServiceStart(const LS_ServiceID_t serviceID, const LS_OSDRender_t osdRender);

/**
 * @brief Feed subtitle data to a service
 *
 * Provides encoded subtitle PES data to the service for decoding and display.
 *
 * @param serviceID Service ID to feed data to
 * @param data Pointer to coded subtitle data
 * @param page Page identifier for the subtitle data
 * @return LS_OK on success, error code otherwise
 */
LS_Status LS_DVBSubDecServicePlay(const LS_ServiceID_t serviceID, const LS_CodedData_t* data, const LS_PageId_t page);

/**
 * @brief Stop a subtitle service
 *
 * Stops the service and clears any displayed subtitles.
 *
 * @param serviceID Service ID to stop
 * @return LS_OK on success, error code otherwise
 */
LS_Status LS_DVBSubDecServiceStop(const LS_ServiceID_t serviceID);

/**
 * @brief Pause a subtitle service
 *
 * Pauses the service, freezing the current subtitle display.
 *
 * @param serviceID Service ID to pause
 * @return LS_OK on success, error code otherwise
 */
LS_Status LS_DVBSubDecServicePause(const LS_ServiceID_t serviceID);

/**
 * @brief Resume a paused subtitle service
 *
 * Resumes a previously paused service.
 *
 * @param serviceID Service ID to resume
 * @return LS_OK on success, error code otherwise
 */
LS_Status LS_DVBSubDecServiceResume(const LS_ServiceID_t serviceID);

/**
 * @brief Mute a subtitle service
 *
 * Mutes the service, preventing subtitle display while continuing
 * to process data.
 *
 * @param serviceID Service ID to mute
 * @return LS_OK on success, error code otherwise
 */
LS_Status LS_DVBSubDecServiceMute(const LS_ServiceID_t serviceID);

/**
 * @brief Unmute a subtitle service
 *
 * Unmutes a previously muted service, resuming subtitle display.
 *
 * @param serviceID Service ID to unmute
 * @return LS_OK on success, error code otherwise
 */
LS_Status LS_DVBSubDecServiceUnMute(const LS_ServiceID_t serviceID);

/**
 * @brief Enable custom display definition
 *
 * Enables the use of a custom DDS instead of the one from the stream.
 *
 * @param serviceID Service ID to configure
 * @param dds Custom display definition structure
 * @return LS_OK on success, error code otherwise
 */
LS_Status LS_DVBSubDecServiceEnableCustomerDDS(const LS_ServiceID_t serviceID, const LS_CustomerDDS_t dds);

/**
 * @brief Disable custom display definition
 *
 * Disables custom DDS and reverts to using the DDS from the stream.
 *
 * @param serviceID Service ID to configure
 * @return LS_OK on success, error code otherwise
 */
LS_Status LS_DVBSubDecServiceDisableCustomerDDS(const LS_ServiceID_t serviceID);

/**
 * @brief Get the current state of a service
 *
 * Retrieves the current state of the specified service.
 *
 * @param serviceID Service ID to query
 * @param state Pointer to receive the service state
 * @return LS_OK on success, error code otherwise
 */
LS_Status LS_DVBSubDecServiceCurrentState(const LS_ServiceID_t serviceID, LS_ServiceState* state);

/**
 * @brief Set the display resolution for a service
 *
 * Sets the output display resolution for subtitle scaling.
 *
 * @param serviceID Service ID to configure
 * @param width Display width in pixels
 * @param height Display height in pixels
 * @return LS_OK on success, error code otherwise
 */
LS_Status LS_DVBSubDecServiceSetDisplayResolution(const LS_ServiceID_t serviceID,
                                                  const int32_t        width,
                                                  const int32_t        height);

/**
 * @brief Set the playback speed for a service
 *
 * Sets the playback speed (e.g., for fast-forward or slow-motion).
 *
 * @param serviceID Service ID to configure
 * @param speed Speed multiplier (100 = normal, 0 = paused)
 * @return LS_OK on success, error code otherwise
 */
LS_Status LS_DVBSubDecServiceSetSpeed(const LS_ServiceID_t serviceID, const int32_t speed);

/**
 * @brief Reset a subtitle service
 *
 * Resets the service to its initial state, clearing all data.
 *
 * @param serviceID Service ID to reset
 * @return LS_OK on success, error code otherwise
 */
LS_Status LS_DVBSubDecServiceReset(const LS_ServiceID_t serviceID);


#ifdef __cplusplus
}
#endif

#endif /*_SFM_SUBDEC_H_*/
