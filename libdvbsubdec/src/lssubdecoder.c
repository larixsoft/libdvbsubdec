/*-----------------------------------------------------------------------------
 * lssubdecoder.c
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
#include "lssubsegdec.h"
#include "lssuben300743.h"
#include "lssubpixmap.h"
#include "lssubconverter.h"
#include "lssubdisplay.h"
#include "lsptsmgr.h"
#include "lssubutils.h"
/*---------------------------------------------------------------------------
 * local static variables
 *--------------------------------------------------------------------------*/
static LS_Factory* gDecoderFactory = NULL;

/*---------------------------------------------------------------------------
 * local static functions declarations
 *--------------------------------------------------------------------------*/
static void       serviceCleanup(LS_Service* service);
static void       applyDDSSettings(LS_DisplayRegion* region, LS_Service* service);
static int32_t    parseSegments(LS_Service* service, uint8_t* data, int32_t data_size, LS_Displayset* displayset);
static int32_t    decodeEpochDisplayset(LS_Displayset* displayset, LS_DisplayPage* page);
static int32_t    decodeNormalCaseDisplayset(LS_Displayset* displayset, LS_DisplayPage* page);
static int32_t    extractPTS(LS_PESHeaderInfo* pesheaderinfo, uint64_t* pts);
static int32_t    listFindDisplayPageByPTSCBFn(void* a, void* b);
static int32_t    DisplayRegionCopy(LS_DisplayRegion* dest, LS_DisplayRegion* src);
static int32_t    processDisplayPTS(LS_Service* service, const uint64_t ptsValue);
static int32_t    ptsCommandHandler(LS_Service* service, const uint64_t ptsValue, const int32_t reason,
                                    void* user_data);
static int32_t    CompareRegionVersionNumber(int32_t prev, int32_t curr);
static void       listDestroyDisplayPageCBFn(void* data, void* userdata);
static LS_SegPCS* duplicateSegPCS(LS_Service* service, const LS_SegPCS* src);

/*---------------------------------------------------------------------------
 * local macros definitions
 *--------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------
 * local static functions
 *--------------------------------------------------------------------------*/
static int32_t
CompareRegionVersionNumber(int32_t prev, int32_t curr)
{
  int32_t status = 0;

  if ((curr > prev) ||
      ((prev == 15) &&
       (curr == 0)))
  {
    status = 1;
  }
  else if (curr == prev)
  {
    status = 0;
  }
  else if (curr < prev)
  {
    status = -1;
  }

  return status;
}


static void
listDestroyDisplayPageCBFn(void* data, void* userdata)
{
  LS_Service* service = NULL;
  LS_DisplayPage* page = NULL;

  if (data &&
      userdata)
  {
    page = (LS_DisplayPage*)data;
    service = (LS_Service*)userdata;
    LS_DisplayPageDelete(service, page);
    LS_DEBUG("display page<%p> was deleted from service<%p>\n", page, service);
  }
}


static int32_t
DisplayRegionCopy(LS_DisplayRegion* destregion, LS_DisplayRegion* srcregion)
{
  LS_Pixmap_t* pixmap = NULL;
  LS_Service* service = NULL;

  if ((destregion == NULL) ||
      (srcregion == NULL))
  {
    LS_ERROR("Invalid parameters:dest<%p>,src<%p>\n", (void*)destregion, (void*)srcregion);
    return LS_ERROR_GENERAL;
  }

  service = destregion->page->service;
  DEBUG_CHECK(service != NULL);

  if (!service)
  {
    LS_ERROR("service = NULL, return\n");
    return LS_ERROR_GENERAL;
  }

  SYS_MEMCPY((void*)destregion, (void*)srcregion, sizeof(LS_DisplayRegion));

  if (srcregion->pixmap != NULL)
  {
    pixmap = LS_NewPixmap(service, srcregion->pixmap->width, srcregion->pixmap->height, srcregion->pixmap->pixelFormat);

    if (pixmap == NULL)
    {
      LS_ERROR("LS_NewPixmap() Failed\n");
      return LS_ERROR_GENERAL;
    }

    pixmap->leftPos = srcregion->pixmap->leftPos;
    pixmap->topPos = srcregion->pixmap->topPos;
    SYS_MEMCPY((void*)(pixmap->data), (void*)(srcregion->pixmap->data), srcregion->pixmap->dataSize);
    destregion->pixmap = pixmap;
  }

  return LS_OK;
}


static int32_t
listFindDisplayPageByPTSCBFn(void* a, void* b)
{
  LS_DisplayPage* page = NULL;
  uint64_t pts = 0;

  if (b == NULL)
  {
    LS_ERROR("Invalid PTS information.\n");
    return -1;
  }

  pts = *((uint64_t*)b);
  page = (LS_DisplayPage*)a;
  LS_TRACE("page<%p>,page->regions<%p>\n", page, page->regions);

  if (page != NULL)
  {
    if (page->ptsValue == pts)
    {
      return 0;
    }
    else
    {
      return -1;
    }
  }
  else
  {
    return -1;
  }
}


static int32_t
extractPTS(LS_PESHeaderInfo* header, uint64_t* pts)
{
  uint8_t PTS_DTS_flag;
  uint64_t PTS_14_0;
  uint64_t PTS_29_15;
  uint64_t PTS_32_30;
  int32_t status = LS_OK;

  if ((header == NULL) ||
      (pts == NULL))
  {
    LS_ERROR("Wrong parameters: header<%p>, pts<%p>\n", (void*)header, (void*)pts);
    return LS_ERROR_GENERAL;
  }

  PTS_DTS_flag = header->PTS_DTS_flags;
  PTS_14_0 = (uint64_t)(header->PTS_14_0);
  PTS_29_15 = (uint64_t)(header->PTS_29_15);
  PTS_32_30 = (uint64_t)(header->PTS_32_30);

  if ((PTS_DTS_flag == 0x02) ||
      (PTS_DTS_flag == 0x03))
  {
    *pts = (PTS_32_30 << 30) | (PTS_29_15 << 15) | (PTS_14_0);
  }
  else
  {
    LS_ERROR("PTS value is not defined in current PES package: " "PTS_DTS_flag<0x%02x>.\n", PTS_DTS_flag);
    status = LS_ERROR_STREAM_DATA;
  }

  return status;
}


