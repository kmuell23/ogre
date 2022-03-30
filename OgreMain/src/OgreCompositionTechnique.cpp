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
#include <string>

#include "OgreCompositionTargetPass.h"
#include "OgreCompositionTechnique.h"
#include "OgreException.h"
#include "OgreIteratorWrapper.h"
#include "OgreRenderSystem.h"
#include "OgreRenderSystemCapabilities.h"
#include "OgreRoot.h"
#include "OgreStableHeaders.h"
#include "OgreTextureManager.h"

namespace Ogre {
class Compositor;

CompositionTechnique::CompositionTechnique(Compositor *parent):
    mParent(parent)
{
    mOutputTarget = new CompositionTargetPass(this);
}
//-----------------------------------------------------------------------
CompositionTechnique::~CompositionTechnique()
{
    removeAllTextureDefinitions();
    removeAllTargetPasses();
    delete  mOutputTarget;
}
//-----------------------------------------------------------------------
CompositionTechnique::TextureDefinition *CompositionTechnique::createTextureDefinition(const String &name)
{
    if(getTextureDefinition(name))
        OGRE_EXCEPT(Exception::ERR_DUPLICATE_ITEM, "Texture '"+name+"' already exists");

    TextureDefinition *t = new TextureDefinition();
    t->name = name;
    mTextureDefinitions.push_back(t);
    return t;
}
//-----------------------------------------------------------------------

void CompositionTechnique::removeTextureDefinition(size_t index)
{
    assert (index < mTextureDefinitions.size() && "Index out of bounds.");
    TextureDefinitions::iterator i = mTextureDefinitions.begin() + index;
    delete (*i);
    mTextureDefinitions.erase(i);
}
//---------------------------------------------------------------------
CompositionTechnique::TextureDefinition *CompositionTechnique::getTextureDefinition(const String& name) const
{
    auto iend = mTextureDefinitions.end();
    for (auto i = mTextureDefinitions.begin(); i != iend; ++i)
    {
        if ((*i)->name == name)
            return *i;
    }

    return 0;

}
//-----------------------------------------------------------------------
void CompositionTechnique::removeAllTextureDefinitions()
{
    TextureDefinitions::iterator i, iend;
    iend = mTextureDefinitions.end();
    for (i = mTextureDefinitions.begin(); i != iend; ++i)
    {
        delete (*i);
    }
    mTextureDefinitions.clear();
}

//-----------------------------------------------------------------------
CompositionTargetPass *CompositionTechnique::createTargetPass()
{
    CompositionTargetPass *t = new CompositionTargetPass(this);
    mTargetPasses.push_back(t);
    return t;
}
//-----------------------------------------------------------------------

void CompositionTechnique::removeTargetPass(size_t index)
{
    assert (index < mTargetPasses.size() && "Index out of bounds.");
    TargetPasses::iterator i = mTargetPasses.begin() + index;
    delete (*i);
    mTargetPasses.erase(i);
}
//-----------------------------------------------------------------------
void CompositionTechnique::removeAllTargetPasses()
{
    TargetPasses::iterator i, iend;
    iend = mTargetPasses.end();
    for (i = mTargetPasses.begin(); i != iend; ++i)
    {
        delete (*i);
    }
    mTargetPasses.clear();
}

//-----------------------------------------------------------------------
bool CompositionTechnique::isSupported(bool acceptTextureDegradation)
{
    // A technique is supported if all materials referenced have a supported
    // technique, and the intermediate texture formats requested are supported
    // Material support is a cast-iron requirement, but if no texture formats 
    // are directly supported we can let the rendersystem create the closest 
    // match for the least demanding technique
    

    // Check output target pass is supported
    if (!mOutputTarget->_isSupported())
    {
        return false;
    }

    // Check all target passes is supported
    TargetPasses::iterator pi, piend;
    piend = mTargetPasses.end();
    for (pi = mTargetPasses.begin(); pi != piend; ++pi)
    {
        CompositionTargetPass* targetPass = *pi;
        if (!targetPass->_isSupported())
        {
            return false;
        }
    }

    TextureDefinitions::iterator i, iend;
    iend = mTextureDefinitions.end();
    TextureManager& texMgr = TextureManager::getSingleton();
    for (i = mTextureDefinitions.begin(); i != iend; ++i)
    {
        TextureDefinition* td = *i;

        // Firstly check MRTs
        if (td->formatList.size() > 
            Root::getSingleton().getRenderSystem()->getCapabilities()->getNumMultiRenderTargets())
        {
            return false;
        }


        for (PixelFormatList::iterator pfi = td->formatList.begin(); pfi != td->formatList.end(); ++pfi)
        {
            // Check whether equivalent supported
            // Need a format which is the same number of bits to pass
            bool accepted = texMgr.isEquivalentFormatSupported(td->type, *pfi, TU_RENDERTARGET);
            if(!accepted && acceptTextureDegradation)
            {
                // Don't care about exact format so long as something is supported
                accepted = texMgr.getNativeFormat(td->type, *pfi, TU_RENDERTARGET) != PF_UNKNOWN;
            }

            if(!accepted)
                return false;
        }

        //Check all render targets have same number of bits
        if( !Root::getSingleton().getRenderSystem()->getCapabilities()->
            hasCapability( RSC_MRT_DIFFERENT_BIT_DEPTHS ) && !td->formatList.empty() )
        {
            PixelFormat nativeFormat = texMgr.getNativeFormat( td->type, td->formatList.front(),
                                                                TU_RENDERTARGET );
            size_t nativeBits = PixelUtil::getNumElemBits( nativeFormat );
            for( PixelFormatList::iterator pfi = td->formatList.begin()+1;
                    pfi != td->formatList.end(); ++pfi )
            {
                PixelFormat nativeTmp = texMgr.getNativeFormat( td->type, *pfi, TU_RENDERTARGET );
                if( PixelUtil::getNumElemBits( nativeTmp ) != nativeBits )
                {
                    return false;
                }
            }
        }
    }
    
    // Must be ok
    return true;
}
//-----------------------------------------------------------------------
Compositor *CompositionTechnique::getParent()
{
    return mParent;
}
//---------------------------------------------------------------------
void CompositionTechnique::setSchemeName(const String& schemeName)
{
    mSchemeName = schemeName;
}

}
