/*****************************************************************************
 * avi.c : AVI file Stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: avi.c,v 1.4 2002/04/25 21:52:42 sam Exp $
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

#include <videolan/vlc.h>

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"
#include "input_ext-plugins.h"

#include "video.h"

/*****************************************************************************
 * Constants
 *****************************************************************************/

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void input_getfunctions( function_list_t * p_function_list );
static int  AVIDemux         ( struct input_thread_s * );
static int  AVIInit          ( struct input_thread_s * );
static void AVIEnd           ( struct input_thread_s * );

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( "RIFF-AVI Stream input" )
    ADD_CAPABILITY( DEMUX, 150 )
    ADD_SHORTCUT( "avi" )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    input_getfunctions( &p_module->p_functions->demux );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/*****************************************************************************
 * Definition of structures and libraries for this plugins 
 *****************************************************************************/
#include "libLE.c"
#include "libioRIFF.c"
#include "avi.h"

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

/********************************************************************/


static void __AVIFreeDemuxData( input_thread_t *p_input )
{
    int i;
    demux_data_avi_file_t *p_avi_demux;
    p_avi_demux = (demux_data_avi_file_t*)p_input->p_demux_data  ; 
    
    if( p_avi_demux->p_riff != NULL ) 
            RIFF_DeleteChunk( p_input->p_demux_data, p_avi_demux->p_riff );
    if( p_avi_demux->p_hdrl != NULL ) 
            RIFF_DeleteChunk( p_input->p_demux_data, p_avi_demux->p_hdrl );
    if( p_avi_demux->p_movi != NULL ) 
            RIFF_DeleteChunk( p_input->p_demux_data, p_avi_demux->p_movi );
    if( p_avi_demux->p_idx1 != NULL ) 
            RIFF_DeleteChunk( p_input->p_demux_data, p_avi_demux->p_idx1 );
    if( p_avi_demux->pp_info != NULL )
    {
        for( i = 0; i < p_avi_demux->i_streams; i++ )
        {
            if( p_avi_demux->pp_info[i] != NULL ) 
            {
#define p_info p_avi_demux->pp_info[i]
/* don't uses RIFF_DeleteChunk -> it will segfault here ( probably because of
    data_packey already unallocated ? */
                if( p_info->p_strl != NULL ) 
                {
                    free( p_info->p_strl );
                }
                if( p_info->p_strh != NULL )
                {
                   free( p_info->p_strh );
                }
                
                if( p_info->p_strf != NULL ) 
                {
                    free( p_info->p_strf );
                }
                if( p_info->p_strd != NULL )
                {
                   free( p_info->p_strd );
                }
                if( p_info->p_index != NULL )
                {
                      free( p_info->p_index );
                }
                free( p_info ); 
#undef  p_info
            }
        }
         free( p_avi_demux->pp_info );
    }
}

static void __AVI_Parse_avih( MainAVIHeader_t *p_avih, byte_t *p_buff )
{
    p_avih->i_microsecperframe = __GetDoubleWordLittleEndianFromBuff( p_buff );
    p_avih->i_maxbytespersec = __GetDoubleWordLittleEndianFromBuff( p_buff + 4);
    p_avih->i_reserved1 = __GetDoubleWordLittleEndianFromBuff( p_buff + 8);
    p_avih->i_flags = __GetDoubleWordLittleEndianFromBuff( p_buff + 12);
    p_avih->i_totalframes = __GetDoubleWordLittleEndianFromBuff( p_buff + 16);
    p_avih->i_initialframes = __GetDoubleWordLittleEndianFromBuff( p_buff + 20);
    p_avih->i_streams = __GetDoubleWordLittleEndianFromBuff( p_buff + 24);
    p_avih->i_suggestedbuffersize = 
                        __GetDoubleWordLittleEndianFromBuff( p_buff + 28);
    p_avih->i_width = __GetDoubleWordLittleEndianFromBuff( p_buff + 32 );
    p_avih->i_height = __GetDoubleWordLittleEndianFromBuff( p_buff + 36 );
    p_avih->i_scale = __GetDoubleWordLittleEndianFromBuff( p_buff + 40 );
    p_avih->i_rate = __GetDoubleWordLittleEndianFromBuff( p_buff + 44 );
    p_avih->i_start = __GetDoubleWordLittleEndianFromBuff( p_buff + 48);
    p_avih->i_length = __GetDoubleWordLittleEndianFromBuff( p_buff + 52);
}

