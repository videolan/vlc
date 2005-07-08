/*****************************************************************************
 * logger.hpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN (Centrale RÃ©seaux) and its contributors
 * $Id$
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
 *          Olivier Teulière <ipkiss@via.ecp.fr>
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

#ifndef LOGGER_HPP
#define LOGGER_HPP

#include "skin_common.hpp"
#include <string>


// helper macros
#define MSG_ERR( msg ) Logger::instance( getIntf() )->error( msg )
#define MSG_WARN( msg ) Logger::instance( getIntf() )->warn( msg )


// Logger class
class Logger: public SkinObject
{
    public:
        /// Get the instance of Logger
        /// Returns NULL if initialization failed
        static Logger *instance( intf_thread_t *pIntf );

        /// Delete the instance of Logger
        static void destroy( intf_thread_t *pIntf );

        /// Print an error message
        void error( const string &rMsg );

        /// Print a warning
        void warn( const string &rMsg );

    private:
        // Private because it's a singleton
        Logger( intf_thread_t *pIntf );
        ~Logger();
};


#endif
