/*****************************************************************************
 * mpeg_audio.c : mpeg_audio Stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: mpeg_audio.c,v 1.5 2002/05/14 14:10:17 fenrir Exp $
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */

#include <videolan/vlc.h>

#include <sys/types.h>

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"
#include "input_ext-plugins.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void input_getfunctions( function_list_t * p_function_list );
static int  MPEGAudioDemux         ( struct input_thread_s * );
static int  MPEGAudioInit          ( struct input_thread_s * );
static void MPEGAudioEnd           ( struct input_thread_s * );

/* TODO: support MPEG-2.5, not difficult */

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( "MPEG I/II Audio stream demux" )
    ADD_CAPABILITY( DEMUX, 110 )
    ADD_SHORTCUT( "mpeg_audio" )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    input_getfunctions( &p_module->p_functions->demux );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
static void input_getfunctions( function_list_t * p_function_list )
{
#define input p_function_list->functions.demux
    input.pf_init             = MPEGAudioInit;
    input.pf_end              = MPEGAudioEnd;
    input.pf_demux            = MPEGAudioDemux;
    input.pf_rewind           = NULL;
#undef input
}

/*****************************************************************************
 * Definitions of structures  and functions used by this plugins 
 *****************************************************************************/

/* XXX set this to 0 to avoid problem with PS XXX */
/* but with some file or web radio will failed to detect */
/* it's you to choose */
#define MPEGAUDIO_MAXTESTPOS    0

#define MPEGAUDIO_MAXFRAMESIZE  1500 /* no exactly */

typedef struct mpegaudio_format_s
{
    u32 i_header;
    int i_version;
    int i_layer;
    int i_crc;
    int i_bitrate;
    int i_samplingfreq;
    int i_padding;
    int i_extension;
    int i_mode;
    int i_modeext;
    int i_copyright;
    int i_original;
    int i_emphasis;

} mpegaudio_format_t;

/* Xing Header if present */
#define FRAMES_FLAG     0x0001  /* these flags is for i_flags */
#define BYTES_FLAG      0x0002  /* because all is optionnal */
#define TOC_FLAG        0x0004
#define VBR_SCALE_FLAG  0x0008
typedef struct mpegaudio_xing_header_s
{
    int i_flags;      /* from Xing header data */
    int i_frames;     /* total bit stream frames from Xing header data */
    int i_bytes;      /* total bit stream bytes from Xing header data */
    int i_vbr_scale;  /* encoded vbr scale from Xing header data */
    u8  i_toc[100];   /* for seek */
    int i_avgbitrate; /* calculated, XXX: bits/sec not Kb */
} mpegaudio_xing_header_t;

typedef struct demux_data_mpegaudio_s
{
    mtime_t i_pts;

    int     i_framecount;
   
    es_descriptor_t         *p_es;
    mpegaudio_format_t      mpeg;
    mpegaudio_xing_header_t xingheader;

} demux_data_mpegaudio_t;


static int mpegaudio_bitrate[2][3][16] =
{
    {
        { 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0 }, /* v1 l1 */
        { 0, 32, 48, 56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 384, 0 }, /* v1 l2 */
        { 0, 32, 40, 48,  56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 0 }  /* v1 l3 */
    },
    
    {
        { 0, 32, 48, 56,  64,  80,  96, 112, 128, 144, 160, 176, 192, 224, 256, 0 }, /* v2 l1 */
        { 0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160, 0 }, /* v2 l2 */
        { 0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160, 0 }  /* v2 l3 */
    }

};

static int mpegaudio_samplingfreq[2][4] = /* version 1 then 2 */
{
    { 44100, 48000, 32000, 0 },
    { 22050, 24000, 16000, 0 }
};

static char* mpegaudio_mode[4] =
{
    "stereo", "joint stereo", "dual channel", "mono"
};

static __inline__ u32 __GetDWBE( byte_t *p_buff )
{
    return( ( (*(p_buff)) << 24 ) + ( (*(p_buff+1)) << 16 ) +
                    ( (*(p_buff+2)) << 8 ) +  ( (*(p_buff+3)) ) );
}

