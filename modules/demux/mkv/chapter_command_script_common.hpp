// Copyright (C) 2003-2024 VLC authors and VideoLAN
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// chapter_command_script.hpp : MatroskaScript codec for Matroska Chapter Codecs
// Authors: Laurent Aimar <fenrir@via.ecp.fr>
//          Steve Lhomme <steve.lhomme@free.fr>

#ifndef VLC_MKV_CHAPTER_COMMAND_SCRIPT_COMMON_HPP_
#define VLC_MKV_CHAPTER_COMMAND_SCRIPT_COMMON_HPP_

#include "chapter_command.hpp"

namespace mkv {

class matroska_script_interpreter_common_c
{
public:
    matroska_script_interpreter_common_c( struct vlc_logger *log, chapter_codec_vm & vm_ )
    :l( log )
    ,vm( vm_ )
    {}

    virtual ~matroska_script_interpreter_common_c() = default;

    // DVD command IDs
    virtual bool Interpret( const binary * p_command, size_t i_size ) = 0;

protected:
    struct vlc_logger *l;
    chapter_codec_vm & vm;
};

class matroska_script_codec_common_c : public chapter_codec_cmds_c
{
public:

    matroska_script_codec_common_c( struct vlc_logger *log, chapter_codec_vm &vm_, enum chapter_codec_id codec_id)
    :chapter_codec_cmds_c(log, vm_, codec_id)
    {}

    bool Enter();
    bool Leave();

    virtual matroska_script_interpreter_common_c & get_interpreter()=0;
};

} // namespace

#endif // VLC_MKV_CHAPTER_COMMAND_SCRIPT_HPP_
