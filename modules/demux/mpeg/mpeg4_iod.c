/*****************************************************************************
 * mpeg4_iod.c: ISO 14496-1 IOD and OD parsers
 *****************************************************************************
 * Copyright (C) 2004-2015 VLC authors and VideoLAN
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_bits.h>

#include "mpeg4_iod.h"

//#define OD_DEBUG 1
static void od_debug( vlc_object_t *p_object, const char *format, ... )
{
#ifdef OD_DEBUG
    va_list ap;
    va_start(ap, format);
    msg_GenericVa( p_object, VLC_MSG_DBG, format, ap );
    va_end(ap);
#else
    VLC_UNUSED(format);
    VLC_UNUSED(p_object);
#endif
}

/*****************************************************************************
 * MP4 specific functions (OD parser)
 *****************************************************************************/
static unsigned ODDescriptorLength( unsigned *pi_data, const uint8_t **pp_data )
{
    unsigned int i_b;
    unsigned int i_len = 0;

    if(*pi_data == 0)
        return 0;

    do
    {
        i_b = **pp_data;
        (*pp_data)++;
        (*pi_data)--;
        i_len = ( i_len << 7 ) + ( i_b&0x7f );

    } while( i_b&0x80 && *pi_data > 0 );

    if (i_len > *pi_data)
        i_len = *pi_data;

    return i_len;
}

static unsigned ODGetBytes( unsigned *pi_data, const uint8_t **pp_data, size_t bytes )
{
    unsigned res = 0;
    while( *pi_data > 0 && bytes-- )
    {
        res <<= 8;
        res |= **pp_data;
        (*pp_data)++;
        (*pi_data)--;
    }

    return res;
}

static char* ODGetURL( unsigned *pi_data, const uint8_t **pp_data )
{
    unsigned len = ODGetBytes( pi_data, pp_data, 1 );
    if (len > *pi_data)
        len = *pi_data;
    char *url = strndup( (char*)*pp_data, len );
    *pp_data += len;
    *pi_data -= len;
    return url;
}

#define ODTag_ObjectDescr           0x01
#define ODTag_InitialObjectDescr    0x02
#define ODTag_ESDescr               0x03
#define ODTag_DecConfigDescr        0x04
#define ODTag_DecSpecificDescr      0x05
#define ODTag_SLDescr               0x06

/* Unified pointer for read helper */
typedef union
{
    od_descriptor_t *p_od;
    od_descriptor_t **pp_ods;
    es_mpeg4_descriptor_t *es_descr;
    decoder_config_descriptor_t *p_dec_config;
    sl_config_descriptor_t *sl_descr;
} od_read_params_t;

static uint8_t OD_Desc_Read( vlc_object_t *, unsigned *, const uint8_t **, uint8_t, uint8_t, od_read_params_t params );

