/*****************************************************************************
 * update.c: VLC update and plugins download
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id: $
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

/* TODO
 * --> check release types.
 * --> make sure that the version comparision method is ok.
 */

/**
 *   \file
 *   This file contains functions related to VLC and plugins update management
 */

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <ctype.h>                                              /* tolower() */

#include <vlc/vlc.h>

#include "vlc_update.h"

#include "vlc_block.h"
#include "vlc_stream.h"
#include "vlc_xml.h"
#include "vlc_interaction.h"

/*****************************************************************************
 * Misc defines
 *****************************************************************************/

/* All release notes and source packages should match on "*"
 * Only binary installers are OS specific ( we only provide these
 * for Win32, Mac OS X, WincCE, beos(?) ) */
#if defined( UNDER_CE )
#   define UPDATE_VLC_OS "*"
#   define UPDATE_VLC_ARCH "*"
#elif defined( WIN32 )
#   define UPDATE_VLC_OS "windows"
#   define UPDATE_VLC_ARCH "i386"
#elif defined( __APPLE__ )
#   define UPDATE_VLC_OS "macosx"
#   if defined( __powerpc__ ) || defined( __ppc__ ) || defined( __ppc64__ )
#       define UPDATE_VLC_ARCH "ppc"
#   else
#       define UPDATE_VLC_ARCH "x86"
#   endif
#elif defined( SYS_BEOS )
#   define UPDATE_VLC_OS "beos"
#   define UPDATE_VLC_ARCH "i386"
#else
#   define UPDATE_VLC_OS "*"
#   define UPDATE_VLC_ARCH "*"
#endif

#define UPDATE_VLC_STATUS_URL "http://update.videolan.org/vlc/status.xml"
#define UPDATE_VLC_MIRRORS_URL "http://update.videolan.org/mirrors.xml"

#define FREE( a ) free(a);a=NULL;
#define STRDUP( a ) ( a ? strdup( a ) : NULL )

/*****************************************************************************
 * Local Prototypes
 *****************************************************************************/

void FreeMirrorsList( update_t * );
void FreeReleasesList( update_t * );
void GetMirrorsList( update_t *, vlc_bool_t );
void GetFilesList( update_t *, vlc_bool_t );

int CompareReleases( struct update_release_t *, struct update_release_t * );
int CompareReleaseToCurrent( struct update_release_t * );

unsigned int update_iterator_Reset( update_iterator_t * );
unsigned int update_iterator_NextFile( update_iterator_t * );
unsigned int update_iterator_PrevFile( update_iterator_t * );
unsigned int update_iterator_NextMirror( update_iterator_t * );
unsigned int update_iterator_PrevMirror( update_iterator_t * );

void update_iterator_GetData( update_iterator_t * );
void update_iterator_ClearData( update_iterator_t * );

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

    vlc_mutex_init( p_this, &p_update->lock );

    p_update->p_vlc = p_this->p_vlc;

    p_update->p_releases = NULL;
    p_update->i_releases = 0;
    p_update->b_releases = VLC_FALSE;

    p_update->p_mirrors = NULL;
    p_update->i_mirrors = 0;
    p_update->b_mirrors = VLC_FALSE;

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
    vlc_mutex_destroy( &p_update->lock );
    FreeMirrorsList( p_update );
    FreeReleasesList( p_update );
    free( p_update );
}

/**
 * Empty the mirrors list
 * *p_update should be locked before using this function
 *
 * \param p_update pointer to the update struct
 * \return nothing
 */
void FreeMirrorsList( update_t *p_update )
{
    int i;

    for( i = 0; i < p_update->i_mirrors; i++ )
    {
        free( p_update->p_mirrors[i].psz_name );
        free( p_update->p_mirrors[i].psz_location );
        free( p_update->p_mirrors[i].psz_type );
        free( p_update->p_mirrors[i].psz_base_url );
    }
    FREE( p_update->p_mirrors );
    p_update->i_mirrors = 0;
    p_update->b_mirrors = VLC_FALSE;
}

/**
 * Empty the releases list
 * *p_update should be locked before calling this function
 *
 * \param p_update pointer to the update struct
 * \return nothing
 */
void FreeReleasesList( update_t *p_update )
{
    int i;

    for( i = 0; i < p_update->i_releases; i++ )
    {
        int j;
        struct update_release_t *p_release = (p_update->p_releases + i);
        for( j = 0; j < p_release->i_files; j++ )
        {
            free( p_release->p_files[j].psz_md5 );
            free( p_release->p_files[j].psz_url );
            free( p_release->p_files[j].psz_description );
        }
        free( p_release->psz_major );
        free( p_release->psz_minor );
        free( p_release->psz_revision );
        free( p_release->psz_extra );
        free( p_release->psz_svn_revision );
        free( p_release->p_files );
    }
    FREE( p_update->p_releases );
    p_update->i_releases = 0;
    p_update->b_releases = VLC_FALSE;
}

/**
 * Get the mirrors list XML file and parse it
 * *p_update has to be unlocked when calling this function
 *
 * \param p_update pointer to the update struct
 * \param b_force set to VLC_TRUE if you want to force the mirrors list update
 * \return nothing
 */
