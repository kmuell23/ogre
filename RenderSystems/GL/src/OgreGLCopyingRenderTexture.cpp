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
module Ogre.RenderSystems.GL;

import :CopyingRenderTexture;
import :HardwarePixelBuffer;

import Ogre.Core;
import Ogre.RenderSystems.GLSupport;

import <string>;

namespace Ogre {

//-----------------------------------------------------------------------------  
    GLCopyingRenderTexture::GLCopyingRenderTexture(GLCopyingRTTManager *manager, 
        std::string_view name, const GLSurfaceDesc &target, bool writeGamma, uint fsaa):
        GLRenderTexture(name, target, writeGamma, fsaa)
    {
    }
    void GLCopyingRenderTexture::getCustomAttribute(std::string_view name, void* pData)
    {
        if( name == GLRenderTexture::CustomAttributeString_TARGET )
        {
            GLSurfaceDesc &target = *static_cast<GLSurfaceDesc*>(pData);
            target.buffer = static_cast<GLHardwarePixelBufferCommon*>(mBuffer);
            target.zoffset = mZOffset;
        }
    }
//-----------------------------------------------------------------------------  
    void GLCopyingRTTManager::unbind(RenderTarget *target)
    {
        // Copy on unbind
        GLSurfaceDesc surface;
        surface.buffer = nullptr;
        target->getCustomAttribute(GLRenderTexture::CustomAttributeString_TARGET, &surface);
        if(surface.buffer)
            static_cast<GLTextureBuffer*>(surface.buffer)->copyFromFramebuffer(surface.zoffset);
    }
}
