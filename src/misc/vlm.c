/*****************************************************************************
 * vlm.c: VLM interface plugin
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id$
 *
 * Authors: Simon Latapie <garf@videolan.org>
 *          Laurent Aimar <fenrir@videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/input.h>

#ifdef HAVE_TIME_H
#   include <time.h>                                              /* ctime() */
#endif

#include "vlc_vlm.h"

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static char          *vlm_Save( vlm_t * );
static int            vlm_Load( vlm_t *, char *);
static vlm_message_t *vlm_Show( vlm_t *, vlm_media_t *, vlm_schedule_t *, char * );
static vlm_message_t *vlm_Help( vlm_t *, char * );

static vlm_media_t *vlm_MediaNew    ( vlm_t *, char *, int );
static int          vlm_MediaDelete ( vlm_t *, vlm_media_t *, char * );
static vlm_media_t *vlm_MediaSearch ( vlm_t *, char * );
static int          vlm_MediaSetup  ( vlm_media_t *, char *, char * );
static int          vlm_MediaControl( vlm_t *, vlm_media_t *, char *, char * );

static vlm_message_t* vlm_MessageNew( char * , char * );
static vlm_message_t* vlm_MessageAdd( vlm_message_t*, vlm_message_t* );

static vlm_schedule_t *vlm_ScheduleNew( vlm_t *, char *);
static int             vlm_ScheduleDelete( vlm_t *, vlm_schedule_t *, char *);
static int             vlm_ScheduleSetup( vlm_schedule_t *, char *, char *);
static vlm_schedule_t *vlm_ScheduleSearch( vlm_t *, char *);


static int ExecuteCommand( vlm_t *, char * , vlm_message_t **);

static int Manage( vlc_object_t* );

/*****************************************************************************
 * vlm_New:
 *****************************************************************************/
vlm_t *__vlm_New ( vlc_object_t *p_this )
{
    vlc_value_t lockval;
    vlm_t *vlm = NULL;

    /* to be sure to avoid multiple creation */
    var_Create( p_this->p_libvlc, "vlm_mutex", VLC_VAR_MUTEX );
    var_Get( p_this->p_libvlc, "vlm_mutex", &lockval );
    vlc_mutex_lock( lockval.p_address );

    if( !(vlm = vlc_object_find( p_this, VLC_OBJECT_VLM, FIND_ANYWHERE )) )
    {
        msg_Info( p_this, "creating vlm" );
        if( ( vlm = vlc_object_create( p_this, VLC_OBJECT_VLM ) ) == NULL )
        {
            vlc_mutex_unlock( lockval.p_address );
            return NULL;
        }

        vlc_mutex_init( p_this->p_vlc, &vlm->lock );
        vlm->i_media      = 0;
        vlm->media        = NULL;
        vlm->i_schedule   = 0;
        vlm->schedule     = NULL;

        vlc_object_yield( vlm );
        vlc_object_attach( vlm, p_this->p_vlc );
    }
    vlc_mutex_unlock( lockval.p_address );


    if( vlc_thread_create( vlm, "vlm thread",
                           Manage, VLC_THREAD_PRIORITY_LOW, VLC_FALSE ) )
    {
        vlc_mutex_destroy( &vlm->lock );
        vlc_object_destroy( vlm );
        return NULL;
    }
    return vlm;
}

/*****************************************************************************
 * vlm_Delete:
 *****************************************************************************/
void vlm_Delete( vlm_t *vlm )
{
    vlc_value_t lockval;
    int i;

    var_Get( vlm->p_libvlc, "vlm_mutex", &lockval );
    vlc_mutex_lock( lockval.p_address );

    vlc_object_release( vlm );

    if( vlm->i_refcount > 0 )
    {
        vlc_mutex_unlock( lockval.p_address );
        return;
    }

    vlm->b_die = VLC_TRUE;
    vlc_thread_join( vlm );

    vlc_mutex_destroy( &vlm->lock );

    for( i = 0; i < vlm->i_media; i++ )
    {
        vlm_media_t *media = vlm->media[i];

        vlm_MediaDelete( vlm, media, NULL );
    }

    if( vlm->media ) free( vlm->media );

    for( i = 0; i < vlm->i_schedule; i++ )
    {
        vlm_ScheduleDelete( vlm, vlm->schedule[i], NULL );
    }

    if( vlm->schedule ) free( vlm->schedule );

    vlc_object_detach( vlm );
    vlc_object_destroy( vlm );
    vlc_mutex_unlock( lockval.p_address );
}

/*****************************************************************************
 * vlm_ExecuteCommand:
 *****************************************************************************/
int vlm_ExecuteCommand( vlm_t *vlm, char *command, vlm_message_t **message)
{
    int result;

    vlc_mutex_lock( &vlm->lock );
    result = ExecuteCommand( vlm, command, message );
    vlc_mutex_unlock( &vlm->lock );

    return result;
}


/*****************************************************************************
 *
 *****************************************************************************/
#if 1
static char *FindEndCommand( char *psz )
{
    char *s_sent = psz;

    switch( *s_sent )
    {
        case '\"':
        {
            s_sent++;

            while( ( *s_sent != '\"' ) && ( *s_sent != '\0' ) )
            {
                if( *s_sent == '\'' )
                {
                    s_sent = FindEndCommand( s_sent );

                    if( s_sent == NULL )
                    {
                        return NULL;
                    }
                }
                else
                {
                    s_sent++;
                }
            }

            if( *s_sent == '\"' )
            {
                s_sent++;
                return s_sent;
            }
            else  /* *s_sent == '\0' , which means the number of " is incorrect */
            {
                return NULL;
            }
            break;
        }
        case '\'':
        {
            s_sent++;

            while( ( *s_sent != '\'' ) && ( *s_sent != '\0' ) )
            {
                if( *s_sent == '\"' )
                {
                    s_sent = FindEndCommand( s_sent );

                    if( s_sent == NULL )
                    {
                        return NULL;
                    }
                }
                else
                {
                    s_sent++;
                }
            }

            if( *s_sent == '\'' )
            {
                s_sent++;
                return s_sent;
            }
            else  /* *s_sent == '\0' , which means the number of ' is incorrect */
            {
                return NULL;
            }
            break;
        }
        default: /* now we can look for spaces */
        {
            while( ( *s_sent != ' ' ) && ( *s_sent != '\0' ) )
            {
                if( ( *s_sent == '\'' ) || ( *s_sent == '\"' ) )
                {
                    s_sent = FindEndCommand( s_sent );
                }
                else
                {
                    s_sent++;
                }
            }
            return s_sent;
        }
    }
}
#else
static char *FindEndCommand( char *psz )
{
    char *s_sent = psz;

    while( *psz &&
            *psz != ' ' && *psz != '\t' &&
            *psz != '\n' && *psz != '\r' )
    {
        if( *psz == '\'' || *psz == '"' )
        {
            char d = *psz++;

            while( *psz && *psz != d && *psz != *psz != '\n' && *psz != '\r' )
            {
                if( ( d == '\'' && *psz == '"' ) ||
                    ( d == '"' && *psz == '\'' ) )
                {
                    psz = FindEndCommand( psz );
                }
            }
            if( psz != d )
            {
                return NULL;
            }
        }
        psz++;
    }
}
#endif


