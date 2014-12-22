/*****************************************************************************
 * archive.c: libarchive based stream filter
 *****************************************************************************
 * Copyright (C) 2014 Videolan Team
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

#include "archive.h"

#include <vlc_plugin.h>
#include <vlc_stream.h>

#include <archive.h>

/****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin()
    set_shortname( "libarchive" )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )
    set_description( N_( "libarchive access" ) )
    set_capability( "access", 0 )
    add_shortcut( "archive" )
    set_callbacks( AccessOpen, AccessClose )
    add_submodule()
        set_shortname( "libarchive" )
        set_subcategory( SUBCAT_INPUT_STREAM_FILTER )
        set_description( N_( "libarchive stream filter" ) )
        set_capability( "stream_filter", 14 ) /* less than rar and gzip */
        set_callbacks( StreamOpen, StreamClose )
vlc_module_end()

bool ProbeArchiveFormat(stream_t *p_stream)
{
    struct
    {
        const uint16_t i_offset;
        const uint8_t  i_length;
        const char * const p_bytes;
    } const magicbytes[9] = {
        /* keep heaviest at top */
        { 257, 5, "ustar" },        //TAR
        { 0,   7, "Rar!\x1A\x07" }, //RAR
        { 0,   4, "xar!" },         //XAR
        { 2,   3, "-lh" },          //LHA/LHZ
        { 0,   3, "PAX" },          //PAX
        { 0,   6, "070707" },       //CPIO
        { 0,   6, "070701" },       //CPIO
        { 0,   6, "070702" },       //CPIO
        { 0,   4, "MSCH" },         //CAB
    };

    const uint8_t *p_peek;
    int i_peek = stream_Peek(p_stream, &p_peek, magicbytes[0].i_offset + magicbytes[0].i_length);

    for(int i=0; i<9;i++)
    {
        if (i_peek <= magicbytes[i].i_offset + magicbytes[i].i_length)
            continue;
        else if ( !memcmp(p_peek + magicbytes[i].i_offset,
                          magicbytes[i].p_bytes,
                          magicbytes[i].i_length) )
            return true;
    }

    return false;
}

void EnableArchiveFormats(struct archive *p_archive)
{
    //    archive_read_support_filter_bzip2(p_archive);
    //    archive_read_support_filter_compress(p_archive);
    //    archive_read_support_filter_gzip(p_archive);
    //    archive_read_support_filter_grzip(p_archive);
    //    archive_read_support_filter_lrzip(p_archive);
    //    archive_read_support_filter_lzip(p_archive);
    archive_read_support_filter_lzma(p_archive);
    archive_read_support_filter_lzop(p_archive);
    archive_read_support_filter_none(p_archive);
    archive_read_support_filter_rpm(p_archive);
    archive_read_support_filter_uu(p_archive);
    archive_read_support_filter_xz(p_archive);

    //    archive_read_support_format_7zip(p_archive);
    archive_read_support_format_ar(p_archive);
    archive_read_support_format_cab(p_archive);
    archive_read_support_format_cpio(p_archive);
    archive_read_support_format_gnutar(p_archive);
    //    archive_read_support_format_iso9660(p_archive);
    archive_read_support_format_lha(p_archive);
    archive_read_support_format_mtree(p_archive);
    archive_read_support_format_rar(p_archive);
    archive_read_support_format_raw(p_archive);
    archive_read_support_format_tar(p_archive);
    archive_read_support_format_xar(p_archive);
    //    archive_read_support_format_zip(p_archive);
}
