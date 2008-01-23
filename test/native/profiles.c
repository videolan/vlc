/*****************************************************************************
 * profiles.c: Test streaming profiles
 *****************************************************************************
 * Copyright (C) 2006 The VideoLAN project
 * $Id$
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

#include "../pyunit.h"
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/vlc.h>
#include <vlc_streaming.h>

#define STDCHAIN1 "#std{access=udp,url=12.42.12.42,mux=ts}"
//#define GUICHAIN1
static void BuildStdChain1( sout_chain_t *p_chain )
{
    streaming_ChainAddStd( p_chain, "udp", "ts", "12.42.12.42" );
}

#define TRACHAIN1 "#transcode{vcodec=mpgv,vb=1024,scale=1.0,acodec=mp3,ab=128,channels=2}:std{mux=mp4,access=file,url=/dev/null}"
static void BuildTranscodeChain1( sout_chain_t *p_chain )
{
    streaming_ChainAddTranscode( p_chain, "mpgv", "mp3", NULL, 1024, 1.0,
                                 128, 2, NULL );
    streaming_ChainAddStd( p_chain, "file", "mp4", "/dev/null" );
}

static void BuildInvalid1( sout_chain_t *p_chain )
{
    streaming_ChainAddStd( p_chain, "file", "mp4", "/dev/null" );
    streaming_ChainAddStd( p_chain, "file", "mp4", "/dev/null" );
}

PyObject *chains_test( PyObject *self, PyObject *args )
{
    sout_chain_t *p_chain = streaming_ChainNew();
    sout_duplicate_t *p_dup;
    ASSERT( p_chain->i_modules == 0, "unclean chain" );
    ASSERT( p_chain->i_options == 0, "unclean chain" );
    ASSERT( p_chain->pp_modules == NULL, "unclean chain" );
    ASSERT( p_chain->ppsz_options == NULL, "unclean chain" );

    /* Check duplicate */
    p_dup = streaming_ChainAddDup( p_chain );
    ASSERT( p_chain->i_modules == 1, "not 1 module" );
    ASSERT( p_dup->i_children == 0, "dup has children" );
    streaming_DupAddChild( p_dup );
    ASSERT( p_dup->i_children == 1, "not 1 child" );
    ASSERT( p_dup->pp_children[0]->i_modules == 0, "unclean child chain");
    streaming_DupAddChild( p_dup );
    ASSERT( p_dup->i_children == 2, "not 2 children" );

    Py_INCREF( Py_None );
    return Py_None;
}

PyObject *gui_chains_test( PyObject *self, PyObject *args )
{
    Py_INCREF( Py_None);
    return Py_None;
}

PyObject *psz_chains_test( PyObject *self, PyObject *args )
{
    sout_chain_t *p_chain = streaming_ChainNew();
    sout_module_t *p_module;
    char *psz_output;
    printf( "\n" );

    BuildStdChain1( p_chain );
    psz_output = streaming_ChainToPsz( p_chain );
    printf( "STD1: %s\n", psz_output );
    ASSERT( !strcmp( psz_output, STDCHAIN1 ), "wrong output for STD1" )
    ASSERT( p_chain->i_modules == 1, "wrong number of modules" );
    p_module = p_chain->pp_modules[0];
    ASSERT( p_module->i_type ==  SOUT_MOD_STD, "wrong type of module" );


    Py_INCREF( Py_None);
    return Py_None;
}
