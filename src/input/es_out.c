/*****************************************************************************
 * es_out.c: Es Out handler for input.
 *****************************************************************************
 * Copyright (C) 2003-2004 VideoLAN
 * $Id$
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
#include <stdlib.h>

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/decoder.h>

#include "input_internal.h"

#include "vlc_playlist.h"
#include "iso_lang.h"
/* FIXME we should find a better way than including that */
#include "../misc/iso-639_def.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
typedef struct
{
    /* Program ID */
    int i_id;

    /* Number of es for this pgrm */
    int i_es;

    vlc_bool_t b_selected;

    /* Clock for this program */
    input_clock_t clock;

} es_out_pgrm_t;

struct es_out_id_t
{
    /* ES ID */
    int       i_id;
    es_out_pgrm_t *p_pgrm;

    /* */
    int64_t i_preroll_end;

    /* Channel in the track type */
    int         i_channel;
    es_format_t fmt;
    char        *psz_language;
    char        *psz_language_code;
    decoder_t   *p_dec;
};

struct es_out_sys_t
{
    input_thread_t *p_input;

    /* all programs */
    int           i_pgrm;
    es_out_pgrm_t **pgrm;
    es_out_pgrm_t **pp_selected_pgrm; /* --programs */
    es_out_pgrm_t *p_pgrm;  /* Master program */

    /* all es */
    int         i_id;
    int         i_es;
    es_out_id_t **es;

    /* mode gestion */
    vlc_bool_t  b_active;
    int         i_mode;

    /* es count */
    int         i_audio;
    int         i_video;
    int         i_sub;

    /* es to select */
    int         i_audio_last;
    int         i_sub_last;
    char        **ppsz_audio_language;
    char        **ppsz_sub_language;

    /* current main es */
    es_out_id_t *p_es_audio;
    es_out_id_t *p_es_video;
    es_out_id_t *p_es_sub;

    /* delay */
    int64_t i_audio_delay;
    int64_t i_spu_delay;
};

static es_out_id_t *EsOutAdd    ( es_out_t *, es_format_t * );
static int          EsOutSend   ( es_out_t *, es_out_id_t *, block_t * );
static void         EsOutDel    ( es_out_t *, es_out_id_t * );
static void         EsOutSelect( es_out_t *out, es_out_id_t *es, vlc_bool_t b_force );
static int          EsOutControl( es_out_t *, int i_query, va_list );

static void         EsOutAddInfo( es_out_t *, es_out_id_t *es );

static void EsSelect( es_out_t *out, es_out_id_t *es );
static void EsUnselect( es_out_t *out, es_out_id_t *es, vlc_bool_t b_update );
static char *LanguageGetName( const char *psz_code );
static char *LanguageGetCode( const char *psz_lang );
static char **LanguageSplit( const char *psz_langs );
static int LanguageArrayIndex( char **ppsz_langs, char *psz_lang );

/*****************************************************************************
 * input_EsOutNew:
 *****************************************************************************/
es_out_t *input_EsOutNew( input_thread_t *p_input )
{
    es_out_t     *out = malloc( sizeof( es_out_t ) );
    es_out_sys_t *p_sys = malloc( sizeof( es_out_sys_t ) );
    vlc_value_t  val;
    int i;

    out->pf_add     = EsOutAdd;
    out->pf_send    = EsOutSend;
    out->pf_del     = EsOutDel;
    out->pf_control = EsOutControl;
    out->p_sys      = p_sys;

    p_sys->p_input = p_input;

    p_sys->b_active = VLC_FALSE;
    p_sys->i_mode   = ES_OUT_MODE_AUTO;


    p_sys->i_pgrm   = 0;
    p_sys->pgrm     = NULL;
    p_sys->p_pgrm   = NULL;

    p_sys->i_id    = 0;
    p_sys->i_es    = 0;
    p_sys->es      = NULL;

    p_sys->i_audio = 0;
    p_sys->i_video = 0;
    p_sys->i_sub   = 0;

    /* */
    var_Get( p_input, "audio-track", &val );
    p_sys->i_audio_last = val.i_int;

    var_Get( p_input, "sub-track", &val );
    p_sys->i_sub_last = val.i_int;

    var_Get( p_input, "audio-language", &val );
    p_sys->ppsz_audio_language = LanguageSplit(val.psz_string);
    if( p_sys->ppsz_audio_language )
    {
        for( i = 0; p_sys->ppsz_audio_language[i]; i++ )
            msg_Dbg( p_input, "Select audio in language[%d] %s",
                     i, p_sys->ppsz_audio_language[i] );
    }
    if( val.psz_string ) free( val.psz_string );

    var_Get( p_input, "sub-language", &val );
    p_sys->ppsz_sub_language = LanguageSplit(val.psz_string);
    if( p_sys->ppsz_sub_language )
    {
        for( i = 0; p_sys->ppsz_sub_language[i]; i++ )
            msg_Dbg( p_input, "Select subtitle in language[%d] %s",
                     i, p_sys->ppsz_sub_language[i] );
    }
    if( val.psz_string ) free( val.psz_string );

    p_sys->p_es_audio = NULL;
    p_sys->p_es_video = NULL;
    p_sys->p_es_sub   = NULL;

    p_sys->i_audio_delay= 0;
    p_sys->i_spu_delay  = 0;

    return out;
}

/*****************************************************************************
 * input_EsOutDelete:
 *****************************************************************************/
