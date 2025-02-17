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
module Ogre.Core;

import :Common;
import :DataStream;
import :Exception;
import :GpuProgram;
import :GpuProgramManager;
import :GpuProgramParams;
import :Log;
import :LogManager;
import :RenderSystem;
import :RenderSystemCapabilities;
import :ResourceGroupManager;
import :Root;
import :StringConverter;
import :StringInterface;

import <map>;
import <memory>;
import <string>;
import <utility>;

namespace Ogre
{
    //-----------------------------------------------------------------------------
    namespace {
    /// Command object - see ParamCommand
    class CmdType : public ParamCommand
    {
    public:
        auto doGet(const void* target) const -> String override;
        void doSet(void* target, std::string_view val) override;
    };
    class CmdSyntax : public ParamCommand
    {
    public:
        auto doGet(const void* target) const -> String override;
        void doSet(void* target, std::string_view val) override;
    };
    class CmdSkeletal : public ParamCommand
    {
    public:
        auto doGet(const void* target) const -> String override;
        void doSet(void* target, std::string_view val) override;
    };
    class CmdMorph : public ParamCommand
    {
    public:
        auto doGet(const void* target) const -> String override;
        void doSet(void* target, std::string_view val) override;
    };
    class CmdPose : public ParamCommand
    {
    public:
        auto doGet(const void* target) const -> String override;
        void doSet(void* target, std::string_view val) override;
    };
    class CmdVTF : public ParamCommand
    {
    public:
        auto doGet(const void* target) const -> String override;
        void doSet(void* target, std::string_view val) override;
    };
    class CmdManualNamedConstsFile : public ParamCommand
    {
    public:
        auto doGet(const void* target) const -> String override;
        void doSet(void* target, std::string_view val) override;
    };
    class CmdAdjacency : public ParamCommand
    {
    public:
        auto doGet(const void* target) const -> String override;
        void doSet(void* target, std::string_view val) override;
    };
    // Command object for setting / getting parameters
    static CmdType msTypeCmd;
    static CmdSyntax msSyntaxCmd;
    static CmdSkeletal msSkeletalCmd;
    static CmdMorph msMorphCmd;
    static CmdPose msPoseCmd;
    static CmdVTF msVTFCmd;
    static CmdManualNamedConstsFile msManNamedConstsFileCmd;
    static CmdAdjacency msAdjacencyCmd;
    }

    //-----------------------------------------------------------------------------
    GpuProgram::GpuProgram(ResourceManager* creator, std::string_view name, ResourceHandle handle, std::string_view group,
                           bool isManual, ManualResourceLoader* loader)
        : Resource(creator, name, handle, group, isManual, loader) 
    {
        createParameterMappingStructures();
    }
    //-----------------------------------------------------------------------------
    void GpuProgram::setType(GpuProgramType t)
    {
        mType = t;
    }
    //-----------------------------------------------------------------------------
    void GpuProgram::setSyntaxCode(std::string_view syntax)
    {
        mSyntaxCode = syntax;
    }
    //-----------------------------------------------------------------------------
    void GpuProgram::setSourceFile(std::string_view filename)
    {
        OgreAssert(!filename.empty(), "invalid filename");

        mFilename = filename;
        mSource.clear();
        mLoadFromFile = true;
        mCompileError = false;
    }
    //-----------------------------------------------------------------------------
    void GpuProgram::setSource(std::string_view source)
    {
        mSource = source;
        mFilename.clear();
        mLoadFromFile = false;
        mCompileError = false;
    }

    auto GpuProgram::_getHash(uint32 seed) const -> uint32
    {
        // include filename as same source can be used with different defines & entry points
        uint32 hash = FastHash(mName.c_str(), mName.size(), seed);
        return FastHash(mSource.c_str(), mSource.size(), hash);
    }

    auto GpuProgram::calculateSize() const -> size_t
    {
        size_t memSize = sizeof(*this);
        memSize += mManualNamedConstantsFile.size() * sizeof(char);
        memSize += mFilename.size() * sizeof(char);
        memSize += mSource.size() * sizeof(char);
        memSize += mSyntaxCode.size() * sizeof(char);

        size_t paramsSize = 0;
        if(mDefaultParams)
            paramsSize += mDefaultParams->calculateSize();
        if(mConstantDefs)
            paramsSize += mConstantDefs->calculateSize();

        return memSize + paramsSize;
    }
    //-----------------------------------------------------------------------------
    void GpuProgram::prepareImpl()
    {
        if (!mLoadFromFile)
            return;

        mSource = ResourceGroupManager::getSingleton().openResource(mFilename, mGroup, this)->getAsString();
    }

