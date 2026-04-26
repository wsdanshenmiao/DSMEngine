#ifndef __COMMON_HLSLI__
#define __COMMON_HLSLI__


// 常用的采样器
SamplerState gPointWrapSampler : register(s0);
SamplerState gLinearWrapSampler : register(s1);
SamplerState gAnisoWrapSampler : register(s2);
SamplerState gPointClampSampler : register(s3);
SamplerState gLinearClampSampler : register(s4);
SamplerState gAnisoClampSampler : register(s5);
SamplerState gPointBorderSampler : register(s6);
SamplerState gLinearBorderSampler : register(s7);
// 比较采样器，用于阴影贴图采样
SamplerComparisonState gShadowSampler : register(s8);



enum CornerID
{
    NearTopLeft = 0,
    NearTopRight = 1,
    NearBottomRight = 2,
    NearBottomLeft = 3,
    FarTopLeft = 4,
    FarTopRight = 5,
    FarBottomRight = 6,
    FarBottomLeft = 7,
    CornerCount = 8
};

enum PlaneID
{
    NearPlane = 0,
    FarPlane = 1,
    RightPlane = 2,
    LeftPlane = 3,
    TopPlane = 4,
    BottomPlane = 5,
    PlaneCount = 6
};

struct BoundingBox
{
    float4 minPoint;
    float4 maxPoint;

    float3 GetMin() { return minPoint.xyz; }
    float3 GetMax() { return maxPoint.xyz; }
    float3 GetCenter() { return (GetMin() + GetMax()) * 0.5f; }
    float3 GetExtents() { return (GetMax() - GetMin()) * 0.5f; }
    float3 GetSize() { return GetMax() - GetMin(); }

    bool IsValid()
    {
        return all(GetMax() >= GetMin());
    }

    bool Contains(float3 p)
    {
        return all(p >= GetMin()) && all(p <= GetMax());
    }

    bool Contains(BoundingBox other)
    {
        float3 _min = GetMin();
        float3 _max = GetMax();
        return all(max(other.GetMax(), _max) == _max) && all(min(other.GetMin(), _min) == _min);
    }

    bool Intersects(BoundingBox other)
    {
        const float3 overlapMin = max(GetMin(), other.GetMin());
        const float3 overlapMax = min(GetMax(), other.GetMax());
        return all(overlapMax >= overlapMin);
    }

    void Encapsulate(float3 p)
    {
        minPoint = float4(min(GetMin(), p), 0);
        maxPoint = float4(max(GetMax(), p), 0);
    }

    void Union(BoundingBox other)
    {
        minPoint = float4(min(GetMin(), other.GetMin()), 0);
        maxPoint = float4(max(GetMax(), other.GetMax()), 0);
    }
};

float3 IntersectThreePlanes(float4 p0, float4 p1, float4 p2)
{
    const float3 n0 = p0.xyz;
    const float3 n1 = p1.xyz;
    const float3 n2 = p2.xyz;

    const float3 n1xn2 = cross(n1, n2);
    const float3 n2xn0 = cross(n2, n0);
    const float3 n0xn1 = cross(n0, n1);
    const float det = dot(n0, n1xn2);

    // Plane equation is dot(n, p) + d = 0.
    return (-p0.w * n1xn2 - p1.w * n2xn0 - p2.w * n0xn1) / det;
}

struct Frustum
{
    float4 planes[PlaneID::PlaneCount];

    float4 GetPlane(PlaneID id)
    {
        return planes[id];
    }

    void GetPlanes(out float4 planesOut[PlaneID::PlaneCount])
    {
        planesOut = planes;
    }

    bool Intersects(BoundingBox box)
    {
        const float3 center = box.GetCenter();
        const float3 extents = box.GetExtents();

        for(uint i = 0; i < PlaneID::PlaneCount; ++i) {
            const float4 plane = planes[i];
            const float3 normal = plane.xyz;

            const float centerDistance = dot(normal, center) + plane.w;
            const float projectedRadius = dot(abs(normal), extents);

            if(centerDistance + projectedRadius < 0.0f)
                return false;
        }
        return true;
    }

