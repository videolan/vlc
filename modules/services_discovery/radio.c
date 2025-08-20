/*****************************************************************************
 * radio.c:  Online Radio Browser services discovery module
 *****************************************************************************
 * Copyright (C) 2025 the VideoLAN team
 *
 * Authors: Zyad M. Ayad <zyad.moh.1011@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_services_discovery.h>
#include <vlc_stream.h>
#include <vlc_access.h>
#include <string.h>
#include <vlc_interrupt.h>
#include <ctype.h>

#define DEFAULT_BASE_URL "https://all.api.radio-browser.info/" // Default Base URL for the Radio Browser API
#define MRL_PREFIX "radio://"
#define CSV_MAX_FIELDS 100

typedef struct
{
    vlc_thread_t thread;
    vlc_interrupt_t *interrupt;

} services_discovery_sys_t;

typedef struct
{
    char *base_url;
} access_sys_t;

typedef struct
{
    stream_t *stream;
    char *header_line;
    char **header_fields;
    size_t fields_count;
    char *line;
    char **fields;
} csv_parser;

/*
    TODO: Handle CSV Fields that has '\n' inside quoted fields
    Currently this module skips stations with such fields
    Reason: we are using vlc_stream_ReadLine which reads until '\n'
*/
static int parse_csv_line(char *line, char **fields, size_t max_fields)
{
    size_t field_count = 0;
    char *p = line;
    char *start = NULL;

    enum
    {
        STATE_START,
        STATE_IN_FIELD,
        STATE_IN_QUOTED_FIELD
    } state = STATE_START;

    while (p && *p)
    {
        if (field_count >= max_fields)
            return -1; // Exceeded max fields

        switch (state)
        {
        case STATE_START:
            if (*p == '"')
            {
                state = STATE_IN_QUOTED_FIELD;
                start = ++p; // Skip the opening quote
            }
            else if (*p == ',')
            {
                fields[field_count++] = p;
                *p++ = '\0';
            }
            else
            {
                state = STATE_IN_FIELD;
                start = p;
            }
            break;

        case STATE_IN_FIELD:
        {
            char *comma = strchr(p, ',');
            if (comma)
            {
                *comma = '\0';
                fields[field_count++] = start;
                p = comma + 1;
                state = STATE_START;
            }
            else
            {
                fields[field_count++] = start;
                p = NULL;
            }
            break;
        }

        case STATE_IN_QUOTED_FIELD:
        {
            char *quote = strchr(p, '"');
            if (!quote)
                return -1;

            if (*(quote + 1) == '"')
            {
                // Escaped quote
                memmove(quote, quote + 1, strlen(quote));
                p = quote + 1;
            }
            else
            {
                *quote = '\0';
                fields[field_count++] = start;
                p = quote + 1;
                state = STATE_START;

                if (*p == ',')
                {
                    p++; // Move past comma
                }
                else if (*p == '\0')
                {
                    p = NULL; // End of input
                }
            }
            break;
        }

        default:
            break;
        }
    }

    return (int)field_count;
}

static int csv_parser_read_line(csv_parser *parser)
{

    free(parser->line);
    parser->line = NULL;

    /* read next line */
    parser->line = vlc_stream_ReadLine(parser->stream);
    if (!parser->line)
    {
        return 0; // End of stream
    }

    /* parse the line into fields */
    int field_count = parse_csv_line(parser->line, parser->fields, parser->fields_count);
    if (field_count < 0 || (size_t)field_count != parser->fields_count)
    {
        return -1;
    }

    return 1;
}

static void csv_parser_free(csv_parser *parser)
{
    if (parser->stream)
    {
        vlc_stream_Delete(parser->stream);
    }
    free(parser->header_line);
    free(parser->line);
    free(parser->fields);
    free(parser->header_fields);
    free(parser);
}

static int csv_parser_get_field_index(csv_parser *parser, const char *field_name)
{
    if (!parser || !field_name || !parser->header_fields)
    {
        return -1; // Invalid parser or field name
    }

    for (size_t i = 0; i < parser->fields_count; i++)
    {
        if (strcmp(parser->header_fields[i], field_name) == 0)
        {
            return i; // Return the field value
        }
    }

    return -1; // Field not found
}

