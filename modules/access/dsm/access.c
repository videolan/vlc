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
#include <vlc_dialog.h>

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <bdsm/bdsm.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
int bdsm_SdOpen( vlc_object_t * );
void bdsm_SdClose( vlc_object_t * );
int bdsm_sd_probe_Open( vlc_object_t * );

static int Open( vlc_object_t * );
static void Close( vlc_object_t * );

#define vlc_sd_probe_Open bdsm_sd_probe_Open

#define USER_TEXT N_("Username")
#define USER_LONGTEXT N_("Username that will be used for the connection, " \
        "if no username is set in the URL.")
#define PASS_TEXT N_("Password")
#define PASS_LONGTEXT N_("Password that will be used for the connection, " \
        "if no username or password are set in URL.")
#define DOMAIN_TEXT N_("SMB domain")
#define DOMAIN_LONGTEXT N_("Domain/Workgroup that " \
    "will be used for the connection. Domain of uri will also be tried.")

#define BDSM_LOGIN_DIALOG_RETRY 1

#define BDSM_HELP N_("libdsm's SMB (Windows network shares) input and browser")

vlc_module_begin ()
    set_shortname( "dsm" )
    set_description( N_("libdsm SMB input") )
    set_help(BDSM_HELP)
    set_capability( "access", 20 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )
    add_string( "smb-user", NULL, USER_TEXT, USER_LONGTEXT, false )
    add_password( "smb-pwd", NULL, PASS_TEXT, PASS_LONGTEXT, false )
    add_string( "smb-domain", NULL, DOMAIN_TEXT, DOMAIN_LONGTEXT, false )
    add_shortcut( "smb", "cifs" )
    set_callbacks( Open, Close )

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
static ssize_t Read( access_t *, uint8_t *, size_t );
static int Seek( access_t *, uint64_t );
static int Control( access_t *, int, va_list );
static int BrowserInit( access_t *p_access );

static void split_domain_login( char **psz_login, char **psz_domain );
static void get_credentials( access_t *p_access );
static int get_address( access_t *p_access );
static void login_dialog( access_t *p_access );
static int login( access_t *p_access );
static void backslash_path( vlc_url_t *p_url );
static bool get_path( access_t *p_access );
static input_item_t* new_item( access_t *p_access, const char *psz_name, int i_type );

struct access_sys_t
{
    netbios_ns         *p_ns;               /**< Netbios name service */
    smb_session        *p_session;          /**< bdsm SMB Session object */
    smb_creds           creds;              /**< Credentials used to connect */

    vlc_url_t           url;
    char               *psz_share;
    char               *psz_path;

    char                netbios_name[16];
    struct in_addr      addr;

    smb_fd              i_fd;               /**< SMB fd for the file we're reading */
    smb_tid             i_tid;              /**< SMB Tree ID we're connected to */

    size_t              i_browse_count;
    size_t              i_browse_idx;
    smb_share_list      shares;
    smb_stat_list       files;
};

/*****************************************************************************
 * Dialog strings
 *****************************************************************************/
#define BDSM_LOGIN_DIALOG_TITLE N_( "%s: Authentication required" )
#define BDSM_LOGIN_DIALOG_TEXT N_( "The computer you are trying to connect "   \
    "to requires authentication.\n Please provide a username (and ideally a "  \
    "domain name using the format DOMAIN\\username)\n and a password." )

/*****************************************************************************
 * Open: Initialize module's data structures and libdsm
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys;
    smb_stat st;

    /* Init p_access */
    access_InitFields( p_access );
    p_sys = p_access->p_sys = (access_sys_t*)calloc( 1, sizeof( access_sys_t ) );
    if( p_access->p_sys == NULL )
        return VLC_ENOMEM;

    p_sys->p_ns = netbios_ns_new();
    if( p_sys->p_ns == NULL )
        goto error;

    p_sys->p_session = smb_session_new();
    if( p_sys->p_session == NULL )
        goto error;

    vlc_UrlParse( &p_sys->url, p_access->psz_location, 0 );
    get_credentials( p_access );
    if( get_address( p_access ) != VLC_SUCCESS )
        goto error;

    msg_Dbg( p_access, "Creds: username = %s, domain = %s",
             p_sys->creds.login, p_sys->creds.domain );
    msg_Dbg( p_access, "Session: Host name = %s, ip = %s", p_sys->netbios_name,
             inet_ntoa( p_sys->addr ) );

    /* Now that we have the required data, let's establish a session */
    if( !smb_session_connect( p_sys->p_session, p_sys->netbios_name,
                              p_sys->addr.s_addr, SMB_TRANSPORT_TCP ) )
    {
        msg_Err( p_access, "Unable to connect/negotiate SMB session");
        goto error;
    }

    get_path( p_access );

    if( login( p_access ) != VLC_SUCCESS )
    {
        msg_Err( p_access, "Unable to connect to share %s", p_sys->psz_share );
        goto error;
    }

    /* If there is no shares, browse them */
    if( !p_sys->psz_share )
        return BrowserInit( p_access );

    msg_Dbg( p_access, "Path: Share name = %s, path = %s", p_sys->psz_share,
             p_sys->psz_path );

    /* Let's finally ask a handle to the file we wanna read ! */
    p_sys->i_fd = smb_fopen( p_sys->p_session, p_sys->i_tid, p_sys->psz_path,
                             SMB_MOD_RO );
    if( !p_sys->i_fd )
    {
        msg_Err( p_access, "Unable to open file with path %s (in share %s)",
                 p_sys->psz_path, p_sys->psz_share );
        goto error;
    }

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
        return VLC_EGENERIC;
}

