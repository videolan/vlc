/*****************************************************************************
 * sftp.c: SFTP input module
 *****************************************************************************
 * Copyright (C) 2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Rémi Duraffort <ivoire@videolan.org>
 *          Petri Hintukainen <phintuka@gmail.com>
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

#include <assert.h>

#include <vlc_access.h>
#include <vlc_dialog.h>
#include <vlc_input_item.h>
#include <vlc_network.h>
#include <vlc_url.h>

#include <libssh2.h>
#include <libssh2_sftp.h>


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t* );
static void Close( vlc_object_t* );

#define PORT_TEXT N_("SFTP port")
#define PORT_LONGTEXT N_("SFTP port number to use on the server")
#define USER_TEXT N_("Username")
#define USER_LONGTEXT N_("Username that will be used for the connection, " \
        "if no username is set in the URL.")
#define PASS_TEXT N_("Password")
#define PASS_LONGTEXT N_("Password that will be used for the connection, " \
        "if no username or password are set in URL.")

vlc_module_begin ()
    set_shortname( "SFTP" )
    set_description( N_("SFTP input") )
    set_capability( "access", 0 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )
    add_integer( "sftp-port", 22, PORT_TEXT, PORT_LONGTEXT, true )
    add_string( "sftp-user", NULL, USER_TEXT, USER_LONGTEXT, false )
    add_password( "sftp-pwd", NULL, PASS_TEXT, PASS_LONGTEXT, false )
    add_shortcut( "sftp" )
    set_callbacks( Open, Close )
vlc_module_end ()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static ssize_t  Read( access_t *, uint8_t *, size_t );
static int      Seek( access_t *, uint64_t );
static int      Control( access_t *, int, va_list );

static input_item_t* DirRead( access_t *p_access );
static int DirControl( access_t *, int, va_list );

struct access_sys_t
{
    int i_socket;
    LIBSSH2_SESSION* ssh_session;
    LIBSSH2_SFTP* sftp_session;
    LIBSSH2_SFTP_HANDLE* file;
    uint64_t filesize;

    /* browser */
    char* psz_username_opt;
    char* psz_password_opt;
};



/**
 * Connect to the sftp server and ask for a file
 * @param p_this: the vlc_object
 * @return VLC_SUCCESS if everything was fine
 */
