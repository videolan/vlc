/*****************************************************************************
 * input_programs.c: es_descriptor_t, pgrm_descriptor_t management
 *****************************************************************************
 * Copyright (C) 1999-2002 VideoLAN
 * $Id: input_programs.c,v 1.115 2003/07/23 22:01:25 gbazin Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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
#include <stdlib.h>
#include <string.h>                                    /* memcpy(), memset() */

#include <vlc/vlc.h>

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"
#include "input_ext-plugins.h"

/*
 * NOTICE : all of these functions expect you to have taken the lock on
 * p_input->stream.lock
 */

/* Navigation callbacks */
static int ProgramCallback( vlc_object_t *, char const *,
                            vlc_value_t, vlc_value_t, void * );
static int TitleCallback( vlc_object_t *, char const *,
                          vlc_value_t, vlc_value_t, void * );
static int ChapterCallback( vlc_object_t *, char const *,
                            vlc_value_t, vlc_value_t, void * );
static int NavigationCallback( vlc_object_t *, char const *,
                               vlc_value_t, vlc_value_t, void * );
static int ESCallback( vlc_object_t *, char const *,
                       vlc_value_t, vlc_value_t, void * );

/*****************************************************************************
 * input_InitStream: init the stream descriptor of the given input
 *****************************************************************************/
int input_InitStream( input_thread_t * p_input, size_t i_data_len )
{
    vlc_value_t text,val;

    p_input->stream.i_stream_id = 0;

    /* initialized to 0 since we don't give the signal to the interface
     * before the end of input initialization */
    p_input->stream.b_changed = 0;
    p_input->stream.pp_es = NULL;
    p_input->stream.pp_selected_es = NULL;
    p_input->stream.p_removed_es = NULL;
    p_input->stream.p_newly_selected_es = NULL;
    p_input->stream.pp_programs = NULL;
    p_input->stream.p_selected_program = NULL;
    p_input->stream.p_new_program = NULL;

    if( i_data_len )
    {
        if ( (p_input->stream.p_demux_data = malloc( i_data_len )) == NULL )
        {
            msg_Err( p_input, "out of memory" );
            return 1;
        }
        memset( p_input->stream.p_demux_data, 0, i_data_len );
    }
    else
    {
        p_input->stream.p_demux_data = NULL;
    }
    
    var_Create( p_input, "intf-change", VLC_VAR_BOOL );
    val.b_bool = VLC_TRUE;
    var_Set( p_input, "intf-change", val );

    /* Create a few object variables used for navigation in the interfaces */
    var_Create( p_input, "program", VLC_VAR_INTEGER | VLC_VAR_HASCHOICE );
    text.psz_string = _("Program");
    var_Change( p_input, "program", VLC_VAR_SETTEXT, &text, NULL );

    var_Create( p_input, "title", VLC_VAR_INTEGER | VLC_VAR_HASCHOICE );
    text.psz_string = _("Title");
    var_Change( p_input, "title", VLC_VAR_SETTEXT, &text, NULL );

    var_Create( p_input, "chapter", VLC_VAR_INTEGER | VLC_VAR_HASCHOICE );
    text.psz_string = _("Chapter");
    var_Change( p_input, "chapter", VLC_VAR_SETTEXT, &text, NULL );

    var_Create( p_input, "navigation", VLC_VAR_VARIABLE | VLC_VAR_HASCHOICE );
    text.psz_string = _("Navigation");
    var_Change( p_input, "navigation", VLC_VAR_SETTEXT, &text, NULL );

    var_Create( p_input, "video-es", VLC_VAR_INTEGER | VLC_VAR_HASCHOICE );
    text.psz_string = _("Video track");
    var_Change( p_input, "video-es", VLC_VAR_SETTEXT, &text, NULL );
    var_Create( p_input, "audio-es", VLC_VAR_INTEGER | VLC_VAR_HASCHOICE );
    text.psz_string = _("Audio track");
    var_Change( p_input, "audio-es", VLC_VAR_SETTEXT, &text, NULL );
    var_Create( p_input, "spu-es", VLC_VAR_INTEGER | VLC_VAR_HASCHOICE );
    text.psz_string = _("Subtitles track");
    var_Change( p_input, "spu-es", VLC_VAR_SETTEXT, &text, NULL );

    var_AddCallback( p_input, "program", ProgramCallback, NULL );
    var_AddCallback( p_input, "title", TitleCallback, NULL );
    var_AddCallback( p_input, "chapter", ChapterCallback, NULL );
    var_AddCallback( p_input, "video-es", ESCallback, NULL );
    var_AddCallback( p_input, "audio-es", ESCallback, NULL );
    var_AddCallback( p_input, "spu-es", ESCallback, NULL );

    return 0;
}

