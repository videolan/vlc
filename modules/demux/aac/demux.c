/*****************************************************************************
 * demux.c : Raw aac Stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: demux.c,v 1.4 2003/01/20 13:03:03 fenrir Exp $
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


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("AAC stream demux" ) );
    set_capability( "demux", 0 );
    set_callbacks( Activate, NULL );
    add_shortcut( "aac" );
vlc_module_end();

/*****************************************************************************
 * Definitions of structures  and functions used by this plugins 
 *****************************************************************************/

/* XXX set this to 0 to avoid problem with PS XXX */
/* but with some file or web radio will failed to detect */
/* it's you to choose */
#define AAC_MAXTESTPOS    0

static int i_aac_samplerate[16] =
{
    96000, 88200, 64000, 48000, 44100, 32000, 
    24000, 22050, 16000, 12000, 11025, 8000,
    7350,  0,     0,     0
};

typedef struct adts_header_s
{
    int i_id;
    int i_framelength;
    int i_layer;
    int i_protection_absent;
    int i_profile;
    int i_samplerate_index;
    int i_private_bit;
    int i_channel_configuration;
    int i_original;
    int i_home;
    int i_emphasis;
    int i_copyright_identification_bit;
    int i_copyright_identification_start;
    int i_aac_frame_length;
    int i_adts_buffer_fullness;
    int i_no_raw_data_blocks_in_frame;
    int i_crc_check;
                    
} adts_header_t;

/* Not yet used */
typedef struct adif_header_s
{
    int b_copyright_id_present;
    u8  i_copyright_id[10];
    
    int b_original_copy;

    int i_home;
    int i_bitstream_type;
    int i_bitrate;
    int i_num_program_config_elements;
    int adif_buffer_fullness;

//    program_config_element


} adif_header_t;

struct demux_sys_t
{
    mtime_t i_pts;

    es_descriptor_t         *p_es;

    int                     b_adif_header;
    adif_header_t           adif_header;

    int                     b_adts_header;
    adts_header_t           adts_header;

    /* extracted information */
    int i_samplerate_index;
    int i_object_type;
    int i_samplerate;
    int i_channels;
    int i_framelength;
    int i_aac_frame_length;
};

/****************************************************************************
 * bit_* : function to get a bitstream from a memory buffer
 ****************************************************************************
 *
 ****************************************************************************/ 
typedef struct bit_s
{
    u8 *p_buffer;
    int i_buffer;
    int i_mask;
} bit_t;

static void bit_init( bit_t *p_bit,
                      u8    *p_buffer,
                      int   i_buffer )
{
    p_bit->p_buffer = p_buffer;
    p_bit->i_buffer = i_buffer;
    p_bit->i_mask = 0x80;
}

static u32 bit_get( bit_t *p_bit )
{
    u32 i_bit;
    if( p_bit->i_buffer <= 0 )
    {
        return( 0 );
    }
    i_bit = ( p_bit->p_buffer[0]&p_bit->i_mask ) ? 1 : 0;
    
    p_bit->i_mask >>= 1;
    if( !p_bit->i_mask )
    {
        p_bit->p_buffer++;
        p_bit->i_buffer--;
        p_bit->i_mask = 0x80;
    }
    return( i_bit );
}

static u32 bit_gets( bit_t *p_bit, int i_count )
{
    u32 i_bits;
    i_bits = 0;
    for( ; i_count > 0; i_count-- )
    {
        i_bits = ( i_bits << 1 )|bit_get( p_bit );
    }
    return( i_bits );
}

/*****************************************************************************
 * Function to manipulate stream easily
 *****************************************************************************
 *
 * SkipBytes : skip bytes :) not yet uoptimised ( read bytes to be skipped )
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
 * GetADIF: find and load adif header if present
 *****************************************************************************/
