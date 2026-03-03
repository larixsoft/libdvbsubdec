/*-----------------------------------------------------------------------------
 * lssubdec.c
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

#ifndef __LS_SUBDEC_C__
#define __LS_SUBDEC_C__

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>

#include "lssubdec.h"
#include "lssubmacros.h"
#include "lssystem.h"
#include "lsmemory.h"
#include "lssubver.h"
#include "lssubdecoder.h"
#include "lssubpes.h"
#include "lssuben300743.h"
#include "lsptsmgr.h"
#include "lssubdisplay.h"
#include "lssubsegdec.h"
#include "lssubutils.h"
/*----------------------------------------------------------------------------
 * version information
 *---------------------------------------------------------------------------*/
const int32_t stm_dvbsubdec__major_version = LS_DVBSUBDEC_MAJOR_VERSION;
const int32_t stm_dvbsubdec__minor_version = LS_DVBSUBDEC_MINOR_VERSION;
const int32_t stm_dvbsubdec__micro_version = LS_DVBSUBDEC_MICRO_VERSION;
const int32_t stm_dvbsubdec__binary_age = LS_DVBSUBDEC_BINARY_AGE;
const int32_t stm_dvbsubdec__interface_age = LS_DVBSUBDEC_INTERFACE_AGE;
const char* stm_dvbsubdec_customer_name = LS_DVBSUBDEC_CUSTOMER_INFO;
/*----------------------------------------------------------------------------
 * local static/global varible declaration.
 *---------------------------------------------------------------------------*/
#define LS_PROCESS_DEFAULT_PRIORITY    0
/*----------------------------------------------------------------------------
 * local static functions declaration.
 *---------------------------------------------------------------------------*/
static void sPrintLOGO(void);

/*----------------------------------------------------------------------------
 * local static functions
 *---------------------------------------------------------------------------*/
static void
sPrintLOGO(void)
{
  printf("\n" "         ***********| Larixsoft DVB Subtitle Decoder version %02d.%02d.%02d |***********\n"
         "         *     (c) 2006-2012    Larixsoft Inc, All rights reserved.                                    *\n"
         "         *========================================================================\n\n" "\n",
         stm_dvbsubdec__major_version,
         stm_dvbsubdec__minor_version,
         stm_dvbsubdec__micro_version);
}


/*----------------------------------------------------------------------------
 * public interface APIs
 *---------------------------------------------------------------------------*/
LS_Status
LS_DVBSubDecInit(const uint32_t sys_heap_size, const LS_SystemFuncs_t sys_funcs)
{
  LS_Status ret_val = LS_OK;
  int32_t res = LS_OK;
  uint32_t new_heap_size = 0;

  sPrintLOGO();

  do
  {
    res = LS_UpdateSystemFuncs(sys_funcs);

    if (LS_OK != res)
    {
      LS_ERROR("%s: LS_UpdateSystemFuncs() failed\n", ServiceErrString(res));
      ret_val = LS_ERROR_SYSTEM_ERROR;
      break;
    }

    if (sys_heap_size < kLS_SYS_HEAP_SIZE)
    {
      LS_INFO("Requested sys_heap_size is too small,reset it to 64K\n");
      new_heap_size = kLS_SYS_HEAP_SIZE;
    }
    else
    {
      new_heap_size = sys_heap_size;
    }

    res = ServiceFactoryInit(new_heap_size);

    if (LS_OK != res)
    {
      LS_ERROR("%s: ServiceFactoryInit() failed\n", ServiceErrString(res));
      ret_val = LS_ERROR_GENERAL;
      break;
    }

    res = LS_PTSMgrInit();

    if (LS_OK != res)
    {
      LS_ERROR("%s: LS_PTSMgrInit() failed\n", ServiceErrString(res));
      ret_val = LS_ERROR_GENERAL;
      break;
    }
  }while (0);

  if (LS_OK != ret_val)
  {
    /*cleanup work here*/
    LS_PTSMgrFinalize();
    ServiceFactoryFinalize();
  }

  LS_LEAVE("%s: Leave LS_DVBSubDecInit()\n", ServiceErrString(ret_val));
  return ret_val;
}


LS_Status
LS_DVBSubDecFinalize(void)
{
  LS_Status ret_val = LS_OK;
  int32_t res = LS_OK;

  LS_ENTER("Enter LS_DVBSubDecFinalize()\n");

  do
  {
    res = LS_PTSMgrFinalize();

    if (LS_OK != res)
    {
      LS_ERROR("%s: LS_PTSMgrFinalize() failed\n", ServiceErrString(res));
      ret_val = LS_ERROR_GENERAL;
      break;
    }

    res = ServiceFactoryFinalize();

    if (LS_OK != res)
    {
      LS_ERROR("%s: ServiceFactoryFinalize() failed\n", ServiceErrString(res));
      ret_val = LS_ERROR_GENERAL;
      break;
    }

    LS_ResetSystemFuncs();
    LS_ResetSystemLogger();
  }while (0);

  LS_LEAVE("%s: Leave LS_DVBSubDecFinalize()\n", ServiceErrString(ret_val));
  return ret_val;
}


