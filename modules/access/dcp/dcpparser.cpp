/*****************************************************************************
 * Copyright (C) 2013 VLC authors and VideoLAN
 *
 * Authors:
 *          Nicolas Bertrand <nico@isf.cc>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Guillaume Gonnaud
 *          Valentin Vetter <vvetter@outlook.com>
 *          Anthony Giniers
 *          Ludovic Hoareau
 *          Loukmane Dessai
 *          Simona-Marinela Prodea <simona dot marinela dot prodea at gmail dot com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
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

/**
 * @file dcpparser.cpp
 * @brief Parsing of DCP XML files
 */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* VLC core API headers */
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_xml.h>
#include <vlc_url.h>

#include <iostream>
#include <string>
#include <list>
#include <vector>

#include "dcpparser.h"

using namespace std;

typedef enum {
    CHUNK_UNKNOWN = 0,
    CHUNK_PATH,
    CHUNK_VOL_INDEX,
    CHUNK_OFFSET,
    CHUNK_LENGTH
} ChunkTag_t;


typedef enum {
    ASSET_UNKNOWN = 0,
    ASSET_ID,
    ASSET_ANNOTATION_TEXT,
    ASSET_PACKING_LIST,
    ASSET_CHUNK_LIST,
    ASSET_HASH,
    ASSET_SIZE,
    ASSET_TYPE,
    ASSET_ORIGINAL_FILENAME
} AssetTag_t;

static const string g_asset_names[] = {
    "Id",
    "AnnotationText",
    "PackingList",
    "ChunkList",
    "Hash",
    "Size",
    "Type",
    "OriginalFileName"
};


typedef enum {
    PKL_UNKNOWN = 0,
    PKL_ID,
    PKL_ISSUE_DATE,
    PKL_ISSUER,
    PKL_CREATOR,
    PKL_ASSET_LIST,
    PKL_ANNOTATION_TEXT, /* start of optional tags */
    PKL_ICON_ID,
    PKL_GROUP_ID,
    PKL_SIGNER,
    PKL_SIGNATURE,
} PKLTag_t;

typedef enum {
    CPL_UNKNOWN = 0,
    CPL_ID,
    CPL_ANNOTATION_TEXT,        /* optional */
    CPL_ICON_ID,                /* optional */
    CPL_ISSUE_DATE,
    CPL_ISSUER,                 /* optional */
    CPL_CREATOR,                /* optional */
    CPL_CONTENT_TITLE,
    CPL_CONTENT_KIND,
    CPL_CONTENT_VERSION,        /* not optional, but not always present*/
    CPL_RATING_LIST,            /* not handled */
    CPL_REEL_LIST,
    CPL_SIGNER,                 /* optional - not handled */
    CPL_SIGNATURE               /* optional - not handled */
} CPLTag_t;


class ChunkList: public std::list<Chunk> {
public :
    ChunkList();
    ~ChunkList();
};

/*
 * Chunk Class
 */
int Chunk::Parse( xml_reader_t *p_xmlReader, string p_node, int p_type){
    string node;
    int type;
    string s_value;
    static const string names[] = {"Path", "VolumeIndex", "Offset",
                               "Length"};
    if (p_type != XML_READER_STARTELEM)
        return -1;
    if( p_node != "Chunk")
        return -1;
    /* loop on Chunks Node */
    while( ( type = XmlFile::ReadNextNode( this->p_demux, p_xmlReader, node ) ) > 0 ) {
        switch (type) {
            case XML_READER_STARTELEM:
            {
                ChunkTag_t chunk_tag = CHUNK_UNKNOWN;
                for(ChunkTag_t i = CHUNK_PATH; i <= CHUNK_LENGTH; i = ChunkTag_t(i+1)) {
                    if( node == names[i-1]) {
                        chunk_tag = i;
                        if ( XmlFile::ReadEndNode( this->p_demux, p_xmlReader, node, type, s_value ) )
                            return -1;
                        switch (chunk_tag) {
                            case CHUNK_PATH:
                                this->s_path = s_value;
                                break;
                            case CHUNK_VOL_INDEX:
                                this->i_vol_index = atoi(s_value.c_str());
                                break;
                            case CHUNK_OFFSET:
                                this->i_offset = atoi(s_value.c_str());
                                break;
                            case CHUNK_LENGTH:
                                this->i_length = atoi(s_value.c_str());
                                break;
                            case CHUNK_UNKNOWN:
                            default:
                                break;
                        }
                        /* break the for loop as a tag is found*/
                        break;
                    }
                }
                if(chunk_tag == CHUNK_UNKNOWN)
                    return -1;
                break;
            }
            case XML_READER_TEXT:
                s_value = node;
                if (unlikely(node.empty()))
                    return -1;
                break;
            case XML_READER_ENDELEM:
                /* Verify if we reach Chuk endelem  */
                if ( node == p_node) {
                    /* verify path */
                    if ( this->s_path.empty() ) {
                        msg_Err(this->p_demux, "Chunk::Parse No path found");
                        return -1;
                    }
                    if ( this->i_vol_index != 1 ) {
                        msg_Err(this->p_demux, "Only one VOLINDEX supported. Patch welcome.");
                        return -1;
                    }
                    /* end of chunk tag parse */
                    return 0;
                }
                break;
        }
    }
    return -1;
}
/*
 * AssetMap Class
 */

AssetMap::~AssetMap() { }

