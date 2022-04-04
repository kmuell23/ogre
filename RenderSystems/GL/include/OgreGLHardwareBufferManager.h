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

export module Ogre.RenderSystems.GL:HardwareBufferManager;

import Ogre.Core;
import Ogre.RenderSystems.GL.glad;

export
namespace Ogre {

    class GLStateCacheManager;
class GLRenderSystem;

// Default threshold at which glMapBuffer becomes more efficient than glBufferSubData (32k?)
#   define OGRE_GL_DEFAULT_MAP_BUFFER_THRESHOLD (1024 * 32)

    /** Implementation of HardwareBufferManager for OpenGL. */
    class GLHardwareBufferManager : public HardwareBufferManager
    {
    protected:
        GLRenderSystem* mRenderSystem;
        char* mScratchBufferPool;
        size_t mMapBufferThreshold;

    public:
        GLHardwareBufferManager();
        ~GLHardwareBufferManager();
        /// Creates a vertex buffer
        HardwareVertexBufferSharedPtr createVertexBuffer(size_t vertexSize, 
            size_t numVerts, HardwareBuffer::Usage usage, bool useShadowBuffer = false);
        /// Create a hardware vertex buffer
        HardwareIndexBufferSharedPtr createIndexBuffer(
            HardwareIndexBuffer::IndexType itype, size_t numIndexes, 
            HardwareBuffer::Usage usage, bool useShadowBuffer = false);
        /// Create a render to vertex buffer
        RenderToVertexBufferSharedPtr createRenderToVertexBuffer();
        /// Utility function to get the correct GL usage based on HBU's
        static GLenum getGLUsage(unsigned int usage);

        /// Utility function to get the correct GL type based on VET's
        static GLenum getGLType(unsigned int type);

        GLStateCacheManager * getStateCacheManager();

        /** Allocator method to allow us to use a pool of memory as a scratch
            area for hardware buffers. This is because glMapBuffer is incredibly
            inefficient, seemingly no matter what options we give it. So for the
            period of lock/unlock, we will instead allocate a section of a local
            memory pool, and use glBufferSubDataARB / glGetBufferSubDataARB
            instead.
        */
        void* allocateScratch(uint32 size);

        /// @see allocateScratch
        void deallocateScratch(void* ptr);

        /** Threshold after which glMapBuffer is used and not glBufferSubData
        */
        size_t getGLMapBufferThreshold() const;
        void setGLMapBufferThreshold( const size_t value );
    };
}
