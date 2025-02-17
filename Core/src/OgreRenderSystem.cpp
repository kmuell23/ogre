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

#include <cassert>
#include <cstddef>
#include <cstdint>

module Ogre.Core;

// RenderSystem implementation
// Note that most of this class is abstract since
//  we cannot know how to implement the behaviour without
//  being aware of the 3D API. However there are a few
//  simple functions which can have a base implementation
import :Common;
import :Config;
import :ConfigOptionMap;
import :DepthBuffer;
import :Exception;
import :GpuProgram;
import :GpuProgramParams;
import :HardwareOcclusionQuery;
import :HardwareVertexBuffer;
import :LogManager;
import :MaterialManager;
import :Plane;
import :Platform;
import :Prerequisites;
import :RenderOperation;
import :RenderSystem;
import :RenderSystemCapabilities;
import :RenderTarget;
import :SharedPtr;
import :StringConverter;
import :StringVector;
import :TextureManager;
import :TextureUnitState;
import :Vector;
import :VertexIndexData;

import <algorithm>;
import <format>;
import <istream>;
import <list>;
import <map>;
import <memory>;
import <string>;
import <utility>;
import <vector>;

namespace Ogre {

    RenderSystem::Listener* RenderSystem::msSharedEventListener = nullptr;

    static const TexturePtr constinit sNullTexPtr;

    //-----------------------------------------------------------------------
    RenderSystem::RenderSystem()
        :
         mTexProjRelativeOrigin(Vector3::ZERO)
    {
        mEventNames.push_back("RenderSystemCapabilitiesCreated");
    }

    void RenderSystem::initFixedFunctionParams()
    {
        if(mFixedFunctionParams)
            return;

        GpuLogicalBufferStructPtr logicalBufferStruct(new GpuLogicalBufferStruct());
        mFixedFunctionParams = std::make_shared<GpuProgramParameters>();
        mFixedFunctionParams->_setLogicalIndexes(logicalBufferStruct);
        mFixedFunctionParams->setAutoConstant(0, GpuProgramParameters::AutoConstantType::WORLD_MATRIX);
        mFixedFunctionParams->setAutoConstant(4, GpuProgramParameters::AutoConstantType::VIEW_MATRIX);
        mFixedFunctionParams->setAutoConstant(8, GpuProgramParameters::AutoConstantType::PROJECTION_MATRIX);
        mFixedFunctionParams->setAutoConstant(12, GpuProgramParameters::AutoConstantType::SURFACE_AMBIENT_COLOUR);
        mFixedFunctionParams->setAutoConstant(13, GpuProgramParameters::AutoConstantType::SURFACE_DIFFUSE_COLOUR);
        mFixedFunctionParams->setAutoConstant(14, GpuProgramParameters::AutoConstantType::SURFACE_SPECULAR_COLOUR);
        mFixedFunctionParams->setAutoConstant(15, GpuProgramParameters::AutoConstantType::SURFACE_EMISSIVE_COLOUR);
        mFixedFunctionParams->setAutoConstant(16, GpuProgramParameters::AutoConstantType::SURFACE_SHININESS);
        mFixedFunctionParams->setAutoConstant(17, GpuProgramParameters::AutoConstantType::POINT_PARAMS);
        mFixedFunctionParams->setConstant(18, Vector4::ZERO); // AutoConstantType::FOG_PARAMS
        mFixedFunctionParams->setConstant(19, Vector4::ZERO); // AutoConstantType::FOG_COLOUR
        mFixedFunctionParams->setAutoConstant(20, GpuProgramParameters::AutoConstantType::AMBIENT_LIGHT_COLOUR);

        // allocate per light parameters. slots 21..69
        for(int i = 0; i < OGRE_MAX_SIMULTANEOUS_LIGHTS; i++)
        {
            size_t light_offset = 21 + i * 6;
            mFixedFunctionParams->setConstant(light_offset + 0, Vector4::ZERO); // position
            mFixedFunctionParams->setConstant(light_offset + 1, Vector4::ZERO); // direction
            mFixedFunctionParams->setConstant(light_offset + 2, Vector4::ZERO); // diffuse
            mFixedFunctionParams->setConstant(light_offset + 3, Vector4::ZERO); // specular
            mFixedFunctionParams->setConstant(light_offset + 4, Vector4::ZERO); // attenuation
            mFixedFunctionParams->setConstant(light_offset + 5, Vector4::ZERO); // spotlight
        }
    }