int AssetMap::Parse ( )
{
    int type = 0;
    int retval;
    int reel_nbr = 0;
    int index = 0;
    int sum_duration_vid = 0;
    int sum_duration_aud = 0;
    string node;
    char *psz_kdm_path;

    CPL  *cpl;
    Reel *reel;
    PKL  *pkl;
    AssetList *_p_asset_list = NULL;

    vector<string> pklfiles;

    /* init XML parser */
    if( this->OpenXml() ) {
        msg_Err( p_demux, "Failed to initialize Assetmap XML parser" );
        return -1;
    }

    /* reading ASSETMAP file to get the asset_list */
    msg_Dbg( p_demux, "reading ASSETMAP file..." );
    while( ( type = XmlFile::ReadNextNode( this->p_demux, this->p_xmlReader, node ) ) ) {
        if( type == -1 )
        {
            this->CloseXml();
            return -1;
        }
        if ( (type == XML_READER_STARTELEM) && ( node =="AssetList")) {
            _p_asset_list =  new (nothrow) AssetList();
            if ( unlikely(_p_asset_list == NULL) ) {
                this->CloseXml();
                return -1;
            }
            p_dcp->p_asset_list = _p_asset_list;
            if (this->ParseAssetList(p_xmlReader, node, type )) {
                this->CloseXml();
                return -1;
            }
            /* asset list found so break*/
            break;
        }
    }

    /* Look for PKLs path */
    if ( (_p_asset_list == NULL) ||  (_p_asset_list->size() == 0) ) {
        msg_Err( p_demux, "Asset list empty" );
        this->CloseXml();
        return -1;
    }

    for (AssetList::iterator iter = _p_asset_list->begin();
            iter != _p_asset_list->end() ; ++iter) {
        string s_filepath;
        s_filepath = (*iter)->getPath();
        if (s_filepath.empty()) {
            msg_Err( p_demux, "No path element for asset" );
            continue;
        }
        /* set an absolute file path */
        s_filepath = p_dcp->path + s_filepath;

        /* case of packing list */
        if ((*iter)->isPackingList()) {
            pklfiles.push_back( s_filepath );
        }
    }

    /* TODO: case of only on PKL managed.
     * Future work needed for managing severals
     * the PKL vector will be used to select the required PKL  */
    if( (pklfiles.size() == 0)  || (pklfiles[0].empty()) )
    {
        msg_Err( p_demux, "Could not find PKL file in ASSETMAP" );
        this->CloseXml();
        return -1;
    }

    /* Create the first PKL */
    pkl = new (nothrow) PKL(p_demux, pklfiles[0], _p_asset_list, p_dcp->path);
    if ( unlikely(pkl == NULL) ) {
        this->CloseXml();
        return -1;
        }
    if (pkl->Parse()) {
        delete pkl;
        this->CloseXml();
        return -1;
        }
    p_dcp->pkls.push_back( pkl );

    /* Now, CPL */
    if ( pkl->FindCPLs() <= 0 ) {
        msg_Err(p_demux, " No CPL found");
        this->CloseXml();
        return -1;
    }
    /* TODO: Only one CPL managed.
     * Future work needed for managing severals
     */

    cpl = pkl->getCPL(0);
    if( cpl == NULL ) {
        msg_Err(p_demux, " No CPL found");
        this->CloseXml();
        return -1;
    }
    if ( cpl->Parse() ) {
        this->CloseXml();
        return -1;
    }

    /* KDM, if needed */
    for( AssetList::iterator iter = _p_asset_list->begin(); iter != _p_asset_list->end(); ++iter )
        if( ! (*iter)->getKeyId().empty() )
        {
            msg_Dbg( p_demux, "DCP is encrypted, searching KDM file...");
            psz_kdm_path = var_InheritString( p_demux, "kdm" );
            if( !psz_kdm_path || !*psz_kdm_path )
            {
                msg_Err( p_demux, "cryptographic key IDs found in CPL and no path to KDM given");
                free( psz_kdm_path );
                this->CloseXml();
                return VLC_EGENERIC;
            }
            KDM p_kdm( p_demux, psz_kdm_path, p_dcp );
            free( psz_kdm_path );
            if( ( retval = p_kdm.Parse() ) )
            {
                this->CloseXml();
                return retval;
            }
            break;
        }

    reel_nbr = cpl->getReelList().size();
    for(index = 0; index != reel_nbr; ++index)
    {
        reel = cpl->getReel(index);

        Asset *asset;
        struct info_reel video;
        struct info_reel audio;

        /* Get picture */
        asset = reel->getTrack(TRACK_PICTURE);
        if (asset != NULL)
        {
            sum_duration_vid += asset->getDuration();
            video.filename = p_dcp->path + asset->getPath();
            video.i_entrypoint = asset->getEntryPoint();
            video.i_duration = asset->getDuration();
            video.i_correction = video.i_entrypoint - sum_duration_vid + video.i_duration;
            video.i_absolute_end = sum_duration_vid;
            video.p_key = asset->getAESKeyById( p_dcp->p_key_list, asset->getKeyId() );
            p_dcp->video_reels.push_back(video);
            msg_Dbg( this->p_demux, "Video Track: %s",asset->getPath().c_str());
            msg_Dbg( this->p_demux, "Entry point: %i",asset->getEntryPoint());
        }
        /* Get audio */
        asset = reel->getTrack(TRACK_SOUND);
        if (asset != NULL)
        {
            /*if (!p_dcp->audio_reels.empty())
            {
                sum_duration_aud = 0;
                for (int i = 0; i != p_dcp->audio_reels.size(); ++i)
                {
                    sum_duration_aud += p_dcp->audio_reels(i).i_duration;
                }
            }*/
            sum_duration_aud += asset->getDuration();
            audio.filename = p_dcp->path + asset->getPath();
            audio.i_entrypoint = asset->getEntryPoint();
            audio.i_duration = asset->getDuration();
            audio.i_correction = audio.i_entrypoint - sum_duration_aud + audio.i_duration;
            audio.i_absolute_end = sum_duration_aud;
            audio.p_key = asset->getAESKeyById( p_dcp->p_key_list, asset->getKeyId() );
            p_dcp->audio_reels.push_back(audio);
            msg_Dbg( this->p_demux, "Audio Track: %s",asset->getPath().c_str());
            msg_Dbg( this->p_demux, "Entry point: %i",asset->getEntryPoint());
        }
    }
    /* free memory */
    this->CloseXml();
    return VLC_SUCCESS;
}



