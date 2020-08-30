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

struct PS_OUTPUT
{
  float4 baseRGB_Metal : SV_Target0;
  float4 emitRGB_Rough : SV_Target1;
  float4 normXYZ_X : SV_Target2;
};

struct NormalInfo
{
  float3 ng;   // Geometric normal
  float3 n;    // Pertubed normal
  float3 t;    // Pertubed tangent
  float3 b;    // Pertubed bitangent
};

cbuffer u_FragSettings : register(b0)
{
  float3 u_Camera;
  float u_Exposure;
  float3 u_EmissiveFactor;

  float u_NormalScale;

  // Metallic Roughness
  float4 u_BaseColorFactor;
  float u_MetallicFactor;
  float u_RoughnessFactor;
  int u_BaseColorUVSet;
  int u_MetallicRoughnessUVSet;

  // General Material
  int u_NormalUVSet;
  int u_EmissiveUVSet;
  int u_OcclusionUVSet;
  float u_OcclusionStrength;

  // Alpha mode
  int u_alphaMode; // 0 = OPAQUE, 1 = ALPHAMASK, 2 = BLEND
  float u_AlphaCutoff;

  float2 __padding;
}

sampler normalSampler;
Texture2D u_NormalSampler;

sampler emissiveSampler;
Texture2D u_EmissiveSampler;

sampler occlusionSampler;
Texture2D u_OcclusionSampler;

sampler baseColorSampler;
Texture2D u_BaseColorSampler;

sampler metallicRoughnessSampler;
Texture2D u_MetallicRoughnessSampler;

struct MaterialInfo
{
  float perceptualRoughness; // roughness value, as authored by the model creator (input to shader)
  float3 f0; // full reflectance color (n incidence angle)

  float alphaRoughness; // roughness mapped to a more linear change in the roughness (proposed by [2])
  float3 albedoColor;

  float3 f90; // reflectance color at grazing angle
  float metallic;

  float3 n;
  float3 baseColor; // getBaseColor()
};

static const float GAMMA = 2.2;
static const float INV_GAMMA = 1.0 / GAMMA;
static const float M_PI = 3.141592653589793;

// linear to sRGB approximation
// see http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
float3 linearTosRGB(float3 color)
{
  return pow(color, float3(INV_GAMMA, INV_GAMMA, INV_GAMMA));
}

// sRGB to linear approximation
// see http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
float3 sRGBToLinear(float3 srgbIn)
{
  return float3(pow(srgbIn.xyz, float3(GAMMA, GAMMA, GAMMA)));
}

float4 sRGBToLinear(float4 srgbIn)
{
  return float4(sRGBToLinear(srgbIn.xyz), srgbIn.w);
}

float clampedDot(float3 x, float3 y)
{
  return clamp(dot(x, y), 0.0, 1.0);
}

float3 toneMap(float3 color)
{
  color *= u_Exposure;

  return linearTosRGB(color);
}

float2 getNormalUV(PS_INPUT input)
{
  float3 uv = float3(0.0, 0.0, 0.0);

  if (u_NormalUVSet >= 0)
  {
    uv = float3(u_NormalUVSet < 1 ? input.v_UVCoord1 : input.v_UVCoord2, 1.0);
  }

  return uv.xy;
}

float2 getEmissiveUV(PS_INPUT input)
{
  float3 uv = float3(0.0, 0.0, 0.0);

  if (u_EmissiveUVSet >= 0)
  {
    uv = float3(u_EmissiveUVSet < 1 ? input.v_UVCoord1 : input.v_UVCoord2, 1.0);
  }

  return uv.xy;
}

float2 getOcclusionUV(PS_INPUT input)
{
  float3 uv = float3(0.0, 0.0, 0.0);

  if (u_OcclusionUVSet >= 0)
  {
    uv = float3(u_OcclusionUVSet < 1 ? input.v_UVCoord1 : input.v_UVCoord2, 1.0);
  }

  return uv.xy;
}

