/*****************************************************************************
 * avi.c : AVI file Stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: avi.c,v 1.33 2002/07/31 20:56:50 sam Exp $
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
#include <string.h>                                              /* strdup() */
#include <errno.h>
#include <sys/types.h>

#include <vlc/vlc.h>
#include <vlc/input.h>

#include "video.h"

#include "libioRIFF.h"
#include "avi.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int    AVIInit   ( vlc_object_t * );
static void __AVIEnd    ( vlc_object_t * );
static int    AVIDemux  ( input_thread_t * );

#define AVIEnd(a) __AVIEnd(VLC_OBJECT(a))

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( "RIFF-AVI demuxer" );
    set_capability( "demux", 150 );
    set_callbacks( AVIInit, __AVIEnd );
vlc_module_end();

/*****************************************************************************
 * Some usefull functions to manipulate memory 
 *****************************************************************************/
static u16 GetWLE( byte_t *p_buff )
{
    u16 i;
    i = (*p_buff) + ( *(p_buff + 1) <<8 );
    return ( i );
}
static u32 GetDWLE( byte_t *p_buff )
{
    u32 i;
    i = (*p_buff) + ( *(p_buff + 1) <<8 ) + 
            ( *(p_buff + 2) <<16 ) + ( *(p_buff + 3) <<24 );
    return ( i );
}
static u32 GetDWBE( byte_t *p_buff )
{
    u32 i;
    i = ((*p_buff)<<24) + ( *(p_buff + 1) <<16 ) + 
            ( *(p_buff + 2) <<8 ) + ( *(p_buff + 3) );
    return ( i );
}
static inline off_t __EVEN( off_t i )
{
    return( (i & 1) ? i+1 : i );
}


/*****************************************************************************
 * Functions for parsing the headers in an avi file
 *****************************************************************************/
static void AVI_Parse_avih( MainAVIHeader_t *p_avih, byte_t *p_buff )
{
    p_avih->i_microsecperframe = GetDWLE( p_buff );
    p_avih->i_maxbytespersec   = GetDWLE( p_buff + 4);
    p_avih->i_reserved1        = GetDWLE( p_buff + 8);
    p_avih->i_flags            = GetDWLE( p_buff + 12);
    p_avih->i_totalframes      = GetDWLE( p_buff + 16);
    p_avih->i_initialframes    = GetDWLE( p_buff + 20);
    p_avih->i_streams          = GetDWLE( p_buff + 24);
    p_avih->i_suggestedbuffersize = GetDWLE( p_buff + 28);
    p_avih->i_width            = GetDWLE( p_buff + 32 );
    p_avih->i_height           = GetDWLE( p_buff + 36 );
    p_avih->i_scale            = GetDWLE( p_buff + 40 );
    p_avih->i_rate             = GetDWLE( p_buff + 44 );
    p_avih->i_start            = GetDWLE( p_buff + 48);
    p_avih->i_length           = GetDWLE( p_buff + 52);
}
static void AVI_Parse_Header( AVIStreamHeader_t *p_strh, byte_t *p_buff )
{
    p_strh->i_type      = GetDWLE( p_buff );
    p_strh->i_handler   = GetDWLE( p_buff + 4 );
    p_strh->i_flags     = GetDWLE( p_buff + 8 );
    p_strh->i_reserved1 = GetDWLE( p_buff + 12);
    p_strh->i_initialframes = GetDWLE( p_buff + 16);
    p_strh->i_scale     = GetDWLE( p_buff + 20);
    p_strh->i_rate      = GetDWLE( p_buff + 24);
    p_strh->i_start     = GetDWLE( p_buff + 28);
    p_strh->i_length    = GetDWLE( p_buff + 32);
    p_strh->i_suggestedbuffersize = GetDWLE( p_buff + 36);
    p_strh->i_quality   = GetDWLE( p_buff + 40);
    p_strh->i_samplesize = GetDWLE( p_buff + 44);
}
static void AVI_Parse_BitMapInfoHeader( bitmapinfoheader_t *h, byte_t *p_data )
{
    h->i_size          = GetDWLE( p_data );
    h->i_width         = GetDWLE( p_data + 4 );
    h->i_height        = GetDWLE( p_data + 8 );
    h->i_planes        = GetWLE( p_data + 12 );
    h->i_bitcount      = GetWLE( p_data + 14 );
    h->i_compression   = GetDWLE( p_data + 16 );
    h->i_sizeimage     = GetDWLE( p_data + 20 );
    h->i_xpelspermeter = GetDWLE( p_data + 24 );
    h->i_ypelspermeter = GetDWLE( p_data + 28 );
    h->i_clrused       = GetDWLE( p_data + 32 );
    h->i_clrimportant  = GetDWLE( p_data + 36 );
}
static void AVI_Parse_WaveFormatEx( waveformatex_t *h, byte_t *p_data )
{
    h->i_formattag     = GetWLE( p_data );
    h->i_channels      = GetWLE( p_data + 2 );
    h->i_samplespersec = GetDWLE( p_data + 4 );
    h->i_avgbytespersec= GetDWLE( p_data + 8 );
    h->i_blockalign    = GetWLE( p_data + 12 );
    h->i_bitspersample = GetWLE( p_data + 14 );
    h->i_size          = GetWLE( p_data + 16 );
}

static inline int AVI_GetESTypeFromTwoCC( u16 i_type )
{
    switch( i_type )
    {
        case( TWOCC_wb ):
            return( AUDIO_ES );
         case( TWOCC_dc ):
         case( TWOCC_db ):
            return( VIDEO_ES );
         default:
            return( UNKNOWN_ES );
    }
}

static vlc_fourcc_t AVI_AudioGetType( u32 i_type )
{
    switch( i_type )
    {
/*        case( WAVE_FORMAT_PCM ):
            return VLC_FOURCC('l','p','c','m'); */
        case( WAVE_FORMAT_AC3 ):
            return VLC_FOURCC('a','5','2',' ');
        case( WAVE_FORMAT_MPEG):
        case( WAVE_FORMAT_MPEGLAYER3):
            return VLC_FOURCC('m','p','g','a'); /* for mpeg2 layer 1 2 ou 3 */
        default:
            return 0;
    }
}

/* Test if it seems that it's a key frame */
static int AVI_GetKeyFlag( vlc_fourcc_t i_fourcc, u8 *p_byte )
{
    switch( i_fourcc )
    {
        case FOURCC_DIV1:
        case FOURCC_div1:
        case FOURCC_MPG4:
        case FOURCC_mpg4:
            if( GetDWBE( p_byte ) != 0x00000100 ) 
            /* startcode perhaps swapped, I haven't tested */
            {
            /* it's seems it's not an msmpegv1 stream 
               but perhaps I'm wrong so return yes */
                return( AVIIF_KEYFRAME );
            }
            else
            {
                return( (*(p_byte+4))&0x06 ? 0 : AVIIF_KEYFRAME);
            }
        case FOURCC_DIV2:
        case FOURCC_div2:
        case FOURCC_MP42:
        case FOURCC_mp42:
        case FOURCC_MPG3:
        case FOURCC_mpg3:
        case FOURCC_div3:
        case FOURCC_MP43:
        case FOURCC_mp43:
        case FOURCC_DIV3:
        case FOURCC_DIV4:
        case FOURCC_div4:
        case FOURCC_DIV5:
        case FOURCC_div5:
        case FOURCC_DIV6:
        case FOURCC_div6:
        case FOURCC_AP41:
        case FOURCC_3IV1:
//            printf( "\n Is a Key Frame %s", (*p_byte)&0xC0 ? "no" : "yes!!" );
            return( (*p_byte)&0xC0 ? 0 : AVIIF_KEYFRAME );
        case FOURCC_DIVX:
        case FOURCC_divx:
        case FOURCC_MP4S:
        case FOURCC_mp4s:
        case FOURCC_M4S2:
        case FOURCC_m4s2:
        case FOURCC_xvid:
        case FOURCC_XVID:
        case FOURCC_XviD:
        case FOURCC_DX50:
        case FOURCC_mp4v:
        case FOURCC_4:
            if( GetDWBE( p_byte ) != 0x000001b6 )
            {
                /* not true , need to find the first VOP header
                    but, I'm lazy */
                return( AVIIF_KEYFRAME );
            }
            else
            {
//                printf( "\n Is a Key Frame %s", (*(p_byte+4))&0xC0 ? "no" : 
//                                                                   "yes!!" );
                return( (*(p_byte+4))&0xC0 ? 0 : AVIIF_KEYFRAME );
            }
        default:
            /* I can't do it, so said yes */
            return( AVIIF_KEYFRAME );
    }

}
/*****************************************************************************
 * Data and functions to manipulate pes buffer
 *****************************************************************************/