static csv_parser *csv_parser_init(void *data, char *psz_url, int max_fields)
{

    services_discovery_t *p_sd = (services_discovery_t *)data;

    csv_parser *parser = malloc(sizeof(csv_parser));
    if (!parser)
    {
        return NULL;
    }

    parser->stream = vlc_stream_NewURL(p_sd, psz_url);
    if (!parser->stream)
    {
        free(parser);
        return NULL;
    }

    parser->header_line = NULL;
    parser->fields_count = max_fields;
    parser->line = NULL;
    parser->fields = malloc(max_fields * sizeof(char *));
    if (!parser->fields)
    {
        vlc_stream_Delete(parser->stream);
        free(parser);
        return NULL;
    }
    parser->header_fields = malloc(max_fields * sizeof(char *));
    if (!parser->header_fields)
    {
        free(parser->fields);
        vlc_stream_Delete(parser->stream);
        free(parser);
        return NULL;
    }

    parser->header_line = vlc_stream_ReadLine(parser->stream);
    if (!parser->header_line)
    {
        free(parser->fields);
        free(parser->header_fields);
        vlc_stream_Delete(parser->stream);
        free(parser);
        return NULL;
    }

    int header_fields_count = parse_csv_line(parser->header_line, parser->header_fields, max_fields);
    if (header_fields_count < 0)
    {
        free(parser->fields);
        free(parser->header_line);
        free(parser->header_fields);
        vlc_stream_Delete(parser->stream);
        free(parser);
        return NULL;
    }

    parser->fields_count = header_fields_count;

    return parser;
}

static void *Run(void *data)
{

    vlc_thread_set_name("vlc-radio");

    services_discovery_t *p_sd = (services_discovery_t *)data;
    services_discovery_sys_t *p_sys = p_sd->p_sys;

    char *base_url = var_InheritString(p_sd, "radio-browser-baseurl");
    if (!base_url)
        return NULL;

    char *countries_endpoint;
    if (asprintf(&countries_endpoint, "%scsv/countries", base_url) < 0)
    {
        free(base_url);
        return NULL;
    }
    free(base_url);

    vlc_interrupt_set(p_sys->interrupt);

    csv_parser *parser = csv_parser_init(p_sd, countries_endpoint, CSV_MAX_FIELDS);
    free(countries_endpoint);
    if (!parser)
    {
        msg_Err(p_sd, "Failed to initialize CSV parser for countries");
        return NULL;
    }
    int name_index = csv_parser_get_field_index(parser, "name");
    int iso_3166_1_index = csv_parser_get_field_index(parser, "iso_3166_1");

    if (name_index < 0 || iso_3166_1_index < 0)
    {
        msg_Err(p_sd, "Missing required fields in country data");
        csv_parser_free(parser);
        return NULL;
    }

    int ret;
    while ((ret = csv_parser_read_line(parser)))
    {
        if (vlc_killed())
            break;
        if (ret < 0)
        {
            continue;
        }

        if (strlen(parser->fields[name_index]) == 0 || strlen(parser->fields[iso_3166_1_index]) != 2)
        {
            continue;
        }

        char *mrl;
        if (asprintf(&mrl, MRL_PREFIX "%s", parser->fields[iso_3166_1_index]) < 0)
        {
            continue;
        }

        input_item_t *country_node = input_item_NewDirectory(mrl, parser->fields[name_index], ITEM_NET);
        free(mrl);
        if (!country_node)
        {
            continue;
        }

        char flag[256];
        snprintf(flag, sizeof(flag), "https://flagsapi.com/%s/flat/64.png", parser->fields[iso_3166_1_index]);
        input_item_SetMeta(country_node, vlc_meta_ArtworkURL, flag);

        services_discovery_AddItem(p_sd, country_node);
    }
    csv_parser_free(parser);
    return NULL;
}

