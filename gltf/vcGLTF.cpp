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

struct vcGLTFNode
{
  udDouble4x4 matrix;
};

struct vcGLTFMeshInstance
{
  int meshID;
  udDouble4x4 matrix;
};

enum vcGLTF_AlphaMode
{
  vcGLTFAM_Opaque,
  vcGLTFAM_Mask,
  vcGLTFAM_Blend
};

struct vcGLTFMeshPrimitive
{
  vcMesh *pMesh;

  udFloat4 baseColorFactor;
  int baseColorTexture;

  float metallicFactor;
  float roughnessFactor;
  int metallicRoughnessTexture;

  double normalScale;
  int normalTexture;

  int emissiveTexture;
  udFloat3 emissiveFactor;

  int occlusionTexture;
  
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

  udChunkedArray<vcGLTFNode> nodes;
  udChunkedArray<vcGLTFMeshInstance> meshInstances;

  int bufferCount;
  vcGLTFBuffer *pBuffers;

  int meshCount;
  vcGLTFMesh *pMeshes;

  int textureCount;
  vcTexture **ppTextures;
};

vcVertexLayoutTypes *s_pShaderTypes = nullptr;
int s_shaderTotalAttributes = 0;
vcShader* s_pBasicShader = nullptr;
vcShaderConstantBuffer *s_pBasicShaderVertUniformBuffer = nullptr;
vcShaderConstantBuffer *s_pBasicShaderFragUniformBuffer = nullptr;
vcShaderSampler *s_pBasicShaderBaseColourSampler = nullptr;
vcShaderSampler *s_pBasicShaderMetallicRoughnessSampler = nullptr;
vcShaderSampler *s_pBasicShaderNormalMapSampler = nullptr;
vcShaderSampler *s_pBasicShaderEmissiveMapSampler = nullptr;
vcShaderSampler *s_pBasicShaderOcclusionMapSampler = nullptr;

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


