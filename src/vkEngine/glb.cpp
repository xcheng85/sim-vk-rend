#include <sstream>
#include <GLTFSDK/GLTF.h>
#include <GLTFSDK/GLTFResourceReader.h>
#include <GLTFSDK/GLBResourceReader.h>
#include <GLTFSDK/Deserialize.h>

#include <glb.h>

#include <vector.h>
#include <matrix.h>
#include <quaternion.h>
#include <misc.h>


std::shared_ptr<Scene> GltfBinaryIOReader::read(const std::string &filePath) {
    std::shared_ptr<Scene> res = std::make_shared<Scene>();
    // to do
    return res;
}

class InMemoryStreamReader : public Microsoft::glTF::IStreamReader {
public:
    InMemoryStreamReader(std::shared_ptr<std::stringstream> stream) : _stream(stream) {}

    std::shared_ptr<std::istream> GetInputStream(const std::string &) const override {
        return _stream;
    }

private:
    std::shared_ptr<std::stringstream> _stream;
};

void PrintDocumentInfo(const Microsoft::glTF::Document &document) {
    // LOGI("Asset Version: %s", document.asset.version.c_str());
    // LOGI("Asset MinVersion: %s", document.asset.minVersion.c_str());
    // LOGI("Asset Generator: %s", document.asset.generator.c_str());
    // LOGI("Asset Copyright: %s", document.asset.copyright.c_str());

    // LOGI("Scene Count: %d", document.scenes.Size());

    if (document.scenes.Size() > 0U) {
        //LOGI("Default Scene Index: %s", document.GetDefaultScene().id.c_str());
    } else {
        //LOGE("Document has no scene");
    }

    // LOGI("Node Count: %d", document.nodes.Size());
    // LOGI("Camera Count: %d", document.cameras.Size());
    // LOGI("Material Count: %d", document.materials.Size());
    // LOGI("Mesh Count: %d", document.meshes.Size());
    // LOGI("Skin Count: %d", document.skins.Size());
    // LOGI("Image Count: %d", document.images.Size());
    // LOGI("Texture Count: %d", document.textures.Size());
    // LOGI("Sampler Count: %d", document.samplers.Size());
    // LOGI("Buffer Count: %d", document.buffers.Size());
    // LOGI("BufferView Count: %d", document.bufferViews.Size());
    // LOGI("Accessor Count: %d", document.accessors.Size());
    // LOGI("Animation Count: %d", document.animations.Size());

    // for (const auto &extension: document.extensionsUsed) {
    //     LOGI("Extension Used: %s", extension.c_str());
    // }
    // for (const auto &extension: document.extensionsRequired) {
    //     LOGI("Extension Required: %s", extension.c_str());
    // }
}

