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

import :AxisAlignedBox;
import :Bitwise;
import :DataStream;
import :Deflate;
import :Exception;
import :Log;
import :LogManager;
import :Math;
import :Matrix3;
import :Matrix4;
import :Node;
import :Plane;
import :Quaternion;
import :Ray;
import :Sphere;
import :StreamSerialiser;
import :StringConverter;
import :Vector;

import <format>;
import <string>;

namespace Ogre
{
    /** General hash function, derived from here
    http://www.azillionmonkeys.com/qed/hash.html
    Original by Paul Hsieh
    */
    static auto SuperFastHash (const char * data, int len, uint32 hashSoFar) -> uint32
    {
#  define OGRE_GET16BITS(d) (*((const uint16 *) (d)))
        uint32 hash;
        int rem;

        if (hashSoFar)
            hash = hashSoFar;
        else
            hash = len;

        if (len <= 0 || data == nullptr) return 0;

        rem = len & 3;
        len >>= 2;

        /* Main loop */
        for (;len > 0; len--) {
            hash  += OGRE_GET16BITS (data);
            uint32 tmp    = (OGRE_GET16BITS (data+2) << 11) ^ hash;
            hash   = (hash << 16) ^ tmp;
            data  += 2*sizeof (uint16);
            hash  += hash >> 11;
        }

        /* Handle end cases */
        switch (rem) {
        case 3: hash += OGRE_GET16BITS (data);
            hash ^= hash << 16;
            hash ^= data[sizeof (uint16)] << 18;
            hash += hash >> 11;
            break;
        case 2: hash += OGRE_GET16BITS (data);
            hash ^= hash << 11;
            hash += hash >> 17;
            break;
        case 1: hash += *data;
            hash ^= hash << 10;
            hash += hash >> 1;
        }

        /* Force "avalanching" of final 127 bits */
        hash ^= hash << 3;
        hash += hash >> 5;
        hash ^= hash << 4;
        hash += hash >> 17;
        hash ^= hash << 25;
        hash += hash >> 6;

        return hash;
#undef OGRE_GET16BITS
    }

