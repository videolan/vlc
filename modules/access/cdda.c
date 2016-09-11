/*****************************************************************************
 * cdda.c : CD digital audio input module for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2003-2006, 2008-2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
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

/**
 * Todo:
 *   - Improve CDDB support (non-blocking, ...)
 *   - Fix tracknumber in MRL
 */

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <vlc_common.h>
#include <vlc_demux.h>
#include <vlc_plugin.h>
#include <vlc_input.h>
#include <vlc_access.h>
#include <vlc_meta.h>
#include <vlc_charset.h> /* ToLocaleDup */

#include "vcd/cdrom.h"  /* For CDDA_DATA_SIZE */

#ifdef HAVE_LIBCDDB
 #include <cddb/cddb.h>
 #include <errno.h>
#endif

/* how many blocks Demux() will read in each iteration */
#define CDDA_BLOCKS_ONCE 20

struct demux_sys_t
{
    vcddev_t    *vcddev;                            /* vcd device descriptor */
    es_out_id_t *es;
    date_t       pts;

    unsigned start; /**< Track first sector */
    unsigned length; /**< Track total sectors */
    unsigned position; /**< Current offset within track sectors */
};

static int Demux(demux_t *demux)
{
    demux_sys_t *sys = demux->p_sys;
    unsigned count = CDDA_BLOCKS_ONCE;

    if (sys->position >= sys->length)
        return VLC_DEMUXER_EOF;

    if (sys->position + count >= sys->length)
        count = sys->length - sys->position;

    block_t *block = block_Alloc(count * CDDA_DATA_SIZE);
    if (unlikely(block == NULL))
        return VLC_DEMUXER_EOF;

    if (ioctl_ReadSectors(VLC_OBJECT(demux), sys->vcddev,
                          sys->start + sys->position,
                          block->p_buffer, count, CDDA_TYPE) < 0)
    {
        msg_Err(demux, "cannot read sector %u", sys->position);
        block_Release(block);

        /* Skip potentially bad sector */
        sys->position++;
        return VLC_DEMUXER_SUCCESS;
    }

    sys->position += count;

    block->i_nb_samples = block->i_buffer / 4;
    block->i_dts = block->i_pts = VLC_TS_0 + date_Get(&sys->pts);
    date_Increment(&sys->pts, block->i_nb_samples);

    es_out_Send(demux->out, sys->es, block);
    es_out_Control(demux->out, ES_OUT_SET_PCR, VLC_TS_0 + date_Get(&sys->pts));
    return VLC_DEMUXER_SUCCESS;
}

