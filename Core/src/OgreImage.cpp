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

#include <cassert>
#include <cstring>

module Ogre.Core;

import :Codec;
import :DataStream;
import :Exception;
import :Image;
import :ImageCodec;
import :ImageResampler;
import :Math;
import :ResourceGroupManager;
import :SharedPtr;
import :String;

import <algorithm>;
import <any>;
import <memory>;
import <span>;

namespace Ogre {
    ImageCodec::~ImageCodec() = default;

    void ImageCodec::decode(const DataStreamPtr& input, ::std::any const& output) const
    {
        auto[data, codec] = decode(input);

        auto pData = static_cast<ImageCodec::ImageData*>(codec.get());

        auto* dest = any_cast<Image*>(output);
        dest->mWidth = pData->width;
        dest->mHeight = pData->height;
        dest->mDepth = pData->depth;
        dest->mBufSize = pData->size;
        dest->mNumMipmaps = pData->num_mipmaps;
        dest->mFlags = pData->flags;
        dest->mFormat = pData->format;
        // Just use internal buffer of returned memory stream
        dest->mBuffer = data->getPtr();
        // Make sure stream does not delete
        data->setFreeOnClose(false);
    }

    auto ImageCodec::encode(::std::any const& input) const -> DataStreamPtr
    {
        auto* src = any_cast<Image*>(input);

        auto imgData = std::make_shared<ImageCodec::ImageData>();
        imgData->format = src->getFormat();
        imgData->height = src->getHeight();
        imgData->width = src->getWidth();
        imgData->depth = src->getDepth();
        imgData->size = src->getSize();
        imgData->num_mipmaps = src->getNumMipmaps();

        // Wrap memory, be sure not to delete when stream destroyed
        auto wrapper = std::make_shared<MemoryDataStream>(src->getData(), src->getSize(), false);
        return encode(wrapper, imgData);
    }
    void ImageCodec::encodeToFile(::std::any const& input, std::string_view outFileName) const
    {
        auto* src = any_cast<Image*>(input);

        auto imgData = std::make_shared<ImageCodec::ImageData>();
        imgData->format = src->getFormat();
        imgData->height = src->getHeight();
        imgData->width = src->getWidth();
        imgData->depth = src->getDepth();
        imgData->size = src->getSize();
		imgData->num_mipmaps = src->getNumMipmaps();

        // Wrap memory, be sure not to delete when stream destroyed
        auto wrapper = std::make_shared<MemoryDataStream>(src->getData(), src->getSize(), false);
        encodeToFile(wrapper, outFileName, imgData);
    }

    //-----------------------------------------------------------------------------
    Image::Image(PixelFormat format, uint32 width, uint32 height, uint32 depth, uchar* buffer, bool autoDelete)
        : 
        mFormat(format)
        
    {
        if (format == PixelFormat::UNKNOWN)
            return;

        size_t size = calculateSize(TextureMipmap{}, 1,  width, height, depth, mFormat);

        if (size == 0)
            return;

        if (!buffer)
            buffer = static_cast<unsigned char*>(malloc(size));
        loadDynamicImage(buffer, width, height, depth, format, autoDelete);
    }

    void Image::create(PixelFormat format, uint32 width, uint32 height, uint32 depth, uint32 numFaces,
                       TextureMipmap numMipMaps)
    {
        size_t size = calculateSize(numMipMaps, numFaces, width, height, depth, format);
        if (!mAutoDelete || !mBuffer || mBufSize != size)
        {
            freeMemory();
            mBuffer = static_cast<unsigned char*>(malloc(size)); // allocate
        }

        // make sure freeMemory() does nothing, we set this true immediately after
        mAutoDelete = false;
        loadDynamicImage(mBuffer, width, height, depth, format, true, numFaces, numMipMaps);
    }

    //-----------------------------------------------------------------------------
    Image::Image( const Image &img )
        
        
    {
        // call assignment operator
        *this = img;
    }

    //-----------------------------------------------------------------------------
    Image::~Image()
    {
        freeMemory();
    }
    //---------------------------------------------------------------------
    void Image::freeMemory()
    {
        //Only delete if this was not a dynamic image (meaning app holds & destroys buffer)
        if( mBuffer && mAutoDelete )
        {
            free(mBuffer);
            mBuffer = nullptr;
        }

    }

