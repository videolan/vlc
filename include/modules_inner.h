/*****************************************************************************
 * modules_inner.h : Macros used from within a module.
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
 * Check that we are within a module.
 *****************************************************************************/
#ifndef MODULE_NAME
#  error "You must define MODULE_NAME before using modules_inner.h !"
#endif

/*****************************************************************************
 * Add a few defines. You do not want to read this section. Really.
 *****************************************************************************/

/* Explanation:
 *
 * if user has #defined MODULE_NAME foo, then we will need:
 * #define MODULE_STRING "foo"
 * #define MODULE_VAR(blah) "VLC_MODULE_foo_blah"
 *
 * and, if BUILTIN is set, we will also need:
 * #define InitModule foo_InitModule
 * #define ActivateModule foo_ActivateModule
 * #define DeactivateModule foo_DeactivateModule
 *
 * this can't easily be done with the C preprocessor, thus a few ugly hacks.
 */

/* I can't believe I need to do this to change « foo » to « "foo" » */
#define UGLY_KLUDGE(z) NASTY_CROCK(z)
#define NASTY_CROCK(z) #z
/* And I need to do _this_ to change « foo bar » to « foo_bar » ! */
#define AWFUL_BRITTLE(y,z) CRUDE_HACK(y,z)
#define CRUDE_HACK(y,z) y##_##z

/* Also, I need to do this to change « blah » to « "VLC_MODULE_foo_blah" » */
#define MODULE_STRING UGLY_KLUDGE(MODULE_NAME)
#define MODULE_VAR(z) "VLC_MODULE_" UGLY_KLUDGE(MODULE_NAME) "_" #z

/* If the module is built-in, then we need to define foo_InitModule instead
 * of InitModule. Same for Activate- and DeactivateModule. */
#ifdef BUILTIN
#  define InitModule AWFUL_BRITTLE(MODULE_NAME,InitModule)
#  define ActivateModule AWFUL_BRITTLE(MODULE_NAME,ActivateModule)
#  define DeactivateModule AWFUL_BRITTLE(MODULE_NAME,DeactivateModule)
#endif

/*****************************************************************************
 * Macros used to build the configuration structure.
 *****************************************************************************/
#define MODULE_CONFIG_START( text ) \
static module_config_t p_config[] = { \
    { MODULE_CONFIG_ITEM_START, text, NULL, NULL, NULL },

#define MODULE_CONFIG_END   \
    { MODULE_CONFIG_ITEM_END, NULL, NULL, NULL, NULL } \
};

#define ADD_FRAME( text ) \
    { MODULE_CONFIG_ITEM_FRAME, text, NULL, NULL, NULL },
#define ADD_PANE( text ) \
    { MODULE_CONFIG_ITEM_PANE, text, NULL, NULL, NULL },
#define ADD_COMMENT( text ) \
    { MODULE_CONFIG_ITEM_COMMENT, text, NULL, NULL, NULL },
#define ADD_STRING( text, name, p_update ) \
    { MODULE_CONFIG_ITEM_STRING, text, name, NULL, p_update },
#define ADD_FILE( text, name, p_update ) \
    { MODULE_CONFIG_ITEM_FILE, text, name, NULL, p_update },
#define ADD_CHECK( text, name, p_update ) \
    { MODULE_CONFIG_ITEM_CHECK, text, name, NULL, p_update },
#define ADD_CHOOSE( text, name, p_getlist, p_update ) \
    { MODULE_CONFIG_ITEM_CHOOSE, text, name, p_getlist, p_update },
#define ADD_RADIO( text, name, p_getlist, p_update ) \
    { MODULE_CONFIG_ITEM_RADIO, text, name, p_getlist, p_update },
#define ADD_SCALE( text, name, p_getlist, p_update ) \
    { MODULE_CONFIG_ITEM_SCALE, text, name, p_getlist, p_update },
#define ADD_SPIN( text, name, p_getlist, p_update ) \
    { MODULE_CONFIG_ITEM_SPIN, text, name, p_getlist, p_update },

