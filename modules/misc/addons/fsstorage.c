/*****************************************************************************
 * addonsstorage.c : Addons Local filesystem storage
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_modules.h>
#include <vlc_stream.h>
#include <vlc_stream_extractor.h>
#include <vlc_addons.h>
#include <vlc_fs.h>
#include <vlc_strings.h>
#include <vlc_xml.h>
#include <vlc_url.h>
#include "xmlreading.h"

#include <sys/stat.h>
#include <errno.h>

#include <unistd.h>     // getpid()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

#define ADDONS_DIR          ""
#define ADDONS_SCRIPTS_DIR  ADDONS_DIR DIR_SEP "lua"
#define ADDONS_CATALOG      ADDONS_DIR DIR_SEP "catalog.xml"

static struct
{
    addon_type_t t;
    const char * const psz_dir;
} const addons_dirs[] = {
    { ADDON_EXTENSION,           ADDONS_SCRIPTS_DIR DIR_SEP "extensions" },
    { ADDON_PLAYLIST_PARSER,     ADDONS_SCRIPTS_DIR DIR_SEP "playlist" },
    { ADDON_SERVICE_DISCOVERY,   ADDONS_SCRIPTS_DIR DIR_SEP "sd" },
    { ADDON_INTERFACE,           ADDONS_SCRIPTS_DIR DIR_SEP "intf" },
    { ADDON_META,                ADDONS_SCRIPTS_DIR DIR_SEP "meta" },
    { ADDON_SKIN2,               ADDONS_DIR DIR_SEP "skins2" },
};

static int   OpenStorage ( vlc_object_t * );
static void  CloseStorage ( vlc_object_t * );
static int   OpenLister ( vlc_object_t * );
static void  CloseLister ( vlc_object_t * );

static int   LoadCatalog ( addons_finder_t * );
static bool  FileBelongsToManagedAddon( addons_finder_t *p_finder,
                                        const addon_type_t e_type,
                                        const char *psz_file );
/*****************************************************************************
 * Module descriptor
 ****************************************************************************/

vlc_module_begin ()
    set_category(CAT_ADVANCED)
    set_subcategory(SUBCAT_ADVANCED_MISC)
    set_shortname(N_("addons local storage"))
    add_shortcut("addons.store.install")
    set_description(N_("Addons local storage installer"))
    set_capability("addons storage", 10)
    set_callbacks(OpenStorage, CloseStorage)

add_submodule ()
    set_category(CAT_ADVANCED)
    set_subcategory(SUBCAT_ADVANCED_MISC)
    add_shortcut("addons.store.list")
    set_description( N_("Addons local storage lister") )
    set_capability( "addons finder", 0 )
    set_callbacks( OpenLister, CloseLister )

vlc_module_end ()

static char * getAddonInstallDir( addon_type_t t )
{
    const char *psz_subdir = NULL;
    char *psz_dir;
    char *psz_userdir = config_GetUserDir( VLC_USERDATA_DIR );
    if ( !psz_userdir ) return NULL;

    for ( unsigned int i=0; i< ARRAY_SIZE(addons_dirs); i++ )
    {
        if ( addons_dirs[i].t == t )
        {
            psz_subdir = addons_dirs[i].psz_dir;
            break;
        }
    }

    if ( !psz_subdir )
    {
        free ( psz_userdir );
        return NULL;
    }

    if ( asprintf( &psz_dir, "%s%s", psz_userdir, psz_subdir ) < 1 )
    {
        free( psz_userdir );
        return NULL;
    }
    free( psz_userdir );

    return psz_dir;
}

static int ListSkin_filter( const char * psz_filename )
{
    int i_len = strlen( psz_filename );
    if ( i_len  <= 4 )
        return 0;
    else
        return ! strcmp( psz_filename + i_len - 4, ".vlt" );
}

static int ListScript_filter( const char * psz_filename )
{
    int i_len = strlen( psz_filename );
    if ( i_len  <= 4 )
        return 0;
    else
        return ! strcmp( psz_filename + i_len - 4, ".lua" );
}

