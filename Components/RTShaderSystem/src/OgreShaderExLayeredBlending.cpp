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

import :ShaderExLayeredBlending;
import :ShaderFFPTexturing;
import :ShaderFunction;
import :ShaderFunctionAtom;
import :ShaderPrerequisites;
import :ShaderProgram;
import :ShaderProgramSet;
import :ShaderScriptTranslator;

import Ogre.Core;

import <algorithm>;
import <list>;
import <memory>;
import <ranges>;
import <string>;
import <vector>;

namespace Ogre::RTShader {


std::string_view const constinit LayeredBlending::Type = "LayeredBlendRTSSEx";

 struct BlendModeDescription {
        /* Type of the blend mode */
        LayeredBlending::BlendMode type;
        /* name of the blend mode. */
        const char* name;
        /* shader function name . */
        const char* funcName;
    };

const BlendModeDescription _blendModes[(int)LayeredBlending::BlendMode::MaxBlendModes] = {
    { LayeredBlending::BlendMode::FFPBlend ,"default", ""},
    { LayeredBlending::BlendMode::BlendNormal ,"normal", "SGX_blend_normal"},
    { LayeredBlending::BlendMode::BlendLighten,"lighten", "SGX_blend_lighten"},
    { LayeredBlending::BlendMode::BlendDarken ,"darken", "SGX_blend_darken"},
    { LayeredBlending::BlendMode::BlendMultiply ,"multiply", "SGX_blend_multiply"},
    { LayeredBlending::BlendMode::BlendAverage ,"average", "SGX_blend_average"},
    { LayeredBlending::BlendMode::BlendAdd ,"add", "SGX_blend_add"},
    { LayeredBlending::BlendMode::BlendSubtract ,"subtract", "SGX_blend_subtract"},
    { LayeredBlending::BlendMode::BlendDifference ,"difference", "SGX_blend_difference"},
    { LayeredBlending::BlendMode::BlendNegation ,"negation", "SGX_blend_negation"},
    { LayeredBlending::BlendMode::BlendExclusion ,"exclusion", "SGX_blend_exclusion"},
    { LayeredBlending::BlendMode::BlendScreen ,"screen", "SGX_blend_screen"},
    { LayeredBlending::BlendMode::BlendOverlay ,"overlay", "SGX_blend_overlay"},
    { LayeredBlending::BlendMode::BlendHardLight ,"hard_light", "SGX_blend_hardLight"},
    { LayeredBlending::BlendMode::BlendSoftLight ,"soft_light", "SGX_blend_softLight"},
    { LayeredBlending::BlendMode::BlendColorDodge ,"color_dodge", "SGX_blend_colorDodge"},
    { LayeredBlending::BlendMode::BlendColorBurn ,"color_burn", "SGX_blend_colorBurn"},
    { LayeredBlending::BlendMode::BlendLinearDodge ,"linear_dodge", "SGX_blend_linearDodge"},
    { LayeredBlending::BlendMode::BlendLinearBurn ,"linear_burn", "SGX_blend_linearBurn"},
    { LayeredBlending::BlendMode::BlendLinearLight ,"linear_light", "SGX_blend_linearLight"},
    { LayeredBlending::BlendMode::BlendVividLight ,"vivid_light", "SGX_blend_vividLight"},
    { LayeredBlending::BlendMode::BlendPinLight ,"pin_light", "SGX_blend_pinLight"},
    { LayeredBlending::BlendMode::BlendHardMix ,"hard_mix", "SGX_blend_hardMix"},
    { LayeredBlending::BlendMode::BlendReflect ,"reflect", "SGX_blend_reflect"},
    { LayeredBlending::BlendMode::BlendGlow ,"glow", "SGX_blend_glow"},
    { LayeredBlending::BlendMode::BlendPhoenix ,"phoenix", "SGX_blend_phoenix"},
    { LayeredBlending::BlendMode::BlendSaturation ,"saturation", "SGX_blend_saturation"},
    { LayeredBlending::BlendMode::BlendColor ,"color", "SGX_blend_color"},
    { LayeredBlending::BlendMode::BlendLuminosity, "luminosity", "SGX_blend_luminosity"}
    };
        

