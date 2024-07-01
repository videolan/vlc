// Copyright (C) 2003-2024 VLC authors and VideoLAN
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// chapter_command_script.hpp : MatroskaScript codec for Matroska Chapter Codecs
// Authors: Laurent Aimar <fenrir@via.ecp.fr>
//          Steve Lhomme <steve.lhomme@free.fr>

#ifndef VLC_MKV_CHAPTER_COMMAND_SCRIPT_HPP_
#define VLC_MKV_CHAPTER_COMMAND_SCRIPT_HPP_

#include "chapter_command.hpp"

namespace mkv {

class matroska_script_interpretor_c
{
public:
    matroska_script_interpretor_c( struct vlc_logger *log, chapter_codec_vm & vm_ )
    :l( log )
    ,vm( vm_ )
    {}

    bool Interpret( const binary * p_command, size_t i_size );

    // DVD command IDs
    static const std::string CMD_MS_GOTO_AND_PLAY;

protected:
    struct vlc_logger *l;
    chapter_codec_vm & vm;
};


class matroska_script_codec_c : public chapter_codec_cmds_c
{
public:
    matroska_script_codec_c( struct vlc_logger *log, chapter_codec_vm & vm_, matroska_script_interpretor_c & interpreter_)
    :chapter_codec_cmds_c( log, vm_, MATROSKA_CHAPTER_CODEC_NATIVE )
    ,interpreter( interpreter_ )
    {}

    bool Enter();
    bool Leave();

protected:
    matroska_script_interpretor_c & interpreter;
};

} // namespace

#endif // VLC_MKV_CHAPTER_COMMAND_SCRIPT_HPP_
