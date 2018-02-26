
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_modules.h>
#include <vlc_object_loader.h>
#include <vlc_vout.h>

#include "../libvlc.h"


object_loader_t *objLoader_get(vlc_object_t *p_parent)
{
    object_loader_t *p_objLoader = vlc_custom_create(p_parent, sizeof(*p_objLoader), "Object loader");
    if (unlikely(p_objLoader == NULL))
        return NULL;

    p_objLoader->p_module = module_need(p_objLoader, "object loader", NULL, false);
    if (unlikely(p_objLoader->p_module == NULL))
    {
        vlc_object_release(p_objLoader);
        return NULL;
    }

    return p_objLoader;
}


void objLoader_release(object_loader_t* p_objLoader)
{
    module_unneed(p_objLoader, p_objLoader->p_module);
    vlc_object_release(p_objLoader);
}


scene_t *objLoader_loadScene(object_loader_t *p_objLoader, const char *psz_path)
{
    if (p_objLoader->loadScene == NULL)
        return NULL;

    return p_objLoader->loadScene(p_objLoader, psz_path);
}


scene_object_t *scene_object_New(float *transformMatrix, unsigned meshId,
                                 unsigned textureId)
{
    scene_object_t *p_object = (scene_object_t *)malloc(sizeof(scene_object_t));
    if (unlikely(p_object == NULL))
        return NULL;

    memcpy(p_object->transformMatrix, transformMatrix, sizeof(p_object->transformMatrix));
    p_object->meshId = meshId;
    p_object->textureId = textureId;

    return p_object;
}


void scene_object_Release(scene_object_t *p_object)
{
    free(p_object);
}


scene_mesh_t *scene_mesh_New(unsigned nVertices, unsigned nFaces,
                             float *vCoords, float *tCoords,
                             unsigned int *faces)
{
    scene_mesh_t *p_mesh = (scene_mesh_t *)malloc(sizeof(scene_mesh_t));
    if (unlikely(p_mesh == NULL))
        return NULL;

    p_mesh->nVertices = nVertices;
    p_mesh->nFaces = nFaces;

    // Allocations.
    p_mesh->vCoords = (float *)malloc(3 * nVertices * sizeof(float));
    if (unlikely(p_mesh->vCoords == NULL))
    {
        free(p_mesh);
        return NULL;
    }

    p_mesh->tCoords = (float *)malloc(2 * nVertices * sizeof(float));
    if (unlikely(p_mesh->tCoords == NULL))
    {
        free(p_mesh->vCoords);
        free(p_mesh);
        return NULL;
    }

    p_mesh->faces = (unsigned *)malloc(3 * nFaces * sizeof(unsigned));
    if (unlikely(p_mesh->faces == NULL))
    {
        free(p_mesh->tCoords);
        free(p_mesh->vCoords);
        free(p_mesh);
        return NULL;
    }

    // Copy the mesh data.
    memcpy(p_mesh->vCoords, vCoords, 3 * nVertices * sizeof(float));
    if (tCoords != NULL) // A mesh can have no texture coordinates
        memcpy(p_mesh->tCoords, tCoords, 2 * nVertices * sizeof(float));
    memcpy(p_mesh->faces, faces, 3 * nFaces * sizeof(unsigned));

    return p_mesh;
}


void scene_mesh_Release(scene_mesh_t *p_mesh)
{
    free(p_mesh->faces);
    free(p_mesh->tCoords);
    free(p_mesh->vCoords);
    free(p_mesh);
}


scene_material_t *scene_material_New(void)
{
    scene_material_t *p_material = (scene_material_t *)calloc(1, sizeof(scene_material_t));
    if (unlikely(p_material == NULL))
        return NULL;

    return p_material;
}


int scene_material_LoadTexture(object_loader_t *p_loader, scene_material_t *p_material,
                               const char *psz_path)
{
    p_material->psz_path = strdup(psz_path);

    image_handler_t *p_imgHandler = image_HandlerCreate(p_loader);
    video_format_t fmt_in, fmt_out;
    video_format_Init(&fmt_in, 0);
    video_format_Init(&fmt_out, VLC_CODEC_RGBA);

    char *psz_url = vlc_path2uri(psz_path, NULL);
    p_material->p_pic = image_ReadUrl(p_imgHandler, psz_url, &fmt_in, &fmt_out);
    free(psz_url);

    video_format_Clean(&fmt_in);
    video_format_Clean(&fmt_out);
    image_HandlerDelete(p_imgHandler);

    if (p_material->p_pic == NULL)
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}


