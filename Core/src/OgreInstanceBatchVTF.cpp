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

#include <cstring>

module Ogre.Core;

import :Common;
import :DualQuaternion;
import :Exception;
import :HardwareBuffer;
import :HardwareBufferManager;
import :HardwareIndexBuffer;
import :HardwarePixelBuffer;
import :HardwareVertexBuffer;
import :InstanceBatchVTF;
import :InstancedEntity;
import :Material;
import :MaterialManager;
import :Pass;
import :PixelFormat;
import :RenderOperation;
import :RenderSystem;
import :RenderSystemCapabilities;
import :Root;
import :SceneManager;
import :StringConverter;
import :SubMesh;
import :Technique;
import :TextureManager;
import :TextureUnitState;
import :Vector;
import :VertexIndexData;

import <algorithm>;
import <limits>;
import <map>;
import <memory>;
import <string>;
import <type_traits>;
import <utility>;

namespace Ogre
{
    static const uint16 constexpr c_maxTexWidth   = 4096;
    static const uint16 constexpr c_maxTexHeight  = 4096;

    BaseInstanceBatchVTF::BaseInstanceBatchVTF( InstanceManager *creator, MeshPtr &meshReference,
                                        const MaterialPtr &material, size_t instancesPerBatch,
                                        const Mesh::IndexMap *indexToBoneMap, std::string_view batchName) :
                InstanceBatch( creator, meshReference, material, instancesPerBatch,
                                indexToBoneMap, batchName ),
                
                mNumWorldMatrices( instancesPerBatch ),
                
                mMaxFloatsPerLine( std::numeric_limits<size_t>::max() )
                
    {
        cloneMaterial( mMaterial );
    }

    BaseInstanceBatchVTF::~BaseInstanceBatchVTF()
    {
        //Remove cloned caster materials (if any)
        for (auto technique : mMaterial->getTechniques())
        {
            if (technique->getShadowCasterMaterial())
                MaterialManager::getSingleton().remove( technique->getShadowCasterMaterial() );
        }

        //Remove cloned material
        MaterialManager::getSingleton().remove( mMaterial );

        //Remove the VTF texture
        if( mMatrixTexture )
            TextureManager::getSingleton().remove( mMatrixTexture );
    }

    //-----------------------------------------------------------------------
    void BaseInstanceBatchVTF::buildFrom( const SubMesh *baseSubMesh, const RenderOperation &renderOperation )
    {
        if (useBoneMatrixLookup())
        {
            //when using bone matrix lookup resource are not shared
            //
            //Future implementation: while the instance vertex buffer can't be shared
            //The texture can be.
            //
            build(baseSubMesh);
        }
        else
        {
            createVertexTexture( baseSubMesh );
            InstanceBatch::buildFrom( baseSubMesh, renderOperation );
        }
    }
    //-----------------------------------------------------------------------
    void BaseInstanceBatchVTF::cloneMaterial( const MaterialPtr &material )
    {
        //Used to track down shadow casters, so the same material caster doesn't get cloned twice
        using MatMap = std::map<std::string_view, MaterialPtr>;
        MatMap clonedMaterials;

        //We need to clone the material so we can have different textures for each batch.
        mMaterial = material->clone( ::std::format("{}/VTFMaterial", mName) );

        //Now do the same with the techniques which have a material shadow caster
        for (Technique *technique : material->getTechniques())
        {
            if( technique->getShadowCasterMaterial() )
            {
                const MaterialPtr &casterMat    = technique->getShadowCasterMaterial();
                std::string_view casterName        = casterMat->getName();

                //Was this material already cloned?
                auto itor = clonedMaterials.find(casterName);

                if( itor == clonedMaterials.end() )
                {
                    //No? Clone it and track it
                    MaterialPtr cloned = casterMat->clone(::std::format("{}/VTFMaterialCaster{}", mName,
                                                    clonedMaterials.size()));
                    technique->setShadowCasterMaterial( cloned );
                    clonedMaterials[casterName] = cloned;
                }
                else
                    technique->setShadowCasterMaterial( itor->second ); //Reuse the previously cloned mat
            }
        }
    }
    //-----------------------------------------------------------------------
    void BaseInstanceBatchVTF::retrieveBoneIdx( VertexData *baseVertexData, HWBoneIdxVec &outBoneIdx )
    {
        const VertexElement *ve = baseVertexData->vertexDeclaration->
                                                            findElementBySemantic( VertexElementSemantic::BLEND_INDICES );
        const VertexElement *veWeights = baseVertexData->vertexDeclaration->findElementBySemantic( VertexElementSemantic::BLEND_WEIGHTS );
        
        HardwareVertexBufferSharedPtr buff = baseVertexData->vertexBufferBinding->getBuffer(ve->getSource());
        HardwareBufferLockGuard baseVertexLock(buff, HardwareBuffer::LockOptions::READ_ONLY);
        char const *baseBuffer = static_cast<char const*>(baseVertexLock.pData);

        for( size_t i=0; i<baseVertexData->vertexCount; ++i )
        {
            auto const *pWeights = reinterpret_cast<float const*>(baseBuffer + veWeights->getOffset());

            uint8 biggestWeightIdx = 0;
            for( uint8 j=1; j< uint8(mWeightCount); ++j )
            {
                biggestWeightIdx = pWeights[biggestWeightIdx] < pWeights[j] ? j : biggestWeightIdx;
            }

            auto const *pIndex = reinterpret_cast<uint8 const*>(baseBuffer + ve->getOffset());
            outBoneIdx[i] = pIndex[biggestWeightIdx];

            baseBuffer += baseVertexData->vertexDeclaration->getVertexSize(ve->getSource());
        }
    }