void PrintResourceInfo(const Microsoft::glTF::Document &document,
                       const Microsoft::glTF::GLTFResourceReader &resourceReader) {
    // Use the resource reader to get each mesh primitive's position data
    for (const auto &mesh: document.meshes.Elements()) {
        //LOGI("Mesh: %s", mesh.id.c_str());

        for (const auto &meshPrimitive: mesh.primitives) {
            std::string accessorId;
            // accessor: An object describing the number and the format of data elements stored in a binary buffer.
            // find the accessor id tied with position data
            if (meshPrimitive.TryGetAttributeAccessorId(Microsoft::glTF::ACCESSOR_POSITION,
                                                        accessorId)) {
                // find the accessor with id
                const Microsoft::glTF::Accessor &accessor = document.accessors.Get(accessorId);
                ASSERT(accessor.componentType == Microsoft::glTF::COMPONENT_FLOAT,
                       "Position data accessor should be float");
                // Sparse accessors
                // https://github.com/KhronosGroup/glTF-Tutorials/blob/main/gltfTutorial/gltfTutorial_005_BuffersBufferViewsAccessors.md
                //LOGI("Sparse Accessor Count: %d", accessor.sparse.count);
//                {
//                    "bufferView" : 1,
//                            "byteOffset" : 0,
//                            "componentType" : 5126,
//                            "count" : 3,
//                            "type" : "VEC3",
//                            "max" : [ 1.0, 1.0, 0.0 ],
//                    "min" : [ 0.0, 0.0, 0.0 ]
//                }
                // how buffer is read from accessor.
//                const BufferView& bufferView = gltfDocument.bufferViews.Get(accessor.bufferViewId);
//                const Buffer& buffer = gltfDocument.buffers.Get(bufferView.bufferId);
                const auto data = resourceReader.ReadBinaryData<float>(document, accessor);
                const auto dataByteLength = data.size() * sizeof(float);
               // LOGI("Mesh has: %d bytes of position data", dataByteLength);
            }
        }
    }

    // Use the resource reader to get each image's data
    for (const auto &image: document.images.Elements()) {
        std::string filename;
        if (image.uri.empty()) {
            ASSERT(!image.bufferViewId.empty(), "image's bufferViewId should be defined");
            auto &bufferView = document.bufferViews.Get(image.bufferViewId);
            auto &buffer = document.buffers.Get(bufferView.bufferId);
            filename += buffer.uri;
        } else if (Microsoft::glTF::IsUriBase64(image.uri)) {
            filename = "Data URI";
        } else {
            filename = image.uri;
        }
        auto data = resourceReader.ReadBinaryData(document, image);
        // LOGI("Image: %d", image.id.c_str());
        // LOGI("Image: %d bytes of image data", data.size());
        // LOGI("Image filename: %s", filename.c_str());
    }
}

