/*****************************************************************************
 * au.c : au file input module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2003 VideoLAN
 * $Id: au.c,v 1.3 2003/08/01 00:05:57 fenrir Exp $
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

#include <vlc/vlc.h>
#include <vlc/input.h>

#include <codecs.h>
#include <ninput.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close  ( vlc_object_t * );

vlc_module_begin();
    set_description( _("AU demuxer") );
    set_capability( "demux", 142 );
    set_callbacks( Open, Close );
vlc_module_end();

/*
 * TODO:
 *  - all adpcm things (I _NEED_ samples)
 */

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int    DemuxPCM   ( input_thread_t * );
/* TODO static int    DemuxADPCM   ( input_thread_t * ); */

#define GetDWBE( p ) __GetDWBE( (uint8_t*)(p) )
static inline uint32_t __GetDWBE( uint8_t *p )
{
    return( ( p[0] << 24 ) + ( p[1] << 16 ) + ( p[2] << 8 ) + p[3] );
}

enum AuType_e
{
    AU_UNKNOWN      =  0,
    AU_MULAW_8      =  1,  /* 8-bit ISDN u-law */
    AU_LINEAR_8     =  2,  /* 8-bit linear PCM */
    AU_LINEAR_16    =  3,  /* 16-bit linear PCM */
    AU_LINEAR_24    =  4,  /* 24-bit linear PCM */
    AU_LINEAR_32    =  5,  /* 32-bit linear PCM */
    AU_FLOAT        =  6,  /* 32-bit IEEE floating point */
    AU_DOUBLE       =  7,  /* 64-bit IEEE floating point */
    AU_ADPCM_G721   =  23, /* 4-bit CCITT g.721 ADPCM */
    AU_ADPCM_G722   =  24, /* CCITT g.722 ADPCM */
    AU_ADPCM_G723_3 =  25, /* CCITT g.723 3-bit ADPCM */
    AU_ADPCM_G723_5 =  26, /* CCITT g.723 5-bit ADPCM */
    AU_ALAW_8       =  27  /* 8-bit ISDN A-law */
};

enum AuCat_e
{
    AU_CAT_UNKNOWN  = 0,
    AU_CAT_PCM      = 1,
    AU_CAT_ADPCM    = 2
};

typedef struct
#ifdef HAVE_ATTRIBUTE_PACKED
    __attribute__((__packed__))
#endif
{
    uint32_t    i_header_size;
    uint32_t    i_data_size;
    uint32_t    i_encoding;
    uint32_t    i_sample_rate;
    uint32_t    i_channels;
} au_t;

struct demux_sys_t
{
    stream_t        *s;

    au_t            au;
    WAVEFORMATEX    wf;

    mtime_t         i_time;

    es_descriptor_t *p_es;

    int             i_frame_size;
    mtime_t         i_frame_length;
};


