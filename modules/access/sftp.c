/*****************************************************************************
 * sftp.c: SFTP input module
 *****************************************************************************
 * Copyright (C) 2009 VLC authors and VideoLAN
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
    add_password("sftp-pwd", NULL, PASS_TEXT, PASS_LONGTEXT)
    add_shortcut( "sftp" )
    set_callbacks( Open, Close )
vlc_module_end ()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static ssize_t  Read( stream_t *, void *, size_t );
static int      Seek( stream_t *, uint64_t );
static int      Control( stream_t *, int, va_list );

static int DirRead( stream_t *, input_item_node_t * );

typedef struct
{
    int i_socket;
    LIBSSH2_SESSION* ssh_session;
    LIBSSH2_SFTP* sftp_session;
    LIBSSH2_SFTP_HANDLE* file;
    uint64_t filesize;
    char *psz_base_url;
} access_sys_t;

static int AuthKeyAgent( stream_t *p_access, const char *psz_username )
{
    access_sys_t* p_sys = p_access->p_sys;
    int i_result = VLC_EGENERIC;
    LIBSSH2_AGENT *p_sshagent = NULL;
    struct libssh2_agent_publickey *p_identity = NULL,
                                   *p_prev_identity = NULL;

    if( !psz_username || !psz_username[0] )
        return i_result;

    p_sshagent = libssh2_agent_init( p_sys->ssh_session );

    if( !p_sshagent )
    {
        msg_Dbg( p_access, "Failed to initialize key agent" );
        goto bailout;
    }
    if( libssh2_agent_connect( p_sshagent ) )
    {
        msg_Dbg( p_access, "Failed to connect key agent" );
        goto bailout;
    }
    if( libssh2_agent_list_identities( p_sshagent ) )
    {
        msg_Dbg( p_access, "Failed to request identities" );
        goto bailout;
    }

    while( libssh2_agent_get_identity( p_sshagent, &p_identity, p_prev_identity ) == 0 )
    {
        msg_Dbg( p_access, "Using key %s", p_identity->comment );
        if( libssh2_agent_userauth( p_sshagent, psz_username, p_identity ) == 0 )
        {
            msg_Info( p_access, "Public key agent authentication succeeded" );
            i_result = VLC_SUCCESS;
            goto bailout;
        }
        msg_Dbg( p_access, "Public key agent authentication failed" );
        p_prev_identity = p_identity;
    }

bailout:
    if( p_sshagent )
    {
        libssh2_agent_disconnect( p_sshagent );
        libssh2_agent_free( p_sshagent );
    }
    return i_result;
}


static int AuthPublicKey( stream_t *p_access, const char *psz_home, const char *psz_username )
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

static void SSHSessionDestroy( stream_t *p_access )
{
    access_sys_t* p_sys = p_access->p_sys;

    if( p_sys->ssh_session )
    {
        libssh2_session_free( p_sys->ssh_session );
        p_sys->ssh_session = NULL;
    }
    if( p_sys->i_socket >= 0 )
    {
        net_Close( p_sys->i_socket );
        p_sys->i_socket = -1;
    }
}

static int SSHSessionInit( stream_t *p_access, const char *psz_host, int i_port )
{
    access_sys_t* p_sys = p_access->p_sys;

    /* Connect to the server using a regular socket */
    assert( p_sys->i_socket == -1 );
    p_sys->i_socket = net_Connect( p_access, psz_host, i_port, SOCK_STREAM,
                                   0 );
    if( p_sys->i_socket < 0 )
        goto error;

    /* Create the ssh connexion and wait until the server answer */
    p_sys->ssh_session = libssh2_session_init();
    if( p_sys->ssh_session == NULL )
        goto error;

    int i_ret;
    while( ( i_ret = libssh2_session_startup( p_sys->ssh_session, p_sys->i_socket ) )
           == LIBSSH2_ERROR_EAGAIN );

    if( i_ret != 0 )
        goto error;

    /* Set the socket in non-blocking mode */
    libssh2_session_set_blocking( p_sys->ssh_session, 1 );
    return VLC_SUCCESS;

error:
    msg_Err( p_access, "Impossible to open the connection to %s:%i",
             psz_host, i_port );
    SSHSessionDestroy( p_access );
    return VLC_EGENERIC;
}

/**
 * Connect to the sftp server and ask for a file
 * @param p_this: the vlc_object
 * @return VLC_SUCCESS if everything was fine
 */