#define BUFFER_MAXTOTALSIZE   512*1024 /* 1/2 Mo */
#define BUFFER_MAXSPESSIZE 1024*200
static int  AVI_PESBuffer_IsFull( AVIStreamInfo_t *p_info )
{
    return( p_info->i_pes_totalsize > BUFFER_MAXTOTALSIZE ? 1 : 0);
}
static void AVI_PESBuffer_Add( input_buffers_t *p_method_data,
                               AVIStreamInfo_t *p_info,
                               pes_packet_t *p_pes,
                               int i_posc,
                               int i_posb )
{
    AVIESBuffer_t   *p_buffer_pes;

    if( p_info->i_pes_totalsize > BUFFER_MAXTOTALSIZE )
    {
        input_DeletePES( p_method_data, p_pes );
        return;
    }
    
    if( !( p_buffer_pes = malloc( sizeof( AVIESBuffer_t ) ) ) )
    {
        input_DeletePES( p_method_data, p_pes );
        return;
    }
    p_buffer_pes->p_next = NULL;
    p_buffer_pes->p_pes = p_pes;
    p_buffer_pes->i_posc = i_posc;
    p_buffer_pes->i_posb = i_posb;

    if( p_info->p_pes_last ) 
    {
        p_info->p_pes_last->p_next = p_buffer_pes;
    }
    p_info->p_pes_last = p_buffer_pes;
    if( !p_info->p_pes_first )
    {
        p_info->p_pes_first = p_buffer_pes;
    }
    p_info->i_pes_count++;
    p_info->i_pes_totalsize += p_pes->i_pes_size;
}
static pes_packet_t *AVI_PESBuffer_Get( AVIStreamInfo_t *p_info )
{
    AVIESBuffer_t   *p_buffer_pes;
    pes_packet_t    *p_pes;
    if( p_info->p_pes_first )
    {
        p_buffer_pes = p_info->p_pes_first;
        p_info->p_pes_first = p_buffer_pes->p_next;
        if( !p_info->p_pes_first )
        {
            p_info->p_pes_last = NULL;
        }
        p_pes = p_buffer_pes->p_pes;

        free( p_buffer_pes );
        p_info->i_pes_count--;
        p_info->i_pes_totalsize -= p_pes->i_pes_size;
        return( p_pes );
    }
    else
    {
        return( NULL );
    }
}
static int AVI_PESBuffer_Drop( input_buffers_t *p_method_data,
                                AVIStreamInfo_t *p_info )
{
    pes_packet_t *p_pes = AVI_PESBuffer_Get( p_info );
    if( p_pes )
    {
        input_DeletePES( p_method_data, p_pes );
        return( 1 );
    }
    else
    {
        return( 0 );
    }
}
static void AVI_PESBuffer_Flush( input_buffers_t *p_method_data,
                                 AVIStreamInfo_t *p_info )
{
    while( p_info->p_pes_first )
    {
        AVI_PESBuffer_Drop( p_method_data, p_info );
    }
}

static void AVI_ParseStreamHeader( u32 i_id, int *i_number, int *i_type )
{
    int c1,c2;

    c1 = ( i_id ) & 0xFF;
    c2 = ( i_id >>  8 ) & 0xFF;

    if( c1 < '0' || c1 > '9' || c2 < '0' || c2 > '9' )
    {
        *i_number = 100; /* > max stream number */
        *i_type = 0;
    }
    else
    {
        *i_number = (c1 - '0') * 10 + (c2 - '0' );
        *i_type = ( i_id >> 16 ) & 0xFFFF;
    }
}

/* Function to manipulate stream easily */
static off_t AVI_TellAbsolute( input_thread_t *p_input )
{
    off_t i_pos;
    vlc_mutex_lock( &p_input->stream.stream_lock );
    i_pos= p_input->stream.p_selected_area->i_tell -
            ( p_input->p_last_data - p_input->p_current_data  );
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return( i_pos );
}
                            
static int AVI_SeekAbsolute( input_thread_t *p_input,
                             off_t i_pos)
{
    off_t i_filepos;
    /* FIXME add support for not seekable stream */

    i_filepos = AVI_TellAbsolute( p_input );
    if( i_pos != i_filepos )
    {
//        msg_Err( p_input, "Seek --> delta %d", i_pos - i_filepos );
        p_input->pf_seek( p_input, i_pos );
        input_AccessReinit( p_input );
    }
    return( 1 );
}


static void __AVI_AddEntryIndex( AVIStreamInfo_t *p_info,
                                 AVIIndexEntry_t *p_index)
{
    AVIIndexEntry_t *p_tmp;
    if( p_info->p_index == NULL )
    {
        p_info->i_idxmax = 16384;
        p_info->i_idxnb = 0;
        p_info->p_index = calloc( p_info->i_idxmax, 
                                  sizeof( AVIIndexEntry_t ) );
        if( p_info->p_index == NULL ) {return;}
    }
    if( p_info->i_idxnb >= p_info->i_idxmax )
    {
        p_info->i_idxmax += 16384;
        p_tmp = realloc( (void*)p_info->p_index,
                           p_info->i_idxmax * 
                           sizeof( AVIIndexEntry_t ) );
        if( !p_tmp ) 
        { 
            p_info->i_idxmax -= 16384;
            return; 
        }
        p_info->p_index = p_tmp;
    }
    /* calculate cumulate length */
    if( p_info->i_idxnb > 0 )
    {
        p_index->i_lengthtotal = p_info->p_index[p_info->i_idxnb-1].i_length +
                p_info->p_index[p_info->i_idxnb-1].i_lengthtotal;
    }
    else
    {
        p_index->i_lengthtotal = 0;
    }

    p_info->p_index[p_info->i_idxnb] = *p_index;
    p_info->i_idxnb++;
}

static void __AVI_GetIndex( input_thread_t *p_input )
{
    AVIIndexEntry_t index;
    byte_t          *p_buff;
    riffchunk_t     *p_idx1;
    int             i_read;
    int             i;
    int             i_number;
    int             i_type;
    int             i_totalentry = 0;
    demux_data_avi_file_t *p_avi_demux =
                        (demux_data_avi_file_t*)p_input->p_demux_data  ;    

    if( RIFF_FindAndGotoDataChunk( p_input,
                                   p_avi_demux->p_riff, 
                                   &p_idx1, 
                                   FOURCC_idx1)!=0 )
    {
        msg_Warn( p_input, "cannot find index" );
        RIFF_GoToChunk( p_input, p_avi_demux->p_hdrl );        
        return;
    }
    p_avi_demux->p_idx1 = p_idx1;
    msg_Dbg( p_input, "loading index" ); 
    for(;;)
    {
        i_read = __MIN( 16*1024, p_idx1->i_size - i_totalentry *16);
        if( ((i_read = input_Peek( p_input, &p_buff, i_read )) < 16 )
              ||( i_totalentry *16 >= p_idx1->i_size ) )
        {
            msg_Dbg( p_input, "read %d idx entries", i_totalentry );
            return;
        }
        i_read /= 16 ;
        for( i = 0; i < i_read; i++ )
        {
            byte_t  *p_peek = p_buff + i * 16;
            i_totalentry++;
            index.i_id      = GetDWLE( p_peek );
            index.i_flags   = GetDWLE( p_peek+4)&(~AVIIF_FIXKEYFRAME);
            index.i_pos     = GetDWLE( p_peek+8);
            index.i_length  = GetDWLE(p_peek+12);
            AVI_ParseStreamHeader( index.i_id, &i_number, &i_type );
            
            if( ( i_number <  p_avi_demux->i_streams )
               &&(p_avi_demux->pp_info[i_number]->i_cat == 
                     AVI_GetESTypeFromTwoCC( i_type ))) 
            {
                __AVI_AddEntryIndex( p_avi_demux->pp_info[i_number],
                                     &index );
            }
        }
        __RIFF_SkipBytes( p_input, 16 * i_read );
    }

}

/* XXX call after get p_movi */
static void __AVI_UpdateIndexOffset( input_thread_t *p_input )
{
    int i_stream;
    int b_start = 1;/* if index pos is based on start of file or not (p_movi) */
    demux_data_avi_file_t *p_avi_demux =
                        (demux_data_avi_file_t*)p_input->p_demux_data;

/* FIXME some work to do :
        * test in the file if it's true, if not do a RIFF_Find...
*/
#define p_info p_avi_demux->pp_info[i_stream]
    for( i_stream = 0; i_stream < p_avi_demux->i_streams; i_stream++ )
    {
        if( ( p_info->p_index )
           && ( p_info->p_index[0].i_pos < p_avi_demux->p_movi->i_pos + 8 ))
        {
            b_start = 0;
            break;
        }
    }
    if( !b_start )
    {
        for( i_stream = 0; i_stream < p_avi_demux->i_streams; i_stream++ )
        {
            int i;
            if( p_info->p_index )
            {
                for( i = 0; i < p_info->i_idxnb; i++ )
                {
                    p_info->p_index[i].i_pos += p_avi_demux->p_movi->i_pos + 8;
                }
            }
        }
    }
#undef p_info
}

/*****************************************************************************
 * AVIEnd: frees unused data
 *****************************************************************************/
