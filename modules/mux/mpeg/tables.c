/*****************************************************************************
 * tables.c
 *****************************************************************************
 * Copyright (C) 2001-2005, 2015 VLC authors and VideoLAN
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_block.h>
#include <vlc_es.h>

# include <dvbpsi/dvbpsi.h>
# include <dvbpsi/demux.h>
# include <dvbpsi/descriptor.h>
# include <dvbpsi/pat.h>
# include <dvbpsi/pmt.h>
# include <dvbpsi/sdt.h>
# include <dvbpsi/dr.h>
# include <dvbpsi/psi.h>

#include "dvbpsi_compat.h"

#include "streams.h"
#include "tsutil.h"
#include "tables.h"
#include "bits.h"
#include "pes.h"

#include "../../codec/jpeg2000.h"
#include "../../packetizer/dts_header.h"

#include <assert.h>

block_t *WritePSISection( dvbpsi_psi_section_t* p_section )
{
    block_t   *p_psi, *p_first = NULL;

    while( p_section )
    {
        int i_size = (uint32_t)(p_section->p_payload_end - p_section->p_data) +
                  (p_section->b_syntax_indicator ? 4 : 0);

        p_psi = block_Alloc( i_size + 1 );
        if( !p_psi )
            goto error;
        p_psi->i_pts = 0;
        p_psi->i_dts = 0;
        p_psi->i_length = 0;
        p_psi->i_buffer = i_size + 1;

        p_psi->p_buffer[0] = 0; /* pointer */
        memcpy( p_psi->p_buffer + 1,
                p_section->p_data,
                i_size );

        block_ChainAppend( &p_first, p_psi );

        p_section = p_section->p_next;
    }

    return( p_first );

error:
    if( p_first )
        block_ChainRelease( p_first );
    return NULL;
}

void BuildPAT( dvbpsi_t *p_dvbpsi,
               void *p_opaque, PEStoTSCallback pf_callback,
               int i_tsid, int i_pat_version_number,
               tsmux_stream_t *p_pat,
               unsigned i_programs, tsmux_stream_t *p_pmt, const int *pi_programs_number )
{
    dvbpsi_pat_t         patpsi;
    dvbpsi_psi_section_t *p_section;

    dvbpsi_pat_init( &patpsi, i_tsid, i_pat_version_number, true /* b_current_next */ );
    /* add all programs */
    for (unsigned i = 0; i < i_programs; i++ )
        dvbpsi_pat_program_add( &patpsi, pi_programs_number[i], p_pmt[i].i_pid );

    p_section = dvbpsi_pat_sections_generate( p_dvbpsi, &patpsi, 0 );
    if( likely(p_section) )
    {
        block_t *p_block = WritePSISection( p_section );
        if( likely(p_block) )
        {
            PEStoTS( p_opaque, pf_callback, p_block, p_pat->i_pid,
                     &p_pat->b_discontinuity, &p_pat->i_continuity_counter );
        }
        dvbpsi_DeletePSISections( p_section );
    }
    dvbpsi_pat_empty( &patpsi );
}
#if 1

static uint32_t GetDescriptorLength24b( int i_length )
{
    uint32_t i_l1, i_l2, i_l3;

    i_l1 = i_length&0x7f;
    i_l2 = ( i_length >> 7 )&0x7f;
    i_l3 = ( i_length >> 14 )&0x7f;

    return( 0x808000 | ( i_l3 << 16 ) | ( i_l2 << 8 ) | i_l1 );
}

