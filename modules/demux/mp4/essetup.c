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
#include "avci.h"
#include "../xiph.h"
#include "../../packetizer/iso_color_tables.h"
#include "mpeg4.h"

#include <vlc_demux.h>
#include <vlc_aout.h>
#include <assert.h>


static void CopyExtradata( const uint8_t *p_extra, size_t i_extra,
                           es_format_t *fmt )
{
    if( i_extra > 0 && !fmt->i_extra )
    {
        fmt->p_extra = malloc( i_extra );
        if( i_extra )
        {
            fmt->i_extra = i_extra;
            memcpy( fmt->p_extra, p_extra, i_extra );
        }
    }
}

static uint32_t GetSampleType( demux_t *p_demux, const MP4_Box_t *p_sample )
{
    const MP4_Box_t *p_frma;
    if( ( p_frma = MP4_BoxGet( p_sample, "sinf/frma" ) ) && BOXDATA(p_frma) )
    {
        msg_Warn( p_demux, "Original Format Box: %4.4s", (char *)&BOXDATA(p_frma)->i_type );
        return BOXDATA(p_frma)->i_type;
    }
    return p_sample->i_type;
}

static void SetupGlobalExtensions( const MP4_Box_t *p_sample,
                                   es_format_t *p_fmt )
{
    if( !p_fmt->i_bitrate )
    {
        const MP4_Box_t *p_btrt = MP4_BoxGet( p_sample, "btrt" );
        if( p_btrt && BOXDATA(p_btrt) )
        {
            p_fmt->i_bitrate = BOXDATA(p_btrt)->i_avg_bitrate;
        }
    }

    const MP4_Box_t *p_glbl = MP4_BoxGet( p_sample, "glbl" );
    if( p_glbl && p_glbl->data.p_binary && p_glbl->data.p_binary->p_blob )
    {
        CopyExtradata( p_glbl->data.p_binary->p_blob,
                       p_glbl->data.p_binary->i_blob, p_fmt );
    }
}

static void SetupESDS( demux_t *p_demux, const mp4_track_t *p_track,
                       const MP4_descriptor_decoder_config_t *p_decconfig,
                       es_format_t *p_fmt )
{
    /* First update information based on i_objectTypeIndication */
    switch( p_decconfig->i_objectProfileIndication )
    {
        /* Private ID */
    case( 0xe0 ): /* NeroDigital: dvd subs */
        if( p_fmt->i_cat == SPU_ES )
        {
            p_fmt->i_codec = VLC_CODEC_SPU;
            if( p_track->i_width > 0 )
                p_fmt->subs.spu.i_original_frame_width = p_track->i_width;
            if( p_track->i_height > 0 )
                p_fmt->subs.spu.i_original_frame_height = p_track->i_height;
        }
        break;
    case( 0xe1 ): /* QCelp for 3gp */
        if( p_fmt->i_cat == AUDIO_ES )
        {
            p_fmt->i_codec = VLC_CODEC_QCELP;
        }
        break;

        /* Fallback */
    default:
        if( MPEG4_Codec_By_ObjectType( p_decconfig->i_objectProfileIndication,
                                       p_decconfig->p_decoder_specific_info,
                                       p_decconfig->i_decoder_specific_info_len,
                                       &p_fmt->i_codec,
                                       &p_fmt->i_profile ) )
            break;
        /* Unknown entry, but don't touch i_fourcc */
        msg_Warn( p_demux,
                  "unknown objectProfileIndication(0x%x) (Track[ID 0x%x])",
                  p_decconfig->i_objectProfileIndication,
                  p_track->i_track_ID );
        return;
    }

    p_fmt->i_original_fourcc = 0; /* so we don't have MP4A as original fourcc */
    p_fmt->i_bitrate = p_decconfig->i_avg_bitrate;

    CopyExtradata( p_decconfig->p_decoder_specific_info,
                   p_decconfig->i_decoder_specific_info_len,
                   p_fmt );

    if( p_fmt->i_codec == VLC_CODEC_SPU &&
            p_fmt->i_extra >= 16 * 4 )
    {
        for( int i = 0; i < 16; i++ )
        {
            p_fmt->subs.spu.palette[1 + i] =
                    GetDWBE((char*)p_fmt->p_extra + i * 4);
        }
        p_fmt->subs.spu.palette[0] = SPU_PALETTE_DEFINED;
    }
}