#define SL_Predefined_Custom 0x00
#define SL_Predefined_NULL   0x01
#define SL_Predefined_MP4    0x02
static bool OD_SLDesc_Read( vlc_object_t *p_object, unsigned i_data, const uint8_t *p_data,
                             od_read_params_t params )
{
    sl_config_descriptor_t *sl_descr = params.sl_descr;

    uint8_t i_predefined = ODGetBytes( &i_data, &p_data, 1 );
    switch( i_predefined )
    {
    case SL_Predefined_Custom:
        if( i_data < 15 )
            return false;
        sl_descr->i_flags = ODGetBytes( &i_data, &p_data, 1 );
        sl_descr->i_timestamp_resolution = ODGetBytes( &i_data, &p_data, 4 );
        sl_descr->i_OCR_resolution = ODGetBytes( &i_data, &p_data, 4 );
        sl_descr->i_timestamp_length = ODGetBytes( &i_data, &p_data, 1 );
        sl_descr->i_OCR_length = ODGetBytes( &i_data, &p_data, 1 );
        sl_descr->i_AU_length = ODGetBytes( &i_data, &p_data, 1 );
        sl_descr->i_instant_bitrate_length = ODGetBytes( &i_data, &p_data, 1 );
        uint16_t i16 = ODGetBytes( &i_data, &p_data, 2 );
        sl_descr->i_degradation_priority_length = i16 >> 12;
        sl_descr->i_AU_seqnum_length = (i16 >> 7) & 0x1f;
        sl_descr->i_packet_seqnum_length = (i16 >> 2) & 0x1f;
        break;
    case SL_Predefined_NULL:
        memset( sl_descr, 0, sizeof(*sl_descr) );
        sl_descr->i_timestamp_resolution = 1000;
        sl_descr->i_timestamp_length = 32;
        break;
    case SL_Predefined_MP4:
        memset( sl_descr, 0, sizeof(*sl_descr) );
        sl_descr->i_flags = USE_TIMESTAMPS_FLAG;
        break;
    default:
        /* reserved */
        return false;
    }

    if( sl_descr->i_flags & USE_DURATION_FLAG )
    {
        if( i_data < 8 )
            return false;
        sl_descr->i_timescale = ODGetBytes( &i_data, &p_data, 4 );
        sl_descr->i_accessunit_duration = ODGetBytes( &i_data, &p_data, 2 );
        sl_descr->i_compositionunit_duration = ODGetBytes( &i_data, &p_data, 2 );
    }

    if( (sl_descr->i_flags & USE_TIMESTAMPS_FLAG) == 0 )
    {
        bs_t s;
        bs_init( &s, p_data, i_data );
        sl_descr->i_startdecoding_timestamp = bs_read( &s, sl_descr->i_timestamp_length );
        sl_descr->i_startcomposition_timestamp = bs_read( &s, sl_descr->i_timestamp_length );
    }

    od_debug( p_object, "   * read sl desc predefined: 0x%x", i_predefined );
    return true;
}

static bool OD_DecSpecificDesc_Read( vlc_object_t *p_object, unsigned i_data, const uint8_t *p_data,
                                      od_read_params_t params )
{
    VLC_UNUSED(p_object);
    decoder_config_descriptor_t *p_dec_config = params.p_dec_config;

    p_dec_config->p_extra = malloc( i_data );
    if( p_dec_config->p_extra )
    {
        p_dec_config->i_extra = i_data;
        memcpy( p_dec_config->p_extra, p_data, p_dec_config->i_extra );
    }

    return !!p_dec_config->i_extra;
}

static bool OD_DecConfigDesc_Read( vlc_object_t *p_object, unsigned i_data, const uint8_t *p_data,
                                    od_read_params_t params )
{
    decoder_config_descriptor_t *p_dec_config = params.p_dec_config;

    if( i_data < 13 )
        return false;

    p_dec_config->i_objectTypeIndication = ODGetBytes( &i_data, &p_data, 1 );
    uint8_t i_flags = ODGetBytes( &i_data, &p_data, 1 );
    p_dec_config->i_streamType = i_flags >> 2;

    ODGetBytes( &i_data, &p_data, 3 ); /* bufferSizeDB */
    ODGetBytes( &i_data, &p_data, 4 ); /* maxBitrate */
    ODGetBytes( &i_data, &p_data, 4 ); /* avgBitrate */

    /* DecoderSpecificDescr */
    OD_Desc_Read( p_object, &i_data, &p_data,
                   ODTag_DecSpecificDescr, 1, params );

    od_debug( p_object, "   * read decoder objecttype: %x streamtype:%x extra: %u",
               p_dec_config->i_objectTypeIndication, p_dec_config->i_streamType, p_dec_config->i_extra );
    /* ProfileLevelIndicator [0..255] */
    return true;
}