    void GpuProgram::safePrepare()
    {
        try
        {
            prepare();
        }
        catch (const RuntimeAssertionException&)
        {
            throw;
        }
        catch (const Exception& e)
        {
            // will already have been logged
            LogManager::getSingleton().stream(LogMessageLevel::Critical)
                << "Program '" << mName << "' is not supported: " << e.getDescription();

            mCompileError = true;
        }
    }

    void GpuProgram::loadImpl()
    {
        if(mCompileError)
            return;

        // Call polymorphic load
        try 
        {
            loadFromSource();
        }
        catch (const RuntimeAssertionException&)
        {
            throw;
        }
        catch (const Exception& e)
        {
            // will already have been logged
            LogManager::getSingleton().stream(LogMessageLevel::Critical)
                << "Program '" << mName << "' is not supported: " << e.getDescription();

            mCompileError = true;
        }
    }
    void GpuProgram::postLoadImpl()
    {
        if (!mDefaultParams || mCompileError)
            return;

        // Keep a reference to old ones to copy
        GpuProgramParametersSharedPtr savedParams = mDefaultParams;
        // reset params to stop them being referenced in the next create
        mDefaultParams.reset();

        // Create new params
        mDefaultParams = createParameters();

        // Copy old (matching) values across
        // Don't use copyConstantsFrom since program may be different
        mDefaultParams->copyMatchingNamedConstantsFrom(*savedParams.get());
    }