    void RenderSystem::setFFPLightParams(uint32 index, bool enabled)
    {
        if(!mFixedFunctionParams)
            return;

        size_t light_offset = 21 + 6 * index;
        if (!enabled)
        {
            mFixedFunctionParams->clearAutoConstant(light_offset + 0);
            mFixedFunctionParams->clearAutoConstant(light_offset + 1);
            mFixedFunctionParams->clearAutoConstant(light_offset + 2);
            mFixedFunctionParams->clearAutoConstant(light_offset + 3);
            mFixedFunctionParams->clearAutoConstant(light_offset + 4);
            mFixedFunctionParams->clearAutoConstant(light_offset + 5);
            return;
        }
        mFixedFunctionParams->setAutoConstant(light_offset + 0, GpuProgramParameters::AutoConstantType::LIGHT_POSITION, index);
        mFixedFunctionParams->setAutoConstant(light_offset + 1, GpuProgramParameters::AutoConstantType::LIGHT_DIRECTION, index);
        mFixedFunctionParams->setAutoConstant(light_offset + 2, GpuProgramParameters::AutoConstantType::LIGHT_DIFFUSE_COLOUR, index);
        mFixedFunctionParams->setAutoConstant(light_offset + 3, GpuProgramParameters::AutoConstantType::LIGHT_SPECULAR_COLOUR, index);
        mFixedFunctionParams->setAutoConstant(light_offset + 4, GpuProgramParameters::AutoConstantType::LIGHT_ATTENUATION, index);
        mFixedFunctionParams->setAutoConstant(light_offset + 5, GpuProgramParameters::AutoConstantType::SPOTLIGHT_PARAMS, index);
    }

    //-----------------------------------------------------------------------
    RenderSystem::~RenderSystem()
    {
        shutdown();
        // Current capabilities managed externally
        mCurrentCapabilities = nullptr;
    }

    auto RenderSystem::getRenderWindowDescription() const -> RenderWindowDescription
    {
        RenderWindowDescription ret;
        auto& miscParams = ret.miscParams;

        auto end = mOptions.end();

        auto opt = mOptions.find("Full Screen");
        if (opt == end)
            OGRE_EXCEPT(ExceptionCodes::INVALIDPARAMS, "Can't find 'Full Screen' option");

        ret.useFullScreen = StringConverter::parseBool(opt->second.currentValue);

        opt = mOptions.find("Video Mode");
        if (opt == end)
            OGRE_EXCEPT(ExceptionCodes::INVALIDPARAMS, "Can't find 'Video Mode' option");

        StringStream mode(opt->second.currentValue);
        String token;

        mode >> ret.width;
        mode >> token; // 'x' as seperator between width and height
        mode >> ret.height;

        // backend specific options. Presence determined by getConfigOptions
        mode >> token; // '@' as seperator between bpp on D3D
        if(!mode.eof())
        {
            uint32 bpp;
            mode >> bpp;
            miscParams.emplace("colourDepth", std::to_string(bpp));
        }

        if((opt = mOptions.find("FSAA")) != end)
        {
            StringStream fsaaMode(opt->second.currentValue);
            uint32_t fsaa;
            fsaaMode >> fsaa;
            miscParams.emplace("FSAA", std::to_string(fsaa));

            // D3D specific
            if(!fsaaMode.eof())
            {
                String hint;
                fsaaMode >> hint;
                miscParams.emplace("FSAAHint", hint);
            }
        }

        if((opt = mOptions.find("VSync")) != end)
            miscParams.emplace("vsync", opt->second.currentValue);

        if((opt = mOptions.find("sRGB Gamma Conversion")) != end)
            miscParams.emplace("gamma", opt->second.currentValue);

        if((opt = mOptions.find("Colour Depth")) != end)
            miscParams.emplace("colourDepth", opt->second.currentValue);

        if((opt = mOptions.find("VSync Interval")) != end)
            miscParams.emplace("vsyncInterval", opt->second.currentValue);

        if((opt = mOptions.find("Display Frequency")) != end)
            miscParams.emplace("displayFrequency", opt->second.currentValue);

        if((opt = mOptions.find("Content Scaling Factor")) != end)
            miscParams["contentScalingFactor"] = opt->second.currentValue;

        if((opt = mOptions.find("Rendering Device")) != end)
        {
            // try to parse "Monitor-NN-"
            auto start = opt->second.currentValue.find('-') + 1;
            auto len = opt->second.currentValue.find('-', start) - start;
            if(start != String::npos)
                miscParams["monitorIndex"] = opt->second.currentValue.substr(start, len);
        }

        return ret;
    }