/*****************************************************************************
 * input_EndStream: free all stream descriptors
 *****************************************************************************/
void input_EndStream( input_thread_t * p_input )
{
    /* Free all programs and associated ES, and associated decoders. */
    while( p_input->stream.i_pgrm_number )
    {
        input_DelProgram( p_input, p_input->stream.pp_programs[0] );
    }

    /* Free standalone ES */
    while( p_input->stream.i_es_number )
    {
        input_DelES( p_input, p_input->stream.pp_es[0] );
    }

    /* Free all areas */
    while( p_input->stream.i_area_nb )
    {
        input_DelArea( p_input, p_input->stream.pp_areas[0] );
    }

    /* Free selected ES */
    if( p_input->stream.pp_selected_es != NULL )
    {
        free( p_input->stream.pp_selected_es );
    }

    if( p_input->stream.p_demux_data != NULL )
    {
        free( p_input->stream.p_demux_data );
    }

    /* Free navigation variables */
    var_Destroy( p_input, "program" );
    var_Destroy( p_input, "title" );
    var_Destroy( p_input, "chapter" );
    var_Destroy( p_input, "video-es" );
    var_Destroy( p_input, "audio-es" );
    var_Destroy( p_input, "spu-es" );
    var_Destroy( p_input, "intf-change" );
}

/*****************************************************************************
 * input_FindProgram: returns a pointer to a program described by its ID
 *****************************************************************************/
pgrm_descriptor_t * input_FindProgram( input_thread_t * p_input,
                                       uint16_t i_pgrm_id )
{
    unsigned int i;

    for( i = 0; i < p_input->stream.i_pgrm_number; i++ )
    {
        if( p_input->stream.pp_programs[i]->i_number == i_pgrm_id )
        {
            return p_input->stream.pp_programs[i];
        }
    }

    return NULL;
}

/*****************************************************************************
 * input_AddProgram: add and init a program descriptor
 *****************************************************************************
 * This program descriptor will be referenced in the given stream descriptor
 *****************************************************************************/
pgrm_descriptor_t * input_AddProgram( input_thread_t * p_input,
                                      u16 i_pgrm_id, size_t i_data_len )
{
    /* Where to add the pgrm */
    pgrm_descriptor_t * p_pgrm = malloc( sizeof(pgrm_descriptor_t) );
    vlc_value_t val;

    if( p_pgrm == NULL )
    {
        msg_Err( p_input, "out of memory" );
        return NULL;
    }

    /* Init this entry */
    p_pgrm->i_number = i_pgrm_id;
    p_pgrm->b_is_ok = 0;
    p_pgrm->i_version = 0;

    p_pgrm->i_es_number = 0;
    p_pgrm->pp_es = NULL;

    input_ClockInit( p_pgrm );

    p_pgrm->i_synchro_state = SYNCHRO_START;

    if( i_data_len )
    {
        p_pgrm->p_demux_data = malloc( i_data_len );
        if( p_pgrm->p_demux_data == NULL )
        {
            msg_Err( p_input, "out of memory" );
            return NULL;
        }
        memset( p_pgrm->p_demux_data, 0, i_data_len );
    }
    else
    {
        p_pgrm->p_demux_data = NULL;
    }

    /* Add an entry to the list of program associated with the stream */
    INSERT_ELEM( p_input->stream.pp_programs,
                 p_input->stream.i_pgrm_number,
                 p_input->stream.i_pgrm_number,
                 p_pgrm );

    val.i_int = i_pgrm_id;
    var_Change( p_input, "program", VLC_VAR_ADDCHOICE, &val, NULL );

    return p_pgrm;
}

/*****************************************************************************
 * input_DelProgram: destroy a program descriptor
 *****************************************************************************
 * All ES descriptions referenced in the descriptor will be deleted.
 *****************************************************************************/
