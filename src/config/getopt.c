/*****************************************************************************
 * getopt_long()
 *****************************************************************************
 * Copyright (C) 1987-1997 Free Software Foundation, Inc.
 * Copyright (C) 2005-2010 the VideoLAN team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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

/* This version of `getopt' appears to the caller like standard Unix `getopt'
   but it behaves differently for the user, since it allows the user
   to intersperse the options with the other arguments.

   As `getopt' works, it permutes the elements of ARGV so that,
   when it is done, all the options precede everything else.  Thus
   all application programs are extended to handle flexible argument order.

   Setting the environment variable POSIXLY_CORRECT disables permutation.
   Then the behavior is completely standard.

   GNU application programs can use a third alternative mode in which
   they can distinguish the relative order of options and other arguments.  */

#include "vlc_getopt.h"

/* For communication from `getopt' to the caller.
   When `getopt' finds an option that takes an argument,
   the argument value is returned here.  */

char *vlc_optarg = NULL;

/* Index in ARGV of the next element to be scanned.
   This is used for communication to and from the caller
   and for communication between successive calls to `getopt'.

   On entry to `getopt', zero means this is the first call; initialize.

   When `getopt' returns -1, this is the index of the first of the
   non-option elements that the caller should itself scan.

   Otherwise, `optind' communicates from one call to the next
   how much of ARGV has been scanned so far.  */

/* 1003.2 says this must be 1 before any call.  */
int vlc_optind = 1;

/* The next char to be scanned in the option-element
   in which the last option character we returned was found.
   This allows us to pick up the scan where we left off.

   If this is zero, or a null string, it means resume the scan
   by advancing to the next ARGV-element.  */

static char *nextchar;

/* Set to an option character which was unrecognized.
   This must be initialized on some systems to avoid linking in the
   system's own getopt implementation.  */

int vlc_optopt = '?';

/* Describe how to deal with options that follow non-option ARGV-elements.

   If the caller did not specify anything,
   the default is REQUIRE_ORDER if the environment variable
   POSIXLY_CORRECT is defined, PERMUTE otherwise.

   REQUIRE_ORDER means don't recognize them as options;
   stop option processing when the first non-option is seen.
   This is what Unix does.
   This mode of operation is selected by either setting the environment
   variable POSIXLY_CORRECT, or using `+' as the first character
   of the list of option characters.

   PERMUTE is the default.  We permute the contents of ARGV as we scan,
   so that eventually all the non-options are at the end.  This allows options
   to be given in any order, even with programs that were not written to
   expect this.

   The special argument `--' forces an end of option-scanning regardless
   of the value of `ordering'.  */

static enum
{
    REQUIRE_ORDER, PERMUTE
}
ordering;

/* Handle permutation of arguments.  */

/* Describe the part of ARGV that contains non-options that have
   been skipped.  `first_nonopt' is the index in ARGV of the first of them;
   `last_nonopt' is the index after the last of them.  */

static int first_nonopt;
static int last_nonopt;

/* Exchange two adjacent subsequences of ARGV.
   One subsequence is elements [first_nonopt,last_nonopt)
   which contains all the non-options that have been skipped so far.
   The other is elements [last_nonopt,optind), which contains all
   the options processed since those non-options were skipped.

   `first_nonopt' and `last_nonopt' are relocated so that they describe
   the new indices of the non-options in ARGV after they are moved.  */

static void exchange(char **);

static void
     exchange(argv)
     char **argv;
{
    int bottom = first_nonopt;
    int middle = last_nonopt;
    int top = vlc_optind;
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

    first_nonopt += (vlc_optind - last_nonopt);
    last_nonopt = vlc_optind;
}

/* Initialize the internal data when the first call is made.  */