    //-----------------------------------------------------------------------
    void RenderSystem::_initRenderTargets()
    {

        // Init stats
        for(auto & mRenderTarget : mRenderTargets)
        {
            mRenderTarget.second->resetStatistics();
        }

    }
    //-----------------------------------------------------------------------
    void RenderSystem::_updateAllRenderTargets(bool swapBuffers)
    {
        // Update all in order of priority
        // This ensures render-to-texture targets get updated before render windows
        for (auto & mPrioritisedRenderTarget : mPrioritisedRenderTargets)
        {
            if( mPrioritisedRenderTarget.second->isActive() && mPrioritisedRenderTarget.second->isAutoUpdated())
                mPrioritisedRenderTarget.second->update(swapBuffers);
        }
    }
    //-----------------------------------------------------------------------
    void RenderSystem::_swapAllRenderTargetBuffers()
    {
        // Update all in order of priority
        // This ensures render-to-texture targets get updated before render windows
        for (auto & mPrioritisedRenderTarget : mPrioritisedRenderTargets)
        {
            if( mPrioritisedRenderTarget.second->isActive() && mPrioritisedRenderTarget.second->isAutoUpdated())
                mPrioritisedRenderTarget.second->swapBuffers();
        }
    }
    //-----------------------------------------------------------------------
    void RenderSystem::_initialise()
    {
        // Have I been registered by call to Root::setRenderSystem?
        /** Don't do this anymore, just allow via Root
        RenderSystem* regPtr = Root::getSingleton().getRenderSystem();
        if (!regPtr || regPtr != this)
            // Register self - library user has come to me direct
            Root::getSingleton().setRenderSystem(this);
        */


        // Subclasses should take it from here
        // They should ALL call this superclass method from
        //   their own initialise() implementations.

        mVertexProgramBound = false;
        mGeometryProgramBound = false;
        mFragmentProgramBound = false;
        mTessellationHullProgramBound = false;
        mTessellationDomainProgramBound = false;
        mComputeProgramBound = false;
    }

    //---------------------------------------------------------------------------------------------
    void RenderSystem::useCustomRenderSystemCapabilities(RenderSystemCapabilities* capabilities)
    {
    if (mRealCapabilities != nullptr)
    {
      OGRE_EXCEPT(ExceptionCodes::INTERNAL_ERROR,
          "Custom render capabilities must be set before the RenderSystem is initialised.",
          "RenderSystem::useCustomRenderSystemCapabilities");
    }

        mCurrentCapabilities = capabilities;
        mUseCustomCapabilities = true;
    }

    //---------------------------------------------------------------------------------------------
    auto RenderSystem::_createRenderWindow(std::string_view name, unsigned int width,
                                                    unsigned int height, bool fullScreen,
                                                    const NameValuePairList* miscParams) -> RenderWindow*
    {
        if (mRenderTargets.find(name) != mRenderTargets.end())
        {
            OGRE_EXCEPT(ExceptionCodes::INVALIDPARAMS, ::std::format("Window with name '{}' already exists", name ));
        }

        // Log a message
        StringStream ss;
        ss << "RenderSystem::_createRenderWindow \"" << name << "\", " <<
            width << "x" << height << " ";
        if (fullScreen)
            ss << "fullscreen ";
        else
            ss << "windowed ";

        if (miscParams)
        {
            ss << " miscParams: ";
            NameValuePairList::const_iterator it;
            for (const auto& p : *miscParams)
            {
                ss << p.first << "=" << p.second << " ";
            }
        }
        LogManager::getSingleton().logMessage(ss.str());

        return nullptr;
    }
    //---------------------------------------------------------------------------------------------
    void RenderSystem::destroyRenderWindow(std::string_view name)
    {
        destroyRenderTarget(name);
    }
    //---------------------------------------------------------------------------------------------
    void RenderSystem::destroyRenderTexture(std::string_view name)
    {
        destroyRenderTarget(name);
    }
    //---------------------------------------------------------------------------------------------
    void RenderSystem::destroyRenderTarget(std::string_view name)
    {
        RenderTarget* rt = detachRenderTarget(name);
        delete rt;
    }
    //---------------------------------------------------------------------------------------------
    void RenderSystem::attachRenderTarget( RenderTarget &target )
    {
        assert( target.getPriority() < OGRE_NUM_RENDERTARGET_GROUPS );

        mRenderTargets.emplace(target.getName(), &target);
        mPrioritisedRenderTargets.emplace(target.getPriority(), &target);
    }

    //---------------------------------------------------------------------------------------------
    auto RenderSystem::getRenderTarget( std::string_view name ) -> RenderTarget *
    {
        auto it = mRenderTargets.find( name );
        RenderTarget *ret = nullptr;

        if( it != mRenderTargets.end() )
        {
            ret = it->second;
        }

        return ret;
    }

