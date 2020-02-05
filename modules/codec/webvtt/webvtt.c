/*****************************************************************************
 * webvtt.c: WEBVTT shared code
 *****************************************************************************
 * Copyright (C) 2017 VideoLabs, VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_charset.h>
#include <vlc_plugin.h>

#include "webvtt.h"

#include <ctype.h>
#include <assert.h>

/*****************************************************************************
 * Modules descriptor.
 *****************************************************************************/

vlc_module_begin ()
    set_capability( "spu decoder", 10 )
    set_shortname( N_("WEBVTT decoder"))
    set_description( N_("WEBVTT subtitles decoder") )
    set_callbacks( webvtt_OpenDecoder, webvtt_CloseDecoder )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_SCODEC )
    add_submodule()
        set_shortname( "WEBVTT" )
        set_description( N_("WEBVTT subtitles parser") )
        set_capability( "demux", 11 )
        set_category( CAT_INPUT )
        set_subcategory( SUBCAT_INPUT_DEMUX )
        set_callbacks( webvtt_OpenDemux, webvtt_CloseDemux )
        add_shortcut( "webvtt" )
    add_submodule()
        set_shortname( "WEBVTT" )
        set_description( N_("WEBVTT subtitles parser") )
        set_capability( "demux", 0 )
        set_category( CAT_INPUT )
        set_subcategory( SUBCAT_INPUT_DEMUX )
        set_callbacks( webvtt_OpenDemuxStream, webvtt_CloseDemux )
        add_shortcut( "webvttstream" )
#ifdef ENABLE_SOUT
    add_submodule()
        set_description( "WEBVTT text encoder" )
        set_capability( "encoder", 101 )
        set_subcategory( SUBCAT_INPUT_SCODEC )
        set_callbacks( webvtt_OpenEncoder, webvtt_CloseEncoder )
#endif
vlc_module_end ()

struct webvtt_text_parser_t
{
    enum
    {
        WEBVTT_SECTION_UNDEFINED = WEBVTT_HEADER_STYLE - 1,
        WEBVTT_SECTION_STYLE = WEBVTT_HEADER_STYLE,
        WEBVTT_SECTION_REGION = WEBVTT_HEADER_REGION,
        WEBVTT_SECTION_NOTE,
        WEBVTT_SECTION_CUES,
    } section;
    char * reads[3];

    void * priv;
    webvtt_cue_t *(*pf_get_cue)( void * );
    void (*pf_cue_done)( void *, webvtt_cue_t * );
    void (*pf_header)( void *, enum webvtt_header_line_e, bool, const char * );

    webvtt_cue_t *p_cue;
};

static vlc_tick_t MakeTime( unsigned t[4] )
{
    return vlc_tick_from_sec( t[0] * 3600 + t[1] * 60 + t[2] ) +
           VLC_TICK_FROM_MS(t[3]);
}

bool webvtt_scan_time( const char *psz, vlc_tick_t *p_time )
{
    unsigned t[4];
    if( sscanf( psz, "%2u:%2u.%3u",
                      &t[1], &t[2], &t[3] ) == 3 )
    {
        t[0] = 0;
        *p_time = MakeTime( t );
        return true;
    }
    else if( sscanf( psz, "%u:%2u:%2u.%3u",
                          &t[0], &t[1], &t[2], &t[3] ) == 4 )
    {
        *p_time = MakeTime( t );
        return true;
    }
    else return false;
}

static bool KeywordMatch( const char *psz, const char *keyword )
{
    const size_t i_len = strlen(keyword);
    return( !strncmp( keyword, psz, i_len ) && (!psz[i_len] || isspace(psz[i_len])) );
}

/*

*/

webvtt_text_parser_t * webvtt_text_parser_New( void *priv,
                    webvtt_cue_t *(*pf_get_cue)( void * ),
                    void (*pf_cue_done)( void *, webvtt_cue_t * ),
                    void (*pf_header)( void *, enum webvtt_header_line_e, bool, const char * ) )
{
    webvtt_text_parser_t *p = malloc(sizeof(*p));
    if( p )
    {
        p->section = WEBVTT_SECTION_UNDEFINED;
        for( int i=0; i<3; i++ )
            p->reads[i] = NULL;
        p->p_cue = NULL;
        p->priv = priv;
        p->pf_cue_done = pf_cue_done;
        p->pf_get_cue = pf_get_cue;
        p->pf_header = pf_header;
    }
    return p;
}