/* Execute a command which ends by '\0' (string) */
static int ExecuteCommand( vlm_t *vlm, char *command , vlm_message_t **p_message)
{
    int i_return = 0;
    int i_command = 0;
    char **p_command = NULL;
    char *cmd = command;
    int i;
    vlm_message_t * message = NULL;

    /* First, parse the line and cut it */
    while( *cmd != '\0' )
    {

        if( *cmd == ' ' )
        {
            cmd++;
        }
        else
        {
            char *p_temp;
            int   i_temp;

            p_temp = FindEndCommand( cmd );

            if( p_temp == NULL )
            {
                i_return = 1;
                goto end_seq;
            }

            i_temp = p_temp - cmd;

            p_command = realloc( p_command , (i_command + 1) * sizeof( char* ) );
            p_command[ i_command ] = malloc( (i_temp + 1) * sizeof( char ) ); // don't forget the '\0'
            strncpy( p_command[ i_command ] , cmd , i_temp );
            (p_command[ i_command ])[ i_temp ] = '\0';

            i_command++;
            cmd = p_temp;
        }
    }

    /* And then Interpret it */

    if( i_command == 0 )
    {
        message = vlm_MessageNew( "" , NULL );
        i_return = 0;
        goto end_seq;
    }

    if( strcmp(p_command[0] , "segfault") == 0 )
    {
        /* the only command we really need */
        *((int *)NULL) = 42;
    }
    else if( strcmp(p_command[0] , "new") == 0 )
    {
        if( i_command >= 3 )
        {
            int i_type;
            vlm_media_t *media;
            vlm_schedule_t *schedule;

            if( strcmp(p_command[2] , "schedule") == 0 )
            {
                /* new vlm_schedule */
                if( vlm_ScheduleSearch( vlm , p_command[1] ) != NULL || strcmp(p_command[1] , "schedule") == 0 )
                {
                    char *error_message = malloc( strlen(p_command[1]) + strlen(" is already used") + 1 );
                    sprintf( error_message, "%s is already used" , p_command[1] );
                    message = vlm_MessageNew( "new" , error_message );
                    free( error_message );
                    i_return = 1;
                    goto end_seq;
                }

                schedule = vlm_ScheduleNew( vlm , p_command[1] );


                if( i_command > 3 ) // hey, there are properties
                {
                    for( i = 3 ; i < i_command ; i++ )
                    {
                        if( strcmp( p_command[i] , "enabled" ) == 0 || strcmp( p_command[i] , "disabled" ) == 0 )
                        {
                            vlm_ScheduleSetup( schedule, p_command[i], NULL );
                        }
                        /* Beware: evrything behind append is considered as command line */
                        else if( strcmp( p_command[i] , "append" ) == 0 )
                        {
                            i++;

                            if( i < i_command )
                            {
                                int j;
                                for( j = (i + 1); j < i_command; j++ )
                                {
                                    p_command[i] = realloc( p_command[i], strlen(p_command[i]) + strlen(p_command[j]) + 1 + 1);
                                    strcat( p_command[i], " " );
                                    strcat( p_command[i], p_command[j] );
                                }

                                vlm_ScheduleSetup( schedule, p_command[i - 1], p_command[i] );
                            }
                            i = i_command;
                        }
                        else
                        {
                            if( (i+1) < i_command )
                            {
                                vlm_ScheduleSetup( schedule, p_command[i], p_command[i+1] );
                                i++;
                            }
                            else
                            {
                                vlm_ScheduleDelete( vlm, schedule, NULL );
                                message = vlm_MessageNew( p_command[i], "Wrong properties syntax" );
                                i_return = 1;
                                goto end_seq;
                            }
                        }
                    }

                    message = vlm_MessageNew( "new" , NULL );
                    i_return = 0;
                    goto end_seq;
                }

                message = vlm_MessageNew( "new" , NULL );
                i_return = 0;
                goto end_seq;
            }

            if( strcmp(p_command[2] , "vod") == 0 )
            {
                i_type = VOD_TYPE;
            }
            else if( strcmp(p_command[2] , "broadcast") == 0 )
            {
                i_type = BROADCAST_TYPE;
            }
            else
            {
                char *error_message = malloc( strlen(p_command[1]) + strlen(": Choose between vod or broadcast") + 1 );
                sprintf( error_message, "%s: Choose between vod or broadcast" , p_command[1] );
                message = vlm_MessageNew( "new" , error_message );
                free( error_message );
                i_return = 1;
                goto end_seq;
            }

            /* new vlm_media */
            if( vlm_MediaSearch( vlm , p_command[1] ) != NULL || strcmp(p_command[1] , "media") == 0 )
            {
                char *error_message = malloc( strlen(p_command[1]) + strlen(" is already used") + 1 );
                sprintf( error_message, "%s is already used" , p_command[1] );
                message = vlm_MessageNew( "new" , error_message );
                free( error_message );
                i_return = 1;
                goto end_seq;
            }

            media = vlm_MediaNew( vlm , p_command[1] , i_type );

            if( i_command > 3 ) // hey, there are properties
            {
                for( i = 3 ; i < i_command ; i++ )
                {
                    if( strcmp( p_command[i] , "enabled" ) == 0 || strcmp( p_command[i] , "disabled" ) == 0 )
                    {
                        vlm_MediaSetup( media, p_command[i], NULL );
                    }
                    else
                    {
                        if( (i+1) < i_command )
                        {
                            vlm_MediaSetup( media, p_command[i], p_command[i+1] );
                            i++;
                        }
                        else
                        {
                            vlm_MediaDelete( vlm, media, NULL );
                            message = vlm_MessageNew( p_command[i] , "Wrong properties syntax" );
                            i_return = 1;
                            goto end_seq;
                        }
                    }
                }

                message = vlm_MessageNew( "new" , NULL );
                i_return = 0;
                goto end_seq;
            }

            message = vlm_MessageNew( "new" , NULL );
            i_return = 0;
            goto end_seq;
        }
        else
        {
            message = vlm_MessageNew( "new" , "Wrong command syntax" );
            i_return = 1;
            goto end_seq;
        }
    }
    else if( strcmp(p_command[0] , "del") == 0 )
    {
        if( i_command >= 2 )
        {
            vlm_media_t *media;
            vlm_schedule_t *schedule;

            media = vlm_MediaSearch( vlm , p_command[1] );
            schedule = vlm_ScheduleSearch( vlm , p_command[1] );

            if( schedule != NULL )
            {
                vlm_ScheduleDelete( vlm, schedule, NULL );
                message = vlm_MessageNew( "del" , NULL );
                i_return = 0;
                goto end_seq;
            }
            else if( media != NULL )
            {
                vlm_MediaDelete( vlm, media, NULL );
                message = vlm_MessageNew( "del" , NULL );
                i_return = 0;
                goto end_seq;
            }
            else if( strcmp(p_command[1] , "media") == 0 )
            {
                for( i = 0; i < vlm->i_media; i++ )
                {
                    vlm_MediaDelete( vlm, vlm->media[i], NULL );
                }
            }
            else if( strcmp(p_command[1] , "schedule") == 0 )
            {
                for( i = 0; i < vlm->i_schedule; i++ )
                {
                    vlm_ScheduleDelete( vlm, vlm->schedule[i], NULL );
                }
            }
            else if( strcmp(p_command[1] , "all") == 0 )
            {
                for( i = 0; i < vlm->i_media; i++ )
                {
                    vlm_MediaDelete( vlm, vlm->media[i], NULL );
                }

                for( i = 0; i < vlm->i_schedule; i++ )
                {
                    vlm_ScheduleDelete( vlm, vlm->schedule[i], NULL );
                }
            }
            else
            {
                char *error_message = malloc( strlen(p_command[1]) + strlen(": media unknown") + 1 );
                sprintf( error_message, "%s: media unknown" , p_command[1] );
                message = vlm_MessageNew( "del" , error_message );
                free( error_message );
                i_return = 1;
                goto end_seq;
            }
        }
        else
        {
            message = vlm_MessageNew( "setup" , "Wrong command syntax" );
            i_return = 1;
            goto end_seq;
        }
    }
    else if( strcmp(p_command[0] , "show") == 0 )
    {
        if( i_command == 1 )
        {
            message = vlm_Show( vlm, NULL , NULL, NULL );
            i_return = 0;
            goto end_seq;
        }
        else if( i_command == 2 )
        {
            vlm_media_t *media;
            vlm_schedule_t *schedule;

            media = vlm_MediaSearch( vlm , p_command[1] );
            schedule = vlm_ScheduleSearch( vlm , p_command[1] );

            if( schedule != NULL )
            {
                message = vlm_Show( vlm, NULL, schedule, NULL );
            }
            else if( media != NULL )
            {
                message = vlm_Show( vlm, media, NULL, NULL );
            }
            else
            {
                message = vlm_Show( vlm, NULL, NULL, p_command[1] );
            }

            i_return = 0;
            goto end_seq;
        }
        else
        {
            message = vlm_MessageNew( "show" , "Wrong command syntax" );
            i_return = 1;
            goto end_seq;
        }
    }
    else if( strcmp(p_command[0] , "help") == 0 )
    {
        if( i_command == 1 )
        {
            message = vlm_Help( vlm, NULL );
            i_return = 0;
            goto end_seq;
        }
        else
        {
            message = vlm_MessageNew( "help" , "Wrong command syntax" );
            i_return = 1;
            goto end_seq;
        }
    }
    else if( strcmp(p_command[0] , "setup") == 0 )
    {
        if( i_command >= 2 )
        {
            vlm_media_t *media;
            vlm_schedule_t *schedule;

            media = vlm_MediaSearch( vlm , p_command[1] );
            schedule = vlm_ScheduleSearch( vlm , p_command[1] );

            if( schedule != NULL )
            {
                for( i = 2 ; i < i_command ; i++ )
                {
                    if( strcmp( p_command[i] , "enabled" ) == 0 || strcmp( p_command[i] , "disabled" ) == 0 )
                    {
                        vlm_ScheduleSetup( schedule, p_command[i], NULL );
                    }
                    /* Beware: evrything behind append is considered as command line */
                    else if( strcmp( p_command[i] , "append" ) == 0 )
                    {
                        i++;

                        if( i < i_command )
                        {
                            int j;
                            for( j = (i + 1); j < i_command; j++ )
                            {
                                p_command[i] = realloc( p_command[i], strlen(p_command[i]) + strlen(p_command[j]) + 1 + 1);
                                strcat( p_command[i], " " );
                                strcat( p_command[i], p_command[j] );
                            }

                            vlm_ScheduleSetup( schedule, p_command[i - 1], p_command[i] );
                        }
                        i = i_command;
                    }
                    else
                    {
                        if( (i+1) < i_command )
                        {
                            vlm_ScheduleSetup( schedule, p_command[i], p_command[i+1] );
                            i++;
                        }
                        else
                        {
                            vlm_ScheduleDelete( vlm, schedule, NULL );
                            message = vlm_MessageNew( "setup" , "Wrong properties syntax" );
                            i_return = 1;
                            goto end_seq;
                        }
                    }
                }

                message = vlm_MessageNew( "setup" , NULL );
                i_return = 0;
                goto end_seq;
            }
            else if( media != NULL )
            {
                for( i = 2 ; i < i_command ; i++ )
                {
                    if( strcmp( p_command[i] , "enabled" ) == 0 || strcmp( p_command[i] , "disabled" ) == 0 )
                    {   /* only one argument */
                        vlm_MediaSetup( media, p_command[i], NULL );
                    }
                    else if( strcmp( p_command[i] , "loop" ) == 0 || strcmp( p_command[i] , "unloop" ) == 0 )
                    {
                        if( media->i_type != BROADCAST_TYPE )
                        {
                            message = vlm_MessageNew( "setup" , "lool only available for broadcast" );
                            i_return = 1;
                            goto end_seq;
                        }
                        else
                        {
                            vlm_MediaSetup( media, p_command[i], NULL );
                        }
                    }
                    else
                    {
                        if( (i+1) < i_command )
                        {
                            vlm_MediaSetup( media, p_command[i], p_command[i+1] );
                            i++;
                        }
                        else
                        {
                            vlm_MediaDelete( vlm, media, NULL );
                            message = vlm_MessageNew( "setup" , "Wrong properties syntax" );
                            i_return = 1;
                            goto end_seq;
                        }
                    }
                }

                message = vlm_MessageNew( "setup" , NULL );
                i_return = 0;
                goto end_seq;
            }
            else
            {
                char *error_message = malloc( strlen(p_command[1]) + strlen(" unknown") + 1 );
                sprintf( error_message, "%s unknown" , p_command[1] );
                message = vlm_MessageNew( "setup" , error_message );
                free( error_message );
                i_return = 1;
                goto end_seq;
            }

        }
        else
        {
            message = vlm_MessageNew( "setup" , "Wrong command syntax" );
            i_return = 1;
            goto end_seq;
        }
    }
    else if( strcmp(p_command[0] , "control") == 0 )
    {

        if( i_command >= 3 )
        {
            vlm_media_t *media;

            media = vlm_MediaSearch( vlm , p_command[1] );

            if( media == NULL )
            {
                char *error_message = malloc( strlen(p_command[1]) + strlen(": media unknown") + 1 );
                sprintf( error_message, "%s: media unknown" , p_command[1] );
                message = vlm_MessageNew( "control" , error_message );
                free( error_message );
                i_return = 1;
                goto end_seq;
            }
            else
            {
                char *psz_args;

                if( i_command >= 4 )
                {
                    psz_args = p_command[3];
                }
                else
                {
                    psz_args = NULL;
                }

                /* for now */
                vlm_MediaControl( vlm, media, p_command[2], psz_args );
                message = vlm_MessageNew( "control" , NULL );
                i_return = 0;
                goto end_seq;
            }
        }
        else
        {
            message = vlm_MessageNew( "control" , "Wrong command syntax" );
            i_return = 1;
            goto end_seq;
        }
    }
    else if( strcmp(p_command[0] , "save") == 0 )
    {
        if( i_command == 2 )
        {
            FILE *file;


            file = fopen( p_command[1], "w" );

            if( file == NULL )
            {
                message = vlm_MessageNew( "save" , "Unable to save file" );
                i_return = 1;
                goto end_seq;
            }
            else
            {
                char *save;

                save = vlm_Save( vlm );

                fwrite( save, strlen( save ) , 1 , file );
                fclose( file );
                free( save );
                message = vlm_MessageNew( "save" , NULL );
                i_return = 0;
                goto end_seq;
            }
        }
        else
        {
            message = vlm_MessageNew( "save" , "Wrong command" );
            i_return = 1;
            goto end_seq;
        }
    }
    else if( strcmp(p_command[0] , "load") == 0 )
    {

        if( i_command == 2 )
        {
            FILE *file;

            file = fopen( p_command[1], "r" );

            if( file == NULL )
            {
                message = vlm_MessageNew( "load" , "Unable to load file" );
                i_return = 1;
                goto end_seq;
            }
            else
            {
                int64_t size;
                char *buffer;

                if( fseek( file, 0, SEEK_END) == 0 )
                {
                    size = ftell( file );
                    fseek( file, 0, SEEK_SET);
                    buffer = malloc( size + 1 );
                    fread( buffer, 1, size, file);
                    buffer[ size ] = '\0';
                    if( vlm_Load( vlm, buffer ) )
                    {
                        free( buffer );
                        message = vlm_MessageNew( "load" , "error while loading file" );
                        i_return = 1;
                        goto end_seq;
                    }
                    free( buffer );
                }
                else
                {
                    message = vlm_MessageNew( "load" , "read file error" );
                    i_return = 1;
                    goto end_seq;
                }
                fclose( file );
                message = vlm_MessageNew( "load" , NULL );
                i_return = 0;
                goto end_seq;
            }
        }
        else
        {
            message = vlm_MessageNew( "load" , "Wrong command" );
            i_return = 1;
            goto end_seq;
        }
    }
    else
    {
        message = vlm_MessageNew( p_command[0] , "Unknown comand" );
        i_return = 1;
        goto end_seq;
    }

end_seq:

    for( i = 0 ; i < i_command ; i++ )
    {
        free( p_command[i] );
    }

    *p_message = message;

    return i_return;
}


