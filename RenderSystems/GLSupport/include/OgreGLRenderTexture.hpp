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

#ifndef OGRE_RENDERSYSTEMS_GLSUPPORT_RENDERTEXTURE_H
#define OGRE_RENDERSYSTEMS_GLSUPPORT_RENDERTEXTURE_H

#include <cstddef>
#include <map>
#include <vector>

#include "OgreConfig.hpp"
#include "OgreGLRenderTarget.hpp"
#include "OgrePixelFormat.hpp"
#include "OgrePlatform.hpp"
#include "OgrePrerequisites.hpp"
#include "OgreRenderTexture.hpp"
#include "OgreSingleton.hpp"

namespace Ogre {
    class GLHardwarePixelBufferCommon;
class GLContext;
class RenderTarget;

    /** GL surface descriptor. Points to a 2D surface that can be rendered to.
     */
    struct GLSurfaceDesc
    {
    public:
        GLHardwarePixelBufferCommon *buffer{nullptr};
        uint32 zoffset{0};
        uint numSamples{0};

        GLSurfaceDesc()  = default;
    };

    /// Frame Buffer Object abstraction
    class GLFrameBufferObjectCommon
    {
    public:
        GLFrameBufferObjectCommon(int32 fsaa);
        virtual ~GLFrameBufferObjectCommon() = default;

        /** Bind FrameBufferObject. Attempt to bind on incompatible GL context will cause FBO destruction and optional recreation.
        */
        virtual bool bind(bool recreateIfNeeded) = 0;

        /** Bind a surface to a certain attachment point.
            attachment: 0..OGRE_MAX_MULTIPLE_RENDER_TARGETS-1
        */
        void bindSurface(size_t attachment, const GLSurfaceDesc &target);
        /** Unbind attachment
        */
        void unbindSurface(size_t attachment);

        /// Accessors
        [[nodiscard]] int32 getFSAA() const noexcept { return mNumSamples; }
        [[nodiscard]] uint32 getWidth() const noexcept;
        [[nodiscard]] uint32 getHeight() const noexcept;
        [[nodiscard]] PixelFormat getFormat() const;

        [[nodiscard]] GLContext* getContext() const noexcept { return mContext; }
        /// Get the GL id for the FBO
        [[nodiscard]] uint32 getGLFBOID() const noexcept { return mFB; }
        /// Get the GL id for the multisample FBO
        [[nodiscard]] uint32 getGLMultisampleFBOID() const noexcept { return mMultisampleFB; }

        [[nodiscard]] const GLSurfaceDesc &getSurface(size_t attachment) const { return mColour[attachment]; }

        void notifyContextDestroyed(GLContext* context) { if(mContext == context) { mContext = nullptr; mFB = 0; mMultisampleFB = 0; } }
    protected:
        GLSurfaceDesc mDepth;
        GLSurfaceDesc mStencil;
        // Arbitrary number of texture surfaces
        GLSurfaceDesc mColour[OGRE_MAX_MULTIPLE_RENDER_TARGETS];
        /// Context that was used to create FBO. It could already be destroyed, so do not dereference this field blindly
        GLContext* mContext;
        uint32 mFB{0};
        uint32 mMultisampleFB{0};
        int32 mNumSamples;

        /** Initialise object (find suitable depth and stencil format).
            Must be called every time the bindings change.
            It fails with an exception (ERR_INVALIDPARAMS) if:
            - Attachment point 0 has no binding
            - Not all bound surfaces have the same size
            - Not all bound surfaces have the same internal format
        */
        virtual void initialise() = 0;
    };

    /** Base class for GL Render Textures
     */
    class GLRenderTexture : public RenderTexture, public GLRenderTarget
    {
    public:
        GLRenderTexture(const String &name, const GLSurfaceDesc &target, bool writeGamma, uint fsaa);
        [[nodiscard]] bool requiresTextureFlipping() const noexcept override { return true; }

        static const String CustomAttributeString_FBO;
        static const String CustomAttributeString_TARGET;
        static const String CustomAttributeString_GLCONTEXT;
    };

    /** Manager/factory for RenderTextures.
     */
    class GLRTTManager : public Singleton<GLRTTManager>
    {
    public:
        GLRTTManager();
        virtual ~GLRTTManager();

        /** Create a texture rendertarget object
         */
        virtual RenderTexture *createRenderTexture(const String &name, const GLSurfaceDesc &target, bool writeGamma, uint fsaa) = 0;

        /** Release a render buffer. Ignore silently if surface.buffer is 0.
         */
        void releaseRenderBuffer(const GLSurfaceDesc &surface);

        /** Check if a certain format is usable as FBO rendertarget format
        */
        bool checkFormat(PixelFormat format) { return mProps[format].valid; }

        /** Bind a certain render target.
            @note only needed for FBO RTTs
         */
        virtual void bind(RenderTarget *target) {}

        /** Unbind a certain render target. This is called before binding another RenderTarget, and
            before the context is switched. It can be used to do a copy, or just be a noop if direct
            binding is used.
            @note only needed for Copying or PBuffer RTTs
        */
        virtual void unbind(RenderTarget *target) {}

        virtual void getBestDepthStencil(PixelFormat internalFormat, uint32 *depthFormat, uint32 *stencilFormat)
        {
            *depthFormat = 0;
            *stencilFormat = 0;
        }

        /** Get the closest supported alternative format. If format is supported, returns format.
         */
        PixelFormat getSupportedAlternative(PixelFormat format);

        /// @copydoc Singleton::getSingleton()
        static GLRTTManager& getSingleton() noexcept;
        /// @copydoc Singleton::getSingleton()
        static GLRTTManager* getSingletonPtr() noexcept;
    protected:
        /** Frame Buffer Object properties for a certain texture format.
         */
        struct FormatProperties
        {
            bool valid; // This format can be used as RTT (FBO)

            /** Allowed modes/properties for this pixel format
             */
            struct Mode
            {
                uchar depth;     // Depth format (0=no depth)
                uchar stencil;   // Stencil format (0=no stencil)
            };

            std::vector<Mode> modes;
        };
        /** Properties for all internal formats defined by OGRE
         */
        FormatProperties mProps[PF_COUNT];

        /** Stencil and depth renderbuffers of the same format are re-used between surfaces of the
            same size and format. This can save a lot of memory when a large amount of rendertargets
            are used.
        */
        struct RBFormat
        {
            RBFormat(uint inFormat, size_t inWidth, size_t inHeight, uint fsaa)
                : format(inFormat), width(inWidth), height(inHeight), samples(fsaa)
            {
            }
            RBFormat() = default;
            uint format;
            size_t width;
            size_t height;
            uint samples;
            // Overloaded comparison operator for usage in map
            bool operator < (const RBFormat &other) const
            {
                if(format < other.format)
                {
                    return true;
                }
                else if(format == other.format)
                {
                    if(width < other.width)
                    {
                        return true;
                    }
                    else if(width == other.width)
                    {
                        if(height < other.height)
                            return true;
                        else if (height == other.height)
                        {
                            if (samples < other.samples)
                                return true;
                        }
                    }
                }
                return false;
            }
        };
        struct RBRef
        {
            RBRef() = default;
            RBRef(GLHardwarePixelBufferCommon* inBuffer) : buffer(inBuffer), refcount(1) {}
            GLHardwarePixelBufferCommon* buffer;
            size_t refcount;
        };
        using RenderBufferMap = std::map<RBFormat, RBRef>;
        RenderBufferMap mRenderBufferMap;
    };

}

#endif
