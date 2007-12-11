/*****************************************************************************
 * update.c: VLC update and plugins download
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea -at- videolan -dot- org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either release 2 of the License, or
 * (at your option) any later release.
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

/**
 *   \file
 *   This file contains functions related to VLC and plugins update management
 */

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#include <vlc/vlc.h>

#include <ctype.h>                                              /* tolower() */
#include <assert.h>


#include <vlc_update.h>

#include <vlc_block.h>
#include <vlc_stream.h>
#include <vlc_interface.h>
#include <vlc_charset.h>

/*****************************************************************************
 * Misc defines
 *****************************************************************************/

#if defined( UNDER_CE )
#   define UPDATE_VLC_STATUS_URL "http://update.videolan.org/vlc/status-ce"
#elif defined( WIN32 )
#   define UPDATE_VLC_STATUS_URL "http://update.videolan.org/vlc/status-win-x86"
#elif defined( __APPLE__ )
#   define UPDATE_VLC_OS "macosx"
#   if defined( __powerpc__ ) || defined( __ppc__ ) || defined( __ppc64__ )
#       define UPDATE_VLC_STATUS_URL "http://update.videolan.org/vlc/status-mac-ppc"
#   else
#       define UPDATE_VLC_STATUS_URL "http://update.videolan.org/vlc/status-mac-x86"
#   endif
#elif defined( SYS_BEOS )
#       define UPDATE_VLC_STATUS_URL "http://update.videolan.org/vlc/status-beos-x86"
#else
#   define UPDATE_VLC_STATUS_URL "http://update.videolan.org/vlc/status"
#endif

#define STRDUP( a ) ( a ? strdup( a ) : NULL )


/*****************************************************************************
 * Local Prototypes
 *****************************************************************************/
static void EmptyRelease( update_t *p_update );
static void GetUpdateFile( update_t *p_update );
static int extracmp( char *psz_1, char *psz_2 );
static int CompareReleases( const struct update_release_t *p1,
                            const struct update_release_t *p2 );


/*****************************************************************************
 * Update_t functions
 *****************************************************************************/

/**
 * Create a new update VLC struct
 *
 * \param p_this the calling vlc_object
 * \return pointer to new update_t or NULL
 */
update_t *__update_New( vlc_object_t *p_this )
{
    update_t *p_update;

    if( p_this == NULL ) return NULL;

    p_update = (update_t *)malloc( sizeof( update_t ) );
    if( !p_update ) return NULL;

    vlc_mutex_init( p_this, &p_update->lock );

    p_update->p_libvlc = p_this->p_libvlc;

    p_update->release.psz_svnrev = NULL;
    p_update->release.psz_extra = NULL;
    p_update->release.psz_url = NULL;
    p_update->release.psz_desc = NULL;

    return p_update;
}

/**
 * Delete an update_t struct
 *
 * \param p_update update_t* pointer
 * \return nothing
 */
void update_Delete( update_t *p_update )
{
    assert( p_update );

    vlc_mutex_destroy( &p_update->lock );

    FREENULL( p_update->release.psz_svnrev );
    FREENULL( p_update->release.psz_extra );
    FREENULL( p_update->release.psz_url );
    FREENULL( p_update->release.psz_desc );

    free( p_update );
}

/**
 * Empty the release struct
 *
 * \param p_update update_t* pointer
 * \return nothing
 */
static void EmptyRelease( update_t *p_update )
{
    p_update->release.i_major = 0;
    p_update->release.i_minor = 0;
    p_update->release.i_revision = 0;

    FREENULL( p_update->release.psz_svnrev );
    FREENULL( p_update->release.psz_extra );
    FREENULL( p_update->release.psz_url );
    FREENULL( p_update->release.psz_desc );
}

/**
 * Get the update file and parse it
 * *p_update has to be unlocked when calling this function
 *
 * \param p_update pointer to update struct
 * \return nothing
 */
