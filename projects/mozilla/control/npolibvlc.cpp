/*****************************************************************************
 * npolibvlc.cpp: official Javascript APIs
 *****************************************************************************
 * Copyright (C) 2002-2009 the VideoLAN team
 *
 * Authors: Damien Fouilleul <Damien.Fouilleul@laposte.net>
 *          JP Dinger <jpd@m2x.nl>
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

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Mozilla stuff */
#ifdef HAVE_MOZILLA_CONFIG_H
#   include <mozilla-config.h>
#endif

#include "vlcplugin.h"
#include "npolibvlc.h"

/*
** Local helper macros and function
*/
#define COUNTNAMES(a,b,c) const int a::b = sizeof(a::c)/sizeof(NPUTF8 *)
#define RETURN_ON_EXCEPTION(this,ex) \
    do { if( libvlc_exception_raised(&ex) ) \
    { \
        NPN_SetException(this, libvlc_errmsg()); \
        libvlc_exception_clear(&ex); \
        return INVOKERESULT_GENERIC_ERROR; \
    } } while(false)

/*
** implementation of libvlc root object
*/

LibvlcRootNPObject::~LibvlcRootNPObject()
{
    /*
    ** When the plugin is destroyed, firefox takes it upon itself to
    ** destroy all 'live' script objects and ignores refcounting.
    ** Therefore we cannot safely assume that refcounting will control
    ** lifespan of objects. Hence they are only lazily created on
    ** request, so that firefox can take ownership, and are not released
    ** when the plugin is destroyed.
    */
    if( isValid() )
    {
        if( audioObj    ) NPN_ReleaseObject(audioObj);
        if( inputObj    ) NPN_ReleaseObject(inputObj);
        if( playlistObj ) NPN_ReleaseObject(playlistObj);
        if( videoObj    ) NPN_ReleaseObject(videoObj);
    }
}

const NPUTF8 * const LibvlcRootNPObject::propertyNames[] =
{
    "audio",
    "input",
    "playlist",
    "subtitle",
    "video",
    "VersionInfo",
};
COUNTNAMES(LibvlcRootNPObject,propertyCount,propertyNames);

enum LibvlcRootNPObjectPropertyIds
{
    ID_root_audio = 0,
    ID_root_input,
    ID_root_playlist,
    ID_root_subtitle,
    ID_root_video,
    ID_root_VersionInfo,
};

