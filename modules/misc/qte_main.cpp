/*****************************************************************************
 * qte_main.c : QT Embedded wrapper for gte_main
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: qte_main.cpp,v 1.1 2003/01/19 22:16:13 jpsaman Exp $
 *
 * Authors: Jean-Paul Saman <jpsaman@wxs.nl>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
extern "C"
{
#include <vlc/vlc.h>
#include <stdlib.h>                                              /* atexit() */
}

#include <qapplication.h>

extern "C"
{

typedef struct qte_thread_t
{
	VLC_COMMON_MEMBERS

    QApplication*       p_qte_application;

} qte_thread_t;

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );

static void QteMain ( qte_thread_t * );

/*****************************************************************************
 * Local variables (mutex-protected).
 *****************************************************************************/
static int            i_refcount = 0;
static qte_thread_t * p_qte_main = NULL;

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Qt Embedded helper module") );
    set_capability( "qte_main", 90 );
    add_shortcut( "qte" );
    set_callbacks( Open, Close );
vlc_module_end();

} /* extern "C" */

/*****************************************************************************
 * Open: initialize and create window
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    vlc_value_t lockval;

    /* FIXME: put this in the module (de)initialization ASAP */
    var_Create( p_this->p_libvlc, "qte", VLC_VAR_MUTEX );

    var_Get( p_this->p_libvlc, "qte", &lockval );
    vlc_mutex_lock( (vlc_mutex_t *) lockval.p_address );

    if( i_refcount > 0 )
    {
        i_refcount++;
        vlc_mutex_unlock( (vlc_mutex_t *) lockval.p_address );

        return VLC_SUCCESS;
    }

    p_qte_main = (qte_thread_t *) vlc_object_create( p_this, sizeof(qte_thread_t) );

    /* Launch the gtk_main() thread. It will not return until it has
     * called gdk_threads_enter(), which ensures us thread safety. */
    if( vlc_thread_create( p_qte_main, "qte_main", QteMain,
                           VLC_THREAD_PRIORITY_LOW, VLC_TRUE ) )
    {
        vlc_object_destroy( p_qte_main );
        i_refcount--;
        vlc_mutex_unlock( (vlc_mutex_t *) lockval.p_address );
        var_Destroy( p_this->p_libvlc, "qte" );
        return VLC_ETHREAD;
    }

    i_refcount++;
    vlc_mutex_unlock( (vlc_mutex_t *) lockval.p_address );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface window
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    vlc_value_t lockval;

    var_Get( p_this->p_libvlc, "qte", &lockval );
    vlc_mutex_lock( (vlc_mutex_t *) lockval.p_address );

    i_refcount--;

    if( i_refcount > 0 )
    {
        vlc_mutex_unlock( (vlc_mutex_t *) lockval.p_address );
        var_Destroy( p_this->p_libvlc, "qte" );
        return;
    }

    p_qte_main->p_qte_application->quit();
    vlc_thread_join( p_qte_main );

    vlc_object_destroy( p_qte_main );
    p_qte_main = NULL;

    vlc_mutex_unlock( (vlc_mutex_t *) lockval.p_address );
    var_Destroy( p_this->p_libvlc, "qte" );
}

/*****************************************************************************
 * QteMain: Qt Embedded thread
 *****************************************************************************
 * this part of the interface is in a separate thread so that we can call
 * qte_main() from within it without annoying the rest of the program.
 *****************************************************************************/
static void QteMain( qte_thread_t *p_this )
{
    int argc = 0;

    msg_Dbg( p_this, "qte_main: enter" );
    QApplication* pApp = new QApplication(argc, NULL);
    if(pApp)
    {
        p_this->p_qte_application = pApp;
    }
    msg_Dbg( p_this, "qte_main: qte application created" );

    /* signal the creation of the window */
    vlc_thread_ready( p_this );
    msg_Dbg( p_this, "qte_main: qte application thread ready" );

    p_this->p_qte_application->exec();
    msg_Dbg( p_this, "qte_main: leaving" );
}

