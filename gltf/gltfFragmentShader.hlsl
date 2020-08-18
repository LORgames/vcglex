//
// This fragment shader defines a reference implementation for Physically Based Shading of
// a microfacet surface material defined by a glTF model.
//
// References:
// [1] Real Shading in Unreal Engine 4
//     http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
// [2] Physically Based Shading at Disney
//     http://blog.selfshadow.com/publications/s2012-shading-course/burley/s2012_pbs_disney_brdf_notes_v3.pdf
// [3] README.md - Environment Maps
//     https://github.com/KhronosGroup/glTF-WebGL-PBR/#environment-maps
// [4] "An Inexpensive BRDF Model for Physically based Rendering" by Christophe Schlick
//     https://www.cs.virginia.edu/~jdl/bib/appearance/analytic%20models/schlick94b.pdf
// [5] "KHR_materials_clearcoat"
//     https://github.com/ux3d/glTF/tree/KHR_materials_pbrClearcoat/extensions/2.0/Khronos/KHR_materials_clearcoat
// [6] "KHR_materials_specular"
//     https://github.com/ux3d/glTF/tree/KHR_materials_pbrClearcoat/extensions/2.0/Khronos/KHR_materials_specular
// [7] "KHR_materials_subsurface"
//     https://github.com/KhronosGroup/glTF/pull/1766
// [8] "KHR_materials_thinfilm"
//     https://github.com/ux3d/glTF/tree/extensions/KHR_materials_thinfilm/extensions/2.0/Khronos/KHR_materials_thinfilm

#ifndef LIGHT_COUNT
# define LIGHT_COUNT 2
#endif

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

SamplerState MeshTextureSampler
{
  Filter = MIN_MAG_MIP_LINEAR;
  AddressU = Wrap;
  AddressV = Wrap;
};

struct PS_OUTPUT
{
  float4 Color0 : SV_Target;
};

struct NormalInfo {
  float3 ng;   // Geometric normal
  float3 n;    // Pertubed normal
  float3 t;    // Pertubed tangent
  float3 b;    // Pertubed bitangent
};

// KHR_lights_punctual extension.
// see https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Khronos/KHR_lights_punctual
struct Light
{
  float3 direction;
  float range;

  float3 color;
  float intensity;

  float3 position;
  float innerConeCos;

  float outerConeCos;
  int type;

  float2 padding;
};

static const int LightType_Directional = 0;
static const int LightType_Point = 1;
static const int LightType_Spot = 2;