static void __AVI_Parse_Header( AVIStreamHeader_t *p_strh, byte_t *p_buff )
{
    p_strh->i_type = __GetDoubleWordLittleEndianFromBuff( p_buff );
    p_strh->i_handler = __GetDoubleWordLittleEndianFromBuff( p_buff + 4 );
    p_strh->i_flags = __GetDoubleWordLittleEndianFromBuff( p_buff + 8 );
    p_strh->i_reserved1 = __GetDoubleWordLittleEndianFromBuff( p_buff + 12);
    p_strh->i_initialframes = __GetDoubleWordLittleEndianFromBuff( p_buff + 16);
    p_strh->i_scale = __GetDoubleWordLittleEndianFromBuff( p_buff + 20);
    p_strh->i_rate = __GetDoubleWordLittleEndianFromBuff( p_buff + 24);
    p_strh->i_start = __GetDoubleWordLittleEndianFromBuff( p_buff + 28);
    p_strh->i_length = __GetDoubleWordLittleEndianFromBuff( p_buff + 32);
    p_strh->i_suggestedbuffersize = 
                        __GetDoubleWordLittleEndianFromBuff( p_buff + 36);
    p_strh->i_quality = __GetDoubleWordLittleEndianFromBuff( p_buff + 40);
    p_strh->i_samplesize = __GetDoubleWordLittleEndianFromBuff( p_buff + 44);
}

int avi_ParseBitMapInfoHeader( bitmapinfoheader_t *h, byte_t *p_data )
{
    h->i_size          = __GetDoubleWordLittleEndianFromBuff( p_data );
    h->i_width         = __GetDoubleWordLittleEndianFromBuff( p_data + 4 );
    h->i_height        = __GetDoubleWordLittleEndianFromBuff( p_data + 8 );
    h->i_planes        = __GetWordLittleEndianFromBuff( p_data + 12 );
    h->i_bitcount      = __GetWordLittleEndianFromBuff( p_data + 14 );
    h->i_compression   = __GetDoubleWordLittleEndianFromBuff( p_data + 16 );
    h->i_sizeimage     = __GetDoubleWordLittleEndianFromBuff( p_data + 20 );
    h->i_xpelspermeter = __GetDoubleWordLittleEndianFromBuff( p_data + 24 );
    h->i_ypelspermeter = __GetDoubleWordLittleEndianFromBuff( p_data + 28 );
    h->i_clrused       = __GetDoubleWordLittleEndianFromBuff( p_data + 32 );
    h->i_clrimportant  = __GetDoubleWordLittleEndianFromBuff( p_data + 36 );
    return( 0 );
}

int avi_ParseWaveFormatEx( waveformatex_t *h, byte_t *p_data )
{
    h->i_formattag     = __GetWordLittleEndianFromBuff( p_data );
    h->i_channels      = __GetWordLittleEndianFromBuff( p_data + 2 );
    h->i_samplespersec = __GetDoubleWordLittleEndianFromBuff( p_data + 4 );
    h->i_avgbytespersec= __GetDoubleWordLittleEndianFromBuff( p_data + 8 );
    h->i_blockalign    = __GetWordLittleEndianFromBuff( p_data + 12 );
    h->i_bitspersample = __GetWordLittleEndianFromBuff( p_data + 14 );
    h->i_size          = __GetWordLittleEndianFromBuff( p_data + 16 );
    return( 0 );
}

