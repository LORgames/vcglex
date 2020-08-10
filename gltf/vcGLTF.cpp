#include "vcGLTF.h"

#include "vcGL/gl/vcMesh.h"
#include "vcGL/gl/vcShader.h"
#include "vcGL/gl/vcTexture.h"

#include "udPlatform.h"
#include "udJSON.h"
#include "udPlatformUtil.h"
#include "udFile.h"
#include "udStringUtil.h"

enum vcGLTFTypes
{
  vcGLTFType_Int8 = 5120,
  vcGLTFType_UInt8 = 5121,
  vcGLTFType_Int16 = 5122,
  vcGLTFType_UInt16 = 5123,
  vcGLTFType_Int32 = 5124,
  vcGLTFType_Uint32 = 5125,
  vcGLTFType_F32 = 5126,
  vcGLTFType_F64 = 5130,

  vcGLTFType_Nearest = 9728,
  vcGLTFType_Linear = 9729,

  vcGLTFType_ClampEdge = 33071,
  vcGLTFType_MirroredRepeat = 33648,
  vcGLTFType_Repeat = 10497,
};

enum vcGLTFFeatures
{
  vcRSF_HasTangents,
  vcRSF_HasUVSet0,
  vcRSF_HasUVSet1,
  vcRSF_HasColour,
  vcRSF_HasSkinning,

  vcRSF_Count
};

enum vcGLTFFeatureBits
{
  vcRSB_None = 0,

  vcRSB_Tangents = 1 << vcRSF_HasTangents,
  vcRSB_UVSet0 = 1 << vcRSF_HasUVSet0,
  vcRSB_UVSet1 = 1 << vcRSF_HasUVSet1,
  vcRSB_Colour = 1 << vcRSF_HasColour,
  vcRSB_Skinned = 1 << vcRSF_HasSkinning,

  vcRSB_Count = 1 << vcRSF_Count
};

struct vcGLTFNode
{
  udFloat3 translation;
  udFloatQuat rotation;
  udFloat3 scale;

  udFloat4x4 GetMat()
  {
    return udFloat4x4::rotationQuat(rotation, translation) * udFloat4x4::scaleNonUniform(scale);
  }
};

struct vcGLTFMeshInstance
{
  int meshID;
  udFloat4x4 matrix;
};

enum vcGLTF_AlphaMode
{
  vcGLTFAM_Opaque,
  vcGLTFAM_Mask,
  vcGLTFAM_Blend
};

struct vcGLTFMeshPrimitive
{
  vcGLTFFeatureBits features;
  vcMesh *pMesh;

  udFloat4 baseColorFactor;
  int baseColorTexture;
  int baseColorUVSet;

  float metallicFactor;
  float roughnessFactor;
  int metallicRoughnessTexture;
  int metallicRoughnessUVSet;

  float normalScale;
  int normalTexture;
  int normalUVSet;

  int emissiveTexture;
  udFloat3 emissiveFactor;
  int emissiveUVSet;

  int occlusionTexture;
  int occlusionUVSet;
  
  bool doubleSided;
  vcGLTF_AlphaMode alpaMode;
  float alphaCutoff;
};

struct vcGLTFMesh
{
  int numPrimitives;
  vcGLTFMeshPrimitive *pPrimitives;
};

struct vcGLTFBuffer
{
  int64_t byteLength;
  uint8_t *pBytes;
};

struct vcGLTFScene
{
  char *pPath;

  udChunkedArray<vcGLTFMeshInstance> meshInstances;

  int nodeCount;
  vcGLTFNode *pNodes;

  int bufferCount;
  vcGLTFBuffer *pBuffers;

  int meshCount;
  vcGLTFMesh *pMeshes;

  int textureCount;
  vcTexture **ppTextures;
};

struct vcGLTFShader
{
  vcShader* pShader;
  vcShaderConstantBuffer *pVertUniformBuffer;
  vcShaderConstantBuffer *pSkinningUniformBuffer;
  vcShaderConstantBuffer *pFragUniformBuffer;

  vcShaderSampler *pBaseColourSampler;
  vcShaderSampler *pMetallicRoughnessSampler;
  vcShaderSampler *pNormalMapSampler;
  vcShaderSampler *pEmissiveMapSampler;
  vcShaderSampler *pOcclusionMapSampler;
} g_shaderTypes[vcRSB_Count] = {};

#ifndef LIGHT_COUNT
# define LIGHT_COUNT 2
#endif

struct vcGLTFVertInputs
{
  udFloat4x4 u_ViewProjectionMatrix;
  udFloat4x4 u_ModelMatrix;
  udFloat4x4 u_NormalMatrix;

#ifdef USE_MORPHING
  uniform float u_morphWeights[WEIGHT_COUNT];
#endif

#ifdef USE_SKINNING
  float4x4 u_jointMatrix[JOINT_COUNT];
  float4x4 u_jointNormalMatrix[JOINT_COUNT];
#endif
} s_gltfVertInfo = {};

struct vcGLTFVertSkinned
{
  udFloat4x4 u_jointMatrix[72];
  udFloat4x4 u_jointNormalMatrix[72];
} s_gltfVertSkinningInfo = {};

#define MATERIAL_METALLICROUGHNESS
#define USE_PUNCTUAL

struct Light
{
  udFloat3 direction;
  float range;

  udFloat3 color;
  float intensity;

  udFloat3 position;

  float innerConeCos;
  float outerConeCos;

  int type;

  udFloat2 padding;
};

struct vcGLTFVertFragSettings
{
  udFloat3 u_Camera;
  float u_Exposure;
  udFloat3 u_EmissiveFactor;

  float u_NormalScale; // Only used with HAS_NORMALS but serves as padding otherwise

  // Metallic Roughness
#ifdef MATERIAL_METALLICROUGHNESS
  udFloat4 u_BaseColorFactor;
  float u_MetallicFactor;
  float u_RoughnessFactor;
  int u_BaseColorUVSet; // Only needed with HAS_BASE_COLOR_MAP
  int u_MetallicRoughnessUVSet; // Only needed with HAS_METALLIC_ROUGHNESS_MAP

# ifdef HAS_BASECOLOR_UV_TRANSFORM
  float3x3 u_BaseColorUVTransform;
# endif

# ifdef HAS_METALLICROUGHNESS_UV_TRANSFORM
  float3x3 u_MetallicRoughnessUVTransform;
# endif
#endif