static void Mpeg4SUBTDecoderSpecific_55( bits_buffer_t *b )
{
    bits_write( b, 8, 0x10 ); /* textFormat, 0x10 for 3GPP TS 26.245 */
    bits_write( b, 8, 0x00 ); /* flags: 1b: associated video info flag
                                        3b: reserved
                                        1b: duration flag
                                        3b: reserved */
    bits_write( b, 8, 52 );   /* remaining size */

    bits_write( b, 32, 0x00 ); /* display flags */

    bits_write( b, 8, 0x00 ); /* horizontal justification (-1: left, 0 center, 1 right) */
    bits_write( b, 8, 0x01 );  /* vertical   justification (-1: top, 0 center, 1 bottom) */

    bits_write( b, 24, 0x00 ); /* background rgb */
    bits_write( b, 8,  0xff ); /* background a */

    bits_write( b, 16,  0x00 ); /* text box top */
    bits_write( b, 16,  0x00 ); /* text box left */
    bits_write( b, 16,  0x00 ); /* text box bottom */
    bits_write( b, 16,  0x00 ); /* text box right */

    bits_write( b, 16,  0x00 ); /* start char */
    bits_write( b, 16,  0x00 ); /* end char */
    bits_write( b, 16,  0x00 ); /* default font id */


    bits_write( b, 8, 0x00 );   /* font style flags */
    bits_write( b, 8,   12 );   /* font size */

    bits_write( b, 24, 0x00 );  /* foreground rgb */
    bits_write( b,  8, 0x00 );  /* foreground a */

    bits_write( b, 24, 0x00 );
    bits_write( b,  8,   22 );  /* atom size */

    bits_write( b,  8,   'f' ); /* atom id */
    bits_write( b,  8,   't' );
    bits_write( b,  8,   'a' );
    bits_write( b,  8,   'b' );

    bits_write( b,  8,  0x00 );
    bits_write( b,  8,  0x01 ); /* entry count */

    bits_write( b, 16,  0x00 ); /* font id */
    bits_write( b,  8,     9 ); /* font name length */
    const char fontname[] = "Helvetica";
    for(int i=0; i<9; i++)
        bits_write( b,  8, fontname[i] ); /* font name */
}

