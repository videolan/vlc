/*****************************************************************************
 * audio.c : mpeg audio Stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: audio.c,v 1.16 2003/03/30 18:14:37 gbazin Exp $
 *
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
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/input.h>

#include <sys/types.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Activate ( vlc_object_t * );
static int  Demux ( input_thread_t * );

/* TODO: support MPEG-2.5, not difficult, but I need somes samples... */

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("MPEG I/II audio stream demuxer" ) );
    set_capability( "demux", 50 );
    set_callbacks( Activate, NULL );
    add_shortcut( "mpegaudio" );
    add_shortcut( "mp3" );
vlc_module_end();

/*****************************************************************************
 * Definitions of structures  and functions used by this plugins
 *****************************************************************************/

/* XXX set this to 0 to avoid problem with PS XXX */
/* but with some file or web radio will failed to detect */
/* it's you to choose */
#define MPEGAUDIO_MAXTESTPOS    0

typedef struct mpeg_header_s
{
    uint32_t i_header;
    int i_version;
    int i_layer;
    int i_crc;
    int i_bitrate;
    int i_samplerate;
    int i_padding;
    int i_extension;
    int i_mode;
    int i_modeext;
    int i_copyright;
    int i_original;
    int i_emphasis;

} mpeg_header_t;

/* Xing Header if present */
#define FRAMES_FLAG     0x0001  /* these flags is for i_flags */
#define BYTES_FLAG      0x0002  /* because all is optionnal */
#define TOC_FLAG        0x0004
#define VBR_SCALE_FLAG  0x0008
typedef struct xing_header_s
{
    int     i_flags;      /* from Xing header data */
    int     i_frames;     /* total bit stream frames from Xing header data */
    int     i_bytes;      /* total bit stream bytes from Xing header data */
    int     i_vbr_scale;  /* encoded vbr scale from Xing header data */
    uint8_t i_toc[100];   /* for seek */
    int     i_avgbitrate; /* calculated, XXX: bits/sec not Kb */
} xing_header_t;

struct demux_sys_t
{
    mtime_t i_pts;

    es_descriptor_t *p_es;
    mpeg_header_t   mpeg;
    xing_header_t   xingheader;

    /* extracted information */
    int i_samplerate;
    int i_samplelength;
    int i_framelength;
};


static int mpegaudio_bitrate[2][3][16] =
{
  {
    /* v1 l1 */
    { 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0},
    /* v1 l2 */
    { 0, 32, 48, 56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 384, 0},
    /* v1 l3 */
    { 0, 32, 40, 48,  56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 0}
  },

  {
     /* v2 l1 */
    { 0, 32, 48, 56,  64,  80,  96, 112, 128, 144, 160, 176, 192, 224, 256, 0},
    /* v2 l2 */
    { 0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160, 0},
    /* v2 l3 */
    { 0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160, 0}
  }

};

static int mpegaudio_samplerate[2][4] = /* version 1 then 2 */
{
    { 44100, 48000, 32000, 0 },
    { 22050, 24000, 16000, 0 }
};

static char* mpegaudio_mode[4] =
{
    "stereo", "joint stereo", "dual channel", "mono"
};

static inline uint32_t GetDWBE( uint8_t *p_buff )
{
    return( ( p_buff[0] << 24 )|( p_buff[1] << 16 )|
            ( p_buff[2] <<  8 )|( p_buff[3] ) );
}

/*****************************************************************************
 * Function to manipulate stream easily
 *****************************************************************************
 *
 * SkipBytes : skip bytes, not yet optimised, read bytes to be skipped :P
 *
 * ReadPes : read data and make a PES
 *
 *****************************************************************************/
static int SkipBytes( input_thread_t *p_input, int i_size )
{
    data_packet_t *p_data;
    int i_read;

    while( i_size > 0 )
    {
        i_read = input_SplitBuffer(p_input, &p_data, __MIN( i_size, 1024 ) );
        if( i_read <= 0 )
        {
            return( 0 );
        }
        input_DeletePacket( p_input->p_method_data, p_data );
        i_size -= i_read;
    }
    return( 1 );
}

