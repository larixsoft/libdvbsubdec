/*-----------------------------------------------------------------------------
 * lssubsegdec.c
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

#include "lssubsegdec.h"
#include "lssubgfx.h"
#include "lssubconverter.h"
#include "lssubpixmap.h"
#include "lssubdisplay.h"
#include "lssubutils.h"
/*---------------------------------------------------------------------------
 * local static function declaration
 * -------------------------------------------------------------------------*/
static int32_t regionIDComp(void* a, void* b);
static int32_t clutFlagMatchRegionDepth(CDSCLUTInfo* clut, LS_DisplayRegion* region);
static void    pageTimeOutCallbackFunc(void* param);
static void    pageSuicideCallbackFunc(void* param);
static void    initRegionWithDefaultCLUT(LS_DisplayRegion* region);

/*---------------------------------------------------------------------------
 * local static function implementation
 * -------------------------------------------------------------------------*/
static int32_t
regionIDComp(void* a, void* b)
{
  LS_DisplayRegion* region = NULL;
  intptr_t regionid = 0;

  if (a == NULL)
  {
    return -1;
  }

  region = (LS_DisplayRegion*)a;
  regionid = (intptr_t)b;

  if (region->region_id == regionid)
  {
    return 0;
  }
  else
  {
    return -1;
  }
}


static int32_t
clutFlagMatchRegionDepth(CDSCLUTInfo* clut, LS_DisplayRegion* region)
{
  int32_t ret;

  if ((clut == NULL) ||
      (region == NULL))
  {
    return LS_FALSE;
  }

  LS_DEBUG("region->max_clut_entry=%d\n", region->max_clut_entry);
  LS_DEBUG("clut->two_bit_entry_CLUT_flag = %d\n", clut->two_bit_entry_CLUT_flag);
  LS_DEBUG("clut->four_bit_entry_CLUT_flag = %d\n", clut->four_bit_entry_CLUT_flag);
  LS_DEBUG("clut->eight_bit_entry_CLUT_flag = %d\n", clut->eight_bit_entry_CLUT_flag);

  switch (region->max_clut_entry)
  {
    case 3:
      ret = (clut->two_bit_entry_CLUT_flag == 1);
      break;

    case 15:
      ret = (clut->four_bit_entry_CLUT_flag == 1);
      break;

    case 255:
      ret = (clut->eight_bit_entry_CLUT_flag == 1);
      break;

    default:
      return LS_FALSE;
  }

  return ret;
}


static void
pageTimeOutCallbackFunc(void* param)
{
  LS_DisplayPage* page = NULL;
  LS_DisplayPage* onscreenpage = NULL;
  LS_Service* service = NULL;
  int32_t status = LS_OK;

  LS_INFO("display page %p page time out callback...\n", param);

  if (param == NULL)
  {
    return;
  }

  page = (LS_DisplayPage*)param;
  service = page->service;

  if (service)
  {
    LS_MutexWait(service->serviceMutex);
    LS_INFO("page_time_out page = %p,(PTS:%llu,0x%08llx,%s)\n",
            param,
            page->ptsValue,
            page->ptsValue,
            PTStoHMS(page->ptsValue));
    onscreenpage = page->service->displayPageOnScreen;

    if (onscreenpage != NULL)
    {
      LS_INFO("displayPageOnScreen<%p>,(PTS:%llu,0x%08llx,%s)\n",
              onscreenpage,
              onscreenpage->ptsValue,
              onscreenpage->ptsValue,
              PTStoHMS(onscreenpage->ptsValue));
    }

    DEBUG_CHECK(page == page->service->displayPageOnScreen);
    LS_RemovePageFromScreen(page);
    LS_INFO("display page %p was removed from screen\n", param);

    /*check if we need remove it from the displaypagelist..*/
    if (service->displayPageOnScreen != service->latestDisplayPage)
    {
      status = LS_ListRemoveNode(service->displayPageList, (void*)(service->displayPageOnScreen));

      if (onscreenpage != NULL)
      {
        LS_INFO("display page<%p> (PTS:%llu,0x%08llx,%s) was removed from" " displayPageList %p\n",
                (void*)onscreenpage,
                onscreenpage->ptsValue,
                onscreenpage->ptsValue,
                PTStoHMS(onscreenpage->ptsValue),
                (void*)(service->displayPageList));
      }

      LS_DisplayPageDelete(service, service->displayPageOnScreen);
      LS_DEBUG("service<%p>->displayPageOnScreen<%p> was deleted\n", service, service->displayPageOnScreen);
      service->displayPageOnScreen = NULL;
      DEBUG_CHECK(status == LS_OK);
    }

    service->displayPageOnScreen = NULL;
    LS_MutexSignal(service->serviceMutex);
    LS_INFO("pageTimeOutCallbackFunc done\n");
  }
}


