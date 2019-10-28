/*****************************************************************************
 * soutchain.cpp: A class to generate Stream Output Chains
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

#include "soutchain.hpp"

QString SoutModule::to_string() const
{
    QString s = moduleName;

    if( options.size() > 0 )
    {
        s += "{";
    }
    OptionsType::const_iterator it;
    for( it=options.begin(); it!=options.end(); )
    {
        s += it->first;
        if( it->second.to_string().compare("") != 0 )
        {
            char *psz = config_StringEscape( qtu(it->second.to_string()) );
            if( psz )
            {
                s += "=" + qfu( psz );
                free( psz );
            }
        }
        ++it;
        if( it != options.end() )
        {
            s += ",";
        }
    }
    if( options.size() > 0 )
    {
        s += "}";
    }
    return s;
}

void SoutModule::option( const QString& option, const SoutOption& value )
{
    options.append( OptionPairType( option, value ) );
}
void SoutModule::option( const QString& option )
{
    options.append( OptionPairType( option, "" ) );
}

QString SoutChain::getHeader() const
{
    return hdr;
}

QString SoutChain::to_string() const
{
    QString chain = hdr;
    for( int m=0; m<modules.size(); m++ )
    {
        chain += modules[m].to_string();
        if( m < modules.size() - 1 )
        {
            chain += ":";
        }
    }

    return chain;
}

void SoutChain::option( const QString& name, const QString& value )
{
    if( modules.size() > 0 )
    {
        modules.back().option( name, value );
    }
}
void SoutChain::option( const QString& name, const int i_value, const int i_precision )
{
    option( name, QString::number( i_value, i_precision ) );
}
void SoutChain::option( const QString& name, const double f_value )
{
    option( name, QString::number( f_value ) );
}
void SoutChain::option( const QString& name, const QString& base, const int i_value, const int i_precision )
{
    option( name, base + ":" + QString::number( i_value, i_precision ) );
}
void SoutChain::option( const QString& name, const SoutModule& nested )
{
    if( modules.size() > 0 )
    {
        modules.back().option( name, nested );
    }
}