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

#include <cstddef>
#include <vector>

#include "OgreAny.hpp"
#include "OgreMaterial.hpp"
#include "OgreMaterialManager.hpp"
#include "OgreMaterialSerializer.hpp"
#include "OgrePrerequisites.hpp"
#include "OgreShaderGenerator.hpp"
#include "OgreShaderMaterialSerializerListener.hpp"
#include "OgreSharedPtr.hpp"
#include "OgreTechnique.hpp"
#include "OgreTextureUnitState.hpp"
#include "OgreUserObjectBindings.hpp"

namespace Ogre {
class Pass;

namespace RTShader {

//-----------------------------------------------------------------------------
SGMaterialSerializerListener::SGMaterialSerializerListener()
{
    mSourceMaterial = nullptr;
}

//-----------------------------------------------------------------------------
void SGMaterialSerializerListener::materialEventRaised(MaterialSerializer* ser, 
                                                        MaterialSerializer::SerializeEvent event, 
                                                        bool& skip, const Material* mat)
{
    if (event == MaterialSerializer::MSE_PRE_WRITE)
    {
        MaterialPtr matPtr = MaterialManager::getSingleton().getByName(mat->getName());
        mSourceMaterial = matPtr.get();
        mSGPassList = ShaderGenerator::getSingleton().createSGPassList(mSourceMaterial);
    }

    if (event == MaterialSerializer::MSE_POST_WRITE)
    {
        mSourceMaterial = nullptr;
        mSGPassList.clear();
    }
}

//-----------------------------------------------------------------------------
void SGMaterialSerializerListener::techniqueEventRaised(MaterialSerializer* ser, 
                                          MaterialSerializer::SerializeEvent event, 
                                          bool& skip, const Technique* tech)
{
    // Pre technique write event.
    if (event == MaterialSerializer::MSE_PRE_WRITE)
    {       
        const Any& techUserData = tech->getUserObjectBindings().getUserAny(ShaderGenerator::SGTechnique::UserKey);

        // Skip writing this technique since it was generated by the Shader Generator.
        if (techUserData.has_value())
        {
            skip = true;
            return;
        }       
    }
}


//-----------------------------------------------------------------------------
void SGMaterialSerializerListener::passEventRaised(MaterialSerializer* ser, 
                                                   MaterialSerializer::SerializeEvent event, 
                                                   bool& skip, const Pass* pass)
{
    // End of pass writing event.
    if (event == MaterialSerializer::MSE_WRITE_END)
    {       
        // Grab the shader generator pass instance.
        ShaderGenerator::SGPass* passEntry = getShaderGeneratedPass(pass);
        
        // Case this pass use as source pass for shader generated pass.
        if (passEntry != nullptr)                          
            ShaderGenerator::getSingleton().serializePassAttributes(ser, passEntry);
    }   
}

//-----------------------------------------------------------------------------
void SGMaterialSerializerListener::textureUnitStateEventRaised(MaterialSerializer* ser, 
                                            MaterialSerializer::SerializeEvent event, 
                                            bool& skip, const TextureUnitState* textureUnit)
{
    // End of pass writing event.
    if (event == MaterialSerializer::MSE_WRITE_END)
    {       
        // Grab the shader generator pass instance.
        ShaderGenerator::SGPass* passEntry = getShaderGeneratedPass(textureUnit->getParent());
        
        // Case this pass use as source pass for shader generated pass.
        if (passEntry != nullptr)                          
            ShaderGenerator::getSingleton().serializeTextureUnitStateAttributes(ser, passEntry, textureUnit);
    }   
}

//-----------------------------------------------------------------------------
auto SGMaterialSerializerListener::getShaderGeneratedPass(const Pass* srcPass) -> ShaderGenerator::SGPass*
{
    SGPassListIterator it    = mSGPassList.begin();
    SGPassListIterator itEnd = mSGPassList.end();

    for (; it != itEnd; ++it)
    {
        ShaderGenerator::SGPass* currPassEntry = *it;

        if (currPassEntry->getSrcPass() == srcPass)
        {
            return currPassEntry;
        }
    }

    return nullptr;
}


}
}