static void GetUpdateFile( update_t *p_update )
{
    stream_t *p_stream = NULL;
    int i_major = 0;
    int i_minor = 0;
    int i_revision = 0;
    char *psz_extra = NULL;
    char *psz_svnrev = NULL;
    char *psz_line = NULL;

    vlc_mutex_lock( &p_update->lock );

    p_stream = stream_UrlNew( p_update->p_libvlc, UPDATE_VLC_STATUS_URL );
    if( !p_stream )
    {
        msg_Err( p_update->p_libvlc, "Failed to open %s for reading",
                 UPDATE_VLC_STATUS_URL );
        goto error;
    }

    /* Try to read three lines */
    if( !( psz_line = stream_ReadLine( p_stream ) ) )
    {
        msg_Err( p_update->p_libvlc, "Update file %s is corrupted : missing version",
                 UPDATE_VLC_STATUS_URL );
        goto error;
    }

    /* first line : version number */
    if( sscanf( psz_line, "%i.%i.%i%as %as", &i_major, &i_minor, &i_revision, &psz_extra, &psz_svnrev ) )
    {
        p_update->release.i_major = i_major;
        p_update->release.i_minor = i_minor;
        p_update->release.i_revision = i_revision;

        p_update->release.psz_svnrev = psz_svnrev ? psz_svnrev : STRDUP( "" );
        p_update->release.psz_extra = psz_extra ? psz_extra : STRDUP( "" );
    }
    else
    {
        msg_Err( p_update->p_libvlc, "Update version false formated" );
        free( psz_line );
        goto error;
    }

    /* Second line : URL */
    if( !( psz_line = stream_ReadLine( p_stream ) ) )
    {
        msg_Err( p_update->p_libvlc, "Update file %s is corrupted : URL missing",
                 UPDATE_VLC_STATUS_URL );
        goto error;
    }
    p_update->release.psz_url = psz_line;


    /* Third line : description */
    if( !( psz_line = stream_ReadLine( p_stream ) ) )
    {
        msg_Err( p_update->p_libvlc, "Update file %s is corrupted : description missing",
                 UPDATE_VLC_STATUS_URL );
        goto error;
    }
    p_update->release.psz_desc = psz_line;

    error:
        vlc_mutex_unlock( &p_update->lock );

        if( p_stream )
            stream_Delete( p_stream );
}

/**
 * Check for updates
 *
 * \param p_update pointer to update struct
 * \returns nothing
 */
void update_Check( update_t *p_update )
{
    assert( p_update );

    EmptyRelease( p_update );

    GetUpdateFile( p_update );
}

/**
 * Compare two extra
 *
 * \param p1 first integer
 * \param p2 second integer
 * \return like strcmp
 */
static int extracmp( char *psz_1, char *psz_2 )
{
    if( psz_1[0] == '-' )
    {
        if( psz_2[0] == '-' )
            return strcmp( psz_1, psz_2 );
        else
            return 1;
    }
    else
    {
        if( psz_2[0] == '-' )
            return -1;
        else
            return strcmp( psz_1, psz_2 );
    }
}
/**
 * Compare two release numbers
 *
 * \param p1 first release
 * \param p2 second release
 * \return UpdateReleaseStatus(Older|Equal|Newer)
 */
static int CompareReleases( const struct update_release_t *p1,
                            const struct update_release_t *p2 )
{
    int32_t d;
    d = ( p1->i_major << 24 ) + ( p1->i_minor << 16 ) + ( p1->i_revision << 8 );
    d = d - ( p2->i_major << 24 ) - ( p2->i_minor << 16 ) - ( p2->i_revision << 8 );
    d += extracmp( p1->psz_extra, p2->psz_extra );

    if( d == 0 )
        d = strcmp( p1->psz_svnrev, p2->psz_svnrev );

    if( d < 0 )
        return UpdateReleaseStatusOlder;
    else if( d == 0 )
        return UpdateReleaseStatusEqual;
    else
        return UpdateReleaseStatusNewer;
}

/**
 * Compare a given release's version number to the current VLC's one
 *
 * \param p a release
 * \return UpdateReleaseStatus(Older|Equal|Newer)
 */
int update_CompareReleaseToCurrent( update_t *p_update )
{
    assert( p_update );

    struct update_release_t c;
    int i_major = 0;
    int i_minor = 0;
    int i_revision = 0;
    char *psz_extra;
    int i_result = UpdateReleaseStatusOlder;

    /* get the current version number */
    if( sscanf( PACKAGE_VERSION, "%i.%i.%i%as", &i_major, &i_minor, &i_revision, &psz_extra ) )
    {
        c.i_major = i_major;
        c.i_minor = i_minor;
        c.i_revision = i_revision;
        if( psz_extra )
            c.psz_extra = psz_extra;
        else
            c.psz_extra = STRDUP( "" );
        c.psz_svnrev = STRDUP( VLC_Changeset() );

        i_result = CompareReleases( &p_update->release, &c );

        free( c.psz_extra );
        free( c.psz_svnrev );
    }
    return i_result;
}