LS_ServiceID_t
LS_DVBSubDecServiceNew(const LS_ServiceMemCfg_t memCfg)
{
  LS_Service* service = NULL;
  int32_t res = LS_OK;

  LS_ENTER("LS_DVBSubDecServiceNew(memCfg<%u,%u,%u>)\n",
           memCfg.codedDataBufferSize,
           memCfg.pixelBufferSize,
           memCfg.compositionBufferSize);

  do
  {
    service = ServiceInstanceNew(&res);

    if ((LS_OK != res) ||
        (NULL == service))
    {
      LS_ERROR("%s: ServiceInstanceNew() failed\n", ServiceErrString(res));
      res = LS_ERROR_GENERAL;
      break;
    }

    service->ptsmgrClientID = LS_PTSMgrRegisterClient(&res);

    if ((LS_OK != res) ||
        (NULL == service->ptsmgrClientID))
    {
      LS_ERROR("%s: LS_PTSMgrRegisterClient() failed\n", ServiceErrString(res));
      res = LS_ERROR_GENERAL;
      break;
    }

    if (memCfg.codedDataBufferSize <= kEN300743_CODED_DATA_BUFFER_B2_SIZE)
    {
      LS_INFO("memCfg.codedDataBufferSize (%d) was reset to kEN300743_CODED_DATA_BUFFER_B2_SIZE\n",
              memCfg.codedDataBufferSize);
      service->memCfg.codedDataBufferSize = kEN300743_CODED_DATA_BUFFER_B2_SIZE;
    }
    else
    {
      service->memCfg.codedDataBufferSize = memCfg.codedDataBufferSize;
    }

    LS_TRACE("service<%p>.memCfg.codedDataBufferSize<%d>\n", (void*)service, service->memCfg.codedDataBufferSize);

    if (memCfg.pixelBufferSize <= kEN300743_PIXEL_BUFFER_B3_SIZE)
    {
      LS_INFO("memCfg.pixelBufferSize (%d) was reset to kEN300743_PIXEL_BUFFER_B3_SIZE\n", memCfg.pixelBufferSize);
      service->memCfg.pixelBufferSize = kEN300743_PIXEL_BUFFER_B3_SIZE;
    }
    else
    {
      service->memCfg.pixelBufferSize = memCfg.pixelBufferSize;
    }

    LS_TRACE("service<%p>.memCfg.codedDataBufferSize<%d>\n", (void*)service, service->memCfg.codedDataBufferSize);

    if (memCfg.compositionBufferSize <= kEN300743_COMPOSITION_BUFFER_SIZE)
    {
      service->memCfg.compositionBufferSize = kEN300743_COMPOSITION_BUFFER_SIZE;
    }
    else
    {
      service->memCfg.compositionBufferSize = memCfg.compositionBufferSize;
    }

    LS_TRACE("service<%p>.memCfg.compositionBufferSize<%d>\n", (void*)service, service->memCfg.compositionBufferSize);
    res = LS_MemInit(&(service->codedDataBufferHeap), service->memCfg.codedDataBufferSize);

    if (LS_OK != res)
    {
      res = LS_ERROR_GENERAL;
      break;
    }

    res = LS_MemInit(&(service->pixelBufferHeap), service->memCfg.pixelBufferSize);

    if (LS_OK != res)
    {
      res = LS_ERROR_GENERAL;
      break;
    }

    res = LS_MemInit(&(service->compositionBufferHeap), service->memCfg.compositionBufferSize);

    if (LS_OK != res)
    {
      res = LS_ERROR_GENERAL;
      break;
    }

    res = ServiceFactoryRegister(service);

    if (LS_OK != res)
    {
      res = LS_ERROR_GENERAL;
      break;
    }
  }while (0);

  if (LS_OK != res)
  {
    if (service)
    {
      /* Clean up any allocated heaps before deleting service */
      if (service->codedDataBufferHeap)
      {
        LS_MemFinalize(service->codedDataBufferHeap);
        service->codedDataBufferHeap = NULL;
      }

      if (service->pixelBufferHeap)
      {
        LS_MemFinalize(service->pixelBufferHeap);
        service->pixelBufferHeap = NULL;
      }

      if (service->compositionBufferHeap)
      {
        LS_MemFinalize(service->compositionBufferHeap);
        service->compositionBufferHeap = NULL;
      }

      /* Also unregister from PTS manager if registered */
      if (service->ptsmgrClientID)
      {
        LS_PTSMgrUnRegisterClient(service->ptsmgrClientID);
        service->ptsmgrClientID = NULL;
      }

      ServiceInstanceDelete(service);
      service = NULL;
    }
  }

  LS_LEAVE("%s: LS_DVBSubDecServiceNew(),return service <%p>\n", ServiceErrString(res), (void*)service);
  return service;
}