/*****************************************************************************
 * Open: check file and initializes structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    demux_sys_t    *p_sys;

    uint8_t         *p_peek;

    vlc_fourcc_t    i_fourcc;

    int             i_cat;

    /* a little test to see if it's a au file */
    if( input_Peek( p_input, &p_peek, 4 ) < 4 )
    {
        msg_Warn( p_input, "AU module discarded (cannot peek)" );
        return VLC_EGENERIC;
    }
    if( strncmp( p_peek, ".snd", 4 ) )
    {
        msg_Warn( p_input, "AU module discarded (not a valid file)" );
        return VLC_EGENERIC;
    }

    p_input->p_demux_data = p_sys = malloc( sizeof( demux_sys_t ) );
    p_sys->i_time = 0;


    if( ( p_sys->s = stream_OpenInput( p_input ) ) == NULL )
    {
        msg_Err( p_input, "cannot create stream" );
        goto error;
    }

    /* skip signature */
    stream_Read( p_sys->s, NULL, 4 );   /* cannot fail */

    /* read header */
    if( stream_Read( p_sys->s, &p_sys->au, sizeof( au_t ) ) < (int)sizeof( au_t ) )
    {
        msg_Err( p_input, "cannot load header" );
        goto error;
    }
    p_sys->au.i_header_size   = GetDWBE( &p_sys->au.i_header_size );
    p_sys->au.i_data_size     = GetDWBE( &p_sys->au.i_data_size );
    p_sys->au.i_encoding      = GetDWBE( &p_sys->au.i_encoding );
    p_sys->au.i_sample_rate   = GetDWBE( &p_sys->au.i_sample_rate );
    p_sys->au.i_channels      = GetDWBE( &p_sys->au.i_channels );

    msg_Dbg( p_input,
             "au file: header_size=%d data_size=%d encoding=0x%x sample_rate=%d channels=%d",
             p_sys->au.i_header_size,
             p_sys->au.i_data_size,
             p_sys->au.i_encoding,
             p_sys->au.i_sample_rate,
             p_sys->au.i_channels );

    if( p_sys->au.i_header_size < 24 )
    {
        msg_Warn( p_input, "AU module discarded (not a valid file)" );
        goto error;
    }

    /* skip extra header data */
    if( p_sys->au.i_header_size > 4 + sizeof( au_t ) )
    {
        stream_Read( p_sys->s, NULL, p_sys->au.i_header_size - 4 - sizeof( au_t ) );
    }

    /* Create WAVEFORMATEX structure */
    p_sys->wf.nChannels     = p_sys->au.i_channels;
    p_sys->wf.nSamplesPerSec= p_sys->au.i_sample_rate;
    p_sys->wf.cbSize        = 0;

    switch( p_sys->au.i_encoding )
    {
        case AU_ALAW_8:        /* 8-bit ISDN A-law */
            p_sys->wf.wFormatTag     = WAVE_FORMAT_ALAW;   // FIXME ??
            p_sys->wf.wBitsPerSample = 8;
            p_sys->wf.nBlockAlign    = 1 * p_sys->wf.nChannels;
            i_fourcc                 = VLC_FOURCC( 'a','l','a','w' );
            i_cat                    = AU_CAT_PCM;
            break;

        case AU_MULAW_8:       /* 8-bit ISDN u-law */
            p_sys->wf.wFormatTag     = WAVE_FORMAT_MULAW;   // FIXME ??
            p_sys->wf.wBitsPerSample = 8;
            p_sys->wf.nBlockAlign    = 1 * p_sys->wf.nChannels;
            i_fourcc                 = VLC_FOURCC( 'u','l','a','w' );
            i_cat                    = AU_CAT_PCM;
            break;

        case AU_LINEAR_8:      /* 8-bit linear PCM */
            p_sys->wf.wFormatTag     = WAVE_FORMAT_PCM;
            p_sys->wf.wBitsPerSample = 8;
            p_sys->wf.nBlockAlign    = 1 * p_sys->wf.nChannels;
            i_fourcc                 = VLC_FOURCC( 't','w','o','s' );
            i_cat                    = AU_CAT_PCM;
            break;

        case AU_LINEAR_16:     /* 16-bit linear PCM */
            p_sys->wf.wFormatTag     = WAVE_FORMAT_PCM;
            p_sys->wf.wBitsPerSample = 16;
            p_sys->wf.nBlockAlign    = 2 * p_sys->wf.nChannels;
            i_fourcc                 = VLC_FOURCC( 't','w','o','s' );
            i_cat                    = AU_CAT_PCM;
            break;

        case AU_LINEAR_24:     /* 24-bit linear PCM */
            p_sys->wf.wFormatTag     = WAVE_FORMAT_PCM;
            p_sys->wf.wBitsPerSample = 24;
            p_sys->wf.nBlockAlign    = 3 * p_sys->wf.nChannels;
            i_fourcc                 = VLC_FOURCC( 't','w','o','s' );
            i_cat                    = AU_CAT_PCM;
            break;

        case AU_LINEAR_32:     /* 32-bit linear PCM */
            p_sys->wf.wFormatTag     = WAVE_FORMAT_PCM;
            p_sys->wf.wBitsPerSample = 32;
            p_sys->wf.nBlockAlign    = 4 * p_sys->wf.nChannels;
            i_fourcc                 = VLC_FOURCC( 't','w','o','s' );
            i_cat                    = AU_CAT_PCM;
            break;

        case AU_FLOAT:         /* 32-bit IEEE floating point */
            p_sys->wf.wFormatTag     = WAVE_FORMAT_UNKNOWN;
            p_sys->wf.wBitsPerSample = 32;
            p_sys->wf.nBlockAlign    = 4 * p_sys->wf.nChannels;
            i_fourcc                  = VLC_FOURCC( 'a', 'u', 0, AU_FLOAT );
            i_cat                    = AU_CAT_PCM;
            break;

        case AU_DOUBLE:        /* 64-bit IEEE floating point */
            p_sys->wf.wFormatTag     = WAVE_FORMAT_UNKNOWN;
            p_sys->wf.wBitsPerSample = 64;
            p_sys->wf.nBlockAlign    = 8 * p_sys->wf.nChannels;
            i_fourcc                 = VLC_FOURCC( 'a', 'u', 0, AU_DOUBLE );
            i_cat                    = AU_CAT_PCM;
            break;

        case AU_ADPCM_G721:    /* 4-bit CCITT g.721 ADPCM */
            p_sys->wf.wFormatTag     = WAVE_FORMAT_UNKNOWN;
            p_sys->wf.wBitsPerSample = 0;
            p_sys->wf.nBlockAlign    = 0 * p_sys->wf.nChannels;
            i_fourcc                 = VLC_FOURCC( 'a', 'u', 0, AU_ADPCM_G721 );
            i_cat                    = AU_CAT_ADPCM;
            break;

        case AU_ADPCM_G722:    /* CCITT g.722 ADPCM */
            p_sys->wf.wFormatTag     = WAVE_FORMAT_UNKNOWN;
            p_sys->wf.wBitsPerSample = 0;
            p_sys->wf.nBlockAlign    = 0 * p_sys->wf.nChannels;
            i_fourcc                 = VLC_FOURCC( 'a', 'u', 0, AU_ADPCM_G722 );
            i_cat                    = AU_CAT_ADPCM;
            break;

        case AU_ADPCM_G723_3:  /* CCITT g.723 3-bit ADPCM */
            p_sys->wf.wFormatTag     = WAVE_FORMAT_UNKNOWN;
            p_sys->wf.wBitsPerSample = 0;
            p_sys->wf.nBlockAlign    = 0 * p_sys->wf.nChannels;
            i_fourcc                 = VLC_FOURCC( 'a', 'u', 0, AU_ADPCM_G723_3 );
            i_cat                    = AU_CAT_ADPCM;
            break;

        case AU_ADPCM_G723_5:  /* CCITT g.723 5-bit ADPCM */
            p_sys->wf.wFormatTag     = WAVE_FORMAT_UNKNOWN;
            p_sys->wf.wBitsPerSample = 0;
            p_sys->wf.nBlockAlign    = 0 * p_sys->wf.nChannels;
            i_fourcc                 = VLC_FOURCC( 'a', 'u', 0, AU_ADPCM_G723_5 );
            i_cat                    = AU_CAT_ADPCM;
            break;

        default:
            msg_Warn( p_input, "unknow encoding=0x%x", p_sys->au.i_encoding );
            i_cat                    = AU_CAT_UNKNOWN;
            goto error;
    }
    p_sys->wf.nAvgBytesPerSec        = p_sys->wf.nSamplesPerSec * p_sys->wf.nChannels * p_sys->wf.wBitsPerSample / 8;



    if( i_cat == AU_CAT_UNKNOWN || i_cat == AU_CAT_ADPCM )
    {
        p_sys->i_frame_size = 0;
        p_sys->i_frame_length = 0;

        msg_Err( p_input, "unsupported codec/type (Please report it)" );
        goto error;
    }
    else
    {
        int i_samples, i_modulo;

        /* read samples for 50ms of */
        i_samples = __MAX( p_sys->wf.nSamplesPerSec / 20, 1 );

        p_sys->i_frame_size = i_samples * p_sys->wf.nChannels * ( (p_sys->wf.wBitsPerSample + 7) / 8 );

        if( p_sys->wf.nBlockAlign > 0 )
        {
            if( ( i_modulo = p_sys->i_frame_size % p_sys->wf.nBlockAlign ) != 0 )
            {
                p_sys->i_frame_size += p_sys->wf.nBlockAlign - i_modulo;
            }
        }

        p_sys->i_frame_length = (mtime_t)1000000 *
                                (mtime_t)i_samples /
                                (mtime_t)p_sys->wf.nSamplesPerSec;

        p_input->pf_demux = DemuxPCM;
    }

    /*  create one program */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( input_InitStream( p_input, 0 ) == -1)
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "cannot init stream" );
        goto error;
    }
    if( input_AddProgram( p_input, 0, 0) == NULL )
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "cannot add program" );
        goto error;
    }

    p_input->stream.p_selected_program = p_input->stream.pp_programs[0];
    p_input->stream.i_mux_rate =  p_sys->wf.nAvgBytesPerSec / 50;

    p_sys->p_es = input_AddES( p_input, p_input->stream.p_selected_program,
                               0x01, AUDIO_ES, NULL, 0 );

    p_sys->p_es->i_stream_id   = 0x01;
    p_sys->p_es->i_fourcc      = i_fourcc;
    p_sys->p_es->p_waveformatex= malloc( sizeof( WAVEFORMATEX ) );
    memcpy( p_sys->p_es->p_waveformatex, &p_sys->wf, sizeof( WAVEFORMATEX ) );

    input_SelectES( p_input, p_sys->p_es );

    p_input->stream.p_selected_program->b_is_ok = 1;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return VLC_SUCCESS;

