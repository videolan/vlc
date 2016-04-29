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
#include <vlc_input_item.h>
#include <vlc_network.h>
#include <vlc_url.h>
#include <vlc_keystore.h>

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

static int DirRead( access_t *, input_item_node_t * );
static int DirControl( access_t *, int, va_list );

struct access_sys_t
{
    int i_socket;
    LIBSSH2_SESSION* ssh_session;
    LIBSSH2_SFTP* sftp_session;
    LIBSSH2_SFTP_HANDLE* file;
    uint64_t filesize;
    char *psz_base_url;
};


static int AuthPublicKey( access_t *p_access, const char *psz_home, const char *psz_username )
{
    access_sys_t* p_sys = p_access->p_sys;
    int i_result = VLC_EGENERIC;
    char *psz_keyfile1 = NULL;
    char *psz_keyfile2 = NULL;

    if( !psz_username || !psz_username[0] )
        return i_result;

    if( asprintf( &psz_keyfile1, "%s/.ssh/id_rsa.pub", psz_home ) == -1 ||
        asprintf( &psz_keyfile2, "%s/.ssh/id_rsa",     psz_home ) == -1 )
        goto bailout;

    if( libssh2_userauth_publickey_fromfile( p_sys->ssh_session, psz_username, psz_keyfile1, psz_keyfile2, NULL ) )
    {
        msg_Dbg( p_access, "Public key authentication failed" );
        goto bailout;
    }

    msg_Info( p_access, "Public key authentication succeeded" );
    i_result = VLC_SUCCESS;

 bailout:
    free( psz_keyfile1 );
    free( psz_keyfile2 );
    return i_result;
}

/**
 * Connect to the sftp server and ask for a file
 * @param p_this: the vlc_object
 * @return VLC_SUCCESS if everything was fine
 */
