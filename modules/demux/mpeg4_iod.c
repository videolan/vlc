/*****************************************************************************
 * mpeg4_iod.c: ISO 14496-1 IOD and parsers
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

//#define IOD_DEBUG 1
static void iod_debug( vlc_object_t *p_object, const char *format, ... )
{
#ifdef IOD_DEBUG
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
 * MP4 specific functions (IOD parser)
 *****************************************************************************/
static unsigned IODDescriptorLength( unsigned *pi_data, const uint8_t **pp_data )
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

static unsigned IODGetBytes( unsigned *pi_data, const uint8_t **pp_data, size_t bytes )
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

static char* IODGetURL( unsigned *pi_data, const uint8_t **pp_data )
{
    unsigned len = IODGetBytes( pi_data, pp_data, 1 );
    if (len > *pi_data)
        len = *pi_data;
    char *url = strndup( (char*)*pp_data, len );
    *pp_data += len;
    *pi_data -= len;
    return url;
}

#define IODTag_ObjectDescr           0x01
#define IODTag_InitialObjectDescr    0x02
#define IODTag_ESDescr               0x03
#define IODTag_DecConfigDescr        0x04
#define IODTag_DecSpecificDescr      0x05
#define IODTag_SLDescr               0x06

/* Unified pointer for read helper */
typedef union
{
    iod_descriptor_t *p_iod;
    es_mpeg4_descriptor_t *es_descr;
    decoder_config_descriptor_t *p_dec_config;
    sl_config_descriptor_t *sl_descr;
} iod_read_params_t;

static uint8_t IOD_Desc_Read( vlc_object_t *, unsigned *, const uint8_t **, uint8_t, uint8_t, iod_read_params_t params );

#define SL_Predefined_Custom 0x00
#define SL_Predefined_NULL   0x01
#define SL_Predefined_MP4    0x02
static bool IOD_SLDesc_Read( vlc_object_t *p_object, unsigned i_data, const uint8_t *p_data,
                             iod_read_params_t params )
{
    sl_config_descriptor_t *sl_descr = params.sl_descr;

    uint8_t i_predefined = IODGetBytes( &i_data, &p_data, 1 );
    switch( i_predefined )
    {
    case SL_Predefined_Custom:
        if( i_data < 15 )
            return false;
        sl_descr->i_flags = IODGetBytes( &i_data, &p_data, 1 );
        sl_descr->i_timestamp_resolution = IODGetBytes( &i_data, &p_data, 4 );
        sl_descr->i_OCR_resolution = IODGetBytes( &i_data, &p_data, 4 );
        sl_descr->i_timestamp_length = IODGetBytes( &i_data, &p_data, 1 );
        sl_descr->i_OCR_length = IODGetBytes( &i_data, &p_data, 1 );
        sl_descr->i_AU_length = IODGetBytes( &i_data, &p_data, 1 );
        sl_descr->i_instant_bitrate_length = IODGetBytes( &i_data, &p_data, 1 );
        uint16_t i16 = IODGetBytes( &i_data, &p_data, 2 );
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
        sl_descr->i_timescale = IODGetBytes( &i_data, &p_data, 4 );
        sl_descr->i_accessunit_duration = IODGetBytes( &i_data, &p_data, 2 );
        sl_descr->i_compositionunit_duration = IODGetBytes( &i_data, &p_data, 2 );
    }

    if( (sl_descr->i_flags & USE_TIMESTAMPS_FLAG) == 0 )
    {
        bs_t s;
        bs_init( &s, p_data, i_data );
        sl_descr->i_startdecoding_timestamp = bs_read( &s, sl_descr->i_timestamp_length );
        sl_descr->i_startcomposition_timestamp = bs_read( &s, sl_descr->i_timestamp_length );
    }

    iod_debug( p_object, "   * read sl desc predefined: 0x%x", i_predefined );
    return true;
}

static bool IOD_DecSpecificDesc_Read( vlc_object_t *p_object, unsigned i_data, const uint8_t *p_data,
                                      iod_read_params_t params )
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