static void GetPMTmpeg4( vlc_object_t *p_object, dvbpsi_pmt_t *p_dvbpmt,
                         unsigned i_mapped_streams, const pes_mapped_stream_t *p_mapped_streams )
{
    uint8_t iod[4096];
    bits_buffer_t bits, bits_fix_IOD;

    /* Make valgrind happy : it works at byte level not bit one so
     * bit_write confuse it (but DON'T CHANGE the way that bit_write is
     * working (needed when fixing some bits) */
    memset( iod, 0, 4096 );

    bits_initwrite( &bits, 4096, iod );
    /* IOD_label_scope */
    bits_write( &bits, 8,   0x11 );
    /* IOD_label */
    bits_write( &bits, 8,   0x01 );
    /* InitialObjectDescriptor */
    bits_align( &bits );
    bits_write( &bits, 8,   0x02 );     /* tag */
    bits_fix_IOD = bits;    /* save states to fix length later */
    bits_write( &bits, 24,
        GetDescriptorLength24b( 0 ) );  /* variable length (fixed later) */
    bits_write( &bits, 10,  0x01 );     /* ObjectDescriptorID */
    bits_write( &bits, 1,   0x00 );     /* URL Flag */
    bits_write( &bits, 1,   0x00 );     /* includeInlineProfileLevelFlag */
    bits_write( &bits, 4,   0x0f );     /* reserved */
    bits_write( &bits, 8,   0xff );     /* ODProfile (no ODcapability ) */
    bits_write( &bits, 8,   0xff );     /* sceneProfile */
    bits_write( &bits, 8,   0xfe );     /* audioProfile (unspecified) */
    bits_write( &bits, 8,   0xfe );     /* visualProfile( // ) */
    bits_write( &bits, 8,   0xff );     /* graphicProfile (no ) */
    for (unsigned i = 0; i < i_mapped_streams; i++ )
    {
        const pes_mapped_stream_t *p_stream = &p_mapped_streams[i];

        if( p_stream->pes->i_stream_id != 0xfa && p_stream->pes->i_stream_id != 0xfb &&
            p_stream->pes->i_stream_id != 0xfe )
            continue;

        bits_buffer_t bits_fix_ESDescr, bits_fix_Decoder;
        /* ES descriptor */
        bits_align( &bits );
        bits_write( &bits, 8,   0x03 );     /* ES_DescrTag */
        bits_fix_ESDescr = bits;
        bits_write( &bits, 24,
                    GetDescriptorLength24b( 0 ) ); /* variable size */
        bits_write( &bits, 16,  p_stream->pes->i_es_id );
        bits_write( &bits, 1,   0x00 );     /* streamDependency */
        bits_write( &bits, 1,   0x00 );     /* URL Flag */
        bits_write( &bits, 1,   0x00 );     /* OCRStreamFlag */
        bits_write( &bits, 5,   0x1f );     /* streamPriority */

        /* DecoderConfigDesciptor */
        bits_align( &bits );
        bits_write( &bits, 8,   0x04 ); /* DecoderConfigDescrTag */
        bits_fix_Decoder = bits;
        bits_write( &bits, 24,  GetDescriptorLength24b( 0 ) );
        if( p_stream->ts->i_stream_type == 0x10 )
        {
            bits_write( &bits, 8, 0x20 );   /* Visual 14496-2 */
            bits_write( &bits, 6, 0x04 );   /* VisualStream */
        }
        else if( p_stream->ts->i_stream_type == 0x1b )
        {
            bits_write( &bits, 8, 0x21 );   /* Visual 14496-2 */
            bits_write( &bits, 6, 0x04 );   /* VisualStream */
        }
        else if( p_stream->ts->i_stream_type == 0x11 ||
                 p_stream->ts->i_stream_type == 0x0f )
        {
            bits_write( &bits, 8, 0x40 );   /* Audio 14496-3 */
            bits_write( &bits, 6, 0x05 );   /* AudioStream */
        }
        else if( p_stream->ts->i_stream_type == 0x12 &&
                 p_stream->fmt->i_codec == VLC_CODEC_SUBT )
        {
            bits_write( &bits, 8, 0x0B );   /* Text Stream */
            bits_write( &bits, 6, 0x04 );   /* VisualStream */
        }
        else
        {
            bits_write( &bits, 8, 0x00 );
            bits_write( &bits, 6, 0x00 );

            msg_Err( p_object, "Unsupported stream_type => broken IOD" );
        }
        bits_write( &bits, 1,   0x00 );         /* UpStream */
        bits_write( &bits, 1,   0x01 );         /* reserved */
        bits_write( &bits, 24,  1024 * 1024 );  /* bufferSizeDB */
        bits_write( &bits, 32,  0x7fffffff );   /* maxBitrate */
        bits_write( &bits, 32,  0 );            /* avgBitrate */

        /* DecoderSpecificInfo */
        if( p_stream->fmt->i_codec == VLC_CODEC_SUBT )
        {
            bits_align( &bits );
            bits_write( &bits, 8,   0x05 ); /* tag */
            bits_write( &bits, 24,  55 );
            /* Create decoder specific info for subt */
            Mpeg4SUBTDecoderSpecific_55( &bits );
        }
        else if( p_stream->fmt->i_extra > 0 )
        {
            /* DecoderSpecificInfo */
            bits_align( &bits );
            bits_write( &bits, 8,   0x05 ); /* tag */
            bits_write( &bits, 24, GetDescriptorLength24b(
                        p_stream->fmt->i_extra ) );
            for (int j = 0; j < p_stream->fmt->i_extra; j++ )
            {
                bits_write( &bits, 8,
                    ((uint8_t*)p_stream->fmt->p_extra)[j] );
            }
        }

        /* fix Decoder length */
        bits_write( &bits_fix_Decoder, 24,
                    GetDescriptorLength24b( bits.i_data -
                    bits_fix_Decoder.i_data - 3 ) );

        /* SLConfigDescriptor : predefined (0x01) */
        bits_align( &bits );
        bits_write( &bits, 8,   0x06 ); /* tag */
        bits_write( &bits, 24,  GetDescriptorLength24b( 8 ) );
        bits_write( &bits, 8,   0x01 );/* predefined */
        bits_write( &bits, 1,   0 );   /* durationFlag */
        bits_write( &bits, 32,  0 );   /* OCRResolution */
        bits_write( &bits, 8,   0 );   /* OCRLength */
        bits_write( &bits, 8,   0 );   /* InstantBitrateLength */
        bits_align( &bits );

        /* fix ESDescr length */
        bits_write( &bits_fix_ESDescr, 24,
                    GetDescriptorLength24b( bits.i_data -
                    bits_fix_ESDescr.i_data - 3 ) );
    }
    bits_align( &bits );
    /* fix IOD length */
    bits_write( &bits_fix_IOD, 24,
                GetDescriptorLength24b(bits.i_data - bits_fix_IOD.i_data - 3 ));

    dvbpsi_pmt_descriptor_add(&p_dvbpmt[0], 0x1d, bits.i_data, bits.p_data);
}