static int Open( vlc_object_t* p_this )
{
    access_t*   p_access = (access_t*)p_this;
    access_sys_t* p_sys;
    vlc_url_t credential_url;
    vlc_credential credential;
    const char* psz_path;
    char* psz_remote_home = NULL;
    char* psz_home = NULL;
    int i_port;
    int i_ret;
    vlc_url_t url;
    size_t i_len;
    int i_type;
    int i_result = VLC_EGENERIC;

    if( !p_access->psz_location )
        return VLC_EGENERIC;

    access_InitFields( p_access );
    p_sys = p_access->p_sys = (access_sys_t*)calloc( 1, sizeof( access_sys_t ) );
    if( !p_sys ) return VLC_ENOMEM;

    p_sys->i_socket = -1;

    vlc_UrlParse( &credential_url, p_access->psz_url );
    vlc_credential_init( &credential, &credential_url );

    /* Parse the URL */
    vlc_UrlParse( &url, p_access->psz_location );
    vlc_uri_decode( url.psz_path );

    /* Check for some parameters */
    if( EMPTY_STR( url.psz_host ) )
    {
        msg_Err( p_access, "You might give a non empty host" );
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

    psz_home = config_GetUserDir( VLC_HOME_DIR );
    char *psz_knownhosts_file;
    if( asprintf( &psz_knownhosts_file, "%s/.ssh/known_hosts", psz_home ) != -1 )
    {
        libssh2_knownhost_readfile( ssh_knownhosts, psz_knownhosts_file,
                LIBSSH2_KNOWNHOST_FILE_OPENSSH );
        free( psz_knownhosts_file );
    }

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

    /* Try public key auth first */
    if( AuthPublicKey( p_access, psz_home, url.psz_username ) != VLC_SUCCESS )
    {
    while( vlc_credential_get( &credential, p_access, "sftp-user", "sftp-pwd",
                               _("SFTP authentication"),
                               _("Please enter a valid login and password for "
                               "the sftp connexion to %s"), url.psz_host ) )
    {
        /* send the login/password */
        if( libssh2_userauth_password( p_sys->ssh_session,
                                       credential.psz_username,
                                       credential.psz_password ) == 0 )
        {
            vlc_credential_store( &credential, p_access );
            break;
        }

        if( AuthPublicKey( p_access, psz_home, credential.psz_username ) == VLC_SUCCESS )
            break;

        msg_Warn( p_access, "sftp auth failed for %s", credential.psz_username );
    }
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

        /* store base url for directory read */
        char *base = vlc_path2uri( psz_path, "sftp" );
        if( !base )
            goto error;
        if( -1 == asprintf( &p_sys->psz_base_url, "sftp://%s%s", p_access->psz_location, base + 7 ) )
        {
            free( base );
            goto error;
        }
        free( base );
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

        if( !p_sys->psz_base_url )
        {
            if( asprintf( &p_sys->psz_base_url, "sftp://%s", p_access->psz_location ) == -1 )
                goto error;

            /* trim trailing '/' */
            size_t len = strlen( p_sys->psz_base_url );
            if( len > 0 && p_sys->psz_base_url[ len - 1 ] == '/' )
                p_sys->psz_base_url[ len - 1 ] = 0;
        }
    }

    if( !p_sys->file )
    {
        msg_Err( p_access, "Unable to open the remote path %s", psz_path );
        goto error;
    }

    i_result = VLC_SUCCESS;

error:
    free( psz_home );
    free( psz_remote_home );
    vlc_UrlClean( &url );
    vlc_credential_clean( &credential );
    vlc_UrlClean( &credential_url );
    if( i_result != VLC_SUCCESS ) {
        Close( p_this );
    }
    return i_result;
}


/* Close: quit the module */
static void Close( vlc_object_t* p_this )
{
    access_t*   p_access = (access_t*)p_this;
    access_sys_t* p_sys = p_access->p_sys;

    if( p_sys->file )
        libssh2_sftp_close_handle( p_sys->file );
    if( p_sys->sftp_session )
        libssh2_sftp_shutdown( p_sys->sftp_session );
    if( p_sys->ssh_session )
        libssh2_session_free( p_sys->ssh_session );
    if( p_sys->i_socket >= 0 )
        net_Close( p_sys->i_socket );

    free( p_sys->psz_base_url );
    free( p_sys );
}


static ssize_t Read( access_t *p_access, uint8_t *buf, size_t len )
{
    access_sys_t *p_sys = p_access->p_sys;

    if( p_access->info.b_eof )
        return 0;

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

static int DirRead (access_t *p_access, input_item_node_t *p_current_node)
{
    access_sys_t *p_sys = p_access->p_sys;
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int i_ret = VLC_SUCCESS;
    int err;
    /* Allocate 1024 bytes for file name. Longer names are skipped.
     * libssh2 does not support seeking in directory streams.
     * Retrying with larger buffer is possible, but would require
     * re-opening the directory stream.
     */
    size_t i_size = 1024;
    char *psz_file = malloc( i_size );

    if( !psz_file )
        return VLC_ENOMEM;

    struct access_fsdir fsdir;
    access_fsdir_init( &fsdir, p_access, p_current_node );

    while( i_ret == VLC_SUCCESS
        && 0 != ( err = libssh2_sftp_readdir( p_sys->file, psz_file, i_size, &attrs ) ) )
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

        /* Create an input item for the current entry */

        char *psz_full_uri, *psz_uri;

        psz_uri = vlc_uri_encode( psz_file );
        if( psz_uri == NULL )
        {
            i_ret = VLC_ENOMEM;
            break;
        }

        if( asprintf( &psz_full_uri, "%s/%s", p_sys->psz_base_url, psz_uri ) == -1 )
        {
            free( psz_uri );
            i_ret = VLC_ENOMEM;
            break;
        }
        free( psz_uri );

        int i_type = LIBSSH2_SFTP_S_ISDIR( attrs.permissions ) ? ITEM_TYPE_DIRECTORY : ITEM_TYPE_FILE;
        i_ret = access_fsdir_additem( &fsdir, psz_full_uri, psz_file, i_type,
                                      ITEM_NET );
        free( psz_full_uri );
    }

    access_fsdir_finish( &fsdir, i_ret == VLC_SUCCESS );
    free( psz_file );
    return i_ret;
}

static int DirControl( access_t *p_access, int i_query, va_list args )
{
    switch( i_query )
    {
    case ACCESS_IS_DIRECTORY:
        *va_arg( args, bool * ) = true; /* might loop */
        break;
    default:
        return access_vaDirectoryControlHelper( p_access, i_query, args );
    }

    return VLC_SUCCESS;
}