void readMeshes(const Microsoft::glTF::Document &document,
                const Microsoft::glTF::GLTFResourceReader &resourceReader,
                Scene &outputScene) {
    // node: // https://github.com/KhronosGroup/glTF/blob/master/specification/2.0/schema/node.schema.json
    // every mesh's index and instance offset
    // while read every mesh, update firstIndex and vertexOffset, bundle into larger buffer
    uint32_t firstIndex = 0;
    uint32_t vertexOffset = 0;
    for (size_t i = 0; i < document.nodes.Size(); ++i) {
        // nodes of scene graph could not have mesh
        if (document.nodes[i].meshId.empty()) {
            continue;
        }
        // string to uint
        uint32_t meshId = std::stoul(document.nodes[i].meshId);
        const Microsoft::glTF::Mesh &mesh = document.meshes[meshId];
        // goal to fill in this internal mesh entity
        Mesh currMesh;
        // step1: node's local transform
        mat4x4f m(1.0f);

        // nodes's local transformation matrix
        // HasIdentityTRS
        //           return translation == Vector3::ZERO
        //                    && rotation == Quaternion::IDENTITY
        //                    && scale == Vector3::ONE;

        if (document.nodes[i].matrix != Microsoft::glTF::Matrix4::IDENTITY) {
            // row-major
            for (int i = 0; i < 4; i++) {
                for (int j = 0; j < 4; j++) {
                    m.data[i][j] = document.nodes[i].matrix.values[i * 4 + j];
                }
            }
        } else if (!document.nodes[i].HasIdentityTRS()) {
            auto matScale = MatrixScale4x4(document.nodes[i].scale.x, document.nodes[i].scale.y,
                                           document.nodes[i].scale.z);
            quatf q(document.nodes[i].rotation.w, document.nodes[i].rotation.x,
                    document.nodes[i].rotation.y, document.nodes[i].rotation.z);

            auto matRot = RotationMatrixFromQuaternion(q);
            auto matTranslate = MatrixTranslation4x4(document.nodes[i].translation.x,
                                                     document.nodes[i].translation.y,
                                                     document.nodes[i].translation.z);

            m = MatrixMultiply4x4(matTranslate, MatrixMultiply4x4(matRot, matScale));
        }
        // 2.
        for (auto &primitive: mesh.primitives) {
            // use Accessor to access all the data buffers
            std::string positionAccessorID;
            std::string normalAccessorID;
            std::string tangentAccessorID;
            // multiple pairs of uv coordinates
            std::string uvAccessorID;
            std::string uvAccessorID2;

            if (primitive.materialId != "") {
                currMesh.materialIdx = document.materials.GetIndex(primitive.materialId);
            }
            // get accessorId first
            // assume normal is included in the glb
            if (primitive.TryGetAttributeAccessorId(Microsoft::glTF::ACCESSOR_POSITION,
                                                    positionAccessorID) &&
                primitive.TryGetAttributeAccessorId(Microsoft::glTF::ACCESSOR_NORMAL,
                                                    normalAccessorID)) {
                // tangent and uv could be optional
                bool hasTangent = primitive.TryGetAttributeAccessorId(
                        Microsoft::glTF::ACCESSOR_TANGENT, tangentAccessorID);
                bool hasUV = primitive.TryGetAttributeAccessorId(
                        Microsoft::glTF::ACCESSOR_TEXCOORD_0, uvAccessorID);
                bool hasUV2 = primitive.TryGetAttributeAccessorId(
                        Microsoft::glTF::ACCESSOR_TEXCOORD_1, uvAccessorID2);
                // indicesAccessorId is for element buffer
                if (document.accessors.Has(primitive.indicesAccessorId) &&
                    document.accessors.Has(positionAccessorID) &&
                    document.accessors.Has(normalAccessorID)) {
                    // get three buffers: ebo, position and normal
                    // interleave or separate ?
                    const Microsoft::glTF::Accessor &positionAccessor =
                            document.accessors[positionAccessorID];
                    const Microsoft::glTF::Accessor &normalAccessor =
                            document.accessors[normalAccessorID];
                    const Microsoft::glTF::Accessor &indicesAccessor =
                            document.accessors[primitive.indicesAccessorId];
                    // index could be u16_t or u32_t
                    // store indices to the currMesh
                    if (indicesAccessor.componentType == Microsoft::glTF::COMPONENT_UNSIGNED_INT) {
                        std::vector<unsigned int> indices =
                                resourceReader.ReadBinaryData<unsigned int>(document,
                                                                            indicesAccessor);
                        for (auto &index: indices) {
                            // LOGI("Indices: %d", index);
                            currMesh.indices.push_back(index);
                        }
                    } else if (indicesAccessor.componentType ==
                               Microsoft::glTF::COMPONENT_UNSIGNED_SHORT) {
                        std::vector<unsigned short> indices =
                                resourceReader.ReadBinaryData<unsigned short>(document,
                                                                              indicesAccessor);
                        for (auto &index: indices) {
                            // LOGI("Indices: %d", index);
                            currMesh.indices.push_back(index);
                        }
                    }
                    // store the vertices into currMesh
                    if (positionAccessor.componentType == Microsoft::glTF::COMPONENT_FLOAT &&
                        normalAccessor.componentType == Microsoft::glTF::COMPONENT_FLOAT) {
                        std::vector<float> positionBuffer =
                                resourceReader.ReadBinaryData<float>(document, positionAccessor);
                        std::vector<float> normalBuffer =
                                resourceReader.ReadBinaryData<float>(document, normalAccessor);

                        auto verticesCount = positionAccessor.count;
                        // vec4f
                        std::vector<float> tangentBuffer(verticesCount * 4, 0.0);
                        // vec2f
                        std::vector<float> uvBuffer(verticesCount * 2, 0.0);
                        // vec2f
                        std::vector<float> uv2Buffer(verticesCount * 2, 0.0);
                        if (hasTangent) {
                            const auto &tangentAccessor = document.accessors[tangentAccessorID];
                            tangentBuffer = resourceReader.ReadBinaryData<float>(document,
                                                                                 tangentAccessor);
                        }

                        if (hasUV) {
                            const auto &uvAccessor = document.accessors[uvAccessorID];
                            uvBuffer = resourceReader.ReadBinaryData<float>(document, uvAccessor);
                        }

                        if (hasUV2) {
                            const auto &uv2Accessor = document.accessors[uvAccessorID2];
                            uv2Buffer = resourceReader.ReadBinaryData<float>(document, uv2Accessor);
                        }

                        for (uint64_t i = 0; i < verticesCount; i++) {
                            const std::array<uint64_t, 4> vec4Offset = {4 * i, 4 * i + 1,
                                                                        4 * i + 2, 4 * i + 3};
                            const std::array<uint64_t, 3> vec3Offset = {3 * i, 3 * i + 1,
                                                                        3 * i + 2};
                            const std::array<uint64_t, 2> vec2Offset = {2 * i, 2 * i + 1};

                            // to begin with, keep it simple
                            //  vec3f(std::array{0.0f, 1.0f, 0.0f}),
//                            Vertex vertex{
//                                    .pos = vec3f(std::array{positionBuffer[vec3Offset[0]],
//                                                            positionBuffer[vec3Offset[1]],
//                                                            positionBuffer[vec3Offset[2]]}),
//                                    .texCoord = vec2f(std::array{uvBuffer[vec2Offset[0]],
//                                                                 uvBuffer[vec2Offset[1]]}),
//                                    .material = uint32_t(currMesh.materialIdx),
//                            };

                            Vertex vertex;
                            vertex.vx = positionBuffer[vec3Offset[0]];
                            vertex.vy = positionBuffer[vec3Offset[1]];
                            vertex.vz = positionBuffer[vec3Offset[2]];

                            vertex.ux = uvBuffer[vec2Offset[0]];
                            vertex.uy = uvBuffer[vec2Offset[1]];
                            vertex.material = uint32_t(currMesh.materialIdx);

                            // apply local transform for all the positions and normals (if exists)
//                            LOGI("Before Transform: [%d %f, %f, %f]",
//                                 i,
//                                 vertex.vx,
//                                 vertex.vy,
//                                 vertex.vz);
                            vertex.transform(m);
                            // LOGI("After Transform: [%f %f %f]",
                            //      vertex.vx,
                            //      vertex.vy,
                            //      vertex.vz);

                            currMesh.vertices.emplace_back(vertex);
                            // To Do: calculating Bounding Volumes
                            if (vertex.vx < currMesh.minAABB[COMPONENT::X]) {
                                currMesh.minAABB[COMPONENT::X] = vertex.vx;
                            }
                            if (vertex.vy < currMesh.minAABB[COMPONENT::Y]) {
                                currMesh.minAABB[COMPONENT::Y] = vertex.vy;
                            }
                            if (vertex.vz < currMesh.minAABB[COMPONENT::Z]) {
                                currMesh.minAABB[COMPONENT::Z] = vertex.vz;
                            }
                            if (vertex.vx > currMesh.maxAABB[COMPONENT::X]) {
                                currMesh.maxAABB[COMPONENT::X] = vertex.vx;
                            }
                            if (vertex.vy > currMesh.maxAABB[COMPONENT::Y]) {
                                currMesh.maxAABB[COMPONENT::Y] = vertex.vy;
                            }
                            if (vertex.vz > currMesh.maxAABB[COMPONENT::Z]) {
                                currMesh.maxAABB[COMPONENT::Z] = vertex.vz;
                            }
                        }
                    }
                }
            }
        }

        // indirect draw buffer
        if (!currMesh.indices.empty() && !currMesh.vertices.empty()) {
            IndirectDrawDef1 indirectDraw{
                    .indexCount = static_cast<uint32_t>(currMesh.indices.size()),
                    .instanceCount = 1,
                    .firstIndex = firstIndex,
                    .vertexOffset = vertexOffset,
                    .firstInstance = 0,
                    .meshId = static_cast<uint32_t>(outputScene.meshes.size()),
                    .materialIndex = currMesh.materialIdx,
            };

            firstIndex += currMesh.indices.size();
            vertexOffset += currMesh.vertices.size();

            currMesh.extents = (currMesh.maxAABB - currMesh.minAABB) * 0.5f;
            currMesh.center = currMesh.minAABB + currMesh.extents;

            // LOGI("Extents: [%f %f %f]", currMesh.extents[COMPONENT::X],
            //      currMesh.extents[COMPONENT::Y],
            //      currMesh.extents[COMPONENT::Z]);
            // LOGI("Center: [%f %f %f]", currMesh.center[COMPONENT::X], currMesh.center[COMPONENT::Y],
            //      currMesh.center[COMPONENT::Z]);

            outputScene.meshes.emplace_back(currMesh);
            outputScene.indirectDraw.emplace_back(indirectDraw);
            outputScene.totalVerticesByteSize +=
                    sizeof(Vertex) * outputScene.meshes.back().vertices.size();
            outputScene.totalIndexByteSize +=
                    sizeof(uint32_t) * outputScene.meshes.back().indices.size();
        }
    }
}

