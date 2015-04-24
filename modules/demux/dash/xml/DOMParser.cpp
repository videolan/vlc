/*
 * DOMParser.cpp
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "DOMParser.h"
#include "../adaptative/tools/Helper.h"

#include <vector>
#include <stack>
#include <vlc_xml.h>
#include <vlc_stream.h>

using namespace dash::xml;
using namespace dash::mpd;

DOMParser::DOMParser    (stream_t *stream) :
    root( NULL ),
    stream( stream ),
    vlc_xml( NULL ),
    vlc_reader( NULL )
{
}

DOMParser::~DOMParser   ()
{
    delete this->root;
    if(this->vlc_reader)
        xml_ReaderDelete(this->vlc_reader);
    if ( this->vlc_xml )
        xml_Delete( this->vlc_xml );
}

Node*   DOMParser::getRootNode              ()
{
    return this->root;
}
bool    DOMParser::parse                    ()
{
    this->vlc_xml = xml_Create(this->stream);

    if(!this->vlc_xml)
        return false;

    this->vlc_reader = xml_ReaderCreate(this->vlc_xml, this->stream);

    if(!this->vlc_reader)
        return false;

    root = processNode();
    if ( root == NULL )
        return false;

    return true;
}

Node* DOMParser::processNode()
{
    const char *data;
    int type;
    std::stack<Node *> lifo;

    while( (type = xml_ReaderNextNode(vlc_reader, &data)) > 0 )
    {
        switch(type)
        {
            case XML_READER_STARTELEM:
            {
                bool empty = xml_ReaderIsEmptyElement(vlc_reader);
                Node *node = new (std::nothrow) Node();
                if(node)
                {
                    if(!lifo.empty())
                        lifo.top()->addSubNode(node);
                    lifo.push(node);

                    node->setName(std::string(data));
                    addAttributesToNode(node);
                }

                if(empty)
                    lifo.pop();
                break;
            }

            case XML_READER_TEXT:
            {
                if(!lifo.empty())
                    lifo.top()->setText(std::string(data));
                break;
            }

            case XML_READER_ENDELEM:
            {
                if(lifo.empty())
                    return NULL;

                Node *node = lifo.top();
                lifo.pop();
                if(lifo.empty())
                    return node;
            }

            default:
                break;
        }
    }

    while( lifo.size() > 1 )
        lifo.pop();

    if(!lifo.empty())
        delete lifo.top();

    return NULL;
}

void    DOMParser::addAttributesToNode      (Node *node)
{
    const char *attrValue;
    const char *attrName;

    while((attrName = xml_ReaderNextAttr(this->vlc_reader, &attrValue)) != NULL)
    {
        std::string key     = attrName;
        std::string value   = attrValue;
        node->addAttribute(key, value);
    }
}
void    DOMParser::print                    (Node *node, int offset)
{
    for(int i = 0; i < offset; i++)
        msg_Dbg(this->stream, " ");

    msg_Dbg(this->stream, "%s", node->getName().c_str());

    std::vector<std::string> keys = node->getAttributeKeys();

    for(size_t i = 0; i < keys.size(); i++)
        msg_Dbg(this->stream, " %s=%s", keys.at(i).c_str(), node->getAttributeValue(keys.at(i)).c_str());

    msg_Dbg(this->stream, "\n");

    offset++;

    for(size_t i = 0; i < node->getSubNodes().size(); i++)
    {
        this->print(node->getSubNodes().at(i), offset);
    }
}
void    DOMParser::print                    ()
{
    this->print(this->root, 0);
}
bool    DOMParser::isDash                   (stream_t *stream)
{
    const std::string namespaces[] = {
        "xmlns=\"urn:mpeg:mpegB:schema:DASH:MPD:DIS2011\"",
        "xmlns=\"urn:mpeg:schema:dash:mpd:2011\"",
        "xmlns=\"urn:mpeg:DASH:schema:MPD:2011\"",
        "xmlns='urn:mpeg:mpegB:schema:DASH:MPD:DIS2011'",
        "xmlns='urn:mpeg:schema:dash:mpd:2011'",
        "xmlns='urn:mpeg:DASH:schema:MPD:2011'",
    };

    const uint8_t *peek;
    int peek_size = stream_Peek(stream, &peek, 1024);
    if (peek_size < (int)namespaces[0].length())
        return false;

    std::string header((const char*)peek, peek_size);
    for( size_t i=0; i<ARRAY_SIZE(namespaces); i++ )
    {
        if ( adaptative::Helper::ifind(header, namespaces[i]) )
            return true;
    }
    return false;
}

Profile DOMParser::getProfile() const
{
    Profile res(Profile::Unknown);
    if(this->root == NULL)
        return res;

    std::string urn = this->root->getAttributeValue("profiles");
    if ( urn.length() == 0 )
        urn = this->root->getAttributeValue("profile"); //The standard spells it the both ways...


    size_t pos;
    size_t nextpos = -1;
    do
    {
        pos = nextpos + 1;
        nextpos = urn.find_first_of(",", pos);
        res = Profile(urn.substr(pos, nextpos - pos));
    }
    while (nextpos != std::string::npos && res == Profile::Unknown);

    return res;
}
