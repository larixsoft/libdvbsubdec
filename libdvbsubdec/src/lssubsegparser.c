/*-----------------------------------------------------------------------------
 * lssubsegparser.c
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

#include "lssubsegparser.h"
#include "lssubsegdec.h"
#include "lssubdec.h"
#include "lssuben300743.h"
#include "lssubmacros.h"
#include "lsmemory.h"
#include "lssystem.h"
#include "lssubutils.h"
#include "lslist.h"
#include "lssubdecoder.h"
/*---------------------------------------------------------------------------
 * local static functions declarations
 *--------------------------------------------------------------------------*/
static int32_t __segODSParsePixelData(uint8_t* data, ODSPixelData* pixeldata);
static int32_t __segODSParseStringData(uint8_t* data, ODSStringData* stringdata);
static int32_t __CDSEntryCompareFunc(void* a, void* b);
static void    __deleteCDSClutInfo(void* data, void* user_data);
static int32_t __segDSSParseDisparityShiftUpdateSequence(LS_Service*                      service,
                                                         uint8_t*                         data,
                                                         DSSDisparityShiftUpdateSequence* sequence,
                                                         uint32_t*                        processed_length);
static DSSDisparityShiftUpdateSequence* __segDSSNewDSSDisparityShiftUpdateSequence(LS_Service* service);
static void                             __segDSSDeleteDSSDisparityShiftUpdateSequence(
  LS_Service* service,
  DSSDisparityShiftUpdateSequence*
  sequence);
static DSSRegionInfo* __SegDSSNewDSSRegionInfo(LS_Service* service);
static void           __SegDSSDeleteDSSRegionInfo(LS_Service* service, DSSRegionInfo* region);
static int32_t        __SegDSSParseDSSRegionInfo(LS_Service*    service,
                                                 uint8_t*       data,
                                                 DSSRegionInfo* region,
                                                 uint32_t*      processed_length);
static int32_t __compareCDSByID(void* a, void* b);
static int32_t __compareRCSByID(void* a, void* b);
static int32_t __compareODSByID(void* a, void* b);
static int32_t __comparePCSRegionInfobyID(void* a, void* b);
static void    ___deleteDSSDivisionPeriod(void* data, void* user_data);

/*---------------------------------------------------------------------------
 * local static functions
 *--------------------------------------------------------------------------*/
static int32_t
__CDSEntryCompareFunc(void* a, void* b)
{
  CDSCLUTInfo* entry = NULL;
  uint8_t entryid;

  if ((a == NULL) ||
      (b == NULL))
  {
    return -1;
  }

  entry = (CDSCLUTInfo*)a;
  entryid = (intptr_t)b;

  if (entry->CLUT_entry_id == entryid)
  {
    return 0;
  }
  else
  {
    return -1;
  }
}


static int32_t
__compareCDSByID(void* a, void* b)
{
  LS_SegCDS* cds = NULL;
  uint16_t clut_id = 0;

  if (a == NULL)
  {
    return -1;
  }

  cds = (LS_SegCDS*)a;
  clut_id = (uint16_t)(((uintptr_t)b) & 0xFFFF);

  if (cds->CLUT_id == clut_id)
  {
    return 0;
  }
  else
  {
    return -1;
  }
}


static int32_t
__compareRCSByID(void* a, void* b)
{
  LS_SegRCS* rcs = NULL;
  uint16_t region_id = 0;

  if (a == NULL)
  {
    return -1;
  }

  rcs = (LS_SegRCS*)a;
  region_id = (uint16_t)(((uintptr_t)b) & 0xFFFF);

  if (rcs->region_id == region_id)
  {
    return 0;
  }
  else
  {
    return -1;
  }
}


static int32_t
__compareODSByID(void* a, void* b)
{
  LS_SegODS* ods = NULL;
  uint16_t object_id = 0;

  if (a == NULL)
  {
    return -1;
  }

  ods = (LS_SegODS*)a;
  object_id = (uint16_t)(((uintptr_t)b) & 0xFFFF);

  if (ods->object_id == object_id)
  {
    return 0;
  }
  else
  {
    return -1;
  }
}


static int32_t
__comparePCSRegionInfobyID(void* a, void* b)
{
  PCSRegionInfo* regioninfo = NULL;
  uint16_t region_id = 0;

  if (a == NULL)
  {
    return -1;
  }

  regioninfo = (PCSRegionInfo*)a;
  region_id = (uint16_t)(((uintptr_t)b) & 0xFFFF);

  if (regioninfo->region_id == region_id)
  {
    return 0;
  }
  else
  {
    return -1;
  }
}


static int32_t
__segODSParsePixelData(uint8_t* data, ODSPixelData* pixeldata)
{
  int32_t status = LS_OK;

  if ((data == NULL) ||
      (pixeldata == NULL))
  {
    return LS_OK;
  }

  pixeldata->top_field_data_block_length = ReadBitStream32(data, 0, 0, 16, &status);
  pixeldata->bottom_field_data_block_length = ReadBitStream32(data, 2, 0, 16, &status);
  pixeldata->top_pixel_data_sub_block = data + 4;
  pixeldata->bottom_pixel_data_sub_block = pixeldata->top_pixel_data_sub_block + pixeldata->top_field_data_block_length;
  return LS_OK;
}


static int32_t
__segODSParseStringData(uint8_t* data, ODSStringData* stringdata)
{
  int32_t status = LS_OK;

  if (data &&
      stringdata)
  {
    stringdata->number_of_codes = ReadBitStream32(data,
                                                  0,
                                                  0,
                                                  8,
                                                  &status);
    stringdata->character_code = data + 1;
  }

  return LS_OK;
}


static DSSDisparityShiftUpdateSequence*
__segDSSNewDSSDisparityShiftUpdateSequence(LS_Service* service)
{
  DSSDisparityShiftUpdateSequence* sequence = NULL;
  int32_t errcode = 0;

  sequence = (DSSDisparityShiftUpdateSequence*)ServiceHeapMalloc(service,
                                                                 COMPOSITION_BUFFER,
                                                                 sizeof(DSSDisparityShiftUpdateSequence));
  DEBUG_CHECK(sequence != NULL);

  if (sequence == NULL)
  {
    LS_ERROR("COMPOSITION_BUFFER,Memory allocation failed: %d bytes\n", sizeof(DSSDisparityShiftUpdateSequence));
    return NULL;
  }

  SYS_MEMSET((void*)sequence, 0, sizeof(DSSDisparityShiftUpdateSequence));
  sequence->division_periods = LS_ListInit(__listMallocFunc, __listFreeFunc, &errcode);

  if (sequence->division_periods == NULL)
  {
    LS_ERROR("LS_ERROR_GENERAL: list init failed!\n");
    ServiceHeapFree(service, COMPOSITION_BUFFER, (void*)sequence);
    return NULL;
  }

  return sequence;
}


static void
__segDSSDeleteDSSDisparityShiftUpdateSequence(LS_Service* service, DSSDisparityShiftUpdateSequence* sequence)
{
  int32_t status = 0;

  if (sequence)
  {
    if (sequence->division_periods)
    {
      status = LS_ListEmptyData(sequence->division_periods, ___deleteDSSDivisionPeriod, (void*)service);
      DEBUG_CHECK(status == LS_OK);

      if (status == LS_ERROR_GENERAL)
      {
        LS_ERROR("LS_ListEmptyData failed\n");
      }

      status = LS_ListDestroy(sequence->division_periods);
      DEBUG_CHECK(status == LS_OK);

      if (status == LS_ERROR_GENERAL)
      {
        LS_ERROR("LS_ListDestroy failed\n");
      }
    }

    ServiceHeapFree(service, COMPOSITION_BUFFER, (void*)sequence);
  }
}