static int32_t
parseSegments(LS_Service* service, uint8_t* data, int32_t size, LS_Displayset* displayset)
{
  int32_t read = 0;
  LS_SegHeader header;
  int32_t finished = LS_FALSE;
  int32_t res = LS_OK;
  int32_t ret_val = LS_OK;

  LS_ENTER("data<%p>,size<%d>\n", (void*)data, size);

  if ((data == NULL) ||
      (size == 0))
  {
    return LS_ERROR_GENERAL;
  }

  // Accept both standard (0x20) and alternative (0x0F) data identifiers
  if (((*data) != kPES_DATA_IDENTIFIER) &&
      ((*data) != kPES_DATA_IDENTIFIER_ALT))
  {
    LS_WARNING("Invalid data identifier<%02x> found, <20> or <0F> is expected\n", (*data));
    return LS_ERROR_GENERAL;
  }

  data++;
  read++;

  // For streams with data_identifier 0x0F, there's no separate subtitle_stream_id byte
  // The segment sync byte (0x0F) follows immediately
  if (((*data) != kPES_SUBTITLE_STREAM_ID) &&
      ((*data) != kSUBTITLING_SEGMENT_SYNC_BYTE))
  {
    LS_WARNING("Invalid byte<%02x> found, expected subtitle_stream_id<00> or segment_sync<0F>\n", *data);
    return LS_ERROR_STREAM_DATA;
  }

  // Skip subtitle_stream_id if present, otherwise we're already at segment data
  if ((*data) == kPES_SUBTITLE_STREAM_ID)
  {
    data++;
    read++;
  }

  while ((*data == kSUBTITLING_SEGMENT_SYNC_BYTE) &&
         read <= size)
  {
    if (read + kSEGMENT_DATA_FIELD_OFFSET > size)
    {
      LS_ERROR("LS_ERROR_STREAM_DATA:Not enough data for one segment\n");
      ret_val = LS_ERROR_STREAM_DATA;
      break;
    }

    SegmentParseHeader(data, &header);

    if (read + header.segment_length + kSEGMENT_DATA_FIELD_OFFSET > size)
    {
      LS_ERROR("LS_ERROR_STREAM_DATA:Not enough data for one segment\n");
      ret_val = LS_ERROR_STREAM_DATA;
      break;
    }

    switch (header.segment_type)
    {
      case kPAGE_COMPOSITION_SEGMENT:
        LS_DEBUG("New Segment: Page Composition Segment (PCS)\n");

        LS_SegPCS* pcs = SegmentNewPCS(service);

        DEBUG_CHECK(pcs != NULL);

        if (pcs == NULL)
        {
          LS_ERROR("Cannot create a new PCS,ignore this page\n");
          finished = LS_TRUE;
          break;
        }
        else
        {
          LS_TRACE("new pcs<%p> created.\n", (void*)pcs);
        }

        SYS_MEMCPY((void*)pcs, (void*)(&header), kSEGMENT_DATA_FIELD_OFFSET);
        /*move the data to the segment_data_field*/
        res = SegmentParsePCS(service, data + kSEGMENT_DATA_FIELD_OFFSET, pcs);

        if (res == LS_ERROR_GENERAL)
        {
          LS_ERROR("Error when parsing PCS,ignore this page\n");
          SegmentDeletePCS(service, pcs);
          pcs = NULL;
          finished = LS_TRUE;
          break;
        }

        SegmentDumpPCS(pcs);
        displayset->pcs = pcs;
        LS_DEBUG("Add one Page Composition Segment (PCS)\n");
        break;

      case kREGION_COMPOSITION_SEGMENT:
        LS_DEBUG("New Segment:Region Composition Segment (RCS)\n");

        LS_SegRCS* rcs = SegmentNewRCS(service);

        DEBUG_CHECK(rcs != NULL);

        if (rcs == NULL)
        {
          LS_ERROR("Cannot Create a New RCS: Ignore This Page.\n");
          finished = LS_TRUE;
          break;
        }

        SYS_MEMCPY((void*)rcs, (void*)(&header), kSEGMENT_DATA_FIELD_OFFSET);
        res = SegmentParseRCS(service, data + kSEGMENT_DATA_FIELD_OFFSET, rcs);

        if (res == LS_ERROR_GENERAL)
        {
          LS_ERROR("Error When Parsing RCS: Ignore This Page\n");
          SegmentDeleteRCS(service, rcs);
          rcs = NULL;
          finished = LS_TRUE;
          break;
        }

        SegmentDumpRCS(rcs);
        LS_ListAppend(displayset->rcs, (void*)rcs);
        LS_DEBUG("Add One Region Composition Segment (RCS)...\n");
        break;

      case kCLUT_DEFINITION_SEGMENT:
        LS_DEBUG("New Segment: CLUT Definition Segment (CDS)\n");

        LS_SegCDS* cds = SegmentNewCDS(service);

        DEBUG_CHECK(cds != NULL);

        if (cds == NULL)
        {
          LS_ERROR("Cannot create a new CDS,ignore this page\n");
          finished = LS_TRUE;
          break;
        }

        SYS_MEMCPY((void*)cds, (void*)&header, kSEGMENT_DATA_FIELD_OFFSET);
        res = SegmentParseCDS(service, data + kSEGMENT_DATA_FIELD_OFFSET, cds);

        if (res == LS_ERROR_GENERAL)
        {
          LS_ERROR("error when parsing CDS,ignore this page\n");
          SegmentDeleteCDS(service, cds);
          cds = NULL;
          finished = LS_TRUE;
          break;
        }

        SegmentDumpCDS(cds);
        LS_ListAppend(displayset->cds, (void*)cds);
        LS_DEBUG("add one CLUT Definition Segment (CDS)\n");
        break;

      case kOBJECT_DATA_SEGMENT:
        LS_DEBUG("New Segment: Object Data Segment (ODS)\n");

        LS_SegODS* ods = SegmentNewODS(service);

        DEBUG_CHECK(ods != NULL);

        if (ods == NULL)
        {
          LS_ERROR("Cannot create a new ODS,ignore this page\n");
          finished = LS_TRUE;
          break;
        }

        SYS_MEMCPY((void*)ods, (void*)&header, kSEGMENT_DATA_FIELD_OFFSET);
        res = SegmentParseODS(service, data + kSEGMENT_DATA_FIELD_OFFSET, ods);

        if (res == LS_ERROR_GENERAL)
        {
          LS_ERROR("error when parsing ODS,ignore this page\n");
          SegmentDeleteODS(service, ods);
          ods = NULL;
          finished = LS_TRUE;
          break;
        }

        SegmentDumpODS(ods);
        LS_ListAppend(displayset->ods, (void*)ods);
        LS_DEBUG("add one Object Data Segment (ODS)\n");
        break;

      case kDISPLAY_DEFINITION_SEGMENT:
        LS_DEBUG("New Segment: Display Definition Segment (DDS)\n");

        LS_SegDDS* dds = SegmentNewDDS(service);

        DEBUG_CHECK(dds != NULL);

        if (dds == NULL)
        {
          LS_ERROR("Cannot create a new DDS,ignore this page\n");
          finished = LS_TRUE;
          break;
        }

        SYS_MEMCPY((void*)dds, (void*)&header, kSEGMENT_DATA_FIELD_OFFSET);
        res = SegmentParseDDS(service, data + kSEGMENT_DATA_FIELD_OFFSET, dds);

        if (res == LS_ERROR_GENERAL)
        {
          LS_ERROR("error when parsing DDS,ignore this page\n");
          SegmentDeleteDDS(service, dds);
          dds = NULL;
          finished = LS_TRUE;
          break;
        }

        SegmentDumpDDS(dds);
        displayset->dds = dds;
        LS_ERROR("add one Display Definition Segment (DDS)\n");
        break;

      case kEND_OF_DISPLAY_SET_SEGMENT:
        LS_DEBUG("New Segment: End of Display Set Segment (EDS)\n");

        LS_SegEDS* eds = SegmentNewEDS(service);

        if (eds)
        {
          SYS_MEMCPY((void*)eds, (void*)&header, kSEGMENT_DATA_FIELD_OFFSET);
          res = SegmentParseEDS(service, data + kSEGMENT_DATA_FIELD_OFFSET, eds);
          SegmentDumpEDS(eds);
          displayset->eds = eds;
        }

        LS_DEBUG("Add one End of Display Set Segment (EDS)\n");
        finished = LS_TRUE;
        break;

      case kDISPARITY_SIGNALLING_SEGMENT:
        LS_DEBUG("New Segment: Disparity Signalling Segment (DSS)\n");
        break;

      default:
        LS_ERROR("unkonw segment type,ignored\n");
    }

    if (finished == LS_TRUE)
    {
      LS_TRACE("Parsing current PES packet finished\n");
      break;
    }

    read += (header.segment_length + 6);
    data += (header.segment_length + 6);

    if ((*data) == kEND_OF_PES_DATA_FIELD_MARKER)
    {
      LS_TRACE("Reach the end of pes data field: 0xFF\n");
      break;
    }

    LS_TRACE("read %d bytes\n", read);
  }

  return ret_val;

  LS_LEAVE("\n");
}


static int32_t
decodeEpochDisplayset(LS_Displayset* displayset, LS_DisplayPage* page)
{
  uint32_t num = 0;
  int32_t res = LS_OK;
  int32_t i;
  PCSRegionInfo* pcs_region_info = NULL;
  LS_Service* service = NULL;

  if ((displayset == NULL) ||
      (page == NULL))
  {
    return LS_ERROR_GENERAL;
  }

  service = page->service;

  if (service == NULL)
  {
    LS_ERROR("Invalid service ID found\n");
    return LS_ERROR_GENERAL;
  }

  /*for each regions defined in RCS,creat a pixmap for it and mark it
     as visiable if we can find it in current PCS*/
  // Check page_id match - page_id=0 in PMT acts as wildcard (accept any page)
  if (((page->service->pmtPageID.ancillaryPageId != 0) ||
       (page->service->pmtPageID.compositionPageId != 0)) &&
      ((displayset->pcs->page_id != page->service->pmtPageID.ancillaryPageId) &&
       (displayset->pcs->page_id != page->service->pmtPageID.compositionPageId)))
  {
    LS_ERROR("PCS->page_id (%d) does not match PMT pmtPageID (a=%d,c=%d)\n",
             displayset->pcs->page_id,
             page->service->pmtPageID.ancillaryPageId,
             page->service->pmtPageID.compositionPageId);
    return LS_ERROR_GENERAL;
  }

  // Log if using wildcard page_id
  if ((page->service->pmtPageID.ancillaryPageId == 0) &&
      (page->service->pmtPageID.compositionPageId == 0))
  {
    LS_INFO("Using wildcard page_id (accepting any page_id=%d)\n", displayset->pcs->page_id);
  }

  page->page_id = displayset->pcs->page_id;
  page->page_version_number = displayset->pcs->page_version_number;
  page->pageState = displayset->pcs->page_state;
  page->time_out = displayset->pcs->page_time_out;
  res = LS_ListCount(displayset->rcs, &num);
  DEBUG_CHECK(res == LS_OK);
  LS_DEBUG("Total %d RCS in this PCS\n", num);

  if (num == 0)
  {
    LS_INFO("There are no RCS defined in the displayset,ignore it...\n");
    return LS_ERROR_GENERAL;
  }

  res = LS_DisplaysetDecodeDDS(displayset->dds, page);
  DEBUG_CHECK(res == LS_OK);

  if (res != LS_OK)
  {
    LS_ERROR("Decode DDS failed\n");
    return LS_ERROR_GENERAL;
  }

  /* Decode DSS for 3D subtitle support */
  res = LS_DisplaysetDecodeDSS(displayset, page);
  DEBUG_CHECK(res == LS_OK);

  if (res != LS_OK)
  {
    LS_ERROR("Decode DSS failed\n");
    return LS_ERROR_GENERAL;
  }

  for (i = 0; i < (int32_t)num; i++)
  {
    LS_SegRCS* seg_rcs = LS_ListNthNode(displayset->rcs, i);

    if (seg_rcs)
    {
      LS_DisplayRegion* region = LS_DisplayRegionNew(service);

      if (region == NULL)
      {
        return LS_ERROR_GENERAL;
      }

      region->region_id = seg_rcs->region_id;
      region->page = page;
      LS_DEBUG("Decoding RCS with region_id=%d\n", seg_rcs->region_id);

      int32_t rcs_result = LS_DisplaysetDecodeRCS(displayset, region, LS_TRUE);

      if (rcs_result != LS_OK)
      {
        LS_WARNING("Failed to decode RCS for region_id=%d, skipping region\n", region->region_id);
        LS_DisplayRegionDelete(service, region);
        continue;          // Skip this region and continue with next
      }

      /*check if this region is in PCS*/
      pcs_region_info = DisplaysetFindPCSReginInfoByRegionID(displayset, region->region_id);

      if (pcs_region_info)
      {
        region->region_visible = LS_TRUE;
        LS_DEBUG("Region(id=%d) was marked VISIBLE\n", region->region_id);
        region->pixmap->leftPos = pcs_region_info->region_horizontal_address;
        region->pixmap->topPos = pcs_region_info->region_vertical_address;
      }
      else
      {
        region->region_visible = LS_FALSE;
        LS_DEBUG("Region(id=%d) was marked INVISIBLE\n", region->region_id);
      }

      res = LS_ListAppend(page->regions, (void*)region);
      DEBUG_CHECK(res == LS_OK);

      if (res != LS_OK)
      {
        LS_WARNING("Region(region_id=%d) can not be added to the list\n", region->region_id);
        LS_DisplayRegionDelete(service, region);
      }

      applyDDSSettings(region, service);
    }
  }

  return LS_OK;
}