void vcGLTF_GenShader()
{
  if (s_pShaderTypes == nullptr || s_shaderTotalAttributes == 0)
    return;

  const char* defines[] = {
    //"HAS_TANGENTS",
    "HAS_UV_SET1",
    "MATERIAL_METALLICROUGHNESS",
    "USE_PUNCTUAL",
    //"DEBUG_OUTPUT",
    //"DEBUG_NORMAL",
  };
  vcShader_CreateFromFile(&s_pBasicShader, "asset://assets/shaders/gltfVertexShader", "asset://assets/shaders/gltfFragmentShader", s_pShaderTypes, s_shaderTotalAttributes, defines, udLengthOf(defines));
  vcShader_GetConstantBuffer(&s_pBasicShaderVertUniformBuffer, s_pBasicShader, "u_EveryFrame", sizeof(s_gltfVertInfo));
  vcShader_GetConstantBuffer(&s_pBasicShaderFragUniformBuffer, s_pBasicShader, "u_FragSettings", sizeof(s_gltfFragInfo));
  vcShader_GetSamplerIndex(&s_pBasicShaderBaseColourSampler, s_pBasicShader, "u_BaseColorSampler");
  vcShader_GetSamplerIndex(&s_pBasicShaderMetallicRoughnessSampler, s_pBasicShader, "u_MetallicRoughnessSampler");
  vcShader_GetSamplerIndex(&s_pBasicShaderNormalMapSampler, s_pBasicShader, "u_NormalSampler");
  vcShader_GetSamplerIndex(&s_pBasicShaderEmissiveMapSampler, s_pBasicShader, "u_EmissiveSampler");
  vcShader_GetSamplerIndex(&s_pBasicShaderOcclusionMapSampler, s_pBasicShader, "u_OcclusionSampler");
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

udResult vcGLTF_CreateMesh(vcGLTFScene *pScene, const udJSON &root, int meshID)
{
  const udJSON &mesh = root.Get("meshes[%d]", meshID);
  
  int numPrimitives = (int)mesh.Get("primitives").ArrayLength();
  pScene->pMeshes[meshID].numPrimitives = numPrimitives;
  pScene->pMeshes[meshID].pPrimitives = udAllocType(vcGLTFMeshPrimitive, numPrimitives, udAF_Zero);

  printf("Mesh Primitives: %d", numPrimitives);

  for (int i = 0; i < numPrimitives; ++i)
  {
    const udJSON &primitive = mesh.Get("primitives[%d]", i);

    vcMeshFlags meshFlags = vcMF_None;

    int mode = primitive.Get("mode").AsInt(4); // Points, Lines, Triangles
    int material = primitive.Get("material").AsInt(-1);

    if (mode != 4)
      __debugbreak();

    if (material >= 0 && material < root.Get("materials").ArrayLength())
    {
      pScene->pMeshes[meshID].pPrimitives[i].baseColorFactor = root.Get("materials[%d].pbrMetallicRoughness.baseColorFactor", material).AsFloat4(udFloat4::one());
      pScene->pMeshes[meshID].pPrimitives[i].baseColorTexture = root.Get("materials[%d].pbrMetallicRoughness.baseColorTexture.index", material).AsInt(-1);
      if (pScene->pMeshes[meshID].pPrimitives[i].baseColorTexture != -1)
        vcGLTF_LoadTexture(pScene, root, pScene->pMeshes[meshID].pPrimitives[i].baseColorTexture);

      pScene->pMeshes[meshID].pPrimitives[i].metallicFactor = root.Get("materials[%d].pbrMetallicRoughness.metallicFactor", material).AsFloat(1.f);
      pScene->pMeshes[meshID].pPrimitives[i].roughnessFactor = root.Get("materials[%d].pbrMetallicRoughness.roughnessFactor", material).AsFloat(1.f);
      pScene->pMeshes[meshID].pPrimitives[i].metallicRoughnessTexture = root.Get("materials[%d].pbrMetallicRoughness.metallicRoughnessTexture.index", material).AsInt(-1);
      if (pScene->pMeshes[meshID].pPrimitives[i].metallicRoughnessTexture != -1)
        vcGLTF_LoadTexture(pScene, root, pScene->pMeshes[meshID].pPrimitives[i].metallicRoughnessTexture);

      pScene->pMeshes[meshID].pPrimitives[i].normalScale = root.Get("materials[%d].normalTexture.scale", material).AsDouble(1.0);
      pScene->pMeshes[meshID].pPrimitives[i].normalTexture = root.Get("materials[%d].normalTexture.index", material).AsInt(-1);
      if (pScene->pMeshes[meshID].pPrimitives[i].normalTexture != -1)
        vcGLTF_LoadTexture(pScene, root, pScene->pMeshes[meshID].pPrimitives[i].normalTexture);
      
      pScene->pMeshes[meshID].pPrimitives[i].emissiveFactor = root.Get("materials[%d].emissiveFactor", material).AsFloat3();
      pScene->pMeshes[meshID].pPrimitives[i].emissiveTexture = root.Get("materials[%d].emissiveTexture.index", material).AsInt(-1);
      if (pScene->pMeshes[meshID].pPrimitives[i].emissiveTexture != -1)
        vcGLTF_LoadTexture(pScene, root, pScene->pMeshes[meshID].pPrimitives[i].emissiveTexture);

      pScene->pMeshes[meshID].pPrimitives[i].occlusionTexture = root.Get("materials[%d].occlusionTexture.index", material).AsInt(-1);
      if (pScene->pMeshes[meshID].pPrimitives[i].occlusionTexture != -1)
        vcGLTF_LoadTexture(pScene, root, pScene->pMeshes[meshID].pPrimitives[i].occlusionTexture);

      const char *pAlphaModeStr = root.Get("materials[%d].alphaMode", material).AsString(nullptr);
      pScene->pMeshes[meshID].pPrimitives[i].alpaMode = vcGLTFAM_Opaque;
      pScene->pMeshes[meshID].pPrimitives[i].alphaCutoff = -1.f;
      if (udStrEquali(pAlphaModeStr, "MASK"))
      {
        pScene->pMeshes[meshID].pPrimitives[i].alpaMode = vcGLTFAM_Mask;
        pScene->pMeshes[meshID].pPrimitives[i].alphaCutoff = root.Get("materials[%d].alphaCutoff").AsFloat(0.5f);
      }
      else if (udStrEquali(pAlphaModeStr, "BLEND"))
      {
        pScene->pMeshes[meshID].pPrimitives[i].alpaMode = vcGLTFAM_Blend;
      }

      pScene->pMeshes[meshID].pPrimitives[i].doubleSided = root.Get("materials[%d].alphaMode", material).AsBool(false);
    }

    int indexAccessor = primitive.Get("indices").AsInt(-1);
    void *pIndexBuffer = nullptr;
    int32_t indexCount = 0;

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

        if (indexType == vcGLTFType_Int16 || indexType == vcGLTFType_UInt16)
          meshFlags = meshFlags | vcMF_IndexShort;
        else if (indexType != vcGLTFType_Int32)
          __debugbreak();

        if (bufferID < pScene->bufferCount && pScene->pBuffers[bufferID].pBytes == nullptr)
          vcGLTF_LoadBuffer(pScene, root, bufferID);

        if (pScene->pBuffers[bufferID].pBytes != nullptr)
          pIndexBuffer = (pScene->pBuffers[bufferID].pBytes + offset);
      }
    }

    const udJSON &attributes = primitive.Get("attributes");
    int totalAttributes = (int)attributes.MemberCount();
    vcVertexLayoutTypes *pTypes = udAllocType(vcVertexLayoutTypes, totalAttributes+1, udAF_None); //+1 in case we need to add normals

    int maxCount = -1;
    bool hasNormals = false;

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

      if (attributeType != vcGLTFType_F32)
        __debugbreak();

      if (udStrEqual(pAttributeName, "POSITION"))
      {
        if (udStrEqual(pAccessorType, "VEC2"))
          pTypes[j] = vcVLT_Position2;
        else if (udStrEqual(pAccessorType, "VEC3"))
          pTypes[j] = vcVLT_Position3;
        else if (udStrEqual(pAccessorType, "VEC4"))
          pTypes[j] = vcVLT_Position4;
        else
          __debugbreak();
      }
      else if (udStrEqual(pAttributeName, "NORMAL"))
      {
        hasNormals = true;

        if (udStrEqual(pAccessorType, "VEC3"))
          pTypes[j] = vcVLT_Normal3;
        else
          __debugbreak();
      }
      else if (udStrEqual(pAttributeName, "TANGENT"))
      {
        if (udStrEqual(pAccessorType, "VEC4"))
          pTypes[j] = vcVLT_Tangent4;
        else
          __debugbreak();
      }
      else if (udStrEqual(pAttributeName, "TEXCOORD_0"))
      {
        if (udStrEqual(pAccessorType, "VEC2"))
          pTypes[j] = vcVLT_TextureCoords2;
        else
          __debugbreak();
      }
      else
      {
        __debugbreak();
      }
    }

    if (!hasNormals)
    {
      pTypes[totalAttributes] = vcVLT_Normal3;
      ++totalAttributes;
    }

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

          udFloat3 normal = {};

          if (pIndexBuffer == nullptr)
          {
            int triangleStart = (vi / 3);
            float *pPF = nullptr;
            
            pPF = (float*)(pScene->pBuffers[bufferID].pBytes + byteOffset + (triangleStart+0) * byteStride);
            udFloat3 p0 = { pPF[0], pPF[1], pPF[2] };

            pPF = (float*)(pScene->pBuffers[bufferID].pBytes + byteOffset + (triangleStart+1) * byteStride);
            udFloat3 p1 = { pPF[0], pPF[1], pPF[2] };
            
            pPF = (float*)(pScene->pBuffers[bufferID].pBytes + byteOffset + (triangleStart+2) * byteStride);
            udFloat3 p2 = { pPF[0], pPF[1], pPF[2] };

            normal = udCross(p1-p0, p2-p0);
          }
          else
          {
            __debugbreak(); // Need to find the triangles this vert is used in and average them
          }

          for (int element = 0; element < count; ++element)
          {
            pVertFloats[element] = normal[element];
          }
        }
      }
      else
      {
        int attributeAccessorIndex = attributes.GetMember(ai)->AsInt(0);
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

        if (udStrEqual(pAccessorType, "VEC3"))
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

    if (s_pBasicShader == nullptr)
    {
      s_pShaderTypes = (vcVertexLayoutTypes*)udMemDup(pTypes, sizeof(vcVertexLayoutTypes)*totalAttributes, 0, udAF_None);
      s_shaderTotalAttributes = totalAttributes;
      vcGLTF_GenShader();
    }
    
    if (pIndexBuffer == nullptr)
      vcMesh_Create(&pScene->pMeshes[meshID].pPrimitives[i].pMesh, pTypes, totalAttributes, pVertData, maxCount, nullptr, 0, vcMF_NoIndexBuffer | meshFlags);
    else
      vcMesh_Create(&pScene->pMeshes[meshID].pPrimitives[i].pMesh, pTypes, totalAttributes, pVertData, maxCount, pIndexBuffer, indexCount, meshFlags);
  
    udFree(pTypes);
  }


  return udR_Success;
}

