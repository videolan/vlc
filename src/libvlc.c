/*****************************************************************************
 * libvlc.c: Implementation of the old libvlc API
 *****************************************************************************
 * Copyright (C) 1998-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Rémi Denis-Courmont <rem # videolan : org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Pretend we are a builtin module
 *****************************************************************************/
#define MODULE_NAME main
#define MODULE_PATH main
#define __BUILTIN__

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/vlc.h>

#include "control/libvlc_internal.h"
#include "libvlc.h"

#include <vlc_playlist.h>

#include <vlc_aout.h>
#include <vlc_vout.h>

/*****************************************************************************
 * VLC_Version: return the libvlc version.
 *****************************************************************************
 * This function returns full version string (numeric version and codename).
 *****************************************************************************/
char const * VLC_Version( void )
{
    return VERSION_MESSAGE;
}

/*****************************************************************************
 * VLC_CompileBy, VLC_CompileHost, VLC_CompileDomain,
 * VLC_Compiler, VLC_Changeset
 *****************************************************************************/
#define DECLARE_VLC_VERSION( func, var )                                    \
char const * VLC_##func ( void )                                            \
{                                                                           \
    return VLC_##var ;                                                      \
}

DECLARE_VLC_VERSION( CompileBy, COMPILE_BY );
DECLARE_VLC_VERSION( CompileHost, COMPILE_HOST );
DECLARE_VLC_VERSION( CompileDomain, COMPILE_DOMAIN );
DECLARE_VLC_VERSION( Compiler, COMPILER );

extern const char psz_vlc_changeset[];
const char* VLC_Changeset( void )
{
    return psz_vlc_changeset;
}

/*****************************************************************************
 * VLC_Error: strerror() equivalent
 *****************************************************************************
 * This function returns full version string (numeric version and codename).
 *****************************************************************************/
char const * VLC_Error( int i_err )
{
    return vlc_error( i_err );
}

/*****************************************************************************
 * VLC_Create: allocate a libvlc instance and intialize global libvlc stuff if needed
 *****************************************************************************
 * This function allocates a libvlc instance and returns a negative value
 * in case of failure. Also, the thread system is initialized.
 *****************************************************************************/
int VLC_Create( void )
{
    libvlc_int_t *p_object = libvlc_InternalCreate();
    if( p_object ) return p_object->i_object_id;
    return VLC_ENOOBJ;
}

#define LIBVLC_FUNC \
    libvlc_int_t * p_libvlc = vlc_current_object( i_object ); \
    if( !p_libvlc ) return VLC_ENOOBJ;
#define LIBVLC_FUNC_END \
    if( i_object ) vlc_object_release( p_libvlc );


/*****************************************************************************
 * VLC_Init: initialize a libvlc instance
 *****************************************************************************
 * This function initializes a previously allocated libvlc instance:
 *  - CPU detection
 *  - gettext initialization
 *  - message queue, module bank and playlist initialization
 *  - configuration and commandline parsing
 *****************************************************************************/
int VLC_Init( int i_object, int i_argc, const char *ppsz_argv[] )
{
    int i_ret;
    LIBVLC_FUNC;
    i_ret = libvlc_InternalInit( p_libvlc, i_argc, ppsz_argv );
    LIBVLC_FUNC_END;
    return i_ret;
}

/*****************************************************************************
 * VLC_AddIntf: add an interface
 *****************************************************************************
 * This function opens an interface plugin and runs it. If b_block is set
 * to 0, VLC_AddIntf will return immediately and let the interface run in a
 * separate thread. If b_block is set to 1, VLC_AddIntf will continue until
 * user requests to quit. If b_play is set to 1, VLC_AddIntf will start playing
 * the playlist when it is completely initialised.
 *****************************************************************************/
int VLC_AddIntf( int i_object, char const *psz_module,
                 vlc_bool_t b_block, vlc_bool_t b_play )
{
    int i_ret;
    LIBVLC_FUNC;
    i_ret = libvlc_InternalAddIntf( p_libvlc, psz_module, b_block, b_play,
                                    0, NULL );
    LIBVLC_FUNC_END;
    return i_ret;
}


/*****************************************************************************
 * VLC_Die: ask vlc to die.
 *****************************************************************************
 * This function sets p_libvlc->b_die to VLC_TRUE, but does not do any other
 * task. It is your duty to call VLC_CleanUp and VLC_Destroy afterwards.
 *****************************************************************************/