    //-----------------------------------------------------------------------
    void BaseInstanceBatchVTF::retrieveBoneIdxWithWeights(VertexData *baseVertexData, HWBoneIdxVec &outBoneIdx, HWBoneWgtVec &outBoneWgt)
    {
        const VertexElement *ve = baseVertexData->vertexDeclaration->findElementBySemantic( VertexElementSemantic::BLEND_INDICES );
        const VertexElement *veWeights = baseVertexData->vertexDeclaration->findElementBySemantic( VertexElementSemantic::BLEND_WEIGHTS );
        
        HardwareVertexBufferSharedPtr buff = baseVertexData->vertexBufferBinding->getBuffer(ve->getSource());
        HardwareBufferLockGuard baseVertexLock(buff, HardwareBuffer::LockOptions::READ_ONLY);
        char const *baseBuffer = static_cast<char const*>(baseVertexLock.pData);

        for( size_t i=0; i<baseVertexData->vertexCount * mWeightCount; i += mWeightCount)
        {
            auto const *pWeights = reinterpret_cast<float const*>(baseBuffer + veWeights->getOffset());
            auto const *pIndex = reinterpret_cast<uint8 const*>(baseBuffer + ve->getOffset());

            float weightMagnitude = 0.0f;
            for( size_t j=0; j < mWeightCount; ++j )
            {
                outBoneWgt[i+j] = pWeights[j];
                weightMagnitude += pWeights[j];
                outBoneIdx[i+j] = pIndex[j];
            }

            //Normalize the bone weights so they add to one
            for(size_t j=0; j < mWeightCount; ++j)
            {
                outBoneWgt[i+j] /= weightMagnitude;
            }

            baseBuffer += baseVertexData->vertexDeclaration->getVertexSize(ve->getSource());
        }
    }
    