static int GetADIF( input_thread_t *p_input,
                    adif_header_t  *p_adif )
{
    u8  *p_peek;
    int i_size;

    if( ( i_size = input_Peek( p_input, &p_peek, 60 ) ) < 60  )
    {
        /* it's false, min size is 11 byte but easier ;) */
        return( 0 );
    }
    if( ( p_peek[0] != 'A' )||( p_peek[1] != 'D' )||
        ( p_peek[2] != 'I' )||( p_peek[3] != 'F' ) )
    {
        return( 0 );
    }
    
    /* we now that we have an adif header */
    
//    return( 1 );
    return( 0 ); /* need some work */
}

/*****************************************************************************
 * GetADTS: find and load adts header 
 *****************************************************************************/
#define ADTS_HEADERSIZE 10 /* 8+2 for crc */
static int GetADTS( input_thread_t  *p_input,
                    adts_header_t   *p_adts,
                    int             i_max_pos,
                    int             *pi_skip )
{
    u8  *p_peek;
    int i_size;
    bit_t bit;

    if( ( i_size = input_Peek( p_input, &p_peek, i_max_pos + ADTS_HEADERSIZE ) )
            < ADTS_HEADERSIZE )
    {
        return( 0 );
    }
    *pi_skip = 0;
    for( ; ; )
    {
        if( i_size < ADTS_HEADERSIZE )
        {
            return( 0 );
        }
        if( ( p_peek[0] != 0xff )||
            ( ( p_peek[1]&0xf6 ) != 0xf0 ) ) /* Layer == 0 */
        {
            p_peek++;
            i_size--;
            *pi_skip += 1;
            continue;
        }
        /* we have find an adts header, load it */
        break;
    }

    bit_init( &bit, p_peek, i_size );
    bit_gets( &bit, 12 ); /* synchro bits */
    
    p_adts->i_id        = bit_get( &bit );
    p_adts->i_layer     = bit_gets( &bit, 2);
    p_adts->i_protection_absent = bit_get( &bit );
    p_adts->i_profile           = bit_gets( &bit, 2 );
    p_adts->i_samplerate_index  = bit_gets( &bit, 4);
    p_adts->i_private_bit       = bit_get( &bit );
    p_adts->i_channel_configuration = bit_gets( &bit, 3);
    p_adts->i_original  = bit_get( &bit );
    p_adts->i_home      = bit_get( &bit );
    if( p_adts->i_id == 0 )
    {
        p_adts->i_emphasis = bit_gets( &bit, 2);
    }

    p_adts->i_copyright_identification_bit   = bit_get( &bit );
    p_adts->i_copyright_identification_start = bit_get( &bit );
    p_adts->i_aac_frame_length               = bit_gets( &bit, 13 );
    p_adts->i_adts_buffer_fullness           = bit_gets( &bit, 11 );
    p_adts->i_no_raw_data_blocks_in_frame    = bit_gets( &bit, 2 );

    if( p_adts->i_protection_absent == 0 )
    {
        p_adts->i_crc_check = bit_gets( &bit, 16 );
    }
    return( 1 );
}

static void ExtractConfiguration( demux_sys_t *p_aac )
{
    if( p_aac->b_adif_header )
    {

    }
    if( p_aac->b_adts_header )
    {
        p_aac->i_samplerate_index = p_aac->adts_header.i_samplerate_index;
        p_aac->i_object_type = p_aac->adts_header.i_profile;
        p_aac->i_samplerate  = i_aac_samplerate[p_aac->i_samplerate_index];
        p_aac->i_channels    = p_aac->adts_header.i_channel_configuration;       
        if( p_aac->i_channels > 6 )
        {
            /* I'm not sure of that, got from faad */
            p_aac->i_channels = 2;
        }
        p_aac->i_aac_frame_length = p_aac->adts_header.i_aac_frame_length;
        p_aac->i_framelength = 1024;
    }
    /* FIXME FIXME for LD, but LD = ?
    if( p_aac->i_object_type == LD )
    {
        p_aac->i_framelength /= 2;
    }
    */
}