  // Specular Glossiness
#ifdef MATERIAL_SPECULARGLOSSINESS
  float u_GlossinessFactor;
  float3 u_SpecularFactor;
  float4 u_DiffuseFactor;

#ifdef HAS_DIFFUSE_MAP
  int u_DiffuseUVSet;
#ifdef HAS_DIFFUSE_UV_TRANSFORM
  float3x3 u_DiffuseUVTransform;
#endif
#endif

#ifdef HAS_SPECULAR_GLOSSINESS_MAP
  int u_SpecularGlossinessUVSet;
#ifdef HAS_SPECULARGLOSSINESS_UV_TRANSFORM
  float3x3 u_SpecularGlossinessUVTransform;
#endif
#endif
#endif

  // Specular / Metalic Override
#ifdef MATERIAL_METALLICROUGHNESS_SPECULAROVERRIDE
  float u_MetallicRoughnessSpecularFactor;
#ifdef HAS_METALLICROUGHNESS_SPECULAROVERRIDE_MAP
  int u_MetallicRougnessSpecularTextureUVSet;
#ifdef HAS_METALLICROUGHNESSSPECULAR_UV_TRANSFORM
  float3x3 u_MetallicRougnessSpecularUVTransform;
#endif
#endif
#endif

  // General Material
  int u_NormalUVSet;
  int u_EmissiveUVSet;
  int u_OcclusionUVSet;
  float u_OcclusionStrength;

  // Alpha mode
  int u_alphaMode; // 0 = OPAQUE, 1 = ALPHAMASK, 2 = BLEND
  float u_AlphaCutoff;

  udFloat2 __padding;

# ifdef HAS_NORMAL_UV_TRANSFORM
  float3x3 u_NormalUVTransform;
# endif

# ifdef HAS_EMISSIVE_UV_TRANSFORM
  float3x3 u_EmissiveUVTransform;
# endif

# ifdef HAS_OCCLUSION_UV_TRANSFORM
  float3x3 u_OcclusionUVTransform;
# endif

#ifdef USE_PUNCTUAL
  Light u_Lights[LIGHT_COUNT];
#endif

#ifdef USE_UBL
  // IBL
  int u_MipCount;
#endif

  // Sheen
#ifdef MATERIAL_SHEEN
  float u_SheenIntensityFactor;
  float3 u_SheenColorFactor;
  float u_SheenRoughness;

#ifdef HAS_SHEEN_COLOR_INTENSITY_MAP
  int u_SheenColorIntensityUVSet;
#ifdef HAS_SHEENCOLORINTENSITY_UV_TRANSFORM
  float3x3 u_SheenColorIntensityUVTransform;
#endif
#endif
#endif

  // Clearcoat
#ifdef MATERIAL_CLEARCOAT
  float u_ClearcoatFactor;
  float u_ClearcoatRoughnessFactor;

  int u_ClearcoatUVSet;
#ifdef HAS_CLEARCOAT_UV_TRANSFORM
  float3x3 u_ClearcoatUVTransform;
#endif

  int u_ClearcoatRoughnessUVSet;
#ifdef HAS_CLEARCOATROUGHNESS_UV_TRANSFORM
  float3x3 u_ClearcoatRoughnessUVTransform;
#endif

  int u_ClearcoatNormalUVSet;
#ifdef HAS_CLEARCOATNORMAL_UV_TRANSFORM
  float3x3 u_ClearcoatNormalUVTransform;
#endif
#endif

  // Anisotropy
#ifdef MATERIAL_ANISOTROPY
  float u_Anisotropy;
  float3 u_AnisotropyDirection;

  int u_AnisotropyUVSet;
#ifdef HAS_ANISOTROPY_UV_TRANSFORM
  float3x3 u_AnisotropyUVTransform;
#endif

  int u_AnisotropyDirectionUVSet;
#ifdef HAS_ANISOTROPY_DIRECTION_UV_TRANSFORM
  float3x3 u_AnisotropyDirectionUVTransform;
#endif
#endif

  // Subsurface
#ifdef MATERIAL_SUBSURFACE
  float u_SubsurfaceScale;
  float u_SubsurfaceDistortion;
  float u_SubsurfacePower;
  float3 u_SubsurfaceColorFactor;
  float u_SubsurfaceThicknessFactor;

#ifdef HAS_SUBSURFACE_COLOR_MAP
  int u_SubsurfaceColorUVSet;
#ifdef HAS_SUBSURFACECOLOR_UV_TRANSFORM
  float3x3 u_SubsurfaceColorUVTransform;
#endif
#endif

#ifdef HAS_SUBSURFACE_THICKNESS_MAP
  int u_SubsurfaceThicknessUVSet;
#ifdef HAS_SUBSURFACETHICKNESS_UV_TRANSFORM
  float3x3 u_SubsurfaceThicknessUVTransform;
#endif
#endif
#endif

  // Thin Film
#ifdef MATERIAL_THIN_FILM
  float u_ThinFilmFactor;
  float u_ThinFilmThicknessMinimum;
  float u_ThinFilmThicknessMaximum;

#ifdef HAS_THIN_FILM_MAP
  int u_ThinFilmUVSet;
#ifdef HAS_THIN_FILM_UV_TRANSFORM
  float3x3 u_ThinFilmUVTransform;
#endif
#endif

#ifdef HAS_THIN_FILM_THICKNESS_MAP
  int u_ThinFilmThicknessUVSet;
#ifdef HAS_THIN_FILM_THICKNESS_UV_TRANSFORM
  float3x3 u_ThinFilmThicknessUVTransform;
#endif
#endif
#endif

#ifdef MATERIAL_IOR
  // IOR (in .x) and the corresponding f0 (in .y)
  float2 u_IOR_and_f0;
#endif

  // Thickness
#ifdef MATERIAL_THICKNESS
  float u_Thickness;

  // Thickness:
#ifdef HAS_THICKNESS_MAP
  int u_ThicknessUVSet;
#ifdef HAS_THICKNESS_UV_TRANSFORM
  float3x3 u_ThicknessUVTransform;
#endif
#endif
#endif

  // Absorption
#ifdef MATERIAL_ABSORPTION
  float3 u_AbsorptionColor;
#endif

  // Transmission
#ifdef MATERIAL_TRANSMISSION
  float u_Transmission;
#endif
} s_gltfFragInfo = {};


