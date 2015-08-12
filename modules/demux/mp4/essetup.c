/*****************************************************************************
 * essetup.h: es setup from stsd and extensions parsing
 *****************************************************************************
 * Copyright (C) 2001-2004, 2010, 2014 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

#include "mp4.h"

#include <vlc_demux.h>
#include <vlc_aout.h>
#include <assert.h>

static void SetupESDS( demux_t *p_demux, mp4_track_t *p_track, const MP4_descriptor_decoder_config_t *p_decconfig )
{
    /* First update information based on i_objectTypeIndication */
    switch( p_decconfig->i_objectProfileIndication )
    {
    case( 0x20 ): /* MPEG4 VIDEO */
        p_track->fmt.i_codec = VLC_CODEC_MP4V;
        break;
    case( 0x21 ): /* H.264 */
        p_track->fmt.i_codec = VLC_CODEC_H264;
        break;
    case( 0x40):
        p_track->fmt.i_codec = VLC_CODEC_MP4A;
        if( p_decconfig->i_decoder_specific_info_len >= 2 &&
                p_decconfig->p_decoder_specific_info[0]       == 0xF8 &&
                (p_decconfig->p_decoder_specific_info[1]&0xE0) == 0x80 )
        {
            p_track->fmt.i_codec = VLC_CODEC_ALS;
        }
        break;
    case( 0x60):
    case( 0x61):
    case( 0x62):
    case( 0x63):
    case( 0x64):
    case( 0x65): /* MPEG2 video */
        p_track->fmt.i_codec = VLC_CODEC_MPGV;
        break;
        /* Theses are MPEG2-AAC */
    case( 0x66): /* main profile */
    case( 0x67): /* Low complexity profile */
    case( 0x68): /* Scaleable Sampling rate profile */
        p_track->fmt.i_codec = VLC_CODEC_MP4A;
        break;
        /* True MPEG 2 audio */
    case( 0x69):
        p_track->fmt.i_codec = VLC_CODEC_MPGA;
        break;
    case( 0x6a): /* MPEG1 video */
        p_track->fmt.i_codec = VLC_CODEC_MPGV;
        break;
    case( 0x6b): /* MPEG1 audio */
        p_track->fmt.i_codec = VLC_CODEC_MPGA;
        break;
    case( 0x6c ): /* jpeg */
        p_track->fmt.i_codec = VLC_CODEC_JPEG;
        break;
    case( 0x6d ): /* png */
        p_track->fmt.i_codec = VLC_CODEC_PNG;
        break;
    case( 0x6e ): /* jpeg2000 */
        p_track->fmt.i_codec = VLC_FOURCC( 'M','J','2','C' );
        break;
    case( 0xa3 ): /* vc1 */
        p_track->fmt.i_codec = VLC_CODEC_VC1;
        break;
    case( 0xa4 ):
        p_track->fmt.i_codec = VLC_CODEC_DIRAC;
        break;
    case( 0xa5 ):
        p_track->fmt.i_codec = VLC_CODEC_A52;
        break;
    case( 0xa6 ):
        p_track->fmt.i_codec = VLC_CODEC_EAC3;
        break;
    case( 0xa9 ): /* dts */
    case( 0xaa ): /* DTS-HD HRA */
    case( 0xab ): /* DTS-HD Master Audio */
        p_track->fmt.i_codec = VLC_CODEC_DTS;
        break;
    case( 0xDD ):
        p_track->fmt.i_codec = VLC_CODEC_VORBIS;
        break;

        /* Private ID */
    case( 0xe0 ): /* NeroDigital: dvd subs */
        if( p_track->fmt.i_cat == SPU_ES )
        {
            p_track->fmt.i_codec = VLC_CODEC_SPU;
            if( p_track->i_width > 0 )
                p_track->fmt.subs.spu.i_original_frame_width = p_track->i_width;
            if( p_track->i_height > 0 )
                p_track->fmt.subs.spu.i_original_frame_height = p_track->i_height;
            break;
        }
    case( 0xe1 ): /* QCelp for 3gp */
        if( p_track->fmt.i_cat == AUDIO_ES )
        {
            p_track->fmt.i_codec = VLC_CODEC_QCELP;
        }
        break;

        /* Fallback */
    default:
        /* Unknown entry, but don't touch i_fourcc */
        msg_Warn( p_demux,
                  "unknown objectProfileIndication(0x%x) (Track[ID 0x%x])",
                  p_decconfig->i_objectProfileIndication,
                  p_track->i_track_ID );
        break;
    }

    p_track->fmt.i_extra = p_decconfig->i_decoder_specific_info_len;
    if( p_track->fmt.i_extra > 0 )
    {
        p_track->fmt.p_extra = malloc( p_track->fmt.i_extra );
        memcpy( p_track->fmt.p_extra, p_decconfig->p_decoder_specific_info,
                p_track->fmt.i_extra );
    }
    if( p_track->fmt.i_codec == VLC_CODEC_SPU &&
            p_track->fmt.i_extra >= 16 * 4 )
    {
        for( int i = 0; i < 16; i++ )
        {
            p_track->fmt.subs.spu.palette[1 + i] =
                    GetDWBE((char*)p_track->fmt.p_extra + i * 4);
        }
        p_track->fmt.subs.spu.palette[0] = 0xBeef;
    }
}