void input_EsOutDelete( es_out_t *out )
{
    es_out_sys_t *p_sys = out->p_sys;
    int i;

    for( i = 0; i < p_sys->i_es; i++ )
    {
        if( p_sys->es[i]->p_dec )
        {
            input_DecoderDelete( p_sys->es[i]->p_dec );
        }
        if( p_sys->es[i]->psz_language )
            free( p_sys->es[i]->psz_language );
        if( p_sys->es[i]->psz_language_code )
            free( p_sys->es[i]->psz_language_code );
        es_format_Clean( &p_sys->es[i]->fmt );

        free( p_sys->es[i] );
    }
    if( p_sys->ppsz_audio_language )
    {
        for( i = 0; p_sys->ppsz_audio_language[i]; i++ )
            free( p_sys->ppsz_audio_language[i] );
        free( p_sys->ppsz_audio_language );
    }
    if( p_sys->ppsz_sub_language )
    {
        for( i = 0; p_sys->ppsz_sub_language[i]; i++ )
            free( p_sys->ppsz_sub_language[i] );
        free( p_sys->ppsz_sub_language );
    }

    if( p_sys->es )
        free( p_sys->es );

    for( i = 0; i < p_sys->i_pgrm; i++ )
    {
        free( p_sys->pgrm[i] );
    }
    if( p_sys->pgrm )
        free( p_sys->pgrm );

    free( p_sys );
    free( out );
}

es_out_id_t *input_EsOutGetFromID( es_out_t *out, int i_id )
{
    int i;
    if( i_id < 0 )
    {
        /* Special HACK, -i_id is tha cat of the stream */
        return (es_out_id_t*)((uint8_t*)NULL-i_id);
    }

    for( i = 0; i < out->p_sys->i_es; i++ )
    {
        if( out->p_sys->es[i]->i_id == i_id )
            return out->p_sys->es[i];
    }
    return NULL;
}

void input_EsOutDiscontinuity( es_out_t *out, vlc_bool_t b_audio )
{
    es_out_sys_t      *p_sys = out->p_sys;
    int i;

    for( i = 0; i < p_sys->i_es; i++ )
    {
        es_out_id_t *es = p_sys->es[i];

        /* Send a dummy block to let decoder know that
         * there is a discontinuity */
        if( es->p_dec && ( !b_audio || es->fmt.i_cat == AUDIO_ES ) )
        {
            input_DecoderDiscontinuity( es->p_dec );
        }
    }
}

void input_EsOutSetDelay( es_out_t *out, int i_cat, int64_t i_delay )
{
    es_out_sys_t *p_sys = out->p_sys;

    if( i_cat == AUDIO_ES )
        p_sys->i_audio_delay = i_delay;
    else if( i_cat == SPU_ES )
        p_sys->i_spu_delay = i_delay;
}

vlc_bool_t input_EsOutDecodersEmpty( es_out_t *out )
{
    es_out_sys_t      *p_sys = out->p_sys;
    int i;

    for( i = 0; i < p_sys->i_es; i++ )
    {
        es_out_id_t *es = p_sys->es[i];

        if( es->p_dec && !input_DecoderEmpty( es->p_dec ) )
            return VLC_FALSE;
    }
    return VLC_TRUE;
}

/*****************************************************************************
 *
 *****************************************************************************/
static void EsOutESVarUpdate( es_out_t *out, es_out_id_t *es,
                              vlc_bool_t b_delete )
{
    es_out_sys_t      *p_sys = out->p_sys;
    input_thread_t    *p_input = p_sys->p_input;
    vlc_value_t       val, text;

    char *psz_var;

    if( es->fmt.i_cat == AUDIO_ES )
        psz_var = "audio-es";
    else if( es->fmt.i_cat == VIDEO_ES )
        psz_var = "video-es";
    else if( es->fmt.i_cat == SPU_ES )
        psz_var = "spu-es";
    else
        return;

    if( b_delete )
    {
        val.i_int = es->i_id;
        var_Change( p_input, psz_var, VLC_VAR_DELCHOICE, &val, NULL );
        var_SetBool( p_sys->p_input, "intf-change", VLC_TRUE );
        return;
    }

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
    if( es->fmt.psz_description && *es->fmt.psz_description )
    {
        if( es->psz_language && *es->psz_language )
        {
            text.psz_string = malloc( strlen( es->fmt.psz_description) + strlen( es->psz_language ) + 10 );
            sprintf( text.psz_string, "%s - [%s]", es->fmt.psz_description, es->psz_language );
        }
        else text.psz_string = strdup( es->fmt.psz_description );
    }
    else
    {
        if( es->psz_language && *es->psz_language )
        {
            char *temp;
            text.psz_string = malloc( strlen( _("Track %i") )+ strlen( es->psz_language ) + 30 );
            asprintf( &temp,  _("Track %i"), val.i_int );
            sprintf( text.psz_string, "%s - [%s]", temp, es->psz_language );
            free( temp );
        }
        else
        {
            text.psz_string = malloc( strlen( _("Track %i") ) + 20 );
            sprintf( text.psz_string, _("Track %i"), val.i_int );
        }
    }

    val.i_int = es->i_id;
    var_Change( p_input, psz_var, VLC_VAR_ADDCHOICE, &val, &text );

    free( text.psz_string );

    var_SetBool( p_sys->p_input, "intf-change", VLC_TRUE );
}

/* EsOutProgramSelect:
 *  Select a program and update the object variable
 */