cbuffer u_FragSettings : register(b0)
{
  float3 u_Camera;
  float u_Exposure;
  float3 u_EmissiveFactor;

  float u_NormalScale;

  // Metallic Roughness
#ifdef MATERIAL_METALLICROUGHNESS
  float4 u_BaseColorFactor;
  float u_MetallicFactor;
  float u_RoughnessFactor;
  int u_BaseColorUVSet;
  int u_MetallicRoughnessUVSet;

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

  float2 __padding;

#ifdef HAS_NORMAL_UV_TRANSFORM
  float3x3 u_NormalUVTransform;
#endif

#ifdef HAS_EMISSIVE_UV_TRANSFORM
  float3x3 u_EmissiveUVTransform;
#endif

# ifdef HAS_OCCLUSION_UV_TRANSFORM
  float3x3 u_OcclusionUVTransform;
# endif

#ifdef USE_PUNCTUAL
  Light u_Lights[LIGHT_COUNT];
#endif

#ifdef USE_IBL
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

Texture2D u_DiffuseSampler;
Texture2D u_SpecularGlossinessSampler;
TextureCube u_LambertianEnvSampler;
TextureCube u_GGXEnvSampler;
Texture2D u_GGXLUT;
TextureCube u_CharlieEnvSampler;
Texture2D u_CharlieLUT;
Texture2D u_ClearcoatSampler;
Texture2D u_ClearcoatRoughnessSampler;
Texture2D u_ClearcoatNormalSampler;
Texture2D u_SheenColorIntensitySampler;
Texture2D u_MetallicRoughnessSpecularSampler;
Texture2D u_SubsurfaceColorSampler;
Texture2D u_SubsurfaceThicknessSampler;
Texture2D u_ThinFilmLUT;
Texture2D u_ThinFilmSampler;
Texture2D u_ThinFilmThicknessSampler;
Texture2D u_ThicknessSampler;
Texture2D u_AnisotropySampler;
Texture2D u_AnisotropyDirectionSampler;

struct MaterialInfo
{
  float perceptualRoughness;      // roughness value, as authored by the model creator (input to shader)
  float3 f0;                        // full reflectance color (n incidence angle)

  float alphaRoughness;           // roughness mapped to a more linear change in the roughness (proposed by [2])
  float3 albedoColor;

  float3 f90;                       // reflectance color at grazing angle
  float metallic;

  float3 n;
  float3 baseColor; // getBaseColor()

  float sheenIntensity;
  float3 sheenColor;
  float sheenRoughness;

  float anisotropy;

  float3 clearcoatF0;
  float3 clearcoatF90;
  float clearcoatFactor;
  float3 clearcoatNormal;
  float clearcoatRoughness;

  float subsurfaceScale;
  float subsurfaceDistortion;
  float subsurfacePower;
  float3 subsurfaceColor;
  float subsurfaceThickness;

  float thinFilmFactor;
  float thinFilmThickness;

  float thickness;

  float3 absorption;

  float transmission;
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

// Uncharted 2 tone map
// see: http://filmicworlds.com/blog/filmic-tonemapping-operators/
float3 toneMapUncharted2Impl(float3 color)
{
  const float A = 0.15;
  const float B = 0.50;
  const float C = 0.10;
  const float D = 0.20;
  const float E = 0.02;
  const float F = 0.30;
  return ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;
}

float3 toneMapUncharted(float3 color)
{
  const float W = 11.2;
  color = toneMapUncharted2Impl(color * 2.0);
  float3 whiteScale = 1.0 / toneMapUncharted2Impl(float3(W, W, W));
  return linearTosRGB(color * whiteScale);
}

// Hejl Richard tone map
// see: http://filmicworlds.com/blog/filmic-tonemapping-operators/
float3 toneMapHejlRichard(float3 color)
{
  color = max(float3(0.0, 0.0, 0.0), color - float3(0.004, 0.004, 0.004));
  return (color * (6.2 * color + .5)) / (color * (6.2 * color + 1.7) + 0.06);
}

// ACES tone map
// see: https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
float3 toneMapACES(float3 color)
{
  const float A = 2.51;
  const float B = 0.03;
  const float C = 2.43;
  const float D = 0.59;
  const float E = 0.14;
  return linearTosRGB(clamp((color * (A * color + B)) / (color * (C * color + D) + E), 0.0, 1.0));
}

float3 toneMap(float3 color)
{
  color *= u_Exposure;

#ifdef TONEMAP_UNCHARTED
  return toneMapUncharted(color);
#endif

#ifdef TONEMAP_HEJLRICHARD
  return toneMapHejlRichard(color);
#endif

#ifdef TONEMAP_ACES
  return toneMapACES(color);
#endif

  return linearTosRGB(color);
}

float2 getNormalUV(PS_INPUT input)
{
  float3 uv = float3(0.0, 0.0, 0.0);

  if (u_NormalUVSet >= 0)
  {
    uv = float3(u_NormalUVSet < 1 ? input.v_UVCoord1 : input.v_UVCoord2, 1.0);

#ifdef HAS_NORMAL_UV_TRANSFORM
    uv *= u_NormalUVTransform;
#endif
  }

  return uv.xy;
}

float2 getEmissiveUV(PS_INPUT input)
{
  float3 uv = float3(0.0, 0.0, 0.0);

  if (u_EmissiveUVSet >= 0)
  {
    uv = float3(u_EmissiveUVSet < 1 ? input.v_UVCoord1 : input.v_UVCoord2, 1.0);

#ifdef HAS_EMISSIVE_UV_TRANSFORM
    uv *= u_EmissiveUVTransform;
#endif
  }

  return uv.xy;
}

float2 getOcclusionUV(PS_INPUT input)
{
  float3 uv = float3(0.0, 0.0, 0.0);

  if (u_OcclusionUVSet >= 0)
  {
    uv = float3(u_OcclusionUVSet < 1 ? input.v_UVCoord1 : input.v_UVCoord2, 1.0);

#ifdef HAS_OCCLUSION_UV_TRANSFORM
    uv *= u_OcclusionUVTransform;
#endif
  }

  return uv.xy;
}

float2 getBaseColorUV(PS_INPUT input)
{
  float3 uv = float3(0.0, 0.0, 0.0);

  if (u_BaseColorUVSet >= 0)
  {
    uv = float3(u_BaseColorUVSet < 1 ? input.v_UVCoord1 : input.v_UVCoord2, 1.0);

#ifdef HAS_BASECOLOR_UV_TRANSFORM
    uv *= u_BaseColorUVTransform;
#endif
  }

  return uv.xy;
}

float2 getMetallicRoughnessUV(PS_INPUT input)
{
  float3 uv = float3(0.0, 0.0, 0.0);

  if (u_MetallicRoughnessUVSet >= 0)
  {
    uv = float3(u_MetallicRoughnessUVSet < 1 ? input.v_UVCoord1 : input.v_UVCoord2, 1.0);

#ifdef HAS_METALLICROUGHNESS_UV_TRANSFORM
    uv *= u_MetallicRoughnessUVTransform;
#endif
  }

  return uv.xy;
}

#ifdef HAS_SPECULAR_GLOSSINESS_MAP
float2 getSpecularGlossinessUV(PS_INPUT input)
{
  float3 uv = float3(u_SpecularGlossinessUVSet < 1 ? input.v_UVCoord1 : input.v_UVCoord2, 1.0);

#ifdef HAS_SPECULARGLOSSINESS_UV_TRANSFORM
  uv *= u_SpecularGlossinessUVTransform;
#endif

  return uv.xy;
}
#endif

#ifdef HAS_DIFFUSE_MAP
float2 getDiffuseUV(PS_INPUT input)
{
  float3 uv = float3(u_DiffuseUVSet < 1 ? input.v_UVCoord1 : input.v_UVCoord2, 1.0);

#ifdef HAS_DIFFUSE_UV_TRANSFORM
  uv *= u_DiffuseUVTransform;
#endif

  return uv.xy;
}
#endif

#ifdef MATERIAL_CLEARCOAT
float2 getClearcoatUV(PS_INPUT input)
{
  float3 uv = float3(u_ClearcoatUVSet < 1 ? input.v_UVCoord1 : input.v_UVCoord2, 1.0);
#ifdef HAS_CLEARCOAT_UV_TRANSFORM
  uv *= u_ClearcoatUVTransform;
#endif
  return uv.xy;
}

float2 getClearcoatRoughnessUV(PS_INPUT input)
{
  float3 uv = float3(u_ClearcoatRoughnessUVSet < 1 ? input.v_UVCoord1 : input.v_UVCoord2, 1.0);
#ifdef HAS_CLEARCOATROUGHNESS_UV_TRANSFORM
  uv *= u_ClearcoatRoughnessUVTransform;
#endif
  return uv.xy;
}

float2 getClearcoatNormalUV(PS_INPUT input)
{
  float3 uv = float3(u_ClearcoatNormalUVSet < 1 ? input.v_UVCoord1 : input.v_UVCoord2, 1.0);
#ifdef HAS_CLEARCOATNORMAL_UV_TRANSFORM
  uv *= u_ClearcoatNormalUVTransform;
#endif
  return uv.xy;
}
#endif

#ifdef HAS_SHEEN_COLOR_INTENSITY_MAP
float2 getSheenUV(PS_INPUT input)
{
  float3 uv = float3(u_SheenColorIntensityUVSet < 1 ? input.v_UVCoord1 : input.v_UVCoord2, 1.0);
#ifdef HAS_SHEENCOLORINTENSITY_UV_TRANSFORM
  uv *= u_SheenUVTransform;
#endif
  return uv.xy;
}
#endif

#ifdef HAS_METALLICROUGHNESS_SPECULAROVERRIDE_MAP
float2 getMetallicRoughnessSpecularUV(PS_INPUT input)
{
  float3 uv = float3(u_MetallicRougnessSpecularTextureUVSet < 1 ? input.v_UVCoord1 : input.v_UVCoord2, 1.0);
#ifdef HAS_METALLICROUGHNESSSPECULAR_UV_TRANSFORM
  uv *= u_MetallicRougnessSpecularUVTransform;
#endif
  return uv.xy;
}
#endif

#ifdef HAS_SUBSURFACE_COLOR_MAP
float2 getSubsurfaceColorUV(PS_INPUT input)
{
  float3 uv = float3(u_SubsurfaceColorUVSet < 1 ? input.v_UVCoord1 : input.v_UVCoord2, 1.0);
#ifdef HAS_SUBSURFACECOLOR_UV_TRANSFORM
  uv *= u_SubsurfaceColorUVTransform;
#endif
  return uv.xy;
}
#endif

#ifdef HAS_SUBSURFACE_THICKNESS_MAP
float2 getSubsurfaceThicknessUV(PS_INPUT input)
{
  float3 uv = float3(u_SubsurfaceThicknessUVSet < 1 ? input.v_UVCoord1 : input.v_UVCoord2, 1.0);
#ifdef HAS_SUBSURFACETHICKNESS_UV_TRANSFORM
  uv *= u_SubsurfaceThicknessUVTransform;
#endif
  return uv.xy;
}
#endif

#ifdef HAS_THIN_FILM_MAP
float2 getThinFilmUV(PS_INPUT input)
{
  float3 uv = float3(u_ThinFilmUVSet < 1 ? input.v_UVCoord1 : input.v_UVCoord2, 1.0);

#ifdef HAS_THIN_FILM_UV_TRANSFORM
  uv *= u_ThinFilmUVTransform;
#endif

  return uv.xy;
}
#endif

#ifdef HAS_THIN_FILM_THICKNESS_MAP
float2 getThinFilmThicknessUV(PS_INPUT input)
{
  float3 uv = float3(u_ThinFilmThicknessUVSet < 1 ? input.v_UVCoord1 : input.v_UVCoord2, 1.0);

#ifdef HAS_THIN_FILM_THICKNESS_UV_TRANSFORM
  uv *= u_ThinFilmThicknessUVTransform;
#endif

  return uv.xy;
}
#endif

#ifdef HAS_THICKNESS_MAP
float2 getThicknessUV(PS_INPUT input)
{
  float3 uv = float3(u_ThicknessUVSet < 1 ? input.v_UVCoord1 : input.v_UVCoord2, 1.0);

#ifdef HAS_THICKNESS_UV_TRANSFORM
  uv *= u_ThicknessUVTransform;
#endif

  return uv.xy;
}
#endif

#ifdef MATERIAL_ANISOTROPY
float2 getAnisotropyUV(PS_INPUT input)
{
  float3 uv = float3(u_AnisotropyUVSet < 1 ? input.v_UVCoord1 : input.v_UVCoord2, 1.0);

#ifdef HAS_ANISOTROPY_UV_TRANSFORM
  uv *= u_AnisotropyUVTransform;
#endif

  return uv.xy;
}

float2 getAnisotropyDirectionUV(PS_INPUT input)
{
  float3 uv = float3(u_AnisotropyDirectionUVSet < 1 ? input.v_UVCoord1 : input.v_UVCoord2, 1.0);

#ifdef HAS_ANISOTROPY_DIRECTION_UV_TRANSFORM
  uv *= u_AnisotropyDirectionUVTransform;
#endif

  return uv.xy;
}
#endif

float4 getVertexColor(PS_INPUT input)
{
  float4 color = float4(1.0, 1.0, 1.0, 1.0);

#ifdef HAS_VERTEX_COLOR
  color = input.v_Color;
#endif

  return color;
}

float clampedDot(float3 x, float3 y)
{
  return clamp(dot(x, y), 0.0, 1.0);
}

float sq(float t)
{
  return t * t;
}

float2 sq(float2 t)
{
  return t * t;
}

float3 sq(float3 t)
{
  return t * t;
}

float4 sq(float4 t)
{
  return t * t;
}

float3 transmissionAbsorption(float3 v, float3 n, float ior, float thickness, float3 absorptionColor)
{
  float3 r = refract(-v, n, 1.0 / ior);
  return exp(-absorptionColor * thickness * dot(-n, r));
}
//
// Fresnel
//
// http://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html
// https://github.com/wdas/brdf/tree/master/src/brdfs
// https://google.github.io/filament/Filament.md.html
//

float3 F_None(float3 f0, float3 f90, float VdotH)
{
  return f0;
}

// The following equation models the Fresnel reflectance term of the spec equation (aka F())
// Implementation of fresnel from [4], Equation 15
float3 F_Schlick(float3 f0, float3 f90, float VdotH)
{
  return f0 + (f90 - f0) * pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0);
}

float3 F_CookTorrance(float3 f0, float3 f90, float VdotH)
{
  float3 f0_sqrt = sqrt(f0);
  float3 ior = (1.0 + f0_sqrt) / (1.0 - f0_sqrt);
  float3 c = float3(VdotH, VdotH, VdotH);
  float3 g = sqrt(sq(ior) + c * c - 1.0);
  return 0.5 * pow(g - c, float3(2.0, 2.0, 2.0)) / pow(g + c, float3(2.0, 2.0, 2.0)) * (1.0 + pow(c * (g + c) - 1.0, float3(2.0, 2.0, 2.0)) / pow(c * (g - c) + 1.0, float3(2.0, 2.0, 2.0)));
}

// Smith Joint GGX
// Note: Vis = G / (4 * NdotL * NdotV)
// see Eric Heitz. 2014. Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs. Journal of Computer Graphics Techniques, 3
// see Real-Time Rendering. Page 331 to 336.
// see https://google.github.io/filament/Filament.md.html#materialsystem/specularbrdf/geometricshadowing(specularg)
float V_GGX(float NdotL, float NdotV, float alphaRoughness)
{
  float alphaRoughnessSq = alphaRoughness * alphaRoughness;

  float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
  float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);

  float GGX = GGXV + GGXL;
  if (GGX > 0.0)
  {
    return 0.5 / GGX;
  }
  return 0.0;
}