static vlm_media_t *vlm_MediaSearch( vlm_t *vlm, char *psz_name )
{
    int i;

    for( i = 0; i < vlm->i_media; i++ )
    {
        if( strcmp( psz_name, vlm->media[i]->psz_name ) == 0 )
        {
            return vlm->media[i];
        }
    }

    return NULL;
}


static vlm_media_t *vlm_MediaNew( vlm_t *vlm , char *psz_name, int i_type )
{
    vlm_media_t *media= malloc( sizeof( vlm_media_t ));

    media->psz_name = strdup( psz_name );
    media->b_enabled = VLC_FALSE;
    media->b_loop = VLC_FALSE;
    media->i_input = 0;
    media->input = NULL;
    media->i_index = 0;
    media->psz_output = NULL;
    media->i_option = 0;
    media->option = NULL;
    media->i_input_option = 0;
    media->input_option = NULL;
    media->i_type = i_type;
    media->p_input = NULL;

    TAB_APPEND( vlm->i_media , vlm->media , media );

    return media;
}


/* for now, simple delete. After, del with options (last arg) */
static int vlm_MediaDelete( vlm_t *vlm, vlm_media_t *media, char *psz_name )
{
    int i;

    if( media == NULL )
    {
        return 1;
    }

    if( media->p_input )
    {
        input_StopThread( media->p_input );

        input_DestroyThread( media->p_input );
        vlc_object_detach( media->p_input );
        vlc_object_destroy( media->p_input );
    }

    TAB_REMOVE( vlm->i_media, vlm->media , media );

    if( vlm->i_media == 0 && vlm->media ) free( vlm->media );

    free( media->psz_name );

    for( i = 0; i < media->i_input; i++ )
    {
        free( media->input[i] );
    }
    if( media->input ) free( media->input );

    if( media->psz_output ) free( media->psz_output );

    for( i = 0; i < media->i_option; i++ )
    {
        free( media->option[i] );
    }
    if(media->option) free( media->option );

    for( i = 0; i < media->i_input_option; i++ )
    {
        free( media->input_option[i] );
    }
    if( media->input_option ) free( media->input_option );

    free( media );

    return 0;
}


