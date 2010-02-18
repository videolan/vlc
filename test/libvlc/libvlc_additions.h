/*
 * libvlc_additions.c - Some helper method that should probably go into libvlc
 */

/**********************************************************************
 *  Copyright (C) 2007 Rémi Denis-Courmont.                           *
 *  This program is free software; you can redistribute and/or modify *
 *  it under the terms of the GNU General Public License as published *
 *  by the Free Software Foundation; version 2 of the license, or (at *
 *  your option) any later version.                                   *
 *                                                                    *
 *  This program is distributed in the hope that it will be useful,   *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of    *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.              *
 *  See the GNU General Public License for more details.              *
 *                                                                    *
 *  You should have received a copy of the GNU General Public License *
 *  along with this program; if not, you can get it from:             *
 *  http://www.gnu.org/copyleft/gpl.html                              *
 **********************************************************************/

static void* media_list_add_file_path(libvlc_instance_t *vlc, libvlc_media_list_t *ml, const char * file_path)
{
    libvlc_media_t *md = libvlc_media_new_location (vlc, file_path);
    libvlc_media_list_add_media (ml, md);
    libvlc_media_release (md);
    return md;
}