float2 getBaseColorUV(PS_INPUT input)
{
  float3 uv = float3(0.0, 0.0, 0.0);

  if (u_BaseColorUVSet >= 0)
  {
    uv = float3(u_BaseColorUVSet < 1 ? input.v_UVCoord1 : input.v_UVCoord2, 1.0);
  }

  return uv.xy;
}

float2 getMetallicRoughnessUV(PS_INPUT input)
{
  float3 uv = float3(0.0, 0.0, 0.0);

  if (u_MetallicRoughnessUVSet >= 0)
  {
    uv = float3(u_MetallicRoughnessUVSet < 1 ? input.v_UVCoord1 : input.v_UVCoord2, 1.0);
  }

  return uv.xy;
}

// Get normal, tangent and bitangent vectors.
NormalInfo getNormalInfo(PS_INPUT input, float3 v)
{
  float2 uv = float2(0.0, 0.0);

  if (u_NormalUVSet >= 0)
    uv = (u_NormalUVSet < 1 ? input.v_UVCoord1 : input.v_UVCoord2);

  float3 uv_dx = ddx(float3(uv, 0.0));
  float3 uv_dy = ddy(float3(uv, 0.0));

  float3 t_ = (uv_dy.y * ddx(input.v_Position) - uv_dx.y * ddy(input.v_Position)) / (uv_dx.x * uv_dy.y - uv_dy.x * uv_dx.y);

  float3 n, t, b, ng;

  // Compute geometrical TBN:
#ifdef HAS_TANGENTS
    // Trivial TBN computation, present as vertex attribute.
    // Normalize eigenvectors as matrix is linearly interpolated.
  t = normalize(input.v_TBN[0]);
  b = normalize(input.v_TBN[1]);
  ng = normalize(input.v_TBN[2]);
#else
  // Normals are either present as vertex attributes or approximated.
  ng = normalize(input.v_Normal);
  t = normalize(t_ - ng * dot(ng, t_));
  b = cross(ng, t);
#endif

  // For a back-facing surface, the tangential basis vectors are negated.
  float facing = step(0.0, dot(v, ng)) * 2.0 - 1.0;
  t *= facing;
  b *= facing;
  ng *= facing;

  // Compute pertubed normals:
  if (u_NormalUVSet >= 0)
  {
    n = u_NormalSampler.Sample(normalSampler, uv).rgb * 2.0 - 1.0;
    n = n * float3(u_NormalScale, u_NormalScale, 1.0);
    n = mul(normalize(n), float3x3(t, b, ng));
  }
  else
  {
    n = ng;
  }

  NormalInfo info;
  info.ng = ng;
  info.t = t;
  info.b = b;
  info.n = n;

  return info;
}

float4 getBaseColor(PS_INPUT input)
{
  float4 baseColor = u_BaseColorFactor;
  float4 tintColor = float4(1.0, 1.0, 1.0, 1.0);

  if (u_BaseColorUVSet >= 0)
    baseColor *= sRGBToLinear(u_BaseColorSampler.Sample(baseColorSampler, getBaseColorUV(input)));

#ifdef HAS_VERTEX_COLOR
  tintColor = input.v_Color;
#endif

  return baseColor * tintColor;
}

MaterialInfo getMetallicRoughnessInfo(PS_INPUT input, MaterialInfo info, float f0_ior)
{
  info.metallic = u_MetallicFactor;
  info.perceptualRoughness = u_RoughnessFactor;

  if (u_MetallicRoughnessUVSet >= 0)
  {
    // Roughness is stored in the 'g' channel, metallic is stored in the 'b' channel.
    // This layout intentionally reserves the 'r' channel for (optional) occlusion map data
    float4 mrSample = u_MetallicRoughnessSampler.Sample(metallicRoughnessSampler, getMetallicRoughnessUV(input));
    info.perceptualRoughness *= mrSample.g;
    info.metallic *= mrSample.b;
  }

  // Achromatic f0 based on IOR.
  float3 f0 = float3(f0_ior, f0_ior, f0_ior);

  info.albedoColor = lerp(info.baseColor.rgb * (float3(1.0, 1.0, 1.0) - f0), float3(0.0, 0.0, 0.0), info.metallic);
  info.f0 = lerp(f0, info.baseColor.rgb, info.metallic);

  return info;
}