LS_Status
LS_DVBSubDecServiceDelete(const LS_ServiceID_t serviceID)
{
  LS_Status ret_val = LS_OK;
  int32_t res = LS_OK;
  LS_ServiceState currentstate = LS_SERVICE_STATE_VOID;
  LS_Service* service = (LS_Service*)serviceID;

  LS_ENTER("LS_DVBSubDecServiceDelete(serviceID<%p>)\n", serviceID);

  do
  {
    res = verifyServiceID(service);

    if (LS_TRUE != res)
    {
      LS_ERROR("LS_ERROR_GENERAL: serviceID <%p> is invalid\n", serviceID);
      ret_val = LS_ERROR_GENERAL;
      break;
    }

    LS_MutexWait(service->serviceMutex);
    currentstate = service->state;

    if (currentstate != LS_SERVICE_STATE_NULL)
    {
      LS_ERROR("LS_ERROR_WRONG_STATE: serviceID<%p> is on %s now and can not be deleted\n",
               serviceID,
               ServiceStateString(currentstate));
      LS_MutexSignal(service->serviceMutex);
      ret_val = LS_ERROR_WRONG_STATE;
      break;
    }

    res = ServiceFactoryUnregister(service);

    if (LS_OK != res)
    {
      LS_WARNING("WARN: ServiceFactoryUnregister(serviceID<%p> failed)\n", serviceID);
      ret_val = LS_ERROR_GENERAL;
    }
    else
    {
      LS_INFO("serviceID<%p> is unregistered now\n", serviceID);
    }

    ServiceInstanceReset(service);
    LS_INFO("serviceID<%p> is reset\n", serviceID);
    LS_PTSMgrUnRegisterClient(service->ptsmgrClientID);
    LS_INFO("serviceID<%p> is unregistered from PTMgr\n", serviceID);

    if (service->codedDataBufferHeap)
    {
      LS_MemFinalize(service->codedDataBufferHeap);
      service->codedDataBufferHeap = NULL;
    }

    if (service->compositionBufferHeap)
    {
      LS_MemFinalize(service->compositionBufferHeap);
      service->compositionBufferHeap = NULL;
    }

    if (service->pixelBufferHeap)
    {
      LS_MemFinalize(service->pixelBufferHeap);
      service->pixelBufferHeap = NULL;
    }

    LS_MutexSignal(service->serviceMutex);
    ServiceInstanceDelete(service);
    LS_INFO("serviceID<%p> is deleted\n", serviceID);
    service = NULL;
  }while (0);

  LS_LEAVE("%s:LS_DVBSubDecServiceDelete()\n", ServiceErrString(ret_val));
  return ret_val;
}


LS_Status
LS_DVBSubDecServiceStart(const LS_ServiceID_t serviceID, const LS_OSDRender_t osdRender)
{
  LS_Status ret_val = LS_OK;
  LS_Service* service = (LS_Service*)serviceID;
  LS_ServiceState currentstate = LS_SERVICE_STATE_VOID;
  int32_t res = LS_OK;

  LS_ENTER("LS_DVBSubDecServiceStart(serviceID<%p>,osdRender<%p>)\n", (void*)serviceID, (void*)&osdRender);

  do
  {
    res = verifyServiceID(service);

    if (LS_TRUE != res)
    {
      LS_ERROR("LS_ERROR_GENERAL:serviceID<%p> is invalid\n", serviceID);
      ret_val = LS_ERROR_GENERAL;
      break;
    }

    LS_MutexWait(service->serviceMutex);
    SYS_MEMCPY((void*)&service->osdRender, (void*)&osdRender, sizeof(osdRender));
    currentstate = service->state;

    if (currentstate == LS_SERVICE_STATE_NULL)
    {
      LS_INFO("service <%p> will be started now\n", (void*)service);
      service->state = LS_SERVICE_STATE_READY;
      ret_val = LS_OK;
    }
    else if ((currentstate == LS_SERVICE_STATE_READY) ||
             (currentstate == LS_SERVICE_STATE_PLAYING) ||
             (currentstate == LS_SERVICE_STATE_PAUSED))
    {
      LS_INFO("service <%p> was started already\n", (void*)service);
      ret_val = LS_OK;
    }
    else
    {
      LS_INFO("service <%p> is in wrong state:%s(%d)\n",
              (void*)service,
              ServiceStateString(currentstate),
              currentstate);
      LS_MutexSignal(service->serviceMutex);
      ret_val = LS_ERROR_WRONG_STATE;
      break;
    }

    LS_MutexSignal(service->serviceMutex);
  }while (0);

  /* Only call LS_DVBSubDecServiceStop() if service was successfully initialized */
  if ((LS_OK != ret_val) &&
      (currentstate != LS_SERVICE_STATE_NULL))
  {
    LS_ERROR("service %p will be stopped\n", service);
    LS_DVBSubDecServiceStop(serviceID);
  }

  LS_LEAVE("%s:LS_DVBSubDecServiceStart()\n", ServiceErrString(ret_val));
  return ret_val;
}


