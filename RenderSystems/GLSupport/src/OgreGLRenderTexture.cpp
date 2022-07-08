/*
  -----------------------------------------------------------------------------
  This source file is part of OGRE
  (Object-oriented Graphics Rendering Engine)
  For the latest info, see http://www.ogre3d.org

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

#include "OgreGLRenderTexture.hpp"

#include <cassert>
#include <utility>

#include "OgreGLHardwarePixelBufferCommon.hpp"
#include "OgreGLRenderSystemCommon.hpp"
#include "OgreRoot.hpp"

namespace Ogre {

    const String GLRenderTexture::CustomAttributeString_FBO = "FBO";
    const String GLRenderTexture::CustomAttributeString_TARGET = "TARGET";
    const String GLRenderTexture::CustomAttributeString_GLCONTEXT = "GLCONTEXT";

    template<> GLRTTManager* Singleton<GLRTTManager>::msSingleton = nullptr;

    GLFrameBufferObjectCommon::GLFrameBufferObjectCommon(int32 fsaa)
        :  mNumSamples(fsaa)
    {
        auto* rs = static_cast<GLRenderSystemCommon*>(
            Root::getSingleton().getRenderSystem());
        mContext = rs->_getCurrentContext();

        // Initialise state
        mDepth.buffer = nullptr;
        mStencil.buffer = nullptr;
        for(auto & x : mColour)
        {
            x.buffer=nullptr;
        }
    }

    void GLFrameBufferObjectCommon::bindSurface(size_t attachment, const GLSurfaceDesc &target)
    {
        assert(attachment < OGRE_MAX_MULTIPLE_RENDER_TARGETS);
        mColour[attachment] = target;
        // Re-initialise
        if(mColour[0].buffer)
            initialise();
    }

    void GLFrameBufferObjectCommon::unbindSurface(size_t attachment)
    {
        assert(attachment < OGRE_MAX_MULTIPLE_RENDER_TARGETS);
        mColour[attachment].buffer = nullptr;
        // Re-initialise if buffer 0 still bound
        if(mColour[0].buffer)
            initialise();
    }

    auto GLFrameBufferObjectCommon::getWidth() const noexcept -> uint32
    {
        assert(mColour[0].buffer);
        return mColour[0].buffer->getWidth();
    }
    auto GLFrameBufferObjectCommon::getHeight() const noexcept -> uint32
    {
        assert(mColour[0].buffer);
        return mColour[0].buffer->getHeight();
    }
    auto GLFrameBufferObjectCommon::getFormat() const -> PixelFormat
    {
        assert(mColour[0].buffer);
        return mColour[0].buffer->getFormat();
    }

    auto GLRTTManager::getSingletonPtr() noexcept -> GLRTTManager*
    {
        return msSingleton;
    }
    auto GLRTTManager::getSingleton() noexcept -> GLRTTManager&
    {
        assert( msSingleton );  return ( *msSingleton );
    }

    GLRTTManager::GLRTTManager() = default;
    // need to implement in cpp due to how Ogre::Singleton works
    GLRTTManager::~GLRTTManager() = default;

    auto GLRTTManager::getSupportedAlternative(PixelFormat format) -> PixelFormat
    {
        if (checkFormat(format))
        {
            return format;
        }

        if(PixelUtil::isDepth(format))
        {
            switch (format)
            {
                default:
                case PF_DEPTH16:
                    format = PF_FLOAT16_R;
                    break;
                case PF_DEPTH24_STENCIL8:
                case PF_DEPTH32F:
                case PF_DEPTH32:
                    format = PF_FLOAT32_R;
                    break;
            }
        }
        else
        {
            /// Find first alternative
            switch (PixelUtil::getComponentType(format))
            {
            case PCT_BYTE:
                format = PF_BYTE_RGBA; // native endian
                break;
            case PCT_SHORT:
                format = PF_SHORT_RGBA;
                break;
            case PCT_FLOAT16:
                format = PF_FLOAT16_RGBA;
                break;
            case PCT_FLOAT32:
                format = PF_FLOAT32_RGBA;
                break;
            default:
                break;
            }
        }

        if (checkFormat(format))
            return format;

        /// If none at all, return to default
        return PF_BYTE_RGBA; // native endian
    }

    void GLRTTManager::releaseRenderBuffer(const GLSurfaceDesc &surface)
    {
        if(surface.buffer == nullptr)
            return;
        RBFormat key(surface.buffer->getGLFormat(), surface.buffer->getWidth(), surface.buffer->getHeight(), surface.numSamples);
        auto it = mRenderBufferMap.find(key);
        if(it != mRenderBufferMap.end())
        {
            // Decrease refcount
            --it->second.refcount;
            if(it->second.refcount==0)
            {
                // If refcount reaches zero, delete buffer and remove from map
                delete it->second.buffer;
                mRenderBufferMap.erase(it);
                //                              std::cerr << "Destroyed renderbuffer of format " << std::hex << key.format << std::dec
                //                                      << " of " << key.width << "x" << key.height << std::endl;
            }
        }
    }

    GLRenderTexture::GLRenderTexture(StringView name,
                                               const GLSurfaceDesc &target,
                                               bool writeGamma,
                                               uint fsaa)
        : RenderTexture(target.buffer, target.zoffset)
    {
        mName = name;
        mHwGamma = writeGamma;
        mFSAA = fsaa;
    }
}