std::vector<uint8_t> readTextureRawBuffer(
        const Microsoft::glTF::Document &document,
        const Microsoft::glTF::GLTFResourceReader &resourceReader,
        const std::string &imageId) {
    auto &image = document.images.Get(imageId);
    auto imageBufferView = document.bufferViews.Get(image.bufferViewId);
    auto imageRawBuffer = resourceReader.ReadBinaryData<uint8_t>(document, imageBufferView);
    return imageRawBuffer;
}

void readTextures(const Microsoft::glTF::Document &document,
                  const Microsoft::glTF::GLTFResourceReader &resourceReader,
                  Scene &outputScene) {
    for (int i = 0; i < document.textures.Size(); ++i) {
        outputScene.textures.emplace_back(std::move(std::make_unique<Texture>(
                readTextureRawBuffer(document, resourceReader,document.textures[i].imageId))));
    }
}

void readMaterials(const Microsoft::glTF::Document &document, Scene &outputScene) {
    for (auto &mat: document.materials.Elements()) {
        Material curr;
        // mat.metallicRoughness.baseColorTexture.textureId is string in gltf sdk
        if (mat.metallicRoughness.baseColorTexture.textureId != "") {
            curr.basecolorTextureId = std::stoi(mat.metallicRoughness.baseColorTexture.textureId);
        }
        if (mat.metallicRoughness.metallicRoughnessTexture.textureId != "") {
            curr.metallicRoughnessTextureId = std::stoi(
                    mat.metallicRoughness.metallicRoughnessTexture.textureId);
        }
        curr.basecolorSamplerId = 0;
        curr.basecolor = vec4f(std::array{
                mat.metallicRoughness.baseColorFactor.r, mat.metallicRoughness.baseColorFactor.g,
                mat.metallicRoughness.baseColorFactor.b, mat.metallicRoughness.baseColorFactor.a
        });
        outputScene.materials.emplace_back(curr);
    }
}

