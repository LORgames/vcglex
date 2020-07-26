#ifndef vcGLTF_h__
#define vcGLTF_h__

#include "vcGL/rendering/vcPolygonModel.h"

struct vcGLTFScene;

// Read the OBJ, optionally only reading a specific count of vertices (to test for valid format for example)
udResult vcGLTF_LoadPolygonModel(vcGLTFScene **ppScene, const char *pFilename, udWorkerPool *pWorkerPool);
void vcGLTF_Destroy(vcGLTFScene **ppScene);

udResult vcGLTF_Render(vcGLTFScene *ppScene, udDouble3 cameraPosition, udDouble4x4 worldMatrix, udDouble4x4 viewMatrix, udDouble4x4 projectionMatrix);

#endif //vcGLTF_h__
