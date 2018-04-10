/*
 * Helper.cpp
 *****************************************************************************
 * Copyright (C) 2010 - 2012 Klagenfurt University
 *
 * Created on: Feb 20, 2012
 * Authors: Christopher Mueller <christopher.mueller@itec.uni-klu.ac.at>
 *          Christian Timmerer  <christian.timmerer@itec.uni-klu.ac.at>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "Helper.h"
#include <algorithm>
using namespace adaptive;

std::string Helper::combinePaths        (const std::string &path1, const std::string &path2)
{
    if ( path2.length() == 0 )
        return path1;
    else if ( path1.length() == 0 )
        return path2;

    char path1Last  = path1.at(path1.size() - 1);
    char path2First = path2.at(0);

    if(path1Last == '/' && path2First == '/')
        return path1 + path2.substr(1, path2.size());

    if(path1Last != '/' && path2First != '/')
        return path1 + "/" + path2;

    return path1 + path2;
}
std::string Helper::getDirectoryPath    (const std::string &path)
{
    std::size_t pos = path.find_last_of('/');

    return (pos != std::string::npos) ? path.substr(0, pos) : path;
}

std::string Helper::getFileExtension (const std::string &uri)
{
    std::string extension;
    std::size_t pos = uri.find_first_of("?#");
    if(pos != std::string::npos)
        extension = uri.substr(0, pos);
    else
        extension = uri;
    pos = extension.find_last_of('.');
    if(pos == std::string::npos || extension.length() - pos < 2)
        return std::string();
    return extension.substr(pos + 1);
}

bool Helper::icaseEquals(std::string str1, std::string str2)
{
    if(str1.size() != str2.size())
        return false;

    std::transform(str1.begin(), str1.end(), str1.begin(), toupper);
    std::transform(str2.begin(), str2.end(), str2.begin(), toupper);
    return str1 == str2;
}

bool Helper::ifind(std::string haystack, std::string needle)
{
    transform(haystack.begin(), haystack.end(), haystack.begin(), toupper);
    transform(needle.begin(), needle.end(), needle.begin(), toupper);
    return haystack.find(needle) != std::string::npos;
}

std::list<std::string> Helper::tokenize(const std::string &str, char c)
{
    std::list<std::string> ret;
    std::size_t prev = 0;
    std::size_t cur = str.find_first_of(c, 0);
    while(cur != std::string::npos)
    {
        ret.push_back(str.substr(prev, cur - prev));
        prev = cur + 1;
        cur = str.find_first_of(c, cur + 1);
    }

    ret.push_back(str.substr(prev));
    return ret;
}