    //-----------------------------------------------------------------------------
    auto GpuProgram::isRequiredCapabilitiesSupported() const noexcept -> bool
    {
        const RenderSystemCapabilities* caps = 
            Root::getSingleton().getRenderSystem()->getCapabilities();

        // Basic support check
        if ((getType() == GpuProgramType::GEOMETRY_PROGRAM && !caps->hasCapability(Capabilities::GEOMETRY_PROGRAM)) ||
            (getType() == GpuProgramType::DOMAIN_PROGRAM && !caps->hasCapability(Capabilities::TESSELLATION_DOMAIN_PROGRAM)) ||
            (getType() == GpuProgramType::HULL_PROGRAM && !caps->hasCapability(Capabilities::TESSELLATION_HULL_PROGRAM)) ||
            (getType() == GpuProgramType::COMPUTE_PROGRAM && !caps->hasCapability(Capabilities::COMPUTE_PROGRAM)))
        {
            return false;
        }

        // Vertex texture fetch required?
        if (isVertexTextureFetchRequired() && 
            !caps->hasCapability(Capabilities::VERTEX_TEXTURE_FETCH))
        {
            return false;
        }

        return true;
    }
    //-----------------------------------------------------------------------------
    auto GpuProgram::isSupported() const noexcept -> bool
    {
        if (mCompileError || !isRequiredCapabilitiesSupported())
            return false;

        return GpuProgramManager::getSingleton().isSyntaxSupported(mSyntaxCode);
    }
    //---------------------------------------------------------------------
    void GpuProgram::createParameterMappingStructures(bool recreateIfExists)
    {
        createLogicalParameterMappingStructures(recreateIfExists);
        createNamedParameterMappingStructures(recreateIfExists);
    }
    //---------------------------------------------------------------------
    void GpuProgram::createLogicalParameterMappingStructures(bool recreateIfExists)
    {
        if (recreateIfExists || !mLogicalToPhysical)
            mLogicalToPhysical = GpuLogicalBufferStructPtr(new GpuLogicalBufferStruct());
    }
    //---------------------------------------------------------------------
    void GpuProgram::createNamedParameterMappingStructures(bool recreateIfExists)
    {
        if (recreateIfExists || !mConstantDefs)
            mConstantDefs = GpuNamedConstantsPtr(new GpuNamedConstants());
    }
    //---------------------------------------------------------------------
    void GpuProgram::setManualNamedConstantsFile(std::string_view paramDefFile)
    {
        mManualNamedConstantsFile = paramDefFile;
        mLoadedManualNamedConstants = false;
    }
    //---------------------------------------------------------------------
    void GpuProgram::setManualNamedConstants(const GpuNamedConstants& namedConstants)
    {
        createParameterMappingStructures();
        *mConstantDefs.get() = namedConstants;

        mLogicalToPhysical->bufferSize = mConstantDefs->bufferSize;
        mLogicalToPhysical->map.clear();
        // need to set up logical mappings too for some rendersystems
        for (auto const& [name, def] : mConstantDefs->map)
        {
            // only consider non-array entries
            if (name.find('[') == String::npos)
            {
                GpuLogicalIndexUseMap::value_type val{
                    def.logicalIndex,
                    GpuLogicalIndexUse{def.physicalIndex, def.arraySize * def.elementSize, def.variability,
                                       GpuConstantDefinition::getBaseType(def.constType)}};
                mLogicalToPhysical->map.emplace(val);
            }
        }


    }
    //-----------------------------------------------------------------------------
    auto GpuProgram::createParameters() -> GpuProgramParametersSharedPtr
    {
        // Default implementation simply returns standard parameters.
        GpuProgramParametersSharedPtr ret = 
            GpuProgramManager::getSingleton().createParameters();
        
        
        // optionally load manually supplied named constants
        if (!mManualNamedConstantsFile.empty() && !mLoadedManualNamedConstants)
        {
            try 
            {
                GpuNamedConstants namedConstants;
                DataStreamPtr stream = 
                    ResourceGroupManager::getSingleton().openResource(
                    mManualNamedConstantsFile, mGroup, this);
                namedConstants.load(stream);
                setManualNamedConstants(namedConstants);
            }
            catch(const Exception& e)
            {
                LogManager::getSingleton().stream() <<
                    "Unable to load manual named constants for GpuProgram " << mName <<
                    ": " << e.getDescription();
            }
            mLoadedManualNamedConstants = true;
        }
        
        
        // set up named parameters, if any
        if (mConstantDefs && !mConstantDefs->map.empty())
        {
            ret->_setNamedConstants(mConstantDefs);
        }
        // link shared logical / physical map for low-level use
        ret->_setLogicalIndexes(mLogicalToPhysical);

        // Copy in default parameters if present
        if (mDefaultParams)
            ret->copyConstantsFrom(*(mDefaultParams.get()));
        
        return ret;
    }
    //-----------------------------------------------------------------------------
    auto GpuProgram::getDefaultParameters() noexcept -> const GpuProgramParametersPtr&
    {
        if (!mDefaultParams)
        {
            mDefaultParams = createParameters();
        }
        return mDefaultParams;
    }
    //-----------------------------------------------------------------------------
    auto GpuProgram::getProgramTypeName(GpuProgramType programType) -> const String
    {
        using enum GpuProgramType;
        switch (programType)
        {
        case VERTEX_PROGRAM:
            return "vertex";
        case GEOMETRY_PROGRAM:
            return "geometry";
        case FRAGMENT_PROGRAM:
            return "fragment";
        case DOMAIN_PROGRAM:
            return "domain";
        case HULL_PROGRAM:
            return "hull";
        case COMPUTE_PROGRAM:
            return "compute";
        default:
            OGRE_EXCEPT(ExceptionCodes::INVALIDPARAMS,
                "Unexpected GPU program type",
                "GpuProgram::GetName");
        }
    }
    //-----------------------------------------------------------------------------
    void GpuProgram::setupBaseParamDictionary()
    {
        ParamDictionary* dict = getParamDictionary();

        dict->addParameter(
            ParameterDef("type", "'vertex_program', 'geometry_program', 'fragment_program', 'hull_program', 'domain_program', 'compute_program'",
                         ParameterType::STRING), &msTypeCmd);
        dict->addParameter(
            ParameterDef("syntax", "Syntax code, e.g. vs_1_1", ParameterType::STRING), &msSyntaxCmd);
        dict->addParameter(
            ParameterDef("includes_skeletal_animation", 
                         "Whether this vertex program includes skeletal animation", ParameterType::BOOL), 
            &msSkeletalCmd);
        dict->addParameter(
            ParameterDef("includes_morph_animation", 
                         "Whether this vertex program includes morph animation", ParameterType::BOOL), 
            &msMorphCmd);
        dict->addParameter(
            ParameterDef("includes_pose_animation", 
                         "The number of poses this vertex program supports for pose animation", ParameterType::INT),
            &msPoseCmd);
        dict->addParameter(
            ParameterDef("uses_vertex_texture_fetch", 
                         "Whether this vertex program requires vertex texture fetch support.", ParameterType::BOOL), 
            &msVTFCmd);
        dict->addParameter(
            ParameterDef("manual_named_constants", 
                         "File containing named parameter mappings for low-level programs.", ParameterType::BOOL), 
            &msManNamedConstsFileCmd);
        dict->addParameter(
            ParameterDef("uses_adjacency_information",
                         "Whether this geometry program requires adjacency information from the input primitives.", ParameterType::BOOL),
            &msAdjacencyCmd);
    }

