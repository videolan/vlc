/*****************************************************************************
 * bdsm/access.c: liBDSM based SMB/CIFS access module
 *****************************************************************************
 * Copyright (C) 2001-2014 VLC authors and VideoLAN
 *
 * Authors: Julien 'Lta' BALLET <contact # lta 'dot' io>
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
#include <vlc_services_discovery.h>
#include <vlc_url.h>
#include <vlc_access.h>
#include <vlc_variables.h>
#include <vlc_keystore.h>
#include <vlc_network.h>
#include <vlc_interrupt.h>

#include <assert.h>
#include <errno.h>
#include <string.h>
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
# include <netinet/in.h>
# include <netdb.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include <bdsm/bdsm.h>
#include "../smb_common.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
int bdsm_SdOpen( vlc_object_t * );
void bdsm_SdClose( vlc_object_t * );
int bdsm_sd_probe_Open( vlc_object_t * );

#define SMB_FORCE_V1_TEXT N_("Force the SMBv1 protocol (At your own risk)")
#define SMB_FORCE_V1_LONGTEXT \
    N_("Enable it, at your own risk, if you can't connect to Windows shares. " \
    "If this option is needed, you should consider updating your Windows / " \
    "Samba server and disabling the SMBv1 protocol as using this protocol " \
    "has security implications.")

static int OpenNotForced( vlc_object_t * );
static int OpenForced( vlc_object_t * );
static void Close( vlc_object_t * );

VLC_SD_PROBE_HELPER( "dsm", N_("Windows networks"), SD_CAT_LAN )

#define BDSM_HELP N_("libdsm's SMB (Windows network shares) input and browser")

vlc_module_begin ()
    set_shortname( "dsm" )
    set_description( N_("libdsm SMB input") )
    set_help(BDSM_HELP)
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )
    add_string( "smb-user", NULL, SMB_USER_TEXT, SMB_USER_LONGTEXT, false )
    add_password("smb-pwd", NULL, SMB_PASS_TEXT, SMB_PASS_LONGTEXT)
    add_string( "smb-domain", NULL, SMB_DOMAIN_TEXT, SMB_DOMAIN_LONGTEXT, false )
    add_bool( "smb-force-v1", false, SMB_FORCE_V1_TEXT, SMB_FORCE_V1_LONGTEXT, false )
    add_shortcut( "smb", "cifs" )

    set_capability( "access", 22 )
    set_callbacks( OpenForced, Close )

    add_submodule()
        set_capability( "access", 20 )
        set_callbacks( OpenNotForced, Close )
        add_shortcut( "smb", "cifs" )

    add_submodule()
        add_shortcut( "dsm-sd" )
        set_description( N_("libdsm NETBIOS discovery module") )
        set_category( CAT_PLAYLIST )
        set_subcategory( SUBCAT_PLAYLIST_SD )
        set_capability( "services_discovery", 0 )
        set_callbacks( bdsm_SdOpen, bdsm_SdClose )

        VLC_SD_PROBE_SUBMODULE

vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static ssize_t Read( stream_t *, void *, size_t );
static int Seek( stream_t *, uint64_t );
static int Control( stream_t *, int, va_list );
static int BrowserInit( stream_t *p_access );

static int get_address( stream_t *p_access );
static int login( stream_t *p_access );
static bool get_path( stream_t *p_access );
static int add_item( stream_t *p_access,  struct vlc_readdir_helper *p_rdh,
                     const char *psz_name, int i_type );

typedef struct
{
    netbios_ns         *p_ns;               /**< Netbios name service */
    smb_session        *p_session;          /**< bdsm SMB Session object */

    vlc_url_t           url;
    char               *psz_fullpath;
    const char         *psz_share;
    const char         *psz_path;

    char                netbios_name[16];
    struct in_addr      addr;

    smb_fd              i_fd;               /**< SMB fd for the file we're reading */
    smb_tid             i_tid;              /**< SMB Tree ID we're connected to */
} access_sys_t;

