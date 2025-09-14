#pragma once
#ifndef __MODELLOADER_H__
#define __MODELLOADER_H__

#include "Model.h"

namespace DSM{
    struct IDevice;
    namespace Geometry { struct GeometryMesh; }

    namespace ModelLoader{
        enum VertexAttributeSlot
        {
            Position = 0,
            TexCoord,
            Normal,
            Tangent,
            Count
        };

        void Init(IDevice* device);
        void Destroy();

        std::shared_ptr<Model> LoadModel(const std::string& filename);
        std::shared_ptr<Model> LoadModelFromGeometry(
            const std::string& name, 
            const Geometry::GeometryMesh& geometryMesh,
            std::shared_ptr<Material> material = nullptr);
    }
}


#endif