cbuffer u_cameraPlaneParams
{
  float s_CameraNearPlane;
  float s_CameraFarPlane;
  float u_clipZNear;
  float u_clipZFar;
}; 

cbuffer u_EveryFrame : register(b0)
{
  float4x4 u_ViewProjectionMatrix;
  float4x4 u_ModelMatrix;
  float4x4 u_NormalMatrix;

#ifdef USE_MORPHING
  uniform float u_morphWeights[WEIGHT_COUNT];
#endif
}

#ifdef HAS_SKINNING
cbuffer u_SkinningInfo : register(b1)
{
  float4x4 u_jointMatrix[72];
  float4x4 u_jointNormalMatrix[72];
}
#endif

struct VS_INPUT
{
  float3 a_Position : POSITION;
  float3 a_Normal : NORMAL;

#ifdef HAS_TANGENTS
  float4 a_Tangent : TANGENT;
#endif

#ifdef HAS_UV_SET1
  float2 a_UV1 : TEXCOORD0;
#endif

#ifdef HAS_UV_SET2
  float2 a_UV2 : TEXCOORD1;
#endif

#ifdef HAS_VERTEX_COLOR
  float4 a_Color : COLOR0;
#endif

#ifdef HAS_SKINNING
  float4 a_Joints : BLENDINDICES0;
  float4 a_Weights : BLENDWEIGHT0;
#endif

#ifdef HAS_SKINNING_EXTENDED
  float4 a_JointsEx : BLENDINDICES1;
  float4 a_WeightsEx : BLENDWEIGHT1;
#endif

#ifdef HAS_TARGET_POSITION0
  float3 a_Target_Position0;
#endif

#ifdef HAS_TARGET_POSITION1
  float3 a_Target_Position1;
#endif

#ifdef HAS_TARGET_POSITION2
  float3 a_Target_Position2;
#endif

#ifdef HAS_TARGET_POSITION3
  float3 a_Target_Position3;
#endif

#ifdef HAS_TARGET_POSITION4
  float3 a_Target_Position4;
#endif

#ifdef HAS_TARGET_POSITION5
  float3 a_Target_Position5;
#endif

#ifdef HAS_TARGET_POSITION6
  float3 a_Target_Position6;
#endif

#ifdef HAS_TARGET_POSITION7
  float3 a_Target_Position7;
#endif

#ifdef HAS_TARGET_NORMAL0
  float3 a_Target_Normal0;
#endif

#ifdef HAS_TARGET_NORMAL1
  float3 a_Target_Normal1;
#endif

#ifdef HAS_TARGET_NORMAL2
  float3 a_Target_Normal2;
#endif

#ifdef HAS_TARGET_NORMAL3
  float3 a_Target_Normal3;
#endif

#ifdef HAS_TARGET_TANGENT0
  float3 a_Target_Tangent0;
#endif

#ifdef HAS_TARGET_TANGENT1
  float3 a_Target_Tangent1;
#endif

#ifdef HAS_TARGET_TANGENT2
  float3 a_Target_Tangent2;
#endif

#ifdef HAS_TARGET_TANGENT3
  float3 a_Target_Tangent3;
#endif
};

struct PS_INPUT
{
  float4 s_Position : SV_POSITION;
  float3 v_Position : POSITION0;

#ifdef HAS_TANGENTS
  float3x3 v_TBN : NORMAL;
#else
  float3 v_Normal : NORMAL;
#endif

  float2 v_UVCoord1 : TEXCOORD0;
  float2 v_UVCoord2 : TEXCOORD1;

#ifdef HAS_VERTEX_COLOR
  float4 v_Color : COLOR0;
#endif
};

#ifdef HAS_SKINNING
float4x4 getSkinningMatrix(VS_INPUT input)
{
  float4x4 skin = float4x4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);

    skin +=
        input.a_Weights.x * u_jointMatrix[int(input.a_Joints.x)] +
        input.a_Weights.y * u_jointMatrix[int(input.a_Joints.y)] +
        input.a_Weights.z * u_jointMatrix[int(input.a_Joints.z)] +
        input.a_Weights.w * u_jointMatrix[int(input.a_Joints.w)];

    #ifdef HAS_SKINNING_EXTENDED
    skin +=
        input.a_WeightsEx.x * u_jointMatrix[int(input.a_JointsEx.x)] +
        input.a_WeightsEx.y * u_jointMatrix[int(input.a_JointsEx.y)] +
        input.a_WeightsEx.z * u_jointMatrix[int(input.a_JointsEx.z)] +
        input.a_WeightsEx.w * u_jointMatrix[int(input.a_JointsEx.w)];
    #endif

    return skin;
}

float4x4 getSkinningNormalMatrix(VS_INPUT input)
{
  float4x4 skin = float4x4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);

  skin +=
        input.a_Weights.x * u_jointNormalMatrix[int(input.a_Joints.x)] +
        input.a_Weights.y * u_jointNormalMatrix[int(input.a_Joints.y)] +
        input.a_Weights.z * u_jointNormalMatrix[int(input.a_Joints.z)] +
        input.a_Weights.w * u_jointNormalMatrix[int(input.a_Joints.w)];

    #ifdef HAS_SKINNING_EXTENDED
  skin +=
        input.a_WeightsEx.x * u_jointNormalMatrix[int(input.a_JointsEx.x)] +
        input.a_WeightsEx.y * u_jointNormalMatrix[int(input.a_JointsEx.y)] +
        input.a_WeightsEx.z * u_jointNormalMatrix[int(input.a_JointsEx.z)] +
        input.a_WeightsEx.w * u_jointNormalMatrix[int(input.a_JointsEx.w)];
    #endif

    return skin;
}
#endif // HAS_SKINNING

