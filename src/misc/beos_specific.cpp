/*****************************************************************************
 * beos_init.cpp: Initialization for BeOS specific features 
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
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
#include "defs.h"

#include <Application.h>
#include <Roster.h>
#include <Path.h>
#include <stdio.h>
#include <malloc.h>

extern "C"
{
#include "common.h"
#include "threads.h"
#include "mtime.h"
}
#include "beos_specific.h"



/*****************************************************************************
 * Static vars
 *****************************************************************************/
static vlc_thread_t beos_app_thread;
static char * psz_beos_program_path;


extern "C"
{

void beos_AppThread( void * args )
{
    BApplication * BeApp = new BApplication("application/x-VLC");
    BeApp->Run();
    delete BeApp;
}

void beos_Create( void )
{
    int i_lenght;
    BPath path;
    app_info info; 
    
    vlc_thread_create( &beos_app_thread, "app thread", (vlc_thread_func_t)beos_AppThread, 0 );
    msleep( 100000 );
    // FIXME: we need to verify that be_app is initialized and the msleep is not enough
    //        but the following code does not work as it should and I have no good
    //        solution at the moment.
    //while( be_app == NULL )
    //    msleep( 5000 );
    
    be_app->GetAppInfo(&info); 
    BEntry entry(&info.ref); 
    entry.GetPath(&path); 
    path.GetParent(&path);
    i_lenght = strlen( path.Path() );
    psz_beos_program_path = (char*) malloc( i_lenght+1 ); /* XXX */
    strcpy( psz_beos_program_path, path.Path() );
}

void beos_Destroy( void )
{
    free( psz_beos_program_path ); /* XXX */
    be_app->PostMessage( B_QUIT_REQUESTED );
    vlc_thread_join( beos_app_thread );
}

char * beos_GetProgramPath( void )
{
    return( psz_beos_program_path );
}

} /* extern "C" */