    void GetCorners(out float3 corners[CornerID::CornerCount])
    {
        const float4 nearP = planes[PlaneID::NearPlane];
        const float4 farP = planes[PlaneID::FarPlane];
        const float4 rightP = planes[PlaneID::RightPlane];
        const float4 leftP = planes[PlaneID::LeftPlane];
        const float4 topP = planes[PlaneID::TopPlane];
        const float4 bottomP = planes[PlaneID::BottomPlane];

        corners[CornerID::NearTopLeft] = IntersectThreePlanes(nearP, leftP, topP);
        corners[CornerID::NearTopRight] = IntersectThreePlanes(nearP, rightP, topP);
        corners[CornerID::NearBottomRight] = IntersectThreePlanes(nearP, rightP, bottomP);
        corners[CornerID::NearBottomLeft] = IntersectThreePlanes(nearP, leftP, bottomP);

        corners[CornerID::FarTopLeft] = IntersectThreePlanes(farP, leftP, topP);
        corners[CornerID::FarTopRight] = IntersectThreePlanes(farP, rightP, topP);
        corners[CornerID::FarBottomRight] = IntersectThreePlanes(farP, rightP, bottomP);
        corners[CornerID::FarBottomLeft] = IntersectThreePlanes(farP, leftP, bottomP);
    }
};

Frustum GetFrustumFromMatrix(float4x4 matrix)
{
    Frustum frustum;
    // Left plane
    frustum.planes[PlaneID::LeftPlane] = matrix[3] + matrix[0];
    // Right plane
    frustum.planes[PlaneID::RightPlane] = matrix[3] - matrix[0];
    // Top plane
    frustum.planes[PlaneID::TopPlane] = matrix[3] - matrix[1];
    // Bottom plane
    frustum.planes[PlaneID::BottomPlane] = matrix[3] + matrix[1];
    // Near plane
    frustum.planes[PlaneID::NearPlane] = matrix[2];
    // Far plane
    frustum.planes[PlaneID::FarPlane] = matrix[3] - matrix[2];

    // Normalize planes
    [unroll]
    for (uint i = 0; i < PlaneID::PlaneCount; ++i)
    {
        float len = length(frustum.GetPlane((PlaneID)i).xyz);
        frustum.planes[i] *= rcp(len);
    }

    return frustum;
}

Frustum GetFrustumFromGroup(uint2 groupId, float minZ, float maxZ, float2 tileScale, float4x4 proj)
{
    float2 tileBias = tileScale - 1 - 2 * float2(groupId);

    // 计算当前分块视锥体的投影矩阵
    proj[0] = float4(proj[0][0] * tileScale.x, 0.0f, tileBias.x, 0.0f);
    proj[1] = float4(0.0f, proj[1][1] * tileScale.y, -tileBias.y, 0.0f);
    proj[3] = float4(0.0f, 0.0f, 1.0f, 0.0f);

    Frustum frustum = GetFrustumFromMatrix(proj);

    frustum.planes[PlaneID::NearPlane] = float4(0, 0, 1, -minZ);
    frustum.planes[PlaneID::FarPlane] = float4(0, 0, -1, maxZ);

    return frustum;
}






/*
 *  2
 *  |\
 *  | \
 *  |  \
 *  |___ \
 *  0       1
 */
// 覆盖全屏的三角形
void GetFullscreenTriangle(uint vertexID, out float4 posCS, out float2 uv, bool reversedZ = false)
{
    uv = float2(vertexID & 2, (vertexID << 1) & 2);
    posCS = float4(uv * float2(2, -2) + float2(-1, 1), reversedZ ? 0 : 1, 1);

}


float2 EncodeFloat3ToFloat2(float3 normal)
{
    normal /= abs(normal.x) + abs(normal.y) + abs(normal.z);
    normal.xy = (normal.z >= 0) ? normal.xy : (1.0 - abs(normal.yx)) * sign(normal.xy);
    return normal.xy * 0.5f + 0.5f;
}