static void __AVIEnd ( vlc_object_t * p_this )
{   
    input_thread_t *    p_input = (input_thread_t *)p_this;
    int i;
    demux_data_avi_file_t *p_avi_demux;
    p_avi_demux = (demux_data_avi_file_t*)p_input->p_demux_data  ; 
    
    if( p_avi_demux->p_riff ) 
            RIFF_DeleteChunk( p_input, p_avi_demux->p_riff );
    if( p_avi_demux->p_hdrl ) 
            RIFF_DeleteChunk( p_input, p_avi_demux->p_hdrl );
    if( p_avi_demux->p_movi ) 
            RIFF_DeleteChunk( p_input, p_avi_demux->p_movi );
    if( p_avi_demux->p_idx1 ) 
            RIFF_DeleteChunk( p_input, p_avi_demux->p_idx1 );
    if( p_avi_demux->pp_info )
    {
        for( i = 0; i < p_avi_demux->i_streams; i++ )
        {
            if( p_avi_demux->pp_info[i] ) 
            {
                if( p_avi_demux->pp_info[i]->p_index )
                {
                      free( p_avi_demux->pp_info[i]->p_index );
                      AVI_PESBuffer_Flush( p_input->p_method_data, 
                                           p_avi_demux->pp_info[i] );
                }
                free( p_avi_demux->pp_info[i] ); 
            }
        }
         free( p_avi_demux->pp_info );
    }
}

/*****************************************************************************
 * AVIInit: check file and initializes AVI structures
 *****************************************************************************/
static int AVIInit( vlc_object_t * p_this )
{   
    input_thread_t *    p_input = (input_thread_t *)p_this;
    riffchunk_t *p_riff,*p_hdrl,*p_movi;
    riffchunk_t *p_avih;
    riffchunk_t *p_strl,*p_strh,*p_strf;
    demux_data_avi_file_t *p_avi_demux;
    es_descriptor_t *p_es = NULL; /* for not warning */
    int i;

    p_input->pf_demux = AVIDemux;

    if( !( p_input->p_demux_data = 
                    p_avi_demux = malloc( sizeof(demux_data_avi_file_t) ) ) )
    {
        msg_Err( p_input, "out of memory" );
        return( -1 );
    }
    memset( p_avi_demux, 0, sizeof( demux_data_avi_file_t ) );
    p_avi_demux->i_rate = DEFAULT_RATE;
    p_avi_demux->b_seekable = ( ( p_input->stream.b_seekable )
                        &&( p_input->stream.i_method == INPUT_METHOD_FILE ) );

    /* Initialize access plug-in structures. */
    if( p_input->i_mtu == 0 )
    {
        /* Improve speed. */
        p_input->i_bufsize = INPUT_DEFAULT_BUFSIZE;
    }

    if( RIFF_TestFileHeader( p_input, &p_riff, FOURCC_AVI ) != 0 )    
    {
        AVIEnd( p_input );
        msg_Warn( p_input, "RIFF-AVI module discarded" );
        return( -1 );
    }
    p_avi_demux->p_riff = p_riff;

    if ( RIFF_DescendChunk(p_input) != 0 )
    {
        AVIEnd( p_input );
        msg_Err( p_input, "cannot look for subchunk" );
        return ( -1 );
    }

    /* it's a riff-avi file, so search for LIST-hdrl */
    if( RIFF_FindListChunk(p_input ,&p_hdrl,p_riff, FOURCC_hdrl) != 0 )
    {
        AVIEnd( p_input );
        msg_Err( p_input, "cannot find \"LIST-hdrl\"" );
        return( -1 );
    }
    p_avi_demux->p_hdrl = p_hdrl;

    if( RIFF_DescendChunk(p_input) != 0 )
    {
        AVIEnd( p_input );
        msg_Err( p_input, "cannot look for subchunk" );
        return ( -1 );
    }
    /* in  LIST-hdrl search avih */
    if( RIFF_FindAndLoadChunk( p_input, p_hdrl, 
                                    &p_avih, FOURCC_avih ) != 0 )
    {
        AVIEnd( p_input );
        msg_Err( p_input, "cannot find \"avih\" chunk" );
        return( -1 );
    }
    AVI_Parse_avih( &p_avi_demux->avih, p_avih->p_data->p_payload_start );
    RIFF_DeleteChunk( p_input, p_avih );
    
    if( p_avi_demux->avih.i_streams == 0 )  
    /* no stream found, perhaps it would be cool to find it */
    {
        AVIEnd( p_input );
        msg_Err( p_input, "no stream defined!" );
        return( -1 );
    }

    /*  create one program */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( input_InitStream( p_input, 0 ) == -1)
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        AVIEnd( p_input );
        msg_Err( p_input, "cannot init stream" );
        return( -1 );
    }
    if( input_AddProgram( p_input, 0, 0) == NULL )
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        AVIEnd( p_input );
        msg_Err( p_input, "cannot add program" );
        return( -1 );
    }
    p_input->stream.p_selected_program = p_input->stream.pp_programs[0];
    p_input->stream.i_mux_rate = p_avi_demux->avih.i_maxbytespersec / 50;
    vlc_mutex_unlock( &p_input->stream.stream_lock ); 

    /* now read info on each stream and create ES */
    p_avi_demux->i_streams = p_avi_demux->avih.i_streams;
    
    p_avi_demux->pp_info = calloc( p_avi_demux->i_streams, 
                                    sizeof( AVIStreamInfo_t* ) );
    memset( p_avi_demux->pp_info, 
            0, 
            sizeof( AVIStreamInfo_t* ) * p_avi_demux->i_streams );

    for( i = 0 ; i < p_avi_demux->i_streams; i++ )
    {
#define p_info  p_avi_demux->pp_info[i]
        p_info = malloc( sizeof(AVIStreamInfo_t ) );
        memset( p_info, 0, sizeof( AVIStreamInfo_t ) );        

        if( ( RIFF_FindListChunk(p_input,
                                &p_strl,p_hdrl, FOURCC_strl) != 0 )
                ||( RIFF_DescendChunk(p_input) != 0 ))
        {
            AVIEnd( p_input );
            msg_Err( p_input, "cannot find \"LIST-strl\"" );
            return( -1 );
        }
        
        /* in  LIST-strl search strh */
        if( RIFF_FindAndLoadChunk( p_input, p_hdrl, 
                                &p_strh, FOURCC_strh ) != 0 )
        {
            RIFF_DeleteChunk( p_input, p_strl );
            AVIEnd( p_input );
            msg_Err( p_input, "cannot find \"strh\"" );
            return( -1 );
        }
        AVI_Parse_Header( &p_info->header,
                            p_strh->p_data->p_payload_start);
        RIFF_DeleteChunk( p_input, p_strh );      

        /* in  LIST-strl search strf */
        if( RIFF_FindAndLoadChunk( p_input, p_hdrl, 
                                &p_strf, FOURCC_strf ) != 0 )
        {
            RIFF_DeleteChunk( p_input, p_strl );
            AVIEnd( p_input );
            msg_Err( p_input, "cannot find \"strf\"" );
            return( -1 );
        }
        /* we don't get strd, it's useless for divx,opendivx,mepgaudio */ 
        if( RIFF_AscendChunk(p_input, p_strl) != 0 )
        {
            RIFF_DeleteChunk( p_input, p_strf );
            RIFF_DeleteChunk( p_input, p_strl );
            AVIEnd( p_input );
            msg_Err( p_input, "cannot go out (\"strl\")" );
            return( -1 );
        }

        /* add one ES */
        vlc_mutex_lock( &p_input->stream.stream_lock );
        p_es = input_AddES( p_input,
                            p_input->stream.p_selected_program, 1+i,
                            p_strf->i_size );
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        p_es->i_stream_id =i; /* XXX: i don't use it */ 
       
        switch( p_info->header.i_type )
        {
            case( FOURCC_auds ):
                p_es->i_cat = AUDIO_ES;
                AVI_Parse_WaveFormatEx( &p_info->audio_format,
                                   p_strf->p_data->p_payload_start ); 
                p_es->i_fourcc = AVI_AudioGetType(
                                     p_info->audio_format.i_formattag );
                break;
                
            case( FOURCC_vids ):
                p_es->i_cat = VIDEO_ES;
                AVI_Parse_BitMapInfoHeader( &p_info->video_format,
                                   p_strf->p_data->p_payload_start ); 

                /* XXX quick hack for playing ffmpeg video, I don't know 
                    who is doing something wrong */
                p_info->header.i_samplesize = 0;
                p_es->i_fourcc = p_info->video_format.i_compression;
                break;
            default:
                msg_Err( p_input, "unknown stream(%d) type", i );
                p_es->i_cat = UNKNOWN_ES;
                break;
        }
        p_info->p_es = p_es;
        p_info->i_cat = p_es->i_cat;
        /* We copy strf for decoder in p_es->p_demux_data */
        memcpy( p_es->p_demux_data, 
                p_strf->p_data->p_payload_start,
                p_strf->i_size );
        RIFF_DeleteChunk( p_input, p_strf );
        RIFF_DeleteChunk( p_input, p_strl );
#undef p_info           
    }



    /* go out of p_hdrl */
    if( RIFF_AscendChunk(p_input, p_hdrl) != 0)
    {
        AVIEnd( p_input );
        msg_Err( p_input, "cannot go out (\"hdrl\")" );
        return( -1 );
    }

    /* go to movi chunk to get it*/
    if( RIFF_FindListChunk(p_input ,&p_movi,p_riff, FOURCC_movi) != 0 )
    {
        msg_Err( p_input, "cannot find \"LIST-movi\"" );
        AVIEnd( p_input );
        return( -1 );
    }
    p_avi_demux->p_movi = p_movi;
    
    /* get index  XXX need to have p_movi */
    if( p_avi_demux->b_seekable )
    {
        /* get index */
        __AVI_GetIndex( p_input ); 
        /* try to get i_idxoffset for each stream  */
        __AVI_UpdateIndexOffset( p_input );
        /* to make sure to go the begining unless demux will see a seek */
        RIFF_GoToChunk( p_input, p_avi_demux->p_movi );

    }
    else
    {
        msg_Warn( p_input, "no index!" );
    }

    if( RIFF_DescendChunk( p_input ) != 0 )
    {
        AVIEnd( p_input );
        msg_Err( p_input, "cannot go in (\"movi\")" );
        return( -1 );
    }

    /* print informations on streams */
    msg_Dbg( p_input, "AVIH: %d stream, flags %s%s%s%s ", 
             p_avi_demux->i_streams,
             p_avi_demux->avih.i_flags&AVIF_HASINDEX?" HAS_INDEX":"",
             p_avi_demux->avih.i_flags&AVIF_MUSTUSEINDEX?" MUST_USE_INDEX":"",
             p_avi_demux->avih.i_flags&AVIF_ISINTERLEAVED?" IS_INTERLEAVED":"",
             p_avi_demux->avih.i_flags&AVIF_TRUSTCKTYPE?" TRUST_CKTYPE":"" );

    for( i = 0; i < p_avi_demux->i_streams; i++ )
    {
#define p_info  p_avi_demux->pp_info[i]
        switch( p_info->p_es->i_cat )
        {
            case( VIDEO_ES ):

                msg_Dbg( p_input, "video(%4.4s) %dx%d %dbpp %ffps",
                         (char*)&p_info->video_format.i_compression,
                         p_info->video_format.i_width,
                         p_info->video_format.i_height,
                         p_info->video_format.i_bitcount,
                         (float)p_info->header.i_rate /
                             (float)p_info->header.i_scale );
                if( (p_avi_demux->p_info_video == NULL) ) 
                {
                    p_avi_demux->p_info_video = p_info;
                    /* TODO add test to see if a decoder has been found */
                    vlc_mutex_lock( &p_input->stream.stream_lock );
                    input_SelectES( p_input, p_info->p_es );
                    vlc_mutex_unlock( &p_input->stream.stream_lock );
                }
                break;

            case( AUDIO_ES ):
                msg_Dbg( p_input, "audio(0x%x) %d channels %dHz %dbits",
                         p_info->audio_format.i_formattag,
                         p_info->audio_format.i_channels,
                         p_info->audio_format.i_samplespersec,
                         p_info->audio_format.i_bitspersample );
                if( (p_avi_demux->p_info_audio == NULL) ) 
                {
                    p_avi_demux->p_info_audio = p_info;
                    vlc_mutex_lock( &p_input->stream.stream_lock );
                    input_SelectES( p_input, p_info->p_es );
                    vlc_mutex_unlock( &p_input->stream.stream_lock );
                }
                break;
            default:
                break;
        }
#undef p_info    
    }


    /* we select the first audio and video ES */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( !p_avi_demux->p_info_video ) 
    {
        msg_Warn( p_input, "no video stream found" );
    }
    if( !p_avi_demux->p_info_audio )
    {
        msg_Warn( p_input, "no audio stream found!" );
    }
    p_input->stream.p_selected_program->b_is_ok = 1;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return( 0 );
}






