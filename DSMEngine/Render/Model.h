#pragma once
#ifndef __MODEL_H__
#define __MODEL_H__

#include <string>
#include <vector>
#include <memory>
#include "Mesh.h"

namespace DSM {
    class MeshRenderer;

    // 模型的数据
    struct Model
    {
        std::string name{};
        std::vector<std::shared_ptr<Mesh>> meshes{};
        std::vector<std::shared_ptr<Material>> materials{};
        BufferHandle materialData{};
    };

}

#endif