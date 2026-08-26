#include "sapana/assets/GltfMeshLoader.hpp"

#include "BasicMath.hpp"
#include "GLTFLoader.hpp"

#include <iostream>
#include <memory>

namespace sapana
{
namespace assets
{

namespace
{

using Diligent::GLTF::DefaultVertexColor;
using Diligent::GLTF::Model;
using Diligent::GLTF::ModelCreateInfo;
using Diligent::GLTF::PositionAttributeName;
using Diligent::GLTF::VertexAttributeDesc;
using Diligent::GLTF::VertexColorAttributeName;
using Diligent::VT_FLOAT32;
using Diligent::VT_UINT32;

/// Vertex layout matching BasicForwardRenderer (ATTRIB0 pos, ATTRIB1 color).
const VertexAttributeDesc kBasicVertexAttrs[] = {
    VertexAttributeDesc{PositionAttributeName, 0, VT_FLOAT32, 3},
    VertexAttributeDesc{VertexColorAttributeName, 0, VT_FLOAT32, 4, &DefaultVertexColor},
};

Diligent::Uint32 CountIndices(const Model& model)
{
    Diligent::Uint32 total = 0;
    for (const auto& mesh : model.Meshes)
    {
        for (const auto& prim : mesh.Primitives)
            total += prim.IndexCount;
    }
    return total;
}

} // namespace

MeshAssetPtr GltfMeshLoader::Load(Diligent::IRenderDevice*  device,
                                  Diligent::IDeviceContext* context,
                                  const std::string&        path)
{
    if (device == nullptr || context == nullptr || path.empty())
    {
        std::cerr << "Sapana GltfMeshLoader: invalid arguments for [" << path << "]\n";
        return nullptr;
    }

    auto mesh            = std::make_shared<MeshAsset>();
    mesh->SourceGltfPath = path;

    // Basic path: Sapana-owned GPU mesh with pos+color layout.
    {
        ModelCreateInfo basicCI;
        basicCI.FileName            = path.c_str();
        basicCI.VertexAttributes    = kBasicVertexAttrs;
        basicCI.NumVertexAttributes = static_cast<Diligent::Uint32>(sizeof(kBasicVertexAttrs) / sizeof(kBasicVertexAttrs[0]));
        basicCI.IndexType           = VT_UINT32;

        try
        {
            Model basicModel(device, context, basicCI);
            if (!basicModel.PrepareGPUResources(device, context))
            {
                std::cerr << "Sapana GltfMeshLoader: Basic GPU resources not ready for [" << path << "]\n";
                return nullptr;
            }

            Diligent::IBuffer* vb = basicModel.GetVertexBuffer(0);
            Diligent::IBuffer* ib = basicModel.GetIndexBuffer();
            if (vb == nullptr || ib == nullptr)
            {
                std::cerr << "Sapana GltfMeshLoader: missing buffers for [" << path << "]\n";
                return nullptr;
            }

            mesh->VertexBuffer = vb;
            mesh->IndexBuffer  = ib;
            mesh->IndexCount   = CountIndices(basicModel);
            mesh->IndexType    = VT_UINT32;

            if (mesh->IndexCount == 0)
            {
                std::cerr << "Sapana GltfMeshLoader: no indices in [" << path << "]\n";
                return nullptr;
            }
        }
        catch (const std::exception& ex)
        {
            std::cerr << "Sapana GltfMeshLoader: Basic load failed for [" << path << "]: " << ex.what() << '\n';
            return nullptr;
        }
        catch (...)
        {
            std::cerr << "Sapana GltfMeshLoader: Basic load failed for [" << path << "]\n";
            return nullptr;
        }
    }

    // PBR path: full default-attribute GLTF::Model (kept alive on MeshAsset).
    {
        ModelCreateInfo pbrCI;
        pbrCI.FileName = path.c_str();

        try
        {
            auto model = std::make_shared<Model>(device, context, pbrCI);
            if (!model->PrepareGPUResources(device, context))
            {
                std::cerr << "Sapana GltfMeshLoader: PBR GPU resources not ready for [" << path << "]\n";
                return mesh;
            }
            mesh->GltfModel = std::move(model);
        }
        catch (const std::exception& ex)
        {
            std::cerr << "Sapana GltfMeshLoader: PBR Model load failed for [" << path << "]: " << ex.what() << '\n';
        }
        catch (...)
        {
            std::cerr << "Sapana GltfMeshLoader: PBR Model load failed for [" << path << "]\n";
        }
    }

    return mesh;
}

} // namespace assets
} // namespace sapana