/*****************************************************************************
 * Open: Initialize module's data structures and libdsm
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    stream_t     *p_access = (stream_t*)p_this;
    access_sys_t *p_sys;
    smb_stat st;
    int status = DSM_ERROR_GENERIC;

    /* Init p_access */
    p_sys = p_access->p_sys = (access_sys_t*)calloc( 1, sizeof( access_sys_t ) );
    if( p_access->p_sys == NULL )
        return VLC_ENOMEM;

    p_sys->p_ns = netbios_ns_new();
    if( p_sys->p_ns == NULL )
        goto error;

    p_sys->p_session = smb_session_new();
    if( p_sys->p_session == NULL )
        goto error;

    if( vlc_UrlParseFixup( &p_sys->url, p_access->psz_url ) != 0 )
        goto error;

    if( get_address( p_access ) != VLC_SUCCESS )
    {
        status = DSM_ERROR_NETWORK;
        goto error;
    }

    msg_Dbg( p_access, "Session: Host name = %s, ip = %s", p_sys->netbios_name,
             inet_ntoa( p_sys->addr ) );

    /* Now that we have the required data, let's establish a session */
    status = smb_session_connect( p_sys->p_session, p_sys->netbios_name,
                                  p_sys->addr.s_addr, SMB_TRANSPORT_TCP );
    if( status != DSM_SUCCESS )
    {
        msg_Err( p_access, "Unable to connect/negotiate SMB session");
        /* FIXME: libdsm wrongly return network error when the server can't
         * handle the SMBv1 protocol */
        status = DSM_ERROR_GENERIC;
        goto error;
    }

    get_path( p_access );

    if( login( p_access ) != VLC_SUCCESS )
    {
        msg_Err( p_access, "Unable to open file with path %s (in share %s)",
                 p_sys->psz_path, p_sys->psz_share );
        goto error;
    }

    /* If there is no shares, browse them */
    if( !p_sys->psz_share )
        return BrowserInit( p_access );

    assert(p_sys->i_fd > 0);

    msg_Dbg( p_access, "Path: Share name = %s, path = %s", p_sys->psz_share,
             p_sys->psz_path );

    st = smb_stat_fd( p_sys->p_session, p_sys->i_fd );
    if( smb_stat_get( st, SMB_STAT_ISDIR ) )
    {
        smb_fclose( p_sys->p_session, p_sys->i_fd );
        return BrowserInit( p_access );
    }

    msg_Dbg( p_access, "Successfully opened smb://%s", p_access->psz_location );

    ACCESS_SET_CALLBACKS( Read, NULL, Control, Seek );
    return VLC_SUCCESS;

    error:
        Close( p_this );

        /* Returning VLC_ETIMEOUT will stop the module probe and prevent to
         * load the next smb module. The dsm module can return this specific
         * error in case of network error (DSM_ERROR_NETWORK) or when the user
         * asked to cancel it (vlc_killed()). Indeed, in these cases, it is
         * useless to try next smb modules. */
        return vlc_killed() || status == DSM_ERROR_NETWORK ? VLC_ETIMEOUT
             : VLC_EGENERIC;
}

static int OpenForced( vlc_object_t *p_this )
{
    if( !var_InheritBool( p_this , "smb-force-v1" ) )
        return VLC_EGENERIC;

    msg_Warn( p_this, "SMB 2/3 disabled by the user, using *unsafe* SMB 1" );
    return Open( p_this );
}

static int OpenNotForced( vlc_object_t *p_this )
{
    if( var_InheritBool( p_this , "smb-force-v1" ) )
        return VLC_EGENERIC; /* OpenForced should have breen probed first */

    return Open( p_this );
}

/*****************************************************************************
 * Close: free unused data structures
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    stream_t     *p_access = (stream_t*)p_this;
    access_sys_t *p_sys = p_access->p_sys;

    if( p_sys->p_ns )
        netbios_ns_destroy( p_sys->p_ns );
    if( p_sys->i_fd )
        smb_fclose( p_sys->p_session, p_sys->i_fd );
    if( p_sys->p_session )
        smb_session_destroy( p_sys->p_session );
    vlc_UrlClean( &p_sys->url );
    free( p_sys->psz_fullpath );

    free( p_sys );
}

/*****************************************************************************
 * Local functions
 *****************************************************************************/