/*****************************************************************************
 * Function to convert pts to chunk or byte
 *****************************************************************************/

static inline mtime_t AVI_PTSToChunk( AVIStreamInfo_t *p_info, 
                                        mtime_t i_pts )
{
    return( (mtime_t)((s64)i_pts *
                      (s64)p_info->header.i_rate /
                      (s64)p_info->header.i_scale /
                      (s64)1000000 ) );
}
static inline mtime_t AVI_PTSToByte( AVIStreamInfo_t *p_info,
                                       mtime_t i_pts )
{
    return( (mtime_t)((s64)i_pts * 
                      (s64)p_info->header.i_samplesize *
                      (s64)p_info->header.i_rate /
                      (s64)p_info->header.i_scale /
                      (s64)1000000 ) );

}
static mtime_t AVI_GetPTS( AVIStreamInfo_t *p_info )
{
    
    if( p_info->header.i_samplesize )
    {
        /* we need a valid entry we will emulate one */
        int i_len;
        if( p_info->i_idxposc == p_info->i_idxnb )
        {
            if( p_info->i_idxposc )
            {
                /* use the last entry */
                i_len = p_info->p_index[p_info->i_idxnb - 1].i_lengthtotal
                            + p_info->p_index[p_info->i_idxnb - 1].i_length
                            + p_info->i_idxposb; /* should be 0 */
            }
            else
            {
                i_len = p_info->i_idxposb; 
                /* no valid entry use only offset*/
            }
        }
        else
        {
            i_len = p_info->p_index[p_info->i_idxposc].i_lengthtotal
                                + p_info->i_idxposb;
        }
        return( (mtime_t)( (s64)1000000 *
                   (s64)i_len *
                    (s64)p_info->header.i_scale /
                    (s64)p_info->header.i_rate /
                    (s64)p_info->header.i_samplesize ) );
    }
    else
    {
        /* even if p_info->i_idxposc isn't valid, there isn't any probllem */
        return( (mtime_t)( (s64)1000000 *
                    (s64)(p_info->i_idxposc ) *
                    (s64)p_info->header.i_scale /
                    (s64)p_info->header.i_rate) );
    }
}


/*****************************************************************************
 * Functions to acces streams data 
 * Uses it, because i plane to read unseekable stream
 * Don't work for the moment for unseekable stream 
 * XXX NEVER set directly i_idxposc and i_idxposb unless you know what you do 
 *****************************************************************************/

/* FIXME FIXME change b_pad to number of bytes to skipp after reading */
static int __AVI_GetDataInPES( input_thread_t *p_input,
                               pes_packet_t   **pp_pes,
                               int i_size,
                               int b_pad )
{

    int i_read;
    data_packet_t *p_data;

    
    if( !(*pp_pes = input_NewPES( p_input->p_method_data ) ) )
    {
        return( 0 );
    }

    
    if( !i_size )
    {
        p_data = input_NewPacket( p_input->p_method_data, 0 );
        (*pp_pes)->p_first = (*pp_pes)->p_last  = p_data;
        (*pp_pes)->i_nb_data = 1;
        (*pp_pes)->i_pes_size = 0;
        return( 0 );
    }
    
    if( ( i_size&1 )&&( b_pad ) )
    {
        b_pad = 1;
        i_size++;
    }
    else
    {
        b_pad = 0;
    }

    do
    {
        i_read = input_SplitBuffer(p_input, &p_data, __MIN( i_size - 
                                                              (*pp_pes)->i_pes_size, 1024 ) );
        if( i_read < 0 )
        {
            return( (*pp_pes)->i_pes_size );
        }
        if( !(*pp_pes)->p_first )
        {
            (*pp_pes)->p_first = 
                    (*pp_pes)->p_last  = p_data;
            (*pp_pes)->i_nb_data = 1;
            (*pp_pes)->i_pes_size = i_read;
        }
        else
        {
            (*pp_pes)->p_last->p_next = 
                    (*pp_pes)->p_last = p_data;
            (*pp_pes)->i_nb_data++;
            (*pp_pes)->i_pes_size += i_read;
        }
    } while( ((*pp_pes)->i_pes_size < i_size)&&( i_read ) );

    if( b_pad )
    {
        (*pp_pes)->i_pes_size--;
        (*pp_pes)->p_last->p_payload_end--;
        i_size--;
    }

	return( i_size );
}

static int __AVI_SeekAndGetChunk( input_thread_t  *p_input,
                                  AVIStreamInfo_t *p_info )
{
    pes_packet_t *p_pes;
    int i_length, i_ret;
    
    i_length = __MIN( p_info->p_index[p_info->i_idxposc].i_length 
                        - p_info->i_idxposb,
                            BUFFER_MAXSPESSIZE );

    AVI_SeekAbsolute( p_input, 
                      (off_t)p_info->p_index[p_info->i_idxposc].i_pos + 
                            p_info->i_idxposb + 8);

    i_ret = __AVI_GetDataInPES( p_input, &p_pes, i_length , 0);

    if( i_ret != i_length )
    {
        return( 0 );
    }
    /*  TODO test key frame if i_idxposb == 0*/
    AVI_PESBuffer_Add( p_input->p_method_data,
                       p_info,
                       p_pes,
                       p_info->i_idxposc,
                       p_info->i_idxposb );
    return( 1 );
}
/* TODO check if it's correct (humm...) and optimisation ... */
/* return 0 if we choose to get only the ck we want 
 *        1 if index is invalid
 *        2 if there is a ck_other before ck_info and the last proced ck_info*/
/* XXX XXX XXX avi file is some BIG shit, and sometime index give 
 * a refenrence to the same chunk BUT with a different size ( usually 0 )
 */