static int32_t
__segDSSParseDisparityShiftUpdateSequence(LS_Service*                      service,
                                          uint8_t*                         data,
                                          DSSDisparityShiftUpdateSequence* sequence,
                                          uint32_t*                        processed_length)
{
  uint32_t processed = 0;
  int32_t status = 0;
  int32_t i = 0;

  if ((data == NULL) ||
      (sequence == NULL) ||
      (processed_length == NULL))
  {
    return LS_ERROR_GENERAL;
  }

  sequence->disparity_shift_update_sequence_length = ReadBitStream32(data, 0, 0, 8, &status);
  sequence->interval_duration = ReadBitStream32(data, 1, 0, 24, &status);
  sequence->division_period_count = ReadBitStream32(data, 4, 0, 8, &status);
  processed += 5;

  for (i = 0; i < sequence->division_period_count; i++)
  {
    DSSDivisionPeriod* period = NULL;

    period = (DSSDivisionPeriod*)ServiceHeapMalloc(service, COMPOSITION_BUFFER, sizeof(DSSDivisionPeriod));
    DEBUG_CHECK(period != NULL);

    if (period == NULL)
    {
      LS_ERROR("COMPOSITION_BUFFER,Memory allocation failed: %d bytes\n", sizeof(DSSDivisionPeriod));
      return LS_ERROR_COMPOSITION_BUFFER;
    }

    SYS_MEMSET((void*)period, 0, sizeof(DSSDivisionPeriod));
    period->interval_count = ReadBitStream32(data + processed, 0, 0, 8, &status);
    period->disparity_shift_update_integer_part = ReadBitStream32(data, 1, 0, 8, &status);
    processed += 2;
    status = LS_ListAppend(sequence->division_periods, (void*)period);

    if (status != LS_OK)
    {
      LS_ERROR("LS_ERROR_GENERAL: list append failed\n");
      SYS_MEMSET((void*)period, 0, sizeof(DSSDivisionPeriod));
      ServiceHeapFree(service, COMPOSITION_BUFFER, (void*)period);
      return LS_ERROR_GENERAL;
    }
  }  /*for(i=0,i<sequence->division_period_count;i++)*/

  return LS_OK;
}


static DSSRegionInfo*
__SegDSSNewDSSRegionInfo(LS_Service* service)
{
  DSSRegionInfo* region = NULL;
  int32_t errcode = 0;

  region = (DSSRegionInfo*)ServiceHeapMalloc(service, COMPOSITION_BUFFER, sizeof(DSSRegionInfo));
  DEBUG_CHECK(region != NULL);

  if (region == NULL)
  {
    LS_ERROR("COMPOSITION_BUFFER,Memory allocation failed: %d bytes\n", sizeof(DSSRegionInfo));
    return NULL;
  }

  SYS_MEMSET((void*)region, 0, sizeof(DSSRegionInfo));
  region->sub_region_info = LS_ListInit(__listMallocFunc, __listFreeFunc, &errcode);
  DEBUG_CHECK(region->sub_region_info != NULL);

  if (region->sub_region_info == NULL)
  {
    LS_ERROR("LS_ERROR_GENERAL: list init failed\n");
    LS_Free(ServiceSystemHeap(), (void*)region);
    return NULL;
  }

  return region;
}


static void
__SegDSSDeleteDSSRegionInfo(LS_Service* service, DSSRegionInfo* region)
{
  uint32_t number = 0;
  int32_t status = LS_OK;
  int32_t i = 0;
  DSSSubRegionInfo* subregioninfo = NULL;

  if (region)
  {
    if (region->sub_region_info)
    {
      status = LS_ListCount(region->sub_region_info, &number);
      DEBUG_CHECK(status == LS_OK);

      for (i = 0; i < (int32_t)number; i++)
      {
        subregioninfo = LS_ListFirstData(region->sub_region_info);

        if (subregioninfo)
        {
          if (subregioninfo->disparity_shift_update_sequence)
          {
            __segDSSDeleteDSSDisparityShiftUpdateSequence(service, subregioninfo->disparity_shift_update_sequence);
          }

          subregioninfo->disparity_shift_update_sequence = NULL;
          ServiceHeapFree(service, COMPOSITION_BUFFER, subregioninfo);
        }
      }

      LS_ListDestroy(region->sub_region_info);
    }

    SYS_MEMSET((void*)region, 0, sizeof(DSSRegionInfo));
    ServiceHeapFree(service, COMPOSITION_BUFFER, (void*)region);
  }
}


static int32_t
__SegDSSParseDSSRegionInfo(LS_Service* service, uint8_t* data, DSSRegionInfo* regioninfo, uint32_t* processed_length)
{
  uint32_t processedlen = 0;
  int32_t status = LS_OK;
  uint32_t sequencelen = 0;
  int32_t n = 0;
  DSSSubRegionInfo* subregion = NULL;

  regioninfo->region_id = ReadBitStream32(data, 0, 0, 8, &status);
  regioninfo->disparity_shift_update_sequence_region_flag = ReadBitStream32(data, 1, 0, 1, &status);
  regioninfo->reserved = ReadBitStream32(data, 1, 1, 5, &status);
  regioninfo->number_of_subregions_minus_1 = ReadBitStream32(data, 1, 6, 2, &status);
  processedlen += 2;

  for (n = 0; n <= regioninfo->number_of_subregions_minus_1; n++)
  {
    subregion = (DSSSubRegionInfo*)ServiceHeapMalloc(service, COMPOSITION_BUFFER, sizeof(DSSSubRegionInfo));
    DEBUG_CHECK(subregion != NULL);

    if (subregion == NULL)
    {
      LS_ERROR("COMPOSITION_BUFFER,Memory allocation failed: %d bytes\n", sizeof(DSSSubRegionInfo));
      return LS_ERROR_COMPOSITION_BUFFER;
    }

    SYS_MEMSET((void*)subregion, 0, sizeof(DSSSubRegionInfo));

    if (regioninfo->number_of_subregions_minus_1 > 0)
    {
      subregion->subregion_horizontal_position = ReadBitStream32(data + processedlen, 0, 0, 16, &status);
      subregion->subregion_width = ReadBitStream32(data + processedlen, 0, 16, 16, &status);
      processedlen += 4;
    }

    subregion->subregion_disparity_shift_integer_part = ReadBitStream32(data + processedlen, 0, 0, 8, &status);
    subregion->subregion_disparity_shift_fractional_part = ReadBitStream32(data + processedlen, 0, 8, 4, &status);
    subregion->reserved = ReadBitStream32(data + processedlen, 0, 12, 4, &status);
    processedlen += 2;

    if (regioninfo->disparity_shift_update_sequence_region_flag == 1)
    {
      DSSDisparityShiftUpdateSequence* sequence = __segDSSNewDSSDisparityShiftUpdateSequence(service);

      DEBUG_CHECK(sequence != NULL);

      if (sequence == NULL)
      {
        LS_ERROR("cannot create a new sequence\n");
        return LS_ERROR_GENERAL;
      }

      status = __segDSSParseDisparityShiftUpdateSequence(service, data + processedlen, sequence, &sequencelen);

      if (status == LS_OK)
      {
        processedlen += sequencelen;
        subregion->disparity_shift_update_sequence = sequence;
      }
      else
      {
        LS_ERROR("SegDSSParseDisparityShiftUpdateSequence() failed\n");
        return LS_ERROR_GENERAL;
      }
    }
  }

  *processed_length = processedlen;
  return LS_OK;
}


static void
__deleteCDSClutInfo(void* data, void* user_data)
{
  LS_Service* service = NULL;
  LS_MemHeap* pMemHeap = NULL;

  if ((data == NULL) ||
      (user_data == NULL))
  {
    LS_DEBUG("nothing to do due data = %p, user_data = %p\n", data, user_data);
    return;
  }

  service = (LS_Service*)user_data;
  pMemHeap = service->compositionBufferHeap;
  LS_Free(pMemHeap, data);
  LS_DEBUG("data %p was freed from service %p COMPOSITION_BUFFER\n", data, user_data);
}


static void
___deleteDSSDivisionPeriod(void* data, void* user_data)
{
  LS_Service* service = NULL;

  if ((data == NULL) ||
      (service == NULL))
  {
    return;
  }

  service = (LS_Service*)user_data;
  LS_Free(service->compositionBufferHeap, data);
  LS_DEBUG("data %p was freed from service %p COMPOSITION_BUFFER\n", data, user_data);
}


/*---------------------------------------------------------------------------
 * API prototype
 *--------------------------------------------------------------------------*/
int32_t
SegmentParseHeader(uint8_t* data, LS_SegHeader* header)
{
  int32_t status = LS_OK;

  LS_ENTER("data=%p,header=%p\n", (void*)data, (void*)header);

  if ((data == NULL) ||
      (header == NULL))
  {
    LS_ERROR("Invalid input arguments: data=%p,header=%p\n", (void*)data, (void*)header);
    return LS_ERROR_GENERAL;
  }

  header->sync_byte = ReadBitStream32(data, 0, 0, 8, &status);
  header->segment_type = ReadBitStream32(data, 1, 0, 8, &status);
  header->page_id = ReadBitStream32(data, 2, 0, 16, &status);
  header->segment_length = ReadBitStream32(data, 4, 0, 16, &status);
  SegmentDumpHeader(header);
  LS_LEAVE("\n");
  return LS_OK;
}