int SetupVideoES( demux_t *p_demux, mp4_track_t *p_track, MP4_Box_t *p_sample )
{
    MP4_Box_data_sample_vide_t *p_vide = p_sample->data.p_sample_vide;
    if(!p_vide)
        return 0;

    p_track->fmt.video.i_width = p_vide->i_width;
    p_track->fmt.video.i_height = p_vide->i_height;
    p_track->fmt.video.i_bits_per_pixel = p_vide->i_depth;

    /* fall on display size */
    if( p_track->fmt.video.i_width <= 0 )
        p_track->fmt.video.i_width = p_track->i_width;
    if( p_track->fmt.video.i_height <= 0 )
        p_track->fmt.video.i_height = p_track->i_height;

    /* Find out apect ratio from display size */
    if( p_track->i_width > 0 && p_track->i_height > 0 &&
        /* Work-around buggy muxed files */
        p_vide->i_width != p_track->i_width )
    {
        p_track->fmt.video.i_sar_num = p_track->i_width  * p_track->fmt.video.i_height;
        p_track->fmt.video.i_sar_den = p_track->i_height * p_track->fmt.video.i_width;
    }

    /* Support for cropping (eg. in H263 files) */
    p_track->fmt.video.i_visible_width = p_track->fmt.video.i_width;
    p_track->fmt.video.i_visible_height = p_track->fmt.video.i_height;

    /* Rotation */
    switch( (int)p_track->f_rotation ) {
        case 90:
            p_track->fmt.video.orientation = ORIENT_ROTATED_90;
            break;
        case 180:
            p_track->fmt.video.orientation = ORIENT_ROTATED_180;
            break;
        case 270:
            p_track->fmt.video.orientation = ORIENT_ROTATED_270;
            break;
    }

    /* It's a little ugly but .. there are special cases */
    switch( p_sample->i_type )
    {
        case( VLC_FOURCC( 's', '2', '6', '3' ) ):
            p_track->fmt.i_codec = VLC_CODEC_H263;
            break;
        case VLC_FOURCC('y','v','1','2'):
            p_track->fmt.i_codec = VLC_CODEC_YV12;
            break;
        case VLC_FOURCC('y','u','v','2'):
            p_track->fmt.i_codec = VLC_FOURCC('Y','U','Y','2');
            break;

        default:
            p_track->fmt.i_codec = p_sample->i_type;
            break;
    }


    /* Read extensions */

    /* Set up A/R from extension atom */
    const MP4_Box_t *p_pasp = MP4_BoxGet( p_sample, "pasp" );
    if( p_pasp && BOXDATA(p_pasp) && BOXDATA(p_pasp)->i_horizontal_spacing > 0 &&
                  BOXDATA(p_pasp)->i_vertical_spacing > 0 )
    {
        p_track->fmt.video.i_sar_num = BOXDATA(p_pasp)->i_horizontal_spacing;
        p_track->fmt.video.i_sar_den = BOXDATA(p_pasp)->i_vertical_spacing;
    }

    const MP4_Box_t *p_fiel = MP4_BoxGet( p_sample, "fiel" );
    if( p_fiel && BOXDATA(p_fiel) )
    {
        p_track->i_block_flags = BOXDATA(p_fiel)->i_flags;
    }

    /* now see if esds is present and if so create a data packet
        with decoder_specific_info  */
    MP4_Box_t *p_esds = MP4_BoxGet( p_sample, "esds" );
    if ( p_esds && BOXDATA(p_esds) && BOXDATA(p_esds)->es_descriptor.p_decConfigDescr )
    {
        assert(p_sample->i_type == ATOM_mp4v);
        SetupESDS( p_demux, p_track, BOXDATA(p_esds)->es_descriptor.p_decConfigDescr );
    }
    else switch( p_sample->i_type )
    {
        /* qt decoder, send the complete chunk */
        case VLC_FOURCC ('h', 'd', 'v', '1'): // HDV 720p30
        case VLC_FOURCC ('h', 'd', 'v', '2'): // HDV 1080i60
        case VLC_FOURCC ('h', 'd', 'v', '3'): // HDV 1080i50
        case VLC_FOURCC ('h', 'd', 'v', '5'): // HDV 720p25
        case VLC_FOURCC ('m', 'x', '5', 'n'): // MPEG2 IMX NTSC 525/60 50mb/s produced by FCP
        case VLC_FOURCC ('m', 'x', '5', 'p'): // MPEG2 IMX PAL 625/60 50mb/s produced by FCP
        case VLC_FOURCC ('m', 'x', '4', 'n'): // MPEG2 IMX NTSC 525/60 40mb/s produced by FCP
        case VLC_FOURCC ('m', 'x', '4', 'p'): // MPEG2 IMX PAL 625/60 40mb/s produced by FCP
        case VLC_FOURCC ('m', 'x', '3', 'n'): // MPEG2 IMX NTSC 525/60 30mb/s produced by FCP
        case VLC_FOURCC ('m', 'x', '3', 'p'): // MPEG2 IMX PAL 625/50 30mb/s produced by FCP
        case VLC_FOURCC ('x', 'd', 'v', '2'): // XDCAM HD 1080i60
        case VLC_FOURCC ('A', 'V', 'm', 'p'): // AVID IMX PAL
            p_track->fmt.i_codec = VLC_CODEC_MPGV;
            break;
        /* qt decoder, send the complete chunk */
        case VLC_CODEC_SVQ1:
        case VLC_CODEC_SVQ3:
        case VLC_FOURCC( 'V', 'P', '3', '1' ):
        case VLC_FOURCC( '3', 'I', 'V', '1' ):
        case VLC_FOURCC( 'Z', 'y', 'G', 'o' ):
        {
            p_track->fmt.i_extra =
                p_sample->data.p_sample_vide->i_qt_image_description;
            if( p_track->fmt.i_extra > 0 )
            {
                p_track->fmt.p_extra = malloc( p_track->fmt.i_extra );
                memcpy( p_track->fmt.p_extra,
                        p_sample->data.p_sample_vide->p_qt_image_description,
                        p_track->fmt.i_extra);
            }
            break;
        }

        case VLC_FOURCC('j', 'p', 'e', 'g'):
            p_track->fmt.i_codec = VLC_CODEC_MJPG;
           break;

        case VLC_CODEC_FFV1:
        {
            MP4_Box_t *p_binary = MP4_BoxGet( p_sample, "glbl" );
            if( p_binary && BOXDATA(p_binary) && BOXDATA(p_binary)->i_blob )
            {
                p_track->fmt.p_extra = malloc( BOXDATA(p_binary)->i_blob );
                if( p_track->fmt.p_extra )
                {
                    p_track->fmt.i_extra = BOXDATA(p_binary)->i_blob;
                    memcpy( p_track->fmt.p_extra, BOXDATA(p_binary)->p_blob,
                            p_track->fmt.i_extra );
                }
            }
            break;
        }

        case VLC_FOURCC( 'v', 'c', '-', '1' ):
        {
            MP4_Box_t *p_dvc1 = MP4_BoxGet( p_sample, "dvc1" );
            if( p_dvc1 && BOXDATA(p_dvc1) )
            {
                p_track->fmt.i_extra = BOXDATA(p_dvc1)->i_vc1;
                if( p_track->fmt.i_extra > 0 )
                {
                    p_track->fmt.p_extra = malloc( BOXDATA(p_dvc1)->i_vc1 );
                    memcpy( p_track->fmt.p_extra, BOXDATA(p_dvc1)->p_vc1,
                            p_track->fmt.i_extra );
                }
            }
            else
            {
                msg_Err( p_demux, "missing dvc1" );
            }
            break;
        }

        /* avc1: send avcC (h264 without annexe B, ie without start code)*/
        case VLC_FOURCC( 'a', 'v', 'c', '3' ):
        case VLC_FOURCC( 'a', 'v', 'c', '1' ):
        {
            MP4_Box_t *p_avcC = MP4_BoxGet( p_sample, "avcC" );

            if( p_avcC && BOXDATA(p_avcC) )
            {
                p_track->fmt.i_extra = BOXDATA(p_avcC)->i_avcC;
                if( p_track->fmt.i_extra > 0 )
                {
                    p_track->fmt.p_extra = malloc( BOXDATA(p_avcC)->i_avcC );
                    memcpy( p_track->fmt.p_extra, BOXDATA(p_avcC)->p_avcC,
                            p_track->fmt.i_extra );
                }
            }
            else
            {
                msg_Err( p_demux, "missing avcC" );
            }
            break;
        }
        case VLC_FOURCC( 'h', 'v', 'c', '1' ):
        case VLC_FOURCC( 'h', 'e', 'v', '1' ):
        {
            MP4_Box_t *p_hvcC = MP4_BoxGet( p_sample, "hvcC" );

            if( p_hvcC && p_hvcC->data.p_binary )
            {
                p_track->fmt.p_extra = malloc( p_hvcC->data.p_binary->i_blob );
                if( p_track->fmt.p_extra )
                {
                    p_track->fmt.i_extra = p_hvcC->data.p_binary->i_blob;
                    memcpy( p_track->fmt.p_extra, p_hvcC->data.p_binary->p_blob,
                            p_hvcC->data.p_binary->i_blob );
                }
                p_track->fmt.i_codec = VLC_CODEC_HEVC;
            }
            else
            {
                msg_Err( p_demux, "missing hvcC" );
            }
            break;
        }

        case ATOM_WMV3:
        {
            MP4_Box_t *p_strf = MP4_BoxGet(  p_sample, "strf", 0 );
            if ( p_strf && BOXDATA(p_strf) )
            {
                p_track->fmt.i_codec = VLC_CODEC_WMV3;
                p_track->fmt.video.i_width = BOXDATA(p_strf)->bmiHeader.biWidth;
                p_track->fmt.video.i_visible_width = p_track->fmt.video.i_width;
                p_track->fmt.video.i_height = BOXDATA(p_strf)->bmiHeader.biHeight;
                p_track->fmt.video.i_visible_height =p_track->fmt.video.i_height;
                p_track->fmt.video.i_bits_per_pixel = BOXDATA(p_strf)->bmiHeader.biBitCount;
                p_track->fmt.i_extra = BOXDATA(p_strf)->i_extra;
                if( p_track->fmt.i_extra > 0 )
                {
                    p_track->fmt.p_extra = malloc( BOXDATA(p_strf)->i_extra );
                    memcpy( p_track->fmt.p_extra, BOXDATA(p_strf)->p_extra,
                            p_track->fmt.i_extra );
                }
                p_track->p_asf = MP4_BoxGet( p_sample, "ASF " );
            }
            break;
        }

        default:
            msg_Dbg( p_demux, "Unrecognized FourCC %4.4s", (char *)&p_sample->i_type );
            break;
    }

    return 1;
}

