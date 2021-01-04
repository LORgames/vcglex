#ifndef vcGLTF_h__
#define vcGLTF_h__

#include "udWorkerPool.h"
#include "udMath.h"

struct vcGLTFScene;
struct vcGLTFAnimation;
struct vcTexture;

// Read the OBJ, optionally only reading a specific count of vertices (to test for valid format for example)
udResult vcGLTF_Load(vcGLTFScene **ppScene, const char *pFilename, udWorkerPool *pWorkerPool);
void vcGLTF_Destroy(vcGLTFScene **ppScene);

void vcGLTF_GenerateGlobalShaders();
void vcGLTF_DestroyGlobalShaders();

enum vcGLTFRenderPass
{
  vcGLTFRP_Opaque,
  vcGLTFRP_Transparent,

  vcGLTFRP_Shadows,
};

enum vcGLTFLightType
{
  vcGLTFLightType_Directional = 0,
  vcGLTFLightType_Point = 1,
  vcGLTFLightType_Spot = 2,
};

struct vcGLTFLight
{
  udFloat3 direction;
  float range;

  udFloat3 color;
  float intensity;

  udFloat3 position;
  float innerConeCos;

  float outerConeCos;
  int type; //vcGLTFLightType

  udFloat2 __padding;
};

struct vcGLTFLightSet
{
  udFloat3 ambientLighting;
  int lightCount; // Upto 8
  vcGLTFLight lights[8];
};

udResult vcGLTF_Update(vcGLTFScene *pScene, double dt);
udResult vcGLTF_Render(vcGLTFScene *ppScene, udRay<double> camera, udDouble4x4 worldMatrix, udDouble4x4 viewMatrix, udDouble4x4 projectionMatrix, vcGLTFRenderPass pass, const vcGLTFLightSet &lighting);

// Some skinning stuff...
void vcGLTF_OverrideDiffuse(vcGLTFScene *pScene, vcTexture *pTexture);

// Some animation extraction helpers
int vcGLTFAnim_GetNumberOfAnimations(vcGLTFScene *pScene);
vcGLTFAnimation* vcGLTFAnim_GetAnimation(vcGLTFScene *pScene, int index);
void vcGLTFAnim_SetAnimation(vcGLTFScene *pScene, vcGLTFAnimation *pAnim);

#endif //vcGLTF_h__
