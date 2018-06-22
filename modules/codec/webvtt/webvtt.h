/*****************************************************************************
 * webvtt.h: WEBVTT shared code
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
#ifndef WEBVTT_H
#define WEBVTT_H

int  webvtt_OpenDecoder   ( vlc_object_t * );
void webvtt_CloseDecoder  ( vlc_object_t * );

int  webvtt_OpenDemux     ( vlc_object_t * );
int  webvtt_OpenDemuxStream (vlc_object_t *);
void webvtt_CloseDemux    ( vlc_object_t * );

#ifdef ENABLE_SOUT
int  webvtt_OpenEncoder   ( vlc_object_t * );
void webvtt_CloseEncoder  ( vlc_object_t * );
#endif

typedef struct webvtt_text_parser_t webvtt_text_parser_t;

enum webvtt_header_line_e
{
    WEBVTT_HEADER_STYLE = 1,
    WEBVTT_HEADER_REGION,
};

typedef struct
{
    vlc_tick_t i_start;
    vlc_tick_t i_stop;
    char *psz_id;
    char *psz_text;
    char *psz_attrs;
} webvtt_cue_t;

static inline void webvtt_cue_Init( webvtt_cue_t *c )
{
    memset( c, 0, sizeof(*c) );
}

static inline void webvtt_cue_Clean( webvtt_cue_t *c )
{
    free( c->psz_attrs );
    free( c->psz_text );
    free( c->psz_id );
}

webvtt_text_parser_t * webvtt_text_parser_New(
            void *priv,
            /* parser callback requesting a new cue to fill
             * set to NULL if no cues data required */
            webvtt_cue_t *(*pf_get_cue)( void * ),
            /* callback when cue is complete set to NULL for no callback */
            void (*pf_cue_done)( void *, webvtt_cue_t * ),
            /* parser callback returning headers
             * set to NULL for no callbacks */
            void (*pf_header)( void *, enum webvtt_header_line_e, bool, const char * )
        );
void webvtt_text_parser_Delete( webvtt_text_parser_t *p );
void webvtt_text_parser_Feed( webvtt_text_parser_t *p, char *psz_line );

bool webvtt_scan_time( const char *psz, vlc_tick_t *p_time );

#endif
