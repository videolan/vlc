/*****************************************************************************
 * avi.c : AVI file Stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: avi.c,v 1.24 2002/06/27 19:05:17 sam Exp $
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
static void input_getfunctions( function_list_t * );
static int  AVIDemux         ( input_thread_t * );
static int  AVIInit          ( input_thread_t * );
static void AVIEnd           ( input_thread_t * );

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( "RIFF-AVI Stream input" )
    ADD_CAPABILITY( DEMUX, 150 )
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
    input.pf_init             = AVIInit;
    input.pf_end              = AVIEnd;
    input.pf_demux            = AVIDemux;
    input.pf_rewind           = NULL;
#undef input
}


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



static inline int __AVI_GetESTypeFromTwoCC( u16 i_type )
{
    switch( i_type )
    {
        case( TWOCC_wb ):
            return( AUDIO_ES );
         case( TWOCC_dc ):
         case( TWOCC_db ):
            return( VIDEO_ES );
    }
    return( UNKNOWN_ES );
}

static int AVI_AudioGetType( u32 i_type )
{
    switch( i_type )
    {
/*        case( WAVE_FORMAT_PCM ):
            return( WAVE_AUDIO_ES ); */
        case( WAVE_FORMAT_AC3 ):
            return( AC3_AUDIO_ES );
        case( WAVE_FORMAT_MPEG):
        case( WAVE_FORMAT_MPEGLAYER3):
            return( MPEG2_AUDIO_ES ); /* 2 for mpeg-2 layer 1 2 ou 3 */
        default:
            return( 0 );
    }
}
static int AVI_VideoGetType( u32 i_type )
{
    switch( i_type )
    {
        case( FOURCC_DIV1 ): /* FIXME it is for msmpeg4v1 or old mpeg4 ?? */
        case( FOURCC_div1 ):
        case( FOURCC_MPG4 ):
        case( FOURCC_mpg4 ):
            return( MSMPEG4v1_VIDEO_ES );

        case( FOURCC_DIV2 ):
        case( FOURCC_div2 ):
        case( FOURCC_MP42 ):
        case( FOURCC_mp42 ):
            return( MSMPEG4v2_VIDEO_ES );
         
        case( FOURCC_MPG3 ):
        case( FOURCC_mpg3 ):
        case( FOURCC_div3 ):
        case( FOURCC_MP43 ):
        case( FOURCC_mp43 ):
        case( FOURCC_DIV3 ):
        case( FOURCC_DIV4 ):
        case( FOURCC_div4 ):
        case( FOURCC_DIV5 ):
        case( FOURCC_div5 ):
        case( FOURCC_DIV6 ):
        case( FOURCC_div6 ):
        case( FOURCC_AP41 ):
        case( FOURCC_3IV1 ):
            return( MSMPEG4v3_VIDEO_ES );


        case( FOURCC_DIVX ):
        case( FOURCC_divx ):
        case( FOURCC_MP4S ):
        case( FOURCC_mp4s ):
        case( FOURCC_M4S2 ):
        case( FOURCC_m4s2 ):
        case( FOURCC_xvid ):
        case( FOURCC_XVID ):
        case( FOURCC_XviD ):
        case( FOURCC_DX50 ):
        case( FOURCC_mp4v ):
        case( FOURCC_4    ):
            return( MPEG4_VIDEO_ES );

        default:
            return( 0 );
    }
}
/*****************************************************************************
 * Data and functions to manipulate pes buffer
 *****************************************************************************/
#define BUFFER_MAXTOTALSIZE   512*1024 /* 1/2 Mo */
#define BUFFER_MAXSPESSIZE 1024*200
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









