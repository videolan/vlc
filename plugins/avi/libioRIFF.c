/*****************************************************************************
 * libioRIFF.c : AVI file Stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: libioRIFF.c,v 1.8 2002/06/27 18:10:16 fenrir Exp $
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

#include <stdlib.h>
#include <vlc/vlc.h>
#include <vlc/input.h>
#include <video.h>

#include "libioRIFF.h"

inline u16 __GetWLE( byte_t *p_buff )
{
    return( (*p_buff) + ( *(p_buff+1) <<8 ) );
}

inline u32 __GetDWLE( byte_t *p_buff )
{
    return( *(p_buff) + ( *(p_buff+1) <<8 ) + 
            ( *(p_buff+2) <<16 ) + ( *(p_buff+3) <<24 ) );
}

inline u32 __EVEN( u32 i )
{
    return( (i & 1) ? ++i : i );
}
        
int __RIFF_TellPos( input_thread_t *p_input, u32 *pos )
{ 
    vlc_mutex_lock( &p_input->stream.stream_lock );
    *pos= p_input->stream.p_selected_area->i_tell - 
            ( p_input->p_last_data - p_input->p_current_data  );
    vlc_mutex_unlock( &p_input->stream.stream_lock );
    return 0;
}

int 	__RIFF_SkipBytes(input_thread_t * p_input,int nb)
{  
    data_packet_t *p_pack;
    int i;
    int i_rest;
    if( p_input->stream.b_seekable )
    {
        u32 i_pos;
        __RIFF_TellPos( p_input, &i_pos);
        p_input->pf_seek( p_input, (off_t)(i_pos + nb) );
        input_AccessReinit( p_input );
    }
    else
    {
        msg_Warn( p_input, "cannot seek, it will take times" );
        if( nb < 0 ) { return( -1 ); }
        i_rest = nb;
        while (i_rest != 0 )
        {
            if ( i_rest >= 4096 )
            {
                i = input_SplitBuffer( p_input, &p_pack, 4096);
            }
            else
            {
                i = input_SplitBuffer( p_input, &p_pack, i_rest);
            }
                    
            if ( i < 0 ) { return ( -1 ); }
            i_rest-=i;
            input_DeletePacket( p_input->p_method_data, p_pack);
            if( ( i == 0 )&&( i_rest != 0 )) { return( -1 ); }
        }
    }
	return ( 0 );
}


void RIFF_DeleteChunk( input_thread_t *p_input, riffchunk_t *p_chunk )
{
    if( p_chunk != NULL)
    {
        if( p_chunk->p_data != NULL )
        {
            input_DeletePacket( p_input->p_method_data, p_chunk->p_data );
        }
        free( p_chunk );
    }
}

riffchunk_t     * RIFF_ReadChunk(input_thread_t * p_input)
{
    riffchunk_t * p_riff;
    int count;
    byte_t * p_peek;
 
	if( !(p_riff = malloc( sizeof(riffchunk_t))) )
	{
		return( NULL );
	}
	
	p_riff->p_data = NULL;
	/* peek to have the begining, 8+4 where 4 are to get type */
	if( ( count = input_Peek( p_input, &p_peek, 12 ) ) < 8 )
	{
		msg_Err( p_input, "cannot peek()" );
		free(p_riff);
		return( NULL );
	}
	
	p_riff->i_id = __GetDWLE( p_peek );
	p_riff->i_size =__GetDWLE( p_peek + 4 );
	p_riff->i_type = ( count == 12 ) ? __GetDWLE( p_peek + 8 ) : 0 ;

	__RIFF_TellPos(p_input, &(p_riff->i_pos) );
	
	return( p_riff );	
}

/**************************************************
 * Va au chunk juste d'apres si il en a encore    *
 * -1 si erreur , 1 si y'en a plus                *
 **************************************************/