RuntimeNPObject::InvokeResult
LibvlcRootNPObject::getProperty(int index, NPVariant &result)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        switch( index )
        {
            case ID_root_audio:
                InstantObj<LibvlcAudioNPObject>( audioObj );
                OBJECT_TO_NPVARIANT(NPN_RetainObject(audioObj), result);
                return INVOKERESULT_NO_ERROR;
            case ID_root_input:
                InstantObj<LibvlcInputNPObject>( inputObj );
                OBJECT_TO_NPVARIANT(NPN_RetainObject(inputObj), result);
                return INVOKERESULT_NO_ERROR;
            case ID_root_playlist:
                InstantObj<LibvlcPlaylistNPObject>( playlistObj );
                OBJECT_TO_NPVARIANT(NPN_RetainObject(playlistObj), result);
                return INVOKERESULT_NO_ERROR;
            case ID_root_subtitle:
                InstantObj<LibvlcSubtitleNPObject>( subtitleObj );
                OBJECT_TO_NPVARIANT(NPN_RetainObject(subtitleObj), result);
                return INVOKERESULT_NO_ERROR;
            case ID_root_video:
                InstantObj<LibvlcVideoNPObject>( videoObj );
                OBJECT_TO_NPVARIANT(NPN_RetainObject(videoObj), result);
                return INVOKERESULT_NO_ERROR;
            case ID_root_VersionInfo:
                return invokeResultString(libvlc_get_version(),result);
            default:
                ;
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

const NPUTF8 * const LibvlcRootNPObject::methodNames[] =
{
    "versionInfo",
};
COUNTNAMES(LibvlcRootNPObject,methodCount,methodNames);

enum LibvlcRootNPObjectMethodIds
{
    ID_root_versionInfo,
};

RuntimeNPObject::InvokeResult LibvlcRootNPObject::invoke(int index,
                  const NPVariant *args, uint32_t argCount, NPVariant &result)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        switch( index )
        {
            case ID_root_versionInfo:
                if( 0 != argCount )
                    return INVOKERESULT_NO_SUCH_METHOD;
                return invokeResultString(libvlc_get_version(),result);
            default:
                ;
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

/*
** implementation of libvlc audio object
*/

const NPUTF8 * const LibvlcAudioNPObject::propertyNames[] =
{
    "mute",
    "volume",
    "track",
    "count",
    "channel",
};
COUNTNAMES(LibvlcAudioNPObject,propertyCount,propertyNames);

enum LibvlcAudioNPObjectPropertyIds
{
    ID_audio_mute,
    ID_audio_volume,
    ID_audio_track,
    ID_audio_count,
    ID_audio_channel,
};

RuntimeNPObject::InvokeResult
LibvlcAudioNPObject::getProperty(int index, NPVariant &result)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        VlcPlugin* p_plugin = getPrivate<VlcPlugin>();
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        switch( index )
        {
            case ID_audio_mute:
            {
                bool muted = libvlc_audio_get_mute(p_plugin->getVLC());
                BOOLEAN_TO_NPVARIANT(muted, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_audio_volume:
            {
                int volume = libvlc_audio_get_volume(p_plugin->getVLC());
                INT32_TO_NPVARIANT(volume, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_audio_track:
            {
                libvlc_media_player_t *p_md = p_plugin->getMD(&ex);
                RETURN_ON_EXCEPTION(this,ex);
                int track = libvlc_audio_get_track(p_md, &ex);
                RETURN_ON_EXCEPTION(this,ex);
                INT32_TO_NPVARIANT(track, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_audio_count:
            {
                libvlc_media_player_t *p_md = p_plugin->getMD(&ex);
                RETURN_ON_EXCEPTION(this,ex);
                // get the number of audio track available
                int i_track = libvlc_audio_get_track_count(p_md, &ex);
                RETURN_ON_EXCEPTION(this,ex);
                // return it
                INT32_TO_NPVARIANT(i_track, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_audio_channel:
            {
                int channel = libvlc_audio_get_channel(p_plugin->getVLC(), &ex);
                RETURN_ON_EXCEPTION(this,ex);
                INT32_TO_NPVARIANT(channel, result);
                return INVOKERESULT_NO_ERROR;
            }
            default:
                ;
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

RuntimeNPObject::InvokeResult
LibvlcAudioNPObject::setProperty(int index, const NPVariant &value)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        VlcPlugin* p_plugin = getPrivate<VlcPlugin>();
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        switch( index )
        {
            case ID_audio_mute:
                if( NPVARIANT_IS_BOOLEAN(value) )
                {
                    libvlc_audio_set_mute(p_plugin->getVLC(),
                                          NPVARIANT_TO_BOOLEAN(value));
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_INVALID_VALUE;
            case ID_audio_volume:
                if( isNumberValue(value) )
                {
                    libvlc_audio_set_volume(p_plugin->getVLC(),
                                            numberValue(value), &ex);
                    RETURN_ON_EXCEPTION(this,ex);
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_INVALID_VALUE;
            case ID_audio_track:
                if( isNumberValue(value) )
                {
                    libvlc_media_player_t *p_md = p_plugin->getMD(&ex);
                    RETURN_ON_EXCEPTION(this,ex);
                    libvlc_audio_set_track(p_md, numberValue(value), &ex);
                    RETURN_ON_EXCEPTION(this,ex);
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_INVALID_VALUE;
            case ID_audio_channel:
                if( isNumberValue(value) )
                {
                    libvlc_audio_set_channel(p_plugin->getVLC(),
                                             numberValue(value), &ex);
                    RETURN_ON_EXCEPTION(this,ex);
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_INVALID_VALUE;
            default:
                ;
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

const NPUTF8 * const LibvlcAudioNPObject::methodNames[] =
{
    "toggleMute",
    "description",
};
COUNTNAMES(LibvlcAudioNPObject,methodCount,methodNames);

enum LibvlcAudioNPObjectMethodIds
{
    ID_audio_togglemute,
    ID_audio_description,
};

RuntimeNPObject::InvokeResult
LibvlcAudioNPObject::invoke(int index, const NPVariant *args,
                            uint32_t argCount, NPVariant &result)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        VlcPlugin* p_plugin = getPrivate<VlcPlugin>();
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        switch( index )
        {
            case ID_audio_togglemute:
                if( argCount == 0 )
                {
                    libvlc_audio_toggle_mute(p_plugin->getVLC());
                    VOID_TO_NPVARIANT(result);
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            case ID_audio_description:
            {
                if( argCount == 1)
                {
                    char *psz_name;
                    int i_trackID, i_limit, i;
                    libvlc_track_description_t *p_trackDesc;

                    libvlc_media_player_t *p_md = p_plugin->getMD(&ex);
                    RETURN_ON_EXCEPTION(this,ex);

                    /* get tracks description */
                    p_trackDesc = libvlc_audio_get_track_description(p_md, &ex);
                    RETURN_ON_EXCEPTION(this,ex);
                    if( !p_trackDesc )
                        return INVOKERESULT_GENERIC_ERROR;

                    /* get the number of track available */
                    i_limit = libvlc_audio_get_track_count(p_md, &ex);
                    RETURN_ON_EXCEPTION(this,ex);

                    /* check if a number is given by the user
                     * and get the track number */
                    if( isNumberValue(args[0]) )
                        i_trackID = numberValue(args[0]);
                    else
                        return INVOKERESULT_INVALID_VALUE;

                    /* if bad number is given return invalid value */
                    if ( ( i_trackID > ( i_limit - 1 ) ) || ( i_trackID < 0 ) )
                        return INVOKERESULT_INVALID_VALUE;

                    /* get the good trackDesc */
                    for( i = 0 ; i < i_trackID ; i++ )
                    {
                        p_trackDesc = p_trackDesc->p_next;
                    }
                    psz_name = p_trackDesc->psz_name;

                    /* display the name of the track chosen */
                    return invokeResultString( psz_name, result );
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            }
            default:
                ;
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

/*
** implementation of libvlc input object
*/

const NPUTF8 * const LibvlcInputNPObject::propertyNames[] =
{
    "length",
    "position",
    "time",
    "state",
    "rate",
    "fps",
    "hasVout",
};
COUNTNAMES(LibvlcInputNPObject,propertyCount,propertyNames);

enum LibvlcInputNPObjectPropertyIds
{
    ID_input_length,
    ID_input_position,
    ID_input_time,
    ID_input_state,
    ID_input_rate,
    ID_input_fps,
    ID_input_hasvout,
};

RuntimeNPObject::InvokeResult
LibvlcInputNPObject::getProperty(int index, NPVariant &result)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        VlcPlugin* p_plugin = getPrivate<VlcPlugin>();
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_media_player_t *p_md = p_plugin->getMD(&ex);
        if( libvlc_exception_raised(&ex) )
        {
            if( index != ID_input_state )
            {
                NPN_SetException(this, libvlc_errmsg());
                libvlc_exception_clear(&ex);
                return INVOKERESULT_GENERIC_ERROR;
            }
            else
            {
                /* for input state, return CLOSED rather than an exception */
                INT32_TO_NPVARIANT(0, result);
                libvlc_exception_clear(&ex);
                return INVOKERESULT_NO_ERROR;
            }
        }

        switch( index )
        {
            case ID_input_length:
            {
                double val = (double)libvlc_media_player_get_length(p_md, &ex);
                RETURN_ON_EXCEPTION(this,ex);
                DOUBLE_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_input_position:
            {
                double val = libvlc_media_player_get_position(p_md, &ex);
                RETURN_ON_EXCEPTION(this,ex);
                DOUBLE_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_input_time:
            {
                double val = (double)libvlc_media_player_get_time(p_md, &ex);
                RETURN_ON_EXCEPTION(this,ex);
                DOUBLE_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_input_state:
            {
                int val = libvlc_media_player_get_state(p_md, &ex);
                RETURN_ON_EXCEPTION(this,ex);
                INT32_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_input_rate:
            {
                float val = libvlc_media_player_get_rate(p_md, &ex);
                RETURN_ON_EXCEPTION(this,ex);
                DOUBLE_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_input_fps:
            {
                double val = libvlc_media_player_get_fps(p_md, &ex);
                RETURN_ON_EXCEPTION(this,ex);
                DOUBLE_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_input_hasvout:
            {
                bool val = p_plugin->player_has_vout(&ex);
                RETURN_ON_EXCEPTION(this,ex);
                BOOLEAN_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            default:
                ;
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

RuntimeNPObject::InvokeResult
LibvlcInputNPObject::setProperty(int index, const NPVariant &value)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        VlcPlugin* p_plugin = getPrivate<VlcPlugin>();
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_media_player_t *p_md = p_plugin->getMD(&ex);
        RETURN_ON_EXCEPTION(this,ex);

        switch( index )
        {
            case ID_input_position:
            {
                if( ! NPVARIANT_IS_DOUBLE(value) )
                {
                    return INVOKERESULT_INVALID_VALUE;
                }

                float val = (float)NPVARIANT_TO_DOUBLE(value);
                libvlc_media_player_set_position(p_md, val, &ex);
                RETURN_ON_EXCEPTION(this,ex);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_input_time:
            {
                int64_t val;
                if( NPVARIANT_IS_INT32(value) )
                    val = (int64_t)NPVARIANT_TO_INT32(value);
                else if( NPVARIANT_IS_DOUBLE(value) )
                    val = (int64_t)NPVARIANT_TO_DOUBLE(value);
                else
                {
                    return INVOKERESULT_INVALID_VALUE;
                }

                libvlc_media_player_set_time(p_md, val, &ex);
                RETURN_ON_EXCEPTION(this,ex);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_input_rate:
            {
                float val;
                if( NPVARIANT_IS_INT32(value) )
                    val = (float)NPVARIANT_TO_INT32(value);
                else if( NPVARIANT_IS_DOUBLE(value) )
                    val = (float)NPVARIANT_TO_DOUBLE(value);
                else
                {
                    return INVOKERESULT_INVALID_VALUE;
                }

                libvlc_media_player_set_rate(p_md, val, &ex);
                RETURN_ON_EXCEPTION(this,ex);
                return INVOKERESULT_NO_ERROR;
            }
            default:
                ;
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

const NPUTF8 * const LibvlcInputNPObject::methodNames[] =
{
    /* no methods */
    "none",
};
COUNTNAMES(LibvlcInputNPObject,methodCount,methodNames);

enum LibvlcInputNPObjectMethodIds
{
    ID_none,
};

RuntimeNPObject::InvokeResult
LibvlcInputNPObject::invoke(int index, const NPVariant *args,
                                    uint32_t argCount, NPVariant &result)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        switch( index )
        {
            case ID_none:
                return INVOKERESULT_NO_SUCH_METHOD;
            default:
                ;
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

/*
** implementation of libvlc playlist items object
*/

const NPUTF8 * const LibvlcPlaylistItemsNPObject::propertyNames[] =
{
    "count",
};
COUNTNAMES(LibvlcPlaylistItemsNPObject,propertyCount,propertyNames);

enum LibvlcPlaylistItemsNPObjectPropertyIds
{
    ID_playlistitems_count,
};

RuntimeNPObject::InvokeResult
LibvlcPlaylistItemsNPObject::getProperty(int index, NPVariant &result)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        VlcPlugin* p_plugin = getPrivate<VlcPlugin>();
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        switch( index )
        {
            case ID_playlistitems_count:
            {
                int val = p_plugin->playlist_count(&ex);
                RETURN_ON_EXCEPTION(this,ex);
                INT32_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            default:
                ;
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

const NPUTF8 * const LibvlcPlaylistItemsNPObject::methodNames[] =
{
    "clear",
    "remove",
};
COUNTNAMES(LibvlcPlaylistItemsNPObject,methodCount,methodNames);

enum LibvlcPlaylistItemsNPObjectMethodIds
{
    ID_playlistitems_clear,
    ID_playlistitems_remove,
};

RuntimeNPObject::InvokeResult
LibvlcPlaylistItemsNPObject::invoke(int index, const NPVariant *args,
                                    uint32_t argCount, NPVariant &result)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        VlcPlugin* p_plugin = getPrivate<VlcPlugin>();
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        switch( index )
        {
            case ID_playlistitems_clear:
                if( argCount == 0 )
                {
                    p_plugin->playlist_clear(&ex);
                    RETURN_ON_EXCEPTION(this,ex);
                    VOID_TO_NPVARIANT(result);
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            case ID_playlistitems_remove:
                if( (argCount == 1) && isNumberValue(args[0]) )
                {
                    p_plugin->playlist_delete_item(numberValue(args[0]),&ex);
                    RETURN_ON_EXCEPTION(this,ex);
                    VOID_TO_NPVARIANT(result);
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            default:
                ;
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

/*
** implementation of libvlc playlist object
*/

LibvlcPlaylistNPObject::~LibvlcPlaylistNPObject()
{
    // Why the isValid()?
    if( isValid() && playlistItemsObj )
        NPN_ReleaseObject(playlistItemsObj);
};

const NPUTF8 * const LibvlcPlaylistNPObject::propertyNames[] =
{
    "itemCount", /* deprecated */
    "isPlaying",
    "items",
};
COUNTNAMES(LibvlcPlaylistNPObject,propertyCount,propertyNames);

enum LibvlcPlaylistNPObjectPropertyIds
{
    ID_playlist_itemcount,
    ID_playlist_isplaying,
    ID_playlist_items,
};

RuntimeNPObject::InvokeResult
LibvlcPlaylistNPObject::getProperty(int index, NPVariant &result)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        VlcPlugin* p_plugin = getPrivate<VlcPlugin>();
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        switch( index )
        {
            case ID_playlist_itemcount: /* deprecated */
            {
                int val = p_plugin->playlist_count(&ex);
                RETURN_ON_EXCEPTION(this,ex);
                INT32_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_playlist_isplaying:
            {
                int val = p_plugin->playlist_isplaying(&ex);
                RETURN_ON_EXCEPTION(this,ex);
                BOOLEAN_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_playlist_items:
            {
                InstantObj<LibvlcPlaylistItemsNPObject>( playlistItemsObj );
                OBJECT_TO_NPVARIANT(NPN_RetainObject(playlistItemsObj), result);
                return INVOKERESULT_NO_ERROR;
            }
            default:
                ;
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

const NPUTF8 * const LibvlcPlaylistNPObject::methodNames[] =
{
    "add",
    "play",
    "playItem",
    "togglePause",
    "stop",
    "next",
    "prev",
    "clear", /* deprecated */
    "removeItem", /* deprecated */
};
COUNTNAMES(LibvlcPlaylistNPObject,methodCount,methodNames);

enum LibvlcPlaylistNPObjectMethodIds
{
    ID_playlist_add,
    ID_playlist_play,
    ID_playlist_playItem,
    ID_playlist_togglepause,
    ID_playlist_stop,
    ID_playlist_next,
    ID_playlist_prev,
    ID_playlist_clear,
    ID_playlist_removeitem
};

RuntimeNPObject::InvokeResult
LibvlcPlaylistNPObject::invoke(int index, const NPVariant *args,
                               uint32_t argCount, NPVariant &result)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        VlcPlugin* p_plugin = getPrivate<VlcPlugin>();
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        switch( index )
        {
            // XXX FIXME this needs squashing into something much smaller
            case ID_playlist_add:
            {
                if( (argCount < 1) || (argCount > 3) )
                    return INVOKERESULT_NO_SUCH_METHOD;
                if( !NPVARIANT_IS_STRING(args[0]) )
                    return INVOKERESULT_NO_SUCH_METHOD;

                // grab URL
                if( NPVARIANT_IS_NULL(args[0]) )
                    return INVOKERESULT_NO_SUCH_METHOD;

                char *s = stringValue(NPVARIANT_TO_STRING(args[0]));
                if( !s )
                    return INVOKERESULT_OUT_OF_MEMORY;

                char *url = p_plugin->getAbsoluteURL(s);
                if( url )
                    free(s);
                else
                    // problem with combining url, use argument
                    url = s;

                char *name = NULL;

                // grab name if available
                if( argCount > 1 )
                {
                    if( NPVARIANT_IS_NULL(args[1]) )
                    {
                        // do nothing
                    }
                    else if( NPVARIANT_IS_STRING(args[1]) )
                    {
                        name = stringValue(NPVARIANT_TO_STRING(args[1]));
                    }
                    else
                    {
                        free(url);
                        return INVOKERESULT_INVALID_VALUE;
                    }
                }

                int i_options = 0;
                char** ppsz_options = NULL;

                // grab options if available
                if( argCount > 2 )
                {
                    if( NPVARIANT_IS_NULL(args[2]) )
                    {
                        // do nothing
                    }
                    else if( NPVARIANT_IS_STRING(args[2]) )
                    {
                        parseOptions(NPVARIANT_TO_STRING(args[2]),
                                     &i_options, &ppsz_options);

                    }
                    else if( NPVARIANT_IS_OBJECT(args[2]) )
                    {
                        parseOptions(NPVARIANT_TO_OBJECT(args[2]),
                                     &i_options, &ppsz_options);
                    }
                    else
                    {
                        free(url);
                        free(name);
                        return INVOKERESULT_INVALID_VALUE;
                    }
                }

                int item = p_plugin->playlist_add_extended_untrusted(url, name,
                      i_options, const_cast<const char **>(ppsz_options), &ex);
                free(url);
                free(name);
                for( int i=0; i< i_options; ++i )
                {
                    free(ppsz_options[i]);
                }
                free(ppsz_options);

                RETURN_ON_EXCEPTION(this,ex);
                INT32_TO_NPVARIANT(item, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_playlist_play:
                if( argCount == 0 )
                {
                    p_plugin->playlist_play(&ex);
                    RETURN_ON_EXCEPTION(this,ex);
                    VOID_TO_NPVARIANT(result);
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            case ID_playlist_playItem:
                if( (argCount == 1) && isNumberValue(args[0]) )
                {
                    p_plugin->playlist_play_item(numberValue(args[0]),&ex);
                    RETURN_ON_EXCEPTION(this,ex);
                    VOID_TO_NPVARIANT(result);
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            case ID_playlist_togglepause:
                if( argCount == 0 )
                {
                    p_plugin->playlist_pause(&ex);
                    RETURN_ON_EXCEPTION(this,ex);
                    VOID_TO_NPVARIANT(result);
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            case ID_playlist_stop:
                if( argCount == 0 )
                {
                    p_plugin->playlist_stop(&ex);
                    RETURN_ON_EXCEPTION(this,ex);
                    VOID_TO_NPVARIANT(result);
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            case ID_playlist_next:
                if( argCount == 0 )
                {
                    p_plugin->playlist_next(&ex);
                    RETURN_ON_EXCEPTION(this,ex);
                    VOID_TO_NPVARIANT(result);
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            case ID_playlist_prev:
                if( argCount == 0 )
                {
                    p_plugin->playlist_prev(&ex);
                    RETURN_ON_EXCEPTION(this,ex);
                    VOID_TO_NPVARIANT(result);
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            case ID_playlist_clear: /* deprecated */
                if( argCount == 0 )
                {
                    p_plugin->playlist_clear(&ex);
                    RETURN_ON_EXCEPTION(this,ex);
                    VOID_TO_NPVARIANT(result);
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            case ID_playlist_removeitem: /* deprecated */
                if( (argCount == 1) && isNumberValue(args[0]) )
                {
                    p_plugin->playlist_delete_item(numberValue(args[0]), &ex);
                    RETURN_ON_EXCEPTION(this,ex);
                    VOID_TO_NPVARIANT(result);
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            default:
                ;
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

// XXX FIXME The new playlist_add creates a media instance and feeds it
// XXX FIXME these options one at a time, so this hunk of code does lots
// XXX FIXME of unnecessairy work. Break out something that can do one
// XXX FIXME option at a time and doesn't need to realloc().
// XXX FIXME Same for the other version of parseOptions.

void LibvlcPlaylistNPObject::parseOptions(const NPString &nps,
                                         int *i_options, char*** ppsz_options)
{
    if( nps.utf8length )
    {
        char *s = stringValue(nps);
        char *val = s;
        if( val )
        {
            long capacity = 16;
            char **options = (char **)malloc(capacity*sizeof(char *));
            if( options )
            {
                int nOptions = 0;

                char *end = val + nps.utf8length;
                while( val < end )
                {
                    // skip leading blanks
                    while( (val < end)
                        && ((*val == ' ' ) || (*val == '\t')) )
                        ++val;

                    char *start = val;
                    // skip till we get a blank character
                    while( (val < end)
                        && (*val != ' ' )
                        && (*val != '\t') )
                    {
                        char c = *(val++);
                        if( ('\'' == c) || ('"' == c) )
                        {
                            // skip till end of string
                            while( (val < end) && (*(val++) != c ) );
                        }
                    }

                    if( val > start )
                    {
                        if( nOptions == capacity )
                        {
                            capacity += 16;
                            char **moreOptions = (char **)realloc(options, capacity*sizeof(char*));
                            if( ! moreOptions )
                            {
                                /* failed to allocate more memory */
                                free(s);
                                /* return what we got so far */
                                *i_options = nOptions;
                                *ppsz_options = options;
                                return;
                            }
                            options = moreOptions;
                        }
                        *(val++) = '\0';
                        options[nOptions++] = strdup(start);
                    }
                    else
                        // must be end of string
                        break;
                }
                *i_options = nOptions;
                *ppsz_options = options;
            }
            free(s);
        }
    }
}

// XXX FIXME See comment at the other parseOptions variant.
void LibvlcPlaylistNPObject::parseOptions(NPObject *obj, int *i_options,
                                          char*** ppsz_options)
{
    /* WARNING: Safari does not implement NPN_HasProperty/NPN_HasMethod */

    NPVariant value;

    /* we are expecting to have a Javascript Array object */
    NPIdentifier propId = NPN_GetStringIdentifier("length");
    if( NPN_GetProperty(_instance, obj, propId, &value) )
    {
        int count = numberValue(value);
        NPN_ReleaseVariantValue(&value);

        if( count )
        {
            long capacity = 16;
            char **options = (char **)malloc(capacity*sizeof(char *));
            if( options )
            {
                int nOptions = 0;

                while( nOptions < count )
                {
                    propId = NPN_GetIntIdentifier(nOptions);
                    if( ! NPN_GetProperty(_instance, obj, propId, &value) )
                        /* return what we got so far */
                        break;

                    if( ! NPVARIANT_IS_STRING(value) )
                    {
                        /* return what we got so far */
                        NPN_ReleaseVariantValue(&value);
                        break;
                    }

                    if( nOptions == capacity )
                    {
                        capacity += 16;
                        char **moreOptions = (char **)realloc(options, capacity*sizeof(char*));
                        if( ! moreOptions )
                        {
                            /* failed to allocate more memory */
                            NPN_ReleaseVariantValue(&value);
                            /* return what we got so far */
                            *i_options = nOptions;
                            *ppsz_options = options;
                            break;
                        }
                        options = moreOptions;
                    }

                    options[nOptions++] = stringValue(value);
                    NPN_ReleaseVariantValue(&value);
                }
                *i_options = nOptions;
                *ppsz_options = options;
            }
        }
    }
}

/*
** implementation of libvlc subtitle object
*/

const NPUTF8 * const LibvlcSubtitleNPObject::propertyNames[] =
{
    "track",
    "count",
};

enum LibvlcSubtitleNPObjectPropertyIds
{
    ID_subtitle_track,
    ID_subtitle_count,
};
COUNTNAMES(LibvlcSubtitleNPObject,propertyCount,propertyNames);

RuntimeNPObject::InvokeResult
LibvlcSubtitleNPObject::getProperty(int index, NPVariant &result)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        VlcPlugin* p_plugin = getPrivate<VlcPlugin>();
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_media_player_t *p_md = p_plugin->getMD(&ex);
        RETURN_ON_EXCEPTION(this,ex);

        switch( index )
        {
            case ID_subtitle_track:
            {
                /* get the current subtitle ID */
                int i_spu = libvlc_video_get_spu(p_md, &ex);
                RETURN_ON_EXCEPTION(this,ex);
                /* return it */
                INT32_TO_NPVARIANT(i_spu, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_subtitle_count:
            {
                /* get the number of subtitles available */
                int i_spu = libvlc_video_get_spu_count(p_md, &ex);
                RETURN_ON_EXCEPTION(this,ex);
                /* return it */
                INT32_TO_NPVARIANT(i_spu, result);
                return INVOKERESULT_NO_ERROR;
            }
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

RuntimeNPObject::InvokeResult
LibvlcSubtitleNPObject::setProperty(int index, const NPVariant &value)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        VlcPlugin* p_plugin = getPrivate<VlcPlugin>();
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_media_player_t *p_md = p_plugin->getMD(&ex);
        RETURN_ON_EXCEPTION(this,ex);

        switch( index )
        {
            case ID_subtitle_track:
            {
                if( isNumberValue(value) )
                {
                    /* set the new subtitle track to show */
                    libvlc_video_set_spu(p_md, numberValue(value), &ex);
                    RETURN_ON_EXCEPTION(this,ex);

                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_INVALID_VALUE;
            }
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

const NPUTF8 * const LibvlcSubtitleNPObject::methodNames[] =
{
    "description"
};
COUNTNAMES(LibvlcSubtitleNPObject,methodCount,methodNames);

enum LibvlcSubtitleNPObjectMethodIds
{
    ID_subtitle_description
};

RuntimeNPObject::InvokeResult
LibvlcSubtitleNPObject::invoke(int index, const NPVariant *args,
                            uint32_t argCount, NPVariant &result)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        VlcPlugin* p_plugin = getPrivate<VlcPlugin>();
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_media_player_t *p_md = p_plugin->getMD(&ex);
        RETURN_ON_EXCEPTION(this,ex);

        switch( index )
        {
            case ID_subtitle_description:
            {
                if( argCount == 1)
                {
                    char *psz_name;
                    int i_spuID, i_limit, i;
                    libvlc_track_description_t *p_spuDesc;

                    /* get subtitles description */
                    p_spuDesc = libvlc_video_get_spu_description(p_md, &ex);
                    RETURN_ON_EXCEPTION(this,ex);
                    if( !p_spuDesc )
                        return INVOKERESULT_GENERIC_ERROR;

                    /* get the number of subtitle available */
                    i_limit = libvlc_video_get_spu_count(p_md, &ex);
                    RETURN_ON_EXCEPTION(this,ex);

                    /* check if a number is given by the user
                     * and get the subtitle number */
                    if( isNumberValue(args[0]) )
                        i_spuID = numberValue(args[0]);
                    else
                        return INVOKERESULT_INVALID_VALUE;

                    /* if bad number is given return invalid value */
                    if ( ( i_spuID > ( i_limit -1 ) ) || ( i_spuID < 0 ) )
                        return INVOKERESULT_INVALID_VALUE;

                    /* get the good spuDesc */
                    for( i = 0 ; i < i_spuID ; i++ )
                    {
                        p_spuDesc = p_spuDesc->p_next;
                    }
                    psz_name = p_spuDesc->psz_name;

                    /* return the name of the track chosen */
                    return invokeResultString(psz_name, result);
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            }
            default:
                return INVOKERESULT_NO_SUCH_METHOD;
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

/*
** implementation of libvlc video object
*/

const NPUTF8 * const LibvlcVideoNPObject::propertyNames[] =
{
    "fullscreen",
    "height",
    "width",
    "aspectRatio",
    "subtitle",
    "crop",
    "teletext",
    "marquee"
};

enum LibvlcVideoNPObjectPropertyIds
{
    ID_video_fullscreen,
    ID_video_height,
    ID_video_width,
    ID_video_aspectratio,
    ID_video_subtitle,
    ID_video_crop,
    ID_video_teletext,
    ID_video_marquee
};
COUNTNAMES(LibvlcVideoNPObject,propertyCount,propertyNames);

RuntimeNPObject::InvokeResult
LibvlcVideoNPObject::getProperty(int index, NPVariant &result)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        VlcPlugin* p_plugin = getPrivate<VlcPlugin>();
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_media_player_t *p_md = p_plugin->getMD(&ex);
        RETURN_ON_EXCEPTION(this,ex);

        switch( index )
        {
            case ID_video_fullscreen:
            {
                int val = p_plugin->get_fullscreen(&ex);
                RETURN_ON_EXCEPTION(this,ex);
                BOOLEAN_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_height:
            {
                int val = libvlc_video_get_height(p_md, &ex);
                RETURN_ON_EXCEPTION(this,ex);
                INT32_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_width:
            {
                int val = libvlc_video_get_width(p_md, &ex);
                RETURN_ON_EXCEPTION(this,ex);
                INT32_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_aspectratio:
            {
                NPUTF8 *psz_aspect = libvlc_video_get_aspect_ratio(p_md, &ex);
                RETURN_ON_EXCEPTION(this,ex);
                if( !psz_aspect )
                    return INVOKERESULT_GENERIC_ERROR;

                STRINGZ_TO_NPVARIANT(psz_aspect, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_subtitle:
            {
                int i_spu = libvlc_video_get_spu(p_md, &ex);
                RETURN_ON_EXCEPTION(this,ex);
                INT32_TO_NPVARIANT(i_spu, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_crop:
            {
                NPUTF8 *psz_geometry = libvlc_video_get_crop_geometry(p_md, &ex);
                RETURN_ON_EXCEPTION(this,ex);
                if( !psz_geometry )
                    return INVOKERESULT_GENERIC_ERROR;

                STRINGZ_TO_NPVARIANT(psz_geometry, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_teletext:
            {
                int i_page = libvlc_video_get_teletext(p_md, &ex);
                RETURN_ON_EXCEPTION(this,ex);
                INT32_TO_NPVARIANT(i_page, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_marquee:
            {
                InstantObj<LibvlcMarqueeNPObject>( marqueeObj );
                OBJECT_TO_NPVARIANT(NPN_RetainObject(marqueeObj), result);
                return INVOKERESULT_NO_ERROR;
            }
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

RuntimeNPObject::InvokeResult
LibvlcVideoNPObject::setProperty(int index, const NPVariant &value)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        VlcPlugin* p_plugin = getPrivate<VlcPlugin>();
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_media_player_t *p_md = p_plugin->getMD(&ex);
        RETURN_ON_EXCEPTION(this,ex);

        switch( index )
        {
            case ID_video_fullscreen:
            {
                if( ! NPVARIANT_IS_BOOLEAN(value) )
                {
                    return INVOKERESULT_INVALID_VALUE;
                }

                int val = NPVARIANT_TO_BOOLEAN(value);
                p_plugin->set_fullscreen(val, &ex);
                RETURN_ON_EXCEPTION(this,ex);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_aspectratio:
            {
                char *psz_aspect = NULL;

                if( ! NPVARIANT_IS_STRING(value) )
                {
                    return INVOKERESULT_INVALID_VALUE;
                }

                psz_aspect = stringValue(NPVARIANT_TO_STRING(value));
                if( !psz_aspect )
                {
                    return INVOKERESULT_GENERIC_ERROR;
                }

                libvlc_video_set_aspect_ratio(p_md, psz_aspect, &ex);
                free(psz_aspect);
                RETURN_ON_EXCEPTION(this,ex);

                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_subtitle:
            {
                if( isNumberValue(value) )
                {
                    libvlc_video_set_spu(p_md, numberValue(value), &ex);
                    RETURN_ON_EXCEPTION(this,ex);

                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_INVALID_VALUE;
            }
            case ID_video_crop:
            {
                char *psz_geometry = NULL;

                if( ! NPVARIANT_IS_STRING(value) )
                {
                    return INVOKERESULT_INVALID_VALUE;
                }

                psz_geometry = stringValue(NPVARIANT_TO_STRING(value));
                if( !psz_geometry )
                {
                    return INVOKERESULT_GENERIC_ERROR;
                }

                libvlc_video_set_crop_geometry(p_md, psz_geometry, &ex);
                free(psz_geometry);
                RETURN_ON_EXCEPTION(this,ex);

                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_teletext:
            {
                if( isNumberValue(value) )
                {
                    libvlc_video_set_teletext(p_md, numberValue(value), &ex);
                    RETURN_ON_EXCEPTION(this,ex);

                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_INVALID_VALUE;
            }
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

const NPUTF8 * const LibvlcVideoNPObject::methodNames[] =
{
    "toggleFullscreen",
    "toggleTeletext",
    "deinterlaceEnable",
    "deinterlaceDisable"
};
COUNTNAMES(LibvlcVideoNPObject,methodCount,methodNames);

enum LibvlcVideoNPObjectMethodIds
{
    ID_video_togglefullscreen,
    ID_video_toggleteletext,
    ID_video_deinterlaceenable,
    ID_video_deinterlacedisable
};

RuntimeNPObject::InvokeResult
LibvlcVideoNPObject::invoke(int index, const NPVariant *args,
                            uint32_t argCount, NPVariant &result)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        VlcPlugin* p_plugin = getPrivate<VlcPlugin>();
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_media_player_t *p_md = p_plugin->getMD(&ex);
        RETURN_ON_EXCEPTION(this,ex);

        switch( index )
        {
            case ID_video_togglefullscreen:
            {
                if( argCount == 0 )
                {
                    p_plugin->toggle_fullscreen(&ex);
                    RETURN_ON_EXCEPTION(this,ex);
                    VOID_TO_NPVARIANT(result);
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            }
            case ID_video_toggleteletext:
            {
                if( argCount == 0 )
                {
                    libvlc_toggle_teletext(p_md, &ex);
                    RETURN_ON_EXCEPTION(this,ex);
                    VOID_TO_NPVARIANT(result);
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            }
            case ID_video_deinterlacedisable:
            {
                libvlc_video_set_deinterlace(p_md, 0, "", &ex);
                RETURN_ON_EXCEPTION(this,ex);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_deinterlaceenable:
            {
                if(argCount == 1)
                {
                    if( NPVARIANT_IS_STRING( args[0] ) )
                    {
                        /* get deinterlace mode from the user */
                        char *psz_mode = stringValue( NPVARIANT_TO_STRING( args[0] ) );
                        /* enable deinterlace filter if possible */
                        libvlc_video_set_deinterlace(p_md, 1, psz_mode, &ex);
                        free(psz_mode);
                        RETURN_ON_EXCEPTION(this,ex);
                        return INVOKERESULT_NO_ERROR;
                    }
                    else
                    {
                        return INVOKERESULT_INVALID_VALUE;
                    }
                }
            }
            default:
                return INVOKERESULT_NO_SUCH_METHOD;
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

/*
** implementation of libvlc marquee object
*/

const NPUTF8 * const LibvlcMarqueeNPObject::propertyNames[] =
{
};

enum LibvlcMarqueeNPObjectPropertyIds
{
};
COUNTNAMES(LibvlcMarqueeNPObject,propertyCount,propertyNames);

RuntimeNPObject::InvokeResult
LibvlcMarqueeNPObject::getProperty(int index, NPVariant &result)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        VlcPlugin* p_plugin = getPrivate<VlcPlugin>();
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_media_player_t *p_md = p_plugin->getMD(&ex);
        RETURN_ON_EXCEPTION(this,ex);

        switch( index )
        {
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

RuntimeNPObject::InvokeResult
LibvlcMarqueeNPObject::setProperty(int index, const NPVariant &value)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        VlcPlugin* p_plugin = getPrivate<VlcPlugin>();
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_media_player_t *p_md = p_plugin->getMD(&ex);
        RETURN_ON_EXCEPTION(this,ex);

        switch( index )
        {
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

const NPUTF8 * const LibvlcMarqueeNPObject::methodNames[] =
{
    "enable",
    "disable",
    "color",
    "opacity",
    "position",
    "refresh",
    "size",
    "text",
    "timeout",
    "x",
    "y"
};
COUNTNAMES(LibvlcMarqueeNPObject,methodCount,methodNames);

enum LibvlcMarqueeNPObjectMethodIds
{
    ID_marquee_enable,
    ID_marquee_disable,
    ID_marquee_color,
    ID_marquee_opacity,
    ID_marquee_position,
    ID_marquee_refresh,
    ID_marquee_size,
    ID_marquee_text,
    ID_marquee_timeout,
    ID_marquee_x,
    ID_marquee_y
};

RuntimeNPObject::InvokeResult
LibvlcMarqueeNPObject::invoke(int index, const NPVariant *args,
                            uint32_t argCount, NPVariant &result)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        VlcPlugin* p_plugin = getPrivate<VlcPlugin>();
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_media_player_t *p_md = p_plugin->getMD(&ex);
        RETURN_ON_EXCEPTION(this,ex);

        switch( index )
        {
            case ID_marquee_enable:
            {
                libvlc_video_set_marquee_option_as_int(p_md, libvlc_marquee_Enabled, true, &ex);
                    RETURN_ON_EXCEPTION(this,ex);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_marquee_disable:
            {
                libvlc_video_set_marquee_option_as_int(p_md, libvlc_marquee_Enabled, false, &ex);
                    RETURN_ON_EXCEPTION(this,ex);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_marquee_color:
            {
                if( argCount == 1)
                {
                    if( NPVARIANT_IS_INT32( args[0] ) )
                    {
                        int i_color = NPVARIANT_TO_INT32( args[0] );
                        libvlc_video_set_marquee_option_as_int(p_md, libvlc_marquee_Color, i_color, &ex);
                            RETURN_ON_EXCEPTION(this,ex);
                        return INVOKERESULT_NO_ERROR;
                    }
                    else
                        return INVOKERESULT_GENERIC_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            }
            case ID_marquee_opacity:
            {
                if( argCount == 1)
                {
                    if( NPVARIANT_IS_INT32( args[0] ) )
                    {
                        int i_opacity = NPVARIANT_TO_INT32( args[0] );
                        libvlc_video_set_marquee_option_as_int(p_md, libvlc_marquee_Opacity, i_opacity, &ex);
                            RETURN_ON_EXCEPTION(this,ex);
                        return INVOKERESULT_NO_ERROR;
                    }
                    else
                        return INVOKERESULT_GENERIC_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            }
            case ID_marquee_position:
            {
                if( argCount == 1)
                {
                    if( NPVARIANT_IS_INT32( args[0] ) )
                    {
                        int i_position = NPVARIANT_TO_INT32( args[0] );
                        libvlc_video_set_marquee_option_as_int(p_md, libvlc_marquee_Position, i_position, &ex);
                            RETURN_ON_EXCEPTION(this,ex);
                        return INVOKERESULT_NO_ERROR;
                    }
                    else
                        return INVOKERESULT_GENERIC_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            }
            case ID_marquee_refresh:
            {
                if( argCount == 1)
                {
                    if( NPVARIANT_IS_INT32( args[0] ) )
                    {
                        int i_refresh = NPVARIANT_TO_INT32( args[0] );
                        libvlc_video_set_marquee_option_as_int(p_md, libvlc_marquee_Refresh, i_refresh, &ex);
                            RETURN_ON_EXCEPTION(this,ex);
                        return INVOKERESULT_NO_ERROR;
                    }
                    else
                        return INVOKERESULT_GENERIC_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            }
            case ID_marquee_size:
            {
                if( argCount == 1)
                {
                    if( NPVARIANT_IS_INT32( args[0] ) )
                    {
                        int i_size = NPVARIANT_TO_INT32( args[0] );
                        libvlc_video_set_marquee_option_as_int(p_md, libvlc_marquee_Size, i_size, &ex);
                            RETURN_ON_EXCEPTION(this,ex);
                        return INVOKERESULT_NO_ERROR;
                    }
                    else
                        return INVOKERESULT_GENERIC_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            }
            case ID_marquee_text:
            {
                if( argCount == 1)
                {
                    if( NPVARIANT_IS_STRING( args[0] ) )
                    {
                        char *psz_text = stringValue( NPVARIANT_TO_STRING( args[0] ) );
                        libvlc_video_set_marquee_option_as_string(p_md, libvlc_marquee_Text, psz_text, &ex);
                            RETURN_ON_EXCEPTION(this,ex);
                        free(psz_text);
                        return INVOKERESULT_NO_ERROR;
                    }
                    else
                        return INVOKERESULT_GENERIC_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            }
            case ID_marquee_timeout:
            {
                if( argCount == 1)
                {
                    if( NPVARIANT_IS_INT32( args[0] ) )
                    {
                        int i_timeout = NPVARIANT_TO_INT32( args[0] );
                        libvlc_video_set_marquee_option_as_int(p_md, libvlc_marquee_Timeout, i_timeout, &ex);
                            RETURN_ON_EXCEPTION(this,ex);
                        return INVOKERESULT_NO_ERROR;
                    }
                    else
                        return INVOKERESULT_GENERIC_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            }
            case ID_marquee_x:
            {
                if( argCount == 1)
                {
                    if( NPVARIANT_IS_INT32( args[0] ) )
                    {
                        int i_x = NPVARIANT_TO_INT32( args[0] );
                        libvlc_video_set_marquee_option_as_int(p_md, libvlc_marquee_X, i_x, &ex);
                            RETURN_ON_EXCEPTION(this,ex);
                        return INVOKERESULT_NO_ERROR;
                    }
                    else
                        return INVOKERESULT_GENERIC_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            }
            case ID_marquee_y:
            {
                if( argCount == 1)
                {
                    if( NPVARIANT_IS_INT32( args[0] ) )
                    {
                        int i_y = NPVARIANT_TO_INT32( args[0] );
                        libvlc_video_set_marquee_option_as_int(p_md, libvlc_marquee_Y, i_y, &ex);
                            RETURN_ON_EXCEPTION(this,ex);
                        return INVOKERESULT_NO_ERROR;
                    }
                    else
                        return INVOKERESULT_GENERIC_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            }
            default:
                return INVOKERESULT_NO_SUCH_METHOD;
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}