static void __AVIFreeDemuxData( input_thread_t *p_input )
{
    int i;
    demux_data_avi_file_t *p_avi_demux;
    p_avi_demux = (demux_data_avi_file_t*)p_input->p_demux_data  ; 
    
    if( p_avi_demux->p_riff != NULL ) 
            RIFF_DeleteChunk( p_input, p_avi_demux->p_riff );
    if( p_avi_demux->p_hdrl != NULL ) 
            RIFF_DeleteChunk( p_input, p_avi_demux->p_hdrl );
    if( p_avi_demux->p_movi != NULL ) 
            RIFF_DeleteChunk( p_input, p_avi_demux->p_movi );
    if( p_avi_demux->p_idx1 != NULL ) 
            RIFF_DeleteChunk( p_input, p_avi_demux->p_idx1 );
    if( p_avi_demux->pp_info != NULL )
    {
        for( i = 0; i < p_avi_demux->i_streams; i++ )
        {
            if( p_avi_demux->pp_info[i] != NULL ) 
            {
                if( p_avi_demux->pp_info[i]->p_index != NULL )
                {
                      free( p_avi_demux->pp_info[i]->p_index );
                      AVI_PESBuffer_Flush( p_input->p_method_data, p_avi_demux->pp_info[i] );
                }
                free( p_avi_demux->pp_info[i] ); 
            }
        }
         free( p_avi_demux->pp_info );
    }
}