static int Open( vlc_object_t* p_this )
{
    stream_t*   p_access = (stream_t*)p_this;
    access_sys_t* p_sys;
    vlc_credential credential;
    char* psz_path = NULL;
    char *psz_session_username = NULL;
    char* psz_home = NULL;
    int i_port;
    vlc_url_t url;
    size_t i_len;
    int i_type;
    int i_result = VLC_EGENERIC;

    if( !p_access->psz_location )
        return VLC_EGENERIC;

    p_sys = p_access->p_sys = vlc_obj_calloc( p_this, 1, sizeof( access_sys_t ) );
    if( !p_sys ) return VLC_ENOMEM;

    p_sys->i_socket = -1;

    /* Parse the URL */
    if( vlc_UrlParseFixup( &url, p_access->psz_url ) != 0 )
    {
        vlc_UrlClean( &url );
        return VLC_EGENERIC;
    }
    vlc_credential_init( &credential, &url );
    if( url.psz_path != NULL )
    {
        psz_path = vlc_uri_decode_duplicate( url.psz_path );
        if( psz_path == NULL )
            goto error;
    }

    /* Check for some parameters */
    if( EMPTY_STR( url.psz_host ) )
    {
        msg_Err( p_access, "Unable to extract host from %s", p_access->psz_url );
        goto error;
    }

    if( url.i_port <= 0 )
        i_port = var_InheritInteger( p_access, "sftp-port" );
    else
        i_port = url.i_port;

    /* Create the ssh connexion and wait until the server answer */
    if( SSHSessionInit( p_access, url.psz_host, i_port ) != VLC_SUCCESS )
        goto error;

    /* List the know hosts */
    LIBSSH2_KNOWNHOSTS *ssh_knownhosts = libssh2_knownhost_init( p_sys->ssh_session );
    if( !ssh_knownhosts )
        goto error;

    psz_home = config_GetUserDir( VLC_HOME_DIR );
    char *psz_knownhosts_file;
    if( asprintf( &psz_knownhosts_file, "%s/.ssh/known_hosts", psz_home ) != -1 )
    {
        if( libssh2_knownhost_readfile( ssh_knownhosts, psz_knownhosts_file,
                                        LIBSSH2_KNOWNHOST_FILE_OPENSSH ) < 0 )
            msg_Err( p_access, "Failure reading known_hosts '%s'", psz_knownhosts_file );
        free( psz_knownhosts_file );
    }

    const char *fingerprint = libssh2_session_hostkey( p_sys->ssh_session, &i_len, &i_type );
    struct libssh2_knownhost *host;
    int knownhost_fingerprint_algo;

    switch( i_type )
    {
        case LIBSSH2_HOSTKEY_TYPE_RSA:
            knownhost_fingerprint_algo = LIBSSH2_KNOWNHOST_KEY_SSHRSA;
            break;

        case LIBSSH2_HOSTKEY_TYPE_DSS:
            knownhost_fingerprint_algo = LIBSSH2_KNOWNHOST_KEY_SSHDSS;
            break;
#if LIBSSH2_VERSION_NUM >= 0x010900
        case LIBSSH2_HOSTKEY_TYPE_ECDSA_256:
            knownhost_fingerprint_algo = LIBSSH2_KNOWNHOST_KEY_ECDSA_256;
            break;

        case LIBSSH2_HOSTKEY_TYPE_ECDSA_384:
            knownhost_fingerprint_algo = LIBSSH2_KNOWNHOST_KEY_ECDSA_384;
            break;

        case LIBSSH2_HOSTKEY_TYPE_ECDSA_521:
            knownhost_fingerprint_algo = LIBSSH2_KNOWNHOST_KEY_ECDSA_521;
            break;
#endif
        default:
            msg_Err( p_access, "Host uses unrecognized session-key algorithm" );
            libssh2_knownhost_free( ssh_knownhosts );
            goto error;

    }

#if LIBSSH2_VERSION_NUM >= 0x010206
    int check = libssh2_knownhost_checkp( ssh_knownhosts, url.psz_host, i_port,
                                         fingerprint, i_len,
                                         LIBSSH2_KNOWNHOST_TYPE_PLAIN |
                                         LIBSSH2_KNOWNHOST_KEYENC_RAW |
                                         knownhost_fingerprint_algo,
                                         &host );
#else
    int check = libssh2_knownhost_check( ssh_knownhosts, url.psz_host,
                                         fingerprint, i_len,
                                         LIBSSH2_KNOWNHOST_TYPE_PLAIN |
                                         LIBSSH2_KNOWNHOST_KEYENC_RAW |
                                         knownhost_fingerprint_algo,
                                         &host );
#endif

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

    vlc_credential_get( &credential, p_access, "sftp-user", "sftp-pwd",
                        NULL, NULL );
    char* psz_userauthlist = NULL;
    bool b_publickey_tried = false;
    do
    {
        if (!credential.psz_username || !credential.psz_username[0])
            continue;

        if( !psz_session_username )
        {
            psz_session_username = strdup( credential.psz_username );
            psz_userauthlist =
                libssh2_userauth_list( p_sys->ssh_session, credential.psz_username,
                                       strlen( credential.psz_username ) );
        }
        else if( strcmp( psz_session_username, credential.psz_username ) != 0 )
        {
            msg_Warn( p_access, "The username changed, starting a new ssh session" );

            SSHSessionDestroy( p_access );
            if( SSHSessionInit( p_access, url.psz_host, i_port ) != VLC_SUCCESS )
                goto error;

            b_publickey_tried = false;
            free( psz_session_username );
            psz_session_username = strdup( credential.psz_username );
            psz_userauthlist =
                libssh2_userauth_list( p_sys->ssh_session, credential.psz_username,
                                       strlen( credential.psz_username ) );
        }
        if( !psz_session_username || !psz_userauthlist )
            goto error;

        /* TODO: Follow PreferredAuthentications in ssh_config */

        if( strstr( psz_userauthlist, "publickey" ) != NULL && !b_publickey_tried )
        {
            /* Don't try public key multiple times to avoid getting black
             * listed */
            b_publickey_tried = true;
            if( AuthKeyAgent( p_access, credential.psz_username ) == VLC_SUCCESS
             || AuthPublicKey( p_access, psz_home, credential.psz_username ) == VLC_SUCCESS )
                break;
        }

        if( strstr( psz_userauthlist, "password" ) != NULL
         && credential.psz_password != NULL
         && libssh2_userauth_password( p_sys->ssh_session,
                                       credential.psz_username,
                                       credential.psz_password ) == 0 )
        {
            vlc_credential_store( &credential, p_access );
            break;
        }

        msg_Warn( p_access, "sftp auth failed for %s", credential.psz_username );
    } while( vlc_credential_get( &credential, p_access, "sftp-user", "sftp-pwd",
                                _("SFTP authentication"),
                                _("Please enter a valid login and password for "
                                "the sftp connexion to %s"), url.psz_host ) );

    /* Create the sftp session */
    p_sys->sftp_session = libssh2_sftp_init( p_sys->ssh_session );

    if( !p_sys->sftp_session )
    {
        msg_Err( p_access, "Unable to initialize the SFTP session" );
        goto error;
    }

    /* No path, default to user Home */
    if( !psz_path )
    {
        const size_t i_size = 1024;
        int i_read;

        char* psz_remote_home = malloc( i_size );
        if( !psz_remote_home )
            goto error;

        i_read = libssh2_sftp_symlink_ex( p_sys->sftp_session, ".", 1,
                                          psz_remote_home, i_size - 1,
                                          LIBSSH2_SFTP_REALPATH );
        if( i_read <= 0 )
        {
            msg_Err( p_access, "Impossible to get the Home directory" );
            free( psz_remote_home );
            goto error;
        }
        psz_remote_home[i_read] = '\0';
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
        p_access->pf_control = access_vaDirectoryControlHelper;

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
    free( psz_session_username );
    free( psz_path );
    vlc_credential_clean( &credential );
    vlc_UrlClean( &url );
    if( i_result != VLC_SUCCESS ) {
        Close( p_this );
    }
    return i_result;
}


/* Close: quit the module */
static void Close( vlc_object_t* p_this )
{
    stream_t*   p_access = (stream_t*)p_this;
    access_sys_t* p_sys = p_access->p_sys;

    if( p_sys->file )
        libssh2_sftp_close_handle( p_sys->file );
    if( p_sys->sftp_session )
        libssh2_sftp_shutdown( p_sys->sftp_session );
    SSHSessionDestroy( p_access );

    free( p_sys->psz_base_url );
}


static ssize_t Read( stream_t *p_access, void *buf, size_t len )
{
    access_sys_t *p_sys = p_access->p_sys;

    ssize_t val = libssh2_sftp_read(  p_sys->file, buf, len );
    if( val < 0 )
    {
        msg_Err( p_access, "read failed" );
        return 0;
    }

    return val;
}


static int Seek( stream_t* p_access, uint64_t i_pos )
{
    access_sys_t *sys = p_access->p_sys;

    libssh2_sftp_seek( sys->file, i_pos );
    return VLC_SUCCESS;
}


static int Control( stream_t* p_access, int i_query, va_list args )
{
    access_sys_t *sys = p_access->p_sys;
    bool*       pb_bool;

    switch( i_query )
    {
    case STREAM_CAN_SEEK:
        pb_bool = va_arg( args, bool * );
        *pb_bool = true;
        break;

    case STREAM_CAN_FASTSEEK:
        pb_bool = va_arg( args, bool * );
        *pb_bool = false;
        break;

    case STREAM_CAN_PAUSE:
    case STREAM_CAN_CONTROL_PACE:
        pb_bool = va_arg( args, bool * );
        *pb_bool = true;
        break;

    case STREAM_GET_SIZE:
        if( p_access->pf_readdir != NULL )
            return VLC_EGENERIC;
        *va_arg( args, uint64_t * ) = sys->filesize;
        break;

    case STREAM_GET_PTS_DELAY:
        *va_arg( args, vlc_tick_t * ) =
            VLC_TICK_FROM_MS(var_InheritInteger( p_access, "network-caching" ));
        break;

    case STREAM_SET_PAUSE_STATE:
        break;

    default:
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}


/*****************************************************************************
 * Directory access
 *****************************************************************************/

static int DirRead (stream_t *p_access, input_item_node_t *p_current_node)
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

    struct vlc_readdir_helper rdh;
    vlc_readdir_helper_init( &rdh, p_access, p_current_node );

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
        i_ret = vlc_readdir_helper_additem( &rdh, psz_full_uri, NULL, psz_file,
                                            i_type, ITEM_NET );
        free( psz_full_uri );
    }

    vlc_readdir_helper_finish( &rdh, i_ret == VLC_SUCCESS );
    free( psz_file );
    return i_ret;
}