/*
 * Asset Class
 */
Asset::~Asset() {}

int Asset::Parse( xml_reader_t *p_xmlReader, string p_node, int p_type)
{
    string node;
    int type;
    string s_value;
    const string s_root_node = "Asset";

    if (p_type != XML_READER_STARTELEM)
        return -1;
    if( p_node != s_root_node)
        return -1;
    /* loop on Assets Node */
    while( ( type = XmlFile::ReadNextNode( this->p_demux, p_xmlReader, node ) ) > 0 ) {
        switch (type) {
            case XML_READER_STARTELEM:
                {
                    AssetTag_t _tag = ASSET_UNKNOWN;
                    for(AssetTag_t i = ASSET_ID; i <= ASSET_ORIGINAL_FILENAME; i = AssetTag_t(i+1)) {
                        if( node == g_asset_names[i-1]) {
                            _tag = i;
                            switch(_tag) {
                                /* Case of complex nodes */
                                case ASSET_PACKING_LIST:
                                    /* case of <PackingList/> tag, bur not compliant with SMPTE-429-9 2007*/
                                    if (xml_ReaderIsEmptyElement( p_xmlReader))
                                    {
                                        this->b_is_packing_list = true;
                                    }
                                    else if ( XmlFile::ReadEndNode( this->p_demux, p_xmlReader, node, type, s_value ) )
                                    {
                                        msg_Err(this->p_demux, "Missing end node in %s", node.c_str());
                                        return -1;
                                    }
                                    if ( s_value == "true" )
                                        this->b_is_packing_list = true;
                                    break;
                                case ASSET_CHUNK_LIST:
                                    if ( this->parseChunkList(p_xmlReader, node, type ) )
                                    {
                                        msg_Err(this->p_demux, "Error parsing chunk list: %s", node.c_str());
                                        return -1;
                                    }
                                    this->s_path = this->chunk_vec[0].getPath();
                                    break;
                                case ASSET_ID:
                                    if ( XmlFile::ReadEndNode( this->p_demux, p_xmlReader, node, type, s_value ) )
                                    {
                                        msg_Err(this->p_demux, "Missing end node in %s", node.c_str());
                                        return -1;
                                    }
                                    this->s_id = s_value;
                                    break;
                                case ASSET_ANNOTATION_TEXT:
                                    if ( XmlFile::ReadEndNode( this->p_demux, p_xmlReader, node, type, s_value ) )
                                    {
                                        msg_Err(this->p_demux, "Missing end node in %s", node.c_str());
                                        return -1;
                                    }
                                    this->s_annotation = s_value;
                                    break;
                                case ASSET_ORIGINAL_FILENAME:
                                case ASSET_HASH:
                                case ASSET_TYPE:
                                case ASSET_SIZE:
                                    /* Asset tags not in AssetMap */
                                    break;
                                default:
                                    msg_Warn(this->p_demux, "Unknown ASSET_TAG: %i", _tag );
                                    break;
                            }
                            /* break the for loop as a tag is found*/
                            break;
                        }
                    }
                    if( _tag == ASSET_UNKNOWN )
                    {
                        msg_Err(this->p_demux, "Unknown ASSET_TAG: %s", node.c_str());
                        return -1;
                    }
                    break;
                }
            case XML_READER_TEXT:
                msg_Err(this->p_demux, " Text element found in Asset");
                return -1;
            case XML_READER_ENDELEM:
                if ( node != s_root_node) {
                    msg_Err(this->p_demux,
                            "Something goes wrong in Asset parsing on Assetmap (node %s)", node.c_str());
                    return -1;
                } else {
                    /*Check Presence of Id and Chunklist */
                    if ( this->s_id.empty() ) {
                        msg_Err(this->p_demux, " No Id element found in Asset");
                        return -1;
                    }
                    if ( this->s_path.empty() ) {
                        msg_Err(this->p_demux, " No path element found in Asset");
                        return -1;
                    }
                    return 0;
                }
                break;
        }
    }
    return -1;
}



int Asset::ParsePKL( xml_reader_t *p_xmlReader)
{
    string node;
    int type;
    string s_value;
    const string s_root_node = "Asset";

    while( ( type = XmlFile::ReadNextNode( this->p_demux, p_xmlReader, node ) ) > 0 ) {
        switch (type) {
            case XML_READER_STARTELEM:
                {
                    AssetTag_t _tag = ASSET_UNKNOWN;
                    for(AssetTag_t i = ASSET_ID; i <= ASSET_ORIGINAL_FILENAME; i = AssetTag_t(i+1)) {
                        if( node == g_asset_names[i-1]) {
                            _tag = i;
                            switch(_tag) {
                                case ASSET_ANNOTATION_TEXT:
                                    if ( XmlFile::ReadEndNode( this->p_demux, p_xmlReader, node, type, s_value ) )
                                        return -1;
                                    if ( this->s_annotation.empty() )
                                        this->s_annotation = s_value;
                                    else
                                        this->s_annotation = this->s_annotation + "--" + s_value;
                                    break;
                                case ASSET_HASH:
                                    if ( XmlFile::ReadEndNode( this->p_demux, p_xmlReader, node, type, s_value ) )
                                        return -1;
                                    this->s_hash = s_value;
                                    break;
                                case ASSET_SIZE:
                                    if ( XmlFile::ReadEndNode( this->p_demux, p_xmlReader, node, type, s_value ) )
                                        return -1;
                                    this->ui_size = atol(s_value.c_str());
                                    break;
                                case ASSET_TYPE:
                                    if ( XmlFile::ReadEndNode( this->p_demux, p_xmlReader, node, type, s_value ) )
                                        return -1;
                                    this->s_type = s_value;
                                    break;
                                case ASSET_ORIGINAL_FILENAME:
                                    if ( XmlFile::ReadEndNode( this->p_demux, p_xmlReader, node, type, s_value ) )
                                        return -1;
                                    this->s_original_filename = s_value;
                                    break;
                                case ASSET_ID: /* already verified */
                                case ASSET_PACKING_LIST:
                                case ASSET_CHUNK_LIST:
                                    /* Asset tags not in PKL */
                                    break;
                                default:
                                    msg_Warn(this->p_demux, "Unknow ASSET_TAG: %i", _tag );
                                    break;
                            }
                            /* break the for loop as a tag is found*/
                            break;
                        }
                    }
                    if( _tag == ASSET_UNKNOWN )
                        return -1;
                break;
            }
            case XML_READER_TEXT:
                return -1;
            case XML_READER_ENDELEM:
                if ( node != s_root_node) {
                    msg_Err(this->p_demux,
                            "Something goes wrong in Asset parsing on PKL (node %s)", node.c_str());
                    return -1;
                } else {
                    /* Verify that mandatory attributes are filled */
                    if (this->s_hash.empty()) {
                        msg_Err(this->p_demux,"Asset Hash tag invalid");
                        return -1;
                    }
                    if (this->ui_size == 0) {
                        msg_Err(this->p_demux,"Asset Size tag invalid");
                        return -1;
                    }
                    if (this->s_type.empty()) {
                        msg_Err(this->p_demux,"Asset Type tag invalid");
                        return -1;
                    }
                    return 0;
                }
                break;
        }

    }

    return -1;
}