void GetMirrorsList( update_t *p_update, vlc_bool_t b_force )
{
    stream_t *p_stream = NULL;

    xml_t *p_xml = NULL;
    xml_reader_t *p_xml_reader = NULL;

    char *psz_eltname = NULL;
    //char *psz_eltvalue = NULL;
    char *psz_name = NULL;
    char *psz_value = NULL;

    struct update_mirror_t tmp_mirror = {0};

    vlc_mutex_lock( &p_update->lock );

    if( p_update->b_mirrors && b_force == VLC_FALSE )
    {
        vlc_mutex_unlock( &p_update->lock );
        return;
    }

    p_xml = xml_Create( p_update->p_vlc );
    if( !p_xml )
    {
        msg_Err( p_update->p_vlc, "Failed to open XML parser" );
        goto error;
    }

    p_stream = stream_UrlNew( p_update->p_vlc, UPDATE_VLC_MIRRORS_URL );
    if( !p_stream )
    {
        msg_Err( p_update->p_vlc, "Failed to open %s for reading",
                 UPDATE_VLC_MIRRORS_URL );
        goto error;
    }

    p_xml_reader = xml_ReaderCreate( p_xml, p_stream );

    if( !p_xml_reader )
    {
        msg_Err( p_update->p_vlc, "Failed to open %s for parsing",
                 UPDATE_VLC_MIRRORS_URL );
        goto error;
    }

    if( p_update->p_mirrors )
    {
        FreeMirrorsList( p_update );
    }

    while( xml_ReaderRead( p_xml_reader ) == 1 )
    {
        switch( xml_ReaderNodeType( p_xml_reader ) )
        {
            case -1:
                msg_Err( p_update->p_vlc, "Error while parsing %s",
                         UPDATE_VLC_MIRRORS_URL );
                goto error;

            case XML_READER_STARTELEM:
                psz_eltname = xml_ReaderName( p_xml_reader );
                if( !psz_eltname )
                {
                    msg_Err( p_update->p_vlc, "Error while parsing %s",
                             UPDATE_VLC_MIRRORS_URL );
                    goto error;
                }

                while( xml_ReaderNextAttr( p_xml_reader ) == VLC_SUCCESS )
                {
                    psz_name = xml_ReaderName( p_xml_reader );
                    psz_value = xml_ReaderValue( p_xml_reader );

                    if( !psz_name || !psz_value )
                    {
                        msg_Err( p_update->p_vlc, "Error while parsing %s",
                                 UPDATE_VLC_MIRRORS_URL );
                        goto error;
                    }

                    if( !strcmp( psz_eltname, "mirror" ) )
                    {
                        if( !strcmp( psz_name, "name" ) )
                            tmp_mirror.psz_name = STRDUP( psz_value );
                        else if( !strcmp( psz_name, "location" ) )
                            tmp_mirror.psz_location = STRDUP( psz_value );
                    }
                    else if( !strcmp( psz_eltname, "url" ) )
                    {
                        if( !strcmp( psz_name, "type" ) )
                            tmp_mirror.psz_type = STRDUP( psz_value );
                        else if( !strcmp( psz_name, "base" ) )
                            tmp_mirror.psz_base_url = STRDUP( psz_value );
                    }
                    FREE( psz_name );
                    FREE( psz_value );
                }
                if( !strcmp( psz_eltname, "url" ) )
                {
                    /* append to mirrors list */
                    p_update->p_mirrors =
                    (struct update_mirror_t *)realloc( p_update->p_mirrors,
                                       (++(p_update->i_mirrors))
                                       *sizeof( struct update_mirror_t ) );
                    p_update->p_mirrors[ p_update->i_mirrors - 1 ] =
                        tmp_mirror;
                    tmp_mirror.psz_name = STRDUP( tmp_mirror.psz_name );
                    tmp_mirror.psz_location = STRDUP( tmp_mirror.psz_location );
                    tmp_mirror.psz_type = NULL;
                    tmp_mirror.psz_base_url = NULL;
                }
                FREE( psz_eltname );
                break;

            case XML_READER_ENDELEM:
                psz_eltname = xml_ReaderName( p_xml_reader );
                if( !psz_eltname )
                {
                    msg_Err( p_update->p_vlc, "Error while parsing %s",
                             UPDATE_VLC_MIRRORS_URL );
                    goto error;
                }

                if( !strcmp( psz_eltname, "mirror" ) )
                {
                    FREE( tmp_mirror.psz_name );
                    FREE( tmp_mirror.psz_location );
                }

                FREE( psz_eltname );
                break;

            /*case XML_READER_TEXT:
                psz_eltvalue = xml_ReaderValue( p_xml_reader );
                FREE( psz_eltvalue );
                break;*/
        }
    }

    p_update->b_mirrors = VLC_TRUE;

    error:
        vlc_mutex_unlock( &p_update->lock );

        free( psz_eltname );
        //free( psz_eltvalue );
        free( psz_name );
        free( psz_value );

        free( tmp_mirror.psz_name );
        free( tmp_mirror.psz_location );
        free( tmp_mirror.psz_type );
        free( tmp_mirror.psz_base_url );

        if( p_xml_reader && p_xml )
            xml_ReaderDelete( p_xml, p_xml_reader );
        if( p_stream )
            stream_Delete( p_stream );
        if( p_xml )
            xml_Delete( p_xml );
}

/**
 * Get the files list XML file and parse it
 * *p_update has to be unlocked when calling this function
 *
 * \param p_update pointer to update struct
 * \param b_force set to VLC_TRUE if you want to force the files list update
 * \return nothing
 */