static inline int __AVI_GetChunkMethod( input_thread_t  *p_input,
                                 AVIStreamInfo_t *p_info,
                                 AVIStreamInfo_t *p_other )
{
    int i_info_pos;
    int i_other_pos;
    
    int i_info_pos_last;
    int i_other_pos_last;

    /*If we don't have a valid entry we need to parse from last 
        defined chunk and it's the only way that we return 1*/
    if( p_info->i_idxposc >= p_info->i_idxnb )
    {
        return( 1 );
    }

    /* KNOW we have a valid entry for p_info */
    /* we return 0 if we haven't an valid entry for p_other */ 
    if( ( !p_other )||( p_other->i_idxposc >= p_other->i_idxnb ) )
    {
        return( 0 );
    }

    /* KNOW there are 2 streams with valid entry */
   
    /* we return 0 if for one of the two streams we will not read 
       chunk-aligned */
    if( ( p_info->i_idxposb )||( p_other->i_idxposb ) )
    { 
        return( 0 );
    }
    
    /* KNOW we have a valid entry for the 2 streams
         and for the 2 we want an aligned chunk (given by i_idxposc )*/
        /* if in stream, the next chunk is back than the one we 
           have just read, it's useless to parse */
    i_info_pos  = p_info->p_index[p_info->i_idxposc].i_pos;
    i_other_pos = p_other->p_index[p_other->i_idxposc].i_pos ;

    i_info_pos_last  = p_info->i_idxposc ? 
                            p_info->p_index[p_info->i_idxposc - 1].i_pos : 0;
    i_other_pos_last = p_other->i_idxposc ?
                            p_other->p_index[p_other->i_idxposc - 1].i_pos : 0 ;
   
    
    if( ( ( p_info->i_idxposc )&&( i_info_pos <= i_info_pos_last ) ) ||
        ( ( p_other->i_idxposc )&&( i_other_pos <= i_other_pos_last ) ) )
    {
        return( 0 );
    }

    /* KNOW for the 2 streams, the ck we want are after the last read 
           or it's the first */

    /* if the first ck_other we want isn't between ck_info_last 
       and ck_info, don't parse */
    /* TODO fix this, use also number in buffered PES */
    if( ( i_other_pos > i_info_pos) /* ck_other too far */
        ||( i_other_pos < i_info_pos_last ) ) /* it's too late for ck_other */
    {
        return( 0 );
    } 
   
    /* we Know we will find ck_other, and before ck_info 
        "if ck_info is too far" will be handle after */
    return( 2 );
}

                         
static inline int __AVI_ChooseSize( int l1, int l2 )
{
    /* XXX l2 is prefered if 0 otherwise min not equal to 0 */
    if( !l2 )
    { 
        return( 0 );
    }
    return( !l1 ? l2 : __MIN( l1,l2 ) );
}

/* We know we will read chunk align */
static int __AVI_GetAndPutChunkInBuffer( input_thread_t  *p_input,
                                  AVIStreamInfo_t *p_info,
                                  int i_size,
                                  int i_ck )
{

    pes_packet_t    *p_pes;
    int i_length; 

    i_length = __MIN( i_size, BUFFER_MAXSPESSIZE );

    /* Skip chunk header */

    if( __AVI_GetDataInPES( p_input, &p_pes, i_length + 8,1 ) != i_length +8 )
    {
        return( 0 );
    }
    p_pes->p_first->p_payload_start += 8;
    p_pes->i_pes_size -= 8;

    i_size = GetDWLE( p_pes->p_first->p_demux_start + 4);

    AVI_PESBuffer_Add( p_input->p_method_data,
                       p_info,
                       p_pes,
                       i_ck,
                       0 );
    /* skip unwanted bytes */
    if( i_length != i_size)
    {
        msg_Err( p_input, "Chunk Size mismatch" );
        AVI_SeekAbsolute( p_input,
                          __EVEN( AVI_TellAbsolute( p_input ) + 
                                  i_size - i_length ) );
    }
    return( 1 );
}

/* XXX Don't use this function directly ! XXX */
static int __AVI_GetChunk( input_thread_t  *p_input,
                           AVIStreamInfo_t *p_info,
                           int b_load )
{
    demux_data_avi_file_t *p_avi_demux =
                        (demux_data_avi_file_t*)p_input->p_demux_data;
    AVIStreamInfo_t *p_other;
    int i_method;
    off_t i_posmax;
    int i;
   
#define p_info_i p_avi_demux->pp_info[i]
    while( p_info->p_pes_first )
    {
        if( ( p_info->p_pes_first->i_posc == p_info->i_idxposc ) 
             &&( p_info->i_idxposb >= p_info->p_pes_first->i_posb )
             &&( p_info->i_idxposb < p_info->p_pes_first->i_posb + 
                     p_info->p_pes_first->p_pes->i_pes_size ) )
  
        {
            return( 1 ); /* we have it in buffer */
        }
        else
        {
            AVI_PESBuffer_Drop( p_input->p_method_data, p_info );
        }
    }
    /* up to now we handle only one audio and video streams at the same time */
    p_other = (p_info == p_avi_demux->p_info_video ) ?
                     p_avi_demux->p_info_audio : p_avi_demux->p_info_video ;

    i_method = __AVI_GetChunkMethod( p_input, p_info, p_other );

    if( !i_method )
    {
        /* get directly the good chunk */
        return( b_load ? __AVI_SeekAndGetChunk( p_input, p_info ) : 1 );
    }
    /* We will parse
        * because invalid index
        * or will find ck_other before ck_info 
    */
//    msg_Warn( p_input, "method %d", i_method ); 
    /* we will calculate the better position we have to reach */
    if( i_method == 1 )
    {
        /* invalid index */
    /*  the position max we have already reached */
        /* FIXME this isn't the better because sometime will fail to
            put in buffer p_other since it could be too far */
        AVIStreamInfo_t *p_info_max = p_info;
        
        for( i = 0; i < p_avi_demux->i_streams; i++ )
        {
            if( p_info_i->i_idxnb )
            {
                if( p_info_max->i_idxnb )
                {
                    if( p_info_i->p_index[p_info_i->i_idxnb -1 ].i_pos >
                            p_info_max->p_index[p_info_max->i_idxnb -1 ].i_pos )
                    {
                        p_info_max = p_info_i;
                    }
                }
                else
                {
                    p_info_max = p_info_i;
                }
            }
        }
        if( p_info_max->i_idxnb )
        {
            /* be carefull that size between index and ck can sometime be 
              different without any error (and other time it's an error) */
           i_posmax = p_info_max->p_index[p_info_max->i_idxnb -1 ].i_pos; 
           /* so choose this, and I know that we have already reach it */
        }
        else
        {
            i_posmax = p_avi_demux->p_movi->i_pos + 12;
        }
    }
    else
    {
        if( !b_load )
        {
            return( 1 ); /* all is ok */
        }
        /* valid index */
        /* we know that the entry and the last one are valid for the 2 stream */
        /* and ck_other will come *before* index so go directly to it*/
        i_posmax = p_other->p_index[p_other->i_idxposc].i_pos;
    }

    AVI_SeekAbsolute( p_input, i_posmax );
    /* the first chunk we will see is :
            * the last chunk that we have already seen for broken index 
            * the first ck for other with good index */ 
    for( ; ; ) /* infinite parsing until the ck we want */
    {
        riffchunk_t  *p_ck;
        int i_type;
        
        /* Get the actual chunk in the stream */
        if( !(p_ck = RIFF_ReadChunk( p_input )) )
        {
            return( 0 );
        }
//        msg_Dbg( p_input, "ck: %4.4s len %d", &p_ck->i_id, p_ck->i_size );
        /* special case for LIST-rec chunk */
        if( ( p_ck->i_id == FOURCC_LIST )&&( p_ck->i_type == FOURCC_rec ) )
        {
            RIFF_DescendChunk( p_input );
            RIFF_DeleteChunk( p_input, p_ck );
            continue;
        }
        AVI_ParseStreamHeader( p_ck->i_id, &i, &i_type );
        /* littles checks but not too much if you want to read all file */ 
        if( i >= p_avi_demux->i_streams )
        /* (AVI_GetESTypeFromTwoCC(i_type) != p_info_i->i_cat) perhaps add it*/
            
        {
            RIFF_DeleteChunk( p_input, p_ck );
            if( RIFF_NextChunk( p_input, p_avi_demux->p_movi ) != 0 )
            {
                return( 0 );
            }
        }
        else
        {
            int i_size;

            /* have we found a new entry (not present in index)? */
            if( ( !p_info_i->i_idxnb )
                ||(p_info_i->p_index[p_info_i->i_idxnb-1].i_pos < p_ck->i_pos))
            {
                AVIIndexEntry_t index;

                index.i_id = p_ck->i_id;
                index.i_flags = AVI_GetKeyFlag( p_info_i->p_es->i_fourcc,
                                                (u8*)&p_ck->i_8bytes);
                index.i_pos = p_ck->i_pos;
                index.i_length = p_ck->i_size;
                __AVI_AddEntryIndex( p_info_i, &index );   
            }


            /* TODO check if p_other is full and then if is possible 
                go directly to the good chunk */
            if( ( p_info_i == p_other )
                &&( !AVI_PESBuffer_IsFull( p_other ) )
                &&( ( !p_other->p_pes_last )||
                    ( p_other->p_pes_last->p_pes->i_pes_size != 
                                                    BUFFER_MAXSPESSIZE ) ) )
            {
                int i_ck = p_other->p_pes_last ? 
                        p_other->p_pes_last->i_posc + 1 : p_other->i_idxposc;
                i_size = __AVI_ChooseSize( p_ck->i_size,
                                           p_other->p_index[i_ck].i_length);
               
                if( p_other->p_index[i_ck].i_pos == p_ck->i_pos )
                {
                    if( !__AVI_GetAndPutChunkInBuffer( p_input, p_other, 
                                                       i_size, i_ck ) )
                    {
                        RIFF_DeleteChunk( p_input, p_ck );
                        return( 0 );
                    }
                }
                else
                {
                    if( RIFF_NextChunk( p_input, p_avi_demux->p_movi ) != 0 )
                    {
                        RIFF_DeleteChunk( p_input, p_ck );

                        return( 0 );
                    }

                }
                        
                RIFF_DeleteChunk( p_input, p_ck );
            }
            else
            if( ( p_info_i == p_info)
                &&( p_info->i_idxposc < p_info->i_idxnb ) )
            {
                /* the first ck_info is ok otherwise it should be 
                        loaded without parsing */
                i_size = __AVI_ChooseSize( p_ck->i_size,
                                 p_info->p_index[p_info->i_idxposc].i_length);


                RIFF_DeleteChunk( p_input, p_ck );
                
                return( b_load ? __AVI_GetAndPutChunkInBuffer( p_input,
                                                               p_info,
                                                               i_size,
                                                     p_info->i_idxposc ) : 1 );
            }
            else
            {
                /* skip it */
                RIFF_DeleteChunk( p_input, p_ck );
                if( RIFF_NextChunk( p_input, p_avi_demux->p_movi ) != 0 )
                {
                    return( 0 );
                }
            }
        }

    
    }
    
#undef p_info_i
}

