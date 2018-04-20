#ifndef VLC_OBJECT_LOADER_H
#define VLC_OBJECT_LOADER_H

#include <float.h>

#include <vlc_common.h>
#include <vlc_image.h>
#include <vlc_url.h>


typedef struct object_loader_t object_loader_t;
typedef struct object_loader_sys_t object_loader_sys_t;


typedef struct
{
    float transformMatrix[16];
    unsigned meshId;
    unsigned textureId;
} scene_object_t;


typedef struct
{
    float *vCoords;
    float *tCoords;
    float *nCoords;
    float *tanCoords;
    unsigned *faces;

    unsigned nVertices;
    unsigned nFaces;

    float boundingSquareRadius;
} scene_mesh_t;


typedef struct
{
    picture_t *p_baseColorTex;
    picture_t *p_metalnessTex;
    picture_t *p_normalTex;
    picture_t *p_roughnessTex;

    float diffuse_color[3]; // RGB
    float emissive_color[3];
    float ambient_color[3];
} scene_material_t;


typedef struct
{
    float angleOuterCone;
    float attenuationConstant;
    float attenuationLinear;
    float attenuationQuadratic;

    float colorAmbient[3];
    float colorDiffuse[3];
    float colorSpecular[3];
    float direction[3];
    float position[3];
} scene_light_t;


typedef struct
{
    scene_object_t **objects;
    scene_mesh_t **meshes;
    scene_material_t **materials;
    scene_light_t **lights;

    float transformMatrix[16];
    float headPositionMatrix[16];

    unsigned nObjects;
    unsigned nMeshes;
    unsigned nMaterials;
    unsigned nLights;

    float screenSize;
    float screenPosition[3];
    float screenNormalDir[3];
    float screenFitDir[3];
} scene_t;


struct object_loader_t
{
    struct vlc_common_members obj;

    /* Module */
    module_t* p_module;

    /* Private structure */
    object_loader_sys_t* p_sys;

    image_handler_t *p_imgHandler;
    video_format_t texPic_fmt_in, texPic_fmt_out;

    scene_t * (*loadScene)(object_loader_t *p_loader, const char *psz_path);
};


VLC_API object_loader_t *objLoader_get(vlc_object_t *p_parent);
VLC_API void objLoader_release(object_loader_t* p_objLoader);
VLC_API scene_t *objLoader_loadScene(object_loader_t *p_objLoader, const char *psz_path);
VLC_API scene_object_t *scene_object_New(float *transformMatrix, unsigned meshId,
                                         unsigned textureId);
VLC_API void scene_object_Release(scene_object_t *p_object);
VLC_API scene_mesh_t *scene_mesh_New(unsigned nVertices, unsigned nFaces,
                                     float *vCoords, float *nCoords, float *tanCoords,
                                     float *tCoords, unsigned int *faces);
VLC_API void scene_mesh_Release(scene_mesh_t *p_mesh);
VLC_API float scene_mesh_computeBoundingSquareRadius(scene_mesh_t *p_object);
VLC_API scene_material_t *scene_material_New(void);
VLC_API picture_t *scene_material_LoadTexture(object_loader_t *p_loader, const char *psz_path);
VLC_API void scene_material_Release(scene_material_t *p_material);
VLC_API scene_light_t *scene_light_New(void);
VLC_API void scene_light_Release(scene_light_t *p_light);
VLC_API void scene_CalcTransformationMatrix(scene_t *p_scene, float sf, float *rotationAngles);
VLC_API void scene_CalcHeadPositionMatrix(scene_t *p_scene, float *p);
VLC_API scene_t *scene_New(unsigned nObjects, unsigned nMeshes, unsigned nTextures,
                           unsigned nLights);
VLC_API void scene_Release(scene_t *p_scene);


#endif // VLC_OBJECT_LOADER_H