/****************************************************************************
 * CheckPS : check if this stream could be some ps, 
 *           yes it's ugly ...  but another idea ?
 *
 *           XXX it seems that aac stream always match ...
 *
 ****************************************************************************/

static int CheckPS( input_thread_t *p_input )
{
    u8 *p_peek;
    int i_size = input_Peek( p_input, &p_peek, 8196 );

    while( i_size >  4 )
    {
        if( ( p_peek[0] == 0 ) && ( p_peek[1] == 0 )&&
            ( p_peek[2] == 1 ) && ( p_peek[3] >= 0xb9 ) )
        {
            return( 1 ); /* Perhaps some ps stream */
        }
        p_peek++;
       i_size--; 
    }
    return( 0 );
}

/*****************************************************************************
 * Activate: initializes AAC demux structures
 *****************************************************************************/
static int Activate( vlc_object_t * p_this )
{
    input_thread_t * p_input = (input_thread_t *)p_this;
    demux_sys_t * p_aac;
    input_info_category_t * p_category;
    module_t * p_id3;
    
    int i_skip;
    int b_forced;

    /* Set the demux function */
    p_input->pf_demux = Demux;

    /* Initialize access plug-in structures. */
    if( p_input->i_mtu == 0 )
    {
    /* Improve speed. */
        p_input->i_bufsize = INPUT_DEFAULT_BUFSIZE;
    }

    b_forced = ( ( *p_input->psz_demux )&&
                 ( !strncmp( p_input->psz_demux, "aac", 10 ) ) ) ? 1 : 0;
    
    /* check if it can be a ps stream */
    if( !b_forced && CheckPS(  p_input ) )
    {
        return( -1 );
    }

    /* skip possible id3 header */    
    p_id3 = module_Need( p_input, "id3", NULL );
    if ( p_id3 ) {
        module_Unneed( p_input, p_id3 );
    }
    
    /* allocate p_aac */
    if( !( p_aac = malloc( sizeof( demux_sys_t ) ) ) )
    {
        msg_Err( p_input, "out of memory" );
        return( -1 );
    }
    memset( p_aac, 0, sizeof( demux_sys_t ) );
    
    /* Now check for adif/adts header */
    i_skip = 0;
    if( GetADIF( p_input, &p_aac->adif_header ) )
    {
        p_aac->b_adif_header = 1;
        msg_Err( p_input,
                 "found ADIF header (unsupported)" );
        free( p_aac );
        return( -1 );
    }
    else
    if( GetADTS( p_input, 
                 &p_aac->adts_header, 
                 b_forced ? 8000 : 0,
                 &i_skip  ) )
    {
        p_aac->b_adts_header = 1;
        msg_Info( p_input,
                  "found ADTS header" );
    }
    else
    {
        msg_Warn( p_input,
                  "AAC module discarded (no header found)" );
        free( p_aac );
        return( -1 );
    }
    ExtractConfiguration( p_aac );

    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( input_InitStream( p_input, 0 ) == -1)
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "cannot init stream" );
        return( -1 );
    }    
    if( input_AddProgram( p_input, 0, 0) == NULL )
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "cannot add program" );
        return( -1 );
    }
    p_input->stream.pp_programs[0]->b_is_ok = 0;
    p_input->stream.p_selected_program = p_input->stream.pp_programs[0];
    
    /* create our ES */ 
    p_aac->p_es = input_AddES( p_input, 
                               p_input->stream.p_selected_program, 
                               1, /* id */
                               0 );
    if( !p_aac->p_es )
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "out of memory" );
        return( -1 );
    }
    
    p_aac->p_es->i_stream_id = 1;
    p_aac->p_es->i_fourcc = VLC_FOURCC( 'm', 'p', '4', 'a' );
    p_aac->p_es->i_cat = AUDIO_ES;
    input_SelectES( p_input, p_aac->p_es );

    p_input->stream.p_selected_program->b_is_ok = 1;
    vlc_mutex_unlock( &p_input->stream.stream_lock );


    
    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( p_aac->b_adif_header )
    {
        p_input->stream.i_mux_rate = 0 / 50;
    }
    if( p_aac->b_adts_header )
    {
        p_input->stream.i_mux_rate = 0 / 50;
    }
    vlc_mutex_unlock( &p_input->stream.stream_lock );
