/*****************************************************************************
 * vlmshell.c: VLM interface plugin
 *****************************************************************************
 * Copyright (C) 2000-2005 VLC authors and VideoLAN
 *
 * Authors: Simon Latapie <garf@videolan.org>
 *          Laurent Aimar <fenrir@videolan.org>
 *          Gildas Bazin <gbazin@videolan.org>
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
#include <vlc_arrays.h>

#include <stdio.h>
#include <ctype.h>                                              /* tolower() */
#include <assert.h>

#include <vlc_vlm.h>

#ifdef ENABLE_VLM

#include <time.h>                                                 /* ctime() */
#include <limits.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <vlc_stream.h>
#include "vlm_internal.h"
#include <vlc_charset.h>
#include <vlc_fs.h>
#include <vlc_sout.h>
#include <vlc_memstream.h>

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/

/* */
static vlm_message_t *vlm_Show( vlm_t *, vlm_media_sys_t *, const char * );

VLC_MALLOC static char *Save( vlm_t * );
static int Load( vlm_t *, char * );

/* */
static vlm_media_sys_t *vlm_MediaSearch( vlm_t *, const char *);

static const char quotes[] = "\"'";
/**
 * FindCommandEnd: look for the end of a possibly quoted string
 * @return NULL on mal-formatted string,
 * pointer past the last character otherwise.
 */
static const char *FindCommandEnd( const char *psz_sent )
{
    unsigned char c, quote = 0;

    while( (c = *psz_sent) != '\0' )
    {
        if( !quote )
        {
            if( strchr(quotes,c) )   // opening quote
                quote = c;
            else if( isspace(c) )         // non-escaped space
                return psz_sent;
            else if( c == '\\' )
            {
                psz_sent++;         // skip escaped character
                if( *psz_sent == '\0' )
                    return psz_sent;
            }
        }
        else
        {
            if( c == quote )         // non-escaped matching quote
                quote = 0;
            else if( (quote == '"') && (c == '\\') )
            {
                psz_sent++;         // skip escaped character
                if (*psz_sent == '\0')
                    return NULL;    // error, closing quote missing
            }
        }
        psz_sent++;
    }

    // error (NULL) if we could not find a matching quote
    return quote ? NULL : psz_sent;
}


/**
 * Unescape a nul-terminated string.
 * Note that in and out can be identical.
 *
 * @param out output buffer (at least <strlen (in) + 1> characters long)
 * @param in nul-terminated string to be unescaped
 *
 * @return 0 on success, -1 on error.
 */
static int Unescape( char *out, const char *in )
{
    unsigned char c, quote = 0;
    bool param = false;

    while( (c = *in++) != '\0' )
    {
        // Don't escape the end of the string if we find a '#'
        // that's the beginning of a vlc command
        // TODO: find a better solution
        if( ( c == '#' && !quote ) || param )
        {
            param = true;
            *out++ = c;
            continue;
        }

        if( !quote )
        {
            if (strchr(quotes,c))   // opening quote
            {
                quote = c;
                continue;
            }
            else if( c == '\\' )
            {
                switch (c = *in++)
                {
                    case '"':
                    case '\'':
                    case '\\':
                        *out++ = c;
                        continue;

                    case '\0':
                        *out = '\0';
                        return 0;
                }
                if( isspace(c) )
                {
                    *out++ = c;
                    continue;
                }
                /* None of the special cases - copy the backslash */
                *out++ = '\\';
            }
        }
        else
        {
            if( c == quote )         // non-escaped matching quote
            {
                quote = 0;
                continue;
            }
            if( (quote == '"') && (c == '\\') )
            {
                switch( c = *in++ )
                {
                    case '"':
                    case '\\':
                        *out++ = c;
                        continue;

                    case '\0':   // should never happen
                        *out = '\0';
                        return -1;
                }
                /* None of the special cases - copy the backslash */
                *out++ = '\\';
            }
        }
        *out++ = c;
    }

    *out = '\0';
    return 0;
}


/*****************************************************************************
 * ExecuteCommand: The main state machine
 *****************************************************************************
 * Execute a command which ends with '\0' (string)
 *****************************************************************************/
static int ExecuteSyntaxError( const char *psz_cmd, vlm_message_t **pp_status )
{
    *pp_status = vlm_MessageNew( psz_cmd, "Wrong command syntax" );
    return VLC_EGENERIC;
}

