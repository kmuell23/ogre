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
#include <cstddef>

module Ogre.Core;

import :Exception;
import :Material;
import :Math;
import :RadixSort;
import :RenderQueueSortingGrouping;
import :Renderable;
import :Technique;

import <algorithm>;
import <ranges>;
import <set>;

namespace Ogre {
namespace {
    /// Comparator to order objects by descending camera distance
    struct DistanceSortDescendingLess
    {
        const Camera* camera;

        auto operator()(const RenderablePass& a, const RenderablePass& b) const -> bool
        {
            if (a.renderable == b.renderable)
            {
                // Same renderable, sort by pass hash
                return a.pass->getHash() < b.pass->getHash();
            }
            else
            {
                // Different renderables, sort by distance
                Real adist = a.renderable->getSquaredViewDepth(camera);
                Real bdist = b.renderable->getSquaredViewDepth(camera);
                if (Math::RealEqual(adist, bdist))
                {
                    // Must return deterministic result, doesn't matter what
                    return a.pass < b.pass;
                }
                else
                {
                    // Sort DESCENDING by dist (i.e. far objects first)
                    return (adist > bdist);
                }
            }

        }
    };

    /// Functor for accessing sort value 1 for radix sort (Pass)
    struct RadixSortFunctorPass
    {
        auto operator()(const RenderablePass& p) const -> uint32
        {
            return p.pass->getHash();
        }
    };

    /// Functor for descending sort value 2 for radix sort (distance)
    struct RadixSortFunctorDistance
    {
        const Camera* camera;