void
SegmentDumpHeader(LS_SegHeader* header)
{
  if (header == NULL)
  {
    return;
  }

  LS_TRACE("header->sync_byte            = 0x%02x\n", header->sync_byte);
  LS_TRACE("header->segment_type     = 0x%02x\n", header->segment_type);
  LS_TRACE("header->page_id                = 0x%04x\n", header->page_id);
  LS_TRACE("header->segment_length = 0x%04x\n", header->segment_length);
}


LS_SegPCS*
SegmentNewPCS(LS_Service* service)
{
  LS_SegPCS* pcs = NULL;
  int32_t errcode = 0;

  pcs = (LS_SegPCS*)ServiceHeapMalloc(service, COMPOSITION_BUFFER, sizeof(LS_SegPCS));
  DEBUG_CHECK(pcs != NULL);

  if (NULL == pcs)
  {
    LS_ERROR("COMPOSITION_BUFFER,Memory allocation failed: %d bytes\n", sizeof(LS_SegPCS));
    return NULL;
  }

  /*init the regioninfos list*/
  SYS_MEMSET((void*)pcs, 0, sizeof(LS_SegPCS));
  pcs->regioninfos = LS_ListInit(__listMallocFunc, __listFreeFunc, &errcode);
  DEBUG_CHECK(pcs->regioninfos != NULL);

  if (pcs->regioninfos == NULL)
  {
    LS_ERROR("LS_ERROR_GENERAL:list init failed\n");
    ServiceHeapFree(service, COMPOSITION_BUFFER, pcs);
    return NULL;
  }

  return pcs;
}


void
SegmentDeletePCS(LS_Service* service, LS_SegPCS* pcs)
{
  PCSRegionInfo* regionInfo = NULL;

  if (pcs &&
      service)
  {
    regionInfo = LS_ListFirstData(pcs->regioninfos);

    while (regionInfo)
    {
      ServiceHeapFree(service, COMPOSITION_BUFFER, (void*)regionInfo);
      regionInfo = LS_ListFirstData(pcs->regioninfos);
    }

    LS_ListDestroy(pcs->regioninfos);
    SYS_MEMSET((void*)pcs, 0, sizeof(LS_SegPCS));
    ServiceHeapFree(service, COMPOSITION_BUFFER, (void*)pcs);
  }

  LS_TRACE("pcs = %p deleted\n", (void*)pcs);
}


int32_t
SegmentParsePCS(LS_Service* service, uint8_t* data, LS_SegPCS* pcs)
{
  int32_t status = LS_OK;
  int32_t processed_length = 0;

  LS_ENTER("data=%p,pcs=%p\n", (void*)data, (void*)pcs);

  if ((data == NULL) ||
      (pcs == NULL))
  {
    LS_LEAVE("\n");
    return LS_ERROR_GENERAL;
  }

  pcs->page_time_out = ReadBitStream32(data, 0, 0, 8, &status);
  pcs->page_version_number = ReadBitStream32(data, 1, 0, 4, &status);
  pcs->page_state = ReadBitStream32(data, 1, 4, 2, &status);
  pcs->reserved = ReadBitStream32(data, 1, 6, 2, &status);
  processed_length = 2;
  data += processed_length;

  while (processed_length < pcs->segment_length)
  {
    PCSRegionInfo* region = (PCSRegionInfo*)ServiceHeapMalloc(service, COMPOSITION_BUFFER, sizeof(PCSRegionInfo));

    if (region == NULL)
    {
      LS_ERROR("COMPOSITION_BUFFER,Memory allocation failed: %d bytes\n", sizeof(PCSRegionInfo));
      LS_LEAVE("\n");
      return LS_ERROR_COMPOSITION_BUFFER;
    }

    SYS_MEMSET((void*)region, 0, sizeof(PCSRegionInfo));
    region->region_id = ReadBitStream32(data, 0, 0, 8, &status);
    region->reserved = ReadBitStream32(data, 1, 0, 8, &status);
    region->region_horizontal_address = ReadBitStream32(data, 2, 0, 16, &status);
    region->region_vertical_address = ReadBitStream32(data, 4, 0, 16, &status);
    LS_TRACE("New region node<%p> was added onto the regioninfos list\n", region);
    status = LS_ListAppend(pcs->regioninfos, (void*)region);
    DEBUG_CHECK(status == LS_OK);

    if (status != LS_OK)
    {
      LS_ERROR("LS_ERROR_GENERAL: append region to list failed\n");
      LS_LEAVE("\n");
      return LS_ERROR_GENERAL;
    }

    data += 6;
    processed_length += 6;
  }

  LS_LEAVE("\n");
  return LS_OK;
}


void
SegmentDumpPCS(LS_SegPCS* pcs)
{
  uint32_t n = 0;
  int32_t i = 0;

  if (pcs == NULL)
  {
    return;
  }

  LS_TRACE("sync_byte                     = %02x\n", pcs->sync_byte);
  LS_TRACE("segment_type                = %02x\n", pcs->segment_type);
  LS_TRACE("page_id                         = %04x\n", pcs->page_id);
  LS_TRACE("segment_length            = %04x\n", pcs->segment_length);
  LS_TRACE("page_time_out             = %02x (%d)\n", pcs->page_time_out, pcs->page_time_out);
  LS_TRACE("page_version_number = %02x\n", pcs->page_version_number);
  LS_TRACE("page_state                    = %02x\n", pcs->page_state);
  LS_TRACE("reserved                        = %02x\n", pcs->reserved);

  if (pcs->regioninfos)
  {
    LS_ListCount(pcs->regioninfos, &n);
    LS_TRACE("There are %d regions in pcs %p\n", n, (void*)pcs);

    for (i = 0; i < (int32_t)n; i++)
    {
      PCSRegionInfo* regioninfo = LS_ListNthNode(pcs->regioninfos, i);

      if (regioninfo)
      {
        LS_TRACE("%dth region:\n", i);
        LS_TRACE("region_id                                 = %02x\n", regioninfo->region_id);
        LS_TRACE("reserved                                    = %02x\n", regioninfo->reserved);
        LS_TRACE("region_horizontal_address = %02x(%d)\n",
                 regioninfo->region_horizontal_address,
                 regioninfo->region_horizontal_address);
        LS_TRACE("region_vertical_address     = %02x(%d)\n",
                 regioninfo->region_vertical_address,
                 regioninfo->region_vertical_address);
      }
    }
  }
}


LS_SegRCS*
SegmentNewRCS(LS_Service* service)
{
  LS_SegRCS* rcs = NULL;
  int32_t errcode = 0;

  rcs = (LS_SegRCS*)ServiceHeapMalloc(service, COMPOSITION_BUFFER, sizeof(LS_SegRCS));
  DEBUG_CHECK(rcs != NULL);

  if (rcs == NULL)
  {
    LS_ERROR("COMPOSITION_BUFFER,Memory allocation failed: %d bytes\n", sizeof(LS_SegRCS));
    return NULL;
  }

  /*init the objectinfo_list list*/
  SYS_MEMSET((void*)rcs, 0, sizeof(LS_SegRCS));
  rcs->objectinfo_list = LS_ListInit(__listMallocFunc, __listFreeFunc, &errcode);
  DEBUG_CHECK(rcs->objectinfo_list != NULL);

  if (rcs->objectinfo_list == NULL)
  {
    LS_ERROR("LS_ERROR_GENERAL: list init failed\n");
    ServiceHeapFree(service, COMPOSITION_BUFFER, rcs);
    return NULL;
  }

  return rcs;
}


void
SegmentDeleteRCS(LS_Service* service, LS_SegRCS* rcs)
{
  RCSObjectInfo* objInfo = NULL;

  if (rcs)
  {
    if (rcs->objectinfo_list)
    {
      objInfo = LS_ListFirstData(rcs->objectinfo_list);

      while (objInfo)
      {
        ServiceHeapFree(service, COMPOSITION_BUFFER, (void*)objInfo);
        objInfo = LS_ListFirstData(rcs->objectinfo_list);
      }

      LS_ListDestroy(rcs->objectinfo_list);
    }

    SYS_MEMSET((void*)rcs, 0, sizeof(LS_SegRCS));
    ServiceHeapFree(service, COMPOSITION_BUFFER, (void*)rcs);
  }
}


