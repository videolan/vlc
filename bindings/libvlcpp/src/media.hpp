/*****************************************************************************
 * media.hpp: represent a media
 *****************************************************************************
 * Copyright (C) 2010 the VideoLAN team
 * $Id$
 *
 * Authors: Rémi Duraffort <ivoire@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef LIBVLCPP_MEDIA_HPP
#define LIBVLCPP_MEDIA_HPP

#include <vlc/libvlc.h>
#include <vlc/libvlc_media.h>

#include "libvlc.hpp"

namespace libvlc
{

class MediaPlayer;

class Media
{
public:
    /**
     * Contructor
     * @param libvlcInstance: instance of the libVLC class
     * @param psz_mrl: MRL of the media
     */
    Media( libVLC &libvlcInstance, const char *psz_mrl );

    /**
     * Copy contructor
     * @param original: the instance to copy
     */
    Media( const Media& original );

    /** \todo: node contructor */

    /** Destructor */
    ~Media();

    /**
     * Add an option to the media
     * @param ppsz_options: the options as a string
     */
    void addOption( const char *ppsz_options );

    /**
     * Add an option to the media
     * @param ppsz_options: the options as a string
     * @param flag: the flag for the options
     */
    void addOption( const char *ppsz_options, libvlc_media_option_t flag );


    /**
     * Get the duration of the media
     * @return the duration
     */
    int64_t duration();

    /**
     * Get preparsed status of the media
     * @return true if the media has been preparsed, false otherwise
     */
    int isPreparsed();

    /**
     * Get the MRL of the media
     * @return the MRL of the media
     */
    char *mrl();

    /**
     * Get the requiered meta
     * @param e_meta: type of the meta
     * @return the requiered meta
     */
    char *meta( libvlc_meta_t e_meta );

    /**
     * Set the given meta
     * @param e_meta: type of the meta
     * @param psz_value: value of the meta
     */
    void setMeta( libvlc_meta_t e_meta, const char *psz_value );

    /**
     * Save the meta to the file
     * @return true if the operation was successfull
     */
    int saveMeta();

    /**
     * Get the state of the media
     * @return the state of the media
     */
    libvlc_state_t state();

    /**
     * Get some statistics about this media
     * @return the statistics
     */
    libvlc_media_stats_t *stats();

    /**\todo: subItems */
    /**\todo: eventManager */

    /**
     * Set media descriptor's user data
     * @param p_user_data: pointer to user data
     */
    void setUserData( void *p_user_data );

    /**
     * Retrive user data specified by a call to setUserData
     * @return the user data pointer
     */
    void *userData();

private:
    /**
     * Get the instance of the libvlc_media_t
     * @return the pointer to libvlc_media_t
     */
    libvlc_media_t *instance();

    /** The media */
    libvlc_media_t *m_media;

    /** Friend class that can access the instance */
    friend class MediaPlayer;

};

};

#endif // LIBVLCPP_MEDIA_HPP