// Anisotropic GGX visibility function, with height correlation.
// T: Tanget, B: Bi-tanget
float V_GGX_anisotropic(float NdotL, float NdotV, float BdotV, float TdotV, float TdotL, float BdotL, float anisotropy, float at, float ab)
{
  float GGXV = NdotL * length(float3(at * TdotV, ab * BdotV, NdotV));
  float GGXL = NdotV * length(float3(at * TdotL, ab * BdotL, NdotL));
  float v = 0.5 / (GGXV + GGXL);
  return clamp(v, 0.0, 1.0);
}

// https://github.com/google/filament/blob/master/shaders/src/brdf.fs#L136
// https://github.com/google/filament/blob/master/libs/ibl/src/CubemapIBL.cpp#L179
// Note: Google call it V_Ashikhmin and V_Neubelt
float V_Ashikhmin(float NdotL, float NdotV)
{
  return clamp(1.0 / (4.0 * (NdotL + NdotV - NdotL * NdotV)), 0.0, 1.0);
}

// https://github.com/google/filament/blob/master/shaders/src/brdf.fs#L131
float V_Kelemen(float LdotH)
{
  // Kelemen 2001, "A Microfacet Based Coupled Specular-Matte BRDF Model with Importance Sampling"
  return 0.25 / (LdotH * LdotH);
}