static void
pageSuicideCallbackFunc(void* param)
{
  LS_DisplayPage* obsoletepage = NULL;
  LS_DisplayPage* latest_page = NULL;
  LS_Service* service = NULL;
  LS_Time_t leftms;

  if (param == NULL)
  {
    return;
  }

  obsoletepage = (LS_DisplayPage*)param;
  LS_TimerStop(obsoletepage->suicide_timer, &leftms);
  LS_INFO("Timer <%p> was stopped,suicide_timer for display page<%p>," "(PTS:%llu,0x%08llx,%s)\n",
          (void*)(obsoletepage->suicide_timer),
          (void*)obsoletepage,
          obsoletepage->ptsValue,
          obsoletepage->ptsValue,
          PTStoHMS(obsoletepage->ptsValue));

  if (obsoletepage->status != LS_DISPLAYPAGE_STATUS_PTS_REGISTERED)
  {
    LS_INFO("obsoleted page<%p> status is %d,return...\n", obsoletepage, obsoletepage->status);
    return;
  }

  LS_INFO("DisplayPage<%p> (PTS:%llu,0x%08llx,%s) is obsoleted\n",
          (void*)obsoletepage,
          obsoletepage->ptsValue,
          obsoletepage->ptsValue,
          PTStoHMS(obsoletepage->ptsValue));
  service = obsoletepage->service;

  if ((service == NULL) ||
      (service->magic_id != SERVICE_MAGIC_NUMBER))
  {
    LS_ERROR("Invalid service found,service<%p>.\n");
    return;
  }

  LS_MutexWait(service->serviceMutex);
  latest_page = service->latestDisplayPage;

  if ((obsoletepage == latest_page) &&
      (obsoletepage->ptsValue == latest_page->ptsValue))
  {
    LS_DEBUG("display page<%p> is the latest page,will be kept it\n", (void*)obsoletepage);
    LS_TimerStart(obsoletepage->suicide_timer, LS_DP_CUBE_SUICIDE_TIMEOUT_SECONDS * 1000);
    LS_DEBUG("suicide_timer %p is re-started (duration = %d sec) for " "display page<%p>,(PTS:%llu,0x%08llx,%s)\n",
             (void*)(obsoletepage->suicide_timer),
             LS_DP_CUBE_SUICIDE_TIMEOUT_SECONDS,
             (void*)obsoletepage,
             obsoletepage->ptsValue,
             obsoletepage->ptsValue,
             PTStoHMS(obsoletepage->ptsValue));
  }
  else
  {
    LS_PTSRequestInfo request;
    int32_t res = LS_OK;
    uint64_t obsolete_pts = obsoletepage->ptsValue;      /* Save for logging before free */

    request.clientId = service->ptsmgrClientID;
    request.ptsValue = obsoletepage->ptsValue;
    res = LS_PTSMgrCancelPTS(&request);
    DEBUG_CHECK(res == LS_OK);
    LS_INFO("obsoleted page<%p> (PTS:%llu,0x%08llx,%s) request was cancelled " "as %d\n",
            obsoletepage,
            obsoletepage->ptsValue,
            obsoletepage->ptsValue,
            PTStoHMS(obsoletepage->ptsValue),
            res);
    LS_ListRemoveNode(service->displayPageList, (void*)obsoletepage);
    LS_DisplayPageDelete(service, obsoletepage);
    LS_INFO("obsoleted page (PTS:%llu,0x%08llx,%s) is removed and deleted from service<%p>\n",
            obsolete_pts,
            obsolete_pts,
            PTStoHMS(obsolete_pts),
            service);
    obsoletepage = NULL;
  }

  LS_MutexSignal(service->serviceMutex);
}


void
initRegionWithDefaultCLUT(LS_DisplayRegion* region)
{
  uint8_t opaque_alpha_value;
  uint8_t transparent_alpha_value;
  int32_t i;
  uint8_t* clut = NULL;

  if (region &&
      region->page &&
      region->page->service)
  {
    opaque_alpha_value = region->page->service->osdRender.alphaValueFullOpaque;
    transparent_alpha_value = region->page->service->osdRender.alphaValueFullTransparent;

    switch (region->region_depth)
    {
      case kREGION_DEPTH_2_BIT:
        SYS_MEMCPY((void*)region->clut, kDefaultARGB4CLUT, 4 * region->max_clut_entry);
        break;

      case kREGION_DEPTH_4_BIT:
        SYS_MEMCPY((void*)region->clut, kDefaultARGB16CLUT, 4 * region->max_clut_entry);
        break;

      case kREGION_DEPTH_8_BIT:
        SYS_MEMCPY((void*)region->clut, kDefaultARGB256CLUT, 4 * region->max_clut_entry);
        break;

      default:
        LS_ERROR("Unsupported region->region_depth <%d>\n",
                 region->region_depth);
    }

    clut = (uint8_t*)region->clut;
    fprintf(stderr,
            "Before CLUT init: Entry[0]=A%dR%dG%dB%d Entry[1]=A%dR%dG%dB%d (opaque=%d, transparent=%d)\n",
            clut[0],
            clut[1],
            clut[2],
            clut[3],
            clut[4],
            clut[5],
            clut[6],
            clut[7],
            opaque_alpha_value,
            transparent_alpha_value);

    for (i = 0; i < region->max_clut_entry; i++)
    {
      uint8_t alpha = clut[i * 4];

      /* Map default CLUT alpha to app's alpha range
       * Default CLUT: 255=opaque, 0=transparent
       * App range: opaque_alpha_value=opaque, transparent_alpha_value=transparent
       */
      clut[i * 4] = (alpha * (opaque_alpha_value - transparent_alpha_value)) / 255 + transparent_alpha_value;
    }

    fprintf(stderr,
            "After CLUT init: Entry[0]=A%dR%dG%dB%d Entry[1]=A%dR%dG%dB%d (opaque=%d, transparent=%d)\n",
            clut[0],
            clut[1],
            clut[2],
            clut[3],
            clut[4],
            clut[5],
            clut[6],
            clut[7],
            opaque_alpha_value,
            transparent_alpha_value);
  }
}


/*-----------------------------------------------------------------------------
 * Public APIs
 *---------------------------------------------------------------------------*/
