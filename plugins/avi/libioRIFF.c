/*****************************************************************************
 * libioRIFF.c : AVI file Stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: libioRIFF.c,v 1.2 2002/04/30 12:35:24 fenrir Exp $
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

typedef struct riffchunk_s
{
    u32 i_id;
    u32 i_size;
    u32 i_type;
    u32 i_pos;   /* peut etre a changer */
    data_packet_t *p_data; /* pas forcement utilise */
    struct riffchunk_s *p_next;
    struct riffchunk_s *p_subchunk;
} riffchunk_t;

/* ttes ces fonctions permettent un acces lineaire sans avoir besoin de revenrir en arriere */
static riffchunk_t  * RIFF_ReadChunk(input_thread_t * p_input);
static int            RIFF_NextChunk(input_thread_t * p_input,riffchunk_t *p_rifffather);
static int            RIFF_DescendChunk(input_thread_t * p_input);
static int            RIFF_AscendChunk(input_thread_t * p_input,riffchunk_t *p_rifffather);
static int            RIFF_FindChunk(input_thread_t * p_input,u32 i_id,riffchunk_t *p_rifffather);
static int            RIFF_GoToChunkData(input_thread_t * p_input);
static int            RIFF_LoadChunkData(input_thread_t * p_input,riffchunk_t *p_riff);
static int            RIFF_TestFileHeader(input_thread_t * p_input, riffchunk_t **pp_riff, u32 i_type);
static int            RIFF_FindAndLoadChunk( input_thread_t * p_input, riffchunk_t *p_riff, riffchunk_t **pp_fmt, u32 i_type );
static int            RIFF_FindAndGotoDataChunk( input_thread_t * p_input, riffchunk_t *p_riff, riffchunk_t **pp_data, u32 i_type );
static int            RIFF_FindListChunk( input_thread_t *p_input, riffchunk_t **pp_riff, riffchunk_t *p_rifffather, u32 i_type );

static void           RIFF_DeleteChunk( input_thread_t * p_input, riffchunk_t *p_chunk );


/* 
 ces fonctions on besoin de pouvoir faire des seek 
 static int            RIFF_GoToChunk(input_thread_t * p_input,riffchunk_t *p_riff);
*/

static u32            RIFF_4cToI(char c1,char c2,char c3,char c4);
static char         * RIFF_IToStr(u32 i);

/*************************************************************************/

/********************************************
 * Fonction locale maintenant               *
 ********************************************/

static int __RIFF_TellPos( input_thread_t *p_input, u32 *pos )
{ /* pas sur que ca marche */
    u32 i;
    
    vlc_mutex_lock( &p_input->stream.stream_lock );
    i = p_input->stream.p_selected_area->i_tell - ( p_input->p_last_data - p_input->p_current_data  );
    vlc_mutex_unlock( &p_input->stream.stream_lock );
    *pos = i; 
    return 0;
}

static int 	__RIFF_SkipBytes(input_thread_t * p_input,int nb)
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
        intf_WarnMsg( 1, "input demux: cannot seek, it will take times" );
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
        }
    }
	return ( 0 );
}


static void             RIFF_DeleteChunk( input_thread_t *p_input, riffchunk_t *p_chunk )
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

/* ******************************************
 * lit une structure riffchunk sans avancer *
 ********************************************/
static riffchunk_t     * RIFF_ReadChunk(input_thread_t * p_input)
{
    riffchunk_t * p_riff;
    int count;
    byte_t * p_peek;
 
	if((p_riff = malloc( sizeof(riffchunk_t))) == NULL)
	{
		intf_ErrMsg("input error: not enough memory (ioriff)" );
		return NULL;
	}
	
	p_riff->p_data = NULL;  /* Par defaut */
	p_riff->p_next = NULL;
	p_riff->p_subchunk = NULL;
	/* peek to have the begining, 8+4 where 4 are to get type */
	count=input_Peek( p_input, &p_peek, 12 );
	if( count < 8 )
	{
		intf_ErrMsg( "input error: cannot peek() (ioriff)" );
		free(p_riff);
		return NULL;
	}
	
	p_riff->i_id = __GetDoubleWordLittleEndianFromBuff( p_peek );
	p_riff->i_size =__GetDoubleWordLittleEndianFromBuff( p_peek + 4 );
	if( count == 12 )
	{
		p_riff->i_type = __GetDoubleWordLittleEndianFromBuff( p_peek + 8 );
	}
	else
	{
		p_riff->i_type = 0;
	}
	__RIFF_TellPos(p_input, &(p_riff->i_pos) );
	
	return( p_riff );	
}