int RIFF_NextChunk( input_thread_t * p_input,riffchunk_t *p_rifffather)
{
    int i_len;
    int i_lenfather;
    riffchunk_t *p_riff;

	if( ( p_riff = RIFF_ReadChunk( p_input ) ) == NULL )
	{
		msg_Err( p_input, "cannot read chunk" );
		return( -1 );
	}
	i_len = __EVEN( p_riff->i_size );

	if ( p_rifffather != NULL )
	{
		i_lenfather = __EVEN( p_rifffather->i_size );
		if ( p_rifffather->i_pos + i_lenfather  <= p_riff->i_pos + i_len + 8 )
		{
            msg_Err( p_input, "next chunk out of bounds" );
			free( p_riff );
			return( 1 ); /* pas dans nos frontiere */
		}
	}
	if ( __RIFF_SkipBytes( p_input,i_len + 8 ) != 0 )
	{ 
		free( p_riff );
		msg_Err( p_input, "cannot go to the next chunk" );
		return( -1 );
	}
	free( p_riff );
	return( 0 );
}

/****************************************************************
 * Permet de rentrer dans un ck RIFF ou LIST                    *
 ****************************************************************/
int	RIFF_DescendChunk(input_thread_t * p_input)
{
	return(  __RIFF_SkipBytes(p_input,12) != 0 ? -1 : 0 );
}

/***************************************************************
 * Permet de sortir d'un sous chunk et d'aller sur le suivant  *
 * chunk                                                       *
 ***************************************************************/

int	RIFF_AscendChunk(input_thread_t * p_input ,riffchunk_t *p_riff)
{
    int i_skip;
    u32 i_posactu;

    __RIFF_TellPos(p_input, &i_posactu);
	i_skip  = __EVEN( p_riff->i_pos + p_riff->i_size + 8 ) - i_posactu;
    return( (( __RIFF_SkipBytes(p_input,i_skip)) != 0) ? -1 : 0 );
}

/***************************************************************
 * Permet de se deplacer jusqu'au premier chunk avec le bon id *
 * *************************************************************/
int	RIFF_FindChunk(input_thread_t * p_input ,u32 i_id,riffchunk_t *p_rifffather)
{
 riffchunk_t *p_riff = NULL;
	do
	{
		if ( p_riff ) 
		{ 
			free(p_riff); 
			if ( RIFF_NextChunk(p_input ,p_rifffather) != 0 ) 
            { 
                return( -1 );
            }
		}
		p_riff=RIFF_ReadChunk(p_input);
 	} while ( ( p_riff )&&( p_riff->i_id != i_id ) );

    if ( ( !p_riff )||( p_riff->i_id != i_id ) )
    { 
        return( -1 );
    }
    free( p_riff );
 	return( 0 );
}

/*****************************************************************
 * Permet de pointer sur la zone de donné du chunk courant       *
 *****************************************************************/
int  RIFF_GoToChunkData(input_thread_t * p_input)
{
	return( ( __RIFF_SkipBytes(p_input,8) != 0 ) ? -1 : 0 );
}

int	RIFF_LoadChunkData(input_thread_t * p_input,riffchunk_t *p_riff )
{
    off_t   i_read = __EVEN( p_riff->i_size );

	RIFF_GoToChunkData(p_input);
	if ( input_SplitBuffer( p_input, 
                            &p_riff->p_data, 
                            i_read ) != i_read )
	{
		msg_Err( p_input, "cannot read enough data " );
		return ( -1 );
	}

    if( p_riff->i_size&1 )
    {
        p_riff->p_data->p_payload_end--;
    }
	return( 0 );
}