void Asset::Dump()
{
    msg_Dbg(this->p_demux,"Id              = %s", this->s_id.c_str());
    msg_Dbg(this->p_demux,"Path            = %s", this->s_path.c_str());
    msg_Dbg(this->p_demux,"Is PKL          = %s", this->b_is_packing_list ? "True" : "False");
    msg_Dbg(this->p_demux,"Hash            = %s", this->s_hash.c_str());
    msg_Dbg(this->p_demux,"Size            = %i", this->ui_size);
    msg_Dbg(this->p_demux,"Type            = %s", this->s_type.c_str());
    msg_Dbg(this->p_demux,"OrignalFileName = %s", this->s_original_filename.c_str());
    msg_Dbg(this->p_demux,"AnnotationText  = %s", this->s_annotation.c_str());
}

int Asset::parseChunkList( xml_reader_t *p_xmlReader, string p_node, int p_type)
{
    string node;
    int type;
    string s_value;
    std::vector<Chunk> chunk_vec;

    if (p_type != XML_READER_STARTELEM)
        return -1;
    if( p_node != "ChunkList" )
        return -1;
    /* loop on Assets Node */
    while( ( type = XmlFile::ReadNextNode( this->p_demux, p_xmlReader, node ) ) > 0 ) {
         switch (type) {
            case XML_READER_STARTELEM:
                {
                    Chunk chunk(this->p_demux);
                    if (node != "Chunk" )
                        return -1;
                    if ( chunk.Parse(p_xmlReader, node, type) )
                        return -1;
                    chunk_vec.push_back(chunk);
                    break;
                }
            case XML_READER_ENDELEM:
                if ( node == p_node) {
                    if (chunk_vec.size() != 1 ) {
                        msg_Err(this->p_demux, "chunklist of size greater than one not supported");
                        return -1;
                        }
                    this->chunk_vec = chunk_vec;
                    return 0;
                }
                break;
        }
    }
    return -1;
}

AESKey * Asset::getAESKeyById( AESKeyList* p_key_list, const string s_id )
{
    /* return NULL if DCP is not encrypted */
    if( !p_key_list || s_id.empty() )
        return NULL;

    for( AESKeyList::iterator index = p_key_list->begin(); index != p_key_list->end(); ++index )
        if( (*index)->getKeyId() == s_id )
            return *index;

    return NULL;
}


int AssetMap::ParseAssetList (xml_reader_t *p_xmlReader, const string p_node, int p_type)
{
    string node;
    int type;
    Asset *asset;

    if (p_type != XML_READER_STARTELEM)
        return -1;
    if( p_node != "AssetList" )
        return -1;
    /* loop on AssetList nodes */
    while( ( type = XmlFile::ReadNextNode( this->p_demux, p_xmlReader, node ) ) > 0 ) {
        switch (type) {
            case XML_READER_STARTELEM:
                if (node != "Asset" )
                    return -1;
                asset = new (nothrow) Asset(this->p_demux);
                if ( unlikely(asset == NULL) )
                    return -1;
                if (asset->Parse(p_xmlReader, node, type)){
                    msg_Err(this->p_demux, "Error parsing Asset in AssetMap");
                    delete asset;
                    return -1;
                }
                p_dcp->p_asset_list->push_back(asset);
                break;

            case XML_READER_ENDELEM:
                if (node == p_node )
                    return 0;
                break;
            default:
            case XML_READER_TEXT:
                msg_Err(this->p_demux, "Error parsing AssetList in AssetMap");
                return -1;
        }
    }
    return -1;
}

Asset * AssetMap::getAssetById(AssetList *asset_list, const string p_id)
{
    AssetList::iterator index = asset_list->begin() ;
    for (index = asset_list->begin(); index != asset_list->end(); ++index)
        if ((*index)->getId() == p_id )
            return *index;
    return NULL;
}

/*
 * XmlFile Class
 */
XmlFile::~XmlFile() {}

int XmlFile::OpenXml()
{
    char *psz_uri;

    psz_uri = vlc_path2uri( this->s_path.c_str(), "file" );
    this->p_stream = vlc_stream_NewURL(this->p_demux, psz_uri );
    free(psz_uri);
    if( ! this->p_stream ) {
        return -1;
    }

    this->p_xmlReader = xml_ReaderCreate( this->p_demux, this->p_stream);
    if( ! this->p_xmlReader ) {
        vlc_stream_Delete( this->p_stream );
        return -1;
    }
    return 0;
}

