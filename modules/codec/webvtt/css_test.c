/*****************************************************************************
 * css_test.c: CSS Parser test
 *****************************************************************************
 * Copyright (C) 2019 VideoLabs, VideoLAN and VLC Authors
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#undef NDEBUG

#include <assert.h>
#include <vlc_common.h>
#include "css_parser.h"

#define BAILOUT(run) { fprintf(stderr, "failed %s line %d\n", run, __LINE__); \
                        goto error; }
#define CHECK(test) run = (test); fprintf(stderr, "* Running test %s\n", run);
#define EXPECT(foo) if(!(foo)) BAILOUT(run)

const char * css =
        "el1 { float0: 1; }\n"
        ".class1 { hex1: #F0000f; }\n"
        "#id1 { text2: \"foo bar\"; }\n"
        ":pseudo { text2: \"foobar\"; }\n"
        "attrib[foo=\"bar\"] { text2: \"foobar\"; }\n"
        "attrib2[foo] { text2: \"foobar\"; }\n"
        "attribincludes[foo~=\"bar\"] { text2: \"foobar\"; }\n"
        "attribdashmatch[foo|=\"bar\"] { text2: \"foobar\"; }\n"
        "attribstarts[foo^=\"bar\"] { text2: \"foobar\"; }\n"
        "attribends[foo$=\"bar\"] { text2: \"foobar\"; }\n"
        "attribcontains[foo*=\"bar\"] { text2: \"foobar\"; }\n"
        "parent1 child1 { float0: 1; }\n"
        "el2,el3 { float0: 1; }\n"
        "el4+el0 { float0: 1; }\n"
        "el5~el0 { float0: 1; }\n"
        "el6>el0 { float0: 1; }\n"
        "values { "
        "  neg: -1;"
        "  ems: 100em;"
        "  exs: 100ex;"
        "  pixels: 100px;"
        "  points: 100pt; "
        "  mm: 100mm;"
        "  percent: 100%;"
        "  ms: 100ms;"
        "  hz: 100Hz;"
        "  degrees: 100deg;"
        "  dimension: 100 -200em 300px;"
        "  string: \"foobar\";"
        "  function: foo(1);"
        "  identifier: foobar;"
        "  hexcolor: #ff00ff;"
        "  unicoderange: U+00-FF;"
        "  uri: url(http://crap/);"
        "}\n"
;

int main(void)
{
    const char *run ="parsing";
    vlc_css_parser_t p;
    vlc_css_parser_Init(&p);
    bool b = vlc_css_parser_ParseBytes(&p, (const uint8_t *)css, strlen(css));
    EXPECT(b);
    vlc_css_parser_Debug(&p);
    const vlc_css_rule_t *rule = p.rules.p_first;

    CHECK("element selector");
    EXPECT(rule && rule->b_valid);
    EXPECT(!strcmp(rule->p_selectors->psz_name,"el1"));
    EXPECT(rule->p_selectors->type == SELECTOR_SIMPLE);
    const vlc_css_declaration_t *decl = rule->p_declarations;
    EXPECT(decl && !strcmp(rule->p_declarations->psz_property, "float0"));
    EXPECT(decl->expr && decl->expr->i_count);
    EXPECT(decl->expr->seq[0].term.type == TYPE_NONE);
    EXPECT(decl->expr->seq[0].term.val == 1.0);

    CHECK("class selector");
    rule = rule->p_next;
    EXPECT(rule && rule->b_valid);
    EXPECT(!strcmp(rule->p_selectors->psz_name,"class1"));
    EXPECT(rule->p_selectors->type == SPECIFIER_CLASS);
    decl = rule->p_declarations;
    EXPECT(decl && !strcmp(rule->p_declarations->psz_property, "hex1"));
    EXPECT(decl->expr && decl->expr->i_count);
    EXPECT(decl->expr->seq[0].term.type == TYPE_HEXCOLOR);
    EXPECT(!strcmp(decl->expr->seq[0].term.psz,"#F0000f"));

    CHECK("id selector");
    rule = rule->p_next;
    EXPECT(rule && rule->b_valid);
    EXPECT(!strcmp(rule->p_selectors->psz_name,"#id1"));
    EXPECT(rule->p_selectors->type == SPECIFIER_ID);
    decl = rule->p_declarations;
    EXPECT(decl && !strcmp(rule->p_declarations->psz_property, "text2"));
    EXPECT(decl->expr && decl->expr->i_count);
    EXPECT(decl->expr->seq[0].term.type == TYPE_STRING);
    EXPECT(!strcmp(decl->expr->seq[0].term.psz,"foo bar"));

    CHECK("pseudoclass selector");
    rule = rule->p_next;
    EXPECT(rule && rule->b_valid);
    EXPECT(!strcmp(rule->p_selectors->psz_name,"pseudo"));
    EXPECT(rule->p_selectors->type == SELECTOR_PSEUDOCLASS);

    CHECK("attribute selector equals");
    rule = rule->p_next;
    EXPECT(rule && rule->b_valid);
    EXPECT(!strcmp(rule->p_selectors->psz_name,"attrib"));
    EXPECT(rule->p_selectors->type == SELECTOR_SIMPLE);
    EXPECT(rule->p_selectors->specifiers.p_first);
    EXPECT(rule->p_selectors->specifiers.p_first->type == SPECIFIER_ATTRIB);
    EXPECT(!strcmp(rule->p_selectors->specifiers.p_first->psz_name, "foo"));
    EXPECT(rule->p_selectors->specifiers.p_first->p_matchsel);
    EXPECT(rule->p_selectors->specifiers.p_first->p_matchsel->match == MATCH_EQUALS);
    EXPECT(!strcmp(rule->p_selectors->specifiers.p_first->p_matchsel->psz_name, "bar"));

    CHECK("attribute selector key only");
    rule = rule->p_next;
    EXPECT(rule && rule->b_valid);
    EXPECT(!strcmp(rule->p_selectors->psz_name,"attrib2"));
    EXPECT(rule->p_selectors->type == SELECTOR_SIMPLE);
    EXPECT(rule->p_selectors->specifiers.p_first);
    EXPECT(rule->p_selectors->specifiers.p_first->type == SPECIFIER_ATTRIB);
    EXPECT(!strcmp(rule->p_selectors->specifiers.p_first->psz_name, "foo"));
    EXPECT(rule->p_selectors->specifiers.p_first->p_matchsel == NULL);

    CHECK("attribute selector ~=");
    rule = rule->p_next;
    EXPECT(rule && rule->b_valid);
    EXPECT(!strcmp(rule->p_selectors->psz_name,"attribincludes"));
    EXPECT(rule->p_selectors->type == SELECTOR_SIMPLE);
    EXPECT(rule->p_selectors->specifiers.p_first);
    EXPECT(rule->p_selectors->specifiers.p_first->type == SPECIFIER_ATTRIB);
    EXPECT(!strcmp(rule->p_selectors->specifiers.p_first->psz_name, "foo"));
    EXPECT(rule->p_selectors->specifiers.p_first->match == MATCH_INCLUDES);
    EXPECT(rule->p_selectors->specifiers.p_first->p_matchsel);
    EXPECT(!strcmp(rule->p_selectors->specifiers.p_first->p_matchsel->psz_name, "bar"));

    CHECK("attribute selector |=");
    rule = rule->p_next;
    EXPECT(rule && rule->b_valid);
    EXPECT(!strcmp(rule->p_selectors->psz_name,"attribdashmatch"));
    EXPECT(rule->p_selectors->type == SELECTOR_SIMPLE);
    EXPECT(rule->p_selectors->specifiers.p_first);
    EXPECT(rule->p_selectors->specifiers.p_first->type == SPECIFIER_ATTRIB);
    EXPECT(!strcmp(rule->p_selectors->specifiers.p_first->psz_name, "foo"));
    EXPECT(rule->p_selectors->specifiers.p_first->match == MATCH_DASHMATCH);
    EXPECT(rule->p_selectors->specifiers.p_first->p_matchsel);
    EXPECT(!strcmp(rule->p_selectors->specifiers.p_first->p_matchsel->psz_name, "bar"));

    CHECK("attribute selector ^=");
    rule = rule->p_next;
    EXPECT(rule && rule->b_valid);
    EXPECT(!strcmp(rule->p_selectors->psz_name,"attribstarts"));
    EXPECT(rule->p_selectors->type == SELECTOR_SIMPLE);
    EXPECT(rule->p_selectors->specifiers.p_first);
    EXPECT(rule->p_selectors->specifiers.p_first->type == SPECIFIER_ATTRIB);
    EXPECT(!strcmp(rule->p_selectors->specifiers.p_first->psz_name, "foo"));
    EXPECT(rule->p_selectors->specifiers.p_first->match == MATCH_BEGINSWITH);
    EXPECT(rule->p_selectors->specifiers.p_first->p_matchsel);
    EXPECT(!strcmp(rule->p_selectors->specifiers.p_first->p_matchsel->psz_name, "bar"));

    CHECK("attribute selector $=");
    rule = rule->p_next;
    EXPECT(rule && rule->b_valid);
    EXPECT(!strcmp(rule->p_selectors->psz_name,"attribends"));
    EXPECT(rule->p_selectors->type == SELECTOR_SIMPLE);
    EXPECT(rule->p_selectors->specifiers.p_first);
    EXPECT(rule->p_selectors->specifiers.p_first->type == SPECIFIER_ATTRIB);
    EXPECT(!strcmp(rule->p_selectors->specifiers.p_first->psz_name, "foo"));
    EXPECT(rule->p_selectors->specifiers.p_first->match == MATCH_ENDSWITH);
    EXPECT(rule->p_selectors->specifiers.p_first->p_matchsel);
    EXPECT(!strcmp(rule->p_selectors->specifiers.p_first->p_matchsel->psz_name, "bar"));

    CHECK("attribute selector *=");
    rule = rule->p_next;
    EXPECT(rule && rule->b_valid);
    EXPECT(!strcmp(rule->p_selectors->psz_name,"attribcontains"));
    EXPECT(rule->p_selectors->type == SELECTOR_SIMPLE);
    EXPECT(rule->p_selectors->specifiers.p_first);
    EXPECT(rule->p_selectors->specifiers.p_first->type == SPECIFIER_ATTRIB);
    EXPECT(!strcmp(rule->p_selectors->specifiers.p_first->psz_name, "foo"));
    EXPECT(rule->p_selectors->specifiers.p_first->match == MATCH_CONTAINS);
    EXPECT(rule->p_selectors->specifiers.p_first->p_matchsel);
    EXPECT(!strcmp(rule->p_selectors->specifiers.p_first->p_matchsel->psz_name, "bar"));

    CHECK("selectors combination parent child");
    rule = rule->p_next;
    EXPECT(rule && rule->b_valid);
    EXPECT(!strcmp(rule->p_selectors->psz_name,"parent1"));
    EXPECT(rule->p_selectors->specifiers.p_first);
    EXPECT(rule->p_selectors->specifiers.p_first->combinator == RELATION_DESCENDENT);
    EXPECT(!strcmp(rule->p_selectors->specifiers.p_first->psz_name, "child1"));

    CHECK("selectors combination alternative");
    rule = rule->p_next;
    EXPECT(rule && rule->b_valid);
    EXPECT(!strcmp(rule->p_selectors->psz_name,"el2"));
    EXPECT(rule->p_selectors->p_next);
    EXPECT(!strcmp(rule->p_selectors->p_next->psz_name,"el3"));

    CHECK("selectors combination directadjacent");
    rule = rule->p_next;
    EXPECT(rule && rule->b_valid);
    EXPECT(!strcmp(rule->p_selectors->psz_name,"el4"));
    EXPECT(rule->p_selectors->specifiers.p_first);
    EXPECT(rule->p_selectors->specifiers.p_first->combinator == RELATION_DIRECTADJACENT);
    EXPECT(!strcmp(rule->p_selectors->specifiers.p_first->psz_name, "el0"));

    CHECK("selectors combination directprecedent");
    rule = rule->p_next;
    EXPECT(rule && rule->b_valid);
    EXPECT(!strcmp(rule->p_selectors->psz_name,"el5"));
    EXPECT(rule->p_selectors->specifiers.p_first);
    EXPECT(rule->p_selectors->specifiers.p_first->combinator == RELATION_INDIRECTADJACENT);
    EXPECT(!strcmp(rule->p_selectors->specifiers.p_first->psz_name, "el0"));

    CHECK("selectors combination child");
    rule = rule->p_next;
    EXPECT(rule && rule->b_valid);
    EXPECT(!strcmp(rule->p_selectors->psz_name,"el6"));
    EXPECT(rule->p_selectors->specifiers.p_first);
    EXPECT(rule->p_selectors->specifiers.p_first->combinator == RELATION_CHILD);
    EXPECT(!strcmp(rule->p_selectors->specifiers.p_first->psz_name, "el0"));

    CHECK("values");
    rule = rule->p_next;
    EXPECT(rule && rule->b_valid);
    EXPECT(!strcmp(rule->p_selectors->psz_name,"values"));
    decl = rule->p_declarations;
    EXPECT(decl && !strcmp(decl->psz_property, "neg"));
    EXPECT(decl->expr && decl->expr->i_count);
    EXPECT(decl->expr->seq[0].term.type == TYPE_NONE);
    EXPECT(decl->expr->seq[0].term.val == -1.0);
    decl = decl->p_next;
    EXPECT(decl && !strcmp(decl->psz_property, "ems"));
    EXPECT(decl->expr && decl->expr->i_count);
    EXPECT(decl->expr->seq[0].term.type == TYPE_EMS);
    EXPECT(decl->expr->seq[0].term.val == 100);
    decl = decl->p_next;
    EXPECT(decl && !strcmp(decl->psz_property, "exs"));
    EXPECT(decl->expr && decl->expr->i_count);
    EXPECT(decl->expr->seq[0].term.type == TYPE_EXS);
    EXPECT(decl->expr->seq[0].term.val == 100);
    decl = decl->p_next;
    EXPECT(decl && !strcmp(decl->psz_property, "pixels"));
    EXPECT(decl->expr && decl->expr->i_count);
    EXPECT(decl->expr->seq[0].term.type == TYPE_PIXELS);
    EXPECT(decl->expr->seq[0].term.val == 100);
    decl = decl->p_next;
    EXPECT(decl && !strcmp(decl->psz_property, "points"));
    EXPECT(decl->expr && decl->expr->i_count);
    EXPECT(decl->expr->seq[0].term.type == TYPE_POINTS);
    EXPECT(decl->expr->seq[0].term.val == 100);
    decl = decl->p_next;
    EXPECT(decl && !strcmp(decl->psz_property, "mm"));
    EXPECT(decl->expr && decl->expr->i_count);
    EXPECT(decl->expr->seq[0].term.type == TYPE_MILLIMETERS);
    EXPECT(decl->expr->seq[0].term.val == 100);
    decl = decl->p_next;
    EXPECT(decl && !strcmp(decl->psz_property, "percent"));
    EXPECT(decl->expr && decl->expr->i_count);
    EXPECT(decl->expr->seq[0].term.type == TYPE_PERCENT);
    EXPECT(decl->expr->seq[0].term.val == 100);
    decl = decl->p_next;
    EXPECT(decl && !strcmp(decl->psz_property, "ms"));
    EXPECT(decl->expr && decl->expr->i_count);
    EXPECT(decl->expr->seq[0].term.type == TYPE_MILLISECONDS);
    EXPECT(decl->expr->seq[0].term.val == 100);
    decl = decl->p_next;
    EXPECT(decl && !strcmp(decl->psz_property, "hz"));
    EXPECT(decl->expr && decl->expr->i_count);
    EXPECT(decl->expr->seq[0].term.type == TYPE_HERTZ);
    EXPECT(decl->expr->seq[0].term.val == 100);
    decl = decl->p_next;
    EXPECT(decl && !strcmp(decl->psz_property, "degrees"));
    EXPECT(decl->expr && decl->expr->i_count);
    EXPECT(decl->expr->seq[0].term.type == TYPE_DEGREES);
    EXPECT(decl->expr->seq[0].term.val == 100);
    decl = decl->p_next;
    EXPECT(decl && !strcmp(decl->psz_property, "dimension"));
    EXPECT(decl->expr && decl->expr->i_count == 3);
    EXPECT(decl->expr->seq[0].term.type == TYPE_NONE);
    EXPECT(decl->expr->seq[0].term.val == 100);
    EXPECT(decl->expr->seq[1].term.type == TYPE_EMS);
    EXPECT(decl->expr->seq[1].term.val == -200);
    EXPECT(decl->expr->seq[2].term.type == TYPE_PIXELS);
    EXPECT(decl->expr->seq[2].term.val == 300);
    decl = decl->p_next;
    EXPECT(decl && !strcmp(decl->psz_property, "string"));
    EXPECT(decl->expr && decl->expr->i_count);
    EXPECT(decl->expr->seq[0].term.type == TYPE_STRING);
    EXPECT(!strcmp(decl->expr->seq[0].term.psz, "foobar"));
    decl = decl->p_next;
    EXPECT(decl && !strcmp(decl->psz_property, "function"));
    EXPECT(decl->expr && decl->expr->i_count);
    EXPECT(decl->expr->seq[0].term.type == TYPE_FUNCTION);
    /* todo */
    decl = decl->p_next;
    EXPECT(decl && !strcmp(decl->psz_property, "identifier"));
    EXPECT(decl->expr && decl->expr->i_count);
    EXPECT(decl->expr->seq[0].term.type == TYPE_IDENTIFIER);
    EXPECT(!strcmp(decl->expr->seq[0].term.psz, "foobar"));
    decl = decl->p_next;
    EXPECT(decl && !strcmp(decl->psz_property, "hexcolor"));
    EXPECT(decl->expr && decl->expr->i_count);
    EXPECT(decl->expr->seq[0].term.type == TYPE_HEXCOLOR);
    EXPECT(!strcmp(decl->expr->seq[0].term.psz, "#ff00ff"));
    decl = decl->p_next;
    EXPECT(decl && !strcmp(decl->psz_property, "unicoderange"));
    EXPECT(decl->expr && decl->expr->i_count);
    EXPECT(decl->expr->seq[0].term.type == TYPE_UNICODERANGE);
    EXPECT(!strcmp(decl->expr->seq[0].term.psz, "U+00-FF"));
    decl = decl->p_next;
    EXPECT(decl && !strcmp(decl->psz_property, "uri"));
    EXPECT(decl->expr && decl->expr->i_count);
    EXPECT(decl->expr->seq[0].term.type == TYPE_URI);
    EXPECT(!strcmp(decl->expr->seq[0].term.psz, "url(http://crap/)"));

    vlc_css_parser_Clean(&p);
    return 0;

error:
    vlc_css_parser_Clean(&p);
    return 1;
}
