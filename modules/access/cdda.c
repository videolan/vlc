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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_input.h>
#include <vlc_access.h>
#include <vlc_meta.h>
#include <vlc_charset.h> /* ToLocaleDup */

#include <vlc_codecs.h> /* For WAVEHEADER */
#include "vcd/cdrom.h"  /* For CDDA_DATA_SIZE */

#ifdef HAVE_LIBCDDB
 #include <cddb/cddb.h>
 #include <errno.h>
#endif

/*****************************************************************************
 * Module descriptior
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin ()
    set_shortname( N_("Audio CD") )
    set_description( N_("Audio CD input") )
    set_capability( "access", 10 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )
    set_callbacks( Open, Close )

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
vlc_module_end ()


/* how many blocks VCDRead will read in each loop */
#define CDDA_BLOCKS_ONCE 20
#define CDDA_DATA_ONCE   (CDDA_BLOCKS_ONCE * CDDA_DATA_SIZE)

/*****************************************************************************
 * Access: local prototypes
 *****************************************************************************/
struct access_sys_t
{
    vcddev_t    *vcddev;                            /* vcd device descriptor */

    /* Current position */
    int         i_sector;                                  /* Current Sector */
    int        *p_sectors;                                  /* Track sectors */

    /* Wave header for the output data */
    WAVEHEADER  waveheader;
    bool        b_header;

    int         i_track;
    int         i_first_sector;
    int         i_last_sector;
};

static block_t *Block( access_t * );
static int      Seek( access_t *, uint64_t );
static int      Control( access_t *, int, va_list );

static int GetTracks( access_t *p_access, input_item_t *p_current );

#ifdef HAVE_LIBCDDB
static cddb_disc_t *GetCDDBInfo( access_t *p_access, int i_titles, int *p_sectors );
#endif

/*****************************************************************************
 * Open: open cdda
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys;
    vcddev_t     *vcddev;
    char         *psz_name;

    if( !p_access->psz_filepath || !*p_access->psz_filepath )
    {
        /* Only when selected */
        if( !p_access->psz_access || !*p_access->psz_access )
            return VLC_EGENERIC;

        psz_name = var_InheritString( p_this, "cd-audio" );
        if( !psz_name )
            return VLC_EGENERIC;
    }
    else psz_name = ToLocaleDup( p_access->psz_filepath );

#if defined( _WIN32 ) || defined( __OS2__ )
    if( psz_name[0] && psz_name[1] == ':' &&
        psz_name[2] == '\\' && psz_name[3] == '\0' ) psz_name[2] = '\0';
