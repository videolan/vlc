/*****************************************************************************
 * extensions.c: Check extensions ordering
 *****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#undef NDEBUG

#include <assert.h>

#include <vlc_common.h>
#include <vlc_input_item.h>

static void check_extensions( const char* const* extensions, size_t nb_exts )
{
    for( size_t i = 0; i < nb_exts - 1; i++ )
        assert( strcmp( extensions[i], extensions[i + 1] ) < 0 );
}

#define CHECK_EXTENSION_WRAPPER( ext_list ) \
     do \
     { \
        const char* const exts[] = { ext_list }; \
        check_extensions( exts, ARRAY_SIZE( exts ) ); \
     } while(0);

int main(void)
{
    CHECK_EXTENSION_WRAPPER( MASTER_EXTENSIONS );
    CHECK_EXTENSION_WRAPPER( SLAVE_SPU_EXTENSIONS );
    CHECK_EXTENSION_WRAPPER( SLAVE_AUDIO_EXTENSIONS );
    return 0;
}