void vcGLTF_GenerateGlobalShaders()
{
  vcGLTF_DestroyGlobalShaders();

  struct TypeStuff
  {
    const char *pDefine;
    vcVertexLayoutTypes layoutType0;
    vcVertexLayoutTypes layoutType1;
  };

  TypeStuff types[vcRSF_Count] = {};

  types[vcRSF_HasTangents].pDefine = "HAS_TANGENTS";
  types[vcRSF_HasTangents].layoutType0 = vcVLT_Tangent4;

  types[vcRSF_HasUVSet0].pDefine = "HAS_UV_SET1";
  types[vcRSF_HasUVSet0].layoutType0 = vcVLT_TextureCoords2_0;

  types[vcRSF_HasUVSet1].pDefine = "HAS_UV_SET2";
  types[vcRSF_HasUVSet1].layoutType0 = vcVLT_TextureCoords2_1;

  types[vcRSF_HasColour].pDefine = "HAS_VERTEX_COLOR";
  types[vcRSF_HasColour].layoutType0 = vcVLT_ColourBGRA;

  types[vcRSF_HasSkinning].pDefine = "HAS_SKINNING";
  types[vcRSF_HasSkinning].layoutType0 = vcVLT_BoneIDs;
  types[vcRSF_HasSkinning].layoutType1 = vcVLT_BoneWeights;

  const int RequiredDefines = 2;
  const char* defines[RequiredDefines + vcRSF_Count] = {};
  defines[0] = "MATERIAL_METALLICROUGHNESS";
  defines[1] = "USE_PUNCTUAL";

  const int RequiredVertTypes = 2;
  vcVertexLayoutTypes vltList[RequiredVertTypes + (vcRSF_Count*2)] = {};
  vltList[0] = vcVLT_Position3;
  vltList[1] = vcVLT_Normal3;

  const char *pVertShaderSource = nullptr;
  const char *pFragShaderSource = nullptr;

  vcShader_LoadTextFromFile("asset://assets/shaders/gltfVertexShader", &pVertShaderSource, vcGLSamplerShaderStage_Vertex);
  vcShader_LoadTextFromFile("asset://assets/shaders/gltfFragmentShader", &pFragShaderSource, vcGLSamplerShaderStage_Fragment);

  for (int i = 0; i < vcRSB_Count; ++i)
  {
    if ((i & (vcRSB_UVSet0 | vcRSB_UVSet1)) == vcRSB_UVSet1)
      continue;

    int extraDefines = 0;
    int extraLayouts = 0;
    for (int j = 0; j < vcRSF_Count; ++j)
    {
      if (i & (1 << j))
      {
        defines[RequiredDefines + extraDefines] = types[j].pDefine;
        vltList[RequiredVertTypes + extraLayouts] = types[j].layoutType0;
        
        ++extraDefines;
        ++extraLayouts;

        if (types[j].layoutType1 != vcVLT_Unsupported)
        {
          vltList[RequiredVertTypes + extraLayouts] = types[j].layoutType1;
          ++extraLayouts;
        }
      }
    }

    vcLayout_Sort(&vltList[RequiredVertTypes], extraLayouts);

    vcShader_CreateFromText(&g_shaderTypes[i].pShader, pVertShaderSource, pFragShaderSource, vltList, RequiredVertTypes + extraLayouts, "gltfVertexShader", "gltfFragmentShader", defines, RequiredDefines + extraDefines);

    vcShader_GetConstantBuffer(&g_shaderTypes[i].pVertUniformBuffer, g_shaderTypes[i].pShader, "u_EveryFrame", sizeof(s_gltfVertInfo));
    vcShader_GetConstantBuffer(&g_shaderTypes[i].pSkinningUniformBuffer, g_shaderTypes[i].pShader, "u_SkinningInfo", sizeof(s_gltfVertSkinningInfo));
    vcShader_GetConstantBuffer(&g_shaderTypes[i].pFragUniformBuffer, g_shaderTypes[i].pShader, "u_FragSettings", sizeof(s_gltfFragInfo));

    vcShader_GetSamplerIndex(&g_shaderTypes[i].pBaseColourSampler, g_shaderTypes[i].pShader, "u_BaseColorSampler");
    vcShader_GetSamplerIndex(&g_shaderTypes[i].pMetallicRoughnessSampler, g_shaderTypes[i].pShader, "u_MetallicRoughnessSampler");
    vcShader_GetSamplerIndex(&g_shaderTypes[i].pNormalMapSampler, g_shaderTypes[i].pShader, "u_NormalSampler");
    vcShader_GetSamplerIndex(&g_shaderTypes[i].pEmissiveMapSampler, g_shaderTypes[i].pShader, "u_EmissiveSampler");
    vcShader_GetSamplerIndex(&g_shaderTypes[i].pOcclusionMapSampler, g_shaderTypes[i].pShader, "u_OcclusionSampler");
  }

  udFree(pVertShaderSource);
  udFree(pFragShaderSource);
}

void vcGLTF_DestroyGlobalShaders()
{
  for (int i = 0; i < vcRSB_Count; ++i)
  {
    vcShader_ReleaseConstantBuffer(g_shaderTypes[i].pShader, g_shaderTypes[i].pVertUniformBuffer);
    vcShader_ReleaseConstantBuffer(g_shaderTypes[i].pShader, g_shaderTypes[i].pFragUniformBuffer);

    vcShader_DestroyShader(&g_shaderTypes[i].pShader);
  }
}

udResult vcGLTF_LoadBuffer(vcGLTFScene *pScene, const udJSON &root, int bufferID)
{
  udResult result = udR_Failure_;

  const char *pPath = root.Get("buffers[%d].uri", bufferID).AsString();
  int64_t loadedSize = 0;

  UD_ERROR_NULL(pPath, udR_ObjectNotFound);

  if (udFile_Load(pPath, &pScene->pBuffers[bufferID].pBytes, &loadedSize) != udR_Success)
    UD_ERROR_CHECK(udFile_Load(udTempStr("%s%s", pScene->pPath, pPath), &pScene->pBuffers[bufferID].pBytes, &loadedSize));

  UD_ERROR_IF(root.Get("buffers[%d].byteLength", bufferID).AsInt() != loadedSize, udR_CorruptData);

  pScene->pBuffers[bufferID].byteLength = loadedSize;
  result = udR_Success;

epilogue:
  if (result != udR_Success)
    udFree(pScene->pBuffers[bufferID].pBytes);

  return result;
}

void vcGLTF_LoadTexture(vcGLTFScene *pScene, const udJSON &root, int textureID)
{
  if (textureID >= 0 && textureID < pScene->textureCount && pScene->ppTextures[textureID] == nullptr)
  {
    const char *pURI = root.Get("images[%d].uri", root.Get("textures[%d].source", textureID).AsInt()).AsString();
    int sampler = root.Get("textures[%d].sampler", textureID).AsInt();

    vcTextureFilterMode filterMode;
    int filter = root.Get("samplers[%d].magFilter", sampler).AsInt();
    if (filter == vcGLTFType_Linear)
      filterMode = vcTFM_Linear;
    else
      filterMode = vcTFM_Nearest;

    vcTextureWrapMode wrapMode;
    int wrapS = root.Get("samplers[%d].wrapS", sampler).AsInt(vcGLTFType_Nearest);
    int wrapT = root.Get("samplers[%d].wrapT", sampler).AsInt(vcGLTFType_Nearest);

    if (wrapS != wrapT)
      __debugbreak();

    if (wrapS == vcGLTFType_ClampEdge)
      wrapMode = vcTWM_Clamp;
    else if (wrapS == vcGLTFType_MirroredRepeat)
      wrapMode = vcTWM_MirroredRepeat;
    else
      wrapMode = vcTWM_Repeat;

    if (udStrBeginsWith(pURI, "data:"))
      vcTexture_CreateFromFilename(&pScene->ppTextures[textureID], pURI, nullptr, nullptr, filterMode, false, wrapMode);
    else if (pURI != nullptr)
      vcTexture_CreateFromFilename(&pScene->ppTextures[textureID], udTempStr("%s/%s", pScene->pPath, pURI), nullptr, nullptr, filterMode, false, wrapMode);
  }
}