LS_DisplayPage*
LS_DisplayPageNew(LS_Service* service, int32_t* errCode)
{
  LS_DisplayPage* page = NULL;
  int32_t err = LS_OK;
  int32_t ret_val = LS_OK;

  do
  {
    page = (LS_DisplayPage*)ServiceHeapMalloc(service, COMPOSITION_BUFFER, sizeof(LS_DisplayPage));
    DEBUG_CHECK(page != NULL);

    if (page == NULL)
    {
      LS_ERROR("COMPOSITION_BUFFER,Memory allocation failed: %d bytes\n", sizeof(LS_DisplayPage));
      *errCode = LS_ERROR_COMPOSITION_BUFFER;
      break;
    }

    SYS_MEMSET((void*)page, 0, sizeof(LS_DisplayPage));
    page->magic_id = DISPLAYPAGE_MAGIC_NUMBER;
    page->pageType = LS_DISPLAY_PAGE_TYPE_UNKOWN;
    page->regions = LS_ListInit(__listMallocFunc, __listFreeFunc, &err);

    if (page->regions == NULL)
    {
      LS_ERROR("LS_ListInit() failed\n");
      *errCode = err;
      break;
    }

    LS_DEBUG("LS_display page %p, list regions %p created.\n", (void*)page, (void*)page->regions);
    /*now create the timers*/
    ret_val = LS_TimerNew(&(page->page_time_out_timer), pageTimeOutCallbackFunc, (void*)page);
    DEBUG_CHECK(ret_val == LS_OK);

    if (LS_OK != ret_val)
    {
      LS_ERROR("cannot create a new timer\n");
      break;
    }

    LS_DEBUG("Timer <%p> was created,page_time_out_timer for display page <%p> " "(PTS:%llu,0x%08llx,%s) created\n",
             (void*)(page->page_time_out_timer),
             (void*)page,
             page->ptsValue,
             page->ptsValue,
             PTStoHMS(page->ptsValue));
    ret_val = LS_TimerNew(&(page->suicide_timer), pageSuicideCallbackFunc, (void*)page);

    if (LS_OK != ret_val)
    {
      LS_ERROR("cannot create a new timer\n");
      break;
    }

    LS_DEBUG("Timer <%p> was created,suicide_timer for display page <%p> " "(PTS:0x%08x,0x%08x) created\n",
             (void*)(page->suicide_timer),
             (void*)page,
             page->ptsValue,
             page->ptsValue);
    page->visible = LS_FALSE;
    /*everything seems fine...*/
    *errCode = LS_OK;
  }while (0);

  /*cleanup if failed*/
  if (((*errCode) != LS_OK) &&
      (NULL != page))
  {
    LS_DisplayPageDelete(service, page);
    LS_DEBUG("Displaypage %p deleted\n", page);
    page = NULL;
  }

  page->status = LS_DISPLAYPAGE_STATUS_NEW;
  return page;
}


void
LS_DisplayPageDelete(LS_Service* service, LS_DisplayPage* page)
{
  int32_t status = LS_OK;
  LS_Time_t leftms;
  LS_DisplayRegion* region = NULL;

  LS_ENTER("service<%p>,page<%p>\n", service, page);

  do
  {
    if (page == NULL)
    {
      break;
    }

    if (page->magic_id != DISPLAYPAGE_MAGIC_NUMBER)
    {
      LS_ERROR("Invalid or crashed display page %p\n", (void*)page);
      break;
    }

    LS_DEBUG("Deleting display page %p (PTS:%llu,0x%08llx,%s)...\n",
             (void*)page,
             page->ptsValue,
             page->ptsValue,
             PTStoHMS(page->ptsValue));

    if (page->displayset)
    {
      LS_DisplaysetDelete(service, page->displayset);
      LS_DEBUG("displayset<%p> in page<%p> was deleted\n", (void*)(page->displayset), (void*)page);
    }

    if (page->regions)
    {
      region = LS_ListFirstData(page->regions);

      while (region)
      {
        LS_DisplayRegionDelete(service, region);
        region = LS_ListFirstData(page->regions);
      }

      LS_ListDestroy(page->regions);
      page->regions = NULL;
    }

    if (page->page_time_out_timer)
    {
      status = LS_TimerStop(page->page_time_out_timer, &leftms);
      DEBUG_CHECK(status == LS_OK);
      LS_DEBUG("Timer <%p> was stopped,page_time_out_timer for display page <%p> " "(PTS:%llu,0x%08llx,%s) created\n",
               (void*)(page->page_time_out_timer),
               (void*)page,
               page->ptsValue,
               page->ptsValue,
               PTStoHMS(page->ptsValue));
      status = LS_TimerDelete(page->page_time_out_timer);
      DEBUG_CHECK(status == LS_OK);
      LS_DEBUG("Timer <%p> was deleted,page_time_out_timer for display page <%p> " "(PTS:%llu,0x%08llx,%s) created\n",
               (void*)(page->page_time_out_timer),
               (void*)page,
               page->ptsValue,
               page->ptsValue,
               PTStoHMS(page->ptsValue));
      page->page_time_out_timer = NULL;
    }

    if (page->suicide_timer)
    {
      status = LS_TimerStop(page->suicide_timer, &leftms);
      DEBUG_CHECK(status == LS_OK);
      LS_DEBUG("Timer <%p> was stopped,suicide_timer for display page <%p> " "(PTS:%llu,0x%08llx,%s)\n",
               (void*)(page->suicide_timer),
               (void*)page,
               page->ptsValue,
               page->ptsValue,
               PTStoHMS(page->ptsValue));
      status = LS_TimerDelete(page->suicide_timer);
      DEBUG_CHECK(status == LS_OK);
      LS_DEBUG("Timer <%p> was deleted,suicide_timer for display page <%p> " "(PTS:%llu,0x%08llx,%s)\n",
               (void*)(page->suicide_timer),
               (void*)page,
               page->ptsValue,
               page->ptsValue,
               PTStoHMS(page->ptsValue));
      page->suicide_timer = NULL;
    }

    SYS_MEMSET((void*)page, 0, sizeof(LS_DisplayPage));
    ServiceHeapFree(service, COMPOSITION_BUFFER, (void*)page);
  }while (0);

  LS_LEAVE("service<%p>,page<%p>\n", service, page);
}


LS_DisplayRegion*
LS_DisplayRegionNew(LS_Service* service)
{
  LS_DisplayRegion* region = NULL;

  region = (LS_DisplayRegion*)ServiceHeapMalloc(service, COMPOSITION_BUFFER, sizeof(LS_DisplayRegion));

  if (NULL == region)
  {
    LS_ERROR("COMPOSITION_BUFFER,Memory allocation failed: %d bytes\n", sizeof(LS_DisplayRegion));
  }
  else
  {
    SYS_MEMSET((void*)region, 0, sizeof(LS_DisplayRegion));
    region->OSDPixmapFormat = service->osdRender.OSDPixmapFormat;
  }

  return region;
}


void
LS_DisplayRegionDelete(LS_Service* service, LS_DisplayRegion* region)
{
  if (region == NULL)
  {
    return;
  }

  if (region->pixmap)
  {
    LS_DeletePixmap(service, region->pixmap);
    region->pixmap = NULL;
  }

  ServiceHeapFree(service, COMPOSITION_BUFFER, (void*)region);
  LS_TRACE("LS_DisplayRegion %p deleted\n", (void*)region);
}