#endif

    /* Open CDDA */
    if( (vcddev = ioctl_Open( VLC_OBJECT(p_access), psz_name ) ) == NULL )
    {
        msg_Warn( p_access, "could not open %s", psz_name );
        free( psz_name );
        return VLC_EGENERIC;
    }
    free( psz_name );

    /* Set up p_access */
    STANDARD_BLOCK_ACCESS_INIT
    p_sys->vcddev = vcddev;

    /* Do we play a single track ? */
    p_sys->i_track = var_InheritInteger( p_access, "cdda-track" ) - 1;

    if( p_sys->i_track < 0 )
    {
        /* We only do separate items if the whole disc is requested */
        input_thread_t *p_input = access_GetParentInput( p_access );

        int i_ret = -1;
        if( p_input )
        {
            input_item_t *p_current = input_GetItem( p_input );
            if( p_current )
                i_ret = GetTracks( p_access, p_current );

            vlc_object_release( p_input );
        }
        if( i_ret < 0 )
            goto error;
    }
    else
    {
        /* Build a WAV header for the output data */
        memset( &p_sys->waveheader, 0, sizeof(WAVEHEADER) );
        SetWLE( &p_sys->waveheader.Format, 1 ); /*WAVE_FORMAT_PCM*/
        SetWLE( &p_sys->waveheader.BitsPerSample, 16);
        p_sys->waveheader.MainChunkID = VLC_FOURCC('R', 'I', 'F', 'F');
        p_sys->waveheader.Length = 0;               /* we just don't know */
        p_sys->waveheader.ChunkTypeID = VLC_FOURCC('W', 'A', 'V', 'E');
        p_sys->waveheader.SubChunkID = VLC_FOURCC('f', 'm', 't', ' ');
        SetDWLE( &p_sys->waveheader.SubChunkLength, 16);
        SetWLE( &p_sys->waveheader.Modus, 2);
        SetDWLE( &p_sys->waveheader.SampleFreq, 44100);
        SetWLE( &p_sys->waveheader.BytesPerSample,
                    2 /*Modus*/ * 16 /*BitsPerSample*/ / 8 );
        SetDWLE( &p_sys->waveheader.BytesPerSec,
                    2*16/8 /*BytesPerSample*/ * 44100 /*SampleFreq*/ );
        p_sys->waveheader.DataChunkID = VLC_FOURCC('d', 'a', 't', 'a');
        p_sys->waveheader.DataLength = 0;           /* we just don't know */

        p_sys->i_first_sector = var_InheritInteger( p_access,
                                                    "cdda-first-sector" );
        p_sys->i_last_sector  = var_InheritInteger( p_access,
                                                    "cdda-last-sector" );
        /* Tracknumber in MRL */
        if( p_sys->i_first_sector < 0 || p_sys->i_last_sector < 0 )
        {
            const int i_titles = ioctl_GetTracksMap( VLC_OBJECT(p_access),
                                                     p_sys->vcddev, &p_sys->p_sectors );
            if( p_sys->i_track >= i_titles )
            {
                msg_Err( p_access, "invalid track number" );
                goto error;
            }
            p_sys->i_first_sector = p_sys->p_sectors[p_sys->i_track];
            p_sys->i_last_sector = p_sys->p_sectors[p_sys->i_track+1];
        }

        p_sys->i_sector = p_sys->i_first_sector;
        p_access->info.i_size = (p_sys->i_last_sector - p_sys->i_first_sector)
                                     * (int64_t)CDDA_DATA_SIZE;
    }

    return VLC_SUCCESS;

error:
    free( p_sys->p_sectors );
    ioctl_Close( VLC_OBJECT(p_access), p_sys->vcddev );
    free( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close: closes cdda
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t *)p_this;
    access_sys_t *p_sys = p_access->p_sys;

    free( p_sys->p_sectors );
    ioctl_Close( p_this, p_sys->vcddev );
    free( p_sys );
}

/*****************************************************************************
 * Block: read data (CDDA_DATA_ONCE)
 *****************************************************************************/
static block_t *Block( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    int i_blocks = CDDA_BLOCKS_ONCE;
    block_t *p_block;

    if( p_sys->i_track < 0 ) p_access->info.b_eof = true;

    /* Check end of file */
    if( p_access->info.b_eof ) return NULL;

    if( !p_sys->b_header )
    {
        /* Return only the header */
        p_block = block_Alloc( sizeof( WAVEHEADER ) );
        memcpy( p_block->p_buffer, &p_sys->waveheader, sizeof(WAVEHEADER) );
        p_sys->b_header = true;
        return p_block;
    }

    if( p_sys->i_sector >= p_sys->i_last_sector )
    {
        p_access->info.b_eof = true;
        return NULL;
    }

    /* Don't read too far */
    if( p_sys->i_sector + i_blocks >= p_sys->i_last_sector )
        i_blocks = p_sys->i_last_sector - p_sys->i_sector;

    /* Do the actual reading */
    if( !( p_block = block_Alloc( i_blocks * CDDA_DATA_SIZE ) ) )
    {
        msg_Err( p_access, "cannot get a new block of size: %i",
                 i_blocks * CDDA_DATA_SIZE );
        return NULL;
    }

    if( ioctl_ReadSectors( VLC_OBJECT(p_access), p_sys->vcddev,
            p_sys->i_sector, p_block->p_buffer, i_blocks, CDDA_TYPE ) < 0 )
    {
        msg_Err( p_access, "cannot read sector %i", p_sys->i_sector );
        block_Release( p_block );

        /* Try to skip one sector (in case of bad sectors) */
        p_sys->i_sector++;
        p_access->info.i_pos += CDDA_DATA_SIZE;
        return NULL;
    }

    /* Update a few values */
    p_sys->i_sector += i_blocks;
    p_access->info.i_pos += p_block->i_buffer;

    return p_block;
}