udFloat3 GetNormal(int i0, int i1, int i2, uint8_t *pBufferStart, ptrdiff_t byteStride)
{
  float *pPF = nullptr;

  pPF = (float*)(pBufferStart + (i0) * byteStride);
  udFloat3 p0 = { pPF[0], pPF[1], pPF[2] };

  pPF = (float*)(pBufferStart + (i1) * byteStride);
  udFloat3 p1 = { pPF[0], pPF[1], pPF[2] };

  pPF = (float*)(pBufferStart + (i2) * byteStride);
  udFloat3 p2 = { pPF[0], pPF[1], pPF[2] };

  return udCross(p1 - p0, p2 - p0);
}

udResult vcGLTF_CreateMesh(vcGLTFScene *pScene, const udJSON &root, int meshID)
{
  const udJSON &mesh = root.Get("meshes[%d]", meshID);
  
  int numPrimitives = (int)mesh.Get("primitives").ArrayLength();
  pScene->pMeshes[meshID].numPrimitives = numPrimitives;
  pScene->pMeshes[meshID].pPrimitives = udAllocType(vcGLTFMeshPrimitive, numPrimitives, udAF_Zero);

  for (int i = 0; i < numPrimitives; ++i)
  {
    const udJSON &primitive = mesh.Get("primitives[%d]", i);

    vcMeshFlags meshFlags = vcMF_None;
    vcGLTFFeatureBits featureBits = vcRSB_None;

    int mode = primitive.Get("mode").AsInt(4); // Points, Lines, Triangles
    int material = primitive.Get("material").AsInt(-1);

    if (mode != 4)
      __debugbreak();

    // This check can't be done because the material needs to be setup correctly
    //if (material >= 0 && material < root.Get("materials").ArrayLength())
    {
      pScene->pMeshes[meshID].pPrimitives[i].baseColorFactor = root.Get("materials[%d].pbrMetallicRoughness.baseColorFactor", material).AsFloat4(udFloat4::one());
      pScene->pMeshes[meshID].pPrimitives[i].baseColorTexture = root.Get("materials[%d].pbrMetallicRoughness.baseColorTexture.index", material).AsInt(-1);
      pScene->pMeshes[meshID].pPrimitives[i].baseColorUVSet = root.Get("materials[%d].pbrMetallicRoughness.baseColorTexture.texCoord", material).AsInt(0);
      if (pScene->pMeshes[meshID].pPrimitives[i].baseColorTexture != -1)
        vcGLTF_LoadTexture(pScene, root, pScene->pMeshes[meshID].pPrimitives[i].baseColorTexture);

      pScene->pMeshes[meshID].pPrimitives[i].metallicFactor = root.Get("materials[%d].pbrMetallicRoughness.metallicFactor", material).AsFloat(1.f);
      pScene->pMeshes[meshID].pPrimitives[i].roughnessFactor = root.Get("materials[%d].pbrMetallicRoughness.roughnessFactor", material).AsFloat(1.f);
      pScene->pMeshes[meshID].pPrimitives[i].metallicRoughnessTexture = root.Get("materials[%d].pbrMetallicRoughness.metallicRoughnessTexture.index", material).AsInt(-1);
      pScene->pMeshes[meshID].pPrimitives[i].metallicRoughnessUVSet = root.Get("materials[%d].pbrMetallicRoughness.metallicRoughnessTexture.texCoord", material).AsInt(0);
      if (pScene->pMeshes[meshID].pPrimitives[i].metallicRoughnessTexture != -1)
        vcGLTF_LoadTexture(pScene, root, pScene->pMeshes[meshID].pPrimitives[i].metallicRoughnessTexture);

      pScene->pMeshes[meshID].pPrimitives[i].normalScale = root.Get("materials[%d].normalTexture.scale", material).AsFloat(1.f);
      pScene->pMeshes[meshID].pPrimitives[i].normalTexture = root.Get("materials[%d].normalTexture.index", material).AsInt(-1);
      pScene->pMeshes[meshID].pPrimitives[i].normalUVSet = root.Get("materials[%d].normalTexture.texCoord", material).AsInt(0);
      if (pScene->pMeshes[meshID].pPrimitives[i].normalTexture != -1)
        vcGLTF_LoadTexture(pScene, root, pScene->pMeshes[meshID].pPrimitives[i].normalTexture);
      
      pScene->pMeshes[meshID].pPrimitives[i].emissiveFactor = root.Get("materials[%d].emissiveFactor", material).AsFloat3();
      pScene->pMeshes[meshID].pPrimitives[i].emissiveTexture = root.Get("materials[%d].emissiveTexture.index", material).AsInt(-1);
      pScene->pMeshes[meshID].pPrimitives[i].emissiveUVSet = root.Get("materials[%d].emissiveTexture.texCoord", material).AsInt(0);
      if (pScene->pMeshes[meshID].pPrimitives[i].emissiveTexture != -1)
        vcGLTF_LoadTexture(pScene, root, pScene->pMeshes[meshID].pPrimitives[i].emissiveTexture);

      pScene->pMeshes[meshID].pPrimitives[i].occlusionTexture = root.Get("materials[%d].occlusionTexture.index", material).AsInt(-1);
      pScene->pMeshes[meshID].pPrimitives[i].occlusionUVSet = root.Get("materials[%d].occlusionTexture.texCoord", material).AsInt(0);
      if (pScene->pMeshes[meshID].pPrimitives[i].occlusionTexture != -1)
        vcGLTF_LoadTexture(pScene, root, pScene->pMeshes[meshID].pPrimitives[i].occlusionTexture);

      const char *pAlphaModeStr = root.Get("materials[%d].alphaMode", material).AsString(nullptr);
      pScene->pMeshes[meshID].pPrimitives[i].alpaMode = vcGLTFAM_Opaque;
      pScene->pMeshes[meshID].pPrimitives[i].alphaCutoff = -1.f;
      if (udStrEquali(pAlphaModeStr, "MASK"))
      {
        pScene->pMeshes[meshID].pPrimitives[i].alpaMode = vcGLTFAM_Mask;
        pScene->pMeshes[meshID].pPrimitives[i].alphaCutoff = root.Get("materials[%d].alphaCutoff", material).AsFloat(0.5f);
      }
      else if (udStrEquali(pAlphaModeStr, "BLEND"))
      {
        pScene->pMeshes[meshID].pPrimitives[i].alpaMode = vcGLTFAM_Blend;
      }

      pScene->pMeshes[meshID].pPrimitives[i].doubleSided = root.Get("materials[%d].doubleSided", material).AsBool(false);
    }

    int indexAccessor = primitive.Get("indices").AsInt(-1);
    void *pIndexBuffer = nullptr;
    int32_t indexCount = 0;
    bool indexCopy = false;

    if (indexAccessor != -1)
    {
      const udJSON &accessor = root.Get("accessors[%d]", indexAccessor);

      if (udStrEqual("SCALAR", accessor.Get("type").AsString()))
      {
        indexCount = accessor.Get("count").AsInt();

        int bufferID = root.Get("bufferViews[%d].buffer", accessor.Get("bufferView").AsInt(-1)).AsInt();
        if (bufferID <= -1)
          __debugbreak();

        int indexType = accessor.Get("componentType").AsInt();
        ptrdiff_t offset = accessor.Get("byteOffset").AsInt64() + root.Get("bufferViews[%d].byteOffset", accessor.Get("bufferView").AsInt(-1)).AsInt();

        if (indexType == vcGLTFType_Int16 || indexType == vcGLTFType_UInt16 || indexType == vcGLTFType_Int8 || indexType == vcGLTFType_UInt8)
          meshFlags = meshFlags | vcMF_IndexShort;
        else if (indexType != vcGLTFType_Int32 && indexType != vcGLTFType_Uint32)
          __debugbreak();

        if (bufferID < pScene->bufferCount && pScene->pBuffers[bufferID].pBytes == nullptr)
          vcGLTF_LoadBuffer(pScene, root, bufferID);

        if (pScene->pBuffers[bufferID].pBytes != nullptr)
          pIndexBuffer = (pScene->pBuffers[bufferID].pBytes + offset);

        if (indexType == vcGLTFType_Int8 || indexType == vcGLTFType_UInt8)
        {
          uint16_t *pNewIndexBuffer = udAllocType(uint16_t, indexCount, udAF_None);
          for (int index = 0; index < indexCount; ++index)
            pNewIndexBuffer[index] = ((uint8_t*)pIndexBuffer)[index];
          indexCopy = true;
          pIndexBuffer = pNewIndexBuffer;
        }
      }
    }

    const udJSON &attributes = primitive.Get("attributes");
    int totalAttributes = (int)attributes.MemberCount();
    vcVertexLayoutTypes *pTypes = udAllocType(vcVertexLayoutTypes, totalAttributes+1, udAF_None); //+1 in case we need to add normals

    int maxCount = -1;
    bool hasNormals = false;

    struct
    {
      const char *pAttrName;
      const char *pAccessorType;
      vcVertexLayoutTypes type;
      vcGLTFFeatureBits featureBits;
    } supportedTypes[] = {
      { "POSITION", "VEC3", vcVLT_Position3, vcRSB_None },
      { "NORMAL", "VEC3", vcVLT_Normal3, vcRSB_None },
      { "TANGENT", "VEC4", vcVLT_Tangent4, vcRSB_Tangents },
      { "TEXCOORD_0", "VEC2", vcVLT_TextureCoords2_0, vcRSB_UVSet0 },
      { "TEXCOORD_1", "VEC2", vcVLT_TextureCoords2_1, vcRSB_UVSet1 },
      { "COLOR_0", "VEC3", vcVLT_ColourBGRA, vcRSB_Colour },
      { "COLOR_0", "VEC4", vcVLT_ColourBGRA, vcRSB_Colour },
      { "JOINTS_0", "VEC4", vcVLT_BoneIDs, vcRSB_Skinned },
      { "WEIGHTS_0", "VEC4", vcVLT_BoneWeights, vcRSB_Skinned },
    };

    for (size_t j = 0; j < totalAttributes; ++j)
    {
      const char *pAttributeName = attributes.GetMemberName(j);
      int attributeAccessorIndex = attributes.GetMember(j)->AsInt(0);

      const udJSON &accessor = root.Get("accessors[%d]", attributeAccessorIndex);

      const char *pAccessorType = accessor.Get("type").AsString();
      int attributeType = accessor.Get("componentType").AsInt(0);
      int attributeCount = accessor.Get("count").AsInt();

      int bufferID = root.Get("bufferViews[%d].buffer", accessor.Get("bufferView").AsInt(-1)).AsInt();
      if (bufferID <= -1)
        __debugbreak();

      if (bufferID < pScene->bufferCount && pScene->pBuffers[bufferID].pBytes == nullptr)
        vcGLTF_LoadBuffer(pScene, root, bufferID);

      if (maxCount == -1)
        maxCount = attributeCount;
      else if (maxCount != attributeCount)
        __debugbreak();

      pTypes[j] = vcVLT_Unsupported;

      for (size_t stIter = 0; stIter < udLengthOf(supportedTypes); ++stIter)
      {
        if (udStrEqual(pAttributeName, supportedTypes[stIter].pAttrName) && udStrEqual(pAccessorType, supportedTypes[stIter].pAccessorType))
        {
          pTypes[j] = supportedTypes[stIter].type;
          featureBits = (vcGLTFFeatureBits)(featureBits | supportedTypes[stIter].featureBits);
          break;
        }
      }

      if ((pTypes[j] == vcVLT_BoneIDs && attributeType != vcGLTFType_UInt16) || (pTypes[j] != vcVLT_BoneIDs && attributeType != vcGLTFType_F32))
        __debugbreak();

      if (pTypes[j] == vcVLT_Normal3)
        hasNormals = true;
      
      if (pTypes[j] == vcVLT_Unsupported)
        __debugbreak();
    }

    if (!hasNormals)
    {
      pTypes[totalAttributes] = vcVLT_Normal3;
      ++totalAttributes;
    }

    vcLayout_Sort(pTypes, totalAttributes);

    uint32_t vertexStride = vcLayout_GetSize(pTypes, totalAttributes);
    uint8_t *pVertData = udAllocType(uint8_t, vertexStride * maxCount, udAF_Zero);

    // Decode the buffers
    int totalOffset = 0;

    for (int ai = 0; ai < totalAttributes; ++ai)
    {
      if (pTypes[ai] == vcVLT_Normal3 && !hasNormals)
      {
        int count = 3;
        int offset = totalOffset;
        totalOffset += count * sizeof(float);

        int posAI = -1;
        for (int pai = 0; pai < totalAttributes; ++pai)
        {
          if (pTypes[pai] == vcVLT_Position3)
          {
            posAI = pai;
            break;
          }
        }

        if (posAI == -1)
          __debugbreak(); // No position found?

        int attributeAccessorIndex = attributes.GetMember(posAI)->AsInt(0);
        const udJSON& accessor = root.Get("accessors[%d]", attributeAccessorIndex);

        int bufferViewID = accessor.Get("bufferView").AsInt(-1);
        int bufferID = root.Get("bufferViews[%d].buffer", bufferViewID).AsInt();
        if (bufferID <= -1)
          __debugbreak();

        ptrdiff_t byteOffset = accessor.Get("byteOffset").AsInt64() + root.Get("bufferViews[%d].byteOffset", accessor.Get("bufferView").AsInt(-1)).AsInt64();
        ptrdiff_t byteStride = accessor.Get("byteStride").AsInt64() + root.Get("bufferViews[%d].byteStride", accessor.Get("bufferView").AsInt(-1)).AsInt64();

        for (int vi = 0; vi < maxCount; ++vi)
        {
          float *pVertFloats = (float*)(pVertData + vertexStride * vi + offset);

          udFloat3 normal = { 0.f, 0.f, 1.f };

          if (pIndexBuffer == nullptr)
          {
            int triangleStart = (vi / 3);
            normal = GetNormal(triangleStart, triangleStart + 1, triangleStart + 2, pScene->pBuffers[bufferID].pBytes + byteOffset, byteStride);
          }
          else
          {
            if (meshFlags & vcMF_IndexShort)
            {
              uint16_t *pIndices = (uint16_t*)pIndexBuffer;
              for (int indexIter = 0; indexIter < indexCount; ++indexIter)
              {
                if (pIndices[indexIter] == vi)
                {
                  int triangleStart = (indexIter / 3) * 3;
                  normal = GetNormal(pIndices[triangleStart], pIndices[triangleStart + 1], pIndices[triangleStart + 2], pScene->pBuffers[bufferID].pBytes + byteOffset, byteStride);
                }
              }
            }
            else
            {
              int32_t *pIndices = (int32_t*)pIndexBuffer;
              for (int indexIter = 0; indexIter < indexCount; ++indexIter)
              {
                if (pIndices[indexIter] == vi)
                {
                  int triangleStart = (indexIter / 3) * 3;
                  normal = GetNormal(pIndices[triangleStart], pIndices[triangleStart + 1], pIndices[triangleStart + 2], pScene->pBuffers[bufferID].pBytes + byteOffset, byteStride);
                }
              }
            }
          }

          for (int element = 0; element < count; ++element)
          {
            pVertFloats[element] = normal[element];
          }
        }
      }
      else
      {
        int attributeAccessorIndex = -1;
        
        for (size_t atIter = 0; atIter < attributes.MemberCount() && attributeAccessorIndex == -1; ++atIter)
        {
          for (size_t stIter = 0; stIter < udLengthOf(supportedTypes); ++stIter)
          {
            if (supportedTypes[stIter].type == pTypes[ai] && udStrEqual(attributes.GetMemberName(atIter), supportedTypes[stIter].pAttrName))
            {
              attributeAccessorIndex = attributes.GetMember(atIter)->AsInt();
              break;
            }
          }
        }

        const udJSON& accessor = root.Get("accessors[%d]", attributeAccessorIndex);

        int bufferViewID = accessor.Get("bufferView").AsInt(-1);
        int bufferID = root.Get("bufferViews[%d].buffer", bufferViewID).AsInt();
        if (bufferID <= -1)
          __debugbreak();

        const char* pAccessorType = accessor.Get("type").AsString();
        ptrdiff_t byteOffset = accessor.Get("byteOffset").AsInt64() + root.Get("bufferViews[%d].byteOffset", accessor.Get("bufferView").AsInt(-1)).AsInt64();
        ptrdiff_t byteStride = accessor.Get("byteStride").AsInt64() + root.Get("bufferViews[%d].byteStride", accessor.Get("bufferView").AsInt(-1)).AsInt64();

        int count = 3;
        int offset = totalOffset;

        if (pTypes[ai] == vcVLT_ColourBGRA)
        {
          if (udStrEqual(pAccessorType, "VEC3"))
            count = 3;
          else
            count = 4;
          totalOffset += sizeof(uint32_t);
        }
        else if (pTypes[ai] == vcVLT_BoneIDs)
        {
          count = 4;
          totalOffset += sizeof(uint32_t);
        }
        else if (udStrEqual(pAccessorType, "VEC3"))
        {
          count = 3;
          totalOffset += (count * sizeof(float));
        }
        else if (udStrEqual(pAccessorType, "VEC2"))
        {
          count = 2;
          totalOffset += (count * sizeof(float));
        }
        else if (udStrEqual(pAccessorType, "VEC4"))
        {
          count = 4;
          totalOffset += (count * sizeof(float));
        }
        else
        {
          __debugbreak();
        }

        if (bufferID >= pScene->bufferCount || pScene->pBuffers[bufferID].pBytes == nullptr)
          continue;

        if (byteStride == 0)
          byteStride = count * sizeof(float);

        if (pTypes[ai] == vcVLT_ColourBGRA)
        {
          for (int vi = 0; vi < maxCount; ++vi)
          {
            float *pFloats = (float*)(pScene->pBuffers[bufferID].pBytes + byteOffset + vi * byteStride);
            int32_t *pVertI32 = (int32_t*)(pVertData + vertexStride * vi + offset);

            uint32_t temp = ((int(udRound(pFloats[0] * 255.f)) & 0xFF) << 0) | ((int(udRound(pFloats[1] * 255.f)) & 0xFF) << 8) | ((int(udRound(pFloats[2] * 255.f)) & 0xFF) << 16);

            if (count == 3)
              temp |= 0xFF000000;
            else
              temp |= (int(pFloats[3] * 255.f) << 24);

            pVertI32[0] = temp;
          }
        }
        else if (pTypes[ai] == vcVLT_BoneIDs)
        {
          for (int vi = 0; vi < maxCount; ++vi)
          {
            uint16_t *pIDs = (uint16_t*)(pScene->pBuffers[bufferID].pBytes + byteOffset + vi * byteStride);
            uint32_t *pVertI32 = (uint32_t*)(pVertData + vertexStride * vi + offset);

            *pVertI32 = ((pIDs[0] & 0xFF) << 24) | ((pIDs[1] & 0xFF) << 16) | ((pIDs[2] & 0xFF) << 8) | ((pIDs[3] & 0xFF) << 0);
          }
        }
        else
        {
          for (int vi = 0; vi < maxCount; ++vi)
          {
            float *pFloats = (float*)(pScene->pBuffers[bufferID].pBytes + byteOffset + vi * byteStride);
            float *pVertFloats = (float*)(pVertData + vertexStride * vi + offset);

            for (int element = 0; element < count; ++element)
            {
              pVertFloats[element] = pFloats[element];
            }
          }
        }
      }
    }

    // Bind the correct shader
    pScene->pMeshes[meshID].pPrimitives[i].features = featureBits;

    vcShader_Bind(g_shaderTypes[featureBits].pShader);

    if (pIndexBuffer == nullptr)
      vcMesh_Create(&pScene->pMeshes[meshID].pPrimitives[i].pMesh, pTypes, totalAttributes, pVertData, maxCount, nullptr, 0, vcMF_NoIndexBuffer | meshFlags);
    else
      vcMesh_Create(&pScene->pMeshes[meshID].pPrimitives[i].pMesh, pTypes, totalAttributes, pVertData, maxCount, pIndexBuffer, indexCount, meshFlags);
  
    udFree(pTypes);
    udFree(pVertData);

    if (indexCopy)
      udFree(pIndexBuffer);
  }


  return udR_Success;
}

