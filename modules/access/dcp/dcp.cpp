/*****************************************************************************
 * Copyright (C) 2012-2013 VLC authors and VideoLAN
 *
 * Authors:
 *          Nicolas Bertrand <nico@isf.cc>
 *          Simona-Marinela Prodea <simona dot marinela dot prodea at gmail dot com>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Guillaume Gonnaud
 *          Valentin Vetter <vvetter@outlook.com>
 *          Anthony Giniers
 *          Ludovic Hoareau
 *          Loukmane Dessai
 *          Pierre Villard <pierre dot villard dot fr at gmail dot com>
 *          Claire Etienne
 *          Aurélie Sbinné
 *          Samuel Kerjose
 *          Julien Puyobro
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

/**
 * @file dcp.cpp
 * @brief DCP access-demux module for Digital Cinema Packages using asdcp library
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define KDM_HELP_TEXT          N_("KDM file")
#define KDM_HELP_LONG_TEXT     N_("Path to Key Delivery Message XML file")

/* VLC core API headers */
#include <vlc_common.h>
#include <vlc_demux.h>
#include <vlc_plugin.h>
#include <vlc_xml.h>
#include <vlc_url.h>
#include <vlc_aout.h>

#ifdef _WIN32
# define KM_WIN32
#endif

/* ASDCP headers */
#include <AS_DCP.h>

#include <vector>

#include "dcpparser.h"

using namespace ASDCP;

#define FRAME_BUFFER_SIZE 1302083 /* maximum frame length, in bytes, after
                                     "Digital Cinema System Specification Version 1.2
                                     with Errata as of 30 August 2012" */

/* Forward declarations */
static int Open( vlc_object_t * );
static void Close( vlc_object_t * );

/* Module descriptor */
vlc_module_begin()
    set_shortname( N_( "DCP" ) )
    add_shortcut( "dcp" )
    add_loadfile( "kdm", "", KDM_HELP_TEXT, KDM_HELP_LONG_TEXT, false )
    set_description( N_( "Digital Cinema Package module" ) )
    set_capability( "access_demux", 0 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )
    set_callbacks( Open, Close )
vlc_module_end()

//! Kind of MXF MEDIA TYPE
typedef enum MxfMedia_t {
    MXF_UNKNOWN = 0,
    MXF_PICTURE,
    MXF_AUDIO,
} MxfMedia_t;

union videoReader_t
{
   /* JPEG2000 essence type */
   ASDCP::JP2K::MXFReader *p_PicMXFReader;

   /* JPEG2000 stereoscopic essence type */
   ASDCP::JP2K::MXFSReader *p_PicMXFSReader;

   /* MPEG2 essence type */
   ASDCP::MPEG2::MXFReader *p_VideoMXFReader;
};

struct audioReader_t
{
    PCM::MXFReader *p_AudioMXFReader;
};

/* ASDCP library (version 1.10.48) can handle files having one of the following Essence Types, as defined in AS_DCP.h:
    ESS_UNKNOWN,     // the file is not a supported AS-DCP essence container
    ESS_MPEG2_VES,   // the file contains an MPEG video elementary stream
    ESS_JPEG_2000,   // the file contains one or more JPEG 2000 codestreams
    ESS_PCM_24b_48k, // the file contains one or more PCM audio pairs
    ESS_PCM_24b_96k, // the file contains one or more PCM audio pairs
    ESS_TIMED_TEXT,  // the file contains an XML timed text document and one or more resources
    ESS_JPEG_2000_S, // the file contains one or more JPEG 2000 codestream pairs (stereoscopic).

    The classes for handling these essence types are defined in AS_DCP.h and are different for each essence type, respectively. The demux_sys_t structure contains members for handling each of these essence types.
*/

class demux_sys_t
{
 public:
    /* ASDCP Picture Essence Type */
    EssenceType_t PictureEssType;

    /* ASDCP Video MXF Reader */
    std::vector<videoReader_t> v_videoReader;

    /* ASDCP Audio MXF Reader */
    std::vector<audioReader_t> v_audioReader;

    /* audio buffer size */
    uint32_t i_audio_buffer;

    /* elementary streams */
    es_out_id_t *p_video_es;
    es_out_id_t *p_audio_es;

    /* DCP object */
    dcp_t *p_dcp;

    /* current absolute frame number */
    uint32_t frame_no;

    /* frame rate */
    int frame_rate_num;
    int frame_rate_denom;

    /* total number of frames */
    uint32_t frames_total;

    /* current video reel */
    unsigned int i_video_reel;

    /* current audio reel */
    unsigned int i_audio_reel;