void input_DelProgram( input_thread_t * p_input, pgrm_descriptor_t * p_pgrm )
{
    unsigned int i_pgrm_index;
    vlc_value_t val;

    /* Find the program in the programs table */
    for( i_pgrm_index = 0; i_pgrm_index < p_input->stream.i_pgrm_number;
         i_pgrm_index++ )
    {
        if( p_input->stream.pp_programs[i_pgrm_index] == p_pgrm )
            break;
    }

    /* If the program wasn't found, do nothing */
    if( i_pgrm_index == p_input->stream.i_pgrm_number )
    {
        msg_Err( p_input, "program does not belong to this input" );
        return;
    }

    val.i_int = i_pgrm_index;
    var_Change( p_input, "program", VLC_VAR_DELCHOICE, &val, NULL );

    /* Free the structures that describe the es that belongs to that program */
    while( p_pgrm->i_es_number )
    {
        input_DelES( p_input, p_pgrm->pp_es[0] );
    }

    /* Free the demux data */
    if( p_pgrm->p_demux_data != NULL )
    {
        free( p_pgrm->p_demux_data );
    }

    /* Remove this program from the stream's list of programs */
    REMOVE_ELEM( p_input->stream.pp_programs,
                 p_input->stream.i_pgrm_number,
                 i_pgrm_index );

    /* Free the description of this program */
    free( p_pgrm );
}

/*****************************************************************************
 * input_AddArea: add and init an area descriptor
 *****************************************************************************
 * This area descriptor will be referenced in the given stream descriptor
 *****************************************************************************/
input_area_t * input_AddArea( input_thread_t * p_input,
                              uint16_t i_area_id, uint16_t i_part_nb )
{
    /* Where to add the pgrm */
    input_area_t * p_area = malloc( sizeof(input_area_t) );
    vlc_value_t val;
    int i;

    if( p_area == NULL )
    {
        msg_Err( p_input, "out of memory" );
        return NULL;
    }

    /* Init this entry */
    p_area->i_id = i_area_id;
    p_area->i_part_nb = i_part_nb;
    p_area->i_part= 0;
    p_area->i_start = 0;
    p_area->i_size = 0;
    p_area->i_tell = 0;
    p_area->i_seek = NO_SEEK;

    /* Add an entry to the list of program associated with the stream */
    INSERT_ELEM( p_input->stream.pp_areas,
                 p_input->stream.i_area_nb,
                 p_input->stream.i_area_nb,
                 p_area );

    /* Don't add empty areas */
    if( i_part_nb == 0 )
        return NULL;

    /* Take care of the navigation variables */
    val.i_int = i_area_id;
    var_Change( p_input, "title", VLC_VAR_ADDCHOICE, &val, NULL );

    val.psz_string = malloc( sizeof("title ") + 5 );
    if( val.psz_string )
    {
        vlc_value_t val2, text, text2;

        sprintf( val.psz_string, "title %2i", i_area_id );
        var_Destroy( p_input, val.psz_string );
        var_Create( p_input, val.psz_string, VLC_VAR_INTEGER |
                    VLC_VAR_HASCHOICE | VLC_VAR_ISCOMMAND );
        var_AddCallback( p_input, val.psz_string, NavigationCallback,
                         (void *)(int)i_area_id );

        text.psz_string = malloc( strlen( _("Title %i") ) + 20 );
        if( text.psz_string )
            sprintf( text.psz_string, _("Title %i"), i_area_id );

        var_Change( p_input, "navigation", VLC_VAR_ADDCHOICE, &val, &text );

        if( text.psz_string ) free( text.psz_string );

        text2.psz_string = malloc( strlen( _("Chapter %i") ) + 20 );

        for( i = 1; i <= i_part_nb; i++ )
        {
            val2.i_int = i;

            if( text2.psz_string )
                sprintf( text2.psz_string, _("Chapter %i"), i );

            var_Change( p_input, val.psz_string,
                        VLC_VAR_ADDCHOICE, &val2, &text2 );
        }

        if( text2.psz_string ) free( text2.psz_string );
    }

    if( p_input->stream.i_area_nb == 2 )
    {
        vlc_value_t text;

        /* Add another bunch of navigation object variables */
        var_Create( p_input, "next-title", VLC_VAR_VOID );
        text.psz_string = _("Next title");
        var_Change( p_input, "next-title", VLC_VAR_SETTEXT, &text, NULL );
        var_Create( p_input, "prev-title", VLC_VAR_VOID );
        text.psz_string = _("Previous title");
        var_Change( p_input, "prev-title", VLC_VAR_SETTEXT, &text, NULL );
        var_AddCallback( p_input, "next-title", TitleCallback, NULL );
        var_AddCallback( p_input, "prev-title", TitleCallback, NULL );

        var_Create( p_input, "next-chapter", VLC_VAR_VOID );
        text.psz_string = _("Next Chapter");
        var_Change( p_input, "next-chapter", VLC_VAR_SETTEXT, &text, NULL );
        var_Create( p_input, "prev-chapter", VLC_VAR_VOID );
        text.psz_string = _("Previous Chapter");
        var_Change( p_input, "prev-chapter", VLC_VAR_SETTEXT, &text, NULL );
        var_AddCallback( p_input, "next-chapter", ChapterCallback, NULL );
        var_AddCallback( p_input, "prev-chapter", ChapterCallback, NULL );
    }

    return p_area;
}

