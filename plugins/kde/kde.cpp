/***************************************************************************
                          kde.cpp  -  description
                             -------------------
    begin                : Sun Mar 25 2001
    copyright            : (C) 2001 by andres
    email                : dae@chez.com
 ***************************************************************************/
#define MODULE_NAME kde
#include "intf_plugin.h"

extern "C"
{

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
ADD_WINDOW( "Configuration for KDE module" )
    ADD_COMMENT( "Ha, ha -- nothing to configure yet" )
MODULE_CONFIG_END

/*****************************************************************************
 * Capabilities defined in the other files.
 *****************************************************************************/

void _M( intf_getfunctions )( function_list_t * p_function_list );

/*****************************************************************************
 * InitModule: get the module structure and configuration.
 *****************************************************************************
 * We have to fill psz_name, psz_longname and psz_version. These variables
 * will be strdup()ed later by the main application because the module can
 * be unloaded later to save memory, and we want to be able to access this
 * data even after the module has been unloaded.
 *****************************************************************************/
MODULE_INIT
{
    p_module->psz_name = MODULE_STRING;
    p_module->psz_longname = "KDE interface module";
    p_module->psz_version = VERSION;

    p_module->i_capabilities = MODULE_CAPABILITY_NULL
                                | MODULE_CAPABILITY_INTF;

    return( 0 );
}

/*****************************************************************************
 * ActivateModule: set the module to an usable state.
 *****************************************************************************
 * This function fills the capability functions and the configuration
 * structure. Once ActivateModule() has been called, the i_usage can
 * be set to 0 and calls to NeedModule() be made to increment it. To unload
 * the module, one has to wait until i_usage == 0 and call DeactivateModule().
 *****************************************************************************/
MODULE_ACTIVATE
{
    p_module->p_functions =
                ( module_functions_t * )malloc( sizeof( module_functions_t ) );
    if( p_module->p_functions == NULL )
    {
        return( -1 );
    }

    _M( intf_getfunctions )( &p_module->p_functions->intf );

    p_module->p_config = p_config;

    return( 0 );
}

/*****************************************************************************
 * DeactivateModule: make sure the module can be unloaded.
 *****************************************************************************
 * This function must only be called when i_usage == 0. If it successfully
 * returns, i_usage can be set to -1 and the module unloaded. Be careful to
 * lock usage_lock during the whole process.
 *****************************************************************************/
MODULE_DEACTIVATE
{
    free( p_module->p_functions );

    return( 0 );
}

} /* extern "C" */