udResult vcGLTF_ProcessChildNode(vcGLTFScene *pScene, const udJSON &root, const udJSON &child, udFloat4x4 parentMatrix)
{
  udFloat4x4 childMatrix = udFloat4x4::identity();
  
  if (child.Get("matrix").IsArray())
  {
    childMatrix = udFloat4x4::create(child.Get("matrix").AsDouble4x4());
  }
  else
  {
    udFloat3 translation = child.Get("translation").AsFloat3();
    udFloatQuat rotation = udFloatQuat::create(child.Get("rotation").AsQuaternion());
    udFloat3 scale = child.Get("scale").AsFloat3(udFloat3::one());
    
    childMatrix = udFloat4x4::rotationQuat(rotation, translation) * udFloat4x4::scaleNonUniform(scale);
  }

  udFloat4x4 chainedMatrix = parentMatrix * childMatrix;

  if (!child.Get("mesh").IsVoid())
  {
    vcGLTFMeshInstance *pMesh = pScene->meshInstances.PushBack();

    pMesh->matrix = chainedMatrix;
    pMesh->meshID = child.Get("mesh").AsInt();

    if (pMesh->meshID >= pScene->meshCount)
      pMesh->meshID = 0;

    if (pScene->pMeshes[pMesh->meshID].pPrimitives == nullptr)
      vcGLTF_CreateMesh(pScene, root, pMesh->meshID);
  }
  else if (!child.Get("children").IsVoid())
  {
    for (int i = 0; i < child.Get("children").ArrayLength(); ++i)
      vcGLTF_ProcessChildNode(pScene, root, root.Get("nodes[%d]", child.Get("children[%zu]", i).AsInt()), chainedMatrix);
  }
  else if (!child.Get("camera").IsVoid() || !child.Get("light").IsVoid())
  {
    // We don't care about these
  }
  else if (child.Get("name").IsString())
  {
    // We don't care about these?
  }
  else
  {
    // New node type
    __debugbreak();
  }

  return udR_Success;
}

