/*****************************************************************************
 * modules.c : Built-in and dynamic modules management functions
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
#include "defs.h"

#include "config.h"

#include <stdlib.h>                                      /* free(), strtol() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                            /* strerror() */
#include <errno.h>                                                 /* ENOMEM */
#include <sys/types.h>                                               /* open */
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>                                                 /* close */

#if defined(HAVE_DLFCN_H)                                /* Linux, BSD, Hurd */
#include <dlfcn.h>                           /* dlopen(), dlsym(), dlclose() */

#elif defined(HAVE_IMAGE_H)                                          /* BeOS */
#include <image.h>

#else
/* FIXME: this isn't supposed to be an error */
#error no dynamic plugins available on your system !
#endif

#ifdef SYS_BEOS
#include "beos_specific.h"
#endif

#include "common.h"
#include "threads.h"

#include "intf_msg.h"
#include "modules.h"
#include "modules_core.h"

/* Local prototypes */
static int AllocateDynModule( module_bank_t * p_bank, char * psz_filename );
static int HideModule( module_t * p_module );
static int FreeModule( module_bank_t * p_bank, module_t * p_module );
static int CallSymbol( module_t * p_module, char * psz_name );

/*****************************************************************************
 * module_InitBank: create the module bank.
 *****************************************************************************
 * This function creates a module bank structure and fills it with the
 * built-in modules, as well as all the dynamic modules it can find.
 *****************************************************************************/
module_bank_t * module_InitBank( void )
{
    module_bank_t * p_bank;

    intf_ErrMsg( "FIXME: module_InitBank unimplemented" );
    p_bank = malloc( sizeof( module_bank_t ) );

    return( p_bank );
}

/*****************************************************************************
 * module_DestroyBank: destroy the module bank.
 *****************************************************************************
 * This function unloads all unused dynamic modules and removes the module
 * bank in case of success.
 *****************************************************************************/
int module_DestroyBank( module_bank_t * p_bank )
{
    intf_ErrMsg( "FIXME: module_DestroyBank unimplemented" );
    return( -1 );
}

/*****************************************************************************
 * module_ResetBank: reset the module bank.
 *****************************************************************************
 * This function resets the plugin bank by unloading all unused dynamic
 * modules.
 *****************************************************************************/
int module_ResetBank( module_bank_t * p_bank )
{
    intf_ErrMsg( "FIXME: module_ResetBank unimplemented" );
    return( -1 );
}

/*****************************************************************************
 * module_ManageBank: manage the module bank.
 *****************************************************************************
 * This function parses the module bank and hides modules that have been
 * unused for a while.
 *****************************************************************************/
void module_ManageBank( module_bank_t * p_bank )
{
    module_t * p_module;

    /* We take the global lock */
    vlc_mutex_lock( &p_bank->lock );

    /* Parse the module list to see if any modules need to be unloaded */
    for( p_module = p_bank->first ;
         p_module != NULL ;
         p_module = p_module->next )
    {
        /* If the module is unused and if it is a dynamic module... */
        if( p_module->i_usage == 0 && !p_module->b_builtin )
        {
            if( p_module->i_unused_delay < MODULE_HIDE_DELAY )
            {
                p_module->i_unused_delay++;
            }
            else
            {
                intf_Msg( "hiding unused module %s", p_module->psz_name );
                HideModule( p_module );
            }
        }
    }

    /* We release the global lock */
    vlc_mutex_lock( &p_bank->lock );

    return;
}

/*****************************************************************************
 * module_Need: increase the usage count of a module and load it if needed.
 *****************************************************************************
 * This function has to be called before a thread starts using a module. If
 * the module is already loaded, we just increase its usage count. If it isn't
 * loaded, we have to dynamically open it and initialize it.
 * If you successfully call module_Need() at any moment, be careful to call
 * module_Unneed() when you don't need it anymore.
 *****************************************************************************/
