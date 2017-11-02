/*****************************************************************************
 * vorepository.c : Videolan.org's Addons Lister
 *****************************************************************************
 * Copyright (C) 2014 VLC authors and VideoLAN
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <fcntl.h>
#include <unistd.h>     /* write() */

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_stream.h>
#include <vlc_stream_extractor.h>
#include <vlc_addons.h>
#include <vlc_xml.h>
#include <vlc_fs.h>
#include <vlc_url.h>
#include "xmlreading.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static int   Open ( vlc_object_t * );
static void  Close ( vlc_object_t * );
static int   Retrieve ( addons_finder_t *p_finder, addon_entry_t *p_entry );
static int   OpenDesignated ( vlc_object_t * );
static int   FindDesignated ( addons_finder_t *p_finder );

#define ADDONS_MODULE_SHORTCUT "addons.vo"
#define ADDONS_REPO_SCHEMEHOST "https://api-addons.videolan.org"
/*****************************************************************************
 * Module descriptor
 ****************************************************************************/

vlc_module_begin ()
    set_category(CAT_ADVANCED)
    set_subcategory(SUBCAT_ADVANCED_MISC)
    set_shortname(N_("Videolan.org's addons finder"))
    add_shortcut(ADDONS_MODULE_SHORTCUT)
    set_description(N_("addons.videolan.org addons finder"))
    set_capability("addons finder", 100)
    set_callbacks(Open, Close)
add_submodule ()
    set_category(CAT_ADVANCED)
    set_subcategory(SUBCAT_ADVANCED_MISC)
    set_shortname(N_("Videolan.org's single archive addons finder"))
    add_shortcut(ADDONS_MODULE_SHORTCUT".vlp")
    set_description(N_("single .vlp archive addons finder"))
    set_capability("addons finder", 101)
    set_callbacks(OpenDesignated, NULL)
vlc_module_end ()

struct addons_finder_sys_t
{
    char *psz_tempfile;
};

static int ParseManifest( addons_finder_t *p_finder, addon_entry_t *p_entry,
                          const char *psz_tempfileuri, stream_t *p_stream )
{
    int i_num_entries_created = 0;
    const char *p_node;
    int i_current_node_type;

    /* attr */
    const char *attr, *value;

    /* temp reading */
    char *psz_filename = NULL;
    int i_filetype = -1;

    xml_reader_t *p_xml_reader = xml_ReaderCreate( p_finder, p_stream );
    if( !p_xml_reader ) return 0;

    if( xml_ReaderNextNode( p_xml_reader, &p_node ) != XML_READER_STARTELEM )
    {
        msg_Err( p_finder, "invalid xml file" );
        goto end;
    }

    if ( strcmp( p_node, "videolan") )
    {
        msg_Err( p_finder, "unsupported XML data format" );
        goto end;
    }

    while( (i_current_node_type = xml_ReaderNextNode( p_xml_reader, &p_node )) > 0 )
    {
        switch( i_current_node_type )
        {
        case XML_READER_STARTELEM:
        {
            BINDNODE("resource", psz_filename, TYPE_STRING)
            data_pointer.e_type = TYPE_NONE;

            /*
             * Manifests are not allowed to update addons properties
             * such as uuid, score, downloads, ...
             * On the other hand, repo API must not set files directly.
             */

            if ( ! strcmp( p_node, "resource" ) )
            {
                while( (attr = xml_ReaderNextAttr( p_xml_reader, &value )) )
                {
                    if ( !strcmp( attr, "type" ) )
                    {
                        i_filetype = ReadType( value );
                    }
                }
            }
            else if ( ! strcmp( p_node, "addon" ) )
            {
                while( (attr = xml_ReaderNextAttr( p_xml_reader, &value )) )
                {
                    if ( !strcmp( attr, "type" ) )
                    {
                        p_entry->e_type = ReadType( value );
                    }
                }
            }

            break;
        }
        case XML_READER_TEXT:
            if ( data_pointer.e_type == TYPE_NONE || !p_entry ) break;
            if ( data_pointer.e_type == TYPE_STRING )
            {
                if( data_pointer.u_data.ppsz )
                    free( *data_pointer.u_data.ppsz );
                *data_pointer.u_data.ppsz = strdup( p_node );
            }
            else
            if ( data_pointer.e_type == TYPE_LONG )
                *data_pointer.u_data.pl = atol( p_node );
            else
            if ( data_pointer.e_type == TYPE_INTEGER )
                *data_pointer.u_data.pi = atoi( p_node );
            break;

        case XML_READER_ENDELEM:

            if ( ! strcmp( p_node, "resource" ) )
            {
                if ( psz_filename && i_filetype >= 0 )
                {
                    addon_file_t *p_file = malloc( sizeof(addon_file_t) );
                    p_file->e_filetype = i_filetype;
                    p_file->psz_filename = strdup( psz_filename );
                    if ( asprintf( & p_file->psz_download_uri, "%s#!/%s",
                                   psz_tempfileuri, psz_filename  ) > 0 )
                    {
                        ARRAY_APPEND( p_entry->files, p_file );
                        msg_Dbg( p_finder, "manifest lists file %s extractable from %s",
                                 psz_filename, p_file->psz_download_uri );
                        i_num_entries_created++;
                    }
                    else
                    {
                        free( p_file->psz_filename );
                        free( p_file );
                    }
                }
                /* reset temp */
                free( psz_filename );
                psz_filename = NULL;
                i_filetype = -1;
            }

            data_pointer.e_type = TYPE_NONE;
            break;

        default:
            break;
        }
    }

end:
   xml_ReaderDelete( p_xml_reader );
   return i_num_entries_created;
}

