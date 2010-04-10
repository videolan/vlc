/*****************************************************************************
 * vlcshell.hp:
 *****************************************************************************
 * Copyright (C) 2009 the VideoLAN team
 * $Id$
 *
 * Authors: Jean-Paul Saman <jpsaman@videolan.org>
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

#ifndef __VLCSHELL_H__
#define __VLCSHELL_H__

/* Mozilla stuff */
#ifdef HAVE_MOZILLA_CONFIG_H
#   include <mozilla-config.h>
#endif


char * NPP_GetMIMEDescription( void );

NPError NPP_Initialize( void );

#ifdef OJI 
jref NPP_GetJavaClass( void );
#endif
void NPP_Shutdown( void );

#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
NPError NPP_New( NPMIMEType pluginType, NPP instance, uint16 mode, int16 argc,
                 char* argn[], char* argv[], NPSavedData* saved );
#else
NPError NPP_New( NPMIMEType pluginType, NPP instance, uint16_t mode, int16_t argc,
                 char* argn[], char* argv[], NPSavedData* saved );
#endif

NPError NPP_Destroy( NPP instance, NPSavedData** save );

NPError NPP_GetValue( NPP instance, NPPVariable variable, void *value );
NPError NPP_SetValue( NPP instance, NPNVariable variable, void *value );

NPError NPP_SetWindow( NPP instance, NPWindow* window );

NPError NPP_NewStream( NPP instance, NPMIMEType type, NPStream *stream,
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
                       NPBool seekable, uint16 *stype );
#else
                       NPBool seekable, uint16_t *stype );
#endif
NPError NPP_DestroyStream( NPP instance, NPStream *stream, NPError reason );
void NPP_StreamAsFile( NPP instance, NPStream *stream, const char* fname );

#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
int32 NPP_WriteReady( NPP instance, NPStream *stream );
int32 NPP_Write( NPP instance, NPStream *stream, int32 offset,
                 int32 len, void *buffer );
#else
int32_t NPP_WriteReady( NPP instance, NPStream *stream );
int32_t NPP_Write( NPP instance, NPStream *stream, int32_t offset,
                 int32_t len, void *buffer );
#endif

void NPP_URLNotify( NPP instance, const char* url,
                    NPReason reason, void* notifyData );
void NPP_Print( NPP instance, NPPrint* printInfo );

#ifdef XP_MACOSX
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
int16 NPP_HandleEvent( NPP instance, void * event );
#else
int16_t NPP_HandleEvent( NPP instance, void * event );
#endif
#endif

#endif