static int SetupRTPReceptionHintTrack( demux_t *p_demux, const mp4_track_t *p_track,
                                       const MP4_Box_t *p_sample, es_format_t *p_fmt,
                                       track_config_t *params )
{
    const uint32_t i_sample_type = GetSampleType( p_demux, p_sample );
    p_fmt->i_original_fourcc = i_sample_type;

    const MP4_Box_t *p_sdp = MP4_BoxGet( p_track->p_track, "udta/hnti/sdp " );
    if( !p_sdp )
    {
        msg_Err(p_demux, "Required 'sdp '-box not found");
        return 0;
    }
    char *strtok_state;
    char * pch = strtok_r(BOXDATA(p_sdp)->psz_text, " =\n", &strtok_state); /* media entry */
    if( pch && pch[0] != 'm' )
    {
        msg_Err(p_demux, "No Media entry found in SDP:%s", pch);
        return 0;
    }

    if( !( pch = strtok_r(NULL, " =\n", &strtok_state) ) ) /* media type */
        return 0;
    /* media type has already been checked */
    msg_Dbg(p_demux, "sdp: media type:%s", pch);
    if( !( pch = strtok_r(NULL, " =\n", &strtok_state) ) ) /* port */
        return 0;
    msg_Dbg(p_demux, "sdp: port:%s", pch);
    if( !( pch = strtok_r(NULL, " =\n", &strtok_state) ) ) /* protocol */
        return 0;
    msg_Dbg(p_demux, "sdp: protocol:%s", pch);

    if( !( pch = strtok_r(NULL, " =\n", &strtok_state) ) ) /* fmt */
        return 0;

    bool codec_set = false;
    /* process rtp types until we get an attribute field or end of sdp */
    while( pch && pch[0] != 'a' )
    {
        int rtp_payload = atoi(pch);
        msg_Dbg(p_demux, "sdp: payload type:%d", rtp_payload);

        if( !codec_set )
        {
            /* Payload types 34 and under have a set type and can be identified here */
            switch( rtp_payload )
            {
             case 3:
                p_fmt->i_codec = VLC_CODEC_GSM;
                codec_set = true;
                break;
             default:
                break;
            }
        }
        pch = strtok_r(NULL, " =\n", &strtok_state); /* attribute or additional payload type */
        if( !pch && !codec_set )
            return 0;
    }

    while( pch && pch[0] == 'a' )
    {
        if( !( pch = strtok_r(NULL, " :=\n", &strtok_state) ) ) /* attribute type */
            return 0;
        msg_Dbg(p_demux, "sdp: atrribute type:%s", pch);

        if( !strcmp(pch, "rtpmap") )
        {
            if( !( pch = strtok_r(NULL, " :=\n", &strtok_state) ) ) /* payload type */
                return 0;
            msg_Dbg(p_demux, "sdp: payload type:%s", pch);
            if( !(pch = strtok_r(NULL, " /:=\n", &strtok_state) ) ) /* encoding name */
                return 0;
            msg_Dbg(p_demux, "sdp: encoding name:%s", pch);

            /* Simply adding codec recognition should work for most codecs */
            /* Codecs using slices need their picture constructed from sample */
            if( !strcmp(pch, "H264") )
            {
                p_fmt->i_codec = VLC_CODEC_H264;
                /* ******* sending AnnexB ! */
                p_fmt->b_packetized = false;
            }
            else if( !strcmp(pch, "GSM") )
            {
                p_fmt->i_codec = VLC_CODEC_GSM;
            }
            else if( !strcmp(pch, "Speex") )
            {
                p_fmt->i_codec = VLC_CODEC_SPEEX;
            }
            else if( !codec_set )
            {
                msg_Err(p_demux, "Support for codec contained in RTP \
                        Reception Hint Track RTP stream has not been added");
                return 0;
            }

            if( !( pch = strtok_r(NULL, " :=\n", &strtok_state) ) ) /* clock rate */
                return 0;
            int clock_rate = atoi(pch);
            msg_Dbg(p_demux, "sdp clock rate:%d", clock_rate);
            if( p_fmt->i_cat == AUDIO_ES )
                p_fmt->audio.i_rate = clock_rate;
        }
        pch = strtok_r(NULL, " =\n", &strtok_state); /* next attribute */
    }

    const MP4_Box_t *p_tims = MP4_BoxGet(p_sample, "tims");
    if( p_tims && BOXDATA(p_tims) && BOXDATA(p_tims)->i_timescale )
    {
        params->i_timescale_override = BOXDATA(p_tims)->i_timescale;
    }
    else
    {
        msg_Warn(p_demux, "Missing mandatory box tims");
        return 0;
    }
#if 0
    const MP4_Box_t *p_tssy = MP4_BoxGet(p_sample, "tssy");
    if( p_tssy && BOXDATA(p_tssy) )
    {
        /* take the 2 last bits which indicate the synchronization mode */
        params->sync_mode = (RTP_timstamp_synchronization_t)
                            BOXDATA(p_tssy)->i_reserved_timestamp_sync & 0x03;
    }

    const MP4_Box_t *p_tsro = MP4_BoxGet(p_sample, "tsro");
    if( p_tsro && BOXDATA(p_tsro) )
        params->i_tsro_offset = BOXDATA(p_tsro)->i_offset;
    else
        msg_Dbg(p_demux, "No tsro box present");
    msg_Dbg(p_demux, "setting tsro: %" PRId32, p_track->i_tsro_offset);
#endif
    return 1;
}


