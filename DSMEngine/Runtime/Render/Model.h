#pragma once
#ifndef __MODEL_H__
#define __MODEL_H__

#include <string>
#include <vector>
#include <memory>
#include "Mesh.h"
#include "Runtime/Math/Transform.h"
#include "Shaders/ForwardShader/ResourceData.h"


namespace DSM {
    class MeshRenderer;

    // 模型的数据
    struct Model
    {
        std::string name{};
        std::string filePath{};
        std::vector<std::shared_ptr<Mesh>> meshes{};
        std::vector<std::shared_ptr<ShaderResource::MaterialData>> materials{};
        BufferHandle materialData{};
        Math::AxisAlignedBox boundingBox{};
    };

}

#endif