    //-----------------------------------------------------------------------------
    auto Image::operator=(const Image& img) -> Image&
    {
        // Only create & copy when other data was owning
        if (img.mBuffer && img.mAutoDelete)
        {
            create(img.mFormat, img.mWidth, img.mHeight, img.mDepth, img.getNumFaces(), img.mNumMipmaps);
            memcpy(mBuffer, img.mBuffer, mBufSize);
        }
        else
        {
            loadDynamicImage(img.mBuffer, img.mWidth, img.mHeight, img.mDepth, img.mFormat, false,
                             img.getNumFaces(), img.mNumMipmaps);
        }

        return *this;
    }

    void Image::setTo(const ColourValue& col)
    {
        OgreAssert(mBuffer, "No image data loaded");
        if(col == ColourValue::ZERO)
        {
            memset(mBuffer, 0, getSize());
            return;
        }

        uchar rawCol[4 * sizeof(float)]; // max packed size currently is 4*float
        PixelUtil::packColour(col, mFormat, rawCol);
        for(size_t p = 0; p < mBufSize; p += mPixelSize)
        {
            memcpy(mBuffer + p, rawCol, mPixelSize);
        }
    }

    //-----------------------------------------------------------------------------
    auto Image::flipAroundY() -> Image &
    {
        OgreAssert(mBuffer, "No image data loaded");
        mNumMipmaps = {}; // Image operations lose precomputed mipmaps

        ushort y;
        switch (mPixelSize)
        {
        case 1:
            for (y = 0; y < mHeight; y++)
            {
                std::ranges::reverse(std::span{mBuffer + mWidth * y, mWidth});
            }
            break;

        case 2:
            for (y = 0; y < mHeight; y++)
            {
                std::ranges::reverse(std::span{(ushort*)mBuffer + mWidth * y, mWidth});
            }
            break;

        case 3:
            using uchar3 = std::array<uchar, 3>;
            for (y = 0; y < mHeight; y++)
            {
                std::ranges::reverse(std::span{(uchar3*)mBuffer + mWidth * y, mWidth});
            }
            break;

        case 4:
            for (y = 0; y < mHeight; y++)
            {
                std::ranges::reverse(std::span{(uint*)mBuffer + mWidth * y, mWidth});
            }
            break;

        default:
            OGRE_EXCEPT( 
                ExceptionCodes::INTERNAL_ERROR,
                "Unknown pixel depth",
                "Image::flipAroundY" );
            break;
        }

        return *this;

    }

    //-----------------------------------------------------------------------------
    auto Image::flipAroundX() -> Image &
    {
        OgreAssert(mBuffer, "No image data loaded");
        
        mNumMipmaps = {}; // Image operations lose precomputed mipmaps
        PixelUtil::bulkPixelVerticalFlip(getPixelBox());

        return *this;
    }

    //-----------------------------------------------------------------------------
    auto Image::loadDynamicImage(uchar* pData, uint32 uWidth, uint32 uHeight, uint32 depth,
                                   PixelFormat eFormat, bool autoDelete, uint32 numFaces, TextureMipmap numMipMaps) -> Image&
    {

        freeMemory();
        // Set image metadata
        mWidth = uWidth;
        mHeight = uHeight;
        mDepth = depth;
        mFormat = eFormat;
        mPixelSize = static_cast<uchar>(PixelUtil::getNumElemBytes( mFormat ));
        mNumMipmaps = numMipMaps;
        mFlags = {};
        // Set flags
        if (PixelUtil::isCompressed(eFormat))
            mFlags |= ImageFlags::COMPRESSED;
        if (mDepth != 1)
            mFlags |= ImageFlags::_3D_TEXTURE;
        if(numFaces == 6)
            mFlags |= ImageFlags::CUBEMAP;
        OgreAssert(numFaces == 6 || numFaces == 1, "Invalid number of faces");

        mBufSize = calculateSize(numMipMaps, numFaces, uWidth, uHeight, depth, eFormat);
        mBuffer = pData;
        mAutoDelete = autoDelete;

        return *this;
    }

