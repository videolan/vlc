// Copyright (C) 2003-2024 VLC authors and VideoLAN
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// chapter_command_dvd.hpp : DVD codec for Matroska Chapter Codecs
// Authors: Laurent Aimar <fenrir@via.ecp.fr>
//          Steve Lhomme <steve.lhomme@free.fr>


#ifndef VLC_MKV_CHAPTER_COMMAND_DVD_HPP_
#define VLC_MKV_CHAPTER_COMMAND_DVD_HPP_

#include "chapter_command.hpp"

#include "dvd_types.hpp"

namespace mkv {

class dvd_command_interpretor_c
{
public:
    dvd_command_interpretor_c( struct vlc_logger *log, chapter_codec_vm & vm_ )
    :l( log )
    ,vm( vm_ )
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

    bool HandleKeyEvent( NavivationKey );
    void HandleMousePressed( unsigned x, unsigned y );

    void SetPci(const uint8_t *, unsigned size);
protected:
    uint16_t GetPRM( size_t index ) const
    {
        if ( index < 256 )
            return p_PRMs[ index ];
        else return 0;
    }

    uint16_t GetGPRM( size_t index ) const
    {
        if ( index < 16 )
            return p_PRMs[ index ];
        else return 0;
    }

    uint16_t GetSPRM( size_t index ) const
    {
        // 21,22,23 reserved for future use
        if ( index >= 0x80 && index < 0x95 )
            return p_PRMs[ index ];
        else return 0;
    }

    bool SetPRM( size_t index, uint16_t value )
    {
        if ( index < 16 )
        {
            p_PRMs[ index ] = value;
            return true;
        }
        return false;
    }

    bool SetGPRM( size_t index, uint16_t value )
    {
        if ( index < 16 )
        {
            p_PRMs[ index ] = value;
            return true;
        }
        return false;
    }

    bool SetSPRM( size_t index, uint16_t value )
    {
        if ( index > 0x80 && index <= 0x8D && index != 0x8C )
        {
            p_PRMs[ index ] = value;
            return true;
        }
        return false;
    }

    bool ProcessNavAction( uint16_t button );

    std::string GetRegTypeName( bool b_value, uint16_t value ) const
    {
        std::string result;
        char s_value[6], s_reg_value[6];
        snprintf( s_value, ARRAY_SIZE(s_value), "%.5d", value );

        if ( b_value )
        {
            result = "value (";
            result += s_value;
            result += ")";
        }
        else if ( value < 0x80 )
        {
            snprintf( s_reg_value, ARRAY_SIZE(s_reg_value), "%.5d", GetPRM( value ) );
            result = "GPreg[";
            result += s_value;
            result += "] (";
            result += s_reg_value;
            result += ")";
        }
        else
        {
            snprintf( s_reg_value, ARRAY_SIZE(s_reg_value), "%.5d", GetPRM( value ) );
            result = "SPreg[";
            result += s_value;
            result += "] (";
            result += s_reg_value;
            result += ")";
        }
        return result;
    }

    uint16_t       p_PRMs[256];
    struct vlc_logger *l;
    chapter_codec_vm & vm;
    pci_t          pci_packet = {};

    // DVD command IDs

    // Tests
    // whether it's a comparison on the value or register
    static const uint16_t CMD_DVD_TEST_VALUE          = 0x80;
    static const uint16_t CMD_DVD_IF_GPREG_AND        = (1 << 4);
    static const uint16_t CMD_DVD_IF_GPREG_EQUAL      = (2 << 4);
    static const uint16_t CMD_DVD_IF_GPREG_NOT_EQUAL  = (3 << 4);
    static const uint16_t CMD_DVD_IF_GPREG_SUP_EQUAL  = (4 << 4);
    static const uint16_t CMD_DVD_IF_GPREG_SUP        = (5 << 4);
    static const uint16_t CMD_DVD_IF_GPREG_INF_EQUAL  = (6 << 4);
    static const uint16_t CMD_DVD_IF_GPREG_INF        = (7 << 4);