static int __AVI_ParseStreamHeader( u32 i_id, int *i_number, u16 *i_type )
{
    int c1,c2,c3,c4;

    c1 = ( i_id ) & 0xFF;
    c2 = ( i_id >>  8 ) & 0xFF;
    c3 = ( i_id >> 16 ) & 0xFF;
    c4 = ( i_id >> 24 ) & 0xFF;

    if( c1 < '0' || c1 > '9' || c2 < '0' || c2 > '9' )
    {
        return( -1 );
    }
    *i_number = (c1 - '0') * 10 + (c2 - '0' );
    *i_type = ( c3 << 8) + c4;
    return( 0 );
}   
/*
static int __AVI_HeaderMoviValid( u32 i_header )
{
    switch( i_header&0xFFFF0000 )
    {
        case( TWOCC_wb ):
        case( TWOCC_db ):
        case( TWOCC_dc ):
        case( TWOCC_pc ):
            return( 1 );
            break;
    }
    switch( i_header )
    {
        case( FOURCC_LIST ):
        case( FOURCC_JUNK ):
            return( 1 );
            break;
    }
    return( 0 );
}
*/
static void __AVI_AddEntryIndex( AVIStreamInfo_t *p_info,
                                 AVIIndexEntry_t *p_index)
{
    if( p_info->p_index == NULL )
    {
        p_info->i_idxmax = 4096;
        p_info->i_idxnb = 0;
        p_info->p_index = calloc( p_info->i_idxmax, 
                                  sizeof( AVIIndexEntry_t ) );
    }
    if( p_info->i_idxnb >= p_info->i_idxmax )
    {
        p_info->i_idxmax += 4096;
        p_info->p_index = realloc( (void*)p_info->p_index,
                                    p_info->i_idxmax * 
                                    sizeof( AVIIndexEntry_t ) );
    }
    /* calculate cumulate length */
    if( p_info->i_idxnb > 0 )
    {
        p_index->i_lengthtotal = p_index->i_length +
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
    demux_data_avi_file_t *p_avi_demux;
    AVIIndexEntry_t index;
    byte_t          *p_buff;
    riffchunk_t     *p_idx1;
    int             i_read;
    int             i;
    int             i_number;
    u16             i_type;
    
    p_avi_demux = (demux_data_avi_file_t*)p_input->p_demux_data  ;    

    if( RIFF_FindAndGotoDataChunk( p_input,
                                   p_avi_demux->p_riff, 
                                   &p_idx1, 
                                   FOURCC_idx1)!=0 )
    {
        intf_WarnMsg( 1, "input init: cannot find index" );
        RIFF_GoToChunk( p_input, p_avi_demux->p_hdrl );        
        return;
    }
    p_avi_demux->p_idx1 = p_idx1;
    intf_WarnMsg( 1, "input init: loading index" ); 
    for(;;)
    {
        if( (i_read = input_Peek( p_input, &p_buff, 16*1024 )) < 16 )
        {
            for( i = 0, i_read = 0; i < p_avi_demux->i_streams; i++ )
            {
                i_read += p_avi_demux->pp_info[i]->i_idxnb;
            }
            intf_WarnMsg( 1,"input info: read %d idx chunk", i_read );
            return;
        }
        i_read /= 16 ;
        /* TODO try to verify if we are beyond end of p_idx1 */
        for( i = 0; i < i_read; i++ )
        {
            byte_t  *p_peek = p_buff + i * 16;
            index.i_id = __GetDoubleWordLittleEndianFromBuff( p_peek );
            index.i_flags = __GetDoubleWordLittleEndianFromBuff( p_peek+4);
            index.i_offset = __GetDoubleWordLittleEndianFromBuff( p_peek+8);
            index.i_length = __GetDoubleWordLittleEndianFromBuff(p_peek+12);
            
            if( (__AVI_ParseStreamHeader( index.i_id, &i_number, &i_type ) != 0)
             ||(i_number > p_avi_demux->i_streams)) 
            {
                continue;
            }
            __AVI_AddEntryIndex( p_avi_demux->pp_info[i_number],
                                 &index );
        }
        __RIFF_SkipBytes( p_input, 16 * i_read );
    }

}
static int __AVI_SeekToChunk( input_thread_t *p_input, AVIStreamInfo_t *p_info )
{
    demux_data_avi_file_t *p_avi_demux;
    p_avi_demux = (demux_data_avi_file_t*)p_input->p_demux_data;
    
    if( (p_info->p_index != NULL)&&(p_info->i_idxpos < p_info->i_idxnb) )
    {
        /* perfect */
        off_t i_pos;
        i_pos = (off_t)p_info->p_index[p_info->i_idxpos].i_offset +
                    p_info->i_idxoffset;

        p_input->pf_seek( p_input, i_pos );
        input_AccessReinit( p_input );
        return( 0 );
    }
    /* index are no longer valid */
    if( p_info->p_index != NULL )
    {
        return( -1 );
    }
    /* no index */
    return( -1 );
}


/* XXX call after get p_movi */
static int __AVI_GetIndexOffset( input_thread_t *p_input )
{
    riffchunk_t *p_chunk;
    demux_data_avi_file_t *p_avi_demux;
    int i;

    p_avi_demux = (demux_data_avi_file_t*)p_input->p_demux_data;
    for( i = 0; i < p_avi_demux->i_streams; i++ )
    {
#define p_info p_avi_demux->pp_info[i]
        if( p_info->p_index == NULL ) {continue;}
        p_info->i_idxoffset = 0;
        __AVI_SeekToChunk( p_input, p_info );
        p_chunk = RIFF_ReadChunk( p_input );
        if( (p_chunk == NULL)||(p_chunk->i_id != p_info->p_index[0].i_id) )
        {
            p_info->i_idxoffset = p_avi_demux->p_movi->i_pos + 8;
            __AVI_SeekToChunk( p_input, p_info );
            p_chunk = RIFF_ReadChunk( p_input );
            if( (p_chunk == NULL)||(p_chunk->i_id != p_info->p_index[0].i_id) )
            {
                intf_WarnMsg( 1, "input demux: can't find offset for stream %d",
                                i);
                continue; /* TODO: search manually from p_movi */
            }
        }
#undef p_info
    }
    return( 0 );
}
static int __AVI_AudioGetType( u32 i_type )
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

static int __AVI_VideoGetType( u32 i_type )
{
    switch( i_type )
    {
        case( FOURCC_DIV3 ):
        case( FOURCC_div3 ):
        case( FOURCC_DIV4 ):
        case( FOURCC_div4 ):
        case( FOURCC_DIV5 ):
        case( FOURCC_div5 ):
        case( FOURCC_DIV6 ):
        case( FOURCC_div6 ):
        case( FOURCC_3IV1 ):
        case( FOURCC_AP41 ):
        case( FOURCC_MP43 ):
        case( FOURCC_mp43 ):
            return( MSMPEG4_VIDEO_ES );

        case( FOURCC_DIVX ):
        case( FOURCC_divx ):
        case( FOURCC_DX50 ):
        case( FOURCC_MP4S ):
        case( FOURCC_MPG4 ):
        case( FOURCC_mpg4 ):
        case( FOURCC_mp4v ):
            return( MPEG4_VIDEO_ES );

        default:
            return( 0 );
    }
}
/**************************************************************************/

/* Tention: bcp de test à ajouter mais aussi beaucoup de MEMOIRE a DESALLOUER pas fait */
static int AVIInit( input_thread_t *p_input )
{
    riffchunk_t *p_riff,*p_hdrl,*p_movi;
    riffchunk_t *p_avih;
    riffchunk_t *p_strl,*p_strh,*p_strf/* ,*p_strd */;
    
    demux_data_avi_file_t *p_avi_demux;
    es_descriptor_t *p_es = NULL; /* for not warning */
    es_descriptor_t *p_es_video = NULL; 
    es_descriptor_t *p_es_audio = NULL;

    int i;
    
    p_avi_demux = malloc( sizeof(demux_data_avi_file_t) );
    memset( p_avi_demux, 0, sizeof( demux_data_avi_file_t ) );
    p_input->p_demux_data = p_avi_demux;

    /* FIXME FIXME Je sais pas trop a quoi ca sert juste copié de ESInit */
    /* Initialize access plug-in structures. */
    if( p_input->i_mtu == 0 )
    {
        /* Improve speed. */
        p_input->i_bufsize = INPUT_DEFAULT_BUFSIZE;
    }

    if( RIFF_TestFileHeader( p_input, &p_riff, FOURCC_AVI ) != 0 )    
    {
        __AVIFreeDemuxData( p_input );
        intf_ErrMsg( "input: RIFF-AVI plug-in discarded (avi_file)" );
        return( -1 );
    }
    p_avi_demux->p_riff = p_riff;

    if ( RIFF_DescendChunk(p_input) != 0 )
    {
        __AVIFreeDemuxData( p_input );
        intf_ErrMsg( "input error: cannot look for subchunk (avi_file)" );
        return ( -1 );
    }

    /* it's a riff-avi file, so search for LIST-hdrl */
    if( RIFF_FindListChunk(p_input ,&p_hdrl,p_riff, FOURCC_hdrl) != 0 )
    {
        __AVIFreeDemuxData( p_input );
        intf_ErrMsg( "input error: cannot find \"LIST-hdrl\" (avi_file)" );
        return( -1 );
    }
    p_avi_demux->p_hdrl = p_hdrl;

    if( RIFF_DescendChunk(p_input) != 0 )
    {
        __AVIFreeDemuxData( p_input );
        intf_ErrMsg( "input error: cannot look for subchunk (avi_file)" );
        return ( -1 );
    }
    /* ds  LIST-hdrl cherche avih */
    if( RIFF_FindAndLoadChunk( p_input, p_hdrl, 
                                    &p_avih, FOURCC_avih ) != 0 )
    {
        __AVIFreeDemuxData( p_input );
        intf_ErrMsg( "input error: cannot find \"avih\" chunk (avi_file)" );
        return( -1 );
    }
    __AVI_Parse_avih( &p_avi_demux->avih, p_avih->p_data->p_payload_start );
    RIFF_DeleteChunk( p_input, p_avih );
    
    if( p_avi_demux->avih.i_streams == 0 )  
            /* aucun flux defini, peut etre essayer de trouver ss connaitre */
                                      /* le nombre serait pas mal, a voir */
    {
        __AVIFreeDemuxData( p_input );
        intf_ErrMsg( "input error: no defined stream !" );
        return( -1 );
    }
    
    /* On creer les tableau pr les flux */
    p_avi_demux->i_streams = p_avi_demux->avih.i_streams;
    
    p_avi_demux->pp_info = calloc( p_avi_demux->i_streams, 
                                    sizeof( AVIStreamInfo_t* ) );
    memset( p_avi_demux->pp_info, 0, 
                        sizeof( AVIStreamInfo_t* ) * p_avi_demux->i_streams );
    
    for( i = 0 ; i < p_avi_demux->i_streams; i++ )
    {
        p_avi_demux->pp_info[i] = malloc( sizeof(AVIStreamInfo_t ) );
        memset( p_avi_demux->pp_info[i], 0, sizeof( AVIStreamInfo_t ) );        

        /* pour chaque flux on cherche ses infos */
        if( RIFF_FindListChunk(p_input,
                                &p_strl,p_hdrl, FOURCC_strl) != 0 )
        {
            __AVIFreeDemuxData( p_input );
            intf_ErrMsg( "input error: cannot find \"LIST-strl\" (avi_file)" );
            return( -1 );
        }
        p_avi_demux->pp_info[i]->p_strl = p_strl;
        
        if( RIFF_DescendChunk(p_input) != 0 )
        {
            __AVIFreeDemuxData( p_input );
            intf_ErrMsg( "input error: cannot look for subchunk (avi_file)" );
            return ( -1 );
        }
        /* ds  LIST-strl cherche strh */
        if( RIFF_FindAndLoadChunk( p_input, p_hdrl, 
                                &p_strh, FOURCC_strh ) != 0 )
        {
            __AVIFreeDemuxData( p_input );
            intf_ErrMsg( "input error: cannot find \"strh\" (avi_file)" );
            return( -1 );
        }
        p_avi_demux->pp_info[i]->p_strh = p_strh;
        
        /* ds  LIST-strl cherche strf */
        if( RIFF_FindAndLoadChunk( p_input, p_hdrl, 
                                &p_strf, FOURCC_strf ) != 0 )
        {
            __AVIFreeDemuxData( p_input );
            intf_ErrMsg( "input error: cannot find \"strf\" (avi_file)" );
            return( -1 );
        }
         p_avi_demux->pp_info[i]->p_strf = p_strf;
        /* FIXME faudrait cherche et charger strd */
        /* mais a priori pas vraiment utile pr divx */

         if( RIFF_AscendChunk(p_input, p_strl) != 0 )
        {
            __AVIFreeDemuxData( p_input );
            intf_ErrMsg( "input error: cannot go out (\"strl\") (avi_file)" );
            return( -1 );
        }
        
    }
    

    if( RIFF_AscendChunk(p_input, p_hdrl) != 0)
    {
        __AVIFreeDemuxData( p_input );
        intf_ErrMsg( "input error: cannot go out (\"hdrl\") (avi_file)" );
        return( -1 );
    }

    intf_Msg( "input init: AVIH: %d stream, flags %s%s%s%s%s%s ", 
            p_avi_demux->i_streams,
            p_avi_demux->avih.i_flags&AVIF_HASINDEX?" HAS_INDEX":"",
            p_avi_demux->avih.i_flags&AVIF_MUSTUSEINDEX?" MUST_USE_INDEX":"",
            p_avi_demux->avih.i_flags&AVIF_ISINTERLEAVED?" IS_INTERLEAVED":"",
            p_avi_demux->avih.i_flags&AVIF_TRUSTCKTYPE?" TRUST_CKTYPE":"",
            p_avi_demux->avih.i_flags&AVIF_WASCAPTUREFILE?" CAPTUREFILE":"",
            p_avi_demux->avih.i_flags&AVIF_COPYRIGHTED?" COPYRIGHTED":"" );

    /* go to movi chunk */
    if( RIFF_FindListChunk(p_input ,&p_movi,p_riff, FOURCC_movi) != 0 )
    {
        intf_ErrMsg( "input error: cannot find \"LIST-movi\" (avi_file)" );
        __AVIFreeDemuxData( p_input );
        return( -1 );
    }
    p_avi_demux->p_movi = p_movi;
    
    /* get index */
    if( (p_input->stream.b_seekable)
         &&((p_avi_demux->avih.i_flags&AVIF_HASINDEX) != 0) )
    {
        __AVI_GetIndex( p_input );
        /* try to get i_idxoffset with first stream*/
        __AVI_GetIndexOffset( p_input );
        RIFF_GoToChunk( p_input, p_avi_demux->p_movi );
    }
    else
    {
        intf_WarnMsg( 1, "input init: cannot get index" );
    }

    if( RIFF_DescendChunk( p_input ) != 0 )
    {
        __AVIFreeDemuxData( p_input );
        intf_ErrMsg( "input error: cannot go in (\"movi\") (avi_file)" );
        return( -1 );
    }
   /* TODO: check for index and read it if possible( seekable )*/

   /** We have now finished with reading the file **/
   /** we make the last initialisation  **/

    if( input_InitStream( p_input, 0 ) == -1)
    {
        __AVIFreeDemuxData( p_input );
        intf_ErrMsg( "input error: cannot init stream" );
        return( -1 );
    }

    if( input_AddProgram( p_input, 0, 0) == NULL )
    {
        __AVIFreeDemuxData( p_input );
        intf_ErrMsg( "input error: cannot add program" );
        return( -1 );
    }
    p_input->stream.p_selected_program = p_input->stream.pp_programs[0];
    p_input->stream.p_new_program = p_input->stream.pp_programs[0] ;
            
    vlc_mutex_lock( &p_input->stream.stream_lock );
    for( i = 0; i < p_avi_demux->i_streams; i++ )
    {
#define p_info  p_avi_demux->pp_info[i]
        __AVI_Parse_Header( &p_info->header,
                        p_info->p_strh->p_data->p_payload_start);
        switch( p_info->header.i_type )
        {
            case( FOURCC_auds ):
                /* pour l'id j'ai mis 12+i pr audio et 42+i pour video */
                /* et le numero du flux(ici i) dans i_stream_id */
                avi_ParseWaveFormatEx( &p_info->audio_format,
                                   p_info->p_strf->p_data->p_payload_start ); 
                p_es = input_AddES( p_input, 
                                p_input->stream.p_selected_program, 12+i, 
                                    p_info->p_strf->i_size );
                p_es->b_audio = 1;
                p_es->i_type = 
                    __AVI_AudioGetType( p_info->audio_format.i_formattag );
                p_es->i_stream_id =i; /* FIXME */                
                if( p_es->i_type == 0 )
                {
                    intf_ErrMsg( "input error: stream(%d,0x%x) not supported",
                                    i,
                                    p_info->audio_format.i_formattag );
                    p_es->i_cat = UNKNOWN_ES;
                }
                else
                {
                    if( p_es_audio == NULL ) {p_es_audio = p_es;}
                    p_es->i_cat = AUDIO_ES;
                }
                break;
                
            case( FOURCC_vids ):
                avi_ParseBitMapInfoHeader( &p_info->video_format,
                                   p_info->p_strf->p_data->p_payload_start ); 

                p_es = input_AddES( p_input, 
                                p_input->stream.p_selected_program, 42+i,
                                    p_info->p_strf->i_size );
                p_es->b_audio = 0;
                p_es->i_type = 
                    __AVI_VideoGetType( p_info->video_format.i_compression );
                p_es->i_stream_id =i; /* FIXME */                
                if( p_es->i_type == 0 )
                {
                    intf_ErrMsg( "input error: stream(%d,%4.4s) not supported",
                               i,
                               (char*)&p_info->video_format.i_compression);
                    p_es->i_cat = UNKNOWN_ES;
                }
                else
                {
                    if( p_es_video == NULL ) {p_es_video = p_es;}
                    p_es->i_cat = VIDEO_ES;
                }
                break;
            default:
                p_es = input_AddES( p_input, 
                                p_input->stream.p_selected_program, 12, 
                                    p_info->p_strf->i_size );
                intf_ErrMsg( "input error: unknown stream(%d) type",
                            i );
                p_es->i_cat = UNKNOWN_ES;
                break;
        }
        p_info->p_es = p_es;
        p_info->i_cat = p_es->i_cat;
        /* We copy strf for decoder in p_es->p_demux_data */
        memcpy( p_es->p_demux_data, 
                p_info->p_strf->p_data->p_payload_start,
                p_info->p_strf->i_size );
        /* print informations on stream */
        switch( p_es->i_cat )
        {
            case( VIDEO_ES ):
                intf_Msg("input init: video(%4.4s) %dx%d %dbpp %ffps (size %d)",
                        (char*)&p_info->video_format.i_compression,
                        p_info->video_format.i_width,
                        p_info->video_format.i_height,
                        p_info->video_format.i_bitcount,
                        (float)p_info->header.i_rate /
                            (float)p_info->header.i_scale,
                        p_info->header.i_samplesize );
                break;
            case( AUDIO_ES ):
                intf_Msg( "input init: audio(0x%x) %d channels %dHz %dbits %ffps (size %d)",
                        p_info->audio_format.i_formattag,
                        p_info->audio_format.i_channels,
                        p_info->audio_format.i_samplespersec,
                        p_info->audio_format.i_bitspersample,
                        (float)p_info->header.i_rate /
                            (float)p_info->header.i_scale,
                        p_info->header.i_samplesize );
                break;
        }

#undef p_info    
    }

    /* we select the first audio and video ES */
    if( p_es_video != NULL ) 
    {
        input_SelectES( p_input, p_es_video );
    }
    else
    {
        intf_ErrMsg( "input error: no video stream found !" );
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        return( -1 );
    }
    if( p_es_audio != NULL ) 
    {
        input_SelectES( p_input, p_es_audio );
    }
    else
    {
        intf_Msg( "input init: no audio stream found !" );
    }

   /* p_input->stream.p_selected_area->i_tell = 0; */
    p_input->stream.i_mux_rate = p_avi_demux->avih.i_maxbytespersec / 50;
    p_input->stream.p_selected_program->b_is_ok = 1;
    vlc_mutex_unlock( &p_input->stream.stream_lock );
    
    return( 0 );
}

static void AVIEnd( input_thread_t *p_input )
{   
    __AVIFreeDemuxData( p_input ); 
    return;
}


static mtime_t __AVI_GetPTS( AVIStreamInfo_t *p_info )
{
    /* XXX you need to had p_info->i_date to have correct pts */
    /* p_info->p_index[p_info->i_idxpos] need to be valid !! */
    mtime_t i_pts;

    /* be careful to  *1000000 before round  ! */
    if( p_info->header.i_samplesize != 0 )
    {
        i_pts = (mtime_t)( (double)1000000.0 *
                    (double)p_info->p_index[p_info->i_idxpos].i_lengthtotal *
                    (double)p_info->header.i_scale /
                    (double)p_info->header.i_rate /
                    (double)p_info->header.i_samplesize );
    }
    else
    {
        i_pts = (mtime_t)( (double)1000000.0 *
                    (double)p_info->i_idxpos *
                    (double)p_info->header.i_scale /
                    (double)p_info->header.i_rate);
    }
    return( i_pts );
}



static void __AVI_NextIndexEntry( input_thread_t *p_input, 
                                  AVIStreamInfo_t *p_info )
{
   p_info->i_idxpos++;
   if( p_info->i_idxpos >= p_info->i_idxnb )
   {
        /* we need to verify if we reach end of file
            or if index is broken and search manually */
        intf_WarnMsg( 1, "input demux: out of index" );
   }
}

static int __AVI_ReAlign( input_thread_t *p_input, 
                            AVIStreamInfo_t  *p_info )
{
    u32     u32_pos;
    off_t   i_pos;
    
    __RIFF_TellPos( p_input, &u32_pos );
     i_pos = (off_t)u32_pos - (off_t)p_info->i_idxoffset;
   
    /* TODO verifier si on est dans p_movi */
    
    if( p_info->p_index[p_info->i_idxnb-1].i_offset <= i_pos )
    {
        p_info->i_idxpos = p_info->i_idxnb-1;
        return( 0 ); 
    }
    
    if( i_pos <= p_info->p_index[0].i_offset )
    {
        p_info->i_idxpos = 0;
        return( 0 );
    }
    /* if we have seek in the current chunk then do nothing 
        __AVI_SeekToChunk will correct */
    if( (p_info->p_index[p_info->i_idxpos].i_offset <= i_pos)
            && ( i_pos < p_info->p_index[p_info->i_idxpos].i_offset + 
                    p_info->p_index[p_info->i_idxpos].i_length ) )
    {
        return( 0 );
    }

    if( i_pos >= p_info->p_index[p_info->i_idxpos].i_offset )
    {
        /* search for a chunk after i_idxpos */
        while( (p_info->p_index[p_info->i_idxpos].i_offset < i_pos)
                &&( p_info->i_idxpos < p_info->i_idxnb - 1 ) )
        {
            p_info->i_idxpos++;
        }
        while( ((p_info->p_index[p_info->i_idxpos].i_flags&AVIIF_KEYFRAME) == 0)
                &&( p_info->i_idxpos < p_info->i_idxnb - 1 ) )
        {
            p_info->i_idxpos++;
        }
    }
    else
    {
        /* search for a chunk before i_idxpos */
        while( (p_info->p_index[p_info->i_idxpos].i_offset + 
                    p_info->p_index[p_info->i_idxpos].i_length >= i_pos)
                        &&( p_info->i_idxpos > 0 ) )
        {
            p_info->i_idxpos--;
        }
        while( ((p_info->p_index[p_info->i_idxpos].i_flags&AVIIF_KEYFRAME) == 0)
                &( p_info->i_idxpos > 0 ) )
        {
            p_info->i_idxpos--;
        }
    }
    
    return( 0 );
}
static void __AVI_SynchroReInit( input_thread_t *p_input,
                                 AVIStreamInfo_t *p_info_master,
                                 AVIStreamInfo_t *p_info_slave )
{
    demux_data_avi_file_t *p_avi_demux;
    p_avi_demux = (demux_data_avi_file_t*)p_input->p_demux_data;
    p_avi_demux->i_date = mdate() + DEFAULT_PTS_DELAY 
                            - __AVI_GetPTS( p_info_master );
    if( p_info_slave != NULL )
    {
        /* TODO: a optimiser */
        p_info_slave->i_idxpos = 0; 
        p_info_slave->b_unselected = 1; /* to correct audio */
    }
    p_input->stream.p_selected_program->i_synchro_state = SYNCHRO_OK;
} 
/** -1 in case of error, 0 of EOF, 1 otherwise **/
static int AVIDemux( input_thread_t *p_input )
{
    /* on cherche un block
       plusieurs cas :
        * encapsuler dans un chunk "rec "
        * juste une succesion de 00dc 01wb ...
        * pire tout audio puis tout video ou vice versa
     */
/* TODO :   * a better method to realign
            * verify that we are reading in p_movi 
            * XXX be sure to send audio before video to avoid click
            * 
 */
    riffchunk_t *p_chunk;
    int i;
    pes_packet_t *p_pes;
    demux_data_avi_file_t *p_avi_demux;

    AVIStreamInfo_t *p_info_video;
    AVIStreamInfo_t *p_info_audio;
    AVIStreamInfo_t *p_info;
    /* XXX arrive pas a avoir acces a cette fct° 
    input_ClockManageRef( p_input,
                            p_input->stream.p_selected_program,
                            (mtime_t)0 ); */
    p_avi_demux = (demux_data_avi_file_t*)p_input->p_demux_data;

    /* search video and audio stream selected */
    p_info_video = NULL;
    p_info_audio = NULL;
    
    for( i = 0; i < p_avi_demux->i_streams; i++ )
    {
        if( p_avi_demux->pp_info[i]->p_es->p_decoder_fifo != NULL )
        {
            switch( p_avi_demux->pp_info[i]->p_es->i_cat )
            {
                case( VIDEO_ES ):
                    p_info_video = p_avi_demux->pp_info[i];
                    break;
                case( AUDIO_ES ):
                    p_info_audio = p_avi_demux->pp_info[i];
                    break;
            }
        }
        else
        {
            p_avi_demux->pp_info[i]->b_unselected = 1;
        }
    }
    if( p_info_video == NULL )
    {
        intf_ErrMsg( "input error: no video ouput selected" );
        return( -1 );
    }

    if( input_ClockManageControl( p_input, p_input->stream.p_selected_program,
                            (mtime_t)0) == PAUSE_S )
    {   
        __AVI_SynchroReInit( p_input, p_info_video, p_info_audio );
    }

    /* after updated p_avi_demux->pp_info[i]->b_unselected  !! */
    if( p_input->stream.p_selected_program->i_synchro_state == SYNCHRO_REINIT )
    { 
        __AVI_ReAlign( p_input, p_info_video ); /*on se realigne pr la video */
        __AVI_SynchroReInit( p_input, p_info_video, p_info_audio );
    }

     /* update i_date if previously unselected ES (ex: 2 channels audio ) */
    if( (p_info_audio != NULL)&&(p_info_audio->b_unselected ))
    {
        /* we have to go to the good pts */
        /* we will reach p_info_ok pts */
        while( __AVI_GetPTS( p_info_audio) < __AVI_GetPTS( p_info_video) )
        {
            __AVI_NextIndexEntry( p_input, p_info_audio );
        }
       p_info_audio->b_unselected = 0 ;
    }
  
    /* what stream we should read in first */
    if( p_info_audio == NULL )
    {
        p_info = p_info_video;
    }
    else
    {
        if( __AVI_GetPTS( p_info_audio ) <= 
                        __AVI_GetPTS( p_info_video ) )
        {
            p_info = p_info_audio;
        }
        else
        {
            p_info = p_info_video;
        }
    }

    /* go to the good chunk to read */

    __AVI_SeekToChunk( p_input, p_info );
    
    /* now we just need to read a chunk */
    if( (p_chunk = RIFF_ReadChunk( p_input )) == NULL )
    {   
        intf_ErrMsg( "input demux: cannot read chunk" );
        return( -1 );
    }

    if( (p_chunk->i_id&0xFFFF0000) != 
                    (p_info->p_index[p_info->i_idxpos].i_id&0xFFFF0000) )
    {
        intf_WarnMsg( 2, "input demux: bad index entry" );
        __AVI_NextIndexEntry( p_input, p_info );
        return( 1 );
    }
/*    
    intf_WarnMsg( 6, "input demux: read %4.4s chunk %d bytes",
                    (char*)&p_chunk->i_id,
                    p_chunk->i_size);
*/                    
    if( RIFF_LoadChunkDataInPES(p_input, p_chunk, &p_pes) != 0 )
    {
        intf_ErrMsg( "input error: cannot read data" );
        return( -1 );
    }

    p_pes->i_rate = p_input->stream.control.i_rate; 
    p_pes->i_pts = p_avi_demux->i_date + __AVI_GetPTS( p_info );
    p_pes->i_dts = 0;
    
    /* send to decoder */
    vlc_mutex_lock( &p_info->p_es->p_decoder_fifo->data_lock );
    /* change MAX_PACKET and replace it to have same duration of audio 
        and video in buffer, to avoid unsynchronization while seeking */
    if( p_info->p_es->p_decoder_fifo->i_depth >= MAX_PACKETS_IN_FIFO )
    {
        /* Wait for the decoder. */
        vlc_cond_wait( &p_info->p_es->p_decoder_fifo->data_wait, 
                        &p_info->p_es->p_decoder_fifo->data_lock );
    }
    vlc_mutex_unlock( &p_info->p_es->p_decoder_fifo->data_lock );
    input_DecodePES( p_info->p_es->p_decoder_fifo, p_pes );

    __AVI_NextIndexEntry( p_input, p_info );
    if( p_info->i_idxpos >= p_info->i_idxnb ) 
    {
        /* reach end of p_index , to be corrected to use p_movi instead */
        return( 0 ); 
    }

    return( 1 );
}
