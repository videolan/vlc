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

#include "npolibvlc.h"
#include "vlcplugin.h"

/*
** implementation of libvlc root object
*/

LibvlcRootNPObject::LibvlcRootNPObject(NPP instance, const NPClass *aClass) :
        RuntimeNPObject(instance, aClass)
{
    audioObj = NPN_CreateObject(instance, RuntimeNPClass<LibvlcAudioNPObject>::getClass());
    inputObj = NPN_CreateObject(instance, RuntimeNPClass<LibvlcInputNPObject>::getClass());
    logObj = NPN_CreateObject(instance, RuntimeNPClass<LibvlcLogNPObject>::getClass());
    playlistObj = NPN_CreateObject(instance, RuntimeNPClass<LibvlcPlaylistNPObject>::getClass());
    videoObj = NPN_CreateObject(instance,RuntimeNPClass<LibvlcVideoNPObject>::getClass());
}

LibvlcRootNPObject::~LibvlcRootNPObject()
{
    NPN_ReleaseObject(audioObj);
    NPN_ReleaseObject(inputObj);
    NPN_ReleaseObject(logObj);
    NPN_ReleaseObject(playlistObj);
    NPN_ReleaseObject(videoObj);
}

const NPUTF8 * const LibvlcRootNPObject::propertyNames[] = 
{
    "audio",
    "input",
    "playlist",
    "video",
};

const int LibvlcRootNPObject::propertyCount = sizeof(LibvlcRootNPObject::propertyNames)/sizeof(NPUTF8 *);

enum LibvlcRootNPObjectPropertyIds
{
    ID_audio = 0,
    ID_input,
    ID_playlist,
    ID_video,
};

RuntimeNPObject::InvokeResult LibvlcRootNPObject::getProperty(int index, NPVariant &result)
{
    switch( index )
    {
        case ID_audio:
            OBJECT_TO_NPVARIANT(NPN_RetainObject(audioObj), result);
            return INVOKERESULT_NO_ERROR;
        case ID_input:
            OBJECT_TO_NPVARIANT(NPN_RetainObject(inputObj), result);
            return INVOKERESULT_NO_ERROR;
        case ID_playlist:
            OBJECT_TO_NPVARIANT(NPN_RetainObject(playlistObj), result);
            return INVOKERESULT_NO_ERROR;
        case ID_video:
            OBJECT_TO_NPVARIANT(NPN_RetainObject(videoObj), result);
            return INVOKERESULT_NO_ERROR;
    }
    return INVOKERESULT_GENERIC_ERROR;
}

const NPUTF8 * const LibvlcRootNPObject::methodNames[] =
{
    /* no methods */
};

const int LibvlcRootNPObject::methodCount = sizeof(LibvlcRootNPObject::methodNames)/sizeof(NPUTF8 *);

/*
** implementation of libvlc audio object
*/

const NPUTF8 * const LibvlcAudioNPObject::propertyNames[] = 
{
    "mute",
    "volume",
};

const int LibvlcAudioNPObject::propertyCount = sizeof(LibvlcAudioNPObject::propertyNames)/sizeof(NPUTF8 *);

enum LibvlcAudioNPObjectPropertyIds
{
    ID_mute,
    ID_volume,
};