/*****************************************************************************
 * input_SetProgram: changes the current program
 *****************************************************************************/
int input_SetProgram( input_thread_t * p_input, pgrm_descriptor_t * p_new_prg )
{
    unsigned int i_es_index;
    int i_required_audio_es;
    int i_required_spu_es;
    int i_audio_es = 0;
    int i_spu_es = 0;
    vlc_value_t val;

    if ( p_input->stream.p_selected_program )
    {
        for ( i_es_index = 1 ; /* 0 should be the PMT */
                i_es_index < p_input->stream.p_selected_program->
                i_es_number ;
                i_es_index ++ )
        {
#define p_es p_input->stream.p_selected_program->pp_es[i_es_index]
            if ( p_es->p_decoder_fifo ) /* if the ES was selected */
            {
                input_UnselectES( p_input , p_es );
            }
#undef p_es
        }
    }

    /* Get the number of the required audio stream */
    var_Get( p_input, "audio", &val );
    if( val.b_bool )
    {
        /* Default is the first one */
        var_Get( p_input, "audio-channel", &val );
        i_required_audio_es = val.i_int;
        if( i_required_audio_es < 0 )
        {
            i_required_audio_es = 1;
        }
    }
    else
    {
        i_required_audio_es = 0;
    }

    /* Same thing for subtitles */
    var_Get( p_input, "video", &val );
    if( val.b_bool )
    {
        /* for spu, default is none */
        var_Get( p_input, "spu-channel", &val );
        i_required_spu_es = val.i_int;
        if( i_required_spu_es < 0 )
        {
            i_required_spu_es = 0;
        }
    }
    else
    {
        i_required_spu_es = 0;
    }

    for( i_es_index = 0 ; i_es_index < p_new_prg->i_es_number ; i_es_index ++ )
    {
        switch( p_new_prg->pp_es[i_es_index]->i_cat )
        {
            case VIDEO_ES:
                msg_Dbg( p_input, "selecting ES %x",
                         p_new_prg->pp_es[i_es_index]->i_id );
                input_SelectES( p_input, p_new_prg->pp_es[i_es_index] );
                break;
            case AUDIO_ES:
                i_audio_es += 1;
                if( i_audio_es <= i_required_audio_es )
                {
                    msg_Dbg( p_input, "selecting ES %x",
                             p_new_prg->pp_es[i_es_index]->i_id );
                    input_SelectES( p_input, p_new_prg->pp_es[i_es_index]);
                }
                break;
            /* Not sure this one is fully specification-compliant */
            case SPU_ES :
                i_spu_es += 1;
                if( i_spu_es <= i_required_spu_es )
                {
                    msg_Dbg( p_input, "selecting ES %x",
                             p_new_prg->pp_es[i_es_index]->i_id );
                    input_SelectES( p_input, p_new_prg->pp_es[i_es_index] );
                }
            break;
            default :
                msg_Dbg( p_input, "ES %x has unknown type",
                         p_new_prg->pp_es[i_es_index]->i_id );
                break;
        }

    }


    p_input->stream.p_selected_program = p_new_prg;

    /* Update the navigation variables without triggering a callback */
    val.i_int = p_new_prg->i_number;
    var_Change( p_input, "program", VLC_VAR_SETVALUE, &val, NULL );

    return( 0 );
}

/*****************************************************************************
 * input_DelArea: destroy a area descriptor
 *****************************************************************************
 * All ES descriptions referenced in the descriptor will be deleted.
 *****************************************************************************/
