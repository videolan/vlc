/*****************************************************************************
 * list.cpp: tests for vlc_list in C++
 *****************************************************************************
 * Copyright (C) 2024 VideoLabs
 *
 * Authors: Alexandre Janniaux <ajanni@videolabs.io>
 *          Pierre Lamot <pierre@videolabs.io>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
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

#include <vlc_common.h>
#include <vlc_list.hpp>

#include <cassert>

struct item {
    int index = 0;
    struct vlc_list node;
};

struct test_list {
    struct vlc_list list;

    test_list()
    {
        vlc_list_init(&list);
    }
};

int main()
{
    {
        test_list o;
        auto l = vlc::from(o.list, &item::node);
        assert(l.begin() == l.end());
        assert(l.cbegin() == l.cend());
        assert(l.rbegin() == l.rend());
        assert(l.crbegin() == l.crend());
    }

    {
        test_list o;
        item t1; t1.index = 5;
        vlc_list_append(&t1.node, &o.list);

        auto l = vlc::from(o.list, &item::node);
        assert(l.begin() != l.end());
        assert(l.cbegin() != l.cend());
        assert(l.rbegin() != l.rend());
        assert(l.crbegin() != l.crend());

        assert(l.begin()->index == 5);
        assert(l.rbegin()->index == 5);
        assert(l.cbegin()->index == 5);
        assert(l.crbegin()->index == 5);

        item t2; t2.index = 10;
        vlc_list_append(&t2.node, &o.list);

        assert(l.begin()->index == 5);
        assert(l.cbegin()->index == 5);
        assert(l.rbegin()->index == 10);
        assert(l.crbegin()->index == 10);

        assert(l.as_reverse().begin()->index == 10);
        assert(l.as_reverse().cbegin()->index == 10);
        assert(l.as_reverse().rbegin()->index == 5);
        assert(l.as_reverse().crbegin()->index == 5);


    }

    /* Check that the type system will handle the const variant. */
    {
        const test_list l;

        /* Note: the type must be enforced here. */
        vlc::const_list<item> t = vlc::from(l.list, &item::node);
        (void)t;
    }

    {
        test_list l;
        item t1, t2;
        vlc_list_append(&t1.node, &l.list);
        vlc_list_append(&t2.node, &l.list);


        {
            int i = 1;
            for (auto &t : vlc::from(l.list, &item::node))
            {
                fprintf(stderr, "Checking mutable list, got %d, expects %d\n", t.index, i);
                t.index = i++;
            }
            assert(t1.index == 1 && t2.index == 2);
        }

        {
            int i = 1;
            const test_list& lc = l;
            for (const auto &t : vlc::from(lc.list, &item::node))
            {
                fprintf(stderr, "Checking const list, got %d, expects %d\n", t.index, i);
                assert(i++ == t.index);
            }
            assert(i == 3);
        }

        {
            /* const reverse version */
            int i = 2;
            const test_list& lc = l;
            for (const auto &t : vlc::from(lc.list, &item::node).as_reverse())
            {
                fprintf(stderr, "Checking reverse mutable list, got %d, expects %d\n", t.index, i);
                assert(i-- == t.index);
            }
            assert(i == 0);
        }

        {
            /* mutable reverse version */
            int i = 2;
            test_list& lc = l;
            for (auto &t : vlc::from(lc.list, &item::node).as_reverse())
            {
                fprintf(stderr, "Checking reverse const list, got %d, expects %d\n", t.index, i);
                assert(i-- == t.index);
            }
            assert(i == 0);
        }
    }

    {
        test_list l;
        item t1, t2;
        vlc_list_append(&t1.node, &l.list);
        vlc_list_append(&t2.node, &l.list);
        t1.index = 1;
        t2.index = 2;

        int index = 0;
        test_list& lc = l;
        for (auto &t : vlc::from(lc.list, &item::node))
        {
            if (t.index == 1)
                vlc_list_remove(&t.node);
            else
                index = t.index;
        }
        assert(index == 2);
        assert(vlc_list_is_first(&t2.node, &l.list));
        assert(vlc_list_is_last(&t2.node, &l.list));
    }

    {
        test_list l;
        item t1, t2;
        vlc_list_append(&t1.node, &l.list);
        vlc_list_append(&t2.node, &l.list);
        t1.index = 1;
        t2.index = 2;

        int index = 0;
        test_list& lc = l;
        for (auto &t : vlc::from(lc.list, &item::node).as_reverse())
        {
            if (t.index == 2)
                vlc_list_remove(&t.node);
            else
                index = t.index;
        }
        assert(index == 1);
        assert(vlc_list_is_first(&t1.node, &l.list));
        assert(vlc_list_is_last(&t1.node, &l.list));
    }

    {
        /* Check that removal are working correctly when the current item is
         * being removed during iteration. */
        test_list l;
        item t1, t2;
        vlc_list_append(&t1.node, &l.list);
        vlc_list_append(&t2.node, &l.list);
        t1.index = 1;
        t2.index = 2;

        auto list = vlc::from(l.list, &item::node);
        auto it = list.begin();
        assert((*it).index == 1);

        vlc_list_remove(&(*it).node);
        ++it;
        assert(it != list.end());
        assert(it == list.begin());
        assert((*it).index == 2);
        --it;
        assert(it == list.end());
    }

    {
        test_list l;
        auto lw = vlc::from(l.list, &item::node);

        item t1; t1.index = 1;
        lw.push_front(t1);

        item t2; t2.index = 2;
        lw.push_front(t2);

        item t3; t3.index = 3;
        lw.push_front(t3);

        int i = 3;
        for (auto &t : lw)
        {
            assert(t.index == i--);
        }
        assert(i == 0);

        auto it = lw.begin();
        ++it;
        assert((*it).index == 2);
        lw.erase(it);
        assert((*++lw.begin()).index == 1);
    }

    {
        test_list l;
        auto lw = vlc::from(l.list, &item::node);

        item t1; t1.index = 1;
        lw.push_front(t1);

        item t2; t2.index = 2;
        lw.push_front(t2);

        item t3; t3.index = 3;
        lw.push_front(t3);

        int i = 1;
        for (auto &t : lw.as_reverse())
        {
            assert(t.index == i++);
        }
        assert(i == 4);

        auto it = lw.begin();
        ++it;
        assert((*it).index == 2);
        lw.erase(it);
        assert((*++lw.begin()).index == 1);
    }

    {
        test_list l;
        auto lw = vlc::from(l.list, &item::node);

        item t1; t1.index = 1;
        lw.push_front(t1);

        item t2; t2.index = 2;
        lw.push_front(t2);

        item t3; t3.index = 3;
        lw.push_front(t3);

        auto lr = vlc::from(l.list, &item::node).as_reverse();
        {
            auto it = lw.erase(lr.begin());
            fprintf(stderr, "Removing as_reverse().begin() in [ 1, 2, 3 ] and "
                    "checking as_reverse().begin(), got %d, expects %d, removed %d\n",
                    lr.begin()->index, 2, it->index);
        }
        assert(lr.begin()->index == 2);

        {
            auto it = lw.erase(lr.rbegin());
            fprintf(stderr, "Removing as_reverse().rbegin() in [ 1, 2 ] and "
                    "checking as_reverse().rbegin(), got %d, expects %d, removed %d\n",
                    lr.rbegin()->index, 2, it->index);
        }
        assert(lr.rbegin()->index == 2);
    }

    return 0;
}
