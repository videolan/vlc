/*****************************************************************************
 * access2 adaptation layer.
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
 * $Id: demux2.c 7783 2004-05-27 00:02:43Z hartman $
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
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/input.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Access2Open ( vlc_object_t * );
static void Access2Close( vlc_object_t * );

vlc_module_begin();
    set_description( _("Access2 adaptation layer" ) );
    set_capability( "access", 50 );
    set_callbacks( Access2Open, Access2Close );
    add_shortcut( "access2" );
    add_shortcut( "http" );
    add_shortcut( "ftp" );
    add_shortcut( "tcp" );
    add_shortcut( "pvr" );

    add_shortcut( "file" );
    add_shortcut( "stream" );
    add_shortcut( "kfir" );

    add_shortcut( "udp" );
    add_shortcut( "udp4" );
    add_shortcut( "udp6" );

    add_shortcut( "rtp" );
    add_shortcut( "rtp4" );
    add_shortcut( "rtp6" );

    add_shortcut( "cdda" );
    add_shortcut( "cddasimple" );

    add_shortcut( "mms" );
    add_shortcut( "mmsh" );
    add_shortcut( "mmst" );
    add_shortcut( "mmsu" );

    add_shortcut( "vcd" );
    add_shortcut( "svcd" );

    add_shortcut( "v4l" );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Access2Read   ( input_thread_t *, byte_t *, size_t );
static void Access2Seek   ( input_thread_t *, off_t );
static int  Access2SetArea( input_thread_t *, input_area_t * );
static int  Access2Control( input_thread_t *, int, va_list );

typedef struct
{
    access_t  *p_access;
    block_t   *p_block;

    int i_title;
    input_title_t **title;

    vlc_bool_t b_first_read;

} access2_sys_t;

static void UpdateInfo( input_thread_t *p_input, vlc_bool_t b_force_chapter );

/*****************************************************************************
 * Access2Open: initializes structures
 *****************************************************************************/