static int __CheckPS( input_thread_t *p_input )
{
    byte_t *p_buff;
    int i_size = input_Peek( p_input, &p_buff, 8196 );

    while( i_size > 0 )
    {
        if( !(*p_buff) && !(*(p_buff + 1)) 
                && (*(p_buff + 2) == 1 ) && (*(p_buff + 3) >= 0xB9 ) )
        {
            return( 1 );  /* it could be ps so ...*/
        }
        p_buff++;
        i_size--;
    }
    return( 0 );
}

/*
#define __GetDWBE( p_buff ) \
    ( ( (*(p_buff)) << 24 ) + ( (*(p_buff+1)) << 16 ) + \
      ( (*(p_buff+2)) << 8 ) +  ( (*(p_buff+3)) ) )
*/
/*****************************************************************************
 * MPEGAudio_CheckHeader : Test the validity of the header 
 *****************************************************************************/
static int MPEGAudio_CheckHeader( u32 i_header )
{
    if( ((( i_header >> 20 )&0x0FFF) != 0x0FFF )  /* header sync */
        || (((i_header >> 17)&0x03) == 0 )  /* valid layer ?*/
        || (((i_header >> 12)&0x0F) == 0x0F )
        || (((i_header >> 12)&0x0F) == 0x00 ) /* valid bitrate ? */
        || (((i_header >> 10) & 0x03) == 0x03 ) /* valide sampling freq ? */
        || ((i_header & 0x03) == 0x02 )) /* valid emphasis ? */
    {
        return( 0 ); /*invalid */
    }
    return( 1 ); /* valid */
}

/*****************************************************************************
 * MPEGAudio_ParseHeader : Parse a header ;)
 *****************************************************************************/
static void MPEGAudio_ParseHeader( u32 i_header, mpegaudio_format_t *p_mpeg )
{
    p_mpeg->i_header = i_header;
    p_mpeg->i_version =  1 - ( ( i_header >> 19 ) & 0x01 );
    p_mpeg->i_layer =  3 - ( ( i_header >> 17 ) & 0x03 );
    p_mpeg->i_crc = 1 - (( i_header >> 16 ) & 0x01);
    p_mpeg->i_bitrate = mpegaudio_bitrate[p_mpeg->i_version][p_mpeg->i_layer][(i_header>>12)&0x0F];
    p_mpeg->i_samplingfreq = mpegaudio_samplingfreq[p_mpeg->i_version][(i_header>>10)&0x03];
    p_mpeg->i_padding = (( i_header >> 9 ) & 0x01);
    p_mpeg->i_extension = ( i_header >> 7 ) & 0x01;
    p_mpeg->i_mode = ( i_header >> 6 ) & 0x03;
    p_mpeg->i_modeext = ( i_header >> 4 ) & 0x03;
    p_mpeg->i_copyright = ( i_header >> 3 ) & 0x01;
    p_mpeg->i_original = ( i_header >> 2 ) & 0x01;
    p_mpeg->i_emphasis = ( i_header ) & 0x03;
}

/*****************************************************************************
 * MPEGAudio_FrameSize : give the size of a frame in the mpeg stream
 *****************************************************************************/
static int MPEGAudio_FrameSize( mpegaudio_format_t *p_mpeg )
{
    /* XXX if crc do i need to add 2 bytes or not? */
    switch( p_mpeg->i_layer )
    {
        case( 0 ):
            return( ( ( ( !p_mpeg->i_version ? 12000 : 6000 ) * 
                         p_mpeg->i_bitrate ) / 
                         p_mpeg->i_samplingfreq + p_mpeg->i_padding ) * 4);
        case( 1 ):
        case( 2 ):
            return( ( ( !p_mpeg->i_version ? 144000 : 72000 ) * 
                         p_mpeg->i_bitrate ) /  
                         p_mpeg->i_samplingfreq + p_mpeg->i_padding );
    }
    return( 1024 ); /* must never happen, 1k to advance in stream*/
}

/*****************************************************************************
 * MPEGAudio_DecodedFrameSize : give the length of the decoded pcm data
 *****************************************************************************/