int VLC_Die( int i_object )
{
    LIBVLC_FUNC;
    vlc_object_kill( p_libvlc );
    LIBVLC_FUNC_END;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * VLC_CleanUp: CleanUp all the intf, playlist, vout, aout
 *****************************************************************************/
int VLC_CleanUp( int i_object )
{
    int i_ret;
    LIBVLC_FUNC;
    i_ret = libvlc_InternalCleanup( p_libvlc );
    LIBVLC_FUNC_END;
    return i_ret;
}

/*****************************************************************************
 * VLC_Destroy: Destroy everything.
 *****************************************************************************
 * This function requests the running threads to finish, waits for their
 * termination, and destroys their structure.
 *****************************************************************************/
int VLC_Destroy( int i_object )
{
    LIBVLC_FUNC;
    return libvlc_InternalDestroy( p_libvlc, i_object ? VLC_TRUE : VLC_FALSE );
}

/*****************************************************************************
 * VLC_VariableSet: set a vlc variable
 *****************************************************************************/
int VLC_VariableSet( int i_object, char const *psz_var, vlc_value_t value )
{
    int i_ret;
    LIBVLC_FUNC;

    /* FIXME: Temporary hack for Mozilla, if variable starts with conf:: then
     * we handle it as a configuration variable. Don't tell Gildas :) -- sam */
    if( !strncmp( psz_var, "conf::", 6 ) )
    {
        module_config_t *p_item;
        char const *psz_newvar = psz_var + 6;

        p_item = config_FindConfig( VLC_OBJECT(p_libvlc), psz_newvar );

        if( p_item )
        {
            switch( p_item->i_type )
            {
                case CONFIG_ITEM_BOOL:
                    config_PutInt( p_libvlc, psz_newvar, value.b_bool );
                    break;
                case CONFIG_ITEM_INTEGER:
                    config_PutInt( p_libvlc, psz_newvar, value.i_int );
                    break;
                case CONFIG_ITEM_FLOAT:
                    config_PutFloat( p_libvlc, psz_newvar, value.f_float );
                    break;
                default:
                    config_PutPsz( p_libvlc, psz_newvar, value.psz_string );
                    break;
            }
            if( i_object ) vlc_object_release( p_libvlc );
            return VLC_SUCCESS;
        }
    }

    i_ret = var_Set( p_libvlc, psz_var, value );

    LIBVLC_FUNC_END;
    return i_ret;
}

/*****************************************************************************
 * VLC_VariableGet: get a vlc variable
 *****************************************************************************/
int VLC_VariableGet( int i_object, char const *psz_var, vlc_value_t *p_value )
{
    int i_ret;
    LIBVLC_FUNC;
    i_ret = var_Get( p_libvlc , psz_var, p_value );
    LIBVLC_FUNC_END;
    return i_ret;
}

/*****************************************************************************
 * VLC_VariableType: get a vlc variable type
 *****************************************************************************/
int VLC_VariableType( int i_object, char const *psz_var, int *pi_type )
{
    int i_type;
    LIBVLC_FUNC;
    /* FIXME: Temporary hack for Mozilla, if variable starts with conf:: then
     * we handle it as a configuration variable. Don't tell Gildas :) -- sam */
    if( !strncmp( psz_var, "conf::", 6 ) )
    {
        module_config_t *p_item;
        char const *psz_newvar = psz_var + 6;

        p_item = config_FindConfig( VLC_OBJECT(p_libvlc), psz_newvar );

        if( p_item )
        {
            switch( p_item->i_type )
            {
                case CONFIG_ITEM_BOOL:
                    i_type = VLC_VAR_BOOL;
                    break;
                case CONFIG_ITEM_INTEGER:
                    i_type = VLC_VAR_INTEGER;
                    break;
                case CONFIG_ITEM_FLOAT:
                    i_type = VLC_VAR_FLOAT;
                    break;
                default:
                    i_type = VLC_VAR_STRING;
                    break;
            }
        }
        else
            i_type = 0;
    }
    else
        i_type = VLC_VAR_TYPE & var_Type( p_libvlc , psz_var );

    LIBVLC_FUNC_END;

    if( i_type > 0 )
    {
        *pi_type = i_type;
        return VLC_SUCCESS;
    }
    return VLC_ENOVAR;
}

#define LIBVLC_PLAYLIST_FUNC \
    libvlc_int_t *p_libvlc = vlc_current_object( i_object );\
    if( !p_libvlc || !p_libvlc->p_playlist ) return VLC_ENOOBJ; \
    vlc_object_yield( p_libvlc->p_playlist );

#define LIBVLC_PLAYLIST_FUNC_END \
    vlc_object_release( p_libvlc->p_playlist ); \
    if( i_object ) vlc_object_release( p_libvlc );

/*****************************************************************************
 * VLC_AddTarget: adds a target for playing.
 *****************************************************************************
 * This function adds psz_target to the playlist
 *****************************************************************************/

int VLC_AddTarget( int i_object, char const *psz_target,
                   char const **ppsz_options, int i_options,
                   int i_mode, int i_pos )
{
    int i_err;
    LIBVLC_PLAYLIST_FUNC;
    i_err = playlist_AddExt( p_libvlc->p_playlist, psz_target,
                             NULL,  i_mode, i_pos, -1,
                             ppsz_options, i_options, VLC_TRUE, VLC_FALSE );
    LIBVLC_PLAYLIST_FUNC_END;
    return i_err;
}

/*****************************************************************************
 * VLC_Play: play the playlist
 *****************************************************************************/
int VLC_Play( int i_object )
{
    LIBVLC_PLAYLIST_FUNC;
    playlist_Play( p_libvlc->p_playlist );
    LIBVLC_PLAYLIST_FUNC_END;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * VLC_Pause: toggle pause
 *****************************************************************************/
int VLC_Pause( int i_object )
{
    LIBVLC_PLAYLIST_FUNC;
    playlist_Pause( p_libvlc->p_playlist );
    LIBVLC_PLAYLIST_FUNC_END;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * VLC_Stop: stop playback
 *****************************************************************************/
int VLC_Stop( int i_object )
{
    LIBVLC_PLAYLIST_FUNC;
    playlist_Stop( p_libvlc->p_playlist );
    LIBVLC_PLAYLIST_FUNC_END;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * VLC_IsPlaying: Query for Playlist Status
 *****************************************************************************/
vlc_bool_t VLC_IsPlaying( int i_object )
{
    vlc_bool_t   b_playing;

    LIBVLC_PLAYLIST_FUNC;
    if( p_libvlc->p_playlist->p_input )
    {
        vlc_value_t  val;
        var_Get( p_libvlc->p_playlist->p_input, "state", &val );
        b_playing = ( val.i_int == PLAYING_S );
    }
    else
    {
        b_playing = playlist_IsPlaying( p_libvlc->p_playlist );
    }
    LIBVLC_PLAYLIST_FUNC_END;
    return b_playing;
}

/**
 * Get the current position in a input
 *
 * Return the current position as a float
 * \note For some inputs, this will be unknown.
 *
 * \param i_object a vlc object id
 * \return a float in the range of 0.0 - 1.0
 */
float VLC_PositionGet( int i_object )
{
    input_thread_t *p_input;
    vlc_value_t val;
    LIBVLC_FUNC;

    p_input = vlc_object_find( p_libvlc, VLC_OBJECT_INPUT, FIND_CHILD );

    if( !p_input )
    {
        if( i_object ) vlc_object_release( p_libvlc );
        return VLC_ENOOBJ;
    }

    var_Get( p_input, "position", &val );
    vlc_object_release( p_input );

    LIBVLC_FUNC_END;
    return val.f_float;
}

/**
 * Set the current position in a input
 *
 * Set the current position in a input and then return
 * the current position as a float.
 * \note For some inputs, this will be unknown.
 *
 * \param i_object a vlc object id
 * \param i_position a float in the range of 0.0 - 1.0
 * \return a float in the range of 0.0 - 1.0
 */
float VLC_PositionSet( int i_object, float i_position )
{
    input_thread_t *p_input;
    vlc_value_t val;
    libvlc_int_t *p_libvlc = vlc_current_object( i_object );

    /* Check that the handle is valid */
    if( !p_libvlc )
    {
        return VLC_ENOOBJ;
    }

    p_input = vlc_object_find( p_libvlc, VLC_OBJECT_INPUT, FIND_CHILD );

    if( !p_input )
    {
        if( i_object ) vlc_object_release( p_libvlc );
        return VLC_ENOOBJ;
    }

    val.f_float = i_position;
    var_Set( p_input, "position", val );
    var_Get( p_input, "position", &val );
    vlc_object_release( p_input );

    if( i_object ) vlc_object_release( p_libvlc );
    return val.f_float;
}

/**
 * Get the current position in a input
 *
 * Return the current position in seconds from the start.
 * \note For some inputs, this will be unknown.
 *
 * \param i_object a vlc object id
 * \return the offset from 0:00 in seconds
 */
int VLC_TimeGet( int i_object )
{
    input_thread_t *p_input;
    vlc_value_t val;
    libvlc_int_t *p_libvlc = vlc_current_object( i_object );

    /* Check that the handle is valid */
    if( !p_libvlc )
    {
        return VLC_ENOOBJ;
    }

    p_input = vlc_object_find( p_libvlc, VLC_OBJECT_INPUT, FIND_CHILD );

    if( !p_input )
    {
        if( i_object ) vlc_object_release( p_libvlc );
        return VLC_ENOOBJ;
    }

    var_Get( p_input, "time", &val );
    vlc_object_release( p_input );

    if( i_object ) vlc_object_release( p_libvlc );
    return val.i_time  / 1000000;
}

/**
 * Seek to a position in the current input
 *
 * Seek i_seconds in the current input. If b_relative is set,
 * then the seek will be relative to the current position, otherwise
 * it will seek to i_seconds from the beginning of the input.
 * \note For some inputs, this will be unknown.
 *
 * \param i_object a vlc object id
 * \param i_seconds seconds from current position or from beginning of input
 * \param b_relative seek relative from current position
 * \return VLC_SUCCESS on success
 */
int VLC_TimeSet( int i_object, int i_seconds, vlc_bool_t b_relative )
{
    input_thread_t *p_input;
    vlc_value_t val;
    libvlc_int_t *p_libvlc = vlc_current_object( i_object );

    /* Check that the handle is valid */
    if( !p_libvlc )
    {
        return VLC_ENOOBJ;
    }

    p_input = vlc_object_find( p_libvlc, VLC_OBJECT_INPUT, FIND_CHILD );

    if( !p_input )
    {
        if( i_object ) vlc_object_release( p_libvlc );
        return VLC_ENOOBJ;
    }

    if( b_relative )
    {
        val.i_time = i_seconds;
        val.i_time = val.i_time * 1000000L;
        var_Set( p_input, "time-offset", val );
    }
    else
    {
        val.i_time = i_seconds;
        val.i_time = val.i_time * 1000000L;
        var_Set( p_input, "time", val );
    }
    vlc_object_release( p_input );

    if( i_object ) vlc_object_release( p_libvlc );
    return VLC_SUCCESS;
}

/**
 * Get the total length of a input
 *
 * Return the total length in seconds from the current input.
 * \note For some inputs, this will be unknown.
 *
 * \param i_object a vlc object id
 * \return the length in seconds
 */
int VLC_LengthGet( int i_object )
{
    input_thread_t *p_input;
    vlc_value_t val;
    libvlc_int_t *p_libvlc = vlc_current_object( i_object );

    /* Check that the handle is valid */
    if( !p_libvlc )
    {
        return VLC_ENOOBJ;
    }

    p_input = vlc_object_find( p_libvlc, VLC_OBJECT_INPUT, FIND_CHILD );

    if( !p_input )
    {
        if( i_object ) vlc_object_release( p_libvlc );
        return VLC_ENOOBJ;
    }

    var_Get( p_input, "length", &val );
    vlc_object_release( p_input );

    if( i_object ) vlc_object_release( p_libvlc );
    return val.i_time  / 1000000L;
}

/**
 * Play the input faster than realtime
 *
 * 2x, 4x, 8x faster than realtime
 * \note For some inputs, this will be impossible.
 *
 * \param i_object a vlc object id
 * \return the current speedrate
 */
float VLC_SpeedFaster( int i_object )
{
    input_thread_t *p_input;
    vlc_value_t val;
    libvlc_int_t *p_libvlc = vlc_current_object( i_object );

    /* Check that the handle is valid */
    if( !p_libvlc )
    {
        return VLC_ENOOBJ;
    }

    p_input = vlc_object_find( p_libvlc, VLC_OBJECT_INPUT, FIND_CHILD );

    if( !p_input )
    {
        if( i_object ) vlc_object_release( p_libvlc );
        return VLC_ENOOBJ;
    }

    val.b_bool = VLC_TRUE;
    var_Set( p_input, "rate-faster", val );
    var_Get( p_input, "rate", &val );
    vlc_object_release( p_input );

    if( i_object ) vlc_object_release( p_libvlc );
    return val.f_float / INPUT_RATE_DEFAULT;
}

/**
 * Play the input slower than realtime
 *
 * 1/2x, 1/4x, 1/8x slower than realtime
 * \note For some inputs, this will be impossible.
 *
 * \param i_object a vlc object id
 * \return the current speedrate
 */
float VLC_SpeedSlower( int i_object )
{
    input_thread_t *p_input;
    vlc_value_t val;
    libvlc_int_t *p_libvlc = vlc_current_object( i_object );

    /* Check that the handle is valid */
    if( !p_libvlc )
    {
        return VLC_ENOOBJ;
    }

    p_input = vlc_object_find( p_libvlc, VLC_OBJECT_INPUT, FIND_CHILD );

    if( !p_input )
    {
        if( i_object ) vlc_object_release( p_libvlc );
        return VLC_ENOOBJ;
    }

    val.b_bool = VLC_TRUE;
    var_Set( p_input, "rate-slower", val );
    var_Get( p_input, "rate", &val );
    vlc_object_release( p_input );

    if( i_object ) vlc_object_release( p_libvlc );
    return val.f_float / INPUT_RATE_DEFAULT;
}

/**
 * Return the current playlist item
 *
 * Returns the index of the playlistitem that is currently selected for play.
 * This is valid even if nothing is currently playing.
 *
 * \param i_object a vlc object id
 * \return the current index
 */
int VLC_PlaylistIndex( int i_object )
{
    (void)i_object;
    printf( "This function is deprecated and should not be used anymore" );
    return -1;
}

/**
 * Total number of items in the playlist
 *
 * \param i_object a vlc object id
 * \return amount of playlist items
 */
int VLC_PlaylistNumberOfItems( int i_object )
{
    int i_size;
    LIBVLC_PLAYLIST_FUNC;
    i_size = p_libvlc->p_playlist->items.i_size;
    LIBVLC_PLAYLIST_FUNC_END;
    return i_size;
}

/**
 * Go to next playlist item
 * \param i_object a vlc object id
 * \return VLC_SUCCESS on success
 */
int VLC_PlaylistNext( int i_object )
{
    LIBVLC_PLAYLIST_FUNC;
    playlist_Next( p_libvlc->p_playlist );
    LIBVLC_PLAYLIST_FUNC_END;
    return VLC_SUCCESS;
}

/**
 * Go to previous playlist item
 * \param i_object a vlc object id
 * \return VLC_SUCCESS on success
 */
int VLC_PlaylistPrev( int i_object )
{
    LIBVLC_PLAYLIST_FUNC;
    playlist_Prev( p_libvlc->p_playlist );
    LIBVLC_PLAYLIST_FUNC_END;
    return VLC_SUCCESS;
}

/**
 * Empty the playlist
 */
int VLC_PlaylistClear( int i_object )
{
    LIBVLC_PLAYLIST_FUNC;
    playlist_Clear( p_libvlc->p_playlist, VLC_TRUE );
    LIBVLC_PLAYLIST_FUNC_END;
    return VLC_SUCCESS;
}

/**
 * Change the volume
 *
 * \param i_object a vlc object id
 * \param i_volume something in a range from 0-200
 * \return the new volume (range 0-200 %)
 */
int VLC_VolumeSet( int i_object, int i_volume )
{
    audio_volume_t i_vol = 0;
    LIBVLC_FUNC;

    if( i_volume >= 0 && i_volume <= 200 )
    {
        i_vol = i_volume * AOUT_VOLUME_MAX / 200;
        aout_VolumeSet( p_libvlc, i_vol );
    }
    LIBVLC_FUNC_END;
    return i_vol * 200 / AOUT_VOLUME_MAX;
}

/**
 * Get the current volume
 *
 * Retrieve the current volume.
 *
 * \param i_object a vlc object id
 * \return the current volume (range 0-200 %)
 */
int VLC_VolumeGet( int i_object )
{
    audio_volume_t i_volume;
    LIBVLC_FUNC;
    aout_VolumeGet( p_libvlc, &i_volume );
    LIBVLC_FUNC_END;
    return i_volume*200/AOUT_VOLUME_MAX;
}

/**
 * Mute/Unmute the volume
 *
 * \param i_object a vlc object id
 * \return VLC_SUCCESS on success
 */
int VLC_VolumeMute( int i_object )
{
    LIBVLC_FUNC;
    aout_VolumeMute( p_libvlc, NULL );
    LIBVLC_FUNC_END;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * VLC_FullScreen: toggle fullscreen mode
 *****************************************************************************/
int VLC_FullScreen( int i_object )
{
    vout_thread_t *p_vout;
    LIBVLC_FUNC;
    p_vout = vlc_object_find( p_libvlc, VLC_OBJECT_VOUT, FIND_CHILD );

    if( !p_vout )
    {
        if( i_object ) vlc_object_release( p_libvlc );
        return VLC_ENOOBJ;
    }

    p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;
    vlc_object_release( p_vout );
    LIBVLC_FUNC_END;
    return VLC_SUCCESS;
}