int32_t
SegmentParseRCS(LS_Service* service, uint8_t* data, LS_SegRCS* rcs)
{
  int32_t status = LS_OK;
  int32_t processed_length;
  RCSObjectInfo* object = NULL;

  if ((data == NULL) ||
      (rcs == NULL))
  {
    LS_ERROR("LS_ERROR_GENERAL: Invalid input parameters:data=%p,rcs=%p\n", (void*)data, (void*)rcs);
    return LS_ERROR_GENERAL;
  }

  rcs->region_id = ReadBitStream32(data, 0, 0, 8, &status);
  rcs->region_version_number = ReadBitStream32(data, 1, 0, 4, &status);
  rcs->region_fill_flag = ReadBitStream32(data, 1, 4, 1, &status);
  rcs->reserved1 = ReadBitStream32(data, 1, 5, 3, &status);
  rcs->region_width = ReadBitStream32(data, 2, 0, 16, &status);
  rcs->region_height = ReadBitStream32(data, 4, 0, 16, &status);
  rcs->region_level_of_compatibility = ReadBitStream32(data, 6, 0, 3, &status);
  rcs->region_depth = ReadBitStream32(data, 6, 3, 3, &status);
  rcs->reserved2 = ReadBitStream32(data, 6, 6, 2, &status);
  rcs->CLUT_id = ReadBitStream32(data, 7, 0, 8, &status);
  rcs->region_8bit_pixel_code = ReadBitStream32(data, 8, 0, 8, &status);
  rcs->region_4bit_pixel_code = ReadBitStream32(data, 9, 0, 4, &status);
  rcs->region_2bit_pixel_code = ReadBitStream32(data, 9, 4, 2, &status);
  rcs->reserved3 = ReadBitStream32(data, 9, 6, 2, &status);
  processed_length = 10;
  data += processed_length;

  while (processed_length < rcs->segment_length)
  {
    object = (RCSObjectInfo*)ServiceHeapMalloc(service, COMPOSITION_BUFFER, sizeof(RCSObjectInfo));
    DEBUG_CHECK(object != NULL);

    if (object == NULL)
    {
      LS_ERROR("COMPOSITION_BUFFER,Memory allocation failed: %d bytes\n", sizeof(RCSObjectInfo));
      return LS_ERROR_COMPOSITION_BUFFER;
    }

    object->object_id = ReadBitStream32(data, 0, 0, 16, &status);
    object->object_type = ReadBitStream32(data, 2, 0, 2, &status);
    object->object_provider_flag = ReadBitStream32(data, 2, 2, 2, &status);
    object->object_horizontal_position = ReadBitStream32(data, 2, 4, 12, &status);
    object->reserved = ReadBitStream32(data, 4, 0, 4, &status);
    object->object_vertical_position = ReadBitStream32(data, 4, 4, 12, &status);

    if ((object->object_type == 0x01) ||
        (object->object_type == 0x02))
    {
      object->foreground_pixel_code = ReadBitStream32(data, 6, 0, 8, &status);
      object->background_pixel_code = ReadBitStream32(data, 7, 0, 8, &status);
    }

    status = LS_ListAppend(rcs->objectinfo_list, (void*)object);

    if (status != LS_OK)
    {
      LS_ERROR("LS_ERROR_GENERAL: list append failed\n");
      return LS_ERROR_GENERAL;
    }

    LS_TRACE("add a new object on the region(%p, region_id = %d)\n", rcs, rcs->region_id);

    if ((object->object_type == 0x01) ||
        (object->object_type == 0x02))
    {
      data += 8;
      processed_length += 8;
    }
    else
    {
      data += 6;
      processed_length += 6;
    }
  }

  return LS_OK;
}


void
SegmentDumpRCS(LS_SegRCS* rcs)
{
  uint32_t n = 0;
  int32_t i = 0;

  if (rcs == NULL)
  {
    LS_INFO("SegmentDumpRCS(): rcs = nill\n");
    return;
  }

  LS_TRACE("sync_byte                            = %02x\n", rcs->sync_byte);
  LS_TRACE("segment_type                     = %02x\n", rcs->segment_type);
  LS_TRACE("page_id                                = %04x\n", rcs->page_id);
  LS_TRACE("segment_length                 = %04x\n", rcs->segment_length);
  LS_TRACE("region_id                            = %02x\n", rcs->region_id);
  LS_TRACE("region_version_number    = %02x\n", rcs->region_version_number);
  LS_TRACE("region_fill_flag             = %02x\n", rcs->region_fill_flag);
  LS_TRACE("reserved                             = %02x\n", rcs->reserved1);
  LS_TRACE("region_width                     = %04x(%d)\n", rcs->region_width, rcs->region_width);
  LS_TRACE("region_height                    = %04x(%d)\n", rcs->region_height, rcs->region_height);
  LS_TRACE("region_level_of_compatibility = %02x\n", rcs->region_level_of_compatibility);
  LS_TRACE("region_depth                     = %02x\n", rcs->region_depth);
  LS_TRACE("reserved                             = %02x\n", rcs->reserved2);
  LS_TRACE("CLUT_id                                = %02x\n", rcs->CLUT_id);
  LS_TRACE("region_8-bit_pixel_code= %02x\n", rcs->region_8bit_pixel_code);
  LS_TRACE("region_4-bit_pixel_code= %02x\n", rcs->region_4bit_pixel_code);
  LS_TRACE("region_2-bit_pixel_code= %02x\n", rcs->region_2bit_pixel_code);
  LS_TRACE("reserved                             = %02x\n", rcs->reserved3);

  if (rcs->objectinfo_list)
  {
    LS_ListCount(rcs->objectinfo_list, &n);
    LS_TRACE("There are %d objects in rcs %p\n", n, (void*)rcs);

    for (i = 0; i < (int32_t)n; i++)
    {
      RCSObjectInfo* object = LS_ListNthNode(rcs->objectinfo_list, i);

      if (object)
      {
        LS_TRACE("%dth object (%p):\n", i, (void*)object);
        LS_TRACE("object_id                                 = %04x\n", object->object_id);
        LS_TRACE("object_type                             = %02x\n", object->object_type);
        LS_TRACE("object_provider_flag            = %02x\n", object->object_provider_flag);
        LS_TRACE("object_horizontal_position= %02x(%d)\n",
                 object->object_horizontal_position,
                 object->object_horizontal_position);
        LS_TRACE("reserved                                    = %02x\n", object->reserved);
        LS_TRACE("object_vertical_position    = %02x(%d)\n",
                 object->object_vertical_position,
                 object->object_vertical_position);

        if ((object->object_type == 0x01) ||
            (object->object_type == 0x02))
        {
          LS_TRACE("foreground_pixel_code     = %02x\n", object->foreground_pixel_code);
          LS_TRACE("background_pixel_code     = %02x\n", object->background_pixel_code);
        }
      }
    }
  }
}


LS_SegCDS*
SegmentNewCDS(LS_Service* service)
{
  LS_SegCDS* cds = NULL;
  int32_t errcode = 0;

  cds = (LS_SegCDS*)ServiceHeapMalloc(service, COMPOSITION_BUFFER, sizeof(LS_SegCDS));
  DEBUG_CHECK(cds != NULL);

  if (cds == NULL)
  {
    LS_ERROR("COMPOSITION_BUFFER,Memory allocation failed: %d bytes\n", sizeof(LS_SegCDS));
    return NULL;
  }

  /*init the regions list*/
  SYS_MEMSET((void*)cds, 0, sizeof(LS_SegCDS));
  cds->clutinfo_list = LS_ListInit(__listMallocFunc, __listFreeFunc, &errcode);
  DEBUG_CHECK(cds->clutinfo_list != NULL);

  if (cds->clutinfo_list == NULL)
  {
    LS_ERROR("LS_ERROR_GENERAL: list init failed\n");
    ServiceHeapFree(service, COMPOSITION_BUFFER, cds);
    return NULL;
  }

  LS_DEBUG("cds->clutinfo_list (%p,%p) malloced\n", (void*)cds, (void*)cds->clutinfo_list);
  return cds;
}