// The following equation(s) model the distribution of microfacet normals across the area being drawn (aka D())
// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from EPIC Games [1], Equation 3.
float D_GGX(float NdotH, float alphaRoughness)
{
  float alphaRoughnessSq = alphaRoughness * alphaRoughness;
  float f = (NdotH * NdotH) * (alphaRoughnessSq - 1.0) + 1.0;
  return alphaRoughnessSq / (M_PI * f * f);
}

// Anisotropic GGX NDF with a single anisotropy parameter controlling the normal orientation.
// See https://google.github.io/filament/Filament.html#materialsystem/anisotropicmodel
// T: Tanget, B: Bi-tanget
float D_GGX_anisotropic(float NdotH, float TdotH, float BdotH, float anisotropy, float at, float ab)
{
  float a2 = at * ab;
  float3 f = float3(ab * TdotH, at * BdotH, a2 * NdotH);
  float w2 = a2 / dot(f, f);
  return a2 * w2 * w2 / M_PI;
}

float D_Ashikhmin(float NdotH, float alphaRoughness)
{
  // Ashikhmin 2007, "Distribution-based BRDFs"
  float a2 = alphaRoughness * alphaRoughness;
  float cos2h = NdotH * NdotH;
  float sin2h = 1.0 - cos2h;
  float sin4h = sin2h * sin2h;
  float cot2 = -cos2h / (a2 * sin2h);
  return 1.0 / (M_PI * (4.0 * a2 + 1.0) * sin4h) * (4.0 * exp(cot2) + sin4h);
}

//Sheen implementation-------------------------------------------------------------------------------------
// See  https://github.com/sebavan/glTF/tree/KHR_materials_sheen/extensions/2.0/Khronos/KHR_materials_sheen

// Estevez and Kulla http://www.aconty.com/pdf/s2017_pbs_imageworks_sheen.pdf
float D_Charlie(float sheenRoughness, float NdotH)
{
  sheenRoughness = max(sheenRoughness, 0.000001); //clamp (0,1]
  float alphaG = sheenRoughness * sheenRoughness;
  float invR = 1.0 / alphaG;
  float cos2h = NdotH * NdotH;
  float sin2h = 1.0 - cos2h;
  return (2.0 + invR) * pow(sin2h, invR * 0.5) / (2.0 * M_PI);
}

//https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#acknowledgments AppendixB
float3 BRDF_lambertian(float3 f0, float3 f90, float3 diffuseColor, float VdotH)
{
  // see https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
  return (1.0 - F_Schlick(f0, f90, VdotH)) * (diffuseColor / M_PI);
}

//  https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#acknowledgments AppendixB
float3 BRDF_specularGGX(float3 f0, float3 f90, float alphaRoughness, float VdotH, float NdotL, float NdotV, float NdotH)
{
  float3 F = F_Schlick(f0, f90, VdotH);
  float Vis = V_GGX(NdotL, NdotV, alphaRoughness);
  float D = D_GGX(NdotH, alphaRoughness);

  return F * Vis * D;
}

float3 BRDF_specularAnisotropicGGX(float3 f0, float3 f90, float alphaRoughness, float VdotH, float NdotL, float NdotV, float NdotH,
  float BdotV, float TdotV, float TdotL, float BdotL, float TdotH, float BdotH, float anisotropy)
{
  // Roughness along tangent and bitangent.
  // Christopher Kulla and Alejandro Conty. 2017. Revisiting Physically Based Shading at Imageworks
  float at = max(alphaRoughness * (1.0 + anisotropy), 0.00001);
  float ab = max(alphaRoughness * (1.0 - anisotropy), 0.00001);

  float3 F = F_Schlick(f0, f90, VdotH);
  float V = V_GGX_anisotropic(NdotL, NdotV, BdotV, TdotV, TdotL, BdotL, anisotropy, at, ab);
  float D = D_GGX_anisotropic(NdotH, TdotH, BdotH, anisotropy, at, ab);

  return F * V * D;
}

// f_sheen
float3 BRDF_specularSheen(float3 sheenColor, float sheenIntensity, float sheenRoughness, float NdotL, float NdotV, float NdotH)
{
  float sheenDistribution = D_Charlie(sheenRoughness, NdotH);
  float sheenVisibility = V_Ashikhmin(NdotL, NdotV);
  return sheenColor * sheenIntensity * sheenDistribution * sheenVisibility;
}
// https://github.com/KhronosGroup/glTF/blob/master/extensions/2.0/Khronos/KHR_lights_punctual/README.md#range-property
float getRangeAttenuation(float range, float distance)
{
  if (range <= 0.0)
  {
    // negative range means unlimited
    return 1.0;
  }
  return max(min(1.0 - pow(distance / range, 4.0), 1.0), 0.0) / pow(distance, 2.0);
}

// https://github.com/KhronosGroup/glTF/blob/master/extensions/2.0/Khronos/KHR_lights_punctual/README.md#inner-and-outer-cone-angles
float getSpotAttenuation(float3 pointToLight, float3 spotDirection, float outerConeCos, float innerConeCos)
{
  float actualCos = dot(normalize(spotDirection), normalize(-pointToLight));
  if (actualCos > outerConeCos)
  {
    if (actualCos < innerConeCos)
    {
      return smoothstep(outerConeCos, innerConeCos, actualCos);
    }
    return 1.0;
  }
  return 0.0;
}

float3 getPunctualRadianceSubsurface(float3 n, float3 v, float3 l, float scale, float distortion, float power, float3 color, float thickness)
{
  float3 distortedHalfway = l + n * distortion;
  float backIntensity = max(0.0, dot(v, -distortedHalfway));
  float reverseDiffuse = pow(clamp(0.0, 1.0, backIntensity), power) * scale;
  return (reverseDiffuse + color) * (1.0 - thickness);
}

float3 getPunctualRadianceTransmission(float3 n, float3 v, float3 l, float alphaRoughness, float ior, float3 f0)
{
  float3 r = refract(-v, n, 1.0 / ior);
  float3 h = normalize(l - r);
  float NdotL = clampedDot(-n, l);
  float NdotV = clampedDot(n, -r);

  float Vis = V_GGX(clampedDot(-n, l), NdotV, alphaRoughness);
  float D = D_GGX(clampedDot(r, l), alphaRoughness);

  return NdotL * f0 * Vis * D;
}