static int MPEGAudio_DecodedFrameSize( mpegaudio_format_t *p_mpeg )
{
    switch( p_mpeg->i_layer )
    {
        case( 0 ): /* layer 1 */
            return( 384);
        case( 1 ): /* layer 2 */
            return( 1152 );
        case( 2 ): /* layer 3 */
            return( !p_mpeg->i_version ? 1152 : 576 ); 
            /* XXX: perhaps we have to /2 for all layer but i'm not sure */
    }
    return( 0 );
}

/*****************************************************************************
 * MPEGAudio_FindFrame : Find a header that could be valid. 
 *****************************************************************************
 * The idea is to search for 2 consecutive headers that seem valid 
 * Perhaps we can search 2 header with same version or samplefreq(...) to be
 * more secure but this seems to be enougth
 *****************************************************************************/
static int MPEGAudio_FindFrame( input_thread_t *p_input, 
                                 int *pi_pos, 
                                 mpegaudio_format_t *p_mpeg,
                                 int i_posmax )
{
    byte_t *p_buff;
    u32 i_header;
    int i_framesize;

    int i_pos = 0;
    int i_size = input_Peek( p_input, &p_buff, i_posmax+MPEGAUDIO_MAXFRAMESIZE);

    while( i_pos + 4 <= __MIN( i_posmax, i_size ) )
    {
        i_header = __GetDWBE( p_buff );
        if( MPEGAudio_CheckHeader( i_header ) )
        {
            MPEGAudio_ParseHeader( i_header, p_mpeg );
            i_framesize = MPEGAudio_FrameSize( p_mpeg );
            if(  i_pos + i_framesize + 4 > i_size )
            {
                *pi_pos = i_pos;
                return( 1 );
            }
            else
            {
                if( MPEGAudio_CheckHeader( __GetDWBE( p_buff + i_framesize ) ) )
                {
                    *pi_pos = i_pos;
                    return( 2 );
                }
            }
        }
        p_buff++;
        i_pos++;
    }

    *pi_pos = 0;
    return( 0 );
}

/*****************************************************************************
 * MPEGAudio_ExtractXingHeader : extract a Xing header if exist
 *****************************************************************************
 * It also calcul avgbitrate, using Xing header if present or assume that
 * the bitrate of the first frame is the same for the all file
 *****************************************************************************/
static void MPEGAudio_ExtractXingHeader( input_thread_t *p_input,
                                    mpegaudio_xing_header_t *p_xh )
{
    int i_pos;
    int i_size;
    mpegaudio_format_t mpeg;
    byte_t  *p_buff;
    
    p_xh->i_flags = 0;  /* nothing present */
    if( !(MPEGAudio_FindFrame( p_input, &i_pos, &mpeg, 2024 )) )
    {
        return; /* failed , can't */
    }
    p_xh->i_avgbitrate = mpeg.i_bitrate * 1000; /* default */

    /* 1024 is enougth */
    if( ( i_size = input_Peek( p_input, &p_buff, 1024 + i_pos ) ) < 8 )
    {
        return;
    }
    p_buff += i_pos;

    /* calculate pos of xing header */
    if( !mpeg.i_version )
    {
        p_buff += mpeg.i_mode != 3 ? 36 : 21;
    }
    else
    {
        p_buff += mpeg.i_mode != 3 ? 21 : 13;
    }
    
    if( (*p_buff != 'X' )||(*(p_buff+1) != 'i' )
        ||(*(p_buff+2) != 'n' )||(*(p_buff+3) != 'g' ) )
    {
        return;
    }
    p_buff += 4;

    p_xh->i_flags = __GetDWBE( p_buff ); 
    p_buff += 4;

    if( p_xh->i_flags&FRAMES_FLAG ) 
    {
        p_xh->i_frames = __GetDWBE( p_buff );
        p_buff += 4;
    }
    if( p_xh->i_flags&BYTES_FLAG ) 
    {
        p_xh->i_bytes = __GetDWBE( p_buff );
        p_buff += 4;
    }
    if( p_xh->i_flags&TOC_FLAG ) 
    {
        FAST_MEMCPY( p_xh->i_toc, p_buff, 100 );
        p_buff += 100;
    }
    if( p_xh->i_flags&VBR_SCALE_FLAG ) 
    {
        p_xh->i_vbr_scale = __GetDWBE( p_buff );
        p_buff += 4;
    }
    if( ( p_xh->i_flags&FRAMES_FLAG )&&( p_xh->i_flags&BYTES_FLAG ) )
    {
        p_xh->i_avgbitrate = 
              ((u64)p_xh->i_bytes * (u64)8 * (u64)mpeg.i_samplingfreq) / 
               ((u64)p_xh->i_frames * (u64)MPEGAudio_DecodedFrameSize( &mpeg));
    }
}
                                    

