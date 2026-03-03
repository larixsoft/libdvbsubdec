/*-----------------------------------------------------------------------------
 * lsptsmgr.h
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
 * @file lsptsmgr.h
 * @brief PTS (Presentation Timestamp) Manager
 *
 * This header provides the PTS management system for synchronizing
 * subtitle display with audio/video playback.
 */

#ifndef __LS_PTS_MGR_H__
#define __LS_PTS_MGR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "lssubdec.h"
/**
 * @brief PTS reason enumeration
 *
 * Defines the reason codes for PTS callbacks.
 */
typedef enum
{
  PTS_PRESENTED       = 0, /**< PTS was presented successfully */
  PTS_INVALID         = 1, /**< PTS is invalid */
  PTS_INVALID_REQUEST = 2, /**< PTS request is invalid */
} PTSReason;
/** @brief Forward declaration */
typedef struct _LS_PTS_Request LS_PTSRequestInfo;
/**
 * @brief PTS notification callback function type
 *
 * Callback invoked when a PTS event occurs.
 *
 * @param ptsValue The PTS value
 * @param reason Reason for the callback
 * @param user_data User-provided data pointer
 */
typedef void (*PTSNotifyCallbackFunc)(const uint64_t ptsValue, PTSReason reason, void* user_data);
/** @brief Opaque PTS manager client ID */
typedef void* LS_PTSMgrClientID;

/**
 * @brief PTS request structure
 *
 * Contains information for a PTS registration request.
 */
struct _LS_PTS_Request
{
  LS_PTSMgrClientID       clientId;               /**< Client ID for this request         */
  uint64_t                ptsValue;               /**< PTS value to trigger on            */
  LS_GetgetCurrentPCRFunc getCurrentPCRFunc;      /**< Callback to get current PCR        */
  void*                   getCurrentPCRFuncData;  /**< User data for PCR callback         */
  PTSNotifyCallbackFunc   PTSCallBackFunc;        /**< Notification callback              */
  void*                   PTSCBFuncData;          /**< User data for notification callback */
  int32_t                 speed;                  /**< Playback speed                     */
};


/**
 * @brief Initialize the PTS manager
 *
 * Initializes the PTS manager thread and subsystems.
 * For each service ID, a separate client should be registered.
 *
 * @return LS_OK (1) on success, error code on failure
 */
int32_t LS_PTSMgrInit(void);

/**
 * @brief Register a client with the PTS manager
 *
 * Creates a new client instance for PTS management.
 *
 * @param errorCode Pointer to receive error code
 * @return Client ID on success, NULL on failure
 */
LS_PTSMgrClientID LS_PTSMgrRegisterClient(int32_t* errorCode);

/**
 * @brief Unregister a client
 *
 * Removes a client from the PTS manager.
 *
 * @param clientId Client ID to unregister
 */
void LS_PTSMgrUnRegisterClient(LS_PTSMgrClientID clientId);

/**
 * @brief Register a PTS event
 *
 * Registers a request to be notified when the specified PTS is reached.
 *
 * @param request PTS request structure
 * @return LS_OK (1) on success, error code on failure
 */
int32_t LS_PTSMgrRegisterPTS(LS_PTSRequestInfo* request);

/**
 * @brief Cancel a PTS event
 *
 * Cancels a previously registered PTS request.
 *
 * @param request PTS request structure to cancel
 * @return LS_OK (1) on success, error code on failure
 */
int32_t LS_PTSMgrCancelPTS(LS_PTSRequestInfo* request);

/**
 * @brief Reset a client
 *
 * Resets all pending PTS requests for a client.
 *
 * @param clientId Client ID to reset
 * @return LS_OK (1) on success, error code on failure
 */
int32_t LS_PTSMgrReset(LS_PTSMgrClientID clientId);

/**
 * @brief Set playback speed for a client
 *
 * Adjusts the speed multiplier for PTS calculations.
 *
 * @param clientId Client ID
 * @param speed Speed multiplier (100 = normal)
 * @return LS_OK (1) on success, error code on failure
 */
int32_t LS_PTSMgrSetSpeed(LS_PTSMgrClientID clientId, int32_t speed);

/**
 * @brief Finalize the PTS manager
 *
 * Shuts down the PTS manager and releases resources.
 *
 * @return LS_OK (1) on success, error code on failure
 */
int32_t LS_PTSMgrFinalize(void);

#ifdef __cplusplus
}
#endif

#endif