int XmlFile::ReadNextNode( demux_t *p_demux, xml_reader_t *p_xmlReader, string& p_node )
{
    const char * c_node;
    int i = xml_ReaderNextNode( p_xmlReader, &c_node );

    if( i <= XML_READER_NONE )
        return i;

    /* remove namespaces, if there are any */
    string s_node = c_node;
    size_t ui_pos = s_node.find( ":" );

    if( ( i == XML_READER_STARTELEM || i == XML_READER_ENDELEM ) && ( ui_pos != string::npos ) )
    {
        try
        {
            p_node = s_node.substr( ui_pos + 1 );
        }
        catch( ... )
        {
            msg_Err( p_demux, "error while handling string" );
            return -1;
        }
    }
    else
        p_node = s_node;

    return i;
}

int XmlFile::ReadEndNode( demux_t *p_demux, xml_reader_t *p_xmlReader, string p_node, int p_type, string &s_value)
{
    string node;

    if ( xml_ReaderIsEmptyElement( p_xmlReader) )
            return 0;

    if (p_type != XML_READER_STARTELEM)
        return -1;

    int n = XmlFile::ReadNextNode( p_demux, p_xmlReader, node );
    if( n == XML_READER_TEXT )
    {
        s_value = node;
        n = XmlFile::ReadNextNode( p_demux, p_xmlReader, node );
        if( ( n == XML_READER_ENDELEM ) && node == p_node)
            return 0;
    }
    return n == XML_READER_ENDELEM ? 0 : -1;
}
/*
 * Reads first node in XML and returns
 * 1 if XML is CPL,
 * 0 if not
 * -1 on error
 */
int XmlFile::isCPL()
{
    string node;
    int type, ret = 0;

    if( this->OpenXml() )
    {
        msg_Err( this->p_demux, "Failed to open CPL XML file" );
        return -1;
    }

    /* read 1st node  and verify that is a CPL */
    type = XmlFile::ReadNextNode( this->p_demux, p_xmlReader, node );
    if( type == -1 ) /* error */
        ret = -1;
    if( type == XML_READER_STARTELEM &&  node == "CompositionPlaylist" )
        ret = 1;

    /* close xml */
    this->CloseXml();
    return ret;
}

void XmlFile::CloseXml() {
    if( this->p_stream )
        vlc_stream_Delete( this->p_stream );
    if( this->p_xmlReader )
        xml_ReaderDelete( this->p_xmlReader );
}

/*
 * PKL Class
 */

PKL::~PKL() {
    vlc_delete_all(vec_cpl);
}

int PKL::Parse()
{
    string node;
    int type;
    string s_value;
    const string s_root_node = "PackingList";

    static const string names[] = {
        "Id",
        "IssueDate",
        "Issuer",
        "Creator",
        "AssetList",
        "AnnotationText",
        "IconId",
        "GroupId",
        "Signer",
        "Signature"
    };

    if (this->OpenXml())
        return -1;

    /* read 1st node  and verify that is a PKL*/
    if ( ! ( ( XML_READER_STARTELEM == XmlFile::ReadNextNode( this->p_demux, this->p_xmlReader, node ) ) &&
                (node == s_root_node) ) ) {
        msg_Err( this->p_demux, "Not a valid XML Packing List");
        goto error;
    }
    while( ( type = XmlFile::ReadNextNode( this->p_demux, this->p_xmlReader, node ) ) ) {
        switch (type) {
            case XML_READER_STARTELEM: {
                PKLTag_t _tag = PKL_UNKNOWN;
                for(PKLTag_t i = PKL_ID; i <= PKL_SIGNATURE; i = PKLTag_t(i+1)) {
                    if( node == names[i-1]) {
                        _tag = i;
                        switch (_tag) {
                            /* case for parsing non terminal nodes */
                            case PKL_ASSET_LIST:
                                if ( this->ParseAssetList(node, type) )
                                    goto error;
                                break;
                            case PKL_SIGNER:
                                if ( this->ParseSigner(node, type) )
                                    goto error;
                                break;
                            case PKL_SIGNATURE:
                                if ( this->ParseSignature(node, type) )
                                    goto error;
                                break;
                            /* Parse simple/end nodes */
                            case PKL_ID:
                                if ( XmlFile::ReadEndNode( this->p_demux, p_xmlReader, node, type, s_value ) )
                                    goto error;
                                this->s_id = s_value;
                                break;
                            case PKL_ISSUE_DATE:
                                if ( XmlFile::ReadEndNode( this->p_demux, p_xmlReader, node, type, s_value ) )
                                    goto error;
                                this->s_issue_date = s_value;
                                break;
                            case PKL_ISSUER:
                                if ( XmlFile::ReadEndNode( this->p_demux, p_xmlReader, node, type, s_value ) )
                                    goto error;
                                this->s_issuer = s_value;
                                break;
                            case PKL_CREATOR:
                                if ( XmlFile::ReadEndNode( this->p_demux, p_xmlReader, node, type, s_value ) )
                                    goto error;
                                this->s_creator = s_value;
                                break;
                            case PKL_ANNOTATION_TEXT:
                                if ( XmlFile::ReadEndNode( this->p_demux, p_xmlReader, node, type, s_value ) )
                                    goto error;
                                this->s_annotation = s_value;
                                break;
                            case PKL_ICON_ID:
                                if ( XmlFile::ReadEndNode( this->p_demux, p_xmlReader, node, type, s_value ) )
                                    goto error;
                                this->s_icon_id = s_value;
                                break;
                            case PKL_GROUP_ID:
                                if ( XmlFile::ReadEndNode( this->p_demux, p_xmlReader, node, type, s_value ) )
                                    goto error;
                                this->s_group_id = s_value;
                                break;
                            default:
                                msg_Warn(this->p_demux, "Unknow PKG_TAG: %i", _tag );
                                break;
                        }
                        /* break the for loop as a tag is found*/
                        break;
                    }
                }
                if( _tag == PKL_UNKNOWN )
                    goto error;
                break;
            }
            case XML_READER_TEXT:
            case -1:
                goto error;
            case XML_READER_ENDELEM:
                if ( node != s_root_node) {
                    msg_Err(this->p_demux,
                        "Something goes wrong in PKL parsing (node %s)", node.c_str());
                    goto error;
                }
                break;
        }
    }
    /* TODO verify presence of mandatory fields*/

    /* Close PKL XML*/
    this->CloseXml();
    return 0;
error:
    msg_Err( this->p_demux, "PKL parsing failed");
    this->CloseXml();
    return -1;
}

