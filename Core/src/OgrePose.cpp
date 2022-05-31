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
#include <cassert>
#include <cstring>
#include <utility>

#include "OgreException.hpp"
#include "OgreHardwareBuffer.hpp"
#include "OgreHardwareBufferManager.hpp"
#include "OgreHardwareVertexBuffer.hpp"
#include "OgrePose.hpp"
#include "OgreVertexIndexData.hpp"

namespace Ogre {
    //---------------------------------------------------------------------
    Pose::Pose(ushort target, String  name)
        : mTarget(target), mName(std::move(name))
    {
    }
    //---------------------------------------------------------------------
    void Pose::addVertex(size_t index, const Vector3& offset)
    {
        OgreAssert(mNormalsMap.empty(),
                   "Inconsistent calls to addVertex, must include normals always or never");

        if(offset.squaredLength() < 1e-6f)
        {
            return;
        }

        mVertexOffsetMap[index] = offset;
        mBuffer.reset();
    }
    //---------------------------------------------------------------------
    void Pose::addVertex(size_t index, const Vector3& offset, const Vector3& normal)
    {
        OgreAssert(mVertexOffsetMap.empty() || !mNormalsMap.empty(),
                   "Inconsistent calls to addVertex, must include normals always or never");

        if(offset.squaredLength() < 1e-6f && normal.squaredLength() < 1e-6f)
        {
            return;
        }

        mVertexOffsetMap[index] = offset;
        mNormalsMap[index] = normal;
        mBuffer.reset();
    }
    //---------------------------------------------------------------------
    void Pose::removeVertex(size_t index)
    {
        auto i = mVertexOffsetMap.find(index);
        if (i != mVertexOffsetMap.end())
        {
            mVertexOffsetMap.erase(i);
            mBuffer.reset();
        }
        auto j = mNormalsMap.find(index);
        if (j != mNormalsMap.end())
        {
            mNormalsMap.erase(j);
        }
    }
    //---------------------------------------------------------------------
    void Pose::clearVertices()
    {
        mVertexOffsetMap.clear();
        mNormalsMap.clear();
        mBuffer.reset();
    }

    //---------------------------------------------------------------------
    const HardwareVertexBufferSharedPtr& Pose::_getHardwareVertexBuffer(const VertexData* origData) const
    {
        size_t numVertices = origData->vertexCount;
        
        if (!mBuffer)
        {
            // Create buffer
            size_t vertexSize = VertexElement::getTypeSize(VET_FLOAT3);
            bool normals = getIncludesNormals();
            if (normals)
                vertexSize += VertexElement::getTypeSize(VET_FLOAT3);
                
            mBuffer = HardwareBufferManager::getSingleton().createVertexBuffer(
                vertexSize, numVertices, HardwareBuffer::HBU_STATIC_WRITE_ONLY);

            HardwareBufferLockGuard bufLock(mBuffer, HardwareBuffer::HBL_DISCARD);
            auto* pFloat = static_cast<float*>(bufLock.pData);
            // initialise - these will be the values used where no pose vertex is included
            memset(pFloat, 0, mBuffer->getSizeInBytes()); 
            if (normals)
            {
                // zeroes are fine for positions (deltas), but for normals we need the original
                // mesh normals, since delta normals don't work (re-normalisation would
                // always result in a blended normal even with full pose applied)
                const VertexElement* origNormElem = 
                    origData->vertexDeclaration->findElementBySemantic(VES_NORMAL, 0);
                assert(origNormElem);
                
                const HardwareVertexBufferSharedPtr& origBuffer = 
                    origData->vertexBufferBinding->getBuffer(origNormElem->getSource());
                HardwareBufferLockGuard origBufLock(origBuffer, HardwareBuffer::HBL_READ_ONLY);
                float* pDst = pFloat + 3;
                float* pSrc;
                origNormElem->baseVertexPointerToElement(origBufLock.pData, &pSrc);
                for (size_t v = 0; v < numVertices; ++v)
                {
                    memcpy(pDst, pSrc, sizeof(float)*3);
                    
                    pDst += 6;
                    pSrc = (float*)(((char*)pSrc) + origBuffer->getVertexSize());
                }
            }
            // Set each vertex
            auto v = mVertexOffsetMap.begin();
            auto n = mNormalsMap.begin();
            
            size_t numFloatsPerVertex = normals ? 6: 3;
            
            while(v != mVertexOffsetMap.end())
            {
                // Remember, vertex maps are *sparse* so may have missing entries
                // This is why we skip
                float* pDst = pFloat + (numFloatsPerVertex * v->first);
                *pDst++ = v->second.x;
                *pDst++ = v->second.y;
                *pDst++ = v->second.z;
                ++v;
                if (normals)
                {
                    *pDst++ = n->second.x;
                    *pDst++ = n->second.y;
                    *pDst++ = n->second.z;
                    ++n;
                }
                
            }
        }
        return mBuffer;
    }
    //---------------------------------------------------------------------
    Pose* Pose::clone() const
    {
        Pose* newPose = new Pose(mTarget, mName);
        newPose->mVertexOffsetMap = mVertexOffsetMap;
        newPose->mNormalsMap = mNormalsMap;
        // Allow buffer to recreate itself, contents may change anyway
        return newPose;
    }

}