/**
 * A page state of "normal case" indicates that the set of RCS may not be
 * complete (the set is only required to include the regions whose region data
 * structures - bitmap or CLUT family - are to be modified in this display set).
 * There is no requirement on decoders to attempt service acquisition at a
 * "normal case" display set.
 */
/*
    1. for all the visible regions in previous display page, pixmap for these
        regions were already allocated and should be allocated again.
    2. For all the regions defined in current PCS, if this region can be found in
        previous page, it should be updated, otherwise, a new pixmap should be
        allocated for this region.
 */
static int32_t
decodeNormalCaseDisplayset(LS_Displayset* displayset, LS_DisplayPage* page)
{
  LS_Service* service = NULL;
  LS_DisplayRegion* region_src = NULL;
  LS_DisplayRegion* region_dest = NULL;
  LS_DisplayRegion* region = NULL;
  PCSRegionInfo* pcs_region_info = NULL;
  LS_SegRCS* rcs = NULL;
  int32_t i = 0;
  int32_t res = LS_ERROR_GENERAL;
  uint32_t number = 0;
  int32_t page_version_updated = LS_FALSE;

  if ((displayset == NULL) ||
      (page == NULL))
  {
    LS_ERROR("Invalid Parameters:ds=%p,page=%p\n", (void*)displayset, (void*)page);
    return LS_ERROR_GENERAL;
  }

  service = page->service;
  DEBUG_CHECK(service != NULL);

  if (service == NULL)
  {
    LS_ERROR("Invalid service serviceID: NULL\n");
    return LS_ERROR_GENERAL;
  }

  if (service->latestDisplayPage == NULL)
  {
    LS_WARNING("No previous page found for this normal case displayset (PTS=%llu)\n",
               (unsigned long long)page->ptsValue);
    LS_INFO("This appears to be a continuation stream without epoch display set\n");
    LS_INFO("Processing this display set as if it were an epoch display set\n");
    // Process this displayset fully like an epoch displayset
    // Use the actual page_id and version from the PCS
    page->page_id = displayset->pcs->page_id;
    page->page_version_number = displayset->pcs->page_version_number;
    page->time_out = displayset->pcs->page_time_out;
    page->pageState = displayset->pcs->page_state;
    page->dds_flag = 0;
    // Note: page->ptsValue is already set correctly by ProcessPESPayLoad before calling this function
    // Process all regions from this displayset
    res = LS_ListCount(displayset->rcs, &number);

    if ((res != LS_OK) ||
        (number == 0))
    {
      LS_WARNING("No RCS in first displayset, cannot continue\n");
      return LS_ERROR_GENERAL;
    }

    // Decode DDS if present
    res = LS_DisplaysetDecodeDDS(displayset->dds, page);

    if (res != LS_OK)
    {
      LS_WARNING("Decode DDS failed in first displayset, continuing without DDS\n");
    }

    // Decode DSS for 3D subtitles if present
    res = LS_DisplaysetDecodeDSS(displayset, page);

    if (res != LS_OK)
    {
      LS_WARNING("Decode DSS failed in first displayset, continuing without 3D support\n");
    }

    // Process all RCS segments
    for (i = 0; i < (int32_t)number; i++)
    {
      LS_SegRCS* seg_rcs = LS_ListNthNode(displayset->rcs, i);

      if (seg_rcs)
      {
        LS_DisplayRegion* region = LS_DisplayRegionNew(service);

        if (region == NULL)
        {
          return LS_ERROR_GENERAL;
        }

        region->region_id = seg_rcs->region_id;
        region->page = page;

        int32_t rcs_result = LS_DisplaysetDecodeRCS(displayset, region, LS_TRUE);

        if (rcs_result != LS_OK)
        {
          LS_WARNING("Failed to decode RCS for region_id=%d, skipping region\n", region->region_id);
          LS_DisplayRegionDelete(service, region);
          continue;            // Skip this region and continue with next
        }

        /*check if this region is in PCS*/
        pcs_region_info = DisplaysetFindPCSReginInfoByRegionID(displayset, region->region_id);

        if (pcs_region_info)
        {
          region->region_visible = LS_TRUE;
          region->pixmap->leftPos = pcs_region_info->region_horizontal_address;
          region->pixmap->topPos = pcs_region_info->region_vertical_address;
        }
        else
        {
          region->region_visible = LS_FALSE;
        }

        res = LS_ListAppend(page->regions, (void*)region);

        if (res != LS_OK)
        {
          LS_DisplayRegionDelete(service, region);
        }

        applyDDSSettings(region, service);
      }
    }

    // Set this as the latest display page
    service->latestDisplayPage = page;
    LS_DEBUG("Early return from decodeNormalCaseDisplayset (PTS=%llu)\n", (unsigned long long)page->ptsValue);
    return LS_OK;
  }

  LS_INFO("Found previous page <%p>,(PTS:%llu,0x%08llx,%s) for this normal " "case displayset\n",
          (void*)service->latestDisplayPage,
          service->latestDisplayPage->ptsValue,
          service->latestDisplayPage->ptsValue,
          PTStoHMS(service->latestDisplayPage->ptsValue));
  page->page_id = displayset->pcs->page_id;
  page->page_version_number = displayset->pcs->page_version_number;
  page->time_out = displayset->pcs->page_time_out;
  page->pageState = displayset->pcs->page_state;
  /*check the page version number to see if we need update later*/
  page_version_updated = ((page->page_id == service->latestDisplayPage->page_id) &&
                          (page->page_version_number !=
                           service->latestDisplayPage->page_version_number)) ? LS_TRUE : LS_FALSE;
  /* use service's last display page as basis, update the regions included in
   * this new displayset
   */
  /*copy DDS from previous page*/
  page->dds_flag = service->latestDisplayPage->dds_flag;
  page->dds = service->latestDisplayPage->dds;
  /*check if new DDS presented in this new page*/
  res = LS_DisplaysetDecodeDDS(displayset->dds, page);
  DEBUG_CHECK(res == LS_OK);

  if (res != LS_OK)
  {
    LS_ERROR("Decode DDS failed\n");
    return LS_ERROR_GENERAL;
  }

  /* Decode DSS for 3D subtitles if present */
  res = LS_DisplaysetDecodeDSS(displayset, page);
  DEBUG_CHECK(res == LS_OK);

  if (res != LS_OK)
  {
    LS_WARNING("Decode DSS failed in update case, continuing without 3D support\n");
  }

  res = LS_ListCount(service->latestDisplayPage->regions, &number);
  DEBUG_CHECK(res == LS_OK);

  if (number == 0)
  {
    LS_WARNING("There are no regions defined in previous page\n");
    return LS_ERROR_GENERAL;
  }

  for (i = 0; i < (int32_t)number; i++)
  {
    region_src = (LS_DisplayRegion*)LS_ListNthNode(service->latestDisplayPage->regions, i);

    if (region_src)
    {
      region_dest = LS_DisplayRegionNew(service);

      if (region_dest)
      {
        region_dest->page = page;
        res = DisplayRegionCopy(region_dest, region_src);

        if (res == LS_ERROR_GENERAL)
        {
          LS_ERROR("Failed when copy region from previous page.\n");
          continue;
        }

        region_dest->page = page;
        /*can we find this region in current PCS? */
        pcs_region_info = DisplaysetFindPCSReginInfoByRegionID(displayset, region_dest->region_id);

        if (pcs_region_info)
        {
          region_dest->region_visible = LS_TRUE;
          LS_DEBUG("Region(id=%d) was marked VISIBLE\n", region_dest->region_id);
          region_dest->pixmap->leftPos = pcs_region_info->region_horizontal_address;
          region_dest->pixmap->topPos = pcs_region_info->region_vertical_address;
        }
        else
        {
          region_dest->region_visible = LS_FALSE;
          LS_DEBUG("Region(id=%d) was marked INVISIBLE\n", region_dest->region_id);
        }

        res = LS_ListAppend(page->regions, (void*)region_dest);
        DEBUG_CHECK(res == LS_OK);
      }
    }
  }

  /* for each regions defined in current RCS list, we need check if
   * update needed
   */
  /*---------------------------------------------------------------------------
   *    A service shall inspect every RCS in the display set to determine if the
   *    region is to be modified, for example,which pixel buffer modifications are
   *    required or where there is a modification to the associated CLUT family.
   *    It is sufficient for the service to inspect the RCS version number to
   *    determine if a region requires modification. There are three possible
   *    causes of modification, any or all of which may cause the modification:
   *        - region fill flag set;
   *        - CLUT contents modification;
   *        - a non-zero length object list.
   *    -- <<en300743v-1-3-1p.pdf>> 5.1.6 Points to note
   *-------------------------------------------------------------------------*/
  res = LS_ListCount(displayset->rcs, &number);
  DEBUG_CHECK(LS_OK == res);

  for (i = 0; i < (int32_t)number; i++)
  {
    rcs = LS_ListNthNode(displayset->rcs, i);

    if (rcs)
    {
      region = LS_DisplaypageFindRegionByID(page, rcs->region_id);

      if (region)
      {
        res = CompareRegionVersionNumber(region->version, rcs->region_version_number);

        if ((res != 0) ||
            (page_version_updated == LS_TRUE))
        {
          LS_DEBUG("Region(id=%d,p_ver=%d,c_ver=%d) will be updated\n",
                   region->region_id,
                   region->version,
                   rcs->region_version_number);

          int32_t rcs_result = LS_DisplaysetDecodeRCS(displayset, region, LS_FALSE);

          if (rcs_result != LS_OK)
          {
            LS_WARNING("Failed to decode RCS for existing region_id=%d, keeping old data\n", region->region_id);
          }
        }
        else
        {
          LS_DEBUG("Region(id=%d,p_ver=%d,c_ver=%d) will not be udpated\n",
                   region->region_id,
                   region->version,
                   rcs->region_version_number);
        }
      }
    }
  }

  return LS_OK;
}