int module_Need( module_t * p_module )
{
    if( p_module->i_usage >= 0 )
    {
        /* This module is already loaded and activated, we can return */
        p_module->i_usage++;
        return( 0 );
    }

    if( p_module->b_builtin )
    {
        /* A built-in module should always have a refcount >= 0 ! */
        intf_ErrMsg( "module error: built-in module %s has refcount %i",
                     p_module->psz_name, p_module->i_usage );
        return( -1 );
    }

    if( p_module->i_usage != -1 )
    {
        /* This shouldn't happen. Ever. We have serious problems here. */
        intf_ErrMsg( "module error: dynamic module %s has refcount %i",
                     p_module->psz_name, p_module->i_usage );
        return( -1 );
    }

    /* i_usage == -1, which means that the module isn't in memory */
    if( ! module_load( p_module->psz_filename, &p_module->handle ) )
    {
        /* The dynamic module couldn't be opened */
        intf_ErrMsg( "module error: cannot open %s (%s)",
                     p_module->psz_filename, module_error() );
        return( -1 );
    }

    if( CallSymbol( p_module, "ActivateModule" ) != 0 )
    {
        /* We couldn't call ActivateModule() -- looks nasty, but
         * we can't do much about it. Just try to unload module. */
        module_unload( p_module->handle );
        p_module->i_usage = -1;
        return( -1 );
    }

    /* Everything worked fine ! The module is ready to be used */
    p_module->i_usage = 1;

    return( 0 );
}

/*****************************************************************************
 * module_Unneed: decrease the usage count of a module.
 *****************************************************************************
 * This function has to be called before a thread starts using a module. If
 * the module is already loaded, we just increase its usage count. If it isn't
 * loaded, we have to dynamically open it and initialize it.
 * If you successfully call module_Need() at any moment, be careful to call
 * module_Unneed() when you don't need it anymore.
 *****************************************************************************/
int module_Unneed( module_t * p_module )
{
    if( p_module->i_usage <= 0 )
    {
        /* This shouldn't happen. Ever. We have serious problems here. */
        intf_ErrMsg( "module error: trying to call module_Unneed() on %s"
                     " which isn't even in use", p_module->psz_name );
        return( -1 );
    }

    /* This module is still in use, we can return */
    p_module->i_usage--;
    p_module->i_unused_delay = 0;

    return( 0 );
}

/*****************************************************************************
 * Following functions are local.
 *****************************************************************************/

/*****************************************************************************
 * AllocateDynModule: load a module into memory and initialize it.
 *****************************************************************************
 * This function loads a dynamically loadable module and allocates a structure
 * for its information data. The module can then be handled by module_Need,
 * module_Unneed and HideModule. It can be removed by FreeModule.
 *****************************************************************************/
static int AllocateDynModule( module_bank_t * p_bank, char * psz_filename )
{
    module_t * p_module;
    module_handle_t handle;

    /* Try to dynamically load the module. */
    if( ! module_load( psz_filename, &handle ) )
    {
        /* The dynamic module couldn't be opened */
        intf_ErrMsg( "module error: cannot open %s (%s)",
                     psz_filename, module_error() );
        return( -1 );
    }

    /* Now that we have successfully loaded the module, we can
     * allocate a structure for it */ 
    p_module = malloc( sizeof( module_t ) );
    if( p_module == NULL )
    {
        intf_ErrMsg( "module error: can't allocate p_module" );
        module_unload( handle );
        return( -1 );
    }

    /* We need to fill these since they may be needed by CallSymbol() */
    p_module->psz_filename = psz_filename;
    p_module->handle = handle;

    if( CallSymbol( p_module, "InitModule" ) != 0 )
    {
        /* We couldn't call InitModule() */
        free( p_module );
        module_unload( handle );
        return( -1 );
    }

    if( CallSymbol( p_module, "ActivateModule" ) != 0 )
    {
        /* We couldn't call ActivateModule() */
        free( p_module );
        module_unload( handle );
        return( -1 );
    }

    /* We strdup() these entries so that they are still valid when the
     * module is unloaded. */
    p_module->psz_filename = strdup( p_module->psz_filename );
    p_module->psz_name = strdup( p_module->psz_name );
    p_module->psz_longname = strdup( p_module->psz_longname );
    p_module->psz_version = strdup( p_module->psz_version );
    if( p_module->psz_filename == NULL 
            || p_module->psz_name == NULL
            || p_module->psz_longname == NULL
            || p_module->psz_version == NULL )
    {
        intf_ErrMsg( "module error: can't duplicate strings" );
        free( p_module->psz_filename );
        free( p_module->psz_name );
        free( p_module->psz_longname );
        free( p_module->psz_version );
        free( p_module );
        module_unload( handle );
        return( -1 );
    }

    /* Everything worked fine ! The module is ready to be added to the list. */
    p_module->i_usage = 0;
    p_module->i_unused_delay = 0;

    p_module->b_builtin = 0;

    /* Link module across linked list */
    if( p_bank->first != NULL )
    {
        p_bank->first->prev = p_module;
    }
    p_module->next = p_bank->first;
    p_bank->first = p_module;

    return( 0 );
}

