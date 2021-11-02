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
 *  - lists the logical entries within a stream:
 *    - type = \ref stream_directory_t
 *    - capability = "stream_directory"
 *
 *  - extract data associated with one specific entry within a stream:
 *    - type = \ref stream_extractor_t
 *    - capability = "stream_extractor"
 *
 * @{
 *
 **/

typedef struct stream_extractor_t {
    struct vlc_object_t obj;

    /**
     * \name Callbacks for entity extraction
     *
     * The following members shall be populated as specified by the
     * documentation associated with \ref stream_t for the equivalent name.
     *
     * @{
     **/
    ssize_t  (*pf_read)(struct stream_extractor_t*, void* buf, size_t len);
    block_t* (*pf_block)(struct stream_extractor_t*, bool* eof);
    int      (*pf_seek)(struct stream_extractor_t*, uint64_t);
    int      (*pf_control)(struct stream_extractor_t*, int request, va_list args);
    /** @} */

    char const* identifier; /**< the name of the entity to be extracted */
    stream_t* source; /**< the source stream to be consumed */
    void* p_sys;      /**< private opaque handle to be used by the module */

} stream_extractor_t;

typedef struct stream_directory_t {
    struct vlc_object_t obj;

    /**
     * \name Callbacks for stream directories
     *
     * The following members shall be populated as specified by the
     * documentation associated with \ref stream_t for the equivalent name.
     *
     * @{
     **/
    int (*pf_readdir)(struct stream_directory_t*, input_item_node_t* );
    /** @} */

    stream_t* source; /**< the source stream to be consumed */
    void* p_sys; /**< private opaque handle to be used by the module */

} stream_directory_t;

/**
 * Create a stream for the data referred to by a \ref mrl
 *
 * This function will create a \ref stream that reads from the specified \ref
 * mrl, potentially making use of \ref stream_extractor%s to access named
 * entities within the data read from the original source.
 *
 *  - See the \ref mrl specification for further information.
 *  - The returned resource shall be deleted through \ref vlc_stream_Delete.
 *
 * \warning This function is only to be used when \ref mrl functionality is
 *          explicitly needed. \ref vlc_stream_NewURL shall be used where
 *          applicable.
 *
 * \param obj the owner of the requested stream
 * \param mrl the mrl for which the stream_t should be created
 * \return `NULL` on error, a pointer to \ref stream_t on success.
 **/
VLC_API stream_t * vlc_stream_NewMRL(vlc_object_t *obj, const char *mrl)
VLC_USED;
#define vlc_stream_NewMRL(a, b) vlc_stream_NewMRL(VLC_OBJECT(a), b)

/**
 * Create a relative MRL for the associated entity
 *
 * This function shall be used by stream_directory_t's in order to
 * generate an MRL that refers to an entity within the stream. Normally
 * this function will only be invoked within `pf_readdir` in order to
 * get the virtual path of the listed items.
 *
 * \warning The returned value is to be freed by the caller
 *
 * \param extractor the stream_directory_t for which the entity belongs
 * \param subentry the name of the entity in question
 *
 * \return a pointer to the resulting MRL on success, NULL on failure
 **/
VLC_API char* vlc_stream_extractor_CreateMRL( stream_directory_t *extractor,
                                              char const* subentry );

/**
 * \name Attach a stream-extractor to the passed stream
 *
 * These functions are used to attach a stream extractor to an already existing
 * stream. As hinted by their names, \ref vlc_stream_extractor_Attach will
 * attach an \em entity-extractor, whereas \ref vlc_stream_directory_Attach
 * will attach a \em stream-directory.
 *
 * \param[out] stream a pointer-to-pointer to stream, `*stream` will
 *             refer to the attached stream on success, and left
 *             untouched on failure.
 * \param identifier (if present) NULL or a c-style string referring to the
 *                   desired entity
 * \param module_name NULL or an explicit stream-extractor module name
 *
 * \return VLC_SUCCESS if a stream-extractor was successfully
 *         attached, an error-code on failure.
 *
 * @{
 **/

VLC_API int vlc_stream_extractor_Attach( stream_t** source,
                                         char const* identifier,
                                         char const* module_name );

VLC_API int vlc_stream_directory_Attach( stream_t** source,
                                         char const* module_name );
/**
 * @}
 */

/**
 * @}
 */

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* include-guard */