 struct SourceModifierDescription {
        /* Type of the source modifier*/
        LayeredBlending::SourceModifier type;
        /* name of the source modifier. */
        const char* name;
        /* shader function name . */
        const char* funcName;
    };

const SourceModifierDescription _sourceModifiers[(int)LayeredBlending::SourceModifier::MaxSourceModifiers] = {
    { LayeredBlending::SourceModifier::None ,""},
    { LayeredBlending::SourceModifier::Source1Modulate ,"src1_modulate"},
    { LayeredBlending::SourceModifier::Source2Modulate ,"src2_modulate"},
    { LayeredBlending::SourceModifier::Source1InvModulate ,"src1_inverse_modulate"},
    { LayeredBlending::SourceModifier::Source2InvModulate ,"src2_inverse_modulate"}
    };
//-----------------------------------------------------------------------
LayeredBlending::LayeredBlending()
= default;

//-----------------------------------------------------------------------
auto LayeredBlending::getType() const noexcept -> std::string_view
{
    return Type;
}


//-----------------------------------------------------------------------
auto LayeredBlending::resolveParameters(ProgramSet* programSet) -> bool
{

    //resolve peremeter for normal texturing procedures
    bool isSuccess = FFPTexturing::resolveParameters(programSet);
    
    if (isSuccess)
    {
        //resolve source modification parameters
        Program* psProgram = programSet->getCpuProgram(GpuProgramType::FRAGMENT_PROGRAM);

        for(TextureBlend& texBlend : std::ranges::reverse_view{mTextureBlends})
        {
            if ((texBlend.sourceModifier != SourceModifier::Invalid) && 
                (texBlend.sourceModifier != SourceModifier::None))
            {
                
                texBlend.modControlParam = psProgram->resolveParameter(
                    GpuProgramParameters::AutoConstantType::CUSTOM, texBlend.customNum);
                if (texBlend.modControlParam.get() == nullptr)
                {   
                    isSuccess = false;
                    break;
                }
            }
        }
    }
    return isSuccess;
}



//-----------------------------------------------------------------------
auto LayeredBlending::resolveDependencies(ProgramSet* programSet) -> bool
{
    FFPTexturing::resolveDependencies(programSet);
    Program* psProgram = programSet->getCpuProgram(GpuProgramType::FRAGMENT_PROGRAM);

    psProgram->addDependency("SGXLib_LayeredBlending");

    return true;
}

//-----------------------------------------------------------------------
void LayeredBlending::copyFrom(const SubRenderState& rhs)
{
    FFPTexturing::copyFrom(rhs);
    
    const auto& rhsTexture = static_cast<const LayeredBlending&>(rhs);
    mTextureBlends = rhsTexture.mTextureBlends; 
}

//-----------------------------------------------------------------------
void LayeredBlending::addPSBlendInvocations(Function* psMain, 
                                         ParameterPtr arg1,
                                         ParameterPtr arg2,
                                         ParameterPtr texel,
                                         int samplerIndex,
                                         const LayerBlendModeEx& blendMode,
                                         const int groupOrder, 
                                         Operand::OpMask mask)
{
    //
    // Add the modifier invocation
    //

    addPSModifierInvocation(psMain, samplerIndex, arg1, arg2, groupOrder, mask);

    //
    // Add the blending function invocations
    //

    BlendMode mode = getBlendMode(samplerIndex);
    
    if ((BlendMode::FFPBlend == mode) || (BlendMode::Invalid == mode))
    {
        FFPTexturing::addPSBlendInvocations(psMain, arg1, arg2, texel, samplerIndex, blendMode, groupOrder, mask);
    }
    else 
    {
        //find the function name for the blend mode
        const char* funcName = nullptr;
        for(const auto & _blendMode : _blendModes)
        {
            if (_blendMode.type == mode)
            {
                funcName = _blendMode.funcName;
                break;
            }
        }

        //add the function of the blend mode
        if (funcName)
        {
            psMain->getStage(groupOrder)
                .callFunction(funcName, In(arg1).mask(mask), In(arg2).mask(mask), Out(mPSOutDiffuse).mask(mask));
        }
    }
}

//-----------------------------------------------------------------------
void LayeredBlending::addPSModifierInvocation(Function* psMain, 
                                         int samplerIndex, 
                                         ParameterPtr arg1,
                                         ParameterPtr arg2,
                                         const int groupOrder, 
                                         Operand::OpMask mask)
{
    SourceModifier modType;
    int customNum;
    if (getSourceModifier(samplerIndex, modType, customNum) == true)
    {
        ParameterPtr modifiedParam;
        const char* funcName = nullptr;
        using enum SourceModifier;
        switch (modType)
        {
        case Source1Modulate:
            funcName = "SGX_src_mod_modulate";
            modifiedParam = arg1;
            break;
        case Source2Modulate:
            funcName = "SGX_src_mod_modulate";
            modifiedParam = arg2;
            break;
        case Source1InvModulate:
            funcName = "SGX_src_mod_inv_modulate";
            modifiedParam = arg1;
            break;
        case Source2InvModulate:
            funcName = "SGX_src_mod_inv_modulate";
            modifiedParam = arg2;
            break;
        default:
            break;
        }

        //add the function of the blend mode
        if (funcName)
        {
            ParameterPtr& controlParam = mTextureBlends[samplerIndex].modControlParam;
            psMain->getStage(groupOrder)
                .callFunction(funcName, In(modifiedParam).mask(mask), In(controlParam).mask(mask),
                              Out(modifiedParam).mask(mask));
        }
    }
}


//-----------------------------------------------------------------------
void LayeredBlending::setBlendMode(unsigned short index, BlendMode mode)
{
    if(mTextureBlends.size() < (size_t)index + 1)
    {
        mTextureBlends.resize(index + 1);
    }
    mTextureBlends[index].blendMode = mode;
}

//-----------------------------------------------------------------------
auto LayeredBlending::getBlendMode(unsigned short index) const -> LayeredBlending::BlendMode
{
    if(index < mTextureBlends.size())
    {
        return mTextureBlends[index].blendMode;
    }
    return BlendMode::Invalid;
}


//-----------------------------------------------------------------------
void LayeredBlending::setSourceModifier(unsigned short index, SourceModifier modType, int customNum)
{
    if(mTextureBlends.size() < (size_t)index + 1)
    {
        mTextureBlends.resize(index + 1);
    }
    mTextureBlends[index].sourceModifier = modType;
    mTextureBlends[index].customNum = customNum;
}

//-----------------------------------------------------------------------
auto LayeredBlending::getSourceModifier(unsigned short index, SourceModifier& modType, int& customNum) const -> bool
{
    modType = SourceModifier::Invalid;
    customNum = 0;
    if(index < mTextureBlends.size())
    {
        modType = mTextureBlends[index].sourceModifier;
        customNum = mTextureBlends[index].customNum;
    }
    return (modType != SourceModifier::Invalid);
}


//----------------------Factory Implementation---------------------------
//-----------------------------------------------------------------------
auto LayeredBlendingFactory::getType() const noexcept -> std::string_view
{
    return LayeredBlending::Type;
}

//-----------------------------------------------------------------------
auto LayeredBlendingFactory::createInstance(ScriptCompiler* compiler, 
                                    PropertyAbstractNode* prop, TextureUnitState* texState, SGScriptTranslator* translator) noexcept -> SubRenderState*
{
    if (prop->name == "layered_blend")
    {
        String blendType;
        if(false == SGScriptTranslator::getString(prop->values.front(), &blendType))
        {
            compiler->addError(ScriptCompiler::CE_INVALIDPARAMETERS, prop->file, prop->line);
            return nullptr;
        }

        LayeredBlending::BlendMode blendMode = stringToBlendMode(blendType);
        if (blendMode == LayeredBlending::BlendMode::Invalid)
        {
            compiler->addError(ScriptCompiler::CE_INVALIDPARAMETERS, prop->file, prop->line,
                "Expected one of the following blend modes: default, normal, " \
                "lighten, darken, multiply, average, add, " \
                "subtract, difference, negation, exclusion, " \
                "screen, overlay, hard_light, soft_light, " \
                "color_dodge, color_burn, linear_dodge, linear_burn, " \
                "linear_light, vivid_light, pin_light, hard_mix, " \
                "reflect, glow, phoenix, saturation, color and luminosity");
            return nullptr;
        }

        
        //get the layer blend sub-render state to work on
        LayeredBlending* layeredBlendState =
            createOrRetrieveSubRenderState(translator);
        
        //update the layer sub render state
        unsigned short texIndex = texState->getParent()->getTextureUnitStateIndex(texState);
        layeredBlendState->setBlendMode(texIndex, blendMode);

        return layeredBlendState;
    }
    if (prop->name == "source_modifier")
    {
        if(prop->values.size() < 3)
        {
            compiler->addError(ScriptCompiler::CE_INVALIDPARAMETERS, prop->file, prop->line, 
                "Expected three or more parameters.");
            return nullptr;
        }

        // Read light model type.
        bool isParseSuccess;
        String modifierString;
        String paramType;
        int customNum;
        
        auto itValue = prop->values.begin();
        isParseSuccess = SGScriptTranslator::getString(*itValue, &modifierString); 
        LayeredBlending::SourceModifier modType = stringToSourceModifier(modifierString);
        isParseSuccess &= modType != LayeredBlending::SourceModifier::Invalid;
        if(isParseSuccess == false)
        {
            compiler->addError(ScriptCompiler::CE_INVALIDPARAMETERS, prop->file, prop->line, 
                "Expected one of the following modifier type as first parameter: " \
                "src1_modulate, src2_modulate, src1_inverse_modulate, src2_inverse_modulate.");
            return nullptr;
        }

        ++itValue;
        isParseSuccess = SGScriptTranslator::getString(*itValue, &paramType); 
        isParseSuccess &= (paramType == "custom");
        if(isParseSuccess == false)
        {
            compiler->addError(ScriptCompiler::CE_INVALIDPARAMETERS, prop->file, prop->line, 
                "Expected reserved word custom as second parameter.");
            return nullptr;
        }
        ++itValue;
        isParseSuccess = SGScriptTranslator::getInt(*itValue, &customNum); 
        if(isParseSuccess == false)
        {
            compiler->addError(ScriptCompiler::CE_INVALIDPARAMETERS, prop->file, prop->line, 
                "Expected number of custom parameter as third parameter.");
            return nullptr;
        }

        //get the layer blend sub-render state to work on
        LayeredBlending* layeredBlendState =
            createOrRetrieveSubRenderState(translator);
        
        //update the layer sub render state
        unsigned short texIndex = texState->getParent()->getTextureUnitStateIndex(texState);
        layeredBlendState->setSourceModifier(texIndex, modType, customNum);

        return layeredBlendState;
            
    }
    
    return nullptr;
        
}

//-----------------------------------------------------------------------
void LayeredBlendingFactory::writeInstance(MaterialSerializer* ser, SubRenderState* subRenderState, 
                                        const TextureUnitState* srcTextureState, const TextureUnitState* dstTextureState)
{
    unsigned short texIndex = srcTextureState->getParent()->
        getTextureUnitStateIndex(srcTextureState);
    
    //get blend mode for current texture unit
    auto* layeredBlendingSubRenderState = static_cast<LayeredBlending*>(subRenderState);
    
    //write the blend mode
    LayeredBlending::BlendMode blendMode = layeredBlendingSubRenderState->getBlendMode(texIndex);
    if (blendMode != LayeredBlending::BlendMode::Invalid)
    {
        ser->writeAttribute(5, "layered_blend");    
        ser->writeValue(blendModeToString(blendMode));
    }

    //write the source modifier
    LayeredBlending::SourceModifier modType;
    int customNum;
    if (layeredBlendingSubRenderState->getSourceModifier(texIndex, modType, customNum) == true)
    {
        ser->writeAttribute(5, "source_modifier");  
        ser->writeValue(sourceModifierToString(modType));
        ser->writeValue("custom");
        ser->writeValue(StringConverter::toString(customNum));
    }

}

//-----------------------------------------------------------------------
auto LayeredBlendingFactory::createInstanceImpl() -> SubRenderState*
{
    return new LayeredBlending;
}

//-----------------------------------------------------------------------
auto LayeredBlendingFactory::stringToBlendMode(std::string_view strValue) -> LayeredBlending::BlendMode
{
    for(const auto & _blendMode : _blendModes)
    {
        if (_blendMode.name == strValue)
        {
            return _blendMode.type;
        }
    }
    return LayeredBlending::BlendMode::Invalid;
}

//-----------------------------------------------------------------------
auto LayeredBlendingFactory::blendModeToString(LayeredBlending::BlendMode blendMode) -> String
{
    for(const auto & _blendMode : _blendModes)
    {
        if (_blendMode.type == blendMode)
        {
            return _blendMode.name;
        }
    }
    return "";
}

//-----------------------------------------------------------------------
auto LayeredBlendingFactory::stringToSourceModifier(std::string_view strValue) -> LayeredBlending::SourceModifier
{
    for(const auto & _sourceModifier : _sourceModifiers)
    {
        if (_sourceModifier.name == strValue)
        {
            return _sourceModifier.type;
        }
    }
    return LayeredBlending::SourceModifier::Invalid;
}

//-----------------------------------------------------------------------
auto LayeredBlendingFactory::sourceModifierToString(LayeredBlending::SourceModifier modifier) -> String
{
    for(const auto & _sourceModifier : _sourceModifiers)
    {
        if (_sourceModifier.type == modifier)
        {
            return _sourceModifier.name;
        }
    }
    return "";
}

//-----------------------------------------------------------------------
auto LayeredBlendingFactory::createOrRetrieveSubRenderState(SGScriptTranslator* translator) -> LayeredBlending*
{
    LayeredBlending* layeredBlendState;
    //check if we already create a blend srs
    SubRenderState* subState = translator->getGeneratedSubRenderState(getType());
    if (subState != nullptr)
    {
        layeredBlendState = static_cast<LayeredBlending*>(subState);
    }
    else
    {
        SubRenderState* subRenderState = createOrRetrieveInstance(translator);
        layeredBlendState = static_cast<LayeredBlending*>(subRenderState);
    }
    return layeredBlendState;
}


}