    //---------------------------------------------------------------------
    uint32 constinit StreamSerialiser::HEADER_ID = 0x00000001;
    uint32 constinit StreamSerialiser::REVERSE_HEADER_ID = 0x10000000;
    uint32 constinit StreamSerialiser::CHUNK_HEADER_SIZE = 
        sizeof(uint32) + // id
        sizeof(uint16) + // version
        sizeof(uint32) + // length
        sizeof(uint32); // checksum
    //---------------------------------------------------------------------
    StreamSerialiser::StreamSerialiser(const DataStreamPtr& stream, Endian endianMode, 
        bool autoHeader, RealStorageFormat realFormat)
        : mStream(stream)
        , mEndian(endianMode)
        , 
         mReadWriteHeader(autoHeader)
        , mRealFormat(realFormat)
    {
        if (mEndian != Endian::AUTO)
        {
            if (mEndian == Endian::BIG)
                mFlipEndian = true;
        }

        OgreAssert(mStream, "Stream is null");
    }
    //---------------------------------------------------------------------
    StreamSerialiser::~StreamSerialiser()
    {
        // really this should be empty if read/write was complete, but be tidy
        if (!mChunkStack.empty())
        {
            LogManager::getSingleton().stream(LogMessageLevel::Warning) <<
                "Warning: stream " << mStream->getName() << " was not fully read / written; " <<
                mChunkStack.size() << " chunks remain unterminated.";
        }
    }
    //---------------------------------------------------------------------
    auto StreamSerialiser::makeIdentifier(const char (&code)[5]) -> uint32
    {
        uint32 ret = 0;
        for (size_t i = 0; i < 4; ++i)
        {
            ret += (code[i] << (i * 8));
        }
        return ret;

    }
    //---------------------------------------------------------------------
    auto StreamSerialiser::getCurrentChunkID() const noexcept -> uint32
    {
        if (mChunkStack.empty())
            return 0;
        else
            return mChunkStack.back()->id;
    }
    //---------------------------------------------------------------------
    auto StreamSerialiser::readChunkBegin() -> const StreamSerialiser::Chunk*
    {
        // Have we figured out the endian mode yet?
        if (mReadWriteHeader)
            readHeader();

        OgreAssert(mEndian != Endian::AUTO,
                   "Endian mode has not been determined, did you disable header without setting?");

        Chunk* chunk = readChunkImpl();
        mChunkStack.emplace_back(chunk);

        return chunk;

    }
    //---------------------------------------------------------------------
    auto StreamSerialiser::readChunkBegin(
        uint32 id, uint16 maxVersion, std::string_view msg) -> const StreamSerialiser::Chunk*
    {
        const Chunk* c = readChunkBegin();
        if (c->id != id)
        {
            // rewind
            undoReadChunk(c->id);
            return nullptr;
        }
        else if (c->version > maxVersion)
        {
            LogManager::getSingleton().stream(LogMessageLevel::Critical) << "Error: " << msg
                << " : Data version is " << c->version << " but this software can only read "
                << "up to version " << maxVersion;
            // skip
            readChunkEnd(c->id);
            return nullptr;
        }

        return c;

    }
    //---------------------------------------------------------------------
    void StreamSerialiser::undoReadChunk(uint32 id)
    {
        Chunk* c = popChunk(id);

        OgreAssert(mStream, "Stream is null");

        mStream->seek(c->offset);

        delete c;

    }
    //---------------------------------------------------------------------
    auto StreamSerialiser::peekNextChunkID() -> uint32
    {
        OgreAssert(mStream, "Stream is null");

        if (eof())
            return 0;

        // Have we figured out the endian mode yet?
        if (mReadWriteHeader)
            readHeader();

        OgreAssert(mEndian != Endian::AUTO,
                   "Endian mode has not been determined, did you disable header without setting?");

        size_t homePos = mStream->tell();
        uint32 ret;
        read(&ret);
        mStream->seek(homePos);

        return ret;
    }
    //---------------------------------------------------------------------
    void StreamSerialiser::readChunkEnd(uint32 id)
    {
        Chunk* c = popChunk(id);

        OgreAssert(mStream, "Stream is null");

        // skip to the end of the chunk if we were not there already
        // this lets us quite reading a chunk anywhere and have the read marker
        // automatically skip to the next one
        if (mStream->tell() < (c->offset + CHUNK_HEADER_SIZE + c->length))
            mStream->seek(c->offset + CHUNK_HEADER_SIZE + c->length);

        delete c;
    }
    //---------------------------------------------------------------------
    auto StreamSerialiser::isEndOfChunk(uint32 id) -> bool
    {
        const Chunk* c = getCurrentChunk();
        assert(c->id == id);
        return mStream->tell() == (c->offset + CHUNK_HEADER_SIZE + c->length);
    }
    //---------------------------------------------------------------------
    void StreamSerialiser::readHeader()
    {
        uint32 headerid;
        size_t actually_read = mStream->read(&headerid, sizeof(uint32));
        // skip back
        mStream->skip(0 - (long)actually_read);
        // validate that this is a header chunk
        if (headerid == REVERSE_HEADER_ID)
        {
            mFlipEndian = true;
        }
        else if (headerid == HEADER_ID)
        {
            mFlipEndian = false;
        }
        else
        {
            // no good
            OGRE_EXCEPT(ExceptionCodes::INVALID_STATE, 
                "Cannot determine endian mode because header is missing", 
                "StreamSerialiser::readHeader");
        }
        determineEndianness();

        mReadWriteHeader = false;

        const Chunk* c = readChunkBegin();
        // endian should be flipped now
        assert(c->id == HEADER_ID);
        (void)c; // Silence warning

        // read real storage format
        bool realIsDouble;
        read(&realIsDouble);
        mRealFormat = realIsDouble? RealStorageFormat::DOUBLE : RealStorageFormat::FLOAT;

        readChunkEnd(HEADER_ID);

    }
    //---------------------------------------------------------------------
    void StreamSerialiser::determineEndianness()
    {
        if (mFlipEndian)
            mEndian = Endian::BIG;
        else
            mEndian = Endian::LITTLE;
    }
    //---------------------------------------------------------------------
    auto StreamSerialiser::getCurrentChunk() const noexcept -> const StreamSerialiser::Chunk*
    {
        if (mChunkStack.empty())
            return nullptr;
        else
            return mChunkStack.back().get();
    }
    //---------------------------------------------------------------------
    auto StreamSerialiser::eof() const -> bool
    {
        OgreAssert(mStream, "Stream is null");
        return mStream->eof(); 
    }
    //---------------------------------------------------------------------
    void StreamSerialiser::checkStream(bool failOnEof, bool validateReadable, bool validateWriteable) const
    {
        OgreAssert(mStream, "Stream is null");

        if (failOnEof && mStream->eof())
            OGRE_EXCEPT(ExceptionCodes::INVALID_STATE, "Invalid operation, end of file on stream");

        if (validateReadable && !mStream->isReadable())
            OGRE_EXCEPT(ExceptionCodes::INVALID_STATE, "Invalid operation, file is not readable");

        if (validateWriteable && !mStream->isWriteable())
            OGRE_EXCEPT(ExceptionCodes::INVALID_STATE, "Invalid operation, file is not writeable");
    }
    //---------------------------------------------------------------------
    void StreamSerialiser::writeHeader()
    {
        if (mEndian == Endian::AUTO)
            determineEndianness();

        // Header chunk has zero data size
        writeChunkImpl(HEADER_ID, 1);

        // real format
        bool realIsDouble = (mRealFormat == RealStorageFormat::DOUBLE);
        write(&realIsDouble);

        writeChunkEnd(HEADER_ID);

        mReadWriteHeader = false;
    }
    //---------------------------------------------------------------------
    void StreamSerialiser::writeChunkBegin(uint32 id, uint16 version /* = 1 */)
    {
        checkStream(false, false, true);

        if (mReadWriteHeader)
            writeHeader();

        OgreAssert(mEndian != Endian::AUTO,
                   "Endian mode has not been determined, did you disable header without setting?");

        writeChunkImpl(id, version);

    }
    //---------------------------------------------------------------------
    void StreamSerialiser::writeChunkEnd(uint32 id)
    {
        checkStream(false, false, true);

        Chunk* c = popChunk(id);

        // update the sizes
        size_t currPos = mStream->tell();
        c->length = static_cast<uint32>(currPos - c->offset - CHUNK_HEADER_SIZE);

        // seek to 'length' position in stream for this chunk
        // skip id (32) and version (16)
        mStream->seek(c->offset + sizeof(uint32) + sizeof(uint16));
        write(&c->length);
        // write updated checksum
        uint32 checksum = calculateChecksum(c);
        write(&checksum);

        // seek back to previous position
        mStream->seek(currPos);

        delete c;

    }
    //---------------------------------------------------------------------
    auto StreamSerialiser::getOffsetFromChunkStart() const -> size_t
    {
        OgreAssert(mStream, "Stream is null");

        if (mChunkStack.empty())
        {
            return 0;
        }
        else
        {
            size_t pos = mStream->tell();
            size_t diff = pos - mChunkStack.back()->offset;
            if(diff >= CHUNK_HEADER_SIZE)
                return diff - CHUNK_HEADER_SIZE;
            else
                return 0; // not in a chunk?

        }

    }
    //---------------------------------------------------------------------
    auto StreamSerialiser::readChunkImpl() -> StreamSerialiser::Chunk*
    {
        auto *chunk = new Chunk();
        chunk->offset = static_cast<uint32>(mStream->tell());
        read(&chunk->id);
        read(&chunk->version);
        read(&chunk->length);
        
        uint32 checksum;
        read(&checksum);
        
        if (checksum != calculateChecksum(chunk))
        {
            // no good, this is an invalid chunk
            uint32 off = chunk->offset;
            delete chunk;
            OGRE_EXCEPT(ExceptionCodes::INVALID_STATE, 
                ::std::format("Corrupt chunk detected in stream {} at byte {}", mStream->getName(), off),
                "StreamSerialiser::readChunkImpl");
        }
        else
        {
            return chunk;
        }


    }
    //---------------------------------------------------------------------
    void StreamSerialiser::writeChunkImpl(uint32 id, uint16 version)
    {
        mChunkStack.push_back(::std::make_unique<Chunk>());

        auto* c = mChunkStack.back().get();
        c->id = id;
        c->version = version;
        c->offset = static_cast<uint32>(mStream->tell());
        c->length = 0;

        write(&c->id);
        write(&c->version);
        write(&c->length);
        // write length again, this is just a placeholder for the checksum (to come later)
        write(&c->length);

    }
    //---------------------------------------------------------------------
    void StreamSerialiser::writeData(const void* buf, size_t size, size_t count)
    {
        checkStream(false, false, true);

        size_t totSize = size * count;
        if (mFlipEndian)
        {
            char* pToWrite = new char[totSize];
            memcpy(pToWrite, buf, totSize);

			Bitwise::bswapChunks(pToWrite, size, count);
            mStream->write(pToWrite, totSize);

            delete[] pToWrite;
        }
        else
        {
            mStream->write(buf, totSize);
        }

    }
    //---------------------------------------------------------------------
    void StreamSerialiser::write(const Vector2* vec, size_t count)
    {
        for (size_t i = 0; i < count; ++i, ++vec)
            write(vec->ptr(), 2);
    }
    //---------------------------------------------------------------------
    void StreamSerialiser::write(const Vector3* vec, size_t count)
    {
        for (size_t i = 0; i < count; ++i, ++vec)
            write(vec->ptr(), 3);
    }
    //---------------------------------------------------------------------
    void StreamSerialiser::write(const Vector4* vec, size_t count)
    {
        for (size_t i = 0; i < count; ++i, ++vec)
            write(vec->ptr(), 4);
    }
    //---------------------------------------------------------------------
    void StreamSerialiser::write(const Quaternion* q, size_t count)
    {
        for (size_t i = 0; i < count; ++i, ++q)
            write(q->ptr(), 4);
    }
    //---------------------------------------------------------------------
    void StreamSerialiser::write(const String* string)
    {
        // uint32 of size first, then (unterminated) string
        auto len = static_cast<uint32>(string->length());
        write(&len);
        mStream->write(string->c_str(), len);
    }
    //---------------------------------------------------------------------
    void StreamSerialiser::write(const Matrix3* m, size_t count)
    {
        for (size_t i = 0; i < count; ++i, ++m)
            write((*m)[0], 9);
    }
    //---------------------------------------------------------------------
    void StreamSerialiser::write(const Matrix4* m, size_t count)
    {
        for (size_t i = 0; i < count; ++i, ++m)
            write((*m)[0], 12);
    }
    //---------------------------------------------------------------------
    void StreamSerialiser::write(const AxisAlignedBox* aabb, size_t count)
    {
        for (size_t i = 0; i < count; ++i, ++aabb)
        {
            bool infinite = aabb->isInfinite();
            write(&infinite);
            write(&aabb->getMinimum());
            write(&aabb->getMaximum());
        }       
    }
    //---------------------------------------------------------------------
    void StreamSerialiser::write(const Sphere* sphere, size_t count)
    {

        for (size_t i = 0; i < count; ++i, ++sphere)
        {
            write(&sphere->getCenter());
            Real radius = sphere->getRadius();
            write(&radius);
        }
    }
    //---------------------------------------------------------------------
    void StreamSerialiser::write(const Plane* plane, size_t count)
    {
        for (size_t i = 0; i < count; ++i, ++plane)
        {
            write(&plane->normal);
            write(&plane->d);
        }
    }
    //---------------------------------------------------------------------
    void StreamSerialiser::write(const Ray* ray, size_t count)
    {
        for (size_t i = 0; i < count; ++i, ++ray)
        {
            write(&ray->getOrigin());
            write(&ray->getDirection());
        }
    }
    //---------------------------------------------------------------------
    void StreamSerialiser::write(const Radian* rad, size_t count)
    {
        for (size_t i = 0; i < count; ++i, ++rad)
        {
            Real r = rad->valueRadians();
            write(&r);
        }
    }
    //---------------------------------------------------------------------
    void StreamSerialiser::write(const Node* node, size_t count)
    {
        for (size_t i = 0; i < count; ++i, ++node)
        {
            write(&node->getPosition());
            write(&node->getOrientation());
            write(&node->getScale());
        }
    }
    //---------------------------------------------------------------------
    void StreamSerialiser::write(const bool* val, size_t count)
    {
        for (size_t i = 0; i < count; ++i, ++val)
        {
            char c = (*val)? 1 : 0;
            write(&c);
        }
    }
    //---------------------------------------------------------------------
    void StreamSerialiser::write(const Real* val, size_t count)
    {
        if (mRealFormat == RealStorageFormat::FLOAT)
            writeData(val, sizeof(float), count);
        else
            writeFloatsAsDoubles(val, count);
    }
    //---------------------------------------------------------------------
    void StreamSerialiser::readData(void* buf, size_t size, size_t count)
    {
        checkStream(true, true, false);

        size_t totSize = size * count;
        mStream->read(buf, totSize);

        if (mFlipEndian)
			Bitwise::bswapChunks(buf, size, count);

    }
    //---------------------------------------------------------------------
    void StreamSerialiser::read(Vector2* vec, size_t count)
    {
        for (size_t i = 0; i < count; ++i, ++vec)
        {
            read(vec->ptr(), 2);
        }
    }
    //---------------------------------------------------------------------
    void StreamSerialiser::read(Vector3* vec, size_t count)
    {
        for (size_t i = 0; i < count; ++i, ++vec)
        {
            read(vec->ptr(), 3);
        }
    }
    //---------------------------------------------------------------------
    void StreamSerialiser::read(Vector4* vec, size_t count)
    {
        for (size_t i = 0; i < count; ++i, ++vec)
        {
            read(vec->ptr(), 4);
        }
    }
    //---------------------------------------------------------------------
    void StreamSerialiser::read(Quaternion* q, size_t count)
    {
        for (size_t i = 0; i < count; ++i, ++q)
        {
            read(q->ptr(), 4);
        }
    }
    //---------------------------------------------------------------------
    void StreamSerialiser::read(Matrix3* m, size_t count)
    {
        for (size_t i = 0; i < count; ++i, ++m)
        {
            read((*m)[0], 9);
        }
    }
    //---------------------------------------------------------------------
    void StreamSerialiser::read(Matrix4* m, size_t count)
    {
        for (size_t i = 0; i < count; ++i, ++m)
        {
            read((*m)[0], 12);
        }
    }
    //---------------------------------------------------------------------
    void StreamSerialiser::read(String* string)
    {
        // String is stored as a uint32 character count, then string
        uint32 len;
        read(&len);
        string->resize(len);
        if (len)
            read(&(*string->begin()), len);
    }
    //---------------------------------------------------------------------
    void StreamSerialiser::read(Real* val, size_t count)
    {
        if (mRealFormat == RealStorageFormat::FLOAT)
            readData(val, sizeof(float), count);
        else
            readDoublesAsFloats(val, count);

    }
    //---------------------------------------------------------------------
    void StreamSerialiser::read(AxisAlignedBox* aabb, size_t count)
    {
        for (size_t i = 0; i < count; ++i, ++aabb)
        {
            bool infinite = false;
            read(&infinite);
            Vector3 tmpMin, tmpMax;
            read(&tmpMin);
            read(&tmpMax);

            if (infinite)
                aabb->setInfinite();
            else
                aabb->setExtents(tmpMin, tmpMax);
        }
    }
    //---------------------------------------------------------------------
    void StreamSerialiser::read(Sphere* sphere, size_t count)
    {
        for (size_t i = 0; i < count; ++i, ++sphere)
        {
            Vector3 center;
            Real radius;
            read(&center);
            read(&radius);
            sphere->setCenter(center);
            sphere->setRadius(radius);
        }
    }
    //---------------------------------------------------------------------
    void StreamSerialiser::read(Plane* plane, size_t count)
    {
        for (size_t i = 0; i < count; ++i, ++plane)
        {
            read(&plane->normal);
            read(&plane->d);
        }
    }
    //---------------------------------------------------------------------
    void StreamSerialiser::read(Ray* ray, size_t count)
    {
        for (size_t i = 0; i < count; ++i, ++ray)
        {
            Vector3 origin, dir;
            read(&origin);
            read(&dir);
            ray->setOrigin(origin);
            ray->setDirection(dir);
        }
    }
    //---------------------------------------------------------------------
    void StreamSerialiser::read(Radian* angle, size_t count)
    {
        for (size_t i = 0; i < count; ++i, ++angle)
        {
            Real rads;
            read(&rads);
            *angle = Radian{rads};
        }
    }
    //---------------------------------------------------------------------
    void StreamSerialiser::read(Node* node, size_t count)
    {
        for (size_t i = 0; i < count; ++i, ++node)
        {
            Vector3 pos, scale;
            Quaternion orient;
            read(&pos);
            read(&orient);
            read(&scale);
            node->setPosition(pos);
            node->setOrientation(orient);
            node->setScale(scale);
        }
    }
    //---------------------------------------------------------------------
    void StreamSerialiser::read(bool* val, size_t count)
    {
        for (size_t i = 0; i < count; ++i, ++val)
        {
            char c;
            read(&c);
            *val = (c == 1);
        }
    }
    //---------------------------------------------------------------------
    void StreamSerialiser::writeFloatsAsDoubles(const float* val, size_t count)
    {
        double t = 0;
        writeConverted(val, t, count);
    }
    //---------------------------------------------------------------------
    void StreamSerialiser::writeDoublesAsFloats(const double* val, size_t count)
    {
        float t = 0;
        writeConverted(val, t, count);
    }
    //---------------------------------------------------------------------
    void StreamSerialiser::readFloatsAsDoubles(double* val, size_t count)
    {
        float t = 0;
        readConverted(val, t, count);
    }
    //---------------------------------------------------------------------
    void StreamSerialiser::readDoublesAsFloats(float* val, size_t count)
    {
        double t = 0;
        readConverted(val, t, count);
    }
    //---------------------------------------------------------------------
    auto StreamSerialiser::calculateChecksum(Chunk* c) -> uint32
    {
        // Always calculate checksums in little endian to make sure they match 
        // Otherwise checksums for the same data on different endians will not match
        uint32 id = c->id;
        uint16 version = c->version;
        uint32 length = c->length;

        // we cannot switch to murmur3 like everywhere else to allow loading legacy files
        uint32 hashVal = SuperFastHash((const char*)&id, sizeof(uint32), 0);
        hashVal = SuperFastHash((const char*)&version, sizeof(uint16), hashVal);
        hashVal = SuperFastHash((const char*)&length, sizeof(uint32), hashVal);

        return hashVal;
    }
    //---------------------------------------------------------------------
    auto StreamSerialiser::popChunk(uint id) -> StreamSerialiser::Chunk*
    {
        OgreAssert(!mChunkStack.empty(), "No active chunk!");

        const Chunk* chunk = mChunkStack.back().get();
        OgreAssert(chunk->id == id, "Incorrect chunk id!");

        Chunk* c = mChunkStack.back().release();
        mChunkStack.pop_back();
        return c;

    }
    void StreamSerialiser::startDeflate(size_t avail_in)
    {
        OgreAssert( !mOriginalStream , "Don't start (un)compressing twice!" );
        DataStreamPtr deflateStream(new DeflateStream(mStream,"",avail_in));
        mOriginalStream = mStream;
        mStream = deflateStream;
    }

    void StreamSerialiser::stopDeflate()
    {
        OgreAssert( mOriginalStream , "Must start (un)compressing first!" );
        mStream = mOriginalStream;
        mOriginalStream.reset();
    }
}