void GetFilesList( update_t *p_update, vlc_bool_t b_force )
{
    stream_t *p_stream = NULL;

    xml_t *p_xml = NULL;
    xml_reader_t *p_xml_reader = NULL;

    char *psz_eltname = NULL;
    char *psz_eltvalue = NULL;
    char *psz_name = NULL;
    char *psz_value = NULL;

    struct update_release_t *p_release = NULL;
    struct update_release_t tmp_release = {0};
    struct update_file_t tmp_file = {0};
    tmp_release.i_type = UPDATE_RELEASE_TYPE_STABLE;

    vlc_bool_t b_os = VLC_FALSE, b_arch = VLC_FALSE;

    vlc_mutex_lock( &p_update->lock );

    if( p_update->b_releases && b_force == VLC_FALSE )
    {
        vlc_mutex_unlock( &p_update->lock );
        return;
    }

    p_xml = xml_Create( p_update->p_vlc );
    if( !p_xml )
    {
        msg_Err( p_update->p_vlc, "Failed to open XML parser" );
        goto error;
    }

    p_stream = stream_UrlNew( p_update->p_vlc, UPDATE_VLC_STATUS_URL );
    if( !p_stream )
    {
        msg_Err( p_update->p_vlc, "Failed to open %s for reading",
                 UPDATE_VLC_STATUS_URL );
        goto error;
    }

    p_xml_reader = xml_ReaderCreate( p_xml, p_stream );

    if( !p_xml_reader )
    {
        msg_Err( p_update->p_vlc, "Failed to open %s for parsing",
                 UPDATE_VLC_STATUS_URL );
        goto error;
    }

    if( p_update->p_releases )
    {
        FreeReleasesList( p_update );
    }

    while( xml_ReaderRead( p_xml_reader ) == 1 )
    {
        switch( xml_ReaderNodeType( p_xml_reader ) )
        {
            case -1:
                msg_Err( p_update->p_vlc, "Error while parsing %s",
                         UPDATE_VLC_STATUS_URL );
                goto error;

            case XML_READER_STARTELEM:
                psz_eltname = xml_ReaderName( p_xml_reader );
                if( !psz_eltname )
                {
                    msg_Err( p_update->p_vlc, "Error while parsing %s",
                             UPDATE_VLC_STATUS_URL );
                    goto error;
                }

                while( xml_ReaderNextAttr( p_xml_reader ) == VLC_SUCCESS )
                {
                    psz_name = xml_ReaderName( p_xml_reader );
                    psz_value = xml_ReaderValue( p_xml_reader );

                    if( !psz_name || !psz_value )
                    {
                        msg_Err( p_update->p_vlc, "Error while parsing %s",
                                 UPDATE_VLC_STATUS_URL );
                        goto error;
                    }

                    if( b_os && b_arch )
                    {
                        if( strcmp( psz_eltname, "version" ) == 0 )
                        {
                            if( !strcmp( psz_name, "major" ) )
                                tmp_release.psz_major = STRDUP( psz_value );
                            else if( !strcmp( psz_name, "minor" ) )
                                tmp_release.psz_minor = STRDUP( psz_value );
                            else if( !strcmp( psz_name, "revision" ) )
                                tmp_release.psz_revision = STRDUP( psz_value );
                            else if( !strcmp( psz_name, "extra" ) )
                                tmp_release.psz_extra = STRDUP( psz_value );
                            else if( !strcmp( psz_name, "svn" ) )
                                tmp_release.psz_svn_revision =
                                                           STRDUP( psz_value );
                            else if( !strcmp( psz_name, "version" ) )
                            {
                                if( !strcmp( psz_value, "unstable" ) )
                                    tmp_release.i_type =
                                                  UPDATE_RELEASE_TYPE_UNSTABLE;
                                else if( !strcmp( psz_value, "testing" ) )
                                    tmp_release.i_type =
                                                  UPDATE_RELEASE_TYPE_TESTING;
                                else
                                    tmp_release.i_type =
                                                  UPDATE_RELEASE_TYPE_STABLE;
                            }
                        }
                        else if( !strcmp( psz_eltname, "file" ) )
                        {
                            if( !strcmp( psz_name, "type" ) )
                            {
                                if( !strcmp( psz_value, "info" ) )
                                    tmp_file.i_type = UPDATE_FILE_TYPE_INFO;
                                else if( !strcmp( psz_value, "source" ) )
                                    tmp_file.i_type = UPDATE_FILE_TYPE_SOURCE;
                                else if( !strcmp( psz_value, "binary" ) )
                                    tmp_file.i_type = UPDATE_FILE_TYPE_BINARY;
                                else if( !strcmp( psz_value, "plugin" ) )
                                    tmp_file.i_type = UPDATE_FILE_TYPE_PLUGIN;
                                else
                                    tmp_file.i_type = UPDATE_FILE_TYPE_UNDEF;
                            }
                            else if( !strcmp( psz_name, "md5" ) )
                                tmp_file.psz_md5 = STRDUP( psz_value );
                            else if( !strcmp( psz_name, "size" ) )
                                tmp_file.l_size = atol( psz_value );
                            else if( !strcmp( psz_name, "url" ) )
                                tmp_file.psz_url = STRDUP( psz_value );
                        }
                    }
                    if( !strcmp( psz_name, "name" )
                        && ( !strcmp( psz_value, UPDATE_VLC_OS )
                           || !strcmp( psz_value, "*" ) )
                        && !strcmp( psz_eltname, "os" ) )
                    {
                        b_os = VLC_TRUE;
                    }
                    if( b_os && !strcmp( psz_name, "name" )
                        && ( !strcmp( psz_value, UPDATE_VLC_ARCH )
                           || !strcmp( psz_value, "*" ) )
                        && !strcmp( psz_eltname, "arch" ) )
                    {
                        b_arch = VLC_TRUE;
                    }
                    FREE( psz_name );
                    FREE( psz_value );
                }
                if( ( b_os && b_arch && strcmp( psz_eltname, "arch" ) ) )
                {
                    if( !strcmp( psz_eltname, "version" ) )
                    {
                        int i;
                        /* look for a previous occurence of this release */
                        for( i = 0; i < p_update->i_releases; i++ )
                        {
                            p_release = p_update->p_releases + i;
                            if( CompareReleases( p_release, &tmp_release )
                                == UPDATE_RELEASE_STATUS_EQUAL )
                            {
                                break;
                            }
                        }
                        /* if this is the first time that we see this release,
                         * append it to the list of releases */
                        if( i == p_update->i_releases )
                        {
                            tmp_release.i_status =
                                CompareReleaseToCurrent( &tmp_release );
                            p_update->p_releases =
               (struct update_release_t *)realloc( p_update->p_releases,
               (++(p_update->i_releases))*sizeof( struct update_release_t ) );
                            p_update->p_releases[ p_update->i_releases - 1 ] =
                                tmp_release;
                            p_release =
                                p_update->p_releases + p_update->i_releases - 1;
                            tmp_release.psz_major = NULL;
                            tmp_release.psz_minor = NULL;
                            tmp_release.psz_revision = NULL;
                            tmp_release.psz_extra = NULL;
                            tmp_release.psz_svn_revision = NULL;
                            tmp_release.i_type = UPDATE_RELEASE_TYPE_STABLE;
                            tmp_release.i_status = 0;
                            tmp_release.p_files = NULL;
                            tmp_release.i_files = 0;
                        }
                        else
                        {
                            FREE( tmp_release.psz_major );
                            FREE( tmp_release.psz_minor );
                            FREE( tmp_release.psz_revision );
                            FREE( tmp_release.psz_extra );
                            FREE( tmp_release.psz_svn_revision );
                            tmp_release.i_type = UPDATE_RELEASE_TYPE_STABLE;
                            FREE( tmp_release.p_files );
                            tmp_release.i_files = 0;
                        }
                    }
                    else if( !strcmp( psz_eltname, "file" ) )
                    {
                        /* append file to p_release's file list */
                        if( p_release == NULL )
                        {
                            goto error;
                        }
                        p_release->p_files =
                    (struct update_file_t *)realloc( p_release->p_files,
                    (++(p_release->i_files))*sizeof( struct update_file_t ) );
                        p_release->p_files[ p_release->i_files - 1 ] = tmp_file;
                        tmp_file.i_type = UPDATE_FILE_TYPE_UNDEF;
                        tmp_file.psz_md5 = NULL;
                        tmp_file.l_size = 0;
                        tmp_file.psz_url = NULL;
                        tmp_file.psz_description = NULL;
                    }
                }
                FREE( psz_eltname );
                break;

            case XML_READER_ENDELEM:
                psz_eltname = xml_ReaderName( p_xml_reader );
                if( !psz_eltname )
                {
                    msg_Err( p_update->p_vlc, "Error while parsing %s",
                             UPDATE_VLC_STATUS_URL );
                    goto error;
                }

                if( !strcmp( psz_eltname, "os" ) )
                    b_os = VLC_FALSE;
                else if( !strcmp( psz_eltname, "arch" ) )
                    b_arch = VLC_FALSE;
                FREE( psz_eltname );
                break;

            case XML_READER_TEXT:
                psz_eltvalue = xml_ReaderValue( p_xml_reader );
                if( p_release && p_release->i_files )
                    p_release->p_files[ p_release->i_files - 1 ]
                               .psz_description = STRDUP( psz_eltvalue );
                FREE( psz_eltvalue );
                break;
        }
    }

    p_update->b_releases = VLC_TRUE;

    error:
        vlc_mutex_unlock( &p_update->lock );

        free( psz_eltname );
        free( psz_eltvalue );
        free( psz_name );
        free( psz_value );

        free( tmp_release.psz_major );
        free( tmp_release.psz_minor );
        free( tmp_release.psz_revision );
        free( tmp_release.psz_extra );
        free( tmp_release.psz_svn_revision );

        free( tmp_file.psz_md5 );
        free( tmp_file.psz_url );
        free( tmp_file.psz_description );

        if( p_xml_reader && p_xml )
            xml_ReaderDelete( p_xml, p_xml_reader );
        if( p_stream )
            stream_Delete( p_stream );
        if( p_xml )
            xml_Delete( p_xml );
}