/*****************************************************************************
 * Close: free unused data structures
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys = p_access->p_sys;

    if( p_sys->p_ns )
        netbios_ns_destroy( p_sys->p_ns );
    if( p_sys->i_fd )
        smb_fclose( p_sys->p_session, p_sys->i_fd );
    if( p_sys->p_session )
        smb_session_destroy( p_sys->p_session );
    vlc_UrlClean( &p_sys->url );
    if( p_sys->shares )
        smb_share_list_destroy( p_sys->shares );
    if( p_sys->files )
        smb_stat_list_destroy( p_sys->files );
    free( p_sys->creds.login );
    free( p_sys->creds.password );
    free( p_sys->creds.domain );
    free( p_sys->psz_share );
    free( p_sys->psz_path );
    free( p_sys );
}

/*****************************************************************************
 * Local functions
 *****************************************************************************/

/* Split DOMAIN\User if it finds a '\' in psz_login. */
static void split_domain_login( char **psz_login, char **psz_domain )
{
    char *user = strchr( *psz_login, '\\' );

    if( user != NULL )
    {
        *psz_domain = *psz_login;
        *user = '\0';
        *psz_login = strdup( user + 1 );
    }
}

/* Get credentials from uri or variables. */
static void get_credentials( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;

    /* Fetch credentials, either from URI or from options if not provided */
    if( p_sys->url.psz_password == NULL )
        p_sys->creds.password = var_InheritString( p_access, "smb-pwd" );
    else
        p_sys->creds.password = strdup( p_sys->url.psz_password );

    /* Here we support smb://DOMAIN\User:password@XXX, get user from options
       or default to "Guest" as last resort */
    if( p_sys->url.psz_username != NULL )
    {
        p_sys->creds.login = strdup( p_sys->url.psz_username );
        split_domain_login( &p_sys->creds.login, &p_sys->creds.domain );
    }
    else
        p_sys->creds.login = var_InheritString( p_access, "smb-user" );

    if( p_sys->creds.domain == NULL )
        p_sys->creds.domain = var_InheritString( p_access, "smb-domain" );
}