    //---------------------------------------------------------------------------------------------
    auto RenderSystem::detachRenderTarget( std::string_view name ) -> RenderTarget *
    {
        auto it = mRenderTargets.find( name );
        RenderTarget *ret = nullptr;

        if( it != mRenderTargets.end() )
        {
            ret = it->second;

            /* Remove the render target from the priority groups. */
            for (auto itarg = mPrioritisedRenderTargets.begin(); itarg != mPrioritisedRenderTargets.end(); ++itarg )
            {
                if( itarg->second == ret ) {
                    mPrioritisedRenderTargets.erase( itarg );
                    break;
                }
            }

            mRenderTargets.erase( it );
        }
        /// If detached render target is the active render target, reset active render target
        if(ret == mActiveRenderTarget)
            mActiveRenderTarget = nullptr;

        return ret;
    }
    //-----------------------------------------------------------------------
    auto RenderSystem::_getViewport() -> Viewport*
    {
        return mActiveViewport;
    }
    //-----------------------------------------------------------------------
    void RenderSystem::_setTextureUnitSettings(size_t texUnit, TextureUnitState& tl)
    {
        // This method is only ever called to set a texture unit to valid details
        // The method _disableTextureUnit is called to turn a unit off
        TexturePtr tex = tl._getTexturePtr();
        if(!tex || tl.isTextureLoadFailing())
            tex = mTextureManager->_getWarningTexture();

        // Bind texture (may be blank)
        _setTexture(texUnit, true, tex);

        // Set texture coordinate set
        _setTextureCoordSet(texUnit, tl.getTextureCoordSet());

        _setSampler(texUnit, *tl.getSampler());

        // Set blend modes
        // Note, colour before alpha is important
        _setTextureBlendMode(texUnit, tl.getColourBlendMode());
        _setTextureBlendMode(texUnit, tl.getAlphaBlendMode());

        // Set texture effects
        // Iterate over new effects
        bool anyCalcs = false;
        for (auto & mEffect : tl.mEffects)
        {
            using enum TextureUnitState::TextureEffectType;
            switch (mEffect.second.type)
            {
            case ENVIRONMENT_MAP:
            {
                auto const subtype = static_cast<TextureUnitState::EnvMapType>(mEffect.second.subtype);
                if (subtype == TextureUnitState::EnvMapType::CURVED)
                {
                    _setTextureCoordCalculation(texUnit, TexCoordCalcMethod::ENVIRONMENT_MAP);
                    anyCalcs = true;
                }
                else if (subtype == TextureUnitState::EnvMapType::PLANAR)
                {
                    _setTextureCoordCalculation(texUnit, TexCoordCalcMethod::ENVIRONMENT_MAP_PLANAR);
                    anyCalcs = true;
                }
                else if (subtype == TextureUnitState::EnvMapType::REFLECTION)
                {
                    _setTextureCoordCalculation(texUnit, TexCoordCalcMethod::ENVIRONMENT_MAP_REFLECTION);
                    anyCalcs = true;
                }
                else if (subtype == TextureUnitState::EnvMapType::NORMAL)
                {
                    _setTextureCoordCalculation(texUnit, TexCoordCalcMethod::ENVIRONMENT_MAP_NORMAL);
                    anyCalcs = true;
                }
                break;
            }
            case UVSCROLL:
            case USCROLL:
            case VSCROLL:
            case ROTATE:
            case TRANSFORM:
                break;
            case PROJECTIVE_TEXTURE:
                _setTextureCoordCalculation(texUnit, TexCoordCalcMethod::PROJECTIVE_TEXTURE,
                    mEffect.second.frustum);
                anyCalcs = true;
                break;
            }
        }
        // Ensure any previous texcoord calc settings are reset if there are now none
        if (!anyCalcs)
        {
            _setTextureCoordCalculation(texUnit, TexCoordCalcMethod::NONE);
        }

        // Change tetxure matrix
        _setTextureMatrix(texUnit, tl.getTextureTransform());


    }

    //-----------------------------------------------------------------------
    void RenderSystem::_disableTextureUnit(size_t texUnit)
    {
        _setTexture(texUnit, false, sNullTexPtr);
    }
    //---------------------------------------------------------------------
    void RenderSystem::_disableTextureUnitsFrom(size_t texUnit)
    {
        size_t disableTo = OGRE_MAX_TEXTURE_LAYERS;
        if (disableTo > mDisabledTexUnitsFrom)
            disableTo = mDisabledTexUnitsFrom;
        mDisabledTexUnitsFrom = texUnit;
        for (size_t i = texUnit; i < disableTo; ++i)
        {
            _disableTextureUnit(i);
        }
    }