static void
serviceCleanup(LS_Service* service)
{
  int32_t ret_val = LS_OK;
  void* service_ptr = service;  /* Save pointer for logging before free */

  do
  {
    if (service)
    {
      if (service->serviceMutex)
      {
        ret_val = LS_MutexDelete(service->serviceMutex);
        DEBUG_CHECK(ret_val == LS_OK);
        service->serviceMutex = NULL;
      }

      if (service->displayPageList)
      {
        ret_val = LS_ListDestroy(service->displayPageList);
        DEBUG_CHECK(ret_val == LS_OK);
        LS_INFO("service->displayPageList (%p,%p) cleaned up now\n", (void*)service, (void*)(service->displayPageList));
      }

      SYS_MEMSET(service, 0, sizeof(LS_Service));
      LS_Free(ServiceSystemHeap(), (void*)(service));
    }
  }while (0);

  LS_DEBUG("service %p was cleaned up: %s\n", (void*)service_ptr, ServiceErrString(ret_val));
}


int32_t
ProcessPESPayLoad(LS_Service* service, LS_PESHeaderInfo* header, uint8_t* payload, int32_t payload_size)
{
  int32_t res = LS_OK;
  int32_t ret_val = LS_OK;
  LS_Displayset* displayset = NULL;
  LS_DisplayPage* displaypage = NULL;
  int32_t errorCode = LS_OK;
  LS_PTSRequestInfo pts_request;
  uint64_t ptsValue = 0;

  LS_ENTER("ProcessPESPayLoad(service<%p>,header<%p>,paylaod<%p>,payload_size<%d>)\n",
           (void*)service,
           (void*)header,
           (void*)payload,
           payload_size);

  do
  {
    if (service == NULL)
    {
      LS_ERROR("LS_ERROR_GENERAL: Invalid parameters:service= NULL\n");
      ret_val = LS_ERROR_GENERAL;
      break;
    }

    /*------------------------------------------------------------------
     * extract the PTS value
     *------------------------------------------------------------------*/
    res = extractPTS(header, &ptsValue);
    LS_DEBUG("Extracted PTS=%llu from PES header\n", (unsigned long long)ptsValue);

    if (res != LS_OK)
    {
      LS_ERROR("LS_ERROR_GENERAL: Extract PTS for current PES failed.\n");
      ret_val = res;
      break;
    }
    else
    {
      LS_TRACE("Extract (PTS:%llu,0x%08llx,%s).\n", ptsValue, ptsValue, PTStoHMS(ptsValue));
    }

    /*------------------------------------------------------------------
     * parse the coded data into a new display set
     *------------------------------------------------------------------*/
    displayset = LS_DisplaysetNew(service, &errorCode);
    DEBUG_CHECK(displayset != NULL);

    if (displayset == NULL)
    {
      LS_ERROR("%s: Creating new displayset failed\n", ServiceErrString(errorCode));
      ret_val = errorCode;
      break;
    }

    LS_DEBUG("Displayset %p created\n", (void*)displayset);
    displayset->pts = ptsValue;
    res = parseSegments(service, payload, payload_size, displayset);

    if (res != LS_OK)
    {
      LS_ERROR("%s: when processing pes data payload %p\n", ServiceErrString(res), (void*)payload);
      ret_val = res;
      break;
    }

    /*------------------------------------------------------------------
     * decode display set into displayable page in pixmap
     *------------------------------------------------------------------*/
    displaypage = LS_DisplayPageNew(service, &errorCode);

    if (displaypage == NULL)
    {
      LS_ERROR("%s: Can not create a new display page\n", ServiceErrString(errorCode));
      ret_val = errorCode;
      break;
    }

    LS_DEBUG("Displaypage %p created\n", (void*)displaypage);
    displaypage->service = service;
    displaypage->ptsValue = ptsValue;
    displaypage->displayset = displayset;

    if (displayset->pcs == NULL)
    {
      /* A display set is not required to contain a page composition
       * segment. Within the same page composition for example a region
       * composition may change. If no page composition segment is
       * contained, the page state is not signalled; however, such
       * display set will result in a new page instance equivalent to
       * a "page update". */
      displaypage->pageType = LS_DISPLAY_PAGE_TYPE_UPDATE_PAGE;

      if (service->latestDisplayPage &&
          service->latestDisplayPage->displayset)
      {
        displayset->pcs = duplicateSegPCS(service, service->latestDisplayPage->displayset->pcs);
      }
    }
    else
    {
      if ((displayset->pcs->page_state == kPCS_PAGE_STATE_ACQUISITION_POINT) ||
          (displayset->pcs->page_state == kPCS_PAGE_STATE_MODE_CHANGE))
      {
        displaypage->pageType = LS_DISPLAY_PAGE_TYPE_EPOCH_PAGE;
      }
      else if (displayset->pcs->page_state == kPCS_PAGE_STATE_NORMAL_CASE)
      {
        displaypage->pageType = LS_DISPLAY_PAGE_TYPE_NORMAL_PAGE;
      }
      else
      {
        LS_ERROR("Unsupported or Reserved page state found:%d\n", displayset->pcs->page_state);
        ret_val = LS_ERROR_STREAM_DATA;
        break;
      }
    }

    switch (displaypage->pageType)
    {
      case LS_DISPLAY_PAGE_TYPE_EPOCH_PAGE:
        LS_INFO("Displaypage: LS_DISPLAY_PAGE_TYPE_EPOCH_PAGE\n");
        res = decodeEpochDisplayset(displayset, displaypage);
        break;

      case LS_DISPLAY_PAGE_TYPE_NORMAL_PAGE:
        LS_INFO("Displaypage: LS_DISPLAY_PAGE_TYPE_NORMAL_PAGE\n");
        res = decodeNormalCaseDisplayset(displayset, displaypage);
        break;

      case LS_DISPLAY_PAGE_TYPE_UPDATE_PAGE:
        LS_INFO("Displaypage: LS_DISPLAY_PAGE_TYPE_UPDATE_PAGE\n");
        res = decodeNormalCaseDisplayset(displayset, displaypage);
        break;

      default:
        LS_ERROR("Displaypage: LS_DISPLAY_PAGE_TYPE_UNKOWN\n");
        res = LS_ERROR_GENERAL;
    }

    if (res != LS_OK)
    {
      LS_ERROR("%s: when decode displayset %p\n", ServiceErrString(res), (void*)displayset);
      ret_val = res;
      break;
    }

    /*------------------------------------------------------------------
     * regiser PTS
     *------------------------------------------------------------------*/
    displaypage->status = LS_DISPLAYPAGE_STATUS_DECODED;
    res = LS_ListAppend(service->displayPageList, (void*)displaypage);

    if (res != LS_OK)
    {
      LS_ERROR("Cannot append displaypage<%p> to the list.\n", (void*)displaypage);
      ret_val = res;
      break;
    }
    else
    {
      LS_INFO("displayPage<%p> (PTS:%llu,0x%08llx,%s) was appended to " "displayPageList %p\n",
              (void*)displaypage,
              displaypage->ptsValue,
              displaypage->ptsValue,
              PTStoHMS(displaypage->ptsValue),
              (void*)service->displayPageList);
      LS_INFO("service->latestDisplayPage = %p\n", service->latestDisplayPage);

      if (service->latestDisplayPage)
      {
        LS_INFO("service->latestDisplayPage = %p," "service->latestDisplayPage->status = %d\n",
                service->latestDisplayPage,
                service->latestDisplayPage->status);

        if (service->latestDisplayPage &&
            (service->latestDisplayPage->status == LS_DISPLAYPAGE_STATUS_RETIRED))
        {
          LS_ListRemoveNode(service->displayPageList, (void*)(service->latestDisplayPage));
          LS_INFO("displayPage<%p>(PTS:%llu,0x%08llx,%s) was removed" " from displayPageList %p\n",
                  (void*)service->latestDisplayPage,
                  service->latestDisplayPage->ptsValue,
                  service->latestDisplayPage->ptsValue,
                  PTStoHMS(service->latestDisplayPage->ptsValue),
                  (void*)service->displayPageList);
          LS_DisplayPageDelete(service, service->latestDisplayPage);

          if (service->displayPageOnScreen == service->latestDisplayPage)
          {
            service->displayPageOnScreen = NULL;
          }

          service->latestDisplayPage = NULL;
        }
        else
        {
          LS_INFO("displayPage<%p> (PTS:%llu,0x%08llx,%s) was NOT " "removed from displayPageList %p\n\n",
                  (void*)service->latestDisplayPage,
                  service->latestDisplayPage->ptsValue,
                  service->latestDisplayPage->ptsValue,
                  PTStoHMS(service->latestDisplayPage->ptsValue),
                  (void*)service->displayPageList);
        }
      }

      service->latestDisplayPage = displaypage;
    }

    pts_request.clientId = service->ptsmgrClientID;
    pts_request.ptsValue = displaypage->ptsValue;
    LS_DEBUG("Registering PTS: displaypage->ptsValue=%llu\n", (unsigned long long)displaypage->ptsValue);
    pts_request.getCurrentPCRFunc = service->osdRender.getCurrentPCRFunc;
    pts_request.getCurrentPCRFuncData = service->osdRender.getCurrentPCRFuncData;
    pts_request.PTSCallBackFunc = ServicePTSNotificationCB;
    pts_request.PTSCBFuncData = (void*)service;
    pts_request.speed = service->serviceSpeed;
    res = LS_PTSMgrRegisterPTS(&pts_request);

    if (res != LS_OK)
    {
      LS_ERROR("%s: Cannot subtit pts request\n",
               ServiceErrString(res));
      ret_val = res;
      break;
    }

    displaypage->status = LS_DISPLAYPAGE_STATUS_PTS_REGISTERED;
    res = LS_TimerStart(displaypage->suicide_timer, LS_DP_CUBE_SUICIDE_TIMEOUT_SECONDS * 1000);
    LS_DEBUG("suicide_timer<%p> for displaypage<%p> is started," "duration = %d sec\n",
             (void*)(displaypage->suicide_timer),
             (void*)displaypage,
             LS_DP_CUBE_SUICIDE_TIMEOUT_SECONDS);
    /*good to go?*/
    ret_val = LS_OK;
  }while (0);

  if (ret_val != LS_OK)
  {
    if (displayset)
    {
      LS_DisplaysetDelete(service, displayset);
      displayset = NULL;
    }

    if (displaypage)
    {
      LS_DisplayPageDelete(service, displaypage);
      displaypage = NULL;
    }
  }

  return ret_val;
}


