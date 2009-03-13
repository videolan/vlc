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

char * NPP_GetMIMEDescription( void );

NPError NPP_Initialize( void );
jref NPP_GetJavaClass( void );
void NPP_Shutdown( void );

NPError NPP_New( NPMIMEType pluginType, NPP instance, uint16 mode, int16 argc,
                 char* argn[], char* argv[], NPSavedData* saved );
NPError NPP_Destroy( NPP instance, NPSavedData** save );

NPError NPP_GetValue( NPP instance, NPPVariable variable, void *value );
NPError NPP_SetValue( NPP instance, NPNVariable variable, void *value );

NPError NPP_SetWindow( NPP instance, NPWindow* window );

NPError NPP_NewStream( NPP instance, NPMIMEType type, NPStream *stream,
                       NPBool seekable, uint16 *stype );
NPError NPP_DestroyStream( NPP instance, NPStream *stream, NPError reason );
void NPP_StreamAsFile( NPP instance, NPStream *stream, const char* fname );

int32 NPP_WriteReady( NPP instance, NPStream *stream );
int32 NPP_Write( NPP instance, NPStream *stream, int32 offset,
                 int32 len, void *buffer );

void NPP_URLNotify( NPP instance, const char* url,
                    NPReason reason, void* notifyData );
void NPP_Print( NPP instance, NPPrint* printInfo );

#ifdef XP_MACOSX
int16 NPP_HandleEvent( NPP instance, void * event );
#endif

#endif