void webvtt_text_parser_Delete( webvtt_text_parser_t *p )
{
    for( int i=0; i<3; i++ )
        free( p->reads[i] );
    free( p );
}

static void forward_line( webvtt_text_parser_t *p, const char *psz_line, bool b_new )
{
    if( p->pf_header )
        p->pf_header( p->priv, (enum webvtt_header_line_e)p->section,
                      b_new, psz_line );
}

void webvtt_text_parser_Feed( webvtt_text_parser_t *p, char *psz_line )
{
    if( psz_line == NULL )
    {
        if( p->p_cue )
        {
            if( p->pf_cue_done )
                p->pf_cue_done( p->priv, p->p_cue );
            p->p_cue = NULL;
        }
        return;
    }

    free(p->reads[0]);
    p->reads[0] = p->reads[1];
    p->reads[1] = p->reads[2];
    p->reads[2] = psz_line;

    /* Lookup keywords */
    if( unlikely(p->section == WEBVTT_SECTION_UNDEFINED) )
    {
        if( KeywordMatch( psz_line, "\xEF\xBB\xBFWEBVTT" ) ||
            KeywordMatch( psz_line, "WEBVTT" )  )
        {
            p->section = WEBVTT_SECTION_UNDEFINED;
            if( p->p_cue )
            {
                if( p->pf_cue_done )
                    p->pf_cue_done( p->priv, p->p_cue );
                p->p_cue = NULL;
            }
            return;
        }
        else if( KeywordMatch( psz_line, "STYLE" ) )
        {
            p->section = WEBVTT_SECTION_STYLE;
            forward_line( p, psz_line, true );
            return;
        }
        else if( KeywordMatch( psz_line, "REGION" ) )
        {
            p->section = WEBVTT_SECTION_REGION;
            forward_line( p, psz_line, true );
            return;
        }
        else if( KeywordMatch( psz_line, "NOTE" ) )
        {
            p->section = WEBVTT_SECTION_NOTE;
            return;
        }
        else if( psz_line[0] != 0 )
        {
            p->section = WEBVTT_SECTION_CUES;
        }
    }

    if( likely(p->section == WEBVTT_SECTION_CUES) )
    {
        if( p->p_cue )
        {
            if( psz_line[0] == 0 )
            {
                if( p->p_cue )
                {
                    if( p->pf_cue_done )
                        p->pf_cue_done( p->priv, p->p_cue );
                    p->p_cue = NULL;
                }
            }
            else
            {
                char *psz_merged;
                if( -1 < asprintf( &psz_merged, "%s\n%s", p->p_cue->psz_text, psz_line ) )
                {
                    free( p->p_cue->psz_text );
                    p->p_cue->psz_text = psz_merged;
                }
                return;
            }
        }

        if( p->reads[1] == NULL )
            return;

        const char *psz_split = strstr( p->reads[1], " --> " );
        if( psz_split )
        {
            vlc_tick_t i_start, i_stop;

            if( webvtt_scan_time( p->reads[1], &i_start ) &&
                webvtt_scan_time( psz_split + 5,  &i_stop ) && i_start <= i_stop )
            {
                const char *psz_attrs = strchr( psz_split + 5 + 5, ' ' );
                p->p_cue = ( p->pf_get_cue ) ? p->pf_get_cue( p->priv ) : NULL;
                if( p->p_cue )
                {
                    p->p_cue->psz_attrs = ( psz_attrs ) ? strdup( psz_attrs ) : NULL;
                    p->p_cue->psz_id = p->reads[0];
                    p->reads[0] = NULL;
                    p->p_cue->psz_text = p->reads[2];
                    p->reads[2] = NULL;
                    p->p_cue->i_start = i_start;
                    p->p_cue->i_stop = i_stop;
                }
            }
        }
    }
    else if( p->section == WEBVTT_SECTION_STYLE )
    {
        forward_line( p, psz_line, false );
        if( psz_line[0] == 0 )
            p->section = WEBVTT_SECTION_UNDEFINED;
    }
    else if( p->section == WEBVTT_SECTION_REGION )
    {
        forward_line( p, psz_line, false );
        if( psz_line[0] == 0 ) /* End of region declaration */
            p->section = WEBVTT_SECTION_UNDEFINED;
    }
    else if( p->section == WEBVTT_SECTION_NOTE )
    {
        if( psz_line[0] == 0 )
            p->section = WEBVTT_SECTION_UNDEFINED;
    }
}