static int32_t
processDisplayPTS(LS_Service* service, const uint64_t ptsValue)
{
  LS_DisplayPage* fired_page = NULL;
  uint32_t dpnumber = 0;
  uint32_t res = 0;
  int32_t i = 0;
  int32_t timeout = 0;

  if (service == NULL)
  {
    LS_ERROR("Invalid Parameters: service=%p,ptsValue=(PTS:%llu,0x%08llx,%s)\n",
             (void*)service,
             ptsValue,
             ptsValue,
             PTStoHMS(ptsValue));
    return LS_ERROR_GENERAL;
  }

  LS_INFO("Now searching display page for (PTS:%llu,0x%08llx,%s)\n", ptsValue, ptsValue, PTStoHMS(ptsValue));
  res = LS_ListCount(service->displayPageList, &dpnumber);

  if ((res != LS_OK) ||
      (dpnumber == 0))
  {
    LS_ERROR("No valid/availible display page for (PTS:%llu,0x%08llx,%s)" " to show,status = %d,dpnumber=%d\n",
             ptsValue,
             ptsValue,
             PTStoHMS(ptsValue),
             res,
             dpnumber);
    return LS_OK;
  }

  for (i = 0; i < (int32_t)dpnumber; i++)
  {
    fired_page = LS_ListNthNode(service->displayPageList, i);

    if (fired_page)
    {
      if ((fired_page->ptsValue == ptsValue) &&
          (fired_page->status == LS_DISPLAYPAGE_STATUS_PTS_REGISTERED))
      {
        LS_INFO("displayPage <%p> (PTS:%llu,0x%08llx,%s) was found for " "matched PTS from displayPageList %p\n",
                (void*)fired_page,
                ptsValue,
                ptsValue,
                PTStoHMS(ptsValue),
                service->displayPageList);
        break;
      }
    }
  }

  if (fired_page == NULL)
  {
    LS_ERROR("Could not find display page with (PTS:%llu,0x%08llx,%s)\n", ptsValue, ptsValue, PTStoHMS(ptsValue));
    return LS_ERROR_GENERAL;
  }
  else
  {
    LS_DEBUG("Fired display page<%p>:(PTS:%llu,0x%08llx,%s),page status = %d\n",
             fired_page,
             ptsValue,
             ptsValue,
             PTStoHMS(ptsValue),
             fired_page->status);

    if ((fired_page != NULL) &&
        (fired_page->status == LS_DISPLAYPAGE_STATUS_PTS_REGISTERED))
    {
      LS_INFO("Found fired page %p : (PTS:%llu,0x%08llx,%s) \n", fired_page, ptsValue, ptsValue, PTStoHMS(ptsValue));
      res = LS_DisplayPageOnScreen(fired_page);
      DEBUG_CHECK(res == LS_OK);
      /*now start the page_time_out timer*/
      timeout = (fired_page->time_out * 100) / (service->serviceSpeed);
      LS_DEBUG("Start page_time_out_timer(%d sec, origin = %d sec)" " for display page %p\n",
               timeout,
               fired_page->time_out,
               (void*)fired_page);
      LS_TimerStart(fired_page->page_time_out_timer, timeout * 1000);
    }

    /*Now we can delete previous displayed display page */
    /*check if current displaying page is the same as latest page*/
    if (service->displayPageOnScreen)
    {
      if (service->displayPageOnScreen != service->latestDisplayPage)
      {
        LS_ListRemoveNode(service->displayPageList, (void*)(service->displayPageOnScreen));
        LS_INFO("display page<%p> (PTS:%llu,0x%08llx,%s) was " "removed from displayPageList %p\n",
                (void*)(service->displayPageOnScreen),
                service->displayPageOnScreen->ptsValue,
                service->displayPageOnScreen->ptsValue,
                PTStoHMS(service->displayPageOnScreen->ptsValue),
                (void*)(service->displayPageList));
        LS_DisplayPageDelete(service, service->displayPageOnScreen);

        if (service->displayPageOnScreen == service->latestDisplayPage)
        {
          service->latestDisplayPage = NULL;
        }

        service->displayPageOnScreen = NULL;
      }
    }

    service->displayPageOnScreen = fired_page;
    LS_DEBUG("display page<%p> (PTS:%llu,0x%08llx,%s) was put on screen " "for displaying\n",
             (void*)fired_page,
             fired_page->ptsValue,
             fired_page->ptsValue,
             PTStoHMS(fired_page->ptsValue));
  }

  return LS_OK;
}


static void
applyDDSSettings(LS_DisplayRegion* region, LS_Service* service)
{
  if ((region == NULL) ||
      (service == NULL))
  {
    LS_ERROR("Invalid parameters:region=%p,service=%p\n", (void*)region, (void*)service);
    return;
  }

  /*apply dds settings from stream first*/
  if (region->page->dds_flag)
  {
    if (region->page->dds.displayWindowFlag)
    {
      region->pixmap->leftPos += region->page->dds.displayWindow.leftPos;
      region->pixmap->topPos += region->page->dds.displayWindow.topPos;
    }
  }
  else
  {
    if (service->DDSFlag)
    {
      if (service->DDS.displayWindowFlag)
      {
        region->pixmap->leftPos += service->DDS.displayWindow.leftPos;
        region->pixmap->topPos += service->DDS.displayWindow.topPos;
      }
    }
  }
}


static int32_t
ptsCommandHandler(LS_Service* service, const uint64_t ptsValue, const int32_t reason, void* user_data)
{
  int32_t ret_val = LS_OK;
  LS_DisplayPage* firedpage = NULL;
  int32_t res = LS_OK;

  LS_ENTER("service<%p>,(PTS:%llu,0x%08llx,%s),reason<%d>,user_data<%p>\n",
           service,
           ptsValue,
           ptsValue,
           PTStoHMS(ptsValue),
           reason,
           user_data);

  switch (reason)
  {
    case PTS_INVALID:
      LS_DEBUG("start to process PTS_INVALID\n");
      firedpage = LS_ListFindUserNode(service->displayPageList, (void*)&ptsValue, listFindDisplayPageByPTSCBFn);

      if (firedpage)
      {
        if (firedpage != service->latestDisplayPage)
        {
          res = LS_ListRemoveNode(service->displayPageList, (void*)firedpage);
          DEBUG_CHECK(res == LS_OK);
          LS_DisplayPageDelete(service, firedpage);

          if (firedpage == service->displayPageOnScreen)
          {
            service->displayPageOnScreen = NULL;
          }

          LS_INFO("display page<%p> (PTS:%llu,0x%08llx,%s) was removed " "from displayPageList %p: latestDisplayPage\n",
                  (void*)firedpage,
                  firedpage->ptsValue,
                  firedpage->ptsValue,
                  PTStoHMS(firedpage->ptsValue),
                  (void*)(service->displayPageList));
        }
        else
        {
          firedpage->status = LS_DISPLAYPAGE_STATUS_RETIRED;
          LS_INFO("display page<%p> (PTS:%llu,0x%08llx,%s) was set as " "LS_DISPLAYPAGE_STATUS_RETIRED\n",
                  (void*)firedpage,
                  firedpage->ptsValue,
                  firedpage->ptsValue,
                  PTStoHMS(firedpage->ptsValue));
        }
      }

      ret_val = LS_OK;
      break;

    case PTS_PRESENTED:
      LS_DEBUG("start to process PTS_PRESENTED for (PTS:%llu,0x%08llx,%s)\n", ptsValue, ptsValue, PTStoHMS(ptsValue));
      ret_val = processDisplayPTS(service, ptsValue);
      break;

    default:
      LS_ERROR("ptsCommandHandler: UNKNOWN reason, ignore...\n");
      ret_val = LS_OK;
  }

  LS_LEAVE("\n");
  return ret_val;
}


