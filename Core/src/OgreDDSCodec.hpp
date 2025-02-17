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

#include <cstddef>

module Ogre.Core:DDSCodec;

import :ImageCodec;
import :PixelFormat;
import :Platform;
import :Prerequisites;

namespace Ogre {
    /** \addtogroup Core
    *  @{
    */
    /** \addtogroup Image
    *  @{
    */

    // Forward declarations
    struct DXTColourBlock;
    struct DXTExplicitAlphaBlock;
    struct DXTInterpolatedAlphaBlock;
struct ColourValue;

    /** Codec specialized in loading DDS (Direct Draw Surface) images.
    @remarks
        We implement our own codec here since we need to be able to keep DXT
        data compressed if the card supports it.
    */
    class DDSCodec : public ImageCodec
    {
    private:
        String mType;

        [[nodiscard]] auto convertFourCCFormat(uint32 fourcc) const -> PixelFormat;
        [[nodiscard]] auto convertDXToOgreFormat(uint32 fourcc) const -> PixelFormat;
        [[nodiscard]] auto convertPixelFormat(uint32 rgbBits, uint32 rMask,
            uint32 gMask, uint32 bMask, uint32 aMask) const -> PixelFormat;

        /// Unpack DXT colours into array of 16 colour values
        void unpackDXTColour(PixelFormat pf, const DXTColourBlock& block, ColourValue* pCol) const;
        /// Unpack DXT alphas into array of 16 colour values
        void unpackDXTAlpha(const DXTExplicitAlphaBlock& block, ColourValue* pCol) const;
        /// Unpack DXT alphas into array of 16 colour values
        void unpackDXTAlpha(const DXTInterpolatedAlphaBlock& block, ColourValue* pCol) const;

        /// Single registered codec instance
        static DDSCodec* msInstance;
    public:
        DDSCodec();
        ~DDSCodec() override = default;

        using ImageCodec::decode;
        using ImageCodec::encode;
        using ImageCodec::encodeToFile;

        void encodeToFile(const MemoryDataStreamPtr& input, std::string_view outFileName, const CodecDataPtr& pData) const override;
        [[nodiscard]] auto decode(const DataStreamPtr& input) const -> DecodeResult override;
        auto magicNumberToFileExt(const char *magicNumberPtr, size_t maxbytes) const -> std::string_view override;
        [[nodiscard]] auto getType() const -> std::string_view override;

        /// Static method to startup and register the DDS codec
        static void startup();
        /// Static method to shutdown and unregister the DDS codec
        static void shutdown();

    };
    /** @} */
    /** @} */

} // namespace
