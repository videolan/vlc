// Copyright (C) 2003-2024 VLC authors and VideoLAN
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// chapter_command_script.hpp : MatroskaScript codec for Matroska Chapter Codecs
// Authors: Laurent Aimar <fenrir@via.ecp.fr>
//          Steve Lhomme <steve.lhomme@free.fr>

#ifndef VLC_MKV_CHAPTER_COMMAND_SCRIPT_HPP_
#define VLC_MKV_CHAPTER_COMMAND_SCRIPT_HPP_

#include "chapter_command_script_common.hpp"

namespace mkv {

class matroska_script_interpretor_c : public matroska_script_interpreter_common_c
{
public:
    matroska_script_interpretor_c( struct vlc_logger *log, chapter_codec_vm & vm_ )
    :matroska_script_interpreter_common_c(log, vm_)
    {}

    bool Interpret( const binary * p_command, size_t i_size ) override;

    // Matroska Script commands
    static const std::string CMD_MS_GOTO_AND_PLAY;

};

class matroska_script_codec_c : public matroska_script_codec_common_c
{
public:
    matroska_script_codec_c( struct vlc_logger *log, chapter_codec_vm & vm_, matroska_script_interpretor_c & interpreter_)
    :matroska_script_codec_common_c( log, vm_, MATROSKA_CHAPTER_CODEC_NATIVE )
    ,interpreter( interpreter_ )
    {}

    matroska_script_interpreter_common_c & get_interpreter() override
    {
        return interpreter;
    }

protected:
    matroska_script_interpretor_c & interpreter;
};

} // namespace

#endif // VLC_MKV_CHAPTER_COMMAND_SCRIPT_HPP_