static int vlm_MediaSetup( vlm_media_t *media, char *psz_cmd, char *psz_value )
{
    if( strcmp( psz_cmd, "loop" ) == 0 )
    {
        media->b_loop = VLC_TRUE;
    }
    else if( strcmp( psz_cmd, "unloop" ) == 0 )
    {
        media->b_loop = VLC_FALSE;
    }
    else if( strcmp( psz_cmd, "enabled" ) == 0 )
    {
        media->b_enabled = VLC_TRUE;
    }
    else if( strcmp( psz_cmd, "disabled" ) == 0 )
    {
        media->b_enabled = VLC_FALSE;
    }
    else if( strcmp( psz_cmd, "input" ) == 0 )
    {
        char *input;

        if( psz_value != NULL && strlen(psz_value) > 1 && ( psz_value[0] == '\'' || psz_value[0] == '\"' ) 
            && ( psz_value[ strlen(psz_value) - 1 ] == '\'' || psz_value[ strlen(psz_value) - 1 ] == '\"' )  )
        {
            input = malloc( strlen(psz_value) - 1 );

            memcpy( input , psz_value + 1 , strlen(psz_value) - 2 );
            input[ strlen(psz_value) - 2 ] = '\0';
        }
        else
        {
            input = strdup( psz_value );
        }

        TAB_APPEND( media->i_input, media->input, input );
    }
    else if( strcmp( psz_cmd, "output" ) == 0 )
    {
        if( media->psz_output != NULL )
        {
            free( media->psz_output );
        }
        media->psz_output = strdup( psz_value );
    }
    else if( strcmp( psz_cmd, "option" ) == 0 )
    {
        char *option;
        option = strdup( psz_value );

        TAB_APPEND( media->i_option, media->option, option );
    }
    else
    {
        return 1;
    }
    return 0;
}

static int vlm_MediaControl( vlm_t *vlm, vlm_media_t *media, char *psz_name, char *psz_args )
{
    if( strcmp( psz_name, "play" ) == 0 )
    {
        int i;

        if( media->b_enabled == VLC_TRUE && media->i_input > 0 )
        {
            if( psz_args != NULL && sscanf( psz_args, "%d", &i ) == 1 && i < media->i_input )
            {
                media->i_index = i;
            }
            else
            {
                media->i_index = 0;
            }

            if( media->psz_output != NULL )
            {
                media->input_option = malloc( sizeof( char* ) );
                media->input_option[0] = malloc( strlen(media->psz_output) + 1 + 6 );
                sprintf( media->input_option[0], ":sout=%s" , media->psz_output );
                media->i_input_option = 1;
            }
            else
            {
                media->input_option = NULL;
                media->i_input_option = 0;
            }

            for( i=0 ; i < media->i_option ; i++ )
            {
                media->i_input_option++;
                media->input_option = realloc( media->input_option, (media->i_input_option) * sizeof( char* ) );
                media->input_option[ media->i_input_option - 1 ] = malloc( strlen(media->option[i]) + 1 + 1 );
                sprintf( media->input_option[ media->i_input_option - 1 ], ":%s" , media->option[i] );
            }

            media->p_input = input_CreateThread( vlm, media->input[media->i_index], media->input_option, media->i_input_option );

            return 0;
        }
        else
        {
            return 1;
        }
    }
    else if( strcmp( psz_name, "seek" ) == 0 )
    {
        vlc_value_t val;
        float f_percentage;

        if( psz_args && sscanf( psz_args, "%f", &f_percentage ) == 1 )
        {
            val.f_float = f_percentage / 100.0 ;
            var_Set( media->p_input, "position", val );
            return 0;
        }
    }
    else if( strcmp( psz_name, "stop" ) == 0 )
    {
        int i;

        if( media->p_input )
        {
            input_StopThread( media->p_input );
            input_DestroyThread( media->p_input );
            vlc_object_detach( media->p_input );
            vlc_object_destroy( media->p_input );
            media->p_input = NULL;

            for( i=0 ; i < media->i_input_option ; i++ )
            {
                free( media->input_option[i] );
            }
            if( media->input_option) free( media->input_option );

            media->input_option = NULL;
            media->i_input_option = 0;
        }

        return 0;
    }
    else if( strcmp( psz_name, "pause" ) == 0 )
    {
        vlc_value_t val;

        val.i_int = 0;

        if( media->p_input != NULL )
        {
            var_Get( media->p_input, "state", &val );
        }

        if( val.i_int == PAUSE_S )
        {
            if( media->p_input )
            {
                val.i_int = PLAYING_S;
                var_Set( media->p_input, "state", val );
            }
        }
        else
        {
            if( media->p_input )
            {
                val.i_int = PAUSE_S;
                var_Set( media->p_input, "state", val );
            }
        }
        return 0;
    }

    return 1;
}

