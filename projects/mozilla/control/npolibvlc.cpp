/*****************************************************************************
 * npolibvlc.cpp: official Javascript APIs
 *****************************************************************************
 * Copyright (C) 2002-2006 the VideoLAN team
 *
 * Authors: Damien Fouilleul <Damien.Fouilleul@laposte.net>
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
** implementation of libvlc root object
*/

LibvlcRootNPObject::~LibvlcRootNPObject()
{
    /*
    ** when plugin is destroyed, firefox takes upon itself to destroy all 'live' script objects
    ** and ignores refcounting. Therefore we cannot safely assume  that refcounting will control
    ** lifespan of objects. Hence they are only lazily created on request, so that firefox can
    ** take ownership, and are not released when plugin is being destroyed.
    */
    if( isValid() )
    {
        if( audioObj    ) NPN_ReleaseObject(audioObj);
        if( inputObj    ) NPN_ReleaseObject(inputObj);
        if( logObj      ) NPN_ReleaseObject(logObj);
        if( playlistObj ) NPN_ReleaseObject(playlistObj);
        if( videoObj    ) NPN_ReleaseObject(videoObj);
    }
}

const NPUTF8 * const LibvlcRootNPObject::propertyNames[] = 
{
    "audio",
    "input",
    "log",
    "playlist",
    "video",
    "VersionInfo",
};

const int LibvlcRootNPObject::propertyCount = sizeof(LibvlcRootNPObject::propertyNames)/sizeof(NPUTF8 *);

enum LibvlcRootNPObjectPropertyIds
{
    ID_root_audio = 0,
    ID_root_input,
    ID_root_log,
    ID_root_playlist,
    ID_root_video,
    ID_root_VersionInfo,
};