static bool OD_ESDesc_Read( vlc_object_t *p_object, unsigned i_data, const uint8_t *p_data,
                             od_read_params_t params )
{
    es_mpeg4_descriptor_t *es_descr = params.es_descr;

    if ( i_data < 3 )
        return false;
    es_descr->i_es_id = ODGetBytes( &i_data, &p_data, 2 );
    uint8_t i_flags = ODGetBytes( &i_data, &p_data, 1 );

    if( ( i_flags >> 7 )&0x01 )
    {
        if ( i_data < 2 )
            return false;
        ODGetBytes( &i_data, &p_data, 2 ); /* dependOn_es_id */
    }

    if( (i_flags >> 6) & 0x01 )
        es_descr->psz_url = ODGetURL( &i_data, &p_data );

    if( ( i_flags >> 5 )&0x01 )
    {
        if ( i_data < 2 )
            return false;
        ODGetBytes( &i_data, &p_data, 2 ); /* OCR_es_id */
    }

    od_debug( p_object, "   * read ES Descriptor for es id %"PRIx16, es_descr->i_es_id );

    /* DecoderConfigDescr */
    params.p_dec_config = &es_descr->dec_descr;
    if ( 1 != OD_Desc_Read( p_object, &i_data, &p_data,
                             ODTag_DecConfigDescr, 1, params ) )
        return false;

    /* SLDescr */
    params.sl_descr = &es_descr->sl_descr;
    OD_Desc_Read( p_object, &i_data, &p_data, ODTag_SLDescr, 1, params );

    /* IPI / IP / IPMP ... */

    es_descr->b_ok = true;

    return true;
}

static bool OD_InitialObjectDesc_Read( vlc_object_t *p_object, unsigned i_data,
                                        const uint8_t *p_data, od_read_params_t params )
{
    od_descriptor_t *p_iod = params.p_od;
    if( i_data < 3 + 5 + 2 )
        return false;

    p_iod->i_ID = ( ODGetBytes( &i_data, &p_data, 1 ) << 2 );
    uint8_t i_flags = ODGetBytes( &i_data, &p_data, 1 );
    p_iod->i_ID |= i_flags >> 6;

    od_debug( p_object, "  * ObjectDescriptorID: %"PRIu16, p_iod->i_ID );
    od_debug( p_object, "  * includeInlineProfileLevel flag: 0x%"PRIx8, ( i_flags >> 4 )&0x01 );
    if ( (i_flags >> 5) & 0x01 )
    {
        p_iod->psz_url = ODGetURL( &i_data, &p_data );
        od_debug( p_object, "  * URL: %s", p_iod->psz_url );
        return true; /* leaves out unparsed remaining extdescr */
    }

    if( i_data < 5 + 2 ) /* at least one ES desc */
        return false;

    /* Profile Level Indication */
    ODGetBytes( &i_data, &p_data, 1 ); /* OD */
    ODGetBytes( &i_data, &p_data, 1 ); /* scene */
    ODGetBytes( &i_data, &p_data, 1 ); /* audio */
    ODGetBytes( &i_data, &p_data, 1 ); /* visual */
    ODGetBytes( &i_data, &p_data, 1 ); /* graphics */

    /* Now read */
    /* 1..255 ESdescr */
    uint8_t i_desc_count = OD_Desc_Read( p_object, &i_data, &p_data,
                                          ODTag_ESDescr, ES_DESCRIPTOR_COUNT, params );
    if( i_desc_count == 0 )
    {
        od_debug( p_object, "   * missing ES Descriptor" );
        return false;
    }

    /* 0..255 OCIdescr */
    /* 0..255 IPMPdescpointer */
    /* 0..255 IPMPdesc */
    /* 0..1   IPMPtoollistdesc */
    /* 0..255 Extensiondescr */

    return true;
}

static bool ODObjectDescriptorRead( vlc_object_t *p_object, unsigned i_data, const uint8_t *p_data,
                                  od_read_params_t params )
{
    od_descriptor_t *p_iod = params.p_od;
    if( i_data < 3 + 2 )
        return false;

    p_iod->i_ID = ( ODGetBytes( &i_data, &p_data, 1 ) << 2 );
    uint8_t i_flags = ODGetBytes( &i_data, &p_data, 1 );
    p_iod->i_ID |= i_flags >> 6;

    od_debug( p_object, "  * ObjectDescriptorID: %"PRIu16, p_iod->i_ID );
    if ( (i_flags >> 5) & 0x01 )
    {
        p_iod->psz_url = ODGetURL( &i_data, &p_data );
        od_debug( p_object, "  * URL: %s", p_iod->psz_url );
        return true;
    }

    if( i_data < 2 ) /* at least one ES desc */
        return false;

    /* 1..255 ESdescr */
    uint8_t i_desc_count = OD_Desc_Read( p_object, &i_data, &p_data,
                                          ODTag_ESDescr, ES_DESCRIPTOR_COUNT, params );
    if( i_desc_count == 0 )
    {
        od_debug( p_object, "   * missing ES Descriptor" );
        return false;
    }

    /* 0..255 OCIdescr */
    /* 0..255 IPMPdescpointer */
    /* 0..255 IPMPdesc */
    /* 0..1   IPMPtoollistdesc */
    /* 0..255 Extensiondescr */

    return true;
}