    //-----------------------------------------------------------------------
    void BaseInstanceBatchVTF::setupMaterialToUseVTF( TextureType textureType, MaterialPtr &material ) const
    {
        for (auto technique : material->getTechniques())
        {
            for (auto pass : technique->getPasses())
            {
                for (auto texUnit : pass->getTextureUnitStates())
                {
                    if( texUnit->getName() == "InstancingVTF" )
                    {
                        texUnit->setTextureName( mMatrixTexture->getName(), textureType );
                        texUnit->setTextureFiltering( TextureFilterOptions::NONE );
                    }
                }
            }

            if( technique->getShadowCasterMaterial() )
            {
                MaterialPtr matCaster = technique->getShadowCasterMaterial();
                setupMaterialToUseVTF( textureType, matCaster );
            }
        }
    }
    //-----------------------------------------------------------------------
    void BaseInstanceBatchVTF::createVertexTexture( const SubMesh* baseSubMesh )
    {
        /*
        TODO: Find a way to retrieve max texture resolution,
        http://www.ogre3d.org/forums/viewtopic.php?t=38305

        Currently assuming it's 4096x4096, which is a safe bet for any hardware with decent VTF*/
        
        size_t uniqueAnimations = mInstancesPerBatch;
        if (useBoneMatrixLookup())
        {
            uniqueAnimations = std::min<size_t>(getMaxLookupTableInstances(), uniqueAnimations);
        }
        mMatricesPerInstance = std::max<size_t>( 1, baseSubMesh->blendIndexToBoneIndexMap.size() );

        if(mUseBoneDualQuaternions && !mTempTransformsArray3x4)
        {
            mTempTransformsArray3x4 = ::std::make_unique<Matrix3x4f[]>(mMatricesPerInstance);
        }
        
        mNumWorldMatrices = uniqueAnimations * mMatricesPerInstance;

        //Calculate the width & height required to hold all the matrices. Start by filling the width
        //first (i.e. 4096x1 4096x2 4096x3, etc)
        
        size_t texWidth         = std::min<size_t>( mNumWorldMatrices * mRowLength, c_maxTexWidth );
        size_t maxUsableWidth   = texWidth;
        if( matricesTogetherPerRow() )
        {
            //The technique requires all matrices from the same instance in the same row
            //i.e. 4094 -> 4095 -> skip 4096 -> 0 (next row) contains data from a new instance 
            mWidthFloatsPadding = texWidth % (mMatricesPerInstance * mRowLength);

            if( mWidthFloatsPadding )
            {
                mMaxFloatsPerLine = texWidth - mWidthFloatsPadding;

                maxUsableWidth = mMaxFloatsPerLine;

                //Values are in pixels, convert them to floats (1 pixel = 4 floats)
                mWidthFloatsPadding *= 4;
                mMaxFloatsPerLine       *= 4;
            }
        }

        size_t texHeight = mNumWorldMatrices * mRowLength / maxUsableWidth;

        if( (mNumWorldMatrices * mRowLength) % maxUsableWidth )
            texHeight += 1;

        //Don't use 1D textures, as OGL goes crazy because the shader should be calling texture1D()...
        TextureType texType = TextureType::_2D;

        mMatrixTexture = TextureManager::getSingleton().createManual(
                                        ::std::format("{}/VTF", mName), mMeshReference->getGroup(), texType,
                                        (uint)texWidth, (uint)texHeight,
                                        TextureMipmap{}, PixelFormat::FLOAT32_RGBA, TextureUsage::DYNAMIC_WRITE_ONLY_DISCARDABLE );

        OgreAssert(mMatrixTexture->getFormat() == PixelFormat::FLOAT32_RGBA, "float texture support required");
        //Set our cloned material to use this custom texture!
        setupMaterialToUseVTF( texType, mMaterial );
    }

    //-----------------------------------------------------------------------
    auto BaseInstanceBatchVTF::convert3x4MatricesToDualQuaternions(Matrix3x4f* matrices, size_t numOfMatrices, float* outDualQuaternions) -> size_t
    {
        DualQuaternion dQuat;
        size_t floatsWritten = 0;

        for (size_t m = 0; m < numOfMatrices; ++m)
        {
            dQuat.fromTransformationMatrix(Affine3::FromPtr(matrices[m][0]));
            
            //Copy the 2x4 matrix
            for(int i = 0; i < 8; ++i)
            {
                *outDualQuaternions++ = static_cast<float>( dQuat[i] );
                ++floatsWritten;
            }
        }

        return floatsWritten;
    }
    
