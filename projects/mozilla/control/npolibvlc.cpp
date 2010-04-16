/*****************************************************************************
 * npolibvlc.cpp: official Javascript APIs
 *****************************************************************************
 * Copyright (C) 2002-2009 the VideoLAN team
 * Copyright (C) 2010 M2X BV
 *
 * Authors: Damien Fouilleul <Damien.Fouilleul@laposte.net>
 *          JP Dinger <jpd@videolan.org>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "vlcplugin.h"
#include "npolibvlc.h"

#include "position.h"

/*
** Local helper macros and function
*/
#define COUNTNAMES(a,b,c) const int a::b = sizeof(a::c)/sizeof(NPUTF8 *)
#define RETURN_ON_ERROR                             \
    do {                                            \
        NPN_SetException(this, libvlc_errmsg());    \
        return INVOKERESULT_GENERIC_ERROR;          \
    }while(0)

#define ERROR_EVENT_NOT_FOUND "ERROR: One or more events could not be found."
#define ERROR_API_VERSION "ERROR: NPAPI version not high enough. (Gecko >= 1.9 needed)"

// Make a copy of an NPVariant.
NPVariant copyNPVariant(const NPVariant& original)
{
    NPVariant res;

    if (NPVARIANT_IS_STRING(original))
        STRINGZ_TO_NPVARIANT(strdup(NPVARIANT_TO_STRING(original).UTF8Characters), res);
    else if (NPVARIANT_IS_INT32(original))
        INT32_TO_NPVARIANT(NPVARIANT_TO_INT32(original), res);
    else if (NPVARIANT_IS_DOUBLE(original))
        DOUBLE_TO_NPVARIANT(NPVARIANT_TO_DOUBLE(original), res);
    else if (NPVARIANT_IS_OBJECT(original))
    {
        NPObject *obj = NPVARIANT_TO_OBJECT(original);
        NPN_RetainObject(obj);
        OBJECT_TO_NPVARIANT(obj, res);
    }
    else if (NPVARIANT_IS_BOOLEAN(original))
        BOOLEAN_TO_NPVARIANT(NPVARIANT_TO_BOOLEAN(original), res);

    return res;
}

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
        if( subtitleObj ) NPN_ReleaseObject(subtitleObj);
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
    "addEventListener",
    "removeEventListener",
};
COUNTNAMES(LibvlcRootNPObject,methodCount,methodNames);

enum LibvlcRootNPObjectMethodIds
{
    ID_root_versionInfo,
    ID_root_addeventlistener,
    ID_root_removeeventlistener,
};

