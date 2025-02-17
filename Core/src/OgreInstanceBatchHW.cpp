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
module Ogre.Core;

import :Exception;
import :HardwareBuffer;
import :HardwareBufferManager;
import :HardwareVertexBuffer;
import :InstanceBatchHW;
import :InstanceManager;
import :InstancedEntity;
import :Matrix4;
import :RenderOperation;
import :RenderQueue;
import :RenderSystem;
import :RenderSystemCapabilities;
import :Root;
import :SceneManager;
import :SharedPtr;
import :StringConverter;
import :SubMesh;
import :VertexIndexData;

import <memory>;
import <string>;
import <vector>;

namespace Ogre
{
    InstanceBatchHW::InstanceBatchHW( InstanceManager *creator, MeshPtr &meshReference,
                                        const MaterialPtr &material, size_t instancesPerBatch,
                                        const Mesh::IndexMap *indexToBoneMap, std::string_view batchName ) :
                InstanceBatch( creator, meshReference, material, instancesPerBatch,
                                indexToBoneMap, batchName )
                
    {
        //Override defaults, so that InstancedEntities don't create a skeleton instance
        mTechnSupportsSkeletal = false;
    }

    InstanceBatchHW::~InstanceBatchHW()
    = default;

    //-----------------------------------------------------------------------
    auto InstanceBatchHW::calculateMaxNumInstances( const SubMesh *baseSubMesh, InstanceManagerFlags flags ) const -> size_t
    {
        size_t retVal = 0;

        RenderSystem *renderSystem = Root::getSingleton().getRenderSystem();
        const RenderSystemCapabilities *capabilities = renderSystem->getCapabilities();

        if( capabilities->hasCapability( Capabilities::VERTEX_BUFFER_INSTANCE_DATA ) )
        {
            //This value is arbitrary (theorical max is 2^30 for D3D9) but is big enough and safe
            retVal = 65535;
        }

        return retVal;
    }
    //-----------------------------------------------------------------------
    void InstanceBatchHW::buildFrom( const SubMesh *baseSubMesh, const RenderOperation &renderOperation )
    {
        InstanceBatch::buildFrom( baseSubMesh, renderOperation );

        //We need to clone the VertexData (but just reference all buffers, except the last one)
        //because last buffer contains data specific to this batch, we need a different binding
        mRenderOperation.vertexData = mRenderOperation.vertexData->clone( false );
        mRemoveOwnVertexData = true;
        VertexData *thisVertexData      = mRenderOperation.vertexData;
        const unsigned short lastSource = thisVertexData->vertexDeclaration->getMaxSource();
        HardwareVertexBufferSharedPtr vertexBuffer =
                                        HardwareBufferManager::getSingleton().createVertexBuffer(
                                        thisVertexData->vertexDeclaration->getVertexSize(lastSource),
                                        mInstancesPerBatch,
                                        HardwareBuffer::STATIC_WRITE_ONLY );
        thisVertexData->vertexBufferBinding->setBinding( lastSource, vertexBuffer );
        vertexBuffer->setIsInstanceData( true );
        vertexBuffer->setInstanceDataStepRate( 1 );
    }
    //-----------------------------------------------------------------------
    void InstanceBatchHW::setupVertices( const SubMesh* baseSubMesh )
    {
        mRenderOperation.vertexData = baseSubMesh->vertexData->clone();
        mRemoveOwnVertexData = true; //Raise flag to remove our own vertex data in the end (not always needed)
        
        VertexData *thisVertexData = mRenderOperation.vertexData;

        //No skeletal animation support in this technique, sorry
        removeBlendData();

        //Modify the declaration so it contains an extra source, where we can put the per instance data
        size_t offset               = 0;
        unsigned short nextTexCoord = thisVertexData->vertexDeclaration->getNextFreeTextureCoordinate();
        const unsigned short newSource = thisVertexData->vertexDeclaration->getMaxSource() + 1;
        for( unsigned char i=0; i<3 + mCreator->getNumCustomParams(); ++i )
        {
            thisVertexData->vertexDeclaration->addElement( newSource, offset, VertexElementType::FLOAT4,
                                                            VertexElementSemantic::TEXTURE_COORDINATES, nextTexCoord++ );
            offset = thisVertexData->vertexDeclaration->getVertexSize( newSource );
        }

        //Create the vertex buffer containing per instance data
        HardwareVertexBufferSharedPtr vertexBuffer =
                                        HardwareBufferManager::getSingleton().createVertexBuffer(
                                        thisVertexData->vertexDeclaration->getVertexSize(newSource),
                                        mInstancesPerBatch,
                                        HardwareBuffer::STATIC_WRITE_ONLY );
        thisVertexData->vertexBufferBinding->setBinding( newSource, vertexBuffer );
        vertexBuffer->setIsInstanceData( true );
        vertexBuffer->setInstanceDataStepRate( 1 );
    }
    //-----------------------------------------------------------------------
    void InstanceBatchHW::setupIndices( const SubMesh* baseSubMesh )
    {
        //We could use just a reference, but the InstanceManager will in the end attampt to delete
        //the pointer, and we can't give it something that doesn't belong to us.
        mRenderOperation.indexData = baseSubMesh->indexData->clone( true );
        mRemoveOwnIndexData = true; //Raise flag to remove our own index data in the end (not always needed)
    }
    //-----------------------------------------------------------------------
    void InstanceBatchHW::removeBlendData()
    {
        VertexData *thisVertexData = mRenderOperation.vertexData;

        unsigned short safeSource = 0xFFFF;
        const VertexElement* blendIndexElem = thisVertexData->vertexDeclaration->findElementBySemantic(
                                                                                VertexElementSemantic::BLEND_INDICES );
        if( blendIndexElem )
        {
            //save the source in order to prevent the next stage from unbinding it.
            safeSource = blendIndexElem->getSource();
            // Remove buffer reference
            thisVertexData->vertexBufferBinding->unsetBinding( blendIndexElem->getSource() );
        }
        // Remove blend weights
        const VertexElement* blendWeightElem = thisVertexData->vertexDeclaration->findElementBySemantic(
                                                                                VertexElementSemantic::BLEND_WEIGHTS );
        if( blendWeightElem && blendWeightElem->getSource() != safeSource )
        {
            // Remove buffer reference
            thisVertexData->vertexBufferBinding->unsetBinding( blendWeightElem->getSource() );
        }

        thisVertexData->vertexDeclaration->removeElement(VertexElementSemantic::BLEND_INDICES);
        thisVertexData->vertexDeclaration->removeElement(VertexElementSemantic::BLEND_WEIGHTS);
        thisVertexData->closeGapsInBindings();
    }
    //-----------------------------------------------------------------------
    auto InstanceBatchHW::checkSubMeshCompatibility( const SubMesh* baseSubMesh ) -> bool
    {
        //Max number of texture coordinates is _usually_ 8, we need at least 3 available
        if( baseSubMesh->vertexData->vertexDeclaration->getNextFreeTextureCoordinate() > 8-2 )
        {
            OGRE_EXCEPT(ExceptionCodes::NOT_IMPLEMENTED, "Given mesh must have at "
                                                        "least 3 free TEXCOORDs",
                        "InstanceBatchHW::checkSubMeshCompatibility");
        }
        if( baseSubMesh->vertexData->vertexDeclaration->getNextFreeTextureCoordinate() >
            8-2-mCreator->getNumCustomParams() ||
            3 + mCreator->getNumCustomParams() >= 8 )
        {
            OGRE_EXCEPT(ExceptionCodes::INVALIDPARAMS, std::format("There are not enough free TEXCOORDs to hold the "
                                                        "custom parameters (required: {}). See InstanceManager"
                                                        "::setNumCustomParams documentation", 3 + mCreator->
                                                        getNumCustomParams()),
                        "InstanceBatchHW::checkSubMeshCompatibility");
        }

        return InstanceBatch::checkSubMeshCompatibility( baseSubMesh );
    }
    //-----------------------------------------------------------------------
    auto InstanceBatchHW::updateVertexBuffer( Camera *currentCamera ) -> size_t
    {
        size_t retVal = 0;

        //Now lock the vertex buffer and copy the 4x3 matrices, only those who need it!
        VertexBufferBinding* binding = mRenderOperation.vertexData->vertexBufferBinding; 
        const auto bufferIdx = ushort(binding->getBufferCount()-1);
        HardwareBufferLockGuard vertexLock(binding->getBuffer(bufferIdx), HardwareBuffer::LockOptions::DISCARD);
        auto *pDest = static_cast<float*>(vertexLock.pData);

        unsigned char numCustomParams           = mCreator->getNumCustomParams();
        size_t customParamIdx                   = 0;

        for (auto const& itor : mInstancedEntities)
        {
            //Cull on an individual basis, the less entities are visible, the less instances we draw.
            //No need to use null matrices at all!
            if( itor->findVisible( currentCamera ) )
            {
                const size_t floatsWritten = itor->getTransforms3x4( (Matrix3x4f*)pDest );

                if( mManager->getCameraRelativeRendering() )
                    makeMatrixCameraRelative3x4( (Matrix3x4f*)pDest, floatsWritten / 12 );

                pDest += floatsWritten;

                //Write custom parameters, if any
                for( unsigned char i=0; i<numCustomParams; ++i )
                {
                    *pDest++ = mCustomParams[customParamIdx+i].x;
                    *pDest++ = mCustomParams[customParamIdx+i].y;
                    *pDest++ = mCustomParams[customParamIdx+i].z;
                    *pDest++ = mCustomParams[customParamIdx+i].w;
                }

                ++retVal;
            }

            customParamIdx += numCustomParams;
        }

        return retVal;
    }
    //-----------------------------------------------------------------------
    void InstanceBatchHW::_boundsDirty()
    {
        //Don't update if we're static, but still mark we're dirty
        if( !mBoundsDirty && !mKeepStatic )
            mCreator->_addDirtyBatch( this );
        mBoundsDirty = true;
    }
    //-----------------------------------------------------------------------
    void InstanceBatchHW::setStaticAndUpdate( bool bStatic )
    {
        //We were dirty but didn't update bounds. Do it now.
        if( mKeepStatic && mBoundsDirty )
            mCreator->_addDirtyBatch( this );

        mKeepStatic = bStatic;
        if( mKeepStatic )
        {
            //One final update, since there will be none from now on
            //(except further calls to this function). Pass NULL because
            //we want to include only those who were added to the scene
            //but we don't want to perform culling
            mRenderOperation.numberOfInstances = updateVertexBuffer( nullptr );
        }
    }
    //-----------------------------------------------------------------------
    void InstanceBatchHW::getWorldTransforms( Matrix4* xform ) const
    {
        *xform = Matrix4::IDENTITY;
    }
    //-----------------------------------------------------------------------
    auto InstanceBatchHW::getNumWorldTransforms() const noexcept -> unsigned short
    {
        return 1;
    }
    //-----------------------------------------------------------------------
    void InstanceBatchHW::_updateRenderQueue( RenderQueue* queue )
    {
        if( !mKeepStatic )
        {
            //Completely override base functionality, since we don't cull on an "all-or-nothing" basis
            //and we don't support skeletal animation
            if( (mRenderOperation.numberOfInstances = updateVertexBuffer( mCurrentCamera )) )
                queue->addRenderable( this, mRenderQueueID, mRenderQueuePriority );
        }
        else
        {
            OgreAssert(!mManager->getCameraRelativeRendering(),
                       "Camera-relative rendering is incompatible with Instancing's static batches. "
                       "Disable at least one of them");

            //Don't update when we're static
            if( mRenderOperation.numberOfInstances )
                queue->addRenderable( this, mRenderQueueID, mRenderQueuePriority );
        }
    }
}
