/*-----------------------------------------------------------------------------
 * lsptsmgr.c
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

#include "lssubdecoder.h"
#include "lsptsmgr.h"
#include "lssubmacros.h"
#include "lssystem.h"
#include "lsmemory.h"
#include "lslist.h"
#include "lssubutils.h"
#include <inttypes.h>
#include <stdbool.h>
/*---------------------------------------------------------------------------
 * local struct
 *--------------------------------------------------------------------------*/
typedef struct _LS_PTSMgrTimerInfo  LS_PTSMgrTimerInfo;
typedef struct _LS_PTSMgr           LS_PTSMgr;

struct _LS_PTSMgr
{
  LS_List*   clientInfo;
  uint32_t   ref_count;
  LS_Mutex_t mutex;
};
struct _LS_PTSMgrTimerInfo
{
  uint32_t          magic_id;
  LS_PTSRequestInfo pts_rquest;
  LS_Timer_t        pts_timer;
  uint32_t          timeout_ms;
  int32_t           current_speed;
  LS_Time_t         time_left;
  int32_t           reason;
};


/*---------------------------------------------------------------------------
 * local macros
 *--------------------------------------------------------------------------*/
#define LS_PTS_MAX_SERVICE    16
#define LS_TIMERINFO_MAGIC    MAKE_MAGIC_NUMBER('T', 'I', 'M', 'E')
/*---------------------------------------------------------------------------
 * local static variables
 *--------------------------------------------------------------------------*/
static LS_PTSMgr* __ptsMgr = NULL;
// File-scope variables for PTS normalization (file playback mode)
static uint64_t s_pts_base = 0;           // Base PTS for normalization
static bool s_pts_base_initialized = false;

/*---------------------------------------------------------------------------
 * local static functions declarations
 *--------------------------------------------------------------------------*/
static LS_PTSMgr*          ptsMgrNew(void);
static void                ptsMgrDelete(LS_PTSMgr* mgr);
static LS_PTSMgrTimerInfo* ptsMgrTimerInfoNew(void);
static void                ptsMgrTimerInfoDelete(LS_PTSMgrTimerInfo* timerinfo, void* user_data);
static int32_t             findTimerInfobyRequest(void* a, void* b);
static void                ptsMgrTimerCallback(void* param);
static void                ptsMgrListDestroyFunc(void* a, void* b);

/*---------------------------------------------------------------------------
 * local static functions
 *--------------------------------------------------------------------------*/
static LS_PTSMgr*
ptsMgrNew(void)
{
  LS_PTSMgr* mgr = NULL;
  int32_t status = 0;
  int32_t errcode = 0;

  mgr = (LS_PTSMgr*)LS_Malloc(ServiceSystemHeap(), sizeof(LS_PTSMgr));
  DEBUG_CHECK(mgr != NULL);

  if (mgr == NULL)
  {
    LS_ERROR("LS_ERROR_SYSTEM_BUFFER: Request for %d bytes failed.\n", sizeof(LS_PTSMgr));
    return NULL;
  }

  SYS_MEMSET((void*)mgr, 0, sizeof(LS_PTSMgr));

  if ((status = LS_MutexCreate(&(mgr->mutex))) != LS_OK)
  {
    LS_ERROR("LS_ERROR_SYSTEM_ERROR: create a new mutex failed\n");
    LS_Free(ServiceSystemHeap(), (void*)mgr);
    return NULL;
  }

  mgr->clientInfo = LS_ListInit(__listMallocFunc, __listFreeFunc, &errcode);
  DEBUG_CHECK(mgr->clientInfo != NULL);

  if (mgr->clientInfo == NULL)
  {
    LS_MutexDelete(mgr->mutex);
    SYS_MEMSET((void*)mgr, 0, sizeof(LS_PTSMgr));
    LS_Free(ServiceSystemHeap(), (void*)mgr);
    LS_ERROR("Failed when create a new list.\n");
    return NULL;
  }

  return mgr;
}


