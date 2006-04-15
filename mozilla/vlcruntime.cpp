#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* vlc stuff */
#ifdef USE_LIBVLC
#   include <vlc/vlc.h>
#endif

/* Mozilla stuff */
#ifdef HAVE_MOZILLA_CONFIG_H
#   include <mozilla-config.h>
#endif
#include <nsISupports.h>
#include <nsMemory.h>
#include <npapi.h>
#include <npruntime.h>

#include "vlcplugin.h"
#include "vlcruntime.h"

/*
** utility functions
*/

static PRInt64 NPVariantToPRInt64(const NPVariant &v)
{
    switch( v.type ) {
	case NPVariantType_Bool:
	    return static_cast<PRInt64>(NPVARIANT_TO_BOOLEAN(v));
	case NPVariantType_Int32:
	    return static_cast<PRInt64>(NPVARIANT_TO_INT32(v));
	case NPVariantType_Double:
	    return static_cast<PRInt64>(NPVARIANT_TO_DOUBLE(v));
	default:
	    return 0;
    }
}

/*
** implementation root object
*/

const NPUTF8 * const VlcRuntimeRootObject::propertyNames[] = { };
const NPUTF8 * const VlcRuntimeRootObject::methodNames[] =
{
    "play",
    "pause",
    "stop",
    "fullscreen",
    "set_volume",
    "get_volume",
    "mute",
    "get_int_variable",
    "set_int_variable",
    "get_bool_variable",
    "set_bool_variable",
    "get_str_variable",
    "set_str_variable",
    "clear_playlist",
    "add_item",
    "next",
    "previous",
    "isplaying",
    "get_length",
    "get_position",
    "get_time",
    "seek",
};

enum VlcRuntimeRootObjectMethodIds
{
    ID_play = 0,
    ID_pause,
    ID_stop,
    ID_fullscreen,
    ID_set_volume,
    ID_get_volume,
    ID_mute,
    ID_get_int_variable,
    ID_set_int_variable,
    ID_get_bool_variable,
    ID_set_bool_variable,
    ID_get_str_variable,
    ID_set_str_variable,
    ID_clear_playlist,
    ID_add_item,
    ID_next,
    ID_previous,
    ID_isplaying,
    ID_get_length,
    ID_get_position,
    ID_get_time,
    ID_seek,
};

const int VlcRuntimeRootObject::propertyCount = sizeof(VlcRuntimeRootObject::propertyNames)/sizeof(NPUTF8 *);
const int VlcRuntimeRootObject::methodCount = sizeof(VlcRuntimeRootObject::methodNames)/sizeof(NPUTF8 *);

bool VlcRuntimeRootObject::getProperty(int index, NPVariant *result)
{
    return false;
}

bool VlcRuntimeRootObject::setProperty(int index, const NPVariant *value)
{
    return false;
}

bool VlcRuntimeRootObject::removeProperty(int index)
{
    return false;
}