#ifdef USE_MORPHING
float4 getTargetPosition(VS_INPUT input)
{
    float4 pos = float4(0, 0, 0, 0);

#ifdef HAS_TARGET_POSITION0
    pos.xyz += u_morphWeights[0] * input.a_Target_Position0;
#endif

#ifdef HAS_TARGET_POSITION1
    pos.xyz += u_morphWeights[1] * input.a_Target_Position1;
#endif

#ifdef HAS_TARGET_POSITION2
    pos.xyz += u_morphWeights[2] * input.a_Target_Position2;
#endif

#ifdef HAS_TARGET_POSITION3
    pos.xyz += u_morphWeights[3] * input.a_Target_Position3;
#endif

#ifdef HAS_TARGET_POSITION4
    pos.xyz += u_morphWeights[4] * input.a_Target_Position4;
#endif

    return pos;
}

float3 getTargetNormal(VS_INPUT input)
{
    float3 normal = float3(0, 0, 0);

#ifdef HAS_TARGET_NORMAL0
    normal += u_morphWeights[0] * input.a_Target_Normal0;
#endif

#ifdef HAS_TARGET_NORMAL1
    normal += u_morphWeights[1] * input.a_Target_Normal1;
#endif

#ifdef HAS_TARGET_NORMAL2
    normal += u_morphWeights[2] * input.a_Target_Normal2;
#endif

#ifdef HAS_TARGET_NORMAL3
    normal += u_morphWeights[3] * input.a_Target_Normal3;
#endif

#ifdef HAS_TARGET_NORMAL4
    normal += u_morphWeights[4] * input.a_Target_Normal4;
#endif

    return normal;
}

float3 getTargetTangent(VS_INPUT input)
{
    float3 tangent = float3(0, 0, 0);

#ifdef HAS_TARGET_TANGENT0
    tangent += u_morphWeights[0] * input.a_Target_Tangent0;
#endif

#ifdef HAS_TARGET_TANGENT1
    tangent += u_morphWeights[1] * input.a_Target_Tangent1;
#endif

#ifdef HAS_TARGET_TANGENT2
    tangent += u_morphWeights[2] * input.a_Target_Tangent2;
#endif

#ifdef HAS_TARGET_TANGENT3
    tangent += u_morphWeights[3] * input.a_Target_Tangent3;
#endif

#ifdef HAS_TARGET_TANGENT4
    tangent += u_morphWeights[4] * input.a_Target_Tangent4;
#endif

    return tangent;
}

#endif // !USE_MORPHING

float4 getPosition(VS_INPUT input)
{
    float4 pos = float4(input.a_Position, 1.0);

#ifdef USE_MORPHING
    pos += getTargetPosition(input);
#endif

#ifdef HAS_SKINNING
    pos = mul(getSkinningMatrix(input), pos);
#endif

    return pos;
}

float3 getNormal(VS_INPUT input)
{
    float3 normal = input.a_Normal;

#ifdef USE_MORPHING
    normal += getTargetNormal(input);
#endif

#ifdef HAS_SKINNING
    normal = mul(getSkinningNormalMatrix(input), float4(normal, 0)).xyz;
#endif

    return normalize(normal);
}

#ifdef HAS_TANGENTS
float3 getTangent(VS_INPUT input)
{
    float3 tangent = input.a_Tangent.xyz;

#ifdef USE_MORPHING
    tangent += getTargetTangent(input);
#endif

#ifdef HAS_SKINNING
    tangent = mul(getSkinningMatrix(input), float4(tangent, 0)).xyz;
#endif

    return normalize(tangent);
}
#endif

PS_INPUT main(VS_INPUT input)
{
  PS_INPUT output;

  float4 pos = mul(u_ModelMatrix, getPosition(input));
  output.v_Position = float3(pos.xyz) / pos.w;

#ifdef HAS_TANGENTS
  float3 normalW = normalize(mul(u_NormalMatrix, float4(getNormal(input), 0.0)).xyz);
  float3 tangentW = normalize(mul(u_ModelMatrix, float4(getTangent(input), 0.0)).xyz);
  float3 bitangentW = cross(normalW, tangentW) * input.a_Tangent.w;
  output.v_TBN = float3x3(tangentW, bitangentW, normalW);
#else // !HAS_TANGENTS
  output.v_Normal = normalize(mul(u_NormalMatrix, float4(getNormal(input), 0.0)).xyz);
#endif

  output.v_UVCoord1 = float2(0.0, 0.0);
  output.v_UVCoord2 = float2(0.0, 0.0);

#ifdef HAS_UV_SET1
  output.v_UVCoord1 = input.a_UV1;
#endif

#ifdef HAS_UV_SET2
  output.v_UVCoord2 = input.a_UV2;
#endif

#if defined(HAS_VERTEX_COLOR)
  output.v_Color = input.a_Color;
#endif

  output.s_Position = mul(u_ViewProjectionMatrix, pos);

  return output;
}