int SetupVideoES( demux_t *p_demux, const mp4_track_t *p_track, const MP4_Box_t *p_sample,
                  es_format_t *p_fmt, track_config_t *p_cfg )
{
    track_config_t trackparams, *params = &trackparams;

    const MP4_Box_data_sample_vide_t *p_vide = p_sample->data.p_sample_vide;
    if(!p_vide)
        return 0;

    const uint32_t i_sample_type = GetSampleType( p_demux, p_sample );

    p_fmt->video.i_width = p_vide->i_width;
    p_fmt->video.i_height = p_vide->i_height;
    p_fmt->video.i_bits_per_pixel = p_vide->i_depth;

    /* fall on display size */
    if( p_fmt->video.i_width <= 0 )
        p_fmt->video.i_width = p_track->i_width;
    if( p_fmt->video.i_height <= 0 )
        p_fmt->video.i_height = p_track->i_height;

    /* Find out apect ratio from display size */
    if( p_track->i_width > 0 && p_track->i_height > 0 &&
        /* Work-around buggy muxed files */
        p_vide->i_width != p_track->i_width )
    {
        p_fmt->video.i_sar_num = p_track->i_width  * p_fmt->video.i_height;
        p_fmt->video.i_sar_den = p_track->i_height * p_fmt->video.i_width;
    }

    /* Support for cropping (eg. in H263 files) */
    p_fmt->video.i_visible_width = p_fmt->video.i_width;
    p_fmt->video.i_visible_height = p_fmt->video.i_height;

    /* Rotation */
    switch( (int)p_track->f_rotation ) {
        case 90:
            p_fmt->video.orientation = ORIENT_ROTATED_90;
            break;
        case 180:
            if (p_track->i_flip == 1) {
                p_fmt->video.orientation = ORIENT_VFLIPPED;
            } else {
                p_fmt->video.orientation = ORIENT_ROTATED_180;
            }
            break;
        case 270:
            p_fmt->video.orientation = ORIENT_ROTATED_270;
            break;
    }

    /* Flip, unless already flipped */
    if (p_track->i_flip == 1 && (int)p_track->f_rotation != 180) {
        video_transform_t transform = (video_transform_t)p_fmt->video.orientation;
        /* Flip first then rotate */
        p_fmt->video.orientation = ORIENT_HFLIPPED;
        video_format_TransformBy(&p_fmt->video, transform);
    }

    /* Set 360 video mode */
    p_fmt->video.projection_mode = PROJECTION_MODE_RECTANGULAR;
    const MP4_Box_t *p_uuid = MP4_BoxGet( p_track->p_track, "uuid" );
    for( ; p_uuid; p_uuid = p_uuid->p_next)
    {
        if( p_uuid->i_type == ATOM_uuid
            && !CmpUUID( &p_uuid->i_uuid, &XML360BoxUUID )
            && p_uuid->data.p_360 )
        {
            p_fmt->video.projection_mode = p_uuid->data.p_360->i_projection_mode;
            switch (p_uuid->data.p_360->e_stereo_mode)
            {
            case XML360_STEREOSCOPIC_TOP_BOTTOM:
                p_fmt->video.multiview_mode = MULTIVIEW_STEREO_TB;
                break;
            case XML360_STEREOSCOPIC_LEFT_RIGHT:
                p_fmt->video.multiview_mode = MULTIVIEW_STEREO_SBS;
                break;
            default:
                p_fmt->video.multiview_mode = MULTIVIEW_2D;
                break;
            }
        }
    }

    const MP4_Box_t *p_st3d = MP4_BoxGet( p_sample, "st3d" );
    if (p_st3d && BOXDATA(p_st3d))
    {
        switch( BOXDATA(p_st3d)->i_stereo_mode )
        {
        case ST3D_MONOSCOPIC:
            p_fmt->video.multiview_mode = MULTIVIEW_2D;
            break;
        case ST3D_STEREOSCOPIC_TOP_BOTTOM:
            p_fmt->video.multiview_mode = MULTIVIEW_STEREO_TB;
            break;
        case ST3D_STEREOSCOPIC_LEFT_RIGHT:
            p_fmt->video.multiview_mode = MULTIVIEW_STEREO_SBS;
            break;
        default:
            msg_Warn( p_demux, "Unknown stereo mode %d", BOXDATA(p_st3d)->i_stereo_mode );
            break;
        }
    }
    else
    {
        for( p_uuid = MP4_BoxGet( p_sample, "uuid" ); p_uuid;
             p_uuid = p_uuid->p_next )
        {
            if( p_uuid->i_type == ATOM_uuid &&
               !CmpUUID( &p_uuid->i_uuid, &PS3DDSBoxUUID ) &&
                p_uuid->data.p_binary &&
                p_uuid->data.p_binary->i_blob == 4 &&
                !memcmp( p_uuid->data.p_binary->p_blob, "\x82\x81\x10\x02", 4 ) )
            {
                p_fmt->video.multiview_mode = MULTIVIEW_STEREO_FRAME;
                break;
            }
        }
    }

    const MP4_Box_t *p_prhd = MP4_BoxGet( p_sample, "sv3d/proj/prhd" );
    if (p_prhd && BOXDATA(p_prhd))
    {
        p_fmt->video.pose.yaw = BOXDATA(p_prhd)->f_pose_yaw_degrees;
        p_fmt->video.pose.pitch = BOXDATA(p_prhd)->f_pose_pitch_degrees;
        p_fmt->video.pose.roll = BOXDATA(p_prhd)->f_pose_roll_degrees;
    }

    const MP4_Box_t *p_equi = MP4_BoxGet( p_sample, "sv3d/proj/equi" );
    const MP4_Box_t *p_cbmp = MP4_BoxGet( p_sample, "sv3d/proj/cbmp" );
    if (p_equi && BOXDATA(p_equi))
        p_fmt->video.projection_mode = PROJECTION_MODE_EQUIRECTANGULAR;
    else if (p_cbmp && BOXDATA(p_cbmp))
        p_fmt->video.projection_mode = PROJECTION_MODE_CUBEMAP_LAYOUT_STANDARD;

    /* It's a little ugly but .. there are special cases */
    switch( i_sample_type )
    {
        case( VLC_FOURCC( 's', '2', '6', '3' ) ):
            p_fmt->i_codec = VLC_CODEC_H263;
            break;
        case VLC_FOURCC('y','v','1','2'):
            p_fmt->i_codec = VLC_CODEC_YV12;
            break;
        case VLC_FOURCC('y','u','v','2'):
            p_fmt->i_codec = VLC_CODEC_YUV2;
            break;
        case VLC_FOURCC('A','B','G','R'):
            p_fmt->i_codec = VLC_CODEC_ARGB;
            p_fmt->video.i_rmask = 0x0000FF;
            p_fmt->video.i_gmask = 0x00FF00;
            p_fmt->video.i_bmask = 0xFF0000;
            break;
        case VLC_FOURCC('2','4','B','G'):
            p_fmt->i_codec = VLC_CODEC_RGB24;
            p_fmt->video.i_rmask = 0x0000FF;
            p_fmt->video.i_gmask = 0x00FF00;
            p_fmt->video.i_bmask = 0xFF0000;
            break;
        case VLC_FOURCC('r','a','w',' '):
            switch( p_vide->i_depth ) {
                case 16:
                    p_fmt->i_codec = VLC_CODEC_RGB15;
                    break;
                case 24:
                    p_fmt->i_codec = VLC_CODEC_RGB24;
                    break;
                case 32:
                    p_fmt->i_codec = VLC_CODEC_ARGB;
                    break;
                case 32 + 8:
                    p_fmt->i_codec = VLC_CODEC_GREY;
                    break;
                default:
                    msg_Dbg( p_demux, "Unrecognized raw video format (depth = %d)",
                             p_vide->i_depth );
                    p_fmt->i_codec = i_sample_type;
                    break;
            }
            break;
        case( VLC_FOURCC( 'r', 'r', 't', 'p' ) ): /* RTP Reception Hint Track */
        {
            if( !SetupRTPReceptionHintTrack( p_demux, p_track, p_sample, p_fmt, params ) )
                p_fmt->i_codec = i_sample_type;
            break;
        }
        default:
            p_fmt->i_codec = i_sample_type;
            break;
    }


    /* Read extensions */

    /* Set up A/R from extension atom */
    const MP4_Box_t *p_pasp = MP4_BoxGet( p_sample, "pasp" );
    if( p_pasp && BOXDATA(p_pasp) && BOXDATA(p_pasp)->i_horizontal_spacing > 0 &&
                  BOXDATA(p_pasp)->i_vertical_spacing > 0 )
    {
        p_fmt->video.i_sar_num = BOXDATA(p_pasp)->i_horizontal_spacing;
        p_fmt->video.i_sar_den = BOXDATA(p_pasp)->i_vertical_spacing;
    }

    const MP4_Box_t *p_fiel = MP4_BoxGet( p_sample, "fiel" );
    if( p_fiel && BOXDATA(p_fiel) )
    {
        p_cfg->i_block_flags = BOXDATA(p_fiel)->i_flags;
    }

    const MP4_Box_t *p_colr = MP4_BoxGet( p_sample, "colr" );
    if ( p_colr != NULL )
    {
        if ( BOXDATA(p_colr)->i_type == VLC_FOURCC( 'n', 'c', 'l', 'c' ) ||
             BOXDATA(p_colr)->i_type == VLC_FOURCC( 'n', 'c', 'l', 'x' ) )
        {
            p_fmt->video.primaries =
                    iso_23001_8_cp_to_vlc_primaries( BOXDATA( p_colr )->nclc.i_primary_idx );
            p_fmt->video.transfer =
                    iso_23001_8_tc_to_vlc_xfer( BOXDATA( p_colr )->nclc.i_transfer_function_idx );
            p_fmt->video.space =
                    iso_23001_8_mc_to_vlc_coeffs( BOXDATA( p_colr )->nclc.i_matrix_idx );
            if ( BOXDATA(p_colr)->i_type == VLC_FOURCC( 'n', 'c', 'l', 'x' ) &&
                    (BOXDATA(p_colr)->nclc.i_full_range >> 7) != 0 )
                p_fmt->video.color_range = COLOR_RANGE_FULL;
            else
                p_fmt->video.color_range = COLOR_RANGE_LIMITED;
        }
    }

    SetupGlobalExtensions( p_sample, p_fmt );

    /* now see if esds is present and if so create a data packet
        with decoder_specific_info  */
    MP4_Box_t *p_esds = MP4_BoxGet( p_sample, "esds" );
    if ( p_esds && BOXDATA(p_esds) && BOXDATA(p_esds)->es_descriptor.p_decConfigDescr )
    {
        assert(i_sample_type == ATOM_mp4v);
        SetupESDS( p_demux, p_track, BOXDATA(p_esds)->es_descriptor.p_decConfigDescr, p_fmt );
    }
    else switch( i_sample_type )
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
            p_fmt->i_codec = VLC_CODEC_MPGV;
            break;
        /* qt decoder, send the complete chunk */
        case VLC_CODEC_SVQ1:
        case VLC_CODEC_SVQ3:
        case VLC_FOURCC( 'V', 'P', '3', '1' ):
        case VLC_FOURCC( '3', 'I', 'V', '1' ):
        case VLC_FOURCC( 'Z', 'y', 'G', 'o' ):
        {
            CopyExtradata( p_sample->data.p_sample_vide->p_qt_image_description,
                           p_sample->data.p_sample_vide->i_qt_image_description,
                           p_fmt );
            break;
        }

        case VLC_FOURCC( 'A', 'V', 'j', '2' ):
            p_fmt->i_codec = VLC_CODEC_JPEG2000;
            /* final decoded resolution stored in ARES w, h, nbfields to group
             * but since avcodec can't tell... */
            break;

        case VLC_FOURCC('j', 'p', 'e', 'g'):
            p_fmt->i_codec = VLC_CODEC_MJPG;
           break;

        case VLC_CODEC_FFV1:
        {
            MP4_Box_t *p_binary = MP4_BoxGet( p_sample, "glbl" );
            if( p_binary && BOXDATA(p_binary) )
            {
                CopyExtradata( BOXDATA(p_binary)->p_blob,
                               BOXDATA(p_binary)->i_blob,
                               p_fmt );
            }
            break;
        }

        case VLC_FOURCC( 'v', 'c', '-', '1' ):
        {
            MP4_Box_t *p_dvc1 = MP4_BoxGet( p_sample, "dvc1" );
            if( p_dvc1 && BOXDATA(p_dvc1) )
            {
                CopyExtradata( BOXDATA(p_dvc1)->p_vc1,
                               BOXDATA(p_dvc1)->i_vc1,
                               p_fmt );
            }
            else
            {
                msg_Err( p_demux, "missing dvc1" );
            }
            break;
        }

        case ATOM_av01:
        {
            static_assert(ATOM_av01 == VLC_CODEC_AV1, "VLC_CODEC_AV1 != ATOM_av01");
            MP4_Box_t *p_av1C = MP4_BoxGet( p_sample, "av1C" );
            if( p_av1C && BOXDATA(p_av1C) )
            {
                p_fmt->i_profile = BOXDATA(p_av1C)->i_profile;
                p_fmt->i_level = BOXDATA(p_av1C)->i_level;
                CopyExtradata( BOXDATA(p_av1C)->p_av1C,
                               BOXDATA(p_av1C)->i_av1C,
                               p_fmt );
            }
            break;
        }

        /* avc1: send avcC (h264 without annexe B, ie without start code)*/
        case VLC_FOURCC( 'a', 'v', 'c', '3' ):
        case VLC_FOURCC( 'a', 'v', 'c', '1' ):
        case VLC_FOURCC( 'd', 'v', 'a', '1' ): /* DolbyVision */
        case VLC_FOURCC( 'd', 'v', 'a', 'v' ): /* DolbyVision */
        {
            MP4_Box_t *p_avcC = MP4_BoxGet( p_sample, "avcC" );

            if( p_avcC && BOXDATA(p_avcC) )
            {
                p_fmt->i_profile = BOXDATA(p_avcC)->i_profile;
                p_fmt->i_level = BOXDATA(p_avcC)->i_level;
                CopyExtradata( BOXDATA(p_avcC)->p_avcC,
                               BOXDATA(p_avcC)->i_avcC,
                               p_fmt );
            }
            else
            {
                msg_Err( p_demux, "missing avcC" );
            }
            break;
        }
        case VLC_FOURCC( 'h', 'v', 'c', '1' ):
        case VLC_FOURCC( 'h', 'e', 'v', '1' ):
        case VLC_FOURCC( 'd', 'v', 'h', 'e' ): /* DolbyVision */
        case VLC_FOURCC( 'd', 'v', 'h', '1' ): /* DolbyVision */
        {
            MP4_Box_t *p_hvcC = MP4_BoxGet( p_sample, "hvcC" );

            /* Handle DV fourcc collision at demux level */
            if( i_sample_type == VLC_FOURCC( 'd', 'v', 'h', '1' ) )
                p_fmt->i_codec = VLC_FOURCC( 'd', 'v', 'h', 'e' );

            if( p_hvcC && p_hvcC->data.p_binary )
            {
                CopyExtradata( p_hvcC->data.p_binary->p_blob,
                               p_hvcC->data.p_binary->i_blob,
                               p_fmt );
            }
            else
            {
                msg_Err( p_demux, "missing hvcC" );
            }
            break;
        }

        case ATOM_vp08:
        case ATOM_vp09:
        case ATOM_vp10:
        {
            const MP4_Box_t *p_vpcC = MP4_BoxGet(  p_sample, "vpcC" );
            if( p_vpcC && BOXDATA(p_vpcC) )
            {
                const MP4_Box_data_vpcC_t *p_data = BOXDATA(p_vpcC);
                if( i_sample_type == ATOM_vp10 )
                    p_fmt->i_codec = VLC_CODEC_VP10;
                else if( i_sample_type == ATOM_vp09 )
                    p_fmt->i_codec = VLC_CODEC_VP9;
                else
                    p_fmt->i_codec = VLC_CODEC_VP8;
                p_fmt->i_profile = p_data->i_profile;
                p_fmt->i_level = p_data->i_level;

                if( p_data->i_version == 0 ) /* old deprecated */
                {
                    const uint8_t colorspacesmapping[] =
                    {
                        COLOR_SPACE_UNDEF,
                        COLOR_SPACE_BT601,
                        COLOR_SPACE_BT709,
                        COLOR_SPACE_SMPTE_170,
                        COLOR_SPACE_SMPTE_240,
                        COLOR_SPACE_BT2020,
                        COLOR_SPACE_BT2020,
                        COLOR_SPACE_SRGB,
                    };
                    if( p_data->i_color_primaries < ARRAY_SIZE(colorspacesmapping) )
                        p_fmt->video.space = colorspacesmapping[p_data->i_color_primaries];

                    if( p_data->i_xfer_function == 0 )
                        p_fmt->video.transfer = TRANSFER_FUNC_BT709;
                    else if ( p_data->i_xfer_function == 1 )
                        p_fmt->video.transfer = TRANSFER_FUNC_SMPTE_ST2084;
                }
                else
                {
                    p_fmt->video.primaries =
                            iso_23001_8_cp_to_vlc_primaries( p_data->i_color_primaries );
                    p_fmt->video.transfer =
                            iso_23001_8_tc_to_vlc_xfer( p_data->i_xfer_function );
                    p_fmt->video.space =
                            iso_23001_8_mc_to_vlc_coeffs( p_data->i_matrix_coeffs );
                }

                p_fmt->video.color_range = p_data->i_fullrange ? COLOR_RANGE_FULL : COLOR_RANGE_LIMITED;
                p_fmt->video.i_bits_per_pixel = p_data->i_bit_depth;

                CopyExtradata( p_data->p_codec_init_data,
                               p_data->i_codec_init_datasize,
                               p_fmt );

                const MP4_Box_t *p_SmDm = MP4_BoxGet( p_sample, "SmDm" );
                if( !p_SmDm )
                    p_SmDm = MP4_BoxGet( p_sample, "mdcv" );
                if( p_SmDm && BOXDATA(p_SmDm) )
                {
                    memcpy( p_fmt->video.mastering.primaries,
                            BOXDATA(p_SmDm)->primaries, sizeof(uint16_t) * 6 );
                    memcpy( p_fmt->video.mastering.white_point,
                            BOXDATA(p_SmDm)->white_point, sizeof(uint16_t) * 2 );
                    p_fmt->video.mastering.max_luminance = BOXDATA(p_SmDm)->i_luminanceMax;
                    p_fmt->video.mastering.min_luminance = BOXDATA(p_SmDm)->i_luminanceMin;
                }

                const MP4_Box_t *p_CoLL = MP4_BoxGet( p_sample, "CoLL" );
                if( !p_CoLL )
                    p_CoLL = MP4_BoxGet( p_sample, "clli" );
                if( p_CoLL && BOXDATA(p_CoLL) )
                {
                    p_fmt->video.lighting.MaxCLL = BOXDATA(p_CoLL)->i_maxCLL;
                    p_fmt->video.lighting.MaxFALL = BOXDATA(p_CoLL)->i_maxFALL;
                }
            }
        }
        break;

        case ATOM_WMV3:
            p_cfg->p_asf = MP4_BoxGet( p_sample, "ASF " );
            /* fallthrough */
        case ATOM_H264:
        case VLC_FOURCC('W','V','C','1'):
        {
            MP4_Box_t *p_strf = MP4_BoxGet(  p_sample, "strf", 0 );
            if ( p_strf && BOXDATA(p_strf) )
            {
                p_fmt->video.i_width = BOXDATA(p_strf)->bmiHeader.biWidth;
                p_fmt->video.i_visible_width = p_fmt->video.i_width;
                p_fmt->video.i_height = BOXDATA(p_strf)->bmiHeader.biHeight;
                p_fmt->video.i_visible_height =p_fmt->video.i_height;
                p_fmt->video.i_bits_per_pixel = BOXDATA(p_strf)->bmiHeader.biBitCount;
                CopyExtradata( BOXDATA(p_strf)->p_extra,
                               BOXDATA(p_strf)->i_extra,
                               p_fmt );
            }
            break;
        }

        case VLC_FOURCC( 'a', 'i', '5', 'p' ):
        case VLC_FOURCC( 'a', 'i', '5', 'q' ):
        case VLC_FOURCC( 'a', 'i', '5', '2' ):
        case VLC_FOURCC( 'a', 'i', '5', '3' ):
        case VLC_FOURCC( 'a', 'i', '5', '5' ):
        case VLC_FOURCC( 'a', 'i', '5', '6' ):
        case VLC_FOURCC( 'a', 'i', '1', 'p' ):
        case VLC_FOURCC( 'a', 'i', '1', 'q' ):
        case VLC_FOURCC( 'a', 'i', '1', '2' ):
        case VLC_FOURCC( 'a', 'i', '1', '3' ):
        case VLC_FOURCC( 'a', 'i', '1', '5' ):
        case VLC_FOURCC( 'a', 'i', '1', '6' ):
        {
            if( !p_fmt->i_extra && p_fmt->video.i_width < UINT16_MAX &&
                p_fiel && BOXDATA(p_fiel) )
            {
                p_fmt->p_extra =
                        AVCi_create_AnnexB( p_fmt->video.i_width,
                                            !!BOXDATA(p_fiel)->i_flags, &p_fmt->i_extra );
            }
            break;
        }

        default:
            msg_Dbg( p_demux, "Unrecognized FourCC %4.4s", (char *)&i_sample_type );
            break;
    }

    return 1;
}