bool VlcRuntimeRootObject::invoke(int index, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
    VlcPlugin *plugin = (VlcPlugin *)(_instance->pdata);
    if( plugin )
    {
	VlcIntf *peer = plugin->GetPeer();
	switch( index )
	{
	    case ID_play:
		peer->Play();
		VOID_TO_NPVARIANT(*result);
		return true;
	    case ID_pause:
		peer->Pause();
		VOID_TO_NPVARIANT(*result);
		return true;
	    case ID_stop:
		peer->Stop();
		VOID_TO_NPVARIANT(*result);
		return true;
	    case ID_fullscreen:
		peer->Fullscreen();
		VOID_TO_NPVARIANT(*result);
		return true;
	    case ID_set_volume:
		if( argCount == 1 )
		{
		    peer->Set_volume(NPVariantToPRInt64(args[0]));
		    VOID_TO_NPVARIANT(*result);
		    return true;
		}
		return false;
	    case ID_get_volume:
		{
		    PRInt64 val;
		    peer->Get_volume(&val);
		    INT32_TO_NPVARIANT(val, *result);
		    return true;
		}
	    case ID_mute:
		peer->Mute();
		VOID_TO_NPVARIANT(*result);
		return true;
	    case ID_get_int_variable:
		if( (argCount == 1)
		    && NPVARIANT_IS_STRING(args[0]) )
		{
		    const NPString &name = NPVARIANT_TO_STRING(args[0]);
		    NPUTF8 *s = new NPUTF8[name.utf8length+1];
		    if( s )
		    {
			PRInt64 val;
			strncpy(s, name.utf8characters, name.utf8length);
			s[name.utf8length] = '\0';
			peer->Get_int_variable(s, &val);
			INT32_TO_NPVARIANT(val, *result);
			delete s;
			return true;
		    }
		}
		return false;
	    case ID_set_int_variable:
		if( (argCount == 2)
		    && NPVARIANT_IS_STRING(args[0]) )
		{
		    const NPString &name = NPVARIANT_TO_STRING(args[0]);
		    NPUTF8 *s = new NPUTF8[name.utf8length+1];
		    if( s )
		    {
			strncpy(s, name.utf8characters, name.utf8length);
			s[name.utf8length] = '\0';
			peer->Set_int_variable(s, NPVariantToPRInt64(args[1]));
			delete s;
			VOID_TO_NPVARIANT(*result);
			return true;
		    }
		}
		return false;
	    case ID_get_bool_variable:
		if( (argCount == 1)
		    && NPVARIANT_IS_STRING(args[0]) )
		{
		    const NPString &name = NPVARIANT_TO_STRING(args[0]);
		    NPUTF8 *s = new NPUTF8[name.utf8length+1];
		    if( s )
		    {
			PRBool val;
			strncpy(s, name.utf8characters, name.utf8length);
			s[name.utf8length] = '\0';
			peer->Get_bool_variable(s, &val);
			BOOLEAN_TO_NPVARIANT(val, *result);
			delete s;
			return true;
		    }
		}
		return false;
	    case ID_set_bool_variable:
		if( (argCount == 2)
		    && NPVARIANT_IS_STRING(args[0])
		    && NPVARIANT_IS_BOOLEAN(args[1]) )
		{
		    const NPString &name = NPVARIANT_TO_STRING(args[0]);
		    NPUTF8 *s = new NPUTF8[name.utf8length+1];
		    if( s )
		    {
			strncpy(s, name.utf8characters, name.utf8length);
			s[name.utf8length] = '\0';
			peer->Set_bool_variable(s, NPVARIANT_TO_BOOLEAN(args[1]));
			delete s;
			VOID_TO_NPVARIANT(*result);
			return true;
		    }
		}
		return false;
	    case ID_get_str_variable:
		if( (argCount == 1)
		    && NPVARIANT_IS_STRING(args[0]) )
		{
		    const NPString &name = NPVARIANT_TO_STRING(args[0]);
		    NPUTF8 *s = new NPUTF8[name.utf8length+1];
		    if( s )
		    {
			char *val;
			strncpy(s, name.utf8characters, name.utf8length);
			s[name.utf8length] = '\0';
			peer->Get_str_variable(s, &val);
			delete s;
			int len = strlen(val);
			NPUTF8 *retval = (NPUTF8 *)NPN_MemAlloc(len);
			if( retval )
			{
			    memcpy(retval, val, len);
			    STRINGN_TO_NPVARIANT(retval, len, *result);
			    free(val);
			    return true;
			}
			free(val);
		    }
		}
		return false;
	    case ID_set_str_variable:
		if( (argCount == 2)
		    && NPVARIANT_IS_STRING(args[0])
		    && NPVARIANT_IS_STRING(args[1]) )
		{
		    const NPString &name = NPVARIANT_TO_STRING(args[0]);
		    NPUTF8 *s = new NPUTF8[name.utf8length+1];
		    if( s )
		    {
			strncpy(s, name.utf8characters, name.utf8length);
			s[name.utf8length] = '\0';
			const NPString &val = NPVARIANT_TO_STRING(args[1]);
			NPUTF8 *v = new NPUTF8[val.utf8length+1];
			if( v )
			{
			    strncpy(v, val.utf8characters, val.utf8length);
			    v[val.utf8length] = '\0';
			    peer->Set_str_variable(s, v);
			    delete s;
			    delete v;
			    VOID_TO_NPVARIANT(*result);
			    return true;
			}
			delete s;
		    }
		}
		return false;
	    case ID_clear_playlist:
		peer->Clear_playlist();
		VOID_TO_NPVARIANT(*result);
		return true;
	    case ID_add_item:
		if( (argCount == 1)
		    && NPVARIANT_IS_STRING(args[0]) )
		{
		    const NPString &name = NPVARIANT_TO_STRING(args[0]);
		    NPUTF8 *s = new NPUTF8[name.utf8length+1];
		    if( s )
		    {
			strncpy(s, name.utf8characters, name.utf8length);
			s[name.utf8length] = '\0';
			peer->Add_item(s);
			delete s;
			return true;
		    }
		}
		return false;
	    case ID_next:
		peer->Next();
		VOID_TO_NPVARIANT(*result);
		return true;
	    case ID_previous:
		peer->Previous();
		VOID_TO_NPVARIANT(*result);
		return true;
	    case ID_isplaying:
		{
		    PRBool val;
		    peer->Isplaying(&val);
		    BOOLEAN_TO_NPVARIANT(val, *result);
		    return true;
		}
	    case ID_get_length:
		{
		    PRInt64 val;
		    peer->Get_length(&val);
		    DOUBLE_TO_NPVARIANT(val, *result);
		    return true;
		}
	    case ID_get_position:
		{
		    PRInt64 val;
		    peer->Get_position(&val);
		    INT32_TO_NPVARIANT(val, *result);
		    return true;
		}
	    case ID_get_time:
		{
		    PRInt64 val;
		    peer->Get_time(&val);
		    INT32_TO_NPVARIANT(val, *result);
		    return true;
		}
	    case ID_seek:
		if( argCount == 2 )
		{
		    peer->Seek(NPVariantToPRInt64(args[0]), NPVariantToPRInt64(args[1]));
		    VOID_TO_NPVARIANT(*result);
		    return true;
		}
		return false;
	}
	NS_RELEASE(peer);
    }
    return false;
}

bool VlcRuntimeRootObject::invokeDefault(const NPVariant *args, uint32_t argCount, NPVariant *result)
{
    return false;
}