RuntimeNPObject::InvokeResult LibvlcAudioNPObject::getProperty(int index, NPVariant &result)
{
    VlcPlugin *p_plugin = reinterpret_cast<VlcPlugin *>(_instance->pdata);
    if( p_plugin )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        switch( index )
        {
            case ID_mute:
            {
                vlc_bool_t muted = libvlc_audio_get_mute(p_plugin->getVLC(), &ex);
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                BOOLEAN_TO_NPVARIANT(muted, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_volume:
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
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

RuntimeNPObject::InvokeResult LibvlcAudioNPObject::setProperty(int index, const NPVariant &value)
{
    VlcPlugin *p_plugin = reinterpret_cast<VlcPlugin *>(_instance->pdata);
    if( p_plugin )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        switch( index )
        {
            case ID_mute:
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
            case ID_volume:
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
    ID_togglemute,
};

RuntimeNPObject::InvokeResult LibvlcAudioNPObject::invoke(int index, const NPVariant *args, uint32_t argCount, NPVariant &result)
{
    VlcPlugin *p_plugin = reinterpret_cast<VlcPlugin *>(_instance->pdata);
    if( p_plugin )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        switch( index )
        {
            case ID_togglemute:
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
                return INVOKERESULT_NO_SUCH_METHOD;
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
    ID_length,
    ID_position,
    ID_time,
    ID_state,
    ID_rate,
    ID_fps,
    ID_hasvout,
};

RuntimeNPObject::InvokeResult LibvlcInputNPObject::getProperty(int index, NPVariant &result)
{
    VlcPlugin *p_plugin = reinterpret_cast<VlcPlugin *>(_instance->pdata);
    if( p_plugin )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_input_t *p_input = libvlc_playlist_get_input(p_plugin->getVLC(), &ex);
        if( libvlc_exception_raised(&ex) )
        {
            if( index != ID_state )
            {
                NPN_SetException(this, libvlc_exception_get_message(&ex));
                libvlc_exception_clear(&ex);
                return INVOKERESULT_GENERIC_ERROR;
            }
            else
            {
                /* for input state, return CLOSED rather than an exception */
                INT32_TO_NPVARIANT(0, result);
                return INVOKERESULT_NO_ERROR;
            }
        }

        switch( index )
        {
            case ID_length:
            {
                double val = (double)libvlc_input_get_length(p_input, &ex);
                libvlc_input_free(p_input);
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                DOUBLE_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_position:
            {
                double val = libvlc_input_get_position(p_input, &ex);
                libvlc_input_free(p_input);
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                DOUBLE_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_time:
            {
                double val = (double)libvlc_input_get_time(p_input, &ex);
                libvlc_input_free(p_input);
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                DOUBLE_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_state:
            {
                int val = libvlc_input_get_state(p_input, &ex);
                libvlc_input_free(p_input);
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                INT32_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_rate:
            {
                float val = libvlc_input_get_rate(p_input, &ex);
                libvlc_input_free(p_input);
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                DOUBLE_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_fps:
            {
                double val = libvlc_input_get_fps(p_input, &ex);
                libvlc_input_free(p_input);
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                DOUBLE_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_hasvout:
            {
                vlc_bool_t val = libvlc_input_has_vout(p_input, &ex);
                libvlc_input_free(p_input);
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                BOOLEAN_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
        }
        libvlc_input_free(p_input);
    }
    return INVOKERESULT_GENERIC_ERROR;
}

RuntimeNPObject::InvokeResult LibvlcInputNPObject::setProperty(int index, const NPVariant &value)
{
    VlcPlugin *p_plugin = reinterpret_cast<VlcPlugin *>(_instance->pdata);
    if( p_plugin )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_input_t *p_input = libvlc_playlist_get_input(p_plugin->getVLC(), &ex);
        if( libvlc_exception_raised(&ex) )
        {
            NPN_SetException(this, libvlc_exception_get_message(&ex));
            libvlc_exception_clear(&ex);
            return INVOKERESULT_GENERIC_ERROR;
        }

        switch( index )
        {
            case ID_position:
            {
                if( ! NPVARIANT_IS_DOUBLE(value) )
                {
                    libvlc_input_free(p_input);
                    return INVOKERESULT_INVALID_VALUE;
                }

                float val = (float)NPVARIANT_TO_DOUBLE(value);
                libvlc_input_set_position(p_input, val, &ex);
                libvlc_input_free(p_input);
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                return INVOKERESULT_NO_ERROR;
            }
            case ID_time:
            {
                vlc_int64_t val;
                if( NPVARIANT_IS_INT32(value) )
                    val = (vlc_int64_t)NPVARIANT_TO_INT32(value);
                else if( NPVARIANT_IS_DOUBLE(value) )
                    val = (vlc_int64_t)NPVARIANT_TO_DOUBLE(value);
                else
                {
                    libvlc_input_free(p_input);
                    return INVOKERESULT_INVALID_VALUE;
                }

                libvlc_input_set_time(p_input, val, &ex);
                libvlc_input_free(p_input);
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                return INVOKERESULT_NO_ERROR;
            }
            case ID_rate:
            {
                float val;
                if( NPVARIANT_IS_INT32(value) )
                    val = (float)NPVARIANT_TO_INT32(value);
                else if( NPVARIANT_IS_DOUBLE(value) )
                    val = (float)NPVARIANT_TO_DOUBLE(value);
                else
                {
                    libvlc_input_free(p_input);
                    return INVOKERESULT_INVALID_VALUE;
                }

                libvlc_input_set_rate(p_input, val, &ex);
                libvlc_input_free(p_input);
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                return INVOKERESULT_NO_ERROR;
            }
        }
        libvlc_input_free(p_input);
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
    ID_severity,
    ID_type,
    ID_name,
    ID_header,
    ID_message,
};

RuntimeNPObject::InvokeResult LibvlcMessageNPObject::getProperty(int index, NPVariant &result)
{
    switch( index )
    {
        case ID_severity:
        {
            INT32_TO_NPVARIANT(_msg.i_severity, result);
            return INVOKERESULT_NO_ERROR;
        }
        case ID_type:
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
        case ID_name:
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
        case ID_header:
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
        case ID_message:
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

void LibvlcMessageIteratorNPObject::setLog(LibvlcLogNPObject* p_vlclog)
{
    _p_vlclog = p_vlclog;
    if( p_vlclog->_p_log )
    {
        _p_iter = libvlc_log_get_iterator(p_vlclog->_p_log, NULL);
    }
    else
        _p_iter = NULL;
};

const NPUTF8 * const LibvlcMessageIteratorNPObject::propertyNames[] = 
{
    "hasNext",
};

const int LibvlcMessageIteratorNPObject::propertyCount = sizeof(LibvlcMessageIteratorNPObject::propertyNames)/sizeof(NPUTF8 *);

enum LibvlcMessageIteratorNPObjectPropertyIds
{
    ID_hasNext,
};

RuntimeNPObject::InvokeResult LibvlcMessageIteratorNPObject::getProperty(int index, NPVariant &result)
{
    switch( index )
    {
        case ID_hasNext:
        {
            if( _p_iter &&  _p_vlclog->_p_log )
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
    if( _p_iter &&  _p_vlclog->_p_log )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        switch( index )
        {
            case ID_messageiterator_next:
                if( argCount == 0 )
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
            default:
                return INVOKERESULT_NO_SUCH_METHOD;
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
    ID_count,
};

RuntimeNPObject::InvokeResult LibvlcMessagesNPObject::getProperty(int index, NPVariant &result)
{
    switch( index )
    {
        case ID_count:
        {
            libvlc_log_t *p_log = _p_vlclog->_p_log;
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
    ID_iterator,
};

RuntimeNPObject::InvokeResult LibvlcMessagesNPObject::invoke(int index, const NPVariant *args, uint32_t argCount, NPVariant &result)
{
    libvlc_exception_t ex;
    libvlc_exception_init(&ex);

    switch( index )
    {
        case ID_messages_clear:
            if( argCount == 0 )
            {
                libvlc_log_t *p_log = _p_vlclog->_p_log;
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

        case ID_iterator:
            if( argCount == 0 )
            {
                LibvlcMessageIteratorNPObject* iter =
                    static_cast<LibvlcMessageIteratorNPObject*>(NPN_CreateObject(_instance, RuntimeNPClass<LibvlcMessageIteratorNPObject>::getClass()));
                if( iter )
                {
                    iter->setLog(_p_vlclog);
                    OBJECT_TO_NPVARIANT(iter, result);
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_OUT_OF_MEMORY;
            }
            return INVOKERESULT_NO_SUCH_METHOD;

        default:
            return INVOKERESULT_NO_SUCH_METHOD;
    }
    return INVOKERESULT_GENERIC_ERROR;
}
 
/*
** implementation of libvlc message object
*/

const NPUTF8 * const LibvlcLogNPObject::propertyNames[] = 
{
    "messages",
    "verbosity",
};

const int LibvlcLogNPObject::propertyCount = sizeof(LibvlcLogNPObject::propertyNames)/sizeof(NPUTF8 *);

enum LibvlcLogNPObjectPropertyIds
{
    ID_messages,
    ID_verbosity,
};

RuntimeNPObject::InvokeResult LibvlcLogNPObject::getProperty(int index, NPVariant &result)
{
    VlcPlugin *p_plugin = reinterpret_cast<VlcPlugin *>(_instance->pdata);
    if( p_plugin )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        switch( index )
        {
            case ID_messages:
            {
                OBJECT_TO_NPVARIANT(NPN_RetainObject(_p_vlcmessages), result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_verbosity:
            {
                if( _p_log )
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
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

RuntimeNPObject::InvokeResult LibvlcLogNPObject::setProperty(int index, const NPVariant &value)
{
    VlcPlugin *p_plugin = reinterpret_cast<VlcPlugin *>(_instance->pdata);
    if( p_plugin )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        switch( index )
        {
            case ID_verbosity:
                if( isNumberValue(value) )
                {
                    libvlc_instance_t* p_libvlc = p_plugin->getVLC();
                    int verbosity = numberValue(value);
                    if( verbosity >= 0 )
                    {
                        if( ! _p_log )
                        {
                            _p_log = libvlc_log_open(p_libvlc, &ex);
                            if( libvlc_exception_raised(&ex) )
                            {
                                NPN_SetException(this, libvlc_exception_get_message(&ex));
                                libvlc_exception_clear(&ex);
                                return INVOKERESULT_GENERIC_ERROR;
                            }
                        }
                        libvlc_set_log_verbosity(p_libvlc, (unsigned)verbosity, &ex);
                        if( libvlc_exception_raised(&ex) )
                        {
                            NPN_SetException(this, libvlc_exception_get_message(&ex));
                            libvlc_exception_clear(&ex);
                            return INVOKERESULT_GENERIC_ERROR;
                        }
                    }
                    else if( _p_log )
                    {
                        /* close log  when verbosity is set to -1 */
                        libvlc_log_close(_p_log, &ex);
                        _p_log = NULL;
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
** implementation of libvlc playlist object
*/


const NPUTF8 * const LibvlcPlaylistNPObject::propertyNames[] = 
{
    "itemCount",
    "isPlaying",
};

const int LibvlcPlaylistNPObject::propertyCount = sizeof(LibvlcPlaylistNPObject::propertyNames)/sizeof(NPUTF8 *);

enum LibvlcPlaylistNPObjectPropertyIds
{
    ID_itemcount,
    ID_isplaying,
};

RuntimeNPObject::InvokeResult LibvlcPlaylistNPObject::getProperty(int index, NPVariant &result)
{
    VlcPlugin *p_plugin = reinterpret_cast<VlcPlugin *>(_instance->pdata);
    if( p_plugin )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        switch( index )
        {
            case ID_itemcount:
            {
                int val = libvlc_playlist_items_count(p_plugin->getVLC(), &ex);
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                INT32_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_isplaying:
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
    "clear",
    "removeItem"
};

const int LibvlcPlaylistNPObject::methodCount = sizeof(LibvlcPlaylistNPObject::methodNames)/sizeof(NPUTF8 *);

enum LibvlcPlaylistNPObjectMethodIds
{
    ID_add,
    ID_play,
    ID_playItem,
    ID_togglepause,
    ID_stop,
    ID_next,
    ID_prev,
    ID_clear,
    ID_removeitem,
};

RuntimeNPObject::InvokeResult LibvlcPlaylistNPObject::invoke(int index, const NPVariant *args, uint32_t argCount, NPVariant &result)
{
    VlcPlugin *p_plugin = reinterpret_cast<VlcPlugin *>(_instance->pdata);
    if( p_plugin )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        switch( index )
        {
            case ID_add:
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
                        delete s;
                        if( ! url )
                            // what happened ?
                            return INVOKERESULT_GENERIC_ERROR;
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
                        name = stringValue(NPVARIANT_TO_STRING(args[0]));
                    }
                    else
                        return INVOKERESULT_NO_SUCH_METHOD;
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
                }

                int item = libvlc_playlist_add_extended(p_plugin->getVLC(),
                                                        url,
                                                        name,
                                                        i_options,
                                                        const_cast<const char **>(ppsz_options),
                                                        &ex);
                delete url;
                delete name;
                for( int i=0; i< i_options; ++i )
                {
                    if( ppsz_options[i] )
                        free(ppsz_options[i]);
                }
                if( ppsz_options )
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
            case ID_play:
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
            case ID_playItem:
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
            case ID_togglepause:
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
            case ID_stop:
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
            case ID_next:
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
            case ID_prev:
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
            case ID_clear:
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
            case ID_removeitem:
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
                return INVOKERESULT_NO_SUCH_METHOD;
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}
 
void LibvlcPlaylistNPObject::parseOptions(const NPString &s, int *i_options, char*** ppsz_options)
{
    if( s.utf8length )
    {
        char *val = stringValue(s);
        if( val )
        {
            long capacity = 16;
            char **options = (char **)malloc(capacity*sizeof(char *));
            if( options )
            {
                int nOptions = 0;

                char *end = val + s.utf8length;
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
                                delete val;
                                /* return what we got so far */
                                *i_options = nOptions;
                                *ppsz_options = options;
                                break;
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
            delete val;
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
};

enum LibvlcVideoNPObjectPropertyIds
{
    ID_fullscreen,
    ID_height,
    ID_width,
};

const int LibvlcVideoNPObject::propertyCount = sizeof(LibvlcVideoNPObject::propertyNames)/sizeof(NPUTF8 *);

RuntimeNPObject::InvokeResult LibvlcVideoNPObject::getProperty(int index, NPVariant &result)
{
    VlcPlugin *p_plugin = reinterpret_cast<VlcPlugin *>(_instance->pdata);
    if( p_plugin )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_input_t *p_input = libvlc_playlist_get_input(p_plugin->getVLC(), &ex);
        if( libvlc_exception_raised(&ex) )
        {
            NPN_SetException(this, libvlc_exception_get_message(&ex));
            libvlc_exception_clear(&ex);
            return INVOKERESULT_GENERIC_ERROR;
        }

        switch( index )
        {
            case ID_fullscreen:
            {
                int val = libvlc_get_fullscreen(p_input, &ex);
                libvlc_input_free(p_input);
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                BOOLEAN_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_height:
            {
                int val = libvlc_video_get_height(p_input, &ex);
                libvlc_input_free(p_input);
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                INT32_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_width:
            {
                int val = libvlc_video_get_width(p_input, &ex);
                libvlc_input_free(p_input);
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                INT32_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
        }
        libvlc_input_free(p_input);
    }
    return INVOKERESULT_GENERIC_ERROR;
}

RuntimeNPObject::InvokeResult LibvlcVideoNPObject::setProperty(int index, const NPVariant &value)
{
    VlcPlugin *p_plugin = reinterpret_cast<VlcPlugin *>(_instance->pdata);
    if( p_plugin )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_input_t *p_input = libvlc_playlist_get_input(p_plugin->getVLC(), &ex);
        if( libvlc_exception_raised(&ex) )
        {
            NPN_SetException(this, libvlc_exception_get_message(&ex));
            libvlc_exception_clear(&ex);
            return INVOKERESULT_GENERIC_ERROR;
        }

        switch( index )
        {
            case ID_fullscreen:
            {
                if( ! NPVARIANT_IS_BOOLEAN(value) )
                {
                    libvlc_input_free(p_input);
                    return INVOKERESULT_INVALID_VALUE;
                }

                int val = NPVARIANT_TO_BOOLEAN(value);
                libvlc_set_fullscreen(p_input, val, &ex);
                libvlc_input_free(p_input);
                if( libvlc_exception_raised(&ex) )
                {
                    NPN_SetException(this, libvlc_exception_get_message(&ex));
                    libvlc_exception_clear(&ex);
                    return INVOKERESULT_GENERIC_ERROR;
                }
                return INVOKERESULT_NO_ERROR;
            }
        }
        libvlc_input_free(p_input);
    }
    return INVOKERESULT_GENERIC_ERROR;
}

const NPUTF8 * const LibvlcVideoNPObject::methodNames[] =
{
    "toggleFullscreen",
};

enum LibvlcVideoNPObjectMethodIds
{
    ID_togglefullscreen,
};

const int LibvlcVideoNPObject::methodCount = sizeof(LibvlcVideoNPObject::methodNames)/sizeof(NPUTF8 *);

RuntimeNPObject::InvokeResult LibvlcVideoNPObject::invoke(int index, const NPVariant *args, uint32_t argCount, NPVariant &result)
{
    VlcPlugin *p_plugin = reinterpret_cast<VlcPlugin *>(_instance->pdata);
    if( p_plugin )
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        libvlc_input_t *p_input = libvlc_playlist_get_input(p_plugin->getVLC(), &ex);
        if( libvlc_exception_raised(&ex) )
        {
            NPN_SetException(this, libvlc_exception_get_message(&ex));
            libvlc_exception_clear(&ex);
            return INVOKERESULT_GENERIC_ERROR;
        }

        switch( index )
        {
            case ID_togglefullscreen:
                if( argCount == 0 )
                {
                    libvlc_toggle_fullscreen(p_input, &ex);
                    libvlc_input_free(p_input);
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
                    /* cannot get input, probably not playing */
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

