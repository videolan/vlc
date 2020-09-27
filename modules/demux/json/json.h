/*****************************************************************************
 * json/json.h:
 *****************************************************************************
 * Copyright © 2020 Rémi Denis-Courmont
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

#ifndef JSON_H
#define JSON_H

#include <stdbool.h>
#include <stdio.h>

enum json_type {
    JSON_NULL,
    JSON_BOOLEAN,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT,
};

struct json_array {
    size_t size;
    struct json_value *entries;
};

struct json_object {
    size_t count;
    struct json_member *members;
};

struct json_value {
    enum json_type type;
    union {
        bool boolean;
        double number;
        char *string;
        struct json_array array;
        struct json_object object;
    };
};

struct json_member {
    char *name;
    struct json_value value;
};

void json_parse_error(void *log, const char *msg);
char *json_unescape(const char *, size_t);

int json_parse(void *log, FILE *in, struct json_object *result);
void json_free(struct json_object *);

const struct json_value *json_get(const struct json_object *obj,
                                  const char *name);
const char *json_get_str(const struct json_object *obj, const char *name);
double json_get_num(const struct json_object *obj, const char *name);

#endif
