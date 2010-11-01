/*****************************************************************************
 * interlacing.c
 *****************************************************************************
 * Copyright (C) 2010 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <assert.h>

#include <vlc_common.h>
#include <vlc_vout.h>

#include "interlacing.h"

/*****************************************************************************
 * Deinterlacing
 *****************************************************************************/
/* XXX
 * You can use the non vout filter if and only if the video properties stay the
 * same (width/height/chroma/fps), at least for now.
 */
static const char *deinterlace_modes[] = {
    ""
    "discard",
    "blend",
    "mean",
    "bob",
    "linear",
    "x",
    "yadif",
    "yadif2x",
    NULL
};
static bool DeinterlaceIsModeValid(const char *mode)
{
    for (unsigned i = 0; deinterlace_modes[i]; i++) {
        if( !strcmp(deinterlace_modes[i], mode))
            return true;
    }
    return false;
}

static char *FilterFind( char *psz_filter_base, const char *psz_module )
{
    const size_t i_module = strlen( psz_module );
    const char *psz_filter = psz_filter_base;

    if( !psz_filter || i_module <= 0 )
        return NULL;

    for( ;; )
    {
        char *psz_find = strstr( psz_filter, psz_module );
        if( !psz_find )
            return NULL;
        if( psz_find[i_module] == '\0' || psz_find[i_module] == ':' )
            return psz_find;
        psz_filter = &psz_find[i_module];
    }
}

static bool DeinterlaceIsPresent( vout_thread_t *p_vout )
{
    char *psz_filter = var_GetNonEmptyString( p_vout, "video-filter" );

    bool b_found = FilterFind( psz_filter, "deinterlace" ) != NULL;

    free( psz_filter );

    return b_found;
}

static void DeinterlaceRemove( vout_thread_t *p_vout )
{
    char *psz_filter = var_GetNonEmptyString( p_vout, "video-filter" );

    char *psz = FilterFind( psz_filter, "deinterlace" );
    if( !psz )
    {
        free( psz_filter );
        return;
    }

    /* */
    strcpy( &psz[0], &psz[strlen("deinterlace")] );
    if( *psz == ':' )
        strcpy( &psz[0], &psz[1] );

    var_SetString( p_vout, "video-filter", psz_filter );
    free( psz_filter );
}
static void DeinterlaceAdd( vout_thread_t *p_vout )
{
    char *psz_filter = var_GetNonEmptyString( p_vout, "video-filter" );

    if( FilterFind( psz_filter, "deinterlace" ) )
    {
        free( psz_filter );
        return;
    }

    /* */
    if( psz_filter )
    {
        char *psz_tmp = psz_filter;
        if( asprintf( &psz_filter, "%s:%s", psz_tmp, "deinterlace" ) < 0 )
            psz_filter = psz_tmp;
        else
            free( psz_tmp );
    }
    else
    {
        psz_filter = strdup( "deinterlace" );
    }

    if( psz_filter )
    {
        var_SetString( p_vout, "video-filter", psz_filter );
        free( psz_filter );
    }
}

static int DeinterlaceCallback( vlc_object_t *p_this, char const *psz_cmd,
                                vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval); VLC_UNUSED(newval); VLC_UNUSED(p_data);
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    /* */
    const int  i_deinterlace = var_GetInteger( p_this, "deinterlace" );
    char       *psz_mode     = var_GetString( p_this, "deinterlace-mode" );
    const bool is_needed     = var_GetBool( p_this, "deinterlace-needed" );
    if( !psz_mode || !DeinterlaceIsModeValid(psz_mode) )
        return VLC_EGENERIC;

    /* */
    char *psz_old = var_CreateGetString( p_vout, "sout-deinterlace-mode" );
    var_SetString( p_vout, "sout-deinterlace-mode", psz_mode );

    msg_Dbg( p_vout, "deinterlace %d, mode %s, is_needed %d", i_deinterlace, psz_mode, is_needed );
    if( i_deinterlace == 0 || ( i_deinterlace == -1 && !is_needed ) )
    {
        DeinterlaceRemove( p_vout );
    }
    else if( !DeinterlaceIsPresent( p_vout ) )
    {
        DeinterlaceAdd( p_vout );
    }
    else if( psz_old && strcmp( psz_old, psz_mode ) )
    {
        var_TriggerCallback( p_vout, "video-filter" );
    }

    /* */
    free( psz_old );
    free( psz_mode );
    return VLC_SUCCESS;
}