static void
ptsMgrDelete(LS_PTSMgr* mgr)
{
  uint32_t number = 0;
  int32_t status = LS_OK;

  if (mgr == NULL)
  {
    return;
  }

  if (mgr->clientInfo)
  {
    status = LS_ListCount(mgr->clientInfo, &number);
    DEBUG_CHECK(status == LS_OK);
    LS_DEBUG("There are %d registered clients.\n", number);
    LS_ListEmpty(mgr->clientInfo);
    LS_ListDestroy(mgr->clientInfo);
  }

  LS_MutexDelete(mgr->mutex);
  SYS_MEMSET((void*)mgr, 0, sizeof(LS_PTSMgr));
  LS_Free(ServiceSystemHeap(), (void*)mgr);
}


static LS_PTSMgrTimerInfo*
ptsMgrTimerInfoNew(void)
{
  LS_PTSMgrTimerInfo* timerinfo = NULL;

  timerinfo = (LS_PTSMgrTimerInfo*)LS_Malloc(ServiceSystemHeap(), sizeof(LS_PTSMgrTimerInfo));
  DEBUG_CHECK(timerinfo != NULL);

  if (timerinfo == NULL)
  {
    LS_ERROR("Memory Error: request %d bytes failed\n", sizeof(LS_PTSMgrTimerInfo));
    return NULL;
  }

  SYS_MEMSET((void*)timerinfo, 0, sizeof(LS_PTSMgrTimerInfo));
  timerinfo->magic_id = LS_TIMERINFO_MAGIC;
  return timerinfo;
}


static void
ptsMgrTimerInfoDelete(LS_PTSMgrTimerInfo* timerinfo, void* user_data)
{
  LS_Time_t timeleft;

  (void)user_data;  /* Unused */

  if (timerinfo != NULL)
  {
    if (timerinfo->pts_timer)
    {
      LS_TimerStop(timerinfo->pts_timer, &timeleft);
      LS_INFO("Timer <%p> was stopped,pts_timer\n", (void*)(timerinfo->pts_timer));
      LS_TimerDelete(timerinfo->pts_timer);
      LS_INFO("Timer <%p> was deleted,pts_timer\n", (void*)(timerinfo->pts_timer));
      timerinfo->pts_timer = NULL;
    }

    SYS_MEMSET((void*)timerinfo, 0, sizeof(LS_PTSMgrTimerInfo));
    timerinfo->magic_id = 0xDEADBEEF;
    LS_Free(ServiceSystemHeap(), (void*)timerinfo);
    LS_DEBUG("timerinfo %p was free'd\n", (void*)timerinfo);
  }
}


static void
ptsMgrListDestroyFunc(void* data, void* user_data)
{
  LS_PTSMgrTimerInfo* timerinfo = (LS_PTSMgrTimerInfo*)data;

  ptsMgrTimerInfoDelete(timerinfo, user_data);
}


