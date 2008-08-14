/*****************************************************************************
 * libvlc_vlm.h:  libvlc_* new external API
 *****************************************************************************
 * Copyright (C) 1998-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Paul Saman <jpsaman _at_ m2x _dot_ nl>
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

#ifndef LIBVLC_VLM_H
#define LIBVLC_VLM_H 1

/**
 * \file
 * This file defines libvlc_vlm_* external API
 */

# ifdef __cplusplus
extern "C" {
# endif

/*****************************************************************************
 * VLM
 *****************************************************************************/
/** \defgroup libvlc_vlm libvlc_vlm
 * \ingroup libvlc
 * LibVLC VLM
 * @{
 */


/**
 * Release the vlm instance related to the given libvlc_instance_t
 *
 * \param p_instance the instance
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_vlm_release( libvlc_instance_t *, libvlc_exception_t * );

/**
 * Add a broadcast, with one input.
 *
 * \param p_instance the instance
 * \param psz_name the name of the new broadcast
 * \param psz_input the input MRL
 * \param psz_output the output MRL (the parameter to the "sout" variable)
 * \param i_options number of additional options
 * \param ppsz_options additional options
 * \param b_enabled boolean for enabling the new broadcast
 * \param b_loop Should this broadcast be played in loop ?
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_vlm_add_broadcast( libvlc_instance_t *, char *, char *, char* ,
                                              int, char **, int, int, libvlc_exception_t * );

/**
 * Add a vod, with one input.
 *
 * \param p_instance the instance
 * \param psz_name the name of the new vod media
 * \param psz_input the input MRL
 * \param i_options number of additional options
 * \param ppsz_options additional options
 * \param b_enabled boolean for enabling the new vod
 * \param psz_mux the muxer of the vod media
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_vlm_add_vod( libvlc_instance_t *, char *, char *, int, char **,
                                        int, char *, libvlc_exception_t * );

/**
 * Delete a media (VOD or broadcast).
 *
 * \param p_instance the instance
 * \param psz_name the media to delete
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_vlm_del_media( libvlc_instance_t *, char *, libvlc_exception_t * );

/**
 * Enable or disable a media (VOD or broadcast).
 *
 * \param p_instance the instance
 * \param psz_name the media to work on
 * \param b_enabled the new status
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_vlm_set_enabled( libvlc_instance_t *, char *, int,
                                            libvlc_exception_t *);

/**
 * Set the output for a media.
 *
 * \param p_instance the instance
 * \param psz_name the media to work on
 * \param psz_output the output MRL (the parameter to the "sout" variable)
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_vlm_set_output( libvlc_instance_t *, char *, char*,
                                           libvlc_exception_t *);

/**
 * Set a media's input MRL. This will delete all existing inputs and
 * add the specified one.
 *
 * \param p_instance the instance
 * \param psz_name the media to work on
 * \param psz_input the input MRL
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_vlm_set_input( libvlc_instance_t *, char *, char*,
                                          libvlc_exception_t *);

/**
 * Add a media's input MRL. This will add the specified one.
 *
 * \param p_instance the instance
 * \param psz_name the media to work on
 * \param psz_input the input MRL
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_vlm_add_input( libvlc_instance_t *, char *, char *,
                                          libvlc_exception_t *p_exception );
/**
 * Set a media's loop status.
 *
 * \param p_instance the instance
 * \param psz_name the media to work on
 * \param b_loop the new status
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_vlm_set_loop( libvlc_instance_t *, char *, int,
                                         libvlc_exception_t *);

/**
 * Set a media's vod muxer.
 *
 * \param p_instance the instance
 * \param psz_name the media to work on
 * \param psz_mux the new muxer
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_vlm_set_mux( libvlc_instance_t *p_instance, char *psz_name,
                                        char *psz_mux, libvlc_exception_t *p_exception );

/**
 * Edit the parameters of a media. This will delete all existing inputs and
 * add the specified one.
 *
 * \param p_instance the instance
 * \param psz_name the name of the new broadcast
 * \param psz_input the input MRL
 * \param psz_output the output MRL (the parameter to the "sout" variable)
 * \param i_options number of additional options
 * \param ppsz_options additional options
 * \param b_enabled boolean for enabling the new broadcast
 * \param b_loop Should this broadcast be played in loop ?
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_vlm_change_media( libvlc_instance_t *, char *, char *, char* ,
                                             int, char **, int, int, libvlc_exception_t * );

/**
 * Play the named broadcast.
 *
 * \param p_instance the instance
 * \param psz_name the name of the broadcast
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_vlm_play_media ( libvlc_instance_t *, char *, libvlc_exception_t * );

/**
 * Stop the named broadcast.
 *
 * \param p_instance the instance
 * \param psz_name the name of the broadcast
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_vlm_stop_media ( libvlc_instance_t *, char *, libvlc_exception_t * );

/**
 * Pause the named broadcast.
 *
 * \param p_instance the instance
 * \param psz_name the name of the broadcast
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_vlm_pause_media( libvlc_instance_t *, char *, libvlc_exception_t * );

/**
 * Seek in the named broadcast.
 *
 * \param p_instance the instance
 * \param psz_name the name of the broadcast
 * \param f_percentage the percentage to seek to
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_vlm_seek_media( libvlc_instance_t *, char *,
                                           float, libvlc_exception_t * );

/**
 * Return information about the named broadcast.
 *
 * \param p_instance the instance
 * \param psz_name the name of the broadcast
 * \param p_e an initialized exception pointer
 * \return string with information about named media
 */
VLC_PUBLIC_API char* libvlc_vlm_show_media( libvlc_instance_t *, char *, libvlc_exception_t * );

/**
 * Get information about media attribute from vlm.
 *
 * \param libvlc instance
 * \param type of information
 * \param default value
 * \return value of media attribute
 */
#define LIBVLC_VLM_GET_MEDIA_ATTRIBUTE( attr, returnType, getType, default)\
returnType libvlc_vlm_get_media_instance_## attr( libvlc_instance_t *, \
                        char *, int , libvlc_exception_t * );

VLC_PUBLIC_API LIBVLC_VLM_GET_MEDIA_ATTRIBUTE( position, float, Float, -1);
VLC_PUBLIC_API LIBVLC_VLM_GET_MEDIA_ATTRIBUTE( time, int, Integer, -1);
VLC_PUBLIC_API LIBVLC_VLM_GET_MEDIA_ATTRIBUTE( length, int, Integer, -1);
VLC_PUBLIC_API LIBVLC_VLM_GET_MEDIA_ATTRIBUTE( rate, int, Integer, -1);
VLC_PUBLIC_API LIBVLC_VLM_GET_MEDIA_ATTRIBUTE( title, int, Integer, 0);
VLC_PUBLIC_API LIBVLC_VLM_GET_MEDIA_ATTRIBUTE( chapter, int, Integer, 0);
VLC_PUBLIC_API LIBVLC_VLM_GET_MEDIA_ATTRIBUTE( seekable, int, Bool, 0);

#undef LIBVLC_VLM_GET_MEDIA_ATTRIBUTE

/** @} */

# ifdef __cplusplus
}
# endif

#endif /* <vlc/libvlc_vlm.h> */
