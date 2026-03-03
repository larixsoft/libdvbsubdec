/*-----------------------------------------------------------------------------
 * lssubdecoder.h
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
 * @file lssubdecoder.h
 * @brief Internal DVB Subtitle Decoder Data Structures
 *
 * This header defines internal data structures and types used by the
 * DVB subtitle decoder implementation. These structures are used for
 * managing services, display pages, regions, and the decoding state machine.
 *
 * @details Key components:
 * - Service management: Factory and service instances
 * - Display page management: Page lifecycle and states
 * - PES packet handling: MPEG-2 PES header parsing
 * - Memory partitioning: Separate heaps for different data types
 * - Command processing: Internal command queue system
 *
 * @note This is an internal header. Applications should use lssubdec.h instead.
 */

#ifndef __LS_DECODER_H__
#define __LS_DECODER_H__


#ifdef __cplusplus
extern "C" {
#endif


#include "lssubport.h"
#include "lslist.h"
#include "lssystem.h"
#include "lsmemory.h"
#include "lsptsmgr.h"
#include "lssuben300743.h"
/*---------------------------------------------------------------------------
 * enums
 *--------------------------------------------------------------------------*/
/**
 * @brief Memory partition enumeration
 *
 * Defines the different memory partitions used by the decoder for
 * storing different types of data.
 */
typedef enum
{
  CODED_DATA_BUFFER,    /**< Coded data buffer partition */
  PIXEL_BUFFER,         /**< Pixel buffer partition */
  COMPOSITION_BUFFER,   /**< Composition buffer partition */
} LS_DECODER_MEMORY_PARTITION;
/**
 * @brief Time constants
 *
 * Defines timeout values used by the decoder.
 */
typedef enum
{
  LS_DP_CUBE_SUICIDE_TIMEOUT_SECONDS = 7200, /**< Timeout for display page suicide (7200 seconds = 2 hours) */
} LS_TimeConstant;
/**
 * @brief Cube type enumeration
 *
 * Defines the types of "cubes" (command/data packets) used internally.
 */
typedef enum
{
  LS_CUBE_TYPE_DISPLAY_PAGE = 1, /**< Display page cube */
  LS_CUBE_TYPE_FIRED_PTS    = 2, /**< Fired PTS cube */
} LS_CubeType;
/**
 * @brief Cube command enumeration
 *
 * Defines the different commands that can be sent via the cube system.
 */
typedef enum
{
  LS_CUBE_CMD_PLAY     = 1,   /**< Play command */
  LS_CUBE_CMD_PAUSE    = 2,   /**< Pause command */
  LS_CUBE_CMD_RESUME   = 3,   /**< Resume command */
  LS_CUBE_CMD_ENABLE   = 4,   /**< Enable command */
  LS_CUBE_CMD_DISABLE  = 5,   /**< Disable command */
  LS_CUBE_CMD_MUTE     = 6,   /**< Mute command */
  LS_CUBE_CMD_UNMUTE   = 7,   /**< Unmute command */
  LS_CUBE_CMD_SPEED    = 8,   /**< Speed command */
  LS_CUBE_CMD_RESET    = 9,   /**< Reset command */
  LS_CUBE_CMD_PTS      = 10,  /**< PTS command */
  LS_CUBE_CMD_CONTINUE = 11   /**< Continue command */
} LS_CubeCmd;
/**
 * @brief Display page type enumeration
 *
 * Defines the different types of display pages.
 */
typedef enum
{
  LS_DISPLAY_PAGE_TYPE_UNKOWN      = -1,  /**< Unknown page type */
  LS_DISPLAY_PAGE_TYPE_EPOCH_PAGE  = 0,   /**< Epoch page (initial state) */
  LS_DISPLAY_PAGE_TYPE_NORMAL_PAGE = 1,   /**< Normal display page */
  LS_DISPLAY_PAGE_TYPE_UPDATE_PAGE = 2,   /**< Update page (partial update) */
} LS_DisplayPageType;
/**
 * @brief Decoder speed enumeration
 *
 * Defines common decoder speed values.
 */
typedef enum
{
  LS_DECODER_SPEED_NORMAL = 100, /**< Normal playback speed */
  LS_DECODER_SPEED_PAUSED = 0,   /**< Paused */
} LS_DecoderSpeed;
/**
 * @brief Display page status enumeration
 *
 * Defines the different states a display page can be in.
 */
typedef enum
{
  LS_DISPLAYPAGE_STATUS_VOID           = -1,  /**< Invalid/void status */
  LS_DISPLAYPAGE_STATUS_NEW            = 0,   /**< Page is newly created */
  LS_DISPLAYPAGE_STATUS_DECODED        = 1,   /**< Page has been decoded */
  LS_DISPLAYPAGE_STATUS_PTS_REGISTERED = 2,   /**< Page PTS has been registered */
  LS_DISPLAYPAGE_STATUS_DISPLAYING     = 3,   /**< Page is currently displaying */
  LS_DISPLAYPAGE_STATUS_RETIRED        = 4    /**< Page has been retired */
} LS_DisplayPageStatus;
/*---------------------------------------------------------------------------
 * typedefs
 *--------------------------------------------------------------------------*/
/** @brief Factory structure for managing services */
typedef struct _LS_Factory        LS_Factory;
/** @brief Service instance structure */
typedef struct _LS_Service        LS_Service;
/** @brief Display page structure */
typedef struct _LS_DisplayPage    LS_DisplayPage;
/** @brief Command/data cube structure */
typedef struct _LS_Cube           LS_Cube;
/** @brief PES header information structure */
typedef struct _LS_PESHeaderInfo  LS_PESHeaderInfo;

/*---------------------------------------------------------------------------
 * structers
 *--------------------------------------------------------------------------*/
/**
 * @brief Factory structure
 *
 * Manages the global decoder instance and all services.
 */
struct _LS_Factory
{
  uint32_t    flag;            /**< Factory flags          */
  LS_MemHeap* systemHeap;      /**< System heap pointer    */
  uint32_t    systemHeapSize;  /**< System heap size       */
  LS_List*    serviceList;     /**< List of active services */
  uint32_t    referenceCount;  /**< Reference count        */
  LS_Mutex_t  mutex;           /**< Factory mutex          */
};


/**
 * @brief Service structure
 *
 * Represents a single subtitle decoder service instance.
 */
struct _LS_Service
{
  uint32_t           magic_id;                /**< Magic number for validation      */
  LS_ServiceState    state;                   /**< Current service state            */
  LS_OSDRender_t     osdRender;               /**< OSD rendering callbacks           */
  LS_List*           displayPageList;         /**< List of display pages            */
  LS_DisplayPage*    latestDisplayPage;       /**< Latest display page              */
  LS_DisplayPage*    displayPageOnScreen;     /**< Currently displayed page         */
  LS_Mutex_t         serviceMutex;            /**< Service mutex                    */
  LS_PageId_t        pmtPageID;               /**< PMT page ID                      */
  int32_t            muteState;               /**< Mute state                       */
  int32_t            serviceSpeed;            /**< Service playback speed           */
  LS_PTSMgrClientID  ptsmgrClientID;          /**< PTS manager client ID            */
  int32_t            DDSFlag;                 /**< DDS enabled flag                 */
  LS_CustomerDDS_t   DDS;                     /**< Customer DDS configuration       */
  int32_t            displayWidth;            /**< Display width                    */
  int32_t            displayHeight;           /**< Display height                   */
  LS_ServiceMemCfg_t memCfg;                  /**< Memory configuration             */
  LS_MemHeap*        codedDataBufferHeap;     /**< Coded data buffer heap           */
  LS_MemHeap*        pixelBufferHeap;         /**< Pixel buffer heap                */
  LS_MemHeap*        compositionBufferHeap;   /**< Composition buffer heap          */
};


/**
 * @brief PES header information structure
 *
 * Contains parsed MPEG-2 PES (Packetized Elementary Stream) header fields.
 */
struct _LS_PESHeaderInfo
{
  uint8_t  packet_start_code_prefix[3];  /**< Packet start code prefix (3 bytes) */
  uint8_t  stream_id;                    /**< Stream ID                         */
  uint16_t PES_packet_length;            /**< PES packet length                 */
  uint8_t  reserved_10;                  /**< Reserved bits (10)                */
  uint8_t  PES_scrambling_control;       /**< PES scrambling control            */
  uint8_t  PES_priority;                 /**< PES priority flag                 */
  uint8_t  data_alignment_indicator;     /**< Data alignment indicator          */
  uint8_t  copyright;                    /**< Copyright flag                    */
  uint8_t  original_or_copy;             /**< Original or copy flag             */
  uint8_t  PTS_DTS_flags;                /**< PTS/DTS flags                     */
  uint8_t  ESCR_flag;                    /**< ESCR flag                         */
  uint8_t  ES_rate_flag;                 /**< ES rate flag                      */
  uint8_t  DSM_trick_mode_flag;          /**< DSM trick mode flag               */
  uint8_t  additional_copy_info_flag;    /**< Additional copy info flag         */
  uint8_t  PES_CRC_flag;                 /**< PES CRC flag                      */
  uint8_t  PES_extension_flag;           /**< PES extension flag                */
  uint8_t  PES_header_data_length;       /**< PES header data length            */
  uint8_t  PTS_0010;                     /**< PTS bits (0010)                   */
  uint8_t  marker_bit_1;                 /**< Marker bit 1                      */
  uint8_t  PTS_32_30;                    /**< PTS bits 32-30                    */
  uint8_t  marker_bit_2;                 /**< Marker bit 2                      */
  uint16_t PTS_29_15;                    /**< PTS bits 29-15                    */
  uint8_t  marker_bit_3;                 /**< Marker bit 3                      */
  uint16_t PTS_14_0;                     /**< PTS bits 14-0                     */
};


/**
 * @brief Display page structure
 *
 * Represents a subtitle display page with all its regions and metadata.
 */
struct _LS_DisplayPage
{
  uint32_t           magic_id;               /**< Magic number for validation */
  uint16_t           page_id;                /**< Page ID                  */
  uint8_t            page_version_number;    /**< Page version number      */
  uint8_t            dss_version;            /**< DSS version number       */
  LS_Service*        service;                /**< Associated service       */
  LS_Displayset*     displayset;             /**< Display set data         */
  LS_DisplayPageType pageType;               /**< Page type                */
  uint64_t           ptsValue;               /**< PTS value for display    */
  uint8_t            pageState;              /**< Page state               */
  uint8_t            time_out;               /**< Timeout value            */
  int32_t            left_ms;                /**< Milliseconds left until timeout */
  LS_Timer_t         page_time_out_timer;    /**< Page timeout timer       */
  LS_Timer_t         suicide_timer;          /**< Suicide timer            */
  LS_List*           regions;                /**< List of regions          */
  int32_t            visible;                /**< Visible flag             */
  int32_t            dds_flag;               /**< DDS flag                 */
  LS_CustomerDDS_t   dds;                    /**< DDS configuration        */
  int8_t             disparity_shift;        /**< Disparity shift for 3D   */
  int32_t            status;                 /**< Page status              */
};


/**
 * @brief Cube structure
 *
 * Internal command/data packet used for inter-thread communication.
 */
struct _LS_Cube
{
  uint32_t    magic_id;     /**< Magic number for validation */
  LS_CubeType type;         /**< Cube type                 */
  union
  {
    uint32_t command;       /**< Command value */
    void*    data;          /**< Data pointer  */
  }     info;
  void* extra[8];           /**< Extra data pointers */
};


/*---------------------------------------------------------------------------
 * API declarations
 *--------------------------------------------------------------------------*/
/**
 * @brief Create a new service factory
 *
 * Initializes the global service factory with the specified heap size.
 *
 * @param sys_heap_size Size of the system heap in bytes
 * @return LS_OK (1) on success, error code on failure
 */
int32_t ServiceFactoryNew(uint32_t sys_heap_size);

/**
 * @brief Delete the service factory
 *
 * Shuts down and releases all factory resources.
 */
void ServiceFactoryDelete(void);

/**
 * @brief Initialize the service factory
 *
 * Performs initialization of the factory subsystems.
 *
 * @param heap_size Heap size for initialization
 * @return LS_OK (1) on success, error code on failure
 */
int32_t ServiceFactoryInit(uint32_t heap_size);

/**
 * @brief Finalize the service factory
 *
 * Performs cleanup of factory subsystems.
 *
 * @return LS_OK (1) on success, error code on failure
 */
int32_t ServiceFactoryFinalize(void);

/**
 * @brief Get the system heap
 *
 * Returns a pointer to the global system heap.
 *
 * @return Pointer to system heap
 */
LS_MemHeap* ServiceSystemHeap(void);

/**
 * @brief Register a service with the factory
 *
 * Adds a service to the factory's service list.
 *
 * @param service Service to register
 * @return LS_OK (1) on success, error code on failure
 */
int32_t ServiceFactoryRegister(LS_Service* service);

/**
 * @brief Unregister a service from the factory
 *
 * Removes a service from the factory's service list.
 *
 * @param service Service to unregister
 * @return LS_OK (1) on success, error code on failure
 */
int32_t ServiceFactoryUnregister(LS_Service* service);

/**
 * @brief Check if a service is registered
 *
 * Checks whether a service is currently registered with the factory.
 *
 * @param service Service to check
 * @return 1 if registered, 0 otherwise
 */
int32_t ServiceFactoryRegisteredStatus(LS_Service* service);

/**
 * @brief Create a new service instance
 *
 * Creates and initializes a new service instance.
 *
 * @param errorCode Pointer to receive error code
 * @return New service instance or NULL on failure
 */
LS_Service* ServiceInstanceNew(int32_t* errorCode);

/**
 * @brief Delete a service instance
 *
 * Releases all resources associated with a service instance.
 *
 * @param service Service to delete
 */
void ServiceInstanceDelete(LS_Service* service);

/**
 * @brief Reset a service instance
 *
 * Resets a service to its initial state.
 *
 * @param service Service to reset
 */
void ServiceInstanceReset(LS_Service* service);

/**
 * @brief PTS notification callback
 *
 * Called when a PTS event occurs.
 *
 * @param ptsValue PTS value
 * @param reason Reason for the notification
 * @param user_data User-provided data pointer
 */
void ServicePTSNotificationCB(const uint64_t ptsValue, PTSReason reason, void* user_data);

/**
 * @brief Allocate memory from a service heap
 *
 * Allocates memory from the specified partition of a service's heap.
 *
 * @param service Service instance
 * @param partition Memory partition to allocate from
 * @param bytes Number of bytes to allocate
 * @return Pointer to allocated memory or NULL on failure
 */
void* ServiceHeapMalloc(LS_Service* service, int32_t partition, uint32_t bytes);

/**
 * @brief Free memory from a service heap
 *
 * Frees memory previously allocated from a service's heap.
 *
 * @param service Service instance
 * @param partition Memory partition to free from
 * @param mem_p Memory to free
 */
void ServiceHeapFree(LS_Service* service, int32_t partition, void* mem_p);

/**
 * @brief Get error string for error code
 *
 * Returns a human-readable string for an error code.
 *
 * @param err_code Error code
 * @return Error description string
 */
char* ServiceErrString(int32_t err_code);

/**
 * @brief Get state string for service state
 *
 * Returns a human-readable string for a service state.
 *
 * @param state Service state
 * @return State description string
 */
char* ServiceStateString(int32_t state);

/**
 * @brief Get string for cube type
 *
 * Returns a human-readable string for a cube type.
 *
 * @param type Cube type
 * @return Type description string
 */
char* LS_CubeTypeString(int32_t type);

/**
 * @brief Get string for cube command
 *
 * Returns a human-readable string for a cube command.
 *
 * @param cmd Cube command
 * @return Command description string
 */
char* LS_CubeCmdString(int32_t cmd);

/**
 * @brief Process PES payload data
 *
 * Processes PES packet payload data for subtitle decoding.
 *
 * @param service Service instance
 * @param header Parsed PES header
 * @param payload Payload data buffer
 * @param payload_size Size of payload data
 * @return LS_OK (1) on success, error code on failure
 */
int32_t ProcessPESPayLoad(LS_Service* service, LS_PESHeaderInfo* header, uint8_t* payload, int32_t payload_size);

/**
 * @brief List memory allocation callback
 *
 * Internal callback for list memory allocation.
 *
 * @param bytes Number of bytes to allocate
 * @return Pointer to allocated memory
 */
void* __listMallocFunc(uint32_t bytes);

/**
 * @brief List memory deallocation callback
 *
 * Internal callback for list memory deallocation.
 *
 * @param mem Memory to free
 */
void __listFreeFunc(void* mem);

/*-------------------------------------------------------------------------
 * Function Declaration and Prototypes
 *------------------------------------------------------------------------*/
/**
 * @brief Pre-process PES packet data
 *
 * Parses the PES header from raw packet data.
 *
 * @param data Raw PES packet data
 * @param pesinfo Pointer to receive parsed PES header
 * @return LS_OK (1) on success, error code on failure
 */
int32_t LS_PreProcessPES(const char* data, LS_PESHeaderInfo* pesinfo);

/**
 * @brief Dump PES header information
 *
 * Prints PES header information for debugging.
 *
 * @param pesinfo PES header to dump
 */
void LS_DumpPESInfo(const LS_PESHeaderInfo* pesinfo);

/**
 * @brief Verify service ID
 *
 * Validates that a service ID is valid.
 *
 * @param service Service to verify
 * @return 1 if valid, 0 otherwise
 */
int32_t verifyServiceID(LS_Service* service);


#ifdef __cplusplus
}
#endif


#endif
