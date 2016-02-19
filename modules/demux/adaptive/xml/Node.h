/*
 * Node.h
 *****************************************************************************
 * Copyright (C) 2010 - 2011 Klagenfurt University
 *
 * Created on: Aug 10, 2010
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

#ifndef NODE_H_
#define NODE_H_

#include <vector>
#include <string>
#include <map>

namespace adaptive
{
    namespace xml
    {
        class Node
        {
            public:
                Node            ();
                virtual ~Node   ();

                const std::vector<Node *>&          getSubNodes         () const;
                void                                addSubNode          (Node *node);
                const std::string&                  getName             () const;
                void                                setName             (const std::string& name);
                bool                                hasAttribute        (const std::string& name) const;
                void                                addAttribute        (const std::string& key, const std::string& value);
                const std::string&                  getAttributeValue   (const std::string& key) const;
                std::vector<std::string>            getAttributeKeys    () const;
                const std::string&                  getText             () const;
                void                                setText( const std::string &text );
                const std::map<std::string, std::string>& getAttributes () const;
                int                                 getType() const;
                void                                setType( int type );
                std::vector<std::string>            toString(int) const;

            private:
                static const std::string            EmptyString;
                std::vector<Node *>                 subNodes;
                std::map<std::string, std::string>  attributes;
                std::string                         name;
                std::string                         text;
                int                                 type;

        };
    }
}

#endif /* NODE_H_ */