/**************************************************
 * Va au chunk juste d'apres si il en a encore    *
 * -1 si erreur , 1 si y'en a plus                *
 **************************************************/
static int RIFF_NextChunk( input_thread_t * p_input,riffchunk_t *p_rifffather)
{
    int i_len;
    int i_lenfather;
    riffchunk_t *p_riff;

	if( ( p_riff = RIFF_ReadChunk( p_input ) ) == NULL )
	{
		intf_ErrMsg( "ioriff: cannot read chunk." );
		return( -1 );
	}
	i_len = p_riff->i_size;
    if( i_len%2 != 0 ) {i_len++;} /* aligné sur un mot */

	if ( p_rifffather != NULL )
	{
		i_lenfather=p_rifffather->i_size; 
        if ( i_lenfather%2 !=0 ) {i_lenfather++;}
		if ( p_rifffather->i_pos + i_lenfather <= p_riff->i_pos + i_len )
		{
            intf_ErrMsg( "ioriff: next chunk out of bound" );
			free( p_riff );
			return( 1 ); /* pas dans nos frontiere */
		}
	}
	if ( __RIFF_SkipBytes( p_input,i_len + 8 ) != 0 )
	{ 
		free( p_riff );
		intf_ErrMsg( "input error: cannot go to the next chunk (ioriff)." );
		return( -1 );
	}
	free( p_riff );
	return( 0 );
}

/****************************************************************
 * Permet de rentrer dans un ck RIFF ou LIST                    *
 ****************************************************************/
static int	RIFF_DescendChunk(input_thread_t * p_input)
{
	if ( __RIFF_SkipBytes(p_input,12) != 0)
	{
		intf_ErrMsg( "input error: cannot go into chunk." );
		return ( -1 );
	}
	return( 0 );
}

/***************************************************************
 * Permet de sortir d'un sous chunk et d'aller sur le suivant  *
 * chunk                                                       *
 ***************************************************************/

static int	RIFF_AscendChunk(input_thread_t * p_input ,riffchunk_t *p_rifffather)
{
    int i_skip;
    u32 i_posactu;

	i_skip  = p_rifffather->i_pos + p_rifffather->i_size + 8;
    if ( i_skip%2 != 0) {i_skip++;} 

    __RIFF_TellPos(p_input, &i_posactu);
    i_skip-=i_posactu;

    if (( __RIFF_SkipBytes(p_input,i_skip)) != 0)
	{
		intf_ErrMsg( "ioriff: cannot exit from subchunk.");
		return( -1 );
	}
	return( 0 );
}

/***************************************************************
 * Permet de se deplacer jusqu'au premier chunk avec le bon id *
 * *************************************************************/
static int	RIFF_FindChunk(input_thread_t * p_input ,u32 i_id,riffchunk_t *p_rifffather)
{
 riffchunk_t *p_riff=NULL;
	do
	{
		if (p_riff!=NULL) 
		{ 
			free(p_riff); 
			if ( RIFF_NextChunk(p_input ,p_rifffather) != 0 ) 
            { 
                return( -1 );
            }
		}
		p_riff=RIFF_ReadChunk(p_input);
 	} while ( ( p_riff != NULL )&&( p_riff->i_id != i_id ) );

    if ( ( p_riff == NULL )||( p_riff->i_id != i_id ) )
    { 
        return( -1 );
    }
    free( p_riff );
 	return( 0 );
}

/*****************************************************************
 * Permet de pointer sur la zone de donné du chunk courant       *
 *****************************************************************/
static int               RIFF_GoToChunkData(input_thread_t * p_input)
{
	if ( __RIFF_SkipBytes(p_input,8) != 0 ) 
    { 
        return( -1 );
    }
	return( 0 );
}