int PKL::FindCPLs()
{
    if ( this->vec_cpl.size() != 0 ) {
        msg_Err(this->p_demux, "CPLs already checked");
        return -1;
    }

    for (AssetList::iterator index = this->asset_list->begin();
            index != this->asset_list->end(); ++index) {
        Asset *asset = *index;
        if ( asset->getType().find("text/xml") == string::npos) {
            /* not an xml file */
            continue;
        }

        CPL *cpl = new (nothrow) CPL(this->p_demux,
                      this->s_dcp_path + asset->getPath(),
                      this->asset_list);
        if ( unlikely(cpl == NULL) )
                    return -1;
        switch( cpl->isCPL() )
        {
            case 1:
                /* CPL Found */
                this->vec_cpl.push_back(cpl);
                break;
            case -1:
                /* error */
                delete cpl;
                return -1;
            case 0:
            default:
                delete cpl;
                break;
        }
    }
    return this->vec_cpl.size();
}


int PKL::ParseAssetList(string p_node, int p_type) {
    string node;
    int type;

    if (p_type != XML_READER_STARTELEM)
        return -1;
    if( p_node != "AssetList")
        return -1;
    while( ( type = XmlFile::ReadNextNode( this->p_demux, this->p_xmlReader, node ) ) ) {
        switch (type) {
            case XML_READER_STARTELEM:
                if( node =="Asset") {
                    if ( this->ParseAsset(node, type) )
                        return -1;
                } else
                    return -1;
                break;
            case XML_READER_ENDELEM:
                if ( node == p_node) {
                    /* parse of chunklist finished */
                    goto end;
                }
                break;
            case -1:
                /* error */
                return -1;
        }
    }
end:
    return 0;
}

int PKL::ParseAsset(string p_node, int p_type) {
    string node;
    int type;
    string s_value;
    Asset *asset = NULL;

    if (p_type != XML_READER_STARTELEM)
        return -1;
    if( p_node != "Asset")
        return -1;

    /* 1st node shall be Id" */
    if( ( type = XmlFile::ReadNextNode( this->p_demux, this->p_xmlReader, node ) ) )
        if ( ! ( ( type == XML_READER_STARTELEM ) && ( node == "Id" ) ) || type == -1 )
            return -1;
    if( ( type = XmlFile::ReadNextNode( this->p_demux, this->p_xmlReader, node ) ) != -1 )
    {
        if( type == XML_READER_TEXT )
        {
            s_value = node;
            if (unlikely(node.empty()))
                return -1;
        }
    }
    else
        return -1;

    if( ( type = XmlFile::ReadNextNode( this->p_demux, this->p_xmlReader, node ) ) != -1 )
    {
        if( type == XML_READER_ENDELEM )
        {
            asset = AssetMap::getAssetById(this->asset_list, s_value);
            if (asset  == NULL)
                return -1;
        }
    }
    else
        return -1;

     if (asset == NULL)
        return -1;
     if ( asset->ParsePKL(this->p_xmlReader) )
        return -1;
    return 0;
}

int PKL::ParseSigner(string p_node, int p_type)
{
    string node;
    int type;

    if (p_type != XML_READER_STARTELEM)
        return -1;
    if( p_node != "Signer")
        return -1;

    while( ( type = XmlFile::ReadNextNode( this->p_demux, this->p_xmlReader, node ) ) > 0 ) {
        /* TODO not implemented. Just parse until end of Signer node */
            if ((node == p_node) && (type == XML_READER_ENDELEM))
                return 0;
    }

    msg_Err(this->p_demux, "Parse of Signer finished bad");
    return -1;
}

int PKL::ParseSignature(string p_node, int p_type)
{
    string node;
    int type;

    if (p_type != XML_READER_STARTELEM)
        return -1;
    if( p_node != "Signature")
        return -1;

    while( ( type = XmlFile::ReadNextNode( this->p_demux, this->p_xmlReader, node ) ) > 0 ) {
        /* TODO not implemented. Just parse until end of Signature node */
            if ((node == p_node) && (type == XML_READER_ENDELEM))
                return 0;
    }
    msg_Err(this->p_demux, "Parse of Signature finished bad");
    return -1;
}

/*
 * Reel Class
 */
int Reel::Parse(string p_node, int p_type) {
    string node;
    int type;
    string s_value;

    if (p_type != XML_READER_STARTELEM)
        return -1;
    if( p_node != "Reel")
        return -1;

    while( ( type = XmlFile::ReadNextNode( this->p_demux, this->p_xmlReader, node ) ) > 0 ) {
        switch (type) {
            case XML_READER_STARTELEM:
                if (node =="Id") {
                    if ( XmlFile::ReadEndNode( this->p_demux, this->p_xmlReader, node, type, s_value ) )
                        return -1;
                    this->s_id = s_value;
                } else if (node == "AnnotationText") {
                    if ( XmlFile::ReadEndNode( this->p_demux, this->p_xmlReader, node, type, s_value ) )
                        return -1;
                    this->s_annotation = s_value;
                } else if ( node =="AssetList" ) {
                    if (this->ParseAssetList(node, type))
                        return -1;
                } else {
                    /* unknown tag */
                    msg_Err(this->p_demux, "Reel::Parse, unknown tag:%s", node.c_str());
                    return -1;
                }
                break;
            case XML_READER_TEXT:
                /* Error */
                msg_Err(this->p_demux, "Reel parsing error");
                return -1;
            case XML_READER_ENDELEM:
                /* verify correctness of end node */
                if ( node == p_node) {
                    /* TODO : verify Reel id */
                    return 0;
                }
                break;
        }
    }
    return -1;
}