static void UpdateServiceType( uint8_t *pi_service_cat, uint8_t *pi_service_type,
                               const tsmux_stream_t *p_ts, const es_format_t *fmt )
{
    uint8_t i_type = 0x00;

    switch( p_ts->i_stream_type )
    {
        case 0x01: /* MPEG1 */
        case 0x02: /* MPEG2 */
        case 0x80:
            i_type = 0x01;
            break;

        case 0x24: /* HEVC */
        case 0x10: /* MPEG4 */
        case 0x1b: /* H264 */
        case 0xA0: /* private */
        case 0xd1: /* dirac */
            i_type = 0x16;
            break;

        default:
            break;
    }

    if( i_type == 0x01 && fmt->video.i_visible_height > 468 &&
                          fmt->video.i_visible_width > 720 ) /* MPEG2 SD -> HD */
    {
         i_type = 0x11;
    }
    else if( i_type == 0x16 && fmt->video.i_visible_height > 468 &&
                               fmt->video.i_visible_width > 720 ) /* Advanced codec SD -> HD */
    {
         i_type = 0x19;
    }

    if( i_type != 0x00 )
    {
        if( *pi_service_cat != VIDEO_ES || i_type > *pi_service_type )
        {
            *pi_service_type = i_type;
            *pi_service_cat = VIDEO_ES;
        }
        return;
    }

    if( *pi_service_cat != VIDEO_ES ) /* Don't overwrite video */
    {
        /* Not video, try audio */
        switch( p_ts->i_stream_type )
        {
            case 0x03: /* MPEG1 audio */
            case 0x04: /* MPEG2 audio */
                i_type = 0x02;
                break;

            case 0x06:
            case 0x0f:
            case 0x81:
            case 0x83:
                i_type = 0x0A; /* Avanced codec digital radio */
                break;

            default:
                break;
        }

        if( i_type > *pi_service_type )
            *pi_service_type = i_type;
    }
}

static inline size_t Write_AnnexA_String( uint8_t *p_dest, const char *p_src )
{
    size_t i_src;
    if( p_src == NULL || !(i_src = strlen( p_src )) )
    {
        p_dest[0] = 0;
        return 1;
    }

    bool b_latin = (p_src[0] > 0x20);
    for ( size_t i=0; i< i_src && b_latin; i++ )
        b_latin &= !( p_src[i] & 0x80 );

    if( b_latin )
    {
        i_src = __MIN( i_src, UINT8_MAX );
        p_dest[0] = i_src; /* Total size */
        memcpy( &p_dest[1], p_src, i_src );
        return 1 + i_src;
    }
    else
    {
        i_src = __MIN( i_src, UINT8_MAX - 1 );
        p_dest[0] = 1 + i_src; /* Total size */
        p_dest[1] = 0x15; /* UTF8 Encoding */
        memcpy( &p_dest[2], p_src, i_src );
        return 2 + i_src;
    }
}