/****************************************************************************
 * Seek
 ****************************************************************************/
static int Seek( access_t *p_access, uint64_t i_pos )
{
    access_sys_t *p_sys = p_access->p_sys;

    /* Next sector to read */
    p_sys->i_sector = p_sys->i_first_sector + i_pos / CDDA_DATA_SIZE;
    assert( p_sys->i_sector >= 0 );
    p_access->info.i_pos = i_pos;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( access_t *p_access, int i_query, va_list args )
{
    switch( i_query )
    {
        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_FASTSEEK:
        case ACCESS_CAN_PAUSE:
        case ACCESS_CAN_CONTROL_PACE:
            *va_arg( args, bool* ) = true;
            break;

        case ACCESS_GET_PTS_DELAY:
            *va_arg( args, int64_t * ) =
                INT64_C(1000) * var_InheritInteger( p_access, "disc-caching" );
            break;

        case ACCESS_SET_PAUSE_STATE:
            break;

        case ACCESS_GET_TITLE_INFO:
        case ACCESS_SET_TITLE:
        case ACCESS_GET_META:
        case ACCESS_SET_SEEKPOINT:
        case ACCESS_SET_PRIVATE_ID_STATE:
        case ACCESS_GET_CONTENT_TYPE:
            return VLC_EGENERIC;

        default:
            msg_Warn( p_access, "unimplemented query in control" );
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static int GetTracks( access_t *p_access, input_item_t *p_current )
{
    access_sys_t *p_sys = p_access->p_sys;

    const int i_titles = ioctl_GetTracksMap( VLC_OBJECT(p_access),
                                             p_sys->vcddev, &p_sys->p_sectors );
    if( i_titles <= 0 )
    {
        if( i_titles < 0 )
            msg_Err( p_access, "unable to count tracks" );
        else if( i_titles <= 0 )
            msg_Err( p_access, "no audio tracks found" );
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
    msg_Dbg( p_access, "fetching infos with CDDB" );
    cddb_disc_t *p_disc = GetCDDBInfo( p_access, i_titles, p_sys->p_sectors );
    if( p_disc )
    {
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
#endif

    /* CD-Text */
    vlc_meta_t **pp_cd_text;
    int        i_cd_text;

    if( ioctl_GetCdText( VLC_OBJECT(p_access), p_sys->vcddev, &pp_cd_text, &i_cd_text ) )
    {
        msg_Dbg( p_access, "CD-TEXT information missing" );
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
        input_item_t *p_input_item;

        char *psz_uri, *psz_opt, *psz_first, *psz_last;
        char *psz_name;

        msg_Dbg( p_access, "track[%d] start=%d", i, p_sys->p_sectors[i] );

        /* */
        if( asprintf( &psz_uri, "cdda://%s", p_access->psz_location ) == -1 )
            psz_uri = NULL;
        if( asprintf( &psz_opt, "cdda-track=%i", i+1 ) == -1 )
            psz_opt = NULL;
        if( asprintf( &psz_first, "cdda-first-sector=%i",p_sys->p_sectors[i] ) == -1 )
            psz_first = NULL;
        if( asprintf( &psz_last, "cdda-last-sector=%i", p_sys->p_sectors[i+1] ) == -1 )
            psz_last = NULL;

        /* Define a "default name" */
        if( asprintf( &psz_name, _("Audio CD - Track %02i"), (i+1) ) == -1 )
            psz_name = NULL;

        /* Create playlist items */
        const mtime_t i_duration = (int64_t)( p_sys->p_sectors[i+1] - p_sys->p_sectors[i] ) *
                                   CDDA_DATA_SIZE * 1000000 / 44100 / 2 / 2;
        p_input_item = input_item_NewWithType( psz_uri, psz_name, 0, NULL, 0,
                                               i_duration, ITEM_TYPE_DISC );
        input_item_CopyOptions( p_current, p_input_item );
        input_item_AddOption( p_input_item, psz_first, VLC_INPUT_OPTION_TRUSTED );
        input_item_AddOption( p_input_item, psz_last, VLC_INPUT_OPTION_TRUSTED );
        input_item_AddOption( p_input_item, psz_opt, VLC_INPUT_OPTION_TRUSTED );

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
            input_item_SetName( p_input_item, psz_track_title );
            input_item_SetTitle( p_input_item, psz_track_title );
        }

        if( NONEMPTY( psz_track_artist ) )
            input_item_SetArtist( p_input_item, psz_track_artist );

        if( NONEMPTY( psz_track_genre ) )
            input_item_SetGenre( p_input_item, psz_track_genre );

        if( NONEMPTY( psz_track_description ) )
            input_item_SetDescription( p_input_item, psz_track_description );

        if( NONEMPTY( psz_album ) )
            input_item_SetAlbum( p_input_item, psz_album );

        if( NONEMPTY( psz_year ) )
            input_item_SetDate( p_input_item, psz_year );

        char psz_num[3+1];
        snprintf( psz_num, sizeof(psz_num), "%d", 1+i );
        input_item_SetTrackNum( p_input_item, psz_num );

        input_item_node_AppendItem( p_root, p_input_item );
        vlc_gc_decref( p_input_item );
        free( psz_uri ); free( psz_opt ); free( psz_name );
        free( psz_first ); free( psz_last );
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

#ifdef HAVE_LIBCDDB
static cddb_disc_t *GetCDDBInfo( access_t *p_access, int i_titles, int *p_sectors )
{
    if( var_InheritInteger( p_access, "album-art" ) == ALBUM_ART_WHEN_ASKED )
        return NULL;

    /* */
    cddb_conn_t *p_cddb = cddb_new();
    if( !p_cddb )
    {
        msg_Warn( p_access, "unable to use CDDB" );
        return NULL;
    }

    /* */

    cddb_http_enable( p_cddb );

    char *psz_tmp = var_InheritString( p_access, "cddb-server" );
    if( psz_tmp )
    {
        cddb_set_server_name( p_cddb, psz_tmp );
        free( psz_tmp );
    }

    cddb_set_server_port( p_cddb, var_InheritInteger( p_access, "cddb-port" ) );

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
        msg_Err( p_access, "unable to create CDDB disc structure." );
        goto error;
    }

    int64_t i_length = 0;
    for( int i = 0; i < i_titles; i++ )
    {
        cddb_track_t *t = cddb_track_new();
        cddb_track_set_frame_offset( t, p_sectors[i] );
        cddb_disc_add_track( p_disc, t );
        const int64_t i_size = ( p_sectors[i+1] - p_sectors[i] ) *
                               (int64_t)CDDA_DATA_SIZE;
        i_length += INT64_C(1000000) * i_size / 44100 / 4  ;
    }

    cddb_disc_set_length( p_disc, (int)(i_length/1000000) );

    if( !cddb_disc_calc_discid( p_disc ) )
    {
        msg_Err( p_access, "CDDB disc ID calculation failed" );
        goto error;
    }

    const int i_matches = cddb_query( p_cddb, p_disc );
    if( i_matches < 0 )
    {
        msg_Warn( p_access, "CDDB error: %s", cddb_error_str(errno) );
        goto error;
    }
    else if( i_matches == 0 )
    {
        msg_Dbg( p_access, "Couldn't find any matches in CDDB." );
        goto error;
    }
    else if( i_matches > 1 )
        msg_Warn( p_access, "found %d matches in CDDB. Using first one.", i_matches );

    cddb_read( p_cddb, p_disc );

    cddb_destroy( p_cddb);
    return p_disc;

error:
    if( p_disc )
        cddb_disc_destroy( p_disc );
    cddb_destroy( p_cddb );
    return NULL;
}
#endif /*HAVE_LIBCDDB*/

