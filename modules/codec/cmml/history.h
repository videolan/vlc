/*****************************************************************************
 * history.h: vlc_history_t (web-browser-like back/forward history) handling
 *****************************************************************************
 * Copyright (C) 2004 Commonwealth Scientific and Industrial Research
 *                    Organisation (CSIRO) Australia
 * Copyright (C) 2004 the VideoLAN team
 *
 * $Id$
 *
 * Authors: Andre Pang <Andre.Pang@csiro.au>
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

#ifndef __VLC_HISTORY_H__
#define __VLC_HISTORY_H__

#define XARRAY_EMBED_IN_HOST_C_FILE
#include "xarray.h"

struct history_item_t
{
    char * psz_name;
    char * psz_uri;
};

struct history_t
{
    unsigned int i_index; /* current index into history */
    XArray * p_xarray;
};

typedef struct history_item_t history_item_t;
typedef struct history_t history_t;


/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
history_t       * history_New                        ( void );
bool        history_GoBackSavingCurrentItem    ( history_t *,
                                                       history_item_t * );
bool        history_GoForwardSavingCurrentItem ( history_t *,
                                                       history_item_t * );
bool        history_CanGoBack                  ( history_t * );
bool        history_CanGoForward               ( history_t * );
history_item_t  * history_Item                       ( history_t * );
void              history_Prune                      ( history_t * );
void              history_PruneAndInsert             ( history_t *,
                                                       history_item_t * );
unsigned int      history_Count                      ( history_t * );
unsigned int      history_Index                      ( history_t * );

history_item_t  * historyItem_New                    ( char *, char * );

#endif /* __VLC_HISTORY_H__ */