void
SegmentDeleteCDS(LS_Service* service, LS_SegCDS* cds)
{
  if (cds)
  {
    if (cds->clutinfo_list)
    {
      LS_ListEmptyData(cds->clutinfo_list, __deleteCDSClutInfo, (void*)service);
      LS_ListDestroy(cds->clutinfo_list);
    }

    SYS_MEMSET((void*)cds, 0, sizeof(LS_SegCDS));
    ServiceHeapFree(service, COMPOSITION_BUFFER, (void*)cds);
  }
}


int32_t
SegmentParseCDS(LS_Service* service, uint8_t* data, LS_SegCDS* cds)
{
  int32_t status = LS_OK;
  int32_t processed_length;

  if ((data == NULL) ||
      (cds == NULL))
  {
    LS_ERROR("LS_ERROR_GENERAL: Invalid input parameters\n");
    return LS_ERROR_GENERAL;
  }

  cds->CLUT_id = ReadBitStream32(data, 0, 0, 8, &status);
  cds->CLUT_version_number = ReadBitStream32(data, 1, 0, 4, &status);
  cds->reserved = ReadBitStream32(data, 1, 4, 4, &status);
  processed_length = 2;
  data += processed_length;

  while (processed_length < cds->segment_length)
  {
    CDSCLUTInfo* clutinfo = (CDSCLUTInfo*)ServiceHeapMalloc(service, COMPOSITION_BUFFER, sizeof(CDSCLUTInfo));

    DEBUG_CHECK(clutinfo != NULL);

    if (clutinfo == NULL)
    {
      LS_ERROR("COMPOSITION_BUFFER,Memory allocation failed: %d bytes\n",
               sizeof(CDSCLUTInfo));
      return LS_ERROR_GENERAL;
    }

    LS_DEBUG("CDSCLUTInfo* clutinfo = %p is malloced %d bytes in COMPOSITION_BUFFER\n",
             (void*)clutinfo,
             sizeof(CDSCLUTInfo));
    clutinfo->CLUT_entry_id = ReadBitStream32(data, 0, 0, 8, &status);
    clutinfo->two_bit_entry_CLUT_flag = ReadBitStream32(data, 1, 0, 1, &status);
    clutinfo->four_bit_entry_CLUT_flag = ReadBitStream32(data, 1, 1, 1, &status);
    clutinfo->eight_bit_entry_CLUT_flag = ReadBitStream32(data, 1, 2, 1, &status);
    clutinfo->reserved = ReadBitStream32(data, 1, 3, 4, &status);
    clutinfo->full_range_flag = ReadBitStream32(data, 1, 7, 1, &status);

    if (clutinfo->full_range_flag == 0x01)
    {
      clutinfo->Y_value = ReadBitStream32(data, 2, 0, 8, &status);
      clutinfo->Cr_value = ReadBitStream32(data, 3, 0, 8, &status);
      clutinfo->Cb_value = ReadBitStream32(data, 4, 0, 8, &status);
      clutinfo->T_value = ReadBitStream32(data, 5, 0, 8, &status);
    }
    else
    {
      clutinfo->Y_value = ReadBitStream32(data, 2, 0, 6, &status);
      clutinfo->Cr_value = ReadBitStream32(data, 2, 6, 4, &status);
      clutinfo->Cb_value = ReadBitStream32(data, 2, 10, 4, &status);
      clutinfo->T_value = ReadBitStream32(data, 2, 14, 2, &status);
    }

    LS_ListAppend(cds->clutinfo_list, (void*)clutinfo);
    LS_TRACE("Add a new clutinfo %p: id=%d,Y=%d,Cr=%d,Cb=%d,T=%d to cds->clutinfo_list(%p -> %p)\n",
             (void*)clutinfo,
             clutinfo->CLUT_entry_id,
             clutinfo->Y_value,
             clutinfo->Cr_value,
             clutinfo->Cb_value,
             clutinfo->T_value,
             (void*)cds,
             (void*)(cds->clutinfo_list));

    if (clutinfo->full_range_flag == 0x01)
    {
      data += 6;
      processed_length += 6;
    }
    else
    {
      data += 4;
      processed_length += 4;
    }
  }

  return LS_OK;
}


void
SegmentDumpCDS(LS_SegCDS* cds)
{
  int32_t i;
  uint32_t n;

  if (cds == NULL)
  {
    LS_INFO("SegmentDumpCDS(): cds = nill\n");
    return;
  }

  LS_TRACE("sync_byte                     = %02x\n", cds->sync_byte);
  LS_TRACE("segment_type                = %02x\n", cds->segment_type);
  LS_TRACE("page_id                         = %04x\n", cds->page_id);
  LS_TRACE("segment_length            = %04x\n", cds->segment_length);
  LS_TRACE("CLUT_id                         = %02x\n", cds->CLUT_id);
  LS_TRACE("CLUT_version_number = %02x\n", cds->CLUT_version_number);
  LS_TRACE("reserved                        = %02x\n", cds->reserved);

  if (cds->clutinfo_list)
  {
    LS_ListCount(cds->clutinfo_list, &n);
    LS_TRACE("There are %d CLUT in cds %p\n", n, (void*)cds);

    for (i = 0; i < (int32_t)n; i++)
    {
      CDSCLUTInfo* clut = LS_ListNthNode(cds->clutinfo_list, i);

      if (clut)
      {
        LS_TRACE("%dth CLUT (%p):\n", i, (void*)clut);
        LS_TRACE("CLUT_entry_id                 = %02x\n", clut->CLUT_entry_id);
        LS_TRACE("2-bit/entry_CLUT_flag = %02x\n", clut->two_bit_entry_CLUT_flag);
        LS_TRACE("4_bit/entry_CLUT_flag = %02x\n", clut->four_bit_entry_CLUT_flag);
        LS_TRACE("8_bit/entry_CLUT_flag = %02x\n", clut->eight_bit_entry_CLUT_flag);
        LS_TRACE("reserved                            = %02x\n", clut->reserved);
        LS_TRACE("full_range_flag             = %02x\n", clut->full_range_flag);
        LS_TRACE("Y_value                             = %02x\n", clut->Y_value);
        LS_TRACE("Cr_value                            = %02x\n", clut->Cr_value);
        LS_TRACE("Cb_value                            = %02x\n", clut->Cb_value);
        LS_TRACE("T_value                             = %02x\n\n", clut->T_value);
      }
    }
  }
}


LS_SegODS*
SegmentNewODS(LS_Service* service)
{
  LS_SegODS* ods = NULL;

  ods = (LS_SegODS*)ServiceHeapMalloc(service, COMPOSITION_BUFFER, sizeof(LS_SegODS));
  DEBUG_CHECK(ods != NULL);

  if (ods == NULL)
  {
    LS_ERROR("COMPOSITION_BUFFER,Memory allocation failed: %d bytes\n", sizeof(LS_SegODS));
    return NULL;
  }

  SYS_MEMSET((void*)ods, 0, sizeof(LS_SegODS));
  return ods;
}


void
SegmentDeleteODS(LS_Service* service, LS_SegODS* ods)
{
  if (ods)
  {
    if (ods->data.stringdata)
    {
      ServiceHeapFree(service, PIXEL_BUFFER, ods->data.stringdata);
    }

    SYS_MEMSET((void*)ods, 0, sizeof(LS_SegODS));
    ServiceHeapFree(service, COMPOSITION_BUFFER, (void*)ods);
  }
}