void input_DelArea( input_thread_t * p_input, input_area_t * p_area )
{
    unsigned int i_area_index;
    vlc_value_t val;

    /* Find the area in the areas table */
    for( i_area_index = 0; i_area_index < p_input->stream.i_area_nb;
         i_area_index++ )
    {
        if( p_input->stream.pp_areas[i_area_index] == p_area )
            break;
    }

    /* If the area wasn't found, do nothing */
    if( i_area_index == p_input->stream.i_area_nb )
    {
        msg_Err( p_input, "area does not belong to this input" );
        return;
    }

    /* Take care of the navigation variables */
    val.psz_string = malloc( sizeof("title ") + 5 );
    if( val.psz_string )
    {
        sprintf( val.psz_string, "title %i", p_area->i_id );
        var_Change( p_input, "navigation", VLC_VAR_DELCHOICE, &val, NULL );
        var_Destroy( p_input, val.psz_string );
    }

    /* Remove this area from the stream's list of areas */
    REMOVE_ELEM( p_input->stream.pp_areas,
                 p_input->stream.i_area_nb,
                 i_area_index );

    /* Free the description of this area */
    free( p_area );

    if( p_input->stream.i_area_nb == 1 )
    {
        /* Del unneeded navigation object variables */
        var_Destroy( p_input, "next-title" );
        var_Destroy( p_input, "prev-title" );
        var_Destroy( p_input, "next-chapter" );
        var_Destroy( p_input, "prev-chapter" );
    }
}


/*****************************************************************************
 * input_FindES: returns a pointer to an ES described by its ID
 *****************************************************************************/
es_descriptor_t * input_FindES( input_thread_t * p_input, uint16_t i_es_id )
{
    unsigned int i;

    for( i = 0; i < p_input->stream.i_es_number; i++ )
    {
        if( p_input->stream.pp_es[i]->i_id == i_es_id )
        {
            return p_input->stream.pp_es[i];
        }
    }

    return NULL;
}

/*****************************************************************************
 * input_AddES:
 *****************************************************************************
 * Reserve a slot in the table of ES descriptors for the ES and add it to the
 * list of ES of p_pgrm. If p_pgrm if NULL, then the ES is considered as stand
 * alone (PSI ?)
 *****************************************************************************/
es_descriptor_t * input_AddES( input_thread_t * p_input,
                               pgrm_descriptor_t * p_pgrm, u16 i_es_id,
                               int i_category, char const *psz_desc,
                               size_t i_data_len )
{
    es_descriptor_t * p_es;
    vlc_value_t val, text;
    char *psz_var = NULL;

    p_es = (es_descriptor_t *)malloc( sizeof(es_descriptor_t) );
    if( p_es == NULL )
    {
        msg_Err( p_input, "out of memory" );
        return( NULL);
    }

    INSERT_ELEM( p_input->stream.pp_es,
                 p_input->stream.i_es_number,
                 p_input->stream.i_es_number,
                 p_es );

    /* Init its values */
    p_es->i_id = i_es_id;
    p_es->p_pes = NULL;
    p_es->p_decoder_fifo = NULL;
    p_es->i_cat = i_category;
    p_es->i_demux_fd = 0;
    p_es->c_packets = 0;
    p_es->c_invalid_packets = 0;
    p_es->b_force_decoder = VLC_FALSE;

    if( i_data_len )
    {
        p_es->p_demux_data = malloc( i_data_len );
        if( p_es->p_demux_data == NULL )
        {
            msg_Err( p_input, "out of memory" );
            return( NULL );
        }
        memset( p_es->p_demux_data, 0, i_data_len );
    }
    else
    {
        p_es->p_demux_data = NULL;
    }
    p_es->p_waveformatex     = NULL;
    p_es->p_bitmapinfoheader = NULL;

    /* Add this ES to the program definition if one is given */
    if( p_pgrm )
    {
        INSERT_ELEM( p_pgrm->pp_es,
                     p_pgrm->i_es_number,
                     p_pgrm->i_es_number,
                     p_es );
        p_es->p_pgrm = p_pgrm;
    }
    else
    {
        p_es->p_pgrm = NULL;
    }

    switch( i_category )
    {
    case AUDIO_ES:
        psz_var = "audio-es";
        break;
    case SPU_ES:
        psz_var = "spu-es";
        break;
    case VIDEO_ES:
        psz_var = "video-es";
        break;
    }

    if( psz_var )
    {
        /* Get the number of ES already added */
        var_Change( p_input, psz_var, VLC_VAR_CHOICESCOUNT, &val, NULL );
        if( val.i_int == 0 )
        {
            vlc_value_t val2;

            /* First one, we need to add the "Disable" choice */
            val2.i_int = -1; text.psz_string = _("Disable");
            var_Change( p_input, psz_var, VLC_VAR_ADDCHOICE, &val2, &text );
            val.i_int++;
        }

        /* Take care of the ES description */
        if( psz_desc )
        {
            p_es->psz_desc = strdup( psz_desc );
        }
        else
        {
            p_es->psz_desc = malloc( strlen( _("Track %i") ) + 20 );
            if( p_es->psz_desc )
                sprintf( p_es->psz_desc, _("Track %i"), val.i_int );
        }

        val.i_int = p_es->i_id;
        text.psz_string = p_es->psz_desc;
        var_Change( p_input, psz_var, VLC_VAR_ADDCHOICE, &val, &text );
    }
    else p_es->psz_desc = NULL;

    return p_es;
}

