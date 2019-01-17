/*****************************************************************************
 * chapter_command.hpp : matroska demuxer
 *****************************************************************************
 * Copyright (C) 2003-2004 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Steve Lhomme <steve.lhomme@free.fr>
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

#ifndef VLC_MKV_CHAPTER_COMMAND_HPP_
#define VLC_MKV_CHAPTER_COMMAND_HPP_

#include "mkv.hpp"

namespace mkv {

const int MATROSKA_CHAPTER_CODEC_NATIVE  = 0x00;
const int MATROSKA_CHAPTER_CODEC_DVD     = 0x01;

const binary MATROSKA_DVD_LEVEL_SS   = 0x30;
const binary MATROSKA_DVD_LEVEL_LU   = 0x2A;
const binary MATROSKA_DVD_LEVEL_TT   = 0x28;
const binary MATROSKA_DVD_LEVEL_PGC  = 0x20;
const binary MATROSKA_DVD_LEVEL_PG   = 0x18;
const binary MATROSKA_DVD_LEVEL_PTT  = 0x10;
const binary MATROSKA_DVD_LEVEL_CN   = 0x08;

struct demux_sys_t;

class chapter_codec_cmds_c
{
public:
    chapter_codec_cmds_c( demux_sys_t & demuxer, int codec_id = -1)
    :p_private_data(NULL)
    ,i_codec_id( codec_id )
    ,sys( demuxer )
    {}

    virtual ~chapter_codec_cmds_c()
    {
        delete p_private_data;
        vlc_delete_all( enter_cmds );
        vlc_delete_all( leave_cmds );
        vlc_delete_all( during_cmds );
    }

    void SetPrivate( const KaxChapterProcessPrivate & private_data )
    {
        p_private_data = new KaxChapterProcessPrivate( private_data );
    }

    void AddCommand( const KaxChapterProcessCommand & command );

    /// \return whether the codec has seeked in the files or not
    virtual bool Enter() { return false; }
    virtual bool Leave() { return false; }
    virtual std::string GetCodecName( bool ) const { return ""; }
    virtual int16 GetTitleNumber() { return -1; }

    KaxChapterProcessPrivate *p_private_data;

protected:
    std::vector<KaxChapterProcessData*> enter_cmds;
    std::vector<KaxChapterProcessData*> during_cmds;
    std::vector<KaxChapterProcessData*> leave_cmds;

    int i_codec_id;
    demux_sys_t & sys;
};


class dvd_command_interpretor_c
{
public:
    dvd_command_interpretor_c( demux_sys_t & demuxer )
    :sys( demuxer )
    {
        memset( p_PRMs, 0, sizeof(p_PRMs) );
        p_PRMs[ 0x80 + 1 ] = 15;
        p_PRMs[ 0x80 + 2 ] = 62;
        p_PRMs[ 0x80 + 3 ] = 1;
        p_PRMs[ 0x80 + 4 ] = 1;
        p_PRMs[ 0x80 + 7 ] = 1;
        p_PRMs[ 0x80 + 8 ] = 1;
        p_PRMs[ 0x80 + 16 ] = 0xFFFFu;
        p_PRMs[ 0x80 + 18 ] = 0xFFFFu;
    }

    bool Interpret( const binary * p_command, size_t i_size = 8 );

    uint16 GetPRM( size_t index ) const
    {
        if ( index < 256 )
            return p_PRMs[ index ];
        else return 0;
    }

    uint16 GetGPRM( size_t index ) const
    {
        if ( index < 16 )
            return p_PRMs[ index ];
        else return 0;
    }

    uint16 GetSPRM( size_t index ) const
    {
        // 21,22,23 reserved for future use
        if ( index >= 0x80 && index < 0x95 )
            return p_PRMs[ index ];
        else return 0;
    }

    bool SetPRM( size_t index, uint16 value )
    {
        if ( index < 16 )
        {
            p_PRMs[ index ] = value;
            return true;
        }
        return false;
    }

    bool SetGPRM( size_t index, uint16 value )
    {
        if ( index < 16 )
        {
            p_PRMs[ index ] = value;
            return true;
        }
        return false;
    }

    bool SetSPRM( size_t index, uint16 value )
    {
        if ( index > 0x80 && index <= 0x8D && index != 0x8C )
        {
            p_PRMs[ index ] = value;
            return true;
        }
        return false;
    }

protected:
    std::string GetRegTypeName( bool b_value, uint16 value ) const
    {
        std::string result;
        char s_value[6], s_reg_value[6];
        sprintf( s_value, "%.5d", value );

        if ( b_value )
        {
            result = "value (";
            result += s_value;
            result += ")";
        }
        else if ( value < 0x80 )
        {
            sprintf( s_reg_value, "%.5d", GetPRM( value ) );
            result = "GPreg[";
            result += s_value;
            result += "] (";
            result += s_reg_value;
            result += ")";
        }
        else
        {
            sprintf( s_reg_value, "%.5d", GetPRM( value ) );
            result = "SPreg[";
            result += s_value;
            result += "] (";
            result += s_reg_value;
            result += ")";
        }
        return result;
    }

    uint16       p_PRMs[256];
    demux_sys_t  & sys;

    // DVD command IDs

    // Tests
    // whether it's a comparison on the value or register
    static const uint16 CMD_DVD_TEST_VALUE          = 0x80;
    static const uint16 CMD_DVD_IF_GPREG_AND        = (1 << 4);
    static const uint16 CMD_DVD_IF_GPREG_EQUAL      = (2 << 4);
    static const uint16 CMD_DVD_IF_GPREG_NOT_EQUAL  = (3 << 4);
    static const uint16 CMD_DVD_IF_GPREG_SUP_EQUAL  = (4 << 4);
    static const uint16 CMD_DVD_IF_GPREG_SUP        = (5 << 4);
    static const uint16 CMD_DVD_IF_GPREG_INF_EQUAL  = (6 << 4);
    static const uint16 CMD_DVD_IF_GPREG_INF        = (7 << 4);

    static const uint16 CMD_DVD_NOP                    = 0x0000;
    static const uint16 CMD_DVD_GOTO_LINE              = 0x0001;
    static const uint16 CMD_DVD_BREAK                  = 0x0002;
    // Links
    static const uint16 CMD_DVD_NOP2                   = 0x2001;
    static const uint16 CMD_DVD_LINKPGCN               = 0x2004;
    static const uint16 CMD_DVD_LINKPGN                = 0x2006;
    static const uint16 CMD_DVD_LINKCN                 = 0x2007;
    static const uint16 CMD_DVD_JUMP_TT                = 0x3002;
    static const uint16 CMD_DVD_JUMPVTS_TT             = 0x3003;
    static const uint16 CMD_DVD_JUMPVTS_PTT            = 0x3005;
    static const uint16 CMD_DVD_JUMP_SS                = 0x3006;
    static const uint16 CMD_DVD_CALLSS_VTSM1           = 0x3008;
    //
    static const uint16 CMD_DVD_SET_HL_BTNN2           = 0x4600;
    static const uint16 CMD_DVD_SET_HL_BTNN_LINKPGCN1  = 0x4604;
    static const uint16 CMD_DVD_SET_STREAM             = 0x5100;
    static const uint16 CMD_DVD_SET_GPRMMD             = 0x5300;
    static const uint16 CMD_DVD_SET_HL_BTNN1           = 0x5600;
    static const uint16 CMD_DVD_SET_HL_BTNN_LINKPGCN2  = 0x5604;
    static const uint16 CMD_DVD_SET_HL_BTNN_LINKCN     = 0x5607;
    // Operations
    static const uint16 CMD_DVD_MOV_SPREG_PREG         = 0x6100;
    static const uint16 CMD_DVD_GPREG_MOV_VALUE        = 0x7100;
    static const uint16 CMD_DVD_SUB_GPREG              = 0x7400;
    static const uint16 CMD_DVD_MULT_GPREG             = 0x7500;
    static const uint16 CMD_DVD_GPREG_DIV_VALUE        = 0x7600;
    static const uint16 CMD_DVD_GPREG_AND_VALUE        = 0x7900;

    // callbacks when browsing inside CodecPrivate
    static bool MatchIsDomain     ( const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size );
    static bool MatchIsVMG        ( const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size );
    static bool MatchVTSNumber    ( const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size );
    static bool MatchVTSMNumber   ( const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size );
    static bool MatchTitleNumber  ( const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size );
    static bool MatchPgcType      ( const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size );
    static bool MatchPgcNumber    ( const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size );
    static bool MatchChapterNumber( const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size );
    static bool MatchCellNumber   ( const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size );
};

class dvd_chapter_codec_c : public chapter_codec_cmds_c
{
public:
    dvd_chapter_codec_c( demux_sys_t & sys )
    :chapter_codec_cmds_c( sys, 1 )
    {}

    bool Enter();
    bool Leave();

    std::string GetCodecName( bool f_for_title = false ) const;
    int16 GetTitleNumber();

protected:
    bool EnterLeaveHelper( char const*, std::vector<KaxChapterProcessData*>* );
};

class matroska_script_interpretor_c
{
public:
    matroska_script_interpretor_c( demux_sys_t & demuxer )
    :sys( demuxer )
    {}

    bool Interpret( const binary * p_command, size_t i_size );

    // DVD command IDs
    static const std::string CMD_MS_GOTO_AND_PLAY;

protected:
    demux_sys_t  & sys;
};


class matroska_script_codec_c : public chapter_codec_cmds_c
{
public:
    matroska_script_codec_c( demux_sys_t & sys )
    :chapter_codec_cmds_c( sys, 0 )
    ,interpretor( sys )
    {}

    bool Enter();
    bool Leave();

protected:
    matroska_script_interpretor_c interpretor;
};

} // namespace

#endif