/* Returns VLC_EGENERIC if it wasn't able to get an ip address to connect to */
static int get_address( stream_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;

    if( p_sys->url.psz_host != NULL &&
        !inet_pton( AF_INET, p_sys->url.psz_host, &p_sys->addr ) )
    {
        /* This is not an ip address, let's try netbios/dns resolve */
        struct addrinfo *p_info = NULL;

        /* Is this a netbios name on this LAN ? */
        uint32_t ip4_addr;
        if( netbios_ns_resolve( p_sys->p_ns, p_sys->url.psz_host,
                                NETBIOS_FILESERVER,
                                &ip4_addr) == 0 )
        {
            p_sys->addr.s_addr = ip4_addr;
            strlcpy( p_sys->netbios_name, p_sys->url.psz_host, 16);
            return VLC_SUCCESS;
        }
        /* or is it an existing dns name ? */
        else if( getaddrinfo( p_sys->url.psz_host, NULL, NULL, &p_info ) == 0 )
        {
            if( p_info->ai_family == AF_INET )
            {
                struct sockaddr_in *in = (struct sockaddr_in *)p_info->ai_addr;
                p_sys->addr.s_addr = in->sin_addr.s_addr;
            }
            if( p_info->ai_family != AF_INET )
            {
                freeaddrinfo( p_info );
                return VLC_EGENERIC;
            }
            freeaddrinfo( p_info );
        }
        else
            return VLC_EGENERIC;
    }

    /* We have an IP address, let's find the NETBIOS name */
    const char *psz_nbt = netbios_ns_inverse( p_sys->p_ns, p_sys->addr.s_addr );
    if( psz_nbt != NULL )
        strlcpy( p_sys->netbios_name, psz_nbt, 16 );
    else
    {
        msg_Warn( p_access, "Unable to get netbios name of %s",
            p_sys->url.psz_host );
        p_sys->netbios_name[0] = '\0';
    }

    return VLC_SUCCESS;
}

/* Returns an errno code */
static int smb_connect( stream_t *p_access, const char *psz_login,
                        const char *psz_password, const char *psz_domain)
{
    access_sys_t *p_sys = p_access->p_sys;
    int ret;

    smb_session_set_creds( p_sys->p_session, psz_domain,
                           psz_login, psz_password );
    if( smb_session_login( p_sys->p_session ) != DSM_SUCCESS )
        return EACCES;

    if( !p_sys->psz_share )
        return 0;

    /* Connect to the share */
    ret = smb_tree_connect( p_sys->p_session, p_sys->psz_share, &p_sys->i_tid );
    if( ret != DSM_SUCCESS )
        goto error;

    /* Let's finally ask a handle to the file we wanna read ! */
    ret = smb_fopen( p_sys->p_session, p_sys->i_tid, p_sys->psz_path,
                     SMB_MOD_RO, &p_sys->i_fd );
    if( ret != DSM_SUCCESS )
        goto error;

    return 0;

error:
    return ret == DSM_ERROR_NT && smb_session_get_nt_status( p_sys->p_session )
        == NT_STATUS_ACCESS_DENIED ? EACCES : ENOENT;
}

/* Performs login with existing credentials and ask the user for new ones on
   failure */
static int login( stream_t *p_access )
{
    int i_ret = VLC_EGENERIC;
    access_sys_t *p_sys = p_access->p_sys;
    vlc_credential credential;
    char *psz_var_domain;
    const char *psz_login, *psz_password, *psz_domain;
    bool b_guest = false;

    vlc_credential_init( &credential, &p_sys->url );
    psz_var_domain = var_InheritString( p_access, "smb-domain" );
    credential.psz_realm = psz_var_domain ? psz_var_domain : NULL;

    vlc_credential_get( &credential, p_access, "smb-user", "smb-pwd",
                        NULL, NULL );

    if( !credential.psz_username )
    {
        psz_login = "Guest";
        psz_password = "";
        b_guest = true;
    }
    else
    {
        psz_login = credential.psz_username;
        psz_password = credential.psz_password;
    }
    psz_domain = credential.psz_realm ? credential.psz_realm : p_sys->netbios_name;

    /* Try to authenticate on the remote machine */
    int connect_err = smb_connect( p_access, psz_login, psz_password, psz_domain );
    if( connect_err == ENOENT )
        goto error;

    if( connect_err == EACCES )
    {
        if (var_Type(p_access, "smb-dialog-failed") != 0)
        {
            /* A higher priority smb module (likely smb2) already requested
             * credentials to the users. It is useless to request it again. */
            goto error;
        }
        while( connect_err == EACCES
            && vlc_credential_get( &credential, p_access, "smb-user", "smb-pwd",
                                   SMB1_LOGIN_DIALOG_TITLE,
                                   SMB_LOGIN_DIALOG_TEXT, p_sys->netbios_name ) )
        {
            b_guest = false;
            psz_login = credential.psz_username;
            psz_password = credential.psz_password;
            psz_domain = credential.psz_realm ? credential.psz_realm
                                              : p_sys->netbios_name;
            connect_err = smb_connect( p_access, psz_login, psz_password, psz_domain );
        }

        if( connect_err != 0 )
        {
            msg_Err( p_access, "Unable to login" );
            goto error;
        }
    }
    assert( connect_err == 0 );

    if( smb_session_is_guest( p_sys->p_session ) == 1 )
    {
        msg_Warn( p_access, "Login failure but you were logged in as a Guest");
        b_guest = true;
    }

    msg_Warn( p_access, "Creds: username = '%s', domain = '%s'",
             psz_login, psz_domain );
    if( !b_guest )
        vlc_credential_store( &credential, p_access );

    i_ret = VLC_SUCCESS;
error:
    vlc_credential_clean( &credential );
    free( psz_var_domain );
    return i_ret;
}