static vlm_message_t *vlm_Show( vlm_t *vlm, vlm_media_t *media, vlm_schedule_t *schedule, char *psz_filter )
{

    if( media != NULL )
    {
        int i;
        vlm_message_t *message;
        vlm_message_t *message_media;
        vlm_message_t *message_child;

        message = vlm_MessageNew( "show" , NULL );
        message_media = vlm_MessageAdd( message , vlm_MessageNew( media->psz_name , NULL ) );

        if( media->i_type == VOD_TYPE )
        {
            vlm_MessageAdd( message_media , vlm_MessageNew( "type" , "vod" ) );
        }
        else
        {
            vlm_MessageAdd( message_media , vlm_MessageNew( "type" , "broadcast" ) );
        }

        vlm_MessageAdd( message_media , vlm_MessageNew( "enabled" , media->b_enabled ? "yes" : "no" ) );

        vlm_MessageAdd( message_media, vlm_MessageNew( "loop" , media->b_loop ? "yes" : "no" ) );

        message_child = vlm_MessageAdd( message_media , vlm_MessageNew( "inputs" , NULL ) );

        for( i=0 ; i < (media->i_input) ; i++ )
        {
            vlm_MessageAdd( message_child , vlm_MessageNew( media->input[i] , NULL ) );
        }

        vlm_MessageAdd( message_media , vlm_MessageNew( "output" , media->psz_output ? media->psz_output : "" ) );

        message_child = vlm_MessageAdd( message_media , vlm_MessageNew( "options" , NULL ) );

        for( i=0 ; i < (media->i_option) ; i++ )
        {
            vlm_MessageAdd( message_child , vlm_MessageNew( media->option[i] , NULL ) );
        }

        if( media->p_input != NULL )
        {
            vlc_value_t val;

            var_Get( media->p_input, "state", &val );

            if( val.i_int == PLAYING_S )
            {
                vlm_MessageAdd( message_media , vlm_MessageNew( "state" , "playing" ) );
            }
            else if( val.i_int == PAUSE_S )
            {
                vlm_MessageAdd( message_media , vlm_MessageNew( "state" , "paused" ) );
            }
            else
            {
                vlm_MessageAdd( message_media , vlm_MessageNew( "state" , "stop" ) );
            }
        }
        else
        {
            vlm_MessageAdd( message_media , vlm_MessageNew( "state" , "stop" ) );
        }

        return message;

    }
    else if( schedule != NULL )
    {
        int i;
        vlm_message_t *message;
        vlm_message_t *message_schedule;
        vlm_message_t *message_child;

        message = vlm_MessageNew( "show" , NULL );
        message_schedule = vlm_MessageAdd( message , vlm_MessageNew( schedule->psz_name , NULL ) );

        vlm_MessageAdd( message_schedule , vlm_MessageNew( "type" , "schedule" ) );

        if( schedule->b_enabled == VLC_TRUE )
        {
            vlm_MessageAdd( message_schedule , vlm_MessageNew( "enabled" , "yes" ) );
        }
        else
        {
            vlm_MessageAdd( message_schedule , vlm_MessageNew( "enabled" , "no" ) );
        }

        if( schedule->i_date != 0 )
        {
            time_t i_time = schedule->i_date / (int64_t)1000000;

#ifdef HAVE_CTIME_R
            char psz_date[500];
            ctime_r( &i_time, psz_date );
#else
            char *psz_date = ctime( &i_time );
#endif
            vlm_MessageAdd( message_schedule , vlm_MessageNew( "date" , psz_date ) );
        }
        else
        {
            vlm_MessageAdd( message_schedule , vlm_MessageNew( "date" , "now" ) );
        }

        if( schedule->i_period != 0 )
        {
            char buffer[100];
            time_t i_time = (time_t) ( schedule->i_period / 1000000 );
            struct tm date;

            date.tm_sec = (int)( i_time % 60 );
            i_time = i_time / 60;
            date.tm_min = (int)( i_time % 60 );
            i_time = i_time / 60;
            date.tm_hour = (int)( i_time % 24 );
            i_time = i_time / 24;
            date.tm_mday = (int)( i_time % 30 );
            i_time = i_time / 30;
            /* okay, okay, months are not always 30 days long */
            date.tm_mon = (int)( i_time % 12 );
            i_time = i_time / 12;
            date.tm_year = (int)i_time;

            sprintf( buffer, "%d years %d months %d days %d hours %d minutes %d seconds", date.tm_year ,
                                                                                          date.tm_mon,
                                                                                          date.tm_mday,
                                                                                          date.tm_hour,
                                                                                          date.tm_min,
                                                                                          date.tm_sec);

            vlm_MessageAdd( message_schedule , vlm_MessageNew( "period" , buffer ) );

            if( schedule->i_repeat >= 0 )
            {
                sprintf( buffer, "%d" , schedule->i_repeat );
                vlm_MessageAdd( message_schedule , vlm_MessageNew( "repeat" , buffer ) );
            }
        }

        message_child = vlm_MessageAdd( message_schedule , vlm_MessageNew( "commands" , NULL ) );

        for( i=0 ; i < (schedule->i_command) ; i++ )
        {
           vlm_MessageAdd( message_child , vlm_MessageNew( schedule->command[i] , NULL ) );
        }

        return message;

    }
    else if( psz_filter && strcmp( psz_filter , "media") == 0 )
    {
        int i;
        vlm_message_t *message;
        vlm_message_t *message_child;

        message = vlm_MessageNew( "show" , NULL );
        message_child = vlm_MessageAdd( message , vlm_MessageNew( "media" , NULL ) );

        for( i = 0; i < vlm->i_media; i++ )
        {
            vlm_media_t *m = vlm->media[i];
            vlm_message_t *message_media = vlm_MessageAdd( message_child , vlm_MessageNew( m->psz_name , NULL ) );

            if( m->i_type == VOD_TYPE )
            {
                vlm_MessageAdd( message_media , vlm_MessageNew( "type" , "vod" ) );
            }
            else
            {
                vlm_MessageAdd( message_media , vlm_MessageNew( "type" , "broadcast" ) );
            }

            if( m->b_enabled == VLC_TRUE )
            {
                vlm_MessageAdd( message_media , vlm_MessageNew( "enabled" , "yes" ) );
            }
            else
            {
                vlm_MessageAdd( message_media , vlm_MessageNew( "enabled" , "no" ) );
            }

            if( m->p_input != NULL )
            {
                vlc_value_t val;

                var_Get( m->p_input, "state", &val );

                if( val.i_int == PLAYING_S )
                {
                    vlm_MessageAdd( message_media , vlm_MessageNew( "state" , "playing" ) );
                }
                else if( val.i_int == PAUSE_S )
                {
                    vlm_MessageAdd( message_media , vlm_MessageNew( "state" , "paused" ) );
                }
                else
                {
                    vlm_MessageAdd( message_media , vlm_MessageNew( "state" , "stop" ) );
                }
            }
            else
            {
                vlm_MessageAdd( message_media , vlm_MessageNew( "state" , "stop" ) );
            }
        }

        return message;
    }
    else if( psz_filter && strcmp( psz_filter , "schedule") == 0 )
    {
        int i;
        vlm_message_t *message;
        vlm_message_t *message_child;

        message = vlm_MessageNew( "show" , NULL );
        message_child = vlm_MessageAdd( message , vlm_MessageNew( "schedule" , NULL ) );

        for( i = 0; i < vlm->i_schedule; i++ )
        {
            vlm_schedule_t *s = vlm->schedule[i];
            vlm_message_t *message_schedule = vlm_MessageAdd( message_child , vlm_MessageNew( s->psz_name , NULL ) ); 

            if( s->b_enabled == VLC_TRUE )
            {
                mtime_t i_time;
                mtime_t i_next_date;

                vlm_MessageAdd( message_schedule , vlm_MessageNew( "enabled" , "yes" ) );

                /* calculate next date */
                i_time = mdate();

                i_next_date = s->i_date;

                if( s->i_period != 0 )
                {
                    int j = 0;
                    while( s->i_date + j * s->i_period <= i_time && s->i_repeat > j )
                    {
                        j++;
                    }

                    i_next_date = s->i_date + j * s->i_period;
                }

                if( i_next_date > i_time )
                {
                    time_t i_date = (time_t) (i_next_date / 1000000) ;

#ifdef HAVE_CTIME_R
                    char psz_date[500];
                    ctime_r( &i_date, psz_date );
#else
                    char *psz_date = ctime( &i_date );
#endif

                    vlm_MessageAdd( message_schedule , vlm_MessageNew( "next launch" , psz_date ) );
                }
            }
            else
            {
                vlm_MessageAdd( message_schedule , vlm_MessageNew( "enabled" , "no" ) );
            }
        }

        return message;
    }
    else if( psz_filter == NULL && media == NULL && schedule == NULL )
    {
        vlm_message_t *show1 = vlm_Show( vlm, NULL, NULL, "media" );
        vlm_message_t *show2 = vlm_Show( vlm, NULL, NULL, "schedule" );

        vlm_MessageAdd( show1 , show2->child[0] );

        /* we must destroy the parent node "show" of show2, and not the children */
        free( show2->psz_name );
        free( show2 );

        return show1;
    }
    else
    {
        return vlm_MessageNew( "show" , NULL );
    }
}