void BuildPMT( dvbpsi_t *p_dvbpsi, vlc_object_t *p_object,
               ts_mux_standard standard,
               void *p_opaque, PEStoTSCallback pf_callback,
               int i_tsid, int i_pmt_version_number,
               int i_pcr_pid,
               sdt_psi_t *p_sdt,
               unsigned i_programs, tsmux_stream_t *p_pmt, const int *pi_programs_number,
               unsigned i_mapped_streams, const pes_mapped_stream_t *p_mapped_streams )
{
    dvbpsi_pmt_t *dvbpmt = vlc_alloc( i_programs, sizeof(dvbpsi_pmt_t) );
    if( !dvbpmt )
            return;

    VLC_UNUSED(standard);
    dvbpsi_sdt_t sdtpsi;
    uint8_t *pi_service_types = NULL;
    uint8_t *pi_service_cats = NULL;
    if( p_sdt )
    {
        dvbpsi_sdt_init( &sdtpsi, 0x42, i_tsid, 1, true, p_sdt->i_netid );
        pi_service_types = calloc( i_programs * 2, sizeof *pi_service_types );
        if( !pi_service_types )
        {
            free( dvbpmt );
            return;
        }
        pi_service_cats = &pi_service_types[i_programs];
    }

    for (unsigned i = 0; i < i_programs; i++ )
    {
        dvbpsi_pmt_init( &dvbpmt[i],
                        pi_programs_number[i],   /* program number */
                        i_pmt_version_number,
                        true,      /* b_current_next */
                        i_pcr_pid );
    }

    for (unsigned i = 0; i < i_mapped_streams; i++ )
    {
        const pes_mapped_stream_t *p_stream = &p_mapped_streams[i];
        if( p_stream->pes->i_stream_id == 0xfa ||
            p_stream->pes->i_stream_id == 0xfb ||
            p_stream->pes->i_stream_id == 0xfe )
        {
            /* Has at least 1 MPEG4 stream */
            GetPMTmpeg4( p_object, dvbpmt, i_mapped_streams, p_mapped_streams );
            break;
        }
    }

    for (unsigned i = 0; i < i_mapped_streams; i++ )
    {
        const pes_mapped_stream_t *p_stream = &p_mapped_streams[i];

        dvbpsi_pmt_es_t *p_es = dvbpsi_pmt_es_add( &dvbpmt[p_stream->i_mapped_prog],
                    p_stream->ts->i_stream_type, p_stream->ts->i_pid );

        if( p_stream->pes->i_stream_id == 0xfa || p_stream->pes->i_stream_id == 0xfb )
        {
            uint8_t     es_id[2];

            /* SL descriptor */
            es_id[0] = (p_stream->pes->i_es_id >> 8)&0xff;
            es_id[1] = (p_stream->pes->i_es_id)&0xff;
            dvbpsi_pmt_es_descriptor_add( p_es, 0x1f, 2, es_id );
        }
        else if( p_stream->ts->i_stream_type == 0xa0 )
        {
            uint8_t data[512];
            size_t i_extra = __MIN( p_stream->fmt->i_extra, 502 );

            /* private DIV3 descripor */
            memcpy( &data[0], &p_stream->fmt->i_codec, 4 );
            data[4] = ( p_stream->fmt->video.i_visible_width >> 8 )&0xff;
            data[5] = ( p_stream->fmt->video.i_visible_width      )&0xff;
            data[6] = ( p_stream->fmt->video.i_visible_height>> 8 )&0xff;
            data[7] = ( p_stream->fmt->video.i_visible_height     )&0xff;
            data[8] = ( i_extra >> 8 )&0xff;
            data[9] = ( i_extra      )&0xff;
            if( i_extra > 0 )
            {
                memcpy( &data[10], p_stream->fmt->p_extra, i_extra );
            }

            /* 0xa0 is private */
            dvbpsi_pmt_es_descriptor_add( p_es, 0xa0, i_extra + 10, data );
        }
        else if( p_stream->fmt->i_codec == VLC_CODEC_JPEG2000 )
        {
            uint8_t *p_data = calloc( 1, 24 + p_stream->fmt->i_extra );
            if( p_data )
            {
                const int profile = j2k_get_profile( p_stream->fmt->video.i_visible_width,
                                                     p_stream->fmt->video.i_visible_height,
                                                     p_stream->fmt->video.i_frame_rate,
                                                     p_stream->fmt->video.i_frame_rate_base, true );
                p_data[0] = 0x01;
                if( profile < J2K_PROFILE_HD )
                    p_data[1] = 0x01; /* 0x0101 */
                else if( profile < J2K_PROFILE_3G )
                    p_data[1] = 0x02; /* 0x0102 */
                else
                    p_data[1] = 0x04; /* 0x0104 */
                SetDWBE( &p_data[2], p_stream->fmt->video.i_visible_width );
                SetDWBE( &p_data[6], p_stream->fmt->video.i_visible_height );
                SetWBE( &p_data[18], p_stream->fmt->video.i_frame_rate_base );
                SetWBE( &p_data[20], p_stream->fmt->video.i_frame_rate );
                p_data[21] = j2k_get_color_spec( p_stream->fmt->video.primaries,
                                                 p_stream->fmt->video.transfer,
                                                 p_stream->fmt->video.space );
                memcpy( &p_data[24], p_stream->fmt->p_extra, p_stream->fmt->i_extra );
                dvbpsi_pmt_es_descriptor_add( p_es, 0x32, 24 + p_stream->fmt->i_extra, p_data );
                free(p_data);
            }
        }
        else if( p_stream->fmt->i_codec == VLC_CODEC_DIRAC )
        {
            /* Dirac registration descriptor */

            uint8_t data[4] = { 'd', 'r', 'a', 'c' };
            dvbpsi_pmt_es_descriptor_add( p_es, 0x05, 4, data );
        }
        else if( p_stream->fmt->i_codec == VLC_CODEC_DTS )
        {
            uint8_t i_ver;
            /* DTS registration descriptor (ETSI TS 101 154 Annex F) */
            if( p_stream->fmt->i_profile == PROFILE_DTS_HD )
            {
                i_ver = 'H';
            }
            else if(vlc_popcount(p_stream->fmt->audio.i_frame_length) == 1)
            {
                i_ver = ctz( p_stream->fmt->audio.i_frame_length >> 8 );
                if(i_ver == 0 || i_ver > 3)
                   i_ver = 1;
                i_ver += '0';
            }
            else i_ver = '0';
            uint8_t data[4] = { 'D', 'T', 'S', i_ver };
            dvbpsi_pmt_es_descriptor_add( p_es, 0x05, 4, data );
        }
        else if( p_stream->fmt->i_codec == VLC_CODEC_A52 )
        {
            uint8_t format[4] = { 'A', 'C', '-', '3'};

            /* "registration" descriptor : "AC-3" */
            dvbpsi_pmt_es_descriptor_add( p_es, 0x05, 4, format );

            if( standard == TS_MUX_STANDARD_ATSC )
            {
                assert(p_stream->ts->i_stream_type == 0x81);
                /* FIXME: ATSC AC-3 audio_stream_descriptor */
                uint8_t data[1] = { 0x00 };
                dvbpsi_pmt_es_descriptor_add( p_es, 0x81, 1, data );
            }
            else
            {
                /* FIXME: DVB AC-3 descriptor */
                uint8_t data[1] = { 0x00 };
                dvbpsi_pmt_es_descriptor_add( p_es, 0x6a, 1, data );
            }
        }
        else if( p_stream->fmt->i_codec == VLC_CODEC_EAC3 )
        {
            uint8_t format[4] = { 'E', 'A', 'C', '3'};

            /* "registration" descriptor : "EAC3" */
            dvbpsi_pmt_es_descriptor_add( p_es, 0x05, 4, format );

            if( standard == TS_MUX_STANDARD_ATSC )
            {
                assert( p_stream->ts->i_stream_type == 0x87 );
                /* FIXME: ATSC EAC3 audio_stream_descriptor */
                uint8_t data[1] = { 0x00 };
                dvbpsi_pmt_es_descriptor_add( p_es, 0xcc, 1, data );
                /* FIXME: ATSC A-71 stream_info_details */
            }
            else
            {
                uint8_t data[1] = { 0x00 };
                dvbpsi_pmt_es_descriptor_add( p_es, 0x7a, 1, data );
            }
        }
        else if( p_stream->fmt->i_codec == VLC_CODEC_OPUS )
        {
            uint8_t data[2] = {
                0x80, /* tag extension */
                p_stream->fmt->audio.i_channels
            };
            dvbpsi_pmt_es_descriptor_add( p_es, 0x7f, 2, data );
            uint8_t format[4] = { 'O', 'p', 'u', 's'};
            /* "registration" descriptor : "Opus" */
            dvbpsi_pmt_es_descriptor_add( p_es, 0x05, 4, format );
        }
        else if( p_stream->fmt->i_codec == VLC_CODEC_TELETEXT )
        {
            if( p_stream->fmt->i_extra )
            {
                dvbpsi_pmt_es_descriptor_add( p_es, 0x56,
                                           p_stream->fmt->i_extra,
                                           p_stream->fmt->p_extra );
            }
            continue;
        }
        else if( p_stream->fmt->i_codec == VLC_CODEC_DVBS )
        {
            /* DVB subtitles */
            if( p_stream->fmt->i_extra )
            {
                /* pass-through from the TS demux */
                dvbpsi_pmt_es_descriptor_add( p_es, 0x59,
                                           p_stream->fmt->i_extra,
                                           p_stream->fmt->p_extra );
            }
            else
            {
                /* from the dvbsub transcoder */
                dvbpsi_subtitling_dr_t descr;
                dvbpsi_subtitle_t sub;
                dvbpsi_descriptor_t *p_descr;

                memcpy( sub.i_iso6392_language_code, p_stream->pes->lang, 3 );
                sub.i_subtitling_type = 0x10; /* no aspect-ratio criticality */
                sub.i_composition_page_id = p_stream->pes->i_es_id & 0xFF;
                sub.i_ancillary_page_id = p_stream->pes->i_es_id >> 16;

                descr.i_subtitles_number = 1;
                descr.p_subtitle[0] = sub;

                p_descr = dvbpsi_GenSubtitlingDr( &descr, 0 );
                /* Work around bug in old libdvbpsi */ p_descr->i_length = 8;
                dvbpsi_pmt_es_descriptor_add( p_es, p_descr->i_tag,
                                           p_descr->i_length, p_descr->p_data );
            }
            continue;
        }

        if( p_stream->pes->i_langs )
        {
            dvbpsi_pmt_es_descriptor_add( p_es, 0x0a, 4*p_stream->pes->i_langs,
                p_stream->pes->lang);
        }

        if( p_sdt )
        {
            UpdateServiceType( &pi_service_cats[p_stream->i_mapped_prog],
                               &pi_service_types[p_stream->i_mapped_prog],
                               p_stream->ts, p_stream->fmt );
        }
    }

    for (unsigned i = 0; i < i_programs; i++ )
    {
        dvbpsi_psi_section_t *sect = dvbpsi_pmt_sections_generate( p_dvbpsi, &dvbpmt[i] );
        if( likely(sect) )
        {
            block_t *pmt = WritePSISection( sect );
            if( likely(pmt) )
            {
                PEStoTS( p_opaque, pf_callback, pmt, p_pmt[i].i_pid,
                         &p_pmt[i].b_discontinuity, &p_pmt[i].i_continuity_counter );
            }
            dvbpsi_DeletePSISections(sect);
        }
        dvbpsi_pmt_empty( &dvbpmt[i] );
    }
    free( dvbpmt );

    if( p_sdt )
    {
        for (unsigned i = 0; i < i_programs; i++ )
        {
            dvbpsi_sdt_service_t *p_service = dvbpsi_sdt_service_add( &sdtpsi,
                                                                      pi_programs_number[i], /* service id */
                                                                      false,     /* eit schedule */
                                                                      false,     /* eit present */
                                                                      4,         /* running status ("4=RUNNING") */
                                                                      false );   /* free ca */

            const char *psz_sdtprov = p_sdt->desc[i].psz_provider;
            const char *psz_sdtserv = p_sdt->desc[i].psz_service_name;

            uint8_t p_sdt_desc[4 + 255 * 2];
            size_t i_sdt_desc = 0;

            /* mapped service type according to es types */
            p_sdt_desc[i_sdt_desc++] = pi_service_types[i];

            /* service provider name length */
            i_sdt_desc += Write_AnnexA_String( &p_sdt_desc[i_sdt_desc], psz_sdtprov );

            /* service name length */
            i_sdt_desc += Write_AnnexA_String( &p_sdt_desc[i_sdt_desc], psz_sdtserv );

            dvbpsi_sdt_service_descriptor_add( p_service, 0x48, i_sdt_desc, p_sdt_desc );
        }
        free( pi_service_types );

        dvbpsi_psi_section_t *sect = dvbpsi_sdt_sections_generate( p_dvbpsi, &sdtpsi );
        if( likely(sect) )
        {
            block_t *p_sdtblock = WritePSISection( sect );
            if( likely(p_sdtblock) )
            {
                PEStoTS( p_opaque, pf_callback, p_sdtblock, p_sdt->ts.i_pid,
                         &p_sdt->ts.b_discontinuity, &p_sdt->ts.i_continuity_counter );
            }
            dvbpsi_DeletePSISections( sect );
        }
        dvbpsi_sdt_empty( &sdtpsi );
    }
}