static int ParseSkins2Info( addons_finder_t *p_finder, stream_t *p_stream,
                            char **ppsz_title, char **ppsz_source )
{
    const char *p_node;
    int i_current_node_type;
    bool b_done = false;

    xml_reader_t *p_xml_reader = xml_ReaderCreate( p_finder, p_stream );
    if( !p_xml_reader ) return VLC_EGENERIC;

    if( xml_ReaderNextNode( p_xml_reader, &p_node ) != XML_READER_STARTELEM )
    {
        msg_Err( p_finder, "invalid xml file" );
        goto error;
    }

    if ( strcmp( p_node, "Theme") )
    {
        msg_Err( p_finder, "unsupported XML data format" );
        goto error;
    }

    while( !b_done && (i_current_node_type = xml_ReaderNextNode( p_xml_reader, &p_node )) > 0 )
    {
        switch( i_current_node_type )
        {
        case XML_READER_STARTELEM:
        {
            if ( !strcmp( p_node, "ThemeInfo" ) )
            {
                const char *attr, *value;
                while( (attr = xml_ReaderNextAttr( p_xml_reader, &value )) )
                {
                    if ( !strcmp( attr, "name" ) )
                        *ppsz_title = strdup( value );
                    else if ( !strcmp( attr, "webpage" ) )
                        *ppsz_source = strdup( value );
                }
                b_done = true;
            }
            break;
        }

        default:
            break;
        }
    }

    xml_ReaderDelete( p_xml_reader );
    return ( b_done ) ? VLC_SUCCESS : VLC_EGENERIC;

error:
    xml_ReaderDelete( p_xml_reader );
    return VLC_EGENERIC;
}

static int ListSkins( addons_finder_t *p_finder )
{
    char *psz_dir = getAddonInstallDir( ADDON_SKIN2 );
    if ( !psz_dir )
        return VLC_EGENERIC;

    char **ppsz_list = NULL;
    int i_count = vlc_scandir( psz_dir, &ppsz_list, ListSkin_filter, NULL );

    for ( int i=0; i< i_count; i++ )
    {
        char *psz_file = ppsz_list[i];
        if( !psz_file )
            break;

        if ( FileBelongsToManagedAddon( p_finder, ADDON_SKIN2, psz_file ) )
        {
            free( psz_file );
             continue;
        }

        char *psz_uri;
        if( asprintf( &psz_uri, "file://%s/%s#!/theme.xml", psz_dir, psz_file ) >= 0)
        {
            int i_ret;
            char *psz_name = NULL;
            char *psz_source = NULL;
            stream_t *p_stream = vlc_stream_NewMRL( p_finder, psz_uri );
            free( psz_uri );
            if ( !p_stream )
            {
                i_ret = VLC_EGENERIC;
            }
            else
            {
                i_ret = ParseSkins2Info( p_finder, p_stream, &psz_name, &psz_source );
                if ( i_ret != VLC_SUCCESS )
                {
                    free( psz_name );
                    free( psz_source );
                }
                vlc_stream_Delete( p_stream );
            }

            addon_entry_t *p_entry = addon_entry_New();
            p_entry->e_type = ADDON_SKIN2;
            p_entry->e_state = ADDON_INSTALLED;
            if ( i_ret == VLC_SUCCESS )
            {
                p_entry->psz_name = psz_name;
                p_entry->psz_description = strdup("Skins2 theme");
                p_entry->psz_source_uri = psz_source;
            }
            else
            {
                p_entry->e_flags |= ADDON_BROKEN;
                p_entry->psz_name = strdup(psz_file);
                p_entry->psz_description = strdup("Skins2 theme");
            }

            ARRAY_APPEND( p_finder->entries, p_entry );
        }
        free( psz_file );
    }

    free( ppsz_list );
    free( psz_dir );
    return VLC_SUCCESS;
}

