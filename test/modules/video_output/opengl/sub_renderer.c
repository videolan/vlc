/*****************************************************************************
 * sub_renderer.c: test for the OpenGL subrenderer code
 *****************************************************************************
 * Copyright (C) 2023 VideoLabs
 *
 * Authors: Alexandre Janniaux <ajanni@videolabs.io>
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

#ifndef VLC_TEST_OPENGL_API
# error "Define VLC_TEST_OPENGL_API to the VLC_OPENGL API to use"
#endif

/* Define a builtin module for mocked parts */
#define MODULE_NAME test_opengl
#undef VLC_DYNAMIC_PLUGIN

#include "../../../libvlc/test.h"
#include "../../../../lib/libvlc_internal.h"
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_opengl.h>
#include <vlc_filter.h>
#include <vlc_modules.h>
#include <vlc_vout_display.h>
#include <vlc_subpicture.h>

#include "../../../../modules/video_output/opengl/sub_renderer.h"
#include "../../../../modules/video_output/opengl/gl_api.h"
#include "../../../../modules/video_output/opengl/interop.h"

static_assert(
    VLC_TEST_OPENGL_API == VLC_OPENGL ||
    VLC_TEST_OPENGL_API == VLC_OPENGL_ES2,
    "VLC_TEST_OPENGL_API must be assigned to VLC_OPENGL or VLC_OPENGL_ES2");

const char vlc_module_name[] = MODULE_STRING;
static const uint8_t red[]   = { 0xff, 0x00, 0x00, 0xff };
static const uint8_t green[] = { 0x00, 0xff, 0x00, 0xff };
static const uint8_t blue[]  = { 0x00, 0x00, 0xff, 0xff };
static const uint8_t white[] = { 0xff, 0xff, 0xff, 0xff };
static const uint8_t black[] = { 0x00, 0x00, 0x00, 0xff };

struct test_point
{
    int x, y;
    const uint8_t *color;
};

static const char *OrientToString(video_orientation_t orient)
{
    switch (orient)
    {
        case ORIENT_TOP_LEFT:       return "normal";
        case ORIENT_TOP_RIGHT:      return "hflip";
        case ORIENT_BOTTOM_RIGHT:   return "rot180";
        case ORIENT_BOTTOM_LEFT:    return "vflip";
        case ORIENT_LEFT_BOTTOM:    return "rot270";
        case ORIENT_LEFT_TOP:       return "transposed";
        case ORIENT_RIGHT_TOP:      return "rot90";
        case ORIENT_RIGHT_BOTTOM:   return "antitransposed";
        default: vlc_assert_unreachable();
    }
}

