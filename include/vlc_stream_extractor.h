/*****************************************************************************
 * vlc_stream_extractor.h
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
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

#ifndef VLC_STREAM_EXTRACTOR_H
#define VLC_STREAM_EXTRACTOR_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup stream_extractor Stream Extractor
 * \ingroup input
 *
 * If a stream can be viewed as a directory, such as when opening a
 * compressed archive, a \em stream-extractor is used to get access to
 * the entities inside said stream.
 *
 * A \em stream-extractor can do one of two things;
 *
 *  - either it lists the logical entries within a stream, or;
 *  - it extracts the data associated with one of those entries based
 *    on a unique identifier.
 *
 * @{
 *
 **/
struct stream_extractor_t {
    VLC_COMMON_MEMBERS;

    union {
        /**
         * Callbacks for entity extraction
         *
         * The following callbacks shall be populated if the stream_extractor is
         * used to extract a specific entity from the source-stream. Each
         * callback shall behave as those, with the same name, specified in \ref
         * stream_t.
         *
         **/
        struct {
            ssize_t  (*pf_read)(struct stream_extractor_t *, void *buf, size_t len);
            block_t* (*pf_block)(struct stream_extractor_t *, bool *eof);
            int      (*pf_seek)(struct stream_extractor_t *, uint64_t);
            int      (*pf_control)(struct stream_extractor_t *, int i_query, va_list);

        } stream;

        /**
         * Callbacks for stream directory listing
         *
         * These callbacks are used when a stream is to be treated as a
         * directory, it shall behave as those, with the same name, specified
         * in \ref stream_t.
         *
         **/
        struct {
            int (*pf_readdir)(struct stream_extractor_t *, input_item_node_t *);

        } directory;

    };

    void* p_sys; /**< Private data pointer */
    stream_t* source; /**< The source stream */
    char* identifier; /**< name of requested entity to extract, or NULL
                       **  when requested to list directories */
};

typedef struct stream_extractor_t stream_extractor_t;

/**
 * Create a relative MRL for the associated entity
 *
 * This function shall be used by stream_extractor_t's in order to
 * generate a MRL that refers to an entity within the stream. Normally
 * this function will only be invoked within `pf_readdir` in order to
 * get the virtual path of the listed items.
 *
 * \warning the returned value is to be freed by the caller
 *
 * \param extractor the stream_extractor_t in which the entity belongs
 * \param subentry the name of the entity in question
 *
 * \return a pointer to the resulting MRL on success, NULL on failure
 **/
VLC_API char* vlc_stream_extractor_CreateMRL( stream_extractor_t*,
                                              char const* subentry );

/**
 * Construct a new stream_extractor-based stream
 *
 * This function is used to attach a stream extractor to an already
 * existing stream.
 *
 * If \p identifier is `NULL`, `*stream` is guaranteed to refer to a
 * directory, otherwise \p identifier denotes the specific subentry
 * that one would like to access within the stream.
 *
 * If \p identifier is not NULL, `*stream` will refer to data for the
 * entity in question.
 *
 * \param[out] stream a pointer-to-pointer to stream, `*stream` will
 *             refer to the attached stream on success, and left
 *             untouched on failure.
 * \param identifier NULL or a c-style string referring to the desired entity
 * \param module_name NULL or an explicit stream-extractor module name
 *
 * \return VLC_SUCCESS if a stream-extractor was successfully
 *         attached, an error-code on failure.
 **/

VLC_API int vlc_stream_extractor_Attach( stream_t** source,
                                         char const* identifier,
                                         char const* module_name );
/**
 * @}
 */

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* include-guard */