static int ParseCategoriesInfo( addons_finder_t *p_finder, stream_t *p_stream )
{
    int i_num_entries_created = 0;

    const char *p_node;
    const char *attr, *value;
    int i_current_node_type;
    addon_entry_t *p_entry = NULL;

    xml_reader_t *p_xml_reader = xml_ReaderCreate( p_finder, p_stream );
    if( !p_xml_reader ) return 0;

    if( xml_ReaderNextNode( p_xml_reader, &p_node ) != XML_READER_STARTELEM )
    {
        msg_Err( p_finder, "invalid xml file" );
        goto end;
    }

    if ( strcmp( p_node, "videolan") )
    {
        msg_Err( p_finder, "unsupported XML data format" );
        goto end;
    }

    while( (i_current_node_type = xml_ReaderNextNode( p_xml_reader, &p_node )) > 0 )
    {
        switch( i_current_node_type )
        {
        case XML_READER_STARTELEM:
        {
            if ( ! strcmp( p_node, "addon" ) )
            {
                if ( p_entry ) /* Unclosed tag */
                    addon_entry_Release( p_entry );
                p_entry = addon_entry_New();
                p_entry->psz_source_module = strdup( ADDONS_MODULE_SHORTCUT );
                p_entry->e_flags = ADDON_MANAGEABLE;
                p_entry->e_state = ADDON_NOTINSTALLED;

                while( (attr = xml_ReaderNextAttr( p_xml_reader, &value )) )
                {
                    if ( !strcmp( attr, "type" ) )
                    {
                        p_entry->e_type = ReadType( value );
                    }
                    else if ( !strcmp( attr, "id" ) )
                    {
                        addons_uuid_read( value, & p_entry->uuid );
                    }
                    else if ( !strcmp( attr, "downloads" ) )
                    {
                        p_entry->i_downloads = atoi( value );
                        if ( p_entry->i_downloads < 0 )
                            p_entry->i_downloads = 0;
                    }
                    else if ( !strcmp( attr, "score" ) )
                    {
                        p_entry->i_score = atoi( value );
                        if ( p_entry->i_score < 0 )
                            p_entry->i_score = 0;
                        else if ( p_entry->i_score > ADDON_MAX_SCORE )
                            p_entry->i_score = ADDON_MAX_SCORE;
                    }
                    else if ( !strcmp( attr, "version" ) )
                    {
                        p_entry->psz_version = strdup( value );
                    }
                }

                break;
            }
            if ( !p_entry ) break;

            BINDNODE("name", p_entry->psz_name, TYPE_STRING)
            BINDNODE("archive", p_entry->psz_archive_uri, TYPE_STRING)
            BINDNODE("summary", p_entry->psz_summary, TYPE_STRING)
            BINDNODE("description", p_entry->psz_description, TYPE_STRING)
            BINDNODE("image", p_entry->psz_image_data, TYPE_STRING)
            BINDNODE("creator", p_entry->psz_author, TYPE_STRING)
            BINDNODE("sourceurl", p_entry->psz_source_uri, TYPE_STRING)
            data_pointer.e_type = TYPE_NONE;

            break;
        }
        case XML_READER_TEXT:
            if ( data_pointer.e_type == TYPE_NONE || !p_entry ) break;
            if ( data_pointer.e_type == TYPE_STRING )
            {
                if( data_pointer.u_data.ppsz )
                    free( *data_pointer.u_data.ppsz );
                *data_pointer.u_data.ppsz = strdup( p_node );
            }
            else
            if ( data_pointer.e_type == TYPE_LONG )
                *data_pointer.u_data.pl = atol( p_node );
            else
            if ( data_pointer.e_type == TYPE_INTEGER )
                *data_pointer.u_data.pi = atoi( p_node );
            break;

        case XML_READER_ENDELEM:
            if ( !p_entry ) break;
            if ( ! strcmp( p_node, "addon" ) )
            {
                /* then append entry */
                ARRAY_APPEND( p_finder->entries, p_entry );
                p_entry = NULL;
                i_num_entries_created++;
            }

            data_pointer.e_type = TYPE_NONE;
            break;

        default:
            break;
        }
    }

end:
   if ( p_entry ) /* Unclosed tag */
       addon_entry_Release( p_entry );
   xml_ReaderDelete( p_xml_reader );
   return i_num_entries_created;
}

