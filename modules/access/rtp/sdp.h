/**
 * @file sdp.h
 * @brief Session Description Protocol (SDP)
 * @ingroup sdp
 */
/*****************************************************************************
 * Copyright © 2020 Rémi Denis-Courmont
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 ****************************************************************************/

#ifndef VLC_SDP_H
#define VLC_SDP_H

#include <stdbool.h>

/**
 * \defgroup sdp Session Description Protocol
 * \ingroup net
 * @{
 */

struct vlc_sdp;
struct vlc_sdp_media;
struct vlc_sdp_conn;
struct vlc_sdp_attr;

/**
 * Parses an SDP session descriptor.
 *
 * \param str start address of the descriptor
 * \param length bytes length of the descriptor
 * \return a parsed SDP or NULL on error (@c errno is set)
 */
struct vlc_sdp *vlc_sdp_parse(const char *str, size_t length);

/**
 * Destroys a parsed SDP session descriptor.
 */
void vlc_sdp_free(struct vlc_sdp *sdp);

const struct vlc_sdp_attr *vlc_sdp_attr_first_by_name(
    struct vlc_sdp_attr *const *ap, const char *name);

/** SDP attribute */
struct vlc_sdp_attr
{
    struct vlc_sdp_attr *next; /*< Next attribute (or NULL) */
    const char *value; /*< Attribute value, or NULL if none */
    char name[]; /*< Attribute name */
};

/** SDP connection address */
struct vlc_sdp_conn
{
    struct vlc_sdp_conn *next; /*< Next address (or NULL) */
    int family; /*< Address family, or AF_UNSPEC if not recognized */
    unsigned char ttl; /*< Multicast TTL */
    unsigned short addr_count; /*< Multicast address count */
    char addr[]; /*< Address name, usually an IP literal */
};

/** SDP media */
struct vlc_sdp_media
{
    struct vlc_sdp_media *next; /*< Next media in the session (or NULL) */
    struct vlc_sdp *session; /*< Pointer to containing session */
    char *type; /*< Media type, e.g. "audio" or "video" */
    unsigned int port; /*< Media port number */
    unsigned int port_count; /*< Number of ports (usually 1) */
    char *proto; /*< Media protocol, e.g. "RTP/AVP" */
    char *format; /*< Protocol-specific format parameters */
    struct vlc_sdp_conn *conns; /*< List of media connection addresses */
    struct vlc_sdp_attr *attrs; /*< List of media attributes */
};

/**
 * Gets a media attribute by name.
 *
 * \param media Session media descriptor.
 * \param name Session attribute name.
 *
 * \note This function does <b>not</b> look for session attributes, as this is
 * not always appropriate.
 * To fallback to session attributes, call vlc_sdp_attr_get() explicitly.
 *
 * \return the first attribute with the specified name or NULL if not found.
 */
static inline
const struct vlc_sdp_attr *vlc_sdp_media_attr_get(
    const struct vlc_sdp_media *media, const char *name)
{
    return vlc_sdp_attr_first_by_name(&media->attrs, name);
}

/**
 * Checks if a median attribute is present.
 *
 * \param media Media descriptor.
 * \param name Attribute name.
 *
 * \retval true if present
 * \retval false it absent
 */ 
static inline
bool vlc_sdp_media_attr_present(const struct vlc_sdp_media *media,
                                const char *name)
{
    return vlc_sdp_media_attr_get(media, name) != NULL;
}

/**
 * Returns a media attribute value.
 *
 * \param media Media descriptor.
 * \param name Attribute name.
 *
 * \note This function cannot distinguish the cases of a missing attribute and
 * of an attribute without a value.
 * Use vlc_sdp_media_attr_present() to check for value-less attributes.
 *
 * \return Nul-terminated attribute value, or NULL if none.
 */
static inline
const char *vlc_sdp_media_attr_value(const struct vlc_sdp_media *media,
                                     const char *name)
{
    const struct vlc_sdp_attr *a = vlc_sdp_media_attr_get(media, name);
    return (a != NULL) ? a->value : NULL;
}

/** SDP session descriptor */
struct vlc_sdp
{
    char *name; /*< Session name */
    char *info; /*< Session description, or NULL if none */
    struct vlc_sdp_conn *conn; /*< Session connection address or NULL */
    struct vlc_sdp_attr *attrs; /*< List of session attributes */
    struct vlc_sdp_media *media; /*< List of session media */
};

/**
 * Returns the media connection address list.
 */
static inline
const struct vlc_sdp_conn *vlc_sdp_media_conn(
    const struct vlc_sdp_media *media)
{
    return (media->conns != NULL) ? media->conns : media->session->conn;
}

/**
 * Gets a session attribute by name.
 *
 * \param sdp Session descriptor.
 * \param name Attribute name.
 *
 * \return the first attribute with the specified name or NULL if not found.
 */
static inline
const struct vlc_sdp_attr *vlc_sdp_attr_get(const struct vlc_sdp *sdp,
                                            const char *name)
{
    return vlc_sdp_attr_first_by_name(&sdp->attrs, name);
}

/**
 * Checks if a session attribute is present.
 *
 * \param sdp Session descriptor.
 * \param name Attribute name.
 *
 * \retval true if present
 * \retval false it absent
 */
static inline
bool vlc_sdp_attr_present(const struct vlc_sdp *sdp, const char *name)
{
    return vlc_sdp_attr_get(sdp, name) != NULL;
}

/**
 * Returns a session attribute value.
 *
 * \param sdp Session descriptor.
 * \param name Attribute name.
 *
 * \note This function cannot distinguish the cases of a missing attribute and
 * of an attribute without a value.
 * Use vlc_sdp_attr_present() to check for value-less attributes.
 *
 * \return Nul-terminated attribute value, or NULL if none.
 */
static inline
const char *vlc_sdp_attr_value(const struct vlc_sdp *sdp, const char *name)
{
    const struct vlc_sdp_attr *a = vlc_sdp_attr_get(sdp, name);
    return (a != NULL) ? a->value : NULL;
}

/** @} */

#endif