static bool SetupAudioFromWaveFormatEx( const MP4_Box_t *p_WMA2, es_format_t *p_fmt )
{
    if( p_WMA2 && BOXDATA(p_WMA2) )
    {
        wf_tag_to_fourcc(BOXDATA(p_WMA2)->Format.wFormatTag, &p_fmt->i_codec, NULL);
        p_fmt->audio.i_channels = BOXDATA(p_WMA2)->Format.nChannels;
        p_fmt->audio.i_rate = BOXDATA(p_WMA2)->Format.nSamplesPerSec;
        p_fmt->i_bitrate = BOXDATA(p_WMA2)->Format.nAvgBytesPerSec * 8;
        p_fmt->audio.i_blockalign = BOXDATA(p_WMA2)->Format.nBlockAlign;
        p_fmt->audio.i_bitspersample = BOXDATA(p_WMA2)->Format.wBitsPerSample;
        CopyExtradata( BOXDATA(p_WMA2)->p_extra,
                       BOXDATA(p_WMA2)->i_extra,
                       p_fmt );
        return true;
    }
    return false;
}

int SetupAudioES( demux_t *p_demux, const mp4_track_t *p_track,
                  const MP4_Box_t *p_sample, es_format_t *p_fmt,
                  track_config_t *p_cfg )
{
    const MP4_Box_data_sample_soun_t *p_soun = p_sample->data.p_sample_soun;
    if(!p_soun)
        return 0;

    const uint32_t i_sample_type = GetSampleType( p_demux, p_sample );
    p_fmt->i_original_fourcc = i_sample_type;

    p_fmt->audio.i_channels = p_soun->i_channelcount;
    p_fmt->audio.i_rate = p_soun->i_sampleratehi;
    if( p_soun->i_qt_version == 0 ) /* otherwise defaults to meaningless 16 */
    {
        p_fmt->audio.i_bitspersample = p_soun->i_samplesize;
        p_fmt->i_bitrate = p_soun->i_channelcount * p_soun->i_sampleratehi *
                           p_soun->i_samplesize;
    }

    /* Endianness atom */
    const MP4_Box_t *p_enda = MP4_BoxGet( p_sample, "wave/enda" );
    if( !p_enda )
        p_enda = MP4_BoxGet( p_sample, "enda" );

    /* It's a little ugly but .. there are special cases */
    switch( i_sample_type )
    {
        case( VLC_FOURCC( 'r', 'r', 't', 'p' ) ): /* RTP Reception Hint Track */
        {
            if( !SetupRTPReceptionHintTrack( p_demux, p_track, p_sample, p_fmt, p_cfg ) )
                return 0;
            break;
        }
        case ATOM_agsm: /* Apple gsm 33 bytes != MS GSM (agsm fourcc, 65 bytes) */
            p_fmt->i_codec = VLC_CODEC_GSM;
            break;
        case( VLC_FOURCC( '.', 'm', 'p', '3' ) ):
        case( VLC_FOURCC( 'm', 's', 0x00, 0x55 ) ):
        {
            p_fmt->i_codec = VLC_CODEC_MP3;
            p_fmt->b_packetized = false;
            break;
        }
        case ATOM_XiVs:
        {
            const MP4_Box_t *p_vCtH = MP4_BoxGet( p_sample, "wave/vCtH" ); /* kCookieTypeVorbisHeader */
            const MP4_Box_t *p_vCtd = MP4_BoxGet( p_sample, "wave/vCt#" ); /* kCookieTypeVorbisComments */
            const MP4_Box_t *p_vCtC = MP4_BoxGet( p_sample, "wave/vCtC" ); /* kCookieTypeVorbisCodebooks */
            if( p_vCtH && p_vCtH->data.p_binary &&
                p_vCtd && p_vCtd->data.p_binary &&
                p_vCtC && p_vCtC->data.p_binary )
            {
                unsigned headers_sizes[3] = {
                    p_vCtH->data.p_binary->i_blob,
                    p_vCtd->data.p_binary->i_blob,
                    p_vCtC->data.p_binary->i_blob
                };

                const void * headers[3] = {
                    p_vCtH->data.p_binary->p_blob,
                    p_vCtd->data.p_binary->p_blob,
                    p_vCtC->data.p_binary->p_blob
                };

                if( xiph_PackHeaders( &p_fmt->i_extra, &p_fmt->p_extra,
                                      headers_sizes, headers, 3 ) == VLC_SUCCESS )
                {
                    p_fmt->i_codec = VLC_CODEC_VORBIS;
                    p_fmt->b_packetized = false;
                }
            }
            break;
        }
        case ATOM_XiFL:
        {
            const MP4_Box_t *p_fCtS = MP4_BoxGet( p_sample, "wave/fCtS" ); /* kCookieTypeFLACStreaminfo */
            if( p_fCtS && p_fCtS->data.p_binary )
            {
                size_t i_extra = 8 + p_fCtS->data.p_binary->i_blob;
                uint8_t *p_extra = malloc(i_extra);
                if( p_extra )
                {
                    p_fmt->i_extra = i_extra;
                    p_fmt->p_extra = p_extra;
                    memcpy( p_extra, "fLaC", 4 );
                    SetDWBE( &p_extra[4], p_fCtS->data.p_binary->i_blob ); /* want the lowest 24bits */
                    p_extra[4] = 0x80; /* 0x80 Last metablock | 0x00 StreamInfo */
                    memcpy( &p_extra[8], p_fCtS->data.p_binary->p_blob, p_fCtS->data.p_binary->i_blob );

                    p_fmt->i_codec = VLC_CODEC_FLAC;
                    p_fmt->b_packetized = false;
                }
            }
            break;
        }
        case ATOM_fLaC:
        {
            const MP4_Box_t *p_dfLa = MP4_BoxGet(  p_sample, "dfLa", 0 );
            if( p_dfLa && p_dfLa->data.p_binary->i_blob > 4 &&
                GetDWBE(p_dfLa->data.p_binary->p_blob) == 0 ) /* fullbox header, avoids creating dedicated parser */
            {
                size_t i_extra = p_dfLa->data.p_binary->i_blob;
                uint8_t *p_extra = malloc(i_extra);
                if( likely( p_extra ) )
                {
                    p_fmt->i_extra = i_extra;
                    p_fmt->p_extra = p_extra;
                    memcpy( p_extra, p_dfLa->data.p_binary->p_blob, p_dfLa->data.p_binary->i_blob);
                    memcpy( p_extra, "fLaC", 4 );
                    p_fmt->i_codec = VLC_CODEC_FLAC;
                }
            }
            break;
        }
        case( ATOM_eac3 ):
        {
            p_fmt->i_codec = VLC_CODEC_EAC3;
            /* TS 102.366. F6 The values of the ChannelCount and SampleSize fields
             *             within the EC3SampleEntry Box shall be ignored. */
            p_fmt->audio.i_channels = 0;
            p_fmt->audio.i_bitspersample = 0;

            const MP4_Box_t *p_dec3 = MP4_BoxGet(  p_sample, "dec3", 0 );
            if( p_dec3 && BOXDATA(p_dec3) )
            {
                p_fmt->i_bitrate = BOXDATA(p_dec3)->i_data_rate * 1000;
            }
            break;
        }
        case( ATOM_AC3 ):
        case( ATOM_ac3 ):
        {
            p_fmt->i_codec = VLC_CODEC_A52;
            /* TS 102.366. F3 The values of the ChannelCount and SampleSize fields
             *             within the AC3SampleEntry Box shall be ignored */
            p_fmt->audio.i_channels = 0;
            p_fmt->audio.i_bitspersample = 0;

            MP4_Box_t *p_dac3 = MP4_BoxGet(  p_sample, "dac3", 0 );
            if( p_dac3 && BOXDATA(p_dac3) )
            {
                static const int pi_bitrate[] = {
                     32,  40,  48,  56,
                     64,  80,  96, 112,
                    128, 160, 192, 224,
                    256, 320, 384, 448,
                    512, 576, 640,
                };
                p_fmt->i_bitrate = 0;
                if( BOXDATA(p_dac3)->i_bitrate_code < sizeof(pi_bitrate)/sizeof(*pi_bitrate) )
                {
                    p_fmt->i_bitrate = pi_bitrate[BOXDATA(p_dac3)->i_bitrate_code] * 1000;

                    if (pi_bitrate[BOXDATA(p_dac3)->i_bitrate_code] == 640
                     && BOXDATA(p_dac3)->i_acmod == 7
                     && BOXDATA(p_dac3)->i_lfeon == 1)
                    {
                        /* DD+ can be an optional codec, and is deployed as an
                         * extension to a "core" AC-3 5.1 640kbit/s audiotrack.
                         * In that case, the AC-3 track might have an EAC3
                         * extension, therefore trigger the A52 packetizer to
                         * detect it (this is needed for aout passhthrough
                         * configuration). */

                        p_fmt->b_packetized = false;
                    }
                }
            }
            break;
        }
        case VLC_CODEC_TRUEHD:
            /* spec violation: 32 bits rate instead of fixed point */
            p_fmt->audio.i_rate = (p_soun->i_sampleratehi << 16) | p_soun->i_sampleratelo;
            break;

        case ATOM_dtsc: /* DTS */
        {
            p_fmt->i_codec = VLC_CODEC_DTS;
            p_fmt->i_profile = PROFILE_DTS;
            break;
        }
        case ATOM_dtse: /* DTS LBR */
        {
            p_fmt->i_codec = VLC_CODEC_DTS;
            p_fmt->i_profile = PROFILE_DTS_EXPRESS;
            break;
        }
        case ATOM_dtsh: /* DTS‐HD audio formats */
        case ATOM_dtsl: /* DTS‐HD Lossless formats */
        {
            p_fmt->i_codec = VLC_CODEC_DTS;
            p_fmt->i_profile = PROFILE_DTS_HD;
            break;
        }

        case VLC_FOURCC( 't', 'w', 'o', 's' ):
            p_fmt->i_codec = VLC_CODEC_S16B;
            p_fmt->i_original_fourcc = i_sample_type;
            p_fmt->audio.i_bitspersample = 16;
            break;

        case VLC_FOURCC( 's', 'o', 'w', 't' ):
            p_fmt->i_codec = VLC_CODEC_S16L;
            p_fmt->i_original_fourcc = i_sample_type;
            p_fmt->audio.i_bitspersample = 16;
            break;

        case 0x0000000:
        case( VLC_FOURCC( 'r', 'a', 'w', ' ' ) ):
        case( VLC_FOURCC( 'N', 'O', 'N', 'E' ) ):
        {
            if( (p_soun->i_samplesize+7)/8 == 1 )
            {
                p_fmt->i_codec = VLC_CODEC_U8;
                p_fmt->audio.i_bitspersample = 8;
            }
            else
            {
                p_fmt->i_codec = VLC_CODEC_S16B;
                p_fmt->audio.i_bitspersample = 16;
            }
            p_fmt->i_original_fourcc = p_fmt->i_codec;

            /* Buggy files workaround */
            if( p_track->i_timescale != p_soun->i_sampleratehi && p_soun->i_qt_version == 0 )
            {
                msg_Warn( p_demux, "i_timescale (%"PRId32") != i_sampleratehi "
                          "(%u), making both equal (report any problem).",
                          p_track->i_timescale, p_soun->i_sampleratehi );

                if( p_soun->i_sampleratehi != 0 )
                    p_cfg->i_timescale_override = p_soun->i_sampleratehi;
                else
                    p_fmt->audio.i_rate = p_track->i_timescale;
            }
            break;
        }

        case ATOM_in24:
            p_fmt->i_original_fourcc =
            p_fmt->i_codec = p_enda && BOXDATA(p_enda)->i_little_endian == 1 ?
                                    VLC_CODEC_S24L : VLC_CODEC_S24B;
            break;
        case ATOM_in32:
            p_fmt->i_original_fourcc =
            p_fmt->i_codec = p_enda && BOXDATA(p_enda)->i_little_endian == 1 ?
                                    VLC_CODEC_S32L : VLC_CODEC_S32B;
            break;
        case ATOM_fl32:
            p_fmt->i_original_fourcc =
            p_fmt->i_codec = p_enda && BOXDATA(p_enda)->i_little_endian == 1 ?
                                    VLC_CODEC_F32L : VLC_CODEC_F32B;
            break;
        case ATOM_fl64:
            p_fmt->i_original_fourcc =
            p_fmt->i_codec = p_enda && BOXDATA(p_enda)->i_little_endian == 1 ?
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
                        p_fmt->i_codec = p_formats[i].i_codec;
                        p_fmt->audio.i_bitspersample = p_soun->i_constbitsperchannel;
                        p_fmt->audio.i_blockalign =
                                p_soun->i_channelcount * p_soun->i_constbitsperchannel / 8;
                        p_cfg->i_sample_size_override = p_fmt->audio.i_blockalign;
                        break;
                    }
                }
            }
            break;
        }
        default:
            p_fmt->i_codec = i_sample_type;
            break;
    }


    /* Process extensions */

    /* Lookup for then channels extension */
    const MP4_Box_t *p_chan = MP4_BoxGet( p_sample, "chan" );
    if ( p_chan )
    {
        uint16_t i_vlc_mapping = 0;
        uint8_t i_channels = 0;
        const uint32_t *p_rg_chans_order = NULL;

        if ( CoreAudio_Layout_to_vlc( &BOXDATA(p_chan)->layout,
                                      &i_vlc_mapping, &i_channels,
                                      &p_rg_chans_order ) != VLC_SUCCESS )
        {
            msg_Warn( p_demux, "discarding chan mapping" );
        }
        else if( i_vlc_mapping )
        {
            const unsigned i_bps = aout_BitsPerSample( p_fmt->i_codec );
            /* Uncompressed audio */
            if( i_bps && aout_CheckChannelReorder( p_rg_chans_order, NULL,
                                                   i_vlc_mapping,
                                                   p_cfg->rgi_chans_reordering ) )
                p_cfg->b_chans_reorder = true;

            /* we can only set bitmap for VLC mapping or [re]mapped pcm audio
             * as vlc can't enumerate channels for compressed content */
            if( i_bps )
            {
                p_fmt->audio.i_channels = vlc_popcount(i_vlc_mapping);
                p_fmt->audio.i_physical_channels = i_vlc_mapping;
            }
        }
    }

    const MP4_Box_t *p_srat = MP4_BoxGet( p_sample, "srat" );
    if ( p_srat )
        p_fmt->audio.i_rate = BOXDATA(p_srat)->i_sample_rate;

    SetupGlobalExtensions( p_sample, p_fmt );

    /* now see if esds is present and if so create a data packet
        with decoder_specific_info  */
    MP4_Box_t *p_esds = MP4_BoxGet( p_sample, "esds" );
    if ( !p_esds ) p_esds = MP4_BoxGet( p_sample, "wave/esds" );
    if ( p_esds && BOXDATA(p_esds) && BOXDATA(p_esds)->es_descriptor.p_decConfigDescr )
    {
        assert(i_sample_type == ATOM_mp4a);
        SetupESDS( p_demux, p_track, BOXDATA(p_esds)->es_descriptor.p_decConfigDescr, p_fmt );
    }
    else switch( i_sample_type )
    {
        case VLC_CODEC_AMR_NB:
            p_fmt->audio.i_rate = 8000;
            break;
        case VLC_CODEC_AMR_WB:
            p_fmt->audio.i_rate = 16000;
            break;
        case VLC_CODEC_QDMC:
        case VLC_CODEC_QDM2:
        case VLC_CODEC_ALAC:
        {
            CopyExtradata( p_sample->data.p_sample_soun->p_qt_description,
                           p_sample->data.p_sample_soun->i_qt_description,
                           p_fmt );
            if( p_fmt->i_extra == 56 && i_sample_type == VLC_CODEC_ALAC )
            {
                p_fmt->audio.i_channels = *((uint8_t*)p_fmt->p_extra + 41);
                p_fmt->audio.i_rate = GetDWBE((uint8_t*)p_fmt->p_extra + 52);
            }
            break;
        }
        case VLC_CODEC_ADPCM_MS:
        case VLC_CODEC_ADPCM_IMA_WAV:
        case VLC_CODEC_QCELP:
        {
            p_fmt->audio.i_blockalign = p_sample->data.p_sample_soun->i_bytes_per_frame;
            break;
        }
        case ATOM_WMA2:
        {
            if( SetupAudioFromWaveFormatEx( MP4_BoxGet( p_sample, "wave/WMA2" ), p_fmt ) )
            {
                p_cfg->p_asf = MP4_BoxGet( p_sample, "wave/ASF " );
            }
            else
            {
                msg_Err( p_demux, "missing WMA2 %4.4s", (char*) &p_sample->p_father->i_type );
            }
            break;
        }
        case ATOM_wma: /* isml wmapro */
        {
            if( !SetupAudioFromWaveFormatEx( MP4_BoxGet( p_sample, "wfex" ), p_fmt ) )
                msg_Err( p_demux, "missing wfex for wma" );
            break;
        }

        default:
            if(p_fmt->i_codec == 0)
                msg_Dbg( p_demux, "Unrecognized FourCC %4.4s", (char *)&i_sample_type );
            break;
    }

    /* Ambisonics */
    const MP4_Box_t *p_SA3D = MP4_BoxGet(p_sample, "SA3D");
    if (p_SA3D && BOXDATA(p_SA3D))
        p_fmt->audio.channel_type = AUDIO_CHANNEL_TYPE_AMBISONICS;

    return 1;
}