        auto operator()(const RenderablePass& p) const -> float
        {
            // Sort DESCENDING by depth (ie far objects first), use negative distance
            // here because radix sorter always dealing with accessing sort
            return static_cast<float>(- p.renderable->getSquaredViewDepth(camera));
        }
    };
}
    //-----------------------------------------------------------------------
    RenderPriorityGroup::RenderPriorityGroup(RenderQueueGroup* parent, 
            bool splitPassesByLightingType,
            bool splitNoShadowPasses, 
            bool shadowCastersNotReceivers)
        : mParent(parent)
        , mSplitPassesByLightingType(splitPassesByLightingType)
        , mSplitNoShadowPasses(splitNoShadowPasses)
        , mShadowCastersNotReceivers(shadowCastersNotReceivers)
    {
        // Initialise collection sorting options
        // this can become dynamic according to invocation later
        defaultOrganisationMode();

        // Transparents will always be sorted this way
        mTransparents.addOrganisationMode(QueuedRenderableCollection::OrganisationMode::SORT_DESCENDING);

        
    }
    //-----------------------------------------------------------------------
    void RenderPriorityGroup::resetOrganisationModes()
    {
        mSolidsBasic.resetOrganisationModes();
        mSolidsDiffuseSpecular.resetOrganisationModes();
        mSolidsDecal.resetOrganisationModes();
        mSolidsNoShadowReceive.resetOrganisationModes();
        mTransparentsUnsorted.resetOrganisationModes();
    }
    //-----------------------------------------------------------------------
    void RenderPriorityGroup::addOrganisationMode(QueuedRenderableCollection::OrganisationMode om)
    {
        mSolidsBasic.addOrganisationMode(om);
        mSolidsDiffuseSpecular.addOrganisationMode(om);
        mSolidsDecal.addOrganisationMode(om);
        mSolidsNoShadowReceive.addOrganisationMode(om);
        mTransparentsUnsorted.addOrganisationMode(om);
    }
    //-----------------------------------------------------------------------
    void RenderPriorityGroup::defaultOrganisationMode()
    {
        resetOrganisationModes();
        addOrganisationMode(QueuedRenderableCollection::OrganisationMode::PASS_GROUP);
    }
    //-----------------------------------------------------------------------
    void RenderPriorityGroup::addRenderable(Renderable* rend, Technique* pTech)
    {
        // Transparent and depth/colour settings mean depth sorting is required?
        // Note: colour write disabled with depth check/write enabled means
        //       setup depth buffer for other passes use.
        if (pTech->isTransparentSortingForced() || 
            (pTech->isTransparent() && 
            (!pTech->isDepthWriteEnabled() ||
             !pTech->isDepthCheckEnabled() ||
             pTech->hasColourWriteDisabled())))
        {
            if (pTech->isTransparentSortingEnabled())
                addTransparentRenderable(pTech, rend);
            else
                addUnsortedTransparentRenderable(pTech, rend);
        }
        else
        {
            if (mSplitNoShadowPasses &&
                mParent->getShadowsEnabled() &&
                (!pTech->getParent()->getReceiveShadows() ||
                 (rend->getCastsShadows() && mShadowCastersNotReceivers)))
            {
                // Add solid renderable and add passes to no-shadow group
                addSolidRenderable(pTech, rend, true);
            }
            else
            {
                if (mSplitPassesByLightingType && mParent->getShadowsEnabled())
                {
                    addSolidRenderableSplitByLightType(pTech, rend);
                }
                else
                {
                    addSolidRenderable(pTech, rend, false);
                }
            }
        }

    }
    //-----------------------------------------------------------------------
    void RenderPriorityGroup::addSolidRenderable(Technique* pTech, 
        Renderable* rend, bool addToNoShadow)
    {
        QueuedRenderableCollection* collection;
        if (addToNoShadow)
        {
            collection = &mSolidsNoShadowReceive;
        }
        else
        {
            collection = &mSolidsBasic;
        }


        for (auto i : pTech->getPasses())
        {
            // Insert into solid list
            collection->addRenderable(i, rend);
        }
    }
    //-----------------------------------------------------------------------
    void RenderPriorityGroup::addSolidRenderableSplitByLightType(Technique* pTech,
        Renderable* rend)
    {
        // Divide the passes into the 3 categories
        const IlluminationPassList& passes = pTech->getIlluminationPasses();

        for(auto p : passes)
        {
            // Insert into solid list
            QueuedRenderableCollection* collection = nullptr;
            using enum IlluminationStage;
            switch(p->stage)
            {
            case AMBIENT:
                collection = &mSolidsBasic;
                break;
            case PER_LIGHT:
                collection = &mSolidsDiffuseSpecular;
                break;
            case DECAL:
                collection = &mSolidsDecal;
                break;
            default:
                assert(false); // should never happen
            };

            collection->addRenderable(p->pass, rend);
        }
    }
    //-----------------------------------------------------------------------
    void RenderPriorityGroup::addUnsortedTransparentRenderable(Technique* pTech, Renderable* rend)
    {
        for (auto i : pTech->getPasses())
        {
            // Insert into transparent list
            mTransparentsUnsorted.addRenderable(i, rend);
        }
    }
    //-----------------------------------------------------------------------
    void RenderPriorityGroup::addTransparentRenderable(Technique* pTech, Renderable* rend)
    {
        for (auto i : pTech->getPasses())
        {
            // Insert into transparent list
            mTransparents.addRenderable(i, rend);
        }
    }
    //-----------------------------------------------------------------------
    void RenderPriorityGroup::removePassEntry(Pass* p)
    {
        mSolidsBasic.removePassGroup(p);
        mSolidsDiffuseSpecular.removePassGroup(p);
        mSolidsNoShadowReceive.removePassGroup(p);
        mSolidsDecal.removePassGroup(p);
        mTransparentsUnsorted.removePassGroup(p);
        mTransparents.removePassGroup(p); // shouldn't be any, but for completeness
    }   
    //-----------------------------------------------------------------------
    void RenderPriorityGroup::clear()
    {
        // Delete queue groups which are using passes which are to be
        // deleted, we won't need these any more and they clutter up 
        // the list and can cause problems with future clones

        const Pass::PassSet& graveyardList = Pass::getPassGraveyard();
        for (auto gi : graveyardList)
        {
            removePassEntry(gi);
        }

        // Now remove any dirty passes, these will have their hashes recalculated
        // by the parent queue after all groups have been processed
        // If we don't do this, the std::map will become inconsistent for new insterts

        const Pass::PassSet& dirtyList = Pass::getDirtyHashList();
        for (auto di : dirtyList)
        {
            removePassEntry(di);
        }
        // NB we do NOT clear the graveyard or the dirty list here, because 
        // it needs to be acted on for all groups, the parent queue takes 
        // care of this afterwards

        // Now empty the remaining collections
        // Note that groups don't get deleted, just emptied hence the difference
        // between the pass groups which are removed above, and clearing done
        // here
        mSolidsBasic.clear();
        mSolidsDecal.clear();
        mSolidsDiffuseSpecular.clear();
        mSolidsNoShadowReceive.clear();
        mTransparentsUnsorted.clear();
        mTransparents.clear();

    }
    //-----------------------------------------------------------------------
    void RenderPriorityGroup::sort(const Camera* cam)
    {
        mSolidsBasic.sort(cam);
        mSolidsDecal.sort(cam);
        mSolidsDiffuseSpecular.sort(cam);
        mSolidsNoShadowReceive.sort(cam);
        mTransparentsUnsorted.sort(cam);
        mTransparents.sort(cam);
    }
    //-----------------------------------------------------------------------
    void RenderPriorityGroup::merge( const RenderPriorityGroup* rhs )
    {
        mSolidsBasic.merge( rhs->mSolidsBasic );
        mSolidsDecal.merge( rhs->mSolidsDecal );
        mSolidsDiffuseSpecular.merge( rhs->mSolidsDiffuseSpecular );
        mSolidsNoShadowReceive.merge( rhs->mSolidsNoShadowReceive );
        mTransparentsUnsorted.merge( rhs->mTransparentsUnsorted );
        mTransparents.merge( rhs->mTransparents );
    }
    //-----------------------------------------------------------------------
    QueuedRenderableCollection::QueuedRenderableCollection()
        