udResult vcGLTF_Load(vcGLTFScene **ppScene, const char *pFilename, udWorkerPool *pWorkerPool)
{
  udUnused(pWorkerPool);

  udResult result = udR_Failure_;
  
  vcGLTFScene *pScene = udAllocType(vcGLTFScene, 1, udAF_Zero);
  pScene->meshInstances.Init(8);

  char *pData = nullptr;
  udJSON gltfData = {};
  const udJSONArray *pSceneNodes = nullptr;

  udFilename path(pFilename);
  int pathLen = 0;

  int baseScene = 0;

  UD_ERROR_CHECK(udFile_Load(pFilename, &pData));
  UD_ERROR_CHECK(gltfData.Parse(pData));
  udFree(pData);

  pathLen = path.ExtractFolder(nullptr, 0);
  pScene->pPath = udAllocType(char, pathLen + 1, udAF_Zero);
  path.ExtractFolder(pScene->pPath, pathLen + 1);

  pScene->nodeCount = (int)gltfData.Get("nodes").ArrayLength();
  pScene->pNodes = udAllocType(vcGLTFNode, pScene->nodeCount, udAF_Zero);

  pScene->bufferCount = (int)gltfData.Get("buffers").ArrayLength();
  pScene->pBuffers = udAllocType(vcGLTFBuffer, pScene->bufferCount, udAF_Zero);
  
  pScene->meshCount = (int)gltfData.Get("meshes").ArrayLength();
  pScene->pMeshes = udAllocType(vcGLTFMesh, pScene->meshCount, udAF_Zero);

  pScene->textureCount = (int)gltfData.Get("textures").ArrayLength();
  pScene->ppTextures = udAllocType(vcTexture*, pScene->textureCount, udAF_Zero);

  baseScene = gltfData.Get("scene").AsInt();
  pSceneNodes = gltfData.Get("scenes[%d].nodes", baseScene).AsArray();
  for (size_t i = 0; i < pSceneNodes->length; ++i)
  {
    vcGLTF_ProcessChildNode(pScene, gltfData, gltfData.Get("nodes[%d]", pSceneNodes->GetElement(i)->AsInt()), udFloat4x4::identity());
  }

  result = udR_Success;
  *ppScene = pScene;
  pScene = nullptr;

epilogue:
  if (pScene != nullptr)
    vcGLTF_Destroy(&pScene);

  udFree(pData);

  return result;
}