float3 DecodeFloat2ToFloat3(float2 val)
{
    val = val * 2.0 - 1.0;
    float3 n = float3(val.xy, 1.0 - abs(val.x) - abs(val.y));
    float t = max(-n.z, 0.0);
    n.xy = (n.z >= 0) ? n.xy : (1.0 - abs(n.yx)) * sign(n.xy);;
    return normalize(n);
}


uint EncodeFloat4ToUint(float4 color)
{
    uint4 packed = uint4(round(saturate(color) * 255.0f));
    return (packed.x) | (packed.y << 8) | (packed.z << 16) | (packed.w << 24);
}

float4 DecodeUintToFloat4(uint packed)
{
    float4 color;
    color.x = (packed & 0x000000FF) / 255.0f;
    color.y = ((packed & 0x0000FF00) >> 8) / 255.0f;
    color.z = ((packed & 0x00FF0000) >> 16) / 255.0f;
    color.w = ((packed & 0xFF000000) >> 24) / 255.0f;
    return color;
}


float DistanceSquared(float3 p1, float3 p2)
{
    return dot(p1 - p2, p1 - p2);
}

float GetLinearDepth(float depth, float4x4 projMatrix)
{
    return projMatrix[3][2] / (depth - projMatrix[2][2]);
}


float4 GetWorldPosition(float2 uv, float4x4 invViewProj, float depth)
{
    float4 posCS = float4(uv * 2 - 1, depth, 1);
    posCS.y *= -1;
    float4 posWS = mul(posCS, invViewProj);
    posWS /= posWS.w;
    
    return posWS;
}

float4 GetViewPosition(float2 uv, float4x4 invProj, float depth)
{
    float4 posCS = float4(uv * 2 - 1, depth, 1);
    posCS.y *= -1;
    float4 posVS = mul(posCS, invProj);
    posVS /= posVS.w;

    return posVS;
}


float Luminance(float3 c)
{
    return dot(c, float3(0.2126f, 0.7152f, 0.0722f));
}

float3 LinearToSRGB(float3 linearCol)
{
    static const float a = 1.0f / 2.4f;
    float3 srgb = 12.92f * linearCol;
    srgb = select(linearCol > 0.0031308f, 1.055f * pow(linearCol, a) - 0.055f, srgb);
    return saturate(srgb);
}

float3 SRGBToLinear(float3 srgb)
{
    static const float a = 1 / 1.055f;
    float3 linearCol = srgb / 12.92f;
    linearCol = select(srgb > 0.04045f, pow((srgb + 0.055f) * a, 2.4f), linearCol);
    return linearCol;
}

float3 RGBToYCoCg(float3 rgb)
{
    // YCoCg transform:
    // Y  = 0.25R + 0.50G + 0.25B
    // Co = 0.50R - 0.50B
    // Cg = -0.25R + 0.50G - 0.25B
    float y = dot(rgb, float3(0.25f, 0.5f, 0.25f));
    float co = dot(rgb, float3(0.5f, 0.0f, -0.5f));
    float cg = dot(rgb, float3(-0.25f, 0.5f, -0.25f));
    return float3(y, co, cg);
}

float3 YCoCgToRGB(float3 ycocg)
{
    float y = ycocg.x;
    float co = ycocg.y;
    float cg = ycocg.z;
    float r = y + co - cg;
    float g = y + cg;
    float b = y - co - cg;
    return saturate(float3(r, g, b));
}

// Alias helpers for YCgCo naming.
float3 RGBToYCgCo(float3 rgb)
{
    return RGBToYCoCg(rgb);
}

float3 YCgCoToRGB(float3 ycgco)
{
    return YCoCgToRGB(ycgco);
}

uint GetCubeMapFaceIndex(float3 dir)
{
    float3 absDir = abs(dir);
    uint axis = 0;
    if(absDir.y > absDir.x && absDir.y > absDir.z){
        axis = 1;
    }
    else if(absDir.z > absDir.x && absDir.z > absDir.y){
        axis = 2;
    }
    uint faceIndex = axis * 2;
    return dir[axis] < 0 ? faceIndex + 1 : faceIndex;
}


#endif // __COMMON_HLSLI__