    //---------------------------------------------------------------------
    void RenderSystem::_cleanupDepthBuffers( bool bCleanManualBuffers )
    {
        for (auto& itMap : mDepthBufferPool)
        {
            for (auto const& itor : itMap.second)
            {
                if( bCleanManualBuffers || !itor->isManual() )
                    delete itor;
            }

            itMap.second.clear();
        }

        mDepthBufferPool.clear();
    }
    void RenderSystem::_beginFrame()
    {
        if (!mActiveViewport)
            OGRE_EXCEPT(ExceptionCodes::INVALID_STATE, "Cannot begin frame - no viewport selected.");
    }
    //-----------------------------------------------------------------------
    auto RenderSystem::_getCullingMode() const -> CullingMode
    {
        return mCullingMode;
    }
    //-----------------------------------------------------------------------
    void RenderSystem::setDepthBufferFor( RenderTarget *renderTarget )
    {
        auto const poolId = renderTarget->getDepthBufferPool();
        if( poolId == DepthBuffer::PoolId::NO_DEPTH )
            return; //RenderTarget explicitly requested no depth buffer

        //Find a depth buffer in the pool
        bool bAttached = false;
        for (auto const& itor :  mDepthBufferPool[poolId])
            if ((bAttached = renderTarget->attachDepthBuffer(itor)))
                break;

        //Not found yet? Create a new one!
        if( !bAttached )
        {
            DepthBuffer *newDepthBuffer = _createDepthBufferFor( renderTarget );

            if( newDepthBuffer )
            {
                newDepthBuffer->_setPoolId( poolId );
                mDepthBufferPool[poolId].push_back( newDepthBuffer );

                bAttached = renderTarget->attachDepthBuffer( newDepthBuffer );

                OgreAssert( bAttached ,"A new DepthBuffer for a RenderTarget was created, but after creation"
                                     " it says it's incompatible with that RT" );
            }
            else
                LogManager::getSingleton().logWarning( ::std::format("Couldn't create a suited DepthBuffer"
                                                       "for RT: {}", renderTarget->getName()));
        }
    }
    //-----------------------------------------------------------------------
    auto RenderSystem::isReverseDepthBufferEnabled() const noexcept -> bool
    {
        return mIsReverseDepthBufferEnabled;
    }
    //-----------------------------------------------------------------------
    void RenderSystem::reinitialise()
    {
        shutdown();
        _initialise();
    }

    void RenderSystem::shutdown()
    {
        // Remove occlusion queries
        for (auto & mHwOcclusionQuerie : mHwOcclusionQueries)
        {
            delete mHwOcclusionQuerie;
        }
        mHwOcclusionQueries.clear();

        _cleanupDepthBuffers();

        // Remove all the render targets. Destroy primary target last since others may depend on it.
        // Keep mRenderTargets valid all the time, so that render targets could receive
        // appropriate notifications, for example FBO based about GL context destruction.
        RenderTarget* primary = nullptr;
        for (auto it = mRenderTargets.begin(); it != mRenderTargets.end(); /* note - no increment */)
        {
            RenderTarget* current = it->second;
            if (!primary && current->isPrimary())
            {
                ++it;
                primary = current;
            }
            else
            {
                it = mRenderTargets.erase(it);
                delete current;
            }
        }
        delete primary;
        mRenderTargets.clear();

        mPrioritisedRenderTargets.clear();
    }