static bool ExecuteIsMedia( vlm_t *p_vlm, const char *psz_name )
{
    int64_t id;

    if( !psz_name || vlm_ControlInternal( p_vlm, VLM_GET_MEDIA_ID, psz_name, &id ) )
        return false;
    return true;
}

static int ExecuteDel( vlm_t *p_vlm, const char *psz_name, vlm_message_t **pp_status )
{
    vlm_media_sys_t *p_media;

    p_media = vlm_MediaSearch( p_vlm, psz_name );

    if( p_media != NULL )
    {
        vlm_ControlInternal( p_vlm, VLM_DEL_MEDIA, p_media->cfg.id );
    }
    else if( !strcmp(psz_name, "media") )
    {
        vlm_ControlInternal( p_vlm, VLM_CLEAR_MEDIAS );
    }
    else if( !strcmp(psz_name, "all") )
    {
        vlm_ControlInternal( p_vlm, VLM_CLEAR_MEDIAS );
    }
    else
    {
        *pp_status = vlm_MessageNew( "del", "%s: media unknown", psz_name );
        return VLC_EGENERIC;
    }

    *pp_status = vlm_MessageSimpleNew( "del" );
    return VLC_SUCCESS;
}

static int ExecuteShow( vlm_t *p_vlm, const char *psz_name, vlm_message_t **pp_status )
{
    vlm_media_sys_t *p_media;

    if( !psz_name )
    {
        *pp_status = vlm_Show( p_vlm, NULL, NULL );
        return VLC_SUCCESS;
    }

    p_media = vlm_MediaSearch( p_vlm, psz_name );

    if( p_media != NULL )
        *pp_status = vlm_Show( p_vlm, p_media, NULL );
    else
        *pp_status = vlm_Show( p_vlm, NULL, psz_name );

    return VLC_SUCCESS;
}

static int ExecuteHelp( vlm_message_t **pp_status )
{
    vlm_message_t *message_child;

#define MessageAdd( a ) \
        vlm_MessageAdd( *pp_status, vlm_MessageSimpleNew( a ) );
#define MessageAddChild( a ) \
        vlm_MessageAdd( message_child, vlm_MessageSimpleNew( a ) );

    *pp_status = vlm_MessageSimpleNew( "help" );

    message_child = MessageAdd( "Commands Syntax:" );
    MessageAddChild( "new (name) broadcast [properties]" );
    MessageAddChild( "setup (name) (properties)" );
    MessageAddChild( "show [(name)|media]" );
    MessageAddChild( "del (name)|all|media" );
    MessageAddChild( "control (name) [instance_name] (command)" );
    MessageAddChild( "save (config_file)" );
    MessageAddChild( "export" );
    MessageAddChild( "load (config_file)" );

    message_child = MessageAdd( "Media Proprieties Syntax:" );
    MessageAddChild( "input (input_name)" );
    MessageAddChild( "inputdel (input_name)|all" );
    MessageAddChild( "inputdeln input_number" );
    MessageAddChild( "output (output_name)" );
    MessageAddChild( "option (option_name)[=value]" );
    MessageAddChild( "enabled|disabled" );
    MessageAddChild( "loop|unloop" );

    message_child = MessageAdd( "Control Commands Syntax:" );
    MessageAddChild( "play [input_number]" );
    MessageAddChild( "pause" );
    MessageAddChild( "stop" );
    MessageAddChild( "seek [+-](percentage) | [+-](seconds)s | [+-](milliseconds)ms" );

    return VLC_SUCCESS;
}