/**
 * Check for updates
 *
 * \param p_update pointer to update struct
 * \param b_force set to VLC_TRUE if you want to force the update
 * \returns nothing
 */
void update_Check( update_t *p_update, vlc_bool_t b_force )
{
    if( p_update == NULL ) return;
    GetMirrorsList( p_update, b_force );
    GetFilesList( p_update, b_force );
}

/**
 * Compare two release numbers
 * The comparision algorith basically performs an alphabetical order (strcmp)
 * comparision of each of the version number elements until it finds two
 * different ones. This is the tricky function.
 *
 * \param p1 first release
 * \param p2 second release
 * \return like strcmp
 */
int CompareReleases( struct update_release_t *p1, struct update_release_t *p2 )
{
    int d;
    if( ( d = strcmp( p1->psz_major, p2->psz_major ) ) ) ;
    else if( ( d = strcmp( p1->psz_minor, p2->psz_minor ) ) ) ;
    else if( ( d = strcmp( p1->psz_revision, p2->psz_revision ) ) ) ;
    else
    {
        d = strcmp( p1->psz_extra, p2->psz_extra );
        if( d<0 )
        {
        /* FIXME:
         * not num < NULL < num
         * -test and -svn releases are thus always considered older than
         * -'' or -0 releases, which is the best i could come up with */
            char *psz_end1;
            char *psz_end2;
            strtol( p1->psz_extra, &psz_end1, 10 );
            strtol( p2->psz_extra, &psz_end2, 10 );
            if( psz_end2 == p2->psz_extra
             && ( psz_end1 != p1->psz_extra || *p1->psz_extra == '\0' ) )
                d = 1;
        }
    }
    if( d < 0 )
        return UPDATE_RELEASE_STATUS_OLDER;
    else if( d == 0 )
        return UPDATE_RELEASE_STATUS_EQUAL;
    else
        return UPDATE_RELEASE_STATUS_NEWER;
}