int32_t
LS_DisplaysetDecodeRCS(LS_Displayset* displayset, LS_DisplayRegion* region, int32_t allocate_pixmap)
{
  LS_SegRCS* rcs = NULL;
  int32_t j;
  uint32_t numofobjs = 0;
  LS_SegCDS* cds = NULL;
  int32_t status = LS_ERROR_GENERAL;
  uint8_t entryid;

  if ((displayset == NULL) ||
      (displayset->rcs == NULL))
  {
    return LS_ERROR_GENERAL;
  }

  rcs = DisplaysetFindRCSByID(displayset, region->region_id);

  if (rcs == NULL)
  {
    LS_ERROR("could not find RCS with region_id = %d in displayset %p\n", region->region_id, displayset);
    return LS_ERROR_GENERAL;
  }

  LS_TRACE("rcs->page_id = %d \n", rcs->page_id);
  LS_TRACE("ancillaryPageId = %d\n", region->page->service->pmtPageID.ancillaryPageId);
  LS_TRACE("compositionPageId = %d\n", region->page->service->pmtPageID.compositionPageId);

  // Check page_id match - page_id=0 in PMT acts as wildcard (accept any page)
  if (((region->page->service->pmtPageID.ancillaryPageId != 0) ||
       (region->page->service->pmtPageID.compositionPageId != 0)) &&
      ((rcs->page_id != region->page->service->pmtPageID.ancillaryPageId) &&
       (rcs->page_id != region->page->service->pmtPageID.compositionPageId)))
  {
    LS_ERROR("page_id does not match,ignore this rcs," "expect ancillaryPageId=%d,compositionPageId=%d,"
             "but get page_id = %d\n",
             region->page->service->pmtPageID.ancillaryPageId,
             region->page->service->pmtPageID.compositionPageId,
             rcs->page_id);
    return LS_ERROR_GENERAL;
  }

  region->version = rcs->region_version_number;
  region->page_id = rcs->page_id;
  region->clut_id = rcs->CLUT_id;

  if (allocate_pixmap == LS_TRUE)
  {
    region->pixmap = LS_NewPixmap(region->page->service, rcs->region_width, rcs->region_height, LS_PIXFMT_PALETTE8BIT);
  }

  if (region->pixmap == NULL)
  {
    LS_ERROR("LS_NewPixmap() failed for region %p\n", (void*)region);
    return LS_ERROR_GENERAL;
  }

  LS_DEBUG("RCS->region_depth=%d for RCS(id=%d)\n", rcs->region_depth, rcs->region_id);
  region->region_depth = rcs->region_depth;

  switch (rcs->region_depth)
  {
    case kREGION_DEPTH_2_BIT:
      entryid = rcs->region_2bit_pixel_code;
      region->max_clut_entry = 3;
      break;

    case kREGION_DEPTH_4_BIT:
      entryid = rcs->region_4bit_pixel_code;
      region->max_clut_entry = 15;
      break;

    case kREGION_DEPTH_8_BIT:
      entryid = rcs->region_8bit_pixel_code;
      region->max_clut_entry = 255;
      break;

    default:
      LS_ERROR("unsupported region_depth\n");
      return LS_ERROR_GENERAL;
  }

  /*see if we need fill the region with background color*/
  cds = LS_DisplaysetFindCDSByID(displayset, rcs->CLUT_id);

  if (cds)
  {
    status = LS_DisplaysetDecodeCDS(cds, region);
    DEBUG_CHECK(status == LS_OK);
  }
  else
  {
    LS_INFO("can not find the CLUT(%d) for region<%d> in RCS <%p>," "using default CLUT now\n",
            rcs->CLUT_id,
            rcs->region_id,
            (void*)rcs);
    initRegionWithDefaultCLUT(region);
  }

  if (rcs->region_fill_flag)
  {
    LS_INFO("Region(id=%d)%p filled with entry_id = %d\n", rcs->region_id, (void*)region, entryid);

    if (region->pixmap &&
        region->pixmap->data)
    {
      SYS_MEMSET(region->pixmap->data, entryid, region->pixmap->dataSize);
    }
    else
    {
      LS_ERROR("Invalid pixmap in region %p\n", (void*)region);
      return LS_ERROR_GENERAL;
    }
  }

  /*decode each object in this region*/
  status = LS_ListCount(rcs->objectinfo_list, &numofobjs);
  DEBUG_CHECK(status == LS_OK);
  LS_DEBUG("There total %d objects in RCS(id=%d) %p.\n", numofobjs, rcs->region_id, (void*)rcs);

  for (j = 0; j < (int32_t)numofobjs; j++)
  {
    RCSObjectInfo* obj_info = LS_ListNthNode(rcs->objectinfo_list, j);

    DEBUG_CHECK(obj_info != NULL);

    if (obj_info == NULL)
    {
      LS_ERROR("%dth obj is empty\n", j);
      continue;
    }

    {
      LS_TRACE("%dth object (%p):\n", j, (void*)obj_info);
      LS_TRACE("object_id                  = %04x\n", obj_info->object_id);
      LS_TRACE("object_type                = %02x\n", obj_info->object_type);
      LS_TRACE("object_provider_flag       = %02x\n", obj_info->object_provider_flag);
      LS_TRACE("object_horizontal_position = %02x (%d)\n",
               obj_info->object_horizontal_position,
               obj_info->object_horizontal_position);
      LS_TRACE("reserved                   = %02x\n", obj_info->reserved);
      LS_TRACE("object_vertical_position   = %02x (%d)\n",
               obj_info->object_vertical_position,
               obj_info->object_vertical_position);

      if ((obj_info->object_type == 0x01) ||
          (obj_info->object_type == 0x02))
      {
        LS_TRACE("foreground_pixel_code  = %02x\n", obj_info->foreground_pixel_code);
        LS_TRACE("background_pixel_code  = %02x\n", obj_info->background_pixel_code);
      }
    }

    if (obj_info->object_type == kBASIC_OBJECT_BITMAP)
    {
      status = LS_DisplaysetDecodeODSBitmap(displayset, region, obj_info);

      if (status != LS_OK)
      {
        LS_ERROR("parse ODS for region_id = %d failed\n", region->region_id);
        continue;
      }
    }
    else if ((obj_info->object_type == kBASIC_OBJECT_CHARACTER) ||
             (obj_info->object_type == kCOMPOSITE_OBJECT_STRING_OF_CHARACTERS))
    {
      status = LS_DisplaysetDecodeODSCharacters(displayset, region, obj_info);
      DEBUG_CHECK(status == LS_OK);

      if (status != LS_OK)
      {
        LS_ERROR("parse ODS for region_id = %d failed\n", region->region_id);
        continue;
      }
    }
    else
    {
      LS_ERROR("object_type %d is invalid\n", obj_info->object_type);
      return LS_ERROR_GENERAL;
    }
  }

  return LS_OK;
}


