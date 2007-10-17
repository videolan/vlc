/*****************************************************************************
 * es_out.c: Es Out handler for input.
 *****************************************************************************
 * Copyright (C) 2003-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Jean-Paul Saman <jpsaman #_at_# m2x dot nl>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>

#include <stdio.h>

#include <vlc_input.h>
#include <vlc_es_out.h>
#include <vlc_block.h>
#include <vlc_aout.h>

#include "input_internal.h"

#include "vlc_playlist.h"
#include "iso_lang.h"
/* FIXME we should find a better way than including that */
#include "../text/iso-639_def.h"

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

    char    *psz_name;
    char    *psz_now_playing;
    char    *psz_publisher;

    vlc_epg_t *p_epg;
} es_out_pgrm_t;

struct es_out_id_t
{
    /* ES ID */
    int       i_id;
    es_out_pgrm_t *p_pgrm;

    /* Misc. */
    int64_t i_preroll_end;

    /* Channel in the track type */
    int         i_channel;
    es_format_t fmt;
    char        *psz_language;
    char        *psz_language_code;

    decoder_t   *p_dec;

    /* Fields for Video with CC */
    vlc_bool_t  pb_cc_present[4];
    es_out_id_t  *pp_cc_es[4];

    /* Field for CC track from a master video */
    es_out_id_t *p_master;
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
    int         i_audio_last, i_audio_id;
    int         i_sub_last, i_sub_id;
    int         i_default_sub_id;   /* As specified in container; if applicable */
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

static vlc_bool_t EsIsSelected( es_out_t *out, es_out_id_t *es );
static void EsSelect( es_out_t *out, es_out_id_t *es );
static void EsUnselect( es_out_t *out, es_out_id_t *es, vlc_bool_t b_update );
static char *LanguageGetName( const char *psz_code );
static char *LanguageGetCode( const char *psz_lang );
static char **LanguageSplit( const char *psz_langs );
static int LanguageArrayIndex( char **ppsz_langs, char *psz_lang );

static char *EsOutProgramGetMetaName( es_out_pgrm_t *p_pgrm );

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
    out->b_sout     = (p_input->p->p_sout != NULL ? VLC_TRUE : VLC_FALSE);

    p_sys->p_input = p_input;

    p_sys->b_active = VLC_FALSE;
    p_sys->i_mode   = ES_OUT_MODE_AUTO;


    TAB_INIT( p_sys->i_pgrm, p_sys->pgrm );
    p_sys->p_pgrm   = NULL;

    p_sys->i_id    = 0;

    TAB_INIT( p_sys->i_es, p_sys->es );

    p_sys->i_audio = 0;
    p_sys->i_video = 0;
    p_sys->i_sub   = 0;

    /* */
    var_Get( p_input, "audio-track", &val );
    p_sys->i_audio_last = val.i_int;

    var_Get( p_input, "sub-track", &val );
    p_sys->i_sub_last = val.i_int;

    p_sys->i_default_sub_id   = -1;

    if( !p_input->b_preparsing )
    {
        var_Get( p_input, "audio-language", &val );
        p_sys->ppsz_audio_language = LanguageSplit(val.psz_string);
        if( p_sys->ppsz_audio_language )
        {
            for( i = 0; p_sys->ppsz_audio_language[i]; i++ )
                msg_Dbg( p_input, "selected audio language[%d] %s",
                         i, p_sys->ppsz_audio_language[i] );
        }
        if( val.psz_string ) free( val.psz_string );

        var_Get( p_input, "sub-language", &val );
        p_sys->ppsz_sub_language = LanguageSplit(val.psz_string);
        if( p_sys->ppsz_sub_language )
        {
            for( i = 0; p_sys->ppsz_sub_language[i]; i++ )
                msg_Dbg( p_input, "selected subtitle language[%d] %s",
                         i, p_sys->ppsz_sub_language[i] );
        }
        if( val.psz_string ) free( val.psz_string );
    }
    else
    {
        p_sys->ppsz_sub_language = NULL;
        p_sys->ppsz_audio_language = NULL;
    }

    var_Get( p_input, "audio-track-id", &val );
    p_sys->i_audio_id = val.i_int;

    var_Get( p_input, "sub-track-id", &val );
    p_sys->i_sub_id = val.i_int;

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