LS_Status
LS_DVBSubDecServicePlay(const LS_ServiceID_t serviceID, const LS_CodedData_t* pesdata, const LS_PageId_t pageID)
{
  LS_PESHeaderInfo pesheader;
  LS_Service* service = (LS_Service*)serviceID;
  uint8_t* payload = NULL;
  uint32_t payloadsize = 0;
  int32_t res = LS_OK;

  LS_ENTER("LS_DVBSubDecServicePlay(serviceID<%p>,data<%p>,pageID<%d,%d>)\n",
           (void*)serviceID,
           pesdata,
           pageID.compositionPageId,
           pageID.ancillaryPageId);
  res = verifyServiceID(service);

  if (LS_TRUE != res)
  {
    LS_ERROR("LS_ERROR_GENERAL:serviceID<%p> is invalid\n", serviceID);
    return LS_ERROR_GENERAL;
  }

  service->pmtPageID.ancillaryPageId = pageID.ancillaryPageId;
  service->pmtPageID.compositionPageId = pageID.compositionPageId;
  res = LS_PreProcessPES(pesdata->data, &pesheader);

  if (LS_OK != res)
  {
    LS_ERROR("%s: stream error, abort...\n", ServiceErrString(res));
    return res;
  }
  else
  {
    payload = (uint8_t*)pesdata->data + kPES_header_data_length_OFFSET + kSIZE_OF_PES_header_data_length +
              pesheader.PES_header_data_length;
    payloadsize = pesheader.PES_packet_length - 3     /*distance to PES_header_data_length(included)*/
                  - pesheader.PES_header_data_length;
    res = ProcessPESPayLoad(service, &pesheader, payload, payloadsize);
    return res;
  }
}


LS_Status
LS_DVBSubDecServiceStop(const LS_ServiceID_t serviceID)
{
  LS_Status ret_val = LS_OK;
  int32_t res = LS_OK;
  LS_Service* service = NULL;
  LS_ServiceState curr_state = LS_SERVICE_STATE_VOID;

  LS_ENTER("LS_DVBSubDecServiceStop(serviceID<%p>)\n", (void*)serviceID);

  do
  {
    if (NULL == serviceID)
    {
      LS_INFO("LS_DVBSubDecServiceStop(): NULL == serviceID");
      ret_val = LS_OK;
      break;
    }

    service = (LS_Service*)serviceID;
    res = ValidateServiceMagicID(service->magic_id);

    if (LS_TRUE != res)
    {
      LS_ERROR("LS_ERROR_GENERAL:ValidateServiceMagicID() failed.\n");
      ret_val = LS_ERROR_GENERAL;
      break;
    }

    res = ServiceFactoryRegisteredStatus(service);

    if (LS_TRUE != res)
    {
      LS_ERROR("LS_ERROR_GENERAL:ServiceFactoryRegisteredStatus() failed.\n");
      ret_val = LS_ERROR_GENERAL;
      break;
    }

    LS_MutexWait(service->serviceMutex);
    curr_state = service->state;

    if ((curr_state == LS_SERVICE_STATE_PLAYING) ||
        (curr_state == LS_SERVICE_STATE_PAUSED) ||
        (curr_state == LS_SERVICE_STATE_READY))
    {
      LS_DEBUG("service<%p> is in (%s) and will be stopped!\n", (void*)service, ServiceStateString(curr_state));
    }
    else if (curr_state == LS_SERVICE_STATE_NULL)
    {
      LS_DEBUG("service<%p> was not started yet\n", (void*)service);
      ret_val = LS_OK;
      LS_MutexSignal(service->serviceMutex);
      break;
    }
    else
    {
      LS_ERROR("service<%p> is in %s and cannot be disabled\n", (void*)service, ServiceStateString(curr_state));
      ret_val = LS_ERROR_WRONG_STATE;
      LS_MutexSignal(service->serviceMutex);
      break;
    }

    if (service->displayPageOnScreen)
    {
      res = LS_ListRemoveNode(service->displayPageList, (void*)(service->displayPageOnScreen));

      if (res == LS_OK)
      {
        LS_DEBUG("service<%p>->displayPageOnScreen<%p> was " "removed from service->displayPageList<%p>\n",
                 service,
                 service->displayPageOnScreen,
                 service->displayPageList);
      }

      LS_DEBUG("service->displayPageOnScreen<%p> will be removed " "from the screen\n", service->displayPageOnScreen);
      LS_RemovePageFromScreen(service->displayPageOnScreen);
      LS_DEBUG("service->displayPageOnScreen<%p> will be deleted\n", service->displayPageOnScreen);
      LS_DisplayPageDelete(service, service->displayPageOnScreen);

      if (service->displayPageOnScreen == service->latestDisplayPage)
      {
        service->latestDisplayPage = NULL;
      }

      LS_DEBUG("service->displayPageOnScreen<%p> is deleted now\n", service->displayPageOnScreen);
      service->displayPageOnScreen = NULL;
    }

    res = LS_PTSMgrReset(service->ptsmgrClientID);

    if (LS_OK != res)
    {
      LS_WARNING("WARN:LS_PTSMgrReset(service<%p>,ptsmgrClientID<%p>) failed\n",
                 (void*)service,
                 service->ptsmgrClientID);
    }

    ServiceInstanceReset(service);
    service->state = LS_SERVICE_STATE_NULL;
    LS_MutexSignal(service->serviceMutex);
  }while (0);

  LS_LEAVE("%s:LS_DVBSubDecServiceStop()\n", ServiceErrString(ret_val));
  return ret_val;
}