static LS_SegPCS*
duplicateSegPCS(LS_Service* service, const LS_SegPCS* src)
{
  int32_t res = 0;
  uint32_t counter = 0;
  LS_SegPCS* pcs = NULL;

  if (NULL == src)
  {
    return NULL;
  }

  pcs = SegmentNewPCS(service);

  if (NULL == pcs)
  {
    LS_ERROR("SegmentNewPCS() failed\n");
    return NULL;
  }

  SYS_MEMSET((void*)pcs, 0, sizeof(*pcs));
  SYS_MEMCPY((void*)pcs, (void*)src, sizeof(*pcs));
  /*update the list of regioninfo*/
  pcs->regioninfos = LS_ListInit(__listMallocFunc, __listFreeFunc, &res);

  if (pcs->regioninfos == NULL)
  {
    LS_ERROR("LS_ListInit() failed\n");
    SegmentDeletePCS(service, pcs);
    return NULL;
  }

  res = LS_ListCount(src->regioninfos, &counter);

  if (res == LS_OK)
  {
    int32_t i = 0;

    for (i = 0; i < (int32_t)counter; i++)
    {
      PCSRegionInfo* src_regioninfo = NULL;
      PCSRegionInfo* dest_regioninfo = NULL;

      src_regioninfo = LS_ListNthNode(src->regioninfos, i);

      if (src_regioninfo)
      {
        dest_regioninfo = (PCSRegionInfo*)ServiceHeapMalloc(service, COMPOSITION_BUFFER, sizeof(PCSRegionInfo));

        if (dest_regioninfo)
        {
          LS_ListAppend(pcs->regioninfos, (void*)dest_regioninfo);
        }
        else
        {
          LS_ERROR("ServiceHeapMalloc() failed\n");
          break;
        }
      }
    }

    return pcs;
  }
  else
  {
    LS_ERROR("LS_ListCount() failed\n");
    SegmentDeletePCS(service, pcs);
    return NULL;
  }
}


/*----------------------------------------------------------------------------
 *    public API
 *---------------------------------------------------------------------------*/
int32_t
ServiceFactoryNew(uint32_t sys_heap_size)
{
  int32_t res = LS_OK;
  int32_t ret_val = LS_OK;
  int32_t errcode = LS_OK;

  do
  {
    gDecoderFactory = (LS_Factory*)SYS_MALLOC(sizeof(LS_Factory));

    if (gDecoderFactory == NULL)
    {
      LS_ERROR("LS_ERROR_SYSTEM_ERROR: request for %d bytes failed\n", sizeof(LS_Factory));
      ret_val = LS_ERROR_SYSTEM_ERROR;
      break;
    }

    SYS_MEMSET((void*)gDecoderFactory, 0, sizeof(LS_Factory));

    if ((res = LS_MutexCreate(&(gDecoderFactory->mutex))) != LS_OK)
    {
      LS_ERROR("LS_ERROR_SYSTEM_ERROR: create new mutex failed\n");
      ret_val = LS_ERROR_SYSTEM_ERROR;
      break;
    }

    gDecoderFactory->systemHeapSize = sys_heap_size;
    gDecoderFactory->referenceCount = 1;

    if ((res = LS_MemInit(&(gDecoderFactory->systemHeap), sys_heap_size)) != LS_OK)
    {
      LS_ERROR("%s: init internal heap failed\n", ServiceErrString(res));
      ret_val = res;
      break;
    }

    gDecoderFactory->serviceList = LS_ListInit(__listMallocFunc, __listFreeFunc, &errcode);
    DEBUG_CHECK(gDecoderFactory->serviceList != NULL);

    if (gDecoderFactory->serviceList == NULL)
    {
      LS_ERROR("%s: init service list failed\n", ServiceErrString(errcode));
      ret_val = errcode;
      break;
    }

    /*everything seems good to go...*/
    ret_val = LS_OK;
  }while (0);

  /*cleanup work if necessary*/
  if (ret_val != LS_OK)
  {
    ServiceFactoryDelete();
    gDecoderFactory = NULL;
  }

  return ret_val;
}


void
ServiceFactoryDelete(void)
{
  int32_t res = LS_OK;
  uint32_t decoders_left = 0;

  if (gDecoderFactory == NULL)
  {
    LS_DEBUG("gDecoderFactory was not initialized yet,return...\n");
    return;
  }

  if (gDecoderFactory->serviceList)
  {
    res = LS_ListCount(gDecoderFactory->serviceList, &decoders_left);

    if (LS_OK != res)
    {
      LS_WARNING("Unknow amount of service services left in the " "factory, memory leak may occur!\n");
    }
    else
    {
      if (decoders_left > 0)
      {
        LS_WARNING("There are still %d service services left in " "factory,memory leak may occur\n", decoders_left);
      }
    }

    LS_ListEmpty(gDecoderFactory->serviceList);
    LS_ListDestroy(gDecoderFactory->serviceList);
  }

  LS_MemFinalize(gDecoderFactory->systemHeap);

  if ((res = LS_MutexDelete(gDecoderFactory->mutex)) != LS_OK)
  {
    LS_ERROR("System error: delete mutex failed\n");
  }

  gDecoderFactory->mutex = NULL;
  SYS_MEMSET((void*)gDecoderFactory, 0, sizeof(LS_Factory));
  SYS_FREE(gDecoderFactory);
}


int32_t
ServiceFactoryInit(uint32_t sys_heap_size)
{
  int32_t ret_val = LS_OK;

  do
  {
    if (gDecoderFactory == NULL)
    {
      ret_val = ServiceFactoryNew(sys_heap_size);

      if (ret_val != LS_OK)
      {
        LS_ERROR("ServiceFactoryNew() failed\n");
        break;
      }
    }
    else
    {
      ret_val = LS_MutexWait(gDecoderFactory->mutex);
      DEBUG_CHECK(ret_val == LS_OK);
      gDecoderFactory->referenceCount += 1;
      ret_val = LS_MutexSignal(gDecoderFactory->mutex);
      DEBUG_CHECK(ret_val == LS_OK);
    }
  }while (0);

  if (ret_val != LS_OK)
  {
    ServiceFactoryDelete();
    gDecoderFactory = NULL;
  }

  return ret_val;
}


LS_MemHeap*
ServiceSystemHeap(void)
{
  int32_t ret_val = LS_OK;
  LS_MemHeap* systemHeap = NULL;

  do
  {
    if (gDecoderFactory != NULL)
    {
      LS_MutexWait(gDecoderFactory->mutex);
      systemHeap = gDecoderFactory->systemHeap;
      LS_MutexSignal(gDecoderFactory->mutex);
    }
    else
    {
      LS_ERROR("gDecoderFactory was not initialized yet\n");
      ret_val = LS_ERROR_GENERAL;
      break;
    }
  }while (0);

  if (ret_val == LS_OK)
  {
    return systemHeap;
  }
  else
  {
    return NULL;
  }
}


int32_t
ServiceFactoryRegister(LS_Service* service)
{
  int32_t ret_val = LS_OK;

  if (gDecoderFactory)
  {
    ret_val = LS_ListAppend(gDecoderFactory->serviceList, (void*)service);
  }
  else
  {
    LS_ERROR("gDecoderFactory was not initialized yet\n");
    ret_val = LS_ERROR_GENERAL;
  }

  return ret_val;
}


int32_t
ServiceFactoryUnregister(LS_Service* service)
{
  int32_t ret_val = LS_OK;

  if (gDecoderFactory)
  {
    ret_val = LS_ListRemoveNode(gDecoderFactory->serviceList, (void*)service);
  }
  else
  {
    LS_ERROR("gDecoderFactory was not initialized yet\n");
    ret_val = LS_ERROR_GENERAL;
  }

  return ret_val;
}


int32_t
ServiceFactoryRegisteredStatus(LS_Service* service)
{
  int32_t ret_val = LS_OK;

  if (gDecoderFactory)
  {
    ret_val = LS_ListFindNode(gDecoderFactory->serviceList, (void*)service);
  }
  else
  {
    LS_ERROR("gDecoderFactory was not initialized yet\n");
    ret_val = LS_ERROR_GENERAL;
  }

  return ret_val;
}


int32_t
ServiceFactoryFinalize(void)
{
  uint32_t ref_count;
  int32_t ret_val = LS_OK;

  if (gDecoderFactory == NULL)
  {
    return LS_OK;
  }

  LS_MutexWait(gDecoderFactory->mutex);
  gDecoderFactory->referenceCount -= 1;
  ref_count = gDecoderFactory->referenceCount;
  LS_MutexSignal(gDecoderFactory->mutex);

  if (ref_count <= 0)
  {
    ServiceFactoryDelete();
    gDecoderFactory = NULL;
  }

  return ret_val;
}