/*****************************************************************************
 * MPEGaudioInit : initializes MPEGaudio structures
 *****************************************************************************/
static int MPEGAudioInit( input_thread_t * p_input )
{
    demux_data_mpegaudio_t *p_mpegaudio;
    mpegaudio_format_t mpeg;
    es_descriptor_t * p_es;
    int i_pos;

    /* XXX: i don't know what it's supposed to do, copied from ESInit */
    /* Initialize access plug-in structures. */
    if( p_input->i_mtu == 0 )
    {
    /* Improve speed. */
        p_input->i_bufsize = INPUT_DEFAULT_BUFSIZE;
    }
    /* check if it can be a ps stream */
    if( __CheckPS(  p_input ) )
    {
        return( -1 );
    }
    /* must be sure that is mpeg audio stream */
    if( MPEGAudio_FindFrame( p_input, &i_pos, &mpeg, MPEGAUDIO_MAXTESTPOS)!= 2 )
    {
        intf_WarnMsg( 2,"input: MPEGAudio plug-in discarded" );
        return( -1 );
    }
    
    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( input_InitStream( p_input, 0 ) == -1)
    {
        intf_ErrMsg( "input error: cannot init stream" );
        return( -1 );
    }    
    if( input_AddProgram( p_input, 0, 0) == NULL )
    {
        intf_ErrMsg( "input error: cannot add program" );
        return( -1 );
    }
    p_input->stream.pp_programs[0]->b_is_ok = 0;
    p_input->stream.p_selected_program = 
            p_input->stream.p_new_program = p_input->stream.pp_programs[0];
    
    /* create our ES */ 
    p_es = input_AddES( p_input, 
                        p_input->stream.p_selected_program, 
                        1, /* id */
                        0 );
    if( !p_es )
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        intf_ErrMsg( "input error: not enough memory." );
        return( -1 );
    }
    p_es->i_stream_id = 1;
    p_es->i_type = !mpeg.i_layer ? MPEG1_AUDIO_ES : MPEG2_AUDIO_ES;
    p_es->i_cat = AUDIO_ES;
    p_es->b_audio = 1;
    input_SelectES( p_input, p_es );

    p_input->stream.p_selected_program->b_is_ok = 1;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    /* create p_mpegaudio and init it */
    p_input->p_demux_data =
           p_mpegaudio = malloc( sizeof( demux_data_mpegaudio_t ));

    if( !p_mpegaudio )
    {
        intf_ErrMsg( "input error: not enough memory." );
        return( -1 );
    }

    /*input_ClockInit(  p_input->stream.p_selected_program ); 
      done by AddProgram */
    p_mpegaudio->p_es = p_es;
    p_mpegaudio->mpeg = mpeg;
    p_mpegaudio->i_framecount = 0;
    p_mpegaudio->i_pts = 0;  

    /* parse Xing Header if present */
    MPEGAudio_ExtractXingHeader( p_input, &p_mpegaudio->xingheader );
    
    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.i_mux_rate = p_mpegaudio->xingheader.i_avgbitrate / 50 / 8;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    /* FIXME FIXME FIXME FIXME FIXME FIXME FIXME */
    /* if i don't do that, it don't work correctly but why ??? */
    if( p_input->stream.b_seekable )
    {
        p_input->pf_seek( p_input, 0 );
        input_AccessReinit( p_input );
    }
    /* FIXME FIXME FIXME FIXME FIXME FIXME FIXME */

    /* all is ok :)) */
    intf_Msg( "input init: Audio MPEG-%d layer %d %s %dHz %dKb/s %s",
                mpeg.i_version + 1,
                mpeg.i_layer +1 ,
                mpegaudio_mode[mpeg.i_mode],
                mpeg.i_samplingfreq,
                p_mpegaudio->xingheader.i_avgbitrate / 1000,
                p_mpegaudio->xingheader.i_flags ?
                        "VBR (Xing)" : "" 
                    );
    return( 0 );
}