    //-----------------------------------------------------------------------
    auto GpuProgram::getLanguage() const noexcept -> std::string_view
    {
        static std::string_view const constexpr language = "asm";

        return language;
    }
    //-----------------------------------------------------------------------
    //-----------------------------------------------------------------------
    auto CmdType::doGet(const void* target) const -> String
    {
        const auto* t = static_cast<const GpuProgram*>(target);
        return ::std::format("{}_program", GpuProgram::getProgramTypeName(t->getType()));
    }
    void CmdType::doSet(void* target, std::string_view val)
    {
        auto* t = static_cast<GpuProgram*>(target);
        if (val == "vertex_program")
        {
            t->setType(GpuProgramType::VERTEX_PROGRAM);
        }
        else if (val == "geometry_program")
        {
            t->setType(GpuProgramType::GEOMETRY_PROGRAM);
        }
        else if (val == "domain_program")
        {
            t->setType(GpuProgramType::DOMAIN_PROGRAM);
        }
        else if (val == "hull_program")
        {
            t->setType(GpuProgramType::HULL_PROGRAM);
        }
        else if (val == "compute_program")
        {
            t->setType(GpuProgramType::COMPUTE_PROGRAM);
        }
        else
        {
            t->setType(GpuProgramType::FRAGMENT_PROGRAM);
        }
    }
    //-----------------------------------------------------------------------
    auto CmdSyntax::doGet(const void* target) const -> String
    {
        const auto* t = static_cast<const GpuProgram*>(target);
        return std::string{ t->getSyntaxCode() };
    }
    void CmdSyntax::doSet(void* target, std::string_view val)
    {
        auto* t = static_cast<GpuProgram*>(target);
        t->setSyntaxCode(val);
    }
    //-----------------------------------------------------------------------
    auto CmdSkeletal::doGet(const void* target) const -> String
    {
        const auto* t = static_cast<const GpuProgram*>(target);
        return StringConverter::toString(t->isSkeletalAnimationIncluded());
    }
    void CmdSkeletal::doSet(void* target, std::string_view val)
    {
        auto* t = static_cast<GpuProgram*>(target);
        t->setSkeletalAnimationIncluded(StringConverter::parseBool(val));
    }
    //-----------------------------------------------------------------------
    auto CmdMorph::doGet(const void* target) const -> String
    {
        const auto* t = static_cast<const GpuProgram*>(target);
        return StringConverter::toString(t->isMorphAnimationIncluded());
    }
    void CmdMorph::doSet(void* target, std::string_view val)
    {
        auto* t = static_cast<GpuProgram*>(target);
        t->setMorphAnimationIncluded(StringConverter::parseBool(val));
    }
    //-----------------------------------------------------------------------
    auto CmdPose::doGet(const void* target) const -> String
    {
        const auto* t = static_cast<const GpuProgram*>(target);
        return StringConverter::toString(t->getNumberOfPosesIncluded());
    }
    void CmdPose::doSet(void* target, std::string_view val)
    {
        auto* t = static_cast<GpuProgram*>(target);
        t->setPoseAnimationIncluded((ushort)StringConverter::parseUnsignedInt(val));
    }
    //-----------------------------------------------------------------------
    auto CmdVTF::doGet(const void* target) const -> String
    {
        const auto* t = static_cast<const GpuProgram*>(target);
        return StringConverter::toString(t->isVertexTextureFetchRequired());
    }
    void CmdVTF::doSet(void* target, std::string_view val)
    {
        auto* t = static_cast<GpuProgram*>(target);
        t->setVertexTextureFetchRequired(StringConverter::parseBool(val));
    }
    //-----------------------------------------------------------------------
    auto CmdManualNamedConstsFile::doGet(const void* target) const -> String
    {
        const auto* t = static_cast<const GpuProgram*>(target);
        return std::string{ t->getManualNamedConstantsFile() };
    }
    void CmdManualNamedConstsFile::doSet(void* target, std::string_view val)
    {
        auto* t = static_cast<GpuProgram*>(target);
        t->setManualNamedConstantsFile(val);
    }
    //-----------------------------------------------------------------------
    auto CmdAdjacency::doGet(const void* target) const -> String
    {
        const auto* t = static_cast<const GpuProgram*>(target);
        return StringConverter::toString(t->isAdjacencyInfoRequired());
    }
    void CmdAdjacency::doSet(void* target, std::string_view val)
    {
        LogManager::getSingleton().logWarning("'uses_adjacency_information' is deprecated. "
        "Set the respective RenderOperation::OpertionType instead.");
        auto* t = static_cast<GpuProgram*>(target);
        t->setAdjacencyInfoRequired(StringConverter::parseBool(val));
    }
}
