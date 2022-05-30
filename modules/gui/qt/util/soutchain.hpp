/*****************************************************************************
 * soutchain.hpp: A class to generate Stream Output Chains
 ****************************************************************************
 * Copyright (C) 2019 Jérôme Froissart <software@froiss.art>
 *
 * Authors: Jérôme Froissart <software@froiss.art>
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

#ifndef VLC_QT_SOUTCHAIN_HPP_
#define VLC_QT_SOUTCHAIN_HPP_

#include "qt.hpp"

#include <QPair>
#include <QList>

class SoutOption;

class SoutModule
{
public:
    SoutModule( const QString& name ) :
        moduleName( name )
    {
    }

    void option( const QString& name, const SoutOption& value );
    void option( const QString& name, const QString& value, bool escaped );
    void option( const QString& name );
    QString to_string() const;

private:
    typedef QPair<QString, SoutOption> OptionPairType;
    typedef QList<OptionPairType> OptionsType;
    QString moduleName;
    OptionsType options;
};


class SoutOption
{
public:
    SoutOption( const QString& value, bool escaped = false ) :
        kind( escaped ? EscapedString : String ),
        stringValue( value ),
        nestedModule("")
    {}

    SoutOption( const char* s, bool escaped = false ) :
        SoutOption( QString(s), escaped )
    {}

    SoutOption( const SoutModule& module ) :
        kind(Nested),
        nestedModule(module)
    {}

    QString to_string() const;

private:
    enum Kind{ String, EscapedString, Nested };
    Kind kind;
    QString stringValue;
    SoutModule nestedModule;
};


/// This class helps building MRLs
///
/// An MRL has the following structure:
///  * a header
///  * any number of modules, which have
///     - a name
///     - any number of key(=value) pairs
///       values can be nested modules
///
/// Example of MRL: HEADERmodule1{a,b=val}:module2:module3{opt,arg=\"value with automatically escaped quotes\",stuff=nestedModule{subkey=subvalue}}
class SoutChain
{
public:
    SoutChain( const QString& header="" ) :
        hdr(header)
    {
    }

    void clear()
    {
        hdr = "";
        modules.clear();
    }

    void header( const QString& newHeader )
    {
        hdr = newHeader;
    }

    SoutModule& begin( const QString& module )
    {
        modules.append( SoutModule( module ) );
        return modules.back();
    }

    // Useless, kept for compatibility with an older API
    void end()
    {
    }

    void module( const SoutModule& module )
    {
        modules.append( module );
    }

    // These should be only in SoutModule, but they are kept in this parent class for compatibility with an older API
    void option( const QString& name, const QString& value = "", bool escaped = false );
    void option( const QString& name, const int i_value, const int i_precision = 10 );
    void option( const QString& name, const double f_value );
    void option( const QString& name, const QString& base, const int i_value, const int i_precision = 10 );
    void option( const QString& name, const SoutModule& nested );

    QString getHeader() const;
    QString to_string() const;

private:
    QString hdr;
    QList<SoutModule> modules;
};

#endif // include guard
