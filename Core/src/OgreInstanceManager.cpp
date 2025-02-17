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

import :Exception;
import :HardwareBuffer;
import :HardwareBufferManager;
import :HardwareIndexBuffer;
import :HardwareVertexBuffer;
import :InstanceBatch;
import :InstanceBatchHW;
import :InstanceBatchHW_VTF;
import :InstanceBatchShader;
import :InstanceBatchVTF;
import :InstanceManager;
import :InstancedEntity;
import :MaterialManager;
import :Mesh;
import :MeshManager;
import :SceneManager;
import :SceneNode;
import :StringConverter;
import :SubMesh;
import :VertexBoneAssignment;
import :VertexIndexData;

import <algorithm>;
import <memory>;
import <utility>;

namespace Ogre
{
    InstanceManager::InstanceManager( std::string_view customName, SceneManager *sceneManager,
                                        std::string_view meshName, std::string_view groupName,
                                        InstancingTechnique instancingTechnique, InstanceManagerFlags instancingFlags,
                                        size_t instancesPerBatch, unsigned short subMeshIdx, bool useBoneMatrixLookup ) :
                mName(customName),
                mInstancesPerBatch( instancesPerBatch ),
                mInstancingTechnique( instancingTechnique ),
                mInstancingFlags( instancingFlags ),
                mSubMeshIdx( subMeshIdx ),
                mSceneManager( sceneManager )
                
    {
        mMeshReference = MeshManager::getSingleton().load( meshName, groupName );

        if(mMeshReference->sharedVertexData)
            unshareVertices(mMeshReference);

        if( mMeshReference->hasSkeleton() && mMeshReference->getSkeleton() )
            mMeshReference->getSubMesh(mSubMeshIdx)->_compileBoneAssignments();
    }
    //----------------------------------------------------------------------
    void InstanceManager::setInstancesPerBatch( size_t instancesPerBatch )
    {
        OgreAssert(mInstanceBatches.empty(), "can only be changed before building the batch");
        mInstancesPerBatch = instancesPerBatch;
    }

    //----------------------------------------------------------------------
    void InstanceManager::setMaxLookupTableInstances( size_t maxLookupTableInstances )
    {
        OgreAssert(mInstanceBatches.empty(), "can only be changed before building the batch");
        mMaxLookupTableInstances = maxLookupTableInstances;
    }
    