/*****************************************************************************
 * input_DelES:
 *****************************************************************************/
void input_DelES( input_thread_t * p_input, es_descriptor_t * p_es )
{
    unsigned int            i_index, i_es_index;
    pgrm_descriptor_t *     p_pgrm;
    char *                  psz_var = NULL;
    vlc_value_t             val;

    /* Find the ES in the ES table */
    for( i_es_index = 0; i_es_index < p_input->stream.i_es_number;
         i_es_index++ )
    {
        if( p_input->stream.pp_es[i_es_index] == p_es )
            break;
    }

    /* If the ES wasn't found, do nothing */
    if( i_es_index == p_input->stream.i_es_number )
    {
        msg_Err( p_input, "ES does not belong to this input" );
        return;
    }

    /* Remove es from its associated variable */
    switch( p_es->i_cat )
    {
    case AUDIO_ES:
        psz_var = "audio-es";
        break;
    case SPU_ES:
        psz_var = "spu-es";
        break;
    case VIDEO_ES:
        psz_var = "video-es";
        break;
    }

    if( psz_var )
    {
        val.i_int = p_es->i_id;
        var_Change( p_input, psz_var, VLC_VAR_DELCHOICE, &val, NULL );

        /* Remove the "Disable" entry if needed */
        var_Change( p_input, psz_var, VLC_VAR_CHOICESCOUNT, &val, NULL );
        if( val.i_int == 1 )
        {
            val.i_int = -1;
            var_Change( p_input, psz_var, VLC_VAR_DELCHOICE, &val, NULL );
        }
    }

    /* Kill associated decoder, if any. */
    if( p_es->p_decoder_fifo != NULL )
    {
        input_UnselectES( p_input, p_es );
    }

    /* Remove this ES from the description of the program if it is associated
     * to one */
    p_pgrm = p_es->p_pgrm;
    if( p_pgrm )
    {
        for( i_index = 0; i_index < p_pgrm->i_es_number; i_index++ )
        {
            if( p_pgrm->pp_es[i_index] == p_es )
            {
                REMOVE_ELEM( p_pgrm->pp_es,
                             p_pgrm->i_es_number,
                             i_index );
                break;
            }
        }
    }

    /* Free the demux data */
    if( p_es->p_demux_data != NULL )
    {
        free( p_es->p_demux_data );
    }
    if( p_es->p_waveformatex )
    {
        free( p_es->p_waveformatex );
    }
    if( p_es->p_bitmapinfoheader )
    {
        free( p_es->p_bitmapinfoheader );
    }

    /* Free the description string */
    if( p_es->psz_desc != NULL )
    {
        free( p_es->psz_desc );
    }

    /* Find the ES in the ES table */
    for( i_es_index = 0; i_es_index < p_input->stream.i_es_number;
         i_es_index++ )
    {
        if( p_input->stream.pp_es[i_es_index] == p_es )
            break;
    }

    /* Remove this ES from the stream's list of ES */
    REMOVE_ELEM( p_input->stream.pp_es,
                 p_input->stream.i_es_number,
                 i_es_index );

    /* Free the ES */
    free( p_es );
}

/*****************************************************************************
 * input_SelectES: selects an ES and spawns the associated decoder
 *****************************************************************************
 * Remember we are still supposed to have stream_lock when entering this
 * function ?
 *****************************************************************************/
