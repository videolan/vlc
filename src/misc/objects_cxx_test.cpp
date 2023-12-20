/*****************************************************************************
 * objects_cxx_test.cpp: vlc_object_t helper tests
 *****************************************************************************
 * Copyright (C) 2023 Videlabs
 *
 * Authors: Alexandre Janniaux <ajanni@videolabs.io>
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
# include <config.h>
#endif

#include <vlc_common.h>
#include <vlc_objects.h>

#include "../libvlc.h"

extern "C" {
    void var_DestroyAll(vlc_object_t *) { /* stub */ }
}

struct ObjectType {
    vlc_object_t obj;
    int value = 5;
    ~ObjectType() { destructor_called = true; }
    static bool destructor_called;
};
bool ObjectType::destructor_called = false;

int main(int argc, char **argv)
{
    (void)argc; (void)argv;

    vlc_object_t *root = new vlc_object_t {};
    int ret = vlc_object_init(root, nullptr, "root");
    assert(ret == 0);

    struct ObjectType *child = vlc_object_create<ObjectType>(root);

    /* Ensure default constructor have been called. */
    assert(child->value == 5);
    assert(ObjectType::destructor_called == false);

    /* No zero-initialization of the object itself. */
    assert(child->obj.priv != NULL);

    /* Ensure destructor have been called. */
    vlc_object_delete(child);
    assert(ObjectType::destructor_called == true);
    ObjectType::destructor_called = false;

    /* Ensure it's not called for C objects. */
    ObjectType *child_c = static_cast<ObjectType*>(vlc_object_create(root, sizeof(ObjectType)));
    assert(child_c->value == 0);
    vlc_object_delete(VLC_OBJECT(child_c));
    assert(ObjectType::destructor_called == false);

    vlc_object_deinit(root);
    delete(root);

    return 0;
}
