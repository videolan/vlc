/*****************************************************************************
 * expr_evaluator.cpp
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "expr_evaluator.hpp"


void ExprEvaluator::parse( const string &rExpr )
{
    m_stack.clear();

    const char *pString = rExpr.c_str();
    list<string> opStack;   // operator stack
    string token;

    // Tokenize the expression
    int begin = 0, end = 0;
    while( pString[begin] )
    {
        // Find the next significant char in the string
        while( pString[begin] == ' ' )
        {
            begin++;
        }

        if( pString[begin] == '(' )
        {
            // '(' found: push it on the stack and continue
            opStack.push_back( "(" );
            begin++;
        }
        else if( pString[begin] == ')' )
        {
            // ')' found: pop the stack until a '(' is found
            while( !opStack.empty() )
            {
                // Pop the stack
                string lastOp = opStack.back();
                opStack.pop_back();
                if( lastOp == "(" )
                {
                    break;
                }
                // Push the operator on the RPN stack
                m_stack.push_back( lastOp );
            }
            begin++;
        }
        else
        {
            // Skip white spaces
            end = begin;
            while( pString[end] && pString[end] != ' ' && pString[end] != ')' )
            {
                end++;
            }
            // Get the next token
            token = rExpr.substr( begin, end - begin );
            begin = end;

            // TODO compare to a set of operators
            if( token == "not" || token == "or" || token == "and" )
            {
                // Pop the operator stack while the operator has a higher
                // precedence than the top of the stack
                while( !opStack.empty() &&
                       hasPrecedency( token, opStack.back() ) )
                {
                    // Pop the stack
                    string lastOp = opStack.back();
                    opStack.pop_back();
                    m_stack.push_back( lastOp );
                }
                opStack.push_back( token );
            }
            else
            {
                m_stack.push_back( token );
            }
        }
    }
    // Finish popping the operator stack
    while( !opStack.empty() )
    {
        string lastOp = opStack.back();
        opStack.pop_back();
        m_stack.push_back( lastOp );
    }
}


string ExprEvaluator::getToken()
{
    if( !m_stack.empty() )
    {
        string token = m_stack.front();
        m_stack.pop_front();
        return token;
    }
    return "";
}


bool ExprEvaluator::hasPrecedency( const string &op1, const string &op2 ) const
{
    // FIXME
    if( op1 == "(" )
    {
        return true;
    }
    if( op1 == "and" )
    {
        return (op2 == "or") || (op2 == "not" );
    }
    if( op1 == "or" )
    {
        return (op2 == "not" );
    }
    return false;
}

