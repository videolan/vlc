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

#include <vector>

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

    this->root = this->processNode();
    if ( this->root == NULL )
        return false;

    return true;
}
Node*   DOMParser::processNode              ()
{
    const char *data;
    int type = xml_ReaderNextNode(this->vlc_reader, &data);
    if(type != -1 && type != XML_READER_NONE && type != XML_READER_ENDELEM)
    {
        Node *node = new Node();
        node->setType( type );

        if ( type != XML_READER_TEXT )
        {
            std::string name    = data;
            bool        isEmpty = xml_ReaderIsEmptyElement(this->vlc_reader);
            node->setName(name);

            this->addAttributesToNode(node);

            if(isEmpty)
                return node;

            Node *subnode = NULL;

            while((subnode = this->processNode()) != NULL)
                node->addSubNode(subnode);
        }
        else
            node->setText( data );
        return node;
    }
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
    const char* psz_namespaceDIS = "urn:mpeg:mpegB:schema:DASH:MPD:DIS2011";
    const char* psz_namespaceIS  = "urn:mpeg:DASH:schema:MPD:2011";

    const uint8_t *peek;
    int peek_size = stream_Peek(stream, &peek, 1024);
    if (peek_size < (int)strlen(psz_namespaceDIS))
        return false;

    std::string header((const char*)peek, peek_size);
    return (header.find(psz_namespaceDIS) != std::string::npos) || (header.find(psz_namespaceIS) != std::string::npos);
}
Profile DOMParser::getProfile               ()
{
    if(this->root == NULL)
        return dash::mpd::UnknownProfile;

    std::string profile = this->root->getAttributeValue("profiles");
    if ( profile.length() == 0 )
        profile = this->root->getAttributeValue("profile"); //The standard spells it the both ways...

    if(profile.find("urn:mpeg:mpegB:profile:dash:isoff-basic-on-demand:cm") != std::string::npos ||
            profile.find("urn:mpeg:dash:profile:isoff-ondemand:2011") != std::string::npos ||
            profile.find("urn:mpeg:dash:profile:isoff-on-demand:2011") != std::string::npos)
        return dash::mpd::BasicCM;

    if(profile.find("urn:mpeg:dash:profile:isoff-main:2011") != std::string::npos)
        return dash::mpd::IsoffMain;

    return dash::mpd::UnknownProfile;
}
