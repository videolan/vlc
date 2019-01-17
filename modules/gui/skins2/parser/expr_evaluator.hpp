/*****************************************************************************
 * expr_evaluator.hpp
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef EXPR_EVALUATOR_HPP
#define EXPR_EVALUATOR_HPP

#include "../src/skin_common.hpp"
#include <list>
#include <string>

/// Expression evaluator using Reverse Polish Notation
class ExprEvaluator: public SkinObject
{
public:
    ExprEvaluator( intf_thread_t *pIntf ): SkinObject( pIntf ) { }
    ~ExprEvaluator() { }

    /// Clear the RPN stack and parse an expression
    void parse( const std::string &rExpr );

    /// Pop the first token from the RPN stack.
    /// Return NULL when the stack is empty.
    std::string getToken();

private:
    /// RPN stack
    std::list<std::string> m_stack;

    /// Returns true if op1 has precedency over op2
    bool hasPrecedency( const std::string &op1, const std::string &op2 ) const;
};

#endif