static int ExecuteControl( vlm_t *p_vlm, const char *psz_name, const int i_arg, char ** ppsz_arg, vlm_message_t **pp_status )
{
    vlm_media_sys_t *p_media;
    const char *psz_control = NULL;
    const char *psz_instance = NULL;
    const char *psz_argument = NULL;
    int i_index;
    int i_result;

    if( !ExecuteIsMedia( p_vlm, psz_name ) )
    {
        *pp_status = vlm_MessageNew( "control", "%s: media unknown", psz_name );
        return VLC_EGENERIC;
    }

    assert( i_arg > 0 );

#define IS(txt) ( !strcmp( ppsz_arg[i_index], (txt) ) )
    i_index = 0;
    if( !IS("play") && !IS("stop") && !IS("pause") && !IS("seek") )
    {
        i_index = 1;
        psz_instance = ppsz_arg[0];

        if( i_index >= i_arg || ( !IS("play") && !IS("stop") && !IS("pause") && !IS("seek") ) )
            return ExecuteSyntaxError( "control", pp_status );
    }
#undef IS
    psz_control = ppsz_arg[i_index];

    if( i_index+1 < i_arg )
        psz_argument = ppsz_arg[i_index+1];

    p_media = vlm_MediaSearch( p_vlm, psz_name );
    assert( p_media );

    if( !strcmp( psz_control, "play" ) )
    {
        int i_input_index = 0;
        int i;

        if( ( psz_argument && sscanf(psz_argument, "%d", &i) == 1 ) && i > 0 && i-1 < p_media->cfg.i_input )
        {
            i_input_index = i-1;
        }
        else if( psz_argument )
        {
            int j;
            vlm_media_t *p_cfg = &p_media->cfg;
            for ( j=0; j < p_cfg->i_input; j++)
            {
                if( !strcmp( p_cfg->ppsz_input[j], psz_argument ) )
                {
                    i_input_index = j;
                    break;
                }
            }
        }

        i_result = vlm_ControlInternal( p_vlm, VLM_START_MEDIA_BROADCAST_INSTANCE, p_media->cfg.id, psz_instance, i_input_index );
    }
    else if( !strcmp( psz_control, "seek" ) )
    {
        if( psz_argument )
        {
            bool b_relative;
            if( psz_argument[0] == '+' || psz_argument[0] == '-' )
                b_relative = true;
            else
                b_relative = false;

            if( strstr( psz_argument, "ms" ) || strstr( psz_argument, "s" ) )
            {
                /* Time (ms or s) */
                int64_t i_new_time;

                if( strstr( psz_argument, "ms" ) )
                    i_new_time = INT64_C(1000) * atoi( psz_argument );
                else
                    i_new_time = INT64_C(1000000) * atoi( psz_argument );

                if( b_relative )
                {
                    int64_t i_time = 0;
                    vlm_ControlInternal( p_vlm, VLM_GET_MEDIA_INSTANCE_TIME, p_media->cfg.id, psz_instance, &i_time );
                    i_new_time += i_time;
                }
                if( i_new_time < 0 )
                    i_new_time = 0;
                i_result = vlm_ControlInternal( p_vlm, VLM_SET_MEDIA_INSTANCE_TIME, p_media->cfg.id, psz_instance, i_new_time );
            }
            else
            {
                /* Percent */
                double d_new_position = vlc_atof_c( psz_argument ) / 100.0;

                if( b_relative )
                {
                    double d_position = 0.0;

                    vlm_ControlInternal( p_vlm, VLM_GET_MEDIA_INSTANCE_POSITION, p_media->cfg.id, psz_instance, &d_position );
                    d_new_position += d_position;
                }
                if( d_new_position < 0.0 )
                    d_new_position = 0.0;
                else if( d_new_position > 1.0 )
                    d_new_position = 1.0;
                i_result = vlm_ControlInternal( p_vlm, VLM_SET_MEDIA_INSTANCE_POSITION, p_media->cfg.id, psz_instance, d_new_position );
            }
        }
        else
        {
            i_result = VLC_EGENERIC;
        }
    }
    else if( !strcmp( psz_control, "stop" ) )
    {
        i_result = vlm_ControlInternal( p_vlm, VLM_STOP_MEDIA_INSTANCE, p_media->cfg.id, psz_instance );
    }
    else if( !strcmp( psz_control, "pause" ) )
    {
        i_result = vlm_ControlInternal( p_vlm, VLM_PAUSE_MEDIA_INSTANCE, p_media->cfg.id, psz_instance );
    }
    else
    {
        i_result = VLC_EGENERIC;
    }

    if( i_result )
    {
        *pp_status = vlm_MessageNew( "control", "unknown error" );
        return VLC_SUCCESS;
    }
    *pp_status = vlm_MessageSimpleNew( "control" );
    return VLC_SUCCESS;
}

static int ExecuteExport( vlm_t *p_vlm, vlm_message_t **pp_status )
{
    char *psz_export = Save( p_vlm );

    *pp_status = vlm_MessageNew( "export", "%s", psz_export );
    free( psz_export );
    return VLC_SUCCESS;
}

