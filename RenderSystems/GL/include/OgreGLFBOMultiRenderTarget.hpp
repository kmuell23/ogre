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

export module Ogre.RenderSystems.GL:FBOMultiRenderTarget;

export import :FrameBufferObject;

export import Ogre.Core;
export import Ogre.RenderSystems.GLSupport;

export
namespace Ogre {
    /** MultiRenderTarget for GL. Requires the FBO extension.
    */
    class GLFBOMultiRenderTarget : public MultiRenderTarget, public GLRenderTarget
    {
    public:
        GLFBOMultiRenderTarget(GLFBOManager *manager, std::string_view name);
        ~GLFBOMultiRenderTarget() override;

        void getCustomAttribute( std::string_view name, void *pData ) override;
        [[nodiscard]] auto getContext() const noexcept -> GLContext* override { return fbo.getContext(); }
        auto getFBO() noexcept -> GLFrameBufferObjectCommon* override { return &fbo; }

        [[nodiscard]] auto requiresTextureFlipping() const noexcept -> bool override { return true; }

        /// Override so we can attach the depth buffer to the FBO
        auto attachDepthBuffer( DepthBuffer *depthBuffer ) -> bool override;
        void detachDepthBuffer() override;
        void _detachDepthBuffer() override;
    private:
        void bindSurfaceImpl(size_t attachment, RenderTexture *target) override;
        void unbindSurfaceImpl(size_t attachment) override; 
        GLFrameBufferObject fbo;
    };

}
