/*-----------------------------------------------------------------------------
 * lssubsegparser.h
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
 * @file lssubsegparser.h
 * @brief DVB Subtitle Segment Parser
 *
 * This header provides functions for parsing DVB subtitle segments
 * according to ETSI EN 300 743 standard.
 */

#ifndef __LS_SEG_PARSCER_H__
#define __LS_SEG_PARSCER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "lssubdecoder.h"
#include "lssuben300743.h"
/*---------------------------------------------------------------------------
 * typedefs
 *--------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------
 * structs
 *--------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------
 * API prototype
 *--------------------------------------------------------------------------*/
/**
 * @brief Parse segment header
 *
 * Parses the common header of all DVB subtitle segments.
 *
 * @param data Segment data buffer
 * @param header Pointer to receive parsed header
 * @return LS_OK (1) on success, error code on failure
 */
int32_t SegmentParseHeader(uint8_t* data, LS_SegHeader* header);

/**
 * @brief Parse PCS segment
 *
 * Parses a Page Composition Segment.
 *
 * @param service Service instance
 * @param data Segment data buffer
 * @param pcs Pointer to receive parsed PCS
 * @return LS_OK (1) on success, error code on failure
 */
int32_t SegmentParsePCS(LS_Service* service, uint8_t* data, LS_SegPCS* pcs);

/**
 * @brief Parse RCS segment
 *
 * Parses a Region Composition Segment.
 *
 * @param service Service instance
 * @param data Segment data buffer
 * @param rcs Pointer to receive parsed RCS
 * @return LS_OK (1) on success, error code on failure
 */
int32_t SegmentParseRCS(LS_Service* service, uint8_t* data, LS_SegRCS* rcs);

/**
 * @brief Parse CDS segment
 *
 * Parses a CLUT Definition Segment.
 *
 * @param service Service instance
 * @param data Segment data buffer
 * @param cds Pointer to receive parsed CDS
 * @return LS_OK (1) on success, error code on failure
 */
int32_t SegmentParseCDS(LS_Service* service, uint8_t* data, LS_SegCDS* cds);

/**
 * @brief Parse ODS segment
 *
 * Parses an Object Data Segment.
 *
 * @param service Service instance
 * @param data Segment data buffer
 * @param ods Pointer to receive parsed ODS
 * @return LS_OK (1) on success, error code on failure
 */
int32_t SegmentParseODS(LS_Service* service, uint8_t* data, LS_SegODS* ods);

/**
 * @brief Parse DDS segment
 *
 * Parses a Display Definition Segment.
 *
 * @param service Service instance
 * @param data Segment data buffer
 * @param dds Pointer to receive parsed DDS
 * @return LS_OK (1) on success, error code on failure
 */
int32_t SegmentParseDDS(LS_Service* service, uint8_t* data, LS_SegDDS* dds);

/**
 * @brief Parse EDS segment
 *
 * Parses an End of Display Set Segment.
 *
 * @param service Service instance
 * @param data Segment data buffer
 * @param eds Pointer to receive parsed EDS
 * @return LS_OK (1) on success, error code on failure
 */
int32_t SegmentParseEDS(LS_Service* service, uint8_t* data, LS_SegEDS* eds);

/**
 * @brief Parse DSS segment
 *
 * Parses a Disparity Signalling Segment.
 *
 * @param service Service instance
 * @param data Segment data buffer
 * @param dss Pointer to receive parsed DSS
 * @return LS_OK (1) on success, error code on failure
 */
int32_t SegmentParseDSS(LS_Service* service, uint8_t* data, LS_SegDSS* dss);

/**
 * @brief Create new PCS structure
 *
 * Allocates and initializes a PCS structure.
 *
 * @param service Service instance
 * @return New PCS or NULL on failure
 */
LS_SegPCS* SegmentNewPCS(LS_Service* service);

/**
 * @brief Delete PCS structure
 *
 * Releases a PCS structure.
 *
 * @param service Service instance
 * @param pcs PCS to delete
 */
void SegmentDeletePCS(LS_Service* service, LS_SegPCS* pcs);

/**
 * @brief Create new RCS structure
 *
 * Allocates and initializes an RCS structure.
 *
 * @param service Service instance
 * @return New RCS or NULL on failure
 */
LS_SegRCS* SegmentNewRCS(LS_Service* service);

/**
 * @brief Delete RCS structure
 *
 * Releases an RCS structure.
 *
 * @param service Service instance
 * @param rcs RCS to delete
 */
void SegmentDeleteRCS(LS_Service* service, LS_SegRCS* rcs);

/**
 * @brief Create new CDS structure
 *
 * Allocates and initializes a CDS structure.
 *
 * @param service Service instance
 * @return New CDS or NULL on failure
 */
LS_SegCDS* SegmentNewCDS(LS_Service* service);

/**
 * @brief Delete CDS structure
 *
 * Releases a CDS structure.
 *
 * @param service Service instance
 * @param cds CDS to delete
 */
void SegmentDeleteCDS(LS_Service* service, LS_SegCDS* cds);

/**
 * @brief Create new ODS structure
 *
 * Allocates and initializes an ODS structure.
 *
 * @param service Service instance
 * @return New ODS or NULL on failure
 */
LS_SegODS* SegmentNewODS(LS_Service* service);

/**
 * @brief Delete ODS structure
 *
 * Releases an ODS structure.
 *
 * @param service Service instance
 * @param ods ODS to delete
 */
