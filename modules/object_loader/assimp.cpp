/*****************************************************************************
 * assimp.cpp: Assimp 3d object loader
 *****************************************************************************
 * Copyright (C) 2002-2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Adrien Maglo <magsoft@videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <vector>
#include <unordered_map>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_object_loader.h>
#include <vlc_stream.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "../misc/webservices/json.h"


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open (vlc_object_t *p_this);
static void Close(vlc_object_t *);

#define PREFIX "assimp-"

vlc_module_begin()
    set_shortname(N_("Assimp"))
    set_category(CAT_3D)
    set_subcategory(SUBCAT_3D_OBJECT_LOADER)
    set_description(N_("Assimp 3d object loader"))
    set_capability("object loader", 10)

    add_shortcut("assimp")
    set_callbacks(Open, Close)
vlc_module_end()


scene_t *loadScene(object_loader_t *p_loader, const char *psz_path);


struct object_loader_sys_t
{
    Assimp::Importer importer;
};


static int Open(vlc_object_t *p_this)
{
    object_loader_t *p_loader = (object_loader_t *)p_this;

    p_loader->p_sys = new(std::nothrow) object_loader_sys_t();
    if (unlikely(p_loader->p_sys == NULL))
        return VLC_ENOMEM;

    p_loader->loadScene = loadScene;

    return VLC_SUCCESS;
}


static void Close(vlc_object_t *p_this)
{
    object_loader_t *p_loader = (object_loader_t *)p_this;

    delete p_loader->p_sys;
}


json_value *getJSONValues(object_loader_t *p_loader, const char *psz_path)
{
    char *psz_url = vlc_path2uri(psz_path, NULL);
    stream_t *p_stream = vlc_stream_NewURL(p_loader, psz_url);
    if (p_stream == NULL)
        return NULL;

    char psz_buffer[10 * 1024] = {0};
    unsigned i_ret = 0;
    while (true)
    {
        int i_read = sizeof(psz_buffer) - i_ret;

        i_read = vlc_stream_Read(p_stream, psz_buffer + i_ret, i_read);
        if( i_read <= 0 )
            break;

        i_ret += i_read;
    }
    vlc_stream_Delete(p_stream);
    psz_buffer[i_ret] = 0;

    json_settings settings;
    char psz_error[128];
    memset (&settings, 0, sizeof(json_settings));
    json_value *root = json_parse_ex(&settings, psz_buffer, psz_error);
    if (root == NULL)
    {
        msg_Warn(p_loader, "Cannot parse json data: %s", psz_error);
        return NULL;
    }

    return root;
}


std::string getDirectoryPath(std::string path)
{
    std::string ret;
    size_t pos = path.find_last_of(DIR_SEP);
    if (pos != std::string::npos)
        ret = path.substr(0, pos + 1);
    return ret;
}


int getViewer(json_value *root, float *position)
{
    json_value viewerValue = (*root)["viewer"];
    if (viewerValue.type == json_none)
        return VLC_EGENERIC;

    json_value positionValue = viewerValue["position"];
    if (positionValue.type == json_none)
        return VLC_EGENERIC;

    for (unsigned i = 0; i < 3; ++i)
    {
        json_value positionCoordValue = positionValue[i];
        if (positionCoordValue.type == json_none)
            return VLC_EGENERIC;
        const double dp = positionCoordValue;
        position[i] = dp;
    }

    return VLC_SUCCESS;
}


int getScreenParams(json_value *root, scene_t *p_scene)
{
    json_value screenValue = (*root)["screen"];
    if (screenValue.type == json_none)
        return VLC_EGENERIC;

    json_value sizeValue = screenValue["size"];
    if (screenValue.type == json_none)
        return VLC_EGENERIC;

    const double ds = sizeValue;
    p_scene->screenSize = ds;

    json_value positionValue = screenValue["position"];
    if (positionValue.type == json_none)
        return VLC_EGENERIC;

    for (unsigned i = 0; i < 3; ++i)
    {
        json_value positionCoordValue = positionValue[i];
        if (positionCoordValue.type == json_none)
            return VLC_EGENERIC;
        const double dp = positionCoordValue;
        p_scene->screenPosition[i] = dp;
    }

    json_value normalDirValue = screenValue["normalDir"];
    if (normalDirValue.type == json_none)
        return VLC_EGENERIC;

    for (unsigned i = 0; i < 3; ++i)
    {
        json_value normalDirCoordValue = normalDirValue[i];
        if (normalDirCoordValue.type == json_none)
            return VLC_EGENERIC;
        const double ds = normalDirCoordValue;
        p_scene->screenNormalDir[i] = ds;
    }

    json_value fitDirValue = screenValue["fitDir"];
    if (fitDirValue.type == json_none)
        return VLC_EGENERIC;

    for (unsigned i = 0; i < 3; ++i)
    {
        json_value fitDirCoordValue = fitDirValue[i];
        if (fitDirCoordValue.type == json_none)
            return VLC_EGENERIC;
        const double ds = fitDirCoordValue;
        p_scene->screenFitDir[i] = ds;
    }

    return VLC_SUCCESS;
}


int getModel(json_value *root, std::string &modelPath, float &scale, float *rotationAngles)
{
    json_value modelValue = (*root)["model"];
    if (modelValue.type == json_none)
        return VLC_EGENERIC;

    json_value scaleValue = modelValue["scale"];
    if (scaleValue.type == json_none)
        return VLC_EGENERIC;

    const double ds = scaleValue;
    scale = ds;

    json_value pathValue = modelValue["path"];
    if (pathValue.type == json_none)
        return VLC_EGENERIC;

    modelPath = std::string(pathValue);

    json_value rotationAnglesValue = modelValue["rotationAngles"];
    if (rotationAnglesValue.type == json_none)
        return VLC_EGENERIC;

    for (unsigned i = 0; i < 3; ++i)
    {
        json_value rotationAngleValue = rotationAnglesValue[i];
        if (rotationAngleValue.type == json_none)
            return VLC_EGENERIC;
        const double ds = rotationAngleValue;
        rotationAngles[i] = ds;
    }

    return VLC_SUCCESS;
}


static inline void aiColor3DToArray(aiColor3D c, float *arr)
{
    arr[0] = c.r; arr[1] = c.g; arr[2] = c.b;
}


static inline void aiVector3DToArray(aiVector3D v, float *arr)
{
    arr[0] = v.x; arr[1] = v.y; arr[2] = v.z;
}


static void addNodes(object_loader_t *p_loader, aiNode *node,
                     aiMatrix4x4 parentGlobalTransform,
                     std::vector<scene_object_t *> &objects,
                     const aiScene *myAiScene,
                     std::unordered_map<unsigned, unsigned> &aiTextureMap,
                     std::unordered_map<unsigned, unsigned> &aiMeshMap)
{
    // Transpose the node transformation matrix to convert to OpenGL matrices.
    aiMatrix4x4 transform = node->mTransformation.Transpose() * parentGlobalTransform;

    for (unsigned c = 0; c < node->mNumChildren; ++c)
        addNodes(p_loader, node->mChildren[c], transform, objects, myAiScene,
                 aiTextureMap, aiMeshMap);

    for (unsigned j = 0; j < node->mNumMeshes; ++j)
    {
        unsigned i_mesh = node->mMeshes[j];
        unsigned i_texture = myAiScene->mMeshes[i_mesh]->mMaterialIndex;

        auto texMapIt = aiTextureMap.find(i_texture);
        if (texMapIt == aiTextureMap.end())
        {
            msg_Warn(p_loader, "Could not add the current object as its texture could not be loaded");
            continue;
        }

        auto meshMapIt = aiMeshMap.find(i_mesh);
        if (meshMapIt == aiMeshMap.end())
        {
            msg_Warn(p_loader, "Could not add the current object as its mesh could not be loaded");
            continue;
        }

        unsigned texMap = texMapIt->second;
        unsigned meshMap = meshMapIt->second;
        objects.push_back(scene_object_New((float *)&transform, meshMap, texMap));
    }
}


scene_t *loadScene(object_loader_t *p_loader, const char *psz_path)
{
    object_loader_sys_t *p_sys = p_loader->p_sys;
    scene_t *p_scene = NULL;

    json_value *root = getJSONValues(p_loader, psz_path);
    if (unlikely(root == NULL))
    {
        msg_Err(p_loader, "Cannot get scene information");
        return NULL;
    }

    std::string modelPath;
    float scale;
    float rotationAngles[3];
    int ret = getModel(root, modelPath, scale, rotationAngles);
    if (ret != VLC_SUCCESS)
    {
        msg_Err(p_loader, "Could not get the model information");
        json_value_free(root);
        return NULL;
    }

    std::string modelFullPath = getDirectoryPath(psz_path) + modelPath;

    const aiScene *myAiScene = p_sys->importer.ReadFile(
        modelFullPath.c_str(),
        aiProcess_GenNormals             |
        aiProcess_CalcTangentSpace       |
        aiProcess_Triangulate            |
        aiProcess_JoinIdenticalVertices  |
        aiProcess_SortByPType            |
        aiProcess_TransformUVCoords);

    if (!myAiScene)
    {
        msg_Err(p_loader, "%s", p_sys->importer.GetErrorString());
        json_value_free(root);
        return NULL;
    }

    // Meshes
    std::vector<scene_mesh_t *> meshes;
    std::unordered_map<unsigned, unsigned> aiMeshMap;
    for (unsigned i = 0; i < myAiScene->mNumMeshes; ++i)
    {
        aiMesh *myAiMesh = myAiScene->mMeshes[i];

        unsigned *facesIndices = new unsigned[3 * myAiMesh->mNumFaces];
        for (unsigned f = 0; f < myAiMesh->mNumFaces; ++f)
        {
            assert(myAiMesh->mFaces[f].mNumIndices == 3);
            memcpy(facesIndices + 3 * f, myAiMesh->mFaces[f].mIndices, 3 * sizeof(*facesIndices));
        }

        float *textureCoords = NULL;
        if (myAiMesh->HasTextureCoords(0) && myAiMesh->mNumUVComponents[0] == 2)
        {
            textureCoords = new float[2 * myAiMesh->mNumVertices];
            for (unsigned v = 0; v < myAiMesh->mNumVertices; ++v)
            {
                textureCoords[2 * v] = myAiMesh->mTextureCoords[0][v].x;
                textureCoords[2 * v + 1] = 1 - myAiMesh->mTextureCoords[0][v].y;
            }
        }

        scene_mesh_t *p_mesh = scene_mesh_New(myAiMesh->mNumVertices, myAiMesh->mNumFaces,
                                              (float *)myAiMesh->mVertices,
                                              (float *)myAiMesh->mNormals,
                                              (float *)myAiMesh->mTangents,
                                              textureCoords, facesIndices);
        delete[] facesIndices;
        delete[] textureCoords;

        if (p_mesh == NULL)
        {
            msg_Warn(p_loader, "Could not load the mesh number %d", i);
            continue;
        }

        meshes.push_back(p_mesh);
        aiMeshMap[i] = meshes.size() - 1;
    }

    // Materials
    std::vector<scene_material_t  *> materials;
    std::unordered_map<unsigned, unsigned> aiTextureMap;
    for (unsigned i = 0; i < myAiScene->mNumMaterials; ++i)
    {
        aiMaterial *myAiMaterial = myAiScene->mMaterials[i];

        aiColor3D diffuseColor, emissiveColor, ambientColor;
        myAiMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor);
        myAiMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor);
        myAiMaterial->Get(AI_MATKEY_COLOR_AMBIENT, ambientColor);

        aiString matName;
        myAiMaterial->Get(AI_MATKEY_NAME, matName);
        msg_Dbg(p_loader, "Material: %s", matName.C_Str());

        unsigned i_nbProperties = myAiMaterial->mNumProperties;
        for (unsigned k = 0; k < i_nbProperties; ++k)
            msg_Dbg(p_loader, "Property name: %s", myAiMaterial->mProperties[k]->mKey.C_Str());

        scene_material_t *p_material = scene_material_New();
        if (p_material == NULL)
            continue;

        unsigned i_nbTextures = myAiMaterial->GetTextureCount(aiTextureType_DIFFUSE);
        if (i_nbTextures > 0)
        {
            aiString path;
            myAiMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &path, NULL, NULL, NULL, NULL, NULL);

            std::string pathStr(path.C_Str());

            if (pathStr.size() > 1 && pathStr[0] == '*')
            {
                // Embeded texture
                unsigned i_textureId = atoi(pathStr.data() + 1);

                aiTexture *tex = myAiScene->mTextures[i_textureId];

                p_material->p_baseColorTex = scene_material_LoadTextureFromData(p_loader, (char *)tex->pcData, tex->mWidth*tex->mHeight);
                if (p_material->p_baseColorTex == NULL)
                    msg_Warn(p_loader, "Could not load the base color texture from embedded data");
            }
            else
            {
                // External texutre
                std::string baseTexFileName = pathStr; //pathStr.substr(strlen("C:\\sourceimages\\"));
                std::string::size_type baseTexFileNameEnd = baseTexFileName.find_last_of("_");
                if (baseTexFileNameEnd != std::string::npos)
                    baseTexFileName = baseTexFileName.substr(0, baseTexFileNameEnd);

                char *psz_dataDir =
                    config_GetSysPath(VLC_PKG_DATA_DIR,
                                      "VirtualTheater" DIR_SEP);
                if (unlikely(psz_dataDir == NULL))
                {
                    msg_Warn(p_loader, "Could not get the main data directory");
                    continue;
                }

                std::string baseTexPath(psz_dataDir);
                #define TEXTURE_DIR DIR_SEP "VirtualTheater" DIR_SEP "Textures" DIR_SEP
                //baseTexPath += TEXTURE_DIR + baseTexFileName;
                baseTexPath += baseTexFileName;

                std::string diffuseTexPath = baseTexPath;
                if (baseTexFileNameEnd != std::string::npos)
                    diffuseTexPath += + "_BaseColor.png";

                msg_Dbg(p_loader, "Base color texture path: %s", diffuseTexPath.c_str());
                p_material->p_baseColorTex = scene_material_LoadTexture(p_loader, diffuseTexPath.c_str());
                if (p_material->p_baseColorTex == NULL)
                    msg_Warn(p_loader, "Could not load the base color texture");

                if (baseTexFileNameEnd != std::string::npos)
                {
                    std::string metalnessTexPath = baseTexPath + "_Metalness.png";
                    msg_Dbg(p_loader, "Metalness texture path: %s", metalnessTexPath.c_str());
                    p_material->p_metalnessTex = scene_material_LoadTexture(p_loader, metalnessTexPath.c_str());
                    if (p_material->p_metalnessTex == NULL)
                        msg_Warn(p_loader, "Could not load the metalness texture");

                    std::string normalTexPath = baseTexPath + "_Normal.png";
                    msg_Dbg(p_loader, "Normal texture path: %s", normalTexPath.c_str());
                    p_material->p_normalTex = scene_material_LoadTexture(p_loader, normalTexPath.c_str());
                    if (p_material->p_normalTex == NULL)
                        msg_Warn(p_loader, "Could not load the normal texture");

                    std::string roughnessTexPath = baseTexPath + "_Roughness.png";
                    msg_Dbg(p_loader, "Roughness texture path: %s", roughnessTexPath.c_str());
                    p_material->p_roughnessTex = scene_material_LoadTexture(p_loader, roughnessTexPath.c_str());
                    if (p_material->p_roughnessTex == NULL)
                        msg_Warn(p_loader, "Could not load the roughness texture");
                }
            }
        }

        aiColor3DToArray(diffuseColor, p_material->diffuse_color);
        aiColor3DToArray(emissiveColor, p_material->emissive_color);
        aiColor3DToArray(ambientColor, p_material->ambient_color);

        materials.push_back(p_material);
        aiTextureMap[i] = materials.size() - 1;
    }

    // Objects
    std::vector<scene_object_t *> objects;
    addNodes(p_loader, myAiScene->mRootNode, aiMatrix4x4(), objects,
             myAiScene, aiTextureMap, aiMeshMap);

    // Ligths
    std::vector<scene_light_t *> lights;
    for (unsigned i = 0; i < myAiScene->mNumLights; ++i)
    {
        aiLight *myAiLight = myAiScene->mLights[i];

        msg_Dbg(p_loader, "Light name: %s", myAiLight->mName.C_Str());

        scene_light_t *p_light = scene_light_New();
        if (p_light == NULL)
            continue;

        aiColor3DToArray(myAiLight->mColorDiffuse, p_light->colorDiffuse);
        aiColor3DToArray(myAiLight->mColorAmbient, p_light->colorAmbient);
        aiColor3DToArray(myAiLight->mColorSpecular, p_light->colorSpecular);
        aiVector3DToArray(myAiLight->mDirection, p_light->direction);
        aiVector3DToArray(myAiLight->mPosition, p_light->position);
        p_light->attenuationConstant = myAiLight->mAttenuationConstant;
        p_light->attenuationLinear = myAiLight->mAttenuationLinear;
        p_light->attenuationQuadratic = myAiLight->mAttenuationQuadratic;

        msg_Info(p_loader, "POSITION:    %f %f %f", myAiLight->mPosition.x,
                 myAiLight->mPosition.y, myAiLight->mPosition.z);
        msg_Info(p_loader, "DIRECTION:   %f %f %f", myAiLight->mDirection.x,
                 myAiLight->mDirection.y, myAiLight->mDirection.z);
        msg_Info(p_loader, "DIFFUSE:     %f %f %f", myAiLight->mColorDiffuse.r,
                 myAiLight->mColorDiffuse.g, myAiLight->mColorDiffuse.b);
        msg_Info(p_loader, "AMBIANT:     %f %f %f", myAiLight->mColorAmbient.r,
                 myAiLight->mColorAmbient.g, myAiLight->mColorAmbient.b);
        msg_Info(p_loader, "SPECULAR:    %f %f %f", myAiLight->mColorSpecular.r,
                 myAiLight->mColorSpecular.g, myAiLight->mColorSpecular.b);
        msg_Info(p_loader, "KC: %f / KL: %f / KQ: %f",
                 p_light->attenuationConstant, p_light->attenuationLinear,
                 p_light->attenuationQuadratic);


        lights.push_back(p_light);
    }

    p_scene = scene_New(objects.size(), meshes.size(), materials.size(),
                        lights.size());
    if (unlikely(p_scene == NULL))
    {
        json_value_free(root);
        return NULL;
    }

    std::copy(objects.begin(), objects.end(), p_scene->objects);
    std::copy(meshes.begin(), meshes.end(), p_scene->meshes);
    std::copy(materials.begin(), materials.end(), p_scene->materials);
    std::copy(lights.begin(), lights.end(), p_scene->lights);

    float position[3];
    ret = getViewer(root, position);
    if (ret != VLC_SUCCESS)
    {
        msg_Err(p_loader, "Could not het the viewer information");
        scene_Release(p_scene);
        json_value_free(root);
        return NULL;
    }

    scene_CalcTransformationMatrix(p_scene, scale, rotationAngles);
    scene_CalcHeadPositionMatrix(p_scene, position);

    memcpy(p_scene->initialPosition, position, 3 * sizeof(float));

    ret = getScreenParams(root, p_scene);
    if (ret != VLC_SUCCESS)
    {
        msg_Err(p_loader, "Could not get the virtual screen information");
        scene_Release(p_scene);
        json_value_free(root);
        return NULL;
    }

    msg_Dbg(p_loader, "3D scene loaded with %d object(s), %d mesh(es) and %d texture(s)",
            p_scene->nObjects, p_scene->nMeshes, p_scene->nMaterials);

    json_value_free(root);
    return p_scene;
}