static int	RIFF_LoadChunkData(input_thread_t * p_input,riffchunk_t *p_riff )
{
    
	RIFF_GoToChunkData(p_input);
	if ( input_SplitBuffer( p_input, &p_riff->p_data, p_riff->i_size ) != p_riff->i_size )
	{
        intf_ErrMsg( "ioriff: cannot read enough data " );
		return ( -1 );
	}
	if ( p_riff->i_size%2 != 0) 
    {
       __RIFF_SkipBytes(p_input,1);
    } /* aligne sur un mot */
	return( 0 );
}

static int	RIFF_LoadChunkDataInPES(input_thread_t * p_input,
                                    pes_packet_t **pp_pes)
{
    u32 i_read;
    data_packet_t *p_data;
    riffchunk_t   *p_riff;
    
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
    if( p_riff->i_size == 0 )
    {
        p_data = input_NewPacket( p_input->p_method_data, 0 );
        (*pp_pes)->p_first = p_data;
        (*pp_pes)->p_last  = p_data;
        (*pp_pes)->i_nb_data = 1;
        (*pp_pes)->i_pes_size = 0;
        return( 0 );
    }
        
    do
    {
        i_read = input_SplitBuffer(p_input, &p_data, p_riff->i_size - 
                                                    (*pp_pes)->i_pes_size );
        if( i_read < 0 )
        {
            /* FIXME free sur tout les packets */
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
    } while( ((*pp_pes)->i_pes_size < p_riff->i_size)&&(i_read != 0) );
   /* i_read =  0 si fin du stream sinon block */
	if ( p_riff->i_size%2 != 0) 
    {
       __RIFF_SkipBytes(p_input,1);
    } /* aligne sur un mot */
	return( 0 );
}




static int	RIFF_GoToChunk(input_thread_t * p_input, riffchunk_t *p_riff)
{
    /* TODO rajouter les test */
    if( p_input->stream.b_seekable )
    {
        p_input->pf_seek( p_input, (off_t)p_riff->i_pos );
        input_AccessReinit( p_input );
	    return 0;
    }

    return( -1 );
}


static u32   RIFF_4cToI(char c1,char c2,char c3,char c4)
{
 u32 i;
	i = ( ((u32)c1) << 24 ) + ( ((u32)c2) << 16 ) + ( ((u32)c3) << 8 ) + (u32)c4;
	return i;
}


static char	* RIFF_IToStr(u32 l)
{
 char *str;
 int i;
	str=calloc(5,sizeof(char));
	for( i = 0; i < 4; i++)
	{
		str[i] = ( l >> ( (3-i) * 8) )&0xFF;
	}
	str[5] = 0;
	return( str );
}

static int   RIFF_TestFileHeader( input_thread_t * p_input, riffchunk_t ** pp_riff, u32 i_type )
{
    *pp_riff = RIFF_ReadChunk( p_input );
    
    if( *pp_riff == NULL )
    {
        intf_ErrMsg( "input error: cannot retrieve header" );
        return( -1 );
    }
    if( (*pp_riff)->i_id != FOURCC_RIFF ) 
    {
        free( *pp_riff );
        return( -1 );
    }
    if( (*pp_riff)->i_type != i_type )
    {
        free( *pp_riff );
        return( -1 );
    } 
    return( 0 );  
}


static int   RIFF_FindAndLoadChunk( input_thread_t * p_input, riffchunk_t *p_riff, riffchunk_t **pp_fmt, u32 i_type )
{
    *pp_fmt = NULL;
    if ( RIFF_FindChunk( p_input, i_type, p_riff ) != 0)
    {
        return( -1 );
    }
    if ( ( (*pp_fmt = RIFF_ReadChunk( p_input )) == NULL) || ( RIFF_LoadChunkData( p_input, *pp_fmt ) != 0 ) )
    {
        if( *pp_fmt != NULL ) { RIFF_DeleteChunk( p_input, *pp_fmt ); }
        return( -1 );
    }
    return( 0 );
}

static int   RIFF_FindAndGotoDataChunk( input_thread_t * p_input, riffchunk_t *p_riff, riffchunk_t **pp_data, u32 i_type )
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

static int   RIFF_FindListChunk( input_thread_t *p_input, riffchunk_t **pp_riff, riffchunk_t *p_rifffather, u32 i_type )
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