static bool FileBelongsToManagedAddon( addons_finder_t *p_finder,
                                       const addon_type_t e_type,
                                       const char *psz_file )
{
    const addon_entry_t *p_entry;
    ARRAY_FOREACH( p_entry, p_finder->entries )
    {
        if ( ( p_entry->e_flags & ADDON_MANAGEABLE ) == 0 )
            continue;
        const addon_file_t *p_file;
        ARRAY_FOREACH( p_file, p_entry->files )
        {
            if ( p_file->e_filetype == e_type
                 && !strcmp( p_file->psz_filename, psz_file ) )
                return true;
        }
    }
    return false;
}

static int ListScripts( addons_finder_t *p_finder, addon_type_t type )
{
    char *psz_dir = getAddonInstallDir( type );
    if ( ! psz_dir ) return VLC_EGENERIC;

    char **ppsz_list = NULL;
    int i_count = vlc_scandir( psz_dir, &ppsz_list, ListScript_filter, NULL );

    for ( int i=0; i< i_count; i++ )
    {
        char *psz_file = ppsz_list[i];
        if( !psz_file )
            break;
        if ( FileBelongsToManagedAddon( p_finder, type, psz_file ) )
             continue;
        addon_entry_t *p_entry = addon_entry_New();
        p_entry->e_state = ADDON_INSTALLED;
        p_entry->e_type = type;
        p_entry->e_flags |= ADDON_BROKEN;
        p_entry->psz_name = strdup(psz_file);
        p_entry->psz_description = strdup("Lua script");

        ARRAY_APPEND( p_finder->entries, p_entry );
        free( psz_file );
    }

    free( ppsz_list );
    free( psz_dir );

    return VLC_SUCCESS;
}

static int List( addons_finder_t *p_finder )
{
    addon_type_t types[] = {
        ADDON_EXTENSION,
        ADDON_PLAYLIST_PARSER,
        ADDON_SERVICE_DISCOVERY,
        ADDON_INTERFACE,
        ADDON_META,
    };
    unsigned int i_type = 0;

    LoadCatalog( p_finder );

    /* Browse dirs to find rogue files */
    while( i_type < ARRAY_SIZE( types ) )
    {
        ListScripts( p_finder, types[i_type++] );
    }
    ListSkins( p_finder );

    return VLC_SUCCESS;
}

static int recursive_mkdir( vlc_object_t *p_this, const char *psz_dirname )
{/* stolen from config_CreateDir() */
    if( !psz_dirname || !*psz_dirname ) return -1;

    if( vlc_mkdir( psz_dirname, 0700 ) == 0 )
        return 0;

    switch( errno )
    {
        case EEXIST:
            return 0;

        case ENOENT:
        {
            /* Let's try to create the parent directory */
            char psz_parent[strlen( psz_dirname ) + 1], *psz_end;
            strcpy( psz_parent, psz_dirname );

            psz_end = strrchr( psz_parent, DIR_SEP_CHAR );
            if( psz_end && psz_end != psz_parent )
            {
                *psz_end = '\0';
                if( recursive_mkdir( p_this, psz_parent ) == 0 )
                {
                    if( !vlc_mkdir( psz_dirname, 0700 ) )
                        return 0;
                }
            }
        }
    }

    msg_Warn( p_this, "could not create %s: %m", psz_dirname );
    return -1;
}