LS_Service*
ServiceInstanceNew(int32_t* errorCode)
{
  LS_Service* service = NULL;
  int32_t errcode = 0;
  int32_t ret_val = LS_OK;
  int32_t res = 0;

  do
  {
    service = (LS_Service*)LS_Malloc(ServiceSystemHeap(), sizeof(LS_Service));

    if (NULL == service)
    {
      LS_ERROR("memory allocation failed\n");
      ret_val = LS_ERROR_SYSTEM_BUFFER;
      break;
    }

    SYS_MEMSET((void*)service, 0, sizeof(LS_Service));
    service->magic_id = SERVICE_MAGIC_NUMBER;
    service->serviceSpeed = LS_DECODER_SPEED_NORMAL;
    /*create a new list for decoded display-set, which is used to display*/
    service->displayPageList = LS_ListInit(__listMallocFunc, __listFreeFunc, &errcode);

    if (NULL == service->displayPageList)
    {
      LS_ERROR("cannot init service->displayPageList\n");
      ret_val = errcode;
      break;
    }

    /*create mutex to protect this service's private data.*/
    res = LS_MutexCreate(&(service->serviceMutex));
    DEBUG_CHECK(LS_OK == res);
    LS_DEBUG("mutex <%p> is created,serviceMutex for service <%p>\n", (void*)(service->serviceMutex), service);
    service->displayWidth = kEN300743_DVB_SUBTITLE_WIDTH;
    service->displayHeight = kEN300743_DVB_SUBTITLE_HEIGHT;
    service->state = LS_SERVICE_STATE_NULL;
    SYS_MEMSET((void*)&service->osdRender, 0, sizeof(service->osdRender));
  } while (0);

  if (ret_val != LS_OK)
  {
    serviceCleanup(service);
    service = NULL;
  }

  *errorCode = ret_val;
  return service;
}


void
ServiceInstanceDelete(LS_Service* service)
{
  int32_t ret_val = LS_OK;

  if (service == NULL)
  {
    return;
  }

  if (service->magic_id != SERVICE_MAGIC_NUMBER)
  {
    LS_ERROR("service<%p>:invalid magic number %x\n", service->magic_id);
    return;
  }

  do
  {
    ret_val = LS_ListDestroy(service->displayPageList);
    DEBUG_CHECK(ret_val == LS_OK);
    DEBUG_CHECK(service->latestDisplayPage == NULL);
    DEBUG_CHECK(service->displayPageOnScreen == NULL);

    if (service->serviceMutex)
    {
      LS_INFO("delete serviceMutex<%p>...\n", (void*)service->serviceMutex);
      ret_val = LS_MutexDelete(service->serviceMutex);
      DEBUG_CHECK(ret_val == LS_OK);
      service->serviceMutex = NULL;
    }

    SYS_MEMSET(service, 0, sizeof(LS_Service));
    LS_Free(ServiceSystemHeap(), (void*)service);
  }while (0);
}


void
ServiceInstanceReset(LS_Service* service)
{
  int32_t ret_val = LS_OK;

  LS_ENTER("service<%p>\n", service);

  if (service == NULL)
  {
    return;
  }

  if (service->magic_id != SERVICE_MAGIC_NUMBER)
  {
    return;
  }

  do
  {
    ret_val = ServiceFactoryRegisteredStatus(service);

    if (LS_OK != ret_val)
    {
      LS_ERROR("service<%p> is not registered yet\n", service);
      break;
    }

    if (service->displayPageList)
    {
      uint32_t count = 0;

      LS_DEBUG("cleanup service<%p>->displayPageList<%p>...\n", service, service->displayPageList);
      LS_ListCount(service->displayPageList, &count);
      LS_DEBUG("There are still %d display pages in the service<%p> " "displayPageList <%p>\n",
               count,
               service,
               service->displayPageList);
      LS_ListEmptyData(service->displayPageList, listDestroyDisplayPageCBFn, service);
      LS_DEBUG("service<%p>->displayPageList<%p> was cleaned up now\n", service, service->displayPageList);
    }

    /*this latestDisplayPage exists in displayPageList*/
    service->latestDisplayPage = NULL;
    /*displayPageOnScreen should be destroyed when service stopped*/
    DEBUG_CHECK(service->displayPageOnScreen == NULL);
    service->state = LS_SERVICE_STATE_NULL;
  }while (0);

  if (ret_val == LS_OK)
  {
    LS_DEBUG("ServiceInstanceReset: Success!\n");
  }
  else
  {
    LS_ERROR("%s: ServiceInstanceReset failed\n", ServiceErrString(ret_val));
  }

  LS_LEAVE("service<%p>\n", service);
}


void
ServicePTSNotificationCB(const uint64_t ptsValue, PTSReason reason, void* user_data)
{
  LS_Service* service = NULL;
  int32_t res = LS_TRUE;

  LS_ENTER("ServicePTSNotificationCB(serviceID=%p,(PTS:%llu,0x%08llx,%s))\n",
           (void*)user_data,
           ptsValue,
           ptsValue,
           PTStoHMS(ptsValue));
  /*when PTS fired, send a message to display thread with a signal*/
  service = (LS_Service*)user_data;
  res = verifyServiceID(service);

  if (LS_TRUE != res)
  {
    LS_ERROR("LS_ERROR_GENERAL: serviceID <%p> is invalid\n", service);
    return;
  }

  LS_MutexWait(service->serviceMutex);
  LS_DEBUG("(PTS:%llu,0x%08llx,%s) fired,reason = %d\n", ptsValue, ptsValue, PTStoHMS(ptsValue), reason);
  res = ptsCommandHandler(service, ptsValue, reason, NULL);

  if (res != LS_OK)
  {
    LS_ERROR("Cannot signal service %p to process fired PTS\n", (void*)service);
    return;
  }

  LS_INFO("(PTS:%llu,0x%08llx,%s) was processed\n", ptsValue, ptsValue, PTStoHMS(ptsValue));
  LS_MutexSignal(service->serviceMutex);
  return;
}


void*
ServiceHeapMalloc(LS_Service* service, int32_t partition, uint32_t bytes)
{
  if (service == NULL)
  {
    return NULL;
  }

  switch (partition)
  {
    case CODED_DATA_BUFFER:
      return LS_Malloc(service->codedDataBufferHeap, bytes);

    case PIXEL_BUFFER:
      return LS_Malloc(service->pixelBufferHeap, bytes);

    case COMPOSITION_BUFFER:
      return LS_Malloc(service->compositionBufferHeap, bytes);

    default:
      return NULL;
  }
}


void
ServiceHeapFree(LS_Service* service, int32_t partition, void* mem_p)
{
  if (service == NULL)
  {
    return;
  }

  switch (partition)
  {
    case CODED_DATA_BUFFER:
      LS_Free(service->codedDataBufferHeap, mem_p);
      break;

    case PIXEL_BUFFER:
      LS_Free(service->pixelBufferHeap, mem_p);
      break;

    case COMPOSITION_BUFFER:
      LS_Free(service->compositionBufferHeap, mem_p);
      break;

    default:
      LS_ERROR("partition %d is not supported\n");
  }

  return;
}


char*
ServiceErrString(int32_t err_code)
{
  char* ret_val = NULL;

  switch (err_code)
  {
    case LS_OK:
      ret_val = "LS_OK = 1";
      break;

    case LS_ERROR_GENERAL:
      ret_val = "LS_ERROR_GENERAL(0)";
      break;

    case LS_ERROR_CODED_DATA_BUFFER:
      ret_val = "LS_ERROR_CODED_DATA_BUFFER(-1)";
      break;

    case LS_ERROR_PIXEL_BUFFER:
      ret_val = "LS_ERROR_PIXEL_BUFFER(-2)";
      break;

    case LS_ERROR_COMPOSITION_BUFFER:
      ret_val = "LS_ERROR_COMPOSITION_BUFFER(-3)";
      break;

    case LS_ERROR_SYSTEM_BUFFER:
      ret_val = "LS_ERROR_SYSTEM_BUFFER(-4)";
      break;

    case LS_ERROR_SYSTEM_ERROR:
      ret_val = "LS_ERROR_SYSTEM_ERROR(-5)";
      break;

    case LS_ERROR_STREAM_DATA:
      ret_val = "LS_ERROR_STREAM_DATA(-6)";
      break;

    case LS_ERROR_WRONG_STATE:
      ret_val = "LS_ERROR_WRONG_STATE(-7)";
      break;

    default:
      ret_val = "UNKNOWN ERROR";
  }

  return ret_val;
}


char*
ServiceStateString(int32_t state)
{
  char* ret_val = NULL;

  switch (state)
  {
    case LS_SERVICE_STATE_VOID:
      ret_val = "LS_SERVICE_STATE_VOID(0)";
      break;

    case LS_SERVICE_STATE_NULL:
      ret_val = "LS_SERVICE_STATE_NULL(1)";
      break;

    case LS_SERVICE_STATE_READY:
      ret_val = "LS_SERVICE_STATE_READY(2)";
      break;

    case LS_SERVICE_STATE_PAUSED:
      ret_val = "LS_SERVICE_STATE_PAUSED(3)";
      break;

    case LS_SERVICE_STATE_PLAYING:
      ret_val = "LS_SERVICE_STATE_PLAYING(4)";
      break;

    default:
      ret_val = "ERROR: UNKNOWN STATE";
  }

  return ret_val;
}


void*
__listMallocFunc(uint32_t bytes)
{
  void* ret_val = NULL;

  ret_val = LS_Malloc(ServiceSystemHeap(), bytes);

  if (ret_val == NULL)
  {
    LS_ERROR("LS_ERROR_SYSTEM_ERROR: request for %d bytes failed\n", bytes);
  }

  return ret_val;
}


void
__listFreeFunc(void* ptr)
{
  LS_Free(ServiceSystemHeap(), ptr);
}


