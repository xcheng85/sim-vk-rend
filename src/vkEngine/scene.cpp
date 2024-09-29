#include <scene.h>

#define STB_IMAGE_IMPLEMENTATION

#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <stb_image_write.h>

#include <misc.h>
#include <thread>

Texture::Texture(const std::vector<uint8_t> &rawBuffer) {
    //LOGI("rawBuffer Size: %d", rawBuffer.size());
    data = stbi_load_from_memory(rawBuffer.data(), rawBuffer.size(), &width, &height,
                                 &channels, STBI_rgb_alpha);
}

Texture::~Texture() {
    log(Level::Info, "Texture::~Texture:", std::this_thread::get_id());
    stbi_image_free(data);
}

Scene::~Scene(){
     log(Level::Info, "Scene::~Scene:", std::this_thread::get_id());
}