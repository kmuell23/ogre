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
#ifndef OGRE_COMPONENTS_RTSHADERSYSTEM_HLSLPROGRAMPROCESSOR_H
#define OGRE_COMPONENTS_RTSHADERSYSTEM_HLSLPROGRAMPROCESSOR_H

#include "OgrePrerequisites.hpp"
#include "OgreShaderProgramProcessor.hpp"


namespace Ogre {
namespace RTShader {
class ProgramSet;

/** \addtogroup Optional
*  @{
*/
/** \addtogroup RTShader
*  @{
*/

/** CG Language program processor class.
*/
class HLSLProgramProcessor : public ProgramProcessor
{

    // Interface.
public: 

    /** Class constructor.
    */
    HLSLProgramProcessor();

    /** Class destructor */
    ~HLSLProgramProcessor() override;

    /** Return the target language of this processor. */
    [[nodiscard]] virtual const String& getTargetLanguage() const noexcept { return TargetLanguage; }

    /** 
    @see ProgramProcessor::preCreateGpuPrograms
    */
    bool preCreateGpuPrograms(ProgramSet* programSet) override;
    /** 
    @see ProgramProcessor::postCreateGpuPrograms
    */
    bool postCreateGpuPrograms(ProgramSet* programSet) override;

    static String TargetLanguage;

};


/** @} */
/** @} */

}
}

#endif