    uint8_t i_chans_to_reorder;            /* do we need channel reordering */
    uint8_t pi_chan_table[AOUT_CHAN_MAX];
    uint8_t i_channels;

    mtime_t i_pts;

    demux_sys_t():
        PictureEssType ( ESS_UNKNOWN ),
        v_videoReader(),
        v_audioReader(),
        p_video_es( NULL ),
        p_audio_es( NULL ),
        p_dcp( NULL ),
        frame_no( 0 ),
        frames_total( 0 ),
        i_video_reel( 0 ),
        i_audio_reel( 0 ),
        i_pts( 0 )
    {}

    ~demux_sys_t()
    {
        switch ( PictureEssType )
        {
            case ESS_UNKNOWN:
                break;
            case ESS_JPEG_2000:
                for ( unsigned int i = 0; i < v_videoReader.size(); i++ )
                {
                    delete v_videoReader[i].p_PicMXFReader;
                }
                break;
            case ESS_JPEG_2000_S:
                for ( unsigned int i = 0; i < v_videoReader.size(); i++ )
                {
                    delete v_videoReader[i].p_PicMXFSReader;
                }
                break;
            case ESS_MPEG2_VES:
                for ( unsigned int i = 0; i < v_videoReader.size(); i++ )
                {
                    delete v_videoReader[i].p_VideoMXFReader;
                }
                break;
            default:
                break;
        }

        for ( unsigned int i = 0; i < v_audioReader.size(); i++ )
        {
            delete v_audioReader[i].p_AudioMXFReader;
        }

        delete p_dcp;
    }
};

/*TODO: basic correlation between SMPTE S428-3/S429-2
 * Real sound is more complex with case of left/right surround, ...
 * and hearing impaired/Narration channels */

/* 1 channel: mono */
static const uint32_t i_channels_1[] =
{ AOUT_CHAN_LEFT, 0 };

/* 2 channels: stereo */
static const uint32_t i_channels_2[]=
{ AOUT_CHAN_LEFT, AOUT_CHAN_RIGHT, 0 };

/* 4 channels */
static const uint32_t i_channels_4[] =
{   AOUT_CHAN_LEFT, AOUT_CHAN_RIGHT, AOUT_CHAN_CENTER,
    AOUT_CHAN_LFE, 0  };

/* 6 channels: 5.1 */
static const uint32_t i_channels_6[] =
{   AOUT_CHAN_LEFT, AOUT_CHAN_RIGHT,      AOUT_CHAN_CENTER,
    AOUT_CHAN_LFE,  AOUT_CHAN_REARLEFT,   AOUT_CHAN_REARRIGHT,
    0 };

/* 7 channels: 6.1 */
static const uint32_t i_channels_7[] =
{   AOUT_CHAN_LEFT,       AOUT_CHAN_RIGHT,    AOUT_CHAN_CENTER,
    AOUT_CHAN_LFE,        AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_REARCENTER, 0  };

/* 8 channels:  7.1 */
static const uint32_t i_channels_8[] =
{   AOUT_CHAN_LEFT,        AOUT_CHAN_RIGHT,      AOUT_CHAN_CENTER,
    AOUT_CHAN_LFE,         AOUT_CHAN_REARLEFT,   AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_MIDDLELEFT,  AOUT_CHAN_MIDDLERIGHT, 0 };

/* 9 channels; 8.1 */
static const uint32_t i_channels_9[] =
{   AOUT_CHAN_LEFT,        AOUT_CHAN_RIGHT,      AOUT_CHAN_CENTER,
    AOUT_CHAN_LFE,         AOUT_CHAN_REARLEFT,   AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_MIDDLELEFT,  AOUT_CHAN_MIDDLERIGHT, AOUT_CHAN_REARCENTER, 0 };

static const uint32_t *pi_channels_aout [] =
{   NULL,
    i_channels_1,
    i_channels_2,
    NULL,
    i_channels_4,
    NULL,
    i_channels_6,
    i_channels_7,
    i_channels_8,
    i_channels_9 };

static const unsigned i_channel_mask[] =
{    0,
    AOUT_CHAN_LEFT,
    AOUT_CHANS_STEREO,
    0,
    AOUT_CHANS_3_1,
    0,
    AOUT_CHANS_5_1,
    AOUT_CHANS_6_1_MIDDLE,
    AOUT_CHANS_7_1,
    AOUT_CHANS_8_1 };

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static int Demux( demux_t * );
static int Control( demux_t *, int, va_list );

int dcpInit ( demux_t *p_demux );
int parseXML ( demux_t * p_demux );
static inline void fillVideoFmt(
        video_format_t * fmt, unsigned int width, unsigned int height,
        unsigned int frame_rate_num, unsigned int frame_rate_denom );