float3 getPunctualRadianceClearCoat(float3 clearcoatNormal, float3 v, float3 l, float3 h, float VdotH, float3 f0, float3 f90, float clearcoatRoughness)
{
  float NdotL = clampedDot(clearcoatNormal, l);
  float NdotV = clampedDot(clearcoatNormal, v);
  float NdotH = clampedDot(clearcoatNormal, h);
  return NdotL * BRDF_specularGGX(f0, f90, clearcoatRoughness * clearcoatRoughness, VdotH, NdotL, NdotV, NdotH);
}

float3 getPunctualRadianceSheen(float3 sheenColor, float sheenIntensity, float sheenRoughness, float NdotL, float NdotV, float NdotH)
{
  return NdotL * BRDF_specularSheen(sheenColor, sheenIntensity, sheenRoughness, NdotL, NdotV, NdotH);
}

#ifdef USE_IBL
float3 getIBLRadianceGGX(float3 n, float3 v, float perceptualRoughness, float3 specularColor)
{
  float NdotV = clampedDot(n, v);
  float lod = clamp(perceptualRoughness * float(u_MipCount), 0.0, float(u_MipCount));
  float3 reflection = normalize(reflect(-v, n));

  float2 brdfSamplePoint = clamp(float2(NdotV, perceptualRoughness), float2(0.0, 0.0), float2(1.0, 1.0));
  float2 brdf = u_GGXLUT.Sample(MeshTextureSampler, brdfSamplePoint).rg;
  float4 specularSample = u_GGXEnvSampler.SampleLevel(MeshTextureSampler, reflection, lod);

  float3 specularLight = specularSample.rgb;

#ifndef USE_HDR
  specularLight = sRGBToLinear(specularLight);
#endif

  return specularLight * (specularColor * brdf.x + brdf.y);
}

float3 getIBLRadianceTransmission(float3 n, float3 v, float perceptualRoughness, float ior, float3 baseColor)
{
  // Sample GGX LUT.
  float NdotV = clampedDot(n, v);
  float2 brdfSamplePoint = clamp(float2(NdotV, perceptualRoughness), float2(0.0, 0.0), float2(1.0, 1.0));
  float2 brdf = u_GGXLUT.Sample(MeshTextureSampler, brdfSamplePoint).rg;

  // Sample GGX environment map.
  float lod = clamp(perceptualRoughness * float(u_MipCount), 0.0, float(u_MipCount));

  // Approximate double refraction by assuming a solid sphere beneath the point.
  float3 r = refract(-v, n, 1.0 / ior);
  float3 m = 2.0 * dot(-n, r) * r + n;
  float3 rr = -refract(-r, m, ior);

  float4 specularSample = u_GGXEnvSampler.SampleLevel(MeshTextureSampler, rr, lod);
  float3 specularLight = specularSample.rgb;

#ifndef USE_HDR
  specularLight = sRGBToLinear(specularLight);
#endif

  return specularLight * (brdf.x + brdf.y);
}

float3 getIBLRadianceLambertian(float3 n, float3 diffuseColor)
{
  float3 diffuseLight = u_LambertianEnvSampler.Sample(MeshTextureSampler, n).rgb;

#ifndef USE_HDR
  diffuseLight = sRGBToLinear(diffuseLight);
#endif

  return diffuseLight * diffuseColor;
}

float3 getIBLRadianceCharlie(float3 n, float3 v, float sheenRoughness, float3 sheenColor, float sheenIntensity)
{
  float NdotV = clampedDot(n, v);
  float lod = clamp(sheenRoughness * float(u_MipCount), 0.0, float(u_MipCount));
  float3 reflection = normalize(reflect(-v, n));

  float2 brdfSamplePoint = clamp(float2(NdotV, sheenRoughness), float2(0.0, 0.0), float2(1.0, 1.0));
  float brdf = u_CharlieLUT.Sample(MeshTextureSampler, brdfSamplePoint).b;
  float4 sheenSample = u_CharlieEnvSampler.SampleLevel(MeshTextureSampler, reflection, lod);

  float3 sheenLight = sheenSample.rgb;

#ifndef USE_HDR
  sheenLight = sRGBToLinear(sheenLight);
#endif

  return sheenIntensity * sheenLight * sheenColor * brdf;
}

float3 getIBLRadianceSubsurface(float3 n, float3 v, float scale, float distortion, float power, float3 color, float thickness)
{
  float3 diffuseLight = u_LambertianEnvSampler.Sample(MeshTextureSampler, n).rgb;

#ifndef USE_HDR
  diffuseLight = sRGBToLinear(diffuseLight);
#endif

  return diffuseLight * getPunctualRadianceSubsurface(n, v, -v, scale, distortion, power, color, thickness);
}
#endif

