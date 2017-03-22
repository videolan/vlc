/*****************************************************************************
 * playlist_preparser.h:
 *****************************************************************************
 * Copyright (C) 1999-2008 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Clément Stenac <zorglub@videolan.org>
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

#ifndef _PLAYLIST_PREPARSER_H
#define _PLAYLIST_PREPARSER_H 1

#include <vlc_input_item.h>
/**
 * Preparser opaque structure.
 *
 * The preparser object will retrieve the meta data of any given input item in
 * an asynchronous way.
 * It will also issue art fetching requests.
 */
typedef struct playlist_preparser_t playlist_preparser_t;

/**
 * This function creates the preparser object and thread.
 */
playlist_preparser_t *playlist_preparser_New( vlc_object_t * );

/**
 * This function enqueues the provided item to be preparsed.
 *
 * The input item is retained until the preparsing is done or until the
 * preparser object is deleted.
 * Listen to vlc_InputItemPreparseEnded event to get notified when item is
 * preparsed.
 *
 * @param timeout maximum time allowed to preparse the item. If -1, the default
 * "preparse-timeout" option will be used as a timeout. If 0, it will wait
 * indefinitely. If > 0, the timeout will be used (in milliseconds).
 * @param id unique id provided by the caller. This is can be used to cancel
 * the request with playlist_preparser_Cancel()
 */
void playlist_preparser_Push( playlist_preparser_t *, input_item_t *,
                              input_item_meta_request_option_t,
                              int timeout, void *id );

void playlist_preparser_fetcher_Push( playlist_preparser_t *, input_item_t *,
                                      input_item_meta_request_option_t );

/**
 * This function cancel all preparsing requests for a given id
 *
 * @param id unique id given to playlist_preparser_Push()
 */
void playlist_preparser_Cancel( playlist_preparser_t *, void *id );

/**
 * This function destroys the preparser object and thread.
 *
 * All pending input items will be released.
 */
void playlist_preparser_Delete( playlist_preparser_t * );

/**
 * This function deactivates the preparser
 *
 * All pending requests will be removed, and it will block until the currently
 * running entity has finished (if any).
 */
void playlist_preparser_Deactivate( playlist_preparser_t * );

#endif