static int Open(vlc_object_t *p_this)
{
    services_discovery_t *p_sd = (services_discovery_t *)p_this;
    services_discovery_sys_t *p_sys = malloc(sizeof(*p_sys));
    if (!p_sys)
        return VLC_ENOMEM;

    p_sd->p_sys = p_sys;
    p_sys->interrupt = vlc_interrupt_create();
    if (!p_sys->interrupt)
    {
        free(p_sys);
        return VLC_ENOMEM;
    }

    p_sd->description = _("Radio");

    if (vlc_clone(&p_sys->thread, Run, p_sd))
    {
        vlc_interrupt_destroy(p_sys->interrupt);
        free(p_sys);
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static void Close(vlc_object_t *p_this)
{
    services_discovery_t *p_sd = (services_discovery_t *)p_this;
    services_discovery_sys_t *p_sys = p_sd->p_sys;

    vlc_interrupt_kill(p_sys->interrupt);
    vlc_join(p_sys->thread, NULL);
    vlc_interrupt_destroy(p_sys->interrupt);
    free(p_sys);
}

static int ReadDirectory(stream_t *p_access, input_item_node_t *p_node)
{
    access_sys_t *p_sys = p_access->p_sys;

    if (!p_sys || !p_sys->base_url)
    {
        return VLC_EGENERIC;
    }

    // Make sure the location is TWO characters, because empty countrycode will cause this moudle to load "ALL" stations
    if (strlen(p_access->psz_location) != 2)
    {
        return VLC_EGENERIC;
    }

    char *stations_endpoint;
    if (asprintf(&stations_endpoint, "%scsv/stations/bycountrycodeexact/%s?hidebroken=true&order=random",
                 p_sys->base_url, p_access->psz_location) < 0)
    {
        return VLC_ENOMEM;
    }

    csv_parser *parser = csv_parser_init(p_access, stations_endpoint, CSV_MAX_FIELDS);
    free(stations_endpoint);

    if (!parser)
    {
        return VLC_EGENERIC;
    }

    int name_index = csv_parser_get_field_index(parser, "name");
    int url_index = csv_parser_get_field_index(parser, "url");
    int favicon_index = csv_parser_get_field_index(parser, "favicon");
    if (name_index < 0 || url_index < 0)
    {
        msg_Err(p_access, "Missing required fields in station data");
        csv_parser_free(parser);
        return VLC_EGENERIC;
    }

    int ret;
    while ((ret = csv_parser_read_line(parser)))
    {
        if(vlc_killed())
            break;
        if (ret < 0)
        {
            continue;
        }

        if (strlen(parser->fields[name_index]) == 0 || strlen(parser->fields[url_index]) == 0)
        {
            continue;
        }

        input_item_t *station_item = input_item_New(parser->fields[url_index], parser->fields[name_index]);
        if (!station_item)
        {
            continue;
        }

        if (favicon_index >= 0 && strlen(parser->fields[favicon_index]) > 0)
        {
            input_item_SetMeta(station_item, vlc_meta_ArtworkURL, parser->fields[favicon_index]);
        }
        input_item_node_AppendItem(p_node, station_item);
        input_item_Release(station_item);
    }
    csv_parser_free(parser);
    return VLC_SUCCESS;
}

static int OpenAccess(vlc_object_t *p_this)
{
    stream_t *p_access = (stream_t *)p_this;
    access_sys_t *p_sys = malloc(sizeof(*p_sys));
    if (!p_sys)
        return VLC_ENOMEM;

    p_sys->base_url = var_InheritString(p_access, "radio-browser-baseurl");
    if (!p_sys->base_url)
    {
        free(p_sys);
        return VLC_EGENERIC;
    }

    p_access->p_sys = p_sys;
    p_access->pf_readdir = ReadDirectory;
    p_access->pf_control = access_vaDirectoryControlHelper;

    return VLC_SUCCESS;
}

static void CloseAccess(vlc_object_t *p_this)
{
    stream_t *p_access = (stream_t *)p_this;
    access_sys_t *p_sys = p_access->p_sys;

    free(p_sys->base_url);
    free(p_sys);
}

VLC_SD_PROBE_HELPER("radio", N_("Radio Browser"), SD_CAT_INTERNET)

#define BASE_URL_TEXT N_("Radio Browser Api Base URL")
#define BASE_URL_LONGTEXT N_("Enter the base URL for the Radio Browser API. The default is " DEFAULT_BASE_URL)

vlc_module_begin()
    set_shortname("Radio")
    set_description(N_("Radio Browser services discovery"))
    set_subcategory(SUBCAT_PLAYLIST_SD)
    set_capability("services_discovery", 0)
    set_callbacks(Open, Close)
    add_shortcut("radio")
    add_string("radio-browser-baseurl", DEFAULT_BASE_URL, BASE_URL_TEXT,
    BASE_URL_LONGTEXT)

    add_submodule()
    set_description(N_("Radio Browser access"))
    set_subcategory(SUBCAT_INPUT_ACCESS)
    set_capability("access", 0)
    set_callbacks(OpenAccess, CloseAccess)
    add_shortcut("radio")

    VLC_SD_PROBE_SUBMODULE
    vlc_module_end()