static stream_t * vlc_stream_NewURL_ND( addons_finder_t *p_obj, const char *psz_uri )
{
    stream_t *p_stream = vlc_stream_NewURL( p_obj, psz_uri );
    if( p_stream )
    {
        /* (non applicable everywhere) remove extra
         * compression, bad wine madness :YYY */
        stream_t *p_chain = vlc_stream_FilterNew( p_stream, "inflate" );
        if( p_chain )
            p_stream = p_chain;
    }
    return p_stream;
}

static int Find( addons_finder_t *p_finder )
{
    stream_t *p_stream = vlc_stream_NewURL_ND( p_finder,
                                            ADDONS_REPO_SCHEMEHOST "/xml" );
    if ( !p_stream )
        return VLC_EGENERIC;

    int i_res = ParseCategoriesInfo( p_finder, p_stream );
    vlc_stream_Delete( p_stream );

    return i_res > 0 ? VLC_SUCCESS : VLC_EGENERIC;
}

static int Retrieve( addons_finder_t *p_finder, addon_entry_t *p_entry )
{
    vlc_mutex_lock( &p_entry->lock );
    if ( !p_entry->psz_archive_uri )
    {
        vlc_mutex_unlock( &p_entry->lock );
        return VLC_EGENERIC;
    }
    char *psz_archive_uri = strdup( p_entry->psz_archive_uri );
    vlc_mutex_unlock( &p_entry->lock );
    if ( !psz_archive_uri )
        return VLC_ENOMEM;

    /* get archive and parse manifest */
    stream_t *p_stream;

    if ( psz_archive_uri[0] == '/' )
    {
        /* Relative path */
        char *psz_uri;
        if ( ! asprintf( &psz_uri, ADDONS_REPO_SCHEMEHOST"%s", psz_archive_uri ) )
        {
            free( psz_archive_uri );
            return VLC_ENOMEM;
        }
        p_stream = vlc_stream_NewURL_ND( p_finder, psz_uri );
        free( psz_uri );
    }
    else
    {
        p_stream = vlc_stream_NewURL_ND( p_finder, psz_archive_uri );
    }

    msg_Dbg( p_finder, "downloading archive %s", psz_archive_uri );
    free ( psz_archive_uri );
    if ( !p_stream ) return VLC_EGENERIC;

    /* In case of pf_ reuse */
    if ( p_finder->p_sys->psz_tempfile )
    {
        vlc_unlink( p_finder->p_sys->psz_tempfile );
        FREENULL( p_finder->p_sys->psz_tempfile );
    }

    p_finder->p_sys->psz_tempfile = tempnam( NULL, "vlp" );
    if ( !p_finder->p_sys->psz_tempfile )
    {
        msg_Err( p_finder, "Can't create temp storage file" );
        vlc_stream_Delete( p_stream );
        return VLC_EGENERIC;
    }

    int fd = vlc_open( p_finder->p_sys->psz_tempfile,
                       O_WRONLY | O_CREAT | O_EXCL, 0600 );
    if( fd == -1 )
    {
        msg_Err( p_finder, "Failed to open addon temp storage file" );
        FREENULL(p_finder->p_sys->psz_tempfile);
        vlc_stream_Delete( p_stream );
        return VLC_EGENERIC;
    }

    char buffer[1<<10];
    ssize_t i_read = 0;
    int i_ret = VLC_SUCCESS;

    while ( ( i_read = vlc_stream_Read( p_stream, &buffer, 1<<10 ) ) > 0 )
    {
        if ( write( fd, buffer, i_read ) != i_read )
        {
            msg_Err( p_finder, "Failed to write to Addon file" );
            i_ret = VLC_EGENERIC;
            break;
        }
    }

    vlc_close( fd );
    vlc_stream_Delete( p_stream );

    if (i_ret)
        return i_ret;

    msg_Dbg( p_finder, "Reading manifest from %s", p_finder->p_sys->psz_tempfile );

    char *psz_tempfileuri = vlc_path2uri( p_finder->p_sys->psz_tempfile, NULL );
    if ( !psz_tempfileuri )
        return VLC_ENOMEM;

    char *psz_manifest_uri;
    if ( asprintf( &psz_manifest_uri, "%s#!/manifest.xml", psz_tempfileuri ) < 1 )
    {
        free( psz_tempfileuri );
        return VLC_ENOMEM;
    }

    p_stream = vlc_stream_NewMRL( p_finder, psz_manifest_uri );
    free( psz_manifest_uri );
    if ( !p_stream )
    {
        free( psz_tempfileuri );
        return VLC_EGENERIC;
    }

    vlc_mutex_lock( &p_entry->lock );
    i_ret = ( ParseManifest( p_finder, p_entry, psz_tempfileuri, p_stream ) > 0 )
                    ? VLC_SUCCESS : VLC_EGENERIC;
    vlc_mutex_unlock( &p_entry->lock );
    free( psz_tempfileuri );
    vlc_stream_Delete( p_stream );

    return i_ret;
}