static uint8_t OD_Desc_Read( vlc_object_t *p_object, unsigned *pi_data, const uint8_t **pp_data,
                              uint8_t i_target_tag, uint8_t i_max_desc, od_read_params_t params )
{
    uint8_t i_read_count = 0;

    for (unsigned i = 0; *pi_data > 2 && i < i_max_desc; i++)
    {
        const uint8_t i_tag = ODGetBytes( pi_data, pp_data, 1 );
        const unsigned i_length = ODDescriptorLength( pi_data, pp_data );
        if( i_target_tag != i_tag || i_length > *pi_data )
            break;

        unsigned i_descriptor_data = i_length;
        const uint8_t *p_descriptor_data = *pp_data;

        od_debug( p_object, "  Reading descriptor 0x%"PRIx8": found tag 0x%"PRIx8" left %d",
                   i_target_tag, i_tag, *pi_data );
        switch( i_tag )
        {
            case ODTag_ObjectDescr:
            {
                od_descriptor_t *p_od = calloc( 1, sizeof( od_descriptor_t ) );
                if( !p_od )
                    break;
                od_read_params_t childparams;
                childparams.p_od = params.pp_ods[i_read_count] = p_od;
                /* od_descriptor_t *p_iod = (od_descriptor_t *) param; */
                if ( !ODObjectDescriptorRead( p_object, i_descriptor_data,
                                               p_descriptor_data, childparams ) )
                {};
                break;
            }

            case ODTag_InitialObjectDescr:
            {
                od_descriptor_t *p_iod = calloc( 1, sizeof( od_descriptor_t ) );
                if( !p_iod )
                    break;
                od_read_params_t childparams;
                childparams.p_od = params.pp_ods[i_read_count] = p_iod;
                /* od_descriptor_t *p_iod = (od_descriptor_t *) param; */
                if ( !OD_InitialObjectDesc_Read( p_object, i_descriptor_data,
                                                  p_descriptor_data, childparams ) )
                {};
                break;
            }

            case ODTag_ESDescr: /**/
            {
                od_descriptor_t *p_iod = params.p_od;
                od_read_params_t childparams;
                childparams.es_descr = &p_iod->es_descr[i_read_count];
                if ( !OD_ESDesc_Read( p_object, i_descriptor_data,
                                       p_descriptor_data, childparams ) )
                {};
                break;
            }

            case ODTag_DecConfigDescr:
            {
                if ( !OD_DecConfigDesc_Read( p_object, i_descriptor_data,
                                              p_descriptor_data, params ) )
                {};
                break;
            }

            case ODTag_DecSpecificDescr:
            {
                if ( !OD_DecSpecificDesc_Read( p_object, i_descriptor_data,
                                                p_descriptor_data, params ) )
                {};
                break;
            }

            case ODTag_SLDescr:
            {
                if ( !OD_SLDesc_Read( p_object, i_descriptor_data,
                                       p_descriptor_data, params ) )
                {};
                break;
            }

            default:
                od_debug( p_object, "trying to read unsupported descriptor" );
                break;
        }

        *pp_data += i_length;
        *pi_data -= i_length;

        i_read_count++;
    }

    return i_read_count;
}

static uint8_t ODInit( vlc_object_t *p_object, unsigned i_data, const uint8_t *p_data,
                       uint8_t i_start_tag, uint8_t i_min, uint8_t i_max, od_descriptor_t **pp_ods )
{
    od_read_params_t params;
    params.pp_ods = pp_ods;
    uint8_t i_read = OD_Desc_Read( p_object, &i_data, &p_data, i_start_tag, i_max, params );
    if ( i_read < i_min )
    {
        od_debug( p_object, "   cannot read first tag 0x%"PRIx8, i_start_tag );
        return 0;
    }

    return i_read;
}