    //-----------------------------------------------------------------------
    void RenderSystem::_beginGeometryCount()
    {
        mBatchCount = mFaceCount = mVertexCount = 0;

    }
    //-----------------------------------------------------------------------
    auto RenderSystem::_getFaceCount() const -> unsigned int
    {
        return static_cast< unsigned int >( mFaceCount );
    }
    //-----------------------------------------------------------------------
    auto RenderSystem::_getBatchCount() const -> unsigned int
    {
        return static_cast< unsigned int >( mBatchCount );
    }
    //-----------------------------------------------------------------------
    auto RenderSystem::_getVertexCount() const -> unsigned int
    {
        return static_cast< unsigned int >( mVertexCount );
    }
    //-----------------------------------------------------------------------
    void RenderSystem::_render(const RenderOperation& op)
    {
        // Update stats
        size_t val;

        if (op.useIndexes)
            val = op.indexData->indexCount;
        else
            val = op.vertexData->vertexCount;

        size_t trueInstanceNum = std::max<size_t>(op.numberOfInstances,1);
        val *= trueInstanceNum;

        // account for a pass having multiple iterations
        if (mCurrentPassIterationCount > 1)
            val *= mCurrentPassIterationCount;
        mCurrentPassIterationNum = 0;

        using enum RenderOperation::OperationType;
        switch(op.operationType)
        {
        case TRIANGLE_LIST:
            mFaceCount += (val / 3);
            break;
        case TRIANGLE_LIST_ADJ:
            mFaceCount += (val / 6);
            break;
        case TRIANGLE_STRIP_ADJ:
            mFaceCount += (val / 2 - 2);
            break;
        case TRIANGLE_STRIP:
        case TRIANGLE_FAN:
            mFaceCount += (val - 2);
            break;
        default:
            break;
        }

        mVertexCount += op.vertexData->vertexCount * trueInstanceNum;
        mBatchCount += mCurrentPassIterationCount;

        // sort out clip planes
        // have to do it here in case of matrix issues
        if (mClipPlanesDirty)
        {
            setClipPlanesImpl(mClipPlanes);
            mClipPlanesDirty = false;
        }
    }
    //-----------------------------------------------------------------------
    void RenderSystem::setInvertVertexWinding(bool invert)
    {
        mInvertVertexWinding = invert;
    }
    //-----------------------------------------------------------------------
    auto RenderSystem::getInvertVertexWinding() const noexcept -> bool
    {
        return mInvertVertexWinding;
    }
    //---------------------------------------------------------------------
    void RenderSystem::setClipPlanes(const PlaneList& clipPlanes)
    {
        if (clipPlanes != mClipPlanes)
        {
            mClipPlanes = clipPlanes;
            mClipPlanesDirty = true;
        }
    }
    //-----------------------------------------------------------------------
    void RenderSystem::_notifyCameraRemoved(const Camera* cam)
    {
        for (auto const& [key, target] : mRenderTargets)
        {
            target->_notifyCameraRemoved(cam);
        }
    }

    //---------------------------------------------------------------------
    auto RenderSystem::updatePassIterationRenderState() -> bool
    {
        if (mCurrentPassIterationCount <= 1)
            return false;

        // Update derived depth bias
        if (mDerivedDepthBias)
        {
            _setDepthBias(mDerivedDepthBiasBase + mDerivedDepthBiasMultiplier * mCurrentPassIterationNum,
                          mDerivedDepthBiasSlopeScale);
        }

        --mCurrentPassIterationCount;
        ++mCurrentPassIterationNum;

        const auto mask = GpuParamVariability::PASS_ITERATION_NUMBER;

        if (mActiveVertexGpuProgramParameters)
        {
            mActiveVertexGpuProgramParameters->incPassIterationNumber();
            bindGpuProgramParameters(GpuProgramType::VERTEX_PROGRAM, mActiveVertexGpuProgramParameters, mask);
        }
        if (mActiveGeometryGpuProgramParameters)
        {
            mActiveGeometryGpuProgramParameters->incPassIterationNumber();
            bindGpuProgramParameters(GpuProgramType::GEOMETRY_PROGRAM, mActiveGeometryGpuProgramParameters, mask);
        }
        if (mActiveFragmentGpuProgramParameters)
        {
            mActiveFragmentGpuProgramParameters->incPassIterationNumber();
            bindGpuProgramParameters(GpuProgramType::FRAGMENT_PROGRAM, mActiveFragmentGpuProgramParameters, mask);
        }
        if (mActiveTessellationHullGpuProgramParameters)
        {
            mActiveTessellationHullGpuProgramParameters->incPassIterationNumber();
            bindGpuProgramParameters(GpuProgramType::HULL_PROGRAM, mActiveTessellationHullGpuProgramParameters, mask);
        }
        if (mActiveTessellationDomainGpuProgramParameters)
        {
            mActiveTessellationDomainGpuProgramParameters->incPassIterationNumber();
            bindGpuProgramParameters(GpuProgramType::DOMAIN_PROGRAM, mActiveTessellationDomainGpuProgramParameters, mask);
        }
        if (mActiveComputeGpuProgramParameters)
        {
            mActiveComputeGpuProgramParameters->incPassIterationNumber();
            bindGpuProgramParameters(GpuProgramType::COMPUTE_PROGRAM, mActiveComputeGpuProgramParameters, mask);
        }
        return true;
    }