int32_t
SegmentParseODS(LS_Service* service, uint8_t* data, LS_SegODS* ods)
{
  int32_t status = LS_OK;

  if ((service == NULL) ||
      (data == NULL) ||
      (ods == NULL))
  {
    LS_ERROR("LS_ERROR_GENERAL: Invalid input parameters\n");
    return LS_ERROR_GENERAL;
  }

  ods->object_id = ReadBitStream32(data, 0, 0, 16, &status);
  ods->object_version_number = ReadBitStream32(data, 2, 0, 4, &status);
  ods->object_coding_method = ReadBitStream32(data, 2, 4, 2, &status);
  ods->non_modifying_colour_flag = ReadBitStream32(data, 2, 6, 1, &status);
  ods->reserved = ReadBitStream32(data, 2, 7, 1, &status);

  if (ods->object_coding_method == kCODING_OF_PIXELS)
  {
    ods->data.pixeldata = (ODSPixelData*)ServiceHeapMalloc(service, PIXEL_BUFFER, sizeof(ODSPixelData));
    DEBUG_CHECK(ods->data.pixeldata != NULL);

    if (ods->data.pixeldata)
    {
      SYS_MEMSET((void*)ods->data.pixeldata, 0, sizeof(ODSPixelData));
      __segODSParsePixelData(data + 3, ods->data.pixeldata);
    }
    else
    {
      LS_ERROR("PIXEL_BUFFER,Memory allocation failed: %d bytes\n", sizeof(ODSPixelData));
      return LS_ERROR_PIXEL_BUFFER;
    }
  }
  else if (ods->object_coding_method == kCODED_AS_A_STRING_OF_CHARACTERS)
  {
    ods->data.stringdata = (ODSStringData*)ServiceHeapMalloc(service, PIXEL_BUFFER, sizeof(ODSStringData));
    DEBUG_CHECK(ods->data.stringdata != NULL);

    if (ods->data.stringdata)
    {
      SYS_MEMSET((void*)ods->data.stringdata, 0, sizeof(ODSStringData));
      __segODSParseStringData(data + 3, ods->data.stringdata);
    }
    else
    {
      LS_ERROR("PIXEL_BUFFER,Memory allocation failed: %d bytes\n", sizeof(ODSStringData));
      return LS_ERROR_PIXEL_BUFFER;
    }
  }
  else
  {
    LS_ERROR("STREAM_DATA:object_coding_method<%02x> is reserved\n", ods->object_coding_method);
    return LS_ERROR_STREAM_DATA;
  }

  return LS_OK;
}


void
SegmentDumpODS(LS_SegODS* ods)
{
  if (ods == NULL)
  {
    return;
  }

  LS_TRACE("sync_byte = %02x\n", ods->sync_byte);
  LS_TRACE("segment_type = %02x\n", ods->segment_type);
  LS_TRACE("page_id = %04x\n", ods->page_id);
  LS_TRACE("segment_length = %04x\n", ods->segment_length);
  LS_TRACE("object_id = %04x\n", ods->object_id);
  LS_TRACE("object_version_number = %02x\n", ods->object_version_number);
  LS_TRACE("object_coding_method = %02x\n", ods->object_coding_method);
  LS_TRACE("non_modifying_colour_flag = %02x\n", ods->non_modifying_colour_flag);
  LS_TRACE("reserved = %02x\n", ods->reserved);

  if (ods->object_coding_method == kCODING_OF_PIXELS)
  {
    LS_TRACE("top_field_data_block_length = %04x(%d)\n",
             ods->data.pixeldata->top_field_data_block_length,
             ods->data.pixeldata->top_field_data_block_length);
    LS_TRACE("bottom_field_data_block_length = %04x(%d)\n",
             ods->data.pixeldata->bottom_field_data_block_length,
             ods->data.pixeldata->bottom_field_data_block_length);
  }
  else if (ods->object_coding_method == kCODED_AS_A_STRING_OF_CHARACTERS)
  {
    LS_TRACE("number_of_codes = %02x\n", ods->data.stringdata->number_of_codes);
    LS_TRACE("character_code(address) = %p\n", ods->data.stringdata->character_code);
  }
}


LS_SegDDS*
SegmentNewDDS(LS_Service* service)
{
  LS_SegDDS* dds = NULL;

  dds = (LS_SegDDS*)ServiceHeapMalloc(service, COMPOSITION_BUFFER, sizeof(LS_SegDDS));
  DEBUG_CHECK(dds != NULL);

  if (dds)
  {
    SYS_MEMSET((void*)dds, 0, sizeof(LS_SegDDS));
    return dds;
  }
  else
  {
    LS_ERROR("COMPOSITION_BUFFER,Memory allocation failed: %d bytes\n", sizeof(LS_SegDDS));
    return NULL;
  }
}


void
SegmentDeleteDDS(LS_Service* service, LS_SegDDS* dds)
{
  if (dds)
  {
    SYS_MEMSET((void*)dds, 0, sizeof(LS_SegDDS));
    ServiceHeapFree(service, COMPOSITION_BUFFER, (void*)dds);
  }
}


int32_t
SegmentParseDDS(LS_Service* service, uint8_t* data, LS_SegDDS* dds)
{
  int32_t status = LS_OK;
  (void)service;  /* Unused */

  if ((data == NULL) ||
      (dds == NULL))
  {
    LS_ERROR("LS_ERROR_GENERAL: Invalid input parameters\n");
    return LS_ERROR_GENERAL;
  }

  dds->dds_version_number = ReadBitStream32(data, 0, 0, 4, &status);
  dds->display_window_flag = ReadBitStream32(data, 0, 4, 1, &status);
  dds->reserved = ReadBitStream32(data, 0, 5, 3, &status);
  dds->display_width = ReadBitStream32(data, 1, 0, 16, &status);
  dds->display_width += 1;
  dds->display_height = ReadBitStream32(data, 3, 0, 16, &status);
  dds->display_height += 1;

  if (dds->display_window_flag == 1)
  {
    dds->display_window_horizontal_position_minimum = ReadBitStream32(data, 5, 0, 16, &status);
    dds->display_window_horizontal_position_maximum = ReadBitStream32(data, 7, 0, 16, &status);
    dds->display_window_vertical_position_minimum = ReadBitStream32(data, 9, 0, 16, &status);
    dds->display_window_vertical_position_maximum = ReadBitStream32(data, 11, 0, 16, &status);
  }

  return LS_OK;
}


void
SegmentDumpDDS(LS_SegDDS* dds)
{
  if (dds == NULL)
  {
    return;
  }

  LS_TRACE("sync_byte           = 0x%02x\n", dds->sync_byte);
  LS_TRACE("segment_type        = 0x%02x\n", dds->segment_type);
  LS_TRACE("page_id             = 0x%04x\n", dds->page_id);
  LS_TRACE("dds_version_number  = 0x%02x\n", dds->dds_version_number);
  LS_TRACE("display_window_flag = 0x%02x\n", dds->display_window_flag);
  LS_TRACE("reserved            = 0x%02x\n", dds->reserved);
  LS_TRACE("display_width       = 0x%04x(%d)\n", dds->display_width, dds->display_width);
  LS_TRACE("display_height      = 0x%04x(%d)\n", dds->display_height, dds->display_height);

  if (dds->display_window_flag == 1)
  {
    LS_TRACE("display_window_horizontal_position_minimum = %04x(%d)\n",
             dds->display_window_horizontal_position_minimum,
             dds->display_window_horizontal_position_minimum);
    LS_TRACE("display_window_horizontal_position_maximum = %04x(%d)\n",
             dds->display_window_horizontal_position_maximum,
             dds->display_window_horizontal_position_maximum);
    LS_TRACE("display_window_vertical_position_minimum   = %04x(%d)\n",
             dds->display_window_vertical_position_minimum,
             dds->display_window_vertical_position_minimum);
    LS_TRACE("display_window_vertical_position_maximum   = %04x(%d)\n",
             dds->display_window_vertical_position_maximum,
             dds->display_window_vertical_position_maximum);
  }
}


LS_SegEDS*
SegmentNewEDS(LS_Service* service)
{
  LS_SegEDS* eds = NULL;

  eds = (LS_SegEDS*)ServiceHeapMalloc(service, COMPOSITION_BUFFER, sizeof(LS_SegEDS));
  DEBUG_CHECK(eds != NULL);

  if (eds)
  {
    SYS_MEMSET((void*)eds, 0, sizeof(LS_SegEDS));
    return eds;
  }
  else
  {
    LS_ERROR("COMPOSITION_BUFFER,Memory allocation failed: %d bytes\n", sizeof(LS_SegEDS));
    return NULL;
  }
}


void
SegmentDeleteEDS(LS_Service* service, LS_SegEDS* eds)
{
  if (eds)
  {
    SYS_MEMSET((void*)eds, 0, sizeof(LS_SegEDS));
    ServiceHeapFree(service, COMPOSITION_BUFFER, (void*)eds);
  }
}