static int Open( vlc_object_t* p_this )
{
    access_t*   p_access = (access_t*)p_this;
    access_sys_t* p_sys;
    const char* psz_path;
    char* psz_username = NULL;
    char* psz_password = NULL;
    char* psz_remote_home = NULL;
    int i_port;
    int i_ret;
    vlc_url_t url;
    size_t i_len;
    int i_type;

    if( !p_access->psz_location )
        return VLC_EGENERIC;

    access_InitFields( p_access );
    p_sys = p_access->p_sys = (access_sys_t*)calloc( 1, sizeof( access_sys_t ) );
    if( !p_sys ) return VLC_ENOMEM;

    p_sys->i_socket = -1;

    /* Parse the URL */
    vlc_UrlParse( &url, p_access->psz_location );

    /* Check for some parameters */
    if( EMPTY_STR( url.psz_host ) )
    {
        msg_Err( p_access, "You might give a non empty host" );
        goto error;
    }

    /* get user/password from url or options */
    if( !EMPTY_STR( url.psz_username ) )
        psz_username = strdup( url.psz_username );
    else
        psz_username = var_InheritString( p_access, "sftp-user" );

    if( url.psz_password )
        psz_password = strdup( url.psz_password );
    else
        psz_password = var_InheritString( p_access, "sftp-pwd" );

    /* If the user name or password is empty, ask the user */
    if( EMPTY_STR( psz_username ) || !psz_password )
    {
        dialog_Login( p_access, &psz_username, &psz_password,
                      _("SFTP authentication"),
                      _("Please enter a valid login and password for the sftp "
                        "connexion to %s"), url.psz_host );
        if( EMPTY_STR(psz_username) || !psz_password )
            goto error;
    }

    if( url.i_port <= 0 )
        i_port = var_InheritInteger( p_access, "sftp-port" );
    else
        i_port = url.i_port;


    /* Connect to the server using a regular socket */
    p_sys->i_socket = net_Connect( p_access, url.psz_host, i_port, SOCK_STREAM, 0 );
    if( p_sys->i_socket < 0 )
    {
        msg_Err( p_access, "Impossible to open the connection to %s:%i", url.psz_host, i_port );
        goto error;
    }

    /* Create the ssh connexion and wait until the server answer */
    if( ( p_sys->ssh_session = libssh2_session_init() ) == NULL )
        goto error;

    while( ( i_ret = libssh2_session_startup( p_sys->ssh_session,
                                              p_sys->i_socket ) )
           == LIBSSH2_ERROR_EAGAIN );

    if( i_ret != 0 )
    {
        msg_Err( p_access, "Impossible to open the connection to %s:%i", url.psz_host, i_port );
        goto error;
    }

    /* Set the socket in non-blocking mode */
    libssh2_session_set_blocking( p_sys->ssh_session, 1 );

    /* List the know hosts */
    LIBSSH2_KNOWNHOSTS *ssh_knownhosts = libssh2_knownhost_init( p_sys->ssh_session );
    if( !ssh_knownhosts )
        goto error;

    char *psz_home = config_GetUserDir( VLC_HOME_DIR );
    char *psz_knownhosts_file;
    if( asprintf( &psz_knownhosts_file, "%s/.ssh/known_hosts", psz_home ) != -1 )
    {
        libssh2_knownhost_readfile( ssh_knownhosts, psz_knownhosts_file,
                LIBSSH2_KNOWNHOST_FILE_OPENSSH );
        free( psz_knownhosts_file );
    }
    free( psz_home );

    const char *fingerprint = libssh2_session_hostkey( p_sys->ssh_session, &i_len, &i_type );
    struct libssh2_knownhost *host;
    int check = libssh2_knownhost_check( ssh_knownhosts, url.psz_host,
                                         fingerprint, i_len,
                                         LIBSSH2_KNOWNHOST_TYPE_PLAIN |
                                         LIBSSH2_KNOWNHOST_KEYENC_RAW,
                                         &host );

    libssh2_knownhost_free( ssh_knownhosts );

    /* Check that it does match or at least that the host is unknown */
    switch(check)
    {
    case LIBSSH2_KNOWNHOST_CHECK_FAILURE:
    case LIBSSH2_KNOWNHOST_CHECK_NOTFOUND:
        msg_Dbg( p_access, "Unable to check the remote host" );
        break;
    case LIBSSH2_KNOWNHOST_CHECK_MATCH:
        msg_Dbg( p_access, "Succesfuly matched the host" );
        break;
    case LIBSSH2_KNOWNHOST_CHECK_MISMATCH:
        msg_Err( p_access, "The host does not match !! The remote key changed !!" );
        goto error;
    }

    //TODO: ask for the available auth methods

    /* send the login/password */
    if( libssh2_userauth_password( p_sys->ssh_session, psz_username, psz_password ) )
    {
        msg_Err( p_access, "Authentication by password failed" );
        goto error;
    }

    /* Create the sftp session */
    p_sys->sftp_session = libssh2_sftp_init( p_sys->ssh_session );

    if( !p_sys->sftp_session )
    {
        msg_Err( p_access, "Unable to initialize the SFTP session" );
        goto error;
    }

    /* No path, default to user Home */
    if( !url.psz_path )
    {
        const size_t i_size = 1024;
        int i_ret;

        psz_remote_home = malloc( i_size );
        if( !psz_remote_home )
            goto error;

        i_ret = libssh2_sftp_symlink_ex( p_sys->sftp_session, ".", 1,
                                         psz_remote_home, i_size - 1,
                                         LIBSSH2_SFTP_REALPATH );
        if( i_ret <= 0 )
        {
            msg_Err( p_access, "Impossible to get the Home directory" );
            goto error;
        }
        psz_remote_home[i_ret] = '\0';
        psz_path = psz_remote_home;
    }
    else
        psz_path = url.psz_path;

    /* Get some information */
    LIBSSH2_SFTP_ATTRIBUTES attributes;
    if( libssh2_sftp_stat( p_sys->sftp_session, psz_path, &attributes ) )
    {
        msg_Err( p_access, "Impossible to get information about the remote path %s", psz_path );
        goto error;
    }

    if( !LIBSSH2_SFTP_S_ISDIR( attributes.permissions ))
    {
        /* Open the given file */
        p_sys->file = libssh2_sftp_open( p_sys->sftp_session, psz_path, LIBSSH2_FXF_READ, 0 );
        p_sys->filesize = attributes.filesize;

        ACCESS_SET_CALLBACKS( Read, NULL, Control, Seek );
    }
    else
    {
        /* Open the given directory */
        p_sys->file = libssh2_sftp_opendir( p_sys->sftp_session, psz_path );

        p_access->pf_readdir = DirRead;
        p_access->pf_control = DirControl;

        if( p_sys->file )
        {
            if( -1 == asprintf( &p_sys->psz_username_opt, "sftp-user=%s", psz_username ) )
                p_sys->psz_username_opt = NULL;
            if( -1 == asprintf( &p_sys->psz_password_opt, "sftp-pwd=%s", psz_password ) )
                p_sys->psz_password_opt = NULL;
        }
    }

    if( !p_sys->file )
    {
        msg_Err( p_access, "Unable to open the remote path %s", psz_path );
        goto error;
    }

    free( psz_password );
    free( psz_username );
    free( psz_remote_home );
    vlc_UrlClean( &url );
    return VLC_SUCCESS;

error:
    if( p_sys->file )
        libssh2_sftp_close_handle( p_sys->file );
    if( p_sys->ssh_session )
        libssh2_session_free( p_sys->ssh_session );
    free( psz_password );
    free( psz_username );
    free( psz_remote_home );
    vlc_UrlClean( &url );
    if( p_sys->i_socket >= 0 )
        net_Close( p_sys->i_socket );
    free( p_sys );
    return VLC_EGENERIC;
}