void scene_material_Release(scene_material_t *p_material)
{
    if (p_material->p_pic != NULL)
        picture_Release(p_material->p_pic);
    free(p_material->psz_path);
    free(p_material);
}


static void matrixMul(float ret[], const float m1[], const float m2[])
{
    for (unsigned i = 0; i < 4; i++)
    {
        for (unsigned j = 0; j < 4; j++)
        {
            float sum = 0;
            for (unsigned k = 0; k < 4; k++) {
                sum = sum + m1[4 * i + k] * m2[4 * k + j];
            }
            ret[4 * i + j] = sum;
        }
    }
}


void scene_CalcTransformationMatrix(scene_t *p_scene)
{
    memset(p_scene->transformMatrix, 0, sizeof(p_scene->transformMatrix));

    // Compute the axis aligned mesh bounding box.
    float min[] = {FLT_MAX, FLT_MAX, FLT_MAX};
    float max[] = {FLT_MIN, FLT_MIN, FLT_MIN};

    for (unsigned m = 0; m < p_scene->nMeshes; ++m)
    {
        scene_mesh_t *p_mesh = p_scene->meshes[m];
        for (unsigned v = 0; v < p_mesh->nVertices; ++v)
        {
            for (unsigned a = 0; a < 3; ++a)
            {
                float val = p_mesh->vCoords[3 * v + a];
                if (val < min[a])
                    min[a] = val;
                if (val > max[a])
                    max[a] = val;
            }
        }
    }

    // Compute the mesh center.
    float c[] = {min[0] + (max[0] - min[0]) / 2.f,
                 min[1] + (max[1] - min[1]) / 2.f,
                 min[2] + (max[2] - min[2]) / 2.f};

    float f_diag = sqrt((max[0] - min[0]) * (max[0] - min[0])
                        + (max[1] - min[1]) * (max[1] - min[1])
                        + (max[2] - min[2]) * (max[2] - min[2]));

    float s = 100 / f_diag;

    // Set the scene transformation matrix.
    const float m1[] = {
    /*          x          y          z         w */
                s,       0.f,       0.f,      0.f,
              0.f,         s,       0.f,      0.f,
              0.f,       0.f,         s,      0.f,
        -s * c[0], -s * c[1], -s * c[2],        1
    };

    float st, ct;
    sincosf(-M_PI / 2.f, &st, &ct);

    // Translation defines
    #define TX 0.2f
    #define TY 6.f
    #define TZ 0.f
    const float m2[] = {
        /*   x    y    z    w */
            ct, 0.f, -st, 0.f,
           0.f, 1.f, 0.f, 0.f,
            st, 0.f,  ct, 0.f,
            TX,  TY,  TZ, 1.f
    };
    #undef TX
    #undef TY
    #undef TZ

    float m[16];
    matrixMul(m, m1, m2);

    memcpy(p_scene->transformMatrix, m, sizeof(m));
}


scene_t *scene_New(unsigned nObjects, unsigned nMeshes, unsigned nMaterials)
{
    scene_t *p_scene = (scene_t *)malloc(sizeof(scene_t));
    if (unlikely(p_scene == NULL))
        return NULL;

    // Allocations.
    p_scene->objects = (scene_object_t **)malloc(nObjects * sizeof(scene_object_t *));
    if (unlikely(p_scene->objects == NULL))
    {
        free(p_scene);
        return NULL;
    }

    p_scene->meshes = (scene_mesh_t **)malloc(nMeshes * sizeof(scene_mesh_t *));
    if (unlikely(p_scene->meshes == NULL))
    {
        free(p_scene->objects);
        free(p_scene);
        return NULL;
    }

    p_scene->materials = (scene_material_t **)malloc(nMaterials * sizeof(scene_material_t *));
    if (unlikely(p_scene->materials == NULL))
    {
        free(p_scene->meshes);
        free(p_scene->objects);
        free(p_scene);
        return NULL;
    }

    p_scene->nObjects = nObjects;
    p_scene->nMeshes = nMeshes;
    p_scene->nMaterials = nMaterials;

    return p_scene;
}


void scene_Release(scene_t *p_scene)
{
    if (p_scene == NULL)
        return;

    for (unsigned i = 0; i < p_scene->nObjects; ++i)
        scene_object_Release(p_scene->objects[i]);

    for (unsigned i = 0; i < p_scene->nMeshes; ++i)
        scene_mesh_Release(p_scene->meshes[i]);

    for (unsigned i = 0; i < p_scene->nMaterials; ++i)
        scene_material_Release(p_scene->materials[i]);

    free(p_scene);
}