    = default;

    //-----------------------------------------------------------------------
    void QueuedRenderableCollection::clear()
    {
        for (auto & i : mGrouped)
        {
            // Clear the list associated with this pass, but leave the pass entry
            i.second.clear();
        }

        // Clear sorted list
        mSortedDescending.clear();
    }
    //-----------------------------------------------------------------------
    void QueuedRenderableCollection::removePassGroup(Pass* p)
    {
        PassGroupRenderableMap::iterator i;

        i = mGrouped.find(p);
        if (i != mGrouped.end())
        {
            // erase from map
            mGrouped.erase(i);
        }
    }
    //-----------------------------------------------------------------------
    void QueuedRenderableCollection::sort(const Camera* cam)
    {
        /// Radix sorter for accessing sort value 1 (Pass)
        static RadixSort<RenderablePassList, RenderablePass, uint32> msRadixSorter1;
        /// Radix sorter for sort value 2 (distance)
        static RadixSort<RenderablePassList, RenderablePass, float> msRadixSorter2;

        // ascending and descending sort both set bit 1
        // We always sort descending, because the only difference is in the
        // acceptVisitor method, where we iterate in reverse in ascending mode
        if (!!(mOrganisationMode & OrganisationMode::SORT_DESCENDING))
        {
            
            // We can either use a stable_sort and the 'less' implementation,
            // or a 2-pass radix sort (once by pass, then by distance, since
            // radix sorting is inherently stable this will work)
            // We use stable_sort if the number of items is 512 or less, since
            // the complexity of the radix sort is approximately O(10N), since 
            // each sort is O(5N) (1 pass histograms, 4 passes sort)
            // Since stable_sort has a worst-case performance of O(N(logN)^2)
            // the performance tipping point is from about 1500 items, but in
            // stable_sorts best-case scenario O(NlogN) it would be much higher.
            // Take a stab at 2000 items.
            
            if (mSortedDescending.size() > 2000)
            {
                // sort by pass
                msRadixSorter1.sort(mSortedDescending, RadixSortFunctorPass());
                // sort by depth
                msRadixSorter2.sort(mSortedDescending, RadixSortFunctorDistance{cam});
            }
            else
            {
                std::stable_sort(
                    mSortedDescending.begin(), mSortedDescending.end(),
                    DistanceSortDescendingLess{cam});
            }
        }

        // Nothing needs to be done for pass groups, they auto-organise

    }
    //-----------------------------------------------------------------------
    void QueuedRenderableCollection::addRenderable(Pass* pass, Renderable* rend)
    {
        // ascending and descending sort both set bit 1
        if (!!(mOrganisationMode & OrganisationMode::SORT_DESCENDING))
        {
            mSortedDescending.push_back(RenderablePass{rend, pass});
        }

        if (!!(mOrganisationMode & OrganisationMode::PASS_GROUP))
        {
            // Optionally create new pass entry, build a new list
            // Note that this pass and list are never destroyed until the
            // engine shuts down, or a pass is destroyed or has it's hash
            // recalculated, although the lists will be cleared
            auto i = mGrouped.emplace(pass, RenderableList()).first;

            // Insert renderable
            i->second.push_back(rend);
            
        }
        
    }
    //-----------------------------------------------------------------------
    void QueuedRenderableCollection::acceptVisitor(
        QueuedRenderableVisitor* visitor, OrganisationMode om) const
    {
        using enum OrganisationMode;
        if ((om & mOrganisationMode) == OrganisationMode{})
        {
            // try to fall back
            if (!!(PASS_GROUP & mOrganisationMode))
                om = PASS_GROUP;
            else if (!!(SORT_ASCENDING & mOrganisationMode))
                om = SORT_ASCENDING;
            else if (!!(SORT_DESCENDING & mOrganisationMode))
                om = SORT_DESCENDING;
            else
                OGRE_EXCEPT(ExceptionCodes::INVALIDPARAMS, 
                    "Organisation mode requested in acceptVistor was not notified "
                    "to this class ahead of time, therefore may not be supported.", 
                    "QueuedRenderableCollection::acceptVisitor");
        }

        switch(om)
        {
        case PASS_GROUP:
            acceptVisitorGrouped(visitor);
            break;
        case SORT_DESCENDING:
            acceptVisitorDescending(visitor);
            break;
        case SORT_ASCENDING:
            acceptVisitorAscending(visitor);
            break;
        }
        
    }
    //-----------------------------------------------------------------------
    void QueuedRenderableCollection::acceptVisitorGrouped(
        QueuedRenderableVisitor* visitor) const
    {
        for (const auto & ipass : mGrouped)
        {
            // Fast bypass if this group is now empty
            if (ipass.second.empty()) continue;

            visitor->visit(ipass.first, const_cast<RenderableList&>(ipass.second));
        } 

    }
    //-----------------------------------------------------------------------
    void QueuedRenderableCollection::acceptVisitorDescending(
        QueuedRenderableVisitor* visitor) const
    {
        // List is already in descending order, so iterate forward
        for (auto i : mSortedDescending)
        {
            visitor->visit(const_cast<RenderablePass*>(&i));
        }
    }
    //-----------------------------------------------------------------------
    void QueuedRenderableCollection::acceptVisitorAscending(
        QueuedRenderableVisitor* visitor) const
    {
        // List is in descending order, so iterate in reverse
        for (auto i : std::ranges::reverse_view(mSortedDescending))
        {
            visitor->visit(const_cast<RenderablePass*>(&i));
        }

    }
    //-----------------------------------------------------------------------
    void QueuedRenderableCollection::merge( const QueuedRenderableCollection& rhs )
    {
        mSortedDescending.insert( mSortedDescending.end(), rhs.mSortedDescending.begin(), rhs.mSortedDescending.end() );

        for(auto const& srcGroup : rhs.mGrouped)
        {
            // Optionally create new pass entry, build a new list
            // Note that this pass and list are never destroyed until the
            // engine shuts down, or a pass is destroyed or has it's hash
            // recalculated, although the lists will be cleared
            auto dstGroup = mGrouped.emplace(srcGroup.first, RenderableList()).first;

            // Insert renderable
            dstGroup->second.insert( dstGroup->second.end(), srcGroup.second.begin(), srcGroup.second.end() );
        }
    }


}
