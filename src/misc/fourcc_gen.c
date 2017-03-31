/*****************************************************************************
 * fourcc_gen.c: FourCC preprocessor
 *****************************************************************************
 * Copyright © 2015 Rémi Denis-Courmont
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

/* DO NOT include "config.h" here */

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define VLC_API
#define VLC_USED
typedef uint32_t vlc_fourcc_t;
typedef struct { unsigned num, den; } vlc_rational_t;
#include "../include/vlc_fourcc.h"

#define VLC_FOURCC(a,b,c,d) { a, b, c, d }
#define A(sfcc) E(sfcc, NULL)
#define B(fcc,dsc) { true, fcc, dsc }
#define E(sfcc,dsc) { false, sfcc, dsc }

typedef struct
{
    bool klass;
    char fourcc[4];
    const char *description;
} staticentry_t;

#include "misc/fourcc_list.h"

struct entry
{
    char fourcc[4];
    char alias[4];
    const char *desc;
};

static int cmp_entry(const void *a, const void *b)
{
    const struct entry *ea = a, *eb = b;
    int d = memcmp(ea->alias, eb->alias, 4);
    if (d == 0)
        d = memcmp(ea->fourcc, eb->fourcc, 4);
    return d;
}

static void process_list(const char *name, const staticentry_t *list, size_t n)
{
    struct entry *entries = malloc(sizeof (*entries) * n);
    if (entries == NULL)
        abort();

    const staticentry_t *klass = NULL;

    for (size_t i = 0; i < n; i++)
    {
        if (list[i].klass)
            klass = &list[i];

        if (klass == NULL)
        {
            fprintf(stderr, "Error: FourCC \"%.4s\" not mapped!\n",
                    list[i].fourcc);
            exit(1);
        }

        memcpy(entries[i].fourcc, klass->fourcc, 4);
        memcpy(entries[i].alias, list[i].fourcc, 4);
        entries[i].desc = list[i].description;
    }

    qsort(entries, n, sizeof (*entries), cmp_entry);

    size_t dups = 0;
    for (size_t i = 1; i < n; i++)
        if (!memcmp(entries[i - 1].alias, entries[i].alias, 4)
         && memcmp(entries[i - 1].fourcc, entries[i].fourcc, 4))
        {
            fprintf(stderr, "Error: FourCC alias \"%.4s\" conflict: "
                    "\"%.4s\" and \"%.4s\"\n", entries[i].alias,
                    entries[i - 1].fourcc, entries[i].fourcc);
            dups++;
        }

    if (dups > 0)
        exit(1);

    printf("static const struct fourcc_mapping mapping_%s[] = {\n", name);
    for (size_t i = 0; i < n; i++)
    {
        if (!memcmp(entries[i].fourcc, entries[i].alias, 4))
            continue;
        printf("    { { { 0x%02hhx, 0x%02hhx, 0x%02hhx, 0x%02hhx } }, "
               "{ { 0x%02hhx, 0x%02hhx, 0x%02hhx, 0x%02hhx } } },\n",
               entries[i].alias[0], entries[i].alias[1], entries[i].alias[2],
               entries[i].alias[3], entries[i].fourcc[0], entries[i].fourcc[1],
               entries[i].fourcc[2], entries[i].fourcc[3]);
    }
    puts("};");
    printf("static const struct fourcc_desc desc_%s[] = {\n", name);
    for (size_t i = 0; i < n; i++)
    {
        if (entries[i].desc == NULL)
            continue;
        printf("    { { { 0x%02hhx, 0x%02hhx, 0x%02hhx, 0x%02hhx } }, "
               "\"%s\" },\n", entries[i].alias[0], entries[i].alias[1],
               entries[i].alias[2], entries[i].alias[3], entries[i].desc);
    }
    puts("};");

    free(entries);
    fprintf(stderr, "%s: %zu entries\n", name, n);
}

int main(void)
{
    puts("/* This file is generated automatically. DO NOT EDIT! */");
    puts("struct fourcc_mapping {");
    puts("    union { unsigned char alias_str[4]; vlc_fourcc_t alias; };");
    puts("    union { unsigned char fourcc_str[4]; vlc_fourcc_t fourcc; };");
    puts("};");
    puts("struct fourcc_desc {");
    puts("    union { unsigned char alias_str[4]; vlc_fourcc_t alias; };");
    puts("    const char desc[52];");
    puts("};");

#define p(t) \
    process_list(#t, p_list_##t, \
                 sizeof (p_list_##t) / sizeof ((p_list_##t)[0]))
    p(video);
    p(audio);
    p(spu);
    return 0;
}