static void test_opengl_offscreen(
        vlc_object_t *root,
        video_orientation_t orientation,
        struct test_point *points,
        size_t points_count
){
    fprintf(stderr, " * Checking orientation %s:\n", OrientToString(orientation));
    struct vlc_decoder_device *device =
        vlc_decoder_device_Create(root, NULL);
    vlc_gl_t *gl = vlc_gl_CreateOffscreen(
            root, device, 4, 4, VLC_TEST_OPENGL_API, NULL, NULL);
    assert(gl != NULL);
    if (device != NULL)
        vlc_decoder_device_Release(device);

    /* HACK: we cannot pass an orientation to the subrenderer right now. */
    gl->orientation = orientation;

    int ret = vlc_gl_MakeCurrent(gl);
    assert(ret == VLC_SUCCESS);

    struct vlc_gl_api api;
    ret = vlc_gl_api_Init(&api, gl);
    assert(ret == VLC_SUCCESS);

    GLuint out_tex;
    api.vt.GenTextures(1, &out_tex);
    api.vt.BindTexture(GL_TEXTURE_2D, out_tex);
    api.vt.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    api.vt.BindTexture(GL_TEXTURE_2D, 0);

    GLuint out_fb;
    api.vt.GenFramebuffers(1, &out_fb);
    api.vt.BindFramebuffer(GL_FRAMEBUFFER, out_fb);
    api.vt.BindFramebuffer(GL_READ_FRAMEBUFFER, out_fb);
    api.vt.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                GL_TEXTURE_2D, out_tex, 0);

    assert(api.vt.CheckFramebufferStatus(GL_FRAMEBUFFER)
            == GL_FRAMEBUFFER_COMPLETE);
    GL_ASSERT_NOERROR(&api.vt);

    api.vt.ClearColor(0.f, 0.f, 0.f, 1.f);
    api.vt.Clear(GL_COLOR_BUFFER_BIT);
    api.vt.Viewport(0, 0, 4, 4);

    video_format_t fmt;
    video_format_Init(&fmt, VLC_CODEC_RGBA);
    video_format_Setup(&fmt, VLC_CODEC_RGBA, 2, 2, 2, 2, 1, 1);
    fmt.primaries = COLOR_PRIMARIES_UNDEF;
    fmt.space = COLOR_SPACE_UNDEF;
    fmt.transfer = TRANSFER_FUNC_UNDEF;
    fmt.projection_mode = PROJECTION_MODE_RECTANGULAR;

    struct vlc_gl_interop *interop =
        vlc_gl_interop_NewForSubpictures(gl);
    assert(interop != NULL);
    GL_ASSERT_NOERROR(&api.vt);

    GLint fbo_binding;
    api.vt.GetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo_binding);
    assert((GLuint)fbo_binding == out_fb);
    assert(api.vt.CheckFramebufferStatus(GL_FRAMEBUFFER)
            == GL_FRAMEBUFFER_COMPLETE);

    struct vlc_gl_sub_renderer *sr = vlc_gl_sub_renderer_New(gl, &api, interop);
    GL_ASSERT_NOERROR(&api.vt);

    api.vt.GetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo_binding);
    assert((GLuint)fbo_binding == out_fb);
    assert(api.vt.CheckFramebufferStatus(GL_FRAMEBUFFER)
            == GL_FRAMEBUFFER_COMPLETE);
    GL_ASSERT_NOERROR(&api.vt);

    picture_t *picture = picture_NewFromFormat(&fmt);
    assert(picture != NULL);

    size_t size = picture->p[0].i_lines * picture->p[0].i_pitch / picture->p[0].i_pixel_pitch;
    for (size_t i=0; i < size; ++i)
        memcpy(&picture->p[0].p_pixels[i * 4], green, sizeof(green));

    /* The subpicture region will be the following:
     *     +-----+
     *     | R G |
     *     | B W |
     *     +-----+
     * */
    memcpy(&picture->p[0].p_pixels[0 * 4], red, sizeof(red));
    memcpy(&picture->p[0].p_pixels[1 * 4], green, sizeof(green));
    memcpy(&picture->p[0].p_pixels[0 * 4 + picture->p[0].i_pitch], blue, sizeof(blue));
    memcpy(&picture->p[0].p_pixels[1 * 4 + picture->p[0].i_pitch], white, sizeof(white));

    vlc_render_subpicture *subpicture = vlc_render_subpicture_New();
    assert(subpicture != NULL);
    subpicture->i_original_picture_width = 4;
    subpicture->i_original_picture_height = 4;

    subpicture_region_t *p_region = subpicture_region_ForPicture(NULL, picture);
    assert(p_region != NULL);
    vlc_spu_regions_push( &subpicture->regions, p_region );

    ret = vlc_gl_sub_renderer_Prepare(sr, subpicture);
    assert(ret == VLC_SUCCESS);
    GL_ASSERT_NOERROR(&api.vt);
    api.vt.GetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo_binding);
    assert((GLuint)fbo_binding == out_fb);

    ret = vlc_gl_sub_renderer_Draw(sr);
    assert(ret == VLC_SUCCESS);
    GL_ASSERT_NOERROR(&api.vt);
    api.vt.GetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo_binding);
    assert((GLuint)fbo_binding == out_fb);

    api.vt.Finish();
    GL_ASSERT_NOERROR(&api.vt);

    /* Disable pixel packing to use glReadPixels. */
    api.vt.BindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    GL_ASSERT_NOERROR(&api.vt);

    uint8_t pixel[4];

    fprintf(stderr, "Results:\n");
    for (size_t i = 0; i < 4; ++i)
    {
        for (size_t j = 0; j < 4; ++j)
        {
            api.vt.ReadPixels(j, 3 - i, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
            fprintf(stderr, "    %u:%u:%u:%u", pixel[0], pixel[1], pixel[2], pixel[3]);
        }
        fprintf(stderr, "\n");
    }

    for (size_t i = 0; i < points_count; ++i)
    {
        api.vt.ReadPixels(points[i].y, 3 - points[i].x, 1, 1, GL_RGBA,
                          GL_UNSIGNED_BYTE, &pixel);
        GL_ASSERT_NOERROR(&api.vt);
        fprintf(stderr, " %s: Checking point %dx%d, need %d:%d:%d, got %d:%d:%d\n",
                 OrientToString(orientation), points[i].x, points[i].y,
                 points[i].color[0], points[i].color[1], points[i].color[2],
                 pixel[0], pixel[1], pixel[2]);
        assert(memcmp(pixel, points[i].color, sizeof(pixel)) == 0);
    }

    vlc_gl_sub_renderer_Delete(sr);
    GL_ASSERT_NOERROR(&api.vt);

    vlc_gl_interop_Delete(interop);
    GL_ASSERT_NOERROR(&api.vt);

    vlc_gl_ReleaseCurrent(gl);
    vlc_gl_Delete(gl);

    picture_Release(picture);
    vlc_render_subpicture_Delete(subpicture);
}