RuntimeNPObject::InvokeResult LibvlcRootNPObject::getProperty(int index, NPVariant &result)
{
    /* is plugin still running */
    if( _instance->pdata )
    {
        switch( index )
        {
            case ID_root_audio:
                // create child object in lazyman fashion to avoid ownership problem with firefox
                if( ! audioObj )
                    audioObj = NPN_CreateObject(_instance, RuntimeNPClass<LibvlcAudioNPObject>::getClass());
                OBJECT_TO_NPVARIANT(NPN_RetainObject(audioObj), result);
                return INVOKERESULT_NO_ERROR;
            case ID_root_input:
                // create child object in lazyman fashion to avoid ownership problem with firefox
                if( ! inputObj )
                    inputObj = NPN_CreateObject(_instance, RuntimeNPClass<LibvlcInputNPObject>::getClass());
                OBJECT_TO_NPVARIANT(NPN_RetainObject(inputObj), result);
                return INVOKERESULT_NO_ERROR;
            case ID_root_log:
                // create child object in lazyman fashion to avoid ownership problem with firefox
                if( ! logObj )
                    logObj = NPN_CreateObject(_instance, RuntimeNPClass<LibvlcLogNPObject>::getClass());
                OBJECT_TO_NPVARIANT(NPN_RetainObject(logObj), result);
                return INVOKERESULT_NO_ERROR;
            case ID_root_playlist:
                // create child object in lazyman fashion to avoid ownership problem with firefox
                if( ! playlistObj )
                    playlistObj = NPN_CreateObject(_instance, RuntimeNPClass<LibvlcPlaylistNPObject>::getClass());
                OBJECT_TO_NPVARIANT(NPN_RetainObject(playlistObj), result);
                return INVOKERESULT_NO_ERROR;
            case ID_root_video:
                // create child object in lazyman fashion to avoid ownership problem with firefox
                if( ! videoObj )
                    videoObj = NPN_CreateObject(_instance,RuntimeNPClass<LibvlcVideoNPObject>::getClass());
                OBJECT_TO_NPVARIANT(NPN_RetainObject(videoObj), result);
                return INVOKERESULT_NO_ERROR;
            case ID_root_VersionInfo:
            {
                int len = strlen(libvlc_get_version());
                NPUTF8 *retval =(NPUTF8*)NPN_MemAlloc(len);
                if( retval )
                {
                    memcpy(retval, libvlc_get_version(), len);
                    STRINGN_TO_NPVARIANT(retval, len, result);
                }
                else
                {
                    NULL_TO_NPVARIANT(result);
                }
                return INVOKERESULT_NO_ERROR;
            }
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

const int LibvlcRootNPObject::methodCount = sizeof(LibvlcRootNPObject::methodNames)/sizeof(NPUTF8 *);

enum LibvlcRootNPObjectMethodIds
{
    ID_root_versionInfo,
};

RuntimeNPObject::InvokeResult LibvlcRootNPObject::invoke(int index, const NPVariant *args, uint32_t argCount, NPVariant &result)
{
    /* is plugin still running */
    if( _instance->pdata )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        switch( index )
        {
            case ID_root_versionInfo:
                if( argCount == 0 )
                {
                    int len = strlen(libvlc_get_version());
                    NPUTF8 *retval =(NPUTF8*)NPN_MemAlloc(len);
                    if( retval )
                    {
                        memcpy(retval, libvlc_get_version(), len);
                        STRINGN_TO_NPVARIANT(retval, len, result);
                    }
                    else
                    {
                        NULL_TO_NPVARIANT(result);
                    }
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
** implementation of libvlc audio object
*/

const NPUTF8 * const LibvlcAudioNPObject::propertyNames[] = 
{
    "mute",
    "volume",
    "track",
    "channel",
};

const int LibvlcAudioNPObject::propertyCount = sizeof(LibvlcAudioNPObject::propertyNames)/sizeof(NPUTF8 *);

enum LibvlcAudioNPObjectPropertyIds
{
    ID_audio_mute,
    ID_audio_volume,
    ID_audio_track,
    ID_audio_channel,
};

RuntimeNPObject::InvokeResult LibvlcAudioNPObject::getProperty(int index, NPVariant &result)
{
    /* is plugin still running */
    if( _instance->pdata )
    {
        VlcPlugin* p_plugin = reinterpret_cast<VlcPlugin*>(_instance->pdata);
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        switch( index )
        {
            case ID_audio_mute:
            {
                bool muted = libvlc_audio_get_mute(p_plugin->getVLC(), &ex);
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                BOOLEAN_TO_NPVARIANT(muted, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_audio_volume:
            {
                int volume = libvlc_audio_get_volume(p_plugin->getVLC(), &ex);
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                INT32_TO_NPVARIANT(volume, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_audio_track:
            {
                libvlc_media_player_t *p_md = libvlc_playlist_get_media_player(p_plugin->getVLC(), &ex);
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                int track = libvlc_audio_get_track(p_md, &ex);
                libvlc_media_player_release(p_md);
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                INT32_TO_NPVARIANT(track, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_audio_channel:
            {
                int channel = libvlc_audio_get_channel(p_plugin->getVLC(), &ex);
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                INT32_TO_NPVARIANT(channel, result);
                return INVOKERESULT_NO_ERROR;
            }
            default:
                ;
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

RuntimeNPObject::InvokeResult LibvlcAudioNPObject::setProperty(int index, const NPVariant &value)
{
    /* is plugin still running */
    if( _instance->pdata )
    {
        VlcPlugin* p_plugin = reinterpret_cast<VlcPlugin*>(_instance->pdata);
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        switch( index )
        {
            case ID_audio_mute:
                if( NPVARIANT_IS_BOOLEAN(value) )
                {
                    libvlc_audio_set_mute(p_plugin->getVLC(),
                                          NPVARIANT_TO_BOOLEAN(value), &ex);
                    if( libvlc_exception_raised(&ex) )
                    {
                        NPN_SetException(this, libvlc_exception_get_message(&ex));
                        libvlc_exception_clear(&ex);
                        return INVOKERESULT_GENERIC_ERROR;
                    }
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_INVALID_VALUE;
            case ID_audio_volume:
                if( isNumberValue(value) )
                {
                    libvlc_audio_set_volume(p_plugin->getVLC(),
                                            numberValue(value), &ex);
                    if( libvlc_exception_raised(&ex) )
                    {
                        NPN_SetException(this, libvlc_exception_get_message(&ex));
                        libvlc_exception_clear(&ex);
                        return INVOKERESULT_GENERIC_ERROR;
                    }
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_INVALID_VALUE;
            case ID_audio_track:
                if( isNumberValue(value) )
                {
                    libvlc_media_player_t *p_md = libvlc_playlist_get_media_player(p_plugin->getVLC(), &ex);
                    if( libvlc_exception_raised(&ex) )
                    {
                        NPN_SetException(this, libvlc_exception_get_message(&ex));
                        libvlc_exception_clear(&ex);
                        return INVOKERESULT_GENERIC_ERROR;
                    }
                    libvlc_audio_set_track(p_md,
                                           numberValue(value), &ex);
                    libvlc_media_player_release(p_md);
                    if( libvlc_exception_raised(&ex) )
                    {
                        NPN_SetException(this, libvlc_exception_get_message(&ex));
                        libvlc_exception_clear(&ex);
                        return INVOKERESULT_GENERIC_ERROR;
                    }
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_INVALID_VALUE;
            case ID_audio_channel:
                if( isNumberValue(value) )
                {
                    libvlc_audio_set_channel(p_plugin->getVLC(),
                                             numberValue(value), &ex);
                    if( libvlc_exception_raised(&ex) )
                    {
                        NPN_SetException(this, libvlc_exception_get_message(&ex));
                        libvlc_exception_clear(&ex);
                        return INVOKERESULT_GENERIC_ERROR;
                    }
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
};

const int LibvlcAudioNPObject::methodCount = sizeof(LibvlcAudioNPObject::methodNames)/sizeof(NPUTF8 *);

enum LibvlcAudioNPObjectMethodIds
{
    ID_audio_togglemute,
};

RuntimeNPObject::InvokeResult LibvlcAudioNPObject::invoke(int index, const NPVariant *args, uint32_t argCount, NPVariant &result)
{
    /* is plugin still running */
    if( _instance->pdata )
    {
        VlcPlugin* p_plugin = reinterpret_cast<VlcPlugin*>(_instance->pdata);
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        switch( index )
        {
            case ID_audio_togglemute:
                if( argCount == 0 )
                {
                    libvlc_audio_toggle_mute(p_plugin->getVLC(), &ex);
                    if( libvlc_exception_raised(&ex) )
                    {
                        NPN_SetException(this, libvlc_exception_get_message(&ex));
                        libvlc_exception_clear(&ex);
                        return INVOKERESULT_GENERIC_ERROR;
                    }
                    else
                    {
                        VOID_TO_NPVARIANT(result);
                        return INVOKERESULT_NO_ERROR;
                    }
                }
                return INVOKERESULT_NO_SUCH_METHOD;
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

const int LibvlcInputNPObject::propertyCount = sizeof(LibvlcInputNPObject::propertyNames)/sizeof(NPUTF8 *);

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

RuntimeNPObject::InvokeResult LibvlcInputNPObject::getProperty(int index, NPVariant &result)
{
    /* is plugin still running */
    if( _instance->pdata )
    {
        VlcPlugin* p_plugin = reinterpret_cast<VlcPlugin*>(_instance->pdata);
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_media_player_t *p_md = libvlc_playlist_get_media_player(p_plugin->getVLC(), &ex);
        if( libvlc_exception_raised(&ex) )
        {
            if( index != ID_input_state )
            {
                NPN_SetException(this, libvlc_exception_get_message(&ex));
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
                libvlc_media_player_release(p_md);
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                DOUBLE_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_input_position:
            {
                double val = libvlc_media_player_get_position(p_md, &ex);
                libvlc_media_player_release(p_md);
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                DOUBLE_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_input_time:
            {
                double val = (double)libvlc_media_player_get_time(p_md, &ex);
                libvlc_media_player_release(p_md);
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                DOUBLE_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_input_state:
            {
                int val = libvlc_media_player_get_state(p_md, &ex);
                libvlc_media_player_release(p_md);
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                INT32_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_input_rate:
            {
                float val = libvlc_media_player_get_rate(p_md, &ex);
                libvlc_media_player_release(p_md);
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                DOUBLE_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_input_fps:
            {
                double val = libvlc_media_player_get_fps(p_md, &ex);
                libvlc_media_player_release(p_md);
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                DOUBLE_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_input_hasvout:
            {
                bool val = libvlc_media_player_has_vout(p_md, &ex);
                libvlc_media_player_release(p_md);
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                BOOLEAN_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            default:
                ;
        }
        libvlc_media_player_release(p_md);
    }
    return INVOKERESULT_GENERIC_ERROR;
}

RuntimeNPObject::InvokeResult LibvlcInputNPObject::setProperty(int index, const NPVariant &value)
{
    /* is plugin still running */
    if( _instance->pdata )
    {
        VlcPlugin* p_plugin = reinterpret_cast<VlcPlugin*>(_instance->pdata);
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_media_player_t *p_md = libvlc_playlist_get_media_player(p_plugin->getVLC(), &ex);
        if( libvlc_exception_raised(&ex) )
        {
            NPN_SetException(this, libvlc_exception_get_message(&ex));
            libvlc_exception_clear(&ex);
            return INVOKERESULT_GENERIC_ERROR;
        }

        switch( index )
        {
            case ID_input_position:
            {
                if( ! NPVARIANT_IS_DOUBLE(value) )
                {
                    libvlc_media_player_release(p_md);
                    return INVOKERESULT_INVALID_VALUE;
                }

                float val = (float)NPVARIANT_TO_DOUBLE(value);
                libvlc_media_player_set_position(p_md, val, &ex);
                libvlc_media_player_release(p_md);
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
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
                    libvlc_media_player_release(p_md);
                    return INVOKERESULT_INVALID_VALUE;
                }

                libvlc_media_player_set_time(p_md, val, &ex);
                libvlc_media_player_release(p_md);
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
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
                    libvlc_media_player_release(p_md);
                    return INVOKERESULT_INVALID_VALUE;
                }

                libvlc_media_player_set_rate(p_md, val, &ex);
                libvlc_media_player_release(p_md);
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                return INVOKERESULT_NO_ERROR;
            }
            default:
                ;
        }
        libvlc_media_player_release(p_md);
    }
    return INVOKERESULT_GENERIC_ERROR;
}

const NPUTF8 * const LibvlcInputNPObject::methodNames[] =
{
    /* no methods */
};

const int LibvlcInputNPObject::methodCount = sizeof(LibvlcInputNPObject::methodNames)/sizeof(NPUTF8 *);

/*
** implementation of libvlc message object
*/

const NPUTF8 * const LibvlcMessageNPObject::propertyNames[] = 
{
    "severity",
    "type",
    "name",
    "header",
    "message",
};

const int LibvlcMessageNPObject::propertyCount = sizeof(LibvlcMessageNPObject::propertyNames)/sizeof(NPUTF8 *);

enum LibvlcMessageNPObjectPropertyIds
{
    ID_message_severity,
    ID_message_type,
    ID_message_name,
    ID_message_header,
    ID_message_message,
};

RuntimeNPObject::InvokeResult LibvlcMessageNPObject::getProperty(int index, NPVariant &result)
{
    /* is plugin still running */
    if( _instance->pdata )
    {
        switch( index )
        {
            case ID_message_severity:
            {
                INT32_TO_NPVARIANT(_msg.i_severity, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_message_type:
            {
                if( _msg.psz_type )
                {
                    int len = strlen(_msg.psz_type);
                    NPUTF8* retval = (NPUTF8*)NPN_MemAlloc(len);
                    if( retval )
                    {
                        memcpy(retval, _msg.psz_type, len);
                        STRINGN_TO_NPVARIANT(retval, len, result);
                    }
                }
                else
                {
                    NULL_TO_NPVARIANT(result);
                }
                return INVOKERESULT_NO_ERROR;
            }
            case ID_message_name:
            {
                if( _msg.psz_name )
                {
                    int len = strlen(_msg.psz_name);
                    NPUTF8* retval = (NPUTF8*)NPN_MemAlloc(len);
                    if( retval )
                    {
                        memcpy(retval, _msg.psz_name, len);
                        STRINGN_TO_NPVARIANT(retval, len, result);
                    }
                }
                else
                {
                    NULL_TO_NPVARIANT(result);
                }
                return INVOKERESULT_NO_ERROR;
            }
            case ID_message_header:
            {
                if( _msg.psz_header )
                {
                    int len = strlen(_msg.psz_header);
                    NPUTF8* retval = (NPUTF8*)NPN_MemAlloc(len);
                    if( retval )
                    {
                        memcpy(retval, _msg.psz_header, len);
                        STRINGN_TO_NPVARIANT(retval, len, result);
                    }
                }
                else
                {
                    NULL_TO_NPVARIANT(result);
                }
                return INVOKERESULT_NO_ERROR;
            }
            case ID_message_message:
            {
                if( _msg.psz_message )
                {
                    int len = strlen(_msg.psz_message);
                    NPUTF8* retval = (NPUTF8*)NPN_MemAlloc(len);
                    if( retval )
                    {
                        memcpy(retval, _msg.psz_message, len);
                        STRINGN_TO_NPVARIANT(retval, len, result);
                    }
                }
                else
                {
                    NULL_TO_NPVARIANT(result);
                }
                return INVOKERESULT_NO_ERROR;
            }
            default:
                ;
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

const NPUTF8 * const LibvlcMessageNPObject::methodNames[] =
{
    /* no methods */
};

const int LibvlcMessageNPObject::methodCount = sizeof(LibvlcMessageNPObject::methodNames)/sizeof(NPUTF8 *);

/*
** implementation of libvlc message iterator object
*/

LibvlcMessageIteratorNPObject::LibvlcMessageIteratorNPObject(NPP instance, const NPClass *aClass) :
    RuntimeNPObject(instance, aClass),
    _p_iter(NULL)
{
    /* is plugin still running */
    if( instance->pdata )
    {
        VlcPlugin* p_plugin = reinterpret_cast<VlcPlugin*>(instance->pdata);
        libvlc_log_t *p_log = p_plugin->getLog();
        if( p_log )
        {
            _p_iter = libvlc_log_get_iterator(p_log, NULL);
        }
    }
};

LibvlcMessageIteratorNPObject::~LibvlcMessageIteratorNPObject()
{
    if( _p_iter )
        libvlc_log_iterator_free(_p_iter, NULL);
}

const NPUTF8 * const LibvlcMessageIteratorNPObject::propertyNames[] = 
{
    "hasNext",
};

const int LibvlcMessageIteratorNPObject::propertyCount = sizeof(LibvlcMessageIteratorNPObject::propertyNames)/sizeof(NPUTF8 *);

enum LibvlcMessageIteratorNPObjectPropertyIds
{
    ID_messageiterator_hasNext,
};

RuntimeNPObject::InvokeResult LibvlcMessageIteratorNPObject::getProperty(int index, NPVariant &result)
{
    /* is plugin still running */
    if( _instance->pdata )
    {
        VlcPlugin* p_plugin = reinterpret_cast<VlcPlugin*>(_instance->pdata);
        switch( index )
        {
            case ID_messageiterator_hasNext:
            {
                if( _p_iter && p_plugin->getLog() )
                {
                    libvlc_exception_t ex;
                    libvlc_exception_init(&ex);

                    BOOLEAN_TO_NPVARIANT(libvlc_log_iterator_has_next(_p_iter, &ex), result);
                    if( libvlc_exception_raised(&ex) )
                    {
                        NPN_SetException(this, libvlc_exception_get_message(&ex));
                        libvlc_exception_clear(&ex);
                        return INVOKERESULT_GENERIC_ERROR;
                    }
                }
                else
                {
                    BOOLEAN_TO_NPVARIANT(0, result);
                }
                return INVOKERESULT_NO_ERROR;
            }
            default:
                ;
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

const NPUTF8 * const LibvlcMessageIteratorNPObject::methodNames[] =
{
    "next",
};

const int LibvlcMessageIteratorNPObject::methodCount = sizeof(LibvlcMessageIteratorNPObject::methodNames)/sizeof(NPUTF8 *);

enum LibvlcMessageIteratorNPObjectMethodIds
{
    ID_messageiterator_next,
};

RuntimeNPObject::InvokeResult LibvlcMessageIteratorNPObject::invoke(int index, const NPVariant *args, uint32_t argCount, NPVariant &result)
{
    /* is plugin still running */
    if( _instance->pdata )
    {
        VlcPlugin* p_plugin = reinterpret_cast<VlcPlugin*>(_instance->pdata);
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        switch( index )
        {
            case ID_messageiterator_next:
                if( argCount == 0 )
                {
                    if( _p_iter && p_plugin->getLog() )
                    {
                        struct libvlc_log_message_t buffer;

                        buffer.sizeof_msg = sizeof(buffer);

                        libvlc_log_iterator_next(_p_iter, &buffer, &ex);
                        if( libvlc_exception_raised(&ex) )
                        {
                            NPN_SetException(this, libvlc_exception_get_message(&ex));
                            libvlc_exception_clear(&ex);
                            return INVOKERESULT_GENERIC_ERROR;
                        }
                        else
                        {
                            LibvlcMessageNPObject* message =
                                static_cast<LibvlcMessageNPObject*>(NPN_CreateObject(_instance, RuntimeNPClass<LibvlcMessageNPObject>::getClass()));
                            if( message )
                            {
                                message->setMessage(buffer);
                                OBJECT_TO_NPVARIANT(message, result);
                                return INVOKERESULT_NO_ERROR;
                            }
                            return INVOKERESULT_OUT_OF_MEMORY;
                        }
                    }
                    return INVOKERESULT_GENERIC_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            default:
                ;
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}
 
/*
** implementation of libvlc message object
*/

const NPUTF8 * const LibvlcMessagesNPObject::propertyNames[] = 
{
    "count",
};

const int LibvlcMessagesNPObject::propertyCount = sizeof(LibvlcMessagesNPObject::propertyNames)/sizeof(NPUTF8 *);

enum LibvlcMessagesNPObjectPropertyIds
{
    ID_messages_count,
};

RuntimeNPObject::InvokeResult LibvlcMessagesNPObject::getProperty(int index, NPVariant &result)
{
    /* is plugin still running */
    if( _instance->pdata )
    {
        VlcPlugin* p_plugin = reinterpret_cast<VlcPlugin*>(_instance->pdata);
        switch( index )
        {
            case ID_messages_count:
            {
                libvlc_log_t *p_log = p_plugin->getLog();
                if( p_log )
                {
                    libvlc_exception_t ex;
                    libvlc_exception_init(&ex);

                    INT32_TO_NPVARIANT(libvlc_log_count(p_log, &ex), result);
                    if( libvlc_exception_raised(&ex) )
                    {
                        NPN_SetException(this, libvlc_exception_get_message(&ex));
                        libvlc_exception_clear(&ex);
                        return INVOKERESULT_GENERIC_ERROR;
                    }
                }
                else
                {
                    INT32_TO_NPVARIANT(0, result);
                }
                return INVOKERESULT_NO_ERROR;
            }
            default:
                ;
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

const NPUTF8 * const LibvlcMessagesNPObject::methodNames[] =
{
    "clear",
    "iterator",
};

const int LibvlcMessagesNPObject::methodCount = sizeof(LibvlcMessagesNPObject::methodNames)/sizeof(NPUTF8 *);

enum LibvlcMessagesNPObjectMethodIds
{
    ID_messages_clear,
    ID_messages_iterator,
};

RuntimeNPObject::InvokeResult LibvlcMessagesNPObject::invoke(int index, const NPVariant *args, uint32_t argCount, NPVariant &result)
{
    /* is plugin still running */
    if( _instance->pdata )
    {
        VlcPlugin* p_plugin = reinterpret_cast<VlcPlugin*>(_instance->pdata);
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        switch( index )
        {
            case ID_messages_clear:
                if( argCount == 0 )
                {
                    libvlc_log_t *p_log = p_plugin->getLog();
                    if( p_log )
                    {
                        libvlc_log_clear(p_log, &ex);
                        if( libvlc_exception_raised(&ex) )
                        {
                            NPN_SetException(this, libvlc_exception_get_message(&ex));
                            libvlc_exception_clear(&ex);
                            return INVOKERESULT_GENERIC_ERROR;
                        }
                    }
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;

            case ID_messages_iterator:
                if( argCount == 0 )
                {
                    LibvlcMessageIteratorNPObject* iter =
                        static_cast<LibvlcMessageIteratorNPObject*>(NPN_CreateObject(_instance, RuntimeNPClass<LibvlcMessageIteratorNPObject>::getClass()));
                    if( iter )
                    {
                        OBJECT_TO_NPVARIANT(iter, result);
                        return INVOKERESULT_NO_ERROR;
                    }
                    return INVOKERESULT_OUT_OF_MEMORY;
                }
                return INVOKERESULT_NO_SUCH_METHOD;

            default:
                ;
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

 
/*
** implementation of libvlc message object
*/


LibvlcLogNPObject::~LibvlcLogNPObject()
{
    if( isValid() )
    {
        if( messagesObj ) NPN_ReleaseObject(messagesObj);
    }
};

const NPUTF8 * const LibvlcLogNPObject::propertyNames[] = 
{
    "messages",
    "verbosity",
};

const int LibvlcLogNPObject::propertyCount = sizeof(LibvlcLogNPObject::propertyNames)/sizeof(NPUTF8 *);

enum LibvlcLogNPObjectPropertyIds
{
    ID_log_messages,
    ID_log_verbosity,
};

RuntimeNPObject::InvokeResult LibvlcLogNPObject::getProperty(int index, NPVariant &result)
{
    /* is plugin still running */
    if( _instance->pdata )
    {
        VlcPlugin* p_plugin = reinterpret_cast<VlcPlugin*>(_instance->pdata);
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        switch( index )
        {
            case ID_log_messages:
            {
                // create child object in lazyman fashion to avoid ownership problem with firefox
                if( ! messagesObj )
                    messagesObj = NPN_CreateObject(_instance, RuntimeNPClass<LibvlcMessagesNPObject>::getClass());
                OBJECT_TO_NPVARIANT(NPN_RetainObject(messagesObj), result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_log_verbosity:
            {
                if( p_plugin->getLog() )
                {
                    INT32_TO_NPVARIANT(libvlc_get_log_verbosity(p_plugin->getVLC(),
                                                                    &ex), result);
                    if( libvlc_exception_raised(&ex) )
                    {
                        NPN_SetException(this, libvlc_exception_get_message(&ex));
                        libvlc_exception_clear(&ex);
                        return INVOKERESULT_GENERIC_ERROR;
                    }
                }
                else
                {
                    /* log is not enabled, return -1 */
                    DOUBLE_TO_NPVARIANT(-1.0, result);
                }
                return INVOKERESULT_NO_ERROR;
            }
            default:
                ;
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

RuntimeNPObject::InvokeResult LibvlcLogNPObject::setProperty(int index, const NPVariant &value)
{
    /* is plugin still running */
    if( _instance->pdata )
    {
        VlcPlugin* p_plugin = reinterpret_cast<VlcPlugin*>(_instance->pdata);
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        switch( index )
        {
            case ID_log_verbosity:
                if( isNumberValue(value) )
                {
                    libvlc_instance_t* p_libvlc = p_plugin->getVLC();
                    libvlc_log_t *p_log = p_plugin->getLog();
                    int verbosity = numberValue(value);
                    if( verbosity >= 0 )
                    {
                        if( ! p_log )
                        {
                            p_log = libvlc_log_open(p_libvlc, &ex);
                            if( libvlc_exception_raised(&ex) )
                            {
                                NPN_SetException(this, libvlc_exception_get_message(&ex));
                                libvlc_exception_clear(&ex);
                                return INVOKERESULT_GENERIC_ERROR;
                            }
                            p_plugin->setLog(p_log);
                        }
                        libvlc_set_log_verbosity(p_libvlc, (unsigned)verbosity, &ex);
                        if( libvlc_exception_raised(&ex) )
                        {
                            NPN_SetException(this, libvlc_exception_get_message(&ex));
                            libvlc_exception_clear(&ex);
                            return INVOKERESULT_GENERIC_ERROR;
                        }
                    }
                    else if( p_log )
                    {
                        /* close log  when verbosity is set to -1 */
                        p_plugin->setLog(NULL);
                        libvlc_log_close(p_log, &ex);
                        if( libvlc_exception_raised(&ex) )
                        {
                            NPN_SetException(this, libvlc_exception_get_message(&ex));
                            libvlc_exception_clear(&ex);
                            return INVOKERESULT_GENERIC_ERROR;
                        }
                    }
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_INVALID_VALUE;
            default:
                ;
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

const NPUTF8 * const LibvlcLogNPObject::methodNames[] =
{
    /* no methods */
};

const int LibvlcLogNPObject::methodCount = sizeof(LibvlcLogNPObject::methodNames)/sizeof(NPUTF8 *);

/*
** implementation of libvlc playlist items object
*/

const NPUTF8 * const LibvlcPlaylistItemsNPObject::propertyNames[] = 
{
    "count",
};

const int LibvlcPlaylistItemsNPObject::propertyCount = sizeof(LibvlcPlaylistItemsNPObject::propertyNames)/sizeof(NPUTF8 *);

enum LibvlcPlaylistItemsNPObjectPropertyIds
{
    ID_playlistitems_count,
};

RuntimeNPObject::InvokeResult LibvlcPlaylistItemsNPObject::getProperty(int index, NPVariant &result)
{
    /* is plugin still running */
    if( _instance->pdata )
    {
        VlcPlugin* p_plugin = reinterpret_cast<VlcPlugin*>(_instance->pdata);
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        switch( index )
        {
            case ID_playlistitems_count:
            {
                libvlc_playlist_lock(p_plugin->getVLC());
                int val = libvlc_playlist_items_count(p_plugin->getVLC(), &ex);
                libvlc_playlist_unlock(p_plugin->getVLC());
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
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

const int LibvlcPlaylistItemsNPObject::methodCount = sizeof(LibvlcPlaylistItemsNPObject::methodNames)/sizeof(NPUTF8 *);

enum LibvlcPlaylistItemsNPObjectMethodIds
{
    ID_playlistitems_clear,
    ID_playlistitems_remove,
};

RuntimeNPObject::InvokeResult LibvlcPlaylistItemsNPObject::invoke(int index, const NPVariant *args, uint32_t argCount, NPVariant &result)
{
    /* is plugin still running */
    if( _instance->pdata )
    {
        VlcPlugin* p_plugin = reinterpret_cast<VlcPlugin*>(_instance->pdata);
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        switch( index )
        {
            case ID_playlistitems_clear:
                if( argCount == 0 )
                {
                    libvlc_playlist_clear(p_plugin->getVLC(), &ex);
                    if( libvlc_exception_raised(&ex) )
                    {
                        NPN_SetException(this, libvlc_exception_get_message(&ex));
                        libvlc_exception_clear(&ex);
                        return INVOKERESULT_GENERIC_ERROR;
                    }
                    else
                    {
                        VOID_TO_NPVARIANT(result);
                        return INVOKERESULT_NO_ERROR;
                    }
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            case ID_playlistitems_remove:
                if( (argCount == 1) && isNumberValue(args[0]) )
                {
                    libvlc_playlist_delete_item(p_plugin->getVLC(), numberValue(args[0]), &ex);
                    if( libvlc_exception_raised(&ex) )
                    {
                        NPN_SetException(this, libvlc_exception_get_message(&ex));
                        libvlc_exception_clear(&ex);
                        return INVOKERESULT_GENERIC_ERROR;
                    }
                    else
                    {
                        VOID_TO_NPVARIANT(result);
                        return INVOKERESULT_NO_ERROR;
                    }
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
    if( isValid() )
    {
        if( playlistItemsObj ) NPN_ReleaseObject(playlistItemsObj);
    }
};

const NPUTF8 * const LibvlcPlaylistNPObject::propertyNames[] = 
{
    "itemCount", /* deprecated */
    "isPlaying",
    "items",
};

const int LibvlcPlaylistNPObject::propertyCount = sizeof(LibvlcPlaylistNPObject::propertyNames)/sizeof(NPUTF8 *);

enum LibvlcPlaylistNPObjectPropertyIds
{
    ID_playlist_itemcount,
    ID_playlist_isplaying,
    ID_playlist_items,
};

RuntimeNPObject::InvokeResult LibvlcPlaylistNPObject::getProperty(int index, NPVariant &result)
{
    /* is plugin still running */
    if( _instance->pdata )
    {
        VlcPlugin* p_plugin = reinterpret_cast<VlcPlugin*>(_instance->pdata);
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        switch( index )
        {
            case ID_playlist_itemcount: /* deprecated */
            {
                libvlc_playlist_lock(p_plugin->getVLC());
                int val = libvlc_playlist_items_count(p_plugin->getVLC(), &ex);
                libvlc_playlist_unlock(p_plugin->getVLC());
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                INT32_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_playlist_isplaying:
            {
                int val = libvlc_playlist_isplaying(p_plugin->getVLC(), &ex);
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                BOOLEAN_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_playlist_items:
            {
                // create child object in lazyman fashion to avoid ownership problem with firefox
                if( ! playlistItemsObj )
                    playlistItemsObj = NPN_CreateObject(_instance, RuntimeNPClass<LibvlcPlaylistItemsNPObject>::getClass());
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

const int LibvlcPlaylistNPObject::methodCount = sizeof(LibvlcPlaylistNPObject::methodNames)/sizeof(NPUTF8 *);

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

RuntimeNPObject::InvokeResult LibvlcPlaylistNPObject::invoke(int index, const NPVariant *args, uint32_t argCount, NPVariant &result)
{
    /* is plugin still running */
    if( _instance->pdata )
    {
        VlcPlugin* p_plugin = reinterpret_cast<VlcPlugin*>(_instance->pdata);
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        switch( index )
        {
            case ID_playlist_add:
            {
                if( (argCount < 1) || (argCount > 3) )
                    return INVOKERESULT_NO_SUCH_METHOD;

                char *url = NULL;

                // grab URL
                if( NPVARIANT_IS_STRING(args[0]) )
                {
                    char *s = stringValue(NPVARIANT_TO_STRING(args[0]));
                    if( s )
                    {
                        url = p_plugin->getAbsoluteURL(s);
                        if( url )
			    free(s);
                        else
                            // problem with combining url, use argument
                            url = s;
                    }
                    else
                        return INVOKERESULT_OUT_OF_MEMORY;
                }
                else
                    return INVOKERESULT_NO_SUCH_METHOD;

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
                        parseOptions(NPVARIANT_TO_STRING(args[2]), &i_options, &ppsz_options);

                    }
                    else if( NPVARIANT_IS_OBJECT(args[2]) )
                    {
                        parseOptions(NPVARIANT_TO_OBJECT(args[2]), &i_options, &ppsz_options);
                    }
                    else
                    {
		        free(url);
                        free(name);
                        return INVOKERESULT_INVALID_VALUE;
                    }
                }

                int item = libvlc_playlist_add_extended_untrusted(p_plugin->getVLC(),
                                                                  url,
                                                                  name,
                                                                  i_options,
                                                                  const_cast<const char **>(ppsz_options),
                                                                  &ex);
                free(url);
                free(name);
                for( int i=0; i< i_options; ++i )
                {
		    free(ppsz_options[i]);
                }
                free(ppsz_options);

                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                else
                {
                    INT32_TO_NPVARIANT(item, result);
                    return INVOKERESULT_NO_ERROR;
                }
            }
            case ID_playlist_play:
                if( argCount == 0 )
                {
                    libvlc_playlist_play(p_plugin->getVLC(), -1, 0, NULL, &ex);
                    if( libvlc_exception_raised(&ex) )
                    {
                        NPN_SetException(this, libvlc_exception_get_message(&ex));
                        libvlc_exception_clear(&ex);
                        return INVOKERESULT_GENERIC_ERROR;
                    }
                    else
                    {
                        VOID_TO_NPVARIANT(result);
                        return INVOKERESULT_NO_ERROR;
                    }
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            case ID_playlist_playItem:
                if( (argCount == 1) && isNumberValue(args[0]) )
                {
                    libvlc_playlist_play(p_plugin->getVLC(), numberValue(args[0]), 0, NULL, &ex);
                    if( libvlc_exception_raised(&ex) )
                    {
                        NPN_SetException(this, libvlc_exception_get_message(&ex));
                        libvlc_exception_clear(&ex);
                        return INVOKERESULT_GENERIC_ERROR;
                    }
                    else
                    {
                        VOID_TO_NPVARIANT(result);
                        return INVOKERESULT_NO_ERROR;
                    }
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            case ID_playlist_togglepause:
                if( argCount == 0 )
                {
                    libvlc_playlist_pause(p_plugin->getVLC(), &ex);
                    if( libvlc_exception_raised(&ex) )
                    {
                        NPN_SetException(this, libvlc_exception_get_message(&ex));
                        libvlc_exception_clear(&ex);
                        return INVOKERESULT_GENERIC_ERROR;
                    }
                    else
                    {
                        VOID_TO_NPVARIANT(result);
                        return INVOKERESULT_NO_ERROR;
                    }
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            case ID_playlist_stop:
                if( argCount == 0 )
                {
                    libvlc_playlist_stop(p_plugin->getVLC(), &ex);
                    if( libvlc_exception_raised(&ex) )
                    {
                        NPN_SetException(this, libvlc_exception_get_message(&ex));
                        libvlc_exception_clear(&ex);
                        return INVOKERESULT_GENERIC_ERROR;
                    }
                    else
                    {
                        VOID_TO_NPVARIANT(result);
                        return INVOKERESULT_NO_ERROR;
                    }
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            case ID_playlist_next:
                if( argCount == 0 )
                {
                    libvlc_playlist_next(p_plugin->getVLC(), &ex);
                    if( libvlc_exception_raised(&ex) )
                    {
                        NPN_SetException(this, libvlc_exception_get_message(&ex));
                        libvlc_exception_clear(&ex);
                        return INVOKERESULT_GENERIC_ERROR;
                    }
                    else
                    {
                        VOID_TO_NPVARIANT(result);
                        return INVOKERESULT_NO_ERROR;
                    }
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            case ID_playlist_prev:
                if( argCount == 0 )
                {
                    libvlc_playlist_prev(p_plugin->getVLC(), &ex);
                    if( libvlc_exception_raised(&ex) )
                    {
                        NPN_SetException(this, libvlc_exception_get_message(&ex));
                        libvlc_exception_clear(&ex);
                        return INVOKERESULT_GENERIC_ERROR;
                    }
                    else
                    {
                        VOID_TO_NPVARIANT(result);
                        return INVOKERESULT_NO_ERROR;
                    }
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            case ID_playlist_clear: /* deprecated */
                if( argCount == 0 )
                {
                    libvlc_playlist_clear(p_plugin->getVLC(), &ex);
                    if( libvlc_exception_raised(&ex) )
                    {
                        NPN_SetException(this, libvlc_exception_get_message(&ex));
                        libvlc_exception_clear(&ex);
                        return INVOKERESULT_GENERIC_ERROR;
                    }
                    else
                    {
                        VOID_TO_NPVARIANT(result);
                        return INVOKERESULT_NO_ERROR;
                    }
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            case ID_playlist_removeitem: /* deprecated */
                if( (argCount == 1) && isNumberValue(args[0]) )
                {
                    libvlc_playlist_delete_item(p_plugin->getVLC(), numberValue(args[0]), &ex);
                    if( libvlc_exception_raised(&ex) )
                    {
                        NPN_SetException(this, libvlc_exception_get_message(&ex));
                        libvlc_exception_clear(&ex);
                        return INVOKERESULT_GENERIC_ERROR;
                    }
                    else
                    {
                        VOID_TO_NPVARIANT(result);
                        return INVOKERESULT_NO_ERROR;
                    }
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            default:
                ;
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

void LibvlcPlaylistNPObject::parseOptions(const NPString &nps, int *i_options, char*** ppsz_options)
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

void LibvlcPlaylistNPObject::parseOptions(NPObject *obj, int *i_options, char*** ppsz_options)
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
                }
                *i_options = nOptions;
                *ppsz_options = options;
            }
        }
    }
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
    "teletext"
};

enum LibvlcVideoNPObjectPropertyIds
{
    ID_video_fullscreen,
    ID_video_height,
    ID_video_width,
    ID_video_aspectratio,
    ID_video_subtitle,
    ID_video_crop,
    ID_video_teletext
};

const int LibvlcVideoNPObject::propertyCount = sizeof(LibvlcVideoNPObject::propertyNames)/sizeof(NPUTF8 *);

RuntimeNPObject::InvokeResult LibvlcVideoNPObject::getProperty(int index, NPVariant &result)
{
    /* is plugin still running */
    if( _instance->pdata )
    {
        VlcPlugin* p_plugin = reinterpret_cast<VlcPlugin*>(_instance->pdata);
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_media_player_t *p_md = libvlc_playlist_get_media_player(p_plugin->getVLC(), &ex);
        if( libvlc_exception_raised(&ex) )
        {
            NPN_SetException(this, libvlc_exception_get_message(&ex));
            libvlc_exception_clear(&ex);
            return INVOKERESULT_GENERIC_ERROR;
        }

        switch( index )
        {
            case ID_video_fullscreen:
            {
                int val = libvlc_get_fullscreen(p_md, &ex);
                libvlc_media_player_release(p_md);
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                BOOLEAN_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_height:
            {
                int val = libvlc_video_get_height(p_md, &ex);
                libvlc_media_player_release(p_md);
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                INT32_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_width:
            {
                int val = libvlc_video_get_width(p_md, &ex);
                libvlc_media_player_release(p_md);
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                INT32_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_aspectratio:
            {
                NPUTF8 *psz_aspect = libvlc_video_get_aspect_ratio(p_md, &ex);
                libvlc_media_player_release(p_md);
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                if( !psz_aspect )
                    return INVOKERESULT_GENERIC_ERROR;

                STRINGZ_TO_NPVARIANT(psz_aspect, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_subtitle:
            {
                int i_spu = libvlc_video_get_spu(p_md, &ex);
                libvlc_media_player_release(p_md);
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                INT32_TO_NPVARIANT(i_spu, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_crop:
            {
                NPUTF8 *psz_geometry = libvlc_video_get_crop_geometry(p_md, &ex);
                libvlc_media_player_release(p_md);
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                if( !psz_geometry )
                    return INVOKERESULT_GENERIC_ERROR;

                STRINGZ_TO_NPVARIANT(psz_geometry, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_teletext:
            {
                int i_page = libvlc_video_get_teletext(p_md, &ex);
                libvlc_media_player_release(p_md);
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                INT32_TO_NPVARIANT(i_page, result);
                return INVOKERESULT_NO_ERROR;
            }
        }
        libvlc_media_player_release(p_md);
    }
    return INVOKERESULT_GENERIC_ERROR;
}

RuntimeNPObject::InvokeResult LibvlcVideoNPObject::setProperty(int index, const NPVariant &value)
{
    /* is plugin still running */
    if( _instance->pdata )
    {
        VlcPlugin* p_plugin = reinterpret_cast<VlcPlugin*>(_instance->pdata);
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_media_player_t *p_md = libvlc_playlist_get_media_player(p_plugin->getVLC(), &ex);
        if( libvlc_exception_raised(&ex) )
        {
            NPN_SetException(this, libvlc_exception_get_message(&ex));
            libvlc_exception_clear(&ex);
            return INVOKERESULT_GENERIC_ERROR;
        }

        switch( index )
        {
            case ID_video_fullscreen:
            {
                if( ! NPVARIANT_IS_BOOLEAN(value) )
                {
                    libvlc_media_player_release(p_md);
                    return INVOKERESULT_INVALID_VALUE;
                }

                int val = NPVARIANT_TO_BOOLEAN(value);
                libvlc_set_fullscreen(p_md, val, &ex);
                libvlc_media_player_release(p_md);

                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_aspectratio:
            {
                char *psz_aspect = NULL;

                if( ! NPVARIANT_IS_STRING(value) )
                {
                    libvlc_media_player_release(p_md);
                    return INVOKERESULT_INVALID_VALUE;
                }

                psz_aspect = stringValue(NPVARIANT_TO_STRING(value));
                if( !psz_aspect )
                {
                    libvlc_media_player_release(p_md);
                    return INVOKERESULT_GENERIC_ERROR;
                }

                libvlc_video_set_aspect_ratio(p_md, psz_aspect, &ex);
                free(psz_aspect);
                libvlc_media_player_release(p_md);

                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_subtitle:
            {
                if( isNumberValue(value) )
                {
                    libvlc_video_set_spu(p_md,
                                         numberValue(value), &ex);
                    libvlc_media_player_release(p_md);
                    if( libvlc_exception_raised(&ex) )
                    {
                        NPN_SetException(this, libvlc_exception_get_message(&ex));
                        libvlc_exception_clear(&ex);
                        return INVOKERESULT_GENERIC_ERROR;
                    }
                    return INVOKERESULT_NO_ERROR;
                }
                libvlc_media_player_release(p_md);
                return INVOKERESULT_INVALID_VALUE;
            }
            case ID_video_crop:
            {
                char *psz_geometry = NULL;

                if( ! NPVARIANT_IS_STRING(value) )
                {
                    libvlc_media_player_release(p_md);
                    return INVOKERESULT_INVALID_VALUE;
                }

                psz_geometry = stringValue(NPVARIANT_TO_STRING(value));
                if( !psz_geometry )
                {
                    libvlc_media_player_release(p_md);
                    return INVOKERESULT_GENERIC_ERROR;
                }

                libvlc_video_set_crop_geometry(p_md, psz_geometry, &ex);
                free(psz_geometry);
                libvlc_media_player_release(p_md);

                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_teletext:
            {
                if( isNumberValue(value) )
                {
                    libvlc_video_set_teletext(p_md,
                                         numberValue(value), &ex);
                    libvlc_media_player_release(p_md);
                    if( libvlc_exception_raised(&ex) )
                    {
                        NPN_SetException(this, libvlc_exception_get_message(&ex));
                        libvlc_exception_clear(&ex);
                        return INVOKERESULT_GENERIC_ERROR;
                    }
                    return INVOKERESULT_NO_ERROR;
                }
                libvlc_media_player_release(p_md);
                return INVOKERESULT_INVALID_VALUE;
            }
        }
        libvlc_media_player_release(p_md);
    }
    return INVOKERESULT_GENERIC_ERROR;
}

const NPUTF8 * const LibvlcVideoNPObject::methodNames[] =
{
    "toggleFullscreen",
    "toggleTeletext"
};

enum LibvlcVideoNPObjectMethodIds
{
    ID_video_togglefullscreen,
    ID_video_toggleteletext
};

const int LibvlcVideoNPObject::methodCount = sizeof(LibvlcVideoNPObject::methodNames)/sizeof(NPUTF8 *);

RuntimeNPObject::InvokeResult LibvlcVideoNPObject::invoke(int index, const NPVariant *args, uint32_t argCount, NPVariant &result)
{
    /* is plugin still running */
    if( _instance->pdata )
    {
        VlcPlugin* p_plugin = reinterpret_cast<VlcPlugin*>(_instance->pdata);
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_media_player_t *p_md = libvlc_playlist_get_media_player(p_plugin->getVLC(), &ex);
        if( libvlc_exception_raised(&ex) )
        {
            NPN_SetException(this, libvlc_exception_get_message(&ex));
            libvlc_exception_clear(&ex);
            return INVOKERESULT_GENERIC_ERROR;
        }

        switch( index )
        {
            case ID_video_togglefullscreen:
                if( argCount == 0 )
                {
                    libvlc_toggle_fullscreen(p_md, &ex);
                    libvlc_media_player_release(p_md);
                    if( libvlc_exception_raised(&ex) )
                    {
                        NPN_SetException(this, libvlc_exception_get_message(&ex));
                        libvlc_exception_clear(&ex);
                        return INVOKERESULT_GENERIC_ERROR;
                    }
                    else
                    {
                        VOID_TO_NPVARIANT(result);
                        return INVOKERESULT_NO_ERROR;
                    }
                }
                else
                {
                    /* cannot get md, probably not playing */
                    if( libvlc_exception_raised(&ex) )
                    {
                        NPN_SetException(this, libvlc_exception_get_message(&ex));
                        libvlc_exception_clear(&ex);
                    }
                    return INVOKERESULT_GENERIC_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            case ID_video_toggleteletext:
                if( argCount == 0 )
                {
                    libvlc_toggle_teletext(p_md, &ex);
                    libvlc_media_player_release(p_md);
                    if( libvlc_exception_raised(&ex) )
                    {
                        NPN_SetException(this, libvlc_exception_get_message(&ex));
                        libvlc_exception_clear(&ex);
                        return INVOKERESULT_GENERIC_ERROR;
                    }
                    else
                    {
                        VOID_TO_NPVARIANT(result);
                        return INVOKERESULT_NO_ERROR;
                    }
                }
                else
                {
                    /* cannot get md, probably not playing */
                    if( libvlc_exception_raised(&ex) )
                    {
                        NPN_SetException(this, libvlc_exception_get_message(&ex));
                        libvlc_exception_clear(&ex);
                    }
                    return INVOKERESULT_GENERIC_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            default:
                return INVOKERESULT_NO_SUCH_METHOD;
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