// Get normal, tangent and bitangent vectors.
NormalInfo getNormalInfo(PS_INPUT input, float3 v)
{
  float2 UV = getNormalUV(input);
  float3 uv_dx = ddx(float3(UV, 0.0));
  float3 uv_dy = ddy(float3(UV, 0.0));

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

  // Due to anisoptry, the tangent can be further rotated around the geometric normal.
#ifdef MATERIAL_ANISOTROPY
  float3 direction = u_AnisotropyDirection;

#ifdef HAS_ANISOTROPY_DIRECTION_MAP
  direction = u_AnisotropyDirectionSampler.Sample(MeshTextureSampler, getAnisotropyDirectionUV(input)).xyz * 2.0 - 1.0;
#endif

  t = mul(float3x3(t, b, ng), normalize(direction));
  b = normalize(cross(ng, t));
#endif

  // Compute pertubed normals:
  if (u_NormalUVSet >= 0)
  {
    n = u_NormalSampler.Sample(normalSampler, UV).rgb * 2.0 - 1.0;
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

#ifdef MATERIAL_METALLICROUGHNESS
float4 getBaseColor(PS_INPUT input)
{
  float4 baseColor = float4(1, 1, 1, 1);

#if defined(MATERIAL_SPECULARGLOSSINESS)
  baseColor = u_DiffuseFactor;
#elif defined(MATERIAL_METALLICROUGHNESS)
  baseColor = u_BaseColorFactor;
#endif

#if defined(MATERIAL_SPECULARGLOSSINESS) && defined(HAS_DIFFUSE_MAP)
  baseColor *= sRGBToLinear(u_DiffuseSampler.Sample(MeshTextureSampler, getDiffuseUV(input)));
#elif defined(MATERIAL_METALLICROUGHNESS)
  if (u_BaseColorUVSet >= 0)
    baseColor *= sRGBToLinear(u_BaseColorSampler.Sample(baseColorSampler, getBaseColorUV(input)));
#endif

  return baseColor * getVertexColor(input);
}
#endif

#ifdef MATERIAL_SPECULARGLOSSINESS
MaterialInfo getSpecularGlossinessInfo(PS_INPUT input, MaterialInfo info)
{
  info.f0 = u_SpecularFactor;
  info.perceptualRoughness = u_GlossinessFactor;

#ifdef HAS_SPECULAR_GLOSSINESS_MAP
  float4 sgSample = sRGBToLinear(u_SpecularGlossinessSampler.Sample(MeshTextureSampler, getSpecularGlossinessUV(input)));
  info.perceptualRoughness *= sgSample.a; // glossiness to roughness
  info.f0 *= sgSample.rgb; // specular
#endif // ! HAS_SPECULAR_GLOSSINESS_MAP

  info.perceptualRoughness = 1.0 - info.perceptualRoughness; // 1 - glossiness
  info.albedoColor = info.baseColor.rgb * (1.0 - max(max(info.f0.r, info.f0.g), info.f0.b));

  return info;
}
#endif

#ifdef MATERIAL_METALLICROUGHNESS_SPECULAROVERRIDE
// KHR_extension_specular alters f0 on metallic materials based on the specular factor specified in the extention
float getMetallicRoughnessSpecularFactor(PS_INPUT input)
{
  //F0 = 0.08 * specularFactor * specularTexture
#ifdef HAS_METALLICROUGHNESS_SPECULAROVERRIDE_MAP
  float4 specSampler = u_MetallicRoughnessSpecularSampler.Sample(MeshTextureSampler, getMetallicRoughnessSpecularUV(input));
  return 0.08 * u_MetallicRoughnessSpecularFactor * specSampler.a;
#endif
  return  0.08 * u_MetallicRoughnessSpecularFactor;
}
#endif

#ifdef MATERIAL_METALLICROUGHNESS
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

#ifdef MATERIAL_METALLICROUGHNESS_SPECULAROVERRIDE
  // Overriding the f0 creates unrealistic materials if the IOR does not match up.
  float3 f0 = float3(getMetallicRoughnessSpecularFactor(input));
#else
  // Achromatic f0 based on IOR.
  float3 f0 = float3(f0_ior, f0_ior, f0_ior);
#endif

  info.albedoColor = lerp(info.baseColor.rgb * (float3(1.0, 1.0, 1.0) - f0), float3(0.0, 0.0, 0.0), info.metallic);
  info.f0 = lerp(f0, info.baseColor.rgb, info.metallic);

  return info;
}
#endif

#ifdef MATERIAL_SHEEN
MaterialInfo getSheenInfo(PS_INPUT input, MaterialInfo info)
{
  info.sheenColor = u_SheenColorFactor;
  info.sheenIntensity = u_SheenIntensityFactor;
  info.sheenRoughness = u_SheenRoughness;

#ifdef HAS_SHEEN_COLOR_INTENSITY_MAP
  float4 sheenSample = u_SheenColorIntensitySampler.Sample(MeshTextureSampler, getSheenUV(input));
  info.sheenColor *= sheenSample.xyz;
  info.sheenIntensity *= sheenSample.w;
#endif

  return info;
}
#endif

#ifdef MATERIAL_SUBSURFACE
MaterialInfo getSubsurfaceInfo(MaterialInfo info)
{
  info.subsurfaceScale = u_SubsurfaceScale;
  info.subsurfaceDistortion = u_SubsurfaceDistortion;
  info.subsurfacePower = u_SubsurfacePower;
  info.subsurfaceColor = u_SubsurfaceColorFactor;
  info.subsurfaceThickness = u_SubsurfaceThicknessFactor;

#ifdef HAS_SUBSURFACE_COLOR_MAP
  info.subsurfaceColor *= u_SubsurfaceColorSampler.Sample(MeshTextureSampler, getSubsurfaceColorUV(input)).rgb;
#endif

#ifdef HAS_SUBSURFACE_THICKNESS_MAP
  info.subsurfaceThickness *= u_SubsurfaceThicknessSampler.Sample(MeshTextureSampler, getSubsurfaceThicknessUV(input)).r;
#endif

  return info;
}
#endif

float3 getThinFilmF0(float3 f0, float3 f90, float NdotV, float thinFilmFactor, float thinFilmThickness)
{
  if (thinFilmFactor == 0.0)
  {
    // No thin film applied.
    return f0;
  }

  float3 lutSample = u_ThinFilmLUT.Sample(MeshTextureSampler, float2(thinFilmThickness, NdotV)).rgb - 0.5;
  float3 intensity = thinFilmFactor * 4.0 * f0 * (1.0 - f0);
  return clamp(intensity * lutSample, 0.0, 1.0);
}

#ifdef MATERIAL_THIN_FILM
MaterialInfo getThinFilmInfo(MaterialInfo info)
{
  info.thinFilmFactor = u_ThinFilmFactor;
  info.thinFilmThickness = u_ThinFilmThicknessMaximum / 1200.0;

#ifdef HAS_THIN_FILM_MAP
  info.thinFilmFactor *= u_ThinFilmSampler.Sample(MeshTextureSampler, getThinFilmUV(input)).r;
#endif

#ifdef HAS_THIN_FILM_THICKNESS_MAP
  float thicknessSampled = u_ThinFilmThicknessSampler.Sample(MeshTextureSampler, getThinFilmThicknessUV(input)).g;
  float thickness = lerp(u_ThinFilmThicknessMinimum / 1200.0, u_ThinFilmThicknessMaximum / 1200.0, thicknessSampled);
  info.thinFilmThickness = thickness;
#endif

  return info;
}
#endif

#ifdef MATERIAL_TRANSMISSION
MaterialInfo getTransmissionInfo(MaterialInfo info)
{
  info.transmission = u_Transmission;
  return info;
}
#endif

MaterialInfo getThicknessInfo(MaterialInfo info)
{
  info.thickness = 1.0;

#ifdef MATERIAL_THICKNESS
  info.thickness = u_Thickness;

#ifdef HAS_THICKNESS_MAP
  info.thickness *= u_ThicknessSampler.Sample(MeshTextureSampler, getThicknessUV(input)).r;
#endif

#endif

  return info;
}

MaterialInfo getAbsorptionInfo(MaterialInfo info)
{
  info.absorption = float3(0.0, 0.0, 0.0);

#ifdef MATERIAL_ABSORPTION
  info.absorption = u_AbsorptionColor;
#endif

  return info;
}

#ifdef MATERIAL_ANISOTROPY
MaterialInfo getAnisotropyInfo(MaterialInfo info)
{
  info.anisotropy = u_Anisotropy;

#ifdef HAS_ANISOTROPY_MAP
  info.anisotropy *= u_AnisotropySampler.Sample(MeshTextureSampler, getAnisotropyUV(input)).r * 2.0 - 1.0;
#endif

  return info;
}
#endif

#ifdef MATERIAL_CLEARCOAT
MaterialInfo getClearCoatInfo(MaterialInfo info, NormalInfo normalInfo)
{
  info.clearcoatFactor = u_ClearcoatFactor;
  info.clearcoatRoughness = u_ClearcoatRoughnessFactor;
  info.clearcoatF0 = float3(0.04, 0.04, 0.04);
  info.clearcoatF90 = float3(clamp(info.clearcoatF0 * 50.0, 0.0, 1.0));

#ifdef HAS_CLEARCOAT_TEXTURE_MAP
  float4 ccSample = u_ClearcoatSampler.Sample(MeshTextureSampler, getClearcoatUV(input));
  info.clearcoatFactor *= ccSample.r;
#endif

#ifdef HAS_CLEARCOAT_ROUGHNESS_MAP
  float4 ccSampleRough = u_ClearcoatRoughnessSampler.Sample(MeshTextureSampler, getClearcoatRoughnessUV(input));
  info.clearcoatRoughness *= ccSampleRough.g;
#endif

#ifdef HAS_CLEARCOAT_NORMAL_MAP
  float4 ccSampleNor = u_ClearcoatNormalSampler.Sample(MeshTextureSampler, getClearcoatNormalUV(input));
  info.clearcoatNormal = normalize(ccSampleNor.xyz);
#else
  info.clearcoatNormal = normalInfo.ng;
#endif

  info.clearcoatRoughness = clamp(info.clearcoatRoughness, 0.0, 1.0);

  return info;
}
#endif

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

#ifdef MATERIAL_IOR
  float ior = u_IOR_and_f0.x;
  float f0_ior = u_IOR_and_f0.y;
#else
  // The default index of refraction of 1.5 yields a dielectric normal incidence reflectance of 0.04.
  float ior = 1.5;
  float f0_ior = 0.04;
#endif

#ifdef MATERIAL_SPECULARGLOSSINESS
  materialInfo = getSpecularGlossinessInfo(input, materialInfo);
#endif

#ifdef MATERIAL_METALLICROUGHNESS
  materialInfo = getMetallicRoughnessInfo(input, materialInfo, f0_ior);
#endif

#ifdef MATERIAL_SHEEN
  materialInfo = getSheenInfo(materialInfo);
#endif

#ifdef MATERIAL_SUBSURFACE
  materialInfo = getSubsurfaceInfo(materialInfo);
#endif

#ifdef MATERIAL_THIN_FILM
  materialInfo = getThinFilmInfo(materialInfo);
#endif

#ifdef MATERIAL_CLEARCOAT
  materialInfo = getClearCoatInfo(materialInfo, normalInfo);
#endif

#ifdef MATERIAL_TRANSMISSION
  materialInfo = getTransmissionInfo(materialInfo);
#endif

#ifdef MATERIAL_ANISOTROPY
  materialInfo = getAnisotropyInfo(materialInfo);
#endif

  materialInfo = getThicknessInfo(materialInfo);
  materialInfo = getAbsorptionInfo(materialInfo);

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

#ifdef MATERIAL_THIN_FILM
  materialInfo.f0 = getThinFilmF0(materialInfo.f0, materialInfo.f90, clampedDot(n, v), materialInfo.thinFilmFactor, materialInfo.thinFilmThickness);
#endif

  // LIGHTING
  float3 f_specular = float3(0.0, 0.0, 0.0);
  float3 f_diffuse = float3(0.0, 0.0, 0.0);
  float3 f_emissive = float3(0.0, 0.0, 0.0);
  float3 f_clearcoat = float3(0.0, 0.0, 0.0);
  float3 f_sheen = float3(0.0, 0.0, 0.0);
  float3 f_subsurface = float3(0.0, 0.0, 0.0);
  float3 f_transmission = float3(0.0, 0.0, 0.0);

  // Calculate lighting contribution from image based lighting source (IBL)
#ifdef USE_IBL
  f_specular += getIBLRadianceGGX(n, v, materialInfo.perceptualRoughness, materialInfo.f0);
  f_diffuse += getIBLRadianceLambertian(n, materialInfo.albedoColor);

#ifdef MATERIAL_CLEARCOAT
  f_clearcoat += getIBLRadianceGGX(materialInfo.clearcoatNormal, v, materialInfo.clearcoatRoughness, materialInfo.clearcoatF0);
#endif

#ifdef MATERIAL_SHEEN
  f_sheen += getIBLRadianceCharlie(n, v, materialInfo.sheenRoughness, materialInfo.sheenColor, materialInfo.sheenIntensity);
#endif

#ifdef MATERIAL_SUBSURFACE
  f_subsurface += getIBLRadianceSubsurface(n, v, materialInfo.subsurfaceScale, materialInfo.subsurfaceDistortion, materialInfo.subsurfacePower, materialInfo.subsurfaceColor, materialInfo.subsurfaceThickness);
#endif

#ifdef MATERIAL_TRANSMISSION
  f_transmission += getIBLRadianceTransmission(n, v, materialInfo.perceptualRoughness, ior, materialInfo.baseColor);
#endif
#endif

#ifdef USE_PUNCTUAL
  for (int i = 0; i < LIGHT_COUNT; ++i)
  {
    Light light = u_Lights[i];

    float3 pointToLight = -light.direction;
    float rangeAttenuation = 1.0;
    float spotAttenuation = 1.0;

    if (light.type != LightType_Directional)
    {
      pointToLight = light.position - input.v_Position;
    }

    // Compute range and spot light attenuation.
    if (light.type != LightType_Directional)
    {
      rangeAttenuation = getRangeAttenuation(light.range, length(pointToLight));
    }
    if (light.type == LightType_Spot)
    {
      spotAttenuation = getSpotAttenuation(pointToLight, light.direction, light.outerConeCos, light.innerConeCos);
    }

    float3 intensity = rangeAttenuation * spotAttenuation * light.intensity * light.color;

    float3 l = normalize(pointToLight);   // Direction from surface point to light
    float3 h = normalize(l + v);          // Direction of the vector between l and v, called halfway vector
    float NdotL = clampedDot(n, l);
    float NdotV = clampedDot(n, v);
    float NdotH = clampedDot(n, h);
    float LdotH = clampedDot(l, h);
    float VdotH = clampedDot(v, h);

    if (NdotL > 0.0 || NdotV > 0.0)
    {
      // Calculation of analytical light
      //https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#acknowledgments AppendixB
      f_diffuse += intensity * NdotL * BRDF_lambertian(materialInfo.f0, materialInfo.f90, materialInfo.albedoColor, VdotH);

#ifdef MATERIAL_ANISOTROPY
      float3 h = normalize(l + v);
      float TdotL = dot(t, l);
      float BdotL = dot(b, l);
      float TdotH = dot(t, h);
      float BdotH = dot(b, h);
      f_specular += intensity * NdotL * BRDF_specularAnisotropicGGX(materialInfo.f0, materialInfo.f90, materialInfo.alphaRoughness,
        VdotH, NdotL, NdotV, NdotH,
        BdotV, TdotV, TdotL, BdotL, TdotH, BdotH, materialInfo.anisotropy);
#else
      f_specular += intensity * NdotL * BRDF_specularGGX(materialInfo.f0, materialInfo.f90, materialInfo.alphaRoughness, VdotH, NdotL, NdotV, NdotH);
#endif

#ifdef MATERIAL_SHEEN
      f_sheen += intensity * getPunctualRadianceSheen(materialInfo.sheenColor, materialInfo.sheenIntensity, materialInfo.sheenRoughness,
        NdotL, NdotV, NdotH);
#endif

#ifdef MATERIAL_CLEARCOAT
      f_clearcoat += intensity * getPunctualRadianceClearCoat(materialInfo.clearcoatNormal, v, l,
        h, VdotH,
        materialInfo.clearcoatF0, materialInfo.clearcoatF90, materialInfo.clearcoatRoughness);
#endif
    }

#ifdef MATERIAL_SUBSURFACE
    f_subsurface += intensity * getPunctualRadianceSubsurface(n, v, l,
      materialInfo.subsurfaceScale, materialInfo.subsurfaceDistortion, materialInfo.subsurfacePower,
      materialInfo.subsurfaceColor, materialInfo.subsurfaceThickness);
#endif

#ifdef MATERIAL_TRANSMISSION
    f_transmission += intensity * getPunctualRadianceTransmission(n, v, l, materialInfo.alphaRoughness, ior, materialInfo.f0);
#endif
  }
#endif // !USE_PUNCTUAL

  f_emissive = u_EmissiveFactor;

  if (u_EmissiveUVSet >= 0)
    f_emissive *= sRGBToLinear(u_EmissiveSampler.Sample(emissiveSampler, getEmissiveUV(input))).rgb;

  float3 color = float3(0.0, 0.0, 0.0);

  ///
  /// Layer blending
  ///

  float clearcoatFactor = 0.0;
  float3 clearcoatFresnel = float3(0.0, 0.0, 0.0);

#ifdef MATERIAL_CLEARCOAT
  clearcoatFactor = materialInfo.clearcoatFactor;
  clearcoatFresnel = F_Schlick(materialInfo.clearcoatF0, materialInfo.clearcoatF90, clampedDot(materialInfo.clearcoatNormal, v));
#endif

#ifdef MATERIAL_ABSORPTION
  f_transmission *= transmissionAbsorption(v, n, ior, materialInfo.thickness, materialInfo.absorption);
#endif

#ifdef MATERIAL_TRANSMISSION
  float3 diffuse = lerp(f_diffuse, f_transmission, materialInfo.transmission);
#else
  float3 diffuse = f_diffuse;
#endif

  color = (f_emissive + diffuse + f_specular + f_subsurface + (1.0 - reflectance) * f_sheen) * (1.0 - clearcoatFactor * clearcoatFresnel) + f_clearcoat * clearcoatFactor;

  float ao = 1.0;
  // Apply optional PBR terms for additional (optional) shading
  if (u_OcclusionUVSet >= 0)
  {
    ao = u_OcclusionSampler.Sample(occlusionSampler, getOcclusionUV(input)).r;
    color = lerp(color, color * ao, u_OcclusionStrength);
  }

#ifndef DEBUG_OUTPUT // no debug

  if (u_alphaMode == 1) //MASK
  {
    // Late discard to avaoid samplig artifacts. See https://github.com/KhronosGroup/glTF-Sample-Viewer/issues/267
    if (baseColor.a < u_AlphaCutoff)
      discard;
    baseColor.a = 1.0;
  }

  // regular shading
  g_finalColor = float4(toneMap(color), baseColor.a);

#else // debug output

#ifdef DEBUG_METALLICROUGHNESS
  g_finalColor.rgb = float3(materialInfo.metallic, materialInfo.perceptualRoughness, 0.0);
#endif

#ifdef DEBUG_NORMAL
  g_finalColor.rgb = n * 0.5 + 0.5;
#endif

#ifdef DEBUG_TANGENT
  g_finalColor.rgb = t * 0.5 + float3(0.5, 0.5, 0.5);
#endif

#ifdef DEBUG_BITANGENT
  g_finalColor.rgb = b * 0.5 + float3(0.5, 0.5, 0.5);
#endif

#ifdef DEBUG_BASECOLOR
  g_finalColor.rgb = linearTosRGB(materialInfo.baseColor);
#endif

#ifdef DEBUG_OCCLUSION
  g_finalColor.rgb = float3(ao);
#endif

#ifdef DEBUG_F0
  g_finalColor.rgb = materialInfo.f0;
#endif

#ifdef DEBUG_FEMISSIVE
  g_finalColor.rgb = f_emissive;
#endif

#ifdef DEBUG_FSPECULAR
  g_finalColor.rgb = f_specular;
#endif

#ifdef DEBUG_FDIFFUSE
  g_finalColor.rgb = f_diffuse;
#endif

#ifdef DEBUG_THICKNESS
  g_finalColor.rgb = float3(materialInfo.thickness);
#endif

#ifdef DEBUG_FCLEARCOAT
  g_finalColor.rgb = f_clearcoat;
#endif

#ifdef DEBUG_FSHEEN
  g_finalColor.rgb = f_sheen;
#endif

#ifdef DEBUG_ALPHA
  g_finalColor.rgb = float3(baseColor.a);
#endif

#ifdef DEBUG_FSUBSURFACE
  g_finalColor.rgb = f_subsurface;
#endif

#ifdef DEBUG_FTRANSMISSION
  g_finalColor.rgb = linearTosRGB(f_transmission);
#endif

  g_finalColor.a = 1.0;

#endif // !DEBUG_OUTPUT

  output.Color0 = g_finalColor;
  return output;
}
