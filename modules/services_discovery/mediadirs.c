/*****************************************************************************
 * mediadirs.c:  Picture/Music/Video user directories as service discoveries
 *****************************************************************************
 * Copyright (C) 2009 the VideoLAN team
 * $Id$
 *
 * Authors: Erwan Tulou <erwan10 aT videolan DoT org>
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
 * Includes
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_url.h>
#include <vlc_charset.h>
#include <vlc_services_discovery.h>

#include <sys/stat.h>


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

enum type_e { Video = 0, Audio = 1, Picture = 2, Unknown = 3 };

static int  Open( vlc_object_t *, enum type_e );
static void Close( vlc_object_t * );

/* Main functions */
#define OPEN_MODULE( type )                        \
static int Open##type( vlc_object_t *p_this )      \
{                                                  \
    msg_Dbg( p_this, "Starting " #type );          \
    return Open( p_this, type );                   \
}

OPEN_MODULE( Video )
OPEN_MODULE( Audio )
OPEN_MODULE( Picture )

#undef OPEN_MODULE

static int vlc_sd_probe_Open( vlc_object_t * );

vlc_module_begin ()
    set_category( CAT_PLAYLIST )
    set_subcategory( SUBCAT_PLAYLIST_SD )

        set_shortname( "Video" )
        set_description( N_("My Videos") )
        set_capability( "services_discovery", 10 )
        set_callbacks( OpenVideo, Close )
        add_shortcut( "video_dir" )

    add_submodule ()
        set_shortname( "Audio" )
        set_description( N_("My Music") )
        set_capability( "services_discovery", 10 )
        set_callbacks( OpenAudio, Close )
        add_shortcut( "audio_dir" )

    add_submodule ()
        set_shortname( "Picture")
        set_description( N_("My Pictures") )
        set_capability( "services_discovery", 10 )
        set_callbacks( OpenPicture, Close )
        add_shortcut( "picture_dir" )

    VLC_SD_PROBE_SUBMODULE
vlc_module_end ()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static void* Run( void* );

static void input_item_subitem_added( const vlc_event_t*, void* );
static int onNewFileAdded( vlc_object_t*, char const *,
                           vlc_value_t, vlc_value_t, void *);

static enum type_e fileType( services_discovery_t *p_sd, const char* psz_file );
static void formatSnapshotItem( input_item_t* );

struct services_discovery_sys_t
{
    vlc_thread_t thread;
    enum type_e i_type;

    char* psz_dir[2];
    const char* psz_var;
};

/*****************************************************************************
 * Open: initialize module
 *****************************************************************************/
static int Open( vlc_object_t *p_this, enum type_e i_type )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    services_discovery_sys_t *p_sys = p_sd->p_sys;

    p_sd->p_sys = p_sys = calloc( 1, sizeof( *p_sys) );
    if( !p_sys )
        return VLC_ENOMEM;

    p_sys->i_type = i_type;

    if( p_sys->i_type == Video )
    {
        p_sys->psz_dir[0] = config_GetUserDir( VLC_VIDEOS_DIR );
        p_sys->psz_dir[1] = var_CreateGetString( p_sd, "input-record-path" );

        p_sys->psz_var = "record-file";
    }
    else if( p_sys->i_type == Audio )
    {
        p_sys->psz_dir[0] = config_GetUserDir( VLC_MUSIC_DIR );
        p_sys->psz_dir[1] = var_CreateGetString( p_sd, "input-record-path" );

        p_sys->psz_var = "record-file";
    }
    else if( p_sys->i_type == Picture )
    {
        p_sys->psz_dir[0] = config_GetUserDir( VLC_PICTURES_DIR );
        p_sys->psz_dir[1] = var_CreateGetString( p_sd, "snapshot-path" );

        p_sys->psz_var = "snapshot-file";
    }
    else
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    var_AddCallback( p_sd->p_libvlc, p_sys->psz_var, onNewFileAdded, p_sd );

    if( vlc_clone( &p_sys->thread, Run, p_sd, VLC_THREAD_PRIORITY_LOW ) )
    {
        var_DelCallback( p_sd->p_libvlc, p_sys->psz_var, onNewFileAdded, p_sd );
        free( p_sys->psz_dir[1] );
        free( p_sys->psz_dir[0] );
        free( p_sys );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Run:
 *****************************************************************************/
static void *Run( void *data )
{
    services_discovery_t *p_sd = data;
    services_discovery_sys_t *p_sys = p_sd->p_sys;

    int canc = vlc_savecancel();

    int num_dir = sizeof( p_sys->psz_dir ) / sizeof( p_sys->psz_dir[0] );
    for( int i = 0; i < num_dir; i++ )
    {
        char* psz_dir = p_sys->psz_dir[i];

        /* make sure the directory exists */
        struct stat st;
        if( psz_dir == NULL            ||
            utf8_stat( psz_dir, &st )  ||
            !S_ISDIR( st.st_mode ) )
            continue;

        // TODO:  make_URI is only for file://, what about dir:// ?
        // char* psz_uri = make_URI( psz_dir );
        char* psz_uri;
        if( asprintf( &psz_uri, "dir://%s",  psz_dir ) == -1 )
            continue;

        input_item_t* p_root = input_item_New( p_sd, psz_uri, NULL );
        if( p_sys->i_type == Picture )
            input_item_AddOption( p_root, "ignore-filetypes=ini,db,lnk,txt",
                                  VLC_INPUT_OPTION_TRUSTED );

        input_item_AddOption( p_root, "recursive=collapse",
                              VLC_INPUT_OPTION_TRUSTED );


        vlc_event_manager_t *p_em = &p_root->event_manager;
        vlc_event_attach( p_em, vlc_InputItemSubItemAdded,
                          input_item_subitem_added, p_sd );

        input_Read( p_sd, p_root );

        vlc_event_detach( p_em, vlc_InputItemSubItemAdded,
                          input_item_subitem_added, p_sd );

        vlc_gc_decref( p_root );
        free( psz_uri );
    }

    vlc_restorecancel(canc);
    return NULL;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    services_discovery_t *p_sd = (services_discovery_t *)p_this;
    services_discovery_sys_t *p_sys = p_sd->p_sys;

    vlc_cancel( p_sys->thread );
    vlc_join( p_sys->thread, NULL );

    var_DelCallback( p_sd->p_libvlc, p_sys->psz_var, onNewFileAdded, p_sd );

    free( p_sys->psz_dir[1] );
    free( p_sys->psz_dir[0] );
    free( p_sys );
}


/*****************************************************************************
 * Callbacks and helper functions
 *****************************************************************************/
static void input_item_subitem_added( const vlc_event_t * p_event,
                                      void * user_data )
{
    services_discovery_t *p_sd = user_data;
    services_discovery_sys_t *p_sys = p_sd->p_sys;

    /* retrieve new item */
    input_item_t *p_item = p_event->u.input_item_subitem_added.p_new_child;

    if( p_sys->i_type == Picture )
        formatSnapshotItem( p_item );

    services_discovery_AddItem( p_sd, p_item, NULL );
}

static int onNewFileAdded( vlc_object_t *p_this, char const *psz_var,
                     vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    services_discovery_t *p_sd = p_data;

    (void)p_this; (void)psz_var; (void)oldval;
    char* psz_file = newval.psz_string;
    if( !psz_file || !*psz_file )
        return VLC_EGENERIC;

    char* psz_uri = make_URI( psz_file );
    input_item_t* p_item = input_item_New( p_sd, psz_uri, NULL );

    if( fileType( p_sd, psz_file ) == Picture )
    {
        formatSnapshotItem( p_item );
        services_discovery_AddItem( p_sd, p_item, NULL );

        msg_Dbg( p_sd, "New snapshot added : %s", psz_file );
    }
    else if( fileType( p_sd, psz_file ) == Audio )
    {
        services_discovery_AddItem( p_sd, p_item, NULL );

        msg_Dbg( p_sd, "New recorded audio added : %s", psz_file );
    }
    else if( fileType( p_sd, psz_file ) == Video )
    {
        services_discovery_AddItem( p_sd, p_item, NULL );

        msg_Dbg( p_sd, "New recorded video added : %s", psz_file );
    }

    vlc_gc_decref( p_item );
    free( psz_uri );

    return VLC_SUCCESS;
}

void formatSnapshotItem( input_item_t *p_item )
{
    if( !p_item )
        return;

    if( !p_item->p_meta )
        p_item->p_meta = vlc_meta_New();

    /* copy the snapshot mrl as a ArtURL */
    if( p_item->p_meta )
    {
        char* psz_uri = NULL;
        psz_uri = input_item_GetURI( p_item );
        if( psz_uri )
            input_item_SetArtURL( p_item, psz_uri );
        free( psz_uri );
    }

    /**
     * TODO: select the best mrl for displaying snapshots
     *   - vlc://pause:10  => snapshot are displayed as Art
     *   - file:///path/image.ext  => snapshot are displayed as videos
     **/
    input_item_SetURI( p_item, "vlc://pause:10" );

    // input_item_AddOption( p_item, "fake-duration=10000",
    //                       VLC_INPUT_OPTION_TRUSTED );
}


enum type_e fileType( services_discovery_t *p_sd, const char* psz_file )
{
    services_discovery_sys_t *p_sys = p_sd->p_sys;
    enum type_e i_ret = Unknown;

    char* psz_dir = strdup( psz_file );
    char* psz_tmp = strrchr( psz_dir, DIR_SEP_CHAR );
    if( psz_tmp )
        *psz_tmp = '\0';

    int num_dir = sizeof( p_sys->psz_dir ) / sizeof( p_sys->psz_dir[0] );
    for( int i = 0; i < num_dir; i++ )
    {
        char* psz_known_dir = p_sys->psz_dir[i];

        if( psz_known_dir && !strcmp( psz_dir, psz_known_dir ) )
            i_ret = p_sys->i_type;
    }

    free( psz_dir );
    return i_ret;
}

static int vlc_sd_probe_Open( vlc_object_t *obj )
{
    vlc_probe_t *probe = (vlc_probe_t *)obj;

    vlc_sd_probe_Add( probe, "video_dir", N_("My Videos") );
    vlc_sd_probe_Add( probe, "audio_dir", N_("My Music") );
    return vlc_sd_probe_Add( probe, "picture_dir", N_("My Pictures") );
}