int32_t
SegmentParseEDS(LS_Service* service, uint8_t* data, LS_SegEDS* eds)
{
  (void)service;  /* Not used for EDS parsing */

  if ((data == NULL) ||
      (eds == NULL))
  {
    LS_ERROR("LS_ERROR_GENERAL: Invalid input parameters\n");
    return LS_ERROR_GENERAL;
  }

  /* Parse the standard segment header (first 6 bytes) */
  eds->sync_byte = data[0];
  eds->segment_type = data[1];
  eds->page_id = (data[2] << 8) | data[3];
  eds->segment_length = (data[4] << 8) | data[5];

  /* EDS segment has only 2 bytes of stuffing after the header */
  if (eds->segment_length > 2)
  {
    LS_WARNING("EDS segment length > 2 (got %d), extra data ignored\n", eds->segment_length);
  }

  eds->stuff[0] = (eds->segment_length >= 1) ? data[6] : 0xFF;
  eds->stuff[1] = (eds->segment_length >= 2) ? data[7] : 0xFF;

  LS_TRACE("Parsed EDS: sync_byte=%02x, segment_type=%02x, page_id=%04x, length=%d\n",
           eds->sync_byte, eds->segment_type, eds->page_id, eds->segment_length);

  return LS_OK;
}


void
SegmentDumpEDS(LS_SegEDS* eds)
{
  if (eds == NULL)
  {
    return;
  }

  LS_TRACE("sync_byte = %02x\n", eds->sync_byte);
  LS_TRACE("segment_type = %02x\n", eds->segment_type);
  LS_TRACE("page_id = %04x\n", eds->page_id);
  LS_TRACE("segment_length = %04x\n", eds->segment_length);
}


LS_SegDSS*
SegmentNewDSS(LS_Service* service)
{
  LS_SegDSS* dss = NULL;

  dss = (LS_SegDSS*)ServiceHeapMalloc(service, COMPOSITION_BUFFER, sizeof(LS_SegDSS));
  DEBUG_CHECK(dss != NULL);

  if (dss == NULL)
  {
    LS_ERROR("COMPOSITION_BUFFER,Memory allocation failed: %d bytes\n", sizeof(LS_SegDSS));
    return NULL;
  }

  SYS_MEMSET((void*)dss, 0, sizeof(LS_SegDSS));
  return dss;
}


void
SegmentDeleteDSS(LS_Service* service, LS_SegDSS* dss)
{
  int32_t status = LS_OK;
  uint32_t number;
  int32_t i;
  DSSRegionInfo* region = NULL;

  if (dss)
  {
    if (dss->disparity_shift_update_sequence)
    {
      __segDSSDeleteDSSDisparityShiftUpdateSequence(service, dss->disparity_shift_update_sequence);
      dss->disparity_shift_update_sequence = NULL;
    }

    if (dss->regions)
    {
      status = LS_ListCount(dss->regions, &number);
      DEBUG_CHECK(status == LS_OK);

      for (i = 0; i < (int32_t)number; i++)
      {
        region = LS_ListFirstData(dss->regions);
        __SegDSSDeleteDSSRegionInfo(service, region);
      }

      status = LS_ListDestroy(dss->regions);
      DEBUG_CHECK(status);

      if (status != LS_OK)
      {
        LS_ERROR("LS_ERROR_GENERAL: destroy list failed\n");
      }
    }

    SYS_MEMSET((void*)dss, 0, sizeof(LS_SegDSS));
    ServiceHeapFree(service, COMPOSITION_BUFFER, (void*)dss);
  }
}


int32_t
SegmentParseDSS(LS_Service* service, uint8_t* data, LS_SegDSS* dss)
{
  int32_t status = LS_OK;
  int32_t processed_length = 0;
  uint32_t sequencelen;

  if ((data == NULL) ||
      (dss == NULL))
  {
    return LS_ERROR_GENERAL;
  }

  dss->dss_version_number = ReadBitStream32(data, 0, 0, 4, &status);
  dss->disparity_shift_update_sequence_page_flag = ReadBitStream32(data, 0, 4, 1, &status);
  dss->reserved = ReadBitStream32(data, 0, 5, 3, &status);
  dss->page_default_disparity_shift = ReadBitStream32(data, 1, 0, 8, &status);
  processed_length += 2;

  if (dss->disparity_shift_update_sequence_page_flag == 1)
  {
    DSSDisparityShiftUpdateSequence* sequence = __segDSSNewDSSDisparityShiftUpdateSequence(service);

    DEBUG_CHECK(sequence != NULL);

    if (sequence == NULL)
    {
      LS_ERROR("LS_ERROR_GENERAL:cannot create a new sequence!\n");
      return LS_ERROR_GENERAL;
    }

    status = __segDSSParseDisparityShiftUpdateSequence(service, data, sequence, &sequencelen);

    if (status == LS_OK)
    {
      processed_length += sequencelen;
      dss->disparity_shift_update_sequence = sequence;
    }
    else
    {
      LS_ERROR("LS_ERROR_GENERAL: update sequence failed\n");
      return LS_ERROR_GENERAL;
    }
  }

  /*move the data pointer to "region_id"*/
  while (processed_length < dss->segment_length)
  {
    DSSRegionInfo* regioninfo = __SegDSSNewDSSRegionInfo(service);

    if (regioninfo == NULL)
    {
      LS_ERROR("memory allocation failed\n");
      return LS_ERROR_GENERAL;
    }

    status = __SegDSSParseDSSRegionInfo(service, data + processed_length, regioninfo, &sequencelen);

    if (status == LS_OK)
    {
      processed_length += sequencelen;
      LS_ListAppend(dss->regions, (void*)regioninfo);
    }
    else
    {
      LS_ERROR("LS_ERROR_GENERAL: parse DSS region info failed\n");
      return LS_ERROR_GENERAL;
    }
  }

  return LS_OK;
}


void
SegmentDumpDSS(LS_SegDSS* dss)
{
  int32_t i;
  uint32_t n;

  if (dss == NULL)
  {
    LS_INFO("SegmentDumpDSS(): dss = null\n");
    return;
  }

  LS_TRACE("sync_byte                                 = %02x\n", dss->sync_byte);
  LS_TRACE("segment_type                              = %02x\n", dss->segment_type);
  LS_TRACE("page_id                                   = %04x\n", dss->page_id);
  LS_TRACE("segment_length                            = %04x\n", dss->segment_length);
  LS_TRACE("dss_version_number                        = %02x\n", dss->dss_version_number);
  LS_TRACE("disparity_shift_update_sequence_page_flag = %02x\n", dss->disparity_shift_update_sequence_page_flag);
  LS_TRACE("reserved                                  = %02x\n", dss->reserved);
  LS_TRACE("page_default_disparity_shift              = %02x\n", dss->page_default_disparity_shift);

  if (dss->disparity_shift_update_sequence != NULL)
  {
    DSSDisparityShiftUpdateSequence* seq = dss->disparity_shift_update_sequence;
    LS_TRACE("disparity_shift_update_sequence:\n");
    LS_TRACE("  interval_duration      = %u\n", seq->interval_duration);
    LS_TRACE("  update_sequence_length = %d\n", seq->disparity_shift_update_sequence_length);
    LS_TRACE("  division_period_count  = %d\n", seq->division_period_count);

    if (seq->division_periods != NULL)
    {
      LS_ListCount(seq->division_periods, &n);
      LS_TRACE("  There are %d division periods\n", n);

      for (i = 0; i < (int32_t)n; i++)
      {
        DSSDivisionPeriod* period = LS_ListNthNode(seq->division_periods, i);
        if (period != NULL)
        {
          LS_TRACE("  [%d] interval_count = %d, disparity_shift_update_integer_part = %d\n",
                   i, period->interval_count, period->disparity_shift_update_integer_part);
        }
      }
    }
  }

  if (dss->regions != NULL)
  {
    LS_ListCount(dss->regions, &n);
    LS_TRACE("There are %d regions in dss %p\n", n, (void*)dss);

    for (i = 0; i < (int32_t)n; i++)
    {
      DSSRegionInfo* region = LS_ListNthNode(dss->regions, i);
      if (region != NULL)
      {
        LS_TRACE("  [%d] region_id = %d, update_seq_flag = %d, num_subregions = %d\n",
                 i, region->region_id, region->disparity_shift_update_sequence_region_flag,
                 region->number_of_subregions_minus_1 + 1);

        if (region->sub_region_info != NULL)
        {
          uint32_t j, m;
          LS_ListCount(region->sub_region_info, &m);
          LS_TRACE("      There are %d sub_regions\n", m);

          for (j = 0; j < m; j++)
          {
            DSSSubRegionInfo* sub = LS_ListNthNode(region->sub_region_info, j);
            if (sub != NULL)
            {
              LS_TRACE("        [%u] pos=%d, width=%d, shift_int=%d, shift_frac=%d\n",
                       j, sub->subregion_horizontal_position, sub->subregion_width,
                       sub->subregion_disparity_shift_integer_part,
                       sub->subregion_disparity_shift_fractional_part);
            }
          }
        }
      }
    }
  }
}