int SetupSpuES( demux_t *p_demux, const mp4_track_t *p_track,
                const MP4_Box_t *p_sample, es_format_t *p_fmt,
                track_config_t *p_cfg )
{
    const uint32_t i_sample_type = GetSampleType( p_demux, p_sample );

    /* It's a little ugly but .. there are special cases */
    switch( i_sample_type )
    {
        case VLC_FOURCC('s','t','p','p'):
            p_fmt->i_codec = VLC_CODEC_TTML;
            break;
        case ATOM_wvtt:
            p_fmt->i_codec = VLC_CODEC_WEBVTT;
            break;
        case ATOM_c608: /* EIA608 closed captions */
            p_fmt->i_codec = VLC_CODEC_CEA608;
            p_fmt->subs.cc.i_reorder_depth = -1;
            break;
        case ATOM_c708: /* EIA708 closed captions */
            p_fmt->i_codec = VLC_CODEC_CEA708;
            p_fmt->subs.cc.i_reorder_depth = -1;
            break;

        case( VLC_FOURCC( 't', 'e', 'x', 't' ) ):
        case( VLC_FOURCC( 't', 'x', '3', 'g' ) ):
        {
            const MP4_Box_data_sample_generic_t *p_text = p_sample->data.p_sample_gen;
            if(!p_text)
                return 0;

            if( i_sample_type == VLC_FOURCC( 't', 'e', 'x', 't' ) )
                p_fmt->i_codec = VLC_CODEC_QTXT;
            else
                p_fmt->i_codec = VLC_CODEC_TX3G;

            if( p_text->i_data > 4 && GetDWBE(p_text->p_data) & 0xC0000000 )
            {
                p_fmt->i_priority = ES_PRIORITY_SELECTABLE_MIN + 1;
                p_cfg->b_forced_spu = true;
            }

            CopyExtradata( p_text->p_data, p_text->i_data, p_fmt );

            /* FIXME UTF-8 doesn't work here ? */
            if( p_track->b_mac_encoding )
                p_fmt->subs.psz_encoding = strdup( "MAC" );
            else
                p_fmt->subs.psz_encoding = strdup( "UTF-8" );
            break;
        }

        default:
            p_fmt->i_codec = i_sample_type;
            break;
    }

    SetupGlobalExtensions( p_sample, p_fmt );

    /* now see if esds is present and if so create a data packet
        with decoder_specific_info  */
    MP4_Box_t *p_esds = MP4_BoxGet( p_sample, "esds" );
    if ( p_esds && BOXDATA(p_esds) && BOXDATA(p_esds)->es_descriptor.p_decConfigDescr )
    {
        SetupESDS( p_demux, p_track, BOXDATA(p_esds)->es_descriptor.p_decConfigDescr, p_fmt );
    }

    return 1;
}
