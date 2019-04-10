/*****************************************************************************
 * jaro_winkler.c: jaro winkler string similarity algorithm implementation
 *****************************************************************************
 * Copyright 2015 Danny Guo
 * Copyright 2018, 2019 Lyndon Brown
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

#ifndef VLC_JARO_WINKLER_H
#define VLC_JARO_WINKLER_H 1

/**
 * Calculate a “Jaro Winkler” metric.
 *
 * Algorithm: <http://en.wikipedia.org/wiki/Jaro%E2%80%93Winkler_distance>
 *
 * Like “Jaro” but gives a boost to strings that have a common prefix.
 *
 * \note: This implementation does not place a limit the common prefix length
 * adjusted for.
 *
 * \param a string A
 * \param b string B
 * \param res [OUT] a pointer to a float to receive the result
 * \return -1 on memory allocation failure, otherwise 0
 */
int vlc_jaro_winkler(const char *a, const char *b, float *res);

#endif
