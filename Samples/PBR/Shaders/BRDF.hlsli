#ifndef __BRDF_HLSLI__
#define __BRDF_HLSLI__

// 完整的表面响应分为漫反射项和镜面反射项
// f = fd + fs

// Cook-Torrance 模型的镜面反射项
// fs(v, l) = D(h, r) * G(v, l, r) * F(v, h, f0) / (4 * NoV * NoL)
// 其中 D 为法线分布函数，G 为几何阴影函数，F 为菲涅尔项，
// h 为半程向量，r 为表面粗糙度， f0 为法线入射时的菲涅尔反射

// 漫反射项
// fd(v, l) = (c / pi) * F_Schlick(n, l, f0, f90) * F_Schlick(n, v, f0, f90)
// 其中 c 为漫反射响应，f90 为平行入射时的菲涅尔反射


static const float s_PI = 3.14159265;


// 使用 GGX 模型的法线分布函数
// Dggx(h, r) =  r^2 / (pi * pow(pow(n * h, 2) * (r^2 - 1) + 1), 2)
float D_GGX(float NoH, float roughness)
{
    float a = roughness * roughness;
    float lower = lerp(1, a, NoH * NoH);    // 1 - (n * h)^2 + (n * h)^2 * r^2
    return a / max(1e-6, s_PI * lower * lower);  // 避免除零
}

// Smith 将几何阴影函数分解为两个函数的乘积
// G(v, l, r) = G1(l, r) * G1(v, r)
// 使用 GGX 模型的 G1 函数
// G1_GGX(v, r) = 2 * (n * v) / (n * v + sqrt(r^2 + (1 - r^2) * (n * v)^2))
// 结合上面镜面反射的分母，可以化简为函数 V(v, r)
// V_GGX(v, r) = 1 / ((n * v) + sqrt(r^2 + (1 - r^2) * (n * v)^2))
float V_SmithGGXCorrelated(float NoV, float NoL, float roughness)
{
    float a = roughness * roughness;
    float gv = NoL + sqrt(a + (1 - a) * NoV * NoV);
    float gl = NoV + sqrt(a + (1 - a) * NoL * NoL);
    return 1 / max(1e-6, gv * gl); // 避免除零
}


// 电介质的法线入射的亮度
static float3 s_DielectricSpecular = float3(0.04, 0.04, 0.04);

// 使用 Schlick 近似的菲涅尔项
// F_Schlick(l, h, f0) = f0 + (1 - f0) * pow(1 - (l * h), 5)
float3 F_Schlick(float3 f0, float f90, float cos)
{
    return f0 + (f90 - f0) * pow(1 - cos, 5);
}

float F_Schlick(float f0, float f90, float cos)
{
    return f0 + (f90 - f0) * pow(1 - cos, 5);
}

// 镜面反射项
float3 SpecularBRDF(float NoV, float NoL, float NoH, float VoH, float3 f0, float roughness)
{
    float ND = D_GGX(NoH, roughness);
    float GV = V_SmithGGXCorrelated(NoV, NoL, roughness);
    float3 F = F_Schlick(f0, 1.0, VoH);
    return ND * GV * F;
}




// 迪士尼漫反射项
float DiffuseBurley(float NoV, float NoL, float LoH, float roughness)
{
    float f90 = 0.5 + 2.0 * roughness * LoH * LoH;
    float lightScatter = F_Schlick(1.0, f90, NoL);
    float viewScatter = F_Schlick(1.0, f90, NoV);
    return lightScatter * viewScatter / s_PI;
}

#endif