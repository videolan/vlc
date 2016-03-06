/*****************************************************************************
 * ts_strings.h : Descriptions for TS known values
 *****************************************************************************
 * Copyright (C) 2015-2016 - VideoLAN Authors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/
#ifndef VLC_TS_STRINGS
#define VLC_TS_STRINGS

static const char * const ISO13818_1_other_descs[] =
{
    "ISO/IEC 13818-1 Reserved",
    "User Private",
};

static const char * const ISO13818_1_streamstypes_descs[] =
{
    "ISO/IEC Reserved",
    "ISO/IEC 11172 Video",
    "ISO/IEC 13818-2 Video or ISO/IEC 11172-2 constrained parameter video stream",
    "ISO/IEC 11172 Audio",
    "ISO/IEC 13818-3 Audio",
    "ISO/IEC 13818-1 private_sections",
    "ISO/IEC 13818-1 PES packets containing private data",
    "ISO/IEC 13522 MHEG",
    "ISO/IEC 13818-1 Annex A DSM CC",
    /* ^ 0x08 */
    "ITU-T Rec. H.222.1",
    "ISO/IEC 13818-6 type A",
    "ISO/IEC 13818-6 type B",
    "ISO/IEC 13818-6 type C",
    "ISO/IEC 13818-6 type D",
    "ISO/IEC 13818-1 auxiliary",
    "ISO/IEC 13818-7 Audio with ADTS transport",
    /* ^ 0x0F */
    "ISO/IEC 14496-2 Visual",
    "ISO/IEC 14496-3 Audio with LATM transport",
    "ISO/IEC 14496-1 SL-packetized or FlexMux stream carried in PES packets",
    "ISO/IEC 14496-1 SL-packetized or FlexMux stream carried in sections",
    "ISO/IEC 13818-6 Synchronized download protocol",
    "Metadata carried in PES packets",
    "Metadata carried in metadata_sections",
    "Metadata carried in ISO/IEC 13818-6 Data Carousel",
    "Metadata carried in ISO/IEC 13818-6 Object Carousel",
    "Metadata carried in ISO/IEC 13818-6 Synchronized download protocol",
    /* ^ 0x19 */
    "MPEG-2 IPMP Stream",
    "AVC video stream as defined in ITU-T Rec. H.264",
    "ISO/IEC 14496-3 Audio",
    "ISO/IEC 14496-17 Text",
    "ISO/IEC 23002-3 auxiliary video stream",
    "SVC video sub-stream as defined in ITU-T H.264 Annex G",
    /* ^ 0x1F */
    "MVC video sub-stream as defined in ITU-T H.264 Annex H",
    "Video stream conforming to one or more profiles as defined in ITU-T T.800",
    "Additional 3D View ITU-T H.262",
    "Additional 3D View ITU-T H.264",
    /* ^ 0x23 */
};

static const char *ISO13818_1_Get_StreamType_Description(uint8_t i_type)
{
    if( i_type <= 0x23 )
        return ISO13818_1_streamstypes_descs[i_type];
    else if (i_type >= 0x0F && i_type < 0x7F)
        return ISO13818_1_streamstypes_descs[0];
    else if( i_type == 0x7F )
        return ISO13818_1_streamstypes_descs[0x1A];
    else
        return ISO13818_1_other_descs[1];
}

static const char * const ISO13818_1_descriptors_descs[] =
{
    "Reserved",
    "Forbidden",
    "Video Stream",
    "Audio Stream",
    "Hierarchy",
    "Registration",
    "Data Stream Alignment",
    "Target Background Grid",
    "Video Window",
    /* ^ 0x08 */
    "CA",
    "ISO 639 Language",
    "System Clock",
    "Multiplex Buffer Utilization",
    "Copyright",
    "Maximum Bitrate",
    "Private Data Indicator",
    /* ^ 0x0F */
    "Smoothing Buffer",
    "STD",
    "IBP",
    "DSM CC Carousel Identifier",
    "DSM CC Association Tag",
    "DSM CC Deferred Association Tag",
    "DSM CC Reserved",
    "DSM CC NPT Reference",
    "DSM CC NPT Endpoint",
    "DSM CC Stream Mode",
    /* ^ 0x19 */
    "DSM CC Stream Event",
    "MPEG-4 Video",
    "MPEG-4 Audio",
    "IOD",
    "SL",
    "FMC",
    /* ^ 0x1F */
    "External ES ID",
    "MuxCode",
    "FmxBufferSize",
    "MultiplexBuffer",
    /* ^ 0x23 */
    "Content Labeling",
    "Metadata Pointer",
    "Metadata",
    "Metadata STD",
    "AVC Video",
    "IPMP",
    "AVC Timing and HRD",
    "MPEG-2 AAC Audio",
    "FlexMux Timing",
    /* ^ 0x2C */
    "MPEG-4 Text",
    "MPEG-4 Audio Extension",
    "Auxiliary Video Stream",
    "SVC Extension",
    "MVC Extension",
    "J2K Video",
    "MVC Operation Point",
    "MPEG-2 Stereoscopic Video Format",
    "Stereoscopic Program Info",
    "Stereoscopic Video Info",
    /* ^ 0x36 */
};

static const char *ISO13818_1_Get_Descriptor_Description(uint8_t i_desc)
{
    if( i_desc < 0x36 )
        return ISO13818_1_descriptors_descs[i_desc];
    else
        return ISO13818_1_other_descs[1];
}

/* From ARIB TR-B14 */
static const struct
{
    uint8_t id;
    const char *psz_desc;
} ARIB_B10_PMT_Descriptors_descs[5] = {
    { 0xC1, "Digital copy control" },
    { 0xDE, "Content availability" },
    { 0xF6, "Access Control" },
    { 0xFC, "Emergency Information" },
    { 0xFD, "Source Coding" },
};

static const char *ARIB_B10_Get_PMT_Descriptor_Description(uint8_t i_desc)
{
    for(uint8_t i=0; i<5; i++)
        if(ARIB_B10_PMT_Descriptors_descs[i].id == i_desc)
            return ARIB_B10_PMT_Descriptors_descs[i].psz_desc;
    return NULL;
}

#endif