static vlm_message_t *vlm_Help( vlm_t *vlm, char *psz_filter )
{
    vlm_message_t *message;

    if( psz_filter == NULL )
    {
        char *help = malloc( strlen( "Commands Syntax:" ) +
                     strlen( "\n new (name) vod|broadcast|schedule [properties]" ) +
                     strlen( "\n setup (name) (properties)" ) +
                     strlen( "\n show [(name)|media|schedule]" ) +
                     strlen( "\n del (name)|all|media|schedule" ) +
                     strlen( "\n control (name) (command)" ) +
                     strlen( "\n save (config_file)" ) +
                     strlen( "\n load (config_file)" ) +
                     strlen( "\nMedia Proprieties Syntax:" ) +
                     strlen( "\n input (input_name)" ) +
                     strlen( "\n output (output_name)" ) +
                     strlen( "\n option (option_name)[=value]" ) +
                     strlen( "\n enabled|disabled" ) +
                     strlen( "\n loop|unloop (broadcast only)" ) +
                     strlen( "\nSchedule Proprieties Syntax:" ) +
                     strlen( "\n enabled|disabled" ) +
                     strlen( "\n append (command_until_rest_of_the_line)" ) +
                     strlen( "\n date (year)/(month)/(day)-(hour):(minutes):(seconds)|now" ) +
                     strlen( "\n period (years_aka_12_months)/(months_aka_30_days)/(days)-(hours):(minutes):(seconds)" ) +
                     strlen( "\n repeat (number_of_repetitions)" ) +
                     strlen( "\nControl Commands Syntax:" ) +
                     strlen( "\n play\n pause\n stop\n seek (percentage)\n" ) + 1 );

        sprintf( help,
                 "Commands Syntax:"
                 "\n new (name) vod|broadcast|schedule [properties]"
                 "\n setup (name) (properties)"
                 "\n show [(name)|media|schedule]"
                 "\n del (name)|all|media|schedule"
                 "\n control (name) (command)"
                 "\n save (config_file)"
                 "\n load (config_file)"
                 "\nMedia Proprieties Syntax:"
                 "\n input (input_name)"
                 "\n output (output_name)"
                 "\n option (option_name)[=value]"
                 "\n enabled|disabled"
                 "\n loop|unloop (broadcast only)"
                 "\nSchedule Proprieties Syntax:"
                 "\n enabled|disabled"
                 "\n append (command_until_rest_of_the_line)"
                 "\n date (year)/(month)/(day)-(hour):(minutes):(seconds)|now"
                 "\n period (years_aka_12_months)/(months_aka_30_days)/(days)-(hours):(minutes):(seconds)"
                 "\n repeat (number_of_repetitions)"
                 "\nControl Commands Syntax:"
                 "\n play\n pause\n stop\n seek (percentage)\n" );

        message = vlm_MessageNew( "help" , NULL );
        vlm_MessageAdd( message , vlm_MessageNew( "Help" , help ) );
        free( help );
        return message;
    }

    return vlm_MessageNew( "help" , NULL );
}

/* file must end by '\0' */
static int vlm_Load( vlm_t *vlm, char *file )
{
    char *pf = file;

    while( *pf != '\0' )
    {
        vlm_message_t *message = NULL;
        int i_temp = 0;
        int i_next;

        while( pf[i_temp] != '\n' && pf[i_temp] != '\0' && pf[i_temp] != '\r' )
        {
            i_temp++;
        }

        if( pf[i_temp] == '\r' || pf[i_temp] == '\n' )
        {
            pf[i_temp] = '\0';
            i_next = i_temp + 1;
        }
        else
        {
            i_next = i_temp;
        }

        if( ExecuteCommand( vlm, pf, &message ) )
        {
            free( message );
            return 1;
        }
        free( message );

        pf += i_next;
    }
    return 0;
}