void CloseDcpAndMxf( demux_t *p_demux );



/*****************************************************************************
 * Open: module init function
 *****************************************************************************/
static int Open( vlc_object_t *obj )
{
    demux_t *p_demux = ( demux_t* ) obj;
    demux_sys_t *p_sys;
    es_format_t video_format, audio_format;
    int retval;

    if( !p_demux->psz_file )
        return VLC_EGENERIC;

    p_sys = new ( nothrow ) demux_sys_t();
    if( unlikely( p_sys == NULL ) ) {
        return VLC_ENOMEM;
    }
    p_demux->p_sys = p_sys;

    /* Allocate DCP object */
    dcp_t *p_dcp = new ( nothrow ) dcp_t;
    if( unlikely( p_dcp == NULL ) ) {
        delete  p_sys;
        return VLC_ENOMEM;
    }
    p_sys->p_dcp = p_dcp;


    /* handle the DCP directory, saving the paths for audio and video file, returning error if unsuccessful */
    if( ( retval = dcpInit( p_demux ) ) )
        goto error;

    /* Open video file */
    EssenceType_t essInter;
    for ( size_t i = 0; i < ( p_sys->p_dcp->video_reels.size() ); i++ )
    {
        EssenceType( p_sys->p_dcp->video_reels[i].filename.c_str(), essInter );
        if ( i == 0 )
        {
            p_sys->PictureEssType = essInter;
        }
        else
        {
            if ( essInter != p_sys->PictureEssType )
            {
                msg_Err( p_demux, "Integrity check failed : different essence containers" );
                retval = VLC_EGENERIC;
                goto error;
            }
        }

        switch( essInter )
        {
            case ESS_UNKNOWN:
                msg_Err( p_demux, "The file %s is not a supported AS_DCP essence container", p_sys->p_dcp->video_reels[i].filename.c_str() );
                retval = VLC_EGENERIC;
                goto error;

            case ESS_JPEG_2000:
            case ESS_JPEG_2000_S: {
                JP2K::PictureDescriptor PicDesc;
                if (p_sys->PictureEssType == ESS_JPEG_2000_S) {     /* 3D JPEG2000 */
                    JP2K::MXFSReader * p_PicMXFSReader = new ( nothrow ) JP2K::MXFSReader();

                    if( !p_PicMXFSReader) {
                        retval = VLC_ENOMEM;
                        goto error;
                    }
                    if( !ASDCP_SUCCESS( p_PicMXFSReader->OpenRead( p_sys->p_dcp->video_reels[i].filename.c_str() ) ) ) {
                        msg_Err( p_demux, "File %s could not be opened with ASDCP", p_sys->p_dcp->video_reels[i].filename.c_str() );
                        retval = VLC_EGENERIC;
                        delete p_PicMXFSReader;
                        goto error;
                    }

                    p_PicMXFSReader->FillPictureDescriptor( PicDesc );
                    videoReader_t videoReader;
                    videoReader.p_PicMXFSReader = p_PicMXFSReader;
                    p_sys->v_videoReader.push_back(videoReader);
                } else {                                            /* 2D JPEG2000 */
                    JP2K::MXFReader *p_PicMXFReader = new ( nothrow ) JP2K::MXFReader();
                    if( !p_PicMXFReader ) {
                        retval = VLC_ENOMEM;
                        goto error;
                    }
                    if( !ASDCP_SUCCESS( p_PicMXFReader->OpenRead( p_sys->p_dcp->video_reels[i].filename.c_str() ) ) ) {
                        msg_Err( p_demux, "File %s could not be opened with ASDCP",
                                        p_sys->p_dcp->video_reels[i].filename.c_str() );
                        retval = VLC_EGENERIC;
                        delete p_PicMXFReader;
                        goto error;
                    }

                    p_PicMXFReader->FillPictureDescriptor( PicDesc );
                    videoReader_t videoReader;
                    videoReader.p_PicMXFReader = p_PicMXFReader;
                    p_sys->v_videoReader.push_back(videoReader);
                }
                es_format_Init( &video_format, VIDEO_ES, VLC_CODEC_JPEG2000 );
                fillVideoFmt( &video_format.video, PicDesc.StoredWidth, PicDesc.StoredHeight,
                                PicDesc.EditRate.Numerator, PicDesc.EditRate.Denominator );


                if ( i > 0 ) {
                    if ( p_sys->frame_rate_num != PicDesc.EditRate.Numerator ||
                            p_sys->frame_rate_denom != PicDesc.EditRate.Denominator )
                    {
                        msg_Err( p_demux, "Integrity check failed : different frame rates" );
                        retval = VLC_EGENERIC;
                        goto error;
                    }
                }
                else
                {
                    p_sys->frame_rate_num   = PicDesc.EditRate.Numerator;
                    p_sys->frame_rate_denom = PicDesc.EditRate.Denominator;
                }

                p_sys->frames_total += p_sys->p_dcp->video_reels[i].i_duration;
                break;
            }
            case ESS_MPEG2_VES: {

                MPEG2::MXFReader *p_VideoMXFReader = new ( nothrow ) MPEG2::MXFReader();

                videoReader_t videoReader;
                videoReader.p_VideoMXFReader = p_VideoMXFReader;
                p_sys->v_videoReader.push_back(videoReader);

                MPEG2::VideoDescriptor  VideoDesc;

                if( !p_VideoMXFReader ) {
                    retval = VLC_ENOMEM;
                    goto error;
                }

                if( !ASDCP_SUCCESS( p_VideoMXFReader->OpenRead( p_sys->p_dcp->video_reels[i].filename.c_str() ) ) ) {
                    msg_Err( p_demux, "File %s could not be opened with ASDCP", p_sys->p_dcp->video_reels[i].filename.c_str() );
                    retval = VLC_EGENERIC;
                    goto error;
                }

                p_VideoMXFReader->FillVideoDescriptor( VideoDesc );

                es_format_Init( &video_format, VIDEO_ES, VLC_CODEC_MPGV );
                fillVideoFmt( &video_format.video, VideoDesc.StoredWidth, VideoDesc.StoredHeight,
                              VideoDesc.EditRate.Numerator, VideoDesc.EditRate.Denominator );


                if ( i > 0 ) {
                    if ( p_sys->frame_rate_num != VideoDesc.EditRate.Numerator ||
                            p_sys->frame_rate_denom != VideoDesc.EditRate.Denominator)
                    {
                        msg_Err( p_demux, "Integrity check failed : different frame rates" );
                        retval = VLC_EGENERIC;
                        goto error;
                    }
                }
                else
                {
                    p_sys->frame_rate_num   = VideoDesc.EditRate.Numerator;
                    p_sys->frame_rate_denom = VideoDesc.EditRate.Denominator;
                }

                p_sys->frames_total += p_sys->p_dcp->video_reels[i].i_duration;
                break;
            }
            default:
                msg_Err( p_demux, "Unrecognized video format" );
                retval = VLC_EGENERIC;
                goto error;
        }
    }

    if ( (p_sys->frame_rate_num == 0) || (p_sys->frame_rate_denom == 0) ) {
        msg_Err(p_demux, "Invalid frame rate (%i/%i)",
                p_sys->frame_rate_num, p_sys->frame_rate_denom);
        retval = VLC_EGENERIC;
        goto error;
    }

    if( ( p_sys->p_video_es = es_out_Add( p_demux->out, &video_format ) ) == NULL ) {
        msg_Err( p_demux, "Failed to add video es" );
        retval = VLC_EGENERIC;
        goto error;
    }

    /* Open audio file */
    EssenceType_t AudioEssType;
    EssenceType_t AudioEssTypeCompare;

    if( !p_sys->p_dcp->audio_reels.empty() )
    {
        EssenceType( p_sys->p_dcp->audio_reels[0].filename.c_str(), AudioEssType );

        if ( (AudioEssType == ESS_PCM_24b_48k) || (AudioEssType == ESS_PCM_24b_96k) ) {
            PCM::AudioDescriptor AudioDesc;

            for ( size_t i = 0; i < ( p_sys->p_dcp->audio_reels.size() ); i++)
            {
                if ( i != 0 )
                {
                    EssenceType( p_sys->p_dcp->audio_reels[i].filename.c_str(), AudioEssTypeCompare );
                    if ( AudioEssTypeCompare != AudioEssType )
                    {
                        msg_Err( p_demux, "Integrity check failed : different audio essence types in %s",
                        p_sys->p_dcp->audio_reels[i].filename.c_str() );
                        retval = VLC_EGENERIC;
                        goto error;
                    }
                }
                PCM::MXFReader *p_AudioMXFReader = new ( nothrow ) PCM::MXFReader();

                if( !p_AudioMXFReader ) {
                    retval = VLC_ENOMEM;
                    goto error;
                }

                if( !ASDCP_SUCCESS( p_AudioMXFReader->OpenRead( p_sys->p_dcp->audio_reels[i].filename.c_str() ) ) ) {
                    msg_Err( p_demux, "File %s could not be opened with ASDCP",
                                    p_sys->p_dcp->audio_reels[i].filename.c_str() );
                    retval = VLC_EGENERIC;
                    delete p_AudioMXFReader;
                    goto error;
                }

                p_AudioMXFReader->FillAudioDescriptor( AudioDesc );

                if (  (AudioDesc.ChannelCount >= sizeof(pi_channels_aout)/sizeof(uint32_t *))
                        || (pi_channels_aout[AudioDesc.ChannelCount] == NULL) )
                {
                    msg_Err(p_demux, " DCP module does not support %i channels", AudioDesc.ChannelCount);
                    retval = VLC_EGENERIC;
                    delete p_AudioMXFReader;
                    goto error;
                }
                audioReader_t audioReader;
                audioReader.p_AudioMXFReader = p_AudioMXFReader;
                p_sys->v_audioReader.push_back( audioReader );

            }
            es_format_Init( &audio_format, AUDIO_ES, VLC_CODEC_S24L );
            if( AudioDesc.AudioSamplingRate.Denominator != 0 )
                audio_format.audio.i_rate =
                    AudioDesc.AudioSamplingRate.Numerator
                    / AudioDesc.AudioSamplingRate.Denominator;
            else if ( AudioEssType == ESS_PCM_24b_96k )
                audio_format.audio.i_rate = 96000;
            else
                audio_format.audio.i_rate = 48000;

            p_sys->i_audio_buffer = PCM::CalcFrameBufferSize( AudioDesc );
            if (p_sys->i_audio_buffer == 0) {
                msg_Err( p_demux, "Failed to get audio buffer size" );
                retval = VLC_EGENERIC;
                goto error;
            }

            audio_format.audio.i_bitspersample = AudioDesc.QuantizationBits;
            audio_format.audio.i_blockalign    = AudioDesc.BlockAlign;
            audio_format.audio.i_channels      =
            p_sys->i_channels                  = AudioDesc.ChannelCount;

            /* Manage channel orders */
            p_sys->i_chans_to_reorder =  aout_CheckChannelReorder(
                    pi_channels_aout[AudioDesc.ChannelCount], NULL,
                    i_channel_mask[AudioDesc.ChannelCount],   p_sys->pi_chan_table );

            if( ( p_sys->p_audio_es = es_out_Add( p_demux->out, &audio_format ) ) == NULL ) {
                msg_Err( p_demux, "Failed to add audio es" );
                retval = VLC_EGENERIC;
                goto error;
            }
        } else {
            msg_Err( p_demux, "The file %s is not a supported AS_DCP essence container",
                    p_sys->p_dcp->audio_reels[0].filename.c_str() );
            retval = VLC_EGENERIC;
            goto error;
        }
    }
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_sys->frame_no = p_sys->p_dcp->video_reels[0].i_entrypoint;

    return VLC_SUCCESS;
error:
    CloseDcpAndMxf( p_demux );
    return retval;
}


