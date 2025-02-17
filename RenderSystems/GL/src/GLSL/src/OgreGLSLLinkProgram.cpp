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

#include "glad/glad.h"
#include <cstring>

module Ogre.RenderSystems.GL;

import :GLSL.SLExtSupport;
import :GLSL.SLLinkProgram;
import :GLSL.SLLinkProgramManager;
import :GLSL.SLProgram;

import Ogre.Core;
import Ogre.RenderSystems.GLSupport;

import <string>;
import <vector>;

namespace Ogre::GLSL {

    static auto getGLGeometryInputPrimitiveType(RenderOperation::OperationType operationType) -> GLint
    {
        using enum RenderOperation::OperationType;
        switch (operationType)
        {
        case POINT_LIST:
            return GL_POINTS;
        case LINE_LIST:
        case LINE_STRIP:
			return GL_LINES;
        case LINE_LIST_ADJ:
        case LINE_STRIP_ADJ:
			return GL_LINES_ADJACENCY_EXT;
        case TRIANGLE_LIST_ADJ:
        case TRIANGLE_STRIP_ADJ:
            return GL_TRIANGLES_ADJACENCY_EXT;
        default:
        case TRIANGLE_LIST:
        case TRIANGLE_STRIP:
        case TRIANGLE_FAN:
            return GL_TRIANGLES;
		}
    }
    //-----------------------------------------------------------------------
    static auto getGLGeometryOutputPrimitiveType(RenderOperation::OperationType operationType) -> GLint
    {
        using enum RenderOperation::OperationType;
        switch (operationType)
        {
        case POINT_LIST:
            return GL_POINTS;
        case LINE_STRIP:
            return GL_LINE_STRIP;
        default:
        case TRIANGLE_STRIP:
            return GL_TRIANGLE_STRIP;
        }
    }

    //-----------------------------------------------------------------------
    GLSLLinkProgram::GLSLLinkProgram(const GLShaderList& shaders) : GLSLProgramCommon(shaders)
    {
        // Initialise uniform cache
        mUniformCache = ::std::make_unique<GLUniformCache>();
    }

    //-----------------------------------------------------------------------
    GLSLLinkProgram::~GLSLLinkProgram()
    {
        glDeleteObjectARB((GLhandleARB)mGLProgramHandle);
    }