static char *vlm_Save( vlm_t *vlm )
{
    char *save = NULL;
    char *p;
    int i,j;
    int i_length = 0;

    for( i = 0; i < vlm->i_media; i++ )
    {
        vlm_media_t *media = vlm->media[i];

        if( media->i_type == VOD_TYPE )
        {
            i_length += strlen( "new  vod " ) + strlen(media->psz_name);
        }
        else
        {
            i_length += strlen( "new  broadcast " ) + strlen(media->psz_name);
        }

        if( media->b_enabled == VLC_TRUE )
        {
            i_length += strlen( "enabled" );
        }
        else
        {
            i_length += strlen( "disabled" );
        }

        if( media->b_loop == VLC_TRUE )
        {
            i_length += strlen( " loop\n" );
        }
        else
        {
            i_length += strlen( "\n" );
        }

        for( j = 0; j < media->i_input; j++ )
        {
            i_length += strlen( "setup  input \"\"\n" ) + strlen( media->psz_name ) + strlen( media->input[j] );
        }

        if( media->psz_output != NULL )
        {
            i_length += strlen(media->psz_name) + strlen(media->psz_output) + strlen( "setup  output \n" );
        }

        for( j=0 ; j < media->i_option ; j++ )
        {
            i_length += strlen(media->psz_name) + strlen(media->option[j]) + strlen("setup  option \n");
        }
    }

    for( i = 0; i < vlm->i_schedule; i++ )
    {
        vlm_schedule_t *schedule = vlm->schedule[i];

        i_length += strlen( "new  schedule " ) + strlen( schedule->psz_name );

        if( schedule->b_enabled == VLC_TRUE )
        {
            i_length += strlen( "date //-:: enabled\n" ) + 14;
        }
        else
        {
            i_length += strlen( "date //-:: disabled\n" ) + 14;
        }


        if( schedule->i_period != 0 )
        {
            i_length += strlen( "setup  " ) + strlen( schedule->psz_name ) + strlen( "period //-::\n" ) + 14;
        }

        if( schedule->i_repeat >= 0 )
        {
            char buffer[12];

            sprintf( buffer, "%d", schedule->i_repeat );
            i_length += strlen( "setup  repeat \n" ) + strlen( schedule->psz_name ) + strlen( buffer );
        }
        else
        {
            i_length++;
        }

        for( j = 0; j < schedule->i_command; j++ )
        {
            i_length += strlen( "setup  append \n" ) + strlen( schedule->psz_name ) + strlen( schedule->command[j] );
        }

    }

    /* Don't forget the '\0' */
    i_length++;
    /* now we have the length of save */

    p = save = malloc( i_length );
    *save = '\0';

    /* finally we can write in it */
    for( i = 0; i < vlm->i_media; i++ )
    {
        vlm_media_t *media = vlm->media[i];

        if( media->i_type == VOD_TYPE )
        {
            p += sprintf( p, "new %s vod ", media->psz_name);
        }
        else
        {
            p += sprintf( p, "new %s broadcast ", media->psz_name);
        }

        if( media->b_enabled == VLC_TRUE )
        {
            p += sprintf( p, "enabled" );
        }
        else
        {
            p += sprintf( p, "disabled" );
        }

        if( media->b_loop == VLC_TRUE )
        {
            p += sprintf( p, " loop\n" );
        }
        else
        {
            p += sprintf( p, "\n" );
        }

        for( j = 0; j < media->i_input; j++ )
        {
            p += sprintf( p, "setup %s input \"%s\"\n", media->psz_name, media->input[j] );
        }

        if( media->psz_output != NULL )
        {
            p += sprintf( p, "setup %s output %s\n", media->psz_name, media->psz_output );
        }

        for( j = 0; j < media->i_option; j++ )
        {
            p += sprintf( p, "setup %s option %s\n", media->psz_name, media->option[j] );
        }
    }

    /* and now, the schedule scripts */

    for( i = 0; i < vlm->i_schedule; i++ )
    {
        vlm_schedule_t *schedule = vlm->schedule[i];
        struct tm date;
        time_t i_time = (time_t) ( schedule->i_date / 1000000 );

#ifdef HAVE_LOCALTIME_R
        localtime_r( &i_time, &date);
#else
        struct tm *p_date = localtime( &i_time );
        date = *p_date;
#endif

        p += sprintf( p, "new %s schedule ", schedule->psz_name);

        if( schedule->b_enabled == VLC_TRUE )
        {
            p += sprintf( p, "date %d/%d/%d-%d:%d:%d enabled\n",
                          date.tm_year + 1900, date.tm_mon + 1, date.tm_mday,
                          date.tm_hour, date.tm_min, date.tm_sec );
        }
        else
        {
            p += sprintf( p, "date %d/%d/%d-%d:%d:%d disabled\n",
                          date.tm_year + 1900, date.tm_mon + 1, date.tm_mday,
                          date.tm_hour, date.tm_min, date.tm_sec);
        }


        if( schedule->i_period != 0 )
        {
            p += sprintf( p, "setup %s ", schedule->psz_name );

            i_time = (time_t) ( schedule->i_period / 1000000 );

            date.tm_sec = (int)( i_time % 60 );
            i_time = i_time / 60;
            date.tm_min = (int)( i_time % 60 );
            i_time = i_time / 60;
            date.tm_hour = (int)( i_time % 24 );
            i_time = i_time / 24;
            date.tm_mday = (int)( i_time % 30 );
            i_time = i_time / 30;
            /* okay, okay, months are not always 30 days long */
            date.tm_mon = (int)( i_time % 12 );
            i_time = i_time / 12;
            date.tm_year = (int)i_time;

            p += sprintf( p, "period %d/%d/%d-%d:%d:%d\n", date.tm_year ,
                                                           date.tm_mon,
                                                           date.tm_mday,
                                                           date.tm_hour,
                                                           date.tm_min,
                                                           date.tm_sec);
        }

        if( schedule->i_repeat >= 0 )
        {
            p += sprintf( p, "setup %s repeat %d\n", schedule->psz_name, schedule->i_repeat );
        }
        else
        {
            p += sprintf( p, "\n" );
        }

        for( j = 0; j < schedule->i_command; j++ )
        {
            p += sprintf( p, "setup %s append %s\n", schedule->psz_name, schedule->command[j] );
        }

    }

    return save;
}

static vlm_schedule_t *vlm_ScheduleNew( vlm_t *vlm , char *psz_name )
{
    vlm_schedule_t *sched= malloc( sizeof( vlm_schedule_t ));

    sched->psz_name = strdup( psz_name );
    sched->b_enabled = VLC_FALSE;
    sched->i_command = 0;
    sched->command = NULL;
    sched->i_date = 0;
    sched->i_period = 0;
    sched->i_repeat = 0;

    TAB_APPEND( vlm->i_schedule , vlm->schedule , sched );

    return sched;
}

/* for now, simple delete. After, del with options (last arg) */
static int vlm_ScheduleDelete( vlm_t *vlm, vlm_schedule_t *sched, char *psz_name )
{
    int i;

    if( sched == NULL )
    {
        return 1;
    }

    TAB_REMOVE( vlm->i_schedule, vlm->schedule , sched );

    if( vlm->i_schedule == 0 && vlm->schedule ) free( vlm->schedule );

    free( sched->psz_name );

    for( i = 0; i < sched->i_command; i++ )
    {
        free( sched->command[i] );
    }

    free( sched );

    return 0;
}

static vlm_schedule_t *vlm_ScheduleSearch( vlm_t *vlm, char *psz_name )
{
    int i;

    for( i = 0; i < vlm->i_schedule; i++ )
    {
        if( strcmp( psz_name, vlm->schedule[i]->psz_name ) == 0 )
        {
            return vlm->schedule[i];
        }
    }

    return NULL;
}