static void EsOutProgramSelect( es_out_t *out, es_out_pgrm_t *p_pgrm )
{
    es_out_sys_t      *p_sys = out->p_sys;
    input_thread_t    *p_input = p_sys->p_input;
    vlc_value_t       val;
    int               i;

    if( p_sys->p_pgrm == p_pgrm )
        return; /* Nothing to do */

    if( p_sys->p_pgrm )
    {
        es_out_pgrm_t *old = p_sys->p_pgrm;
        msg_Dbg( p_input, "Unselecting program id=%d", old->i_id );

        for( i = 0; i < p_sys->i_es; i++ )
        {
            if( p_sys->es[i]->p_pgrm == old && p_sys->es[i]->p_dec &&
                p_sys->i_mode != ES_OUT_MODE_ALL )
                EsUnselect( out, p_sys->es[i], VLC_TRUE );
        }

        p_sys->p_es_audio = NULL;
        p_sys->p_es_sub = NULL;
        p_sys->p_es_video = NULL;
    }

    msg_Dbg( p_input, "Selecting program id=%d", p_pgrm->i_id );

    /* Mark it selected */
    p_pgrm->b_selected = VLC_TRUE;

    /* Switch master stream */
    if( p_sys->p_pgrm && p_sys->p_pgrm->clock.b_master )
    {
        p_sys->p_pgrm->clock.b_master = VLC_FALSE;
    }
    p_pgrm->clock.b_master = VLC_TRUE;
    p_sys->p_pgrm = p_pgrm;

    /* Update "program" */
    val.i_int = p_pgrm->i_id;
    var_Change( p_input, "program", VLC_VAR_SETVALUE, &val, NULL );

    /* Update "es-*" */
    var_Change( p_input, "audio-es", VLC_VAR_CLEARCHOICES, NULL, NULL );
    var_Change( p_input, "video-es", VLC_VAR_CLEARCHOICES, NULL, NULL );
    var_Change( p_input, "spu-es",   VLC_VAR_CLEARCHOICES, NULL, NULL );
    for( i = 0; i < p_sys->i_es; i++ )
    {
        if( p_sys->es[i]->p_pgrm == p_sys->p_pgrm )
            EsOutESVarUpdate( out, p_sys->es[i], VLC_FALSE );
        EsOutSelect( out, p_sys->es[i], VLC_FALSE );
    }

    var_SetBool( p_sys->p_input, "intf-change", VLC_TRUE );
}

/* EsOutAddProgram:
 *  Add a program
 */
static es_out_pgrm_t *EsOutProgramAdd( es_out_t *out, int i_group )
{
    es_out_sys_t      *p_sys = out->p_sys;
    input_thread_t    *p_input = p_sys->p_input;
    vlc_value_t       val;

    es_out_pgrm_t *p_pgrm = malloc( sizeof( es_out_pgrm_t ) );

    /* Init */
    p_pgrm->i_id = i_group;
    p_pgrm->i_es = 0;
    p_pgrm->b_selected = VLC_FALSE;
    input_ClockInit( &p_pgrm->clock, VLC_FALSE, p_input->input.i_cr_average );

    /* Append it */
    TAB_APPEND( p_sys->i_pgrm, p_sys->pgrm, p_pgrm );

    /* Update "program" variable */
    val.i_int = i_group;
    var_Change( p_input, "program", VLC_VAR_ADDCHOICE, &val, NULL );

    if( i_group == var_GetInteger( p_input, "program" ) )
    {
        EsOutProgramSelect( out, p_pgrm );
    }
    else
    {
        var_SetBool( p_sys->p_input, "intf-change", VLC_TRUE );
    }
    return p_pgrm;
}

/* EsOutAdd:
 *  Add an es_out
 */
static es_out_id_t *EsOutAdd( es_out_t *out, es_format_t *fmt )
{
    es_out_sys_t      *p_sys = out->p_sys;
    input_thread_t    *p_input = p_sys->p_input;

    es_out_id_t       *es = malloc( sizeof( es_out_id_t ) );
    es_out_pgrm_t     *p_pgrm = NULL;
    int i;

    if( fmt->i_group < 0 )
    {
        msg_Err( p_input, "invalid group number" );
        return NULL;
    }

    /* Search the program */
    for( i = 0; i < p_sys->i_pgrm; i++ )
    {
        if( fmt->i_group == p_sys->pgrm[i]->i_id )
        {
            p_pgrm = p_sys->pgrm[i];
            break;
        }
    }
    if( p_pgrm == NULL )
    {
        /* Create a new one */
        p_pgrm = EsOutProgramAdd( out, fmt->i_group );
    }

    /* Increase ref count for program */
    p_pgrm->i_es++;

    /* Set up ES */
    if( fmt->i_id < 0 )
        fmt->i_id = out->p_sys->i_id;
    es->i_id = fmt->i_id;
    es->p_pgrm = p_pgrm;
    es_format_Copy( &es->fmt, fmt );
    es->i_preroll_end = -1;

    switch( fmt->i_cat )
    {
    case AUDIO_ES:
        es->i_channel = p_sys->i_audio;
        break;

    case VIDEO_ES:
        es->i_channel = p_sys->i_video;
        break;

    case SPU_ES:
        es->i_channel = p_sys->i_sub;
        break;

    default:
        es->i_channel = 0;
        break;
    }
    es->psz_language = LanguageGetName( fmt->psz_language ); /* remember so we only need to do it once */
    es->psz_language_code = LanguageGetCode( fmt->psz_language );
    es->p_dec = NULL;

    if( es->p_pgrm == p_sys->p_pgrm )
        EsOutESVarUpdate( out, es, VLC_FALSE );

    /* Select it if needed */
    EsOutSelect( out, es, VLC_FALSE );


    TAB_APPEND( out->p_sys->i_es, out->p_sys->es, es );
    p_sys->i_id++;  /* always incremented */
    switch( fmt->i_cat )
    {
        case AUDIO_ES:
            p_sys->i_audio++;
            break;
        case SPU_ES:
            p_sys->i_sub++;
            break;
        case VIDEO_ES:
            p_sys->i_video++;
            break;
    }

    EsOutAddInfo( out, es );

    return es;
}

