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
module;

#include <cstddef>

module Ogre.Components.RTShaderSystem;

import :ShaderFFPLighting;
import :ShaderFFPRenderState;
import :ShaderFunction;
import :ShaderFunctionAtom;
import :ShaderParameter;
import :ShaderPrecompiledHeaders;
import :ShaderPrerequisites;
import :ShaderProgram;
import :ShaderProgramSet;
import :ShaderRenderState;
import :ShaderScriptTranslator;

import Ogre.Core;

import <list>;
import <memory>;
import <string>;
import <vector>;

namespace Ogre::RTShader {

/************************************************************************/
/*                                                                      */
/************************************************************************/
std::string_view const constinit FFPLighting::Type = "FFP_Lighting";

//-----------------------------------------------------------------------
FFPLighting::FFPLighting()
{
	mTrackVertexColourType			= TrackVertexColourEnum::NONE;
	mSpecularEnable					= false;
	mNormalisedEnable               = false;
	mTwoSidedLighting               = false;
}

//-----------------------------------------------------------------------
auto FFPLighting::getType() const noexcept -> std::string_view
{
	return Type;
}


//-----------------------------------------------------------------------
auto FFPLighting::getExecutionOrder() const noexcept -> FFPShaderStage
{
	return FFPShaderStage::LIGHTING;
}

//-----------------------------------------------------------------------
void FFPLighting::updateGpuProgramsParams(Renderable* rend, const Pass* pass, const AutoParamDataSource* source,
										  const LightList* pLightList)
{		
	if (mLightParamsList.empty())
		return;

	Light::LightTypes curLightType = Light::LightTypes::DIRECTIONAL; 
	unsigned int curSearchLightIndex = 0;

	// Update per light parameters.
	for (auto & curParams : mLightParamsList)
	{
			if (curLightType != curParams.mType)
		{
			curLightType = curParams.mType;
			curSearchLightIndex = 0;
		}

		// Search a matching light from the current sorted lights of the given renderable.
		uint32 j;
		for (j = curSearchLightIndex; j < (pLightList ? pLightList->size() : 0); ++j)
		{
			if (pLightList->at(j)->getType() == curLightType)
			{
				curSearchLightIndex = j + 1;
				break;
			}			
		}

		switch (curParams.mType)
		{
			using enum Light::LightTypes;
		case DIRECTIONAL:
		    // update light index. data will be set by scene manager
			curParams.mDirection->updateExtraInfo(j);
			break;

		case POINT:
			// update light index. data will be set by scene manager
			curParams.mPosition->updateExtraInfo(j);
			curParams.mAttenuatParams->updateExtraInfo(j);
			break;

		case SPOTLIGHT:
		{						
			// update light index. data will be set by scene manager
			curParams.mPosition->updateExtraInfo(j);
            curParams.mAttenuatParams->updateExtraInfo(j);
            curParams.mSpotParams->updateExtraInfo(j);
			curParams.mDirection->updateExtraInfo(j);
		}
			break;
		}

		
		// Update diffuse colour.
        curParams.mDiffuseColour->updateExtraInfo(j);

		// Update specular colour if need to.
		if (mSpecularEnable)
		{
            curParams.mSpecularColour->updateExtraInfo(j);
		}																			
	}
}

//-----------------------------------------------------------------------
auto FFPLighting::resolveParameters(ProgramSet* programSet) -> bool
{
	Program* vsProgram = programSet->getCpuProgram(GpuProgramType::VERTEX_PROGRAM);
	Function* vsMain = vsProgram->getEntryPointFunction();

	// Resolve world view IT matrix.
	mWorldViewITMatrix = vsProgram->resolveParameter(GpuProgramParameters::AutoConstantType::NORMAL_MATRIX);
	mViewNormal = vsMain->resolveLocalParameter(Parameter::Content::NORMAL_VIEW_SPACE);
	
	// Get surface ambient colour if need to.
	if ((mTrackVertexColourType & TrackVertexColourEnum::AMBIENT) == TrackVertexColourEnum{})
	{		
		mDerivedAmbientLightColour = vsProgram->resolveParameter(GpuProgramParameters::AutoConstantType::DERIVED_AMBIENT_LIGHT_COLOUR);
	}
	else
	{
		mLightAmbientColour = vsProgram->resolveParameter(GpuProgramParameters::AutoConstantType::AMBIENT_LIGHT_COLOUR);
	}
	
	// Get surface emissive colour if need to.
	if ((mTrackVertexColourType & TrackVertexColourEnum::EMISSIVE) == TrackVertexColourEnum{})
	{
		mSurfaceEmissiveColour = vsProgram->resolveParameter(GpuProgramParameters::AutoConstantType::SURFACE_EMISSIVE_COLOUR);
	}

	// Get derived scene colour.
	mDerivedSceneColour = vsProgram->resolveParameter(GpuProgramParameters::AutoConstantType::DERIVED_SCENE_COLOUR);
	
	// Get surface shininess.
	mSurfaceShininess = vsProgram->resolveParameter(GpuProgramParameters::AutoConstantType::SURFACE_SHININESS);
	
	// Resolve input vertex shader normal.
    mVSInNormal = vsMain->resolveInputParameter(Parameter::Content::NORMAL_OBJECT_SPACE);
	
	if (mTrackVertexColourType != TrackVertexColourEnum{})
	{
		mInDiffuse = vsMain->resolveInputParameter(Parameter::Content::COLOR_DIFFUSE);
	}
	

	// Resolve output vertex shader diffuse colour.
	mOutDiffuse = vsMain->resolveOutputParameter(Parameter::Content::COLOR_DIFFUSE);
	
	// Resolve per light parameters.
	bool needViewPos = mSpecularEnable;
	for (unsigned int i=0; i < mLightParamsList.size(); ++i)
	{		
		using enum Light::LightTypes;
		switch (mLightParamsList[i].mType)
		{
		case DIRECTIONAL:
			mLightParamsList[i].mDirection = vsProgram->resolveParameter(GpuProgramParameters::AutoConstantType::LIGHT_DIRECTION_VIEW_SPACE, i);
			mLightParamsList[i].mPSInDirection = mLightParamsList[i].mDirection;
			break;
		
		case POINT:
		    needViewPos = true;
			
			mLightParamsList[i].mPosition = vsProgram->resolveParameter(GpuProgramParameters::AutoConstantType::LIGHT_POSITION_VIEW_SPACE, i);
			mLightParamsList[i].mAttenuatParams = vsProgram->resolveParameter(GpuProgramParameters::AutoConstantType::LIGHT_ATTENUATION, i);
			break;
		
		case SPOTLIGHT:
		    needViewPos = true;
			
			mLightParamsList[i].mPosition = vsProgram->resolveParameter(GpuProgramParameters::AutoConstantType::LIGHT_POSITION_VIEW_SPACE, i);
            mLightParamsList[i].mAttenuatParams = vsProgram->resolveParameter(GpuProgramParameters::AutoConstantType::LIGHT_ATTENUATION, i);

			mLightParamsList[i].mDirection = vsProgram->resolveParameter(GpuProgramParameters::AutoConstantType::LIGHT_DIRECTION_VIEW_SPACE, i);
			mLightParamsList[i].mPSInDirection = mLightParamsList[i].mDirection;

			mLightParamsList[i].mSpotParams = vsProgram->resolveParameter(GpuProgramParameters::AutoConstantType::SPOTLIGHT_PARAMS, i);

			break;
		}

		// Resolve diffuse colour.
		if ((mTrackVertexColourType & TrackVertexColourEnum::DIFFUSE) == TrackVertexColourEnum{})
		{
			mLightParamsList[i].mDiffuseColour = vsProgram->resolveParameter(GpuProgramParameters::AutoConstantType::DERIVED_LIGHT_DIFFUSE_COLOUR, i);
		}
		else
		{
			mLightParamsList[i].mDiffuseColour = vsProgram->resolveParameter(GpuProgramParameters::AutoConstantType::LIGHT_DIFFUSE_COLOUR_POWER_SCALED, i);
		}		

		if (mSpecularEnable)
		{
			// Resolve specular colour.
			if ((mTrackVertexColourType & TrackVertexColourEnum::SPECULAR) == TrackVertexColourEnum{})
			{
				mLightParamsList[i].mSpecularColour = vsProgram->resolveParameter(GpuProgramParameters::AutoConstantType::DERIVED_LIGHT_SPECULAR_COLOUR, i);
			}
			else
			{
				mLightParamsList[i].mSpecularColour = vsProgram->resolveParameter(GpuProgramParameters::AutoConstantType::LIGHT_SPECULAR_COLOUR_POWER_SCALED, i);
			}

			if (mOutSpecular.get() == nullptr)
			{
				mOutSpecular = vsMain->resolveOutputParameter(Parameter::Content::COLOR_SPECULAR);
			}
		}		
	}

	if(needViewPos)
	{
        mWorldViewMatrix = vsProgram->resolveParameter(GpuProgramParameters::AutoConstantType::WORLDVIEW_MATRIX);
        mVSInPosition = vsMain->resolveInputParameter(Parameter::Content::POSITION_OBJECT_SPACE);
        mViewPos = vsMain->resolveLocalParameter(Parameter::Content::POSITION_VIEW_SPACE);
	}

	return true;
}

//-----------------------------------------------------------------------
auto FFPLighting::resolveDependencies(ProgramSet* programSet) -> bool
{
	Program* vsProgram = programSet->getCpuProgram(GpuProgramType::VERTEX_PROGRAM);

	vsProgram->addDependency(FFP_LIB_TRANSFORM);
	vsProgram->addDependency(SGX_LIB_PERPIXELLIGHTING);

	if(mNormalisedEnable)
	    vsProgram->addPreprocessorDefines("NORMALISED");

	return true;
}

//-----------------------------------------------------------------------
auto FFPLighting::addFunctionInvocations(ProgramSet* programSet) -> bool
{
	Program* vsProgram = programSet->getCpuProgram(GpuProgramType::VERTEX_PROGRAM);	
	Function* vsMain = vsProgram->getEntryPointFunction();	
    auto stage = vsMain->getStage(std::to_underlying(FFPVertexShaderStage::LIGHTING));

	// Add the global illumination functions.
	addGlobalIlluminationInvocation(stage);

    // Add per light functions.
    for (const auto& lp : mLightParamsList)
    {
        addIlluminationInvocation(&lp, stage);
    }

    return true;
}

//-----------------------------------------------------------------------
void FFPLighting::addGlobalIlluminationInvocation(const FunctionStageRef& stage)
{
    // Transform normal to view space
	if(!mLightParamsList.empty())
	    stage.callFunction(FFP_FUNC_TRANSFORM, mWorldViewITMatrix, mVSInNormal, mViewNormal);

    if(mViewPos)
    {
        // Transform position to view space.
        stage.callFunction(FFP_FUNC_TRANSFORM, mWorldViewMatrix, mVSInPosition, mViewPos);
    }

	if ((mTrackVertexColourType & TrackVertexColourEnum::AMBIENT) == TrackVertexColourEnum{} &&
		(mTrackVertexColourType & TrackVertexColourEnum::EMISSIVE) == TrackVertexColourEnum{})
	{
		stage.assign(mDerivedSceneColour, mOutDiffuse);
	}
	else
	{
		if (!!(mTrackVertexColourType & TrackVertexColourEnum::AMBIENT))
		{
            stage.mul(mLightAmbientColour, mInDiffuse, mOutDiffuse);
		}
		else
		{
		    stage.assign(mDerivedAmbientLightColour, mOutDiffuse);
		}

		if (!!(mTrackVertexColourType & TrackVertexColourEnum::EMISSIVE))
		{
            stage.add(mInDiffuse, mOutDiffuse, mOutDiffuse);
		}
		else
		{
            stage.add(mSurfaceEmissiveColour, mOutDiffuse, mOutDiffuse);
		}		
	}
}

//-----------------------------------------------------------------------
void FFPLighting::addIlluminationInvocation(const LightParams* curLightParams, const FunctionStageRef& stage)
{
    // Merge diffuse colour with vertex colour if need to.
    if (!!(mTrackVertexColourType & TrackVertexColourEnum::DIFFUSE))
    {
        stage.mul(In(mInDiffuse).xyz(), In(curLightParams->mDiffuseColour).xyz(),
                  Out(curLightParams->mDiffuseColour).xyz());
    }

    // Merge specular colour with vertex colour if need to.
    if (mSpecularEnable && !!(mTrackVertexColourType & TrackVertexColourEnum::SPECULAR))
    {
        stage.mul(In(mInDiffuse).xyz(), In(curLightParams->mSpecularColour).xyz(),
                  Out(curLightParams->mSpecularColour).xyz());
    }

    using enum Light::LightTypes;
    switch (curLightParams->mType)
    {
    case DIRECTIONAL:
        if (mSpecularEnable)
        {
            stage.callFunction(SGX_FUNC_LIGHT_DIRECTIONAL_DIFFUSESPECULAR,
                               {In(mViewNormal), In(mViewPos), In(curLightParams->mPSInDirection).xyz(),
                                In(curLightParams->mDiffuseColour).xyz(), In(curLightParams->mSpecularColour).xyz(),
                                In(mSurfaceShininess), InOut(mOutDiffuse).xyz(), InOut(mOutSpecular).xyz()});
        }

        else
        {
            stage.callFunction(SGX_FUNC_LIGHT_DIRECTIONAL_DIFFUSE,
                               {In(mViewNormal), In(curLightParams->mPSInDirection).xyz(),
                                In(curLightParams->mDiffuseColour).xyz(), InOut(mOutDiffuse).xyz()});
        }
        break;

    case POINT:
        if (mSpecularEnable)
        {
            stage.callFunction(SGX_FUNC_LIGHT_POINT_DIFFUSESPECULAR,
                               {In(mViewNormal), In(mViewPos), In(curLightParams->mPosition).xyz(),
                                In(curLightParams->mAttenuatParams), In(curLightParams->mDiffuseColour).xyz(),
                                In(curLightParams->mSpecularColour).xyz(), In(mSurfaceShininess),
                                InOut(mOutDiffuse).xyz(), InOut(mOutSpecular).xyz()});
        }
        else
        {
            stage.callFunction(SGX_FUNC_LIGHT_POINT_DIFFUSE,
                               {In(mViewNormal), In(mViewPos), In(curLightParams->mPosition).xyz(),
                                In(curLightParams->mAttenuatParams), In(curLightParams->mDiffuseColour).xyz(),
                                InOut(mOutDiffuse).xyz()});
        }
				
        break;

    case SPOTLIGHT:
        if (mSpecularEnable)
        {
            stage.callFunction(SGX_FUNC_LIGHT_SPOT_DIFFUSESPECULAR,
                               {In(mViewNormal), In(mViewPos), In(curLightParams->mPosition).xyz(),
                                In(curLightParams->mPSInDirection).xyz(), In(curLightParams->mAttenuatParams),
                                In(curLightParams->mSpotParams).xyz(), In(curLightParams->mDiffuseColour).xyz(),
                                In(curLightParams->mSpecularColour).xyz(), In(mSurfaceShininess),
                                InOut(mOutDiffuse).xyz(), InOut(mOutSpecular).xyz()});
        }
        else
        {
            stage.callFunction(SGX_FUNC_LIGHT_SPOT_DIFFUSE,
                               {In(mViewNormal), In(mViewPos), In(curLightParams->mPosition).xyz(),
                                In(curLightParams->mPSInDirection).xyz(), In(curLightParams->mAttenuatParams),
                                In(curLightParams->mSpotParams).xyz(), In(curLightParams->mDiffuseColour).xyz(),
                                InOut(mOutDiffuse).xyz()});
        }
        break;
    }
}


//-----------------------------------------------------------------------
void FFPLighting::copyFrom(const SubRenderState& rhs)
{
	const auto& rhsLighting = static_cast<const FFPLighting&>(rhs);

	setLightCount(rhsLighting.getLightCount());
	mNormalisedEnable = rhsLighting.mNormalisedEnable;
	mTwoSidedLighting = rhsLighting.mTwoSidedLighting;
}

//-----------------------------------------------------------------------
auto FFPLighting::preAddToRenderState(const RenderState* renderState, Pass* srcPass, Pass* dstPass) noexcept -> bool
{
    //! [disable]
	if (!srcPass->getLightingEnabled())
		return false;
	//! [disable]

	auto lightCount = renderState->getLightCount();
	
	setTrackVertexColourType(srcPass->getVertexColourTracking());			

	if (srcPass->getShininess() > 0.0 &&
		srcPass->getSpecular() != ColourValue::Black)
	{
		setSpecularEnable(true);
	}
	else
	{
		setSpecularEnable(false);	
	}

	// Case this pass should run once per light(s) -> override the light policy.
	if (srcPass->getIteratePerLight())
	{		

		// This is the preferred case -> only one type of light is handled.
		if (srcPass->getRunOnlyForOneLightType())
		{
			if (srcPass->getOnlyLightType() == Light::LightTypes::POINT)
			{
				lightCount[0] = srcPass->getLightCountPerIteration();
				lightCount[1] = 0;
				lightCount[2] = 0;
			}
			else if (srcPass->getOnlyLightType() == Light::LightTypes::DIRECTIONAL)
			{
				lightCount[0] = 0;
				lightCount[1] = srcPass->getLightCountPerIteration();
				lightCount[2] = 0;
			}
			else if (srcPass->getOnlyLightType() == Light::LightTypes::SPOTLIGHT)
			{
				lightCount[0] = 0;
				lightCount[1] = 0;
				lightCount[2] = srcPass->getLightCountPerIteration();
			}
		}

		// This is worse case -> all light types expected to be handled.
		// Can not handle this request in efficient way - throw an exception.
		else
		{
			OGRE_EXCEPT(ExceptionCodes::INVALIDPARAMS,
				"Using iterative lighting method with RT Shader System requires specifying explicit light type.",
				"FFPLighting::preAddToRenderState");			
		}
	}

	setLightCount(lightCount);

	return true;
}

auto FFPLighting::setParameter(std::string_view name, std::string_view value) noexcept -> bool
{
	if(name == "normalise" || name == "normalised") // allow both spelling variations
	{
		return StringConverter::parse(value, mNormalisedEnable);
	}

	return false;
}

//-----------------------------------------------------------------------
void FFPLighting::setLightCount(const Vector3i& lightCount)
{
	for (int type=0; type < 3; ++type)
	{
		for (int i=0; i < lightCount[type]; ++i)
		{
			LightParams curParams;

			if (type == 0)
				curParams.mType = Light::LightTypes::POINT;
			else if (type == 1)
				curParams.mType = Light::LightTypes::DIRECTIONAL;
			else if (type == 2)
				curParams.mType = Light::LightTypes::SPOTLIGHT;

			mLightParamsList.push_back(curParams);
		}
	}			
}

//-----------------------------------------------------------------------
auto FFPLighting::getLightCount() const -> Vector3i
{
	Vector3i lightCount{0, 0, 0};

	for (auto curParams : mLightParamsList)
	{
			if (curParams.mType == Light::LightTypes::POINT)
			lightCount[0]++;
		else if (curParams.mType == Light::LightTypes::DIRECTIONAL)
			lightCount[1]++;
		else if (curParams.mType == Light::LightTypes::SPOTLIGHT)
			lightCount[2]++;
	}

	return lightCount;
}

//-----------------------------------------------------------------------
auto FFPLightingFactory::getType() const noexcept -> std::string_view
{
	return FFPLighting::Type;
}

//-----------------------------------------------------------------------
auto	FFPLightingFactory::createInstance(ScriptCompiler* compiler, 
												PropertyAbstractNode* prop, Pass* pass, SGScriptTranslator* translator) noexcept -> SubRenderState*
{
    if (prop->name != "lighting_stage" || prop->values.empty())
        return nullptr;

    auto it = prop->values.begin();
    String val;

    if(!SGScriptTranslator::getString(*it, &val))
    {
        compiler->addError(ScriptCompiler::CE_INVALIDPARAMETERS, prop->file, prop->line);
        return nullptr;
    }

    SubRenderState* ret = nullptr;
    if (val == "ffp")
    {
        ret = createOrRetrieveInstance(translator);
    }

    if(ret && prop->values.size() >= 2)
    {
        if(!SGScriptTranslator::getString(*it, &val))
        {
            compiler->addError(ScriptCompiler::CE_INVALIDPARAMETERS, prop->file, prop->line);
            return nullptr;
        }

        static_cast<FFPLighting*>(ret)->setNormaliseEnabled(val == "normalised");
    }

    return ret;
}

//-----------------------------------------------------------------------
void FFPLightingFactory::writeInstance(MaterialSerializer* ser, SubRenderState* subRenderState, 
											Pass* srcPass, Pass* dstPass)
{
	ser->writeAttribute(4, "lighting_stage");
	ser->writeValue("ffp");
}

//-----------------------------------------------------------------------
auto	FFPLightingFactory::createInstanceImpl() -> SubRenderState*
{
	return new FFPLighting;
}

}