/* Returns VLC_EGENERIC if it wasn't able to get an ip address to connect to */
static int get_address( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;

    if( !inet_aton( p_sys->url.psz_host, &p_sys->addr ) )
    {
        /* This is not an ip address, let's try netbios/dns resolve */
        struct addrinfo *p_info = NULL;

        /* Is this a netbios name on this LAN ? */
        if( netbios_ns_resolve( p_sys->p_ns, p_sys->url.psz_host,
                                NETBIOS_FILESERVER,
                                &p_sys->addr.s_addr) )
        {
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
            freeaddrinfo( p_info );
            if( p_info->ai_family != AF_INET )
                return VLC_EGENERIC;
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

    /* If no domain was explicitly specified, let's use the machine name */
    if( p_sys->creds.domain == NULL && p_sys->netbios_name[0] )
        p_sys->creds.domain = strdup( p_sys->netbios_name );

    return VLC_SUCCESS;
}

/* Displays a dialog for the user to enter his/her credentials */
static void login_dialog( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;

    char *psz_login = NULL, *psz_pass = NULL, *psz_title;
    int i_ret;

    i_ret = asprintf( &psz_title, BDSM_LOGIN_DIALOG_TITLE, p_sys->netbios_name );
    if( i_ret != -1 )
        dialog_Login( p_access, &psz_login, &psz_pass, psz_title,
                      BDSM_LOGIN_DIALOG_TEXT );
    else
        dialog_Login( p_access, &psz_login, &psz_pass, BDSM_LOGIN_DIALOG_TITLE,
                      BDSM_LOGIN_DIALOG_TEXT );
    free( psz_title );

    if( psz_login != NULL )
    {
        free( p_sys->creds.login );
        p_sys->creds.login = psz_login;
        split_domain_login( &p_sys->creds.login, &p_sys->creds.domain );
    }

    if( psz_pass != NULL )
    {
        free( p_sys->creds.password );
        p_sys->creds.password = psz_pass;
    }
}

static int smb_connect( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    const char *psz_login = p_sys->creds.login ?
                            p_sys->creds.login : "Guest";
    const char *psz_password = p_sys->creds.password ?
                               p_sys->creds.password : "Guest";
    const char *psz_domain = p_sys->creds.domain ?
                             p_sys->creds.domain : p_sys->netbios_name;

    smb_session_set_creds( p_sys->p_session, psz_domain,
                           psz_login, psz_password );
    if( smb_session_login( p_sys->p_session ) )
    {
        if( p_sys->psz_share )
        {
            /* Connect to the share */
            p_sys->i_tid = smb_tree_connect( p_sys->p_session, p_sys->psz_share );
            if( !p_sys->i_tid )
                return VLC_EGENERIC;
        }
        return VLC_SUCCESS;
    }
    else
        return VLC_EGENERIC;
}

/* Performs login with existing credentials and ask the user for new ones on
   failure */
static int login( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;

    /* Try to authenticate on the remote machine */
    if( smb_connect( p_access ) != VLC_SUCCESS )
    {
        for( int i = 0; i < BDSM_LOGIN_DIALOG_RETRY; i++ )
        {
            login_dialog( p_access );
            if( smb_connect( p_access ) == VLC_SUCCESS )
                return VLC_SUCCESS;
        }

        msg_Err( p_access, "Unable to login with username = %s, domain = %s",
                   p_sys->creds.login, p_sys->creds.domain );
        return VLC_EGENERIC;
    }
    else if( smb_session_is_guest( p_sys->p_session )  )
        msg_Warn( p_access, "Login failure but you were logged in as a Guest");

    return VLC_SUCCESS;
}

static void backslash_path( vlc_url_t *p_url )
{
    char *iter = p_url->psz_path;

    /* Let's switch the path delimiters from / to \ */
    while( *iter != '\0' )
    {
        if( *iter == '/' )
            *iter = '\\';
        iter++;
    }
}

/* Get the share and filepath from uri (also replace all / by \ in url.psz_path) */
static bool get_path( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    char *iter;

    if( p_sys->url.psz_path == NULL )
        return false;

    backslash_path( &p_sys->url );

    /* Is path longer than just "/" ? */
    if( strlen( p_sys->url.psz_path ) > 1 )
    {
        iter = p_sys->url.psz_path;
        while( *iter == '\\' ) iter++; /* Handle smb://Host/////Share/ */

        p_sys->psz_share = strdup( iter );
        if ( p_sys->psz_share == NULL )
            return false;
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
        p_sys->psz_path = strdup( "" );

        msg_Dbg( p_access, "no file path provided, will switch to browser ");
        return true;
    }

    p_sys->psz_path = strdup( iter + 1); /* Skip the first \ */
    *iter = '\0';
    if( p_sys->psz_path == NULL )
        return false;

    return true;
}

/*****************************************************************************
 * Seek: try to go at the right place
 *****************************************************************************/
static int Seek( access_t *p_access, uint64_t i_pos )
{
    access_sys_t *p_sys = p_access->p_sys;
    int64_t      i_ret;

    if( i_pos >= INT64_MAX )
        return VLC_EGENERIC;

    msg_Dbg( p_access, "seeking to %"PRId64, i_pos );

    /* seek cannot fail in bdsm, but the subsequent read can */
    i_ret = smb_fseek(p_sys->p_session, p_sys->i_fd, i_pos, SMB_SEEK_SET);

    p_access->info.b_eof = false;
    p_access->info.i_pos = i_ret;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Read:
 *****************************************************************************/
static ssize_t Read( access_t *p_access, uint8_t *p_buffer, size_t i_len )
{
    access_sys_t *p_sys = p_access->p_sys;
    int i_read;

    if( p_access->info.b_eof ) return 0;

    i_read = smb_fread( p_sys->p_session, p_sys->i_fd, p_buffer, i_len );
    if( i_read < 0 )
    {
        msg_Err( p_access, "read failed" );
        return -1;
    }

    if( i_read == 0 ) p_access->info.b_eof = true;
    else if( i_read > 0 ) p_access->info.i_pos += i_read;

    return i_read;
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

    case ACCESS_GET_SIZE:
    {
        smb_stat st = smb_stat_fd( p_access->p_sys->p_session,
                                   p_access->p_sys->i_fd );
        *va_arg( args, uint64_t * ) = smb_stat_get( st, SMB_STAT_SIZE );
        break;
    }
    case ACCESS_GET_PTS_DELAY:
        *va_arg( args, int64_t * ) = INT64_C(1000)
            * var_InheritInteger( p_access, "network-caching" );
        break;

    case ACCESS_SET_PAUSE_STATE:
        /* Nothing to do */
        break;

    default:
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static input_item_t *new_item( access_t *p_access, const char *psz_name,
                               int i_type )
{
    access_sys_t *p_sys = p_access->p_sys;
    input_item_t *p_item;
    char         *psz_uri, *psz_option = NULL;
    int           i_ret;

    i_ret = asprintf( &psz_uri, "smb://%s/%s", p_access->psz_location, psz_name );
    if( i_ret == -1 )
        return NULL;

    p_item = input_item_NewWithTypeExt( psz_uri, psz_name, 0, NULL, 0, -1,
                                        i_type, 1 );
    free( psz_uri );
    if( p_item == NULL )
        return NULL;

    /* Here we save on the node the credentials that allowed us to login.
     * That way the user isn't prompted more than once for credentials */
    if( p_sys->creds.login )
    {
        i_ret = asprintf( &psz_option, "smb-user=%s", p_sys->creds.login );
        if( i_ret == -1 )
            goto bailout;
        input_item_AddOption( p_item, psz_option, VLC_INPUT_OPTION_TRUSTED );
        free( psz_option );
    }

    if( p_sys->creds.password )
    {
        i_ret = asprintf( &psz_option, "smb-pwd=%s", p_sys->creds.password );
        if( i_ret == -1 )
            goto bailout;
        input_item_AddOption( p_item, psz_option, VLC_INPUT_OPTION_TRUSTED );
        free( psz_option );
    }

    if( p_sys->creds.domain )
    {
        i_ret = asprintf( &psz_option, "smb-domain=%s", p_sys->creds.domain );
        if( i_ret == -1 )
            goto bailout;
        input_item_AddOption( p_item, psz_option, VLC_INPUT_OPTION_TRUSTED );
        free( psz_option );
    }

    return p_item;
bailout:
    if( p_item )
        input_item_Release( p_item );
    free( psz_option );
    return NULL;
}

static input_item_t* BrowseShare( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    const char     *psz_name;
    input_item_t   *p_item = NULL;

    if( !p_sys->i_browse_count )
        p_sys->i_browse_count = smb_share_get_list( p_sys->p_session,
                                                    &p_sys->shares );
    for( ; !p_item && p_sys->i_browse_idx < p_sys->i_browse_count
         ; p_sys->i_browse_idx++ )
    {
        psz_name = smb_share_list_at( p_sys->shares, p_sys->i_browse_idx );

        if( psz_name[strlen( psz_name ) - 1] == '$')
            continue;

        p_item = new_item( p_access, psz_name, ITEM_TYPE_DIRECTORY );
        if( !p_item )
            return NULL;
    }
    return p_item;
}

static input_item_t* BrowseDirectory( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    smb_stat        st;
    input_item_t   *p_item = NULL;
    char           *psz_query;
    const char     *psz_name;
    int             i_ret;

    if( !p_sys->i_browse_count )
    {
        if( p_sys->psz_path != NULL )
        {
            i_ret = asprintf( &psz_query, "%s\\*", p_sys->psz_path );
            if( i_ret == -1 )
                return NULL;
            p_sys->files = smb_find( p_sys->p_session, p_sys->i_tid, psz_query );
            free( psz_query );
        }
        else
            p_sys->files = smb_find( p_sys->p_session, p_sys->i_tid, "\\*" );
        if( p_sys->files == NULL )
            return NULL;
        p_sys->i_browse_count = smb_stat_list_count( p_sys->files );
    }

    if( p_sys->i_browse_idx < p_sys->i_browse_count )
    {
        int i_type;

        st = smb_stat_list_at( p_sys->files, p_sys->i_browse_idx++ );

        if( st == NULL )
            return NULL;

        psz_name = smb_stat_name( st );

        i_type = smb_stat_get( st, SMB_STAT_ISDIR ) ?
                 ITEM_TYPE_DIRECTORY : ITEM_TYPE_FILE;
        p_item = new_item( p_access, psz_name, i_type );
        if( !p_item )
            return NULL;
    }
    return p_item;
}

static int BrowserInit( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;

    if( p_sys->psz_share == NULL )
        p_access->pf_readdir = BrowseShare;
    else
    {
        p_access->pf_readdir = BrowseDirectory;
        p_access->info.b_dir_can_loop = true;
    }
    p_access->pf_control = access_vaDirectoryControlHelper;

    return VLC_SUCCESS;
}
