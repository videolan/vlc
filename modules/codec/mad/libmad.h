/***************************************************************************
           mad_libmad.h  -  description
               -------------------
    begin                : Mon Nov 5 2001
    copyright            : (C) 2001 by Jean-Paul Saman
    email                : jpsaman@wxs.nl
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef _VLC_MAD_LIBMAD_H_
#define _VLC_MAD_LIBMAD_H_

/*
 * Function prototypes for libmad callback functions.
 */

/*
 * Each of the following functions will return one of:
 * MAD_FLOW_CONTINUE = continue normally
 * MAD_FLOW_STOP     = stop decoding normally
 * MAD_FLOW_BREAK    = stop decoding and signal an error
 * MAD_FLOW_IGNORE   = ignore the current frame
 */

/* enum mad_flow (*input_func)(void *, struct mad_stream *);*/
enum mad_flow libmad_input(void *data, struct mad_stream *p_libmad_stream);

/* enum mad_flow (*header_func)(void *, struct mad_header const *);*/
enum mad_flow libmad_header(void *data, struct mad_header const *p_libmad_header);

/* enum mad_flow (*filter_func)(void *, struct mad_stream const *, struct mad_frame *); */
// enum mad_flow libmad_filter(void *data, struct mad_stream const *p_libmad_stream, struct mad_frame *p_libmad_frame);

/* enum mad_flow (*output_func)(void *, struct mad_header const *, struct mad_pcm *); */
//enum mad_flow libmad_output(void *data, struct mad_header const *p_libmad_header, struct mad_pcm *p_libmad_pcm);
enum mad_flow libmad_output3(void *data, struct mad_header const *p_libmad_header, struct mad_pcm *p_libmad_pcm);

/* enum mad_flow (*error_func)(void *, struct mad_stream *, struct mad_frame *); */
enum mad_flow libmad_error(void *data, struct mad_stream *p_libmad_stream, struct mad_frame *p_libmad_frame);

/* enum mad_flow (*message_func)(void *, void *, unsigned int *); */
/* enum mad_flow libmad_message(void *, void*, unsigned int*); */

#endif