int	RIFF_LoadChunkDataInPES(input_thread_t * p_input,
                                    pes_packet_t **pp_pes,
                                    int i_size_index)
{
    u32 i_read;
    data_packet_t *p_data;
    riffchunk_t   *p_riff;
    int i_size;
    int b_pad = 0;
    
    if( (p_riff = RIFF_ReadChunk( p_input )) == NULL )
    {
        *pp_pes = NULL;
        return( -1 );
    }
	RIFF_GoToChunkData(p_input);
    *pp_pes = input_NewPES( p_input->p_method_data );

    if( *pp_pes == NULL )
    {
        return( -1 );
    }

    if( (!p_riff->i_size) || (!i_size_index ) )
    {
        i_size = __MAX( i_size_index, p_riff->i_size );
    }
    else
    {
        i_size = __MIN( p_riff->i_size, i_size_index );
    }
    
    if( !p_riff->i_size )
    {
        p_data = input_NewPacket( p_input->p_method_data, 0 );
        (*pp_pes)->p_first = p_data;
        (*pp_pes)->p_last  = p_data;
        (*pp_pes)->i_nb_data = 1;
        (*pp_pes)->i_pes_size = 0;
        return( 0 );
    }
    if( i_size&1 )
    {
        i_size++;
        b_pad = 1;
    }

    do
    {
        i_read = input_SplitBuffer(p_input, &p_data, i_size - 
                                                    (*pp_pes)->i_pes_size );
        if( i_read < 0 )
        {
            /* FIXME free on all packets */
            return( -1 );
        }
        if( (*pp_pes)->p_first == NULL )
        {
            (*pp_pes)->p_first = p_data;
            (*pp_pes)->p_last  = p_data;
            (*pp_pes)->i_nb_data = 1;
            (*pp_pes)->i_pes_size = ( p_data->p_payload_end - 
                                        p_data->p_payload_start );
        }
        else
        {
            (*pp_pes)->p_last->p_next = p_data;
            (*pp_pes)->p_last = p_data;
            (*pp_pes)->i_nb_data++;
            (*pp_pes)->i_pes_size += ( p_data->p_payload_end -
                                       p_data->p_payload_start );
        }
    } while( ((*pp_pes)->i_pes_size < i_size)&&(i_read != 0) );

    if( b_pad )
    {
        (*pp_pes)->i_pes_size--;
        (*pp_pes)->p_last->p_payload_end--;
    }
	return( 0 );
}

int	RIFF_GoToChunk(input_thread_t * p_input, riffchunk_t *p_riff)
{
    if( p_input->stream.b_seekable )
    {
        p_input->pf_seek( p_input, (off_t)p_riff->i_pos );
        input_AccessReinit( p_input );
	    return( 0 );
    }
    return( -1 );
}

int   RIFF_TestFileHeader( input_thread_t * p_input, riffchunk_t ** pp_riff, u32 i_type )
{
    if( !( *pp_riff = RIFF_ReadChunk( p_input ) ) )
    {
        return( -1 );
    }
    if( ( (*pp_riff)->i_id != FOURCC_RIFF )||( (*pp_riff)->i_type != i_type ) )
    {
        free( *pp_riff );
        return( -1 );
    } 
    return( 0 );  
}


int   RIFF_FindAndLoadChunk( input_thread_t * p_input, riffchunk_t *p_riff, riffchunk_t **pp_fmt, u32 i_type )
{
    *pp_fmt = NULL;
    if ( RIFF_FindChunk( p_input, i_type, p_riff ) != 0)
    {
        return( -1 );
    }
    if ( ( (*pp_fmt = RIFF_ReadChunk( p_input )) == NULL) 
                    || ( RIFF_LoadChunkData( p_input, *pp_fmt ) != 0 ) )
    {
        if( *pp_fmt != NULL ) 
        { 
            RIFF_DeleteChunk( p_input, *pp_fmt ); 
        }
        return( -1 );
    }
    return( 0 );
}

int   RIFF_FindAndGotoDataChunk( input_thread_t * p_input, riffchunk_t *p_riff, riffchunk_t **pp_data, u32 i_type )
{
    *pp_data = NULL;
    if ( RIFF_FindChunk( p_input, i_type, p_riff ) != 0)
    {
        return( -1 );
    }
    if ( ( *pp_data = RIFF_ReadChunk( p_input ) ) == NULL )
    {
        return( -1 );
    }
    if ( RIFF_GoToChunkData( p_input ) != 0 )
    {
        RIFF_DeleteChunk( p_input, *pp_data );
        return( -1 );
    }
    return( 0 );
}

int   RIFF_FindListChunk( input_thread_t *p_input, riffchunk_t **pp_riff, riffchunk_t *p_rifffather, u32 i_type )
{
    int i_ok;
    
    *pp_riff = NULL;
    i_ok = 0;
    while( i_ok == 0 )
    {
        if( *pp_riff != NULL )
        {
            free( *pp_riff );
        }
        if( RIFF_FindChunk( p_input, FOURCC_LIST, p_rifffather) != 0 )
        {
            return( -1 );
        }
        *pp_riff = RIFF_ReadChunk( p_input );
                        
        if( *pp_riff == NULL )
        {
            return( -1 );
        }
        if( (*pp_riff)->i_type != i_type )
        {
            if( RIFF_NextChunk( p_input, p_rifffather ) != 0 )
            {
                return( -1 );
            }
        }
        else
        {
            i_ok = 1;
        }
    }
    return( 0 );  
}
