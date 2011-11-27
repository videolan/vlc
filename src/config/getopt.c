/*****************************************************************************
 * getopt_long()
 *****************************************************************************
 * Copyright (C) 1987-1997 Free Software Foundation, Inc.
 * Copyright (C) 2005-2010 VLC authors and VideoLAN
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
#include <config.h>
#endif
#include <vlc_common.h>

#include <stdio.h>
#include <string.h>

#include "vlc_getopt.h"

/* Exchange two adjacent subsequences of ARGV.
   One subsequence is elements [first_nonopt,last_nonopt)
   which contains all the non-options that have been skipped so far.
   The other is elements [last_nonopt,optind), which contains all
   the options processed since those non-options were skipped.

   `first_nonopt' and `last_nonopt' are relocated so that they describe
   the new indices of the non-options in ARGV after they are moved.  */

static void exchange(char **argv, vlc_getopt_t *restrict state)
{
    int bottom = state->first_nonopt;
    int middle = state->last_nonopt;
    int top = state->ind;
    char *tem;

    /* Exchange the shorter segment with the far end of the longer segment.
       That puts the shorter segment into the right place.
       It leaves the longer segment in the right place overall,
       but it consists of two parts that need to be swapped next.  */

    while (top > middle && middle > bottom)
    {
        if (top - middle > middle - bottom)
        {
            /* Bottom segment is the short one.  */
            int len = middle - bottom;
            register int i;

            /* Swap it with the top part of the top segment.  */
            for (i = 0; i < len; i++)
            {
                tem = argv[bottom + i];
                argv[bottom + i] = argv[top - (middle - bottom) + i];
                argv[top - (middle - bottom) + i] = tem;
            }
            /* Exclude the moved bottom segment from further swapping.  */
            top -= len;
        }
        else
        {
            /* Top segment is the short one.  */
            int len = top - middle;
            register int i;

            /* Swap it with the bottom part of the bottom segment.  */
            for (i = 0; i < len; i++)
            {
                tem = argv[bottom + i];
                argv[bottom + i] = argv[middle + i];
                argv[middle + i] = tem;
            }
            /* Exclude the moved top segment from further swapping.  */
            bottom += len;
        }
    }

    /* Update records for the slots the non-options now occupy.  */

    state->first_nonopt += (state->ind - state->last_nonopt);
    state->last_nonopt = state->ind;
}


/* Scan elements of ARGV (whose length is ARGC) for option characters
   given in OPTSTRING.

   If an element of ARGV starts with '-', and is not exactly "-" or "--",
   then it is an option element.  The characters of this element
   (aside from the initial '-') are option characters.  If `getopt'
   is called repeatedly, it returns successively each of the option characters
   from each of the option elements.

   If `getopt' finds another option character, it returns that character,
   updating `optind' and `nextchar' so that the next call to `getopt' can
   resume the scan with the following option character or ARGV-element.

   If there are no more option characters, `getopt' returns -1.
   Then `optind' is the index in ARGV of the first ARGV-element
   that is not an option.  (The ARGV-elements have been permuted
   so that those that are not options now come last.)

   OPTSTRING is a string containing the legitimate option characters.
   If an option character is seen that is not listed in OPTSTRING,
   return '?'.

   If a char in OPTSTRING is followed by a colon, that means it wants an arg,
   so the following text in the same ARGV-element, or the text of the following
   ARGV-element, is returned in `optarg'.

   If OPTSTRING starts with `-' or `+', it requests different methods of
   handling the non-option ARGV-elements.
   See the comments about REQUIRE_ORDER, above.

   Long-named options begin with `--' instead of `-'.
   Their names may be abbreviated as long as the abbreviation is unique
   or is an exact match for some defined option.  If they have an
   argument, it follows the option name in the same ARGV-element, separated
   from the option name by a `=', or else the in next ARGV-element.
   When `getopt' finds a long-named option, it returns 0 if that option's
   `flag' field is nonzero, the value of the option's `val' field
   if the `flag' field is zero.

   The elements of ARGV aren't really const, because we permute them.
   But we pretend they're const in the prototype to be compatible
   with other systems.

   LONGOPTS is a vector of `struct option' terminated by an
   element containing a name which is zero.

   LONGIND returns the index in LONGOPT of the long-named option found.
   It is only valid when a long-named option has been found by the most
   recent call.  */

