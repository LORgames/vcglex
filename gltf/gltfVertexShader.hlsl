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

#ifdef USE_SKINNING
  float4x4 u_jointMatrix[JOINT_COUNT];
  float4x4 u_jointNormalMatrix[JOINT_COUNT];
#endif
}

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

#ifdef HAS_JOINT_SET1
  float4 a_Joint1;
#endif

#ifdef HAS_JOINT_SET2
  float4 a_Joint2;
#endif

#ifdef HAS_WEIGHT_SET1
  float4 a_Weight1;
#endif

#ifdef HAS_WEIGHT_SET2
  float4 a_Weight2;
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

#ifdef USE_SKINNING
float4x4 getSkinningMatrix(VS_INPUT input)
{
    float4x4 sk= float4x4(0);

    #if defined(HAS_WEIGHT_SET1) && defined(HAS_JOINT_SET1)
    sk+=
        input.a_Weight1.x * u_jointMatrix[int(input.a_Joint1.x)] +
        input.a_Weight1.y * u_jointMatrix[int(input.a_Joint1.y)] +
        input.a_Weight1.z * u_jointMatrix[int(input.a_Joint1.z)] +
        input.a_Weight1.w * u_jointMatrix[int(input.a_Joint1.w)];
    #endif

    #if defined(HAS_WEIGHT_SET2) && defined(HAS_JOINT_SET2)
    sk+=
        input.a_Weight2.x * u_jointMatrix[int(input.a_Joint2.x)] +
        input.a_Weight2.y * u_jointMatrix[int(input.a_Joint2.y)] +
        input.a_Weight2.z * u_jointMatrix[int(input.a_Joint2.z)] +
        input.a_Weight2.w * u_jointMatrix[int(input.a_Joint2.w)];
    #endif

    return skin;
}

float4x4 getSkinningNormalMatrix(VS_INPUT input)
{
    float4x4 sk= float4x4(0);

    #if defined(HAS_WEIGHT_SET1) && defined(HAS_JOINT_SET1)
    sk+=
        input.a_Weight1.x * u_jointNormalMatrix[int(input.a_Joint1.x)] +
        input.a_Weight1.y * u_jointNormalMatrix[int(input.a_Joint1.y)] +
        input.a_Weight1.z * u_jointNormalMatrix[int(input.a_Joint1.z)] +
        input.a_Weight1.w * u_jointNormalMatrix[int(input.a_Joint1.w)];
    #endif

    #if defined(HAS_WEIGHT_SET2) && defined(HAS_JOINT_SET2)
    sk+=
        input.a_Weight2.x * u_jointNormalMatrix[int(input.a_Joint2.x)] +
        input.a_Weight2.y * u_jointNormalMatrix[int(input.a_Joint2.y)] +
        input.a_Weight2.z * u_jointNormalMatrix[int(input.a_Joint2.z)] +
        input.a_Weight2.w * u_jointNormalMatrix[int(input.a_Joint2.w)];
    #endif

    return skin;
}
#endif // !USE_SKINNING

#ifdef USE_MORPHING
float4 getTargetPosition(VS_INPUT input)
{
    float4 pos = float4(0);

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
    float3 normal = float3(0);

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
    float3 tangent = float3(0);

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

#ifdef USE_SKINNING
    pos = getSkinningMatrix(input) * pos;
#endif

    return pos;
}

float3 getNormal(VS_INPUT input)
{
    float3 normal = input.a_Normal;

#ifdef USE_MORPHING
    normal += getTargetNormal(input);
#endif

#ifdef USE_SKINNING
    normal = float3x3(getSkinningNormalMatrix(input)) * normal;
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

#ifdef USE_SKINNING
    tangent = float3x3(getSkinningMatrix(input)) * tangent;
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