    //-----------------------------------------------------------------------
    void BaseInstanceBatchVTF::updateVertexTexture()
    {
        //Now lock the texture and copy the 4x3 matrices!
        HardwareBufferLockGuard matTexLock(mMatrixTexture->getBuffer(), HardwareBuffer::LockOptions::DISCARD);
        const PixelBox &pixelBox = mMatrixTexture->getBuffer()->getCurrentLock();

        auto *pDest = reinterpret_cast<float*>(pixelBox.data);

        Matrix3x4f* transforms;

        //If using dual quaternion skinning, write the transforms to a temporary buffer,
        //then convert to dual quaternions, then later write to the pixel buffer
        //Otherwise simply write the transforms to the pixel buffer directly
        if(mUseBoneDualQuaternions)
        {
            transforms = mTempTransformsArray3x4.get();
        }
        else
        {
            transforms = (Matrix3x4f*)pDest;
        }

        
        for (auto const& itor : mInstancedEntities)
        {
            size_t floatsWritten = itor->getTransforms3x4( transforms );

            if( mManager->getCameraRelativeRendering() )
                makeMatrixCameraRelative3x4( transforms, floatsWritten / 12 );

            if(mUseBoneDualQuaternions)
            {
                floatsWritten = convert3x4MatricesToDualQuaternions(transforms, floatsWritten / 12, pDest);
                pDest += floatsWritten;
            }
            else
            {
                transforms += floatsWritten / 12;
            }
        }
    }
    /** update the lookup numbers for entities with shared transforms */
    void BaseInstanceBatchVTF::updateSharedLookupIndexes()
    {
        if (mTransformSharingDirty)
        {
            if (useBoneMatrixLookup())
            {
                //In each entity update the "transform lookup number" so that:
                // 1. All entities sharing the same transformation will share the same unique number
                // 2. "transform lookup number" will be numbered from 0 up to getMaxLookupTableInstances
                uint16 lookupCounter = 0;
                using MapTransformId = std::map<Affine3 *, uint16>;
                MapTransformId transformToId;
                for(auto const& itEnt : mInstancedEntities)
                {
                    if (itEnt->isInScene())
                    {
                        Affine3* transformUniqueId = itEnt->mBoneMatrices;
                        auto itLu = transformToId.find(transformUniqueId);
                        if (itLu == transformToId.end())
                        {
                            itLu = transformToId.insert(std::make_pair(transformUniqueId,lookupCounter)).first;
                            ++lookupCounter;
                        }
                        itEnt->setTransformLookupNumber(itLu->second);
                    }
                    else 
                    {
                        itEnt->setTransformLookupNumber(0);
                    }
                }

                if (lookupCounter > getMaxLookupTableInstances())
                {
                    OGRE_EXCEPT(ExceptionCodes::INVALID_STATE,"Number of unique bone matrix states exceeds current limitation.","BaseInstanceBatchVTF::updateSharedLookupIndexes()");
                }
            }

            mTransformSharingDirty = false;
        }
    }

    //-----------------------------------------------------------------------
    auto BaseInstanceBatchVTF::generateInstancedEntity(size_t num) -> InstancedEntity*
    {
        InstancedEntity* sharedTransformEntity = nullptr;
        if ((useBoneMatrixLookup()) && (num >= getMaxLookupTableInstances()))
        {
            sharedTransformEntity = mInstancedEntities[num % getMaxLookupTableInstances()].get();
            if (sharedTransformEntity->mSharedTransformEntity)
            {
                sharedTransformEntity = sharedTransformEntity->mSharedTransformEntity;
            }
        }

        return new InstancedEntity(this, static_cast<uint32>(num), sharedTransformEntity);
    }


