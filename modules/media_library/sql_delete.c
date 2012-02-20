/*****************************************************************************
 * sql_delete.c: SQL-based media library: all database delete functions
 *****************************************************************************
 * Copyright (C) 2008-2010 The VideoLAN Team and AUTHORS
 * $Id$
 *
 * Authors: Antoine Lejeune <phytos@videolan.org>
 *          Jean-Philippe André <jpeg@videolan.org>
 *          Rémi Duraffort <ivoire@videolan.org>
 *          Adrien Maglo <magsoft@videolan.org>
 *          Srikanth Raju <srikiraju at gmail dot com>
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

#include "sql_media_library.h"


/**
 * @brief Generic DELETE function for many medias
 * Delete a media and all its referencies which don't point
 * an anything else.
 *
 * @param p_ml This media_library_t object
 * @param p_array list of ids to delete
 * @return VLC_SUCCESS or VLC_EGENERIC
 * TODO: Expand to delete media/artist/album given any params
 */
int Delete( media_library_t *p_ml, vlc_array_t *p_array )
{
    char *psz_idlist = NULL, *psz_tmp = NULL;
    int i_return = VLC_ENOMEM;

    int i_rows = 0, i_cols = 0;
    char **pp_results = NULL;

    if( vlc_array_count( p_array ) <= 0 )
    {
        i_return = VLC_SUCCESS;
        goto quit_delete_final;
    }
    for( int i = 0; i < vlc_array_count( p_array ); i++ )
    {
        ml_element_t* find = ( ml_element_t * )
            vlc_array_item_at_index( p_array, i );
        assert( find->criteria == ML_ID );
        if( !psz_idlist )
        {
            if( asprintf( &psz_tmp, "( %d", find->value.i ) == -1)
            {
                goto quit_delete_final;
            }
        }
        else
        {
            if( asprintf( &psz_tmp, "%s, %d", psz_idlist,
                    find->value.i ) == -1)
            {
                goto quit_delete_final;
            }
        }
        free( psz_idlist );
        psz_idlist = psz_tmp;
        psz_tmp = NULL;
    }
    free( psz_tmp );
    if( asprintf( &psz_tmp, "%s )", psz_idlist ? psz_idlist : "(" ) == -1 )
    {
        goto quit_delete_final;
    }
    psz_idlist = psz_tmp;
    psz_tmp = NULL;

    msg_Dbg( p_ml, "Multi Delete id list: %s", psz_idlist );

    /**
     * Below ensures you are emitting media-deleted only
     * for existant media
     */
    Begin( p_ml );
    i_return = Query( p_ml, &pp_results, &i_rows, &i_cols,
            "SELECT id FROM media WHERE id IN %s", psz_idlist );
    if( i_return != VLC_SUCCESS )
        goto quit;

    i_return = QuerySimple( p_ml,
            "DELETE FROM media WHERE media.id IN %s", psz_idlist );
    if( i_return != VLC_SUCCESS )
        goto quit;

    i_return = QuerySimple( p_ml,
            "DELETE FROM extra WHERE extra.id IN %s", psz_idlist );
    if( i_return != VLC_SUCCESS )
        goto quit;

quit:
    if( i_return == VLC_SUCCESS )
    {
        Commit( p_ml );
        /* Emit delete on var media-deleted */
        for( int i = 1; i <= i_rows; i++ )
        {
            var_SetInteger( p_ml, "media-deleted", atoi( pp_results[i*i_cols] ) );
        }
    }
    else
        Rollback( p_ml );
quit_delete_final:
    FreeSQLResult( p_ml, pp_results );
    free( psz_tmp );
    free( psz_idlist );
    return i_return;
}