Asset * Reel::getTrack(TrackType_t e_track)
{
    switch (e_track) {
        case TRACK_PICTURE:
            return this->p_picture_track;
        case TRACK_SOUND:
            return this->p_sound_track;
        case TRACK_SUBTITLE:
            return this->p_subtitle_track;
        case TRACK_UNKNOWN:
        default:
            break;
    }
    return NULL;
}

int Reel::ParseAssetList(string p_node, int p_type) {
    string node;
    int type;
    string s_value;

    if (p_type != XML_READER_STARTELEM)
        return -1;
    if( p_node != "AssetList")
        return -1;

    while( ( type = XmlFile::ReadNextNode( this->p_demux, this->p_xmlReader, node ) ) > 0 )  {
        switch (type) {
            case XML_READER_STARTELEM:
                if (node =="MainPicture") {
                    if ( this->ParseAsset(node, type, TRACK_PICTURE) )
                        return -1;
                } else if (node =="MainSound") {
                    if ( this->ParseAsset(node, type, TRACK_SOUND) )
                        return -1;
                } else if (node =="MainSubtitle") {
                    if ( this->ParseAsset(node, type, TRACK_SUBTITLE) )
                        return -1;
                } else {
                    /* unknown tag */
                    msg_Err(this->p_demux, "Reel::ParseAssetList, unknown tag:%s", node.c_str());
                    return -1;
                }
                break;
            case XML_READER_TEXT:
                /* Parsing error */
                msg_Err(this->p_demux, "AssetList parsing error");
                return -1;
            case XML_READER_ENDELEM:
                /* verify correctness of end node */
                if ( node == p_node) {
                    /* TODO : verify id */
                    return 0;
                }
                break;
        }
    }
    return -1;
}

int Reel::ParseAsset(string p_node, int p_type, TrackType_t e_track) {
    string node;
    int type;
    string s_value;
    bool b_stop_parse = false;
    Asset *asset = NULL;

    if (p_type != XML_READER_STARTELEM)
        return -1;

    /* 1st node shall be Id */
    if( ( type = XmlFile::ReadNextNode( this->p_demux, this->p_xmlReader, node ) ) )
        if( ! ( ( type == XML_READER_STARTELEM ) && ( node == "Id" ) ) || type == -1 )
            return -1;

    if ( XmlFile::ReadEndNode( this->p_demux, this->p_xmlReader, node, type, s_value ) )
        return -1;

    asset = AssetMap::getAssetById(this->p_asset_list, s_value);
    if (asset  == NULL)
        return -1;

    while(  (! b_stop_parse) &&
            ( type = XmlFile::ReadNextNode( this->p_demux, this->p_xmlReader, node ) ) ) {
        switch (type) {
            case XML_READER_STARTELEM:
                if (node =="EditRate") {
                    if ( XmlFile::ReadEndNode( this->p_demux, this->p_xmlReader, node, type, s_value ) )
                        return -1;
                } else if (node == "AnnotationText") {
                    if ( XmlFile::ReadEndNode( this->p_demux, this->p_xmlReader, node, type, s_value ) )
                        return -1;
                    asset->setAnnotation(s_value);
                } else if (node == "IntrinsicDuration") {
                    if ( XmlFile::ReadEndNode( this->p_demux, this->p_xmlReader, node, type, s_value ) )
                        return -1;
                    asset->setIntrinsicDuration(atoi(s_value.c_str()));
                } else if (node == "EntryPoint") {
                    if ( XmlFile::ReadEndNode( this->p_demux, this->p_xmlReader, node, type, s_value ) )
                        return -1;
                    asset->setEntryPoint(atoi(s_value.c_str()));
                } else if (node == "Duration") {
                    if ( XmlFile::ReadEndNode( this->p_demux, this->p_xmlReader, node, type, s_value ) )
                        return -1;
                    asset->setDuration(atoi(s_value.c_str()));
                } else if (node == "KeyId") {
                    if ( XmlFile::ReadEndNode( this->p_demux, this->p_xmlReader, node, type, s_value ) )
                        return -1;
                    asset->setKeyId( s_value );
                } else if (node == "Hash") {
                    if ( XmlFile::ReadEndNode( this->p_demux, this->p_xmlReader, node, type, s_value ) )
                        return -1;
                } else if (node == "FrameRate") {
                    if ( XmlFile::ReadEndNode( this->p_demux, this->p_xmlReader, node, type, s_value ) )
                        return -1;
                } else if (node == "ScreenAspectRatio") {
                    if ( XmlFile::ReadEndNode( this->p_demux, this->p_xmlReader, node, type, s_value ) )
                        return -1;
                } else if (node == "Language") {
                    if ( XmlFile::ReadEndNode( this->p_demux, this->p_xmlReader, node, type, s_value ) )
                        return -1;
                } else {
                    /* unknown tag */
                    msg_Err(this->p_demux, "Reel::ParseAsset unknown tag:%s", node.c_str());
                    return -1;
                }
                break;
            case XML_READER_TEXT:
            case -1:
                /* error */
                return -1;
                break;
            case XML_READER_ENDELEM:
                /* verify correctness of end node */
                if ( node == p_node) {
                    /* TODO : verify id */
                    b_stop_parse = true;
                }
        }
    }
    /* store by track */
    switch (e_track) {
        case TRACK_PICTURE:
            this->p_picture_track = asset;
            break;
        case TRACK_SOUND:
            this->p_sound_track = asset;
            break;
        case TRACK_SUBTITLE:
            this->p_subtitle_track = asset;
            break;
        case TRACK_UNKNOWN:
        default:
            break;
    }
    return 0;
}

/*
 * CPL Class
 */