/**
 * Compare a given release's version number to the current VLC's one
 *
 * \param p a release
 * \return >0 if newer, 0 if equal and <0 if older
 */
int CompareReleaseToCurrent( struct update_release_t *p )
{
    struct update_release_t c = {0};
    int r;
    c.psz_major = STRDUP( PACKAGE_VERSION_MAJOR );
    c.psz_minor = STRDUP( PACKAGE_VERSION_MINOR );
    c.psz_revision = STRDUP( PACKAGE_VERSION_REVISION );
    c.psz_extra = STRDUP( PACKAGE_VERSION_EXTRA );
    r =  CompareReleases( p, &c );
    free( c.psz_major );
    free( c.psz_minor );
    free( c.psz_revision );
    free( c.psz_extra );
    return r;
}

/*****************************************************************************
 * Updatei_iterator_t functions
 *****************************************************************************/

/**
 * Create a new update iterator structure. This structure can then be used to
 * describe a position and move through the update and mirror trees/lists.
 * This will use an existing update struct or create a new one if none is
 * found
 *
 * \param p_u the calling update_t
 * \return a pointer to an update iterator
 */
update_iterator_t *update_iterator_New( update_t *p_u )
{
    update_iterator_t *p_uit = NULL;

    if( p_u == NULL )
        return NULL;

    p_uit = (update_iterator_t *)malloc( sizeof( update_iterator_t ) );
    if( p_uit == NULL ) return NULL;

    p_uit->p_u = p_u;

    p_uit->i_m = -1;
    p_uit->i_r = -1;
    p_uit->i_f = -1;

    p_uit->i_t = UPDATE_FILE_TYPE_ALL;
    p_uit->i_rs = UPDATE_RELEASE_STATUS_ALL;
    p_uit->i_rt = UPDATE_RELEASE_TYPE_STABLE;

    p_uit->file.i_type = UPDATE_FILE_TYPE_NONE;
    p_uit->file.psz_md5 = NULL;
    p_uit->file.psz_url = NULL;
    p_uit->file.l_size = 0;
    p_uit->file.psz_description = NULL;

    p_uit->release.psz_version = NULL;
    p_uit->release.psz_svn_revision = NULL;
    p_uit->release.i_type = UPDATE_RELEASE_TYPE_UNSTABLE;
    p_uit->release.i_status = UPDATE_RELEASE_STATUS_NONE;

    p_uit->mirror.psz_name = NULL;
    p_uit->mirror.psz_location = NULL;
    p_uit->mirror.psz_type = NULL;

    return p_uit;
}

/**
 * Delete an update iterator structure (duh!)
 *
 * \param p_uit pointer to an update iterator
 * \return nothing
 */
void update_iterator_Delete( update_iterator_t *p_uit )
{
    if( !p_uit ) return;
    update_iterator_ClearData( p_uit );
    free( p_uit );
}

/**
 * Reset an update_iterator_t structure
 *
 * \param p_uit pointer to an update iterator
 * \return UPDATE_FAIL upon error, UPDATE_SUCCESS otherwise
 */
unsigned int update_iterator_Reset( update_iterator_t *p_uit )
{
    if( !p_uit ) return UPDATE_FAIL;

    p_uit->i_r = -1;
    p_uit->i_f = -1;
    p_uit->i_m = -1;

    update_iterator_ClearData( p_uit );
    return UPDATE_SUCCESS;
}

/**
 * Finds the next file in the update tree that matches status and type
 * requirements set in the update_iterator
 *
 * \param p_uit update iterator
 * \return UPDATE_FAIL if we can't find the next file, UPDATE_SUCCESS|UPDATE_FILE if we stay in the same release, UPDATE_SUCCESS|UPDATE_RELEASE|UPDATE_FILE if we change the release index
 */