/* Close: quit the module */
static void Close( vlc_object_t* p_this )
{
    access_t*   p_access = (access_t*)p_this;
    access_sys_t* p_sys = p_access->p_sys;

    libssh2_sftp_close_handle( p_sys->file );
    libssh2_sftp_shutdown( p_sys->sftp_session );

    libssh2_session_free( p_sys->ssh_session );
    net_Close( p_sys->i_socket );

    free( p_sys->psz_password_opt );
    free( p_sys->psz_username_opt );

    free( p_sys );
}


static ssize_t Read( access_t *p_access, uint8_t *buf, size_t len )
{
    access_sys_t *p_sys = p_access->p_sys;

    if( p_access->info.b_eof )
        return NULL;

    ssize_t val = libssh2_sftp_read(  p_sys->file, (char *)buf, len );
    if( val < 0 )
    {
        p_access->info.b_eof = true;
        msg_Err( p_access, "read failed" );
        return 0;
    }
    else if( val == 0 )
        p_access->info.b_eof = true;

    return val;
}


static int Seek( access_t* p_access, uint64_t i_pos )
{
    p_access->info.b_eof = false;

    libssh2_sftp_seek( p_access->p_sys->file, i_pos );
    return VLC_SUCCESS;
}


static int Control( access_t* p_access, int i_query, va_list args )
{
    bool*       pb_bool;
    int64_t*    pi_64;

    switch( i_query )
    {
    case ACCESS_CAN_SEEK:
        pb_bool = (bool*)va_arg( args, bool* );
        *pb_bool = true;
        break;

    case ACCESS_CAN_FASTSEEK:
        pb_bool = (bool*)va_arg( args, bool* );
        *pb_bool = false;
        break;

    case ACCESS_CAN_PAUSE:
    case ACCESS_CAN_CONTROL_PACE:
        pb_bool = (bool*)va_arg( args, bool* );
        *pb_bool = true;
        break;

    case ACCESS_GET_SIZE:
        if( p_access->pf_readdir != NULL )
            return VLC_EGENERIC;
        *va_arg( args, uint64_t * ) = p_access->p_sys->filesize;
        break;

    case ACCESS_GET_PTS_DELAY:
        pi_64 = (int64_t*)va_arg( args, int64_t* );
        *pi_64 = INT64_C(1000)
               * var_InheritInteger( p_access, "network-caching" );
        break;

    case ACCESS_SET_PAUSE_STATE:
        break;

    default:
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}


/*****************************************************************************
 * Directory access
 *****************************************************************************/

static input_item_t* DirRead( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    input_item_t *p_item = NULL;
    int err;
    /* Allocate 1024 bytes for file name. Longer names are skipped.
     * libssh2 does not support seeking in directory streams.
     * Retrying with larger buffer is possible, but would require
     * re-opening the directory stream.
     */
    size_t i_size = 1024;
    char *psz_file = malloc( i_size );

    if( !psz_file )
        return NULL;

    while( !p_item && 0 != ( err = libssh2_sftp_readdir( p_sys->file, psz_file, i_size, &attrs ) ) )
    {
        if( err < 0 )
        {
            if( err == LIBSSH2_ERROR_BUFFER_TOO_SMALL )
            {
                /* seeking back is not possible ... */
                msg_Dbg( p_access, "skipped too long file name" );
                continue;
            }
            if( err == LIBSSH2_ERROR_EAGAIN )
            {
                continue;
            }
            msg_Err( p_access, "directory read failed" );
            break;
        }

        if( psz_file[0] == '.' )
        {
            continue;
        }

        /* Create an input item for the current entry */

        char *psz_full_uri, *psz_uri;

        psz_uri = encode_URI_component( psz_file );
        if( psz_uri == NULL )
            continue;

        if( asprintf( &psz_full_uri, "sftp://%s/%s", p_access->psz_location, psz_uri ) == -1 )
        {
            free( psz_uri );
            continue;
        }
        free( psz_uri );

        int i_type = LIBSSH2_SFTP_S_ISDIR( attrs.permissions ) ? ITEM_TYPE_DIRECTORY : ITEM_TYPE_FILE;
        p_item = input_item_NewWithTypeExt( psz_full_uri, psz_file,
                                            0, NULL, 0, 0, i_type, 1 );

        if( p_item == NULL )
        {
            free( psz_full_uri );
            break;
        }

        /* Here we save on the node the credentials that allowed us to login.
         * That way the user isn't prompted more than once for credentials */
        if( p_sys->psz_password_opt )
            input_item_AddOption( p_item, p_sys->psz_password_opt, VLC_INPUT_OPTION_TRUSTED );
        if( p_sys->psz_username_opt )
            input_item_AddOption( p_item, p_sys->psz_username_opt, VLC_INPUT_OPTION_TRUSTED );

        free( psz_full_uri );
    }

    free( psz_file );
    return p_item;
}

static int DirControl( access_t *p_access, int i_query, va_list args )
{
    switch( i_query )
    {
    case ACCESS_IS_DIRECTORY:
        *va_arg( args, bool * ) = false; /* is not sorted */
        *va_arg( args, bool * ) = true; /* might loop */
        break;
    default:
        return access_vaDirectoryControlHelper( p_access, i_query, args );
    }

    return VLC_SUCCESS;
}