void vcGLTF_Destroy(vcGLTFScene **ppScene)
{
  if (ppScene == nullptr || *ppScene == nullptr)
    return;

  vcGLTFScene *pScene = *ppScene;
  *ppScene = nullptr;

  for (int i = 0; i < pScene->meshCount; ++i)
  {
    for (int j = 0; j < pScene->pMeshes[i].numPrimitives; ++j)
      vcMesh_Destroy(&pScene->pMeshes[i].pPrimitives[j].pMesh);

    udFree(pScene->pMeshes[i].pPrimitives);
  }
  udFree(pScene->pMeshes);

  for (int i = 0; i < pScene->bufferCount; ++i)
    udFree(pScene->pBuffers[i].pBytes);
  udFree(pScene->pBuffers);

  pScene->meshInstances.Deinit();

  udFree(pScene->pNodes);

  for (int i = 0; i < pScene->textureCount; ++i)
    vcTexture_Destroy(&pScene->ppTextures[i]);
  udFree(pScene->ppTextures);

  udFree(pScene->pPath);

  udFree(pScene);
}

bool g_allowBaseColourMaps = true;
bool g_allowNormalMaps = true;
bool g_allowMetalRoughMaps = true;
bool g_allowEmissiveMaps = true;
bool g_allowOcclusionMaps = true;

