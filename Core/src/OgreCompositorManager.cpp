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
#include <memory>

#include "OgreCompositionPass.hpp"
#include "OgreCompositionTargetPass.hpp"
#include "OgreCompositionTechnique.hpp"
#include "OgreCompositor.hpp"
#include "OgreCompositorChain.hpp"
#include "OgreCompositorInstance.hpp"
#include "OgreCompositorManager.hpp"
#include "OgreException.hpp"
#include "OgreHardwareBuffer.hpp"
#include "OgreRectangle2D.hpp"
#include "OgreRenderSystem.hpp"
#include "OgreRenderTarget.hpp"
#include "OgreRoot.hpp"
#include "OgreTextureManager.hpp"
#include "OgreViewport.hpp"

namespace Ogre {
class CompositorLogic;
class CustomCompositionPass;
class Renderable;

template<> CompositorManager* Singleton<CompositorManager>::msSingleton = nullptr;
auto CompositorManager::getSingletonPtr() -> CompositorManager*
{
    return msSingleton;
}
auto CompositorManager::getSingleton() -> CompositorManager&
{  
    assert( msSingleton );  return ( *msSingleton );  
}//-----------------------------------------------------------------------
CompositorManager::CompositorManager()
    
{
    initialise();

    // Loading order (just after materials)
    mLoadOrder = 110.0f;

    // Resource type
    mResourceType = "Compositor";

    // Register with resource group manager
    ResourceGroupManager::getSingleton()._registerResourceManager(mResourceType, this);

}
//-----------------------------------------------------------------------
CompositorManager::~CompositorManager()
{
    freeChains();
    freePooledTextures(false);
    delete mRectangle;

    // Resources cleared by superclass
    // Unregister with resource group manager
    ResourceGroupManager::getSingleton()._unregisterResourceManager(mResourceType);
    ResourceGroupManager::getSingleton()._unregisterScriptLoader(this);
}
//-----------------------------------------------------------------------
auto CompositorManager::createImpl(const String& name, ResourceHandle handle,
    const String& group, bool isManual, ManualResourceLoader* loader,
    const NameValuePairList* params) -> Resource*
{
    return new Compositor(this, name, handle, group, isManual, loader);
}
//-----------------------------------------------------------------------
auto CompositorManager::create (const String& name, const String& group,
                                bool isManual, ManualResourceLoader* loader,
                                const NameValuePairList* createParams) -> CompositorPtr
{
    return static_pointer_cast<Compositor>(createResource(name,group,isManual,loader,createParams));
}
//-----------------------------------------------------------------------
auto CompositorManager::getByName(const String& name, const String& groupName) const -> CompositorPtr
{
    return static_pointer_cast<Compositor>(getResourceByName(name, groupName));
}
//-----------------------------------------------------------------------
void CompositorManager::initialise()
{
}
//-----------------------------------------------------------------------
auto CompositorManager::getCompositorChain(Viewport *vp) -> CompositorChain *
{
    auto i=mChains.find(vp);
    if(i != mChains.end())
    {
        return i->second;
    }
    else
    {
        auto *chain = new CompositorChain(vp);
        mChains[vp] = chain;
        return chain;
    }
}
//-----------------------------------------------------------------------
auto CompositorManager::hasCompositorChain(const Viewport *vp) const -> bool
{
    return mChains.find(vp) != mChains.end();
}
//-----------------------------------------------------------------------
void CompositorManager::removeCompositorChain(const Viewport *vp)
{
    auto i = mChains.find(vp);
    if (i != mChains.end())
    {
        delete  i->second;
        mChains.erase(i);
    }
}
//-----------------------------------------------------------------------
void CompositorManager::removeAll()
{
    freeChains();
    ResourceManager::removeAll();
}
//-----------------------------------------------------------------------
void CompositorManager::freeChains()
{
    Chains::iterator i, iend=mChains.end();
    for(i=mChains.begin(); i!=iend;++i)
    {
        delete  i->second;
    }
    mChains.clear();
}
//-----------------------------------------------------------------------
auto CompositorManager::_getTexturedRectangle2D() -> Renderable *
{
    if(!mRectangle)
    {
        /// 2D rectangle, to use for render_quad passes
        mRectangle = new Rectangle2D(true, HardwareBuffer::HBU_DYNAMIC_WRITE_ONLY_DISCARDABLE);
    }
    RenderSystem* rs = Root::getSingleton().getRenderSystem();
    Viewport* vp = rs->_getViewport();
    Real hOffset = rs->getHorizontalTexelOffset() / (0.5f * vp->getActualWidth());
    Real vOffset = rs->getVerticalTexelOffset() / (0.5f * vp->getActualHeight());
    mRectangle->setCorners(-1 + hOffset, 1 - vOffset, 1 + hOffset, -1 - vOffset);
    return mRectangle;
}
//-----------------------------------------------------------------------
auto CompositorManager::addCompositor(Viewport *vp, const String &compositor, int addPosition) -> CompositorInstance *
{
    CompositorPtr comp = getByName(compositor);
    if(!comp)
        OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS, "Compositor '"+compositor+"' not found");
    CompositorChain *chain = getCompositorChain(vp);
    return chain->addCompositor(comp, addPosition==-1 ? CompositorChain::LAST : (size_t)addPosition);
}
//-----------------------------------------------------------------------
void CompositorManager::removeCompositor(Viewport *vp, const String &compositor)
{
    CompositorChain *chain = getCompositorChain(vp);
    size_t pos = chain->getCompositorPosition(compositor);

    if(pos == CompositorChain::NPOS)
        OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS, "Compositor '"+compositor+"' not in chain");

    chain->removeCompositor(pos);
}
//-----------------------------------------------------------------------
void CompositorManager::setCompositorEnabled(Viewport *vp, const String &compositor, bool value)
{
    CompositorChain *chain = getCompositorChain(vp);
    size_t pos = chain->getCompositorPosition(compositor);

    if(pos == CompositorChain::NPOS)
        OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS, "Compositor '"+compositor+"' not in chain");

    chain->setCompositorEnabled(pos, value);
}
//---------------------------------------------------------------------
void CompositorManager::_reconstructAllCompositorResources()
{
    // In order to deal with shared resources, we have to disable *all* compositors
    // first, that way shared resources will get freed
    using InstVec = std::vector<CompositorInstance *>;
    InstVec instancesToReenable;
    for (auto i = mChains.begin(); i != mChains.end(); ++i)
    {
        CompositorChain* chain = i->second;
        for (CompositorInstance* inst : chain->getCompositorInstances())
        {
            if (inst->getEnabled())
            {
                inst->setEnabled(false);
                instancesToReenable.push_back(inst);
            }
        }
    }

    //UVs are lost, and will never be reconstructed unless we do them again, now
    if( mRectangle )
        mRectangle->setDefaultUVs();

    for (auto i = instancesToReenable.begin(); i != instancesToReenable.end(); ++i)
    {
        CompositorInstance* inst = *i;
        inst->setEnabled(true);
    }
}
//---------------------------------------------------------------------
auto CompositorManager::getPooledTexture(const String& name, 
    const String& localName,
    uint32 w, uint32 h, PixelFormat f, uint aa, const String& aaHint, bool srgb,
    CompositorManager::UniqueTextureSet& texturesAssigned, 
    CompositorInstance* inst, CompositionTechnique::TextureScope scope, TextureType type) -> TexturePtr
{
    OgreAssert(scope != CompositionTechnique::TS_GLOBAL, "Global scope texture can not be pooled");

    TextureDef def(w, h, type, f, aa, aaHint, srgb);

    if (scope == CompositionTechnique::TS_CHAIN)
    {
        StringPair pair = std::make_pair(inst->getCompositor()->getName(), localName);
        TextureDefMap& defMap = mChainTexturesByDef[pair];
        auto it = defMap.find(def);
        if (it != defMap.end())
        {
            return it->second;
        }
        // ok, we need to create a new one
        TexturePtr newTex = TextureManager::getSingleton().createManual(
            name, 
            ResourceGroupManager::INTERNAL_RESOURCE_GROUP_NAME, TEX_TYPE_2D, 
            (uint)w, (uint)h, 0, f, TU_RENDERTARGET, nullptr,
            srgb, aa, aaHint);
        defMap.emplace(def, newTex);
        return newTex;
    }

    auto i = mTexturesByDef.emplace(def, TextureList()).first;

    CompositorInstance* previous = inst->getChain()->getPreviousInstance(inst);
    CompositorInstance* next = inst->getChain()->getNextInstance(inst);

    TexturePtr ret;
    TextureList& texList = i->second;
    // iterate over the existing textures and check if we can re-use
    for (auto t = texList.begin(); t != texList.end(); ++t)
    {
        TexturePtr& tex = *t;
        // check not already used
        if (texturesAssigned.find(tex.get()) == texturesAssigned.end())
        {
            bool allowReuse = true;
            // ok, we didn't use this one already
            // however, there is an edge case where if we re-use a texture
            // which has an 'input previous' pass, and it is chained from another
            // compositor, we can end up trying to use the same texture for both
            // so, never allow a texture with an input previous pass to be 
            // shared with its immediate predecessor in the chain
            if (isInputPreviousTarget(inst, localName))
            {
                // Check whether this is also an input to the output target of previous
                // can't use CompositorInstance::mPreviousInstance, only set up
                // during compile
                if (previous && isInputToOutputTarget(previous, tex))
                    allowReuse = false;
            }
            // now check the other way around since we don't know what order they're bound in
            if (isInputToOutputTarget(inst, localName))
            {
                
                if (next && isInputPreviousTarget(next, tex))
                    allowReuse = false;
            }
            
            if (allowReuse)
            {
                ret = tex;
                break;
            }

        }
    }

    if (!ret)
    {
        // ok, we need to create a new one
        ret = TextureManager::getSingleton().createManual(
            name, 
            ResourceGroupManager::INTERNAL_RESOURCE_GROUP_NAME, TEX_TYPE_2D, 
            w, h, 0, f, TU_RENDERTARGET, nullptr,
            srgb, aa, aaHint); 

        texList.push_back(ret);

    }

    // record that we used this one in the requester's list
    texturesAssigned.insert(ret.get());


    return ret;
}
//---------------------------------------------------------------------
auto CompositorManager::isInputPreviousTarget(CompositorInstance* inst, const Ogre::String& localName) -> bool
{
    const CompositionTechnique::TargetPasses& passes = inst->getTechnique()->getTargetPasses();
    CompositionTechnique::TargetPasses::const_iterator it;
    for (it = passes.begin(); it != passes.end(); ++it)
    {
        CompositionTargetPass* tp = *it;
        if (tp->getInputMode() == CompositionTargetPass::IM_PREVIOUS &&
            tp->getOutputName() == localName)
        {
            return true;
        }

    }

    return false;

}
//---------------------------------------------------------------------
auto CompositorManager::isInputPreviousTarget(CompositorInstance* inst, TexturePtr tex) -> bool
{
    const CompositionTechnique::TargetPasses& passes = inst->getTechnique()->getTargetPasses();
    CompositionTechnique::TargetPasses::const_iterator it;
    for (it = passes.begin(); it != passes.end(); ++it)
    {
        CompositionTargetPass* tp = *it;
        if (tp->getInputMode() == CompositionTargetPass::IM_PREVIOUS)
        {
            // Don't have to worry about an MRT, because no MRT can be input previous
            TexturePtr t = inst->getTextureInstance(tp->getOutputName(), 0);
            if (t && t.get() == tex.get())
                return true;
        }

    }

    return false;

}
//---------------------------------------------------------------------
auto CompositorManager::isInputToOutputTarget(CompositorInstance* inst, const Ogre::String& localName) -> bool
{
    CompositionTargetPass* tp = inst->getTechnique()->getOutputTargetPass();
    auto pit = tp->getPasses().begin();

    for (;pit != tp->getPasses().end(); ++pit)
    {
        CompositionPass* p = *pit;
        for (size_t i = 0; i < p->getNumInputs(); ++i)
        {
            if (p->getInput(i).name == localName)
                return true;
        }
    }

    return false;

}
//---------------------------------------------------------------------()
auto CompositorManager::isInputToOutputTarget(CompositorInstance* inst, TexturePtr tex) -> bool
{
    CompositionTargetPass* tp = inst->getTechnique()->getOutputTargetPass();
    auto pit = tp->getPasses().begin();

    for (;pit != tp->getPasses().end(); ++pit)
    {
        CompositionPass* p = *pit;
        for (size_t i = 0; i < p->getNumInputs(); ++i)
        {
            TexturePtr t = inst->getTextureInstance(p->getInput(i).name, 0);
            if (t && t.get() == tex.get())
                return true;
        }
    }

    return false;

}
//---------------------------------------------------------------------
void CompositorManager::freePooledTextures(bool onlyIfUnreferenced)
{
    if (onlyIfUnreferenced)
    {
        for (auto i = mTexturesByDef.begin(); i != mTexturesByDef.end(); ++i)
        {
            TextureList& texList = i->second;
            for (auto j = texList.begin(); j != texList.end();)
            {
                // if the resource system, plus this class, are the only ones to have a reference..
                // NOTE: any material references will stop this texture getting freed (e.g. compositor demo)
                // until this routine is called again after the material no longer references the texture
                if (j->use_count() == ResourceGroupManager::RESOURCE_SYSTEM_NUM_REFERENCE_COUNTS + 1)
                {
                    TextureManager::getSingleton().remove((*j)->getHandle());
                    j = texList.erase(j);
                }
                else
                    ++j;
            }
        }
        for (auto i = mChainTexturesByDef.begin(); i != mChainTexturesByDef.end(); ++i)
        {
            TextureDefMap& texMap = i->second;
            for (auto j = texMap.begin(); j != texMap.end();) 
            {
                const TexturePtr& tex = j->second;
                if (tex.use_count() == ResourceGroupManager::RESOURCE_SYSTEM_NUM_REFERENCE_COUNTS + 1)
                {
                    TextureManager::getSingleton().remove(tex->getHandle());
                    texMap.erase(j++);
                }
                else
                    ++j;
            }
        }
    }
    else
    {
        // destroy all
        mTexturesByDef.clear();
        mChainTexturesByDef.clear();
    }

}
//---------------------------------------------------------------------
void CompositorManager::registerCompositorLogic(const String& name, CompositorLogic* logic)
{   
    OgreAssert(!name.empty(), "Compositor logic name must not be empty");
    if (mCompositorLogics.find(name) != mCompositorLogics.end())
    {
        OGRE_EXCEPT(Exception::ERR_DUPLICATE_ITEM,
            "Compositor logic '" + name + "' already exists.",
            "CompositorManager::registerCompositorLogic");
    }
    mCompositorLogics[name] = logic;
}
//---------------------------------------------------------------------
void CompositorManager::unregisterCompositorLogic(const String& name)
{
    auto itor = mCompositorLogics.find(name);
    if( itor == mCompositorLogics.end() )
    {
        OGRE_EXCEPT(Exception::ERR_ITEM_NOT_FOUND,
            "Compositor logic '" + name + "' not registered.",
            "CompositorManager::unregisterCompositorLogic");
    }

    mCompositorLogics.erase( itor );
}
//---------------------------------------------------------------------
auto CompositorManager::getCompositorLogic(const String& name) -> CompositorLogic*
{
    auto it = mCompositorLogics.find(name);
    if (it == mCompositorLogics.end())
    {
        OGRE_EXCEPT(Exception::ERR_ITEM_NOT_FOUND,
            "Compositor logic '" + name + "' not registered.",
            "CompositorManager::getCompositorLogic");
    }
    return it->second;
}
//---------------------------------------------------------------------
auto CompositorManager::hasCompositorLogic(const String& name) -> bool
{
	return mCompositorLogics.find(name) != mCompositorLogics.end();
}
//---------------------------------------------------------------------
void CompositorManager::registerCustomCompositionPass(const String& name, CustomCompositionPass* logic)
{   
    OgreAssert(!name.empty(), "Compositor pass name must not be empty");
    if (mCustomCompositionPasses.find(name) != mCustomCompositionPasses.end())
    {
        OGRE_EXCEPT(Exception::ERR_DUPLICATE_ITEM,
            "Custom composition pass  '" + name + "' already exists.",
            "CompositorManager::registerCustomCompositionPass");
    }
    mCustomCompositionPasses[name] = logic;
}
//---------------------------------------------------------------------
void CompositorManager::unregisterCustomCompositionPass(const String& name)
{	
	auto itor = mCustomCompositionPasses.find(name);
	if( itor == mCustomCompositionPasses.end() )
	{
		OGRE_EXCEPT(Exception::ERR_ITEM_NOT_FOUND,
			"Custom composition pass '" + name + "' not registered.",
			"CompositorManager::unRegisterCustomCompositionPass");
	}
	mCustomCompositionPasses.erase( itor );
}
//---------------------------------------------------------------------
auto CompositorManager::hasCustomCompositionPass(const String& name) -> bool
{
	return mCustomCompositionPasses.find(name) != mCustomCompositionPasses.end();
}
//---------------------------------------------------------------------
auto CompositorManager::getCustomCompositionPass(const String& name) -> CustomCompositionPass*
{
    auto it = mCustomCompositionPasses.find(name);
    if (it == mCustomCompositionPasses.end())
    {
        OGRE_EXCEPT(Exception::ERR_ITEM_NOT_FOUND,
            "Custom composition pass '" + name + "' not registered.",
            "CompositorManager::getCustomCompositionPass");
    }
    return it->second;
}
//-----------------------------------------------------------------------
void CompositorManager::_relocateChain( Viewport* sourceVP, Viewport* destVP )
{
    if (sourceVP != destVP)
    {
        CompositorChain *chain = getCompositorChain(sourceVP);
        Ogre::RenderTarget *srcTarget = sourceVP->getTarget();
        Ogre::RenderTarget *dstTarget = destVP->getTarget();
        if (srcTarget != dstTarget)
        {
            srcTarget->removeListener(chain);
            dstTarget->addListener(chain);
        }
        chain->_notifyViewport(destVP);
        mChains.erase(sourceVP);
        mChains[destVP] = chain;
    }
}
//-----------------------------------------------------------------------
}