static bool IOD_DecConfigDesc_Read( vlc_object_t *p_object, unsigned i_data, const uint8_t *p_data,
                                    iod_read_params_t params )
{
    decoder_config_descriptor_t *p_dec_config = params.p_dec_config;

    if( i_data < 13 )
        return false;

    p_dec_config->i_objectTypeIndication = IODGetBytes( &i_data, &p_data, 1 );
    uint8_t i_flags = IODGetBytes( &i_data, &p_data, 1 );
    p_dec_config->i_streamType = i_flags >> 2;

    IODGetBytes( &i_data, &p_data, 3 ); /* bufferSizeDB */
    IODGetBytes( &i_data, &p_data, 4 ); /* maxBitrate */
    IODGetBytes( &i_data, &p_data, 4 ); /* avgBitrate */

    /* DecoderSpecificDescr */
    IOD_Desc_Read( p_object, &i_data, &p_data,
                   IODTag_DecSpecificDescr, 1, params );

    iod_debug( p_object, "   * read decoder objecttype: %x streamtype:%x extra: %u",
               p_dec_config->i_objectTypeIndication, p_dec_config->i_streamType, p_dec_config->i_extra );
    /* ProfileLevelIndicator [0..255] */
    return true;
}

static bool IOD_ESDesc_Read( vlc_object_t *p_object, unsigned i_data, const uint8_t *p_data,
                             iod_read_params_t params )
{
    es_mpeg4_descriptor_t *es_descr = params.es_descr;

    if ( i_data < 3 )
        return false;
    es_descr->i_es_id = IODGetBytes( &i_data, &p_data, 2 );
    uint8_t i_flags = IODGetBytes( &i_data, &p_data, 1 );

    if( ( i_flags >> 7 )&0x01 )
    {
        if ( i_data < 2 )
            return false;
        IODGetBytes( &i_data, &p_data, 2 ); /* dependOn_es_id */
    }

    if( (i_flags >> 6) & 0x01 )
        es_descr->psz_url = IODGetURL( &i_data, &p_data );

    if( ( i_flags >> 5 )&0x01 )
    {
        if ( i_data < 2 )
            return false;
        IODGetBytes( &i_data, &p_data, 2 ); /* OCR_es_id */
    }

    iod_debug( p_object, "   * read ES Descriptor for es id %"PRIx16, es_descr->i_es_id );

    /* DecoderConfigDescr */
    params.p_dec_config = &es_descr->dec_descr;
    if ( 1 != IOD_Desc_Read( p_object, &i_data, &p_data,
                             IODTag_DecConfigDescr, 1, params ) )
        return false;

    /* SLDescr */
    params.sl_descr = &es_descr->sl_descr;
    IOD_Desc_Read( p_object, &i_data, &p_data, IODTag_SLDescr, 1, params );

    /* IPI / IP / IPMP ... */

    es_descr->b_ok = true;

    return true;
}

static bool IOD_InitialObjectDesc_Read( vlc_object_t *p_object, unsigned i_data,
                                        const uint8_t *p_data, iod_read_params_t params )
{
    iod_descriptor_t *p_iod = params.p_iod;
    if( i_data < 3 + 5 + 2 )
        return false;

    uint16_t i_object_descriptor_id = ( IODGetBytes( &i_data, &p_data, 1 ) << 2 );
    uint8_t i_flags = IODGetBytes( &i_data, &p_data, 1 );
    i_object_descriptor_id |= i_flags >> 6;

    iod_debug( p_object, "  * ObjectDescriptorID: %"PRIu16, i_object_descriptor_id );
    iod_debug( p_object, "  * includeInlineProfileLevel flag: 0x%"PRIx8, ( i_flags >> 4 )&0x01 );
    if ( (i_flags >> 5) & 0x01 )
    {
        p_iod->psz_url = IODGetURL( &i_data, &p_data );
        iod_debug( p_object, "  * URL: %s", p_iod->psz_url );
        return true; /* leaves out unparsed remaining extdescr */
    }

    if( i_data < 5 + 2 ) /* at least one ES desc */
        return false;

    /* Profile Level Indication */
    IODGetBytes( &i_data, &p_data, 1 ); /* OD */
    IODGetBytes( &i_data, &p_data, 1 ); /* scene */
    IODGetBytes( &i_data, &p_data, 1 ); /* audio */
    IODGetBytes( &i_data, &p_data, 1 ); /* visual */
    IODGetBytes( &i_data, &p_data, 1 ); /* graphics */

    /* Now read */
    /* 1..255 ESdescr */
    uint8_t i_desc_count = IOD_Desc_Read( p_object, &i_data, &p_data,
                                          IODTag_ESDescr, ES_DESCRIPTOR_COUNT, params );
    if( i_desc_count == 0 )
    {
        iod_debug( p_object, "   * missing ES Descriptor" );
        return false;
    }

    /* 0..255 OCIdescr */
    /* 0..255 IPMPdescpointer */
    /* 0..255 IPMPdesc */
    /* 0..1   IPMPtoollistdesc */
    /* 0..255 Extensiondescr */

    return true;
}