    //-----------------------------------------------------------------------------
    auto Image::loadRawData(const DataStreamPtr& stream, uint32 uWidth, uint32 uHeight, uint32 uDepth,
                              PixelFormat eFormat, uint32 numFaces, TextureMipmap numMipMaps) -> Image&
    {

        size_t size = calculateSize(numMipMaps, numFaces, uWidth, uHeight, uDepth, eFormat);
        OgreAssert(size == stream->size(), "Wrong stream size");

        auto *buffer = static_cast<unsigned char*>(malloc(size));
        stream->read(buffer, size);

        return loadDynamicImage(buffer,
            uWidth, uHeight, uDepth,
            eFormat, true, numFaces, numMipMaps);

    }
    //-----------------------------------------------------------------------------
    auto Image::load(std::string_view strFileName, std::string_view group) -> Image &
    {

        String strExt;

        size_t pos = strFileName.find_last_of('.');
        if( pos != String::npos && pos < (strFileName.length() - 1))
        {
            strExt = strFileName.substr(pos+1);
        }

        DataStreamPtr encoded = ResourceGroupManager::getSingleton().openResource(strFileName, group);
        return load(encoded, strExt);

    }
    //-----------------------------------------------------------------------------
    void Image::save(std::string_view filename)
    {
        OgreAssert(mBuffer, "No image data loaded");

        std::string_view base, ext;
        StringUtil::splitBaseFilename(filename, base, ext);

        // getCodec throws when no codec is found
        Codec::getCodec(ext)->encodeToFile(this, filename);
    }
    //---------------------------------------------------------------------
    auto Image::encode(std::string_view formatextension) -> DataStreamPtr
    {
        OgreAssert(mBuffer, "No image data loaded");
        // getCodec throws when no codec is found
        return Codec::getCodec(formatextension)->encode(this);
    }
    //-----------------------------------------------------------------------------
    auto Image::load(const DataStreamPtr& stream, std::string_view type ) -> Image &
    {
        freeMemory();

        Codec * pCodec = nullptr;
        if (!type.empty())
        {
            // use named codec
            pCodec = Codec::getCodec(type);
        }
        else
        {
            // derive from magic number
            // read the first 32 bytes or file size, if less
            size_t magicLen = std::min(stream->size(), (size_t)32);
            char magicBuf[32];
            stream->read(magicBuf, magicLen);
            // return to start
            stream->seek(0);
            pCodec = Codec::getCodec(magicBuf, magicLen);

            if (!pCodec)
                OGRE_EXCEPT(ExceptionCodes::INVALIDPARAMS,
                            "Unable to load image: Image format is unknown. Unable to identify codec. "
                            "Check it or specify format explicitly.");
        }

        pCodec->decode(stream, this);

        // compute the pixel size
        mPixelSize = static_cast<uchar>(PixelUtil::getNumElemBytes( mFormat ));
        // make sure we delete
        mAutoDelete = true;

        return *this;
    }
    //---------------------------------------------------------------------
    auto Image::getFileExtFromMagic(const DataStreamPtr stream) -> std::string_view
    {
        // read the first 32 bytes or file size, if less
        size_t magicLen = std::min(stream->size(), (size_t)32);
        char magicBuf[32];
        stream->read(magicBuf, magicLen);
        // return to start
        stream->seek(0);
        Codec* pCodec = Codec::getCodec(magicBuf, magicLen);

        if(pCodec)
            return pCodec->getType();
        else
            return BLANKSTRING;

    }
    //-----------------------------------------------------------------------------
    auto Image::getSize() const -> size_t
    {
        return mBufSize;
    }

    //-----------------------------------------------------------------------------
    auto Image::getNumMipmaps() const noexcept -> TextureMipmap
    {
        return mNumMipmaps;
    }

    //-----------------------------------------------------------------------------
    auto Image::hasFlag(const ImageFlags imgFlag) const -> bool
    {
        return (mFlags & imgFlag) != ImageFlags{};
    }

    //-----------------------------------------------------------------------------
    auto Image::getDepth() const noexcept -> uint32
    {
        return mDepth;
    }
    //-----------------------------------------------------------------------------
    auto Image::getWidth() const noexcept -> uint32
    {
        return mWidth;
    }