int vlc_getopt_long(int argc, char *const *argv,
                    const char *optstring,
                    const struct vlc_option *restrict longopts, int *longind,
                    vlc_getopt_t *restrict state)
{
    state->arg = NULL;

    if (state->ind == 0)
    {
        /* Initialize the internal data when the first call is made.  */
        /* Start processing options with ARGV-element 1 (since ARGV-element 0
           is the program name); the sequence of previously skipped
           non-option ARGV-elements is empty.  */
        state->first_nonopt = state->last_nonopt = state->ind = 1;
        state->nextchar = NULL;
    }

#define NONOPTION_P (argv[state->ind][0] != '-' || argv[state->ind][1] == '\0')

    if (state->nextchar == NULL || *state->nextchar == '\0')
    {
        /* Advance to the next ARGV-element.  */

        /* Give FIRST_NONOPT & LAST_NONOPT rational values if OPTIND has been
           moved back by the user (who may also have changed the arguments).  */
        if (state->last_nonopt > state->ind)
            state->last_nonopt = state->ind;
        if (state->first_nonopt > state->ind)
            state->first_nonopt = state->ind;

        /* If we have just processed some options following some non-options,
           exchange them so that the options come first.  */

        if (state->first_nonopt != state->last_nonopt
            && state->last_nonopt != state->ind)
            exchange((char **) argv, state);
        else if (state->last_nonopt != state->ind)
            state->first_nonopt = state->ind;

        /* Skip any additional non-options
           and extend the range of non-options previously skipped.  */

        while (state->ind < argc && NONOPTION_P)
            state->ind++;
        state->last_nonopt = state->ind;

        /* The special ARGV-element `--' means premature end of options.
           Skip it like a null option,
           then exchange with previous non-options as if it were an option,
           then skip everything else like a non-option.  */

        if (state->ind != argc && !strcmp(argv[state->ind], "--"))
        {
            state->ind++;

            if (state->first_nonopt != state->last_nonopt
                && state->last_nonopt != state->ind)
                exchange((char **) argv, state);
            else if (state->first_nonopt == state->last_nonopt)
                state->first_nonopt = state->ind;
            state->last_nonopt = argc;

            state->ind = argc;
        }

        /* If we have done all the ARGV-elements, stop the scan
           and back over any non-options that we skipped and permuted.  */

        if (state->ind == argc)
        {
            /* Set the next-arg-index to point at the non-options
               that we previously skipped, so the caller will digest them.  */
            if (state->first_nonopt != state->last_nonopt)
                state->ind = state->first_nonopt;
            return -1;
        }

        /* If we have come to a non-option and did not permute it,
           either stop the scan or describe it to the caller and pass it by.  */

        if (NONOPTION_P)
        {
            state->arg = argv[state->ind++];
            return 1;
        }

        /* We have found another option-ARGV-element.
           Skip the initial punctuation.  */

        state->nextchar = (argv[state->ind] + 1
                        + (argv[state->ind][1] == '-'));
    }

    /* Decode the current option-ARGV-element.  */

    /* Check whether the ARGV-element is a long option.  */

    if (argv[state->ind][1] == '-')
    {
        char *nameend;
        const struct vlc_option *p;
        const struct vlc_option *pfound = NULL;
        int exact = 0;
        int ambig = 0;
        int indfound = -1;
        int option_index;

        for (nameend = state->nextchar; *nameend && *nameend != '='; nameend++)
            /* Do nothing.  */ ;

        /* Test all long options for either exact match
           or abbreviated matches.  */
        for (p = longopts, option_index = 0; p->name; p++, option_index++)
            if (!strncmp(p->name, state->nextchar, nameend - state->nextchar))
            {
                if ((unsigned int) (nameend - state->nextchar)
                    == (unsigned int) strlen(p->name))
                {
                    /* Exact match found.  */
                    pfound = p;
                    indfound = option_index;
                    exact = 1;
                    break;
                }
                else if (pfound == NULL)
                {
                    /* First nonexact match found.  */
                    pfound = p;
                    indfound = option_index;
                }
                else
                    /* Second or later nonexact match found.  */
                    ambig = 1;
            }

        if (ambig && !exact)
        {
            state->nextchar += strlen(state->nextchar);
            state->ind++;
            state->opt = 0;
            return '?';
        }

        if (pfound != NULL)
        {
            option_index = indfound;
            state->ind++;
            if (*nameend)
            {
                if (pfound->has_arg)
                    state->arg = nameend + 1;
                else
                {
                    state->nextchar += strlen(state->nextchar);

                    state->opt = pfound->val;
                    return '?';
                }
            }
            else if (pfound->has_arg)
            {
                if (state->ind < argc)
                    state->arg = argv[state->ind++];
                else
                {
                    state->nextchar += strlen(state->nextchar);
                    state->opt = pfound->val;
                    return optstring[0] == ':' ? ':' : '?';
                }
            }
            state->nextchar += strlen(state->nextchar);
            if (longind != NULL)
                *longind = option_index;
            if (pfound->flag)
            {
                *(pfound->flag) = pfound->val;
                return 0;
            }
            return pfound->val;
        }

        state->nextchar = (char *) "";
        state->ind++;
        state->opt = 0;
        return '?';
    }

    /* Look at and handle the next short option-character.  */

    {
        char c = *(state->nextchar)++;
        char *temp = strchr(optstring, c);

        /* Increment `optind' when we start to process its last character.  */
        if (*state->nextchar == '\0')
            ++state->ind;

        if (temp == NULL || c == ':')
        {
            state->opt = c;
            return '?';
        }
        /* Convenience. Treat POSIX -W foo same as long option --foo */
        if (temp[0] == 'W' && temp[1] == ';')
        {
            char *nameend;
            const struct vlc_option *p;
            const struct vlc_option *pfound = NULL;
            int exact = 0;
            int ambig = 0;
            int indfound = 0;
            int option_index;

            /* This is an option that requires an argument.  */
            if (*state->nextchar != '\0')
            {
                state->arg = state->nextchar;
                /* If we end this ARGV-element by taking the rest as an arg,
                   we must advance to the next element now.  */
                state->ind++;
            }
            else if (state->ind == argc)
            {
                state->opt = c;
                if (optstring[0] == ':')
                    c = ':';
                else
                    c = '?';
                return c;
            }
            else
                /* We already incremented `optind' once;
                   increment it again when taking next ARGV-elt as argument.  */
                state->arg = argv[state->ind++];

            /* optarg is now the argument, see if it's in the
               table of longopts.  */

            for (state->nextchar = nameend = state->arg; *nameend && *nameend != '='; nameend++)
                /* Do nothing.  */ ;

            /* Test all long options for either exact match
               or abbreviated matches.  */
            for (p = longopts, option_index = 0; p->name; p++, option_index++)
                if (!strncmp(p->name, state->nextchar, nameend - state->nextchar))
                {
                    if ((unsigned int) (nameend - state->nextchar)
                        == strlen(p->name))
                    {
                        /* Exact match found.  */
                        pfound = p;
                        indfound = option_index;
                        exact = 1;
                        break;
                    }
                    else if (pfound == NULL)
                    {
                        /* First nonexact match found.  */
                        pfound = p;
                        indfound = option_index;
                    }
                    else
                        /* Second or later nonexact match found.  */
                        ambig = 1;
                }
            if (ambig && !exact)
            {
                state->nextchar += strlen(state->nextchar);
                state->ind++;
                return '?';
            }
            if (pfound != NULL)
            {
                option_index = indfound;
                if (*nameend)
                {
                    if (pfound->has_arg)
                        state->arg = nameend + 1;
                    else
                    {
                        state->nextchar += strlen(state->nextchar);
                        return '?';
                    }
                }
                else if (pfound->has_arg)
                {
                    if (state->ind < argc)
                        state->arg = argv[state->ind++];
                    else
                    {
                        state->nextchar += strlen(state->nextchar);
                        return optstring[0] == ':' ? ':' : '?';
                    }
                }
                state->nextchar += strlen(state->nextchar);
                if (longind != NULL)
                    *longind = option_index;
                if (pfound->flag)
                {
                    *(pfound->flag) = pfound->val;
                    return 0;
                }
                return pfound->val;
            }
            state->nextchar = NULL;
            return 'W';    /* Let the application handle it.   */
        }
        if (temp[1] == ':')
        {
            /* This is an option that requires an argument.  */
            if (*state->nextchar != '\0')
            {
                state->arg = state->nextchar;
                /* If we end this ARGV-element by taking the rest as an arg,
                   we must advance to the next element now.  */
                state->ind++;
            }
            else if (state->ind == argc)
            {
                state->opt = c;
                if (optstring[0] == ':')
                    c = ':';
                else
                    c = '?';
            }
            else
                /* We already incremented `optind' once;
                   increment it again when taking next ARGV-elt as argument.  */
                state->arg = argv[state->ind++];
            state->nextchar = NULL;
        }
        return c;
    }
}
