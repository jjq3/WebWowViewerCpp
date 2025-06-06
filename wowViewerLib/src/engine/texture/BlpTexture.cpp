//
// Created by deamon on 19.06.17.
//

#include <vector>
#include <cmath>
#include <iostream>
#include <assert.h>

#include "BlpTexture.h"
#include "DxtDecompress.h"
#include "../persistance/helper/ChunkFileReader.h"

std::atomic<int> blpTexturesLoaded = 0;
std::atomic<int> blpTexturesSizeLoaded = 0;

TextureFormat getTextureType(BlpFile *blpFile) {
    TextureFormat textureFormat = TextureFormat::Undetected;
    switch (blpFile->preferredFormat) {
        case BLPPixelFormat::PIXEL_DXT1:
            if (blpFile->alphaChannelBitDepth > 0) {
                textureFormat = TextureFormat::S3TC_RGBA_DXT1;
            } else {
                textureFormat = TextureFormat::S3TC_RGB_DXT1;
            }
            break;
        case BLPPixelFormat::PIXEL_DXT3:
            textureFormat = TextureFormat::S3TC_RGBA_DXT3;
            break;
        case BLPPixelFormat::PIXEL_ARGB8888:
            textureFormat = TextureFormat::RGBA;
            break;

        case BLPPixelFormat::PIXEL_ARGB1555:
            textureFormat = TextureFormat::PalARGB1555DitherFloydSteinberg;
            break;
        case BLPPixelFormat::PIXEL_ARGB4444:
            textureFormat = TextureFormat::PalARGB4444DitherFloydSteinberg;
            break;
        case BLPPixelFormat::PIXEL_RGB565:
            textureFormat = TextureFormat::RGBA;
            break;
        case BLPPixelFormat::PIXEL_DXT5:
            textureFormat = TextureFormat::S3TC_RGBA_DXT5;
            break;
        case BLPPixelFormat::PIXEL_UNSPECIFIED:
            textureFormat = TextureFormat::RGBA;
            break;
        case BLPPixelFormat::PIXEL_ARGB2565:
            textureFormat = TextureFormat::PalARGB2565DitherFloydSteinberg;
            break;
        case BLPPixelFormat::PIXEL_BC5:
            textureFormat = TextureFormat::BC5_UNORM;
            break;

        default:
            break;
    }
    return textureFormat;
}
HMipmapsVector parseMipmaps(BlpFile *blpFile, TextureFormat textureFormat, size_t blpFileSize) {
    int32_t width = blpFile->width;
    int32_t height = blpFile->height;

    int minSize = 0;
    if ((textureFormat == TextureFormat::S3TC_RGBA_DXT5) || (textureFormat == TextureFormat::S3TC_RGBA_DXT3)) {
        minSize = (int32_t) (floor((1 + 3) / 4) * floor((1 + 3) / 4) * 16);
    }
    if ((textureFormat == TextureFormat::S3TC_RGB_DXT1) || (textureFormat == TextureFormat::S3TC_RGBA_DXT1)) {
        minSize = (int32_t) (floor((1 + 3) / 4) * floor((1 + 3) / 4) * 8);
    }

    int mipmapsCnt = 0;
    for (int i = 0; i < 15; i++) {
        if ((blpFile->lengths[i] == 0) || (blpFile->offsets[i] == 0)) break;
        mipmapsCnt++;
    }
    auto mipmaps = std::make_shared<std::vector<mipmapStruct_t>>();
    auto &pmipmaps = (*mipmaps);
    pmipmaps.resize(mipmapsCnt);

    for (int i = 0; i < mipmapsCnt; i++) {
        if ((blpFile->lengths[i] == 0) || (blpFile->offsets[i] == 0)) break;
        if ( (blpFile->offsets[i] >= blpFileSize) ||
             ((blpFile->offsets[i] + blpFile->lengths[i]) >= blpFileSize)
        ) {
            std::cout << "wrong offset in blp file" << std::endl;
            continue;
        }

        uint8_t *data = ((uint8_t *) blpFile)+blpFile->offsets[i]; //blpFile->lengths[i]);

        //Check dimensions for dxt textures
        int32_t validSize = blpFile->lengths[i];
        if ((textureFormat == TextureFormat::S3TC_RGBA_DXT5) || (textureFormat == TextureFormat::S3TC_RGBA_DXT3)) {
            validSize = (int32_t) (floor((width + 3) / 4) * floor((height + 3) / 4) * 16);
        }
        if ((textureFormat == TextureFormat::S3TC_RGB_DXT1) || (textureFormat == TextureFormat::S3TC_RGBA_DXT1)) {
            validSize = (int32_t) (floor((width + 3) / 4) * floor((height + 3) / 4) * 8);
        }

//        if (minSize == validSize) break;

        mipmapStruct_t &mipmapStruct = pmipmaps[i];
        mipmapStruct.height = height;
        mipmapStruct.width = width;
        mipmapStruct.texture.resize(validSize, 0);

        //If the bytes are not compressed
        if (blpFile->preferredFormat == BLPPixelFormat::PIXEL_UNSPECIFIED &&
            blpFile->colorEncoding == BLPColorEncoding::COLOR_PALETTE)
        {
            uint8_t* paleteData = data;
            validSize = 4 * width * height;
            mipmapStruct.texture = std::vector<uint8_t>(validSize, 0);

            for (int j = 0; j < width * height; j++) {
                uint8_t colIndex = paleteData[j];
                uint8_t b = blpFile->palette[colIndex * 4 + 0];
                uint8_t g = blpFile->palette[colIndex * 4 + 1];
                uint8_t r = blpFile->palette[colIndex * 4 + 2];
                uint8_t a = blpFile->palette[colIndex * 4 + 3];

                mipmapStruct.texture[j * 4 + 0] = r;
                mipmapStruct.texture[j * 4 + 1] = g;
                mipmapStruct.texture[j * 4 + 2] = b;
                mipmapStruct.texture[j * 4 + 3] = a;
            }
        }
        else if ((blpFile->preferredFormat == BLPPixelFormat::PIXEL_UNSPECIFIED &&
                  blpFile->colorEncoding == BLPColorEncoding::COLOR_ARGB8888) ||
                 (blpFile->preferredFormat == BLPPixelFormat::PIXEL_ARGB8888 &&
                  blpFile->colorEncoding == BLPColorEncoding::COLOR_ARGB8888))
        {
            //Turn BGRA into RGBA

            validSize = 4 * width * height;
            mipmapStruct.texture = std::vector<uint8_t>(validSize, 0);
            for (int j = 0; j < width * height; j++) {
                uint8_t b = data[j * 4 + 0];
                uint8_t g = data[j * 4 + 1];
                uint8_t r = data[j * 4 + 2];
                uint8_t a = data[j * 4 + 3];

                mipmapStruct.texture[j * 4 + 0] = r;
                mipmapStruct.texture[j * 4 + 1] = g;
                mipmapStruct.texture[j * 4 + 2] = b;
                mipmapStruct.texture[j * 4 + 3] = a;
            }
        } else {
            size_t sizeToCopy = std::min<size_t>(blpFile->lengths[i], mipmapStruct.texture.size());
            assert(sizeToCopy <= mipmapStruct.texture.size());
            std::copy(data, data + sizeToCopy, mipmapStruct.texture.data());
        }

        height = height / 2;
        width = width / 2;
        height = (height == 0) ? 1 : height;
        width = (width == 0) ? 1 : width;
    }

    return mipmaps;
}