int SetupAudioES( demux_t *p_demux, mp4_track_t *p_track, MP4_Box_t *p_sample )
{
    MP4_Box_data_sample_soun_t *p_soun = p_sample->data.p_sample_soun;
    if(!p_soun)
        return 0;

    p_track->fmt.audio.i_channels = p_soun->i_channelcount;
    p_track->fmt.audio.i_rate = p_soun->i_sampleratehi;
    p_track->fmt.i_bitrate = p_soun->i_channelcount * p_soun->i_sampleratehi *
                             p_soun->i_samplesize;
    p_track->fmt.audio.i_bitspersample = p_soun->i_samplesize;

    p_track->fmt.i_original_fourcc = p_sample->i_type;

    if( ( p_track->i_sample_size == 1 || p_track->i_sample_size == 2 ) )
    {
        if( p_soun->i_qt_version == 0 )
        {
            switch( p_sample->i_type )
            {
            case VLC_CODEC_ADPCM_IMA_QT:
                p_soun->i_qt_version = 1;
                p_soun->i_sample_per_packet = 64;
                p_soun->i_bytes_per_packet  = 34;
                p_soun->i_bytes_per_frame   = 34 * p_soun->i_channelcount;
                p_soun->i_bytes_per_sample  = 2;
                break;
            case VLC_CODEC_MACE3:
                p_soun->i_qt_version = 1;
                p_soun->i_sample_per_packet = 6;
                p_soun->i_bytes_per_packet  = 2;
                p_soun->i_bytes_per_frame   = 2 * p_soun->i_channelcount;
                p_soun->i_bytes_per_sample  = 2;
                break;
            case VLC_CODEC_MACE6:
                p_soun->i_qt_version = 1;
                p_soun->i_sample_per_packet = 12;
                p_soun->i_bytes_per_packet  = 2;
                p_soun->i_bytes_per_frame   = 2 * p_soun->i_channelcount;
                p_soun->i_bytes_per_sample  = 2;
                break;
            default:
                p_track->fmt.i_codec = p_sample->i_type;
                break;
            }

        }
        else if( p_soun->i_qt_version == 1 && p_soun->i_sample_per_packet <= 0 )
        {
            p_soun->i_qt_version = 0;
        }
    }
    else if( p_sample->data.p_sample_soun->i_qt_version == 1 )
    {
        switch( p_sample->i_type )
        {
        case( VLC_FOURCC( '.', 'm', 'p', '3' ) ):
        case( VLC_FOURCC( 'm', 's', 0x00, 0x55 ) ):
        {
            if( p_track->i_sample_size > 1 )
                p_soun->i_qt_version = 0;
            break;
        }
        case( ATOM_ac3 ):
        case( ATOM_eac3 ):
        case( VLC_FOURCC( 'm', 's', 0x20, 0x00 ) ):
            p_soun->i_qt_version = 0;
            break;
        default:
            break;
        }

        if ( p_sample->data.p_sample_soun->i_compressionid == 0xFFFE /* -2 */)
        {
            /* redefined sample tables for vbr audio */
        }
        else if ( p_track->i_sample_size != 0 && p_soun->i_sample_per_packet == 0 )
        {
            msg_Err( p_demux, "Invalid sample per packet value for qt_version 1. Broken muxer! %u %u",
                     p_track->i_sample_size, p_soun->i_sample_per_packet );
            p_soun->i_qt_version = 0;
        }
    }

    /* Endianness atom */
    const MP4_Box_t *p_enda = MP4_BoxGet( p_sample, "wave/enda" );
    if( !p_enda )
        p_enda = MP4_BoxGet( p_sample, "enda" );

    /* It's a little ugly but .. there are special cases */
    switch( p_sample->i_type )
    {
        case ATOM_agsm: /* Apple gsm 33 bytes != MS GSM (agsm fourcc, 65 bytes) */
            p_track->fmt.i_codec = VLC_CODEC_GSM;
            break;
        case( VLC_FOURCC( '.', 'm', 'p', '3' ) ):
        case( VLC_FOURCC( 'm', 's', 0x00, 0x55 ) ):
        {
            p_track->fmt.i_codec = VLC_CODEC_MPGA;
            break;
        }
        case( ATOM_eac3 ):
        {
            const MP4_Box_t *p_dec3 = MP4_BoxGet(  p_sample, "dec3", 0 );

            p_track->fmt.i_codec = VLC_CODEC_EAC3;
            if( p_dec3 && BOXDATA(p_dec3) )
            {
                p_track->fmt.audio.i_channels = 0;
                p_track->fmt.i_bitrate = BOXDATA(p_dec3)->i_data_rate * 1000;
                p_track->fmt.audio.i_bitspersample = 0;
            }
            break;
        }
        case( ATOM_ac3 ):
        {
            MP4_Box_t *p_dac3 = MP4_BoxGet(  p_sample, "dac3", 0 );

            p_track->fmt.i_codec = VLC_CODEC_A52;
            if( p_dac3 && BOXDATA(p_dac3) )
            {
                static const int pi_bitrate[] = {
                     32,  40,  48,  56,
                     64,  80,  96, 112,
                    128, 160, 192, 224,
                    256, 320, 384, 448,
                    512, 576, 640,
                };
                p_track->fmt.audio.i_channels = 0;
                p_track->fmt.i_bitrate = 0;
                if( BOXDATA(p_dac3)->i_bitrate_code < sizeof(pi_bitrate)/sizeof(*pi_bitrate) )
                    p_track->fmt.i_bitrate = pi_bitrate[BOXDATA(p_dac3)->i_bitrate_code] * 1000;
                p_track->fmt.audio.i_bitspersample = 0;
            }
            break;
        }

        case( VLC_FOURCC( 'r', 'a', 'w', ' ' ) ):
        case( VLC_FOURCC( 'N', 'O', 'N', 'E' ) ):
        {
            if( (p_soun->i_samplesize+7)/8 == 1 )
                p_track->fmt.i_codec = VLC_CODEC_U8;
            else
                p_track->fmt.i_codec = VLC_FOURCC( 't', 'w', 'o', 's' );

            /* Buggy files workaround */
            if( (p_track->i_timescale != p_soun->i_sampleratehi) )
            {
                msg_Warn( p_demux, "i_timescale (%"PRId32") != i_sampleratehi "
                          "(%u), making both equal (report any problem).",
                          p_track->i_timescale, p_soun->i_sampleratehi );

                if( p_soun->i_sampleratehi != 0 )
                    p_track->i_timescale = p_soun->i_sampleratehi;
                else
                    p_soun->i_sampleratehi = p_track->i_timescale;
            }
            break;
        }

        case ATOM_in24:
            p_track->fmt.i_codec = p_enda && BOXDATA(p_enda)->i_little_endian == 1 ?
                                    VLC_FOURCC('4','2','n','i') : VLC_FOURCC('i','n','2','4');
            break;
        case ATOM_in32:
            p_track->fmt.i_codec = p_enda && BOXDATA(p_enda)->i_little_endian == 1 ?
                                    VLC_CODEC_S32L : VLC_CODEC_S32B;
            break;
        case ATOM_fl32:
            p_track->fmt.i_codec = p_enda && BOXDATA(p_enda)->i_little_endian == 1 ?
                                    VLC_CODEC_F32L : VLC_CODEC_F32B;
            break;
        case ATOM_fl64:
            p_track->fmt.i_codec = p_enda && BOXDATA(p_enda)->i_little_endian == 1 ?
                                    VLC_CODEC_F64L : VLC_CODEC_F64B;
            break;

        case VLC_CODEC_DVD_LPCM:
        {
            if( p_soun->i_qt_version == 2 )
            {
                /* Flags:
                 *  0x01: IsFloat
                 *  0x02: IsBigEndian
                 *  0x04: IsSigned
                 */
                static const struct {
                    unsigned     i_flags;
                    unsigned     i_mask;
                    unsigned     i_bits;
                    vlc_fourcc_t i_codec;
                } p_formats[] = {
                    { 0x01,           0x03, 32, VLC_CODEC_F32L },
                    { 0x01,           0x03, 64, VLC_CODEC_F64L },
                    { 0x01|0x02,      0x03, 32, VLC_CODEC_F32B },
                    { 0x01|0x02,      0x03, 64, VLC_CODEC_F64B },

                    { 0x00,           0x05,  8, VLC_CODEC_U8 },
                    { 0x00|     0x04, 0x05,  8, VLC_CODEC_S8 },

                    { 0x00,           0x07, 16, VLC_CODEC_U16L },
                    { 0x00|0x02,      0x07, 16, VLC_CODEC_U16B },
                    { 0x00     |0x04, 0x07, 16, VLC_CODEC_S16L },
                    { 0x00|0x02|0x04, 0x07, 16, VLC_CODEC_S16B },

                    { 0x00,           0x07, 24, VLC_CODEC_U24L },
                    { 0x00|0x02,      0x07, 24, VLC_CODEC_U24B },
                    { 0x00     |0x04, 0x07, 24, VLC_CODEC_S24L },
                    { 0x00|0x02|0x04, 0x07, 24, VLC_CODEC_S24B },

                    { 0x00,           0x07, 32, VLC_CODEC_U32L },
                    { 0x00|0x02,      0x07, 32, VLC_CODEC_U32B },
                    { 0x00     |0x04, 0x07, 32, VLC_CODEC_S32L },
                    { 0x00|0x02|0x04, 0x07, 32, VLC_CODEC_S32B },

                    {0, 0, 0, 0}
                };

                for( int i = 0; p_formats[i].i_codec; i++ )
                {
                    if( p_formats[i].i_bits == p_soun->i_constbitsperchannel &&
                        (p_soun->i_formatflags & p_formats[i].i_mask) == p_formats[i].i_flags )
                    {
                        p_track->fmt.i_codec = p_formats[i].i_codec;
                        p_track->fmt.audio.i_bitspersample = p_soun->i_constbitsperchannel;
                        p_track->fmt.audio.i_blockalign =
                                p_soun->i_channelcount * p_soun->i_constbitsperchannel / 8;
                        p_track->i_sample_size = p_track->fmt.audio.i_blockalign;

                        p_soun->i_qt_version = 0;
                        break;
                    }
                }
            }
            break;
        }
        default:
            p_track->fmt.i_codec = p_sample->i_type;
            break;
    }


    /* Process extensions */

    /* Lookup for then channels extension */
    const MP4_Box_t *p_chan = MP4_BoxGet( p_sample, "chan" );
    if ( p_chan )
    {
        if ( BOXDATA(p_chan)->layout.i_channels_layout_tag == MP4_CHAN_USE_CHANNELS_BITMAP )
        {
            uint32_t rgi_chans_sequence[AOUT_CHAN_MAX + 1];
            uint16_t i_vlc_mapping = 0;
            uint8_t i_channels = 0;
            const uint32_t i_bitmap = BOXDATA(p_chan)->layout.i_channels_bitmap;
            for (uint8_t i=0;i<MP4_CHAN_BITMAP_MAPPING_COUNT;i++)
            {
                if ( chan_bitmap_mapping[i].i_bitmap & i_bitmap )
                {
                    i_channels++;
                    if ( (chan_bitmap_mapping[i].i_vlc & i_vlc_mapping) ||
                         i_channels > AOUT_CHAN_MAX )
                    {
                        /* double mapping or unsupported number of channels */
                        i_vlc_mapping = 0;
                        msg_Warn( p_demux, "discarding chan mapping" );
                        break;
                    }
                    i_vlc_mapping |= chan_bitmap_mapping[i].i_vlc;
                    rgi_chans_sequence[i_channels - 1] = chan_bitmap_mapping[i].i_vlc;
                }
            }
            rgi_chans_sequence[i_channels] = 0;
            p_track->b_chans_reorder = !!
                    aout_CheckChannelReorder( rgi_chans_sequence, NULL, i_vlc_mapping,
                                              p_track->rgi_chans_reordering );
        }

    }

    /* now see if esds is present and if so create a data packet
        with decoder_specific_info  */
    MP4_Box_t *p_esds = MP4_BoxGet( p_sample, "esds" );
    if ( !p_esds ) p_esds = MP4_BoxGet( p_sample, "wave/esds" );
    if ( p_esds && BOXDATA(p_esds) && BOXDATA(p_esds)->es_descriptor.p_decConfigDescr )
    {
        assert(p_sample->i_type == ATOM_mp4a);
        SetupESDS( p_demux, p_track, BOXDATA(p_esds)->es_descriptor.p_decConfigDescr );
    }
    else switch( p_sample->i_type )
    {
        case VLC_CODEC_AMR_NB:
            p_track->fmt.audio.i_rate = 8000;
            break;
        case VLC_CODEC_AMR_WB:
            p_track->fmt.audio.i_rate = 16000;
            break;
        case VLC_FOURCC( 'Q', 'D', 'M', 'C' ):
        case VLC_CODEC_QDM2:
        case VLC_CODEC_ALAC:
        {
            p_track->fmt.i_extra =
                p_sample->data.p_sample_soun->i_qt_description;
            if( p_track->fmt.i_extra > 0 )
            {
                p_track->fmt.p_extra = malloc( p_track->fmt.i_extra );
                memcpy( p_track->fmt.p_extra,
                        p_sample->data.p_sample_soun->p_qt_description,
                        p_track->fmt.i_extra);
            }
            if( p_track->fmt.i_extra == 56 && p_sample->i_type == VLC_CODEC_ALAC )
            {
                p_track->fmt.audio.i_channels = *((uint8_t*)p_track->fmt.p_extra + 41);
                p_track->fmt.audio.i_rate = GetDWBE((uint8_t*)p_track->fmt.p_extra + 52);
            }
            break;
        }
        case VLC_CODEC_ADPCM_MS:
        case VLC_CODEC_ADPCM_IMA_WAV:
        case VLC_CODEC_QCELP:
        {
            p_track->fmt.audio.i_blockalign = p_sample->data.p_sample_soun->i_bytes_per_frame;
            break;
        }
        case ATOM_WMA2:
        {
            MP4_Box_t *p_WMA2 = MP4_BoxGet( p_sample, "wave/WMA2" );
            if( p_WMA2 && BOXDATA(p_WMA2) )
            {
                p_track->fmt.audio.i_channels = BOXDATA(p_WMA2)->Format.nChannels;
                p_track->fmt.audio.i_rate = BOXDATA(p_WMA2)->Format.nSamplesPerSec;
                p_track->fmt.i_bitrate = BOXDATA(p_WMA2)->Format.nAvgBytesPerSec * 8;
                p_track->fmt.audio.i_blockalign = BOXDATA(p_WMA2)->Format.nBlockAlign;
                p_track->fmt.audio.i_bitspersample = BOXDATA(p_WMA2)->Format.wBitsPerSample;
                p_track->fmt.i_extra = BOXDATA(p_WMA2)->i_extra;
                if( p_track->fmt.i_extra > 0 )
                {
                    p_track->fmt.p_extra = malloc( BOXDATA(p_WMA2)->i_extra );
                    memcpy( p_track->fmt.p_extra, BOXDATA(p_WMA2)->p_extra,
                            p_track->fmt.i_extra );
                }
                p_track->p_asf = MP4_BoxGet( p_sample, "wave/ASF " );
            }
            else
            {
                msg_Err( p_demux, "missing WMA2 %4.4s", (char*) &p_sample->p_father->i_type );
            }
            break;
        }

        default:
            msg_Dbg( p_demux, "Unrecognized FourCC %4.4s", (char *)&p_sample->i_type );
            break;
    }

    /* Late fixes */
    if ( p_soun->i_qt_version == 0 && p_track->fmt.i_codec == VLC_CODEC_QCELP )
    {
        /* Shouldn't be v0, as it is a compressed codec !*/
        p_soun->i_qt_version = 1;
        p_soun->i_compressionid = 0xFFFE;
    }

    return 1;
}

