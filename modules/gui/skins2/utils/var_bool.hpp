/*****************************************************************************
 * var_bool.hpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VAR_BOOL_HPP
#define VAR_BOOL_HPP

#include "variable.hpp"
#include "observer.hpp"


/// Interface for read-only boolean variable
class VarBool: public Variable, public Subject<VarBool>
{
public:
    /// Get the variable type
    virtual const std::string &getType() const { return m_type; }

    /// Get the boolean value
    virtual bool get() const = 0;

protected:
    VarBool( intf_thread_t *pIntf ): Variable( pIntf ) { }
    virtual ~VarBool() { }

private:
    /// Variable type
    static const std::string m_type;
};


/// Constant true VarBool
class VarBoolTrue: public VarBool
{
public:
    VarBoolTrue( intf_thread_t *pIntf ): VarBool( pIntf ) { }
    virtual ~VarBoolTrue() { }
    virtual bool get() const { return true; }
};


/// Constant false VarBool
class VarBoolFalse: public VarBool
{
public:
    VarBoolFalse( intf_thread_t *pIntf ): VarBool( pIntf ) { }
    virtual ~VarBoolFalse() { }
    virtual bool get() const { return false; }
};


/// Boolean variable implementation (read/write)
class VarBoolImpl: public VarBool
{
public:
    VarBoolImpl( intf_thread_t *pIntf );
    virtual ~VarBoolImpl() { }

    // Get the boolean value
    virtual bool get() const { return m_value; }

    /// Set the internal value
    virtual void set( bool value );

private:
    /// Boolean value
    bool m_value;
};


/// Conjunction of two boolean variables (AND)
class VarBoolAndBool: public VarBool, public Observer<VarBool>
{
public:
    VarBoolAndBool( intf_thread_t *pIntf, VarBool &rVar1, VarBool &rVar2 );
    virtual ~VarBoolAndBool();
    virtual bool get() const { return m_value; }

    // Called when one of the observed variables is changed
    void onUpdate( Subject<VarBool> &rVariable, void* );

private:
    /// Boolean variables
    VarBool &m_rVar1, &m_rVar2;
    bool m_value;
};


/// Disjunction of two boolean variables (OR)
class VarBoolOrBool: public VarBool, public Observer<VarBool>
{
public:
    VarBoolOrBool( intf_thread_t *pIntf, VarBool &rVar1, VarBool &rVar2 );
    virtual ~VarBoolOrBool();
    virtual bool get() const { return m_value; }

    // Called when one of the observed variables is changed
    void onUpdate( Subject<VarBool> &rVariable, void* );

private:
    /// Boolean variables
    VarBool &m_rVar1, &m_rVar2;
    bool m_value;
};


/// Negation of a boolean variable (NOT)
class VarNotBool: public VarBool, public Observer<VarBool>
{
public:
    VarNotBool( intf_thread_t *pIntf, VarBool &rVar );
    virtual ~VarNotBool();
    virtual bool get() const { return !m_rVar.get(); }

    // Called when the observed variable is changed
    void onUpdate( Subject<VarBool> &rVariable, void* );

private:
    /// Boolean variable
    VarBool &m_rVar;
};


#endif