static void AVI_ParseStreamHeader( u32 i_id, int *i_number, int *i_type )
{
    int c1,c2,c3,c4;

    c1 = ( i_id ) & 0xFF;
    c2 = ( i_id >>  8 ) & 0xFF;
    c3 = ( i_id >> 16 ) & 0xFF;
    c4 = ( i_id >> 24 ) & 0xFF;

    if( c1 < '0' || c1 > '9' || c2 < '0' || c2 > '9' )
    {
        *i_number = 100; /* > max stream number */
        *i_type = 0;
    }
    else
    {
        *i_number = (c1 - '0') * 10 + (c2 - '0' );
        *i_type = ( c4 << 8) + c3;
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
            index.i_flags   = GetDWLE( p_peek+4);
            index.i_pos     = GetDWLE( p_peek+8);
            index.i_length  = GetDWLE(p_peek+12);
            AVI_ParseStreamHeader( index.i_id, &i_number, &i_type );
            
            if( ( i_number <  p_avi_demux->i_streams )
               &&(p_avi_demux->pp_info[i_number]->i_cat == 
                     __AVI_GetESTypeFromTwoCC( i_type ))) 
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
        * test in the ile if it's true, if not do a RIFF_Find...
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


#if 0
FILE *DumpAudio;
#endif

/*****************************************************************************
 * AVIInit: check file and initializes AVI structures
 *****************************************************************************/
static int AVIInit( input_thread_t *p_input )
{
    riffchunk_t *p_riff,*p_hdrl,*p_movi;
    riffchunk_t *p_avih;
    riffchunk_t *p_strl,*p_strh,*p_strf;
    demux_data_avi_file_t *p_avi_demux;
    es_descriptor_t *p_es = NULL; /* for not warning */

    int i;

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
        __AVIFreeDemuxData( p_input );
        msg_Warn( p_input, "RIFF-AVI module discarded" );
        return( -1 );
    }
    p_avi_demux->p_riff = p_riff;

    if ( RIFF_DescendChunk(p_input) != 0 )
    {
        __AVIFreeDemuxData( p_input );
        msg_Err( p_input, "cannot look for subchunk" );
        return ( -1 );
    }

    /* it's a riff-avi file, so search for LIST-hdrl */
    if( RIFF_FindListChunk(p_input ,&p_hdrl,p_riff, FOURCC_hdrl) != 0 )
    {
        __AVIFreeDemuxData( p_input );
        msg_Err( p_input, "cannot find \"LIST-hdrl\"" );
        return( -1 );
    }
    p_avi_demux->p_hdrl = p_hdrl;

    if( RIFF_DescendChunk(p_input) != 0 )
    {
        __AVIFreeDemuxData( p_input );
        msg_Err( p_input, "cannot look for subchunk" );
        return ( -1 );
    }
    /* in  LIST-hdrl search avih */
    if( RIFF_FindAndLoadChunk( p_input, p_hdrl, 
                                    &p_avih, FOURCC_avih ) != 0 )
    {
        __AVIFreeDemuxData( p_input );
        msg_Err( p_input, "cannot find \"avih\" chunk" );
        return( -1 );
    }
    AVI_Parse_avih( &p_avi_demux->avih, p_avih->p_data->p_payload_start );
    RIFF_DeleteChunk( p_input, p_avih );
    
    if( p_avi_demux->avih.i_streams == 0 )  
    /* no stream found, perhaps it would be cool to find it */
    {
        __AVIFreeDemuxData( p_input );
        msg_Err( p_input, "no stream defined!" );
        return( -1 );
    }

    /*  create one program */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( input_InitStream( p_input, 0 ) == -1)
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        __AVIFreeDemuxData( p_input );
        msg_Err( p_input, "cannot init stream" );
        return( -1 );
    }
    if( input_AddProgram( p_input, 0, 0) == NULL )
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        __AVIFreeDemuxData( p_input );
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
    memset( p_avi_demux->pp_info, 0, 
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
            __AVIFreeDemuxData( p_input );
            msg_Err( p_input, "cannot find \"LIST-strl\"" );
            return( -1 );
        }
        
        /* in  LIST-strl search strh */
        if( RIFF_FindAndLoadChunk( p_input, p_hdrl, 
                                &p_strh, FOURCC_strh ) != 0 )
        {
            RIFF_DeleteChunk( p_input, p_strl );
            __AVIFreeDemuxData( p_input );
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
            __AVIFreeDemuxData( p_input );
            msg_Err( p_input, "cannot find \"strf\"" );
            return( -1 );
        }
        /* we don't get strd, it's useless for divx,opendivx,mepgaudio */ 
        if( RIFF_AscendChunk(p_input, p_strl) != 0 )
        {
            RIFF_DeleteChunk( p_input, p_strf );
            RIFF_DeleteChunk( p_input, p_strl );
            __AVIFreeDemuxData( p_input );
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
                p_es->b_audio = 1;
                p_es->i_type = 
                    AVI_AudioGetType( p_info->audio_format.i_formattag );
                if( !p_es->i_type )
                {
                    msg_Err( p_input, "stream(%d,0x%x) not supported", i,
                                       p_info->audio_format.i_formattag );
                    p_es->i_cat = UNKNOWN_ES;
                }
                break;
                
            case( FOURCC_vids ):
                p_es->i_cat = VIDEO_ES;
                AVI_Parse_BitMapInfoHeader( &p_info->video_format,
                                   p_strf->p_data->p_payload_start ); 
                p_es->b_audio = 0;
                p_es->i_type = 
                    AVI_VideoGetType( p_info->video_format.i_compression );
                if( !p_es->i_type )
                {
                    msg_Err( p_input, "stream(%d,%4.4s) not supported", i,
                              (char*)&p_info->video_format.i_compression);
                    p_es->i_cat = UNKNOWN_ES;
                }
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
        __AVIFreeDemuxData( p_input );
        msg_Err( p_input, "cannot go out (\"hdrl\")" );
        return( -1 );
    }

    /* go to movi chunk to get it*/
    if( RIFF_FindListChunk(p_input ,&p_movi,p_riff, FOURCC_movi) != 0 )
    {
        msg_Err( p_input, "cannot find \"LIST-movi\"" );
        __AVIFreeDemuxData( p_input );
        return( -1 );
    }
    p_avi_demux->p_movi = p_movi;
    
    /* get index  XXX need to have p_movi */
    if( ( p_avi_demux->b_seekable )
        &&( p_avi_demux->avih.i_flags&AVIF_HASINDEX ) ) 
    {
        /* get index */
        __AVI_GetIndex( p_input ); 
        /* try to get i_idxoffset for each stream  */
        __AVI_UpdateIndexOffset( p_input );
        /* to make sure to go the begining because unless demux will see a seek */
        RIFF_GoToChunk( p_input, p_avi_demux->p_movi );
        if( RIFF_DescendChunk( p_input ) != 0 )
        {
            __AVIFreeDemuxData( p_input );
            msg_Err( p_input, "cannot go in (\"movi\")" );
            return( -1 );
        }
    }
    else
    {
        msg_Warn( p_input, "no index!" );
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
                    /* TODO add test to see if a decoder has been foud */
                    vlc_mutex_lock( &p_input->stream.stream_lock );
                    input_SelectES( p_input, p_info->p_es );
                    vlc_mutex_unlock( &p_input->stream.stream_lock );
                }
                break;

            case( AUDIO_ES ):
                msg_Dbg( p_input, "audio(0x%x) %d channels %dHz %dbits %d bytes",
                         p_info->audio_format.i_formattag,
                         p_info->audio_format.i_channels,
                         p_info->audio_format.i_samplespersec,
                         p_info->audio_format.i_bitspersample,
                         p_info->header.i_samplesize );
                if( (p_avi_demux->p_info_audio == NULL) ) 
                {
                    p_avi_demux->p_info_audio = p_info;
                    vlc_mutex_lock( &p_input->stream.stream_lock );
                    input_SelectES( p_input, p_info->p_es );
                    vlc_mutex_unlock( &p_input->stream.stream_lock );
                }
                break;
            case( UNKNOWN_ES ):
                msg_Warn( p_input, "unhandled stream %d", i );
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
#if 0
    DumpAudio = fopen( "tmp.mp3", "w+" );
#endif

    return( 0 );
}