    //----------------------------------------------------------------------
    void InstanceManager::setNumCustomParams( unsigned char numCustomParams )
    {
        OgreAssert(mInstanceBatches.empty(), "can only be changed before building the batch");
        mNumCustomParams = numCustomParams;
    }
    //----------------------------------------------------------------------
    auto InstanceManager::getMaxOrBestNumInstancesPerBatch( std::string_view materialName, size_t suggestedSize,
                                                                InstanceManagerFlags flags ) -> size_t
    {
        //Get the material
        MaterialPtr mat = MaterialManager::getSingleton().getByName( materialName,
                                                                    mMeshReference->getGroup() );
        InstanceBatch *batch = nullptr;

        //Base material couldn't be found
        if( !mat )
            return 0;

        switch( mInstancingTechnique )
        {
        case ShaderBased:
            batch = new InstanceBatchShader( this, mMeshReference, mat, suggestedSize,
                                                    nullptr, ::std::format("{}/TempBatch", mName) );
            break;
        case TextureVTF:
            batch = new InstanceBatchVTF( this, mMeshReference, mat, suggestedSize,
                                                    nullptr, ::std::format("{}/TempBatch", mName) );
            static_cast<InstanceBatchVTF*>(batch)->setBoneDualQuaternions(!!(mInstancingFlags & InstanceManagerFlags::USEBONEDUALQUATERNIONS));
            static_cast<InstanceBatchVTF*>(batch)->setUseOneWeight(!!(mInstancingFlags & InstanceManagerFlags::USEONEWEIGHT));
            static_cast<InstanceBatchVTF*>(batch)->setForceOneWeight(!!(mInstancingFlags & InstanceManagerFlags::FORCEONEWEIGHT));
            break;
        case HWInstancingBasic:
            batch = new InstanceBatchHW( this, mMeshReference, mat, suggestedSize,
                                                    nullptr, ::std::format("{}/TempBatch", mName) );
            break;
        case HWInstancingVTF:
            batch = new InstanceBatchHW_VTF( this, mMeshReference, mat, suggestedSize,
                                                    nullptr, ::std::format("{}/TempBatch", mName) );
            static_cast<InstanceBatchHW_VTF*>(batch)->setBoneMatrixLookup(!!(mInstancingFlags & InstanceManagerFlags::VTFBONEMATRIXLOOKUP), mMaxLookupTableInstances);
            static_cast<InstanceBatchHW_VTF*>(batch)->setBoneDualQuaternions(!!(mInstancingFlags & InstanceManagerFlags::USEBONEDUALQUATERNIONS));
            static_cast<InstanceBatchHW_VTF*>(batch)->setUseOneWeight(!!(mInstancingFlags & InstanceManagerFlags::USEONEWEIGHT));
            static_cast<InstanceBatchHW_VTF*>(batch)->setForceOneWeight(!!(mInstancingFlags & InstanceManagerFlags::FORCEONEWEIGHT));
            break;
        default:
            OGRE_EXCEPT(ExceptionCodes::NOT_IMPLEMENTED,
                    ::std::format("Unimplemented instancing technique: {}", mInstancingTechnique),
                    "InstanceBatch::getMaxOrBestNumInstancesPerBatches()");
        }

        const size_t retVal = batch->calculateMaxNumInstances( mMeshReference->getSubMesh(mSubMeshIdx),
                                                                flags );

        delete batch;

        return retVal;
    }
    //----------------------------------------------------------------------
    auto InstanceManager::createInstancedEntity( std::string_view materialName ) -> InstancedEntity*
    {
        InstanceBatch *instanceBatch;

        if( mInstanceBatches.empty() ||
            (mInstanceBatches[materialName].size () == 0))
            instanceBatch = buildNewBatch( materialName, true );
        else
            instanceBatch = getFreeBatch( materialName );

        return instanceBatch->createInstancedEntity();
    }
    //-----------------------------------------------------------------------
    inline auto InstanceManager::getFreeBatch( std::string_view materialName ) -> InstanceBatch*
    {
        auto& batchVec = mInstanceBatches[materialName];

        for (auto const& itor : batchVec)
        {
            if( !itor->isBatchFull() )
                return itor.get();
        }

        //None found, or they're all full
        return buildNewBatch( materialName, false );
    }
    //-----------------------------------------------------------------------
    auto InstanceManager::buildNewBatch( std::string_view materialName, bool firstTime ) -> InstanceBatch*
    {
        //Get the bone to index map for the batches
        Mesh::IndexMap &idxMap = mMeshReference->getSubMesh(mSubMeshIdx)->blendIndexToBoneIndexMap;
        idxMap = idxMap.empty() ? mMeshReference->sharedBlendIndexToBoneIndexMap : idxMap;

        //Get the material
        MaterialPtr mat = MaterialManager::getSingleton().getByName( materialName,
                                                                    mMeshReference->getGroup() );

        //Get the array of batches grouped by this material
        auto& materialInstanceBatch = mInstanceBatches[materialName];

        ::std::unique_ptr<InstanceBatch> batch = nullptr;

        switch( mInstancingTechnique )
        {
        case ShaderBased:
            batch = ::std::make_unique<InstanceBatchShader>( this, mMeshReference, mat, mInstancesPerBatch,
                                                    &idxMap, ::std::format("{}/InstanceBatch_{}", mName, mIdCount++));
            break;
        case TextureVTF:
            batch = ::std::make_unique<InstanceBatchVTF>( this, mMeshReference, mat, mInstancesPerBatch,
                                                    &idxMap, ::std::format("{}/InstanceBatch_{}", mName, mIdCount++));
            static_cast<InstanceBatchVTF*>(batch.get())->setBoneDualQuaternions(!!(mInstancingFlags & InstanceManagerFlags::USEBONEDUALQUATERNIONS));
            static_cast<InstanceBatchVTF*>(batch.get())->setUseOneWeight(!!(mInstancingFlags & InstanceManagerFlags::USEONEWEIGHT));
            static_cast<InstanceBatchVTF*>(batch.get())->setForceOneWeight(!!(mInstancingFlags & InstanceManagerFlags::FORCEONEWEIGHT));
            break;
        case HWInstancingBasic:
            batch = ::std::make_unique<InstanceBatchHW>( this, mMeshReference, mat, mInstancesPerBatch,
                                                    &idxMap, ::std::format("{}/InstanceBatch_{}", mName, mIdCount++));
            break;
        case HWInstancingVTF:
            batch = ::std::make_unique<InstanceBatchHW_VTF>( this, mMeshReference, mat, mInstancesPerBatch,
                                                    &idxMap, ::std::format("{}/InstanceBatch_{}", mName, mIdCount++));
            static_cast<InstanceBatchHW_VTF*>(batch.get())->setBoneMatrixLookup(!!(mInstancingFlags & InstanceManagerFlags::VTFBONEMATRIXLOOKUP), mMaxLookupTableInstances);
            static_cast<InstanceBatchHW_VTF*>(batch.get())->setBoneDualQuaternions(!!(mInstancingFlags & InstanceManagerFlags::USEBONEDUALQUATERNIONS));
            static_cast<InstanceBatchHW_VTF*>(batch.get())->setUseOneWeight(!!(mInstancingFlags & InstanceManagerFlags::USEONEWEIGHT));
            static_cast<InstanceBatchHW_VTF*>(batch.get())->setForceOneWeight(!!(mInstancingFlags & InstanceManagerFlags::FORCEONEWEIGHT));
            break;
        default:
            OGRE_EXCEPT(ExceptionCodes::NOT_IMPLEMENTED,
                    ::std::format("Unimplemented instancing technique: {}", mInstancingTechnique),
                    "InstanceBatch::buildNewBatch()");
        }

        batch->_notifyManager( mSceneManager );


        if( !firstTime )
        {
            //TODO: Check different materials have the same mInstancesPerBatch upper limit
            //otherwise we can't share
            batch->buildFrom( mMeshReference->getSubMesh(mSubMeshIdx), mSharedRenderOperation );
        }
        else
        {
            //Ensure we don't request more than we can
            const size_t maxInstPerBatch = batch->calculateMaxNumInstances( mMeshReference->
                                                        getSubMesh(mSubMeshIdx), mInstancingFlags );
            mInstancesPerBatch = std::min( maxInstPerBatch, mInstancesPerBatch );
            batch->_setInstancesPerBatch( mInstancesPerBatch );

            OgreAssert(mInstancesPerBatch, "unsupported instancing technique");
            //TODO: Create a "merge" function that merges all submeshes into one big submesh
            //instead of just sending submesh #0

            //Get the RenderOperation to be shared with further instances.
            mSharedRenderOperation = batch->build( mMeshReference->getSubMesh(mSubMeshIdx) );
        }

        const BatchSettings &batchSettings = mBatchSettings[materialName];
        batch->setCastShadows( batchSettings.setting[std::to_underlying(CAST_SHADOWS)] );

        //Batches need to be part of a scene node so that their renderable can be rendered
        SceneNode *sceneNode = mSceneManager->getRootSceneNode()->createChildSceneNode();
        sceneNode->attachObject( batch.get() );
        sceneNode->showBoundingBox( batchSettings.setting[std::to_underlying(SHOW_BOUNDINGBOX)] );

        materialInstanceBatch.push_back( ::std::move(batch) );

        return materialInstanceBatch.back().get();
    }
    //-----------------------------------------------------------------------
    void InstanceManager::cleanupEmptyBatches()
    {
        //Do this now to avoid any dangling pointer inside mDirtyBatches
        _updateDirtyBatches();

        for (auto& [key, value] : mInstanceBatches)
        {
            auto it = value.begin();
            auto en = value.end();

            while( it != en )
            {
                if( (*it)->isBatchUnused() )
                {
                    //Remove it from the list swapping with the last element and popping back
                    size_t idx = it - value.begin();
                    *it = ::std::move(value.back());
                    value.pop_back();

                    //Restore invalidated iterators
                    it = value.begin() + idx;
                    en = value.end();
                }
                else
                    ++it;
            }
        }

        //By this point it may happen that all mInstanceBatches' objects are also empty
        //however if we call mInstanceBatches.clear(), next time we'll create an InstancedObject
        //we'll end up calling buildFirstTime() instead of buildNewBatch(), which is not the idea
        //(takes more time and will leak the shared render operation)
    }
    //-----------------------------------------------------------------------
    void InstanceManager::defragmentBatches( bool optimizeCull,
                                                InstanceBatch::InstancedEntityVec &usedEntities,
                                                InstanceBatch::CustomParamsVec &usedParams,
                                                ::std::vector<::std::unique_ptr<InstanceBatch>> &fragmentedBatches )
    {
        auto itor = fragmentedBatches.begin();
        auto end  = fragmentedBatches.end();

        while( itor != end  )
        {
            if( !(*itor)->isStatic() )
                (*itor)->_defragmentBatch( optimizeCull, usedEntities, usedParams );
            ++itor;

            if  (usedEntities.empty())
                break;
        }

        auto lastImportantBatch = itor;

        while( itor != end )
        {
            if( (*itor)->isStatic() )
            {
                //This isn't a meaningless batch, move it forward so it doesn't get wipe
                //when we resize the container (faster than removing element by element)
                *lastImportantBatch++ = ::std::move(*itor);
            }

            ++itor;
        }

        //Remove remaining batches all at once from the vector
        const size_t remainingBatches = end - lastImportantBatch;
        fragmentedBatches.resize( fragmentedBatches.size() - remainingBatches );
    }
    //-----------------------------------------------------------------------
    void InstanceManager::defragmentBatches( bool optimizeCulling )
    {
        //Do this now to avoid any dangling pointer inside mDirtyBatches
        _updateDirtyBatches();

        //Do this for every material
        for(auto& instanceBatchPair : mInstanceBatches)
        {
            InstanceBatch::InstancedEntityVec   usedEntities;
            InstanceBatch::CustomParamsVec      usedParams;
            usedEntities.reserve( instanceBatchPair.second.size() * mInstancesPerBatch );

            // only 1 allocation required with reserve
            size_t entitiesInUseCount{};
            for(auto& instanceBatch : instanceBatchPair.second)
            {
                //  branchless optimization
                    entitiesInUseCount
                +=  !instanceBatch->isStatic()
                * instanceBatch->getUsedEntityCount()
                ;
            }
            usedEntities.reserve(entitiesInUseCount);

            //Collect all Instanced Entities being used by _all_ batches from this material
            for(auto& instanceBatch : instanceBatchPair.second)
            {
                //Don't collect instances from static batches, we assume they're correctly set
                //Plus, we don't want to put InstancedEntities from non-static into static batches
                if( !instanceBatch->isStatic() )
                    instanceBatch->transferInstancedEntitiesInUse( usedEntities, usedParams );
            }

            defragmentBatches( optimizeCulling, usedEntities, usedParams, instanceBatchPair.second );
        }
    }
    //-----------------------------------------------------------------------
    void InstanceManager::setSetting( BatchSettingId id, bool value, std::string_view materialName )
    {
        assert( id < NUM_SETTINGS );

        if( materialName == BLANKSTRING )
        {
            //Setup all existing materials
            for (auto const& itor : mInstanceBatches)
            {
                mBatchSettings[itor.first].setting[std::to_underlying(id)] = value;
                applySettingToBatches( id, value, itor.second );
            }
        }
        else
        {
            //Setup a given material
            mBatchSettings[materialName].setting[std::to_underlying(id)] = value;

            auto itor = mInstanceBatches.find( materialName );
            //Don't crash or throw if the batch with that material hasn't been created yet
            if( itor != mInstanceBatches.end() )
                applySettingToBatches( id, value, itor->second );
        }
    }
    //-----------------------------------------------------------------------
    auto InstanceManager::getSetting( BatchSettingId id, std::string_view materialName ) const -> bool
    {
        assert( id < NUM_SETTINGS );

        auto itor = mBatchSettings.find( materialName );
        if( itor != mBatchSettings.end() )
            return itor->second.setting[std::to_underlying(id)]; //Return current setting

        //Return default
        return BatchSettings().setting[std::to_underlying(id)];
    }
    auto InstanceManager::hasSettings(std::string_view materialName) const -> bool
    {
        return mBatchSettings.find(materialName) != mBatchSettings.end();
    }
    //-----------------------------------------------------------------------
    void InstanceManager::applySettingToBatches( BatchSettingId id, bool value,
                                                 const ::std::vector<::std::unique_ptr<InstanceBatch>> &container )
    {
        for (auto const& itor : container)
        {
            using enum BatchSettingId;
            switch( id )
            {
            case CAST_SHADOWS:
                itor->setCastShadows( value );
                break;
            case SHOW_BOUNDINGBOX:
                itor->getParentSceneNode()->showBoundingBox( value );
                break;
            default:
                break;
            }
        }
    }
    //-----------------------------------------------------------------------
    void InstanceManager::setBatchesAsStaticAndUpdate( bool bStatic )
    {
        for (auto const& itor : mInstanceBatches)
        {
            for (auto const& it : itor.second)
            {
                it->setStaticAndUpdate( bStatic );
            }
        }
    }
    //-----------------------------------------------------------------------
    void InstanceManager::_addDirtyBatch( InstanceBatch *dirtyBatch )
    {
        if( mDirtyBatches.empty() )
            mSceneManager->_addDirtyInstanceManager( this );

        mDirtyBatches.push_back( dirtyBatch );
    }
    //-----------------------------------------------------------------------
    void InstanceManager::_updateDirtyBatches()
    {
        for (auto const& itor : mDirtyBatches)
        {
            itor->_updateBounds();
        }

        mDirtyBatches.clear();
    }
    //-----------------------------------------------------------------------
    // Helper functions to unshare the vertices
    //-----------------------------------------------------------------------
    using IndicesMap = std::map<uint32, uint32>;

