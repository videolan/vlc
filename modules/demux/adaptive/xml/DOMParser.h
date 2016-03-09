/*
 * DOMParser.h
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

#ifndef DOMPARSER_H_
#define DOMPARSER_H_

#include <vlc_common.h>
#include <vlc_stream.h>

#include "Node.h"

namespace adaptive
{
    namespace xml
    {
        class DOMParser
        {
            public:
                DOMParser           ();
                DOMParser           (stream_t *stream);
                virtual ~DOMParser  ();

                bool                parse       (bool);
                bool                reset       (stream_t *);
                Node*               getRootNode ();
                void                print       ();

            private:
                Node                *root;
                stream_t            *stream;

                xml_reader_t        *vlc_reader;

                Node*   processNode             (bool);
                void    addAttributesToNode     (Node *node);
                void    print                   (Node *node, int offset);
        };
    }
}

#endif /* DOMPARSER_H_ */
