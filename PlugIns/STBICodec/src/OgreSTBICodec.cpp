/*
-----------------------------------------------------------------------------
This source file is part of OGRE
(Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.ogre3d.org/

Copyright (c) 2000-2014 Torus Knot Software Ltd

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
-----------------------------------------------------------------------------
*/
module;

#include <cstdlib>
#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#include <zlib.h>
#include "stbi/stb_image.h"

extern "C" auto custom_zlib_compress(unsigned char* data, int data_len, int* out_len, int /*quality*/) -> unsigned char*;
#define STBIW_ZLIB_COMPRESS custom_zlib_compress
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "stbi/stb_image_write.h"

module Ogre.PlugIns.STBICodec;

import Ogre.Core;

import <format>;
import <memory>;
import <ostream>;
import <string>;
import <utility>;
import <vector>;

extern "C" auto custom_zlib_compress(Ogre::uchar* data, int data_len, int* out_len, int /*quality*/) -> Ogre::uchar*
{
    unsigned long destLen = compressBound(data_len);
    auto* dest = (Ogre::uchar*)malloc(destLen);
    int ret = compress(dest, &destLen, data, data_len); // use default quality
    if (ret != Z_OK)
    {
        free(dest);
        OGRE_EXCEPT(Ogre::ExceptionCodes::INTERNAL_ERROR, "compress failed");
    }

    *out_len = destLen;
    return dest;
}

namespace Ogre {