int32_t
LS_PreProcessPES(const char* data, LS_PESHeaderInfo* pesinfo)
{
  int32_t res = LS_OK;
  uint32_t bo = 0;

#if 0
  LS_DumpMem(data, 32);
#endif

  SYS_MEMSET((void*)pesinfo, 0, sizeof(LS_PESHeaderInfo));
  pesinfo->packet_start_code_prefix[0] = (uint8_t)ReadBitStream32((uint8_t*)data, 0, bo, 8, &res);
  bo += 8;
  pesinfo->packet_start_code_prefix[1] = (uint8_t)ReadBitStream32((uint8_t*)data, 0, bo, 8, &res);
  bo += 8;
  pesinfo->packet_start_code_prefix[2] = (uint8_t)ReadBitStream32((uint8_t*)data, 0, bo, 8, &res);
  bo += 8;

  if (!((pesinfo->packet_start_code_prefix[0] == kB0000) &&
        (pesinfo->packet_start_code_prefix[1] == kB0000) &&
        (pesinfo->packet_start_code_prefix[2] == kB0001)))
  {
    LS_WARNING("Wrong packet_start_code_prefix (%02x %02x %02x) found in " "current pes packet,should be 00 00 01\n",
               pesinfo->packet_start_code_prefix[0],
               pesinfo->packet_start_code_prefix[1],
               pesinfo->packet_start_code_prefix[2]);
    return LS_ERROR_STREAM_DATA;
  }

  pesinfo->stream_id = (uint8_t)ReadBitStream32((uint8_t*)data, 0, bo, 8, &res);
  bo += 8;

  if ((pesinfo->stream_id != kPRIVATE_STREAM_1_STREAM_ID) &&
      (pesinfo->stream_id != 0xBE))                                                        // Also accept Private Stream 2
  {
    LS_ERROR("wrong stream_id (%02x) found in current pes packet," " should be 'BD' or 'BE'\n", pesinfo->stream_id);
    return LS_ERROR_STREAM_DATA;
  }

  pesinfo->PES_packet_length = (uint16_t)ReadBitStream32((uint8_t*)data, 0, bo, 16, &res);
  bo += 16;
  pesinfo->reserved_10 = (uint8_t)ReadBitStream32((uint8_t*)data, 0, bo, 2, &res);
  bo += 2;
  pesinfo->PES_scrambling_control = (uint8_t)ReadBitStream32((uint8_t*)data, 0, bo, 2, &res);
  bo += 2;
  pesinfo->PES_priority = (uint8_t)ReadBitStream32((uint8_t*)data, 0, bo, 1, &res);
  bo += 1;
  pesinfo->data_alignment_indicator = (uint8_t)ReadBitStream32((uint8_t*)data, 0, bo, 1, &res);
  bo += 1;
  pesinfo->copyright = (uint8_t)ReadBitStream32((uint8_t*)data, 0, bo, 1, &res);
  bo += 1;
  pesinfo->original_or_copy = (uint8_t)ReadBitStream32((uint8_t*)data, 0, bo, 1, &res);
  bo += 1;
  pesinfo->PTS_DTS_flags = (uint8_t)ReadBitStream32((uint8_t*)data, 0, bo, 2, &res);
  bo += 2;

  if ((pesinfo->PTS_DTS_flags == 0x00) ||
      (pesinfo->PTS_DTS_flags == 0x01))
  {
    LS_DEBUG("No PTS field presented in current PES.PTS_DTS_flags = 0x%02x\n", pesinfo->PTS_DTS_flags);
    return LS_ERROR_STREAM_DATA;
  }

  pesinfo->ESCR_flag = (uint8_t)ReadBitStream32((uint8_t*)data, 0, bo, 1, &res);
  bo += 1;
  pesinfo->ES_rate_flag = (uint8_t)ReadBitStream32((uint8_t*)data, 0, bo, 1, &res);
  bo += 1;
  pesinfo->DSM_trick_mode_flag = (uint8_t)ReadBitStream32((uint8_t*)data, 0, bo, 1, &res);
  bo += 1;
  pesinfo->additional_copy_info_flag = (uint8_t)ReadBitStream32((uint8_t*)data, 0, bo, 1, &res);
  bo += 1;
  pesinfo->PES_CRC_flag = (uint8_t)ReadBitStream32((uint8_t*)data, 0, bo, 1, &res);
  bo += 1;
  pesinfo->PES_extension_flag = (uint8_t)ReadBitStream32((uint8_t*)data, 0, bo, 1, &res);
  bo += 1;
  pesinfo->PES_header_data_length = (uint8_t)ReadBitStream32((uint8_t*)data, 0, bo, 8, &res);
  bo += 8;

  if (pesinfo->PES_packet_length <= (pesinfo->PES_header_data_length + 3))
  {
    LS_WARNING("Not enough data, PES_packet_length = %d\n", pesinfo->PES_packet_length);
    return LS_ERROR_STREAM_DATA;
  }

  if ((pesinfo->PTS_DTS_flags == 0x02) ||                         /*'10' or '11'*/
      (pesinfo->PTS_DTS_flags == 0x03))
  {
    pesinfo->PTS_0010 = (uint8_t)ReadBitStream32((uint8_t*)data, 0, bo, 4, &res);
    bo += 4;
    pesinfo->PTS_32_30 = (uint8_t)ReadBitStream32((uint8_t*)data, 0, bo, 3, &res);
    bo += 3;
    pesinfo->marker_bit_1 = (uint8_t)ReadBitStream32((uint8_t*)data, 0, bo, 1, &res);
    bo += 1;
    pesinfo->PTS_29_15 = (uint16_t)ReadBitStream32((uint8_t*)data, 0, bo, 15, &res);
    bo += 15;
    pesinfo->marker_bit_2 = (uint8_t)ReadBitStream32((uint8_t*)data, 0, bo, 1, &res);
    bo += 1;
    pesinfo->PTS_14_0 = (uint16_t)ReadBitStream32((uint8_t*)data, 0, bo, 15, &res);
    bo += 15;
    pesinfo->marker_bit_3 = (uint8_t)ReadBitStream32((uint8_t*)data,
                                                     0,
                                                     bo,
                                                     1,
                                                     &res);
  }

#ifdef PES_DATA_DEBUG
  LS_DumpPESInfo(pesinfo);
#endif
  return LS_OK;
}


void
LS_DumpPESInfo(const LS_PESHeaderInfo* pes)
{
  (void)pes;  /* Unused when PES_DATA_DEBUG not defined */
#ifdef PES_DATA_DEBUG
  LS_ENTER("pes=%p\n", pes);
  LS_INFO("packet_start_code_prefix: %02x %02x %02x\n",
          pes->packet_start_code_prefix[0],
          pes->packet_start_code_prefix[1],
          pes->packet_start_code_prefix[2]);
  LS_INFO("stream_id: 0x%02x\n", pes->stream_id);
  LS_INFO("PES_packet_length: 0x%04x (%d)\n", pes->PES_packet_length, pes->PES_packet_length);
  LS_INFO("reserved_10: 0x%02x\n", pes->reserved_10);
  LS_INFO("PES_scrambling_control: 0x%02x\n", pes->PES_scrambling_control);
  LS_INFO("PES_priority: 0x%02x\n", pes->PES_priority);
  LS_INFO("data_alignment_indicator: 0x%02x\n", pes->data_alignment_indicator);
  LS_INFO("copyright: 0x%02x\n", pes->copyright);
  LS_INFO("original_or_copy: 0x%02x\n", pes->original_or_copy);
  LS_INFO("PTS_DTS_flags: 0x%02x\n", pes->PTS_DTS_flags);
  LS_INFO("ESCR_flag: 0x%02x\n", pes->ESCR_flag);
  LS_INFO("ES_rate_flag: 0x%02x\n", pes->ES_rate_flag);
  LS_INFO("DSM_trick_mode_flag: 0x%02x\n", pes->DSM_trick_mode_flag);
  LS_INFO("additional_copy_info_flag: 0x%02x\n", pes->additional_copy_info_flag);
  LS_INFO("PES_CRC_flag: 0x%02x\n", pes->PES_CRC_flag);
  LS_INFO("PES_extension_flag: 0x%02x\n", pes->PES_extension_flag);
  LS_INFO("PES_header_data_length: 0x%02x (%d)\n", pes->PES_header_data_length, pes->PES_header_data_length);
  LS_INFO("PTS_0010: 0x%02x\n", pes->PTS_0010);
  LS_INFO("PTS_32_30: 0x%02x\n", pes->PTS_32_30);
  LS_INFO("PTS_29_15: 0x%02x\n", pes->PTS_29_15);
  LS_INFO("PTS_14_0: 0x%02x\n", pes->PTS_14_0);
  LS_LEAVE("\n");
#endif
}


int32_t
verifyServiceID(LS_Service* service)
{
  int32_t ret_val = LS_TRUE;
  int32_t res = LS_OK;

  do
  {
    if (NULL == service)
    {
      LS_ERROR("LS_ERROR_GENERAL: NULL == service\n");
      ret_val = LS_FALSE;
      break;
    }

    res = ServiceFactoryRegisteredStatus(service);

    if (LS_OK != res)
    {
      LS_ERROR("LS_ERROR_GENERAL: service <%p> is not registered\n", service);
      ret_val = LS_FALSE;
      break;
    }

    res = ValidateServiceMagicID(service->magic_id);

    if (LS_TRUE != res)
    {
      LS_ERROR("LS_ERROR_GENERAL: service <%p> is corrupted,magic_id =%s\n", service, service->magic_id);
      ret_val = LS_FALSE;
      break;
    }
  } while (0);

  return ret_val;
}