RuntimeNPObject::InvokeResult LibvlcRootNPObject::invoke(int index,
                  const NPVariant *args, uint32_t argCount, NPVariant &result)
{
    /* is plugin still running */
    if( !isPluginRunning() )
        return INVOKERESULT_GENERIC_ERROR;

    switch( index )
    {
    case ID_root_versionInfo:
        if( 0 != argCount )
            return INVOKERESULT_NO_SUCH_METHOD;
        return invokeResultString(libvlc_get_version(),result);

    case ID_root_addeventlistener:
    case ID_root_removeeventlistener:
        if( (3 != argCount) ||
            !NPVARIANT_IS_STRING(args[0]) ||
            !NPVARIANT_IS_OBJECT(args[1]) ||
            !NPVARIANT_IS_BOOLEAN(args[2]) )
            break;

        if( !VlcPlugin::canUseEventListener() )
        {
            NPN_SetException(this, ERROR_API_VERSION);
            return INVOKERESULT_GENERIC_ERROR;
        }

        NPObject *listener = NPVARIANT_TO_OBJECT(args[1]);
        VlcPlugin* p_plugin = getPrivate<VlcPlugin>();

        bool b;
        if(ID_root_removeeventlistener!=index)
            b = p_plugin->events.insert(NPVARIANT_TO_STRING(args[0]),
                                     listener, NPVARIANT_TO_BOOLEAN(args[2]));
        else
            b = p_plugin->events.remove(NPVARIANT_TO_STRING(args[0]),
                                     listener, NPVARIANT_TO_BOOLEAN(args[2]));

        VOID_TO_NPVARIANT(result);

        return b ? INVOKERESULT_NO_ERROR : INVOKERESULT_GENERIC_ERROR;
    }
    return INVOKERESULT_NO_SUCH_METHOD;
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

        libvlc_media_player_t *p_md = p_plugin->getMD();
        if( !p_md )
            RETURN_ON_ERROR;

        switch( index )
        {
            case ID_audio_mute:
            {
                bool muted = libvlc_audio_get_mute(p_md);
                BOOLEAN_TO_NPVARIANT(muted, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_audio_volume:
            {
                int volume = libvlc_audio_get_volume(p_md);
                INT32_TO_NPVARIANT(volume, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_audio_track:
            {
                int track = libvlc_audio_get_track(p_md);
                INT32_TO_NPVARIANT(track, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_audio_count:
            {
                // get the number of audio track available
                int i_track = libvlc_audio_get_track_count(p_md);
                // return it
                INT32_TO_NPVARIANT(i_track, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_audio_channel:
            {
                int channel = libvlc_audio_get_channel(p_md);
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

        libvlc_media_player_t *p_md = p_plugin->getMD();
        if( !p_md )
            RETURN_ON_ERROR;

        switch( index )
        {
            case ID_audio_mute:
                if( NPVARIANT_IS_BOOLEAN(value) )
                {
                    libvlc_audio_set_mute(p_md,
                                          NPVARIANT_TO_BOOLEAN(value));
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_INVALID_VALUE;
            case ID_audio_volume:
                if( isNumberValue(value) )
                {
                    libvlc_audio_set_volume(p_md, numberValue(value));
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_INVALID_VALUE;
            case ID_audio_track:
                if( isNumberValue(value) )
                {
                    libvlc_audio_set_track(p_md, numberValue(value));
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_INVALID_VALUE;
            case ID_audio_channel:
                if( isNumberValue(value) )
                {
                    libvlc_audio_set_channel(p_md, numberValue(value));
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
        libvlc_media_player_t *p_md = p_plugin->getMD();
        if( !p_md )
            RETURN_ON_ERROR;

        switch( index )
        {
            case ID_audio_togglemute:
                if( argCount == 0 )
                {
                    libvlc_audio_toggle_mute(p_md);
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

                    /* get tracks description */
                    p_trackDesc = libvlc_audio_get_track_description(p_md);
                    if( !p_trackDesc )
                        return INVOKERESULT_GENERIC_ERROR;

                    /* get the number of track available */
                    i_limit = libvlc_audio_get_track_count(p_md);

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
        libvlc_media_player_t *p_md = p_plugin->getMD();
        if( !p_md )
        {
            if( index != ID_input_state )
                RETURN_ON_ERROR;
            else
            {
                /* for input state, return CLOSED rather than an exception */
                INT32_TO_NPVARIANT(0, result);
                return INVOKERESULT_NO_ERROR;
            }
        }

        switch( index )
        {
            case ID_input_length:
            {
                double val = (double)libvlc_media_player_get_length(p_md);
                DOUBLE_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_input_position:
            {
                double val = libvlc_media_player_get_position(p_md);
                DOUBLE_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_input_time:
            {
                double val = (double)libvlc_media_player_get_time(p_md);
                DOUBLE_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_input_state:
            {
                int val = libvlc_media_player_get_state(p_md);
                INT32_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_input_rate:
            {
                float val = libvlc_media_player_get_rate(p_md);
                DOUBLE_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_input_fps:
            {
                double val = libvlc_media_player_get_fps(p_md);
                DOUBLE_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_input_hasvout:
            {
                bool val = p_plugin->player_has_vout();
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
        libvlc_media_player_t *p_md = p_plugin->getMD();
        if( !p_md )
            RETURN_ON_ERROR;

        switch( index )
        {
            case ID_input_position:
            {
                if( ! NPVARIANT_IS_DOUBLE(value) )
                {
                    return INVOKERESULT_INVALID_VALUE;
                }

                float val = (float)NPVARIANT_TO_DOUBLE(value);
                libvlc_media_player_set_position(p_md, val);
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

                libvlc_media_player_set_time(p_md, val);
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

                libvlc_media_player_set_rate(p_md, val);
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

        switch( index )
        {
            case ID_playlistitems_count:
            {
                int val = p_plugin->playlist_count();
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

        switch( index )
        {
            case ID_playlistitems_clear:
                if( argCount == 0 )
                {
                    p_plugin->playlist_clear();
                    VOID_TO_NPVARIANT(result);
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            case ID_playlistitems_remove:
                if( (argCount == 1) && isNumberValue(args[0]) )
                {
                    if( !p_plugin->playlist_delete_item(numberValue(args[0])) )
                        return INVOKERESULT_GENERIC_ERROR;
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

        switch( index )
        {
            case ID_playlist_itemcount: /* deprecated */
            {
                int val = p_plugin->playlist_count();
                INT32_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_playlist_isplaying:
            {
                int val = p_plugin->playlist_isplaying();
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
                      i_options, const_cast<const char **>(ppsz_options));
                free(url);
                free(name);
                if( item == -1 )
                    RETURN_ON_ERROR;

                for( int i=0; i< i_options; ++i )
                {
                    free(ppsz_options[i]);
                }
                free(ppsz_options);

                INT32_TO_NPVARIANT(item, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_playlist_play:
                if( argCount == 0 )
                {
                    p_plugin->playlist_play();
                    VOID_TO_NPVARIANT(result);
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            case ID_playlist_playItem:
                if( (argCount == 1) && isNumberValue(args[0]) )
                {
                    p_plugin->playlist_play_item(numberValue(args[0]));
                    VOID_TO_NPVARIANT(result);
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            case ID_playlist_togglepause:
                if( argCount == 0 )
                {
                    p_plugin->playlist_pause();
                    VOID_TO_NPVARIANT(result);
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            case ID_playlist_stop:
                if( argCount == 0 )
                {
                    p_plugin->playlist_stop();
                    VOID_TO_NPVARIANT(result);
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            case ID_playlist_next:
                if( argCount == 0 )
                {
                    p_plugin->playlist_next();
                    VOID_TO_NPVARIANT(result);
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            case ID_playlist_prev:
                if( argCount == 0 )
                {
                    p_plugin->playlist_prev();
                    VOID_TO_NPVARIANT(result);
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            case ID_playlist_clear: /* deprecated */
                if( argCount == 0 )
                {
                    p_plugin->playlist_clear();
                    VOID_TO_NPVARIANT(result);
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            case ID_playlist_removeitem: /* deprecated */
                if( (argCount == 1) && isNumberValue(args[0]) )
                {
                    if( !p_plugin->playlist_delete_item(numberValue(args[0])) )
                        return INVOKERESULT_GENERIC_ERROR;
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
    if( nps.UTF8Length )
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

                char *end = val + nps.UTF8Length;
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
        libvlc_media_player_t *p_md = p_plugin->getMD();
        if( !p_md )
            RETURN_ON_ERROR;

        switch( index )
        {
            case ID_subtitle_track:
            {
                /* get the current subtitle ID */
                int i_spu = libvlc_video_get_spu(p_md);
                /* return it */
                INT32_TO_NPVARIANT(i_spu, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_subtitle_count:
            {
                /* get the number of subtitles available */
                int i_spu = libvlc_video_get_spu_count(p_md);
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
        libvlc_media_player_t *p_md = p_plugin->getMD();
        if( !p_md )
            RETURN_ON_ERROR;

        switch( index )
        {
            case ID_subtitle_track:
            {
                if( isNumberValue(value) )
                {
                    /* set the new subtitle track to show */
                    libvlc_video_set_spu(p_md, numberValue(value));

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
        libvlc_media_player_t *p_md = p_plugin->getMD();
        if( !p_md )
            RETURN_ON_ERROR;

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
                    p_spuDesc = libvlc_video_get_spu_description(p_md);
                    if( !p_spuDesc )
                        return INVOKERESULT_GENERIC_ERROR;

                    /* get the number of subtitle available */
                    i_limit = libvlc_video_get_spu_count(p_md);

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

LibvlcVideoNPObject::~LibvlcVideoNPObject()
{
    if( isValid() )
    {
        if( marqueeObj ) NPN_ReleaseObject(marqueeObj);
        if( logoObj    ) NPN_ReleaseObject(logoObj);
        if( deintObj   ) NPN_ReleaseObject(deintObj);
    }
}

const NPUTF8 * const LibvlcVideoNPObject::propertyNames[] =
{
    "fullscreen",
    "height",
    "width",
    "aspectRatio",
    "subtitle",
    "crop",
    "teletext",
    "marquee",
    "logo",
    "deinterlace",
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
    ID_video_marquee,
    ID_video_logo,
    ID_video_deinterlace,
};
COUNTNAMES(LibvlcVideoNPObject,propertyCount,propertyNames);

RuntimeNPObject::InvokeResult
LibvlcVideoNPObject::getProperty(int index, NPVariant &result)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        VlcPlugin* p_plugin = getPrivate<VlcPlugin>();
        libvlc_media_player_t *p_md = p_plugin->getMD();
        if( !p_md )
            RETURN_ON_ERROR;

        switch( index )
        {
            case ID_video_fullscreen:
            {
                int val = p_plugin->get_fullscreen();
                BOOLEAN_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_height:
            {
                int val = libvlc_video_get_height(p_md);
                INT32_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_width:
            {
                int val = libvlc_video_get_width(p_md);
                INT32_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_aspectratio:
            {
                NPUTF8 *psz_aspect = libvlc_video_get_aspect_ratio(p_md);
                if( !psz_aspect )
                    return INVOKERESULT_GENERIC_ERROR;

                STRINGZ_TO_NPVARIANT(psz_aspect, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_subtitle:
            {
                int i_spu = libvlc_video_get_spu(p_md);
                INT32_TO_NPVARIANT(i_spu, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_crop:
            {
                NPUTF8 *psz_geometry = libvlc_video_get_crop_geometry(p_md);
                if( !psz_geometry )
                    return INVOKERESULT_GENERIC_ERROR;

                STRINGZ_TO_NPVARIANT(psz_geometry, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_teletext:
            {
                int i_page = libvlc_video_get_teletext(p_md);
                if( i_page < 0 )
                    return INVOKERESULT_GENERIC_ERROR;
                INT32_TO_NPVARIANT(i_page, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_marquee:
            {
                InstantObj<LibvlcMarqueeNPObject>( marqueeObj );
                OBJECT_TO_NPVARIANT(NPN_RetainObject(marqueeObj), result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_logo:
            {
                InstantObj<LibvlcLogoNPObject>( logoObj );
                OBJECT_TO_NPVARIANT(NPN_RetainObject(logoObj), result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_deinterlace:
            {
                InstantObj<LibvlcDeinterlaceNPObject>( deintObj );
                OBJECT_TO_NPVARIANT(NPN_RetainObject(deintObj), result);
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
        libvlc_media_player_t *p_md = p_plugin->getMD();
        if( !p_md )
            RETURN_ON_ERROR;

        switch( index )
        {
            case ID_video_fullscreen:
            {
                if( ! NPVARIANT_IS_BOOLEAN(value) )
                {
                    return INVOKERESULT_INVALID_VALUE;
                }

                int val = NPVARIANT_TO_BOOLEAN(value);
                p_plugin->set_fullscreen(val);
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

                libvlc_video_set_aspect_ratio(p_md, psz_aspect);
                free(psz_aspect);

                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_subtitle:
            {
                if( isNumberValue(value) )
                {
                    libvlc_video_set_spu(p_md, numberValue(value));

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

                libvlc_video_set_crop_geometry(p_md, psz_geometry);
                free(psz_geometry);

                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_teletext:
            {
                if( isNumberValue(value) )
                {
                    libvlc_video_set_teletext(p_md, numberValue(value));
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
};
COUNTNAMES(LibvlcVideoNPObject,methodCount,methodNames);

enum LibvlcVideoNPObjectMethodIds
{
    ID_video_togglefullscreen,
    ID_video_toggleteletext,
};

RuntimeNPObject::InvokeResult
LibvlcVideoNPObject::invoke(int index, const NPVariant *args,
                            uint32_t argCount, NPVariant &result)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        VlcPlugin* p_plugin = getPrivate<VlcPlugin>();
        libvlc_media_player_t *p_md = p_plugin->getMD();
        if( !p_md )
            RETURN_ON_ERROR;

        switch( index )
        {
            case ID_video_togglefullscreen:
            {
                if( argCount == 0 )
                {
                    p_plugin->toggle_fullscreen();
                    VOID_TO_NPVARIANT(result);
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            }
            case ID_video_toggleteletext:
            {
                if( argCount == 0 )
                {
                    libvlc_toggle_teletext(p_md);
                    VOID_TO_NPVARIANT(result);
                    return INVOKERESULT_NO_ERROR;
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
** implementation of libvlc marquee object
*/

const NPUTF8 * const LibvlcMarqueeNPObject::propertyNames[] =
{
    "color",
    "opacity",
    "position",
    "refresh",
    "size",
    "text",
    "timeout",
    "x",
    "y",
};

enum LibvlcMarqueeNPObjectPropertyIds
{
    ID_marquee_color,
    ID_marquee_opacity,
    ID_marquee_position,
    ID_marquee_refresh,
    ID_marquee_size,
    ID_marquee_text,
    ID_marquee_timeout,
    ID_marquee_x,
    ID_marquee_y,
};

COUNTNAMES(LibvlcMarqueeNPObject,propertyCount,propertyNames);

static const unsigned char marquee_idx[] = {
    libvlc_marquee_Color,
    libvlc_marquee_Opacity,
    libvlc_marquee_Position,
    libvlc_marquee_Refresh,
    libvlc_marquee_Size,
    0,
    libvlc_marquee_Timeout,
    libvlc_marquee_X,
    libvlc_marquee_Y,
};

RuntimeNPObject::InvokeResult
LibvlcMarqueeNPObject::getProperty(int index, NPVariant &result)
{
    char *psz;

    if( !isPluginRunning() )
        return INVOKERESULT_GENERIC_ERROR;

    VlcPlugin* p_plugin = getPrivate<VlcPlugin>();
    libvlc_media_player_t *p_md = p_plugin->getMD();
    if( !p_md )
        RETURN_ON_ERROR;

    switch( index )
    {
    case ID_marquee_color:
    case ID_marquee_opacity:
    case ID_marquee_refresh:
    case ID_marquee_timeout:
    case ID_marquee_size:
    case ID_marquee_x:
    case ID_marquee_y:
        INT32_TO_NPVARIANT(
            libvlc_video_get_marquee_int(p_md, marquee_idx[index]),
            result );
        return INVOKERESULT_NO_ERROR;

    case ID_marquee_position:
        STRINGZ_TO_NPVARIANT( position_bynumber(
            libvlc_video_get_marquee_int(p_md, libvlc_marquee_Position) ),
            result );

        break;

    case ID_marquee_text:
        psz = libvlc_video_get_marquee_string(p_md, libvlc_marquee_Text);
        if( psz )
        {
            STRINGZ_TO_NPVARIANT(psz, result);
            return INVOKERESULT_NO_ERROR;
        }
        break;
    }
    return INVOKERESULT_GENERIC_ERROR;
}

RuntimeNPObject::InvokeResult
LibvlcMarqueeNPObject::setProperty(int index, const NPVariant &value)
{
    size_t i;

    if( !isPluginRunning() )
        return INVOKERESULT_GENERIC_ERROR;

    VlcPlugin* p_plugin = getPrivate<VlcPlugin>();
    libvlc_media_player_t *p_md = p_plugin->getMD();
    if( !p_md )
        RETURN_ON_ERROR;

    switch( index )
    {
    case ID_marquee_color:
    case ID_marquee_opacity:
    case ID_marquee_refresh:
    case ID_marquee_timeout:
    case ID_marquee_x:
    case ID_marquee_y:
        if( NPVARIANT_IS_INT32( value ) )
        {
            libvlc_video_set_marquee_int(p_md, marquee_idx[index],
                                         NPVARIANT_TO_INT32( value ));
            return INVOKERESULT_NO_ERROR;
        }
        break;

    case ID_marquee_position:
        if( !NPVARIANT_IS_STRING(value) ||
            !position_byname( NPVARIANT_TO_STRING(value).UTF8Characters, i ) )
            return INVOKERESULT_INVALID_VALUE;

        libvlc_video_set_marquee_int(p_md, libvlc_marquee_Position, i);
        return INVOKERESULT_NO_ERROR;

    case ID_marquee_text:
        if( NPVARIANT_IS_STRING( value ) )
        {
            char *psz_text = stringValue( NPVARIANT_TO_STRING( value ) );
            libvlc_video_set_marquee_string(p_md, libvlc_marquee_Text,
                                            psz_text);
            free(psz_text);
            return INVOKERESULT_NO_ERROR;
        }
        break;
    }
    return INVOKERESULT_NO_SUCH_METHOD;
}

const NPUTF8 * const LibvlcMarqueeNPObject::methodNames[] =
{
    "enable",
    "disable",
};
COUNTNAMES(LibvlcMarqueeNPObject,methodCount,methodNames);

enum LibvlcMarqueeNPObjectMethodIds
{
    ID_marquee_enable,
    ID_marquee_disable,
};

RuntimeNPObject::InvokeResult
LibvlcMarqueeNPObject::invoke(int index, const NPVariant *args,
                            uint32_t argCount, NPVariant &result)
{
    if( !isPluginRunning() )
        return INVOKERESULT_GENERIC_ERROR;

    VlcPlugin* p_plugin = getPrivate<VlcPlugin>();
    libvlc_media_player_t *p_md = p_plugin->getMD();
    if( !p_md )
        RETURN_ON_ERROR;

    switch( index )
    {
    case ID_marquee_enable:
    case ID_marquee_disable:
        libvlc_video_set_marquee_int(p_md, libvlc_marquee_Enable,
                                     index!=ID_marquee_disable);
        VOID_TO_NPVARIANT(result);
        return INVOKERESULT_NO_ERROR;
    }
    return INVOKERESULT_NO_SUCH_METHOD;
}

const NPUTF8 * const LibvlcLogoNPObject::propertyNames[] = {
    "delay",
    "repeat",
    "opacity",
    "position",
    "x",
    "y",
};
enum LibvlcLogoNPObjectPropertyIds {
    ID_logo_delay,
    ID_logo_repeat,
    ID_logo_opacity,
    ID_logo_position,
    ID_logo_x,
    ID_logo_y,
};
COUNTNAMES(LibvlcLogoNPObject,propertyCount,propertyNames);
static const unsigned char logo_idx[] = {
    libvlc_logo_delay,
    libvlc_logo_repeat,
    libvlc_logo_opacity,
    0,
    libvlc_logo_x,
    libvlc_logo_y,
};

RuntimeNPObject::InvokeResult
LibvlcLogoNPObject::getProperty(int index, NPVariant &result)
{
    if( !isPluginRunning() )
        return INVOKERESULT_GENERIC_ERROR;

    VlcPlugin* p_plugin = getPrivate<VlcPlugin>();
    libvlc_media_player_t *p_md = p_plugin->getMD();
    if( !p_md )
        RETURN_ON_ERROR;

    switch( index )
    {
    case ID_logo_delay:
    case ID_logo_repeat:
    case ID_logo_opacity:
    case ID_logo_x:
    case ID_logo_y:

        INT32_TO_NPVARIANT(
            libvlc_video_get_logo_int(p_md, logo_idx[index]), result);
        break;

    case ID_logo_position:
        STRINGZ_TO_NPVARIANT( position_bynumber(
            libvlc_video_get_logo_int(p_md, libvlc_logo_position) ),
            result );
        break;
    default:
        return INVOKERESULT_GENERIC_ERROR;
    }
    return INVOKERESULT_NO_ERROR;
}

RuntimeNPObject::InvokeResult
LibvlcLogoNPObject::setProperty(int index, const NPVariant &value)
{
    size_t i;

    if( !isPluginRunning() )
        return INVOKERESULT_GENERIC_ERROR;

    VlcPlugin* p_plugin = getPrivate<VlcPlugin>();
    libvlc_media_player_t *p_md = p_plugin->getMD();
    if( !p_md )
        RETURN_ON_ERROR;

    switch( index )
    {
    case ID_logo_delay:
    case ID_logo_repeat:
    case ID_logo_opacity:
    case ID_logo_x:
    case ID_logo_y:
        if( !NPVARIANT_IS_INT32(value) )
            return INVOKERESULT_INVALID_VALUE;

        libvlc_video_set_logo_int(p_md, logo_idx[index],
                                  NPVARIANT_TO_INT32( value ));
        break;

    case ID_logo_position:
        if( !NPVARIANT_IS_STRING(value) ||
            !position_byname( NPVARIANT_TO_STRING(value).UTF8Characters, i ) )
            return INVOKERESULT_INVALID_VALUE;

        libvlc_video_set_logo_int(p_md, libvlc_logo_position, i);
        break;
    default:
        return INVOKERESULT_GENERIC_ERROR;
    }
    return INVOKERESULT_NO_ERROR;
}


const NPUTF8 * const LibvlcLogoNPObject::methodNames[] = {
    "enable",
    "disable",
    "file",
};
enum LibvlcLogoNPObjectMethodIds {
    ID_logo_enable,
    ID_logo_disable,
    ID_logo_file,
};
COUNTNAMES(LibvlcLogoNPObject,methodCount,methodNames);

RuntimeNPObject::InvokeResult
LibvlcLogoNPObject::invoke(int index, const NPVariant *args,
                           uint32_t argCount, NPVariant &result)
{
    char *buf, *h;
    size_t i, len;

    if( !isPluginRunning() )
        return INVOKERESULT_GENERIC_ERROR;

    libvlc_media_player_t *p_md = getPrivate<VlcPlugin>()->getMD();
    if( !p_md )
        RETURN_ON_ERROR;

    switch( index )
    {
    case ID_logo_enable:
    case ID_logo_disable:
        if( argCount != 0 )
            return INVOKERESULT_GENERIC_ERROR;

        libvlc_video_set_logo_int(p_md, libvlc_logo_enable,
                                  index != ID_logo_disable);
        VOID_TO_NPVARIANT(result);
        break;

    case ID_logo_file:
        if( argCount == 0 )
            return INVOKERESULT_GENERIC_ERROR;

        for( len=0,i=0;i<argCount;++i )
        {
            if( !NPVARIANT_IS_STRING(args[i]) )
                return INVOKERESULT_INVALID_VALUE;
            len+=NPVARIANT_TO_STRING(args[i]).UTF8Length+1;
        }

        buf = (char *)malloc( len+1 );
        if( !buf )
            return INVOKERESULT_OUT_OF_MEMORY;

        for( h=buf,i=0;i<argCount;++i )
        {
            if(i) *h++=';';
            len=NPVARIANT_TO_STRING(args[i]).UTF8Length;
            memcpy(h,NPVARIANT_TO_STRING(args[i]).UTF8Characters,len);
            h+=len;
        }
        *h='\0';

        libvlc_video_set_logo_string(p_md, libvlc_logo_file, buf);
        free( buf );
        VOID_TO_NPVARIANT(result);
        break;
    default:
        return INVOKERESULT_NO_SUCH_METHOD;
    }
    return INVOKERESULT_NO_ERROR;
}


const NPUTF8 * const LibvlcDeinterlaceNPObject::propertyNames[] = {
};
enum LibvlcDeinterlaceNPObjectPropertyIds {
};
COUNTNAMES(LibvlcDeinterlaceNPObject,propertyCount,propertyNames);

RuntimeNPObject::InvokeResult
LibvlcDeinterlaceNPObject::getProperty(int index, NPVariant &result)
{
    return INVOKERESULT_GENERIC_ERROR;
}

RuntimeNPObject::InvokeResult
LibvlcDeinterlaceNPObject::setProperty(int index, const NPVariant &value)
{
    return INVOKERESULT_GENERIC_ERROR;
}


const NPUTF8 * const LibvlcDeinterlaceNPObject::methodNames[] = {
    "enable",
    "disable",
};
enum LibvlcDeinterlaceNPObjectMethodIds {
    ID_deint_enable,
    ID_deint_disable,
};
COUNTNAMES(LibvlcDeinterlaceNPObject,methodCount,methodNames);

RuntimeNPObject::InvokeResult
LibvlcDeinterlaceNPObject::invoke(int index, const NPVariant *args,
                           uint32_t argCount, NPVariant &result)
{
    char *psz;

    if( !isPluginRunning() )
        return INVOKERESULT_GENERIC_ERROR;

    libvlc_media_player_t *p_md = getPrivate<VlcPlugin>()->getMD();
    if( !p_md )
        RETURN_ON_ERROR;

    switch( index )
    {
    case ID_deint_disable:
        libvlc_video_set_deinterlace(p_md, NULL);
        break;

    case ID_deint_enable:
        if( argCount != 1 || !NPVARIANT_IS_STRING( args[0] ) )
            return INVOKERESULT_INVALID_VALUE;

        psz = stringValue( NPVARIANT_TO_STRING( args[0] ) );
        libvlc_video_set_deinterlace(p_md, psz);
        free(psz);
        break;

    default:
        return INVOKERESULT_NO_SUCH_METHOD;
    }
    return INVOKERESULT_NO_ERROR;
}