    //-----------------------------------------------------------------------------
    auto Image::getHeight() const noexcept -> uint32
    {
        return mHeight;
    }
    //-----------------------------------------------------------------------------
    auto Image::getNumFaces() const noexcept -> uint32
    {
        if(hasFlag(ImageFlags::CUBEMAP))
            return 6;
        return 1;
    }
    //-----------------------------------------------------------------------------
    auto Image::getRowSpan() const -> size_t
    {
        return mWidth * mPixelSize;
    }

    //-----------------------------------------------------------------------------
    auto Image::getFormat() const -> PixelFormat
    {
        return mFormat;
    }

    //-----------------------------------------------------------------------------
    auto Image::getBPP() const -> uchar
    {
        return mPixelSize * 8;
    }

    //-----------------------------------------------------------------------------
    auto Image::getHasAlpha() const noexcept -> bool
    {
        return (PixelUtil::getFlags(mFormat) & PixelFormatFlags::HASALPHA) != PixelFormatFlags{};
    }
    //-----------------------------------------------------------------------------
    void Image::applyGamma( uchar *buffer, Real gamma, size_t size, uchar bpp )
    {
        if( gamma == 1.0f )
            return;

        OgreAssert( bpp == 24 || bpp == 32, "");

        uint stride = bpp >> 3;
        
        uchar gammaramp[256];
        const Real exponent = 1.0f / gamma;
        for(int i = 0; i < 256; i++) {
            gammaramp[i] = static_cast<uchar>(Math::Pow(i/255.0f, exponent)*255+0.5f);
        }

        for( size_t i = 0, j = size / stride; i < j; i++, buffer += stride )
        {
            buffer[0] = gammaramp[buffer[0]];
            buffer[1] = gammaramp[buffer[1]];
            buffer[2] = gammaramp[buffer[2]];
        }
    }
    //-----------------------------------------------------------------------------
    void Image::resize(ushort width, ushort height, Filter filter)
    {
        OgreAssert(mAutoDelete, "resizing dynamic images is not supported");
        OgreAssert(mDepth == 1, "only 2D formats supported");

        // reassign buffer to temp image, make sure auto-delete is true
        Image temp(mFormat, mWidth, mHeight, 1, mBuffer, true);

        // do not delete[] mBuffer!  temp will destroy it
        mBuffer = nullptr;

        // set new dimensions, allocate new buffer
        create(mFormat, width, height); // Loses precomputed mipmaps

        // scale the image from temp into our resized buffer
        Image::scale(temp.getPixelBox(), getPixelBox(), filter);
    }
    //-----------------------------------------------------------------------
    void Image::scale(const PixelBox &src, const PixelBox &scaled, Filter filter) 
    {
        assert(PixelUtil::isAccessible(src.format));
        assert(PixelUtil::isAccessible(scaled.format));
        Image buf; // For auto-delete
        // Assume no intermediate buffer needed
        PixelBox temp = scaled;
        using enum Filter;
        switch (filter) 
        {
        default:
        case NEAREST:
            if(src.format != scaled.format)
            {
                // Allocate temporary buffer of destination size in source format 
                buf.create(src.format, scaled.getWidth(), scaled.getHeight(), scaled.getDepth());
                temp = buf.getPixelBox();
            }
            // super-optimized: no conversion
            switch (PixelUtil::getNumElemBytes(src.format)) 
            {
            case 1: NearestResampler<1>::scale(src, temp); break;
            case 2: NearestResampler<2>::scale(src, temp); break;
            case 3: NearestResampler<3>::scale(src, temp); break;
            case 4: NearestResampler<4>::scale(src, temp); break;
            case 6: NearestResampler<6>::scale(src, temp); break;
            case 8: NearestResampler<8>::scale(src, temp); break;
            case 12: NearestResampler<12>::scale(src, temp); break;
            case 16: NearestResampler<16>::scale(src, temp); break;
            default:
                // never reached
                assert(false);
            }
            if(temp.data != scaled.data)
            {
                // Blit temp buffer
                PixelUtil::bulkPixelConversion(temp, scaled);
            }
            break;

        case BILINEAR:
            switch (src.format) 
            {
                using enum PixelFormat;
            case L8: case R8: case A8: case BYTE_LA:
            case R8G8B8: case B8G8R8:
            case R8G8B8A8: case B8G8R8A8:
            case A8B8G8R8: case A8R8G8B8:
            case X8B8G8R8: case X8R8G8B8:
                if(src.format != scaled.format)
                {
                    // Allocate temp buffer of destination size in source format 
                    buf.create(src.format, scaled.getWidth(), scaled.getHeight(), scaled.getDepth());
                    temp = buf.getPixelBox();
                }
                // super-optimized: byte-oriented math, no conversion
                switch (PixelUtil::getNumElemBytes(src.format)) 
                {
                case 1: LinearResampler_Byte<1>::scale(src, temp); break;
                case 2: LinearResampler_Byte<2>::scale(src, temp); break;
                case 3: LinearResampler_Byte<3>::scale(src, temp); break;
                case 4: LinearResampler_Byte<4>::scale(src, temp); break;
                default:
                    // never reached
                    assert(false);
                }
                if(temp.data != scaled.data)
                {
                    // Blit temp buffer
                    PixelUtil::bulkPixelConversion(temp, scaled);
                }
                break;
            case FLOAT32_RGB:
            case FLOAT32_RGBA:
                if (scaled.format == FLOAT32_RGB || scaled.format == FLOAT32_RGBA)
                {
                    // float32 to float32, avoid unpack/repack overhead
                    LinearResampler_Float32::scale(src, scaled);
                    break;
                }
                // else, fall through
            default:
                // non-optimized: floating-point math, performs conversion but always works
                LinearResampler::scale(src, scaled);
            }
            break;
        }
    }