/*****************************************************************************
 * Close: module destroy function
 *****************************************************************************/
static inline void Close( vlc_object_t *obj )
{
    demux_t *p_demux = ( demux_t* ) obj;
    CloseDcpAndMxf( p_demux );
}



/*****************************************************************************
 * Demux: DCP Demuxing function
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t *p_video_frame = NULL, *p_audio_frame = NULL;

    PCM::FrameBuffer   AudioFrameBuff( p_sys->i_audio_buffer);
    AESDecContext video_aes_ctx, audio_aes_ctx;

    /* swaping video reels */
    if  ( p_sys->frame_no == p_sys->p_dcp->video_reels[p_sys->i_video_reel].i_absolute_end )
    {
        if ( p_sys->i_video_reel + 1 == p_sys->v_videoReader.size() )
        {
            return 0;
        }
        else
        {
            p_sys->i_video_reel++;
        }
    }

    /* swaping audio reels */
    if  ( !p_sys->p_dcp->audio_reels.empty() && p_sys->frame_no == p_sys->p_dcp->audio_reels[p_sys->i_audio_reel].i_absolute_end )
     {
         if ( p_sys->i_audio_reel + 1 == p_sys->v_audioReader.size() )
         {
             return 0;//should never go there
         }
         else
         {
             p_sys->i_audio_reel++;
         }
     }

    /* video frame */

    /* initialize AES context, if reel is encrypted */
    if( p_sys->p_dcp->video_reels.size() > p_sys->i_video_reel &&
        p_sys->p_dcp->video_reels[p_sys->i_video_reel].p_key )
    {
        if( ! ASDCP_SUCCESS( video_aes_ctx.InitKey( p_sys->p_dcp->video_reels[p_sys->i_video_reel].p_key->getKey() ) ) )
        {
            msg_Err( p_demux, "ASDCP failed to initialize AES key" );
            goto error;
        }
    }

    switch( p_sys->PictureEssType )
    {
        case ESS_JPEG_2000:
        case ESS_JPEG_2000_S:{
            JP2K::FrameBuffer  PicFrameBuff(FRAME_BUFFER_SIZE);
            int nextFrame = p_sys->frame_no + p_sys->p_dcp->video_reels[p_sys->i_video_reel].i_correction;
            if ( ( p_video_frame = block_Alloc( FRAME_BUFFER_SIZE )) == NULL )
                goto error;

            if ( ! ASDCP_SUCCESS(
                    PicFrameBuff.SetData(p_video_frame->p_buffer, FRAME_BUFFER_SIZE)) )
                goto error_asdcp;
            if ( p_sys->PictureEssType == ESS_JPEG_2000_S ) {
                if ( ! ASDCP_SUCCESS(
                        p_sys->v_videoReader[p_sys->i_video_reel].p_PicMXFSReader->ReadFrame(nextFrame, JP2K::SP_LEFT, PicFrameBuff, &video_aes_ctx, 0)) ) {
                    PicFrameBuff.SetData(0,0);
                    goto error_asdcp;
                }
             } else {
                if ( ! ASDCP_SUCCESS(
                        p_sys->v_videoReader[p_sys->i_video_reel].p_PicMXFReader->ReadFrame(nextFrame, PicFrameBuff, &video_aes_ctx, 0)) ) {
                    PicFrameBuff.SetData(0,0);
                    goto error_asdcp;
                }
            }
            p_video_frame->i_buffer = PicFrameBuff.Size();
            break;
        }
        case ESS_MPEG2_VES: {
            MPEG2::FrameBuffer VideoFrameBuff(FRAME_BUFFER_SIZE);
            if ( ( p_video_frame = block_Alloc( FRAME_BUFFER_SIZE )) == NULL )
                goto error;

            if ( ! ASDCP_SUCCESS(
                    VideoFrameBuff.SetData(p_video_frame->p_buffer, FRAME_BUFFER_SIZE)) )
                goto error_asdcp;

            if ( ! ASDCP_SUCCESS(
                    p_sys->v_videoReader[p_sys->i_video_reel].p_VideoMXFReader->ReadFrame(p_sys->frame_no + p_sys->p_dcp->video_reels[p_sys->i_video_reel].i_correction, VideoFrameBuff, &video_aes_ctx, 0)) ) {
                VideoFrameBuff.SetData(0,0);
                goto error_asdcp;
            }

            p_video_frame->i_buffer = VideoFrameBuff.Size();
            break;
        }
        default:
            msg_Err( p_demux, "Unrecognized video format" );
            goto error;
    }

    p_video_frame->i_length = CLOCK_FREQ * p_sys->frame_rate_denom / p_sys->frame_rate_num;
    p_video_frame->i_pts = CLOCK_FREQ * p_sys->frame_no * p_sys->frame_rate_denom / p_sys->frame_rate_num;

    if( !p_sys->p_dcp->audio_reels.empty() )
    {
        /* audio frame */
        if ( ( p_audio_frame = block_Alloc( p_sys->i_audio_buffer )) == NULL ) {
            goto error;
        }

        /* initialize AES context, if reel is encrypted */
        if( p_sys->p_dcp->audio_reels.size() > p_sys->i_audio_reel &&
            p_sys->p_dcp->audio_reels[p_sys->i_audio_reel].p_key )
        {
            if( ! ASDCP_SUCCESS( audio_aes_ctx.InitKey( p_sys->p_dcp->audio_reels[p_sys->i_audio_reel].p_key->getKey() ) ) )
            {
                msg_Err( p_demux, "ASDCP failed to initialize AES key" );
                goto error;
            }
        }

        if ( ! ASDCP_SUCCESS(
                AudioFrameBuff.SetData(p_audio_frame->p_buffer, p_sys->i_audio_buffer)) ) {
            goto error_asdcp;
        }

        if ( ! ASDCP_SUCCESS(
                p_sys->v_audioReader[p_sys->i_audio_reel].p_AudioMXFReader->ReadFrame(p_sys->frame_no + p_sys->p_dcp->audio_reels[p_sys->i_audio_reel].i_correction, AudioFrameBuff, &audio_aes_ctx, 0)) ) {
            AudioFrameBuff.SetData(0,0);
            goto error_asdcp;
        }

        if( p_sys->i_chans_to_reorder )
            aout_ChannelReorder( p_audio_frame->p_buffer, p_audio_frame->i_buffer,
                    p_sys->i_channels,
                    p_sys->pi_chan_table, VLC_CODEC_S24L );

        p_audio_frame->i_buffer = AudioFrameBuff.Size();
        p_audio_frame->i_length = CLOCK_FREQ * p_sys->frame_rate_denom / p_sys->frame_rate_num;
        p_audio_frame->i_pts = CLOCK_FREQ * p_sys->frame_no * p_sys->frame_rate_denom / p_sys->frame_rate_num;
        /* Video is the main pts */
        if ( p_audio_frame->i_pts != p_video_frame->i_pts ) {
            msg_Err( p_demux, "Audio and video frame pts are not in sync" );
        }
    }

    p_sys->i_pts = p_video_frame->i_pts;
    es_out_SetPCR( p_demux->out, p_sys->i_pts );
    if(p_video_frame)
        es_out_Send( p_demux->out, p_sys->p_video_es, p_video_frame );
    if(p_audio_frame)
        es_out_Send( p_demux->out, p_sys->p_audio_es, p_audio_frame );

    p_sys->frame_no++;

    return 1;