static int ExecuteSave( vlm_t *p_vlm, const char *psz_file, vlm_message_t **pp_status )
{
    FILE *f = vlc_fopen( psz_file, "wt" );
    char *psz_save = NULL;

    if( !f )
        goto error;

    psz_save = Save( p_vlm );
    if( psz_save == NULL )
        goto error;
    if( fputs( psz_save, f ) == EOF )
        goto error;
    if( fclose( f ) )
    {
        f = NULL;
        goto error;
    }

    free( psz_save );

    *pp_status = vlm_MessageSimpleNew( "save" );
    return VLC_SUCCESS;

error:
    free( psz_save );
    if( f )
         fclose( f );
    *pp_status = vlm_MessageNew( "save", "Unable to save to file");
    return VLC_EGENERIC;
}

static int ExecuteLoad( vlm_t *p_vlm, const char *psz_path, vlm_message_t **pp_status )
{
    int fd = vlc_open( psz_path, O_RDONLY|O_NONBLOCK );
    if( fd == -1 )
    {
        *pp_status = vlm_MessageNew( "load", "Unable to load from file" );
        return VLC_EGENERIC;
    }

    struct stat st;
    char *psz_buffer = NULL;

    if( fstat( fd, &st ) || !S_ISREG( st.st_mode )
     || st.st_size >= SSIZE_MAX
     || ((psz_buffer = malloc( st.st_size + 1 )) == NULL)
     || read( fd, psz_buffer, st.st_size ) < (ssize_t)st.st_size )
    {
        free( psz_buffer );
        vlc_close( fd );

        *pp_status = vlm_MessageNew( "load", "Read file error" );
        return VLC_EGENERIC;
    }

    vlc_close( fd );

    psz_buffer[st.st_size] = '\0';

    if( Load( p_vlm, psz_buffer ) )
    {
        free( psz_buffer );

        *pp_status = vlm_MessageNew( "load", "Error while loading file" );
        return VLC_EGENERIC;
    }

    free( psz_buffer );

    *pp_status = vlm_MessageSimpleNew( "load" );
    return VLC_SUCCESS;
}

static int ExecuteMediaProperty( vlm_t *p_vlm, int64_t id, bool b_new,
                                 const int i_property, char *ppsz_property[], vlm_message_t **pp_status )
{
    const char *psz_cmd = b_new ? "new" : "setup";
    vlm_media_t *p_cfg = NULL;
    int i_result;
    int i;

#undef ERROR
#undef MISSING
#define ERROR( txt ) do { *pp_status = vlm_MessageNew( psz_cmd, txt); goto error; } while(0)
    if( vlm_ControlInternal( p_vlm, VLM_GET_MEDIA, id, &p_cfg ) )
        ERROR( "unknown media" );

#define MISSING(cmd) do { if( !psz_value ) ERROR( "missing argument for " cmd ); } while(0)
    for( i = 0; i < i_property; i++ )
    {
        const char *psz_option = ppsz_property[i];
        const char *psz_value = i+1 < i_property ? ppsz_property[i+1] :  NULL;

        if( !strcmp( psz_option, "enabled" ) )
        {
            p_cfg->b_enabled = true;
        }
        else if( !strcmp( psz_option, "disabled" ) )
        {
            p_cfg->b_enabled = false;
        }
        else if( !strcmp( psz_option, "input" ) )
        {
            MISSING( "input" );
            TAB_APPEND( p_cfg->i_input, p_cfg->ppsz_input, strdup(psz_value) );
            i++;
        }
        else if( !strcmp( psz_option, "inputdel" ) && psz_value && !strcmp( psz_value, "all" ) )
        {
            while( p_cfg->i_input > 0 )
                TAB_REMOVE( p_cfg->i_input, p_cfg->ppsz_input, p_cfg->ppsz_input[0] );
            i++;
        }
        else if( !strcmp( psz_option, "inputdel" ) )
        {
            int j;

            MISSING( "inputdel" );

            for( j = 0; j < p_cfg->i_input; j++ )
            {
                if( !strcmp( p_cfg->ppsz_input[j], psz_value ) )
                {
                    TAB_REMOVE( p_cfg->i_input, p_cfg->ppsz_input, p_cfg->ppsz_input[j] );
                    break;
                }
            }
            i++;
        }
        else if( !strcmp( psz_option, "inputdeln" ) )
        {
            MISSING( "inputdeln" );

            int idx = atoi( psz_value );
            if( idx > 0 && idx <= p_cfg->i_input )
                TAB_REMOVE( p_cfg->i_input, p_cfg->ppsz_input, p_cfg->ppsz_input[idx-1] );
            i++;
        }
        else if( !strcmp( psz_option, "output" ) )
        {
            MISSING( "output" );

            free( p_cfg->psz_output );
            p_cfg->psz_output = *psz_value ? strdup( psz_value ) : NULL;
            i++;
        }
        else if( !strcmp( psz_option, "option" ) )
        {
            MISSING( "option" );

            TAB_APPEND( p_cfg->i_option, p_cfg->ppsz_option, strdup( psz_value ) );
            i++;
        }
        else if( !strcmp( psz_option, "loop" ) )
        {
            p_cfg->broadcast.b_loop = true;
        }
        else if( !strcmp( psz_option, "unloop" ) )
        {
            p_cfg->broadcast.b_loop = false;
        }
        else
        {
            fprintf( stderr, "PROP: name=%s unknown\n", psz_option );
            ERROR( "Wrong command syntax" );
        }
    }
#undef MISSING
#undef ERROR

    /* */
    i_result = vlm_ControlInternal( p_vlm, VLM_CHANGE_MEDIA, p_cfg );
    vlm_media_Delete( p_cfg );

    *pp_status = vlm_MessageSimpleNew( psz_cmd );
    return i_result;

error:
    if( p_cfg )
    {
        if( b_new )
            vlm_ControlInternal( p_vlm, VLM_DEL_MEDIA, p_cfg->id );
        vlm_media_Delete( p_cfg );
    }
    return VLC_EGENERIC;
}