unsigned int update_iterator_NextFile( update_iterator_t *p_uit )
{
    int r,f=-1,old_r;

    if( !p_uit ) return UPDATE_FAIL;

    old_r=p_uit->i_r;

    /* if the update iterator was already in a "no match" state, start over */
    if( p_uit->i_r == -1 ) p_uit->i_r = 0;
    //if( p_uit->i_f == -1 ) p_uit->i_f = 0;

    vlc_mutex_lock( &p_uit->p_u->lock );

    for( r = p_uit->i_r; r < p_uit->p_u->i_releases; r++ )
    {
        if( !( p_uit->p_u->p_releases[r].i_status & p_uit->i_rs ) ) continue;
        for( f = ( r == p_uit->i_r ? p_uit->i_f + 1 : 0 );
             f < p_uit->p_u->p_releases[r].i_files; f++ )
        {
            if( p_uit->p_u->p_releases[r].p_files[f].i_type & p_uit->i_t )
            {
                goto done;/* "double break" */
            }
        }
    }
    done:
    p_uit->i_r = r;
    p_uit->i_f = f;

    r = p_uit->p_u->i_releases;

    if( old_r == p_uit->i_r )
    {
        update_iterator_GetData( p_uit );
        vlc_mutex_unlock( &p_uit->p_u->lock );
        return UPDATE_SUCCESS|UPDATE_FILE;
    }
    else if( p_uit->i_r == r )
    {
        p_uit->i_r = -1;
        p_uit->i_f = -1;
        update_iterator_GetData( p_uit );
        vlc_mutex_unlock( &p_uit->p_u->lock );
        return UPDATE_FAIL;
    }
    else
    {
        update_iterator_GetData( p_uit );
        vlc_mutex_unlock( &p_uit->p_u->lock );
        return UPDATE_SUCCESS|UPDATE_RELEASE|UPDATE_FILE;
    }
}

/**
 * Finds the previous file in the update tree that matches status and type
 * requirements set in the update_iterator
 *
 * \param p_uit update iterator
 * \return UPDATE_FAIL if we can't find the previous file, UPDATE_SUCCESS|UPDATE_FILE if we stay in the same release, UPDATE_SUCCESS|UPDATE_RELEASE|UPDATE_FILE if we change the release index
 */
//TODO: test
unsigned int update_iterator_PrevFile( update_iterator_t *p_uit )
{
    int r,f=-1,old_r;

    if( !p_uit ) return UPDATE_FAIL;

    old_r=p_uit->i_r;

    /* if the update iterator was already in a "no match" state, start over
     * (begin at the end of the list) */
    if( p_uit->i_r == -1 ) p_uit->i_r = p_uit->p_u->i_releases - 1;
    p_uit->i_f = p_uit->p_u->p_releases[p_uit->i_r].i_files + 1;

    vlc_mutex_lock( &p_uit->p_u->lock );

    for( r = p_uit->i_r; r >= 0; r-- )
    {
        if( !( p_uit->p_u->p_releases[r].i_status & p_uit->i_rs ) ) continue;
        for( f =( r==p_uit->i_r ? p_uit->i_f - 1 : p_uit->p_u->p_releases[r].i_files );
             f >= 0; f-- )
        {
            if( p_uit->p_u->p_releases[r].p_files[f].i_type & p_uit->i_t )
            {
                goto done;/* "double break" */
            }
        }
    }
    done:
    p_uit->i_r = r;
    p_uit->i_f = f;

    r = p_uit->p_u->i_releases;

    if( old_r == p_uit->i_r )
    {
        update_iterator_GetData( p_uit );
        vlc_mutex_unlock( &p_uit->p_u->lock );
        return UPDATE_SUCCESS|UPDATE_FILE;
    }
    else if( p_uit->i_r == -1 )
    {
        p_uit->i_r = -1;
        p_uit->i_f = -1;
        update_iterator_GetData( p_uit );
        vlc_mutex_unlock( &p_uit->p_u->lock );
        return UPDATE_FAIL;
    }
    else
    {
        update_iterator_GetData( p_uit );
        vlc_mutex_unlock( &p_uit->p_u->lock );
        return UPDATE_SUCCESS|UPDATE_RELEASE|UPDATE_FILE;
    }
}

/**
 * Finds the next mirror in the update tree
 *
 * \param update iterator
 * \return UPDATE_FAIL if we can't find the next mirror, UPDATE_SUCCESS|UPDATE_MIRROR otherwise
 */
unsigned int update_iterator_NextMirror( update_iterator_t *p_uit )
{
    if( !p_uit ) return UPDATE_FAIL;
    vlc_mutex_lock( &p_uit->p_u->lock );
    p_uit->i_m++;
    if( p_uit->i_m >= p_uit->p_u->i_mirrors ) p_uit->i_m = -1;
    update_iterator_GetData( p_uit );
    vlc_mutex_unlock( &p_uit->p_u->lock );
    return p_uit->i_m == -1 ? UPDATE_FAIL : UPDATE_SUCCESS|UPDATE_MIRROR;
}

/**
 * Finds the previous mirror in the update tree
 *
 * \param update iterator
 * \return UPDATE_FAIL if we can't find a previous mirror, UPDATE_SUCCESS|UPDATE_MIRROR otherwise
 */
