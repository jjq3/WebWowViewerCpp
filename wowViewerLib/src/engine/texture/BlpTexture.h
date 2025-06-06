//
// Created by deamon on 19.06.17.
//

#ifndef WOWVIEWERLIB_BLPTEXTURE_H
#define WOWVIEWERLIB_BLPTEXTURE_H

#include "../persistance/header/blpFileHeader.h"
#include "../../include/sharedFile.h"
#include "../persistance/PersistentFile.h"
#include <vector>
#include <atomic>

enum class TextureFormat {
    Undetected,
    S3TC_RGBA_DXT1,
    S3TC_RGB_DXT1,
    S3TC_RGBA_DXT3,
    S3TC_RGBA_DXT5,
    BGRA,
    RGBA,
    PalARGB1555DitherFloydSteinberg,
    PalARGB4444DitherFloydSteinberg,
    PalARGB2565DitherFloydSteinberg,
    BC5_UNORM
};

class mipmapStruct_t {
public:
    std::vector<uint8_t> texture;
    int32_t width;
    int32_t height;
};
typedef std::shared_ptr<std::vector<mipmapStruct_t>> HMipmapsVector;

class BlpTexture : public PersistentFile{
public:
    explicit BlpTexture(std::string fileName){ m_textureName = fileName;};
    explicit BlpTexture(int fileDataId){m_textureName = std::to_string(fileDataId); m_fileDataId = fileDataId; };

    ~BlpTexture();

    std::string getTextureName() { return m_textureName; };
    void process(HFileContent blpFile, const std::string &fileName) override;
    const HMipmapsVector getMipmapsVector();

    TextureFormat getTextureFormat() {
        return m_textureFormat;
    }
    int getFileSize() {
        if (m_blpFile == nullptr) return 0;
        return m_blpFile->size();
    }
private:
    std::string m_textureName;
    int m_fileDataId = 0;

    HFileContent m_blpFile = nullptr;
    TextureFormat m_textureFormat = TextureFormat::Undetected;
};

extern std::atomic<int> blpTexturesLoaded;
extern std::atomic<int> blpTexturesSizeLoaded;


#endif //WOWVIEWERLIB_BLPTEXTURE_H
