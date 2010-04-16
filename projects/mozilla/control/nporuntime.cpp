/*****************************************************************************
 * runtime.cpp: support for NPRuntime API for Netscape Script-able plugins
 *              FYI: http://www.mozilla.org/projects/plugins/npruntime.html
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
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

#include "nporuntime.h"
#include "vlcplugin.h"

char* RuntimeNPObject::stringValue(const NPString &s)
{
    NPUTF8 *val = static_cast<NPUTF8*>(malloc((s.UTF8Length+1) * sizeof(*val)));
    if( val )
    {
        strncpy(val, s.UTF8Characters, s.UTF8Length);
        val[s.UTF8Length] = '\0';
    }
    return val;
}

char* RuntimeNPObject::stringValue(const NPVariant &v)
{
    char *s = NULL;
    if( NPVARIANT_IS_STRING(v) )
    {
        return stringValue(NPVARIANT_TO_STRING(v));
    }
    return s;
}

RuntimeNPObject::InvokeResult RuntimeNPObject::getProperty(int index, NPVariant &result)
{
    /* default behaviour */
    return INVOKERESULT_GENERIC_ERROR;
}

RuntimeNPObject::InvokeResult RuntimeNPObject::setProperty(int index, const NPVariant &value)
{
    /* default behaviour */
    return INVOKERESULT_GENERIC_ERROR;
}

RuntimeNPObject::InvokeResult RuntimeNPObject::removeProperty(int index)
{
    /* default behaviour */
    return INVOKERESULT_GENERIC_ERROR;
}

RuntimeNPObject::InvokeResult RuntimeNPObject::invoke(int index, const NPVariant *args, uint32_t argCount, NPVariant &result)
{
    /* default beahviour */
    return INVOKERESULT_GENERIC_ERROR;
}

RuntimeNPObject::InvokeResult RuntimeNPObject::invokeDefault(const NPVariant *args, uint32_t argCount, NPVariant &result)
{
    /* return void */
    VOID_TO_NPVARIANT(result);
    return INVOKERESULT_NO_ERROR;
}

bool RuntimeNPObject::returnInvokeResult(RuntimeNPObject::InvokeResult result)
{
    switch( result )
    {
        case INVOKERESULT_NO_ERROR:
            return true;
        case INVOKERESULT_GENERIC_ERROR:
            break;
        case INVOKERESULT_NO_SUCH_METHOD:
            NPN_SetException(this, "No such method or arguments mismatch");
            break;
        case INVOKERESULT_INVALID_ARGS:
            NPN_SetException(this, "Invalid arguments");
            break;
        case INVOKERESULT_INVALID_VALUE:
            NPN_SetException(this, "Invalid value in assignment");
            break;
        case INVOKERESULT_OUT_OF_MEMORY:
            NPN_SetException(this, "Out of memory");
            break;
    }
    return false;
}

RuntimeNPObject::InvokeResult
RuntimeNPObject::invokeResultString(const char *psz, NPVariant &result)
{
    if( !psz )
        NULL_TO_NPVARIANT(result);
    else
    {
        size_t len = strlen(psz);
        NPUTF8* retval = (NPUTF8*)NPN_MemAlloc(len);
        if( !retval )
            return INVOKERESULT_OUT_OF_MEMORY;
        else
        {
            memcpy(retval, psz, len);
            STRINGN_TO_NPVARIANT(retval, len, result);
        }
    }
    return INVOKERESULT_NO_ERROR;
}