    //-----------------------------------------------------------------------
    void GLSLLinkProgram::activate()
    {
        if (!mLinked)
        {           
            glGetError(); //Clean up the error. Otherwise will flood log.

            mGLProgramHandle = (size_t)glCreateProgramObjectARB();

            GLenum glErr = glGetError();
            if(glErr != GL_NO_ERROR)
            {
                reportGLSLError( glErr, "GLSLLinkProgram::activate", "Error Creating GLSL Program Object", 0 );
            }

            uint32 hash = getCombinedHash();

            if ( GpuProgramManager::getSingleton().canGetCompiledShaderBuffer() &&
                 GpuProgramManager::getSingleton().isMicrocodeAvailableInCache(hash) &&
                 !mShaders[std::to_underlying(GpuProgramType::GEOMETRY_PROGRAM)])
            {
                getMicrocodeFromCache(hash);
            }
            else
            {
                compileAndLink();

            }
            buildGLUniformReferences();
            extractAttributes();
        }
        if (mLinked)
        {
            glUseProgramObjectARB( (GLhandleARB)mGLProgramHandle );

            GLenum glErr = glGetError();
            if(glErr != GL_NO_ERROR)
            {
                reportGLSLError( glErr, "GLSLLinkProgram::Activate",
                    "Error using GLSL Program Object : ", mGLProgramHandle, false, false);
            }
        }
    }
    //-----------------------------------------------------------------------
    void GLSLLinkProgram::getMicrocodeFromCache(uint32 id)
    {
        GpuProgramManager::Microcode cacheMicrocode = 
            GpuProgramManager::getSingleton().getMicrocodeFromCache(id);
        
        GLenum binaryFormat = *((GLenum *)(cacheMicrocode->getPtr()));
        uint8 * programBuffer = cacheMicrocode->getPtr() + sizeof(GLenum);
        size_t sizeOfBuffer = cacheMicrocode->size() - sizeof(GLenum);
        glProgramBinary(mGLProgramHandle,
                        binaryFormat, 
                        programBuffer,
                        static_cast<GLsizei>(sizeOfBuffer)
                        );

        glGetProgramiv(mGLProgramHandle, GL_LINK_STATUS, &mLinked);
        if (!mLinked)
        {
            //
            // Something must have changed since the program binaries
            // were cached away.  Fallback to source shader loading path,
            // and then retrieve and cache new program binaries once again.
            //
            compileAndLink();
        }
    }
    //-----------------------------------------------------------------------
    void GLSLLinkProgram::extractAttributes()
    {
        size_t numAttribs = sizeof(msCustomAttributes)/sizeof(CustomAttribute);

        for (size_t i = 0; i < numAttribs; ++i)
        {
            const CustomAttribute& a = msCustomAttributes[i];
            GLint attrib = glGetAttribLocationARB((GLhandleARB)mGLProgramHandle, a.name);

            if (attrib != -1)
            {
                mValidAttributes.insert(a.attrib);

                if(a.semantic != VertexElementSemantic::TEXTURE_COORDINATES) continue;

                // also enable next 4 attributes to allow matrix types in texcoord semantic
                // might cause problems with mixing builtin and custom names,
                // but then again you should not
                for(int j = 0; j < 4; j++)
                    mValidAttributes.insert(msCustomAttributes[i + j].attrib);
            }
        }
    }
    //-----------------------------------------------------------------------
    auto GLSLLinkProgram::isAttributeValid(VertexElementSemantic semantic, uint index) -> bool
    {
        return mValidAttributes.find(getFixedAttributeIndex(semantic, index)) != mValidAttributes.end();
    }
    //-----------------------------------------------------------------------
    void GLSLLinkProgram::buildGLUniformReferences()
    {
        if (!mUniformRefsBuilt)
        {
            const GpuConstantDefinitionMap* vertParams = nullptr;
            const GpuConstantDefinitionMap* fragParams = nullptr;
            const GpuConstantDefinitionMap* geomParams = nullptr;
            if (mShaders[std::to_underlying(GpuProgramType::VERTEX_PROGRAM)])
            {
                vertParams = &(mShaders[std::to_underlying(GpuProgramType::VERTEX_PROGRAM)]->getConstantDefinitions().map);
            }
            if (mShaders[std::to_underlying(GpuProgramType::GEOMETRY_PROGRAM)])
            {
                geomParams = &(mShaders[std::to_underlying(GpuProgramType::GEOMETRY_PROGRAM)]->getConstantDefinitions().map);
            }
            if (mShaders[std::to_underlying(GpuProgramType::FRAGMENT_PROGRAM)])
            {
                fragParams = &(mShaders[std::to_underlying(GpuProgramType::FRAGMENT_PROGRAM)]->getConstantDefinitions().map);
            }

            GLSLLinkProgramManager::extractUniforms(
                    mGLProgramHandle, vertParams, geomParams, fragParams, mGLUniformReferences);

            mUniformRefsBuilt = true;
        }
    }