    //-----------------------------------------------------------------------
    void BaseInstanceBatchVTF::getWorldTransforms( Matrix4* xform ) const
    {
        *xform = Matrix4::IDENTITY;
    }
    //-----------------------------------------------------------------------
    auto BaseInstanceBatchVTF::getNumWorldTransforms() const noexcept -> unsigned short
    {
        return 1;
    }
    //-----------------------------------------------------------------------
    void BaseInstanceBatchVTF::_updateRenderQueue(RenderQueue* queue)
    {
        InstanceBatch::_updateRenderQueue( queue );

        if( mBoundsUpdated || mDirtyAnimation || mManager->getCameraRelativeRendering() )
            updateVertexTexture();

        mBoundsUpdated = false;
    }
    //-----------------------------------------------------------------------
    // InstanceBatchVTF
    //-----------------------------------------------------------------------
    InstanceBatchVTF::InstanceBatchVTF( 
        InstanceManager *creator, MeshPtr &meshReference, 
        const MaterialPtr &material, size_t instancesPerBatch, 
        const Mesh::IndexMap *indexToBoneMap, std::string_view batchName )
            : BaseInstanceBatchVTF (creator, meshReference, material, 
                                    instancesPerBatch, indexToBoneMap, batchName)
    {

    }
    //-----------------------------------------------------------------------
    InstanceBatchVTF::~InstanceBatchVTF()
    = default;   
    //-----------------------------------------------------------------------
    void InstanceBatchVTF::setupVertices( const SubMesh* baseSubMesh )
    {
        mRenderOperation.vertexData = new VertexData();
        mRemoveOwnVertexData = true; //Raise flag to remove our own vertex data in the end (not always needed)

        VertexData *thisVertexData = mRenderOperation.vertexData;
        VertexData *baseVertexData = baseSubMesh->vertexData.get();

        thisVertexData->vertexStart = 0;
        thisVertexData->vertexCount = baseVertexData->vertexCount * mInstancesPerBatch;

        HardwareBufferManager::getSingleton().destroyVertexDeclaration( thisVertexData->vertexDeclaration );
        thisVertexData->vertexDeclaration = baseVertexData->vertexDeclaration->clone();

        HWBoneIdxVec hwBoneIdx;
        HWBoneWgtVec hwBoneWgt;

        //Blend weights may not be present because HW_VTF does not require to be skeletally animated
        const VertexElement *veWeights = baseVertexData->vertexDeclaration->
                                                    findElementBySemantic( VertexElementSemantic::BLEND_WEIGHTS );
        if( veWeights )
        {
            //One weight is recommended for VTF
            mWeightCount = (forceOneWeight() || useOneWeight()) ?
                                1 : veWeights->getSize() / sizeof(float);
        }
        else
        {
            mWeightCount = 1;
        }

        hwBoneIdx.resize( baseVertexData->vertexCount * mWeightCount, 0 );

        if( mMeshReference->hasSkeleton() && mMeshReference->getSkeleton() )
        {
            if(mWeightCount > 1)
            {
                hwBoneWgt.resize( baseVertexData->vertexCount * mWeightCount, 0 );
                retrieveBoneIdxWithWeights(baseVertexData, hwBoneIdx, hwBoneWgt);
            }
            else
            {
                retrieveBoneIdx( baseVertexData, hwBoneIdx );
                thisVertexData->vertexDeclaration->removeElement( VertexElementSemantic::BLEND_INDICES );
                thisVertexData->vertexDeclaration->removeElement( VertexElementSemantic::BLEND_WEIGHTS );

                thisVertexData->vertexDeclaration->closeGapsInSource();
            }

        }

        for( unsigned short i=0; i<thisVertexData->vertexDeclaration->getMaxSource()+1; ++i )
        {
            //Create our own vertex buffer
            HardwareVertexBufferSharedPtr vertexBuffer =
                HardwareBufferManager::getSingleton().createVertexBuffer(
                thisVertexData->vertexDeclaration->getVertexSize(i),
                thisVertexData->vertexCount,
                HardwareBuffer::STATIC_WRITE_ONLY );
            thisVertexData->vertexBufferBinding->setBinding( i, vertexBuffer );

            //Grab the base submesh data
            HardwareVertexBufferSharedPtr baseVertexBuffer =
                baseVertexData->vertexBufferBinding->getBuffer(i);

            HardwareBufferLockGuard thisLock(vertexBuffer, HardwareBuffer::LockOptions::DISCARD);
            HardwareBufferLockGuard baseLock(baseVertexBuffer, HardwareBuffer::LockOptions::READ_ONLY);
            char* thisBuf = static_cast<char*>(thisLock.pData);
            char* baseBuf = static_cast<char*>(baseLock.pData);

            //Copy and repeat
            for( size_t j=0; j<mInstancesPerBatch; ++j )
            {
                const size_t sizeOfBuffer = baseVertexData->vertexCount *
                    baseVertexData->vertexDeclaration->getVertexSize(i);
                memcpy( thisBuf + j * sizeOfBuffer, baseBuf, sizeOfBuffer );
            }
        }

        createVertexTexture( baseSubMesh );
        createVertexSemantics( thisVertexData, baseVertexData, hwBoneIdx, hwBoneWgt);
    }
    //-----------------------------------------------------------------------
    void InstanceBatchVTF::setupIndices( const SubMesh* baseSubMesh )
    {
        mRenderOperation.indexData = new IndexData();
        mRemoveOwnIndexData = true; //Raise flag to remove our own index data in the end (not always needed)

        IndexData *thisIndexData = mRenderOperation.indexData;
        IndexData *baseIndexData = baseSubMesh->indexData.get();

        thisIndexData->indexStart = 0;
        thisIndexData->indexCount = baseIndexData->indexCount * mInstancesPerBatch;

        //TODO: Check numVertices is below max supported by GPU
        HardwareIndexBuffer::IndexType indexType = HardwareIndexBuffer::IndexType::_16BIT;
        if( mRenderOperation.vertexData->vertexCount > 65535 )
            indexType = HardwareIndexBuffer::IndexType::_32BIT;
        thisIndexData->indexBuffer = HardwareBufferManager::getSingleton().createIndexBuffer(
            indexType, thisIndexData->indexCount, HardwareBuffer::STATIC_WRITE_ONLY );

        HardwareBufferLockGuard thisLock(thisIndexData->indexBuffer, HardwareBuffer::LockOptions::DISCARD);
        HardwareBufferLockGuard baseLock(baseIndexData->indexBuffer, HardwareBuffer::LockOptions::READ_ONLY);
        auto *thisBuf16 = static_cast<uint16*>(thisLock.pData);
        auto *thisBuf32 = static_cast<uint32*>(thisLock.pData);
        bool baseIndex16bit = baseIndexData->indexBuffer->getType() == HardwareIndexBuffer::IndexType::_16BIT;

        for( size_t i=0; i<mInstancesPerBatch; ++i )
        {
            const size_t vertexOffset = i * mRenderOperation.vertexData->vertexCount / mInstancesPerBatch;

            const auto *initBuf16 = static_cast<const uint16 *>(baseLock.pData);
            const auto *initBuf32 = static_cast<const uint32 *>(baseLock.pData);

            for( size_t j=0; j<baseIndexData->indexCount; ++j )
            {
                uint32 originalVal = baseIndex16bit ? *initBuf16++ : *initBuf32++;

                if( indexType == HardwareIndexBuffer::IndexType::_16BIT )
                    *thisBuf16++ = static_cast<uint16>(originalVal + vertexOffset);
                else
                    *thisBuf32++ = static_cast<uint32>(originalVal + vertexOffset);
            }
        }
    }
    //-----------------------------------------------------------------------
    void InstanceBatchVTF::createVertexSemantics( 
        VertexData *thisVertexData, VertexData *baseVertexData, const HWBoneIdxVec &hwBoneIdx, const HWBoneWgtVec &hwBoneWgt)
    {
        const size_t texWidth  = mMatrixTexture->getWidth();
        const size_t texHeight = mMatrixTexture->getHeight();

        //Calculate the texel offsets to correct them offline
        //Akwardly enough, the offset is needed in OpenGL too
        Vector2 texelOffsets;
        //RenderSystem *renderSystem = Root::getSingleton().getRenderSystem();
        texelOffsets.x = /*renderSystem->getHorizontalTexelOffset()*/ -0.5f / (float)texWidth;
        texelOffsets.y = /*renderSystem->getVerticalTexelOffset()*/ -0.5f / (float)texHeight;

        //Only one weight per vertex is supported. It would not only be complex, but prohibitively slow.
        //Put them in a new buffer, since it's 32 bytes aligned :-)
        const unsigned short newSource = thisVertexData->vertexDeclaration->getMaxSource() + 1;
        size_t maxFloatsPerVector = 4;
        size_t offset = 0;

        for(size_t i = 0; i < mWeightCount; i += maxFloatsPerVector / mRowLength)
        {
            offset += thisVertexData->vertexDeclaration->addElement( newSource, offset, VertexElementType::FLOAT4, VertexElementSemantic::TEXTURE_COORDINATES,
                thisVertexData->vertexDeclaration->
                getNextFreeTextureCoordinate() ).getSize();
            offset += thisVertexData->vertexDeclaration->addElement( newSource, offset, VertexElementType::FLOAT4, VertexElementSemantic::TEXTURE_COORDINATES,
                thisVertexData->vertexDeclaration->
                getNextFreeTextureCoordinate() ).getSize();
        }

        //Add the weights (supports up to four, which is Ogre's limit)
        if(mWeightCount > 1)
        {
            thisVertexData->vertexDeclaration->addElement(newSource, offset, VertexElementType::FLOAT4, VertexElementSemantic::BLEND_WEIGHTS,
                                        thisVertexData->vertexDeclaration->getNextFreeTextureCoordinate() );
        }

        //Create our own vertex buffer
        HardwareVertexBufferSharedPtr vertexBuffer =
            HardwareBufferManager::getSingleton().createVertexBuffer(
            thisVertexData->vertexDeclaration->getVertexSize(newSource),
            thisVertexData->vertexCount,
            HardwareBuffer::STATIC_WRITE_ONLY );
        thisVertexData->vertexBufferBinding->setBinding( newSource, vertexBuffer );

        HardwareBufferLockGuard vertexLock(vertexBuffer, HardwareBuffer::LockOptions::DISCARD);
        auto *thisFloat = static_cast<float*>(vertexLock.pData);
        
        //Copy and repeat
        for( size_t i=0; i<mInstancesPerBatch; ++i )
        {
            for( size_t j=0; j<baseVertexData->vertexCount * mWeightCount; j += mWeightCount )
            {
                size_t numberOfMatricesInLine = 0;

                for(size_t wgtIdx = 0; wgtIdx < mWeightCount; ++wgtIdx)
                {
                    for( size_t k=0; k < mRowLength; ++k)
                    {
                        size_t instanceIdx = (hwBoneIdx[j+wgtIdx] + i * mMatricesPerInstance) * mRowLength + k;
                        //x
                        *thisFloat++ = ((instanceIdx % texWidth) / (float)texWidth) - (float)texelOffsets.x;
                        //y
                        *thisFloat++ = ((instanceIdx / texWidth) / (float)texHeight) - (float)texelOffsets.y;
                    }

                    ++numberOfMatricesInLine;

                    //If another matrix can't be fit, we're on another line, or if this is the last weight
                    if((numberOfMatricesInLine + 1) * mRowLength > maxFloatsPerVector || (wgtIdx+1) == mWeightCount)
                    {
                        //Place zeroes in the remaining coordinates
                        for ( size_t k=mRowLength * numberOfMatricesInLine; k < maxFloatsPerVector; ++k)
                        {
                            *thisFloat++ = 0.0f;
                            *thisFloat++ = 0.0f;
                        }

                        numberOfMatricesInLine = 0;
                    }
                }

                //Don't need to write weights if there is only one
                if(mWeightCount > 1)
                {
                    //Write the weights
                    for(size_t wgtIdx = 0; wgtIdx < mWeightCount; ++wgtIdx)
                    {
                        *thisFloat++ = hwBoneWgt[j+wgtIdx];
                    }

                    //Fill the rest of the line with zeros
                    for(size_t wgtIdx = mWeightCount; wgtIdx < maxFloatsPerVector; ++wgtIdx)
                    {
                        *thisFloat++ = 0.0f;
                    }
                }
            }
        }
    }
    //-----------------------------------------------------------------------
    auto InstanceBatchVTF::calculateMaxNumInstances( 
                    const SubMesh *baseSubMesh, InstanceManagerFlags flags ) const -> size_t
    {
        size_t retVal = 0;

        RenderSystem *renderSystem = Root::getSingleton().getRenderSystem();
        const RenderSystemCapabilities *capabilities = renderSystem->getCapabilities();

        //VTF must be supported
        if( capabilities->hasCapability( Capabilities::VERTEX_TEXTURE_FETCH ) )
        {
            //TODO: Check PixelFormat::FLOAT32_RGBA is supported (should be, since it was the 1st one)
            const size_t numBones = std::max<size_t>( 1, baseSubMesh->blendIndexToBoneIndexMap.size() );
            retVal = c_maxTexWidth * c_maxTexHeight / mRowLength / numBones;

            if(!!(flags & InstanceManagerFlags::USE16BIT))
            {
                if( baseSubMesh->vertexData->vertexCount * retVal > 0xFFFF )
                    retVal = 0xFFFF / baseSubMesh->vertexData->vertexCount;
            }

            if(!!(flags & InstanceManagerFlags::VTFBESTFIT))
            {
                const size_t instancesPerBatch = std::min( retVal, mInstancesPerBatch );
                //Do the same as in createVertexTexture()
                const size_t numWorldMatrices = instancesPerBatch * numBones;

                size_t texWidth  = std::min<size_t>( numWorldMatrices * mRowLength, c_maxTexWidth );
                size_t texHeight = numWorldMatrices * mRowLength / c_maxTexWidth;

                const size_t remainder = (numWorldMatrices * mRowLength) % c_maxTexWidth;

                if( remainder && texHeight > 0 )
                    retVal = static_cast<size_t>(texWidth * texHeight / (float)mRowLength / (float)(numBones));
            }
        }

        return retVal;

    }
}