static int FindDesignated( addons_finder_t *p_finder )
{
    char *psz_manifest;
    const char *psz_path = p_finder->psz_uri + 7; // remove scheme

    if ( asprintf( &psz_manifest, "file://%s#!/manifest.xml",
                   psz_path ) < 1 )
        return VLC_ENOMEM;

    stream_t *p_stream = vlc_stream_NewMRL( p_finder, psz_manifest );
    free( psz_manifest );
    if ( !p_stream ) return VLC_EGENERIC;

    if ( ParseCategoriesInfo( p_finder, p_stream ) )
    {
        /* Do archive uri fixup */
        FOREACH_ARRAY( addon_entry_t *p_entry, p_finder->entries )
        if ( likely( !p_entry->psz_archive_uri ) )
                p_entry->psz_archive_uri = strdup( p_finder->psz_uri );
        FOREACH_END()
    }
    else
    {
        vlc_stream_Delete( p_stream );
        return VLC_EGENERIC;
    }

    vlc_stream_Delete( p_stream );

    return VLC_SUCCESS;
}

static int Open(vlc_object_t *p_this)
{
    addons_finder_t *p_finder = (addons_finder_t*) p_this;

    p_finder->p_sys = (addons_finder_sys_t*) malloc(sizeof(addons_finder_sys_t));
    if ( !p_finder->p_sys )
        return VLC_ENOMEM;
    p_finder->p_sys->psz_tempfile = NULL;

    if ( p_finder->psz_uri &&
         strcmp( "repo://"ADDONS_MODULE_SHORTCUT, p_finder->psz_uri ) &&
         memcmp( "repo://", p_finder->psz_uri, 8 ) )
    {
        free( p_finder->p_sys );
        return VLC_EGENERIC;
    }

    p_finder->pf_find = Find;
    p_finder->pf_retrieve = Retrieve;

    return VLC_SUCCESS;
}

static void Close(vlc_object_t *p_this)
{
    addons_finder_t *p_finder = (addons_finder_t*) p_this;
    if ( p_finder->p_sys->psz_tempfile )
    {
        vlc_unlink( p_finder->p_sys->psz_tempfile );
        free( p_finder->p_sys->psz_tempfile );
    }
    free( p_finder->p_sys );
}

static int OpenDesignated(vlc_object_t *p_this)
{
    addons_finder_t *p_finder = (addons_finder_t*) p_this;
    if ( !p_finder->psz_uri
         || strncmp( "file://", p_finder->psz_uri, 7 )
         || strncmp( ".vlp", p_finder->psz_uri + strlen( p_finder->psz_uri ) - 4, 4 )
       )
        return VLC_EGENERIC;

    p_finder->pf_find = FindDesignated;
    p_finder->pf_retrieve = Retrieve;

    return VLC_SUCCESS;
}