    //-----------------------------------------------------------------------
    void GLSLLinkProgram::updateUniforms(GpuProgramParametersSharedPtr params, 
        GpuParamVariability mask, GpuProgramType fromProgType)
    {
        // iterate through uniform reference list and update uniform values

        // determine if we need to transpose matrices when binding
        bool transpose = !mShaders[std::to_underlying(fromProgType)] || mShaders[std::to_underlying(fromProgType)]->getColumnMajorMatrices();

        for (auto const& currentUniform : mGLUniformReferences)
        {
            // Only pull values from buffer it's supposed to be in (vertex or fragment)
            // This method will be called twice, once for vertex program params, 
            // and once for fragment program params.
            if (fromProgType == currentUniform.mSourceProgType)
            {
                const GpuConstantDefinition* def = currentUniform.mConstantDef;
                if (!!(def->variability & mask))
                {

                    auto glArraySize = (GLsizei)def->arraySize;

                    void* val = def->isSampler() ? (void*)params->getRegPointer(def->physicalIndex)
                                                 : (void*)params->getFloatPointer(def->physicalIndex);

                    bool shouldUpdate = mUniformCache->updateUniform(currentUniform.mLocation, val,
                                                                     def->elementSize * def->arraySize * 4);
                    if(!shouldUpdate)
                        continue;

                    using enum GpuConstantType;
                    // get the index in the parameter real list
                    switch (def->constType)
                    {
                    case FLOAT1:
                        glUniform1fvARB(currentUniform.mLocation, glArraySize,
                            params->getFloatPointer(def->physicalIndex));
                        break;
                    case FLOAT2:
                        glUniform2fvARB(currentUniform.mLocation, glArraySize,
                            params->getFloatPointer(def->physicalIndex));
                        break;
                    case FLOAT3:
                        glUniform3fvARB(currentUniform.mLocation, glArraySize,
                            params->getFloatPointer(def->physicalIndex));
                        break;
                    case FLOAT4:
                        glUniform4fvARB(currentUniform.mLocation, glArraySize,
                            params->getFloatPointer(def->physicalIndex));
                        break;
                    case MATRIX_2X2:
                        glUniformMatrix2fvARB(currentUniform.mLocation, glArraySize,
                            transpose, params->getFloatPointer(def->physicalIndex));
                        break;
                    case MATRIX_2X3:
                        if (GLAD_GL_VERSION_2_1)
                        {
                            glUniformMatrix2x3fv(currentUniform.mLocation, glArraySize,
                                GL_FALSE, params->getFloatPointer(def->physicalIndex));
                        }
                        break;
                    case MATRIX_2X4:
                        if (GLAD_GL_VERSION_2_1)
                        {
                            glUniformMatrix2x4fv(currentUniform.mLocation, glArraySize,
                                GL_FALSE, params->getFloatPointer(def->physicalIndex));
                        }
                        break;
                    case MATRIX_3X2:
                        if (GLAD_GL_VERSION_2_1)
                        {
                            glUniformMatrix3x2fv(currentUniform.mLocation, glArraySize,
                                GL_FALSE, params->getFloatPointer(def->physicalIndex));
                        }
                        break;
                    case MATRIX_3X3:
                        glUniformMatrix3fvARB(currentUniform.mLocation, glArraySize,
                            transpose, params->getFloatPointer(def->physicalIndex));
                        break;
                    case MATRIX_3X4:
                        if (GLAD_GL_VERSION_2_1)
                        {
                            glUniformMatrix3x4fv(currentUniform.mLocation, glArraySize,
                                GL_FALSE, params->getFloatPointer(def->physicalIndex));
                        }
                        break;
                    case MATRIX_4X2:
                        if (GLAD_GL_VERSION_2_1)
                        {
                            glUniformMatrix4x2fv(currentUniform.mLocation, glArraySize,
                                GL_FALSE, params->getFloatPointer(def->physicalIndex));
                        }
                        break;
                    case MATRIX_4X3:
                        if (GLAD_GL_VERSION_2_1)
                        {
                            glUniformMatrix4x3fv(currentUniform.mLocation, glArraySize,
                                GL_FALSE, params->getFloatPointer(def->physicalIndex));
                        }
                        break;
                    case MATRIX_4X4:
                        glUniformMatrix4fvARB(currentUniform.mLocation, glArraySize,
                            transpose, params->getFloatPointer(def->physicalIndex));
                        break;
                    case SAMPLER1D:
                    case SAMPLER1DSHADOW:
                    case SAMPLER2D:
                    case SAMPLER2DSHADOW:
                    case SAMPLER2DARRAY:
                    case SAMPLER3D:
                    case SAMPLERCUBE:
                        // samplers handled like 1-element ints
                    case INT1:
                        glUniform1ivARB(currentUniform.mLocation, glArraySize, (GLint*)val);
                        break;
                    case INT2:
                        glUniform2ivARB(currentUniform.mLocation, glArraySize,
                            (GLint*)params->getIntPointer(def->physicalIndex));
                        break;
                    case INT3:
                        glUniform3ivARB(currentUniform.mLocation, glArraySize,
                            (GLint*)params->getIntPointer(def->physicalIndex));
                        break;
                    case INT4:
                        glUniform4ivARB(currentUniform.mLocation, glArraySize,
                            (GLint*)params->getIntPointer(def->physicalIndex));
                        break;
                    case UNKNOWN:
                    default:
                        break;

                    } // end switch
                } // variability & mask
            } // fromProgType == currentUniform->mSourceProgType
  
        } // end for
    }
    //-----------------------------------------------------------------------
    void GLSLLinkProgram::compileAndLink()
    {
        uint32 hash = 0;
        if (mShaders[std::to_underlying(GpuProgramType::VERTEX_PROGRAM)])
        {
            // attach Vertex Program
            mShaders[std::to_underlying(GpuProgramType::VERTEX_PROGRAM)]->attachToProgramObject(mGLProgramHandle);

            // Some drivers (e.g. OS X on nvidia) incorrectly determine the attribute binding automatically

            // and end up aliasing existing built-ins. So avoid! 
            // Bind all used attribs - not all possible ones otherwise we'll get 
            // lots of warnings in the log, and also may end up aliasing names used
            // as varyings by accident
            // Because we can't ask GL whether an attribute is used in the shader
            // until it is linked (chicken and egg!) we have to parse the source

            size_t numAttribs = sizeof(msCustomAttributes)/sizeof(CustomAttribute);
            std::string_view vpSource = mShaders[std::to_underlying(GpuProgramType::VERTEX_PROGRAM)]->getSource();
            
            hash = mShaders[std::to_underlying(GpuProgramType::VERTEX_PROGRAM)]->_getHash(hash);
            for (size_t i = 0; i < numAttribs; ++i)
            {
                const CustomAttribute& a = msCustomAttributes[i];

                // we're looking for either: 
                //   attribute vec<n> <semantic_name>
                //   in vec<n> <semantic_name>
                // The latter is recommended in GLSL 1.3 onwards 
                // be slightly flexible about formatting
                String::size_type pos = vpSource.find(a.name);
                bool foundAttr = false;
                while (pos != String::npos && !foundAttr)
                {
                    String::size_type startpos = vpSource.find("attribute", pos < 20 ? 0 : pos-20);
                    if (startpos == String::npos)
                        startpos = vpSource.find("in", pos-20);
                    if (startpos != String::npos && startpos < pos)
                    {
                        // final check 
                        auto const expr = vpSource.substr(startpos, pos + strlen(a.name) - startpos);
                        auto const vec = StringUtil::split(expr);
                        if ((vec[0] == "in" || vec[0] == "attribute") && vec[2] == a.name)
                        {
                            glBindAttribLocationARB((GLhandleARB)mGLProgramHandle, a.attrib, a.name);
                            foundAttr = true;
                        }
                    }
                    // Find the position of the next occurrence if needed
                    pos = vpSource.find(a.name, pos + strlen(a.name));
                }
            }
        }

        if (auto gshader = static_cast<GLSLProgram*>(mShaders[std::to_underlying(GpuProgramType::GEOMETRY_PROGRAM)]))
        {
            hash = mShaders[std::to_underlying(GpuProgramType::GEOMETRY_PROGRAM)]->_getHash(hash);
            // attach Geometry Program
            mShaders[std::to_underlying(GpuProgramType::GEOMETRY_PROGRAM)]->attachToProgramObject(mGLProgramHandle);

            //Don't set adjacency flag. We handle it internally and expose "false"

            RenderOperation::OperationType inputOperationType = gshader->getInputOperationType();
            glProgramParameteriEXT(mGLProgramHandle, GL_GEOMETRY_INPUT_TYPE_EXT,
                getGLGeometryInputPrimitiveType(inputOperationType));

            RenderOperation::OperationType outputOperationType = gshader->getOutputOperationType();

            glProgramParameteriEXT(mGLProgramHandle, GL_GEOMETRY_OUTPUT_TYPE_EXT,
                getGLGeometryOutputPrimitiveType(outputOperationType));

            glProgramParameteriEXT(mGLProgramHandle, GL_GEOMETRY_VERTICES_OUT_EXT,
                gshader->getMaxOutputVertices());
        }

        if (mShaders[std::to_underlying(GpuProgramType::FRAGMENT_PROGRAM)])
        {
            hash = mShaders[std::to_underlying(GpuProgramType::FRAGMENT_PROGRAM)]->_getHash(hash);
            // attach Fragment Program
            mShaders[std::to_underlying(GpuProgramType::FRAGMENT_PROGRAM)]->attachToProgramObject(mGLProgramHandle);
        }

        
        // now the link

        glLinkProgramARB( (GLhandleARB)mGLProgramHandle );
        glGetObjectParameterivARB( (GLhandleARB)mGLProgramHandle, GL_OBJECT_LINK_STATUS_ARB, &mLinked );

        // force logging and raise exception if not linked
        GLenum glErr = glGetError();
        if(glErr != GL_NO_ERROR)
        {
            reportGLSLError( glErr, "GLSLLinkProgram::compileAndLink",
                "Error linking GLSL Program Object : ", mGLProgramHandle, !mLinked, !mLinked );
        }
        
        if(mLinked)
        {
            logObjectInfo(  getCombinedName() + String(" GLSL link result : "), mGLProgramHandle );
        }

        if (mLinked)
        {
            if ( GpuProgramManager::getSingleton().getSaveMicrocodesToCache() )
            {
                // add to the microcode to the cache

                // get buffer size
                GLint binaryLength = 0;
                glGetProgramiv(mGLProgramHandle, GL_PROGRAM_BINARY_LENGTH, &binaryLength);

                // turns out we need this param when loading
                // it will be the first bytes of the array in the microcode
                GLenum binaryFormat = 0; 

                // create microcode
                GpuProgramManager::Microcode newMicrocode = 
                    GpuProgramManager::getSingleton().createMicrocode(binaryLength + sizeof(GLenum));

                // get binary
                uint8 * programBuffer = newMicrocode->getPtr() + sizeof(GLenum);
                glGetProgramBinary(mGLProgramHandle, binaryLength, nullptr, &binaryFormat, programBuffer);

                // save binary format
                memcpy(newMicrocode->getPtr(), &binaryFormat, sizeof(GLenum));

                // add to the microcode to the cache
                GpuProgramManager::getSingleton().addMicrocodeToCache(hash, newMicrocode);
            }
        }
    }
    //-----------------------------------------------------------------------
} // namespace Ogre