void vout_InitInterlacingSupport( vout_thread_t *p_vout, bool is_interlaced )
{
    vlc_value_t val, text;

    msg_Dbg( p_vout, "Deinterlacing available" );

    /* Create the configuration variables */
    /* */
    var_Create( p_vout, "deinterlace", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT | VLC_VAR_HASCHOICE );
    int i_deinterlace = var_GetInteger( p_vout, "deinterlace" );
    i_deinterlace = __MAX( __MIN( i_deinterlace, 1 ), -1 );

    text.psz_string = _("Deinterlace");
    var_Change( p_vout, "deinterlace", VLC_VAR_SETTEXT, &text, NULL );

    const module_config_t *p_optd = config_FindConfig( VLC_OBJECT(p_vout), "deinterlace" );
    var_Change( p_vout, "deinterlace", VLC_VAR_CLEARCHOICES, NULL, NULL );
    for( int i = 0; p_optd && i < p_optd->i_list; i++ )
    {
        val.i_int  = p_optd->pi_list[i];
        text.psz_string = (char*)vlc_gettext(p_optd->ppsz_list_text[i]);
        var_Change( p_vout, "deinterlace", VLC_VAR_ADDCHOICE, &val, &text );
    }
    var_AddCallback( p_vout, "deinterlace", DeinterlaceCallback, NULL );
    /* */
    var_Create( p_vout, "deinterlace-mode", VLC_VAR_STRING | VLC_VAR_DOINHERIT | VLC_VAR_HASCHOICE );
    char *psz_deinterlace = var_GetNonEmptyString( p_vout, "deinterlace-mode" );

    text.psz_string = _("Deinterlace mode");
    var_Change( p_vout, "deinterlace-mode", VLC_VAR_SETTEXT, &text, NULL );

    const module_config_t *p_optm = config_FindConfig( VLC_OBJECT(p_vout), "deinterlace-mode" );
    var_Change( p_vout, "deinterlace-mode", VLC_VAR_CLEARCHOICES, NULL, NULL );
    for( int i = 0; p_optm && i < p_optm->i_list; i++ )
    {
        if( !DeinterlaceIsModeValid( p_optm->ppsz_list[i] ) )
            continue;

        val.psz_string  = p_optm->ppsz_list[i];
        text.psz_string = (char*)vlc_gettext(p_optm->ppsz_list_text[i]);
        var_Change( p_vout, "deinterlace-mode", VLC_VAR_ADDCHOICE, &val, &text );
    }
    var_AddCallback( p_vout, "deinterlace-mode", DeinterlaceCallback, NULL );
    /* */
    var_Create( p_vout, "deinterlace-needed", VLC_VAR_BOOL );
    var_AddCallback( p_vout, "deinterlace-needed", DeinterlaceCallback, NULL );

    /* Override the initial value from filters if present */
    char *psz_filter_mode = NULL;
    if( DeinterlaceIsPresent( p_vout ) )
        psz_filter_mode = var_CreateGetNonEmptyString( p_vout, "sout-deinterlace-mode" );
    if( psz_filter_mode )
    {
        i_deinterlace = 1;
        free( psz_deinterlace );
        psz_deinterlace = psz_filter_mode;
    }

    /* */
    val.psz_string = psz_deinterlace ? psz_deinterlace : p_optm->orig.psz;
    var_Change( p_vout, "deinterlace-mode", VLC_VAR_SETVALUE, &val, NULL );
    val.b_bool = is_interlaced;
    var_Change( p_vout, "deinterlace-needed", VLC_VAR_SETVALUE, &val, NULL );

    var_SetInteger( p_vout, "deinterlace", i_deinterlace );
    free( psz_deinterlace );
}

void vout_SetInterlacingState( vout_thread_t *p_vout, vout_interlacing_support_t *state, bool is_interlaced )
{
     /* Wait 30s before quiting interlacing mode */
    const int interlacing_change = (!!is_interlaced) - (!!state->is_interlaced);
    if ((interlacing_change == 1) ||
        (interlacing_change == -1 && state->date + 30000000 < mdate())) {

        msg_Dbg( p_vout, "Detected %s video",
                 is_interlaced ? "interlaced" : "progressive" );
        var_SetBool( p_vout, "deinterlace-needed", is_interlaced );

        state->is_interlaced = is_interlaced;
    }
    if (is_interlaced)
        state->date = mdate();
}