static void backslash_path( char *psz_path )
{
    char *iter = psz_path;

    /* Let's switch the path delimiters from / to \ */
    while( *iter != '\0' )
    {
        if( *iter == '/' )
            *iter = '\\';
        iter++;
    }
}

/* Get the share and filepath from uri (also replace all / by \ in url.psz_path) */
static bool get_path( stream_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    char *iter;

    if( p_sys->url.psz_path == NULL )
        return false;

    p_sys->psz_fullpath = vlc_uri_decode_duplicate( p_sys->url.psz_path );
    if( p_sys->psz_fullpath == NULL )
        return false;

    backslash_path( p_sys->psz_fullpath );

    /* Is path longer than just "/" ? */
    if( strlen( p_sys->psz_fullpath ) > 1 )
    {
        iter = p_sys->psz_fullpath;
        while( *iter == '\\' ) iter++; /* Handle smb://Host/////Share/ */

        p_sys->psz_share = iter;
    }
    else
    {
        msg_Dbg( p_access, "no share, nor file path provided, will switch to browser");
        return false;
    }

    iter = strchr( p_sys->psz_share, '\\' );
    if( iter == NULL || strlen(iter + 1) == 0 )
    {
        if( iter != NULL ) /* Remove the trailing \ */
            *iter = '\0';
        p_sys->psz_path = "";

        msg_Dbg( p_access, "no file path provided, will switch to browser ");
        return true;
    }

    p_sys->psz_path = iter + 1; /* Skip the first \ */
    *iter = '\0';

    return true;
}

/*****************************************************************************
 * Seek: try to go at the right place
 *****************************************************************************/