static uint8_t IOD_Desc_Read( vlc_object_t *p_object, unsigned *pi_data, const uint8_t **pp_data,
                              uint8_t i_target_tag, uint8_t i_max_desc, iod_read_params_t params )
{
    uint8_t i_read_count = 0;

    for (unsigned i = 0; *pi_data > 2 && i < i_max_desc; i++)
    {
        const uint8_t i_tag = IODGetBytes( pi_data, pp_data, 1 );
        const unsigned i_length = IODDescriptorLength( pi_data, pp_data );
        if( i_target_tag != i_tag || i_length > *pi_data )
            break;

        unsigned i_descriptor_data = i_length;
        const uint8_t *p_descriptor_data = *pp_data;

        iod_debug( p_object, "  Reading descriptor 0x%"PRIx8": found tag 0x%"PRIx8" left %d",
                   i_target_tag, i_tag, *pi_data );
        switch( i_tag )
        {
            case IODTag_InitialObjectDescr:
            {
                /* iod_descriptor_t *p_iod = (iod_descriptor_t *) param; */
                if ( !IOD_InitialObjectDesc_Read( p_object, i_descriptor_data,
                                                  p_descriptor_data, params ) )
                {};
                break;
            }

            case IODTag_ESDescr: /**/
            {
                iod_descriptor_t *p_iod = params.p_iod;
                iod_read_params_t childparams;
                childparams.es_descr = &p_iod->es_descr[i_read_count];
                if ( !IOD_ESDesc_Read( p_object, i_descriptor_data,
                                       p_descriptor_data, childparams ) )
                {};
                break;
            }

            case IODTag_DecConfigDescr:
            {
                if ( !IOD_DecConfigDesc_Read( p_object, i_descriptor_data,
                                              p_descriptor_data, params ) )
                {};
                break;
            }

            case IODTag_DecSpecificDescr:
            {
                if ( !IOD_DecSpecificDesc_Read( p_object, i_descriptor_data,
                                                p_descriptor_data, params ) )
                {};
                break;
            }

            case IODTag_SLDescr:
            {
                if ( !IOD_SLDesc_Read( p_object, i_descriptor_data,
                                       p_descriptor_data, params ) )
                {};
                break;
            }

            default:
                iod_debug( p_object, "trying to read unsupported descriptor" );
                break;
        }

        *pp_data += i_length;
        *pi_data -= i_length;

        i_read_count++;
    }

    return i_read_count;
}

iod_descriptor_t *IODNew( vlc_object_t *p_object, unsigned i_data, const uint8_t *p_data )
{
    if( i_data < 4 )
        return NULL;

    uint8_t i_iod_scope = IODGetBytes( &i_data, &p_data, 1 ); /* scope */
    uint8_t i_iod_label = IODGetBytes( &i_data, &p_data, 1 );
    if( i_iod_label == 0x02 ) /* old vlc's buggy implementation of the IOD_descriptor */
    {
        i_iod_label = i_iod_scope;
        i_iod_scope = 0x10; /* Add the missing front iod scope byte */
        i_data++; p_data--; /* next byte must be tag */
    }

    iod_debug( p_object, "  * iod label:0x%"PRIx8" scope:0x%"PRIx8,
               i_iod_label, i_iod_scope );

    if( i_iod_scope != 0x10 && i_iod_scope != 0x11 ) /* Uniqueness in program or transport */
    {
        iod_debug( p_object, "  * can't handle reserved scope 0x%"PRIx8, i_iod_scope );
        return NULL;
    }

    /* Initial Object Descriptor must follow */
    iod_descriptor_t *p_iod = calloc( 1, sizeof( iod_descriptor_t ) );
    if( !p_iod )
        return NULL;

    /* IOD_InitialObjectDescrTag Parsing */
    iod_read_params_t params;
    params.p_iod = p_iod;
    if ( 1 != IOD_Desc_Read( p_object, &i_data, &p_data,
                             IODTag_InitialObjectDescr, 1, params ) )
    {
        iod_debug( p_object, "   cannot read InitialObjectDescr" );
        free( p_iod );
        return NULL;
    }

    return p_iod;
}

void IODFree( iod_descriptor_t *p_iod )
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
