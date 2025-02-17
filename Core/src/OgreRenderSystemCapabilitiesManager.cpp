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

module Ogre.Core;

import :Archive;
import :ArchiveManager;
import :DataStream;
import :RenderSystemCapabilities;
import :RenderSystemCapabilitiesManager;
import :RenderSystemCapabilitiesSerializer;
import :SharedPtr;
import :StringVector;

import <utility>;
import <vector>;

namespace Ogre {

    //-----------------------------------------------------------------------
    template<> RenderSystemCapabilitiesManager* Singleton<RenderSystemCapabilitiesManager>::msSingleton = nullptr;
    auto RenderSystemCapabilitiesManager::getSingletonPtr() noexcept -> RenderSystemCapabilitiesManager*
    {
        return msSingleton;
    }
    auto RenderSystemCapabilitiesManager::getSingleton() noexcept -> RenderSystemCapabilitiesManager&
    {
        assert( msSingleton );  return ( *msSingleton );
    }

    //-----------------------------------------------------------------------
    RenderSystemCapabilitiesManager::RenderSystemCapabilitiesManager() :  mScriptPattern("*.rendercaps")
    {
        mSerializer = ::std::make_unique<RenderSystemCapabilitiesSerializer>();
    }

    //-----------------------------------------------------------------------
    void RenderSystemCapabilitiesManager::parseCapabilitiesFromArchive(std::string_view filename, std::string_view archiveType, bool recursive)
    {
        // get the list of .rendercaps files
        Archive* arch = ArchiveManager::getSingleton().load(filename, archiveType, true);
        StringVectorPtr files = arch->find(mScriptPattern, recursive);

        // loop through .rendercaps files and load each one
        for (auto & it : *files)
        {
            DataStreamPtr stream = arch->open(it);
            mSerializer->parseScript(stream);
            stream->close();
        }
    }

    auto RenderSystemCapabilitiesManager::loadParsedCapabilities(std::string_view name) -> RenderSystemCapabilities*
    {
        return mCapabilitiesMap.find(name)->second.get();
    }

    auto RenderSystemCapabilitiesManager::getCapabilities() const -> const CapabilitiesMap&
    {
        return mCapabilitiesMap;
    }

    /** Method used by RenderSystemCapabilitiesSerializer::parseScript */
    void RenderSystemCapabilitiesManager::_addRenderSystemCapabilities(std::string_view name, RenderSystemCapabilities* caps)
    {
        mCapabilitiesMap.emplace(name, caps);
    }
}
