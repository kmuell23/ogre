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
export module Ogre.RenderSystems.GL:GpuProgram;

import Ogre.Core;
import Ogre.RenderSystems.GL.glad;

export
namespace Ogre {
class ResourceManager;

    struct GLGpuProgramBase
    {
        virtual ~GLGpuProgramBase() {}
        /// Execute the binding functions for this program
        virtual void bindProgram(void) = 0;
        /// Execute the binding functions for this program
        virtual void unbindProgram(void) = 0;
        /// Execute the param binding functions for this program
        virtual void bindProgramParameters(GpuProgramParametersSharedPtr params, uint16 mask) = 0;
        /// Test whether attribute index for a given semantic is valid
        virtual bool isAttributeValid(VertexElementSemantic semantic, uint index);
    };

    /** Generalised low-level GL program, can be applied to multiple types (eg ARB and NV) */
    class GLGpuProgram : public GpuProgram, public GLGpuProgramBase
    {
    public:
        GLGpuProgram(ResourceManager* creator, const String& name, ResourceHandle handle,
            const String& group, bool isManual = false, ManualResourceLoader* loader = 0);
        virtual ~GLGpuProgram();
    protected:

        GLuint mProgramID;
    };

    /** Specialisation of the GL low-level program for ARB programs. */
    class GLArbGpuProgram : public GLGpuProgram
    {
    public:
        GLArbGpuProgram(ResourceManager* creator, const String& name, ResourceHandle handle,
            const String& group, bool isManual = false, ManualResourceLoader* loader = 0);
        virtual ~GLArbGpuProgram();

        /// Execute the binding functions for this program
        void bindProgram(void);
        /// Execute the unbinding functions for this program
        void unbindProgram(void);
        /// Execute the param binding functions for this program
        void bindProgramParameters(GpuProgramParametersSharedPtr params, uint16 mask);

        /// Get the GL type for the program
        GLenum getProgramType(void) const;

    protected:
        void loadFromSource(void);
        /// @copydoc Resource::unloadImpl
        void unloadImpl(void);

    };



} // namespace Ogre