static void
ptsMgrTimerCallback(void* param)
{
  LS_PTSMgrClientID clientId = NULL;
  LS_PTSMgrTimerInfo* timerinfo = NULL;
  int32_t status = LS_OK;
  LS_Time_t left_ms;

  LS_INFO("Entering ptsMgrTimerCallback(param = %p)\n", (void*)param);

  if (param == NULL)
  {
    LS_ERROR("Invalid param: NULL\n");
    return;
  }

  LS_MutexWait(__ptsMgr->mutex);
  timerinfo = (LS_PTSMgrTimerInfo*)param;

  if (timerinfo == NULL)
  {
    LS_ERROR("Bad param:nil\n");
    LS_MutexSignal(__ptsMgr->mutex);
    return;
  }

  if (timerinfo->magic_id != LS_TIMERINFO_MAGIC)
  {
    LS_ERROR("timerinfo %p is corrupted with bad magic_id = 0x%08x\n", (void*)timerinfo, timerinfo->magic_id);
    LS_MutexSignal(__ptsMgr->mutex);
    return;
  }

  /*stop the timer first*/
  if (timerinfo->pts_timer)
  {
    status = LS_TimerStop(timerinfo->pts_timer, &left_ms);
    DEBUG_CHECK(status == LS_OK);
    LS_INFO("Timer <%p> was stopped,pts_timer\n", (void*)(timerinfo->pts_timer));
    status = LS_TimerDelete(timerinfo->pts_timer);
    DEBUG_CHECK(status == LS_OK);
    LS_INFO("Timer <%p> was deleted,pts_timer\n", (void*)(timerinfo->pts_timer));
    timerinfo->pts_timer = NULL;
  }

  clientId = timerinfo->pts_rquest.clientId;
  LS_DEBUG("clientId = %p\n", (void*)clientId);
  status = LS_ListFindNode(__ptsMgr->clientInfo, (void*)clientId);

  if (status != LS_OK)
  {
    LS_ERROR("Invalid clientId: %p found\n", (void*)clientId);
    LS_MutexSignal(__ptsMgr->mutex);
    return;
  }

  LS_DEBUG("timerinfo = %p, clientId = %p\n", (void*)timerinfo, (void*)clientId);

  if (timerinfo->pts_rquest.PTSCallBackFunc)
  {
    timerinfo->pts_rquest.PTSCallBackFunc(timerinfo->pts_rquest.ptsValue,
                                          timerinfo->reason,
                                          timerinfo->pts_rquest.PTSCBFuncData);
  }
  else
  {
    LS_ERROR("There is no PTSCallBackFunc setup for (PTS:%llu,0x%08llx,%s).\n",
             timerinfo->pts_rquest.ptsValue,
             timerinfo->pts_rquest.ptsValue,
             PTStoHMS(timerinfo->pts_rquest.ptsValue));
    LS_MutexSignal(__ptsMgr->mutex);
    return;
  }

  status = LS_ListRemoveNode((LS_List*)clientId, (void*)timerinfo);
  DEBUG_CHECK(status == LS_OK);
  SYS_MEMSET((void*)timerinfo, 0, sizeof(LS_PTSMgrTimerInfo));
  ptsMgrTimerInfoDelete(timerinfo, NULL);
  LS_MutexSignal(__ptsMgr->mutex);
}


static int32_t
findTimerInfobyRequest(void* a, void* b)
{
  LS_PTSRequestInfo* request = NULL;
  LS_PTSMgrTimerInfo* timerinfo = NULL;

  if ((a == NULL) ||
      (b == NULL))
  {
    return -1;
  }

  request = (LS_PTSRequestInfo*)b;
  timerinfo = (LS_PTSMgrTimerInfo*)a;

  if (timerinfo->pts_rquest.ptsValue == request->ptsValue)
  {
    return 0;
  }
  else
  {
    return 1;
  }
}


/*---------------------------------------------------------------------------
 * public APIs
 *--------------------------------------------------------------------------*/
int32_t
LS_PTSMgrInit(void)
{
  if (__ptsMgr)
  {
    LS_INFO("PTS Manager was inited.\n");
    LS_MutexWait(__ptsMgr->mutex);
    __ptsMgr->ref_count += 1;
    LS_MutexSignal((__ptsMgr->mutex));
    return LS_OK;
  }

  __ptsMgr = ptsMgrNew();

  if (__ptsMgr == NULL)
  {
    LS_ERROR("Cannot init PTS Manager.\n");
    return LS_ERROR_GENERAL;
  }

  LS_MutexWait(__ptsMgr->mutex);
  __ptsMgr->ref_count += 1;
  LS_MutexSignal((__ptsMgr->mutex));
  return LS_OK;
}


LS_PTSMgrClientID
LS_PTSMgrRegisterClient(int32_t* errorCode)
{
  LS_List* list = NULL;
  int32_t status = LS_OK;
  int32_t errcode = LS_ERROR_GENERAL;

  list = LS_ListInit(__listMallocFunc, __listFreeFunc, &errcode);

  if (list == NULL)
  {
    LS_ERROR("%s: Cannot register to PTSMgr\n", ServiceErrString(errcode));
    *errorCode = errcode;
    return NULL;
  }
  else
  {
    status = LS_ListAppend(__ptsMgr->clientInfo, (void*)list);

    if (status != LS_OK)
    {
      LS_ERROR("%s: Cannot register to PTSMgr\n", ServiceErrString(status));
      LS_ListDestroy(list);
      *errorCode = status;
      return NULL;
    }
    else
    {
      LS_DEBUG("Successfully registered to PTSMgr,client ID=%p\n", (void*)list);
      *errorCode = LS_OK;
      return (LS_PTSMgrClientID)list;
    }
  }
}