LS_Status
LS_DVBSubDecServicePause(const LS_ServiceID_t serviceID)
{
  LS_Status ret_val = LS_OK;
  int32_t res = LS_OK;
  LS_Service* service = NULL;
  LS_ServiceState curr_state = LS_SERVICE_STATE_VOID;
  LS_Time_t left_ms;

  LS_ENTER("LS_DVBSubDecServicePause(serviceID<%p>)\n", (void*)serviceID);

  do
  {
    if (NULL == serviceID)
    {
      LS_INFO("LS_DVBSubDecServicePause(): NULL == serviceID");
      ret_val = LS_OK;
      break;
    }

    service = (LS_Service*)serviceID;
    res = ValidateServiceMagicID(service->magic_id);

    if (LS_TRUE != res)
    {
      LS_ERROR("LS_ERROR_GENERAL:ValidateServiceMagicID() failed.\n");
      ret_val = LS_ERROR_GENERAL;
      break;
    }

    res = ServiceFactoryRegisteredStatus(service);

    if (LS_TRUE != res)
    {
      LS_ERROR("LS_ERROR_GENERAL:ServiceFactoryRegisteredStatus() failed.\n");
      ret_val = LS_ERROR_GENERAL;
      break;
    }

    LS_MutexWait(service->serviceMutex);
    curr_state = service->state;

    if ((curr_state == LS_SERVICE_STATE_PLAYING) ||
        (curr_state == LS_SERVICE_STATE_READY))
    {
      LS_DEBUG("service<%p> is in (%s) and will be paused!\n", (void*)service, ServiceStateString(curr_state));
    }
    else if (curr_state == LS_SERVICE_STATE_PAUSED)
    {
      LS_DEBUG("service<%p> is already in LS_SERVICE_STATE_PAUSED\n", (void*)service);
      ret_val = LS_OK;
      LS_MutexSignal(service->serviceMutex);
      break;
    }
    else if (curr_state == LS_SERVICE_STATE_NULL)
    {
      LS_DEBUG("service<%p> was not started yet\n", (void*)service);
      ret_val = LS_OK;
      LS_MutexSignal(service->serviceMutex);
      break;
    }
    else
    {
      LS_ERROR("service<%p> is in %s state and cannot be disabled\n", (void*)service, ServiceStateString(curr_state));
      ret_val = LS_ERROR_WRONG_STATE;
      LS_MutexSignal(service->serviceMutex);
      break;
    }

    res = LS_PTSMgrSetSpeed(service->ptsmgrClientID, LS_DECODER_SPEED_PAUSED);

    if (res != LS_OK)
    {
      LS_ERROR("%s:LS_PTSMgrSetSpeed() failed\n", ServiceErrString(res));
      ret_val = res;
      LS_MutexSignal(service->serviceMutex);
      break;
    }

    if (service->displayPageOnScreen)
    {
      res = LS_TimerStop(service->displayPageOnScreen->page_time_out_timer, &left_ms);
      LS_DEBUG("Timer <%p> was stopped,page_time_out_time for " "display page<%p>\n",
               service->displayPageOnScreen->page_time_out_timer,
               service->displayPageOnScreen);

      if (res == LS_OK)
      {
        /*reset the page time-out time which will be used when service is resumed*/
        service->displayPageOnScreen->time_out -= left_ms.milliseconds / 1000;
      }
    }

    service->state = LS_SERVICE_STATE_PAUSED;
    LS_MutexSignal(service->serviceMutex);
  }while (0);

  LS_LEAVE("%s:LS_DVBSubDecServicePause()\n", ServiceErrString(ret_val));
  return ret_val;
}