/*****************************************************************************
 * HideModule: remove a module from memory but keep its structure.
 *****************************************************************************
 * This function can only be called if i_usage == 0. It will make a call
 * to the module's inner DeactivateModule() symbol, and then unload it
 * from memory. A call to module_Need() will automagically load it again.
 *****************************************************************************/
static int HideModule( module_t * p_module )
{
    if( p_module->b_builtin )
    {
        /* A built-in module should never be hidden. */
        intf_ErrMsg( "module error: trying to hide built-in module %s",
                     p_module->psz_name );
        return( -1 );
    }

    if( p_module->i_usage >= 1 )
    {
        intf_ErrMsg( "module error: trying to hide module %s which is still"
                     " in use", p_module->psz_name );
        return( -1 );
    }

    if( p_module->i_usage <= -1 )
    {
        intf_ErrMsg( "module error: trying to hide module %s which is already"
                     " hidden", p_module->psz_name );
        return( -1 );
    }

    if( CallSymbol( p_module, "DeactivateModule" ) != 0 )
    {
        /* We couldn't call DeactivateModule() -- looks nasty, but
         * we can't do much about it. Just try to unload module anyway. */
        module_unload( p_module->handle );
        p_module->i_usage = -1;
        return( -1 );
    }

    /* Everything worked fine, we can safely unload the module. */
    module_unload( p_module->handle );
    p_module->i_usage = -1;

    return( 0 );
}

/*****************************************************************************
 * FreeModule: delete a module and its structure.
 *****************************************************************************
 * This function can only be called if i_usage <= 0.
 *****************************************************************************/
static int FreeModule( module_bank_t * p_bank, module_t * p_module )
{
    /* If the module is not in use but is still in memory, we first have
     * to hide it and remove it from memory before we can free the
     * data structure. */
    if( p_module->b_builtin )
    {
        if( p_module->i_usage != 0 )
        {
            intf_ErrMsg( "module error: trying to free builtin module %s with"
                         " usage %i", p_module->psz_name, p_module->i_usage );
            return( -1 );
        }
    }
    else
    {
        if( p_module->i_usage >= 1 )
        {
            intf_ErrMsg( "module error: trying to free module %s which is"
                         " still in use", p_module->psz_name );
            return( -1 );
        }

        /* Two possibilities here: i_usage == -1 and the module is already
         * unloaded, we can continue, or i_usage == 0, and we have to hide
         * the module before going on. */
        if( p_module->i_usage == 0 )
        {
            if( HideModule( p_module ) != 0 )
            {
                return( -1 );
            }
        }
    }

    /* Unlink the module from the linked list. */
    if( p_module == p_bank->first )
    {
        p_bank->first = p_module->next;
    }

    if( p_module->prev != NULL )
    {
        p_module->prev->next = p_module->next;
    }

    if( p_module->next != NULL )
    {
        p_module->next->prev = p_module->prev;
    }

    /* We free the structures that we strdup()ed in Allocate*Module(). */
    free( p_module->psz_filename );
    free( p_module->psz_name );
    free( p_module->psz_longname );
    free( p_module->psz_version );

    free( p_module );

    return( 0 );
}

/*****************************************************************************
 * CallSymbol: calls a module symbol.
 *****************************************************************************
 * This function calls a symbol given its name and a module structure. The
 * symbol MUST refer to a function returning int and taking a module_t* as
 * an argument.
 *****************************************************************************/
static int CallSymbol( module_t * p_module, char * psz_name )
{
    typedef int ( symbol_t ) ( module_t * p_module );
    symbol_t * p_symbol;

    /* Try to resolve the symbol */
    p_symbol = module_getsymbol( p_module->handle, psz_name );

    if( !p_symbol )
    {
        /* We couldn't load the symbol */
        intf_ErrMsg( "module error: cannot find %s in module %s (%s)",
                     psz_name, p_module->psz_filename, module_error() );
        return( -1 );
    }

    /* We can now try to call the symbol */
    if( p_symbol( p_module ) != 0 )
    {
        /* With a well-written module we shouldn't have to print an
         * additional error message here, but just make sure. */
        intf_ErrMsg( "module error: failed calling %s in module %s",
                     psz_name, p_module->psz_filename );
        return( -1 );
    }

    /* Everything worked fine, we can return */
    return( 0 );
}

