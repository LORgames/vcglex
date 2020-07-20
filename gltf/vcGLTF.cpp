#include "vcGLTF.h"

#include "vcGL/gl/vcMesh.h"
#include "vcGL/gl/vcShader.h"
#include "vcGL/gl/vcTexture.h"

#include "udPlatform.h"
#include "udJSON.h"
#include "udPlatformUtil.h"
#include "udFile.h"
#include "udStringUtil.h"

struct vcGLTFNode
{
  udDouble4x4 matrix;
};

struct vcGLTFMeshInstance
{
  int meshID;
  udDouble4x4 matrix;
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
  vcMesh** ppMeshes;
};

vcShader* s_pBasicShader = nullptr;

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

epilogue:
  if (result != udR_Success)
    udFree(pScene->pBuffers[bufferID].pBytes);

  return result;
}

udResult vcGLTF_CreateMesh(vcGLTFScene *pScene, const udJSON &root, int meshID)
{
  pScene->ppMeshes;

  const udJSON &mesh = root.Get("meshes[%d]", meshID);
  
  int numPrimitives = (int)mesh.Get("primitives").ArrayLength();

  printf("Mesh Primitives: %d", numPrimitives);

  for (int i = 0; i < numPrimitives; ++i)
  {
    const udJSON &primitive = mesh.Get("primitives[%d]", i);

    int mode = primitive.Get("mode").AsInt(4); // Points, Lines, Triangles
    int material = primitive.Get("material").AsInt(-1);

    if (mode != 4)
      __debugbreak();

    int indexAccessor = primitive.Get("indices").AsInt(-1);
    vcGLTFBuffer *pIndexBuffer = nullptr;
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

        if (bufferID < pScene->bufferCount && pScene->pBuffers[bufferID].pBytes == nullptr)
          vcGLTF_LoadBuffer(pScene, root, bufferID);

        if (pScene->pBuffers[bufferID].pBytes != nullptr)
          pIndexBuffer = &pScene->pBuffers[bufferID];
      }
    }

    const udJSON &attributes = primitive.Get("attributes");
    int totalAttributes = (int)attributes.MemberCount();
    vcVertexLayoutTypes *pTypes = udAllocType(vcVertexLayoutTypes, totalAttributes, udAF_None);

    int maxCount = -1;

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

      if (attributeType != 5126) // Float
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
        if (udStrEqual(pAccessorType, "VEC3"))
          pTypes[j] = vcVLT_Normal3;
        else
          __debugbreak();
      }
      else
      {
        __debugbreak();
      }
    }

    uint32_t vertLen = vcLayout_GetSize(pTypes, totalAttributes);
    uint8_t *pVertData = udAllocType(uint8_t, vertLen * maxCount, udAF_Zero);

    float *pVertFloats = (float*)pVertData;

    // Decode the buffers
    for (int vi = 0; vi < maxCount; ++vi)
    {
      for (int ai = 0; ai < totalAttributes; ++ai)
      {
        int attributeAccessorIndex = attributes.GetMember(ai)->AsInt(0);
        const udJSON& accessor = root.Get("accessors[%d]", attributeAccessorIndex);

        const char* pAccessorType = accessor.Get("type").AsString();

        int bufferID = root.Get("bufferViews[%d].buffer", accessor.Get("bufferView").AsInt(-1)).AsInt();
        if (bufferID <= -1)
          __debugbreak();

        if (bufferID < pScene->bufferCount && pScene->pBuffers[bufferID].pBytes != nullptr)
        {
          if (udStrEqual(pAccessorType, "VEC3"))
          {
            float *pFloats = (float*)pScene->pBuffers[bufferID].pBytes;
            pVertFloats[vi * totalAttributes * 6 + ai * 3 + 0] = pFloats[vi * 3 + 0];
            pVertFloats[vi * totalAttributes * 6 + ai * 3 + 1] = pFloats[vi * 3 + 1];
            pVertFloats[vi * totalAttributes * 6 + ai * 3 + 2] = pFloats[vi * 3 + 2];
          }
          else
          {
            __debugbreak();
          }
        }
      }
    }

    vcShader_CreateFromFile()

    if (pIndexBuffer == nullptr)
      vcMesh_Create(&pScene->ppMeshes[meshID], pTypes, totalAttributes, pVertData, maxCount, nullptr, 0, vcMF_NoIndexBuffer);
    else
      vcMesh_Create(&pScene->ppMeshes[meshID], pTypes, totalAttributes, pVertData, maxCount, pIndexBuffer->pBytes, indexCount);
  
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

    if (pScene->ppMeshes[pMesh->meshID] == nullptr)
      vcGLTF_CreateMesh(pScene, root, pMesh->meshID);
  }
  else if (!child.Get("children").IsVoid())
  {
    for (int i = 0; i < child.Get("children").ArrayLength(); ++i)
      vcGLTF_ProcessChildNode(pScene, root, root.Get("nodes[%d]", child.Get("children[%zu]", i).AsInt()), chainedMatrix);
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
  pScene->ppMeshes = udAllocType(vcMesh *, pScene->meshCount, udAF_Zero);

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
  udFree(pScene->ppMeshes);

  udFree(pScene);
}

udResult vcGLTF_Render(vcGLTFScene *pScene, udDouble4x4 globalMatrix)
{
  for (size_t i = 0; i < pScene->meshInstances.length; ++i)
    vcMesh_Render(pScene->ppMeshes[pScene->meshInstances[i].meshID]);

  udUnused(pScene);
  udUnused(globalMatrix);

  return udR_Unsupported;
}