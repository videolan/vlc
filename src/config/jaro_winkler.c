/*****************************************************************************
 * jaro_winkler.c: jaro winkler string similarity algorithm implementation
 *****************************************************************************
 * Copyright 2015 Danny Guo
 * Copyright 2018 Lyndon Brown
 *
 * Authors: Danny Guo <dguo@users.noreply.github.com>
 *          Lyndon Brown <jnqnfe@gmail.com>
 *
 * Licensed under the MIT license. You may not copy, modify, or distribute this
 * file except in compliance with said license. You can find a copy of this
 * license either in the LICENSE file, or alternatively at
 * <http://opensource.org/licenses/MIT>.
 *****************************************************************************
 * This file is based upon the Jaro Winkler implementation of the `strsim`
 * Rust crate, authored by Danny Guo, available at
 * <https://github.com/dguo/strsim-rs>; more specifically the (as yet un-merged)
 * optimised copy authored by myself (Lyndon Brown), available at
 * <https://github.com/dguo/strsim-rs/pull/31>. The code is available under the
 * MIT license.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <sys/types.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

#include "vlc_jaro_winkler.h"

#define MAX(a, b) ( ((a) > (b)) ? (a) : (b) )
#define MIN(a, b) ( ((a) < (b)) ? (a) : (b) )

/**
 * Checks both strings for a common prefix, returning the number of matching
 * bytes.
 */
static inline size_t split_on_common_prefix(const char *a, const char *b) {
    size_t len = 0;
    while (*(a) && *(b) && *(a++) == *(b++)) len++;
    return len;
}

/**
 * This is the inner Jaro algorithm, with a parameter for passing back the
 * length of the prefix common to both strings, used for efficiency of the
 * Jaro-Winkler implementation.
 */
static inline int jaro_inner(const char *a, const char *b, size_t *ret_prefix_cc, float* res) {
    assert(a && b && ret_prefix_cc && res);

    if ((a[0] == '\0') ^ (b[0] == '\0')) {
        *res = 0.0;
        return 0;
    }

    size_t prefix_char_count = split_on_common_prefix(a, b);
    const char *a_suffix = a + prefix_char_count;
    const char *b_suffix = b + prefix_char_count;

    if (a_suffix[0] == '\0' && b_suffix[0] == '\0') {
        *res = 1.0;
        return 0;
    }

    *ret_prefix_cc = prefix_char_count;

    size_t a_numchars = strlen(a_suffix) + prefix_char_count;
    size_t b_suffix_len = strlen(b_suffix);
    size_t b_numchars = b_suffix_len + prefix_char_count;

    // The check for lengths of one here is to prevent integer overflow when
    // calculating the search range.
    if (a_numchars == 1 && b_numchars == 1) {
        *res = 0.0;
        return 0;
    }

    size_t search_range = (MAX(a_numchars, b_numchars) / 2) - 1;

    /* catch overflow */
    assert(a_numchars <= INT_MAX);
    assert(search_range <= INT_MAX);

    bool *b_consumed = calloc(b_numchars, sizeof(*b_consumed));
    if (!b_consumed) {
        *res = 0.0;
        return -1;
    }

    size_t matches = prefix_char_count;
    size_t transpositions = 0;
    size_t b_match_index = 0;

    const char *a_char = a_suffix;
    for (size_t i = 0; *a_char; i++) {
        size_t bound_start = i > search_range ? i - search_range : 0;
        size_t bound_end = MIN(b_numchars, i + search_range + 1);

        if (bound_start >= bound_end) {
            a_char++;
            continue;
        }

        if (bound_start > b_suffix_len) {
            // end of b string
            break;
        }

        const char *b_char = b_suffix + bound_start;
        for (size_t j = bound_start; *b_char && j < bound_end; j++) {
            if (*a_char == *b_char && !b_consumed[j]) {
                b_consumed[j] = true;
                matches++;

                if (j < b_match_index) {
                    transpositions++;
                }
                b_match_index = j;

                break;
            }
            b_char++;
        }
        a_char++;
    }
    free(b_consumed);

    if (matches == 0) {
        *res = 0.0;
        return 0;
    }

    *res = (1.0 / 3.0) *
           (((float)matches / (float)a_numchars) +
            ((float)matches / (float)b_numchars) +
            (((float)matches - (float)transpositions) / (float)matches));
    return 0;
}

int vlc_jaro_winkler(const char *a, const char *b, float* res) {
    size_t prefix_char_count = 0;
    float jaro_distance;
    if (jaro_inner(a, b, &prefix_char_count, &jaro_distance) != 0) {
        return -1;
    }

    float jaro_winkler_distance =
        jaro_distance + (0.1 * (float)prefix_char_count * (1.0 - jaro_distance));

    *res = (jaro_winkler_distance <= 1.0) ? jaro_winkler_distance : 1.0;
    return 0;
}