static int DemuxControl(demux_t *demux, int query, va_list args)
{
    demux_sys_t *sys = demux->p_sys;

    /* One sector is 40000/3 µs */
    static_assert (CDDA_DATA_SIZE * CLOCK_FREQ * 3 ==
                   4 * 44100 * INT64_C(40000), "Wrong time/sector ratio");

    switch (query)
    {
        case DEMUX_CAN_SEEK:
        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_CONTROL_PACE:
            *va_arg(args, bool*) = true;
            break;
        case DEMUX_GET_PTS_DELAY:
            *va_arg(args, int64_t *) =
                INT64_C(1000) * var_InheritInteger(demux, "disc-caching");
            break;

        case DEMUX_SET_PAUSE_STATE:
            break;

        case DEMUX_GET_POSITION:
            *va_arg(args, double *) = (double)(sys->position)
                                      / (double)(sys->length);
            break;
 
        case DEMUX_SET_POSITION:
            sys->position = lround(va_arg(args, double) * sys->length);
            break;

        case DEMUX_GET_LENGTH:
            *va_arg(args, mtime_t *) = (INT64_C(40000) * sys->length) / 3;
            break;
        case DEMUX_GET_TIME:
            *va_arg(args, mtime_t *) = (INT64_C(40000) * sys->position) / 3;
            break;
        case DEMUX_SET_TIME:
            sys->position = (va_arg(args, mtime_t) * 3) / INT64_C(40000);
            break;

        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static int DemuxOpen(vlc_object_t *obj)
{
    demux_t *demux = (demux_t *)obj;

    unsigned track = var_InheritInteger(obj, "cdda-track");
    if (track == 0)
        return VLC_EGENERIC; /* Whole disc -> use access plugin */

    char *path;
    if (demux->psz_file != NULL)
        path = ToLocaleDup(demux->psz_file);
    else
        path = var_InheritString(obj, "cd-audio");
    if (path == NULL)
        return VLC_EGENERIC;

#if defined( _WIN32 ) || defined( __OS2__ )
    if (path[0] != '\0' && strcmp(path + 1, ":" DIR_SEP) == 0)
        path[2] = '\0';
 #endif

    demux_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
    {
        free(path);
        return VLC_ENOMEM;
    }
    demux->p_sys = sys;

    /* Open CDDA */
    sys->vcddev = ioctl_Open(obj, path);
    if (sys->vcddev == NULL)
        msg_Warn(obj, "could not open %s", path);
    free(path);
    if (sys->vcddev == NULL)
    {
        free(sys);
        return VLC_EGENERIC;
    }

    sys->start = var_InheritInteger(obj, "cdda-first-sector");
    sys->length = var_InheritInteger(obj, "cdda-last-sector") - sys->start;

    /* Track number in input item */
    if (sys->start == (unsigned)-1 || sys->length == (unsigned)-1)
    {
        int *sectors; /* Track sectors */
        unsigned titles = ioctl_GetTracksMap(obj, sys->vcddev, &sectors);

        if (track > titles)
        {
            msg_Err(obj, "invalid track number: %u/%u", track, titles);
            free(sectors);
            goto error;
        }

        sys->start = sectors[track - 1];
        sys->length = sectors[track] - sys->start;
        free(sectors);
    }

    es_format_t fmt;

    es_format_Init(&fmt, AUDIO_ES, VLC_CODEC_S16L);
    fmt.audio.i_rate = 44100;
    fmt.audio.i_channels = 2;
    sys->es = es_out_Add(demux->out, &fmt);

    date_Init(&sys->pts, 44100, 1);
    date_Set(&sys->pts, 0);

    sys->position = 0;
    demux->pf_demux = Demux;
    demux->pf_control = DemuxControl;
    return VLC_SUCCESS;

error:
    ioctl_Close(obj, sys->vcddev);
    free(sys);
    return VLC_EGENERIC;
}

static void DemuxClose(vlc_object_t *obj)
{
    demux_t *demux = (demux_t *)obj;
    demux_sys_t *sys = demux->p_sys;

    ioctl_Close(obj, sys->vcddev);
    free(sys);
}

/*****************************************************************************
 * Access: local prototypes
 *****************************************************************************/
struct access_sys_t
{
    vcddev_t    *vcddev;                            /* vcd device descriptor */
    int        *p_sectors;                                  /* Track sectors */
};

#ifdef HAVE_LIBCDDB
static cddb_disc_t *GetCDDBInfo( vlc_object_t *obj, int i_titles, int *p_sectors )
{
    if( var_InheritInteger( obj, "album-art" ) != ALBUM_ART_ALL &&
       !var_InheritBool( obj, "metadata-network-access" ) )
    {
        msg_Dbg( obj, "album art policy set to manual: not fetching" );
        return NULL;
    }

    /* */
    cddb_conn_t *p_cddb = cddb_new();
    if( !p_cddb )
    {
        msg_Warn( obj, "unable to use CDDB" );
        return NULL;
    }

    /* */

    cddb_http_enable( p_cddb );

    char *psz_tmp = var_InheritString( obj, "cddb-server" );
    if( psz_tmp )
    {
        cddb_set_server_name( p_cddb, psz_tmp );
        free( psz_tmp );
    }

    cddb_set_server_port( p_cddb, var_InheritInteger( obj, "cddb-port" ) );

    cddb_set_email_address( p_cddb, "vlc@videolan.org" );

    cddb_set_http_path_query( p_cddb, "/~cddb/cddb.cgi" );
    cddb_set_http_path_submit( p_cddb, "/~cddb/submit.cgi" );


    char *psz_cachedir;
    char *psz_temp = config_GetUserDir( VLC_CACHE_DIR );

    if( asprintf( &psz_cachedir, "%s" DIR_SEP "cddb", psz_temp ) > 0 ) {
        cddb_cache_enable( p_cddb );
        cddb_cache_set_dir( p_cddb, psz_cachedir );
        free( psz_cachedir );
    }
    free( psz_temp );

    cddb_set_timeout( p_cddb, 10 );

    /* */
    cddb_disc_t *p_disc = cddb_disc_new();
    if( !p_disc )
    {
        msg_Err( obj, "unable to create CDDB disc structure." );
        goto error;
    }

    int64_t i_length = 2000000; /* PreGap */
    for( int i = 0; i < i_titles; i++ )
    {
        cddb_track_t *t = cddb_track_new();
        cddb_track_set_frame_offset( t, p_sectors[i] + 150 );  /* Pregap offset */

        cddb_disc_add_track( p_disc, t );
        const int64_t i_size = ( p_sectors[i+1] - p_sectors[i] ) *
                               (int64_t)CDDA_DATA_SIZE;
        i_length += INT64_C(1000000) * i_size / 44100 / 4  ;

        msg_Dbg( obj, "Track %i offset: %i", i, p_sectors[i] + 150 );
    }

    msg_Dbg( obj, "Total length: %i", (int)(i_length/1000000) );
    cddb_disc_set_length( p_disc, (int)(i_length/1000000) );

    if( !cddb_disc_calc_discid( p_disc ) )
    {
        msg_Err( obj, "CDDB disc ID calculation failed" );
        goto error;
    }

    const int i_matches = cddb_query( p_cddb, p_disc );
    if( i_matches < 0 )
    {
        msg_Warn( obj, "CDDB error: %s", cddb_error_str(errno) );
        goto error;
    }
    else if( i_matches == 0 )
    {
        msg_Dbg( obj, "Couldn't find any matches in CDDB." );
        goto error;
    }
    else if( i_matches > 1 )
        msg_Warn( obj, "found %d matches in CDDB. Using first one.", i_matches );

    cddb_read( p_cddb, p_disc );

    cddb_destroy( p_cddb);
    return p_disc;

error:
    if( p_disc )
        cddb_disc_destroy( p_disc );
    cddb_destroy( p_cddb );
    return NULL;
}
#endif /* HAVE_LIBCDDB */

static int GetTracks( access_t *p_access, input_item_t *p_current )
{
    vlc_object_t *obj = VLC_OBJECT(p_access);
    access_sys_t *p_sys = p_access->p_sys;

    const int i_titles = ioctl_GetTracksMap( obj, p_sys->vcddev,
                                             &p_sys->p_sectors );
    if( i_titles <= 0 )
    {
        if( i_titles < 0 )
            msg_Err( obj, "unable to count tracks" );
        else if( i_titles <= 0 )
            msg_Err( obj, "no audio tracks found" );
        return VLC_EGENERIC;;
    }

    /* */
    input_item_SetName( p_current, "Audio CD" );

    const char *psz_album = NULL;
    const char *psz_year = NULL;
    const char *psz_genre = NULL;
    const char *psz_artist = NULL;
    const char *psz_description = NULL;

/* Return true if the given string is not NULL and not empty */
#define NONEMPTY( psz ) ( (psz) && *(psz) )
/* If the given string is NULL or empty, fill it by the return value of 'code' */
#define ON_EMPTY( psz, code ) do { if( !NONEMPTY( psz) ) { (psz) = code; } } while(0)

    /* Retreive CDDB information */
#ifdef HAVE_LIBCDDB
    char psz_year_buffer[4+1];
    msg_Dbg( obj, "fetching infos with CDDB..." );
    cddb_disc_t *p_disc = GetCDDBInfo( obj, i_titles, p_sys->p_sectors );
    if( p_disc )
    {
        msg_Dbg( obj, "Disc ID: %08x", cddb_disc_get_discid( p_disc ) );
        psz_album = cddb_disc_get_title( p_disc );
        psz_genre = cddb_disc_get_genre( p_disc );

        /* */
        const unsigned i_year = cddb_disc_get_year( p_disc );
        if( i_year > 0 )
        {
            psz_year = psz_year_buffer;
            snprintf( psz_year_buffer, sizeof(psz_year_buffer), "%u", i_year );
        }

        /* Set artist only if unique */
        for( int i = 0; i < i_titles; i++ )
        {
            cddb_track_t *t = cddb_disc_get_track( p_disc, i );
            if( !t )
                continue;
            const char *psz_track_artist = cddb_track_get_artist( t );
            if( psz_artist && psz_track_artist &&
                strcmp( psz_artist, psz_track_artist ) )
            {
                psz_artist = NULL;
                break;
            }
            psz_artist = psz_track_artist;
        }
    }
    else
        msg_Dbg( obj, "GetCDDBInfo failed" );
#endif

    /* CD-Text */
    vlc_meta_t **pp_cd_text;
    int        i_cd_text;

    if( ioctl_GetCdText( obj, p_sys->vcddev, &pp_cd_text, &i_cd_text ) )
    {
        msg_Dbg( obj, "CD-TEXT information missing" );
        i_cd_text = 0;
        pp_cd_text = NULL;
    }

    /* Retrieve CD-TEXT information but prefer CDDB */
    if( i_cd_text > 0 && pp_cd_text[0] )
    {
        const vlc_meta_t *p_disc = pp_cd_text[0];
        ON_EMPTY( psz_album,       vlc_meta_Get( p_disc, vlc_meta_Album ) );
        ON_EMPTY( psz_genre,       vlc_meta_Get( p_disc, vlc_meta_Genre ) );
        ON_EMPTY( psz_artist,      vlc_meta_Get( p_disc, vlc_meta_Artist ) );
        ON_EMPTY( psz_description, vlc_meta_Get( p_disc, vlc_meta_Description ) );
    }

    if( NONEMPTY( psz_album ) )
    {
        input_item_SetName( p_current, psz_album );
        input_item_SetAlbum( p_current, psz_album );
    }

    if( NONEMPTY( psz_genre ) )
        input_item_SetGenre( p_current, psz_genre );

    if( NONEMPTY( psz_artist ) )
        input_item_SetArtist( p_current, psz_artist );

    if( NONEMPTY( psz_year ) )
        input_item_SetDate( p_current, psz_year );

    if( NONEMPTY( psz_description ) )
        input_item_SetDescription( p_current, psz_description );

    const mtime_t i_duration = (int64_t)( p_sys->p_sectors[i_titles] - p_sys->p_sectors[0] ) *
                               CDDA_DATA_SIZE * 1000000 / 44100 / 2 / 2;
    input_item_SetDuration( p_current, i_duration );

    input_item_node_t *p_root = input_item_node_Create( p_current );

    /* Build title table */
    for( int i = 0; i < i_titles; i++ )
    {
        char *psz_opt, *psz_name;

        msg_Dbg( obj, "track[%d] start=%d", i, p_sys->p_sectors[i] );

        /* Define a "default name" */
        if( asprintf( &psz_name, _("Audio CD - Track %02i"), (i+1) ) == -1 )
            psz_name = p_access->psz_url;

        /* Create playlist items */
        const mtime_t i_duration = (int64_t)( p_sys->p_sectors[i+1] - p_sys->p_sectors[i] ) *
                                   CDDA_DATA_SIZE * 1000000 / 44100 / 2 / 2;

        input_item_t *p_item = input_item_NewDisc( p_access->psz_url,
                                                   psz_name, i_duration );
        if( likely(psz_name != p_access->psz_url) )
            free( psz_name );

        if( unlikely(p_item == NULL) )
            continue;

        input_item_CopyOptions( p_item, p_current );

        if( likely(asprintf( &psz_opt, "cdda-track=%i", i+1 ) != -1) )
        {
            input_item_AddOption( p_item, psz_opt, VLC_INPUT_OPTION_TRUSTED );
            free( psz_opt );
        }
        if( likely(asprintf( &psz_opt, "cdda-first-sector=%i",
                             p_sys->p_sectors[i] ) != -1) )
        {
            input_item_AddOption( p_item, psz_opt, VLC_INPUT_OPTION_TRUSTED );
            free( psz_opt );
        }
        if( likely(asprintf( &psz_opt, "cdda-last-sector=%i",
                             p_sys->p_sectors[i+1] ) != -1) )
        {
            input_item_AddOption( p_item, psz_opt, VLC_INPUT_OPTION_TRUSTED );
            free( psz_opt );
        }

        const char *psz_track_title = NULL;
        const char *psz_track_artist = NULL;
        const char *psz_track_genre = NULL;
        const char *psz_track_description = NULL;

#ifdef HAVE_LIBCDDB
        /* Retreive CDDB information */
        if( p_disc )
        {
            cddb_track_t *t = cddb_disc_get_track( p_disc, i );
            if( t != NULL )
            {
                psz_track_title = cddb_track_get_title( t );
                psz_track_artist = cddb_track_get_artist( t );
            }
        }
#endif

        /* Retreive CD-TEXT information but prefer CDDB */
        if( i+1 < i_cd_text && pp_cd_text[i+1] )
        {
            const vlc_meta_t *t = pp_cd_text[i+1];

            ON_EMPTY( psz_track_title,       vlc_meta_Get( t, vlc_meta_Title ) );
            ON_EMPTY( psz_track_artist,      vlc_meta_Get( t, vlc_meta_Artist ) );
            ON_EMPTY( psz_track_genre,       vlc_meta_Get( t, vlc_meta_Genre ) );
            ON_EMPTY( psz_track_description, vlc_meta_Get( t, vlc_meta_Description ) );
        }

        /* */
        ON_EMPTY( psz_track_artist,       psz_artist );
        ON_EMPTY( psz_track_genre,        psz_genre );
        ON_EMPTY( psz_track_description,  psz_description );

        /* */
        if( NONEMPTY( psz_track_title ) )
        {
            input_item_SetName( p_item, psz_track_title );
            input_item_SetTitle( p_item, psz_track_title );
        }

        if( NONEMPTY( psz_track_artist ) )
            input_item_SetArtist( p_item, psz_track_artist );

        if( NONEMPTY( psz_track_genre ) )
            input_item_SetGenre( p_item, psz_track_genre );

        if( NONEMPTY( psz_track_description ) )
            input_item_SetDescription( p_item, psz_track_description );

        if( NONEMPTY( psz_album ) )
            input_item_SetAlbum( p_item, psz_album );

        if( NONEMPTY( psz_year ) )
            input_item_SetDate( p_item, psz_year );

        char psz_num[3+1];
        snprintf( psz_num, sizeof(psz_num), "%d", 1+i );
        input_item_SetTrackNum( p_item, psz_num );

        input_item_node_AppendItem( p_root, p_item );
        vlc_gc_decref( p_item );
    }
#undef ON_EMPTY
#undef NONEMPTY

    input_item_node_PostAndDelete( p_root );

    /* */
    for( int i = 0; i < i_cd_text; i++ )
    {
        vlc_meta_t *p_meta = pp_cd_text[i];
        if( !p_meta )
            continue;
        vlc_meta_Delete( p_meta );
    }
    free( pp_cd_text );

#ifdef HAVE_LIBCDDB
    if( p_disc )
        cddb_disc_destroy( p_disc );
#endif
    return VLC_SUCCESS;
}

static block_t *BlockDummy( access_t *p_access, bool *restrict eof )
{
    (void) p_access;
    *eof = true;
    return NULL;
}

static int AccessOpen(vlc_object_t *obj)
{
    access_t *p_access = (access_t *)obj;
    vcddev_t     *vcddev;
    char         *psz_name;

    /* Do we play a single track ? */
    if (var_InheritInteger(obj, "cdda-track") != 0)
        return VLC_EGENERIC;

    if( !p_access->psz_filepath || !*p_access->psz_filepath )
    {
        psz_name = var_InheritString(obj, "cd-audio");
        if( !psz_name )
            return VLC_EGENERIC;
    }
    else psz_name = ToLocaleDup( p_access->psz_filepath );

#if defined( _WIN32 ) || defined( __OS2__ )
    if( psz_name[0] && psz_name[1] == ':' &&
        psz_name[2] == '\\' && psz_name[3] == '\0' ) psz_name[2] = '\0';
#endif

    access_sys_t *p_sys = calloc( 1, sizeof (*p_sys) );
    if( unlikely(p_sys == NULL) )
    {
        free( psz_name );
        return VLC_ENOMEM;
    }
    p_access->p_sys = p_sys;

    /* Open CDDA */
    vcddev = ioctl_Open( VLC_OBJECT(p_access), psz_name );
    if( vcddev == NULL )
        msg_Warn( p_access, "could not open %s", psz_name );
    free( psz_name );
    if( vcddev == NULL )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_sys->vcddev = vcddev;

    /* We only do separate items if the whole disc is requested */
    input_thread_t *p_input = p_access->p_input;
    if( p_input )
    {
        input_item_t *p_current = input_GetItem( p_input );
        if (p_current != NULL && GetTracks(p_access, p_current) < 0)
            goto error;
    }

    p_access->pf_block = BlockDummy;
    p_access->pf_seek = NULL;
    p_access->pf_read = NULL;
    p_access->pf_control = access_vaDirectoryControlHelper;
    return VLC_SUCCESS;

error:
    free( p_sys->p_sectors );
    ioctl_Close( VLC_OBJECT(p_access), p_sys->vcddev );
    free( p_sys );
    return VLC_EGENERIC;
}

static void AccessClose(vlc_object_t *obj)
{
    access_t *access = (access_t *)obj;
    access_sys_t *sys = access->p_sys;

    free(sys->p_sectors );
    ioctl_Close(obj, sys->vcddev);
    free(sys);
}

/*****************************************************************************
 * Module descriptior
 *****************************************************************************/
#define CDAUDIO_DEV_TEXT N_("Audio CD device")
#if defined( _WIN32 ) || defined( __OS2__ )
# define CDAUDIO_DEV_LONGTEXT N_( \
    "This is the default Audio CD drive (or file) to use. Don't forget the " \
    "colon after the drive letter (e.g. D:)")
# define CD_DEVICE      "D:"
#else
# define CDAUDIO_DEV_LONGTEXT N_( \
    "This is the default Audio CD device to use." )
