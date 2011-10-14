/*****************************************************************************
 * log.c: libvlc new API log functions
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 *
 * $Id$
 *
 * Authors: Damien Fouilleul <damienf@videolan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "libvlc_internal.h"
#include <vlc/libvlc.h>
#include <assert.h>

unsigned libvlc_get_log_verbosity( const libvlc_instance_t *p_instance )
{
    (void) p_instance;
    return -1;
}

void libvlc_set_log_verbosity( libvlc_instance_t *p_instance, unsigned level )
{
    (void) p_instance;
    (void) level;
}

libvlc_log_t *libvlc_log_open( libvlc_instance_t *p_instance )
{
    (void) p_instance;
    return malloc(1);
}

void libvlc_log_close( libvlc_log_t *p_log )
{
    free(p_log);
}

unsigned libvlc_log_count( const libvlc_log_t *p_log )
{
    (void) p_log;
    return 0;
}

void libvlc_log_clear( libvlc_log_t *p_log )
{
    (void) p_log;
}

libvlc_log_iterator_t *libvlc_log_get_iterator( const libvlc_log_t *p_log )
{
    return (p_log != NULL) ? malloc(1) : NULL;
}

void libvlc_log_iterator_free( libvlc_log_iterator_t *p_iter )
{
    free( p_iter );
}

int libvlc_log_iterator_has_next( const libvlc_log_iterator_t *p_iter )
{
    (void) p_iter;
    return 0;
}

libvlc_log_message_t *libvlc_log_iterator_next( libvlc_log_iterator_t *p_iter,
                                                libvlc_log_message_t *buffer )
{
    (void) p_iter; (void) buffer;
    return NULL;
}