static int Seek( stream_t *p_access, uint64_t i_pos )
{
    access_sys_t *p_sys = p_access->p_sys;

    if( i_pos >= INT64_MAX )
        return VLC_EGENERIC;

    msg_Dbg( p_access, "seeking to %"PRId64, i_pos );

    if (smb_fseek(p_sys->p_session, p_sys->i_fd, i_pos, SMB_SEEK_SET) == -1)
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Read:
 *****************************************************************************/
static ssize_t Read( stream_t *p_access, void *p_buffer, size_t i_len )
{
    access_sys_t *p_sys = p_access->p_sys;
    int i_read;

    i_read = smb_fread( p_sys->p_session, p_sys->i_fd, p_buffer, i_len );
    if( i_read < 0 )
    {
        msg_Err( p_access, "read failed" );
        if (errno != EINTR && errno != EAGAIN)
            return 0;
        return -1;
    }

    return i_read;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( stream_t *p_access, int i_query, va_list args )
{
    access_sys_t *p_sys = p_access->p_sys;

    switch( i_query )
    {
    case STREAM_CAN_SEEK:
    case STREAM_CAN_PAUSE:
    case STREAM_CAN_CONTROL_PACE:
        *va_arg( args, bool* ) = true;
        break;

    case STREAM_CAN_FASTSEEK:
        *va_arg( args, bool* ) = false;
        break;

    case STREAM_GET_SIZE:
    {
        smb_stat st = smb_stat_fd( p_sys->p_session, p_sys->i_fd );
        *va_arg( args, uint64_t * ) = smb_stat_get( st, SMB_STAT_SIZE );
        break;
    }
    case STREAM_GET_PTS_DELAY:
        *va_arg( args, vlc_tick_t * ) = VLC_TICK_FROM_MS(
            var_InheritInteger( p_access, "network-caching" ) );
        break;

    case STREAM_SET_PAUSE_STATE:
        /* Nothing to do */
        break;

    default:
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static int add_item( stream_t *p_access, struct vlc_readdir_helper *p_rdh,
                     const char *psz_name, int i_type )
{
    char         *psz_uri;
    int           i_ret;

    char *psz_encoded_name = vlc_uri_encode( psz_name );
    if( psz_encoded_name == NULL )
        return VLC_ENOMEM;
    const char *psz_sep = p_access->psz_location[0] != '\0'
        && p_access->psz_location[strlen(p_access->psz_location) -1] != '/'
        ? "/" : "";
    i_ret = asprintf( &psz_uri, "smb://%s%s%s", p_access->psz_location,
                      psz_sep, psz_encoded_name );
    free( psz_encoded_name );
    if( i_ret == -1 )
        return VLC_ENOMEM;

    i_ret = vlc_readdir_helper_additem( p_rdh, psz_uri, NULL, psz_name, i_type,
                                        ITEM_NET );
    free( psz_uri );
    return i_ret;
}

static int BrowseShare( stream_t *p_access, input_item_node_t *p_node )
{
    access_sys_t *p_sys = p_access->p_sys;
    smb_share_list  shares;
    const char     *psz_name;
    size_t          share_count;
    int             i_ret = VLC_SUCCESS;

    if( smb_share_get_list( p_sys->p_session, &shares, &share_count )
        != DSM_SUCCESS )
        return VLC_EGENERIC;

    struct vlc_readdir_helper rdh;
    vlc_readdir_helper_init( &rdh, p_access, p_node );

    for( size_t i = 0; i < share_count && i_ret == VLC_SUCCESS; i++ )
    {
        psz_name = smb_share_list_at( shares, i );

        if( psz_name[strlen( psz_name ) - 1] == '$')
            continue;

        i_ret = add_item( p_access, &rdh, psz_name, ITEM_TYPE_DIRECTORY );
    }

    vlc_readdir_helper_finish( &rdh, i_ret == VLC_SUCCESS );

    smb_share_list_destroy( shares );
    return i_ret;
}

static int BrowseDirectory( stream_t *p_access, input_item_node_t *p_node )
{
    access_sys_t *p_sys = p_access->p_sys;
    smb_stat_list   files;
    smb_stat        st;
    char           *psz_query;
    const char     *psz_name;
    size_t          files_count;
    int             i_ret = VLC_SUCCESS;

    if( p_sys->psz_path != NULL )
    {
        if( asprintf( &psz_query, "%s\\*", p_sys->psz_path ) == -1 )
            return VLC_ENOMEM;
        files = smb_find( p_sys->p_session, p_sys->i_tid, psz_query );
        free( psz_query );
    }
    else
        files = smb_find( p_sys->p_session, p_sys->i_tid, "\\*" );

    if( files == NULL )
        return VLC_EGENERIC;

    struct vlc_readdir_helper rdh;
    vlc_readdir_helper_init( &rdh, p_access, p_node );

    files_count = smb_stat_list_count( files );
    for( size_t i = 0; i < files_count && i_ret == VLC_SUCCESS; i++ )
    {
        int i_type;

        st = smb_stat_list_at( files, i );

        if( st == NULL )
        {
            continue;
        }

        psz_name = smb_stat_name( st );

        i_type = smb_stat_get( st, SMB_STAT_ISDIR ) ?
                 ITEM_TYPE_DIRECTORY : ITEM_TYPE_FILE;
        i_ret = add_item( p_access, &rdh, psz_name, i_type );
    }

    vlc_readdir_helper_finish( &rdh, i_ret == VLC_SUCCESS );

    smb_stat_list_destroy( files );
    return i_ret;
}

static int BrowserInit( stream_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;

    if( p_sys->psz_share == NULL )
        p_access->pf_readdir = BrowseShare;
    else
        p_access->pf_readdir = BrowseDirectory;
    p_access->pf_control = access_vaDirectoryControlHelper;

    return VLC_SUCCESS;
}
