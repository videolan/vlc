/*
 * Helper.h
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

#ifndef HELPER_H_
#define HELPER_H_

#include <string>
#include <list>

namespace adaptive
{
    class Helper
    {
        public:
            static std::string combinePaths     (const std::string &path1, const std::string &path2);
            static std::string getDirectoryPath (const std::string &path);
            static std::string getFileExtension (const std::string &uri);
            static bool        icaseEquals     (std::string str1, std::string str2);
            static bool        ifind            (std::string haystack, std::string needle);
            static std::list<std::string> tokenize(const std::string &, char);
    };
}

#endif /* HELPER_H_ */