int main( int argc, char **argv )
{
    (void)argc; (void)argv;
    test_init();

    const char * const vlc_argv[] = {
        "-vvv", "--aout=dummy", "--text-renderer=dummy",
    };

    libvlc_instance_t *vlc = libvlc_new(ARRAY_SIZE(vlc_argv), vlc_argv);
    vlc_object_t *root = &vlc->p_libvlc_int->obj;

    const char *cap =
        (VLC_TEST_OPENGL_API == VLC_OPENGL)     ? "opengl offscreen" :
        (VLC_TEST_OPENGL_API == VLC_OPENGL_ES2) ? "opengl es2 offscreen" :
        NULL;
    assert(cap != NULL);

    fprintf(stderr, "Looking for cap %s\n", cap);

    module_t **providers;
    size_t strict_matches;
    ssize_t provider_available = vlc_module_match(
            cap, NULL, false, &providers, &strict_matches);
    (void)strict_matches;
    free(providers);

    if (provider_available <= 0)
    {
        libvlc_release(vlc);
        return 77;
    }

    struct vlc_decoder_device *device =
        vlc_decoder_device_Create(root, NULL);
    vlc_gl_t *gl = vlc_gl_CreateOffscreen(
            root, device, 3, 3, VLC_TEST_OPENGL_API, NULL, NULL);
    if (device != NULL)
        vlc_decoder_device_Release(device);

    if (gl == NULL)
    {
        libvlc_release(vlc);
        return 77;
    }
    vlc_gl_Delete(gl);

    struct test_point points_normal[] = {
        { 0, 0, red },
        { 0, 1, green },
        { 1, 0, blue },
        { 1, 1, white },
        { 1, 2, black },
        { 2, 1, black },
        { 2, 2, black },
    };
    test_opengl_offscreen(root, ORIENT_NORMAL,
                          points_normal, ARRAY_SIZE(points_normal));

    struct test_point points_rotated_90[] = {
        { 3, 0, red },
        { 2, 0, green },
        { 3, 1, blue },
        { 2, 1, white },
        { 1, 0, black },
        { 2, 2, black },
        { 3, 3, black },
    };
    test_opengl_offscreen(root, ORIENT_ROTATED_90,
                          points_rotated_90, ARRAY_SIZE(points_rotated_90));

    struct test_point points_rotated_180[] = {
        { 3, 3, red },
        { 3, 2, green },
        { 2, 3, blue },
        { 2, 2, white },
        { 2, 1, black },
        { 1, 2, black },
        { 1, 1, black },
    };
    test_opengl_offscreen(root, ORIENT_ROTATED_180,
                          points_rotated_180, ARRAY_SIZE(points_rotated_180));

    struct test_point points_rotated_270[] = {
        { 0, 3, red },
        { 1, 3, green },
        { 0, 2, blue },
        { 1, 2, white },
        { 2, 3, black },
        { 1, 1, black },
        { 0, 0, black },
    };
    test_opengl_offscreen(root, ORIENT_ROTATED_270,
                          points_rotated_270, ARRAY_SIZE(points_rotated_270));

    struct test_point points_hflip[] = {
        { 0, 3, red },
        { 0, 2, green },
        { 1, 3, blue },
        { 1, 2, white },
        { 1, 1, black },
        { 2, 2, black },
        { 2, 1, black },

    };
    test_opengl_offscreen(root, ORIENT_HFLIPPED,
                          points_hflip, ARRAY_SIZE(points_hflip));

    struct test_point points_vflip[] = {
        { 3, 0, red },
        { 3, 1, green },
        { 2, 0, blue },
        { 2, 1, white },
        { 2, 2, black },
        { 1, 1, black },
        { 1, 2, black },
    };
    test_opengl_offscreen(root, ORIENT_VFLIPPED,
                          points_vflip, ARRAY_SIZE(points_vflip));

    struct test_point points_transposed[] = {
        { 3, 3, red },
        { 2, 3, green },
        { 3, 2, blue },
        { 2, 2, white },
        { 1, 3, black },
        { 2, 1, black },
        { 3, 0, black },
    };
    test_opengl_offscreen(root, ORIENT_TRANSPOSED,
                          points_transposed, ARRAY_SIZE(points_transposed));

    struct test_point points_antitransposed[] = {
        { 0, 0, red },
        { 1, 0, green },
        { 0, 1, blue },
        { 1, 1, white },
        { 2, 0, black },
        { 1, 2, black },
        { 0, 3, black },
    };
    test_opengl_offscreen(root, ORIENT_ANTI_TRANSPOSED,
                          points_antitransposed, ARRAY_SIZE(points_antitransposed));

    libvlc_release(vlc);
    return 0;
}