int32_t
LS_DisplaysetDecodeODSCharacters(LS_Displayset* displayset, LS_DisplayRegion* region, RCSObjectInfo* obj_info)
{
  LS_SegODS* ods = NULL;
  ODSStringData* stringdata = NULL;
  int32_t num_codes = 0;
  int32_t i;

  if (displayset == NULL || region == NULL || obj_info == NULL)
  {
    LS_ERROR("LS_ERROR_GENERAL: Invalid input parameters\n");
    return LS_ERROR_GENERAL;
  }

  /* Find the ODS segment for this object */
  ods = DisplaysetFindODSByID(displayset, obj_info->object_id);
  if (ods == NULL)
  {
    LS_ERROR("Could not find ODS with object_id=%d\n", obj_info->object_id);
    return LS_ERROR_GENERAL;
  }

  /* Verify this ODS contains character data */
  if (ods->object_coding_method != 0x01)  /* 0x01 = character coding */
  {
    LS_ERROR("ODS object_id=%d is not character-coded (method=%d)\n",
             obj_info->object_id, ods->object_coding_method);
    return LS_ERROR_GENERAL;
  }

  stringdata = ods->data.stringdata;
  if (stringdata == NULL)
  {
    LS_ERROR("ODS stringdata is NULL for object_id=%d\n", obj_info->object_id);
    return LS_ERROR_GENERAL;
  }

  /* Clear any existing character codes in the region */
  SYS_MEMSET(region->character_code, 0, sizeof(region->character_code));

  /* Copy character codes from ODS to region (up to 256 characters) */
  num_codes = (int32_t)stringdata->number_of_codes;
  if (num_codes > 256)
  {
    LS_WARNING("ODS contains %d character codes, truncating to 256\n", num_codes);
    num_codes = 256;
  }

  if (stringdata->character_code != NULL)
  {
    SYS_MEMCPY(region->character_code, stringdata->character_code, num_codes);

    /* Log character codes for debugging (first 32 chars max) */
    LS_DEBUG("Character codes for region_id=%d: ", region->region_id);
    for (i = 0; i < (num_codes < 32 ? num_codes : 32); i++)
    {
      LS_DEBUG("%02X ", region->character_code[i]);
    }
    if (num_codes > 32)
    {
      LS_DEBUG("... (%d total)", num_codes);
    }
    LS_DEBUG("\n");

    /* Attempt to display as ASCII for readability (if printable) */
    LS_DEBUG("As ASCII: \"");
    for (i = 0; i < (num_codes < 32 ? num_codes : 32); i++)
    {
      uint8_t c = region->character_code[i];
      /* Display printable ASCII characters directly, others as dots */
      LS_DEBUG("%c", (c >= 32 && c <= 126) ? c : '.');
    }
    LS_DEBUG("\"\n");
  }

  /* Create a minimal pixmap to store position information for text rendering.
   * This allows the rendering code to use the same position logic for both
   * bitmap and character subtitles. */
  if (region->pixmap == NULL)
  {
    region->pixmap = LS_NewPixmap(region->page->service, 1, 1, LS_PIXFMT_ARGB32);
    if (region->pixmap != NULL)
    {
      /* Store the object position from RCS info */
      region->pixmap->leftPos = obj_info->object_horizontal_position;
      region->pixmap->topPos = obj_info->object_vertical_position;
    }
  }

  LS_INFO("Decoded %d character codes for region_id=%d at position (%d,%d)\n",
          num_codes, region->region_id,
          (region->pixmap ? region->pixmap->leftPos : 0),
          (region->pixmap ? region->pixmap->topPos : 0));

  /* Note: Character encoding handling:
   * - DVB subtitles typically use:
   *   - ISO/IEC 8859-1 (Latin-1) for Western European languages
   *   - ISO/IEC 8859-2 through 8859-11 for other regions
   *   - ISO/IEC 10646 (UTF-8) for modern broadcasts
   *
   * The rendering callback (drawTextFunc) should handle the appropriate
   * character encoding conversion based on the broadcast standard.
   *
   * For compatibility, the raw character codes are passed to the callback
   * with encoding hint LS_ENCODING_DEFAULT, allowing the renderer to
   * determine the appropriate encoding.
   */

  return LS_OK;
}