static const char *vlc_getopt_initialize(const char *optstring)
{
    /* Start processing options with ARGV-element 1 (since ARGV-element 0
       is the program name); the sequence of previously skipped
       non-option ARGV-elements is empty.  */

    first_nonopt = last_nonopt = vlc_optind = 1;

    nextchar = NULL;

    const char *posixly_correct = getenv("POSIXLY_CORRECT");

    /* Determine how to handle the ordering of options and nonoptions.  */

    if (posixly_correct != NULL)
        ordering = REQUIRE_ORDER;
    else
        ordering = PERMUTE;

    return optstring;
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

int
    vlc_getopt_long(argc, argv, optstring, longopts, longind)
     int argc;
     char *const *argv;
     const char *optstring;
     const struct vlc_option *restrict longopts;
     int *longind;
{
    vlc_optarg = NULL;

    if (vlc_optind == 0)
    {
        optstring = vlc_getopt_initialize(optstring);
        vlc_optind = 1;    /* Don't scan ARGV[0], the program name.  */
    }

#define NONOPTION_P (argv[vlc_optind][0] != '-' || argv[vlc_optind][1] == '\0')

    if (nextchar == NULL || *nextchar == '\0')
    {
        /* Advance to the next ARGV-element.  */

        /* Give FIRST_NONOPT & LAST_NONOPT rational values if OPTIND has been
           moved back by the user (who may also have changed the arguments).  */
        if (last_nonopt > vlc_optind)
            last_nonopt = vlc_optind;
        if (first_nonopt > vlc_optind)
            first_nonopt = vlc_optind;

        if (ordering == PERMUTE)
        {
            /* If we have just processed some options following some non-options,
               exchange them so that the options come first.  */

            if (first_nonopt != last_nonopt && last_nonopt != vlc_optind)
                exchange((char **) argv);
            else if (last_nonopt != vlc_optind)
                first_nonopt = vlc_optind;

            /* Skip any additional non-options
               and extend the range of non-options previously skipped.  */

            while (vlc_optind < argc && NONOPTION_P)
                vlc_optind++;
            last_nonopt = vlc_optind;
        }

        /* The special ARGV-element `--' means premature end of options.
           Skip it like a null option,
           then exchange with previous non-options as if it were an option,
           then skip everything else like a non-option.  */

        if (vlc_optind != argc && !strcmp(argv[vlc_optind], "--"))
        {
            vlc_optind++;

            if (first_nonopt != last_nonopt && last_nonopt != vlc_optind)
                exchange((char **) argv);
            else if (first_nonopt == last_nonopt)
                first_nonopt = vlc_optind;
            last_nonopt = argc;

            vlc_optind = argc;
        }

        /* If we have done all the ARGV-elements, stop the scan
           and back over any non-options that we skipped and permuted.  */

        if (vlc_optind == argc)
        {
            /* Set the next-arg-index to point at the non-options
               that we previously skipped, so the caller will digest them.  */
            if (first_nonopt != last_nonopt)
                vlc_optind = first_nonopt;
            return -1;
        }

        /* If we have come to a non-option and did not permute it,
           either stop the scan or describe it to the caller and pass it by.  */

        if (NONOPTION_P)
        {
            if (ordering == REQUIRE_ORDER)
                return -1;
            vlc_optarg = argv[vlc_optind++];
            return 1;
        }

        /* We have found another option-ARGV-element.
           Skip the initial punctuation.  */

        nextchar = (argv[vlc_optind] + 1
                + (argv[vlc_optind][1] == '-'));
    }

    /* Decode the current option-ARGV-element.  */

    /* Check whether the ARGV-element is a long option.  */

    if (argv[vlc_optind][1] == '-')
    {
        char *nameend;
        const struct vlc_option *p;
        const struct vlc_option *pfound = NULL;
        int exact = 0;
        int ambig = 0;
        int indfound = -1;
        int option_index;

        for (nameend = nextchar; *nameend && *nameend != '='; nameend++)
            /* Do nothing.  */ ;

        /* Test all long options for either exact match
           or abbreviated matches.  */
        for (p = longopts, option_index = 0; p->name; p++, option_index++)
            if (!strncmp(p->name, nextchar, nameend - nextchar))
            {
                if ((unsigned int) (nameend - nextchar)
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
            nextchar += strlen(nextchar);
            vlc_optind++;
            vlc_optopt = 0;
            return '?';
        }

        if (pfound != NULL)
        {
            option_index = indfound;
            vlc_optind++;
            if (*nameend)
            {
                if (pfound->has_arg)
                    vlc_optarg = nameend + 1;
                else
                {
                    nextchar += strlen(nextchar);

                    vlc_optopt = pfound->val;
                    return '?';
                }
            }
            else if (pfound->has_arg)
            {
                if (vlc_optind < argc)
                    vlc_optarg = argv[vlc_optind++];
                else
                {
                    nextchar += strlen(nextchar);
                    vlc_optopt = pfound->val;
                    return optstring[0] == ':' ? ':' : '?';
                }
            }
            nextchar += strlen(nextchar);
            if (longind != NULL)
                *longind = option_index;
            if (pfound->flag)
            {
                *(pfound->flag) = pfound->val;
                return 0;
            }
            return pfound->val;
        }

        nextchar = (char *) "";
        vlc_optind++;
        vlc_optopt = 0;
        return '?';
    }

    /* Look at and handle the next short option-character.  */

    {
        char c = *nextchar++;
        char *temp = strchr(optstring, c);

        /* Increment `optind' when we start to process its last character.  */
        if (*nextchar == '\0')
            ++vlc_optind;

        if (temp == NULL || c == ':')
        {
            vlc_optopt = c;
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
            if (*nextchar != '\0')
            {
                vlc_optarg = nextchar;
                /* If we end this ARGV-element by taking the rest as an arg,
                   we must advance to the next element now.  */
                vlc_optind++;
            }
            else if (vlc_optind == argc)
            {
                vlc_optopt = c;
                if (optstring[0] == ':')
                    c = ':';
                else
                    c = '?';
                return c;
            }
            else
                /* We already incremented `optind' once;
                   increment it again when taking next ARGV-elt as argument.  */
                vlc_optarg = argv[vlc_optind++];

            /* optarg is now the argument, see if it's in the
               table of longopts.  */

            for (nextchar = nameend = vlc_optarg; *nameend && *nameend != '='; nameend++)
                /* Do nothing.  */ ;

            /* Test all long options for either exact match
               or abbreviated matches.  */
            for (p = longopts, option_index = 0; p->name; p++, option_index++)
                if (!strncmp(p->name, nextchar, nameend - nextchar))
                {
                    if ((unsigned int) (nameend - nextchar) == strlen(p->name))
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
                nextchar += strlen(nextchar);
                vlc_optind++;
                return '?';
            }
            if (pfound != NULL)
            {
                option_index = indfound;
                if (*nameend)
                {
                    if (pfound->has_arg)
                        vlc_optarg = nameend + 1;
                    else
                    {
                        nextchar += strlen(nextchar);
                        return '?';
                    }
                }
                else if (pfound->has_arg)
                {
                    if (vlc_optind < argc)
                        vlc_optarg = argv[vlc_optind++];
                    else
                    {
                        nextchar += strlen(nextchar);
                        return optstring[0] == ':' ? ':' : '?';
                    }
                }
                nextchar += strlen(nextchar);
                if (longind != NULL)
                    *longind = option_index;
                if (pfound->flag)
                {
                    *(pfound->flag) = pfound->val;
                    return 0;
                }
                return pfound->val;
            }
            nextchar = NULL;
            return 'W';    /* Let the application handle it.   */
        }
        if (temp[1] == ':')
        {
            /* This is an option that requires an argument.  */
            if (*nextchar != '\0')
            {
                vlc_optarg = nextchar;
                /* If we end this ARGV-element by taking the rest as an arg,
                   we must advance to the next element now.  */
                vlc_optind++;
            }
            else if (vlc_optind == argc)
            {
                vlc_optopt = c;
                if (optstring[0] == ':')
                    c = ':';
                else
                    c = '?';
            }
            else
                /* We already incremented `optind' once;
                   increment it again when taking next ARGV-elt as argument.  */
                vlc_optarg = argv[vlc_optind++];
            nextchar = NULL;
        }
        return c;
    }
}