int SetupCCES( demux_t *p_demux, mp4_track_t *p_track, MP4_Box_t *p_sample )
{
    VLC_UNUSED(p_demux);

    switch( p_sample->i_type )
    {
        case( ATOM_c608 ): /* EIA608 closed captions */
        //case( ATOM_c708 ): /* EIA708 closed captions */
            p_track->fmt.i_codec = VLC_CODEC_EIA608_1;
            p_track->fmt.i_cat = SPU_ES;
            break;
        default:
            return 0;
    }

    return 1;
}

int SetupSpuES( demux_t *p_demux, mp4_track_t *p_track, MP4_Box_t *p_sample )
{
    MP4_Box_data_sample_text_t *p_text = p_sample->data.p_sample_text;
    if(!p_text)
        return 0;

    /* It's a little ugly but .. there are special cases */
    switch( p_sample->i_type )
    {
        case( VLC_FOURCC( 't', 'e', 'x', 't' ) ):
        case( VLC_FOURCC( 't', 'x', '3', 'g' ) ):
        {
            p_track->fmt.i_codec = VLC_CODEC_TX3G;

            text_style_t *p_style = text_style_Create( STYLE_NO_DEFAULTS );
            if ( p_style )
            {
                if ( p_text->i_font_size ) /* !WARN: in % of 5% height */
                {
                    p_style->f_font_relsize = p_text->i_font_size * 5 / 100;
                }
                if ( p_text->i_font_color )
                {
                    p_style->i_font_color = p_text->i_font_color >> 8;
                    p_style->i_font_alpha = p_text->i_font_color & 0xFF;
                    p_style->i_features |= (STYLE_HAS_FONT_ALPHA | STYLE_HAS_FONT_COLOR);
                }
                if ( p_text->i_background_color[3] >> 8 )
                {
                    p_style->i_background_color = p_text->i_background_color[0] >> 8;
                    p_style->i_background_color |= p_text->i_background_color[1] >> 8;
                    p_style->i_background_color |= p_text->i_background_color[2] >> 8;
                    p_style->i_background_alpha = p_text->i_background_color[3] >> 8;
                    p_style->i_features |= (STYLE_HAS_BACKGROUND_ALPHA | STYLE_HAS_BACKGROUND_COLOR);
                }
            }
            p_track->fmt.subs.p_style = p_style;

            /* FIXME UTF-8 doesn't work here ? */
            if( p_track->b_mac_encoding )
                p_track->fmt.subs.psz_encoding = strdup( "MAC" );
            else
                p_track->fmt.subs.psz_encoding = strdup( "UTF-8" );
            break;
        }

        default:
            p_track->fmt.i_codec = p_sample->i_type;
            break;
    }

    /* now see if esds is present and if so create a data packet
        with decoder_specific_info  */
    MP4_Box_t *p_esds = MP4_BoxGet( p_sample, "esds" );
    if ( p_esds && BOXDATA(p_esds) && BOXDATA(p_esds)->es_descriptor.p_decConfigDescr )
    {
        SetupESDS( p_demux, p_track, BOXDATA(p_esds)->es_descriptor.p_decConfigDescr );
    }

    return 1;
}