LS_Status
LS_DVBSubDecServiceResume(const LS_ServiceID_t serviceID)
{
  LS_Status ret_val = LS_OK;
  int32_t res = LS_OK;
  LS_Service* service = NULL;
  LS_ServiceState curr_state = LS_SERVICE_STATE_VOID;

  LS_ENTER("LS_DVBSubDecServiceResume(serviceID<%p>)\n", (void*)serviceID);

  do
  {
    if (NULL == serviceID)
    {
      LS_INFO("LS_DVBSubDecServiceResume(): NULL == serviceID");
      ret_val = LS_OK;
      break;
    }

    service = (LS_Service*)serviceID;
    res = ValidateServiceMagicID(service->magic_id);

    if (LS_TRUE != res)
    {
      LS_ERROR("LS_ERROR_GENERAL:ValidateServiceMagicID() failed.\n");
      ret_val = LS_ERROR_GENERAL;
      break;
    }

    res = ServiceFactoryRegisteredStatus(service);

    if (LS_TRUE != res)
    {
      LS_ERROR("LS_ERROR_GENERAL:ServiceFactoryRegisteredStatus() failed.\n");
      ret_val = LS_ERROR_GENERAL;
      break;
    }

    LS_MutexWait(service->serviceMutex);
    curr_state = service->state;

    if (curr_state == LS_SERVICE_STATE_PAUSED)
    {
      LS_DEBUG("service<%p> is in (%s) and will be resumed!\n", (void*)service, ServiceStateString(curr_state));
    }
    else if (curr_state == LS_SERVICE_STATE_PLAYING)
    {
      LS_DEBUG("service<%p> is already in LS_SERVICE_STATE_PLAYING\n", (void*)service);
      ret_val = LS_OK;
      LS_MutexSignal(service->serviceMutex);
      break;
    }
    else
    {
      LS_ERROR("service<%p> is in %s state and cannot be resumed\n", (void*)service, ServiceStateString(curr_state));
      ret_val = LS_OK;
      LS_MutexSignal(service->serviceMutex);
      break;
    }

    res = LS_PTSMgrSetSpeed(service->ptsmgrClientID, service->serviceSpeed);

    if (res != LS_OK)
    {
      LS_ERROR("%s: LS_PTSMgrSetSpeed(service<%p>,serviceSpeed<%d>) failed\n",
               ServiceErrString(res),
               (void*)service,
               service->serviceSpeed);
      ret_val = res;
      LS_MutexSignal(service->serviceMutex);
      break;
    }

    if (service->displayPageOnScreen)
    {
      LS_TimerStart(service->displayPageOnScreen->page_time_out_timer, service->displayPageOnScreen->time_out * 1000);
      LS_DEBUG("page_time_out_timer %p for display page %p,"
               "(PTS:%llu,0x%08llx,%s) was re-started with duration = %d\n",
               (void*)service->displayPageOnScreen->page_time_out_timer,
               (void*)service->displayPageOnScreen,
               service->displayPageOnScreen->ptsValue,
               service->displayPageOnScreen->ptsValue,
               PTStoHMS(service->displayPageOnScreen->ptsValue),
               service->displayPageOnScreen->time_out);
    }

    service->state = LS_SERVICE_STATE_PLAYING;
    LS_MutexSignal(service->serviceMutex);
  }while (0);

  LS_LEAVE("%s:LS_DVBSubDecServicePause()\n", ServiceErrString(ret_val));
  return ret_val;
}


