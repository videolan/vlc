/*****************************************************************************
 * lru.c : Last recently used cache implementation
 *****************************************************************************
 * Copyright (C) 2020 - VideoLabs, VLC authors and VideoLAN
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
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_list.h>
#include "lru.h"

struct vlc_lru_entry
{
    char *psz_key;
    void *value;
    struct vlc_list node;
};

struct vlc_lru
{
    void (*releaseValue)(void *, void *);
    void *priv;
    unsigned max;
    vlc_dictionary_t dict;
    struct vlc_list list;
    struct vlc_lru_entry *last;
};

static void vlc_lru_releaseentry( void *value, void *priv )
{
    struct vlc_lru_entry *entry = value;
    struct vlc_lru *lru = priv;
    free(entry->psz_key);
    if( lru->releaseValue )
        lru->releaseValue( lru->priv, entry->value );
    free(entry);
}

vlc_lru * vlc_lru_New( unsigned max,
                       void(*releaseValue)(void *, void *), void *priv )
{
    vlc_lru *lru = malloc(sizeof(*lru));
    if( lru )
    {
        lru->priv = priv;
        lru->max = max;
        vlc_dictionary_init( &lru->dict, max );
        vlc_list_init( &lru->list );
        lru->releaseValue = releaseValue;
        lru->last = NULL;
    }
    return lru;
}

void vlc_lru_Release( vlc_lru *lru )
{
    vlc_dictionary_clear( &lru->dict, vlc_lru_releaseentry, lru );
    free( lru );
}

bool vlc_lru_HasKey( vlc_lru *lru, const char *psz_key )
{
    return vlc_dictionary_has_key( &lru->dict, psz_key );
}

void * vlc_lru_Get( vlc_lru *lru, const char *psz_key )
{
    struct vlc_lru_entry *entry = vlc_dictionary_value_for_key( &lru->dict, psz_key );
    if( entry )
    {
        if( !vlc_list_is_first( &entry->node, &lru->list ) )
        {
            if( vlc_list_is_last( &entry->node, &lru->list ) )
            {
                lru->last = vlc_list_entry(entry->node.prev, struct vlc_lru_entry, node);
            }
            vlc_list_remove( &entry->node );
            vlc_list_add_after( &entry->node, &lru->list );
        }
        return entry->value;
    }
    return NULL;
}

void vlc_lru_Insert( vlc_lru *lru, const char *psz_key, void *value )
{
    struct vlc_lru_entry *entry = calloc(1, sizeof(*entry));
    if(!entry)
    {
        lru->releaseValue(lru->priv, value);
        return;
    }
    entry->psz_key = strdup( psz_key );
    if(!entry->psz_key)
    {
        lru->releaseValue(lru->priv, value);
        free(entry);
        return;
    }
    entry->value = value;
    vlc_list_init( &entry->node );

    if( vlc_list_is_empty( &lru->list ) )
        lru->last = entry;
    vlc_dictionary_insert( &lru->dict, psz_key, entry );
    vlc_list_add_after( &entry->node, &lru->list );

    if( (unsigned)vlc_dictionary_keys_count(&lru->dict) >= lru->max )
    {
        struct vlc_lru_entry *toremove = lru->last;
        lru->last = vlc_list_entry(toremove->node.prev, struct vlc_lru_entry, node);
        vlc_list_remove(&toremove->node);
        vlc_dictionary_remove_value_for_key(&lru->dict, toremove->psz_key, NULL, NULL);
        vlc_lru_releaseentry(toremove, lru);
    }
}

void vlc_lru_Apply( vlc_lru *lru,
                    void(*func)(void *, const char *, void *),
                    void *priv )
{
    struct vlc_lru_entry *entry;
    vlc_list_foreach( entry, &lru->list, node )
        func( priv, entry->psz_key, entry->value );
}
