#pragma region Copyright (c) 2014-2017 OpenRCT2 Developers
/*****************************************************************************
 * OpenRCT2, an open source clone of Roller Coaster Tycoon 2.
 *
 * OpenRCT2 is the work of many authors, a full list can be found in contributors.md
 * For more information, visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * A full copy of the GNU General Public License can be found in licence.txt
 *****************************************************************************/
#pragma endregion

#ifndef DISABLE_OPENGL

#include <algorithm>
#include <stdexcept>
#include <vector>
#include <openrct2/drawing/Drawing.h>
#include "TextureCache.h"

constexpr uint32 UNUSED_INDEX = 0xFFFFFFFF;

TextureCache::TextureCache()
{
    std::fill(_indexMap.begin(), _indexMap.end(), UNUSED_INDEX);
}

TextureCache::~TextureCache()
{
    FreeTextures();
}

void TextureCache::InvalidateImage(uint32 image)
{
    uint32 index = _indexMap[image];
    if (index == UNUSED_INDEX)
        return;

    AtlasTextureInfo& elem = _textureCache.at(index);

    _atlases[elem.index].Free(elem);
    _indexMap[image] = UNUSED_INDEX;

    if (index == _textureCache.size() - 1)
    {
        // Last element can be popped back.
        _textureCache.pop_back();
    }
    else
    {
        // Swap last element with element to erase and then pop back.
        AtlasTextureInfo& last = _textureCache.back();

        // Move last to current.
        elem = last;

        // Change index for moved element.
        _indexMap[last.image] = index;

        _textureCache.pop_back();
    }
}

BasicTextureInfo TextureCache::GetOrLoadImageTexture(uint32 image)
{
    image &= 0x7FFFF;

    uint32 index = _indexMap[image];
    if (index != UNUSED_INDEX)
    {
        const auto& info = _textureCache[index];
        return
        {
            info.index,
            info.normalizedBounds,
        };
    }

    index = (uint32)_textureCache.size();

    AtlasTextureInfo info = LoadImageTexture(image);

    _textureCache.push_back(info);
    _indexMap[image] = index;

    return info;
}

BasicTextureInfo TextureCache::GetOrLoadGlyphTexture(uint32 image, uint8 * palette)
{
    GlyphId glyphId;
    glyphId.Image = image;
    std::copy_n(palette, sizeof(glyphId.Palette), (uint8 *)&glyphId.Palette);

    auto kvp = _glyphTextureMap.find(glyphId);
    if (kvp != _glyphTextureMap.end())
    {
        const auto& info = kvp->second;
        return
        {
            info.index,
            info.normalizedBounds,
        };
    }

    auto cacheInfo = LoadGlyphTexture(image, palette);
    auto it = _glyphTextureMap.insert(std::make_pair(glyphId, cacheInfo));

    return (*it.first).second;
}

void TextureCache::CreateTextures()
{
    if (!_initialized)
    {
        // Determine width and height to use for texture atlases
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &_atlasesTextureDimensions);
        if (_atlasesTextureDimensions > TEXTURE_CACHE_MAX_ATLAS_SIZE) {
            _atlasesTextureDimensions = TEXTURE_CACHE_MAX_ATLAS_SIZE;
        }

        // Determine maximum number of atlases (minimum of size and array limit)
        glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &_atlasesTextureIndicesLimit);
        if (_atlasesTextureDimensions < _atlasesTextureIndicesLimit) _atlasesTextureIndicesLimit = _atlasesTextureDimensions;

        glGenTextures(1, &_atlasesTexture);
        glBindTexture(GL_TEXTURE_2D_ARRAY, _atlasesTexture);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        glGenTextures(1, &_paletteTexture);
        glBindTexture(GL_TEXTURE_RECTANGLE, _paletteTexture);
        glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        GeneratePaletteTexture();

        _initialized = true;
        _atlasesTextureIndices = 0;
    }
}

void TextureCache::GeneratePaletteTexture()
{
    rct_drawpixelinfo dpi = CreateDPI(256, PALETTE_TO_G1_OFFSET_COUNT + 5);

    // Init no-op palette
    for (int i=0; i < 256; ++i)
    {
        dpi.bits[i] = i;
    }

    for (int i=0; i < PALETTE_TO_G1_OFFSET_COUNT; ++i)
    {
        GLint y = PaletteToY(i);
        uint16 image = palette_to_g1_offset[i];
        auto element = gfx_get_g1_element(image);
        gfx_draw_sprite_software(&dpi, image, -element->x_offset, y - element->y_offset, 0);
    }

    glBindTexture(GL_TEXTURE_RECTANGLE, _paletteTexture);
    glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_R8UI, 256, PALETTE_TO_G1_OFFSET_COUNT + 5, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, dpi.bits);
    DeleteDPI(dpi);
}

void TextureCache::EnlargeAtlasesTexture(GLuint newEntries)
{
    CreateTextures();

    GLuint newIndices = _atlasesTextureIndices + newEntries;

    // Retrieve current array data
    auto oldPixels = std::vector<char>(_atlasesTextureDimensions * _atlasesTextureDimensions * _atlasesTextureIndices);
    if (oldPixels.size() > 0)
    {
        glGetTexImage(GL_TEXTURE_2D_ARRAY, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, oldPixels.data());
    }

    glBindTexture(GL_TEXTURE_2D_ARRAY, _atlasesTexture);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_R8UI, _atlasesTextureDimensions, _atlasesTextureDimensions, newIndices, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, nullptr);

    // Restore old data
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, _atlasesTextureDimensions, _atlasesTextureDimensions, _atlasesTextureIndices, GL_RED_INTEGER, GL_UNSIGNED_BYTE, oldPixels.data());

    _atlasesTextureIndices = newIndices;
}