static int ReadPES( input_thread_t *p_input,
                    pes_packet_t **pp_pes,
                    int i_size )
{
    pes_packet_t *p_pes;

    *pp_pes = NULL;

    if( !(p_pes = input_NewPES( p_input->p_method_data )) )
    {
        msg_Err( p_input, "cannot allocate new PES" );
        return( 0 );
    }

    while( i_size > 0 )
    {
        data_packet_t   *p_data;
        int i_read;

        if( (i_read = input_SplitBuffer( p_input,
                                         &p_data,
                                         __MIN( i_size, 1024 ) ) ) <= 0 )
        {
            input_DeletePES( p_input->p_method_data, p_pes );
            return( 0 );
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
        i_size -= i_read;
    }
    *pp_pes = p_pes;
    return( 1 );
}

/*****************************************************************************
 * CheckHeader : Test the validity of the header
 *****************************************************************************/
static int CheckHeader( uint32_t i_header )
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
 * DecodedFrameSize : give the length of the decoded pcm data
 *****************************************************************************/
static int DecodedFrameSize( mpeg_header_t *p_mpeg )
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

/****************************************************************************
 * GetHeader : find an mpeg header and load it
 ****************************************************************************/
static int GetHeader( input_thread_t  *p_input,
                      mpeg_header_t   *p_mpeg,
                      int             i_max_pos,
                      int             *pi_skip )
{
    uint32_t i_header;
    uint8_t  *p_peek;
    int i_size;

    *pi_skip = 0;
    i_size = input_Peek( p_input, &p_peek, i_max_pos + 4 );

    for( ; ; )
    {
        if( i_size < 4 )
        {
            return( 0 );
        }
        if( !CheckHeader( GetDWBE( p_peek ) ) )
        {
            p_peek++;
            i_size--;
            *pi_skip += 1;
            continue;
        }
        /* we found an header, load it */
        break;
    }
    i_header = GetDWBE( p_peek );
    p_mpeg->i_header = i_header;
    p_mpeg->i_version =  1 - ( ( i_header >> 19 ) & 0x01 );
    p_mpeg->i_layer =  3 - ( ( i_header >> 17 ) & 0x03 );
    p_mpeg->i_crc = 1 - (( i_header >> 16 ) & 0x01);
    p_mpeg->i_bitrate =
        mpegaudio_bitrate[p_mpeg->i_version][p_mpeg->i_layer][(i_header>>12)&0x0F];
    p_mpeg->i_samplerate = mpegaudio_samplerate[p_mpeg->i_version][(i_header>>10)&0x03];
    p_mpeg->i_padding = (( i_header >> 9 ) & 0x01);
    p_mpeg->i_extension = ( i_header >> 7 ) & 0x01;
    p_mpeg->i_mode = ( i_header >> 6 ) & 0x03;
    p_mpeg->i_modeext = ( i_header >> 4 ) & 0x03;
    p_mpeg->i_copyright = ( i_header >> 3 ) & 0x01;
    p_mpeg->i_original = ( i_header >> 2 ) & 0x01;
    p_mpeg->i_emphasis = ( i_header ) & 0x03;

    return( 1 );
}

/*****************************************************************************
 * ExtractXingHeader : extract a Xing header if exist
 *****************************************************************************
 * It also calcul avgbitrate, using Xing header if present or assume that
 * the bitrate of the first frame is the same for the all file
 *****************************************************************************/
static void ExtractXingHeader( input_thread_t *p_input,
                               xing_header_t *p_xh )
{
    int i_skip;
    int i_size;
    uint8_t  *p_peek;
    mpeg_header_t mpeg;

    p_xh->i_flags = 0;  /* nothing present */
    if( !( GetHeader( p_input,
                      &mpeg,
                      8192,
                      &i_skip ) ) )
    {
        msg_Err( p_input, "ExtractXingHeader failed, shouldn't ..." );
        return;
    }

    p_xh->i_avgbitrate = mpeg.i_bitrate * 1000; /* default */

    /* 1024 is enougth */
    if( ( i_size = input_Peek( p_input, &p_peek, 1024 + i_skip ) ) < 8 )
    {
        return;
    }
    p_peek += i_skip;
    i_size -= i_skip;

    /* calculate pos of xing header */
    if( !mpeg.i_version )
    {
        p_peek += mpeg.i_mode != 3 ? 36 : 21;
        i_size -= mpeg.i_mode != 3 ? 36 : 21;
    }
    else
    {
        p_peek += mpeg.i_mode != 3 ? 21 : 13;
        i_size -= mpeg.i_mode != 3 ? 21 : 13;
    }
    if( i_size < 8 )
    {
        return;
    }
    if( ( p_peek[0] != 'X' )||( p_peek[1] != 'i' )||
        ( p_peek[2] != 'n' )||( p_peek[3] != 'g' ) )
    {
        return;
    }
    else
    {
        msg_Dbg( p_input, "Xing header is present" );
        p_peek += 4;
        i_size -= 4;
    }
    if( i_size < 4 )
    {
        return;
    }
    else
    {
        p_xh->i_flags = GetDWBE( p_peek );
        p_peek += 4;
        i_size -= 4;
    }

    if( ( p_xh->i_flags&FRAMES_FLAG )&&( i_size >= 4 ) )
    {
        p_xh->i_frames = GetDWBE( p_peek );
        if( p_xh->i_frames == 0 ) p_xh->i_flags &= ~FRAMES_FLAG;
        p_peek += 4;
        i_size -= 4;
    }
    if( ( p_xh->i_flags&BYTES_FLAG ) &&( i_size >= 4 ) )

    {
        p_xh->i_bytes = GetDWBE( p_peek );
        if( p_xh->i_bytes == 0 ) p_xh->i_flags &= ~BYTES_FLAG;
        p_peek += 4;
        i_size -= 4;
    }
    if( ( p_xh->i_flags&TOC_FLAG ) &&( i_size >= 100 ) )

    {
        memcpy( p_xh->i_toc, p_peek, 100 );
        p_peek += 100;
        i_size -= 100;
    }
    if( ( p_xh->i_flags&VBR_SCALE_FLAG ) &&( i_size >= 4 ) )

    {
        p_xh->i_vbr_scale = GetDWBE( p_peek );
        p_peek += 4;
        i_size -= 4;
    }

    if( ( p_xh->i_flags&FRAMES_FLAG )&&( p_xh->i_flags&BYTES_FLAG ) )
    {
        p_xh->i_avgbitrate =
              ( (uint64_t)p_xh->i_bytes *
                (uint64_t)8 *
                (uint64_t)mpeg.i_samplerate) /
               ((uint64_t)p_xh->i_frames * (uint64_t)DecodedFrameSize( &mpeg ) );
    }
}

/****************************************************************************
 * ExtractConfiguration : extract usefull informations from mpeg_header_t
 ****************************************************************************/
static void ExtractConfiguration( demux_sys_t *p_demux )
{
    p_demux->i_samplerate   = p_demux->mpeg.i_samplerate;

    p_demux->i_samplelength = DecodedFrameSize( &p_demux->mpeg );

    /* XXX if crc do i need to add 2 bytes or not? */
    switch( p_demux->mpeg.i_layer )
    {
        case( 0 ):
            p_demux->i_framelength =
                ( ( 12000 * p_demux->mpeg.i_bitrate ) /
                       p_demux->mpeg.i_samplerate + p_demux->mpeg.i_padding ) * 4;
            break;
        case( 1 ):
            p_demux->i_framelength =
                  ( 144000 * p_demux->mpeg.i_bitrate ) /
                       p_demux->mpeg.i_samplerate + p_demux->mpeg.i_padding;
            break;
        case( 2 ):
            p_demux->i_framelength =
                  (p_demux->mpeg.i_version ? 72000 : 144000) *
                  p_demux->mpeg.i_bitrate /
                       p_demux->mpeg.i_samplerate + p_demux->mpeg.i_padding;
            break;

    }
}

/****************************************************************************
 * CheckPS : check if this stream could be some ps,
 *           yes it's ugly ...  but another idea ?
 *
 ****************************************************************************/
static int CheckPS( input_thread_t *p_input )
{
    uint8_t  *p_peek;
    int i_startcode = 0;
    int i_size = input_Peek( p_input, &p_peek, 8196 );

    while( i_size > 4 )
    {
        if( ( p_peek[0] == 0 ) && ( p_peek[1] == 0 ) &&
            ( p_peek[2] == 1 ) && ( p_peek[3] >= 0xb9 ) &&
            ++i_startcode >= 3 )
        {
            return 1;
        }
        p_peek++;
        i_size--;
    }

    return 0;
}

/*****************************************************************************
 * Activate: initializes MPEGaudio structures
 *****************************************************************************/
static int Activate( vlc_object_t * p_this )
{
    input_thread_t * p_input = (input_thread_t *)p_this;
    demux_sys_t * p_demux;
    input_info_category_t * p_category;
    module_t * p_id3;

    int i_found;
    int b_forced;
    int i_skip;

    /* Set the demux function */
    p_input->pf_demux = Demux;

    /* Initialize access plug-in structures. */
    if( p_input->i_mtu == 0 )
    {
        /* Improve speed. */
        p_input->i_bufsize = INPUT_DEFAULT_BUFSIZE;
    }

    if( ( *p_input->psz_demux )
        &&( ( !strncmp( p_input->psz_demux, "mpegaudio", 10 ) )||
            ( !strncmp( p_input->psz_demux, "mp3", 3 ) ) ) )
    {
        b_forced = 1;
    }
    else
    {
        b_forced = 0;
    }
    p_id3 = module_Need( p_input, "id3", NULL );
    if ( p_id3 ) {
        module_Unneed( p_input, p_id3 );
    }

    /* create p_demux and init it */
    if( !( p_demux = p_input->p_demux_data = malloc( sizeof(demux_sys_t) ) ) )
    {
        msg_Err( p_input, "out of memory" );
        return( -1 );
    }
    memset( p_demux, 0, sizeof(demux_sys_t) );

    /* check if it could be a ps stream */
    if( !b_forced && CheckPS(  p_input ))
    {
        free( p_input->p_demux_data );
        return( -1 );
    }


    /* must be sure that is mpeg audio stream unless forced */
    if( !( i_found = GetHeader( p_input,
                                &p_demux->mpeg,
                                b_forced ? 4000 : MPEGAUDIO_MAXTESTPOS,
                                &i_skip ) ) )
    {
        if( b_forced )
        {
            msg_Warn( p_input,
                      "this does not look like an MPEG audio stream, "
                      "but continuing anyway" );
        }
        else
        {
            msg_Warn( p_input, "MPEGAudio module discarded (no frame found)" );
            free( p_input->p_demux_data );
            return( -1 );
        }
    }
    else
    {
        ExtractConfiguration( p_demux );
    }


    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( input_InitStream( p_input, 0 ) == -1)
    {
        msg_Err( p_input, "cannot init stream" );
        free( p_input->p_demux_data );
        return( -1 );
    }
    if( input_AddProgram( p_input, 0, 0) == NULL )
    {
        msg_Err( p_input, "cannot add program" );
        free( p_input->p_demux_data );
        return( -1 );
    }
    p_input->stream.pp_programs[0]->b_is_ok = 0;
    p_input->stream.p_selected_program = p_input->stream.pp_programs[0];

    /* create our ES */
    p_demux->p_es = input_AddES( p_input,
                                 p_input->stream.p_selected_program,
                                 1, /* id */
                                 0 );
    if( !p_demux->p_es )
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "out of memory" );
        free( p_input->p_demux_data );
        return( -1 );
    }
    p_demux->p_es->i_stream_id = 1;
    p_demux->p_es->i_fourcc = VLC_FOURCC('m','p','g','a');
    p_demux->p_es->i_cat = AUDIO_ES;

    input_SelectES( p_input, p_demux->p_es );

    p_input->stream.p_selected_program->b_is_ok = 1;
    vlc_mutex_unlock( &p_input->stream.stream_lock );


    if( i_found )
    {
        /* parse Xing Header if present */
        ExtractXingHeader( p_input, &p_demux->xingheader );

        vlc_mutex_lock( &p_input->stream.stream_lock );
        p_input->stream.i_mux_rate = p_demux->xingheader.i_avgbitrate / 50 / 8;
        vlc_mutex_unlock( &p_input->stream.stream_lock );


        /* all is ok :)) */
        msg_Dbg( p_input, "audio MPEG-%d layer %d %s %dHz %dKb/s %s",
                p_demux->mpeg.i_version + 1,
                p_demux->mpeg.i_layer + 1,
                mpegaudio_mode[p_demux->mpeg.i_mode],
                p_demux->mpeg.i_samplerate,
                p_demux->xingheader.i_avgbitrate / 1000,
                p_demux->xingheader.i_flags ?
                        "VBR (Xing)" : ""
                    );

        vlc_mutex_lock( &p_input->stream.stream_lock );
        p_category = input_InfoCategory( p_input, _("mpeg") );
        input_AddInfo( p_category, _("Input Type"), "Audio MPEG-%d",
                       p_demux->mpeg.i_version +1 );
        input_AddInfo( p_category, _("Layer"), "%d", p_demux->mpeg.i_layer + 1 );
        input_AddInfo( p_category, _("Mode"),
                       mpegaudio_mode[p_demux->mpeg.i_mode] );
        input_AddInfo( p_category, _("Sample Rate"), "%dHz",
                       p_demux->mpeg.i_samplerate );
        input_AddInfo( p_category, _("Average Bitrate"), "%dKb/s",
                       p_demux->xingheader.i_avgbitrate / 1000 );
        vlc_mutex_unlock( &p_input->stream.stream_lock );
    }
    else
    {
        msg_Dbg( p_input,
                 "assuming audio MPEG, but not frame header yet found" );
        vlc_mutex_lock( &p_input->stream.stream_lock );
        p_category = input_InfoCategory( p_input, _("mpeg") );
        input_AddInfo( p_category, _("Input Type"), "Audio MPEG-?" );
        vlc_mutex_unlock( &p_input->stream.stream_lock );

    }

    return( 0 );
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( input_thread_t * p_input )
{
    demux_sys_t  *p_demux = p_input->p_demux_data;
    pes_packet_t *p_pes;
    int          i_skip;

    if( !GetHeader( p_input,
                    &p_demux->mpeg,
                    8192,
                    &i_skip ) )
    {
        if( i_skip > 0)
        {
            msg_Dbg( p_input,
                     "skipping %d bytes (garbage ?)",
                     i_skip );
            SkipBytes( p_input, i_skip );
            return( 1 );
        }
        else
        {
            msg_Dbg( p_input,
                     "cannot find next frame (EOF ?)" );
            return( 0 );
        }
    }

    ExtractConfiguration( p_demux );

    input_ClockManageRef( p_input,
                          p_input->stream.p_selected_program,
                          p_demux->i_pts );

    /*
     * For layer 1 and 2 i_skip is garbage but for layer 3 it is not.
     * Since mad accept without to much trouble garbage I don't skip
     * it ( in case I misdetect garbage ... )
     *
     */
    if( !ReadPES( p_input, &p_pes, p_demux->i_framelength + i_skip) )
    {
        msg_Warn( p_input,
                "cannot read data" );
        return( -1 );
    }

    p_pes->i_rate = p_input->stream.control.i_rate;
    p_pes->i_dts =
        p_pes->i_pts = input_ClockGetTS( p_input,
                                         p_input->stream.p_selected_program,
                                         p_demux->i_pts );

    if( !p_demux->p_es->p_decoder_fifo )
    {
        msg_Err( p_input, "no audio decoder" );
        input_DeletePES( p_input->p_method_data, p_pes );
        return( -1 ); /* perhaps not, it's my choice */
    }
    else
    {
        input_DecodePES( p_demux->p_es->p_decoder_fifo, p_pes );
    }
    p_demux->i_pts += (mtime_t)90000 *
                      (mtime_t)p_demux->i_samplelength /
                      (mtime_t)p_demux->i_samplerate;
    return( 1 );

}