LS_Status
LS_DVBSubDecServiceMute(const LS_ServiceID_t serviceID)
{
  LS_Status ret_val = LS_OK;
  int32_t res = LS_OK;
  LS_Service* service = (LS_Service*)serviceID;
  LS_ServiceState curr_state = LS_SERVICE_STATE_VOID;

  LS_ENTER("LS_DVBSubDecServiceMute(serviceID<%p>)\n", (void*)serviceID);

  do
  {
    res = verifyServiceID(service);

    if (LS_TRUE != res)
    {
      LS_ERROR("LS_ERROR_GENERAL: serviceID <%p> is invalid\n", serviceID);
      ret_val = LS_ERROR_GENERAL;
      break;
    }

    LS_MutexWait(service->serviceMutex);
    curr_state = service->state;

    if ((curr_state == LS_SERVICE_STATE_NULL) ||
        (curr_state == LS_SERVICE_STATE_VOID) ||
        (curr_state == LS_SERVICE_STATE_READY))
    {
      LS_WARNING("service<%p> is not in playing mode,this command " "is invalid\n", service);
      ret_val = LS_OK;
      LS_MutexSignal(service->serviceMutex);
      break;
    }

    if (service->displayPageOnScreen)
    {
      LS_RemovePageFromScreen(service->displayPageOnScreen);

      if (service->displayPageOnScreen != service->latestDisplayPage)
      {
        LS_DisplayPageDelete(service, service->displayPageOnScreen);
      }

      service->displayPageOnScreen = NULL;
      service->muteState = LS_TRUE;
    }

    LS_MutexSignal(service->serviceMutex);
  }while (0);

  LS_LEAVE("%s:LS_DVBSubDecServiceMute()\n", ServiceErrString(ret_val));
  return ret_val;
}


LS_Status
LS_DVBSubDecServiceUnMute(const LS_ServiceID_t serviceID)
{
  LS_Status ret_val = LS_OK;
  int32_t res = LS_OK;
  LS_Service* service = (LS_Service*)serviceID;

  LS_ENTER("LS_DVBSubDecServiceUnMute(serviceID<%p>)\n", (void*)serviceID);

  do
  {
    res = verifyServiceID(service);

    if (LS_TRUE != res)
    {
      LS_ERROR("LS_ERROR_GENERAL: serviceID <%p> is invalid\n", serviceID);
      ret_val = LS_ERROR_GENERAL;
      break;
    }

    LS_MutexWait(service->serviceMutex);
    service->muteState = LS_FALSE;
    LS_MutexSignal(service->serviceMutex);
  }while (0);

  LS_LEAVE("%s:LS_DVBSubDecServiceMute()\n", ServiceErrString(ret_val));
  return ret_val;
}


LS_Status
LS_DVBSubDecServiceEnableCustomerDDS(const LS_ServiceID_t serviceID, const LS_CustomerDDS_t dds)
{
  LS_Status ret_val = LS_OK;
  int32_t res = LS_OK;
  LS_Service* service = (LS_Service*)serviceID;

  LS_ENTER("LS_DVBSubDecServiceEnableCustomerDDS(serviceID<%p>)\n", (void*)serviceID);

  do
  {
    res = verifyServiceID(service);

    if (LS_TRUE != res)
    {
      LS_ERROR("LS_ERROR_GENERAL: serviceID <%p> is invalid\n", serviceID);
      ret_val = LS_ERROR_GENERAL;
      break;
    }

    LS_MutexWait(service->serviceMutex);
    service->DDSFlag = LS_TRUE;
    service->DDS = dds;
    LS_MutexSignal(service->serviceMutex);
    LS_DEBUG("serviceID <%p> 's customer DDS is enabled\n", (void*)service);
    ret_val = LS_OK;
  }while (0);

  LS_LEAVE("%s:LS_DVBSubDecServiceEnableCustomerDDS()\n", ServiceErrString(ret_val));
  return ret_val;
}


LS_Status
LS_DVBSubDecServiceDisableCustomerDDS(const LS_ServiceID_t serviceID)
{
  LS_Status ret_val = LS_OK;
  int32_t res = LS_OK;
  LS_Service* service = (LS_Service*)serviceID;

  LS_ENTER("LS_DVBSubDecServiceDisableCustomerDDS(serviceID<%p>)\n", (void*)serviceID);

  do
  {
    res = verifyServiceID(service);

    if (LS_TRUE != res)
    {
      LS_ERROR("LS_ERROR_GENERAL: serviceID <%p> is invalid\n", serviceID);
      ret_val = LS_ERROR_GENERAL;
      break;
    }

    LS_MutexWait(service->serviceMutex);
    service->DDSFlag = LS_TRUE;
    LS_MutexSignal(service->serviceMutex);
    LS_DEBUG("serviceID <%p> 's customer DDS is disabledd\n", (void*)service);
    ret_val = LS_OK;
  }while (0);

  LS_LEAVE("%s:LS_DVBSubDecServiceDisableCustomerDDS()\n", ServiceErrString(ret_val));
  return ret_val;
}