void BlpTexture::process(HFileContent blpFile, const std::string &fileName) {
    /* Post load for texture data. Can't define them through declarative definition */
    /* Determine texture format */
    int fileSize = blpFile->size();
    BlpFile *pBlpFile = (BlpFile *) blpFile->data();
    if (pBlpFile->fileIdent != '2PLB') {
        std::cerr << "Wrong ident for BLP2 file " << pBlpFile->fileIdent << " " << fileName << std::endl;
        this->fsStatus = FileStatus ::FSRejected;
        return;
    }
    this->m_textureFormat = getTextureType(pBlpFile);

    /* Load texture by mipmaps */
    assert(this->m_textureFormat != TextureFormat::Undetected);
    m_blpFile = blpFile;

//    /* Load texture into GL memory */
//    this->texture = createGlTexture(pBlpFile, textureFormat, mipmaps, fileName);
    this->fsStatus = FileStatus ::FSLoaded;
    this->m_textureName = fileName;

    blpTexturesLoaded.fetch_add(1);
    blpTexturesSizeLoaded.fetch_add(blpFile->size());
}

const HMipmapsVector BlpTexture::getMipmapsVector() {
    return parseMipmaps( (BlpFile *)m_blpFile->data(), m_textureFormat, m_blpFile->size());
}

BlpTexture::~BlpTexture() {
    if (m_blpFile) {
        blpTexturesLoaded.fetch_add(-1);
        blpTexturesSizeLoaded.fetch_add(-m_blpFile->size());
    }
}