void
LS_PTSMgrUnRegisterClient(LS_PTSMgrClientID clientId)
{
  int32_t status = LS_OK;

  status = LS_ListRemoveNode(__ptsMgr->clientInfo, (void*)clientId);
  DEBUG_CHECK(status == LS_OK);

  if (status == LS_OK)
  {
    status = LS_ListEmptyData((LS_List*)clientId, (DestroyFunc)ptsMgrTimerInfoDelete, NULL);
    DEBUG_CHECK(status == LS_OK);
    status = LS_ListDestroy((LS_List*)clientId);
    DEBUG_CHECK(status == LS_OK);
  }

  LS_DEBUG("Unregister clientId %p return %d\n", (void*)clientId, status);
}


int32_t
LS_PTSMgrRegisterPTS(LS_PTSRequestInfo* request)
{
  LS_PTSMgrClientID clientId = NULL;
  LS_PTSMgrTimerInfo* timerinfo = NULL;
  LS_GetgetCurrentPCRFunc getCurrentVideoPCRFct = NULL;
  int32_t status = LS_OK;
  uint32_t pcr_ms;
  uint32_t pts_ms;
  uint32_t mx_pcr_ms;
  int32_t pts_pcr_delta;
  int32_t normalized_delta;
  uint64_t current_pcr;
  int32_t pcr_wrapped = LS_FALSE;

  if (request == NULL)
  {
    LS_ERROR("Invalid parameter: NULL\n");
    return LS_ERROR_GENERAL;
  }

  clientId = request->clientId;
  status = LS_ListFindNode(__ptsMgr->clientInfo, (void*)clientId);

  if (status != LS_OK)
  {
    LS_ERROR("clientId %p was invalid ID\n", (void*)clientId);
    return LS_ERROR_GENERAL;
  }

  getCurrentVideoPCRFct = request->getCurrentPCRFunc;

  if (getCurrentVideoPCRFct == NULL)
  {
    LS_ERROR("Invalid PTS request: no function to get current PCR value\n");
    return LS_ERROR_GENERAL;
  }

  /*request sanity check done*/
  timerinfo = ptsMgrTimerInfoNew();
  DEBUG_CHECK(timerinfo != NULL);

  if (timerinfo == NULL)
  {
    LS_ERROR("ptsMgrTimerInfoNew failed\n");
    return LS_ERROR_GENERAL;
  }

  status = LS_TimerNew(&(timerinfo->pts_timer), ptsMgrTimerCallback, (void*)timerinfo);

  if (status != LS_OK)
  {
    LS_ERROR("Cannot create timer,failed.\n");
    ptsMgrTimerInfoDelete(timerinfo, NULL);
    return LS_ERROR_GENERAL;
  }

  LS_INFO("Timer <%p> was created,timerinfo->pts_timer = 0x%08x\n", timerinfo, &(timerinfo->pts_timer));

  // IMPORTANT: Initialize base PTS BEFORE calling getCurrentVideoPCRFct
  // For file playback, skip PTS=0 entries which are from immediate-firing displaysets
  if (!s_pts_base_initialized)
  {
    if (request->ptsValue > 0)
    {
      s_pts_base = request->ptsValue;
      s_pts_base_initialized = true;
      LS_DEBUG("BASE PTS initialized to %llu (for normalization)\n", (unsigned long long)s_pts_base);
    }
    else
    {
      LS_DEBUG("Skipping PTS=0 for base PTS initialization\n");
    }
  }

  status = getCurrentVideoPCRFct(&current_pcr, request->getCurrentPCRFuncData);

  if (status != LS_OK)
  {
    LS_ERROR("Cannot get current PCR value\n");
    ptsMgrTimerInfoDelete(timerinfo, NULL);
    return LS_ERROR_GENERAL;
  }

  SYS_MEMCPY(&(timerinfo->pts_rquest), request, sizeof(LS_PTSRequestInfo));

  /**
   * now calculate the delta value between current PCR and future PTS.
   */
  // Normalize PTS value relative to base
  uint64_t normalized_pts = request->ptsValue - s_pts_base;

  pts_ms = (uint32_t)(normalized_pts / 90);
  LS_DEBUG("PTS=%llu norm=%llu ms=%u base=%llu\n",
           (unsigned long long)request->ptsValue,
           (unsigned long long)normalized_pts,
           pts_ms,
           (unsigned long long)s_pts_base);
  pcr_ms = (uint32_t)(current_pcr / 90);
  LS_TRACE("LS_PTSMgrRegisterPTS (PCR:%llu,0x%08llx,%s) =%u ms\n",
           current_pcr,
           current_pcr,
           PTStoHMS(current_pcr),
           pcr_ms);

  /*check if PCR wrapped*/
  if ((pts_ms < pcr_ms) &&
      ((pcr_ms - pts_ms) >= 12 * 60 * 60 * 1000))
  {
    LS_DEBUG("PCR roll over mode?\n");
    pcr_wrapped = LS_TRUE;
  }

  if (pcr_wrapped)
  {
    mx_pcr_ms = ((uint64_t)0x1FFFFFFFF) / 90;
    pts_pcr_delta = mx_pcr_ms - pcr_ms + pts_ms;
  }
  else
  {
    pts_pcr_delta = pts_ms - pcr_ms;
  }

  LS_DEBUG("pts_pcr_delta = %d ms\n", pts_pcr_delta);

  if (request->speed < 0)
  {
    LS_INFO("Speed %d indicates rewind mode,PTS request will be cancelled\n", request->speed);
    timerinfo->timeout_ms = 1;
    timerinfo->current_speed = request->speed;
    timerinfo->reason = PTS_INVALID;
    status = LS_ListAppend((LS_List*)request->clientId, (void*)timerinfo);
    status = LS_TimerStart(timerinfo->pts_timer, 5);
    DEBUG_CHECK(status == LS_OK);
    return LS_OK;
  }

  if (request->speed == 0)
  {
    LS_INFO("Speed %d indicates paused mode and request is not accepted\n");
    return LS_OK;
  }

  if (request->speed > 0)
  {
    normalized_delta = pts_pcr_delta * 100 / request->speed;

    // Clamp maximum delay for file playback (prevent excessively long waits)
    // Increased from 10s to 60s to handle legitimate subtitle timing differences
    const uint32_t MAX_FILE_DELAY_MS = 60000;

    if (normalized_delta > (int32_t)MAX_FILE_DELAY_MS)
    {
      LS_INFO("Clamping delay from %d ms to %d ms (exceeds maximum)\n", normalized_delta, MAX_FILE_DELAY_MS);
      normalized_delta = MAX_FILE_DELAY_MS;
    }

    LS_DEBUG("pts=%llu pts_ms=%u pcr=%" PRIu64 " pcr_ms=%u delta=%d norm_delta=%d speed=%d\n",
             (unsigned long long)request->ptsValue,
             pts_ms,
             (unsigned long long)current_pcr,
             pcr_ms,
             pts_pcr_delta,
             normalized_delta,
             request->speed);

    // For file playback (PCR=0), add 2-second lead time to first subtitle
    // This ensures first subtitle displays after 2 seconds instead of immediately
    static bool first_subtitle_done = false;

    if ((current_pcr == 0) &&
        !first_subtitle_done &&
        (normalized_delta >= 0))
    {
      normalized_delta += 2000;        // Add 2 seconds
      first_subtitle_done = true;
      LS_DEBUG("Added 2-second lead time for first subtitle, new delay=%d ms\n", normalized_delta);
    }

    if (normalized_delta <= 0)
    {
      LS_WARNING("(PTS:%llu,0x%08llx,%s) request is late for current PCR," "will be fired immediately\n",
                 request->ptsValue,
                 request->ptsValue,
                 PTStoHMS(request->ptsValue));
      normalized_delta = 1;
      timerinfo->timeout_ms = normalized_delta;
      timerinfo->current_speed = request->speed;
      timerinfo->reason = PTS_PRESENTED;
      status = LS_ListAppend((LS_List*)request->clientId, (void*)timerinfo);
      LS_INFO("timer started for (PTS:%llu,0x%08llx,%s),duration %d ms\n",
              request->ptsValue,
              request->ptsValue,
              PTStoHMS(request->ptsValue),
              normalized_delta);
      status = LS_TimerStart(timerinfo->pts_timer, normalized_delta);
      DEBUG_CHECK(status == LS_OK);
    }
    else if (normalized_delta > 0)
    {
      LS_DEBUG("(PTS:%llu,0x%08llx,%s)will be presented in %d ms\n",
               request->ptsValue,
               request->ptsValue,
               PTStoHMS(request->ptsValue),
               normalized_delta);
      timerinfo->timeout_ms = pts_pcr_delta;
      timerinfo->current_speed = request->speed;
      timerinfo->reason = PTS_PRESENTED;
      status = LS_ListAppend((LS_List*)request->clientId, (void*)timerinfo);
      LS_INFO("timer started for (PTS:%llu,0x%08llx,%s) with duration %d ms\n",
              request->ptsValue,
              request->ptsValue,
              PTStoHMS(request->ptsValue),
              pts_pcr_delta);
      status = LS_TimerStart(timerinfo->pts_timer, normalized_delta);
      DEBUG_CHECK(status == LS_OK);
    }

    return LS_OK;
  }

  return LS_OK;
}