int input_SelectES( input_thread_t * p_input, es_descriptor_t * p_es )
{
    vlc_value_t val;
    char *psz_var = NULL;

    if( p_es == NULL )
    {
        msg_Err( p_input, "nothing to do in input_SelectES" );
        return -1;
    }

    if( p_es->i_cat == VIDEO_ES || p_es->i_cat == SPU_ES )
    {
        var_Get( p_input, "video", &val );
        if( !val.b_bool )
        {
            msg_Dbg( p_input, "video is disabled, not selecting ES 0x%x",
                     p_es->i_id );
            return -1;
        }
    }

    if( p_es->i_cat == AUDIO_ES )
    {
        var_Get( p_input, "audio", &val );
        if( !val.b_bool )
        {
            msg_Dbg( p_input, "audio is disabled, not selecting ES 0x%x",
                     p_es->i_id );
            return -1;
        }
    }

    msg_Dbg( p_input, "selecting ES 0x%x", p_es->i_id );

    if( p_es->p_decoder_fifo != NULL )
    {
        msg_Err( p_input, "ES 0x%x is already selected", p_es->i_id );
        return -1;
    }

    /* Release the lock, not to block the input thread during
     * the creation of the thread. */
    vlc_mutex_unlock( &p_input->stream.stream_lock );
    p_es->p_decoder_fifo = input_RunDecoder( p_input, p_es );
    vlc_mutex_lock( &p_input->stream.stream_lock );

    if( p_es->p_decoder_fifo == NULL )
    {
        return -1;
    }

    /* Update the es variable without triggering a callback */
    switch( p_es->i_cat )
    {
    case AUDIO_ES:
        psz_var = "audio-es";
        break;
    case SPU_ES:
        psz_var = "spu-es";
        break;
    case VIDEO_ES:
        psz_var = "video-es";
        break;
    }

    if( psz_var )
    {
        val.i_int = p_es->i_id;
        var_Change( p_input, psz_var, VLC_VAR_SETVALUE, &val, NULL );
    }

    return 0;
}

/*****************************************************************************
 * input_UnselectES: removes an ES from the list of selected ES
 *****************************************************************************/
int input_UnselectES( input_thread_t * p_input, es_descriptor_t * p_es )
{
    unsigned int i_index = 0;
    vlc_value_t val;
    char *psz_var = NULL;

    if( p_es == NULL )
    {
        msg_Err( p_input, "nothing to do in input_UnselectES" );
        return -1;
    }

    msg_Dbg( p_input, "unselecting ES 0x%x", p_es->i_id );

    if( p_es->p_decoder_fifo == NULL )
    {
        msg_Err( p_input, "ES 0x%x is not selected", p_es->i_id );
        return( -1 );
    }

    /* Update the es variable without triggering a callback */
    switch( p_es->i_cat )
    {
    case AUDIO_ES:
        psz_var = "audio-es";
        break;
    case SPU_ES:
        psz_var = "spu-es";
        break;
    case VIDEO_ES:
        psz_var = "video-es";
        break;
    }

    if( psz_var )
    {
        val.i_int = -1;
        var_Change( p_input, psz_var, VLC_VAR_SETVALUE, &val, NULL );
    }

    /* Actually unselect the ES */
    input_EndDecoder( p_input, p_es );
    p_es->p_pes = NULL;

    if( ( p_es->p_decoder_fifo == NULL ) &&
        ( p_input->stream.i_selected_es_number > 0 ) )
    {
        while( ( i_index < p_input->stream.i_selected_es_number - 1 ) &&
               ( p_input->stream.pp_selected_es[i_index] != p_es ) )
        {
            i_index++;
        }

        /* XXX: no need to memmove, we have unsorted data */
        REMOVE_ELEM( p_input->stream.pp_selected_es,
                     p_input->stream.i_selected_es_number,
                     i_index );

        if( p_input->stream.i_selected_es_number == 0 )
        {
            msg_Dbg( p_input, "no more selected ES" );
            return 1;
        }
    }

    return 0;
}

/*****************************************************************************
 * Navigation callback: a bunch of navigation variables are used as an
 *  alternative to the navigation API.
 *****************************************************************************/
static int ProgramCallback( vlc_object_t *p_this, char const *psz_cmd,
                  vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    vlc_value_t val;

    if( oldval.i_int == newval.i_int )
       return VLC_SUCCESS;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( ( newval.i_int > 0 ) )
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        input_ChangeProgram( p_input, (uint16_t)newval.i_int );
        input_SetStatus( p_input, INPUT_STATUS_PLAY );
        vlc_mutex_lock( &p_input->stream.stream_lock );
    }
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    val.b_bool = VLC_TRUE;
    var_Set( p_input, "intf-change", val );

    return VLC_SUCCESS;
}

static int TitleCallback( vlc_object_t *p_this, char const *psz_cmd,
                  vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    input_area_t *p_area;
    vlc_value_t val, val_list;
    int i, i_step = 0;

    if( !strcmp( psz_cmd, "next-title" ) ) i_step++;
    else if( !strcmp( psz_cmd, "prev-title" ) ) i_step--;

    if( !i_step && oldval.i_int == newval.i_int ) return VLC_SUCCESS;

    /* Sanity check should have already been done by var_Set(). */
    vlc_mutex_lock( &p_input->stream.stream_lock );

    if( i_step )
    {
        var_Get( p_this, "title", &newval );
        var_Change( p_this, "title", VLC_VAR_GETCHOICES, &val_list, NULL );
        for( i = 0; i < val_list.p_list->i_count; i++ )
        {
            if( val_list.p_list->p_values[i].i_int == newval.i_int &&
                i + i_step >= 0 && i + i_step < val_list.p_list->i_count )
            {
                newval.i_int = val_list.p_list->p_values[i + i_step].i_int;
                break;
            }
        }
        var_Change( p_this, "title", VLC_VAR_FREELIST, &val_list, NULL );
    }

    p_area = p_input->stream.pp_areas[newval.i_int];
    p_area->i_part = 1;

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    input_ChangeArea( p_input, p_area );
    input_SetStatus( p_input, INPUT_STATUS_PLAY );

    val.b_bool = VLC_TRUE;
    var_Set( p_input, "intf-change", val );

    return VLC_SUCCESS;
}