unsigned int update_iterator_PrevMirror( update_iterator_t *p_uit )
{
    if( !p_uit ) return UPDATE_FAIL;
    vlc_mutex_lock( &p_uit->p_u->lock );
    p_uit->i_m--;
    update_iterator_GetData( p_uit );
    vlc_mutex_unlock( &p_uit->p_u->lock );
    return p_uit->i_m == -1 ? UPDATE_FAIL : UPDATE_SUCCESS|UPDATE_MIRROR;
}

/**
 * Change the update iterator's position in the file and mirrors tree
 * If position is negative, don't change it
 *
 * \param i_m position in mirrors list
 * \param i_r position in releases list
 * \param i_f position in release's files list
 * \return UPDATE_FAIL when changing position fails or position wasn't changed, a combination of UPDATE_MIRROR, UPDATE_RELEASE and UPDATE_FILE otherwise
 */
unsigned int update_iterator_ChooseMirrorAndFile( update_iterator_t *p_uit,
                                        int i_m, int i_r, int i_f )
{
    unsigned int i_val = 0;

    if( !p_uit ) return 0;
    vlc_mutex_lock( &p_uit->p_u->lock );

    if( i_m >= 0 )
    {
        if( i_m < p_uit->p_u->i_mirrors )
        {
            if( i_m != p_uit->i_m )
                i_val |= UPDATE_MIRROR;
            p_uit->i_m = i_m;
        }
        else i_m = -1;
    }

    if( i_r >= 0 )
    {
        if( i_r < p_uit->p_u->i_releases )
        {
            if( i_r != p_uit->i_r )
                i_val |= UPDATE_FILE;
            p_uit->i_r = i_r;
        }
        else i_r = -1;
    }

    if( i_f >= 0 )
    {
        if( i_r >= 0 && i_r < p_uit->p_u->i_releases
            && i_f < p_uit->p_u->p_releases[p_uit->i_r].i_files )
        {
            if( i_f != p_uit->i_f )
                i_val |= UPDATE_FILE;
            p_uit->i_f = i_f;
        }
        else i_f = -1;
    }

    update_iterator_GetData( p_uit );
    vlc_mutex_unlock( &p_uit->p_u->lock );

    if(    ( i_m < 0 || p_uit->i_m >= 0 )
        && ( i_r < 0 || p_uit->i_r >= 0 )
        && ( i_f < 0 || p_uit->i_f >= 0 ) )
    {
        /* Everything worked */
        return UPDATE_SUCCESS|i_val;
    }
    else
    {
        /* Something failed */
        return UPDATE_FAIL;
    }
}

/**
 * Fills the iterator data (file, release and mirror structs)
 * The update struct should be locked before calling this function.
 *
 * \param p_uit update iterator
 * \return nothing
 */
void update_iterator_GetData( update_iterator_t *p_uit )
{
    struct update_release_t *p_r = NULL;
    struct update_file_t *p_f = NULL;
    struct update_mirror_t *p_m = NULL;

    update_iterator_ClearData( p_uit );

    if( p_uit->i_m >= 0 )
    {
        p_m = p_uit->p_u->p_mirrors + p_uit->i_m;
        p_uit->mirror.psz_name = STRDUP( p_m->psz_name );
        p_uit->mirror.psz_location = STRDUP( p_m->psz_location );
        p_uit->mirror.psz_type = STRDUP( p_m->psz_type );
    }

    if( p_uit->i_r >= 0 )
    {
        p_r = p_uit->p_u->p_releases + p_uit->i_r;
        asprintf( &p_uit->release.psz_version, "%s.%s.%s-%s",
                                              p_r->psz_major,
                                              p_r->psz_minor,
                                              p_r->psz_revision,
                                              p_r->psz_extra );
        p_uit->release.psz_svn_revision = STRDUP( p_r->psz_svn_revision );
        p_uit->release.i_type = p_r->i_type;
        p_uit->release.i_status = p_r->i_status;
        if( p_uit->i_f >= 0 )
        {
            p_f = p_r->p_files + p_uit->i_f;
            p_uit->file.i_type = p_f->i_type;
            p_uit->file.psz_md5 = STRDUP( p_f->psz_md5 );
            p_uit->file.l_size = p_f->l_size;
            p_uit->file.psz_description = STRDUP( p_f->psz_description);
            if( p_f->psz_url[0] == '/' )
            {
                if( p_m )
                {
                    asprintf( &p_uit->file.psz_url, "%s%s",
                              p_m->psz_base_url, p_f->psz_url );
                }
            }
            else
            {
                p_uit->file.psz_url = STRDUP( p_f->psz_url );
            }
        }
    }
}

/**
 * Clears the iterator data (file, release and mirror structs)
 *
 * \param p_uit update iterator
 * \return nothing
 */
void update_iterator_ClearData( update_iterator_t *p_uit )
{
    p_uit->file.i_type = UPDATE_FILE_TYPE_NONE;
    FREE( p_uit->file.psz_md5 );
    p_uit->file.l_size = 0;
    FREE( p_uit->file.psz_description );
    FREE( p_uit->file.psz_url );
    FREE( p_uit->release.psz_version );
    FREE( p_uit->release.psz_svn_revision );
    p_uit->release.i_type = UPDATE_RELEASE_TYPE_UNSTABLE;
    p_uit->release.i_status = UPDATE_RELEASE_STATUS_NONE;
    FREE( p_uit->mirror.psz_name );
    FREE( p_uit->mirror.psz_location );
    FREE( p_uit->mirror.psz_type );
}