#if 0
    /* FIXME FIXME FIXME FIXME FIXME FIXME FIXME */
    /* if i don't do that, it don't work correctly but why ??? */
    if( p_input->stream.b_seekable )
    {
        p_input->pf_seek( p_input, 0 );
        input_AccessReinit( p_input );
    }
    /* FIXME FIXME FIXME FIXME FIXME FIXME FIXME */
#endif

    /* all is ok :)) */
    if( p_aac->b_adif_header )
    {
    }

    if( p_aac->b_adts_header )
    {
        msg_Dbg( p_input,
                 "audio AAC MPEG-%d layer-%d  %d channels %dHz",
                 p_aac->adts_header.i_id == 1 ? 2 : 4,
                 4 - p_aac->adts_header.i_layer, /* it's always 4 in fact */
                 p_aac->i_channels,
                 p_aac->i_samplerate );

        vlc_mutex_lock( &p_input->stream.stream_lock );
        p_category = input_InfoCategory( p_input, "aac" );
    
        input_AddInfo( p_category, "input type", "MPEG-%d AAC",
                       p_aac->adts_header.i_id == 1 ? 2 : 4 );

        input_AddInfo( p_category, "layer", "%d", 
                       4 - p_aac->adts_header.i_layer );
        input_AddInfo( p_category, "channels", "%d", 
                       p_aac->i_channels );
        input_AddInfo( p_category, "sample rate", "%dHz", 
                       p_aac->i_samplerate );

        vlc_mutex_unlock( &p_input->stream.stream_lock );
    }

    p_input->p_demux_data = p_aac;
    

    return( 0 );
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( input_thread_t * p_input )
{
    int i_skip;
    int i_found;
    
    pes_packet_t    *p_pes;
    demux_sys_t *p_aac = p_input->p_demux_data;

    /* Get a frame */
    if( p_aac->b_adif_header )
    {
        i_skip = 0;
        return( -1 ); /* FIXME */
    }
    else
    if( p_aac->b_adts_header )
    {
        i_found = GetADTS( p_input, 
                           &p_aac->adts_header,
                           8000,
                           &i_skip );
    }
    else
    {
        return( -1 );
    }
    ExtractConfiguration( p_aac );
   
    /* skip garbage bytes */
    if( i_skip > 0 )
    {
        msg_Dbg( p_input,
                 "skipping %d bytes",
                 i_skip );
        SkipBytes( p_input, i_skip );
    }

    if( !i_found )
    {
        msg_Info( p_input, "can't find next frame" );
        return( 0 );
    }
    
    input_ClockManageRef( p_input,
                          p_input->stream.p_selected_program,
                          p_aac->i_pts );
   
    if( !ReadPES( p_input, &p_pes, p_aac->i_aac_frame_length ) )
    {
        msg_Warn( p_input,
                 "cannot read data" );
        return( -1 );
    }
    p_pes->i_dts =
        p_pes->i_pts = input_ClockGetTS( p_input,
                                         p_input->stream.p_selected_program,
                                         p_aac->i_pts );

    if( !p_aac->p_es->p_decoder_fifo )
    {
        msg_Err( p_input, "no audio decoder" );
        input_DeletePES( p_input->p_method_data, p_pes );
        return( -1 ); /* perhaps not, it's my choice */
    }
    else
    {
        input_DecodePES( p_aac->p_es->p_decoder_fifo, p_pes );
    }

    /* Update date information */
    p_aac->i_pts += (mtime_t)90000 * 
                    (mtime_t)p_aac->i_framelength /
                    (mtime_t)p_aac->i_samplerate;

    return( 1 );
}