CDSCLUTInfo*
CDSFindEntryByID(LS_SegCDS* cds, uint8_t entry_id)
{
  CDSCLUTInfo* entry = NULL;

  if ((cds == NULL) ||
      (cds->clutinfo_list == NULL))
  {
    return LS_ERROR_GENERAL;
  }

  entry = LS_ListFindUserNode(cds->clutinfo_list, (void*)((intptr_t)entry_id), __CDSEntryCompareFunc);
  return entry;
}


LS_Displayset*
LS_DisplaysetNew(LS_Service* service, int32_t* status)
{
  LS_Displayset* displayset = NULL;
  int32_t errcode = 0;

  displayset = (LS_Displayset*)ServiceHeapMalloc(service, COMPOSITION_BUFFER, sizeof(LS_Displayset));
  DEBUG_CHECK(displayset != NULL);

  if (displayset == NULL)
  {
    LS_ERROR("COMPOSITION_BUFFER,Memory allocation failed: %d bytes\n", sizeof(LS_Displayset));
    *status = LS_ERROR_COMPOSITION_BUFFER;
    return NULL;
  }

  SYS_MEMSET((void*)displayset, 0, sizeof(LS_Displayset));
  displayset->magic_id = DISPLAYSET_MAGIC_NUMBER;
  /*init RCS list for multiple RCS in this displayset*/
  displayset->rcs = LS_ListInit(__listMallocFunc, __listFreeFunc, &errcode);
  DEBUG_CHECK(displayset->rcs != NULL);

  if (displayset->rcs == NULL)
  {
    LS_ERROR("LS_ERROR_GENERAL: list init failed\n");
    SYS_MEMSET((void*)displayset, 0, sizeof(LS_Displayset));
    ServiceHeapFree(service, COMPOSITION_BUFFER, (void*)displayset);
    *status = LS_ERROR_GENERAL;
    return NULL;
  }

  LS_DEBUG("displayset(%p)->rcs(%p) created\n", (void*)displayset, (void*)(displayset->rcs));
  /*init CDS list for multiple CDS in this displayset*/
  displayset->cds = LS_ListInit(__listMallocFunc, __listFreeFunc, &errcode);
  DEBUG_CHECK(displayset->cds != NULL);

  if (displayset->cds == NULL)
  {
    LS_ERROR("LS_ERROR_GENERAL: list init failed\n");
    LS_ListDestroy(displayset->rcs);
    SYS_MEMSET((void*)displayset, 0, sizeof(LS_Displayset));
    ServiceHeapFree(service, COMPOSITION_BUFFER, (void*)displayset);
    *status = LS_ERROR_GENERAL;
    return NULL;
  }

  LS_DEBUG("displayset(%p)->cds(%p) created\n", (void*)displayset, (void*)(displayset->cds));
  /*init ODS list for multiple ODS in this displayset*/
  displayset->ods = LS_ListInit(__listMallocFunc, __listFreeFunc, &errcode);
  DEBUG_CHECK(displayset->ods != NULL);

  if (displayset->ods == NULL)
  {
    LS_ERROR("LS_ERROR_GENERAL: list init failed\n");
    LS_ListDestroy(displayset->rcs);
    LS_ListDestroy(displayset->cds);
    SYS_MEMSET((void*)displayset, 0, sizeof(LS_Displayset));
    ServiceHeapFree(service, COMPOSITION_BUFFER, (void*)displayset);
    *status = LS_ERROR_GENERAL;
    return NULL;
  }

  LS_DEBUG("displayset(%p)->ods(%p) created\n", (void*)displayset, (void*)(displayset->ods));
  /* seems everything is fine now, return it ... ... */
  LS_DEBUG("displayset %p created\n", (void*)displayset);
  *status = LS_OK;
  return displayset;
}


void
LS_DisplaysetDelete(LS_Service* service, LS_Displayset* displayset)
{
  LS_SegRCS* rcs = NULL;
  LS_SegCDS* cds = NULL;
  LS_SegODS* ods = NULL;

  if ((displayset == NULL) ||
      (service == NULL))
  {
    LS_DEBUG("nill service or nill displayset,return...\n");
    return;
  }

  LS_DEBUG("Deleting Displayset %p...\n", (void*)displayset);

  if (displayset->magic_id != DISPLAYSET_MAGIC_NUMBER)
  {
    LS_ERROR("displayset %p corrupted: bad magic number\n", (void*)displayset);
    return;
  }

  if (displayset->pcs)
  {
    SegmentDeletePCS(service, displayset->pcs);
  }

  if (displayset->rcs)
  {
    rcs = LS_ListFirstData(displayset->rcs);

    while (rcs)
    {
      SegmentDeleteRCS(service, rcs);
      rcs = LS_ListFirstData(displayset->rcs);
    }

    LS_ListDestroy(displayset->rcs);
  }

  if (displayset->cds)
  {
    cds = LS_ListFirstData(displayset->cds);

    while (cds)
    {
      SegmentDeleteCDS(service, cds);
      cds = LS_ListFirstData(displayset->cds);
    }

    LS_ListDestroy(displayset->cds);
  }

  if (displayset->ods)
  {
    ods = LS_ListFirstData(displayset->ods);

    while (ods)
    {
      SegmentDeleteODS(service, ods);
      ods = LS_ListFirstData(displayset->ods);
    }

    LS_ListDestroy(displayset->ods);
  }

  if (displayset->dds)
  {
    SegmentDeleteDDS(service, displayset->dds);
  }

  if (displayset->dss)
  {
    SegmentDeleteDSS(service, displayset->dss);
  }

  if (displayset->eds)
  {
    SegmentDeleteEDS(service, displayset->eds);
  }

  /* Save pointer for logging before free */
  void* displayset_ptr = displayset;

  SYS_MEMSET((void*)displayset, 0, sizeof(LS_Displayset));
  ServiceHeapFree(service, COMPOSITION_BUFFER, displayset);
  LS_DEBUG("Displayset %p deleted\n", (void*)displayset_ptr);
}


LS_SegCDS*
LS_DisplaysetFindCDSByID(LS_Displayset* displayset, uint16_t clut_id)
{
  if (displayset == NULL)
  {
    return NULL;
  }

  if (displayset->cds)
  {
    LS_TRACE("Searching CDS for clut_id = %d ...\n", clut_id);
    return LS_ListFindUserNode(displayset->cds, (void*)((intptr_t)clut_id), __compareCDSByID);
  }
  else
  {
    return NULL;
  }
}


LS_SegRCS*
DisplaysetFindRCSByID(LS_Displayset* displayset, uint16_t region_id)
{
  if (displayset == NULL)
  {
    return NULL;
  }

  if (displayset->rcs)
  {
    LS_TRACE("Searching RCS for region_id = %d ...\n", region_id);
    return LS_ListFindUserNode(displayset->rcs, (void*)((intptr_t)region_id), __compareRCSByID);
  }
  else
  {
    return NULL;
  }
}


LS_SegODS*
DisplaysetFindODSByID(LS_Displayset* displayset, uint16_t object_id)
{
  if (displayset == NULL)
  {
    return NULL;
  }

  if (displayset->ods)
  {
    LS_TRACE("Searching ODS for object_id = %d ...\n", object_id);
    return LS_ListFindUserNode(displayset->ods, (void*)((intptr_t)object_id), __compareODSByID);
  }
  else
  {
    return NULL;
  }
}


PCSRegionInfo*
DisplaysetFindPCSReginInfoByRegionID(LS_Displayset* displayset, uint16_t region_id)
{
  PCSRegionInfo* region_info = NULL;

  if ((displayset == NULL) ||
      (displayset->pcs == NULL) ||
      (displayset->pcs->regioninfos == NULL))
  {
    return NULL;
  }

  region_info = LS_ListFindUserNode(displayset->pcs->regioninfos,
                                    (void*)((intptr_t)region_id),
                                    __comparePCSRegionInfobyID);
  return region_info;
}