/**
 * Perform an action on the update iterator
 * Only the first matching action is performed.
 *
 * \param p_uit update iterator
 * \param i_action update action bitmask. can be a combination of UPDATE_NEXT, UPDATE_PREV, UPDATE_MIRROR, UPDATE_RELEASE, UPDATE_FILE, UPDATE_RESET
 * \return UPDATE_FAIL if action fails, UPDATE_SUCCESS|(combination of UPDATE_MIRROR, UPDATE_RELEASE and UPDATE_FILE if these changed) otherwise
 */
unsigned int update_iterator_Action( update_iterator_t *p_uit, int i_action )
{
    if( i_action & UPDATE_RESET )
    {
        return update_iterator_Reset( p_uit );
    }
    else
    if( i_action & UPDATE_MIRROR )
    {
        if( i_action & UPDATE_PREV )
        {
            return update_iterator_PrevMirror( p_uit );
        }
        else
        {
            return update_iterator_NextMirror( p_uit );
        }
    }
    /*else if( i_action & UPDATE_RELEASE )
    {
        if( i_action & UPDATE_PREV )
        {
            return update_iterator_PrevRelease( p_uit );
        }
        else
        {
            return update_iterator_NextRelease( p_uit );
        }
    }*/
    else if( i_action & UPDATE_FILE )
    {
        if( i_action & UPDATE_PREV )
        {
            return update_iterator_PrevFile( p_uit );
        }
        else
        {
            return update_iterator_NextFile( p_uit );
        }
    }
    else
    {
        return UPDATE_SUCCESS;
    }
}

/**
 * Object to launch download thread in a different object
 */
typedef struct {
    VLC_COMMON_MEMBERS
    char *psz_dest;     //< Download destination
    char *psz_src;      //< Download source
    char *psz_status;   //< Download status displayed in progress dialog
} download_thread_t;

void update_download_for_real( download_thread_t *p_this );

/**
 * Download the file selected by the update iterator. This function will
 * launch the download in a new thread (downloads can be long)
 *
 * \param p_uit update iterator
 * \param psz_dest destination file path
 * \return nothing
 */
void update_download( update_iterator_t *p_uit, char *psz_dest )
{
    download_thread_t *p_dt =
        vlc_object_create( p_uit->p_u->p_vlc, sizeof( download_thread_t ) );

    p_dt->psz_dest = strdup( psz_dest );
    p_dt->psz_src = strdup( p_uit->file.psz_url );
    asprintf( &p_dt->psz_status, "%s - %s (%s)\nSource: %s\nDestination: %s",
              p_uit->file.psz_description, p_uit->release.psz_version,
              p_uit->release.psz_svn_revision, p_uit->file.psz_url,
              psz_dest);

    vlc_thread_create( p_dt, "download thread", update_download_for_real,
                       VLC_THREAD_PRIORITY_LOW, VLC_FALSE );
}

/**
 * The true download function.
 *
 * \param p_this the download_thread_t object
 * \return nothing
 */
void update_download_for_real( download_thread_t *p_this )
{
    char *psz_dest = p_this->psz_dest;
    char *psz_src = p_this->psz_src;
    stream_t *p_stream;
    vlc_t *p_vlc = p_this->p_vlc;

    FILE *p_file = NULL;
    void *p_buffer;

    char *psz_status;

    int i_progress;
    int i_size;
    int i_done = 0;

    vlc_thread_ready( p_this );

    asprintf( &psz_status, "%s\nDownloading... %.1f%% done",
              p_this->psz_status, 0.0 );
    i_progress = intf_UserProgress( p_vlc, "Downloading...",
                                    psz_status, 0.0 );

    p_stream = stream_UrlNew( p_vlc, psz_src );
    if( !p_stream )
    {
        msg_Err( p_vlc, "Failed to open %s for reading", psz_src );
    }
    else
    {

        p_file = fopen( psz_dest, "w" );
        if( !p_file )
        {
            msg_Err( p_vlc, "Failed to open %s for writing", psz_dest );
        }
        else
        {
            int i_read;

            i_size = (int)(stream_Size(p_stream)/(1<<10));
            p_buffer = (void *)malloc( 1<<10 );

            while( ( i_read = stream_Read( p_stream, p_buffer, 1<<10 ) ) )
            {
                float f_progress;

                fwrite( p_buffer, i_read, 1, p_file );

                i_done++;
                free( psz_status );
                f_progress = 100.0*(float)i_done/(float)i_size;
                asprintf( &psz_status, "%s\nDownloading... %.1f%% done",
                           p_this->psz_status, f_progress );
                intf_UserProgressUpdate( p_vlc, i_progress,
                                         psz_status, f_progress );
            }

            free( p_buffer );
            fclose( p_file );
            stream_Delete( p_stream );

            free( psz_status );
            asprintf( &psz_status, "%s\nDone (100.00%%)",
                       p_this->psz_status );
            intf_UserProgressUpdate( p_vlc, i_progress, psz_status, 100.0 );
            free( psz_status );
        }
    }

    free( p_this->psz_dest );
    free( p_this->psz_src );
    free( p_this->psz_status );

#ifdef WIN32
    CloseHandle( p_this->thread_id );
#endif

    vlc_object_destroy( p_this );
}