static int InstallFile( addons_storage_t *p_this, const char *psz_downloadlink,
                        const char *psz_dest )
{
    stream_t *p_stream;
    FILE *p_destfile;
    char buffer[1<<10];
    int i_read = 0;

    p_stream = vlc_stream_NewMRL( p_this, psz_downloadlink );
    if( !p_stream )
    {
        msg_Err( p_this, "Failed to access Addon download url %s", psz_downloadlink );
        return VLC_EGENERIC;
    }

    char *psz_path = strdup( psz_dest );
    if ( !psz_path )
    {
        vlc_stream_Delete( p_stream );
        return VLC_ENOMEM;
    }
    char *psz_buf = strrchr( psz_path, DIR_SEP_CHAR );
    if( psz_buf )
    {
        *++psz_buf = '\0';
        /* ensure directory exists */
        if( !EMPTY_STR( psz_path ) ) recursive_mkdir( VLC_OBJECT(p_this), psz_path );
    }
    free( psz_path );

    p_destfile = vlc_fopen( psz_dest, "w" );
    if( !p_destfile )
    {
        msg_Err( p_this, "Failed to open Addon storage file %s", psz_dest );
        vlc_stream_Delete( p_stream );
        return VLC_EGENERIC;
    }

    while ( ( i_read = vlc_stream_Read( p_stream, &buffer, 1<<10 ) ) > 0 )
    {
        if ( fwrite( &buffer, i_read, 1, p_destfile ) < 1 )
        {
            msg_Err( p_this, "Failed to write to Addon file" );
            break;
        }
    }

    fclose( p_destfile );
    if ( i_read < 0 )
        vlc_unlink( psz_dest );
    vlc_stream_Delete( p_stream );
    return i_read >= 0 ? VLC_SUCCESS : VLC_EGENERIC;
}

static int InstallAllFiles( addons_storage_t *p_this, const addon_entry_t *p_entry )
{
    const addon_file_t *p_file;
    char *psz_dest;

    if ( p_entry->files.i_size < 1 )
        return VLC_EGENERIC;

    ARRAY_FOREACH( p_file, p_entry->files )
    {
        switch( p_file->e_filetype )
        {
            case ADDON_EXTENSION:
            case ADDON_PLAYLIST_PARSER:
            case ADDON_SERVICE_DISCOVERY:
            case ADDON_INTERFACE:
            case ADDON_META:
            case ADDON_SKIN2:
            {
                if ( strstr( p_file->psz_filename, ".." ) )
                    return VLC_EGENERIC;

                char *psz_translated_filename = strdup( p_file->psz_filename );
                if ( !psz_translated_filename )
                    return VLC_ENOMEM;
                char *tmp = psz_translated_filename;
                while (*tmp++) if ( *tmp == '/' ) *tmp = DIR_SEP_CHAR;

                char *psz_dir = getAddonInstallDir( p_file->e_filetype );
                if ( !psz_dir || asprintf( &psz_dest, "%s"DIR_SEP"%s", psz_dir,
                               psz_translated_filename ) < 1 )
                {
                    free( psz_dir );
                    free( psz_translated_filename );
                    return VLC_EGENERIC;
                }
                free( psz_translated_filename );
                free( psz_dir );

                if ( InstallFile( p_this, p_file->psz_download_uri, psz_dest ) != VLC_SUCCESS )
                {
                    free( psz_dest );
                    return VLC_EGENERIC;
                }

                free( psz_dest );
                break;
            }
            /* Ignore all other unhandled files */
            case ADDON_UNKNOWN:
            case ADDON_PLUGIN:
            case ADDON_OTHER:
            default:
                break;
        }
    }

    return VLC_SUCCESS;
}

static int Install( addons_storage_t *p_storage, addon_entry_t *p_entry )
{
    vlc_object_t *p_this = VLC_OBJECT( p_storage );
    int i_ret = VLC_EGENERIC;

    if ( ! p_entry->psz_source_module )
        return i_ret;

    /* Query origin module for download path */
    addons_finder_t *p_finder = vlc_object_create( p_this, sizeof( addons_finder_t ) );
    if( !p_finder )
        return VLC_ENOMEM;

    module_t *p_module = module_need( p_finder, "addons finder",
                                      p_entry->psz_source_module, true );
    if( p_module )
    {
        if ( p_finder->pf_retrieve( p_finder, p_entry ) == VLC_SUCCESS )
        {
            /* Do things while retrieved data is here */
            vlc_mutex_lock( &p_entry->lock );
            i_ret = InstallAllFiles( p_storage, p_entry );
            vlc_mutex_unlock( &p_entry->lock );
            /* !Do things while retrieved data is here */
        }

        module_unneed( p_finder, p_module );
    }

    vlc_object_delete(p_finder);

    return i_ret;
}