    /* FIXME duplicate work EsOutProgramDel (but we cannot use it) add a EsOutProgramClean ? */
    for( i = 0; i < p_sys->i_pgrm; i++ )
    {
        es_out_pgrm_t *p_pgrm = p_sys->pgrm[i];
        if( p_pgrm->psz_now_playing )
            free( p_pgrm->psz_now_playing );
        if( p_pgrm->psz_publisher )
            free( p_pgrm->psz_publisher );
        if( p_pgrm->psz_name )
            free( p_pgrm->psz_name );
        if( p_pgrm->p_epg )
            vlc_epg_Delete( p_pgrm->p_epg );

        free( p_pgrm );
    }
    TAB_CLEAN( p_sys->i_pgrm, p_sys->pgrm );

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

static void EsOutDiscontinuity( es_out_t *out, vlc_bool_t b_flush, vlc_bool_t b_audio )
{
    es_out_sys_t      *p_sys = out->p_sys;
    int i;

    for( i = 0; i < p_sys->i_es; i++ )
    {
        es_out_id_t *es = p_sys->es[i];

        /* Send a dummy block to let decoder know that
         * there is a discontinuity */
        if( es->p_dec && ( !b_audio || es->fmt.i_cat == AUDIO_ES ) )
            input_DecoderDiscontinuity( es->p_dec, b_flush );
    }
}
void input_EsOutChangeRate( es_out_t *out )
{
    es_out_sys_t      *p_sys = out->p_sys;
    int i;

    EsOutDiscontinuity( out, VLC_FALSE, VLC_FALSE );

    for( i = 0; i < p_sys->i_pgrm; i++ )
        input_ClockSetRate( p_sys->p_input, &p_sys->pgrm[i]->clock );
}

void input_EsOutSetDelay( es_out_t *out, int i_cat, int64_t i_delay )
{
    es_out_sys_t *p_sys = out->p_sys;

    if( i_cat == AUDIO_ES )
        p_sys->i_audio_delay = i_delay;
    else if( i_cat == SPU_ES )
        p_sys->i_spu_delay = i_delay;
}
void input_EsOutChangeState( es_out_t *out )
{
    es_out_sys_t *p_sys = out->p_sys;
    input_thread_t *p_input = p_sys->p_input;

    if( p_input->i_state  == PAUSE_S )
    {
        /* Send discontinuity to decoders (it will allow them to flush
         *                  * if implemented */
        EsOutDiscontinuity( out, VLC_FALSE, VLC_FALSE );
    }
    else
    {
        /* Out of pause, reset pcr */
        es_out_Control( out, ES_OUT_RESET_PCR );
    }
}
void input_EsOutChangePosition( es_out_t *out )
{
    //es_out_sys_t *p_sys = out->p_sys;

    es_out_Control( out, ES_OUT_RESET_PCR );
    EsOutDiscontinuity( out, VLC_TRUE, VLC_FALSE );
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
static void EsOutESVarUpdateGeneric( es_out_t *out, int i_id, es_format_t *fmt, const char *psz_language,
                                     vlc_bool_t b_delete )
{
    es_out_sys_t      *p_sys = out->p_sys;
    input_thread_t    *p_input = p_sys->p_input;
    vlc_value_t       val, text;

    const char *psz_var;

    if( fmt->i_cat == AUDIO_ES )
        psz_var = "audio-es";
    else if( fmt->i_cat == VIDEO_ES )
        psz_var = "video-es";
    else if( fmt->i_cat == SPU_ES )
        psz_var = "spu-es";
    else
        return;

    if( b_delete )
    {
        val.i_int = i_id;
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
    if( fmt->psz_description && *fmt->psz_description )
    {
        if( psz_language && *psz_language )
        {
            text.psz_string = malloc( strlen( fmt->psz_description) +
                                      strlen( psz_language ) + 10 );
            sprintf( text.psz_string, "%s - [%s]", fmt->psz_description,
                                                   psz_language );
        }
        else text.psz_string = strdup( fmt->psz_description );
    }
    else
    {
        if( psz_language && *psz_language )
        {
            char *temp;
            text.psz_string = malloc( strlen( _("Track %i") )+
                                      strlen( psz_language ) + 30 );
            asprintf( &temp,  _("Track %i"), val.i_int );
            sprintf( text.psz_string, "%s - [%s]", temp, psz_language );
            free( temp );
        }
        else
        {
            text.psz_string = malloc( strlen( _("Track %i") ) + 20 );
            sprintf( text.psz_string, _("Track %i"), val.i_int );
        }
    }

    val.i_int = i_id;
    var_Change( p_input, psz_var, VLC_VAR_ADDCHOICE, &val, &text );

    free( text.psz_string );

    var_SetBool( p_sys->p_input, "intf-change", VLC_TRUE );
}

static void EsOutESVarUpdate( es_out_t *out, es_out_id_t *es,
                              vlc_bool_t b_delete )
{
    EsOutESVarUpdateGeneric( out, es->i_id, &es->fmt, es->psz_language, b_delete );
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
        msg_Dbg( p_input, "unselecting program id=%d", old->i_id );

        for( i = 0; i < p_sys->i_es; i++ )
        {
            if( p_sys->es[i]->p_pgrm == old && EsIsSelected( out, p_sys->es[i] ) &&
                p_sys->i_mode != ES_OUT_MODE_ALL )
                EsUnselect( out, p_sys->es[i], VLC_TRUE );
        }

        p_sys->p_es_audio = NULL;
        p_sys->p_es_sub = NULL;
        p_sys->p_es_video = NULL;
    }

    msg_Dbg( p_input, "selecting program id=%d", p_pgrm->i_id );

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

    /* Update now playing */
    input_item_SetNowPlaying( p_input->p->input.p_item,
                              p_pgrm->psz_now_playing );
    input_item_SetPublisher( p_input->p->input.p_item,
                             p_pgrm->psz_publisher );

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
    p_pgrm->psz_name = NULL;
    p_pgrm->psz_now_playing = NULL;
    p_pgrm->psz_publisher = NULL;
    p_pgrm->p_epg = NULL;
    input_ClockInit( p_input, &p_pgrm->clock, VLC_FALSE, p_input->p->input.i_cr_average );

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

/* EsOutDelProgram:
 *  Delete a program
 */
static int EsOutProgramDel( es_out_t *out, int i_group )
{
    es_out_sys_t      *p_sys = out->p_sys;
    input_thread_t    *p_input = p_sys->p_input;
    es_out_pgrm_t     *p_pgrm = NULL;
    vlc_value_t       val;
    int               i;

    for( i = 0; i < p_sys->i_pgrm; i++ )
    {
        if( p_sys->pgrm[i]->i_id == i_group )
        {
            p_pgrm = p_sys->pgrm[i];
            break;
        }
    }

    if( p_pgrm == NULL )
        return VLC_EGENERIC;

    if( p_pgrm->i_es )
    {
        msg_Dbg( p_input, "can't delete program %d which still has %i ES",
                 i_group, p_pgrm->i_es );
        return VLC_EGENERIC;
    }

    TAB_REMOVE( p_sys->i_pgrm, p_sys->pgrm, p_pgrm );

    /* If program is selected we need to unselect it */
    if( p_sys->p_pgrm == p_pgrm ) p_sys->p_pgrm = NULL;

    if( p_pgrm->psz_name ) free( p_pgrm->psz_name );
    if( p_pgrm->psz_now_playing ) free( p_pgrm->psz_now_playing );
    if( p_pgrm->psz_publisher ) free( p_pgrm->psz_publisher );
    if( p_pgrm->p_epg )
        vlc_epg_Delete( p_pgrm->p_epg );
    free( p_pgrm );

    /* Update "program" variable */
    val.i_int = i_group;
    var_Change( p_input, "program", VLC_VAR_DELCHOICE, &val, NULL );

    var_SetBool( p_sys->p_input, "intf-change", VLC_TRUE );

    return VLC_SUCCESS;
}

/* EsOutProgramMeta:
 */
static char *EsOutProgramGetMetaName( es_out_pgrm_t *p_pgrm )
{
    char *psz = NULL;
    if( p_pgrm->psz_name )
        asprintf( &psz, _("%s [%s %d]"), p_pgrm->psz_name, _("Program"), p_pgrm->i_id );
    else
        asprintf( &psz, "%s %d", _("Program"), p_pgrm->i_id );
    return psz;
}

static void EsOutProgramMeta( es_out_t *out, int i_group, vlc_meta_t *p_meta )
{
    es_out_sys_t      *p_sys = out->p_sys;
    es_out_pgrm_t     *p_pgrm = NULL;
    input_thread_t    *p_input = p_sys->p_input;
    char              *psz_cat;
    const char        *psz_title = NULL;
    const char        *psz_provider = NULL;
    int i;

    msg_Dbg( p_input, "EsOutProgramMeta: number=%d", i_group );

    /* Check against empty meta data (empty for what we handle) */
    if( !vlc_meta_Get( p_meta, vlc_meta_Title) &&
        !vlc_meta_Get( p_meta, vlc_meta_NowPlaying) &&
        !vlc_meta_Get( p_meta, vlc_meta_Publisher) &&
        vlc_dictionary_keys_count( &p_meta->extra_tags ) <= 0 )
            return;
    /* Find program */
    for( i = 0; i < p_sys->i_pgrm; i++ )
    {
        if( p_sys->pgrm[i]->i_id == i_group )
        {
            p_pgrm = p_sys->pgrm[i];
            break;
        }
    }
    if( p_pgrm == NULL )
        p_pgrm = EsOutProgramAdd( out, i_group );   /* Create it */

    /* */
    psz_title = vlc_meta_Get( p_meta, vlc_meta_Title);
    psz_provider = vlc_meta_Get( p_meta, vlc_meta_Publisher);

    /* Update the description text of the program */
    if( psz_title && *psz_title )
    {
        vlc_value_t val;
        vlc_value_t text;

        if( !p_pgrm->psz_name || strcmp( p_pgrm->psz_name, psz_title ) )
        {
            char *psz_cat = EsOutProgramGetMetaName( p_pgrm );

            /* Remove old entries */
            input_Control( p_input, INPUT_DEL_INFO, psz_cat, NULL );
            /* TODO update epg name */
            free( psz_cat );
        }
        if( p_pgrm->psz_name ) free( p_pgrm->psz_name );
        p_pgrm->psz_name = strdup( psz_title );

        /* ugly but it works */
        val.i_int = i_group;
        var_Change( p_input, "program", VLC_VAR_DELCHOICE, &val, NULL );

        if( psz_provider && *psz_provider )
        {
            asprintf( &text.psz_string, "%s [%s]", psz_title, psz_provider );
            var_Change( p_input, "program", VLC_VAR_ADDCHOICE, &val, &text );
            free( text.psz_string );
        }
        else
        {
            text.psz_string = (char *)psz_title;
            var_Change( p_input, "program", VLC_VAR_ADDCHOICE, &val, &text );
        }
    }

    psz_cat = EsOutProgramGetMetaName( p_pgrm );
    if( psz_provider )
    {
        if( p_sys->p_pgrm == p_pgrm )
            input_item_SetPublisher( p_input->p->input.p_item, psz_provider );
        input_Control( p_input, INPUT_ADD_INFO, psz_cat, input_MetaTypeToLocalizedString(vlc_meta_Publisher), psz_provider );
    }
    char ** ppsz_all_keys = vlc_dictionary_all_keys( &p_meta->extra_tags );
    for( i = 0; ppsz_all_keys[i]; i++ )
    {
        input_Control( p_input, INPUT_ADD_INFO, psz_cat, _(ppsz_all_keys[i]),
                       vlc_dictionary_value_for_key( &p_meta->extra_tags, ppsz_all_keys[i] ) );
        free( ppsz_all_keys[i] );
    }
    free( ppsz_all_keys );

    free( psz_cat );
}

static void vlc_epg_Merge( vlc_epg_t *p_dst, const vlc_epg_t *p_src )
{
    int i;

    /* Add new event */
    for( i = 0; i < p_src->i_event; i++ )
    {
        vlc_epg_event_t *p_evt = p_src->pp_event[i];
        vlc_bool_t b_add = VLC_TRUE;
        int j;

        for( j = 0; j < p_dst->i_event; j++ )
        {
            if( p_dst->pp_event[j]->i_start == p_evt->i_start && p_dst->pp_event[j]->i_duration == p_evt->i_duration )
            {
                b_add = VLC_FALSE;
                break;
            }
            if( p_dst->pp_event[j]->i_start > p_evt->i_start )
                break;
        }
        if( b_add )
        {
            vlc_epg_event_t *p_copy = malloc( sizeof(vlc_epg_event_t) );
            if( !p_copy )
                break;
            memset( p_copy, 0, sizeof(vlc_epg_event_t) );
            p_copy->i_start = p_evt->i_start;
            p_copy->i_duration = p_evt->i_duration;
            p_copy->psz_name = p_evt->psz_name ? strdup( p_evt->psz_name ) : NULL;
            p_copy->psz_short_description = p_evt->psz_short_description ? strdup( p_evt->psz_short_description ) : NULL;
            p_copy->psz_description = p_evt->psz_description ? strdup( p_evt->psz_description ) : NULL;
            TAB_INSERT( p_dst->i_event, p_dst->pp_event, p_copy, j );
        }
    }
    /* Update current */
    vlc_epg_SetCurrent( p_dst, p_src->p_current ? p_src->p_current->i_start : -1 );

    /* Keep only 1 old event  */
    if( p_dst->p_current )
    {
        while( p_dst->i_event > 1 && p_dst->pp_event[0] != p_dst->p_current && p_dst->pp_event[1] != p_dst->p_current )
            TAB_REMOVE( p_dst->i_event, p_dst->pp_event, p_dst->pp_event[0] );
    }
}

static void EsOutProgramEpg( es_out_t *out, int i_group, vlc_epg_t *p_epg )
{
    es_out_sys_t      *p_sys = out->p_sys;
    input_thread_t    *p_input = p_sys->p_input;
    es_out_pgrm_t     *p_pgrm = NULL;
    char *psz_cat;
    int i;

    /* Find program */
    for( i = 0; i < p_sys->i_pgrm; i++ )
    {
        if( p_sys->pgrm[i]->i_id == i_group )
        {
            p_pgrm = p_sys->pgrm[i];
            break;
        }
    }
    if( p_pgrm == NULL )
        p_pgrm = EsOutProgramAdd( out, i_group );   /* Create it */

    /* Merge EPG */
    if( !p_pgrm->p_epg )
        p_pgrm->p_epg = vlc_epg_New( p_pgrm->psz_name );
    vlc_epg_Merge( p_pgrm->p_epg, p_epg );

    /* Update info */
    psz_cat = EsOutProgramGetMetaName( p_pgrm );
#ifdef HAVE_LOCALTIME_R
    char *psz_epg;
    if( asprintf( &psz_epg, "EPG %s", psz_cat ) == -1 )
        psz_epg = NULL;
    input_Control( p_input, INPUT_DEL_INFO, psz_epg, NULL );
    msg_Dbg( p_input, "EsOutProgramEpg: number=%d name=%s", i_group, p_pgrm->p_epg->psz_name );
    for( i = 0; i < p_pgrm->p_epg->i_event; i++ )
    {
        const vlc_epg_event_t *p_evt = p_pgrm->p_epg->pp_event[i];
        time_t t_start = (time_t)p_evt->i_start;
        struct tm tm_start;
        char psz_start[128];

        localtime_r( &t_start, &tm_start );

        snprintf( psz_start, sizeof(psz_start), "%2.2d:%2.2d:%2.2d", tm_start.tm_hour, tm_start.tm_min, tm_start.tm_sec );
        if( p_evt->psz_short_description || p_evt->psz_description )
            input_Control( p_input, INPUT_ADD_INFO, psz_epg, psz_start, "%s (%2.2d:%2.2d) - %s",
                           p_evt->psz_name,
                           p_evt->i_duration/60/60, (p_evt->i_duration/60)%60,
                           p_evt->psz_short_description ? p_evt->psz_short_description : p_evt->psz_description );
        else
            input_Control( p_input, INPUT_ADD_INFO, psz_epg, psz_start, "%s (%2.2d:%2.2d)",
                           p_evt->psz_name,
                           p_evt->i_duration/60/60, (p_evt->i_duration/60)%60 );
    }
    free( psz_epg );
#endif
    /* Update now playing */
    if( p_pgrm->psz_now_playing )
        free( p_pgrm->psz_now_playing );
    p_pgrm->psz_now_playing = NULL;
    if( p_epg->p_current && p_epg->p_current->psz_name && *p_epg->p_current->psz_name )
        p_pgrm->psz_now_playing = strdup( p_epg->p_current->psz_name );

    if( p_pgrm == p_sys->p_pgrm )
        input_item_SetNowPlaying( p_input->p->input.p_item, p_pgrm->psz_now_playing );

    if( p_pgrm->psz_now_playing )
    {
        input_Control( p_input, INPUT_ADD_INFO, psz_cat,
            input_MetaTypeToLocalizedString(vlc_meta_NowPlaying),
            p_pgrm->psz_now_playing );
    }
    else
    {
        input_Control( p_input, INPUT_DEL_INFO, psz_cat,
            input_MetaTypeToLocalizedString(vlc_meta_NowPlaying) );
    }

    free( psz_cat );
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
    {
        audio_replay_gain_t rg;

        es->i_channel = p_sys->i_audio;

        vlc_mutex_lock( &p_input->p->input.p_item->lock );
        memset( &rg, 0, sizeof(rg) );
        vlc_audio_replay_gain_MergeFromMeta( &rg, p_input->p->input.p_item->p_meta );
        vlc_mutex_unlock( &p_input->p->input.p_item->lock );

        for( i = 0; i < AUDIO_REPLAY_GAIN_MAX; i++ )
        {
            if( !es->fmt.audio_replay_gain.pb_peak[i] )
            {
                es->fmt.audio_replay_gain.pb_peak[i] = rg.pb_peak[i];
                es->fmt.audio_replay_gain.pf_peak[i] = rg.pf_peak[i];
            }
            if( !es->fmt.audio_replay_gain.pb_gain[i] )
            {
                es->fmt.audio_replay_gain.pb_gain[i] = rg.pb_gain[i];
                es->fmt.audio_replay_gain.pf_gain[i] = rg.pf_gain[i];
            }
        }
        break;
    }

    case VIDEO_ES:
        es->i_channel = p_sys->i_video;
        if( fmt->video.i_frame_rate && fmt->video.i_frame_rate_base )
            vlc_ureduce( &es->fmt.video.i_frame_rate,
                         &es->fmt.video.i_frame_rate_base,
                         fmt->video.i_frame_rate,
                         fmt->video.i_frame_rate_base, 0 );
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
    for( i = 0; i < 4; i++ )
        es->pb_cc_present[i] = VLC_FALSE;
    es->p_master = VLC_FALSE;

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

static vlc_bool_t EsIsSelected( es_out_t *out, es_out_id_t *es )
{
    if( es->p_master )
    {
        vlc_bool_t b_decode = VLC_FALSE;
        if( es->p_master->p_dec )
            input_DecoderGetCcState( es->p_master->p_dec, &b_decode,
                                     es->fmt.i_codec == VLC_FOURCC('c','c','1',' ') ? 0 : 1 );
        return b_decode;
    }
    else
    {
        return es->p_dec != NULL;
    }
}
static void EsSelect( es_out_t *out, es_out_id_t *es )
{
    es_out_sys_t   *p_sys = out->p_sys;
    input_thread_t *p_input = p_sys->p_input;
    vlc_value_t    val;
    const char     *psz_var;

    if( EsIsSelected( out, es ) )
    {
        msg_Warn( p_input, "ES 0x%x is already selected", es->i_id );
        return;
    }

    if( es->p_master )
    {
        if( !es->p_master->p_dec )
            return;

        if( input_DecoderSetCcState( es->p_master->p_dec, VLC_TRUE,
                                     es->fmt.i_codec == VLC_FOURCC('c','c','1',' ') ? 0 : 1 ) )
            return;
    }
    else
    {
        if( es->fmt.i_cat == VIDEO_ES || es->fmt.i_cat == SPU_ES )
        {
            if( !var_GetBool( p_input, "video" ) ||
                ( p_input->p->p_sout && !var_GetBool( p_input, "sout-video" ) ) )
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
                ( p_input->p->p_sout && !var_GetBool( p_input, "sout-audio" ) ) )
            {
                msg_Dbg( p_input, "audio is disabled, not selecting ES 0x%x",
                         es->i_id );
                return;
            }
        }
        if( es->fmt.i_cat == SPU_ES )
        {
            var_Get( p_input, "spu", &val );
            if( !var_GetBool( p_input, "spu" ) ||
                ( p_input->p->p_sout && !var_GetBool( p_input, "sout-spu" ) ) )
            {
                msg_Dbg( p_input, "spu is disabled, not selecting ES 0x%x",
                         es->i_id );
                return;
            }
        }

        es->i_preroll_end = -1;
        es->p_dec = input_DecoderNew( p_input, &es->fmt, VLC_FALSE );
        if( es->p_dec == NULL || es->p_pgrm != p_sys->p_pgrm )
            return;
    }

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
    const char     *psz_var;

    if( !EsIsSelected( out, es ) )
    {
        msg_Warn( p_input, "ES 0x%x is already unselected", es->i_id );
        return;
    }

    if( es->p_master )
    {
        if( es->p_master->p_dec )
            input_DecoderSetCcState( es->p_master->p_dec, VLC_FALSE, es->fmt.i_codec == VLC_FOURCC('c','c','1',' ') ? 0 : 1 );
    }
    else
    {
        const int i_spu_id = var_GetInteger( p_input, "spu-es");
        int i;
        for( i = 0; i < 4; i++ )
        {
            if( !es->pb_cc_present[i] || !es->pp_cc_es[i] )
                continue;

            if( i_spu_id == es->pp_cc_es[i]->i_id )
            {
                /* Force unselection of the CC */
                val.i_int = -1;
                var_Change( p_input, "spu-es", VLC_VAR_SETVALUE, &val, NULL );
                if( !b_update )
                    var_SetBool( p_sys->p_input, "intf-change", VLC_TRUE );
            }
            EsOutDel( out, es->pp_cc_es[i] );

            es->pb_cc_present[i] = VLC_FALSE;
        }
        input_DecoderDelete( es->p_dec );
        es->p_dec = NULL;
    }

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

    /* Mark it as unselected */
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
        if( !EsIsSelected( out, es ) )
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
                if( !EsIsSelected( out, es ) )
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

            if( p_sys->i_audio_id >= 0 )
            {
                if( es->i_id == p_sys->i_audio_id )
                    i_wanted = es->i_channel;
                else
                    return;
            }
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
            else if( p_sys->i_default_sub_id >= 0 )
            {
                if( es->i_id == p_sys->i_default_sub_id )
                    i_wanted = es->i_channel;
            }

            if( p_sys->i_sub_last >= 0 )
                i_wanted  = p_sys->i_sub_last;

            if( p_sys->i_sub_id >= 0 )
            {
                if( es->i_id == p_sys->i_sub_id )
                    i_wanted = es->i_channel;
                else
                    return;
            }
        }
        else if( i_cat == VIDEO_ES )
        {
            i_wanted  = es->i_channel;
        }

        if( i_wanted == es->i_channel && !EsIsSelected( out, es ) )
            EsSelect( out, es );
    }

    /* FIXME TODO handle priority here */
    if( EsIsSelected( out, es ) )
    {
        if( i_cat == AUDIO_ES )
        {
            if( p_sys->i_mode == ES_OUT_MODE_AUTO &&
                p_sys->p_es_audio &&
                p_sys->p_es_audio != es &&
                EsIsSelected( out, p_sys->p_es_audio ) )
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
                EsIsSelected( out, p_sys->p_es_sub ) )
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
    int i_total=0;

    if( es->fmt.i_cat == AUDIO_ES )
        i_delay = p_sys->i_audio_delay;
    else if( es->fmt.i_cat == SPU_ES )
        i_delay = p_sys->i_spu_delay;
    else
        i_delay = 0;

    if( p_input->p_libvlc->b_stats )
    {
        vlc_mutex_lock( &p_input->p->counters.counters_lock );
        stats_UpdateInteger( p_input, p_input->p->counters.p_demux_read,
                             p_block->i_buffer, &i_total );
        stats_UpdateFloat( p_input , p_input->p->counters.p_demux_bitrate,
                           (float)i_total, NULL );
        vlc_mutex_unlock( &p_input->p->counters.counters_lock );
    }

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

    if( p_block->i_dts > 0 && (p_block->i_flags&BLOCK_FLAG_PREROLL) )
    {
        p_block->i_dts += i_delay;
    }
    else if( p_block->i_dts > 0 )
    {
        p_block->i_dts =
            input_ClockGetTS( p_input, &p_pgrm->clock, p_block->i_dts ) + i_delay;
    }
    if( p_block->i_pts > 0 && (p_block->i_flags&BLOCK_FLAG_PREROLL) )
    {
        p_block->i_pts += i_delay;
    }
    else if( p_block->i_pts > 0 )
    {
        p_block->i_pts =
            input_ClockGetTS( p_input, &p_pgrm->clock, p_block->i_pts ) + i_delay;
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

    p_block->i_rate = p_input->p->i_rate;

    /* TODO handle mute */
    if( es->p_dec &&
        ( es->fmt.i_cat != AUDIO_ES ||
          ( p_input->p->i_rate >= INPUT_RATE_DEFAULT/AOUT_MAX_INPUT_RATE &&
            p_input->p->i_rate <= INPUT_RATE_DEFAULT*AOUT_MAX_INPUT_RATE ) ) )
    {
        vlc_bool_t pb_cc[4];
        vlc_bool_t b_cc_new = VLC_FALSE;
        int i;
        input_DecoderDecode( es->p_dec, p_block );

        /* Check CC status */
        input_DecoderIsCcPresent( es->p_dec, pb_cc );
        for( i = 0; i < 4; i++ )
        {
            static const vlc_fourcc_t fcc[4] = {
                VLC_FOURCC('c', 'c', '1', ' '),
                VLC_FOURCC('c', 'c', '2', ' '),
                VLC_FOURCC('c', 'c', '3', ' '),
                VLC_FOURCC('c', 'c', '4', ' '),
            };
            static const char *ppsz_description[4] = {
                N_("Closed captions 1"),
                N_("Closed captions 2"),
                N_("Closed captions 3"),
                N_("Closed captions 4"),
            };
            es_format_t fmt;

            if(  es->pb_cc_present[i] || !pb_cc[i] )
                continue;
            msg_Dbg( p_input, "Adding CC track %d for es[%d]", 1+i, es->i_id );

            es_format_Init( &fmt, SPU_ES, fcc[i] );
            fmt.i_group = es->fmt.i_group;
            fmt.psz_description = strdup( _(ppsz_description[i] ) );
            es->pp_cc_es[i] = EsOutAdd( out, &fmt );
            es->pp_cc_es[i]->p_master = es;
            es_format_Clean( &fmt );

            /* */
            es->pb_cc_present[i] = VLC_TRUE;
            b_cc_new = VLC_TRUE;
        }
        if( b_cc_new )
            var_SetBool( p_sys->p_input, "intf-change", VLC_TRUE );
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
    vlc_bool_t b_reselect = VLC_FALSE;
    int i;

    /* We don't try to reselect */
    if( es->p_dec )
    {
        while( !out->p_sys->p_input->b_die && es->p_dec )
        {
            if( input_DecoderEmpty( es->p_dec ) )
                break;
            msleep( 20*1000 );
        }
        EsUnselect( out, es, es->p_pgrm == p_sys->p_pgrm );
    }

    if( es->p_pgrm == p_sys->p_pgrm )
        EsOutESVarUpdate( out, es, VLC_TRUE );

    TAB_REMOVE( p_sys->i_es, p_sys->es, es );

    es->p_pgrm->i_es--;
    if( es->p_pgrm->i_es == 0 )
    {
        msg_Dbg( p_sys->p_input, "Program doesn't contain anymore ES" );
    }

    if( p_sys->p_es_audio == es || p_sys->p_es_video == es ||
        p_sys->p_es_sub == es ) b_reselect = VLC_TRUE;

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

    /* Re-select another track when needed */
    if( b_reselect )
        for( i = 0; i < p_sys->i_es; i++ )
        {
            if( es->fmt.i_cat == p_sys->es[i]->fmt.i_cat )
                EsOutSelect( out, p_sys->es[i], VLC_FALSE );
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
            if( b && !EsIsSelected( out, es ) )
            {
                EsSelect( out, es );
                return EsIsSelected( out, es ) ? VLC_SUCCESS : VLC_EGENERIC;
            }
            else if( !b && EsIsSelected( out, es ) )
            {
                EsUnselect( out, es, es->p_pgrm == p_sys->p_pgrm );
                return VLC_SUCCESS;
            }
            return VLC_SUCCESS;

        case ES_OUT_GET_ES_STATE:
            es = (es_out_id_t*) va_arg( args, es_out_id_t * );
            pb = (vlc_bool_t*) va_arg( args, vlc_bool_t * );

            *pb = EsIsSelected( out, es );
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
                    if( EsIsSelected( out, p_sys->es[i] ) )
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
                    if( EsIsSelected( out, p_sys->es[i] ) )
                        EsUnselect( out, p_sys->es[i],
                                    p_sys->es[i]->p_pgrm == p_sys->p_pgrm );
                }
            }
            else if( es == (es_out_id_t*)((uint8_t*)NULL+AUDIO_ES) )
            {
                for( i = 0; i < p_sys->i_es; i++ )
                {
                    if( p_sys->es[i]->fmt.i_cat == AUDIO_ES &&
                        EsIsSelected( out, p_sys->es[i] ) )
                        EsUnselect( out, p_sys->es[i],
                                    p_sys->es[i]->p_pgrm == p_sys->p_pgrm );
                }
            }
            else if( es == (es_out_id_t*)((uint8_t*)NULL+VIDEO_ES) )
            {
                for( i = 0; i < p_sys->i_es; i++ )
                {
                    if( p_sys->es[i]->fmt.i_cat == VIDEO_ES &&
                        EsIsSelected( out, p_sys->es[i] ) )
                        EsUnselect( out, p_sys->es[i],
                                    p_sys->es[i]->p_pgrm == p_sys->p_pgrm );
                }
            }
            else if( es == (es_out_id_t*)((uint8_t*)NULL+SPU_ES) )
            {
                for( i = 0; i < p_sys->i_es; i++ )
                {
                    if( p_sys->es[i]->fmt.i_cat == SPU_ES &&
                        EsIsSelected( out, p_sys->es[i] ) )
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
            {
                playlist_t * p_playlist = pl_Yield( p_sys->p_input );
                PL_LOCK;
                p_playlist->gc_date = mdate();
                vlc_object_signal_unlocked( p_playlist );
                PL_UNLOCK;
                pl_Release( p_playlist );
            }
            return VLC_SUCCESS;
 
        case ES_OUT_SET_DEFAULT:
        {
            es = (es_out_id_t*) va_arg( args, es_out_id_t * );

            if( es == NULL )
            {
                /*p_sys->i_default_video_id = -1;*/
                /*p_sys->i_default_audio_id = -1;*/
                p_sys->i_default_sub_id = -1;
            }
            else if( es == (es_out_id_t*)((uint8_t*)NULL+AUDIO_ES) )
            {
                /*p_sys->i_default_video_id = -1;*/
            }
            else if( es == (es_out_id_t*)((uint8_t*)NULL+VIDEO_ES) )
            {
                /*p_sys->i_default_audio_id = -1;*/
            }
            else if( es == (es_out_id_t*)((uint8_t*)NULL+SPU_ES) )
            {
                p_sys->i_default_sub_id = -1;
            }
            else
            {
                /*if( es->fmt.i_cat == VIDEO_ES )
                    p_sys->i_default_video_id = es->i_id;
                else
                if( es->fmt.i_cat == AUDIO_ES )
                    p_sys->i_default_audio_id = es->i_id;
                else*/
                if( es->fmt.i_cat == SPU_ES )
                    p_sys->i_default_sub_id = es->i_id;
            }
            return VLC_SUCCESS;
        }

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
            input_ClockSetPCR( p_sys->p_input, &p_pgrm->clock, i_pcr );
            return VLC_SUCCESS;
        }

        case ES_OUT_RESET_PCR:
            for( i = 0; i < p_sys->i_pgrm; i++ )
                input_ClockResetPCR( p_sys->p_input, &p_sys->pgrm[i]->clock );
            return VLC_SUCCESS;

        case ES_OUT_GET_TS:
            if( p_sys->p_pgrm )
            {
                int64_t i_ts = (int64_t)va_arg( args, int64_t );
                int64_t *pi_ts = (int64_t *)va_arg( args, int64_t * );
                *pi_ts = input_ClockGetTS( p_sys->p_input,
                                           &p_sys->p_pgrm->clock, i_ts );
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

            /* XXX We should call input_ClockGetTS but PCR has been reseted
             * and it will return 0, so we won't call input_ClockGetTS on all preroll samples
             * but that's ugly(more time discontinuity), it need to be improved -- fenrir */
            es->i_preroll_end = i_date;

            return VLC_SUCCESS;
        }
        case ES_OUT_SET_GROUP_META:
        {
            int i_group = (int)va_arg( args, int );
            vlc_meta_t *p_meta = (vlc_meta_t*)va_arg( args, vlc_meta_t * );

            EsOutProgramMeta( out, i_group, p_meta );
            return VLC_SUCCESS;
        }
        case ES_OUT_SET_GROUP_EPG:
        {
            int i_group = (int)va_arg( args, int );
            vlc_epg_t *p_epg = (vlc_epg_t*)va_arg( args, vlc_epg_t * );

            EsOutProgramEpg( out, i_group, p_epg );
            return VLC_SUCCESS;
        }
        case ES_OUT_DEL_GROUP:
        {
            int i_group = (int)va_arg( args, int );

            return EsOutProgramDel( out, i_group );
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

    if( psz_langs == NULL ) return NULL;

    psz_parser = psz_dup = strdup(psz_langs);

    while( psz_parser && *psz_parser )
    {
        char *psz;
        char *psz_code;

        psz = strchr(psz_parser, ',' );
        if( psz ) *psz++ = '\0';

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

    free( psz_dup );
    return ppsz;
}

static int LanguageArrayIndex( char **ppsz_langs, char *psz_lang )
{
    int i;

    if( !ppsz_langs || !psz_lang ) return -1;

    for( i = 0; ppsz_langs[i]; i++ )
        if( !strcasecmp( ppsz_langs[i], psz_lang ) ) return i;

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
    lldiv_t         div;

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
                           "%u", fmt->audio.i_channels );

        if( fmt->audio.i_rate > 0 )
        {
            input_Control( p_input, INPUT_ADD_INFO, psz_cat, _("Sample rate"),
                           _("%u Hz"), fmt->audio.i_rate );
            var_SetInteger( p_input, "sample-rate", fmt->audio.i_rate );
        }

        if( fmt->audio.i_bitspersample > 0 )
            input_Control( p_input, INPUT_ADD_INFO, psz_cat,
                           _("Bits per sample"), "%u",
                           fmt->audio.i_bitspersample );

        if( fmt->i_bitrate > 0 )
        {
            input_Control( p_input, INPUT_ADD_INFO, psz_cat, _("Bitrate"),
                           _("%u kb/s"), fmt->i_bitrate / 1000 );
            var_SetInteger( p_input, "bit-rate", fmt->i_bitrate );
        }
        break;

    case VIDEO_ES:
        input_Control( p_input, INPUT_ADD_INFO, psz_cat,
                       _("Type"), _("Video") );

        if( fmt->video.i_width > 0 && fmt->video.i_height > 0 )
            input_Control( p_input, INPUT_ADD_INFO, psz_cat,
                           _("Resolution"), "%ux%u",
                           fmt->video.i_width, fmt->video.i_height );

        if( fmt->video.i_visible_width > 0 &&
            fmt->video.i_visible_height > 0 )
            input_Control( p_input, INPUT_ADD_INFO, psz_cat,
                           _("Display resolution"), "%ux%u",
                           fmt->video.i_visible_width,
                           fmt->video.i_visible_height);
       if( fmt->video.i_frame_rate > 0 &&
           fmt->video.i_frame_rate_base > 0 )
       {
           div = lldiv( (float)fmt->video.i_frame_rate /
                               fmt->video.i_frame_rate_base * 1000000,
                               1000000 );
           input_Control( p_input, INPUT_ADD_INFO, psz_cat,
                          _("Frame rate"), I64Fd".%06u",
                          div.quot, (unsigned int )div.rem );
       }
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