    template< typename TIndexType >
    void collectUsedIndices(IndicesMap& indicesMap, IndexData* idxData)
    {
        HardwareBufferLockGuard indexLock(idxData->indexBuffer,
                                          idxData->indexStart * sizeof(TIndexType),
                                          idxData->indexCount * sizeof(TIndexType),
                                          HardwareBuffer::LockOptions::READ_ONLY);
        auto *data = (TIndexType*)indexLock.pData;

        for (size_t i = 0; i < idxData->indexCount; i++)
        {
            TIndexType index = data[i];
            if (indicesMap.find(index) == indicesMap.end())
            {
                //We need to guarantee that the size is read before an entry is added, hence these are on separate lines.
                auto size = (uint32)(indicesMap.size());
                indicesMap[index] = size;
            }
        }
    }
    //-----------------------------------------------------------------------
    template< typename TIndexType >
    void copyIndexBuffer(IndexData* idxData, const IndicesMap& indicesMap, size_t indexStart)
    {
        size_t start = std::max(indexStart, idxData->indexStart);
        size_t count = idxData->indexCount - (start - idxData->indexStart);

        //We get errors if we try to lock a zero size buffer.
        if (count == 0) {
            return;
        }
        HardwareBufferLockGuard indexLock(idxData->indexBuffer,
                                          start * sizeof(TIndexType),
                                          count * sizeof(TIndexType),
                                          HardwareBuffer::LockOptions::NORMAL);
        auto *data = (TIndexType*)indexLock.pData;

        for (size_t i = 0; i < count; i++)
        {
            //Access data and write on two separate lines, to avoid compiler confusion.
            auto originalPos = data[i];
            auto indexEntry = indicesMap.find(originalPos);
            data[i] = (TIndexType)(indexEntry->second);
        }
    }
    //-----------------------------------------------------------------------
    void InstanceManager::unshareVertices(const Ogre::MeshPtr &mesh)
    {
        // Retrieve data to copy bone assignments
        const Mesh::VertexBoneAssignmentList& boneAssignments = mesh->getBoneAssignments();
        auto it = boneAssignments.begin();
        auto end = boneAssignments.end();
        size_t curVertexOffset = 0;

        // Access shared vertices
        VertexData* sharedVertexData = mesh->sharedVertexData;

        for (size_t subMeshIdx = 0; subMeshIdx < mesh->getNumSubMeshes(); subMeshIdx++)
        {
            SubMesh *subMesh = mesh->getSubMesh(subMeshIdx);

            IndexData *indexData = subMesh->indexData.get();
            HardwareIndexBuffer::IndexType idxType = indexData->indexBuffer->getType();
            IndicesMap indicesMap;
            if (idxType == HardwareIndexBuffer::IndexType::_16BIT) {
                collectUsedIndices<uint16>(indicesMap, indexData);
            } else {
                collectUsedIndices<uint32>(indicesMap, indexData);
            }

            //Also collect indices for all LOD faces.
            for (auto& lodIndex : subMesh->mLodFaceList) {
                //Typically the LOD indices would use the same buffer type as the main index. But we'll check to make extra sure.
                if (lodIndex->indexBuffer->getType() == HardwareIndexBuffer::IndexType::_16BIT) {
                    collectUsedIndices<uint16>(indicesMap, lodIndex);
                } else {
                    collectUsedIndices<uint32>(indicesMap, lodIndex);
                }
            }


            auto *newVertexData = new VertexData();
            newVertexData->vertexCount = indicesMap.size();
            newVertexData->vertexDeclaration = sharedVertexData->vertexDeclaration->clone();

            for (uint16 bufIdx = 0; bufIdx < uint16(sharedVertexData->vertexBufferBinding->getBufferCount()); bufIdx++)
            {
                HardwareVertexBufferSharedPtr sharedVertexBuffer = sharedVertexData->vertexBufferBinding->getBuffer(bufIdx);
                size_t vertexSize = sharedVertexBuffer->getVertexSize();                

                HardwareVertexBufferSharedPtr newVertexBuffer = HardwareBufferManager::getSingleton().createVertexBuffer
                    (vertexSize, newVertexData->vertexCount, sharedVertexBuffer->getUsage(), sharedVertexBuffer->hasShadowBuffer());

                HardwareBufferLockGuard oldLock(sharedVertexBuffer, 0, sharedVertexData->vertexCount * vertexSize, HardwareBuffer::LockOptions::READ_ONLY);
                HardwareBufferLockGuard newLock(newVertexBuffer, 0, newVertexData->vertexCount * vertexSize, HardwareBuffer::LockOptions::NORMAL);

                for (auto const& indIt : indicesMap)
                {
                    memcpy((uint8*)newLock.pData + vertexSize * indIt.second,
                           (uint8*)oldLock.pData + vertexSize * indIt.first, vertexSize);
                }

                newVertexData->vertexBufferBinding->setBinding(bufIdx, newVertexBuffer);
            }

            if (idxType == HardwareIndexBuffer::IndexType::_16BIT)
            {
                copyIndexBuffer<uint16>(indexData, indicesMap, 0);
            }
            else
            {
                copyIndexBuffer<uint32>(indexData, indicesMap, 0);
            }

            //Need to adjust all of the LOD faces too.
            size_t lastIndexEnd = 0;
            for (size_t i = 0; i < subMesh->mLodFaceList.size(); ++i) {
				auto lodIndex = subMesh->mLodFaceList[i];
				//When using "generated" mesh lods, the indices are shared, so that index[n] would overlap with
				//index[n]. We need to take this into account to make sure we don't process data twice (with incorrect
				//data as a result). We check if the index either is the first one, or if it has a different buffer
				//than the previous one we processed. Since the overlap seems to be progressing we only need to keep
				// track of the last used index.
                if (i == 0 || lodIndex->indexBuffer != subMesh->mLodFaceList[i - 1]->indexBuffer) {
                    lastIndexEnd = 0;
                }

                //Typically the LOD indices would use the same buffer type as the main index. But we'll check to make extra sure.
                if (lodIndex->indexBuffer->getType() == HardwareIndexBuffer::IndexType::_16BIT) {
                    copyIndexBuffer<uint16>(lodIndex, indicesMap, lastIndexEnd);
                } else {
                    copyIndexBuffer<uint32>(lodIndex, indicesMap, lastIndexEnd);
                }
                lastIndexEnd = lodIndex->indexStart + lodIndex->indexCount;

			}

            // Store new attributes
            subMesh->useSharedVertices = false;
            subMesh->vertexData.reset(newVertexData);

            // Transfer bone assignments to the submesh
            size_t offset = curVertexOffset + newVertexData->vertexCount;
            for (; it != end; ++it)
            {
                size_t vertexIdx = (*it).first;
                if (vertexIdx > offset)
                    break;

                VertexBoneAssignment boneAssignment = (*it).second;
                boneAssignment.vertexIndex = static_cast<unsigned int>(boneAssignment.vertexIndex - curVertexOffset);
                subMesh->addBoneAssignment(boneAssignment);
            }
            curVertexOffset = newVertexData->vertexCount + 1;
        }

        // Release shared vertex data
        delete mesh->sharedVertexData;
        mesh->sharedVertexData = nullptr;
        mesh->clearBoneAssignments();

        if( mesh->isEdgeListBuilt() )
        {
            mesh->freeEdgeList();
            mesh->buildEdgeList();
        }
    }
    //-----------------------------------------------------------------------
    auto InstanceManager::getInstanceBatchIterator( std::string_view materialName ) const -> InstanceManager::InstanceBatchIterator
    {
        auto it = mInstanceBatches.find( materialName );
        if(it != mInstanceBatches.end())
            return { it->second.begin(), it->second.end() };

        OGRE_EXCEPT(ExceptionCodes::INVALID_STATE, ::std::format("Cannot create instance batch iterator. "
                    "Material {} cannot be found", materialName ));
    }
}
