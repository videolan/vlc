/*****************************************************************************
 * rpn.c : RPN evaluator for the HTTP Interface
 *****************************************************************************
 * Copyright (C) 2001-2005 the VideoLAN team
 * $Id: http.c 12225 2005-08-18 10:01:30Z massiot $
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#include "http.h"

void SSInit( rpn_stack_t *st )
{
    st->i_stack = 0;
}

void SSClean( rpn_stack_t *st )
{
    while( st->i_stack > 0 )
    {
        free( st->stack[--st->i_stack] );
    }
}

void SSPush( rpn_stack_t *st, const char *s )
{
    if( st->i_stack < STACK_MAX )
    {
        st->stack[st->i_stack++] = strdup( s );
    }
}

char * SSPop( rpn_stack_t *st )
{
    if( st->i_stack <= 0 )
    {
        return strdup( "" );
    }
    else
    {
        return st->stack[--st->i_stack];
    }
}

int SSPopN( rpn_stack_t *st, mvar_t  *vars )
{
    char *name;
    char *value;

    char *end;
    int  i;

    name = SSPop( st );
    i = strtol( name, &end, 0 );
    if( end == name )
    {
        value = mvar_GetValue( vars, name );
        i = atoi( value );
    }
    free( name );

    return( i );
}

void SSPushN( rpn_stack_t *st, int i )
{
    char v[512];

    sprintf( v, "%d", i );
    SSPush( st, v );
}

void  EvaluateRPN( intf_thread_t *p_intf, mvar_t  *vars,
                   rpn_stack_t *st, char *exp )
{
    intf_sys_t    *p_sys = p_intf->p_sys;

    while( exp != NULL && *exp != '\0' )
    {
        char *p, *s;

        /* skip space */
        while( *exp == ' ' )
        {
            exp++;
        }

        if( *exp == '\'' )
        {
            /* extract string */
            p = E_(FirstWord)( exp, exp );
            SSPush( st, exp );
            exp = p;
            continue;
        }

        /* extract token */
        p = E_(FirstWord)( exp, exp );
        s = exp;
        if( p == NULL )
        {
            exp += strlen( exp );
        }
        else
        {
            exp = p;
        }

        if( *s == '\0' )
        {
            break;
        }

        /* 1. Integer function */
        if( !strcmp( s, "!" ) )
        {
            SSPushN( st, ~SSPopN( st, vars ) );
        }
        else if( !strcmp( s, "^" ) )
        {
            SSPushN( st, SSPopN( st, vars ) ^ SSPopN( st, vars ) );
        }
        else if( !strcmp( s, "&" ) )
        {
            SSPushN( st, SSPopN( st, vars ) & SSPopN( st, vars ) );
        }
        else if( !strcmp( s, "|" ) )
        {
            SSPushN( st, SSPopN( st, vars ) | SSPopN( st, vars ) );
        }
        else if( !strcmp( s, "+" ) )
        {
            SSPushN( st, SSPopN( st, vars ) + SSPopN( st, vars ) );
        }
        else if( !strcmp( s, "-" ) )
        {
            int j = SSPopN( st, vars );
            int i = SSPopN( st, vars );
            SSPushN( st, i - j );
        }
        else if( !strcmp( s, "*" ) )
        {
            SSPushN( st, SSPopN( st, vars ) * SSPopN( st, vars ) );
        }
        else if( !strcmp( s, "/" ) )
        {
            int i, j;

            j = SSPopN( st, vars );
            i = SSPopN( st, vars );

            SSPushN( st, j != 0 ? i / j : 0 );
        }
        else if( !strcmp( s, "%" ) )
        {
            int i, j;

            j = SSPopN( st, vars );
            i = SSPopN( st, vars );

            SSPushN( st, j != 0 ? i % j : 0 );
        }
        /* 2. integer tests */
        else if( !strcmp( s, "=" ) )
        {
            SSPushN( st, SSPopN( st, vars ) == SSPopN( st, vars ) ? -1 : 0 );
        }
        else if( !strcmp( s, "!=" ) )
        {
            SSPushN( st, SSPopN( st, vars ) != SSPopN( st, vars ) ? -1 : 0 );
        }
        else if( !strcmp( s, "<" ) )
        {
            int j = SSPopN( st, vars );
            int i = SSPopN( st, vars );

            SSPushN( st, i < j ? -1 : 0 );
        }
        else if( !strcmp( s, ">" ) )
        {
            int j = SSPopN( st, vars );
            int i = SSPopN( st, vars );

            SSPushN( st, i > j ? -1 : 0 );
        }
        else if( !strcmp( s, "<=" ) )
        {
            int j = SSPopN( st, vars );
            int i = SSPopN( st, vars );

            SSPushN( st, i <= j ? -1 : 0 );
        }
        else if( !strcmp( s, ">=" ) )
        {
            int j = SSPopN( st, vars );
            int i = SSPopN( st, vars );

            SSPushN( st, i >= j ? -1 : 0 );
        }
        /* 3. string functions */
        else if( !strcmp( s, "strcat" ) )
        {
            char *s2 = SSPop( st );
            char *s1 = SSPop( st );
            char *str = malloc( strlen( s1 ) + strlen( s2 ) + 1 );

            strcpy( str, s1 );
            strcat( str, s2 );

            SSPush( st, str );
            free( s1 );
            free( s2 );
            free( str );
        }
        else if( !strcmp( s, "strcmp" ) )
        {
            char *s2 = SSPop( st );
            char *s1 = SSPop( st );

            SSPushN( st, strcmp( s1, s2 ) );
            free( s1 );
            free( s2 );
        }
        else if( !strcmp( s, "strncmp" ) )
        {
            int n = SSPopN( st, vars );
            char *s2 = SSPop( st );
            char *s1 = SSPop( st );

            SSPushN( st, strncmp( s1, s2 , n ) );
            free( s1 );
            free( s2 );
        }
        else if( !strcmp( s, "strsub" ) )
        {
            int n = SSPopN( st, vars );
            int m = SSPopN( st, vars );
            int i_len;
            char *s = SSPop( st );
            char *str;

            if( n >= m )
            {
                i_len = n - m + 1;
            }
            else
            {
                i_len = 0;
            }

            str = malloc( i_len + 1 );

            memcpy( str, s + m - 1, i_len );
            str[ i_len ] = '\0';

            SSPush( st, str );
            free( s );
            free( str );
        }
        else if( !strcmp( s, "strlen" ) )
        {
            char *str = SSPop( st );

            SSPushN( st, strlen( str ) );
            free( str );
        }
        else if( !strcmp( s, "str_replace" ) )
        {
            char *psz_to = SSPop( st );
            char *psz_from = SSPop( st );
            char *psz_in = SSPop( st );
            char *psz_in_current = psz_in;
            char *psz_out = malloc( strlen(psz_in) * strlen(psz_to) + 1 );
            char *psz_out_current = psz_out;

            while( (p = strstr( psz_in_current, psz_from )) != NULL )
            {
                memcpy( psz_out_current, psz_in_current, p - psz_in_current );
                psz_out_current += p - psz_in_current;
                strcpy( psz_out_current, psz_to );
                psz_out_current += strlen(psz_to);
                psz_in_current = p + strlen(psz_from);
            }
            strcpy( psz_out_current, psz_in_current );
            psz_out_current += strlen(psz_in_current);
            *psz_out_current = '\0';

            SSPush( st, psz_out );
            free( psz_to );
            free( psz_from );
            free( psz_in );
            free( psz_out );
        }
        else if( !strcmp( s, "url_extract" ) )
        {
            char *url = mvar_GetValue( vars, "url_value" );
            char *name = SSPop( st );
            char value[512];
            char *tmp;

            E_(uri_extract_value)( url, name, value, 512 );
            E_(uri_decode_url_encoded)( value );
            tmp = E_(FromUTF8)( p_intf, value );
            SSPush( st, tmp );
            free( tmp );
            free( name );
        }
        else if( !strcmp( s, "url_encode" ) )
        {
            char *url = SSPop( st );
            char *value;

            value = E_(ToUTF8)( p_intf, url );
            free( url );
            url = value;
            value = vlc_UrlEncode( url );
            free( url );
            SSPush( st, value );
            free( value );
        }
        else if( !strcmp( s, "addslashes" ) )
        {
            char *psz_src = SSPop( st );
            char *psz_dest;
            char *str = psz_src;

            p = psz_dest = malloc( strlen( str ) * 2 + 1 );

            while( *str != '\0' )
            {
                if( *str == '"' || *str == '\'' )
                {
                    *p++ = '\\';
                }
                *p++ = *str;
                str++;
            }
            *p = '\0';

            SSPush( st, psz_dest );
            free( psz_src );
            free( psz_dest );
        }
        else if( !strcmp( s, "stripslashes" ) )
        {
            char *psz_src = SSPop( st );
            char *psz_dest;

            p = psz_dest = strdup( psz_src );

            while( *psz_src != '\0' )
            {
                if( *psz_src == '\\' )
                {
                    *psz_src++;
                }
                *p++ = *psz_src;
                psz_src++;
            }
            *p = '\0';

            SSPush( st, psz_dest );
            free( psz_src );
            free( psz_dest );
        }
        else if( !strcmp( s, "htmlspecialchars" ) )
        {
            char *psz_src = SSPop( st );
            char *psz_dest;
            char *str = psz_src;

            p = psz_dest = malloc( strlen( str ) * 6 + 1 );

            while( *str != '\0' )
            {
                if( *str == '&' )
                {
                    strcpy( p, "&amp;" );
                    p += 5;
                }
                else if( *str == '\"' )
                {
                    strcpy( p, "&quot;" );
                    p += 6;
                }
                else if( *str == '\'' )
                {
                    strcpy( p, "&#039;" );
                    p += 6;
                }
                else if( *str == '<' )
                {
                    strcpy( p, "&lt;" );
                    p += 4;
                }
                else if( *str == '>' )
                {
                    strcpy( p, "&gt;" );
                    p += 4;
                }
                else
                {
                    *p++ = *str;
                }
                str++;
            }
            *p = '\0';

            SSPush( st, psz_dest );
            free( psz_src );
            free( psz_dest );
        }
        else if( !strcmp( s, "realpath" ) )
        {
            char dir[MAX_DIR_SIZE], *src;
            char *psz_src = SSPop( st );
            char *psz_dir = psz_src;
            char sep;

            /* convert all / to native separator */
#if defined( WIN32 )
            while( (p = strchr( psz_dir, '/' )) )
            {
                *p = '\\';
            }
            sep = '\\';
#else
            sep = '/';
#endif

            if( *psz_dir == '~' )
            {
                /* This is incomplete : we should also support the ~cmassiot/ syntax. */
                snprintf( dir, sizeof(dir), "%s/%s", p_intf->p_vlc->psz_homedir,
                          psz_dir + 1 );
                psz_dir = dir;
            }

            /* first fix all .. dir */
            p = src = psz_dir;
            while( *src )
            {
                if( src[0] == '.' && src[1] == '.' )
                {
                    src += 2;
                    if( p <= &psz_dir[1] )
                    {
                        continue;
                    }

                    p -= 2;

                    while( p > &psz_dir[1] && *p != sep )
                    {
                        p--;
                    }
                }
                else if( *src == sep )
                {
                    if( p > psz_dir && p[-1] == sep )
                    {
                        src++;
                    }
                    else
                    {
                        *p++ = *src++;
                    }
                }
                else
                {
                    do
                    {
                        *p++ = *src++;
                    } while( *src && *src != sep );
                }
            }
            if( p != psz_dir + 1 && p[-1] == '/' ) p--;
            *p = '\0';

            SSPush( st, psz_dir );
            free( psz_src );
        }
        /* 4. stack functions */
        else if( !strcmp( s, "dup" ) )
        {
            char *str = SSPop( st );
            SSPush( st, str );
            SSPush( st, str );
            free( str );
        }
        else if( !strcmp( s, "drop" ) )
        {
            char *str = SSPop( st );
            free( str );
        }
        else if( !strcmp( s, "swap" ) )
        {
            char *s1 = SSPop( st );
            char *s2 = SSPop( st );

            SSPush( st, s1 );
            SSPush( st, s2 );
            free( s1 );
            free( s2 );
        }
        else if( !strcmp( s, "flush" ) )
        {
            SSClean( st );
            SSInit( st );
        }
        else if( !strcmp( s, "store" ) )
        {
            char *value = SSPop( st );
            char *name  = SSPop( st );

            mvar_PushNewVar( vars, name, value );
            free( name );
            free( value );
        }
        else if( !strcmp( s, "value" ) )
        {
            char *name  = SSPop( st );
            char *value = mvar_GetValue( vars, name );

            SSPush( st, value );

            free( name );
        }
        /* 5. player control */
        else if( !strcmp( s, "vlc_play" ) )
        {
            int i_id = SSPopN( st, vars );
            int i_ret;

            i_ret = playlist_Control( p_sys->p_playlist, PLAYLIST_ITEMPLAY,
                                      playlist_ItemGetById( p_sys->p_playlist,
                                      i_id ) );
            msg_Dbg( p_intf, "requested playlist item: %i", i_id );
            SSPushN( st, i_ret );
        }
        else if( !strcmp( s, "vlc_stop" ) )
        {
            playlist_Control( p_sys->p_playlist, PLAYLIST_STOP );
            msg_Dbg( p_intf, "requested playlist stop" );
        }
        else if( !strcmp( s, "vlc_pause" ) )
        {
            playlist_Control( p_sys->p_playlist, PLAYLIST_PAUSE );
            msg_Dbg( p_intf, "requested playlist pause" );
        }
        else if( !strcmp( s, "vlc_next" ) )
        {
            playlist_Control( p_sys->p_playlist, PLAYLIST_SKIP, 1 );
            msg_Dbg( p_intf, "requested playlist next" );
        }
        else if( !strcmp( s, "vlc_previous" ) )
        {
            playlist_Control( p_sys->p_playlist, PLAYLIST_SKIP, -1 );
            msg_Dbg( p_intf, "requested playlist previous" );
        }
        else if( !strcmp( s, "vlc_seek" ) )
        {
            char *psz_value = SSPop( st );
            E_(Seek)( p_intf, psz_value );
            msg_Dbg( p_intf, "requested playlist seek: %s", psz_value );
            free( psz_value );
        }
        else if( !strcmp( s, "vlc_var_type" )
                  || !strcmp( s, "vlc_config_type" ) )
        {
            const char *psz_type = NULL;
            char *psz_variable = SSPop( st );
            vlc_object_t *p_object;
            int i_type;

            if( !strcmp( s, "vlc_var_type" ) )
            {
                p_object = VLC_OBJECT(p_sys->p_input);
                if( p_object != NULL )
                    i_type = var_Type( p_object, psz_variable );
            }
            else
            {
                p_object = VLC_OBJECT(p_intf);
                i_type = config_GetType( p_object, psz_variable );
            }

            if( p_object != NULL )
            {
                switch( i_type & VLC_VAR_TYPE )
                {
                case VLC_VAR_BOOL:
                    psz_type = "VLC_VAR_BOOL";
                    break;
                case VLC_VAR_INTEGER:
                    psz_type = "VLC_VAR_INTEGER";
                    break;
                case VLC_VAR_HOTKEY:
                    psz_type = "VLC_VAR_HOTKEY";
                    break;
                case VLC_VAR_STRING:
                    psz_type = "VLC_VAR_STRING";
                    break;
                case VLC_VAR_MODULE:
                    psz_type = "VLC_VAR_MODULE";
                    break;
                case VLC_VAR_FILE:
                    psz_type = "VLC_VAR_FILE";
                    break;
                case VLC_VAR_DIRECTORY:
                    psz_type = "VLC_VAR_DIRECTORY";
                    break;
                case VLC_VAR_VARIABLE:
                    psz_type = "VLC_VAR_VARIABLE";
                    break;
                case VLC_VAR_FLOAT:
                    psz_type = "VLC_VAR_FLOAT";
                    break;
                default:
                    psz_type = "UNDEFINED";
                }
            }
            else
                psz_type = "INVALID";

            SSPush( st, psz_type );
            free( psz_variable );
        }
        else if( !strcmp( s, "vlc_var_set" ) )
        {
            char *psz_variable = SSPop( st );

            if( p_sys->p_input != NULL )
            {
                vlc_bool_t b_error = VLC_FALSE;
                char *psz_value = NULL;
                vlc_value_t val;
                int i_type;

                i_type = var_Type( p_sys->p_input, psz_variable );

                switch( i_type & VLC_VAR_TYPE )
                {
                case VLC_VAR_BOOL:
                    val.b_bool = SSPopN( st, vars );
                    msg_Dbg( p_intf, "requested input var change: %s->%d",
                             psz_variable, val.b_bool );
                    break;
                case VLC_VAR_INTEGER:
                case VLC_VAR_HOTKEY:
                    val.i_int = SSPopN( st, vars );
                    msg_Dbg( p_intf, "requested input var change: %s->%d",
                             psz_variable, val.i_int );
                    break;
                case VLC_VAR_STRING:
                case VLC_VAR_MODULE:
                case VLC_VAR_FILE:
                case VLC_VAR_DIRECTORY:
                case VLC_VAR_VARIABLE:
                    val.psz_string = psz_value = SSPop( st );
                    msg_Dbg( p_intf, "requested input var change: %s->%s",
                             psz_variable, psz_value );
                    break;
                case VLC_VAR_FLOAT:
                    psz_value = SSPop( st );
                    val.f_float = atof( psz_value );
                    msg_Dbg( p_intf, "requested input var change: %s->%f",
                             psz_variable, val.f_float );
                    break;
                default:
                    msg_Warn( p_intf, "invalid variable type %d (%s)",
                              i_type & VLC_VAR_TYPE, psz_variable );
                    b_error = VLC_TRUE;
                }

                if( !b_error )
                    var_Set( p_sys->p_input, psz_variable, val );
                if( psz_value != NULL )
                    free( psz_value );
            }
            else
                msg_Warn( p_intf, "vlc_var_set called without an input" );
            free( psz_variable );
        }
        else if( !strcmp( s, "vlc_var_get" ) )
        {
            char *psz_variable = SSPop( st );

            if( p_sys->p_input != NULL )
            {
                vlc_value_t val;
                int i_type;

                i_type = var_Type( p_sys->p_input, psz_variable );
                var_Get( p_sys->p_input, psz_variable, &val );

                switch( i_type & VLC_VAR_TYPE )
                {
                case VLC_VAR_BOOL:
                    SSPushN( st, val.b_bool );
                    break;
                case VLC_VAR_INTEGER:
                case VLC_VAR_HOTKEY:
                    SSPushN( st, val.i_int );
                    break;
                case VLC_VAR_STRING:
                case VLC_VAR_MODULE:
                case VLC_VAR_FILE:
                case VLC_VAR_DIRECTORY:
                case VLC_VAR_VARIABLE:
                    SSPush( st, val.psz_string );
                    free( val.psz_string );
                    break;
                case VLC_VAR_FLOAT:
                {
                    char psz_value[20];
                    snprintf( psz_value, sizeof(psz_value), "%f", val.f_float );
                    SSPush( st, psz_value );
                    break;
                }
                default:
                    msg_Warn( p_intf, "invalid variable type %d (%s)",
                              i_type & VLC_VAR_TYPE, psz_variable );
                    SSPush( st, "" );
                }
            }
            else
            {
                msg_Warn( p_intf, "vlc_var_get called without an input" );
                SSPush( st, "" );
            }
            free( psz_variable );
        }
        else if( !strcmp( s, "vlc_config_set" ) )
        {
            char *psz_variable = SSPop( st );
            int i_type = config_GetType( p_intf, psz_variable );

            switch( i_type & VLC_VAR_TYPE )
            {
            case VLC_VAR_BOOL:
            case VLC_VAR_INTEGER:
                config_PutInt( p_intf, psz_variable, SSPopN( st, vars ) );
                break;
            case VLC_VAR_STRING:
            case VLC_VAR_MODULE:
            case VLC_VAR_FILE:
            case VLC_VAR_DIRECTORY:
            {
                char *psz_string = SSPop( st );
                config_PutPsz( p_intf, psz_variable, psz_string );
                free( psz_string );
                break;
            }
            case VLC_VAR_FLOAT:
            {
                char *psz_string = SSPop( st );
                config_PutFloat( p_intf, psz_variable, atof(psz_string) );
                free( psz_string );
                break;
            }
            default:
                msg_Warn( p_intf, "vlc_config_set called on unknown var (%s)",
                          psz_variable );
            }
            free( psz_variable );
        }
        else if( !strcmp( s, "vlc_config_get" ) )
        {
            char *psz_variable = SSPop( st );
            int i_type = config_GetType( p_intf, psz_variable );

            switch( i_type & VLC_VAR_TYPE )
            {
            case VLC_VAR_BOOL:
            case VLC_VAR_INTEGER:
                SSPushN( st, config_GetInt( p_intf, psz_variable ) );
                break;
            case VLC_VAR_STRING:
            case VLC_VAR_MODULE:
            case VLC_VAR_FILE:
            case VLC_VAR_DIRECTORY:
            {
                char *psz_string = config_GetPsz( p_intf, psz_variable );
                SSPush( st, psz_string );
                free( psz_string );
                break;
            }
            case VLC_VAR_FLOAT:
            {
                char psz_string[20];
                snprintf( psz_string, sizeof(psz_string), "%f",
                          config_GetFloat( p_intf, psz_variable ) );
                SSPush( st, psz_string );
                break;
            }
            default:
                msg_Warn( p_intf, "vlc_config_get called on unknown var (%s)",
                          psz_variable );
            }
            free( psz_variable );
        }
        else if( !strcmp( s, "vlc_config_save" ) )
        {
            char *psz_module = SSPop( st );
            int i_result;

            if( !*psz_module )
            {
                free( psz_module );
                psz_module = NULL;
            }
            i_result = config_SaveConfigFile( p_intf, psz_module );

            if( psz_module != NULL )
                free( psz_module );
            SSPushN( st, i_result );
        }
        else if( !strcmp( s, "vlc_config_reset" ) )
        {
            config_ResetAll( p_intf );
        }
        /* 6. playlist functions */
        else if( !strcmp( s, "playlist_add" ) )
        {
            char *psz_name = SSPop( st );
            char *mrl = SSPop( st );
            char *tmp;
            playlist_item_t *p_item;
            int i_id;

            tmp = E_(ToUTF8)( p_intf, psz_name );
            free( psz_name );
            psz_name = tmp;
            tmp = E_(ToUTF8)( p_intf, mrl );
            free( mrl );
            mrl = tmp;

            if( !*psz_name )
            {
                p_item = E_(MRLParse)( p_intf, mrl, mrl );
            }
            else
            {
                p_item = E_(MRLParse)( p_intf, mrl, psz_name );
            }

            if( p_item == NULL || p_item->input.psz_uri == NULL ||
                 !*p_item->input.psz_uri )
            {
                i_id = VLC_EGENERIC;
                msg_Dbg( p_intf, "invalid requested mrl: %s", mrl );
            }
            else
            {
                i_id = playlist_AddItem( p_sys->p_playlist, p_item,
                                         PLAYLIST_APPEND, PLAYLIST_END );
                msg_Dbg( p_intf, "requested mrl add: %s", mrl );
            }
            SSPushN( st, i_id );

            free( mrl );
            free( psz_name );
        }
        else if( !strcmp( s, "playlist_empty" ) )
        {
            playlist_LockClear( p_sys->p_playlist );
            msg_Dbg( p_intf, "requested playlist empty" );
        }
        else if( !strcmp( s, "playlist_delete" ) )
        {
            int i_id = SSPopN( st, vars );
            playlist_LockDelete( p_sys->p_playlist, i_id );
            msg_Dbg( p_intf, "requested playlist delete: %d", i_id );
        }
        else if( !strcmp( s, "playlist_move" ) )
        {
            int i_newpos = SSPopN( st, vars );
            int i_pos = SSPopN( st, vars );
            if ( i_pos < i_newpos )
            {
                playlist_Move( p_sys->p_playlist, i_pos, i_newpos + 1 );
            }
            else
            {
                playlist_Move( p_sys->p_playlist, i_pos, i_newpos );
            }
            msg_Dbg( p_intf, "requested to move playlist item %d to %d",
                     i_pos, i_newpos);
        }
        else
        {
            SSPush( st, s );
        }
    }
}