CPL::~CPL() {
    vlc_delete_all(vec_reel);
}

int CPL::Parse()
{
    string node;
    int type;
    string s_value;
    const string s_root_node = "CompositionPlaylist";

    static const string names[] = {
        "Id",
        "AnnotationText",
        "IconId",
        "IssueDate",
        "Issuer",
        "Creator",
        "ContentTitleText",
        "ContentKind",
        "ContentVersion",
        "RatingList",
        "ReelList",
        "Signer",
        "Signature"
    };

    if (this->OpenXml())
        return -1;

    /* read 1st node  and verify that is a CPL*/
    if( ! ( ( XML_READER_STARTELEM == XmlFile::ReadNextNode( this->p_demux, this->p_xmlReader, node ) ) &&
                (node == s_root_node) ) ) {
        msg_Err( this->p_demux, "Not a valid XML Packing List");
        goto error;
    }

    while( ( type = XmlFile::ReadNextNode( this->p_demux, this->p_xmlReader, node ) ) ) {
        switch (type) {
            case XML_READER_STARTELEM: {
                CPLTag_t _tag = CPL_UNKNOWN;
                for(CPLTag_t i = CPL_ID; i <= CPL_SIGNATURE; i = CPLTag_t(i+1)) {
                    if( node == names[i-1]) {
                        _tag = i;
                        switch (_tag) {
                            /* case for parsing non terminal nodes */
                            case CPL_REEL_LIST:
                                if ( this->ParseReelList(node, type) )
                                    goto error;
                                break;
                            case CPL_CONTENT_VERSION:
                            case CPL_SIGNER:
                            case CPL_SIGNATURE:
                            case CPL_RATING_LIST:
                                if ( this->DummyParse(node,type) )
                                    goto error;
                                break;
                                /* Parse simple/end nodes */
                            case CPL_ID:
                                if ( XmlFile::ReadEndNode( this->p_demux, p_xmlReader, node, type, s_value ) )
                                    goto error;
                                this->s_id = s_value;
                                break;
                            case CPL_ANNOTATION_TEXT:
                                if ( XmlFile::ReadEndNode( this->p_demux, p_xmlReader, node, type, s_value ) )
                                    goto error;
                                this->s_annotation = s_value;
                                break;
                            case CPL_ICON_ID:
                                if ( XmlFile::ReadEndNode( this->p_demux, p_xmlReader, node, type, s_value ) )
                                    goto error;
                                this->s_icon_id = s_value;
                                break;
                            case CPL_ISSUE_DATE:
                                if ( XmlFile::ReadEndNode( this->p_demux, p_xmlReader, node, type, s_value ) )
                                    goto error;
                                this->s_issue_date= s_value;
                                break;
                            case CPL_ISSUER:
                                if ( XmlFile::ReadEndNode( this->p_demux, p_xmlReader, node, type, s_value ) )
                                    goto error;
                                this->s_issuer = s_value;
                                break;
                            case CPL_CREATOR:
                                if ( XmlFile::ReadEndNode( this->p_demux, p_xmlReader, node, type, s_value ) )
                                    goto error;
                                this->s_creator = s_value;
                                break;
                            case CPL_CONTENT_TITLE:
                                if ( XmlFile::ReadEndNode( this->p_demux, p_xmlReader, node, type, s_value ) )
                                    goto error;
                                this->s_content_title = s_value;
                                break;
                            case CPL_CONTENT_KIND:
                                if ( XmlFile::ReadEndNode( this->p_demux, p_xmlReader, node, type, s_value ) )
                                    goto error;
                                this->s_content_kind = s_value;
                                break;
                            default:
                                msg_Warn(this->p_demux, "Unknow CPL_TAG: %i", _tag );
                                break;
                        }

                        /* break the for loop as a tag is found*/
                        break;
                    }
                }
                if( _tag == CPL_UNKNOWN )
                    goto error;
                break;
            }
            case XML_READER_TEXT:
            case -1:
               goto error;
            case XML_READER_ENDELEM:
               if ( node != s_root_node) {
                   msg_Err(this->p_demux,
                           "Something goes wrong in CKL parsing (node %s)", node.c_str());
                   goto error;
               }
               break;
        }
    }

    /* TODO verify presence of mandatory fields*/

    /* Close CPL XML*/
    this->CloseXml();
    return 0;
error:
    this->CloseXml();
    return -1;
}

int CPL::ParseReelList(string p_node, int p_type) {
    string node;
    int type;

    if (p_type != XML_READER_STARTELEM)
        return -1;
    if( p_node != "ReelList")
        return -1;
    while( ( type = XmlFile::ReadNextNode( this->p_demux, this->p_xmlReader, node ) ) > 0 ) {
        switch (type) {
            case XML_READER_STARTELEM: {
                Reel *p_reel = new (nothrow) Reel( this->p_demux, this->asset_list, this->p_xmlReader);
                if ( unlikely(p_reel == NULL) )
                    return -1;
                if( node =="Reel") {
                    if ( p_reel->Parse(node, type) ) {
                        delete p_reel;
                        return -1;
                    }
                } else {
                    delete p_reel;
                    return -1;
                }
                this->vec_reel.push_back(p_reel);

                break;
            }
            case XML_READER_TEXT:
                /* impossible */
                break;
            case XML_READER_ENDELEM:
                if ( node == p_node)
                    return 0;
                break;
            }
    }
    return -1;
}


int CPL::DummyParse(string p_node, int p_type)
{
    string node;
    int type;

    if (p_type != XML_READER_STARTELEM)
        return -1;

    if (xml_ReaderIsEmptyElement( this->p_xmlReader))
        return 0;

    while( ( type = XmlFile::ReadNextNode( this->p_demux, this->p_xmlReader, node ) ) > 0 ) {
        /* TODO not implemented. Just pase until end of input node */
        if ((node == p_node) && (type == XML_READER_ENDELEM))
            return 0;
    }

    return -1;
}