/*****************************************************************************
 * AVIEnd: frees unused data
 *****************************************************************************/
static void AVIEnd( input_thread_t *p_input )
{   
#if 0
    fclose( DumpAudio );
#endif
    __AVIFreeDemuxData( p_input ); 
    return;
}



/*****************************************************************************
 * Function to convert pts to chunk or byte
 *****************************************************************************/

static inline mtime_t AVI_PTSToChunk( AVIStreamInfo_t *p_info, 
                                        mtime_t i_pts )
{
    return( (mtime_t)((double)i_pts *
                      (double)p_info->header.i_rate /
                      (double)p_info->header.i_scale /
                      (double)1000000.0 ) );
}
static inline mtime_t AVI_PTSToByte( AVIStreamInfo_t *p_info,
                                       mtime_t i_pts )
{
    return( (mtime_t)((double)i_pts * 
                      (double)p_info->header.i_samplesize *
                      (double)p_info->header.i_rate /
                      (double)p_info->header.i_scale /
                      (double)1000000.0 ) );

}
static mtime_t AVI_GetPTS( AVIStreamInfo_t *p_info )
{
    /* p_info->p_index[p_info->i_idxposc] need to be valid !! */
    /* be careful to  *1000000 before round  ! */
    if( p_info->header.i_samplesize != 0 )
    {
        return( (mtime_t)( (double)1000000.0 *
                   (double)(p_info->p_index[p_info->i_idxposc].i_lengthtotal +
                             p_info->i_idxposb )*
                    (double)p_info->header.i_scale /
                    (double)p_info->header.i_rate /
                    (double)p_info->header.i_samplesize ) );
    }
    else
    {
        return( (mtime_t)( (double)1000000.0 *
                    (double)(p_info->i_idxposc ) *
                    (double)p_info->header.i_scale /
                    (double)p_info->header.i_rate) );
    }
}


/*****************************************************************************
 * Functions to acces streams data 
 * Uses it, because i plane to read unseekable stream
 * Don't work for the moment for unseekable stream 
 * XXX NEVER set directly i_idxposc and i_idxposb
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

    if( ( i_size&1 )&&( b_pad ) )
    {
        b_pad = 1;
        i_size++;
    }
    else
    {
        b_pad = 0;
    }
    
    if( !i_size )
    {
        (*pp_pes)->p_first = (*pp_pes)->p_last  = NULL;
        (*pp_pes)->i_nb_data = 0;
        (*pp_pes)->i_pes_size = 0;
        return( 0 );
    }

    do
    {
        i_read = input_SplitBuffer(p_input, &p_data, i_size - 
                                                    (*pp_pes)->i_pes_size );
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
 
/* XXX FIXME up to now, we assume that all chunk are one after one */
/* XXX Don't use this function directly ! XXX */
                         