/* be sure that i_ck will be a valid index entry */
static int AVI_SetStreamChunk( input_thread_t    *p_input,
                               AVIStreamInfo_t   *p_info,
                               int   i_ck )
{

    p_info->i_idxposc = i_ck;
    p_info->i_idxposb = 0;

    if(  i_ck < p_info->i_idxnb )
    {
        return( 1 );
    }
    else
    {
        p_info->i_idxposc = p_info->i_idxnb - 1;
        do
        {
            p_info->i_idxposc++;
            if( !__AVI_GetChunk( p_input, p_info, 0 ) )
            {
                return( 0 );
            }
        } while( p_info->i_idxposc < i_ck );

        return( 1 );
    }
}


/* XXX FIXME up to now, we assume that all chunk are one after one */
static int AVI_SetStreamBytes( input_thread_t    *p_input, 
                               AVIStreamInfo_t   *p_info,
                               off_t   i_byte )
{
    if( ( p_info->i_idxnb > 0 )
        &&( i_byte < p_info->p_index[p_info->i_idxnb - 1].i_lengthtotal + 
                p_info->p_index[p_info->i_idxnb - 1].i_length ) )
    {
        /* index is valid to find the ck */
        /* uses dichototmie to be fast enougth */
        int i_idxposc = __MIN( p_info->i_idxposc, p_info->i_idxnb - 1 );
        int i_idxmax  = p_info->i_idxnb;
        int i_idxmin  = 0;
        for( ;; )
        {
            if( p_info->p_index[i_idxposc].i_lengthtotal > i_byte )
            {
                i_idxmax  = i_idxposc ;
                i_idxposc = ( i_idxmin + i_idxposc ) / 2 ;
            }
            else
            {
                if( p_info->p_index[i_idxposc].i_lengthtotal + 
                        p_info->p_index[i_idxposc].i_length <= i_byte)
                {
                    i_idxmin  = i_idxposc ;
                    i_idxposc = (i_idxmax + i_idxposc ) / 2 ;
                }
                else
                {
                    p_info->i_idxposc = i_idxposc;
                    p_info->i_idxposb = i_byte - 
                            p_info->p_index[i_idxposc].i_lengthtotal;
                    return( 1 );
                }
            }
        }
        
    }
    else
    {
        p_info->i_idxposc = p_info->i_idxnb - 1;
        p_info->i_idxposb = 0;
        do
        {
            p_info->i_idxposc++;
            if( !__AVI_GetChunk( p_input, p_info, 0 ) )
            {
                return( 0 );
            }
        } while( p_info->p_index[p_info->i_idxposc].i_lengthtotal +
                    p_info->p_index[p_info->i_idxposc].i_length <= i_byte );

        p_info->i_idxposb = i_byte -
                       p_info->p_index[p_info->i_idxposc].i_lengthtotal;
        return( 1 );
    }
}

static pes_packet_t *AVI_ReadStreamChunkInPES(  input_thread_t  *p_input,
                                                AVIStreamInfo_t *p_info )

{
    if( p_info->i_idxposc > p_info->i_idxnb )
    {
        return( NULL );
    }

    /* we want chunk (p_info->i_idxposc,0) */
    p_info->i_idxposb = 0;
    if( !__AVI_GetChunk( p_input, p_info, 1) )
    {
        msg_Err( p_input, "Got one chunk : failed" );
        return( NULL );
    }
    p_info->i_idxposc++;
    return( AVI_PESBuffer_Get( p_info ) );
}

static pes_packet_t *AVI_ReadStreamBytesInPES(  input_thread_t  *p_input,
                                                AVIStreamInfo_t *p_info,
                                                int i_byte )
{
    pes_packet_t    *p_pes;
    data_packet_t   *p_data;
    int             i_count = 0;
    int             i_read;

        
    if( !( p_pes = input_NewPES( p_input->p_method_data ) ) )
    {
        return( NULL );
    }
fprintf(stderr, "blah ibyte %i\n", i_byte);
    if( !( p_data = input_NewPacket( p_input->p_method_data, i_byte ) ) )
    {
        input_DeletePES( p_input->p_method_data, p_pes );
        return( NULL );
    }

    p_pes->p_first =
            p_pes->p_last = p_data;
    p_pes->i_nb_data = 1;
    p_pes->i_pes_size = i_byte;

    while( i_byte > 0 )
    {
        if( !__AVI_GetChunk( p_input, p_info, 1) )
        {
         msg_Err( p_input, "Got one chunk : failed" );
           
            input_DeletePES( p_input->p_method_data, p_pes );
            return( NULL );
        }

        i_read = __MIN( p_info->p_pes_first->p_pes->i_pes_size - 
                            ( p_info->i_idxposb - p_info->p_pes_first->i_posb ),
                        i_byte);
        /* FIXME FIXME FIXME follow all data packet */
        memcpy( p_data->p_payload_start + i_count, 
                p_info->p_pes_first->p_pes->p_first->p_payload_start + 
                    p_info->i_idxposb - p_info->p_pes_first->i_posb,
                i_read );

        AVI_PESBuffer_Drop( p_input->p_method_data, p_info );
        i_byte  -= i_read;
        i_count += i_read;

        p_info->i_idxposb += i_read;
        if( p_info->p_index[p_info->i_idxposc].i_length <= p_info->i_idxposb )
        {
            p_info->i_idxposb -= p_info->p_index[p_info->i_idxposc].i_length;
            p_info->i_idxposc++;
        }
    }
   return( p_pes );
}
        


/* try to realign after a seek */
static int AVI_ReAlign( input_thread_t *p_input,
                        AVIStreamInfo_t *p_info )
{
    int i;
    off_t   i_pos;
    int     b_after = 0;
    demux_data_avi_file_t *p_avi_demux =
                        (demux_data_avi_file_t*)p_input->p_demux_data;


    for( i = 0; i < p_avi_demux->i_streams; i++ )
    {
        AVI_PESBuffer_Flush( p_input->p_method_data, p_avi_demux->pp_info[i] );
    }
    /* Reinit clock
       TODO use input_ClockInit instead but need to be exported 
    p_input->stream.p_selected_program->last_cr = 0;
    p_input->stream.p_selected_program->last_syscr = 0;
    p_input->stream.p_selected_program->cr_ref = 0;
    p_input->stream.p_selected_program->sysdate_ref = 0;
    p_input->stream.p_selected_program->delta_cr = 0;
    p_input->stream.p_selected_program->c_average_count = 0; */
       
    i_pos = AVI_TellAbsolute( p_input );

    p_info->i_idxposc--; /* in fact  p_info->i_idxposc is for ck to be read */
    

    if( ( p_info->i_idxposc <= 0)
        ||( i_pos <= p_info->p_index[0].i_pos ) )
    {
        /*  before beginning of stream  */
        return( p_info->header.i_samplesize ?
                    AVI_SetStreamBytes( p_input, p_info, 0 ) :
                        AVI_SetStreamChunk( p_input, p_info, 0 ) );
    }
    
    b_after = ( i_pos >= p_info->p_index[p_info->i_idxposc].i_pos );
    /* now find in what chunk we are */
    while( ( i_pos < p_info->p_index[p_info->i_idxposc].i_pos )
           &&( p_info->i_idxposc > 0 ) )
    {
        /* search before i_idxposc */

        if( !AVI_SetStreamChunk( p_input, p_info, p_info->i_idxposc - 1 ) )
        {
            return( 0 );   
        }
    }
    
    while( i_pos >= p_info->p_index[p_info->i_idxposc].i_pos +
               p_info->p_index[p_info->i_idxposc].i_length + 8 )
    {
        /* search after i_idxposc */

        if( !AVI_SetStreamChunk( p_input, p_info, p_info->i_idxposc + 1 ) )
        {
            return( 0 );
        }
    }

    /* search nearest key frame, only for video */
    if( p_info->i_cat == VIDEO_ES )
    {
        if( b_after )
        {
            while(!(p_info->p_index[p_info->i_idxposc].i_flags&AVIIF_KEYFRAME) )
            {
                if( !AVI_SetStreamChunk( p_input, p_info, 
                                         p_info->i_idxposc + 1 ) )
                {
                    return( 0 );
                }
            }
        }
        else
        { 
            while( ( p_info->i_idxposc > 0 ) &&
              (!(p_info->p_index[p_info->i_idxposc].i_flags&AVIIF_KEYFRAME)) )
            {

                if( !AVI_SetStreamChunk( p_input, p_info, 
                                        p_info->i_idxposc - 1 ) )
                {

                    return( 0 );
                }
            }
        }
    } 
    return( 1 );
}

