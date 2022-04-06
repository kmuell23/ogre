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
#include <map>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "OgreCommon.hpp"
#include "OgrePrerequisites.hpp"
#include "OgreStringInterface.hpp"

namespace Ogre {
    /// Dictionary of parameters
    static ParamDictionaryMap msDictionary;

    ParamDictionary::ParamDictionary() = default;
    ParamDictionary::~ParamDictionary() = default;

    auto ParamDictionary::getParamCommand(const String& name) -> ParamCommand*
    {
        ParamCommandMap::iterator i = mParamCommands.find(name);
        if (i != mParamCommands.end())
        {
            return i->second;
        }
        else
        {
            return nullptr;
        }
    }

    auto ParamDictionary::getParamCommand(const String& name) const -> const ParamCommand*
    {
        ParamCommandMap::const_iterator i = mParamCommands.find(name);
        if (i != mParamCommands.end())
        {
            return i->second;
        }
        else
        {
            return nullptr;
        }
    }

    void ParamDictionary::addParameter(const String& name, ParamCommand* paramCmd)
    {
        mParamDefs.push_back(name);
        mParamCommands[name] = paramCmd;
    }

    auto StringInterface::createParamDictionary(const String& className) -> bool
    {
        ParamDictionaryMap::iterator it = msDictionary.find(className);

        if ( it == msDictionary.end() )
        {
            mParamDict = &msDictionary.insert( std::make_pair( className, ParamDictionary() ) ).first->second;
            mParamDictName = className;
            return true;
        }
        else
        {
            mParamDict = &it->second;
            mParamDictName = className;
            return false;
        }
    }

    auto StringInterface::getParameters() const -> const ParameterList&
    {
        static ParameterList emptyList;

        const ParamDictionary* dict = getParamDictionary();
        if (dict)
            return dict->getParameters();
        else
            return emptyList;

    }

    auto StringInterface::getParameter(const String& name) const -> String
    {
        // Get dictionary
        const ParamDictionary* dict = getParamDictionary();

        if (dict)
        {
            // Look up command object
            const ParamCommand* cmd = dict->getParamCommand(name);

            if (cmd)
            {
                return cmd->doGet(this);
            }
        }

        // Fallback
        return "";
    }

    auto StringInterface::setParameter(const String& name, const String& value) -> bool
    {
        // Get dictionary
        ParamDictionary* dict = getParamDictionary();

        if (dict)
        {
            // Look up command object
            ParamCommand* cmd = dict->getParamCommand(name);
            if (cmd)
            {
                cmd->doSet(this, value);
                return true;
            }
        }
        // Fallback
        return false;
    }
    //-----------------------------------------------------------------------
    void StringInterface::setParameterList(const NameValuePairList& paramList)
    {
        NameValuePairList::const_iterator i, iend;
        iend = paramList.end();
        for (i = paramList.begin(); i != iend; ++i)
        {
            setParameter(i->first, i->second);
        }
    }

    void StringInterface::copyParametersTo(StringInterface* dest) const
    {
        // Get dictionary
        if (const ParamDictionary* dict = getParamDictionary())
        {
            // Iterate through own parameters
            for (const auto& name : dict->mParamDefs)
            {
                dest->setParameter(name, getParameter(name));
            }
        }
    }

    //-----------------------------------------------------------------------
    void StringInterface::cleanupDictionary ()
    {
        msDictionary.clear();
    }
}