static int __AVI_GetChunk( input_thread_t  *p_input,
                           AVIStreamInfo_t *p_info,
                           int b_load )
{
    demux_data_avi_file_t *p_avi_demux =
                        (demux_data_avi_file_t*)p_input->p_demux_data;
    AVIStreamInfo_t *p_other;
    int i_other_ck; /* dernier ck lu pour p_other */
    int b_parse;
    
#define p_video p_avi_demux->p_info_video
#define p_audio p_avi_demux->p_info_audio
#define p_info_i p_avi_demux->pp_info[i]
    while( p_info->p_pes_first )
    {
        if( ( p_info->p_pes_first->i_posc == p_info->i_idxposc ) 
/*            &&( p_info->i_idxposb == p_info->p_pes_first->i_posb ) ) */
             &&( p_info->i_idxposb >= p_info->p_pes_first->i_posb )
             &&( p_info->i_idxposb < p_info->p_pes_first->i_posb + p_info->p_pes_first->p_pes->i_pes_size ) )
  
        {
            return( 1 );
        }
        else
        {
            AVI_PESBuffer_Drop( p_input->p_method_data, p_info );
        }
    }
    
    /* up to now we handle only one audio and one video stream at the same time */
    p_other = (p_info == p_video ) ? p_audio : p_video ;
    if( p_other )
    {
        i_other_ck = p_other->p_pes_last ? p_other->p_pes_last->i_posc : p_other->i_idxposc - 1;
    }
    else
    {
        i_other_ck  = -1;
    }
    /* XXX -1 --> aucun lu */
    
    if( p_info->i_idxposc >= p_info->i_idxnb )
    {
        /* invalid index for p_info -> read one after one, load all ck
            for p_other until the one for p_info */
        b_parse = 1;
    }
    else
    if( p_info->i_idxposb )
    {
        b_parse = 0;
    }
    else
    if( !p_other )
    {
        b_parse = 0;
    }
    else
    if( ( i_other_ck +1 >= p_other->i_idxnb )||( i_other_ck == -1 )||( p_info->i_idxposc == 0) ) 
        /* XXX see if i_other_ck +1 >= p_other->i_idxnb if it is necessary */
    {
        b_parse = 1;
    }
    else
    {
        /* Avoid : * read an already read ck
                   * create a discontinuity */
        if( p_info->p_index[p_info->i_idxposc].i_pos < p_other->p_index[i_other_ck].i_pos )
        {
            b_parse = 0;
        }
        else
        {
            if( p_info->p_index[p_info->i_idxposc-1].i_pos > p_other->p_index[i_other_ck + 1].i_pos )
            {
                b_parse = 0;
            }
            else
            {
                b_parse = 1;
            }
        }

    }

    /* XXX XXX just for test */
//     b_parse = 0;
    /* XXX XXX */

    if( !b_parse )
    {
        pes_packet_t *p_pes;
        int i_length;
//        msg_Warn( p_input, "parsing 0" );   
        i_length = __MIN( p_info->p_index[p_info->i_idxposc].i_length - p_info->i_idxposb,
                              BUFFER_MAXSPESSIZE );

        AVI_SeekAbsolute( p_input, 
                          (off_t)p_info->p_index[p_info->i_idxposc].i_pos + 
                                p_info->i_idxposb + 8);
        /* FIXME lit aligné donc risque de lire un car de trop si position non aligné */
        if( !b_load )
        {
            return( 1 );
        }
        
        if( __AVI_GetDataInPES( p_input, 
                                &p_pes, 
                                i_length , 
                                0) != i_length )
        {
            msg_Err( p_input, "%d ERROR", p_info->i_cat );
            return( 0 );
        }
        AVI_PESBuffer_Add( p_input->p_method_data,
                           p_info,
                           p_pes,
                           p_info->i_idxposc,
                           p_info->i_idxposb );
        return( 1 );
    }
    else
    {
        int i;
        off_t i_posmax;

//        msg_Warn( p_input, "parsing 1" );   
        if( p_info->i_idxposc - 1 >= 0 )
        {
            i_posmax = p_info->p_index[p_info->i_idxposc - 1].i_pos +
                         __EVEN( p_info->p_index[p_info->i_idxposc - 1].i_length ) + 8;
        }
        else
        {
            i_posmax = p_avi_demux->p_movi->i_pos + 12;
        }
        if( i_other_ck >= 0 )
        {
            i_posmax = __MAX( i_posmax,
                              p_other->p_index[i_other_ck].i_pos + 
                                __EVEN( p_other->p_index[i_other_ck].i_length ) + 8 );
        }
        AVI_SeekAbsolute( p_input, i_posmax );

        for( ;; )
        {
            riffchunk_t  *p_ck;
            int i_ck;
            int i_type;
    
            if( !(p_ck = RIFF_ReadChunk( p_input )) )
            {
                return( 0 );
            }

            if( p_ck->i_id == FOURCC_LIST )
            {
                if( p_ck->i_type == FOURCC_rec )
                {
                    RIFF_DescendChunk( p_input );
                    continue;
                }
            }

            AVI_ParseStreamHeader( p_ck->i_id, &i, &i_type );
//            msg_Dbg( p_input, "ck: %4.4s", &p_ck->i_id );
            if( ( i >= p_avi_demux->i_streams )
                ||(__AVI_GetESTypeFromTwoCC( i_type ) != p_info_i->i_cat ) )
            {
                if( RIFF_NextChunk( p_input, p_avi_demux->p_movi ) != 0 )
                {
                    return( 0 );
                }
            }
            else
            {
                if( ( !p_info_i->i_idxnb )
                    ||( p_info_i->p_index[p_info_i->i_idxnb-1].i_pos +
                            __EVEN( p_info_i->p_index[p_info_i->i_idxnb-1].i_length ) + 8 <=
                                p_ck->i_pos ) )
                {
                    /* add a new entry */
                    AVIIndexEntry_t index;

                    index.i_id = p_ck->i_id;
                    index.i_flags = AVIIF_KEYFRAME;
                    index.i_pos = p_ck->i_pos;
                    index.i_length = p_ck->i_size;
                    __AVI_AddEntryIndex( p_info_i, &index );
                }
                RIFF_DeleteChunk( p_input, p_ck );

                /* load the packet */
                if( p_info_i == p_info )
                {
                    /* special case with broken index */
                    i_ck = p_info->i_idxposc > p_info->i_idxnb - 1 ? p_info->i_idxnb - 1 : p_info->i_idxposc;
                }
                else
                {
                    i_ck = p_info_i->p_pes_last ? p_info_i->p_pes_last->i_posc + 1 : p_info_i->i_idxposc;
                }
                /* TODO check if buffer overflow and if so seek to get the good ck if possible */
                /*       check if last ck was troncated */
                if( ( b_load )
                    &&( ( p_info_i == p_audio )||( p_info_i == p_video ) ) 
                    &&( ( !p_info_i->p_pes_last )
                        ||( p_info_i->p_pes_last->p_pes->i_pes_size != BUFFER_MAXSPESSIZE ) ) )
                {
                    pes_packet_t    *p_pes;
                    int i_offset = ( ( p_info_i == p_info )&&( i_ck == p_info->i_idxposc ) ) ?
                                        p_info->i_idxposb : 0;
                    int i_length = __MIN( p_info_i->p_index[i_ck].i_length - i_offset,
                                          BUFFER_MAXSPESSIZE );

                    AVI_SeekAbsolute( p_input, 
                                      (off_t)p_info_i->p_index[i_ck].i_pos + 
                                            i_offset + 8);
                    if( __AVI_GetDataInPES( p_input, &p_pes, i_length,1 ) != i_length)
                    {
                        return( 0 );
                    }
                    AVI_PESBuffer_Add( p_input->p_method_data,
                                       p_info_i,
                                       p_pes,
                                       i_ck,
                                       i_offset );
                    if( ( p_info_i == p_info )
                        &&( p_info->i_idxposc == i_ck ) )
                    {
                        return( 1 );
                    }
                }
                else
                {
                    RIFF_NextChunk( p_input, p_avi_demux->p_movi );
                    if( ( p_info_i == p_info )
                        &&( p_info->i_idxposc == i_ck ) )
                    {
                        return( 1 );
                    }
                }
            }
        }
    }

#undef p_video
#undef p_audio
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
        return( __AVI_GetChunk( p_input, p_info, 0) );
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
#if 0
    fwrite( p_data->p_payload_start, 1, i_count, DumpAudio );
#endif
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
        AVI_SetStreamChunk( p_input, p_info, p_info->i_idxposc - 1 );
    }
    
    while( i_pos >= p_info->p_index[p_info->i_idxposc].i_pos +
               p_info->p_index[p_info->i_idxposc].i_length + 8 )
    {
        /* search after i_idxposc */
        AVI_SetStreamChunk( p_input, p_info, p_info->i_idxposc + 1 );
    }

    /* search nearest key frame, only for video */
    if( p_info->i_cat == VIDEO_ES )
    {
        if( b_after )
        {
            while(!(p_info->p_index[p_info->i_idxposc].i_flags&AVIIF_KEYFRAME) )
            {
                AVI_SetStreamChunk( p_input, p_info, p_info->i_idxposc + 1 );
            }
        }
        else
        { 
            while( ( p_info->i_idxposc > 0 ) &&
              (!(p_info->p_index[p_info->i_idxposc].i_flags&AVIIF_KEYFRAME)) )
            {
                AVI_SetStreamChunk( p_input, p_info, p_info->i_idxposc - 1 );
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
    if( ( p_info_video )&&( p_info_audio ) )
    {
        /* now resynch audio video video */
        /*don't care of AVIF_KEYFRAME */
        if( !p_info_audio->header.i_samplesize )
        {
            int i_chunk = AVI_PTSToChunk( p_info_audio, 
                                            AVI_GetPTS( p_info_video ));

            AVI_SetStreamChunk( p_input, 
                                p_info_audio, 
                                i_chunk );
        }
        else
        {
            int i_byte = AVI_PTSToByte( p_info_audio, 
                                          AVI_GetPTS( p_info_video ) ) ;
            AVI_SetStreamBytes( p_input,
                                 p_info_audio,
                                 i_byte );
        }
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
            i_pts = AVI_GetPTS( p_info ); /* FIXME will segfault with bad index */
 
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
        i_pts = AVI_GetPTS( p_info ); /* FIXME will segfault with bad index */
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
 * Handle multiple pes, and set pts to the good value 
 *****************************************************************************/
static inline void AVI_DecodePES( input_thread_t *p_input,
                                  AVIStreamInfo_t *p_info,
                                  pes_packet_t *p_pes )
{
    pes_packet_t    *p_pes_next;
    /* input_decode want only one pes, but AVI_GetFrameInPES give
          multiple pes so send one by one */
    /* we now that p_info != NULL */
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
 * AVIDemux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int AVIDemux( input_thread_t *p_input )
{
    int i;
    pes_packet_t *p_pes;
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
                  &&( p_avi_demux->pp_info[i]->p_es->p_decoder_fifo != NULL ) )
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
                  &&( p_avi_demux->pp_info[i]->p_es->p_decoder_fifo != NULL ) )
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
    if( p_avi_demux->i_rate != DEFAULT_RATE )
    {
        p_info_slave = NULL;
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
                          p_avi_demux->i_pcr ); 
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
    p_pes = AVI_GetFrameInPES( p_input,
                               p_info_master,
                               100000 ); /* 100 ms */


    if( p_info_slave )
    {
        pes_packet_t *p_pes_slave;
        p_pes_slave = AVI_GetFrameInPES( p_input,
                                         p_info_slave,
                                         AVI_GetPTS( p_info_master ) -
                                             AVI_GetPTS( p_info_slave) );
        AVI_DecodePES( p_input,
                       p_info_slave,
                       p_pes_slave );
    }

    AVI_DecodePES( p_input,
                   p_info_master,
                   p_pes);

    /* at the end ? */
    return( p_pes ? 1 : 0 );

/*    return( p_info_master->i_idxposc > p_info_master->i_idxnb ? 0 : 1 );*/
}