/* make difference between audio and video pts as little as possible */
static void AVI_SynchroReInit( input_thread_t *p_input )
{
    demux_data_avi_file_t *p_avi_demux =
                        (demux_data_avi_file_t*)p_input->p_demux_data;
    
#define p_info_video p_avi_demux->p_info_video
#define p_info_audio p_avi_demux->p_info_audio
    if( ( !p_info_audio )||( !p_info_video ) )
    {
        return;
    }
    /* now resynch audio video video */
    /*don't care of AVIF_KEYFRAME */
    if( !p_info_audio->header.i_samplesize )
    {
        AVI_SetStreamChunk( p_input, 
                            p_info_audio, 
                            AVI_PTSToChunk( p_info_audio,
                                            AVI_GetPTS( p_info_video ) ) );
    }
    else
    {
        AVI_SetStreamBytes( p_input,
                            p_info_audio,
                            AVI_PTSToByte( p_info_audio,
                                            AVI_GetPTS( p_info_video ) ) ); 
    }
#undef p_info_video
#undef p_info_audio
} 

/*****************************************************************************
 * AVI_GetFrameInPES : get dpts length(µs) in pes from stream
 *****************************************************************************
 * Handle multiple pes, and set pts to the good value 
 *****************************************************************************/
static pes_packet_t *AVI_GetFrameInPES( input_thread_t *p_input,
                                        AVIStreamInfo_t *p_info,
                                        mtime_t i_dpts)
{
    int i;
    pes_packet_t *p_pes = NULL;
    pes_packet_t *p_pes_tmp = NULL;
    pes_packet_t *p_pes_first = NULL;
    mtime_t i_pts;

    if( i_dpts < 1000 ) 
    { 
        return( NULL ) ; 
    }

    if( !p_info->header.i_samplesize )
    {
        int i_chunk = __MAX( AVI_PTSToChunk( p_info, i_dpts), 1 );
        p_pes_first = NULL;
        for( i = 0; i < i_chunk; i++ )
        {
            /* get pts while is valid */
            i_pts = AVI_GetPTS( p_info ); 
 
            p_pes_tmp = AVI_ReadStreamChunkInPES( p_input, p_info );

            if( !p_pes_tmp )
            {
                return( p_pes_first );
            }
            p_pes_tmp->i_pts = i_pts;
            if( !p_pes_first )
            {
                p_pes_first = p_pes_tmp;
            }
            else
            {
                p_pes->p_next = p_pes_tmp;
            }
            p_pes = p_pes_tmp;
        }
        return( p_pes_first );
    }
    else
    {
        /* stream is byte based */
        int i_byte = AVI_PTSToByte( p_info, i_dpts);
        if( i_byte < 50 ) /* to avoid some problem with audio */
        {
            return( NULL );
        }
        i_pts = AVI_GetPTS( p_info );  /* ok even with broken index */
        p_pes = AVI_ReadStreamBytesInPES( p_input, p_info, i_byte);

        if( p_pes )
        {
            p_pes->i_pts = i_pts;
        }
        return( p_pes );
    }
}
/*****************************************************************************
 * AVI_DecodePES : send a pes to decoder 
 *****************************************************************************
 * Handle multiple pes, and update pts to the good value 
 *****************************************************************************/
static inline void AVI_DecodePES( input_thread_t *p_input,
                                  AVIStreamInfo_t *p_info,
                                  pes_packet_t *p_pes )
{
    pes_packet_t    *p_pes_next;
    /* input_decode want only one pes, but AVI_GetFrameInPES give
          multiple pes so send one by one */
    while( p_pes )
    {
        p_pes_next = p_pes->p_next;
        p_pes->p_next = NULL;
        p_pes->i_pts = input_ClockGetTS( p_input, 
                                         p_input->stream.p_selected_program, 
                                         p_pes->i_pts * 9/100);
        input_DecodePES( p_info->p_es->p_decoder_fifo, p_pes );
        p_pes = p_pes_next;
    }
  
}

/*****************************************************************************
 * AVIDemux_Seekable: reads and demuxes data packets for stream seekable
 *****************************************************************************
 * Called by AVIDemux, that make common work
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int AVIDemux_Seekable( input_thread_t *p_input,
                              AVIStreamInfo_t *p_info_master,
                              AVIStreamInfo_t *p_info_slave )
{
    demux_data_avi_file_t *p_avi_demux = 
                (demux_data_avi_file_t*)p_input->p_demux_data;

    pes_packet_t *p_pes_master;
    pes_packet_t *p_pes_slave;

    /* check for signal from interface */
    if( p_input->stream.p_selected_program->i_synchro_state == SYNCHRO_REINIT )
    { 
        /* we can supposed that is a seek */
        /* first wait for empty buffer, arbitrary time */
        msleep( DEFAULT_PTS_DELAY );
        /* then try to realign in stream */
        if( !AVI_ReAlign( p_input, p_info_master ) )
        {
            return( 0 ); /* assume EOF */
        }
        AVI_SynchroReInit( p_input ); 
    }

    /* take care of newly selected audio ES */
    if( p_info_master->b_selected )
    {
        p_info_master->b_selected = 0;
        AVI_SynchroReInit( p_input ); 
    }
    if( ( p_info_slave )&&( p_info_slave->b_selected ) )
    {
        p_info_slave->b_selected = 0;
        AVI_SynchroReInit( p_input );
    }

    /* wait for the good time */
    input_ClockManageRef( p_input,
                          p_input->stream.p_selected_program,
                          p_avi_demux->i_pcr /*- DEFAULT_PTS_DELAY / 2 */); 
    /* calculate pcr, time when we must read the next data */
    /* 9/100 kludge ->need to convert to 1/1000000 clock unit to 1/90000 */
    if( p_info_slave )
    {
        p_avi_demux->i_pcr =  __MIN( AVI_GetPTS( p_info_master ),
                                     AVI_GetPTS( p_info_slave ) ) * 9/100;
    }
    else
    {
        p_avi_demux->i_pcr =  AVI_GetPTS( p_info_master ) * 9/100;
    }

    /* get video and audio frames */
    p_pes_master = AVI_GetFrameInPES( p_input,
                                      p_info_master,
                                      100000 ); /* 100 ms */
    AVI_DecodePES( p_input,
                   p_info_master,
                   p_pes_master);


    if( p_info_slave )
    {
        p_pes_slave = AVI_GetFrameInPES( p_input,
                                         p_info_slave,
                                         AVI_GetPTS( p_info_master ) -
                                             AVI_GetPTS( p_info_slave) );
        AVI_DecodePES( p_input,
                       p_info_slave,
                       p_pes_slave );
    }


    /* at the end ? */
    return( p_pes_master ? 1 : 0 );

}

/*****************************************************************************
 * AVIDemux_NotSeekable: reads and demuxes data packets for stream seekable
 *****************************************************************************
 * Called by AVIDemux, that makes common work
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/

/* 0 if can be load/updated, 1 if skip, 2 if descend into, 3 if exit, 4 if error and need recover */
static int __AVIDemux_ChunkAction( int i_streams_max,
                                   riffchunk_t *p_ck )
{
    int i_stream;
    int i_type;

    AVI_ParseStreamHeader( p_ck->i_id, &i_stream, &i_type );
    if( i_stream < i_streams_max )
    {
        return( 0 ); /* read and/or update stream info */
    }

    if( i_stream <= 99 )
    {
        /* should not happen but ... */
        return( 1 );
    }

    /* now we know that it's not a stream */

    switch( p_ck->i_id )
    {
        case( FOURCC_JUNK ):
            return( 1 );
        case( FOURCC_idx1 ):
            return( 3 );
        case( FOURCC_LIST ):
            if( p_ck->i_type == FOURCC_rec )
            {
                return( 2 );
            }
            else
            {
                return( 1 );
            }
        default:
            break;
    } 
    /* test for ix?? */

    if( ( p_ck->i_id & 0xFFFF ) == VLC_TWOCC( 'i','x' ) )
    {
        return( 1 );
    }

    return( 4 );
}