error:
    if( p_sys->s )
    {
        stream_Release( p_sys->s );
    }
    free( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * DemuxPCM: read packet and send them to decoders
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int DemuxPCM( input_thread_t *p_input )
{
    demux_sys_t  *p_sys = p_input->p_demux_data;
    pes_packet_t *p_pes;

    if( p_input->stream.p_selected_program->i_synchro_state == SYNCHRO_REINIT )
    {
        int64_t i_pos;

        stream_Control( p_sys->s, STREAM_GET_POSITION, &i_pos );
        if( p_sys->wf.nBlockAlign != 0 )
        {
            i_pos += p_sys->wf.nBlockAlign - i_pos % p_sys->wf.nBlockAlign;
            if( stream_Control( p_sys->s, STREAM_SET_POSITION, i_pos ) )
            {
                msg_Err( p_input, "STREAM_SET_POSITION failed (cannot resync)" );
            }
        }
    }

    input_ClockManageRef( p_input,
                          p_input->stream.p_selected_program,
                          p_sys->i_time * 9 / 100 );

    if( ( p_pes = stream_PesPacket( p_sys->s, p_sys->i_frame_size ) ) == NULL )
    {
        msg_Warn( p_input, "cannot read data" );
        return 0;
    }
    p_pes->i_dts =
    p_pes->i_pts = input_ClockGetTS( p_input,
                                     p_input->stream.p_selected_program,
                                     p_sys->i_time * 9 / 100 );

    if( !p_sys->p_es->p_decoder_fifo )
    {
        msg_Err( p_input, "no audio decoder" );
        input_DeletePES( p_input->p_method_data, p_pes );
        return( -1 );
    }

    input_DecodePES( p_sys->p_es->p_decoder_fifo, p_pes );
    p_sys->i_time += p_sys->i_frame_length;
    return( 1 );
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    demux_sys_t    *p_sys = p_input->p_demux_data;

    stream_Release( p_sys->s );
    free( p_sys );
}