# if defined(__OpenBSD__)
#  define CD_DEVICE      "/dev/cd0c"
# elif defined(__linux__)
#  define CD_DEVICE      "/dev/sr0"
# else
#  define CD_DEVICE      "/dev/cdrom"
# endif
#endif

vlc_module_begin ()
    set_shortname( N_("Audio CD") )
    set_description( N_("Audio CD input") )
    set_capability( "access", 10 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )
    set_callbacks(AccessOpen, AccessClose)

    add_loadfile( "cd-audio", CD_DEVICE, CDAUDIO_DEV_TEXT,
                  CDAUDIO_DEV_LONGTEXT, false )

    add_usage_hint( N_("[cdda:][device][@[track]]") )
    add_integer( "cdda-track", 0 , NULL, NULL, true )
        change_volatile ()
    add_integer( "cdda-first-sector", -1, NULL, NULL, true )
        change_volatile ()
    add_integer( "cdda-last-sector", -1, NULL, NULL, true )
        change_volatile ()

#ifdef HAVE_LIBCDDB
    add_string( "cddb-server", "freedb.videolan.org", N_( "CDDB Server" ),
            N_( "Address of the CDDB server to use." ), true )
    add_integer( "cddb-port", 80, N_( "CDDB port" ),
            N_( "CDDB Server port to use." ), true )
        change_integer_range( 1, 65535 )
#endif

    add_shortcut( "cdda", "cddasimple" )

    add_submodule()
    set_capability( "access_demux", 10 )
    set_callbacks(DemuxOpen, DemuxClose)
vlc_module_end ()