static int AVI_NotSeekableRecover( input_thread_t *p_input )
{
    byte_t *p_id;
    u32 i_id;
    int i_number, i_type;
    data_packet_t *p_pack;

    for( ; ; )
    {
        if( input_Peek( p_input, &p_id, 4 ) < 4 )
        {
            return( 0 ); /* Failed */
        }
        i_id = GetDWLE( p_id );
        switch( i_id )
        {
            case( FOURCC_idx1 ):
            case( FOURCC_JUNK ):
            case( FOURCC_LIST ):
                return( 1 );
            default:
                AVI_ParseStreamHeader( i_id, &i_number, &i_type );
                if( i_number <= 99 )
                {
                    switch( i_type )
                    {
                        case( TWOCC_wb ):
                        case( TWOCC_db ):
                        case( TWOCC_dc ):
                        case( TWOCC_pc ):
                            return( 1 );
                    }
                }
                else
                {

                }
        }
        /* Read 1 byte VERY unoptimised */
        if( input_SplitBuffer( p_input, &p_pack, 1) < 1 )
        {
            return( 0 );
        }
        input_DeletePacket( p_input->p_method_data, p_pack);
    }

}

static int AVIDemux_NotSeekable( input_thread_t *p_input,
                                 AVIStreamInfo_t *p_info_master,
                                 AVIStreamInfo_t *p_info_slave )
{
    demux_data_avi_file_t *p_avi_demux = 
                (demux_data_avi_file_t*)p_input->p_demux_data;
    int i_loop;
    int i_stream;
    int i_type;
    
    riffchunk_t *p_ck;
    pes_packet_t *p_pes;
   
/*
    i_filepos = AVI_TellAbsolute( p_input );
    p_input->pf_seek( p_input, i_filepos ); 
    input_AccessReinit( p_input );
*/
    
#define p_info p_avi_demux->pp_info[i_stream]

    /* The managment is very basic, we will read packets, caclulate pts 
    and send it to decoder, synchro made on video, and audio is very less
    important */
    
    /* wait the good time */
    input_ClockManageRef( p_input,
                          p_input->stream.p_selected_program,
                          p_avi_demux->i_pcr /*- DEFAULT_PTS_DELAY / 2 */); 
    /* TODO be smart, seeing if we can wait for min( audio, video )
        or there is a too big deep */
    if( !p_info_slave )
    {
        p_avi_demux->i_pcr =  AVI_GetPTS( p_info_master ) * 9/100;
    }
    else
    {
        p_avi_demux->i_pcr =  __MIN( AVI_GetPTS( p_info_master ),
                                 AVI_GetPTS( p_info_slave ) ) * 9/100;
        p_avi_demux->i_pcr =  AVI_GetPTS( p_info_master ) * 9/100;
    }
    
    for( i_loop = 0; i_loop < 10; i_loop++ )
    {
        int b_load =0;
        
        /* first find a ck for master or slave */
        do
        {

            if( !(p_ck = RIFF_ReadChunk( p_input ) ) )
            {
                msg_Err( p_input, "Badd" );
                return( 0 ); /* assume EOF */
            }
            //msg_Err( p_input,"Looking ck: %4.4s %d",&p_ck->i_id, p_ck->i_size );

            switch( __AVIDemux_ChunkAction( p_avi_demux->i_streams, p_ck ) )
            {
                case( 0 ): /* load it if possible */
                    b_load = 1;
                    break;
                case( 1 ): /* skip it */
                    RIFF_DeleteChunk( p_input, p_ck );
                    if( RIFF_NextChunk( p_input, p_avi_demux->p_movi ) != 0 )
                    {
                        return( 0 );
                    }
                    b_load = 0;
                    break;
                case( 2 ): /* descend into */
                    RIFF_DeleteChunk( p_input, p_ck );
                    RIFF_DescendChunk( p_input );
                    b_load = 0;
                    break;
                case( 3 ): /* exit */
                    RIFF_DeleteChunk( p_input, p_ck );
                    return( 0 );
                case( 4 ): /* Error */
                    RIFF_DeleteChunk( p_input, p_ck );
                    msg_Warn( p_input, "unknown chunk id 0x%8.8x, trying to recover", p_ck->i_id );
                    if( !AVI_NotSeekableRecover( p_input ) )
                    {
                        msg_Err( p_input, "cannot recover, dying" );
                        return( -1 );
                    }
                    else
                    {
                        msg_Warn( p_input, "recovered sucessfully" );
                    }
                    b_load = 0;
                    break;
            }

        } while( !b_load );

        AVI_ParseStreamHeader( p_ck->i_id, &i_stream, &i_type );
        /* now check if we really have to load it */
        if( ( p_info != p_info_master )&&( p_info != p_info_slave ) )
        {
            b_load = 0;
        }
        else
        {
            if( p_info == p_info_master )
            {
                b_load = 1;
            }
            else
            {
                mtime_t i_dpts;
                i_dpts = AVI_GetPTS( p_info_slave ) - 
                            AVI_GetPTS( p_info_master );
                if( i_dpts < 0 ) {i_dpts = - i_dpts; }
                if( i_dpts < 600000 )
                {
                    b_load = 1;
                } 
                else
                {
                    b_load = 0;
                }
            }

        }

        /* now do we can load this chunk ? */ 
        if( b_load )
        {

            if( __AVI_GetDataInPES( p_input, &p_pes, p_ck->i_size + 8, 1) != p_ck->i_size + 8)
            {
                return( 0 );
            }
            p_pes->p_first->p_payload_start += 8;
            p_pes->i_pes_size -= 8;
            /* get PTS */
            p_pes->i_pts = AVI_GetPTS( p_info );
            AVI_DecodePES( p_input, p_info, p_pes );
        }
        else
        {

            if( RIFF_NextChunk( p_input, p_avi_demux->p_movi ) != 0 )
            {
                RIFF_DeleteChunk( p_input, p_ck );
                return( 0 );
            }
        } 
        
        /* finaly update stream information */
        if( p_info->header.i_samplesize )
        {
            p_info->i_idxposb += p_ck->i_size;
        }
        else
        {
            p_info->i_idxposc++;
        }
        
        RIFF_DeleteChunk( p_input, p_ck );
    }

    return( 1 );
#undef p_info
}
/*****************************************************************************
 * AVIDemux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 * TODO add support for unstreable file, just read a chunk and send it 
 *      to the right decoder, very easy
 *****************************************************************************/

static int AVIDemux( input_thread_t *p_input )
{
    int i;
    AVIStreamInfo_t *p_info_master;
    AVIStreamInfo_t *p_info_slave;    

    demux_data_avi_file_t *p_avi_demux = 
                (demux_data_avi_file_t*)p_input->p_demux_data;

    /* search new video and audio stream selected 
          if current have been unselected*/
    if( ( !p_avi_demux->p_info_video )
            || ( !p_avi_demux->p_info_video->p_es->p_decoder_fifo ) )
    {
        p_avi_demux->p_info_video = NULL;
        for( i = 0; i < p_avi_demux->i_streams; i++ )
        {
            if( ( p_avi_demux->pp_info[i]->i_cat == VIDEO_ES )
                  &&( p_avi_demux->pp_info[i]->p_es->p_decoder_fifo ) )
            {
                p_avi_demux->p_info_video = p_avi_demux->pp_info[i];
                p_avi_demux->p_info_video->b_selected = 1;
                break;
            }
        }
    }
    if( ( !p_avi_demux->p_info_audio )
            ||( !p_avi_demux->p_info_audio->p_es->p_decoder_fifo ) )
    {
        p_avi_demux->p_info_audio = NULL;
        for( i = 0; i < p_avi_demux->i_streams; i++ )
        {
            if( ( p_avi_demux->pp_info[i]->i_cat == AUDIO_ES )
                  &&( p_avi_demux->pp_info[i]->p_es->p_decoder_fifo ) )
            {
                p_avi_demux->p_info_audio = p_avi_demux->pp_info[i];
                p_avi_demux->p_info_audio->b_selected = 1;
                break;
            }
        }
    }
    /* by default video is master for resync audio (after a seek .. ) */
    if( p_avi_demux->p_info_video )
    {
        p_info_master = p_avi_demux->p_info_video;
        p_info_slave  = p_avi_demux->p_info_audio;
    }
    else
    {
        p_info_master = p_avi_demux->p_info_audio;
        p_info_slave  = NULL;
    }
    
    if( !p_info_master ) 
    {
        msg_Err( p_input, "no stream selected" );
        return( -1 );
    }

    /* manage rate, if not default: skeep audio */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( p_input->stream.control.i_rate != p_avi_demux->i_rate )
    {
        if( p_avi_demux->p_info_audio)
        {
             p_avi_demux->p_info_audio->b_selected = 1;
        }
        p_avi_demux->i_rate = p_input->stream.control.i_rate;
    }
    vlc_mutex_unlock( &p_input->stream.stream_lock );    
    p_avi_demux->i_rate = DEFAULT_RATE;
    if( p_avi_demux->i_rate != DEFAULT_RATE )
    {
        p_info_slave = NULL;
    }

    if( p_avi_demux->b_seekable )
    {
        return( AVIDemux_Seekable( p_input,
                                   p_info_master,
                                   p_info_slave) );
    }
    else
    {
        return( AVIDemux_NotSeekable( p_input,
                                      p_info_master,
                                      p_info_slave ) );
    }
}



