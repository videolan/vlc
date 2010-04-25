/*****************************************************************************
 * postprocessing.c
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

#include "postprocessing.h"

static bool PostProcessIsPresent( const char *psz_filter )
{
    const char  *psz_pp = "postproc";
    const size_t i_pp = strlen(psz_pp);
    return psz_filter &&
           !strncmp( psz_filter, psz_pp, strlen(psz_pp) ) &&
           ( psz_filter[i_pp] == '\0' || psz_filter[i_pp] == ':' );
}

static int PostProcessCallback( vlc_object_t *p_this, char const *psz_cmd,
                                vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval); VLC_UNUSED(p_data);

    static const char *psz_pp = "postproc";

    char *psz_vf2 = var_GetString( p_vout, "video-filter" );

    if( newval.i_int <= 0 )
    {
        if( PostProcessIsPresent( psz_vf2 ) )
        {
            strcpy( psz_vf2, &psz_vf2[strlen(psz_pp)] );
            if( *psz_vf2 == ':' )
                strcpy( psz_vf2, &psz_vf2[1] );
        }
    }
    else
    {
        if( !PostProcessIsPresent( psz_vf2 ) )
        {
            if( psz_vf2 )
            {
                char *psz_tmp = psz_vf2;
                if( asprintf( &psz_vf2, "%s:%s", psz_pp, psz_tmp ) < 0 )
                    psz_vf2 = psz_tmp;
                else
                    free( psz_tmp );
            }
            else
            {
                psz_vf2 = strdup( psz_pp );
            }
        }
    }
    if( newval.i_int > 0 )
        var_SetInteger( p_vout, "postproc-q", newval.i_int );
    if( psz_vf2 )
    {
        var_SetString( p_vout, "video-filter", psz_vf2 );
        free( psz_vf2 );
    }
    else if( newval.i_int > 0 )
    {
        var_TriggerCallback( p_vout, "video-filter" );
    }
    return VLC_SUCCESS;
}
static void PostProcessEnable( vout_thread_t *p_vout )
{
    vlc_value_t text;
    msg_Dbg( p_vout, "Post-processing available" );
    var_Create( p_vout, "postprocess", VLC_VAR_INTEGER | VLC_VAR_HASCHOICE );
    text.psz_string = _("Post processing");
    var_Change( p_vout, "postprocess", VLC_VAR_SETTEXT, &text, NULL );

    for( int i = 0; i <= 6; i++ )
    {
        vlc_value_t val;
        vlc_value_t text;
        char psz_text[1+1];

        val.i_int = i;
        snprintf( psz_text, sizeof(psz_text), "%d", i );
        if( i == 0 )
            text.psz_string = _("Disable");
        else
            text.psz_string = psz_text;
        var_Change( p_vout, "postprocess", VLC_VAR_ADDCHOICE, &val, &text );
    }
    var_AddCallback( p_vout, "postprocess", PostProcessCallback, NULL );

    /* */
    char *psz_filter = var_GetNonEmptyString( p_vout, "video-filter" );
    int i_postproc_q = 0;
    if( PostProcessIsPresent( psz_filter ) )
        i_postproc_q = var_CreateGetInteger( p_vout, "postproc-q" );

    var_SetInteger( p_vout, "postprocess", i_postproc_q );

    free( psz_filter );
}
static void PostProcessDisable( vout_thread_t *p_vout )
{
    msg_Dbg( p_vout, "Post-processing no more available" );
    var_Destroy( p_vout, "postprocess" );
}

void vout_SetPostProcessingState(vout_thread_t *vout, vout_postprocessing_support_t *state, int qtype)
{
	const int postproc_change = (qtype != QTYPE_NONE) - (state->qtype != QTYPE_NONE);
	if (postproc_change == 1)
		PostProcessEnable(vout);
	else if (postproc_change == -1)
		PostProcessDisable(vout);
	if (postproc_change)
		state->qtype = qtype;
}