std::shared_ptr<Scene> GltfBinaryIOReader::read(const std::vector<char> &binarybuffer) {
    std::shared_ptr<Scene> res = std::make_shared<Scene>();
    Scene &scene = *res.get();

    std::shared_ptr<std::stringstream> sstream = std::make_shared<std::stringstream>();
    for (const auto c: binarybuffer) {
        *sstream << c;
    }

    auto streamReader = std::make_shared<InMemoryStreamReader>(sstream);
    // in memory reader does not care about filepath
    auto glbStream = streamReader->GetInputStream("");

    // If the file has a '.glb' extension then create a GLBResourceReader. This class derives
    // from GLTFResourceReader and adds support for reading manifests from a GLB container's
    // JSON chunk and resource data from the binary chunk.
    auto glbResourceReader = std::make_shared<Microsoft::glTF::GLBResourceReader>(
            std::move(streamReader), std::move(glbStream));

    std::string manifest = glbResourceReader->GetJson(); // Get the manifest from the JSON chunk

    Microsoft::glTF::Document document;
    try {
        document = Microsoft::glTF::Deserialize(manifest);
    }
    catch (const Microsoft::glTF::GLTFException &ex) {
        std::stringstream ss;

        ss << "Microsoft::glTF::Deserialize failed: ";
        ss << ex.what();

        throw std::runtime_error(ss.str());
    }

    std::cout << "### glTF Info - ###\n\n";
    PrintDocumentInfo(document);
    PrintResourceInfo(document, *glbResourceReader);

    readMeshes(document, *glbResourceReader, scene);
    readTextures(document, *glbResourceReader, scene);
    readMaterials(document, scene);
    return res;
}