    //-----------------------------------------------------------------------------    

    auto Image::getColourAt(uint32 x, uint32 y, uint32 z) const -> ColourValue
    {
        ColourValue rval;
        PixelUtil::unpackColour(&rval, mFormat, getData(x, y, z));
        return rval;
    }

    //-----------------------------------------------------------------------------    
    
    void Image::setColourAt(ColourValue const &cv, uint32 x, uint32 y, uint32 z)
    {
        PixelUtil::packColour(cv, mFormat, getData(x, y, z));
    }

    //-----------------------------------------------------------------------------    

    auto Image::getPixelBox(uint32 face, TextureMipmap mipmap) const -> PixelBox
    {
        // Image data is arranged as:
        // face 0, top level (mip 0)
        // face 0, mip 1
        // face 0, mip 2
        // face 1, top level (mip 0)
        // face 1, mip 1
        // face 1, mip 2
        // etc
        OgreAssert(mipmap <= getNumMipmaps(), "out of range");
        OgreAssert(face < getNumFaces(), "out of range");
        // Calculate mipmap offset and size
        uint8 *offset = mBuffer;
        // Base offset is number of full faces
        uint32 width = getWidth(), height=getHeight(), depth=getDepth();
        auto numMips = getNumMipmaps();

        // Figure out the offsets 
        size_t fullFaceSize = 0;
        size_t finalFaceSize = 0;
        uint32 finalWidth = 0, finalHeight = 0, finalDepth = 0;
        for(uint32 mip=0; mip <= std::to_underlying(numMips); ++mip)
        {
            if (mip == std::to_underlying(mipmap))
            {
                finalFaceSize = fullFaceSize;
                finalWidth = width;
                finalHeight = height;
                finalDepth = depth;
            }
            fullFaceSize += PixelUtil::getMemorySize(width, height, depth, getFormat());

            /// Half size in each dimension
            if(width!=1) width /= 2;
            if(height!=1) height /= 2;
            if(depth!=1) depth /= 2;
        }
        // Advance pointer by number of full faces, plus mip offset into
        offset += face * fullFaceSize;
        offset += finalFaceSize;
        // Return subface as pixelbox
        PixelBox src(finalWidth, finalHeight, finalDepth, getFormat(), offset);
        return src;
    }
    //-----------------------------------------------------------------------------
    auto Image::calculateSize(TextureMipmap mipmaps, uint32 faces, uint32 width, uint32 height, uint32 depth,
                                PixelFormat format) -> size_t
    {
        size_t size = 0;
        for(uint32 mip=0; mip<=std::to_underlying(mipmaps); ++mip)
        {
            size += PixelUtil::getMemorySize(width, height, depth, format)*faces; 
            if(width!=1) width /= 2;
            if(height!=1) height /= 2;
            if(depth!=1) depth /= 2;
        }
        return size;
    }
    //---------------------------------------------------------------------
    auto Image::loadTwoImagesAsRGBA(std::string_view rgbFilename, std::string_view alphaFilename,
        std::string_view groupName, PixelFormat fmt) -> Image &
    {
        Image rgb, alpha;

        rgb.load(rgbFilename, groupName);
        alpha.load(alphaFilename, groupName);

        return combineTwoImagesAsRGBA(rgb, alpha, fmt);

    }
    //---------------------------------------------------------------------
    auto Image::loadTwoImagesAsRGBA(const DataStreamPtr& rgbStream,
                                      const DataStreamPtr& alphaStream, PixelFormat fmt,
                                      std::string_view rgbType, std::string_view alphaType) -> Image&
    {
        Image rgb, alpha;

        rgb.load(rgbStream, rgbType);
        alpha.load(alphaStream, alphaType);

        return combineTwoImagesAsRGBA(rgb, alpha, fmt);

    }
    //---------------------------------------------------------------------
    auto Image::combineTwoImagesAsRGBA(const Image& rgb, const Image& alpha, PixelFormat fmt) -> Image &
    {
        // the images should be the same size, have the same number of mipmaps
        OgreAssert(rgb.getWidth() == alpha.getWidth() && rgb.getHeight() == alpha.getHeight() &&
                       rgb.getDepth() == alpha.getDepth(),
                   "Images must be the same dimensions");
        OgreAssert(rgb.getNumMipmaps() == alpha.getNumMipmaps() && rgb.getNumFaces() == alpha.getNumFaces(),
                   "Images must have the same number of surfaces");

        // Format check
        OgreAssert(PixelUtil::getComponentCount(fmt) == 4, "Target format must have 4 components");

        OgreAssert(!(PixelUtil::isCompressed(fmt) || PixelUtil::isCompressed(rgb.getFormat()) ||
                     PixelUtil::isCompressed(alpha.getFormat())),
                   "Compressed formats are not supported in this method");

        uint32 numFaces = rgb.getNumFaces();
        create(fmt, rgb.getWidth(), rgb.getHeight(), rgb.getDepth(), numFaces, rgb.getNumMipmaps());

        for (uint32 face = 0; face < numFaces; ++face)
        {
            for (uint8 mip = 0; mip <= std::to_underlying(mNumMipmaps); ++mip)
            {
                // convert the RGB first
                PixelBox srcRGB = rgb.getPixelBox(face, static_cast<TextureMipmap>(mip));
                PixelBox dst = getPixelBox(face, static_cast<TextureMipmap>(mip));
                PixelUtil::bulkPixelConversion(srcRGB, dst);

                // now selectively add the alpha
                PixelBox srcAlpha = alpha.getPixelBox(face, static_cast<TextureMipmap>(mip));
                uchar* psrcAlpha = srcAlpha.data;
                uchar* pdst = dst.data;
                for (uint32 d = 0; d < mDepth; ++d)
                {
                    for (uint32 y = 0; y < mHeight; ++y)
                    {
                        for (uint32 x = 0; x < mWidth; ++x)
                        {
                            ColourValue colRGBA, colA;
                            // read RGB back from dest to save having another pointer
                            PixelUtil::unpackColour(&colRGBA, mFormat, pdst);
                            PixelUtil::unpackColour(&colA, alpha.getFormat(), psrcAlpha);

                            // combine RGB from alpha source texture
                            colRGBA.a = (colA.r + colA.g + colA.b) / 3.0f;

                            PixelUtil::packColour(colRGBA, mFormat, pdst);
                            
                            psrcAlpha += PixelUtil::getNumElemBytes(alpha.getFormat());
                            pdst += PixelUtil::getNumElemBytes(mFormat);
                        }
                    }
                }
            }
        }

        return *this;
    }
    //---------------------------------------------------------------------

    
}
