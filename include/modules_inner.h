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
 * #define MODULE_FUNC( zog ) module_foo_zog
 *
 * this can't easily be done with the C preprocessor, thus a few ugly hacks.
 */

/* I can't believe I need to do this to change « foo » to « "foo" » */
#define UGLY_KLUDGE( z ) NASTY_CROCK( z )
#define NASTY_CROCK( z ) #z
/* And I need to do _this_ to change « foo bar » to « module_foo_bar » ! */
#define AWFUL_BRITTLE( y, z ) CRUDE_HACK( y, z )
#define CRUDE_HACK( y, z ) module_##y##_##z

#define MODULE_VAR( z ) "VLC_MODULE_" #z

/* If the module is built-in, then we need to define foo_InitModule instead
 * of InitModule. Same for Activate- and DeactivateModule. */
#ifdef BUILTIN
#   define _M( function )    AWFUL_BRITTLE( MODULE_NAME, function )
#   define MODULE_INIT \
      int AWFUL_BRITTLE( MODULE_NAME, InitModule )      ( module_t *p_module )
#   define MODULE_ACTIVATE \
      int AWFUL_BRITTLE( MODULE_NAME, ActivateModule )  ( module_t *p_module )
#   define MODULE_DEACTIVATE \
      int AWFUL_BRITTLE( MODULE_NAME, DeactivateModule )( module_t *p_module )
#else
#   define _M( function )    function
#   define MODULE_INIT       int InitModule       ( module_t *p_module )
#   define MODULE_ACTIVATE   int ActivateModule   ( module_t *p_module )
#   define MODULE_DEACTIVATE int DeactivateModule ( module_t *p_module )
#endif

/* Now the real stuff */
#define MODULE_STRING UGLY_KLUDGE( MODULE_NAME )

/*****************************************************************************
 * Macros used to build the configuration structure.
 *****************************************************************************/
#define MODULE_CONFIG_START \
static module_config_t p_config[] = { \
    { MODULE_CONFIG_ITEM_START, NULL, NULL, NULL, NULL },

#define MODULE_CONFIG_END \
    { MODULE_CONFIG_ITEM_END, NULL, NULL, NULL, NULL } \
};

#define ADD_WINDOW( text ) \
    { MODULE_CONFIG_ITEM_WINDOW, text, NULL, NULL, NULL },
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