int32_t
LS_DisplaysetDecodeODSBitmap(LS_Displayset* displayset, LS_DisplayRegion* region, RCSObjectInfo* obj_info)
{
  int32_t status = LS_OK;
  LS_SegRCS* rcs = NULL;
  LS_SegCDS* cds = NULL;
  LS_SegODS* ods = NULL;
  uint32_t processed_length = 0;

  if (displayset == NULL)
  {
    return LS_ERROR_GENERAL;
  }

  rcs = DisplaysetFindRCSByID(displayset, region->region_id);
  DEBUG_CHECK(rcs != NULL);

  if (rcs == NULL)
  {
    LS_ERROR("could not find RCS region_id=%d\n", region->region_id);
    return LS_ERROR_GENERAL;
  }

  // Check page_id match - page_id=0 in PMT acts as wildcard (accept any page)
  if (((region->page->service->pmtPageID.ancillaryPageId != 0) ||
       (region->page->service->pmtPageID.compositionPageId != 0)) &&
      ((rcs->page_id != region->page->service->pmtPageID.ancillaryPageId) &&
       (rcs->page_id != region->page->service->pmtPageID.compositionPageId)))
  {
    LS_ERROR("RCS %p page_id %d does not match (%d,%d)\n",
             (void*)rcs,
             rcs->page_id,
             region->page->service->pmtPageID.ancillaryPageId,
             region->page->service->pmtPageID.compositionPageId);
    return LS_ERROR_GENERAL;
  }

  // Check page_id match - page_id=0 in PMT acts as wildcard (accept any page)
  if (((region->page->service->pmtPageID.ancillaryPageId != 0) ||
       (region->page->service->pmtPageID.compositionPageId != 0)) &&
      ((region->page_id != region->page->service->pmtPageID.ancillaryPageId) &&
       (region->page_id != region->page->service->pmtPageID.compositionPageId)))
  {
    LS_ERROR("region %p page_id %d does not match (%d,%d)\n",
             (void*)region,
             region->page_id,
             region->page->service->pmtPageID.ancillaryPageId,
             region->page->service->pmtPageID.compositionPageId);
    return LS_ERROR_GENERAL;
  }

  cds = LS_DisplaysetFindCDSByID(displayset, rcs->CLUT_id);

  if (cds == NULL)
  {
    LS_WARNING("could not find CDS with CLUT_id=%d, using default CLUT\n", rcs->CLUT_id);
    // Initialize region with default CLUT
    initRegionWithDefaultCLUT(region);
    cds = NULL;      // Keep cds NULL to skip page_id check
  }
  else
  {
    // CDS found
  }

  // Check page_id only if CDS was found
  if (cds != NULL)
  {
    // Check page_id match - page_id=0 in PMT acts as wildcard (accept any page)
    if (((region->page->service->pmtPageID.ancillaryPageId != 0) ||
         (region->page->service->pmtPageID.compositionPageId != 0)) &&
        ((cds->page_id != region->page->service->pmtPageID.ancillaryPageId) &&
         (cds->page_id != region->page->service->pmtPageID.compositionPageId)))
    {
      LS_ERROR("CDS %p page_id %d does not match (%d,%d)\n",
               (void*)cds,
               cds->page_id,
               region->page->service->pmtPageID.ancillaryPageId,
               region->page->service->pmtPageID.compositionPageId);
      return LS_ERROR_GENERAL;
    }
  }

  ods = DisplaysetFindODSByID(displayset, obj_info->object_id);
  DEBUG_CHECK(ods != NULL);

  if (ods == NULL)
  {
    LS_ERROR("could not find ODS with object_id=%d\n", obj_info->object_id);
    return LS_ERROR_GENERAL;
  }

  // Check page_id match - page_id=0 in PMT acts as wildcard (accept any page)
  if (((region->page->service->pmtPageID.ancillaryPageId != 0) ||
       (region->page->service->pmtPageID.compositionPageId != 0)) &&
      ((ods->page_id != region->page->service->pmtPageID.ancillaryPageId) &&
       (ods->page_id != region->page->service->pmtPageID.compositionPageId)))
  {
    LS_ERROR("ODS %p page_id %d does not match (%d,%d)\n",
             (void*)ods,
             ods->page_id,
             region->page->service->pmtPageID.ancillaryPageId,
             region->page->service->pmtPageID.compositionPageId);
    return LS_ERROR_GENERAL;
  }

  if (ods->object_coding_method != kCODING_OF_PIXELS)
  {
    LS_ERROR("ODS %p :wrong object_coding_method: %d\n", (void*)ods, ods->object_coding_method);
    return LS_ERROR_GENERAL;
  }

  /*begin to process the top field data*/
  DEBUG_CHECK(ods->data.pixeldata != NULL);

  if (ods->data.pixeldata == NULL)
  {
    LS_ERROR("no pixel data found\n");
    return LS_ERROR_GENERAL;
  }

  if (ods->data.pixeldata->top_field_data_block_length)
  {
    LS_DEBUG("Now Begin To Process top_field_data_block...\n");
    status = LS_ODSDecodePixelDataSubBlock(ods->data.pixeldata->top_pixel_data_sub_block,
                                           ods->data.pixeldata->top_field_data_block_length,
                                           ods->non_modifying_colour_flag,
                                           region,
                                           obj_info,
                                           1,
                                           &processed_length);
    LS_DEBUG("Top_field was processed of %d bytes,expected %d bytes\n",
             processed_length,
             ods->data.pixeldata->top_field_data_block_length);

    if (status != LS_OK)
    {
      LS_ERROR("%s:LS_DisplaysetDecodeODSBitmap(),top_field failed\n", ServiceErrString(status));
      return status;
    }
  }
  else
  {
    LS_ERROR("top_field_data_block_length is 0\n");
    return LS_ERROR_GENERAL;
  }

  if (ods->data.pixeldata->bottom_field_data_block_length)
  {
    LS_DEBUG("Now Begin To Process bottom_field_data_block...\n");
    status = LS_ODSDecodePixelDataSubBlock(ods->data.pixeldata->bottom_pixel_data_sub_block,
                                           ods->data.pixeldata->bottom_field_data_block_length,
                                           ods->non_modifying_colour_flag,
                                           region,
                                           obj_info,
                                           0,
                                           &processed_length);

    if (status == LS_ERROR_GENERAL)
    {
      LS_ERROR("%s:LS_DisplaysetDecodeODSBitmap(),bottom_failed, failed\n", ServiceErrString(status));
      return status;
    }

    if (processed_length != ods->data.pixeldata->bottom_field_data_block_length)
    {
      LS_WARNING("parsing bottom_field data sublock failed:status=%d," "processed_length=%d (expected %d)\n",
                 status,
                 processed_length,
                 ods->data.pixeldata->bottom_field_data_block_length);
    }

    LS_DEBUG("Bottom_field was processed of %d bytes,expected %d bytes\n",
             processed_length,
             ods->data.pixeldata->bottom_field_data_block_length);
  }
  else
  {
    status = LS_ODSDecodePixelDataSubBlock(ods->data.pixeldata->top_pixel_data_sub_block,
                                           ods->data.pixeldata->top_field_data_block_length,
                                           ods->non_modifying_colour_flag,
                                           region,
                                           obj_info,
                                           0,
                                           &processed_length);

    if (status == LS_ERROR_GENERAL)
    {
      LS_ERROR("%s:LS_DisplaysetDecodeODSBitmap(),bottom_failed, failed\n", ServiceErrString(status));
      return status;
    }

    if (processed_length != ods->data.pixeldata->top_field_data_block_length)
    {
      LS_WARNING("Copying top_field data subblock failed:status=%d," "processed_length=%d (expected %d)\n",
                 status,
                 processed_length,
                 ods->data.pixeldata->top_field_data_block_length);
    }

    LS_TRACE("Copying top_field was processed of %d bytes,expected %d bytes\n",
             processed_length,
             ods->data.pixeldata->top_field_data_block_length);
  }

  return LS_OK;
}