    //-----------------------------------------------------------------------
    void RenderSystem::setSharedListener(Listener* listener)
    {
        assert(msSharedEventListener == nullptr || listener == nullptr); // you can set or reset, but for safety not directly override
        msSharedEventListener = listener;
    }
    //-----------------------------------------------------------------------
    auto RenderSystem::getSharedListener() noexcept -> RenderSystem::Listener*
    {
        return msSharedEventListener;
    }
    //-----------------------------------------------------------------------
    void RenderSystem::addListener(Listener* l)
    {
        mEventListeners.push_back(l);
    }
    //-----------------------------------------------------------------------
    void RenderSystem::removeListener(Listener* l)
    {
        mEventListeners.remove(l);
    }
    //-----------------------------------------------------------------------
    void RenderSystem::fireEvent(std::string_view name, const NameValuePairList* params)
    {
        for(auto & mEventListener : mEventListeners)
        {
            mEventListener->eventOccurred(name, params);
        }

        if(msSharedEventListener)
            msSharedEventListener->eventOccurred(name, params);
    }
    //-----------------------------------------------------------------------
    void RenderSystem::destroyHardwareOcclusionQuery( HardwareOcclusionQuery *hq)
    {
        auto i =
            std::ranges::find(mHwOcclusionQueries, hq);
        if (i != mHwOcclusionQueries.end())
        {
            mHwOcclusionQueries.erase(i);
            delete hq;
        }
    }
    //-----------------------------------------------------------------------
    void RenderSystem::bindGpuProgram(GpuProgram* prg)
    {
        using enum GpuProgramType;
        switch(prg->getType())
        {
        case VERTEX_PROGRAM:
            // mark clip planes dirty if changed (programmable can change space)
            if (!mVertexProgramBound && !mClipPlanes.empty())
                mClipPlanesDirty = true;

            mVertexProgramBound = true;
            break;
        case GEOMETRY_PROGRAM:
            mGeometryProgramBound = true;
            break;
        case FRAGMENT_PROGRAM:
            mFragmentProgramBound = true;
            break;
        case HULL_PROGRAM:
            mTessellationHullProgramBound = true;
            break;
        case DOMAIN_PROGRAM:
            mTessellationDomainProgramBound = true;
            break;
        case COMPUTE_PROGRAM:
            mComputeProgramBound = true;
            break;
        default:
            break;
        }
    }
    //-----------------------------------------------------------------------
    void RenderSystem::unbindGpuProgram(GpuProgramType gptype)
    {
        using enum GpuProgramType;
        switch(gptype)
        {
        case VERTEX_PROGRAM:
            // mark clip planes dirty if changed (programmable can change space)
            if (mVertexProgramBound && !mClipPlanes.empty())
                mClipPlanesDirty = true;
            mVertexProgramBound = false;
            break;
        case GEOMETRY_PROGRAM:
            mGeometryProgramBound = false;
            break;
        case FRAGMENT_PROGRAM:
            mFragmentProgramBound = false;
            break;
        case HULL_PROGRAM:
            mTessellationHullProgramBound = false;
            break;
        case DOMAIN_PROGRAM:
            mTessellationDomainProgramBound = false;
            break;
        case COMPUTE_PROGRAM:
            mComputeProgramBound = false;
            break;
        default:
            break;
        }
    }
    //-----------------------------------------------------------------------
    auto RenderSystem::isGpuProgramBound(GpuProgramType gptype) -> bool
    {
        using enum GpuProgramType;
        switch(gptype)
        {
        case VERTEX_PROGRAM:
            return mVertexProgramBound;
        case GEOMETRY_PROGRAM:
            return mGeometryProgramBound;
        case FRAGMENT_PROGRAM:
            return mFragmentProgramBound;
        case HULL_PROGRAM:
            return mTessellationHullProgramBound;
        case DOMAIN_PROGRAM:
            return mTessellationDomainProgramBound;
        case COMPUTE_PROGRAM:
            return mComputeProgramBound;
        default:
            break;
        }
        // Make compiler happy
        return false;
    }
    //---------------------------------------------------------------------
    void RenderSystem::_setTextureProjectionRelativeTo(bool enabled, const Vector3& pos)
    {
        mTexProjRelative = enabled;
        mTexProjRelativeOrigin = pos;

    }
    //---------------------------------------------------------------------
    auto RenderSystem::_pauseFrame() -> RenderSystem::RenderSystemContext*
    {
        _endFrame();
        return new RenderSystem::RenderSystemContext;
    }
    //---------------------------------------------------------------------
    void RenderSystem::_resumeFrame(RenderSystemContext* context)
    {
        _beginFrame();
        delete context;
    }
    //---------------------------------------------------------------------
    auto RenderSystem::_getDefaultViewportMaterialScheme( ) const -> std::string_view
    {
        if ( !(getCapabilities()->hasCapability(Ogre::Capabilities::FIXED_FUNCTION)) )
        {
            // I am returning the exact value for now - I don't want to add dependency for the RTSS just for one string
            static std::string_view const constexpr ShaderGeneratorDefaultScheme = "ShaderGeneratorDefaultScheme";
            return ShaderGeneratorDefaultScheme;
        }
        else
        {
            return MaterialManager::DEFAULT_SCHEME_NAME;
        }
    }
    //---------------------------------------------------------------------
    auto RenderSystem::getGlobalInstanceVertexBuffer() const -> Ogre::HardwareVertexBufferSharedPtr
    {
        return mGlobalInstanceVertexBuffer;
    }
    //---------------------------------------------------------------------
    void RenderSystem::setGlobalInstanceVertexBuffer( const HardwareVertexBufferSharedPtr &val )
    {
        if ( val && !val->isInstanceData() )
        {
            OGRE_EXCEPT(ExceptionCodes::INVALIDPARAMS,
                        "A none instance data vertex buffer was set to be the global instance vertex buffer.",
                        "RenderSystem::setGlobalInstanceVertexBuffer");
        }
        mGlobalInstanceVertexBuffer = val;
    }
    //---------------------------------------------------------------------
    auto RenderSystem::getGlobalNumberOfInstances() const -> size_t
    {
        return mGlobalNumberOfInstances;
    }
    //---------------------------------------------------------------------
    void RenderSystem::setGlobalNumberOfInstances( const size_t val )
    {
        mGlobalNumberOfInstances = val;
    }

