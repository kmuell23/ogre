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

module Ogre.Core;

import :HardwareVertexBuffer;
import :Material;
import :MaterialManager;
import :MovableObject;
import :Pass;
import :RenderOperation;
import :RenderQueue;
import :RenderQueueSortingGrouping;
import :Renderable;
import :SceneManager;
import :SceneManagerEnumerator;
import :SharedPtr;
import :Technique;
import :VertexIndexData;

import <map>;
import <memory>;
import <utility>;

namespace Ogre {

    //---------------------------------------------------------------------
    RenderQueue::RenderQueue()
         
    {
        // Create the 'main' queue up-front since we'll always need that
        mGroups[std::to_underlying(RenderQueueGroupID::MAIN)] = std::make_unique<RenderQueueGroup>(
            mSplitPassesByLightingType, mSplitNoShadowPasses, mShadowCastersCannotBeReceivers);

        // set default queue
        mDefaultQueueGroup = RenderQueueGroupID::MAIN;
        mDefaultRenderablePriority = Renderable::DEFAULT_PRIORITY;

    }
    //---------------------------------------------------------------------
    RenderQueue::~RenderQueue()
    {
        
        // trigger the pending pass updates, otherwise we could leak
        Pass::processPendingPassUpdates();
    }
    //-----------------------------------------------------------------------
    void RenderQueue::addRenderable(Renderable* pRend, RenderQueueGroupID groupID, ushort priority)
    {
        // Find group
        RenderQueueGroup* pGroup = getQueueGroup(groupID);

        Technique* pTech;

        // tell material it's been used
        if (pRend->getMaterial())
            pRend->getMaterial()->touch();

        // Check material & technique supplied (the former since the default implementation
        // of getTechnique is based on it for backwards compatibility
        if(!pRend->getMaterial() || !pRend->getTechnique())
        {
            // Use default base white, with lighting only if vertices has normals
            RenderOperation op;
            pRend->getRenderOperation(op);
            bool useLighting = (nullptr != op.vertexData->vertexDeclaration->findElementBySemantic(VertexElementSemantic::NORMAL));
            MaterialPtr defaultMat = MaterialManager::getSingleton().getDefaultMaterial(useLighting);
            defaultMat->load();
            pTech = defaultMat->getBestTechnique();
        }
        else
            pTech = pRend->getTechnique();

        if (mRenderableListener)
        {
            // Allow listener to override technique and to abort
            if (!mRenderableListener->renderableQueued(pRend, groupID, priority, 
                &pTech, this))
                return; // rejected

            // tell material it's been used (incase changed)
            pTech->getParent()->touch();
        }
        
        pGroup->addRenderable(pRend, pTech, priority);

    }
    //-----------------------------------------------------------------------
    void RenderQueue::clear(bool destroyPassMaps)
    {
        // Note: We clear dirty passes from all RenderQueues in all 
        // SceneManagers, because the following recalculation of pass hashes
        // also considers all RenderQueues and could become inconsistent, otherwise.
        for (auto const& [key, value] : SceneManagerEnumerator::getSingleton().getSceneManagers())
        {
            RenderQueue* queue = value->getRenderQueue();

            for (auto & mGroup : queue->mGroups)
            {
                if(mGroup)
                    mGroup->clear(destroyPassMaps);
            }
        }

        // Now trigger the pending pass updates
        Pass::processPendingPassUpdates();

        // NB this leaves the items present (but empty)
        // We're assuming that frame-by-frame, the same groups are likely to 
        //  be used, so no point destroying the vectors and incurring the overhead
        //  that would cause, let them be destroyed in the destructor.
    }
    //-----------------------------------------------------------------------
    void RenderQueue::addRenderable(Renderable* pRend, RenderQueueGroupID groupID)
    {
        addRenderable(pRend, groupID, mDefaultRenderablePriority);
    }
    //-----------------------------------------------------------------------
    void RenderQueue::addRenderable(Renderable* pRend)
    {
        addRenderable(pRend, mDefaultQueueGroup, mDefaultRenderablePriority);
    }
    //-----------------------------------------------------------------------
    auto RenderQueue::getDefaultQueueGroup() const noexcept -> RenderQueueGroupID
    {
        return mDefaultQueueGroup;
    }
    //-----------------------------------------------------------------------
    void RenderQueue::setDefaultQueueGroup(RenderQueueGroupID grp)
    {
        mDefaultQueueGroup = grp;
    }
    //-----------------------------------------------------------------------
    auto RenderQueue::getDefaultRenderablePriority() const noexcept -> ushort
    {
        return mDefaultRenderablePriority;
    }
    //-----------------------------------------------------------------------
    void RenderQueue::setDefaultRenderablePriority(ushort priority)
    {
        mDefaultRenderablePriority = priority;
    }
    
    
    //-----------------------------------------------------------------------
    auto RenderQueue::getQueueGroup(RenderQueueGroupID groupID) -> RenderQueueGroup*
    {
        if (!mGroups[std::to_underlying(groupID)])
        {
            // Insert new
            mGroups[std::to_underlying(groupID)] = std::make_unique<RenderQueueGroup>(mSplitPassesByLightingType, mSplitNoShadowPasses,
                                                        mShadowCastersCannotBeReceivers);
        }

        return mGroups[std::to_underlying(groupID)].get();

    }
    //-----------------------------------------------------------------------
    void RenderQueue::setSplitPassesByLightingType(bool split)
    {
        mSplitPassesByLightingType = split;

        for (auto & mGroup : mGroups)
        {
            if(mGroup)
                mGroup->setSplitPassesByLightingType(split);
        }
    }
    //-----------------------------------------------------------------------
    auto RenderQueue::getSplitPassesByLightingType() const noexcept -> bool
    {
        return mSplitPassesByLightingType;
    }
    //-----------------------------------------------------------------------
    void RenderQueue::setSplitNoShadowPasses(bool split)
    {
        mSplitNoShadowPasses = split;

        for (auto & mGroup : mGroups)
        {
            if(mGroup)
                mGroup->setSplitNoShadowPasses(split);
        }
    }
    //-----------------------------------------------------------------------
    auto RenderQueue::getSplitNoShadowPasses() const noexcept -> bool
    {
        return mSplitNoShadowPasses;
    }
    //-----------------------------------------------------------------------
    void RenderQueue::setShadowCastersCannotBeReceivers(bool ind)
    {
        mShadowCastersCannotBeReceivers = ind;

        for (auto & mGroup : mGroups)
        {
            if(mGroup)
                mGroup->setShadowCastersCannotBeReceivers(ind);
        }
    }
    //-----------------------------------------------------------------------
    auto RenderQueue::getShadowCastersCannotBeReceivers() const noexcept -> bool
    {
        return mShadowCastersCannotBeReceivers;
    }
    //-----------------------------------------------------------------------
    void RenderQueue::merge( const RenderQueue* rhs )
    {
        for (size_t i = 0; i < std::to_underlying(RenderQueueGroupID::COUNT); ++i)
        {
            if(!rhs->mGroups[i])
                continue;

            RenderQueueGroup* pDstGroup = getQueueGroup( static_cast<RenderQueueGroupID>(i) );
            pDstGroup->merge( rhs->mGroups[i].get() );
        }
    }

    //---------------------------------------------------------------------
    void RenderQueue::processVisibleObject(MovableObject* mo, 
        Camera* cam, 
        bool onlyShadowCasters, 
        VisibleObjectsBoundsInfo* visibleBounds)
    {
        // receiveShadows is a material property, so we can query it before LOD
        bool receiveShadows = getQueueGroup(mo->getRenderQueueGroup())->getShadowsEnabled() && mo->getReceivesShadows();

        if(onlyShadowCasters && !mo->getCastShadows() && !receiveShadows)
            return;

        mo->_notifyCurrentCamera(cam);
        if (!mo->isVisible())
            return;

        const auto& bbox = mo->getWorldBoundingBox(true);
        const auto& bsphere = mo->getWorldBoundingSphere(true);

        if (!onlyShadowCasters || mo->getCastShadows())
        {
            mo->_updateRenderQueue(this);
            if (visibleBounds)
            {
                visibleBounds->merge(bbox, bsphere, cam, receiveShadows);
            }
        }
        // not shadow caster, receiver only?
        else if (receiveShadows)
        {
            visibleBounds->mergeNonRenderedButInFrustum(bbox, bsphere, cam);
        }
    }

}
