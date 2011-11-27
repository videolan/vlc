/*****************************************************************************
 * variables.h: object variables typedefs
 *****************************************************************************
 * Copyright (C) 2002-2006 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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

#ifndef LIBVLC_VARIABLES_H
# define LIBVLC_VARIABLES_H 1

typedef struct callback_entry_t callback_entry_t;

typedef struct variable_ops_t
{
    int  (*pf_cmp) ( vlc_value_t, vlc_value_t );
    void (*pf_dup) ( vlc_value_t * );
    void (*pf_free) ( vlc_value_t * );
} variable_ops_t;

/**
 * The structure describing a variable.
 * \note vlc_value_t is the common union for variable values
 */
struct variable_t
{
    char *       psz_name; /**< The variable unique name (must be first) */

    /** The variable's exported value */
    vlc_value_t  val;

    /** The variable display name, mainly for use by the interfaces */
    char *       psz_text;

    const variable_ops_t *ops;

    int          i_type;   /**< The type of the variable */
    unsigned     i_usage;  /**< Reference count */

    /** If the variable has min/max/step values */
    vlc_value_t  min, max, step;

    /** Index of the default choice, if the variable is to be chosen in
     * a list */
    int          i_default;
    /** List of choices */
    vlc_list_t   choices;
    /** List of friendly names for the choices */
    vlc_list_t   choices_text;

    /** Set to TRUE if the variable is in a callback */
    bool   b_incallback;

    /** Number of registered callbacks */
    int                i_entries;
    /** Array of registered callbacks */
    callback_entry_t * p_entries;
};

extern void var_DestroyAll( vlc_object_t * );

#endif
