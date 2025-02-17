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

module Ogre.Core;

import :CompositionPass;
import :CompositionTargetPass;
import :MaterialManager;
import :RenderSystem;
import :Root;

namespace Ogre {
CompositionTargetPass::CompositionTargetPass(CompositionTechnique *parent):
    mParent(parent),
    
    mMaterialScheme(MaterialManager::DEFAULT_SCHEME_NAME)
    
{
    if (Root::getSingleton().getRenderSystem())
    {
        mMaterialScheme = Root::getSingleton().getRenderSystem()->_getDefaultViewportMaterialScheme();
    }
}
//-----------------------------------------------------------------------
CompositionTargetPass::~CompositionTargetPass()
{
    removeAllPasses();
}
//-----------------------------------------------------------------------
void CompositionTargetPass::setInputMode(InputMode mode)
{
    mInputMode = mode;
}
//-----------------------------------------------------------------------
auto CompositionTargetPass::getInputMode() const -> CompositionTargetPass::InputMode
{
    return mInputMode;
}
//-----------------------------------------------------------------------
void CompositionTargetPass::setOutputName(std::string_view out)
{
    mOutputName = out;
}
//-----------------------------------------------------------------------
auto CompositionTargetPass::getOutputName() const -> std::string_view
{
    return mOutputName;
}
//-----------------------------------------------------------------------
void CompositionTargetPass::setOnlyInitial(bool value)
{
    mOnlyInitial = value;
}
//-----------------------------------------------------------------------
auto CompositionTargetPass::getOnlyInitial() noexcept -> bool
{
    return mOnlyInitial;
}
//-----------------------------------------------------------------------
void CompositionTargetPass::setVisibilityMask(QueryTypeMask mask)
{
    mVisibilityMask = mask;
}
//-----------------------------------------------------------------------
auto CompositionTargetPass::getVisibilityMask() noexcept -> QueryTypeMask
{
    return mVisibilityMask;
}
//-----------------------------------------------------------------------
void CompositionTargetPass::setLodBias(float bias)
{
    mLodBias = bias;
}
//-----------------------------------------------------------------------
auto CompositionTargetPass::getLodBias() noexcept -> float
{
    return mLodBias;
}
//-----------------------------------------------------------------------
void CompositionTargetPass::setMaterialScheme(std::string_view schemeName)
{
    mMaterialScheme = schemeName;
}
//-----------------------------------------------------------------------
auto CompositionTargetPass::getMaterialScheme() const noexcept -> std::string_view
{
    return mMaterialScheme;
}
//-----------------------------------------------------------------------
void CompositionTargetPass::setShadowsEnabled(bool enabled)
{
    mShadowsEnabled = enabled;
}
//-----------------------------------------------------------------------
auto CompositionTargetPass::getShadowsEnabled() const noexcept -> bool
{
    return mShadowsEnabled;
}
//-----------------------------------------------------------------------
auto CompositionTargetPass::createPass(CompositionPass::PassType type) -> CompositionPass *
{
    auto *t = new CompositionPass(this);
    t->setType(type);
    mPasses.push_back(t);
    return t;
}
//-----------------------------------------------------------------------

void CompositionTargetPass::removePass(size_t index)
{
    assert (index < mPasses.size() && "Index out of bounds.");
    auto i = mPasses.begin() + index;
    delete (*i);
    mPasses.erase(i);
}
//-----------------------------------------------------------------------
void CompositionTargetPass::removeAllPasses()
{
    for (auto & mPasse : mPasses)
    {
        delete mPasse;
    }
    mPasses.clear();
}

//-----------------------------------------------------------------------
auto CompositionTargetPass::getParent() -> CompositionTechnique *
{
    return mParent;
}

//-----------------------------------------------------------------------
auto CompositionTargetPass::_isSupported() -> bool
{
    // A target pass is supported if all passes are supported

    for (CompositionPass* pass : mPasses)
    {
        if (!pass->_isSupported())
        {
            return false;
        }
    }

    return true;
}

}