static void EsSelect( es_out_t *out, es_out_id_t *es )
{
    es_out_sys_t   *p_sys = out->p_sys;
    input_thread_t *p_input = p_sys->p_input;
    vlc_value_t    val;
    char           *psz_var;

    if( es->p_dec )
    {
        msg_Warn( p_input, "ES 0x%x is already selected", es->i_id );
        return;
    }

    if( es->fmt.i_cat == VIDEO_ES || es->fmt.i_cat == SPU_ES )
    {
        if( !var_GetBool( p_input, "video" ) ||
            ( p_input->p_sout && !var_GetBool( p_input, "sout-video" ) ) )
        {
            msg_Dbg( p_input, "video is disabled, not selecting ES 0x%x",
                     es->i_id );
            return;
        }
    }
    else if( es->fmt.i_cat == AUDIO_ES )
    {
        var_Get( p_input, "audio", &val );
        if( !var_GetBool( p_input, "audio" ) ||
            ( p_input->p_sout && !var_GetBool( p_input, "sout-audio" ) ) )
        {
            msg_Dbg( p_input, "audio is disabled, not selecting ES 0x%x",
                     es->i_id );
            return;
        }
    }

    es->i_preroll_end = -1;
    es->p_dec = input_DecoderNew( p_input, &es->fmt, VLC_FALSE );
    if( es->p_dec == NULL || es->p_pgrm != p_sys->p_pgrm )
        return;

    if( es->fmt.i_cat == VIDEO_ES )
        psz_var = "video-es";
    else if( es->fmt.i_cat == AUDIO_ES )
        psz_var = "audio-es";
    else if( es->fmt.i_cat == SPU_ES )
        psz_var = "spu-es";
    else
        return;

    /* Mark it as selected */
    val.i_int = es->i_id;
    var_Change( p_input, psz_var, VLC_VAR_SETVALUE, &val, NULL );


    var_SetBool( p_sys->p_input, "intf-change", VLC_TRUE );
}

static void EsUnselect( es_out_t *out, es_out_id_t *es, vlc_bool_t b_update )
{
    es_out_sys_t   *p_sys = out->p_sys;
    input_thread_t *p_input = p_sys->p_input;
    vlc_value_t    val;
    char           *psz_var;

    if( es->p_dec == NULL )
    {
        msg_Warn( p_input, "ES 0x%x is already unselected", es->i_id );
        return;
    }

    input_DecoderDelete( es->p_dec );
    es->p_dec = NULL;

    if( !b_update )
        return;

    /* Update var */
    if( es->p_dec == NULL )
        return;
    if( es->fmt.i_cat == VIDEO_ES )
        psz_var = "video-es";
    else if( es->fmt.i_cat == AUDIO_ES )
        psz_var = "audio-es";
    else if( es->fmt.i_cat == SPU_ES )
        psz_var = "spu-es";
    else
        return;

    /* Mark it as selected */
    val.i_int = -1;
    var_Change( p_input, psz_var, VLC_VAR_SETVALUE, &val, NULL );

    var_SetBool( p_sys->p_input, "intf-change", VLC_TRUE );
}

/**
 * Select an ES given the current mode
 * XXX: you need to take a the lock before (stream.stream_lock)
 *
 * \param out The es_out structure
 * \param es es_out_id structure
 * \param b_force ...
 * \return nothing
 */