static int Access2Open( vlc_object_t * p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    access2_sys_t  *p_sys   = malloc( sizeof( access2_sys_t ) );
    access_t       *p_access;

    char           *psz_uri;
    int            i_int;
    int64_t        i_64;
    vlc_bool_t     b_bool;

    psz_uri = malloc( strlen( p_input->psz_access ) + strlen( p_input->psz_demux ) + strlen( p_input->psz_name  ) + 1 + 3 + 1 );
    if( p_input->psz_demux && *p_input->psz_demux )
    {
        sprintf( psz_uri, "%s/%s://%s", p_input->psz_access, p_input->psz_demux, p_input->psz_name );
    }
    else if( p_input->psz_access && *p_input->psz_access )
    {
        sprintf( psz_uri, "%s://%s", p_input->psz_access, p_input->psz_name );
    }
    else
    {
        sprintf( psz_uri, "://%s", p_input->psz_name );
    }

    p_access = access2_New( p_input, psz_uri );

    free( psz_uri );

    if( !p_access )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* Init p_input->* */
    p_input->pf_read           = Access2Read;
    p_input->pf_seek           = Access2Seek;
    p_input->pf_set_program    = input_SetProgram;
    p_input->pf_set_area       = Access2SetArea;
    p_input->pf_access_control = Access2Control;
    p_input->p_access_data = (access_sys_t*)p_sys;

    p_sys->p_access = p_access;
    p_sys->p_block = NULL;
    p_sys->b_first_read = VLC_TRUE;

    /* mtu */
    access2_Control( p_access, ACCESS_GET_MTU, &i_int );
    p_input->i_mtu = i_int;
    /* pts delay */
    access2_Control( p_access, ACCESS_GET_PTS_DELAY, &i_64 );
    p_input->i_pts_delay = i_64;

    if( p_access->psz_demux && *p_access->psz_demux )
    {
        if( !p_input->psz_demux || *p_input->psz_demux == '\0' )
            p_input->psz_demux = strdup( p_access->psz_demux );
    }
    /* title */
    if( access2_Control( p_access, ACCESS_GET_TITLE_INFO,
                         &p_sys->title, &p_sys->i_title ) )
    {
        p_sys->i_title = 0;
        p_sys->title   = NULL;
    }

    /* Init p_input->stream.* */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( p_sys->i_title > 0 )
    {
        /* FIXME handle the area 0 */
        int64_t i_start = 0;
        int i;

        /* Initialize ES structures */
        input_InitStream( p_input, 0 );

#define area p_input->stream.pp_areas
        for( i = 0 ; i < p_sys->i_title ; i++ )
        {
            input_title_t *t = p_sys->title[i];

            input_AddArea( p_input, i+1,
                           t->i_seekpoint > 0 ? t->i_seekpoint : 1 );

            /* Absolute start offset and size */
            area[i]->i_start = i_start;
            area[i]->i_size  = t->i_size;

            i_start += t->i_size;
        }
#undef area

        /* Set the area */
        Access2SetArea( p_input, p_input->stream.pp_areas[1+p_access->info.i_title] );
    }

    /* size */
    p_input->stream.p_selected_area->i_size  = p_access->info.i_size;
    /* seek */
    access2_Control( p_access, ACCESS_CAN_SEEK, &b_bool );
    p_input->stream.b_seekable = b_bool;
    /* pace */
    access2_Control( p_access, ACCESS_CAN_CONTROL_PACE, &b_bool );
    p_input->stream.b_pace_control = b_bool;
   /* End of init */
    access2_Control( p_access, ACCESS_CAN_FASTSEEK, &b_bool );
    if( b_bool )
        p_input->stream.i_method = INPUT_METHOD_FILE;   /* FIXME */
    else
        p_input->stream.i_method = INPUT_METHOD_NETWORK;/* FIXME */
    p_input->stream.p_selected_area->i_tell = p_access->info.i_pos;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Access2Close: frees unused data
 *****************************************************************************/
static void Access2Close( vlc_object_t * p_this )
{
    input_thread_t *p_input = (input_thread_t*)p_this;
    access2_sys_t  *p_sys = (access2_sys_t*)p_input->p_access_data;
    int i;

    access2_Delete( p_sys->p_access );

    for( i = 0; i < p_sys->i_title; i++ )
    {
        vlc_input_title_Delete( p_sys->title[i] );
    }
    if( p_sys->title ) free( p_sys->title );

    if( p_sys->p_block )
        block_Release( p_sys->p_block );

    free( p_sys );
}

/*****************************************************************************
 * Access2Read: Read data
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, size otherwise
 *****************************************************************************/
static int  Access2Read( input_thread_t *p_input, byte_t *p_buffer, size_t i_len )
{
    access2_sys_t *p_sys = (access2_sys_t*)p_input->p_access_data;
    access_t      *p_access = p_sys->p_access;
    int           i_total = 0;

    /* TODO update i_size (ex: when read more than half current size and only every 100 times or ...) */

    if( p_access->pf_read )
    {
        i_total = p_access->pf_read( p_access, (uint8_t*)p_buffer, (int)i_len );
        goto update;
    }

    /* Should never occur */
    if( p_access->pf_block == NULL )
    {
        i_total = 0;
        goto update;
    }

    /* Emulate the read */
    while( i_total < i_len )
    {
        int i_copy;
        if( p_sys->p_block == NULL )
        {
            p_sys->p_block = p_access->pf_block( p_access );
        }
        if( p_sys->p_block == NULL )
        {
            if( !p_access->info.b_eof && i_total <= 0 )
                i_total = -1;
            goto update;
        }

        i_copy = __MIN( i_len - i_total, p_sys->p_block->i_buffer );
        if( i_copy > 0 )
        {
            memcpy( &p_buffer[i_total], p_sys->p_block->p_buffer, i_copy );

            p_sys->p_block->i_buffer -= i_copy;
            p_sys->p_block->p_buffer += i_copy;
        }

        if( p_sys->p_block->i_buffer <= 0 )
        {
            block_Release( p_sys->p_block );
            p_sys->p_block = NULL;
        }

        i_total += i_copy;
    }
update:
    if( p_sys->b_first_read )
    {
        /* Some access update it only after first read (like udp) */
        if( p_access->psz_demux && *p_access->psz_demux )
        {
            if( !p_input->psz_demux || *p_input->psz_demux == '\0' )
                p_input->psz_demux = strdup( p_access->psz_demux );
        }
        p_sys->b_first_read = VLC_FALSE;
    }

    vlc_mutex_lock( &p_input->stream.stream_lock );
    UpdateInfo( p_input, VLC_FALSE );
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return i_total;
}

/*****************************************************************************
 * Access2SetArea: initialize input data for title x.
 * It should be called for each user navigation request.
 ****************************************************************************/
static int Access2SetArea( input_thread_t * p_input, input_area_t * p_area )
{
    access2_sys_t *p_sys = (access2_sys_t*)p_input->p_access_data;
    access_t      *p_access = p_sys->p_access;
    int           i_seekpoint = p_area->i_part - 1;

    /* we can't use the interface slider until initilization is complete */
    p_input->stream.b_seekable = 0;

    if( p_area != p_input->stream.p_selected_area )
    {
        /* Call access2 control */
        if( access2_Control( p_access, ACCESS_SET_TITLE, (int)(p_area->i_id - 1) ) )
            return VLC_EGENERIC;

        p_input->stream.p_selected_area = p_area;
    }
    fprintf( stderr, "Access2SetArea: part=%d seekpart=%d nbseekpoint=%d\n",
             p_area->i_part, i_seekpoint, p_sys->title[p_access->info.i_title]->i_seekpoint );

    if( i_seekpoint != p_access->info.i_seekpoint &&
        i_seekpoint >= 0 &&
        i_seekpoint < p_sys->title[p_access->info.i_title]->i_seekpoint )
    {
        msg_Dbg( p_input, "setting seekpoint" );
        access2_Control( p_access, ACCESS_SET_SEEKPOINT, (int)i_seekpoint );
    }

    UpdateInfo( p_input, VLC_TRUE );

    /* warn interface that something has changed */
    p_input->stream.b_seekable = VLC_TRUE;
    p_input->stream.b_changed = VLC_TRUE;
    return VLC_EGENERIC;
}


/*****************************************************************************
 * Access2Seek:
 *****************************************************************************
 * Returns VLC_EGENERIC/VLC_SUCCESS
 *****************************************************************************/
static void Access2Seek( input_thread_t *p_input, off_t i_pos )
{
    access2_sys_t *p_sys = (access2_sys_t*)p_input->p_access_data;
    access_t      *p_access = p_sys->p_access;

    if( p_access->pf_seek != NULL && p_input->stream.b_seekable )
    {
        if( !p_access->pf_seek( p_access, i_pos ) )
        {
            if( p_sys->p_block )
            {
                block_Release( p_sys->p_block );
                p_sys->p_block = NULL;
            }

            vlc_mutex_lock( &p_input->stream.stream_lock );
            p_input->stream.p_selected_area->i_tell = p_access->info.i_pos;
            vlc_mutex_unlock( &p_input->stream.stream_lock );
        }
    }
}

/*****************************************************************************
 * Access2Control:
 *****************************************************************************
 * Returns VLC_EGENERIC/VLC_SUCCESS
 *****************************************************************************/
static int  Access2Control( input_thread_t *p_input, int i_query, va_list args )
{
    access2_sys_t *p_sys = (access2_sys_t*)p_input->p_access_data;
    access_t      *p_access = p_sys->p_access;

    return access2_vaControl( p_access, i_query, args );
}

/*****************************************************************************
 * UpdateInfo:
 *****************************************************************************/
static void UpdateInfo( input_thread_t *p_input, vlc_bool_t b_force_chapter )
{
    access2_sys_t *p_sys = (access2_sys_t*)p_input->p_access_data;
    access_t      *p_access = p_sys->p_access;

    /* Title change */
    if( p_access->info.i_update & INPUT_UPDATE_TITLE )
    {
        int i_seekpoint;
        int i;

        vlc_value_t val;
        msg_Dbg( p_input, "INPUT_UPDATE_TITLE" );

        p_input->stream.p_selected_area = p_input->stream.pp_areas[p_access->info.i_title+1];
        p_input->stream.p_selected_area->i_part = 1;
        p_input->stream.p_selected_area->i_tell = p_access->info.i_pos;
        p_input->stream.b_changed = VLC_TRUE;

        val.i_int = p_access->info.i_title+1;
        var_Change( p_input, "title", VLC_VAR_SETVALUE, &val, NULL );

        var_Change( p_input, "chapter", VLC_VAR_CLEARCHOICES, NULL, NULL );
        i_seekpoint = p_sys->title[p_access->info.i_title]->i_seekpoint;
        for( i = 0; i < i_seekpoint ? i_seekpoint : 0; i++ )
        {
            val.i_int = i + 1;
            var_Change( p_input, "chapter", VLC_VAR_ADDCHOICE, &val, NULL );
        }

        p_access->info.i_update &= ~INPUT_UPDATE_TITLE;
    }
    /* b_force_chapter: don't force a chapter change when a there is
     * pending user one (as it reset i_part */
    if( p_access->info.i_update & INPUT_UPDATE_SEEKPOINT &&
        ( b_force_chapter || !p_input->stream.p_new_area ) )
    {
        vlc_value_t val;
        /* TODO is it complete ? */
        p_input->stream.p_selected_area->i_part = p_access->info.i_seekpoint + 1;

        val.i_int = p_input->stream.p_selected_area->i_part;
        var_Change( p_input, "chapter", VLC_VAR_SETVALUE, &val, NULL );

        p_access->info.i_update &= ~INPUT_UPDATE_SEEKPOINT;
    }
    /* Size change */
    if( p_access->info.i_update & INPUT_UPDATE_SIZE )
    {
        p_input->stream.p_selected_area->i_size  = p_access->info.i_size;
        p_input->stream.b_changed = VLC_TRUE;

        p_access->info.i_update &= ~INPUT_UPDATE_SIZE;
    }
}