static int ChapterCallback( vlc_object_t *p_this, char const *psz_cmd,
                  vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    input_area_t *p_area;
    vlc_value_t val, val_list;
    int i, i_step = 0;

    if( !strcmp( psz_cmd, "next-chapter" ) ) i_step++;
    else if( !strcmp( psz_cmd, "prev-chapter" ) ) i_step--;

    if( !i_step && oldval.i_int == newval.i_int ) return VLC_SUCCESS;

    /* Sanity check should have already been done by var_Set(). */
    vlc_mutex_lock( &p_input->stream.stream_lock );

    if( i_step )
    {
        var_Get( p_this, "chapter", &newval );
        var_Change( p_this, "chapter", VLC_VAR_GETCHOICES, &val_list, NULL );
        for( i = 0; i < val_list.p_list->i_count; i++ )
        {
            if( val_list.p_list->p_values[i].i_int == newval.i_int &&
                i + i_step >= 0 && i + i_step < val_list.p_list->i_count )
            {
                newval.i_int = val_list.p_list->p_values[i + i_step].i_int;
                break;
            }
        }
        var_Change( p_this, "chapter", VLC_VAR_FREELIST, &val_list, NULL );
    }

    p_area = p_input->stream.p_selected_area;
    p_input->stream.p_selected_area->i_part = newval.i_int;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    input_ChangeArea( p_input, p_area );
    input_SetStatus( p_input, INPUT_STATUS_PLAY );

    val.b_bool = VLC_TRUE;
    var_Set( p_input, "intf-change", val );

    return VLC_SUCCESS;
}

static int NavigationCallback( vlc_object_t *p_this, char const *psz_cmd,
                  vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    uint16_t i_area_id = (int)p_data;
    vlc_value_t val;

    vlc_mutex_lock( &p_input->stream.stream_lock );

    if( p_input->stream.p_selected_area->i_id == i_area_id &&
        oldval.i_int == newval.i_int )
    {
        /* Nothing to do */
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        return VLC_SUCCESS;
    }

    if( ( i_area_id < p_input->stream.i_area_nb ) && ( newval.i_int > 0 ) &&
        ( (uint16_t)newval.i_int <=
          p_input->stream.pp_areas[i_area_id]->i_part_nb ) )
    {
        input_area_t *p_area = p_input->stream.pp_areas[i_area_id];
        p_area->i_part = newval.i_int;
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        input_ChangeArea( p_input, p_area );
        input_SetStatus( p_input, INPUT_STATUS_PLAY );
        vlc_mutex_lock( &p_input->stream.stream_lock );
    }
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    val.b_bool = VLC_TRUE;
    var_Set( p_input, "intf-change", val );

    return VLC_SUCCESS;
}

static int ESCallback( vlc_object_t *p_this, char const *psz_cmd,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    unsigned int i;
    vlc_value_t val;

    vlc_mutex_lock( &p_input->stream.stream_lock );

    /* Unselect old ES */
    for( i = 0 ; i < p_input->stream.i_es_number ; i++ )
    {
        if( p_input->stream.pp_es[i]->i_id == oldval.i_int &&
            p_input->stream.pp_es[i]->p_decoder_fifo != NULL )
        {
            input_UnselectES( p_input, p_input->stream.pp_es[i] );
        }
    }

    /* Select new ES */
    for( i = 0 ; i < p_input->stream.i_es_number ; i++ )
    {
        if( p_input->stream.pp_es[i]->i_id == newval.i_int &&
            p_input->stream.pp_es[i]->p_decoder_fifo == NULL )
        {
            input_SelectES( p_input, p_input->stream.pp_es[i] );
        }
    }

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    val.b_bool = VLC_TRUE;
    var_Set( p_input, "intf-change", val );

    return VLC_SUCCESS;
}