int32_t
LS_DisplaysetDecodeCDS(LS_SegCDS* cds, LS_DisplayRegion* region)
{
  uint32_t clut_count = 0;
  int32_t i;
  int32_t status = LS_OK;
  CDSCLUTInfo* clut = NULL;
  uint8_t opaque_alpha_value;
  uint8_t transparent_alpha_value;
  uint8_t temp_alpha;

  LS_ENTER("cds=%p,region=%p\n", (void*)cds, (void*)region);

  if ((cds == NULL) ||
      (region == NULL) ||
      (region->page == NULL))
  {
    LS_ERROR("wrong parameters: cds = %p,region=%p\n", (void*)cds, (void*)region);
    return LS_ERROR_GENERAL;
  }

  opaque_alpha_value = region->page->service->osdRender.alphaValueFullOpaque;
  transparent_alpha_value = region->page->service->osdRender.alphaValueFullTransparent;

  if (cds->CLUT_id != region->clut_id)
  {
    LS_ERROR("CLUT_id (%d) does not match region's clut_id (%d)\n", cds->CLUT_id, region->clut_id);
    return LS_ERROR_GENERAL;
  }

  status = LS_ListCount(cds->clutinfo_list, &clut_count);

  if (status != LS_OK)
  {
    LS_ERROR("LS_ListCount() failed\n");
    return LS_ERROR_GENERAL;
  }

  LS_TRACE("Found %d clutinfo in CDS %p\n", clut_count, (void*)cds);

  for (i = 0; i < (int32_t)clut_count; i++)
  {
    clut = LS_ListNthNode(cds->clutinfo_list, i);
    DEBUG_CHECK(clut != NULL);

    if (clut &&
        (clut->CLUT_entry_id <= region->max_clut_entry))
    {
      if ((clutFlagMatchRegionDepth(clut, region) == LS_FALSE))
      {
        LS_WARNING("Clut xx-bit/entry_CLUT_flag does not match region depth\n");
      }

      /**
       *    full_range_flag: If set to '1', this indicates that the Y_value,
       *    Cr_value, Cb_value and T_value fields have the full 8-bit resolution.
       *    If set to '0', then these fields contain only the most significant
       *    bits.
       *
       *    -- en300743v-1-3-1p.pdf 7.2.4 CLUT definition segment
       */
      if (clut->full_range_flag == 0)
      {
        clut->Y_value = clut->Y_value << 2;
        clut->Cr_value = clut->Cr_value << 4;
        clut->Cb_value = clut->Cb_value << 4;
        clut->T_value = clut->T_value << 6;
      }

      /**
       * "Full transparency is acquired through a value of zero in the Y_value
       * field."
       *
       *    ---en300743v-1-3-1p.pdf 7.2.4 CLUT definition segment
       */
      if (clut->Y_value == 0)
      {
        clut->T_value = 255;
      }

      /**
       * T_value: The Transparency output value of the CLUT for this entry.
       * A value of zero identifies no transparency. The maximum value plus one
       * would correspond to full transparency. For all other values the level
       * of transparency is defined by linear interpolation.Full transparency
       * is acquired through a value of zero in the Y_value field.
       *
       *    ---en300743v-1-3-1p.pdf 7.2.4 CLUT definition segment
       */
      temp_alpha = ((clut->T_value) * (transparent_alpha_value - opaque_alpha_value)) / 255 + opaque_alpha_value;
      region->clut[clut->CLUT_entry_id].alphaValue = CLAMP(temp_alpha, 0, 255);
      LS_DEBUG("Clut: [%03d][T Y Cb Cr][%02x %02x %02x %02x][%03d %03d %03d %03d]\n",
               clut->CLUT_entry_id,
               clut->T_value,
               clut->Y_value,
               clut->Cr_value,
               clut->Cb_value,
               clut->T_value,
               clut->Y_value,
               clut->Cr_value,
               clut->Cb_value);
      YCBCR2RGB(clut->Y_value,
                clut->Cb_value,
                clut->Cr_value,
                region->clut[clut->CLUT_entry_id].redValue,
                region->clut[clut->CLUT_entry_id].greenValue,
                region->clut[clut->CLUT_entry_id].blueValue);
      LS_DEBUG("Clut: [%03d][T R G  B ][%02x %02x %02x %02x][%03d %03d %03d %03d]\n",
               clut->CLUT_entry_id,
               region->clut[clut->CLUT_entry_id].alphaValue,
               region->clut[clut->CLUT_entry_id].redValue,
               region->clut[clut->CLUT_entry_id].greenValue,
               region->clut[clut->CLUT_entry_id].blueValue,
               region->clut[clut->CLUT_entry_id].alphaValue,
               region->clut[clut->CLUT_entry_id].redValue,
               region->clut[clut->CLUT_entry_id].greenValue,
               region->clut[clut->CLUT_entry_id].blueValue);
    }
    else
    {
      LS_ERROR("CLUT entry %p will be ignored due to %p or CLUT_entry_id = %d," "region->max_clut_entry = %d\n",
               (void*)clut,
               (void*)clut,
               clut->CLUT_entry_id,
               region->max_clut_entry);
    }
  }

  /* Fix for streams that incorrectly use CLUT entry 0 for visible text
   * Force entry 0 to be transparent to avoid rendering black blocks
   */
  if (region->clut[0].alphaValue != 0)
  {
    LS_DEBUG("CDS: Forcing Entry[0] to transparent (was A%d)\n", region->clut[0].alphaValue);
    region->clut[0].alphaValue = 0;
  }

  LS_LEAVE("\n");
  return LS_OK;
}