/* Ok, setup schedule command will be able to support only one (argument value) at a time  */
static int vlm_ScheduleSetup( vlm_schedule_t *schedule, char *psz_cmd, char *psz_value )
{
    if( strcmp( psz_cmd, "enabled" ) == 0 )
    {
        schedule->b_enabled = VLC_TRUE;
    }
    else if( strcmp( psz_cmd, "disabled" ) == 0 )
    {
        schedule->b_enabled = VLC_FALSE;
    }
    else if( strcmp( psz_cmd, "date" ) == 0 )
    {
        struct tm time;
        char *p;
        time_t date;

        time.tm_sec = 0;         /* seconds */
        time.tm_min = 0;         /* minutes */
        time.tm_hour = 0;        /* hours */
        time.tm_mday = 0;        /* day of the month */
        time.tm_mon = 0;         /* month */
        time.tm_year = 0;        /* year */
        time.tm_wday = 0;        /* day of the week */
        time.tm_yday = 0;        /* day in the year */
        time.tm_isdst = 0;       /* daylight saving time */

        /* date should be year/month/day-hour:minutes:seconds */
        p = strchr( psz_value , '-' );

        if( strcmp( psz_value, "now" ) == 0 )
        {
            schedule->i_date = 0;
        }
        else if( p == NULL && sscanf( psz_value, "%d:%d:%d" , &time.tm_hour, &time.tm_min, &time.tm_sec ) != 3 ) /* it must be a hour:minutes:seconds */
        {
            return 1;
        }
        else
        {
            int i,j,k;

            switch( sscanf( p + 1, "%d:%d:%d" , &i, &j, &k ) )
            {
                case 1:
                    time.tm_sec = i;
                    break;
                case 2:
                    time.tm_min = i;
                    time.tm_sec = j;
                    break;
                case 3:
                    time.tm_hour = i;
                    time.tm_min = j;
                    time.tm_sec = k;
                    break;
                default:
                    return 1;
            }

            *p = '\0';

            switch( sscanf( psz_value, "%d/%d/%d" , &i, &j, &k ) )
            {
                case 1:
                    time.tm_mday = i;
                    break;
                case 2:
                    time.tm_mon = i - 1;
                    time.tm_mday = j;
                    break;
                case 3:
                    time.tm_year = i - 1900;
                    time.tm_mon = j - 1;
                    time.tm_mday = k;
                    break;
                default:
                    return 1;
            }

            date = mktime( &time );
            schedule->i_date = ((mtime_t) date) * 1000000;
        }
    }
    else if( strcmp( psz_cmd, "period" ) == 0 )
    {
        struct tm time;
        char *p;
        char *psz_time = NULL, *psz_date = NULL;
        time_t date;
        int i,j,k;

        /* First, if date or period are modified, repeat should be equal to -1 */
        schedule->i_repeat = -1;

        time.tm_sec = 0;         /* seconds */
        time.tm_min = 0;         /* minutes */
        time.tm_hour = 0;        /* hours */
        time.tm_mday = 0;        /* day of the month */
        time.tm_mon = 0;         /* month */
        time.tm_year = 0;        /* year */
        time.tm_wday = 0;        /* day of the week */
        time.tm_yday = 0;        /* day in the year */
        time.tm_isdst = 0;       /* daylight saving time */

        /* date should be year/month/day-hour:minutes:seconds */
        p = strchr( psz_value , '-' );
        if( p )
        {
            psz_date = psz_value;
            psz_time = p + 1;

            *p = '\0';
        }
        else
        {
            psz_time = psz_value;
        }


        switch( sscanf( psz_time, "%d:%d:%d" , &i, &j, &k ) )
        {
            case 1:
                time.tm_sec = i;
                break;
            case 2:
                time.tm_min = i;
                time.tm_sec = j;
                break;
            case 3:
                time.tm_hour = i;
                time.tm_min = j;
                time.tm_sec = k;
                break;
            default:
                return 1;
        }
        if( psz_date )
        {
            switch( sscanf( psz_date, "%d/%d/%d" , &i, &j, &k ) )
            {
                case 1:
                    time.tm_mday = i;
                    break;
                case 2:
                    time.tm_mon = i;
                    time.tm_mday = j;
                    break;
                case 3:
                    time.tm_year = i;
                    time.tm_mon = j;
                    time.tm_mday = k;
                    break;
                default:
                    return 1;
            }
        }

        /* ok, that's stupid... who is going to schedule streams every 42 years ? */
        date = (((( time.tm_year * 12 + time.tm_mon ) * 30 + time.tm_mday ) * 24 + time.tm_hour ) * 60 + time.tm_min ) * 60 + time.tm_sec ;
        schedule->i_period = ((mtime_t) date) * 1000000;
    }
    else if( strcmp( psz_cmd, "repeat" ) == 0 )
    {
        int i;

        if( sscanf( psz_value, "%d" , &i ) == 1 )
        {
            schedule->i_repeat = i;
        }
        else
        {
            return 1;
        }
    }
    else if( strcmp( psz_cmd, "append" ) == 0 )
    {
        char *command = strdup( psz_value );

        TAB_APPEND( schedule->i_command, schedule->command, command );
    }
    else
    {
        return 1;
    }
    return 0;
}

/*****************************************************************************
 * Manage:
 *****************************************************************************/
static int Manage( vlc_object_t* p_object )
{
    vlm_t *vlm = (vlm_t*)p_object;
    int i,j;
    mtime_t i_lastcheck;
    mtime_t i_time;

    i_lastcheck = mdate();

    msleep( 100000 );

    while( !vlm->b_die )
    {
        vlc_mutex_lock( &vlm->lock );

        /* destroy the inputs that wants to die, and launch the next input */
        for( i = 0; i < vlm->i_media; i++ )
        {
            vlm_media_t *media = vlm->media[i];

            if( media->p_input != NULL && ( media->p_input->b_eof || media->p_input->b_error ) )
            {
                input_StopThread( media->p_input );

                input_DestroyThread( media->p_input );
                vlc_object_detach( media->p_input );
                vlc_object_destroy( media->p_input );
                media->p_input = NULL;
                media->i_index++;

                if( media->i_index == media->i_input && media->b_loop == VLC_TRUE )
                {
                    media->i_index = 0;
                }

                if( media->i_index < media->i_input )
                {
                    char buffer[12];

                    sprintf( buffer, "%d", media->i_index );
                    vlm_MediaControl( vlm, media, "play", buffer );
                }
            }
        }

        /* scheduling */
        i_time = mdate();

        for( i = 0; i < vlm->i_schedule; i++ )
        {
            mtime_t i_real_date = vlm->schedule[i]->i_date;

            if( vlm->schedule[i]->i_date == 0 ) // now !
            {
                vlm->schedule[i]->i_date = (i_time / 1000000) * 1000000 ;
                i_real_date = i_time;
            }
            else if( vlm->schedule[i]->i_period != 0 )
            {
                int j = 0;
                while( vlm->schedule[i]->i_date + j * vlm->schedule[i]->i_period <= i_lastcheck &&
                       ( vlm->schedule[i]->i_repeat > j || vlm->schedule[i]->i_repeat == -1 ) )
                {
                    j++;
                }

                i_real_date = vlm->schedule[i]->i_date + j * vlm->schedule[i]->i_period;
            }

            if( vlm->schedule[i]->b_enabled == VLC_TRUE )
            {
                if( i_real_date <= i_time && i_real_date > i_lastcheck )
                {
                    for( j = 0 ; j < vlm->schedule[i]->i_command ; j++ )
                    {
                        vlm_message_t *message = NULL;

                        ExecuteCommand( vlm, vlm->schedule[i]->command[j] , &message );

                        /* for now, drop the message */
                        free( message );
                    }
                }
            }
        }

        i_lastcheck = i_time;

        vlc_mutex_unlock( &vlm->lock );

        msleep( 100000 );
    }

    return VLC_SUCCESS;
}


static vlm_message_t* vlm_MessageNew( char *psz_name , char *psz_value )
{
    vlm_message_t *message = malloc( sizeof(vlm_message_t) );

    if( psz_name )
    {
        message->psz_name = strdup( psz_name );
    }
    else
    {
        return NULL;
    }

    if( psz_value )
    {
        message->psz_value = strdup( psz_value );
    }
    else
    {
        message->psz_value = NULL;
    }

    message->i_child = 0;
    message->child = NULL;

    return message;
}

void vlm_MessageDelete( vlm_message_t* message )
{
    int i;

    if( message->psz_name ) free( message->psz_name );
    if( message->psz_value ) free( message->psz_value );

    for( i = 0; i < message->i_child; i++)
    {
        vlm_MessageDelete( message->child[i] );
    }

    free( message );
}

/* add a child */
static vlm_message_t* vlm_MessageAdd( vlm_message_t* message , vlm_message_t* child )
{
    if( message == NULL ) return NULL;

    if( child )
    {
        TAB_APPEND( message->i_child , message->child , child );
    }

    return child;
}

