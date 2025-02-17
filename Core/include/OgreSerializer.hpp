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

export module Ogre.Core:Serializer;

export import :MemoryAllocatorConfig;
export import :Platform;
export import :Prerequisites;
export import :Quaternion;
export import :SharedPtr;

export import <string_view>;

export
namespace Ogre {

    /** \addtogroup Core
    *  @{
    */
    /** \addtogroup General
    *  @{
    */
    /** Generic class for serialising data to / from binary stream-based files.
    @remarks
        This class provides a number of useful methods for exporting / importing data
        from stream-oriented binary files (e.g. .mesh and .skeleton).
    */
    class Serializer : public SerializerAlloc
    {
    public:
        Serializer();
        ~Serializer();

    protected:

        uint32 mCurrentstreamLen;
        DataStreamPtr mStream;
        String mVersion;
        bool mFlipEndian{false}; /// Default to native endian, derive from header

        // Internal methods
        void writeFileHeader();
        void writeChunkHeader(uint16 id, size_t size);
        auto calcChunkHeaderSize() -> size_t;
        auto calcStringSize(std::string_view string) -> size_t;

        void writeFloats(const float* const pfloat, size_t count);
        void writeFloats(const double* const pfloat, size_t count);
        void writeShorts(const uint16* const pShort, size_t count);
        void writeInts(const uint32* const pInt, size_t count); 
        void writeBools(const bool* const pLong, size_t count);
        void writeObject(const Vector3& vec);
        void writeObject(const Quaternion& q);
        
        void writeString(std::string_view string);
        void writeData(const void* const buf, size_t size, size_t count);
        
        void readFileHeader(const DataStreamPtr& stream);
        auto readChunk(const DataStreamPtr& stream) -> unsigned short;
        
        void readBools(const DataStreamPtr& stream, bool* pDest, size_t count);
        void readFloats(const DataStreamPtr& stream, float* pDest, size_t count);
        void readFloats(const DataStreamPtr& stream, double* pDest, size_t count);
        void readShorts(const DataStreamPtr& stream, uint16* pDest, size_t count);
        void readInts(const DataStreamPtr& stream, uint32* pDest, size_t count);
        void readObject(const DataStreamPtr& stream, Vector3& pDest);
        void readObject(const DataStreamPtr& stream, Quaternion& pDest);

        auto readString(const DataStreamPtr& stream) -> String;
        auto readString(const DataStreamPtr& stream, size_t numChars) -> String;
        
        void flipToLittleEndian(void* pData, size_t size, size_t count = 1);
        void flipFromLittleEndian(void* pData, size_t size, size_t count = 1);

        /// Determine the endianness of the incoming stream compared to native
        void determineEndianness(const DataStreamPtr& stream);
        /// Determine the endianness to write with based on option
        void determineEndianness(std::endian requestedEndian);

        void pushInnerChunk(const DataStreamPtr& stream);
        void popInnerChunk(const DataStreamPtr& stream);
        void backpedalChunkHeader(const DataStreamPtr& stream);
    };
    /** @} */
    /** @} */

}