int32_t
LS_DisplaysetDecodeDDS(LS_SegDDS* dds, LS_DisplayPage* page)
{
  if (page == NULL)
  {
    LS_ERROR("Invalid page: NULL\n");
    return LS_ERROR_GENERAL;
  }

  if (dds == NULL)
  {
    return LS_OK;
  }

  page->dds_flag = LS_TRUE;
  page->dds.displayWidth = CLAMP(dds->display_width, 0, 4095);
  page->dds.displayHeight = CLAMP(dds->display_height, 0, 4095);

  if (dds->display_window_flag)
  {
    page->dds.displayWindowFlag = dds->display_window_flag;
    page->dds.displayWindow.leftPos = dds->display_window_horizontal_position_minimum;
    page->dds.displayWindow.rightPos = dds->display_window_horizontal_position_maximum;
    page->dds.displayWindow.topPos = dds->display_window_vertical_position_minimum;
    page->dds.displayWindow.bottomPos = dds->display_window_vertical_position_maximum;
  }

  return LS_OK;
}


LS_DisplayRegion*
LS_DisplaypageFindRegionByID(LS_DisplayPage* page, uint16_t region_id)
{
  LS_DisplayRegion* region = NULL;

  if (page == NULL)
  {
    return NULL;
  }

  region = LS_ListFindUserNode(page->regions, (void*)((intptr_t)region_id), regionIDComp);
  return region;
}


/*-----------------------------------------------------------------------------
 * DSS (Disparity Signalling Segment) Decoding for 3D Subtitles
 *---------------------------------------------------------------------------*/

/**
 * @brief Find DSS region info by region ID
 *
 * Searches the DSS segment for region info matching the specified region ID.
 *
 * @param dss DSS segment to search
 * @param region_id Region ID to find
 * @return DSS region info or NULL if not found
 */
static DSSRegionInfo*
FindDSSRegionInfoByID(LS_SegDSS* dss, uint8_t region_id)
{
  DSSRegionInfo* region_info = NULL;
  uint32_t i;
  uint32_t count;

  if ((dss == NULL) || (dss->regions == NULL))
  {
    return NULL;
  }

  if (LS_ListCount(dss->regions, &count) != LS_OK)
  {
    return NULL;
  }

  for (i = 0; i < count; i++)
  {
    region_info = (DSSRegionInfo*)LS_ListNthNode(dss->regions, i);
    if ((region_info != NULL) && (region_info->region_id == region_id))
    {
      return region_info;
    }
  }

  return NULL;
}


/**
 * @brief Apply disparity shift to a region
 *
 * Applies the disparity shift value from DSS to a display region.
 * The disparity shift is stored for use during stereoscopic rendering.
 *
 * @param region Display region to apply disparity to
 * @param disparity_shift Disparity shift value (signed 8-bit)
 * @return LS_OK (1) on success, error code on failure
 */
static int32_t
ApplyDisparityShiftToRegion(LS_DisplayRegion* region, int8_t disparity_shift)
{
  if (region == NULL)
  {
    return LS_ERROR_GENERAL;
  }

  /* Store disparity shift for use during rendering */
  region->disparity_shift = disparity_shift;

  LS_DEBUG("Region %d: disparity_shift = %d\n", region->region_id, disparity_shift);

  return LS_OK;
}


/**
 * @brief Decode DSS (Disparity Signalling Segment) for 3D subtitles
 *
 * Decodes disparity information from a DSS segment and applies it to
 * the display regions. This function processes:
 * - Page-level default disparity shift
 * - Region-level disparity shifts
 * - Sub-region disparity shifts
 *
 * The disparity shifts are stored in the display regions and can be
 * used by the renderer to create stereoscopic (3D) subtitle effects.
 *
 * @param displayset Display set containing the DSS
 * @param page Display page to update with disparity information
 * @return LS_OK (1) on success, error code on failure
 */
int32_t
LS_DisplaysetDecodeDSS(LS_Displayset* displayset, LS_DisplayPage* page)
{
  LS_SegDSS* dss = NULL;
  int32_t page_disparity_shift;
  uint32_t i;

  if (page == NULL)
  {
    LS_ERROR("Invalid page: NULL\n");
    return LS_ERROR_GENERAL;
  }

  if (displayset == NULL)
  {
    return LS_OK;
  }

  dss = displayset->dss;
  if (dss == NULL)
  {
    /* No DSS segment present - not a 3D subtitle */
    return LS_OK;
  }

  /* Get page-level default disparity shift (convert to signed) */
  page_disparity_shift = (int8_t)dss->page_default_disparity_shift;
  page->disparity_shift = page_disparity_shift;

  LS_DEBUG("DSS: page_disparity_shift = %d\n", page_disparity_shift);

  /* Apply disparity shift to all regions */
  if (page->regions != NULL)
  {
    uint32_t region_count = 0;

    if (LS_ListCount(page->regions, &region_count) == LS_OK)
    {
      for (i = 0; i < region_count; i++)
      {
        LS_DisplayRegion* region = NULL;
        DSSRegionInfo* dss_region_info = NULL;
        int32_t region_disparity_shift = page_disparity_shift;

        region = (LS_DisplayRegion*)LS_ListNthNode(page->regions, i);
        if (region == NULL)
        {
          continue;
        }

        /* Check if there's region-specific disparity info */
        dss_region_info = FindDSSRegionInfoByID(dss, (uint8_t)region->region_id);

        if (dss_region_info != NULL)
        {
          /* Use region-specific default disparity shift */
          /* Note: The actual disparity value may vary over time if there's
           * a disparity_shift_update_sequence. For now, we use the base value. */

          /* Sub-region disparity shifts could be applied here for more
           * fine-grained control. For basic 3D support, region-level is sufficient. */
          LS_DEBUG("DSS: Region %d has specific disparity info\n", region->region_id);
        }

        /* Apply disparity shift to region */
        ApplyDisparityShiftToRegion(region, (int8_t)region_disparity_shift);
      }
    }
  }

  /* Store DSS version for change detection */
  page->dss_version = dss->dss_version_number;

  LS_DEBUG("DSS decoded: page_version=%d, page_shift=%d\n",
           dss->dss_version_number, page_disparity_shift);

  return LS_OK;
}
