#include <scene.h>

#define STB_IMAGE_IMPLEMENTATION

#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <stb_image_write.h>

#include <misc.h>
#include <thread>

Texture::Texture(const std::vector<uint8_t> &rawBuffer)
{
    // LOGI("rawBuffer Size: %d", rawBuffer.size());
    _data = stbi_load_from_memory(rawBuffer.data(), rawBuffer.size(), &_width, &_height,
                                  &_channels, STBI_rgb_alpha);
}

Texture::~Texture()
{
    log(Level::Info, "Texture::~Texture:", std::this_thread::get_id());
    stbi_image_free(_data);
}

TextureKtx::TextureKtx(std::string path)
{
    log(Level::Info, "TextureKtx: ", path);

    auto result = ktxTexture_CreateFromNamedFile(path.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &_ktxTexture);
    ASSERT(result == KTX_SUCCESS, "ktxTexture_CreateFromNamedFile should success");

    _width = _ktxTexture->baseWidth;
    _height = _ktxTexture->baseHeight;
    _data = ktxTexture_GetData(_ktxTexture);
}

TextureKtx::~TextureKtx()
{
    if (_ktxTexture)
        ktxTexture_Destroy(_ktxTexture);
}

Scene::~Scene()
{
    log(Level::Info, "Scene::~Scene:", std::this_thread::get_id());
}