error_asdcp:
    msg_Err( p_demux, "Couldn't read frame with ASDCP");
error:
    if (p_video_frame)
        block_Release(p_video_frame);
    if (p_audio_frame)
        block_Release(p_audio_frame);
    return -1;
}

/*****************************************************************************
 * Control: handle the controls
 *****************************************************************************/
static int Control( demux_t *p_demux, int query, va_list args )
{
    double f,*pf;
    bool *pb;
    int64_t *pi64, i64;
    demux_sys_t *p_sys = p_demux->p_sys;

    switch ( query )
    {
        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_CONTROL_PACE:
            pb = va_arg ( args, bool* );
            *pb = true;
            break;

        case DEMUX_CAN_SEEK:
            pb = va_arg( args, bool * );
            if( p_sys->PictureEssType != ESS_MPEG2_VES )
                *pb = true;
            else
                *pb = false;
            break;

        case DEMUX_SET_PAUSE_STATE:
            return VLC_SUCCESS;

        case DEMUX_GET_POSITION:
            pf = va_arg( args, double * );
            if( p_sys->frames_total != 0 )
                *pf = (double) p_sys->frame_no / (double) p_sys->frames_total;
            else {
                msg_Warn( p_demux, "Total number of frames is 0" );
                *pf = 0.0;
            }
            break;

        case DEMUX_SET_POSITION:
            f = va_arg( args, double );
            p_sys->frame_no = (int) ( f * p_sys->frames_total );
            break;

        case DEMUX_GET_LENGTH:
            pi64 = va_arg ( args, int64_t * );
            *pi64 =  ( p_sys->frames_total * p_sys->frame_rate_denom / p_sys->frame_rate_num ) * CLOCK_FREQ;
            break;

        case DEMUX_GET_TIME:
            pi64 = va_arg( args, int64_t * );
            *pi64 = p_sys->i_pts >= 0 ? p_sys->i_pts : 0;
            break;

        case DEMUX_SET_TIME:
            i64 = va_arg( args, int64_t );
            msg_Warn( p_demux, "DEMUX_SET_TIME"  );
            p_sys->frame_no = i64 * p_sys->frame_rate_num / ( CLOCK_FREQ * p_sys->frame_rate_denom );
            p_sys->i_pts= i64;
            es_out_SetPCR(p_demux->out, p_sys->i_pts);
            es_out_Control( p_demux->out, ES_OUT_SET_NEXT_DISPLAY_TIME, ( mtime_t ) i64 );
            break;
        case DEMUX_GET_PTS_DELAY:
            pi64 = va_arg( args, int64_t * );
            *pi64 =
                INT64_C(1000) * var_InheritInteger( p_demux, "file-caching" );
            return VLC_SUCCESS;


        default:
            msg_Warn( p_demux, "Unknown query %d in DCP Control", query );
            return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}


/*****************************************************************************
 * Low-level functions : string manipulation, free function, etc
 *****************************************************************************/
/**
 * Function to fill video_format_t fields for an elementary stream
 * @param fmt video format structure
 * @param width picture width
 * @param height picture height
 * @param frame_rate_num video frame rate numerator
 * @param frame_rate_denom video frame rate denominator
 */
static inline void fillVideoFmt( video_format_t * fmt, unsigned int width, unsigned int height, unsigned int frame_rate_num, unsigned int frame_rate_denom )
{
    fmt->i_width = width;
    fmt->i_height = height;
    /* As input are square pixels let VLC  or decoder fix SAR, origin,
     * and visible area */
    fmt->i_frame_rate = frame_rate_num;
    fmt->i_frame_rate_base = frame_rate_denom;
}

/**
 * Function to free memory in case of error or when closing the module
 * @param p_demux DCP access-demux
 */


void CloseDcpAndMxf( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    /* close the files */
    switch( p_sys->PictureEssType )
    {
        case ESS_UNKNOWN:
            break;
        case ESS_JPEG_2000:
            for ( size_t i = 0; i < p_sys->v_videoReader.size(); i++ )
            {
                if( p_sys->v_videoReader[i].p_PicMXFReader )
                    p_sys->v_videoReader[i].p_PicMXFReader->Close();
            }
            break;
        case ESS_JPEG_2000_S:
            for ( size_t i = 0; i < p_sys->v_videoReader.size(); i++ )
            {
                if( p_sys->v_videoReader[i].p_PicMXFSReader )
                    p_sys->v_videoReader[i].p_PicMXFSReader->Close();
            }
            break;
        case ESS_MPEG2_VES:
            for ( size_t i = 0; i < p_sys->v_videoReader.size(); i++ )
            {
                if( p_sys->v_videoReader[i].p_VideoMXFReader )
                    p_sys->v_videoReader[i].p_VideoMXFReader->Close();
            }
            break;
        default:
            break;
    }

    for ( size_t i = 0; i < p_sys->v_audioReader.size(); i++ )
    {
        if( p_sys->v_audioReader[i].p_AudioMXFReader )
            p_sys->v_audioReader[i].p_AudioMXFReader->Close();
    }

    delete( p_sys );
}


/*****************************************************************************
 * DCP init
 *****************************************************************************/

/**
 * Function to handle the operations with the DCP directory.
 * @param p_demux Demux pointer.
 * @return Integer according to the success or not of the process.
 */
int dcpInit ( demux_t *p_demux )
{
    int retval;

    demux_sys_t *p_sys = p_demux->p_sys;
    dcp_t *p_dcp = p_sys->p_dcp;

    p_dcp->path = p_demux->psz_file;
    /* Add a '/' in end of path if needed */
    if ( *(p_dcp->path).rbegin() != '/')
        p_dcp->path.append( "/" );

    /* Parsing XML files to get audio and video files */
    msg_Dbg( p_demux, "parsing XML files..." );
    if( ( retval = parseXML( p_demux ) ) )
        return retval;

    msg_Dbg(p_demux, "parsing XML files done");

    return VLC_SUCCESS;
}


/*****************************************************************************
 * functions for XML parsing
 *****************************************************************************/

/**
 * Function to retrieve the path to the ASSETMAP file.
 * @param p_demux DCP access_demux.
 */
static std::string assetmapPath( demux_t * p_demux )
{
    DIR *dir = NULL;
    struct dirent *ent = NULL;
    dcp_t *p_dcp = p_demux->p_sys->p_dcp;
    std::string result;

    if( ( dir = opendir (p_dcp->path.c_str() ) ) != NULL )
    {
        /* print all the files and directories within directory */
        while( ( ent = readdir ( dir ) ) != NULL )
        {
            if( strcasecmp( "assetmap", ent->d_name ) == 0 || strcasecmp( "assetmap.xml", ent->d_name ) == 0 )
            {
                /* copy of "path" in "res" */
                result = p_dcp->path;
                result.append( ent->d_name );
                break;
            }
        }
        closedir( dir );
    }
    else
        msg_Err( p_demux, "Could not open the directory: %s", p_dcp->path.c_str() );

    /* if no assetmap file */
    if( result.empty() )
        msg_Err( p_demux, "No ASSETMAP found in the directory: %s", p_dcp->path.c_str() );

    return result;
}


/**
 * Function which parses XML files in DCP directory in order to get video and audio files
 * @param p_demux Demux pointer.
 * @return Integer according to the success or not of the operation
 */
int parseXML ( demux_t * p_demux )
{
    int retval;

    std::string assetmap_path = assetmapPath( p_demux );
    /* We get the ASSETMAP file path */
    if( assetmap_path.empty() )
        return VLC_EGENERIC;

    /* We parse the ASSETMAP File in order to get CPL File path, PKL File path
     and to store UID/Path of all files in DCP directory (except ASSETMAP file) */
    AssetMap *assetmap = new (nothrow) AssetMap( p_demux, assetmap_path, p_demux->p_sys->p_dcp );
    if( ( retval = assetmap->Parse() ) )
        return retval;

    delete assetmap;
    return VLC_SUCCESS; /* TODO : perform checking on XML parsing */
}