    auto RenderSystem::getGlobalInstanceVertexBufferVertexDeclaration() const noexcept -> VertexDeclaration*
    {
        return mGlobalInstanceVertexBufferVertexDeclaration;
    }
    //---------------------------------------------------------------------
    void RenderSystem::setGlobalInstanceVertexBufferVertexDeclaration( VertexDeclaration* val )
    {
        mGlobalInstanceVertexBufferVertexDeclaration = val;
    }
    //---------------------------------------------------------------------
    void RenderSystem::getCustomAttribute(std::string_view name, void* pData)
    {
        OGRE_EXCEPT(ExceptionCodes::INVALIDPARAMS, "Attribute not found.", "RenderSystem::getCustomAttribute");
    }

    void RenderSystem::initConfigOptions()
    {
        // FS setting possibilities
        ConfigOption optFullScreen;
        optFullScreen.name = "Full Screen";
        optFullScreen.possibleValues.push_back( "No" );
        optFullScreen.possibleValues.push_back( "Yes" );
        optFullScreen.currentValue = optFullScreen.possibleValues[0];
        optFullScreen.immutable = false;
        mOptions["Full Screen"] = optFullScreen;

        ConfigOption optVSync;
        optVSync.name = "VSync";
        optVSync.immutable = false;
        optVSync.possibleValues.push_back("No");
        optVSync.possibleValues.push_back("Yes");
        optVSync.currentValue = optVSync.possibleValues[1];
        mOptions["VSync"] = optVSync;

        ConfigOption optVSyncInterval;
        optVSyncInterval.name = "VSync Interval";
        optVSyncInterval.immutable = false;
        optVSyncInterval.possibleValues.push_back("1");
        optVSyncInterval.possibleValues.push_back("2");
        optVSyncInterval.possibleValues.push_back("3");
        optVSyncInterval.possibleValues.push_back("4");
        optVSyncInterval.currentValue = optVSyncInterval.possibleValues[0];
        mOptions["VSync Interval"] = optVSyncInterval;

        ConfigOption optSRGB;
        optSRGB.name = "sRGB Gamma Conversion";
        optSRGB.immutable = false;
        optSRGB.possibleValues.push_back("No");
        optSRGB.possibleValues.push_back("Yes");
        optSRGB.currentValue = optSRGB.possibleValues[0];
        mOptions["sRGB Gamma Conversion"] = optSRGB;
    }

    auto RenderSystem::reverseCompareFunction(CompareFunction func) -> CompareFunction
    {
        using enum CompareFunction;
        switch(func)
        {
        default:
            return func;
        case LESS:
            return GREATER;
        case LESS_EQUAL:
            return GREATER_EQUAL;
        case GREATER_EQUAL:
            return LESS_EQUAL;
        case GREATER:
            return LESS;
        }
    }

    auto RenderSystem::flipFrontFace() const -> bool
    {
        return mInvertVertexWinding != mActiveRenderTarget->requiresTextureFlipping();
    }
}