static int ExecuteNew( vlm_t *p_vlm, const char *psz_name, const char *psz_type, const int i_property, char *ppsz_property[], vlm_message_t **pp_status )
{
    /* Check name */
    if( !strcmp( psz_name, "all" ) || !strcmp( psz_name, "media" ) )
    {
        *pp_status = vlm_MessageNew( "new", "\"all\" and \"media\" are reserved names" );
        return VLC_EGENERIC;
    }
    if( ExecuteIsMedia( p_vlm, psz_name ) )
    {
        *pp_status = vlm_MessageNew( "new", "%s: Name already in use", psz_name );
        return VLC_EGENERIC;
    }
    /* */
    if( !strcmp( psz_type, "broadcast" ) )
    {
        vlm_media_t cfg;
        int64_t id;

        vlm_media_Init( &cfg );
        cfg.psz_name = strdup( psz_name );

        if( vlm_ControlInternal( p_vlm, VLM_ADD_MEDIA, &cfg, &id ) )
        {
            vlm_media_Clean( &cfg );
            *pp_status = vlm_MessageNew( "new", "could not create media" );
            return VLC_EGENERIC;
        }
        vlm_media_Clean( &cfg );
        return ExecuteMediaProperty( p_vlm, id, true, i_property, ppsz_property, pp_status );
    }
    else
    {
        *pp_status = vlm_MessageNew( "new", "%s: unsupported type", psz_type );
        return VLC_EGENERIC;
    }
}

static int ExecuteSetup( vlm_t *p_vlm, const char *psz_name, const int i_property, char *ppsz_property[], vlm_message_t **pp_status )
{
    if( ExecuteIsMedia( p_vlm, psz_name ) )
    {
        int64_t id;
        if( vlm_ControlInternal( p_vlm, VLM_GET_MEDIA_ID, psz_name, &id ) )
            goto error;
        return ExecuteMediaProperty( p_vlm, id, false, i_property, ppsz_property, pp_status );
    }

error:
    *pp_status = vlm_MessageNew( "setup", "%s unknown", psz_name );
    return VLC_EGENERIC;
}