    STBIImageCodec::RegisteredCodecList STBIImageCodec::msCodecList;
    //---------------------------------------------------------------------
    void STBIImageCodec::startup()
    {
        stbi_convert_iphone_png_to_rgb(1);
        stbi_set_unpremultiply_on_load(1);

        LogManager::getSingleton().logMessage("stb_image - v2.27 - public domain image loader");
        
        // Register codecs
        String exts = "jpeg,jpg,png,bmp,psd,tga,gif,pic,ppm,pgm,hdr";
        auto const extsVector = StringUtil::split(exts, ",");
        for (auto & v : extsVector)
        {
            ImageCodec* codec = new STBIImageCodec(v);
            msCodecList.push_back(codec);
            Codec::registerCodec(codec);
        }

        LogManager::getSingleton().logMessage(::std::format("Supported formats: {}", exts));
    }
    //---------------------------------------------------------------------
    void STBIImageCodec::shutdown()
    {
        for (auto & i : msCodecList)
        {
            Codec::unregisterCodec(i);
            delete i;
        }
        msCodecList.clear();
    }
    //---------------------------------------------------------------------
    STBIImageCodec::STBIImageCodec(std::string_view type):
        mType(type)
    { 
    }
    //---------------------------------------------------------------------
    auto STBIImageCodec::encode(const MemoryDataStreamPtr& input,
                                         const CodecDataPtr& pData) const -> DataStreamPtr
    {
        if(mType != "png") {
            OGRE_EXCEPT(ExceptionCodes::NOT_IMPLEMENTED,
                        "currently only encoding to PNG supported",
                        "STBIImageCodec::encode" ) ;
        }

        auto* pImgData = static_cast<ImageData*>(pData.get());
        PixelFormat format = pImgData->format;
        uchar* inputData = input->getPtr();

        // Convert image data to ABGR format for STBI (unless it's already compatible)
        uchar* tempData = nullptr;
        if(format != Ogre::PixelFormat::A8B8G8R8 && format != PixelFormat::B8G8R8 && format != PixelFormat::BYTE_LA && 
            format != PixelFormat::L8 && format != PixelFormat::R8)
        {   
            format = Ogre::PixelFormat::A8B8G8R8;
            size_t tempDataSize = pImgData->width * pImgData->height * pImgData->depth * Ogre::PixelUtil::getNumElemBytes(format);
            tempData = new unsigned char[tempDataSize];
            Ogre::PixelBox pbIn(pImgData->width, pImgData->height, pImgData->depth, pImgData->format, inputData);
            Ogre::PixelBox pbOut(pImgData->width, pImgData->height, pImgData->depth, format, tempData);
            PixelUtil::bulkPixelConversion(pbIn, pbOut);

            inputData = tempData;
        }

        // Save to PNG
        int channels = (int)PixelUtil::getComponentCount(format);
        int stride = pImgData->width * (int)PixelUtil::getNumElemBytes(format);
        int len;
        uchar* data = stbi_write_png_to_mem(inputData, stride, pImgData->width, pImgData->height, channels, &len);

        if(tempData)
        {
            delete [] tempData;
        }

        if (!data) {
            OGRE_EXCEPT(ExceptionCodes::INTERNAL_ERROR,
                ::std::format("Error encoding image: {}", String(stbi_failure_reason())),
                "STBIImageCodec::encode");
        }

        return DataStreamPtr(new MemoryDataStream(data, len, true));
    }
    //---------------------------------------------------------------------
    void STBIImageCodec::encodeToFile(const MemoryDataStreamPtr& input, std::string_view outFileName,
                                      const CodecDataPtr& pData) const
    {
        MemoryDataStreamPtr data = static_pointer_cast<MemoryDataStream>(encode(input, pData));
        std::ofstream f(outFileName.data(), std::ios::out | std::ios::binary);

        if (!f.is_open())
        {
            OGRE_EXCEPT(ExceptionCodes::INTERNAL_ERROR, ::std::format("could not open file {}", outFileName));
        }

        f.write((char*)data->getPtr(), data->size());
    }
    //---------------------------------------------------------------------
    auto STBIImageCodec::decode(const DataStreamPtr& input) const -> ImageCodec::DecodeResult
    {
        String contents = input->getAsString();

        int width, height, components;
        stbi_uc* pixelData = stbi_load_from_memory((const uchar*)contents.data(),
                static_cast<int>(contents.size()), &width, &height, &components, 0);

        if (!pixelData)
        {
            OGRE_EXCEPT(ExceptionCodes::INTERNAL_ERROR, 
                ::std::format("Error decoding image: {}", String(stbi_failure_reason())),
                "STBIImageCodec::decode");
        }

        SharedPtr<ImageData> imgData(new ImageData());

        imgData->depth = 1; // only 2D formats handled by this codec
        imgData->width = width;
        imgData->height = height;
        imgData->num_mipmaps = {}; // no mipmaps in non-DDS
        imgData->flags = {};

        switch( components )
        {
            case 1:
                imgData->format = PixelFormat::BYTE_L;
                break;
            case 2:
                imgData->format = PixelFormat::BYTE_LA;
                break;
            case 3:
                imgData->format = PixelFormat::BYTE_RGB;
                break;
            case 4:
                imgData->format = PixelFormat::BYTE_RGBA;
                break;
            default:
                stbi_image_free(pixelData);
                OGRE_EXCEPT(ExceptionCodes::ITEM_NOT_FOUND,
                            "Unknown or unsupported image format",
                            "STBIImageCodec::decode");
                break;
        }
        
        size_t dstPitch = imgData->width * PixelUtil::getNumElemBytes(imgData->format);
        imgData->size = dstPitch * imgData->height;
        MemoryDataStreamPtr output(new MemoryDataStream(pixelData, imgData->size, true));
        
        DecodeResult ret;
        ret.first = output;
        ret.second = imgData;
        return ret;
    }
    //---------------------------------------------------------------------    
    auto STBIImageCodec::getType() const -> std::string_view
    {
        return mType;
    }
    //---------------------------------------------------------------------
    auto STBIImageCodec::magicNumberToFileExt(const char *magicNumberPtr, size_t maxbytes) const -> std::string_view
    {
        return BLANKSTRING;
    }

    auto STBIPlugin::getName() const noexcept -> std::string_view {
        static std::string_view const constexpr name = "STB Image Codec";
        return name;
    }
}