int32_t
LS_PTSMgrCancelPTS(LS_PTSRequestInfo* request)
{
  LS_PTSMgrClientID clientId = NULL;
  LS_PTSMgrTimerInfo* timerinfo = NULL;
  int32_t status = LS_OK;

  if (request == NULL)
  {
    return LS_ERROR_GENERAL;
  }

  clientId = request->clientId;
  status = LS_ListFindNode(__ptsMgr->clientInfo, (void*)clientId);

  if (status != LS_OK)
  {
    LS_ERROR("clientId %p was invalid ID\n", (void*)clientId);
    return LS_ERROR_GENERAL;
  }

  timerinfo = LS_ListRemoveUserNode((LS_List*)(clientId), (void*)request, findTimerInfobyRequest);

  if (timerinfo == NULL)
  {
    LS_WARNING("Request for (PTS:%llu,0x%08llx,%s) was fired or wrong request\n",
               request->ptsValue,
               request->ptsValue,
               PTStoHMS(request->ptsValue));
    return LS_OK;
  }
  else
  {
    LS_TimerStop(timerinfo->pts_timer, &(timerinfo->time_left));
    LS_INFO("Timer <%p> was stopped,pts_timer\n", (void*)(timerinfo->pts_timer));
    LS_TimerDelete(timerinfo->pts_timer);
    LS_INFO("Timer <%p> was deleted,pts_timer\n", (void*)(timerinfo->pts_timer));
    timerinfo->pts_timer = NULL;
    ptsMgrTimerInfoDelete(timerinfo, NULL);
    return LS_OK;
  }
}


