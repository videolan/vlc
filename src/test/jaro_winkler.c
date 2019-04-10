/*****************************************************************************
 * jaro_winkler.c: Tests for our Jaro Winkler algorithm
 *****************************************************************************
 * Copyright 2015 Danny Guo
 * Copyright 2018 Lyndon Brown
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h> /* fabs() */

#include <vlc_common.h>
#include <vlc_strings.h>

#include "../config/vlc_jaro_winkler.h"

const char vlc_module_name[] = "test_jarowinkler";

# define test1( expected, a, b ) \
    assert(vlc_jaro_winkler(a, b, &actual) == 0); \
    failed = (actual != expected); \
    problems |= failed; \
    printf("[TEST] expected: %f, actual: %f, accuracy: n/a, result: %s, (a: %s), (b: %s)\n", \
        expected, actual, (failed) ? "FAIL" : "pass", a, b);

# define test2( expected, a, b, accuracy ) \
    assert(vlc_jaro_winkler(a, b, &actual) == 0); \
    failed = (fabs(expected - actual) >= accuracy); \
    problems |= failed; \
    printf("[TEST] expected: %f, actual: %f, accuracy: %f, result: %s, (a: %s), (b: %s)\n", \
        expected, actual, accuracy, (failed) ? "FAIL": "pass", a, b);

int main( void )
{
    bool problems = false, failed = false;
    float actual;

    // both_empty
    test1(1.0, "", "");

    // first_empty
    test1(0.0, "", "jaro-winkler");

    // second_empty
    test1(0.0, "distance", "");

    // same
    test1(1.0, "Jaro-Winkler", "Jaro-Winkler");

    // diff_short
    test2(0.813, "dixon", "dicksonx", 0.001);
    test2(0.813, "dicksonx", "dixon", 0.001);

    // same_one_character
    test1(1.0, "a", "a");

    // diff_one_character
    test1(0.0, "a", "b");

    // diff_no_transposition
    test2(0.840, "dwayne", "duane", 0.001);

    // diff_with_transposition
    test2(0.961, "martha", "marhta", 0.001);

    // names
    test2(0.562, "Friedrich Nietzsche", "Fran-Paul Sartre", 0.001);

    // long_prefix
    test2(0.911, "cheeseburger", "cheese fries", 0.001);

    // more_names
    test2(0.868, "Thorkel", "Thorgier", 0.001);

    // length_of_one
    test2(0.738, "Dinsdale", "D", 0.001);

    // very_long_prefix
    test2(1.0, "thequickbrownfoxjumpedoverx", "thequickbrownfoxjumpedovery", 0.001);

    return (problems) ? -1 : 0;
}
