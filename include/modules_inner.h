/*****************************************************************************
 * modules_inner.h : Macros used from within a module.
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: modules_inner.h,v 1.15 2002/04/23 14:16:20 sam Exp $
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
#if !( defined( MODULE_NAME ) || defined( MAKE_DEP ) )
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
#define STRINGIFY( z )   UGLY_KLUDGE( z )
#define UGLY_KLUDGE( z ) #z
/* And I need to do _this_ to change « foo bar » to « module_foo_bar » ! */
#define CONCATENATE( y, z ) CRUDE_HACK( y, z )
#define CRUDE_HACK( y, z )  y##__MODULE_##z

#define MODULE_VAR( z ) "VLC_MODULE_" #z

/* If the module is built-in, then we need to define foo_InitModule instead
 * of InitModule. Same for Activate- and DeactivateModule. */
#ifdef BUILTIN
#   define _M( function )          CONCATENATE( function, MODULE_NAME )
#   define __VLC_SYMBOL( symbol )  CONCATENATE( symbol, MODULE_NAME )
#   define DECLARE_SYMBOLS         ;
#   define STORE_SYMBOLS           ;
#else
#   define _M( function )          function
#   define __VLC_SYMBOL( symbol  ) CONCATENATE( symbol, MODULE_SYMBOL )
#   define DECLARE_SYMBOLS         module_symbols_t* p_symbols;
#   define STORE_SYMBOLS           p_symbols = p_module->p_symbols;
#endif

#define MODULE_STRING STRINGIFY( MODULE_NAME )

/*
 * InitModule: this function is called once and only once, when the module
 * is looked at for the first time. We get the useful data from it, for
 * instance the module name, its shortcuts, its capabilities... we also create
 * a copy of its config because the module can be unloaded at any time.
 */
#define MODULE_INIT_START                                                     \
    DECLARE_SYMBOLS;                                                          \
    int __VLC_SYMBOL( InitModule ) ( module_t *p_module )                     \
    {                                                                         \
        int i_shortcut = 0;                                                   \
        struct module_config_s* p_item;                                       \
        p_module->psz_name = MODULE_STRING;                                   \
        p_module->psz_longname = MODULE_STRING;                               \
        p_module->psz_program = NULL;                                         \
        p_module->i_capabilities = 0;                                         \
        p_module->i_cpu_capabilities = 0;

#define MODULE_INIT_STOP                                                      \
        STORE_SYMBOLS;                                                        \
        p_module->pp_shortcuts[ i_shortcut ] = NULL;                          \
        p_module->i_config_items = 0;                                         \
        for( p_item = p_config;                                               \
             p_item->i_type != MODULE_CONFIG_HINT_END;                        \
             p_item++ )                                                       \
        {                                                                     \
            if( p_item->i_type & MODULE_CONFIG_ITEM )                         \
                p_module->i_config_items++;                                   \
        }                                                                     \
        vlc_mutex_init( &p_module->config_lock );                             \
        p_module->p_config = config_Duplicate( p_config );                    \
        if( p_module->p_config == NULL )                                      \
        {                                                                     \
            intf_ErrMsg( MODULE_STRING                                        \
                         " InitModule error: can't duplicate p_config" );     \
            return( -1 );                                                     \
        }                                                                     \
        for( p_item = p_module->p_config;                                     \
             p_item->i_type != MODULE_CONFIG_HINT_END;                        \
             p_item++ )                                                       \
        {                                                                     \
            p_item->p_lock = &p_module->config_lock;                          \
        }                                                                     \
        return( 0 );                                                          \
    }

#define ADD_CAPABILITY( cap, score )                                          \
    p_module->i_capabilities |= 1 << MODULE_CAPABILITY_##cap;                 \
    p_module->pi_score[ MODULE_CAPABILITY_##cap ] = score;

#define ADD_REQUIREMENT( cap )                                                \
    p_module->i_cpu_capabilities |= CPU_CAPABILITY_##cap;

#define ADD_PROGRAM( program )                                                \
    p_module->psz_program = program;

#define ADD_SHORTCUT( shortcut )                                              \
    p_module->pp_shortcuts[ i_shortcut ] = shortcut;                          \
    i_shortcut++;

#define SET_DESCRIPTION( desc )                                               \
    p_module->psz_longname = desc;

/*
 * ActivateModule: this function is called before functions can be accessed,
 * we do allocation tasks here, and maybe additional stuff such as large
 * table allocation. Once ActivateModule is called we are almost sure the
 * module will be used.
 */
#define MODULE_ACTIVATE_START                                                 \
    int __VLC_SYMBOL( ActivateModule ) ( module_t *p_module )                 \
    {                                                                         \
        p_module->p_functions =                                               \
          ( module_functions_t * )malloc( sizeof( module_functions_t ) );     \
        if( p_module->p_functions == NULL )                                   \
        {                                                                     \
            return( -1 );                                                     \
        }                                                                     \
        STORE_SYMBOLS;

#define MODULE_ACTIVATE_STOP                                                  \
        return( 0 );                                                          \
    }

/*
 * DeactivateModule: this function is called after we are finished with the
 * module. Everything that has been done in ActivateModule needs to be undone
 * here.
 */
#define MODULE_DEACTIVATE_START                                               \
    int __VLC_SYMBOL( DeactivateModule )( module_t *p_module )                \
    {                                                                         \
        free( p_module->p_functions );

#define MODULE_DEACTIVATE_STOP                                                \
        return( 0 );                                                          \
    }