LS_Status
LS_DVBSubDecServiceCurrentState(const LS_ServiceID_t serviceID, LS_ServiceState* state)
{
  LS_Status ret_val = LS_OK;
  int32_t res = LS_OK;
  LS_Service* service = (LS_Service*)serviceID;

  LS_ENTER("LS_DVBSubDecServiceCurrentState(serviceID<%p>)\n", (void*)serviceID);

  do
  {
    res = verifyServiceID(service);

    if (LS_TRUE != res)
    {
      LS_ERROR("LS_ERROR_GENERAL: serviceID <%p> is invalid\n", serviceID);
      ret_val = LS_ERROR_GENERAL;
      break;
    }

    LS_MutexWait(service->serviceMutex);
    *state = service->state;
    LS_MutexSignal(service->serviceMutex);
    LS_DEBUG("serviceID <%p> 's current state is %s\n", (void*)service, ServiceStateString(*state));
    ret_val = LS_OK;
  }while (0);

  LS_LEAVE("%s:LS_DVBSubDecServiceCurrentState()\n", ServiceErrString(ret_val));
  return ret_val;
}


LS_Status
LS_DVBSubDecServiceSetDisplayResolution(const LS_ServiceID_t serviceID, int32_t width, int32_t height)
{
  LS_Status ret_val = LS_OK;
  int32_t res = LS_OK;
  LS_Service* service = (LS_Service*)serviceID;

  LS_ENTER("LS_DVBSubDecServiceSetDisplayResolution(serviceID<%p>,width<%d>,height<%d>)\n",
           (void*)serviceID,
           width,
           height);

  do
  {
    res = verifyServiceID(service);

    if (LS_TRUE != res)
    {
      LS_ERROR("LS_ERROR_GENERAL: serviceID <%p> is invalid\n", serviceID);
      ret_val = LS_ERROR_GENERAL;
      break;
    }

    LS_MutexWait(service->serviceMutex);
    service->displayHeight = CLAMP(height, 0, 4096);
    service->displayWidth = CLAMP(width, 0, 4096);
    LS_MutexSignal(service->serviceMutex);
    ret_val = LS_OK;
  }while (0);

  LS_LEAVE("%s:LS_DVBSubDecServiceSetDisplayResolution()\n", ServiceErrString(ret_val));
  return ret_val;
}


LS_Status
LS_DVBSubDecServiceSetSpeed(const LS_ServiceID_t serviceID, int32_t speed)
{
  LS_Status ret_val = LS_OK;
  int32_t res = LS_OK;
  LS_Service* service = (LS_Service*)serviceID;

  LS_ENTER("LS_DVBSubDecServiceSetSpeed(serviceID=%p,speed=%d)\n", (void*)serviceID, speed);

  do
  {
    res = verifyServiceID(service);

    if (LS_TRUE != res)
    {
      LS_ERROR("LS_ERROR_GENERAL: serviceID <%p> is invalid\n", serviceID);
      ret_val = LS_ERROR_GENERAL;
      break;
    }

    LS_MutexWait(service->serviceMutex);
    res = LS_PTSMgrSetSpeed(service->ptsmgrClientID, service->serviceSpeed);

    if (LS_OK != res)
    {
      LS_ERROR("%s:LS_PTSMgrSetSpeed() failed\n", ServiceErrString(res));
      ret_val = res;
      LS_MutexSignal(service->serviceMutex);
      break;
    }

    if (speed < 0)
    {
      LS_DEBUG("serviceID <%p>,speed <%d> will reset the service\n", (void*)service, service->serviceSpeed);
      ServiceInstanceReset(service);
    }

    service->serviceSpeed = speed;
    ret_val = LS_OK;
    LS_MutexSignal(service->serviceMutex);
  }while (0);

  LS_LEAVE("%s:LS_DVBSubDecServiceSetSpeed()\n", ServiceErrString(ret_val));
  return ret_val;
}


LS_Status
LS_DVBSubDecServiceReset(const LS_ServiceID_t serviceID)
{
  LS_Status ret_val = LS_OK;
  int32_t res = LS_OK;
  LS_Service* service = (LS_Service*)serviceID;

  LS_ENTER("LS_DVBSubDecServiceReset(serviceID=%p)\n", (void*)serviceID);

  do
  {
    res = verifyServiceID(service);

    if (LS_TRUE != res)
    {
      LS_ERROR("LS_ERROR_GENERAL: serviceID <%p> is invalid\n", serviceID);
      ret_val = LS_ERROR_GENERAL;
      break;
    }

    LS_MutexWait(service->serviceMutex);

    if (service->displayPageOnScreen)
    {
      LS_RemovePageFromScreen(service->displayPageOnScreen);
      LS_DisplayPageDelete(service, service->displayPageOnScreen);

      if (service->displayPageOnScreen == service->latestDisplayPage)
      {
        service->latestDisplayPage = NULL;
      }

      service->displayPageOnScreen = NULL;
    }

    ServiceInstanceReset(service);
    LS_MutexSignal(service->serviceMutex);
  }while (0);

  LS_LEAVE("status=%d\n", ret_val);
  return ret_val;
}


#endif                                                                                        /*__LS_SUBDEC_C__*/
/*EOF*/