/*****************************************************************************
 * MPEGAudioEnd: frees unused data
 *****************************************************************************/
static void MPEGAudioEnd( input_thread_t * p_input )
{
    
}


/*****************************************************************************
 * MPEGAudioDemux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int MPEGAudioDemux( input_thread_t * p_input )
{
    int i_pos;
    int i_toread;
    pes_packet_t    *p_pes;
    mpegaudio_format_t mpeg;
    demux_data_mpegaudio_t *p_mpegaudio = 
                        (demux_data_mpegaudio_t*) p_input->p_demux_data;

    /*  look for a frame */
    if( !MPEGAudio_FindFrame( p_input, &i_pos, &mpeg, 4096 ) )
    {
        intf_WarnMsg( 1, "input error: cannot find next frame");
        return( 0 );
    }
    
    /* if stream has changed */
    if( ( mpeg.i_version != p_mpegaudio->mpeg.i_version )
        ||( mpeg.i_layer != p_mpegaudio->mpeg.i_layer )
        ||( mpeg.i_samplingfreq != p_mpegaudio->mpeg.i_samplingfreq ) )
    {
        intf_WarnMsg( 1, "input demux: stream has changed" );
        p_mpegaudio->i_framecount = 0;
        p_mpegaudio->i_pts = 0;
    }

    input_ClockManageRef( p_input,
                          p_input->stream.p_selected_program,
                          p_mpegaudio->i_pts );

    /* in fact i_pos may be garbage but ... i don't want to skip it 
        it's borring ;) */

    i_toread = MPEGAudio_FrameSize( &mpeg ) + i_pos;
    /* create one pes */
    if( !(p_pes = input_NewPES( p_input->p_method_data )) )
    {
        intf_ErrMsg( "input demux: out of memory" );
        return( -1 );
    }

    while( i_toread > 0 )
    {
        data_packet_t   *p_data;
        int i_read;

        if( (i_read = input_SplitBuffer( p_input, &p_data, i_toread ) ) <= 0 )
        {
            break;
        }
        if( !p_pes->p_first )
        {
            p_pes->p_first = p_data;
            p_pes->i_nb_data = 1;
            p_pes->i_pes_size = i_read;
        }
        else
        {
            p_pes->p_last->p_next  = p_data;
            p_pes->i_nb_data++;
            p_pes->i_pes_size += i_read;
        }
        p_pes->p_last  = p_data;
        i_toread -= i_read;
    }
    /* XXX VERY STRANGE need to *90000 and not *1000000 WHYYYY ??? 
        I know that for mpeg system clock unit is based on 90000 but 
        pts is not supposed to be in microsec for vlc ? 
        At least it's welcome to write it somewhere in input_clock.c 
        i've wasted time because of it :( */
    p_mpegaudio->i_pts = (mtime_t)90000 * 
                               (mtime_t)p_mpegaudio->i_framecount * 
                               (mtime_t)MPEGAudio_DecodedFrameSize( &mpeg ) /
                               (mtime_t)mpeg.i_samplingfreq;
    p_pes->i_dts = 0;
    p_pes->i_pts = input_ClockGetTS( p_input,
                                     p_input->stream.p_selected_program,
                                     p_mpegaudio->i_pts );

    if( !p_mpegaudio->p_es->p_decoder_fifo )
    {
        intf_ErrMsg( "input demux: no audio decoder" );
        input_DeletePES( p_input->p_method_data, p_pes );
        return( -1 ); /* perhaps not, it's my choice */
    }
    else
    {
        input_DecodePES( p_mpegaudio->p_es->p_decoder_fifo, p_pes );
    }

    p_mpegaudio->i_framecount++;
    p_mpegaudio->mpeg = mpeg; 

    return( 1 );
}