void SegmentDeleteODS(LS_Service* service, LS_SegODS* ods);

/**
 * @brief Create new DDS structure
 *
 * Allocates and initializes a DDS structure.
 *
 * @param service Service instance
 * @return New DDS or NULL on failure
 */
LS_SegDDS* SegmentNewDDS(LS_Service* service);

/**
 * @brief Delete DDS structure
 *
 * Releases a DDS structure.
 *
 * @param service Service instance
 * @param dds DDS to delete
 */
void SegmentDeleteDDS(LS_Service* service, LS_SegDDS* dds);

/**
 * @brief Create new EDS structure
 *
 * Allocates and initializes an EDS structure.
 *
 * @param service Service instance
 * @return New EDS or NULL on failure
 */
LS_SegEDS* SegmentNewEDS(LS_Service* service);

/**
 * @brief Delete EDS structure
 *
 * Releases an EDS structure.
 *
 * @param service Service instance
 * @param eds EDS to delete
 */
void SegmentDeleteEDS(LS_Service* service, LS_SegEDS* eds);

/**
 * @brief Create new DSS structure
 *
 * Allocates and initializes a DSS structure.
 *
 * @param service Service instance
 * @return New DSS or NULL on failure
 */
LS_SegDSS* SegmentNewDSS(LS_Service* service);

/**
 * @brief Delete DSS structure
 *
 * Releases a DSS structure.
 *
 * @param service Service instance
 * @param dss DSS to delete
 */
void SegmentDeleteDSS(LS_Service* service, LS_SegDSS* dss);

/**
 * @brief Dump segment header
 *
 * Prints segment header for debugging.
 *
 * @param header Segment header to dump
 */
void SegmentDumpHeader(LS_SegHeader* header);

/**
 * @brief Dump PCS
 *
 * Prints PCS data for debugging.
 *
 * @param pcs PCS to dump
 */
void SegmentDumpPCS(LS_SegPCS* pcs);

/**
 * @brief Dump RCS
 *
 * Prints RCS data for debugging.
 *
 * @param rcs RCS to dump
 */
void SegmentDumpRCS(LS_SegRCS* rcs);

/**
 * @brief Dump CDS
 *
 * Prints CDS data for debugging.
 *
 * @param cds CDS to dump
 */
void SegmentDumpCDS(LS_SegCDS* cds);

/**
 * @brief Dump ODS
 *
 * Prints ODS data for debugging.
 *
 * @param ods ODS to dump
 */
void SegmentDumpODS(LS_SegODS* ods);

/**
 * @brief Dump DDS
 *
 * Prints DDS data for debugging.
 *
 * @param dds DDS to dump
 */
void SegmentDumpDDS(LS_SegDDS* dds);

/**
 * @brief Dump EDS
 *
 * Prints EDS data for debugging.
 *
 * @param eds EDS to dump
 */
void SegmentDumpEDS(LS_SegEDS* eds);

/**
 * @brief Dump DSS
 *
 * Prints DSS data for debugging.
 *
 * @param dss DSS to dump
 */
void SegmentDumpDSS(LS_SegDSS* dss);

/**
 * @brief Find CDS entry by ID
 *
 * Searches a CDS for a CLUT entry with the specified ID.
 *
 * @param cds CDS to search
 * @param entry_id CLUT entry ID to find
 * @return CLUT entry or NULL if not found
 */
CDSCLUTInfo* CDSFindEntryByID(LS_SegCDS* cds, uint8_t entry_id);

/**
 * @brief Create new display set
 *
 * Allocates and initializes a display set structure.
 *
 * @param service Service instance
 * @param status Pointer to receive status
 * @return New display set or NULL on failure
 */
LS_Displayset* LS_DisplaysetNew(LS_Service* service, int32_t* status);

/**
 * @brief Delete display set
 *
 * Releases a display set structure.
 *
 * @param service Service instance
 * @param displayset Display set to delete
 */
void LS_DisplaysetDelete(LS_Service* service, LS_Displayset* displayset);

/**
 * @brief Find CDS by ID in display set
 *
 * Searches a display set for a CDS with the specified CLUT ID.
 *
 * @param displayset Display set to search
 * @param clut_id CLUT ID to find
 * @return CDS or NULL if not found
 */
LS_SegCDS* LS_DisplaysetFindCDSByID(LS_Displayset* displayset, uint16_t clut_id);

/**
 * @brief Find RCS by ID in display set
 *
 * Searches a display set for an RCS with the specified region ID.
 *
 * @param displayset Display set to search
 * @param region_id Region ID to find
 * @return RCS or NULL if not found
 */
LS_SegRCS* DisplaysetFindRCSByID(LS_Displayset* displayset, uint16_t region_id);

/**
 * @brief Find ODS by ID in display set
 *
 * Searches a display set for an ODS with the specified object ID.
 *
 * @param displayset Display set to search
 * @param object_id Object ID to find
 * @return ODS or NULL if not found
 */
LS_SegODS* DisplaysetFindODSByID(LS_Displayset* displayset, uint16_t object_id);

/**
 * @brief Find PCS region info by region ID
 *
 * Searches a display set for PCS region info with the specified region ID.
 *
 * @param displayset Display set to search
 * @param region_id Region ID to find
 * @return PCS region info or NULL if not found
 */
PCSRegionInfo* DisplaysetFindPCSReginInfoByRegionID(LS_Displayset* displayset, uint16_t region_id);

#ifdef __cplusplus
}
#endif


#endif /*__LS_SEGMENT_H*/