PS_OUTPUT main(PS_INPUT input)
{
  PS_OUTPUT output;
  float4 g_finalColor = float4(1.0, 1.0, 1.0, 1.0);

  float4 baseColor = getBaseColor(input);

  if (u_alphaMode == 0) //OPAQUE
    baseColor.a = 1.0;

#ifdef MATERIAL_UNLIT
  g_finalColor = (float4(linearTosRGB(baseColor.rgb), baseColor.a));
  return;
#endif

  float3 v = normalize(u_Camera - input.v_Position);
  NormalInfo normalInfo = getNormalInfo(input, v);
  float3 n = normalInfo.n;
  float3 t = normalInfo.t;
  float3 b = normalInfo.b;

  float NdotV = clampedDot(n, v);
  float TdotV = clampedDot(t, v);
  float BdotV = clampedDot(b, v);

  MaterialInfo materialInfo = (MaterialInfo)0;
  materialInfo.baseColor = baseColor.rgb;

  // The default index of refraction of 1.5 yields a dielectric normal incidence reflectance of 0.04.
  float ior = 1.5;
  float f0_ior = 0.04;

  materialInfo = getMetallicRoughnessInfo(input, materialInfo, f0_ior);

  materialInfo.perceptualRoughness = clamp(materialInfo.perceptualRoughness, 0.0, 1.0);
  materialInfo.metallic = clamp(materialInfo.metallic, 0.0, 1.0);

  // Roughness is authored as perceptual roughness; as is convention,
  // convert to material roughness by squaring the perceptual roughness.
  materialInfo.alphaRoughness = materialInfo.perceptualRoughness * materialInfo.perceptualRoughness;

  // Compute reflectance.
  float reflectance = max(max(materialInfo.f0.r, materialInfo.f0.g), materialInfo.f0.b);

  // Anything less than 2% is physically impossible and is instead considered to be shadowing. Compare to "Real-Time-Rendering" 4th editon on page 325.
  float clamped = clamp(reflectance * 50.0, 0.0, 1.0);
  materialInfo.f90 = float3(clamped, clamped, clamped);

  materialInfo.n = n;

  // LIGHTING
  float3 f_emissive = float3(0.0, 0.0, 0.0);

  f_emissive = u_EmissiveFactor;

  if (u_EmissiveUVSet >= 0)
    f_emissive *= sRGBToLinear(u_EmissiveSampler.Sample(emissiveSampler, getEmissiveUV(input))).rgb;

  float ao = 1.0;

  // Apply optional PBR terms for additional (optional) shading
  if (u_OcclusionUVSet >= 0)
  {
    ao = u_OcclusionSampler.Sample(occlusionSampler, getOcclusionUV(input)).r;
    baseColor = lerp(baseColor, baseColor * ao, u_OcclusionStrength);
  }

  if (u_alphaMode == 1) //MASK
  {
    // Late discard to avaoid samplig artifacts. See https://github.com/KhronosGroup/glTF-Sample-Viewer/issues/267
    if (baseColor.a < u_AlphaCutoff)
      discard;

    baseColor.a = 1.0;
  }

  output.baseRGB_Metal = float4(baseColor.xyz, materialInfo.metallic);
  output.emitRGB_Rough = float4(f_emissive, materialInfo.perceptualRoughness);
  output.normXYZ_X = float4(n * 0.5 + 0.5, 0.0);

  return output;
}