static void EsOutSelect( es_out_t *out, es_out_id_t *es, vlc_bool_t b_force )
{
    es_out_sys_t      *p_sys = out->p_sys;

    int i_cat = es->fmt.i_cat;

    if( !p_sys->b_active ||
        ( !b_force && es->fmt.i_priority < 0 ) )
    {
        return;
    }

    if( p_sys->i_mode == ES_OUT_MODE_ALL || b_force )
    {
        if( !es->p_dec )
            EsSelect( out, es );
    }
    else if( p_sys->i_mode == ES_OUT_MODE_PARTIAL )
    {
        vlc_value_t val;
        int i;
        var_Get( p_sys->p_input, "programs", &val );
        for ( i = 0; i < val.p_list->i_count; i++ )
        {
            if ( val.p_list->p_values[i].i_int == es->p_pgrm->i_id || b_force )
            {
                if( !es->p_dec )
                    EsSelect( out, es );
                break;
            }
        }
        var_Change( p_sys->p_input, "programs", VLC_VAR_FREELIST, &val, NULL );
    }
    else if( p_sys->i_mode == ES_OUT_MODE_AUTO )
    {
        int i_wanted  = -1;

        if( es->p_pgrm != p_sys->p_pgrm )
            return;

        if( i_cat == AUDIO_ES )
        {
            int idx1 = LanguageArrayIndex( p_sys->ppsz_audio_language,
                                     es->psz_language_code );

            if( p_sys->p_es_audio &&
                p_sys->p_es_audio->fmt.i_priority >= es->fmt.i_priority )
            {
                int idx2 = LanguageArrayIndex( p_sys->ppsz_audio_language,
                                         p_sys->p_es_audio->psz_language_code );

                if( idx1 < 0 || ( idx2 >= 0 && idx2 <= idx1 ) )
                    return;
                i_wanted = es->i_channel;
            }
            else
            {
                /* Select audio if (no audio selected yet)
                 * - no audio-language
                 * - no audio code for the ES
                 * - audio code in the requested list */
                if( idx1 >= 0 ||
                    !strcmp( es->psz_language_code, "??" ) ||
                    !p_sys->ppsz_audio_language )
                    i_wanted = es->i_channel;
            }

            if( p_sys->i_audio_last >= 0 )
                i_wanted = p_sys->i_audio_last;
        }
        else if( i_cat == SPU_ES )
        {
            int idx1 = LanguageArrayIndex( p_sys->ppsz_sub_language,
                                     es->psz_language_code );

            if( p_sys->p_es_sub &&
                p_sys->p_es_sub->fmt.i_priority >= es->fmt.i_priority )
            {
                int idx2 = LanguageArrayIndex( p_sys->ppsz_sub_language,
                                         p_sys->p_es_sub->psz_language_code );

                msg_Dbg( p_sys->p_input, "idx1=%d(%s) idx2=%d(%s)",
                        idx1, es->psz_language_code, idx2,
                        p_sys->p_es_sub->psz_language_code );

                if( idx1 < 0 || ( idx2 >= 0 && idx2 <= idx1 ) )
                    return;
                /* We found a SPU that matches our language request */
                i_wanted  = es->i_channel;
            }
            else if( idx1 >= 0 )
            {
                msg_Dbg( p_sys->p_input, "idx1=%d(%s)",
                        idx1, es->psz_language_code );

                i_wanted  = es->i_channel;
            }
            if( p_sys->i_sub_last >= 0 )
                i_wanted  = p_sys->i_sub_last;
        }
        else if( i_cat == VIDEO_ES )
        {
            i_wanted  = es->i_channel;
        }

        if( i_wanted == es->i_channel && es->p_dec == NULL )
            EsSelect( out, es );
    }

    /* FIXME TODO handle priority here */
    if( es->p_dec )
    {
        if( i_cat == AUDIO_ES )
        {
            if( p_sys->i_mode == ES_OUT_MODE_AUTO &&
                p_sys->p_es_audio &&
                p_sys->p_es_audio != es &&
                p_sys->p_es_audio->p_dec )
            {
                EsUnselect( out, p_sys->p_es_audio, VLC_FALSE );
            }
            p_sys->p_es_audio = es;
        }
        else if( i_cat == SPU_ES )
        {
            if( p_sys->i_mode == ES_OUT_MODE_AUTO &&
                p_sys->p_es_sub &&
                p_sys->p_es_sub != es &&
                p_sys->p_es_sub->p_dec )
            {
                EsUnselect( out, p_sys->p_es_sub, VLC_FALSE );
            }
            p_sys->p_es_sub = es;
        }
        else if( i_cat == VIDEO_ES )
        {
            p_sys->p_es_video = es;
        }
    }
}

/**
 * Send a block for the given es_out
 *
 * \param out the es_out to send from
 * \param es the es_out_id
 * \param p_block the data block to send
 */