int FillPMTESParams( ts_mux_standard standard, const es_format_t *fmt,
                     tsmux_stream_t *ts, pesmux_stream_t *pes )
{
    switch( fmt->i_codec )
    {
    /* VIDEO */

    case VLC_CODEC_MPGV:
    case VLC_CODEC_MP2V:
    case VLC_CODEC_MP1V:
        /* TODO: do we need to check MPEG-I/II ? */
        ts->i_stream_type = 0x02;
        pes->i_stream_id = 0xe0;
        break;
    case VLC_CODEC_MP4V:
        ts->i_stream_type = 0x10;
        pes->i_stream_id = 0xe0;
        pes->i_es_id = ts->i_pid;
        break;
    case VLC_CODEC_HEVC:
        ts->i_stream_type = 0x24;
        pes->i_stream_id = 0xe0;
        break;
    case VLC_CODEC_H264:
        ts->i_stream_type = 0x1b;
        pes->i_stream_id = 0xe0;
        break;
    /* XXX dirty dirty but somebody want crapy MS-codec XXX */
    case VLC_CODEC_H263I:
    case VLC_CODEC_H263:
    case VLC_CODEC_WMV3:
    case VLC_CODEC_WMV2:
    case VLC_CODEC_WMV1:
    case VLC_CODEC_DIV3:
    case VLC_CODEC_DIV2:
    case VLC_CODEC_DIV1:
    case VLC_CODEC_MJPG:
        ts->i_stream_type = 0xa0; /* private */
        pes->i_stream_id = 0xa0;   /* beurk */
        break;
    case VLC_CODEC_DIRAC:
        /* stream_id makes use of stream_id_extension */
        pes->i_stream_id = (PES_EXTENDED_STREAM_ID << 8) | 0x60;
        ts->i_stream_type = 0xd1;
        break;
    case VLC_CODEC_JPEG2000:
        if( !j2k_is_valid_framerate( fmt->video.i_frame_rate,
                                     fmt->video.i_frame_rate_base ) )
            return VLC_EGENERIC;
        ts->i_stream_type = 0x21;
        pes->i_stream_id = 0xbd;
    break;

    /* AUDIO */

    case VLC_CODEC_MPGA:
    case VLC_CODEC_MP2:
    case VLC_CODEC_MP3:
        ts->i_stream_type = fmt->audio.i_rate >= 32000 ? 0x03 : 0x04;
        pes->i_stream_id = 0xc0;
        break;
    case VLC_CODEC_A52:
        pes->i_stream_id = 0xbd;
        if( standard == TS_MUX_STANDARD_ATSC )
        {
            ts->i_stream_type = 0x81;
        }
        else
        {
            ts->i_stream_type = 0x06;
        }
        break;
    case VLC_CODEC_DVD_LPCM:
        ts->i_stream_type = 0x83;
        pes->i_stream_id = 0xbd;
        break;
    case VLC_CODEC_OPUS:
        if (fmt->audio.i_channels > 8)
            return VLC_EGENERIC;
        pes->i_stream_id = 0xbd;
        pes->i_stream_id = 0x06;
        break;
    case VLC_CODEC_EAC3:
        pes->i_stream_id = 0xbd;
        if( standard == TS_MUX_STANDARD_ATSC )
        {
            /* FIXME: Mandatory EAC3 audio_descriptor */
            ts->i_stream_type = 0x87;
        }
        else
        {
            ts->i_stream_type = 0x06;
        }
        break;
    case VLC_CODEC_DTS:
        if( standard == TS_MUX_STANDARD_ATSC )
        {
            return VLC_EGENERIC;
        }
        else
        {
            ts->i_stream_type = 0x06;
            pes->i_stream_id = 0xbd;
        }
        break;
    case VLC_CODEC_MP4A:
        /* XXX: make that configurable in some way when LOAS
         * is implemented for AAC in TS */
        //ts->i_stream_type = 0x11; /* LOAS/LATM */
        ts->i_stream_type = 0x0f; /* ADTS */
        pes->i_stream_id = 0xc0;
        pes->i_es_id = ts->i_pid;
        break;

    /* TEXT */

    case VLC_CODEC_SPU:
        ts->i_stream_type = 0x82;
        pes->i_stream_id = 0xbd;
        break;
    case VLC_CODEC_SUBT:
        ts->i_stream_type = 0x12;
        pes->i_stream_id = 0xfa;
        pes->i_es_id = ts->i_pid;
        break;
    case VLC_CODEC_DVBS:
        ts->i_stream_type = 0x06;
        pes->i_es_id = fmt->subs.dvb.i_id;
        pes->i_stream_id = 0xbd;
        break;
    case VLC_CODEC_TELETEXT:
        ts->i_stream_type = 0x06;
        pes->i_stream_id = 0xbd; /* FIXME */
        break;

    default:
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

#endif