AtlasTextureInfo TextureCache::LoadImageTexture(uint32 image)
{
    rct_drawpixelinfo dpi = GetImageAsDPI(image, 0);

    auto cacheInfo = AllocateImage(dpi.width, dpi.height);
    cacheInfo.image = image;

    glBindTexture(GL_TEXTURE_2D_ARRAY, _atlasesTexture);
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, cacheInfo.bounds.x, cacheInfo.bounds.y, cacheInfo.index, dpi.width, dpi.height, 1, GL_RED_INTEGER, GL_UNSIGNED_BYTE, dpi.bits);

    DeleteDPI(dpi);

    return cacheInfo;
}

AtlasTextureInfo TextureCache::LoadGlyphTexture(uint32 image, uint8 * palette)
{
    rct_drawpixelinfo dpi = GetGlyphAsDPI(image, palette);

    auto cacheInfo = AllocateImage(dpi.width, dpi.height);
    cacheInfo.image = image;

    glBindTexture(GL_TEXTURE_2D_ARRAY, _atlasesTexture);
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, cacheInfo.bounds.x, cacheInfo.bounds.y, cacheInfo.index, dpi.width, dpi.height, 1, GL_RED_INTEGER, GL_UNSIGNED_BYTE, dpi.bits);

    DeleteDPI(dpi);

    return cacheInfo;
}

AtlasTextureInfo TextureCache::AllocateImage(sint32 imageWidth, sint32 imageHeight)
{
    CreateTextures();

    // Find an atlas that fits this image
    for (Atlas& atlas : _atlases)
    {
        if (atlas.GetFreeSlots() > 0 && atlas.IsImageSuitable(imageWidth, imageHeight))
        {
            return atlas.Allocate(imageWidth, imageHeight);
        }
    }

    // If there is no such atlas, then create a new one
    if ((sint32) _atlases.size() >= _atlasesTextureIndicesLimit)
    {
        throw std::runtime_error("more texture atlases required, but device limit reached!");
    }

    sint32 atlasIndex = (sint32) _atlases.size();
    sint32 atlasSize = (sint32) powf(2, (float) Atlas::CalculateImageSizeOrder(imageWidth, imageHeight));

#ifdef DEBUG
    log_verbose("new texture atlas #%d (size %d) allocated\n", atlasIndex, atlasSize);
#endif

    _atlases.emplace_back(atlasIndex, atlasSize);
    _atlases.back().Initialise(_atlasesTextureDimensions, _atlasesTextureDimensions);

    // Enlarge texture array to support new atlas
    EnlargeAtlasesTexture(1);

    // And allocate from the new atlas
    return _atlases.back().Allocate(imageWidth, imageHeight);
}

rct_drawpixelinfo TextureCache::GetImageAsDPI(uint32 image, uint32 tertiaryColour)
{
    auto g1Element = gfx_get_g1_element(image & 0x7FFFF);
    sint32 width = g1Element->width;
    sint32 height = g1Element->height;

    rct_drawpixelinfo dpi = CreateDPI(width, height);
    gfx_draw_sprite_software(&dpi, image, -g1Element->x_offset, -g1Element->y_offset, tertiaryColour);
    return dpi;
}

rct_drawpixelinfo TextureCache::GetGlyphAsDPI(uint32 image, uint8 * palette)
{
    auto g1Element = gfx_get_g1_element(image & 0x7FFFF);
    sint32 width = g1Element->width;
    sint32 height = g1Element->height;

    rct_drawpixelinfo dpi = CreateDPI(width, height);
    gfx_draw_sprite_palette_set_software(&dpi, image, -g1Element->x_offset, -g1Element->y_offset, palette, nullptr);
    return dpi;
}

void TextureCache::FreeTextures()
{
    // Free array texture
    glDeleteTextures(1, &_atlasesTexture);
    _textureCache.clear();
    std::fill(_indexMap.begin(), _indexMap.end(), UNUSED_INDEX);
}

rct_drawpixelinfo TextureCache::CreateDPI(sint32 width, sint32 height)
{
    size_t numPixels = width * height;
    auto pixels8 = new uint8[numPixels];
    std::fill_n(pixels8, numPixels, 0);

    rct_drawpixelinfo dpi;
    dpi.bits = pixels8;
    dpi.pitch = 0;
    dpi.x = 0;
    dpi.y = 0;
    dpi.width = width;
    dpi.height = height;
    dpi.zoom_level = 0;
    return dpi;
}

void TextureCache::DeleteDPI(rct_drawpixelinfo dpi)
{
    delete dpi.bits;
}

GLuint TextureCache::GetAtlasesTexture()
{
    return _atlasesTexture;
}

GLuint TextureCache::GetPaletteTexture()
{
    return _paletteTexture;
}

GLint TextureCache::PaletteToY(uint32 palette)
{
    return palette > PALETTE_WATER ? palette + 5 : palette + 1;
}

#endif /* DISABLE_OPENGL */