int ExecuteCommand( vlm_t *p_vlm, const char *psz_command,
                           vlm_message_t **pp_message )
{
    size_t i_command = 0;
    size_t i_command_len = strlen( psz_command );
    char *buf = malloc( i_command_len + 1 ), *psz_buf = buf;
    size_t i_ppsz_command_len = (3 + (i_command_len + 1) / 2);
    char **ppsz_command = vlc_alloc( i_ppsz_command_len, sizeof(char *) );
    vlm_message_t *p_message = NULL;
    int i_ret = 0;

    if( !psz_buf || !ppsz_command )
    {
        p_message = vlm_MessageNew( "Memory error",
                        "allocation failed for command of length %zu",
                        i_command_len );
        goto error;
    }

    /* First, parse the line and cut it */
    while( *psz_command != '\0' )
    {
        const char *psz_temp;

        if(isspace ((unsigned char)*psz_command))
        {
            psz_command++;
            continue;
        }

        /* support for comments */
        if( i_command == 0 && *psz_command == '#')
        {
            p_message = vlm_MessageSimpleNew( "" );
            goto success;
        }

        psz_temp = FindCommandEnd( psz_command );

        if( psz_temp == NULL )
        {
            p_message = vlm_MessageNew( "Incomplete command", "%s", psz_command );
            goto error;
        }

        assert (i_command < i_ppsz_command_len);

        ppsz_command[i_command] = psz_buf;
        memcpy (psz_buf, psz_command, psz_temp - psz_command);
        psz_buf[psz_temp - psz_command] = '\0';

        Unescape (psz_buf, psz_buf);

        i_command++;
        psz_buf += psz_temp - psz_command + 1;
        psz_command = psz_temp;

        assert (buf + i_command_len + 1 >= psz_buf);
    }

    /*
     * And then Interpret it
     */

#define IF_EXECUTE( name, check, cmd ) if( !strcmp(ppsz_command[0], name ) ) { if( (check) ) goto syntax_error;  if( (cmd) ) goto error; goto success; }
    if( i_command == 0 )
    {
        p_message = vlm_MessageSimpleNew( "" );
        goto success;
    }
    else IF_EXECUTE( "del",     (i_command != 2),   ExecuteDel(p_vlm, ppsz_command[1], &p_message) )
    else IF_EXECUTE( "show",    (i_command > 2),    ExecuteShow(p_vlm, i_command > 1 ? ppsz_command[1] : NULL, &p_message) )
    else IF_EXECUTE( "help",    (i_command != 1),   ExecuteHelp( &p_message ) )
    else IF_EXECUTE( "control", (i_command < 3),    ExecuteControl(p_vlm, ppsz_command[1], i_command - 2, &ppsz_command[2], &p_message) )
    else IF_EXECUTE( "save",    (i_command != 2),   ExecuteSave(p_vlm, ppsz_command[1], &p_message) )
    else IF_EXECUTE( "export",  (i_command != 1),   ExecuteExport(p_vlm, &p_message) )
    else IF_EXECUTE( "load",    (i_command != 2),   ExecuteLoad(p_vlm, ppsz_command[1], &p_message) )
    else IF_EXECUTE( "new",     (i_command < 3),    ExecuteNew(p_vlm, ppsz_command[1], ppsz_command[2], i_command-3, &ppsz_command[3], &p_message) )
    else IF_EXECUTE( "setup",   (i_command < 2),    ExecuteSetup(p_vlm, ppsz_command[1], i_command-2, &ppsz_command[2], &p_message) )
    else
    {
        p_message = vlm_MessageNew( ppsz_command[0], "Unknown VLM command" );
        goto error;
    }
#undef IF_EXECUTE

success:
    *pp_message = p_message;
    free( buf );
    free( ppsz_command );
    return VLC_SUCCESS;

syntax_error:
    i_ret = ExecuteSyntaxError( ppsz_command[0], pp_message );
    free( buf );
    free( ppsz_command );
    return i_ret;

error:
    *pp_message = p_message;
    free( buf );
    free( ppsz_command );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Media handling
 *****************************************************************************/
vlm_media_sys_t *vlm_MediaSearch( vlm_t *vlm, const char *psz_name )
{
    int i;

    for( i = 0; i < vlm->i_media; i++ )
    {
        if( strcmp( psz_name, vlm->media[i]->cfg.psz_name ) == 0 )
            return vlm->media[i];
    }

    return NULL;
}

/*****************************************************************************
 * Message handling functions
 *****************************************************************************/
vlm_message_t *vlm_MessageSimpleNew( const char *psz_name )
{
    if( !psz_name ) return NULL;

    vlm_message_t *p_message = malloc( sizeof(*p_message) );
    if( !p_message )
        return NULL;

    p_message->psz_name = strdup( psz_name );
    if( !p_message->psz_name )
    {
        free( p_message );
        return NULL;
    }
    p_message->psz_value = NULL;
    p_message->i_child = 0;
    p_message->child = NULL;

    return p_message;
}

vlm_message_t *vlm_MessageNew( const char *psz_name,
                               const char *psz_format, ... )
{
    vlm_message_t *p_message = vlm_MessageSimpleNew( psz_name );
    va_list args;

    if( !p_message )
        return NULL;

    assert( psz_format );
    va_start( args, psz_format );
    if( vasprintf( &p_message->psz_value, psz_format, args ) == -1 )
        p_message->psz_value = NULL;
    va_end( args );

    if( !p_message->psz_value )
    {
        vlm_MessageDelete( p_message );
        return NULL;
    }
    return p_message;
}

void vlm_MessageDelete( vlm_message_t *p_message )
{
    free( p_message->psz_name );
    free( p_message->psz_value );
    while( p_message->i_child-- )
        vlm_MessageDelete( p_message->child[p_message->i_child] );
    free( p_message->child );
    free( p_message );
}

/* Add a child */
vlm_message_t *vlm_MessageAdd( vlm_message_t *p_message,
                               vlm_message_t *p_child )
{
    if( p_message == NULL ) return NULL;

    if( p_child )
    {
        TAB_APPEND( p_message->i_child, p_message->child, p_child );
    }

    return p_child;
}

/*****************************************************************************
 * Misc utility functions
 *****************************************************************************/
static vlm_message_t *vlm_ShowMedia( vlm_media_sys_t *p_media )
{
    vlm_media_t *p_cfg = &p_media->cfg;
    vlm_message_t *p_msg;
    vlm_message_t *p_msg_sub;
    int i;

    p_msg = vlm_MessageSimpleNew( p_cfg->psz_name );
    vlm_MessageAdd( p_msg,
                    vlm_MessageNew( "type", "broadcast" ) );
    vlm_MessageAdd( p_msg,
                    vlm_MessageNew( "enabled", p_cfg->b_enabled ? "yes" : "no" ) );

    vlm_MessageAdd( p_msg,
                    vlm_MessageNew( "loop", p_cfg->broadcast.b_loop ? "yes" : "no" ) );

    p_msg_sub = vlm_MessageAdd( p_msg, vlm_MessageSimpleNew( "inputs" ) );
    for( i = 0; i < p_cfg->i_input; i++ )
    {
        char *psz_tmp;
        if( asprintf( &psz_tmp, "%d", i+1 ) != -1 )
        {
            vlm_MessageAdd( p_msg_sub,
                       vlm_MessageNew( psz_tmp, "%s", p_cfg->ppsz_input[i] ) );
            free( psz_tmp );
        }
    }

    vlm_MessageAdd( p_msg,
                    vlm_MessageNew( "output", "%s", p_cfg->psz_output ? p_cfg->psz_output : "" ) );

    p_msg_sub = vlm_MessageAdd( p_msg, vlm_MessageSimpleNew( "options" ) );
    for( i = 0; i < p_cfg->i_option; i++ )
        vlm_MessageAdd( p_msg_sub, vlm_MessageSimpleNew( p_cfg->ppsz_option[i] ) );

    p_msg_sub = vlm_MessageAdd( p_msg, vlm_MessageSimpleNew( "instances" ) );
    for( i = 0; i < p_media->i_instance; i++ )
    {
        vlm_media_instance_sys_t *p_instance = p_media->instance[i];
        vlm_message_t *p_msg_instance;

        vlc_player_Lock(p_instance->player);
        enum vlc_player_state state = vlc_player_GetState(p_instance->player);
        float position = vlc_player_GetPosition(p_instance->player);
        vlc_tick_t time = vlc_player_GetTime(p_instance->player);
        vlc_tick_t length = vlc_player_GetLength(p_instance->player);
        float rate = vlc_player_GetRate(p_instance->player);
        ssize_t title = vlc_player_GetSelectedTitleIdx(p_instance->player);
        ssize_t chapter = vlc_player_GetSelectedChapterIdx(p_instance->player);
        bool can_seek = vlc_player_CanSeek(p_instance->player);
        vlc_player_Unlock(p_instance->player);

        p_msg_instance = vlm_MessageAdd( p_msg_sub, vlm_MessageSimpleNew( "instance" ) );

        vlm_MessageAdd( p_msg_instance,
                        vlm_MessageNew( "name" , "%s", p_instance->psz_name ? p_instance->psz_name : "default" ) );
        vlm_MessageAdd( p_msg_instance,
                        vlm_MessageNew( "state",
                            state == VLC_PLAYER_STATE_PLAYING ? "playing" :
                            state == VLC_PLAYER_STATE_PAUSED ? "paused" :
                            "stopped" ) );

        /* FIXME should not do that this way */
#define APPEND_INPUT_INFO( key, format, value ) \
        vlm_MessageAdd( p_msg_instance, vlm_MessageNew( key, format, value ) )
        APPEND_INPUT_INFO( "position", "%f", position );
        APPEND_INPUT_INFO( "time", "%"PRId64, time );
        APPEND_INPUT_INFO( "length", "%"PRId64, length );
        APPEND_INPUT_INFO( "rate", "%f", rate );
        APPEND_INPUT_INFO( "title", "%zd", title );
        APPEND_INPUT_INFO( "chapter", "%zd", chapter );
        APPEND_INPUT_INFO( "can-seek", "%d", can_seek );
#undef APPEND_INPUT_INFO
        vlm_MessageAdd( p_msg_instance, vlm_MessageNew( "playlistindex",
                        "%d", p_instance->i_index + 1 ) );
    }
    return p_msg;
}

static vlm_message_t *vlm_Show( vlm_t *vlm, vlm_media_sys_t *media,
                                const char *psz_filter )
{
    if( media != NULL )
    {
        vlm_message_t *p_msg = vlm_MessageSimpleNew( "show" );
        if( p_msg )
            vlm_MessageAdd( p_msg, vlm_ShowMedia( media ) );
        return p_msg;
    }

    else if( psz_filter && !strcmp( psz_filter, "media" ) )
    {
        vlm_message_t *p_msg;
        vlm_message_t *p_msg_child;
        int i_broadcast = vlm->i_media;

        p_msg = vlm_MessageSimpleNew( "show" );
        p_msg_child = vlm_MessageAdd( p_msg, vlm_MessageNew( "media",
                                      "( %d broadcast )", i_broadcast ) );

        for( int i = 0; i < vlm->i_media; i++ )
            vlm_MessageAdd( p_msg_child, vlm_ShowMedia( vlm->media[i] ) );

        return p_msg;
    }

    else if( ( psz_filter == NULL ) && ( media == NULL ) )
    {
        return vlm_Show( vlm, NULL, "media" );
    }

    else
    {
        return vlm_MessageSimpleNew( "show" );
    }
}

/*****************************************************************************
 * Config handling functions
 *****************************************************************************/
static int Load( vlm_t *vlm, char *file )
{
    char *pf = file;
    int  i_line = 1;

    while( *pf != '\0' )
    {
        vlm_message_t *message = NULL;
        int i_end = 0;

        while( pf[i_end] != '\n' && pf[i_end] != '\0' && pf[i_end] != '\r' )
        {
            i_end++;
        }

        if( pf[i_end] == '\r' || pf[i_end] == '\n' )
        {
            pf[i_end] = '\0';
            i_end++;
            if( pf[i_end] == '\n' ) i_end++;
        }

        if( *pf && ExecuteCommand( vlm, pf, &message ) )
        {
            if( message )
            {
                if( message->psz_value )
                    msg_Err( vlm, "Load error on line %d: %s: %s",
                             i_line, message->psz_name, message->psz_value );
                vlm_MessageDelete( message );
            }
            return 1;
        }
        if( message ) vlm_MessageDelete( message );

        pf += i_end;
        i_line++;
    }

    return 0;
}

VLC_MALLOC static char *Save( vlm_t *vlm )
{
    const char *psz_header = "\n"
                             "# VLC media player VLM command batch\n"
                             "# https://www.videolan.org/vlc/\n\n" ;

    struct vlc_memstream stream;

    vlc_memstream_open( &stream );
    vlc_memstream_puts( &stream, psz_header );

    for( int i = 0; i < vlm->i_media; i++ )
    {
        vlm_media_sys_t *media = vlm->media[i];
        vlm_media_t *p_cfg = &media->cfg;

        vlc_memstream_printf( &stream, "new %s broadcast %sabled",
                              p_cfg->psz_name,
                              p_cfg->b_enabled ? "en" : "dis" );

        if( p_cfg->broadcast.b_loop )
            vlc_memstream_puts( &stream, " loop" );
        vlc_memstream_putc( &stream, '\n' );

        for( int j = 0; j < p_cfg->i_input; j++ )
            vlc_memstream_printf( &stream, "setup %s input \"%s\"\n",
                                  p_cfg->psz_name, p_cfg->ppsz_input[j] );

        if( p_cfg->psz_output )
            vlc_memstream_printf( &stream, "setup %s output %s\n",
                                  p_cfg->psz_name, p_cfg->psz_output );

        for( int j = 0; j < p_cfg->i_option; j++ )
            vlc_memstream_printf( &stream, "setup %s option %s\n",
                                  p_cfg->psz_name, p_cfg->ppsz_option[j] );
    }

    if( vlc_memstream_close( &stream ) )
        return NULL;
    return stream.ptr;
}

#endif /* ENABLE_VLM */
