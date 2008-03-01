/*****************************************************************************
 * dynamicoverlay_commands.def : dynamic overlay plugin commands
 *****************************************************************************
 * Copyright (C) 2008 the VideoLAN team
 * $Id$
 *
 * Author: SÃ¸ren BÃ¸g <avacore@videolan.org>
 *         Jean-Paul Saman <jpsaman@videolan.org>
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

#include <sys/shm.h>

/* Commands must be sorted alphabetically.
   I haven't found out how to implement quick sort in cpp */
COMMAND( DataSharedMem, INT( i_id ) INT( i_width ) INT( i_height )
         CHARS( p_fourcc, 4 ) INT( i_shmid ), , VLC_TRUE, {
    overlay_t *p_ovl = ListGet( &p_sys->overlays, p_params->i_id );
    if( p_ovl == NULL ) {
        msg_Err( p_filter, "Invalid overlay: %d", p_params->i_id );
        return VLC_EGENERIC;
    }

    struct shmid_ds shminfo;
    if( shmctl( p_params->i_shmid, IPC_STAT, &shminfo ) == -1 ) {
        msg_Err( p_filter, "Unable to access shared memory" );
        return VLC_EGENERIC;
    }
    size_t i_size = shminfo.shm_segsz;

    if( strncmp( p_params->p_fourcc, "TEXT", 4 ) == 0 ) {
        if( p_params->i_height != 1 || p_params->i_width < 1 ) {
            msg_Err( p_filter,
                     "Invalid width and/or height. when specifing text height "
                     "must be 1 and width the number of bytes in the string, "
                     "including the null terminator" );
            return VLC_EGENERIC;
        }

        if( p_params->i_width > i_size ) {
            msg_Err( p_filter,
                     "Insufficient data in shared memory. need %d, got %d",
                     p_params->i_width, i_size );
            return VLC_EGENERIC;
        }

        p_ovl->data.p_text = malloc( p_params->i_width );
        if( p_ovl->data.p_text == NULL )
        {
            msg_Err( p_filter, "Unable to allocate string storage" );
            return VLC_ENOMEM;
        }

        vout_InitFormat( &p_ovl->format, VLC_FOURCC( 'T', 'E', 'X', 'T' ), 0, 0,
                         0 );

        char *p_data = shmat( p_params->i_shmid, NULL, SHM_RDONLY );
        if( p_data == NULL )
        {
            msg_Err( p_filter, "Unable to attach to shared memory" );
            free( p_ovl->data.p_text );
            p_ovl->data.p_text = NULL;
            return VLC_ENOMEM;
        }

        memcpy( p_ovl->data.p_text, p_data, p_params->i_width );

        shmdt( p_data );
    } else {
        p_ovl->data.p_pic = malloc( sizeof( picture_t ) );
        if( p_ovl->data.p_pic == NULL )
        {
            msg_Err( p_filter, "Unable to allocate picture structure" );
            return VLC_ENOMEM;
        }

        vout_InitFormat( &p_ovl->format, VLC_FOURCC( p_params->p_fourcc[0],
                                                     p_params->p_fourcc[1],
                                                     p_params->p_fourcc[2],
                                                     p_params->p_fourcc[3] ),
                         p_params->i_width, p_params->i_height,
                         VOUT_ASPECT_FACTOR );
        if( vout_AllocatePicture( p_filter, p_ovl->data.p_pic,
                                  p_ovl->format.i_chroma, p_params->i_width,
                                  p_params->i_height, p_ovl->format.i_aspect ) )
        {
            msg_Err( p_filter, "Unable to allocate picture" );
            free( p_ovl->data.p_pic );
            p_ovl->data.p_pic = NULL;
            return VLC_ENOMEM;
        }

        size_t i_neededsize = 0;
        for( size_t i_plane = 0; i_plane < p_ovl->data.p_pic->i_planes;
             ++i_plane ) {
            i_neededsize += p_ovl->data.p_pic->p[i_plane].i_visible_lines *
                            p_ovl->data.p_pic->p[i_plane].i_visible_pitch;
        }
        if( i_neededsize > i_size ) {
            msg_Err( p_filter,
                     "Insufficient data in shared memory. need %d, got %d",
                     i_neededsize, i_size );
            p_ovl->data.p_pic->pf_release( p_ovl->data.p_pic );
            free( p_ovl->data.p_pic );
            p_ovl->data.p_pic = NULL;
            return VLC_EGENERIC;
        }

        char *p_data = shmat( p_params->i_shmid, NULL, SHM_RDONLY );
        if( p_data == NULL )
        {
            msg_Err( p_filter, "Unable to attach to shared memory" );
            p_ovl->data.p_pic->pf_release( p_ovl->data.p_pic );
            free( p_ovl->data.p_pic );
            p_ovl->data.p_pic = NULL;
            return VLC_ENOMEM;
        }

        char *p_in = p_data;
        for( size_t i_plane = 0; i_plane < p_ovl->data.p_pic->i_planes;
             ++i_plane ) {
            char *p_out = p_ovl->data.p_pic->p[i_plane].p_pixels;
            for( size_t i_line = 0;
                 i_line < p_ovl->data.p_pic->p[i_plane].i_visible_lines;
                 ++i_line ) {
                p_filter->p_libvlc->pf_memcpy( p_out, p_in,
                                p_ovl->data.p_pic->p[i_plane].i_visible_pitch );
                p_out += p_ovl->data.p_pic->p[i_plane].i_pitch;
                p_in += p_ovl->data.p_pic->p[i_plane].i_visible_pitch;
            }
        }

        shmdt( p_data );
    }

    p_sys->b_updated = p_ovl->b_active;

    return VLC_SUCCESS;
} )
COMMAND( DeleteImage, INT( i_id ), , VLC_TRUE, {
    p_sys->b_updated = VLC_TRUE;

    return ListRemove( &p_sys->overlays, p_params->i_id );
} )
COMMAND( EndAtomic, , , VLC_FALSE, {
    QueueTransfer( &p_sys->pending, &p_sys->atomic );
    p_sys->b_atomic = VLC_FALSE;
    return VLC_SUCCESS;
} )
COMMAND( GenImage, , INT( i_newid ), VLC_FALSE, {
    overlay_t *p_ovl = OverlayCreate();
    if( p_ovl == NULL ) {
        return VLC_ENOMEM;
    }

    ssize_t i_idx = ListAdd( &p_sys->overlays, p_ovl );
    if( i_idx < 0 ) {
        return i_idx;
    }

    p_results->i_newid = i_idx;
    return VLC_SUCCESS;
} )
COMMAND( GetAlpha, INT( i_id ), INT( i_alpha ), VLC_FALSE, {
    overlay_t *p_ovl = ListGet( &p_sys->overlays, p_params->i_id );
    if( p_ovl == NULL ) {
        return VLC_EGENERIC;
    }
    p_results->i_alpha = p_ovl->i_alpha;

    return VLC_SUCCESS;
} )
COMMAND( GetPosition, INT( i_id ), INT( i_x ) INT( i_y ), VLC_FALSE, {
    overlay_t *p_ovl = ListGet( &p_sys->overlays, p_params->i_id );
    if( p_ovl == NULL ) {
        return VLC_EGENERIC;
    }
    p_results->i_x = p_ovl->i_x;
    p_results->i_y = p_ovl->i_y;

    return VLC_SUCCESS;
} )
COMMAND( GetVisibility, INT( i_id ), INT( i_vis ), VLC_FALSE, {
    overlay_t *p_ovl = ListGet( &p_sys->overlays, p_params->i_id );
    if( p_ovl == NULL ) {
        return VLC_EGENERIC;
    }
    p_results->i_vis = ( p_ovl->b_active == VLC_TRUE ) ? 1 : 0;

    return VLC_SUCCESS;
} )
COMMAND( SetAlpha, INT( i_id ) INT( i_alpha ), , VLC_TRUE, {
    overlay_t *p_ovl = ListGet( &p_sys->overlays, p_params->i_id );
    if( p_ovl == NULL ) {
        return VLC_EGENERIC;
    }
    p_ovl->i_alpha = p_params->i_alpha;

    p_sys->b_updated = p_ovl->b_active;

    return VLC_SUCCESS;
} )
COMMAND( SetPosition, INT( i_id ) INT( i_x ) INT( i_y ), , VLC_TRUE, {
    overlay_t *p_ovl = ListGet( &p_sys->overlays, p_params->i_id );
    if( p_ovl == NULL ) {
        return VLC_EGENERIC;
    }
    p_ovl->i_x = p_params->i_x;
    p_ovl->i_y = p_params->i_y;

    p_sys->b_updated = p_ovl->b_active;

    return VLC_SUCCESS;
} )
COMMAND( SetVisibility, INT( i_id ) INT( i_vis ), , VLC_TRUE, {
    overlay_t *p_ovl = ListGet( &p_sys->overlays, p_params->i_id );
    if( p_ovl == NULL ) {
        return VLC_EGENERIC;
    }
    p_ovl->b_active = ( p_params->i_vis == 0 ) ? VLC_FALSE : VLC_TRUE;

    p_sys->b_updated = VLC_TRUE;

    return VLC_SUCCESS;
} )
COMMAND( StartAtomic, , , VLC_FALSE, {
    p_sys->b_atomic = VLC_TRUE;
    return VLC_SUCCESS;
} )