udResult vcGLTF_Render(vcGLTFScene *pScene, udRay<double> camera, udDouble4x4 worldMatrix, udDouble4x4 viewMatrix, udDouble4x4 projectionMatrix)
{
  int bound = -1;

  udFloat4x4 SpaceChange = { 1, 0, 0, 0, 0, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1 };

  for (int i = 0; i < 72; ++i)
  {
    s_gltfVertSkinningInfo.u_jointMatrix[i] = udFloat4x4::identity();
    s_gltfVertSkinningInfo.u_jointNormalMatrix[i] = udFloat4x4::identity();
  }

  for (size_t i = 0; i < pScene->meshInstances.length; ++i)
  {
    s_gltfVertInfo.u_ModelMatrix = udFloat4x4::create(worldMatrix) * SpaceChange * pScene->meshInstances[i].matrix;
    s_gltfVertInfo.u_ViewProjectionMatrix = udFloat4x4::create(projectionMatrix * viewMatrix);
    s_gltfVertInfo.u_NormalMatrix = udTranspose(udInverse(s_gltfVertInfo.u_ModelMatrix));

    s_gltfFragInfo.u_Camera = udFloat3::create(camera.position);

    // Lights
    // Directional Lights
    s_gltfFragInfo.u_Lights[0].direction = udNormalize(udFloat3::create(0.f, -0.1f, -0.9f));
    s_gltfFragInfo.u_Lights[0].color = { 0.9f, 0.8f, 1.f };
    s_gltfFragInfo.u_Lights[0].intensity = 10.f;
    s_gltfFragInfo.u_Lights[0].type = 0;

    // Pointlight
    s_gltfFragInfo.u_Lights[1].direction = udFloat3::create(camera.direction);
    s_gltfFragInfo.u_Lights[1].position = udFloat3::create(camera.position);
    s_gltfFragInfo.u_Lights[1].color = { 1.0f, 1.0f, 1.f };
    s_gltfFragInfo.u_Lights[1].intensity = 1.f;
    s_gltfFragInfo.u_Lights[1].range = 100000.f;
    s_gltfFragInfo.u_Lights[1].type = 2;
    s_gltfFragInfo.u_Lights[1].innerConeCos = udCos(UD_PIf / 8.f);
    s_gltfFragInfo.u_Lights[1].outerConeCos = udCos(UD_PIf / 4.f);

    for (int j = 0; j < pScene->pMeshes[pScene->meshInstances[i].meshID].numPrimitives; ++j)
    {
      const vcGLTFMeshPrimitive &prim = pScene->pMeshes[pScene->meshInstances[i].meshID].pPrimitives[j];
      const vcGLTFShader &shader = g_shaderTypes[prim.features];

      if (bound != prim.features)
      {
        vcShader_Bind(shader.pShader);
        bound = prim.features;
      }

      vcShader_BindConstantBuffer(shader.pShader, shader.pVertUniformBuffer, &s_gltfVertInfo, sizeof(s_gltfVertInfo));

      //Material
      s_gltfFragInfo.u_EmissiveFactor = prim.emissiveFactor;
      s_gltfFragInfo.u_BaseColorFactor = prim.baseColorFactor;
      s_gltfFragInfo.u_MetallicFactor = prim.metallicFactor;
      s_gltfFragInfo.u_RoughnessFactor = prim.roughnessFactor;

      s_gltfFragInfo.u_Exposure = 1.f;
      s_gltfFragInfo.u_NormalScale = (float)prim.normalScale;

      s_gltfFragInfo.u_alphaMode = prim.alpaMode;
      s_gltfFragInfo.u_AlphaCutoff = prim.alphaCutoff;

      if (prim.alpaMode == vcGLTFAM_Blend)
        vcGLState_SetBlendMode(vcGLSBM_Interpolative);
      else
        vcGLState_SetBlendMode(vcGLSBM_None);

      if (g_allowBaseColourMaps && prim.baseColorTexture >= 0)
      {
        s_gltfFragInfo.u_BaseColorUVSet = prim.baseColorUVSet;
        vcShader_BindTexture(shader.pShader, pScene->ppTextures[prim.baseColorTexture], 0, shader.pBaseColourSampler);
      }
      else
      {
        s_gltfFragInfo.u_BaseColorUVSet = -1;
      }

      if (g_allowMetalRoughMaps && prim.metallicRoughnessTexture >= 0)
      {
        s_gltfFragInfo.u_MetallicRoughnessUVSet = prim.metallicRoughnessUVSet;
        vcShader_BindTexture(shader.pShader, pScene->ppTextures[prim.metallicRoughnessTexture], 0, shader.pMetallicRoughnessSampler);
      }
      else
      {
        s_gltfFragInfo.u_MetallicRoughnessUVSet = -1;
      }

      if (g_allowNormalMaps && prim.normalTexture >= 0)
      {
        s_gltfFragInfo.u_NormalUVSet = prim.normalUVSet;
        vcShader_BindTexture(shader.pShader, pScene->ppTextures[prim.normalTexture], 0, shader.pNormalMapSampler);
      }
      else
      {
        s_gltfFragInfo.u_NormalUVSet = -1;
      }

      if (g_allowEmissiveMaps && prim.emissiveTexture >= 0)
      {
        s_gltfFragInfo.u_EmissiveUVSet = prim.emissiveUVSet;
        vcShader_BindTexture(shader.pShader, pScene->ppTextures[prim.emissiveTexture], 0, shader.pEmissiveMapSampler);
      }
      else
      {
        s_gltfFragInfo.u_EmissiveUVSet = -1;
      }

      if (g_allowOcclusionMaps && prim.occlusionTexture >= 0)
      {
        s_gltfFragInfo.u_OcclusionUVSet = prim.occlusionUVSet;
        s_gltfFragInfo.u_OcclusionStrength = 1.f;
        vcShader_BindTexture(shader.pShader, pScene->ppTextures[prim.occlusionTexture], 0, shader.pOcclusionMapSampler);
      }
      else
      {
        s_gltfFragInfo.u_OcclusionUVSet = -1;
      }

      vcShader_BindConstantBuffer(shader.pShader, shader.pFragUniformBuffer, &s_gltfFragInfo, sizeof(s_gltfFragInfo));

      if (prim.doubleSided)
        vcGLState_SetFaceMode(vcGLSFM_Solid, vcGLSCM_None, true, false);
      else
        vcGLState_SetFaceMode(vcGLSFM_Solid, vcGLSCM_Back, true, false);

      vcMesh_Render(prim.pMesh);
    }
  }

  return udR_Success;
}