#define WRITE_WITH_ENTITIES( formatstring, varname ) \
if ( varname ) \
{\
    psz_tempstring = vlc_xml_encode( varname );\
    fprintf( p_catalog, formatstring, psz_tempstring );\
    free( psz_tempstring );\
}\

static int WriteCatalog( addons_storage_t *p_storage,
                         addon_entry_t **pp_entries, int i_entries )
{
    addon_entry_t *p_entry;
    char *psz_file;
    char *psz_file_tmp;
    char *psz_tempstring;
    char *psz_userdir = config_GetUserDir( VLC_USERDATA_DIR );
    if ( !psz_userdir ) return VLC_ENOMEM;

    if ( asprintf( &psz_file, "%s%s", psz_userdir, ADDONS_CATALOG ) < 1 )
    {
        free( psz_userdir );
        return VLC_ENOMEM;
    }
    free( psz_userdir );

    if ( asprintf( &psz_file_tmp, "%s.tmp%"PRIu32, psz_file, (uint32_t)getpid() ) < 1 )
    {
        free( psz_file );
        return VLC_ENOMEM;
    }

    char *psz_path = strdup( psz_file );
    if ( !psz_path )
    {
        free( psz_file );
        free( psz_file_tmp );
        return VLC_ENOMEM;
    }

    char *psz_buf = strrchr( psz_path, DIR_SEP_CHAR );
    if( psz_buf )
    {
        *++psz_buf = '\0';
        /* ensure directory exists */
        if( !EMPTY_STR( psz_path ) ) recursive_mkdir( VLC_OBJECT(p_storage), psz_path );
    }
    free( psz_path );

    FILE *p_catalog = vlc_fopen( psz_file_tmp, "wt" );
    if ( !p_catalog )
    {
        free( psz_file );
        free( psz_file_tmp );
        return VLC_EGENERIC;
    }

    /* write XML header */
    fprintf( p_catalog, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" );
    fprintf( p_catalog, "<videolan xmlns=\"http://videolan.org/ns/vlc/addons/1.0\">\n" );
    fprintf( p_catalog, "\t<addons>\n" );

    for ( int i=0; i<i_entries; i++ )
    {
        p_entry = pp_entries[i];
        vlc_mutex_lock( &p_entry->lock );
        psz_tempstring = NULL;

        if ( ( p_entry->e_state != ADDON_INSTALLED ) ||
             !( p_entry->e_flags & ADDON_MANAGEABLE ) )
        {
            vlc_mutex_unlock( &p_entry->lock );
            continue;
        }

        if ( p_entry->psz_source_module )
            psz_tempstring = vlc_xml_encode( p_entry->psz_source_module );

        char *psz_uuid = addons_uuid_to_psz( ( const addon_uuid_t * ) & p_entry->uuid );
        fprintf( p_catalog, "\t\t<addon source=\"%s\" type=\"%s\" id=\"%s\" "
                                 "downloads=\"%ld\" score=\"%d\"",
                 ( psz_tempstring ) ? psz_tempstring : "",
                 getTypePsz( p_entry->e_type ),
                 psz_uuid,
                 p_entry->i_downloads,
                 p_entry->i_score );
        free( psz_uuid );
        free( psz_tempstring );

        WRITE_WITH_ENTITIES( " version=\"%s\"", p_entry->psz_version )
        fprintf( p_catalog, ">\n" );

        WRITE_WITH_ENTITIES( "\t\t\t<name>%s</name>\n", p_entry->psz_name )
        WRITE_WITH_ENTITIES( "\t\t\t<summary>%s</summary>\n", p_entry->psz_summary )

        if ( p_entry->psz_description )
        {
            psz_tempstring = p_entry->psz_description;
            /* FIXME: do real escaping */
            while( ( psz_tempstring = strstr( psz_tempstring, "]]>" ) ) )
                *psz_tempstring = ' ';
            fprintf( p_catalog, "\t\t\t<description><![CDATA[%s]]></description>\n", p_entry->psz_description );
        }

        WRITE_WITH_ENTITIES( "\t\t\t<image>%s</image>\n", p_entry->psz_image_data )
        WRITE_WITH_ENTITIES( "\t\t\t<archive>%s</archive>\n", p_entry->psz_archive_uri )

        fprintf( p_catalog, "\t\t\t<authorship>\n" );
        WRITE_WITH_ENTITIES( "\t\t\t\t<creator>%s</creator>\n", p_entry->psz_author )
        WRITE_WITH_ENTITIES( "\t\t\t\t<sourceurl>%s</sourceurl>\n", p_entry->psz_source_uri )
        fprintf( p_catalog, "\t\t\t</authorship>\n" );

        addon_file_t *p_file;
        ARRAY_FOREACH( p_file, p_entry->files )
        {
            psz_tempstring = vlc_xml_encode( p_file->psz_filename );
            fprintf( p_catalog, "\t\t\t<resource type=\"%s\">%s</resource>\n",
                     getTypePsz( p_file->e_filetype ), psz_tempstring );
            free( psz_tempstring );
        }

        fprintf( p_catalog, "\t\t</addon>\n" );

        vlc_mutex_unlock( &p_entry->lock );
    }

    fprintf( p_catalog, "\t</addons>\n" );
    fprintf( p_catalog, "</videolan>\n" );
    fclose( p_catalog );

    int i_ret = vlc_rename( psz_file_tmp, psz_file );
    free( psz_file );
    free( psz_file_tmp );

    if( i_ret == -1 )
    {
        msg_Err( p_storage, "could not rename temp catalog: %s",
                 vlc_strerror_c(errno) );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static int LoadCatalog( addons_finder_t *p_finder )
{
    char *psz_path;
    char * psz_userdir = config_GetUserDir( VLC_USERDATA_DIR );
    if ( !psz_userdir ) return VLC_ENOMEM;

    if ( asprintf( &psz_path, "%s%s", psz_userdir, ADDONS_CATALOG ) < 1 )
    {
        free( psz_userdir );
        return VLC_ENOMEM;
    }
    free( psz_userdir );

    addon_entry_t *p_entry = NULL;
    const char *p_node;
    int i_current_node_type;
    int i_ret = VLC_SUCCESS;

    /* attr */
    const char *attr, *value;

    /* temp reading */
    char *psz_filename = NULL;
    int i_filetype = -1;

    struct stat stat_;
    if ( vlc_stat( psz_path, &stat_ ) )
    {
        free( psz_path );
        return VLC_EGENERIC;
    }

    char *psz_catalog_uri = vlc_path2uri( psz_path, "file" );
    free( psz_path );
    if ( !psz_catalog_uri )
        return VLC_EGENERIC;

    stream_t *p_stream = vlc_stream_NewURL( p_finder, psz_catalog_uri );
    free( psz_catalog_uri );
    if (! p_stream ) return VLC_EGENERIC;

    xml_reader_t *p_xml_reader = xml_ReaderCreate( p_finder, p_stream );
    if( !p_xml_reader )
    {
        vlc_stream_Delete( p_stream );
        return VLC_EGENERIC;
    }

    if( xml_ReaderNextNode( p_xml_reader, &p_node ) != XML_READER_STARTELEM )
    {
        msg_Err( p_finder, "invalid catalog" );
        i_ret = VLC_EGENERIC;
        goto end;
    }

    if ( strcmp( p_node, "videolan") )
    {
        msg_Err( p_finder, "unsupported catalog data format" );
        i_ret = VLC_EGENERIC;
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
                if ( p_entry ) /* ?!? Unclosed tag */
                    addon_entry_Release( p_entry );

                p_entry = addon_entry_New();
                //p_entry->psz_source_module = strdup( ADDONS_MODULE_SHORTCUT );
                p_entry->e_flags = ADDON_MANAGEABLE;
                p_entry->e_state = ADDON_INSTALLED;

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
                    else if ( !strcmp( attr, "source" ) )
                    {
                        p_entry->psz_source_module = strdup( value );
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
            BINDNODE("resource", psz_filename, TYPE_STRING)
            BINDNODE("creator", p_entry->psz_author, TYPE_STRING)
            BINDNODE("sourceurl", p_entry->psz_source_uri, TYPE_STRING)
            data_pointer.e_type = TYPE_NONE;

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

            break;
        }
        case XML_READER_TEXT:
            if ( data_pointer.e_type == TYPE_NONE || !p_entry ) break;
            if ( data_pointer.e_type == TYPE_STRING )
                *data_pointer.u_data.ppsz = strdup( p_node );
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
            }

            if ( ! strcmp( p_node, "resource" ) )
            {
                if ( p_entry && psz_filename && i_filetype >= 0 )
                {
                    addon_file_t *p_file = malloc( sizeof(addon_file_t) );
                    p_file->e_filetype = i_filetype;
                    p_file->psz_filename = psz_filename;
                    p_file->psz_download_uri = NULL;
                    ARRAY_APPEND( p_entry->files, p_file );
                }
                /* reset temp */
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
   if ( p_entry ) /* ?!? Unclosed tag */
       addon_entry_Release( p_entry );
   xml_ReaderDelete( p_xml_reader );
   vlc_stream_Delete( p_stream );
   return i_ret;
}

static int Remove( addons_storage_t *p_storage, addon_entry_t *p_entry )
{
    vlc_mutex_lock( &p_entry->lock );
    addon_file_t *p_file;
    ARRAY_FOREACH( p_file, p_entry->files )
    {
        switch( p_file->e_filetype )
        {
            case ADDON_EXTENSION:
            case ADDON_PLAYLIST_PARSER:
            case ADDON_SERVICE_DISCOVERY:
            case ADDON_INTERFACE:
            case ADDON_META:
            case ADDON_SKIN2:
            {
                char *psz_dest;

                char *psz_translated_filename = strdup( p_file->psz_filename );
                if ( !psz_translated_filename )
                    return VLC_ENOMEM;
                char *tmp = psz_translated_filename;
                while (*tmp++) if ( *tmp == '/' ) *tmp = DIR_SEP_CHAR;

                char *psz_dir = getAddonInstallDir( p_file->e_filetype );
                if ( !psz_dir || asprintf( &psz_dest, "%s"DIR_SEP"%s", psz_dir,
                                           psz_translated_filename ) < 1 )
                {
                    free( psz_dir );
                    free( psz_translated_filename );
                    return VLC_EGENERIC;
                }
                free( psz_dir );
                free( psz_translated_filename );

                vlc_unlink( psz_dest );
                msg_Dbg( p_storage, "removing %s", psz_dest );

                free( psz_dest );
                break;
            }
                /* Ignore all other unhandled files */
            case ADDON_UNKNOWN:
            case ADDON_PLUGIN:
            case ADDON_OTHER:
            default:
                break;
        }
    }

    /* Remove file info on success */
    ARRAY_FOREACH( p_file, p_entry->files )
    {
        free( p_file->psz_filename );
        free( p_file->psz_download_uri );
        free( p_file );
    }
    ARRAY_RESET( p_entry->files );

    vlc_mutex_unlock( &p_entry->lock );
    return VLC_SUCCESS;
}

static int OpenStorage(vlc_object_t *p_this)
{
    addons_storage_t *p_storage = (addons_storage_t*) p_this;

    p_storage->pf_install = Install;
    p_storage->pf_remove = Remove;
    p_storage->pf_catalog = WriteCatalog;

    return VLC_SUCCESS;
}

static void CloseStorage(vlc_object_t *p_this)
{
    VLC_UNUSED( p_this );
}

static int OpenLister(vlc_object_t *p_this)
{
    addons_finder_t *p_finder = (addons_finder_t*) p_this;
    p_finder->pf_find = List;
    p_finder->pf_retrieve = NULL;

    return VLC_SUCCESS;
}

static void CloseLister(vlc_object_t *p_this)
{
    VLC_UNUSED( p_this );
}