od_descriptor_t *IODNew( vlc_object_t *p_object, unsigned i_data, const uint8_t *p_data )
{
    if( i_data < 4 )
        return NULL;

    uint8_t i_iod_scope = ODGetBytes( &i_data, &p_data, 1 ); /* scope */
    uint8_t i_iod_label = ODGetBytes( &i_data, &p_data, 1 );
    if( i_iod_label == 0x02 ) /* old vlc's buggy implementation of the OD_descriptor */
    {
        i_iod_label = i_iod_scope;
        i_iod_scope = 0x10; /* Add the missing front iod scope byte */
        i_data++; p_data--; /* next byte must be tag */
    }

    od_debug( p_object, "  * iod label:0x%"PRIx8" scope:0x%"PRIx8,
               i_iod_label, i_iod_scope );

    if( i_iod_scope != 0x10 && i_iod_scope != 0x11 ) /* Uniqueness in program or transport */
    {
        od_debug( p_object, "  * can't handle reserved scope 0x%"PRIx8, i_iod_scope );
        return NULL;
    }

    od_descriptor_t * ods[1];
    uint8_t i_count = ODInit( p_object, i_data, p_data, ODTag_InitialObjectDescr, 1, 1, ods );
    if( !i_count )
    {
        ODFree( ods[0] );
        return NULL;
    }
    return ods[0];
}

void ODFree( od_descriptor_t *p_iod )
{
    if( p_iod->psz_url )
    {
        free( p_iod->psz_url );
        free( p_iod );
        return;
    }

    for( int i = 0; i < 255; i++ )
    {
#define es_descr p_iod->es_descr[i]
        if( es_descr.b_ok )
        {
            if( es_descr.psz_url )
                free( es_descr.psz_url );
            else
                free( es_descr.dec_descr.p_extra );
        }
#undef  es_descr
    }
    free( p_iod );
}

/*****************************************************************************
 * SL Packet Parser
 *****************************************************************************/
sl_header_data DecodeSLHeader( unsigned i_data, const uint8_t *p_data,
                               const sl_config_descriptor_t *sl )
{
    sl_header_data ret = { 0 };

    bs_t s;
    bs_init( &s, p_data, i_data );

    bool b_has_ocr = false;
    bool b_is_idle = false;
    bool b_has_padding = false;
    uint8_t i_padding = 0;

    if( sl->i_flags & USE_ACCESS_UNIT_START_FLAG )
        ret.b_au_start = bs_read1( &s );
    if( sl->i_flags & USE_ACCESS_UNIT_END_FLAG )
        ret.b_au_end = bs_read1( &s );
    if( sl->i_OCR_length > 0 )
        b_has_ocr = bs_read1( &s );
    if( sl->i_flags & USE_IDLE_FLAG )
        b_is_idle = bs_read1( &s );
    if( sl->i_flags & USE_PADDING_FLAG )
        b_has_padding = bs_read1( &s );

    if( ret.b_au_end == ret.b_au_start && ret.b_au_start == false )
        ret.b_au_end = ret.b_au_start = true;

    if( b_has_padding )
        i_padding = bs_read( &s, 3 );

    /* Optional fields */
    if( !b_is_idle && ( !b_has_padding || !i_padding ) ) /* When not idle and not only padding */
    {
        bool b_has_dts = false;
        bool b_has_cts = false;
        bool b_has_instant_bitrate = false;
        struct
        {
            bool *p_b;
            mtime_t *p_t;
        } const timestamps[2] = { { &b_has_dts, &ret.i_dts }, { &b_has_cts, &ret.i_pts } };

        bs_read( &s, sl->i_packet_seqnum_length );

        if( sl->i_degradation_priority_length && bs_read1( &s ) )
            bs_read( &s, sl->i_degradation_priority_length );

        if( b_has_ocr )
            bs_read( &s, sl->i_OCR_length );

        if ( ret.b_au_start )
        {
            if( sl->i_flags & USE_RANDOM_ACCESS_POINT_FLAG )
                bs_read1( &s );

            bs_read( &s, sl->i_AU_seqnum_length );

            if ( sl->i_flags & USE_TIMESTAMPS_FLAG )
            {
                b_has_dts = bs_read1( &s );
                b_has_cts = bs_read1( &s );
            }

            if( sl->i_instant_bitrate_length )
                b_has_instant_bitrate = bs_read1( &s );

            for( int i=0; i<2; i++ )
            {
                if( !*(timestamps[i].p_b) )
                    continue;
                uint64_t i_read = bs_read( &s, __MIN( 32, sl->i_timestamp_length ) );
                if( sl->i_timestamp_length > 32 )
                {
                    uint8_t i_bits = __MAX( 1, sl->i_timestamp_length - 32 );
                    i_read = i_read << i_bits;
                    i_read |= bs_read( &s, i_bits );
                }
                if( sl->i_timestamp_resolution )
                    *(timestamps[i].p_t) = VLC_TS_0 + CLOCK_FREQ * i_read / sl->i_timestamp_resolution;
            }

            bs_read( &s, sl->i_AU_length );

            if( b_has_instant_bitrate )
                bs_read( &s, sl->i_instant_bitrate_length );
        }

        /* more to read if ExtSLConfigDescrTag */
    }

    if ( b_has_padding && !i_padding ) /* all padding */
        ret.i_size =  i_data;
    else
        ret.i_size = (bs_pos( &s ) + 7) / 8;

    return ret;
}