    static const uint16_t CMD_DVD_NOP                    = 0x0000;
    static const uint16_t CMD_DVD_GOTO_LINE              = 0x0001;
    static const uint16_t CMD_DVD_BREAK                  = 0x0002;
    // Links
    static const uint16_t CMD_DVD_NOP2                   = 0x2001;
    static const uint16_t CMD_DVD_LINKPGCN               = 0x2004;
    static const uint16_t CMD_DVD_LINKPGN                = 0x2006;
    static const uint16_t CMD_DVD_LINKCN                 = 0x2007;
    static const uint16_t CMD_DVD_JUMP_TT                = 0x3002;
    static const uint16_t CMD_DVD_JUMPVTS_TT             = 0x3003;
    static const uint16_t CMD_DVD_JUMPVTS_PTT            = 0x3005;
    static const uint16_t CMD_DVD_JUMP_SS                = 0x3006;
    static const uint16_t CMD_DVD_CALLSS_VTSM1           = 0x3008;
    //
    static const uint16_t CMD_DVD_SET_HL_BTNN2           = 0x4600;
    static const uint16_t CMD_DVD_SET_HL_BTNN_LINKPGCN1  = 0x4604;
    static const uint16_t CMD_DVD_SET_STREAM             = 0x5100;
    static const uint16_t CMD_DVD_SET_GPRMMD             = 0x5300;
    static const uint16_t CMD_DVD_SET_HL_BTNN1           = 0x5600;
    static const uint16_t CMD_DVD_SET_HL_BTNN_LINKPGCN2  = 0x5604;
    static const uint16_t CMD_DVD_SET_HL_BTNN_LINKCN     = 0x5607;
    // Operations
    static const uint16_t CMD_DVD_MOV_SPREG_PREG         = 0x6100;
    static const uint16_t CMD_DVD_GPREG_MOV_VALUE        = 0x7100;
    static const uint16_t CMD_DVD_SUB_GPREG              = 0x7400;
    static const uint16_t CMD_DVD_MULT_GPREG             = 0x7500;
    static const uint16_t CMD_DVD_GPREG_DIV_VALUE        = 0x7600;
    static const uint16_t CMD_DVD_GPREG_AND_VALUE        = 0x7900;

    // callbacks when browsing inside CodecPrivate
    static bool MatchIsDomain     ( const chapter_codec_cmds_c & );
    static bool MatchIsVMG        ( const chapter_codec_cmds_c & );
    static bool MatchVTSNumber    ( const chapter_codec_cmds_c &, uint16_t i_title );
    static bool MatchVTSMNumber   ( const chapter_codec_cmds_c &, uint8_t i_title );
    static bool MatchTitleNumber  ( const chapter_codec_cmds_c &, uint8_t i_title );
    static bool MatchPgcType      ( const chapter_codec_cmds_c &, uint8_t i_pgc );
    static bool MatchPgcNumber    ( const chapter_codec_cmds_c &, uint16_t i_pgc_n );
    static bool MatchChapterNumber( const chapter_codec_cmds_c &, uint8_t i_ptt );
    static bool MatchCellNumber   ( const chapter_codec_cmds_c &, uint8_t i_cell_n );
};

class dvd_chapter_codec_c : public chapter_codec_cmds_c
{
public:
    dvd_chapter_codec_c( struct vlc_logger *log, chapter_codec_vm & vm_, dvd_command_interpretor_c & intepretor_ )
    :chapter_codec_cmds_c( log, vm_, MATROSKA_CHAPTER_CODEC_DVD )
    ,intepretor(intepretor_)
    {}

    bool Enter() override;
    bool Leave() override;

    std::string GetCodecName( bool f_for_title = false ) const override;
    int16_t GetTitleNumber() const override;

protected:
    bool EnterLeaveHelper( char const*, ChapterProcess & );
    dvd_command_interpretor_c & intepretor;
};

} // namespace

#endif // VLC_MKV_CHAPTER_COMMAND_DVD_HPP_
