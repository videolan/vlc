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
 * Capabilities defined in the other files.
 *****************************************************************************/
void _M( intf_getfunctions )( function_list_t * p_function_list );

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
ADD_WINDOW( "Configuration for KDE module" )
    ADD_COMMENT( "Ha, ha -- nothing to configure yet" )
MODULE_CONFIG_STOP

MODULE_INIT_START
    p_module->i_capabilities = MODULE_CAPABILITY_NULL
                                | MODULE_CAPABILITY_INTF;
    p_module->psz_longname = "KDE interface module";
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    _M( intf_getfunctions )( &p_module->p_functions->intf );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

} /* extern "C" */