/*****************************************************************************
 * OD Commands Parser
 *****************************************************************************/
#define ODTag_ObjectDescrUpdate 0x01
#define ODTag_ObjectDescrRemove 0x02

static void ObjectDescrUpdateCommandRead( vlc_object_t *p_object, od_descriptors_t *p_ods,
                                          unsigned i_data, const uint8_t *p_data )
{
    od_descriptor_t *p_odsread[255];
    uint8_t i_count = ODInit( p_object, i_data, p_data, ODTag_ObjectDescr, 1, 255, p_odsread );
    for( int i=0; i<i_count; i++ )
    {
        od_descriptor_t *p_od = p_odsread[i];
        int i_pos = -1;
        ARRAY_BSEARCH( p_ods->objects, ->i_ID, int, p_od->i_ID, i_pos );
        if ( i_pos > -1 )
        {
            ODFree( p_ods->objects.p_elems[i_pos] );
            p_ods->objects.p_elems[i_pos] = p_od;
        }
        else
        {
            ARRAY_APPEND( p_ods->objects, p_od );
        }
    }
}

static void ObjectDescrRemoveCommandRead( vlc_object_t *p_object, od_descriptors_t *p_ods,
                                          unsigned i_data, const uint8_t *p_data )
{
    VLC_UNUSED(p_object);
    bs_t s;
    bs_init( &s, p_data, i_data );
    for( unsigned i=0; i< (i_data * 8 / 10); i++ )
    {
        uint16_t i_id = bs_read( &s, 10 );
        int i_pos = -1;
        ARRAY_BSEARCH( p_ods->objects, ->i_ID, int, i_id, i_pos );
        if( i_pos > -1 )
            ARRAY_REMOVE( p_ods->objects, i_pos );
    }
}

void DecodeODCommand( vlc_object_t *p_object, od_descriptors_t *p_ods,
                      unsigned i_data, const uint8_t *p_data )
{
    while( i_data )
    {
        const uint8_t i_tag = ODGetBytes( &i_data, &p_data, 1 );
        const unsigned i_length = ODDescriptorLength( &i_data, &p_data );
        if( !i_length || i_length > i_data )
            break;
        od_debug( p_object, "Decode tag 0x%x length %d", i_tag, i_length );
        switch( i_tag )
        {
            case ODTag_ObjectDescrUpdate:
                ObjectDescrUpdateCommandRead( p_object, p_ods, i_data, p_data );
                break;
            case ODTag_ObjectDescrRemove:
                ObjectDescrRemoveCommandRead( p_object, p_ods, i_data, p_data );
                break;
            default:
                break;
        }
        p_data += i_length;
        i_data -= i_data;
    }
}
