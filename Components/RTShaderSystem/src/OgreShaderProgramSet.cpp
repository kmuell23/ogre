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

module Ogre.Components.RTShaderSystem;

import :ShaderProgram;
import :ShaderProgramSet;

import Ogre.Core;

import <memory>;
import <utility>;

namespace Ogre::RTShader {

//-----------------------------------------------------------------------------
ProgramSet::ProgramSet() = default;

//-----------------------------------------------------------------------------
ProgramSet::~ProgramSet() = default;

//-----------------------------------------------------------------------------
void ProgramSet::setCpuProgram(std::unique_ptr<Program>&& program)
{
    using enum GpuProgramType;
    switch(program->getType())
    {
    case VERTEX_PROGRAM:
        mVSCpuProgram = std::move(program);
        break;
    case FRAGMENT_PROGRAM:
        mPSCpuProgram = std::move(program);
        break;
    default:
        OGRE_EXCEPT(ExceptionCodes::NOT_IMPLEMENTED, "", "");
        break;
    }
}

//-----------------------------------------------------------------------------
auto ProgramSet::getCpuProgram(GpuProgramType type) const -> Program*
{
    using enum GpuProgramType;
    switch(type)
    {
    case VERTEX_PROGRAM:
        return mVSCpuProgram.get();
    case FRAGMENT_PROGRAM:
        return mPSCpuProgram.get();
    default:
        return nullptr;
    }
}
//-----------------------------------------------------------------------------
void ProgramSet::setGpuProgram(const GpuProgramPtr& program)
{
    using enum GpuProgramType;
    switch(program->getType())
    {
    case VERTEX_PROGRAM:
        mVSGpuProgram = program;
        break;
    case FRAGMENT_PROGRAM:
        mPSGpuProgram = program;
        break;
    default:
        OGRE_EXCEPT(ExceptionCodes::NOT_IMPLEMENTED, "", "");
        break;
    }
}

//-----------------------------------------------------------------------------
auto ProgramSet::getGpuProgram(GpuProgramType type) const -> const GpuProgramPtr&
{
    using enum GpuProgramType;
    switch(type)
    {
    case VERTEX_PROGRAM:
        return mVSGpuProgram;
        break;
    case FRAGMENT_PROGRAM:
        return mPSGpuProgram;
        break;
    default:
        break;
    }

    static GpuProgramPtr nullPtr;
    return nullPtr;
}

}