udResult vcGLTF_ProcessChildNode(vcGLTFScene *pScene, const udJSON &root, const udJSON &child, udDouble4x4 parentMatrix)
{
  udDouble4x4 childMatrix = child.Get("matrix").AsDouble4x4();
  udDouble4x4 chainedMatrix = parentMatrix * childMatrix;

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
  else if (!child.Get("camera").IsVoid())
  {
    // We don't care about cameras
  }
  else
  {
    // New node type
    __debugbreak();
  }

  return udR_Success;
}

udResult vcGLTF_LoadPolygonModel(vcGLTFScene **ppScene, const char *pFilename, udWorkerPool *pWorkerPool)
{
  udUnused(pWorkerPool);

  udResult result = udR_Failure_;
  
  vcGLTFScene *pScene = udAllocType(vcGLTFScene, 1, udAF_Zero);
  pScene->nodes.Init(32);
  pScene->meshInstances.Init(8);

  char *pData = nullptr;
  udJSON gltfData = {};
  const udJSONArray *pSceneNodes = nullptr;

  udFilename path(pFilename);
  int pathLen = 0;

  int baseScene = 0;

  UD_ERROR_CHECK(udFile_Load(pFilename, &pData));
  UD_ERROR_CHECK(gltfData.Parse(pData));

  pathLen = path.ExtractFolder(nullptr, 0);
  pScene->pPath = udAllocType(char, pathLen + 1, udAF_Zero);
  path.ExtractFolder(pScene->pPath, pathLen + 1);

  pScene->bufferCount = (int)gltfData.Get("buffers").ArrayLength();
  pScene->pBuffers = udAllocType(vcGLTFBuffer, pScene->bufferCount, udAF_Zero);
  
  pScene->meshCount = (int)gltfData.Get("meshes").ArrayLength();
  pScene->pMeshes = udAllocType(vcGLTFMesh, pScene->meshCount, udAF_Zero);

  pScene->textureCount = (int)gltfData.Get("textures").ArrayLength();
  pScene->ppTextures = udAllocType(vcTexture*, pScene->textureCount, udAF_Zero);

  baseScene = gltfData.Get("scene").AsInt();
  pSceneNodes = gltfData.Get("scenes[%d].nodes").AsArray();
  for (size_t i = 0; i < pSceneNodes->length; ++i)
  {
    vcGLTF_ProcessChildNode(pScene, gltfData, gltfData.Get("nodes[%d]", pSceneNodes->GetElement(i)->AsInt()), udDouble4x4::identity());
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

  udFree(pScene->pBuffers);
  pScene->meshInstances.Deinit();
  pScene->nodes.Deinit();
  udFree(pScene->pMeshes);
  udFree(pScene->ppTextures);

  udFree(pScene);
}

bool g_allowBaseColourMaps = true;
bool g_allowNormalMaps = true;
bool g_allowMetalRoughMaps = true;
bool g_allowEmissiveMaps = true;
bool g_allowOcclusionMaps = true;

udResult vcGLTF_Render(vcGLTFScene *pScene, udRay<double> camera, udDouble4x4 worldMatrix, udDouble4x4 viewMatrix, udDouble4x4 projectionMatrix)
{
  vcShader_Bind(s_pBasicShader);

  static udDouble4x4 SpaceChange = { 1, 0, 0, 0,   0, 0, 1, 0,   0, -1, 0, 0,   0, 0, 0, 1 };

  for (size_t i = 0; i < pScene->meshInstances.length; ++i)
  {
    s_gltfVertInfo.u_ModelMatrix = udFloat4x4::create(worldMatrix * SpaceChange * pScene->meshInstances[i].matrix);
    s_gltfVertInfo.u_ViewProjectionMatrix = udFloat4x4::create(projectionMatrix * viewMatrix);
    s_gltfVertInfo.u_NormalMatrix = udFloat4x4::create(udTranspose(udInverse(worldMatrix * SpaceChange * pScene->meshInstances[i].matrix)));
    vcShader_BindConstantBuffer(s_pBasicShader, s_pBasicShaderVertUniformBuffer, &s_gltfVertInfo, sizeof(s_gltfVertInfo));

    s_gltfFragInfo.u_Camera = udFloat3::create(camera.position);

    // Lights
    // Directional Lights
    s_gltfFragInfo.u_Lights[0].direction = udNormalize(udFloat3::create(0.f, -0.1f, -0.9f));
    s_gltfFragInfo.u_Lights[0].color = { 0.9f, 0.8f, 1.f };
    s_gltfFragInfo.u_Lights[0].intensity = 1.f;
    s_gltfFragInfo.u_Lights[0].type = 0;

    // Pointlight
    s_gltfFragInfo.u_Lights[1].direction = udFloat3::create(camera.direction);
    s_gltfFragInfo.u_Lights[1].position = udFloat3::create(camera.position);
    s_gltfFragInfo.u_Lights[1].color = { 0.1f, 0.1f, 1.f };
    s_gltfFragInfo.u_Lights[1].intensity = 100.f;
    s_gltfFragInfo.u_Lights[1].range = 100000.f;
    s_gltfFragInfo.u_Lights[1].type = 2;
    s_gltfFragInfo.u_Lights[1].innerConeCos = udCos(UD_PIf / 8.f);
    s_gltfFragInfo.u_Lights[1].outerConeCos = udCos(UD_PIf / 4.f);

    for (int j = 0; j < pScene->pMeshes[pScene->meshInstances[i].meshID].numPrimitives; ++j)
    {
      //Material
      s_gltfFragInfo.u_EmissiveFactor = pScene->pMeshes[pScene->meshInstances[i].meshID].pPrimitives[j].emissiveFactor;
      s_gltfFragInfo.u_BaseColorFactor = pScene->pMeshes[pScene->meshInstances[i].meshID].pPrimitives[j].baseColorFactor;
      s_gltfFragInfo.u_MetallicFactor = pScene->pMeshes[pScene->meshInstances[i].meshID].pPrimitives[j].metallicFactor;
      s_gltfFragInfo.u_RoughnessFactor = pScene->pMeshes[pScene->meshInstances[i].meshID].pPrimitives[j].roughnessFactor;

      s_gltfFragInfo.u_Exposure = 1.f;
      s_gltfFragInfo.u_NormalScale = (float)pScene->pMeshes[pScene->meshInstances[i].meshID].pPrimitives[j].normalScale;

      s_gltfFragInfo.u_alphaMode = pScene->pMeshes[pScene->meshInstances[i].meshID].pPrimitives[j].alpaMode;
      s_gltfFragInfo.u_AlphaCutoff = pScene->pMeshes[pScene->meshInstances[i].meshID].pPrimitives[j].alphaCutoff;

      if (g_allowNormalMaps && pScene->pMeshes[pScene->meshInstances[i].meshID].pPrimitives[j].normalTexture >= 0)
      {
        s_gltfFragInfo.u_NormalUVSet = 0;
        vcShader_BindTexture(s_pBasicShader, pScene->ppTextures[pScene->pMeshes[pScene->meshInstances[i].meshID].pPrimitives[j].normalTexture], 0, s_pBasicShaderNormalMapSampler);
      }
      else
      {
        s_gltfFragInfo.u_NormalUVSet = -1;
      }

      if (g_allowEmissiveMaps && pScene->pMeshes[pScene->meshInstances[i].meshID].pPrimitives[j].emissiveTexture >= 0)
      {
        s_gltfFragInfo.u_EmissiveUVSet = 0;
        vcShader_BindTexture(s_pBasicShader, pScene->ppTextures[pScene->pMeshes[pScene->meshInstances[i].meshID].pPrimitives[j].emissiveTexture], 0, s_pBasicShaderEmissiveMapSampler);
      }
      else
      {
        s_gltfFragInfo.u_EmissiveUVSet = -1;
      }

      if (g_allowOcclusionMaps && pScene->pMeshes[pScene->meshInstances[i].meshID].pPrimitives[j].occlusionTexture >= 0)
      {
        s_gltfFragInfo.u_OcclusionUVSet = 0;
        s_gltfFragInfo.u_OcclusionStrength = 1.f;
        vcShader_BindTexture(s_pBasicShader, pScene->ppTextures[pScene->pMeshes[pScene->meshInstances[i].meshID].pPrimitives[j].occlusionTexture], 0, s_pBasicShaderOcclusionMapSampler);
      }
      else
      {
        s_gltfFragInfo.u_OcclusionUVSet = -1;
      }

      if (g_allowBaseColourMaps && pScene->pMeshes[pScene->meshInstances[i].meshID].pPrimitives[j].baseColorTexture >= 0)
      {
        s_gltfFragInfo.u_BaseColorUVSet = 0;
        vcShader_BindTexture(s_pBasicShader, pScene->ppTextures[pScene->pMeshes[pScene->meshInstances[i].meshID].pPrimitives[j].baseColorTexture], 0, s_pBasicShaderBaseColourSampler);
      }
      else
      {
        s_gltfFragInfo.u_BaseColorUVSet = -1;
      }

      if (g_allowMetalRoughMaps && pScene->pMeshes[pScene->meshInstances[i].meshID].pPrimitives[j].metallicRoughnessTexture >= 0)
      {
        s_gltfFragInfo.u_MetallicRoughnessUVSet = 0;
        vcShader_BindTexture(s_pBasicShader, pScene->ppTextures[pScene->pMeshes[pScene->meshInstances[i].meshID].pPrimitives[j].metallicRoughnessTexture], 0, s_pBasicShaderMetallicRoughnessSampler);
      }
      else
      {
        s_gltfFragInfo.u_MetallicRoughnessUVSet = -1;
      }

      vcShader_BindConstantBuffer(s_pBasicShader, s_pBasicShaderFragUniformBuffer, &s_gltfFragInfo, sizeof(s_gltfFragInfo));

      if (pScene->pMeshes[pScene->meshInstances[i].meshID].pPrimitives[j].doubleSided)
        vcGLState_SetFaceMode(vcGLSFM_Solid, vcGLSCM_None, true, false);
      else
        vcGLState_SetFaceMode(vcGLSFM_Solid, vcGLSCM_Back, true, false);

      vcMesh_Render(pScene->pMeshes[pScene->meshInstances[i].meshID].pPrimitives[j].pMesh);
    }
  }

  return udR_Success;
}