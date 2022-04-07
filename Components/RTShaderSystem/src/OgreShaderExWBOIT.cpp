// This file is part of the OGRE project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at https://www.ogre3d.org/licensing.
// SPDX-License-Identifier: MIT
module Ogre.Components.RTShaderSystem:ShaderExWBOIT.Obj;

import :ShaderExWBOIT;
import :ShaderFFPRenderState;
import :ShaderFunction;
import :ShaderFunctionAtom;
import :ShaderGenerator;
import :ShaderParameter;
import :ShaderProgram;
import :ShaderProgramSet;
import :ShaderScriptTranslator;

import Ogre.Core;

import <cstddef>;
import <list>;
import <memory>;
import <string>;

namespace Ogre::RTShader {
        class RenderState;
    }  // namespace Ogre

namespace Ogre::RTShader
{

/************************************************************************/
/*                                                                      */
/************************************************************************/
String WBOIT::Type = "WBOIT";

//-----------------------------------------------------------------------
auto WBOIT::getType() const -> const String& { return Type; }

//-----------------------------------------------------------------------
auto WBOIT::getExecutionOrder() const -> int { return FFP_POST_PROCESS; }

auto WBOIT::preAddToRenderState(const RenderState* renderState, Pass* srcPass, Pass* dstPass) -> bool
{
    dstPass->setTransparentSortingEnabled(false);
    dstPass->setSeparateSceneBlending(SBF_ONE, SBF_ONE, SBF_ZERO, SBF_ONE_MINUS_SOURCE_ALPHA);
    return true;
}

auto WBOIT::createCpuSubPrograms(ProgramSet* programSet) -> bool
{
    Program* psProgram = programSet->getCpuProgram(GPT_FRAGMENT_PROGRAM);
    psProgram->addDependency("SGXLib_WBOIT");

    Function* vsMain = programSet->getCpuProgram(GPT_VERTEX_PROGRAM)->getMain();
    Function* psMain = psProgram->getMain();

    auto vsOutPos = vsMain->resolveOutputParameter(Parameter::SPC_POSITION_PROJECTIVE_SPACE);

    bool isD3D9 = ShaderGenerator::getSingleton().getTargetLanguage() == "hlsl" &&
                  !GpuProgramManager::getSingleton().isSyntaxSupported("vs_4_0_level_9_1");

    if (isD3D9)
    {
        auto vstage = vsMain->getStage(FFP_VS_POST_PROCESS);
        auto vsPos = vsMain->resolveOutputParameter(Parameter::SPC_UNKNOWN, GCT_FLOAT4);
        vstage.assign(vsOutPos, vsPos);
        std::swap(vsOutPos, vsPos);
    }

    auto viewPos = psMain->resolveInputParameter(vsOutPos);

    auto accum = psMain->resolveOutputParameter(Parameter::SPC_COLOR_DIFFUSE);
    auto revealage = psMain->resolveOutputParameter(Parameter::SPC_COLOR_SPECULAR);

    auto stage = psMain->getStage(FFP_PS_POST_PROCESS);

    if (isD3D9)
    {
        stage.div(viewPos, In(viewPos).w(), viewPos);
    }

    stage.callFunction("SGX_WBOIT", {In(viewPos).z(), InOut(accum), Out(revealage)});

    return true;
}

//-----------------------------------------------------------------------
auto WBOITFactory::getType() const -> const String& { return WBOIT::Type; }

//-----------------------------------------------------------------------
auto WBOITFactory::createInstance(ScriptCompiler* compiler, PropertyAbstractNode* prop, Pass* pass,
                                               SGScriptTranslator* translator) -> SubRenderState*
{
    if (prop->name != "weighted_blended_oit" || prop->values.empty())
        return nullptr;

    auto it = prop->values.begin();
    bool val;
    if(!SGScriptTranslator::getBoolean(*it++, &val))
    {
        compiler->addError(ScriptCompiler::CE_INVALIDPARAMETERS, prop->file, prop->line);
        return nullptr;
    }

    if (!val)
        return nullptr;

    auto ret = static_cast<WBOIT*>(createOrRetrieveInstance(translator));
    return ret;
}

//-----------------------------------------------------------------------
void WBOITFactory::writeInstance(MaterialSerializer* ser, SubRenderState* subRenderState, Pass* srcPass,
                                   Pass* dstPass)
{
    ser->writeAttribute(4, "weighted_blended_oit");
    ser->writeValue("true");
}

//-----------------------------------------------------------------------
auto WBOITFactory::createInstanceImpl() -> SubRenderState* { return new WBOIT; }

} // namespace Ogre