static int EsOutSend( es_out_t *out, es_out_id_t *es, block_t *p_block )
{
    es_out_sys_t *p_sys = out->p_sys;
    input_thread_t    *p_input = p_sys->p_input;
    es_out_pgrm_t *p_pgrm = es->p_pgrm;
    int64_t i_delay;

    if( es->fmt.i_cat == AUDIO_ES )
        i_delay = p_sys->i_audio_delay;
    else if( es->fmt.i_cat == SPU_ES )
        i_delay = p_sys->i_spu_delay;
    else
        i_delay = 0;

    /* Mark preroll blocks */
    if( es->i_preroll_end >= 0 )
    {
        int64_t i_date = p_block->i_pts;
        if( i_date <= 0 )
            i_date = p_block->i_dts;

        if( i_date < es->i_preroll_end )
            p_block->i_flags |= BLOCK_FLAG_PREROLL;
        else
            es->i_preroll_end = -1;
    }

    /* +11 -> avoid null value with non null dts/pts */
    if( p_block->i_dts > 0 )
    {
        p_block->i_dts =
            input_ClockGetTS( p_input, &p_pgrm->clock,
                              ( p_block->i_dts + 11 ) * 9 / 100 ) + i_delay;
    }
    if( p_block->i_pts > 0 )
    {
        p_block->i_pts =
            input_ClockGetTS( p_input, &p_pgrm->clock,
                              ( p_block->i_pts + 11 ) * 9 / 100 ) + i_delay;
    }
    if ( es->fmt.i_codec == VLC_FOURCC( 't', 'e', 'l', 'x' ) )
    {
        mtime_t current_date = mdate();
        if( !p_block->i_pts
               || p_block->i_pts > current_date + 10000000
               || current_date > p_block->i_pts )
        {
            /* ETSI EN 300 472 Annex A : do not take into account the PTS
             * for teletext streams. */
            p_block->i_pts = current_date + 400000
                               + p_input->i_pts_delay + i_delay;
        }
    }

    p_block->i_rate = p_input->i_rate;

    /* TODO handle mute */
    if( es->p_dec && ( es->fmt.i_cat != AUDIO_ES ||
        p_input->i_rate == INPUT_RATE_DEFAULT ) )
    {
        input_DecoderDecode( es->p_dec, p_block );
    }
    else
    {
        block_Release( p_block );
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * EsOutDel:
 *****************************************************************************/
static void EsOutDel( es_out_t *out, es_out_id_t *es )
{
    es_out_sys_t *p_sys = out->p_sys;

    /* We don't try to reselect */
    if( es->p_dec )
        EsUnselect( out, es, es->p_pgrm == p_sys->p_pgrm );

    if( es->p_pgrm == p_sys->p_pgrm )
        EsOutESVarUpdate( out, es, VLC_TRUE );

    TAB_REMOVE( p_sys->i_es, p_sys->es, es );

    es->p_pgrm->i_es--;
    if( es->p_pgrm->i_es == 0 )
    {
        msg_Warn( p_sys->p_input, "Program doesn't contain anymore ES, "
                  "TODO cleaning ?" );
    }

    if( p_sys->p_es_audio == es ) p_sys->p_es_audio = NULL;
    if( p_sys->p_es_video == es ) p_sys->p_es_video = NULL;
    if( p_sys->p_es_sub   == es ) p_sys->p_es_sub   = NULL;

    switch( es->fmt.i_cat )
    {
        case AUDIO_ES:
            p_sys->i_audio--;
            break;
        case SPU_ES:
            p_sys->i_sub--;
            break;
        case VIDEO_ES:
            p_sys->i_video--;
            break;
    }

    if( es->psz_language )
        free( es->psz_language );
    if( es->psz_language_code )
        free( es->psz_language_code );

    es_format_Clean( &es->fmt );

    free( es );
}

/**
 * Control query handler
 *
 * \param out the es_out to control
 * \param i_query A es_out query as defined in include/ninput.h
 * \param args a variable list of arguments for the query
 * \return VLC_SUCCESS or an error code
 */
static int EsOutControl( es_out_t *out, int i_query, va_list args )
{
    es_out_sys_t *p_sys = out->p_sys;
    vlc_bool_t  b, *pb;
    int         i, *pi;

    es_out_id_t *es;

    switch( i_query )
    {
        case ES_OUT_SET_ES_STATE:
            es = (es_out_id_t*) va_arg( args, es_out_id_t * );
            b = (vlc_bool_t) va_arg( args, vlc_bool_t );
            if( b && es->p_dec == NULL )
            {
                EsSelect( out, es );
                return es->p_dec ? VLC_SUCCESS : VLC_EGENERIC;
            }
            else if( !b && es->p_dec )
            {
                EsUnselect( out, es, es->p_pgrm == p_sys->p_pgrm );
                return VLC_SUCCESS;
            }
            return VLC_SUCCESS;

        case ES_OUT_GET_ES_STATE:
            es = (es_out_id_t*) va_arg( args, es_out_id_t * );
            pb = (vlc_bool_t*) va_arg( args, vlc_bool_t * );

            *pb = es->p_dec ? VLC_TRUE : VLC_FALSE;
            return VLC_SUCCESS;

        case ES_OUT_SET_ACTIVE:
        {
            b = (vlc_bool_t) va_arg( args, vlc_bool_t );
            p_sys->b_active = b;
            /* Needed ? */
            if( b )
                var_SetBool( p_sys->p_input, "intf-change", VLC_TRUE );
            return VLC_SUCCESS;
        }

        case ES_OUT_GET_ACTIVE:
            pb = (vlc_bool_t*) va_arg( args, vlc_bool_t * );
            *pb = p_sys->b_active;
            return VLC_SUCCESS;

        case ES_OUT_SET_MODE:
            i = (int) va_arg( args, int );
            if( i == ES_OUT_MODE_NONE || i == ES_OUT_MODE_ALL ||
                i == ES_OUT_MODE_AUTO || i == ES_OUT_MODE_PARTIAL )
            {
                p_sys->i_mode = i;

                /* Reapply policy mode */
                for( i = 0; i < p_sys->i_es; i++ )
                {
                    if( p_sys->es[i]->p_dec )
                    {
                        EsUnselect( out, p_sys->es[i],
                                    p_sys->es[i]->p_pgrm == p_sys->p_pgrm );
                    }
                }
                for( i = 0; i < p_sys->i_es; i++ )
                {
                    EsOutSelect( out, p_sys->es[i], VLC_FALSE );
                }
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case ES_OUT_GET_MODE:
            pi = (int*) va_arg( args, int* );
            *pi = p_sys->i_mode;
            return VLC_SUCCESS;

        case ES_OUT_SET_ES:
            es = (es_out_id_t*) va_arg( args, es_out_id_t * );
            /* Special case NULL, NULL+i_cat */
            if( es == NULL )
            {
                for( i = 0; i < p_sys->i_es; i++ )
                {
                    if( p_sys->es[i]->p_dec )
                        EsUnselect( out, p_sys->es[i],
                                    p_sys->es[i]->p_pgrm == p_sys->p_pgrm );
                }
            }
            else if( es == (es_out_id_t*)((uint8_t*)NULL+AUDIO_ES) )
            {
                for( i = 0; i < p_sys->i_es; i++ )
                {
                    if( p_sys->es[i]->p_dec &&
                        p_sys->es[i]->fmt.i_cat == AUDIO_ES )
                        EsUnselect( out, p_sys->es[i],
                                    p_sys->es[i]->p_pgrm == p_sys->p_pgrm );
                }
            }
            else if( es == (es_out_id_t*)((uint8_t*)NULL+VIDEO_ES) )
            {
                for( i = 0; i < p_sys->i_es; i++ )
                {
                    if( p_sys->es[i]->p_dec &&
                        p_sys->es[i]->fmt.i_cat == VIDEO_ES )
                        EsUnselect( out, p_sys->es[i],
                                    p_sys->es[i]->p_pgrm == p_sys->p_pgrm );
                }
            }
            else if( es == (es_out_id_t*)((uint8_t*)NULL+SPU_ES) )
            {
                for( i = 0; i < p_sys->i_es; i++ )
                {
                    if( p_sys->es[i]->p_dec &&
                        p_sys->es[i]->fmt.i_cat == SPU_ES )
                        EsUnselect( out, p_sys->es[i],
                                    p_sys->es[i]->p_pgrm == p_sys->p_pgrm );
                }
            }
            else
            {
                for( i = 0; i < p_sys->i_es; i++ )
                {
                    if( es == p_sys->es[i] )
                    {
                        EsOutSelect( out, es, VLC_TRUE );
                        break;
                    }
                }
            }
            return VLC_SUCCESS;

        case ES_OUT_SET_PCR:
        case ES_OUT_SET_GROUP_PCR:
        {
            es_out_pgrm_t *p_pgrm = NULL;
            int            i_group = 0;
            int64_t        i_pcr;

            if( i_query == ES_OUT_SET_PCR )
            {
                p_pgrm = p_sys->p_pgrm;
            }
            else
            {
                int i;
                i_group = (int)va_arg( args, int );
                for( i = 0; i < p_sys->i_pgrm; i++ )
                {
                    if( p_sys->pgrm[i]->i_id == i_group )
                    {
                        p_pgrm = p_sys->pgrm[i];
                        break;
                    }
                }
            }
            if( p_pgrm == NULL )
                p_pgrm = EsOutProgramAdd( out, i_group );   /* Create it */

            i_pcr = (int64_t)va_arg( args, int64_t );
            /* search program */
            /* 11 is a vodoo trick to avoid non_pcr*9/100 to be null */
            input_ClockSetPCR( p_sys->p_input, &p_pgrm->clock,
                               (i_pcr + 11 ) * 9 / 100);
            return VLC_SUCCESS;
        }

        case ES_OUT_RESET_PCR:
            for( i = 0; i < p_sys->i_pgrm; i++ )
            {
                p_sys->pgrm[i]->clock.i_synchro_state =  SYNCHRO_REINIT;
                p_sys->pgrm[i]->clock.last_pts = 0;
            }
            return VLC_SUCCESS;

        case ES_OUT_GET_TS:
            if( p_sys->p_pgrm )
            {
                int64_t i_ts = (int64_t)va_arg( args, int64_t );
                int64_t *pi_ts = (int64_t *)va_arg( args, int64_t * );
                *pi_ts = input_ClockGetTS( p_sys->p_input,
                                           &p_sys->p_pgrm->clock,
                                           ( i_ts + 11 ) * 9 / 100 );
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case ES_OUT_GET_GROUP:
            pi = (int*) va_arg( args, int* );
            if( p_sys->p_pgrm )
                *pi = p_sys->p_pgrm->i_id;
            else
                *pi = -1;    /* FIXME */
            return VLC_SUCCESS;

        case ES_OUT_SET_GROUP:
        {
            int j;
            i = (int) va_arg( args, int );
            for( j = 0; j < p_sys->i_pgrm; j++ )
            {
                es_out_pgrm_t *p_pgrm = p_sys->pgrm[j];
                if( p_pgrm->i_id == i )
                {
                    EsOutProgramSelect( out, p_pgrm );
                    return VLC_SUCCESS;
                }
            }
            return VLC_EGENERIC;
        }

        case ES_OUT_SET_FMT:
        {
            /* This ain't pretty but is need by some demuxers (eg. Ogg )
             * to update the p_extra data */
            es_format_t *p_fmt;
            es = (es_out_id_t*) va_arg( args, es_out_id_t * );
            p_fmt = (es_format_t*) va_arg( args, es_format_t * );
            if( es == NULL ) return VLC_EGENERIC;

            if( p_fmt->i_extra )
            {
                es->fmt.i_extra = p_fmt->i_extra;
                es->fmt.p_extra = realloc( es->fmt.p_extra, p_fmt->i_extra );
                memcpy( es->fmt.p_extra, p_fmt->p_extra, p_fmt->i_extra );

                if( !es->p_dec ) return VLC_SUCCESS;

#if 1
                input_DecoderDelete( es->p_dec );
                es->p_dec = input_DecoderNew( p_sys->p_input,
                                              &es->fmt, VLC_FALSE );

#else
                es->p_dec->fmt_in.i_extra = p_fmt->i_extra;
                es->p_dec->fmt_in.p_extra =
                    realloc( es->p_dec->fmt_in.p_extra, p_fmt->i_extra );
                memcpy( es->p_dec->fmt_in.p_extra,
                        p_fmt->p_extra, p_fmt->i_extra );
#endif
            }

            return VLC_SUCCESS;
        }

        case ES_OUT_SET_NEXT_DISPLAY_TIME:
        {
            int64_t i_date;

            es = (es_out_id_t*) va_arg( args, es_out_id_t * );
            i_date = (int64_t)va_arg( args, int64_t );

            if( !es || !es->p_dec )
                return VLC_EGENERIC;

            es->i_preroll_end = i_date;
            input_DecoderPreroll( es->p_dec, i_date );

            return VLC_SUCCESS;
        }

        default:
            msg_Err( p_sys->p_input, "unknown query in es_out_Control" );
            return VLC_EGENERIC;
    }
}

/****************************************************************************
 * LanguageGetName: try to expend iso639 into plain name
 ****************************************************************************/
static char *LanguageGetName( const char *psz_code )
{
    const iso639_lang_t *pl;

    if( psz_code == NULL )
    {
        return strdup( "" );
    }

    if( strlen( psz_code ) == 2 )
    {
        pl = GetLang_1( psz_code );
    }
    else if( strlen( psz_code ) == 3 )
    {
        pl = GetLang_2B( psz_code );
        if( !strcmp( pl->psz_iso639_1, "??" ) )
        {
            pl = GetLang_2T( psz_code );
        }
    }
    else
    {
        return strdup( psz_code );
    }

    if( !strcmp( pl->psz_iso639_1, "??" ) )
    {
       return strdup( psz_code );
    }
    else
    {
        if( *pl->psz_native_name )
        {
            return strdup( pl->psz_native_name );
        }
        return strdup( pl->psz_eng_name );
    }
}

/* Get a 2 char code */
static char *LanguageGetCode( const char *psz_lang )
{
    const iso639_lang_t *pl;

    if( psz_lang == NULL || *psz_lang == '\0' )
        return strdup("??");

    for( pl = p_languages; pl->psz_iso639_1 != NULL; pl++ )
    {
        if( !strcasecmp( pl->psz_eng_name, psz_lang ) ||
            !strcasecmp( pl->psz_native_name, psz_lang ) ||
            !strcasecmp( pl->psz_iso639_1, psz_lang ) ||
            !strcasecmp( pl->psz_iso639_2T, psz_lang ) ||
            !strcasecmp( pl->psz_iso639_2B, psz_lang ) )
            break;
    }

    if( pl->psz_iso639_1 != NULL )
        return strdup( pl->psz_iso639_1 );

    return strdup("??");
}

static char **LanguageSplit( const char *psz_langs )
{
    char *psz_dup;
    char *psz_parser;
    char **ppsz = NULL;
    int i_psz = 0;

    if( psz_langs == NULL )
        return NULL;

    psz_parser = psz_dup = strdup(psz_langs);

    while( psz_parser && *psz_parser )
    {
        char *psz;
        char *psz_code;

        psz = strchr(psz_parser, ',' );
        if( psz )
        {
            *psz++ = '\0';
        }

        psz_code = LanguageGetCode( psz_parser );
        if( strcmp( psz_code, "??" ) )
        {
            TAB_APPEND( i_psz, ppsz, psz_code );
        }

        psz_parser = psz;
    }

    if( i_psz )
    {
        TAB_APPEND( i_psz, ppsz, NULL );
    }

    return ppsz;
}

static int LanguageArrayIndex( char **ppsz_langs, char *psz_lang )
{
    int i;

    if( !ppsz_langs || !psz_lang )
        return -1;

    for( i = 0; ppsz_langs[i]; i++ )
        if( !strcasecmp( ppsz_langs[i], psz_lang ) )
            return i;

    return -1;
}

/****************************************************************************
 * EsOutAddInfo:
 * - add meta info to the playlist item
 ****************************************************************************/
static void EsOutAddInfo( es_out_t *out, es_out_id_t *es )
{
    es_out_sys_t   *p_sys = out->p_sys;
    input_thread_t *p_input = p_sys->p_input;
    es_format_t    *fmt = &es->fmt;
    char           *psz_cat;

    /* Add stream info */
    asprintf( &psz_cat, _("Stream %d"), out->p_sys->i_id - 1 );

    input_Control( p_input, INPUT_ADD_INFO, psz_cat, _("Codec"),
                   "%.4s", (char*)&fmt->i_codec );

    input_Control( p_input, INPUT_ADD_INFO, psz_cat, _("Language"),
                   "%s", es->psz_language );

    /* Add information */
    switch( fmt->i_cat )
    {
    case AUDIO_ES:
        input_Control( p_input, INPUT_ADD_INFO, psz_cat,
                       _("Type"), _("Audio") );

        if( fmt->audio.i_channels > 0 )
            input_Control( p_input, INPUT_ADD_INFO, psz_cat, _("Channels"),
                           "%d", fmt->audio.i_channels );

        if( fmt->audio.i_rate > 0 )
            input_Control( p_input, INPUT_ADD_INFO, psz_cat, _("Sample rate"),
                           _("%d Hz"), fmt->audio.i_rate );

        if( fmt->audio.i_bitspersample > 0 )
            input_Control( p_input, INPUT_ADD_INFO, psz_cat,
                           _("Bits per sample"), "%d",
                           fmt->audio.i_bitspersample );

        if( fmt->i_bitrate > 0 )
            input_Control( p_input, INPUT_ADD_INFO, psz_cat, _("Bitrate"),
                           _("%d kb/s"), fmt->i_bitrate / 1000 );
        break;

    case VIDEO_ES:
        input_Control( p_input, INPUT_ADD_INFO, psz_cat,
                       _("Type"), _("Video") );

        if( fmt->video.i_width > 0 && fmt->video.i_height > 0 )
            input_Control( p_input, INPUT_ADD_INFO, psz_cat,
                           _("Resolution"), "%dx%d",
                           fmt->video.i_width, fmt->video.i_height );

        if( fmt->video.i_visible_width > 0 &&
            fmt->video.i_visible_height > 0 )
            input_Control( p_input, INPUT_ADD_INFO, psz_cat,
                           _("Display resolution"), "%dx%d",
                           fmt->video.i_visible_width,
                           fmt->video.i_visible_height);
        break;

    case SPU_ES:
        input_Control( p_input, INPUT_ADD_INFO, psz_cat,
                       _("Type"), _("Subtitle") );
        break;

    default:
        break;
    }

    free( psz_cat );
}