int32_t
LS_PTSMgrReset(LS_PTSMgrClientID clientId)
{
  uint32_t counter = 0;
  int32_t status = LS_OK;

  status = LS_ListFindNode(__ptsMgr->clientInfo, (void*)clientId);

  if (status != LS_OK)
  {
    LS_ERROR("clientId %p was invalid ID\n", clientId);
    return LS_ERROR_GENERAL;
  }

  status = LS_ListCount((LS_List*)clientId, &counter);

  if (status != LS_OK)
  {
    LS_ERROR("LS_ListCount() failed\n");
    return LS_ERROR_GENERAL;
  }

  LS_DEBUG("There are total %d requests for client %p to be cancelled\n", counter, clientId);
  status = LS_ListEmptyData((LS_List*)clientId, ptsMgrListDestroyFunc, (void*)NULL);

  if (status != LS_OK)
  {
    LS_ERROR("LS_PTSMgrReset failed for client %p\n", (void*)clientId);
    return LS_ERROR_GENERAL;
  }

  return LS_OK;
}


int32_t
LS_PTSMgrSetSpeed(LS_PTSMgrClientID clientId, int32_t speed)
{
  uint32_t count = 0;
  int32_t status = LS_OK;
  int32_t i = 0;
  LS_PTSMgrTimerInfo* timerinfo = NULL;
  LS_Time_t left_ms = { 0, 0 };

  status = LS_ListFindNode(__ptsMgr->clientInfo, (void*)clientId);

  if (status != LS_OK)
  {
    LS_ERROR("clientId %p was an INVALID ID\n", (void*)clientId);
    return LS_ERROR_GENERAL;
  }

  /**
   * for each timer in client list, stop it and save the left time
   * when next time speed is changed again, restart the timer based
   * on the speed and left time to calcutet the time-out time.
   */
  status = LS_ListCount((LS_List*)clientId, &count);
  DEBUG_CHECK(status == LS_OK);
  LS_DEBUG("There is %d pending PTS requests for client %p\n", count, (void*)clientId);

  for (i = 0; i < (int32_t)count; i++)
  {
    timerinfo = LS_ListNthNode((LS_List*)clientId, i);

    if (timerinfo)
    {
      if (speed < 0)
      {
        LS_DEBUG("Rewinding mode,all pending PTS request will be cancelled\n");

        if (timerinfo->current_speed != 0)
        {
          LS_TimerStop(timerinfo->pts_timer, &timerinfo->time_left);
          LS_DEBUG("Timer <%p> was stopped,pts_timer\n", timerinfo->pts_timer);
        }

        timerinfo->current_speed = speed;
        timerinfo->reason = PTS_INVALID;
        LS_TimerStart(timerinfo->pts_timer, 2);
      }
      else if (speed == 0)
      {
        LS_DEBUG("Paused Mode,all pending PTS will be set PAUSED\n");

        if (timerinfo->current_speed != 0)
        {
          LS_TimerStop(timerinfo->pts_timer, &left_ms);
          LS_DEBUG("Timer <%p> was stopped,pts_timer\n", timerinfo->pts_timer);
          timerinfo->time_left.milliseconds = left_ms.milliseconds * timerinfo->current_speed / 100;
        }

        timerinfo->reason = PTS_PRESENTED;
        DEBUG_CHECK(timerinfo->time_left.milliseconds <= timerinfo->timeout_ms);
        timerinfo->current_speed = speed;
      }
      else
      {
        LS_DEBUG("FastForward Mode,all pending PTS timer will be changed\n");

        if (timerinfo->current_speed != 0)
        {
          LS_TimerStop(timerinfo->pts_timer, &left_ms);
          LS_DEBUG("Timer <%p> was stopped,pts_timer\n", timerinfo->pts_timer);
          timerinfo->time_left.milliseconds = left_ms.milliseconds * timerinfo->current_speed / 100;
          timerinfo->current_speed = speed;
          timerinfo->reason = PTS_PRESENTED;
          LS_DEBUG("PTS will be presented in %d ms now\n", timerinfo->time_left.milliseconds * 100 / speed);
          LS_TimerStart(timerinfo->pts_timer, timerinfo->time_left.milliseconds * 100 / speed);
        }
        else
        {
          timerinfo->current_speed = speed;
          timerinfo->reason = PTS_PRESENTED;
          LS_DEBUG("PTS will be presented in %d ms now\n", timerinfo->time_left.milliseconds * 100 / speed);
          LS_TimerStart(timerinfo->pts_timer, timerinfo->time_left.milliseconds * 100 / speed);
        }
      }
    }
  }

  return LS_OK;
}


int32_t
LS_PTSMgrFinalize(void)
{
  uint32_t ref_count = 0;

  if (__ptsMgr == NULL)
  {
    LS_ERROR("__ptsMgr is nil\n");
    return LS_OK;
  }

  LS_MutexWait(__ptsMgr->mutex);
  __ptsMgr->ref_count -= 1;
  ref_count = __ptsMgr->ref_count;
  LS_MutexSignal((__ptsMgr->mutex));

  if (ref_count <= 0)
  {
    ptsMgrDelete(__ptsMgr);
    __ptsMgr = NULL;
  }

  return LS_OK;
}
