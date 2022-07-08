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

#ifndef OGRE_CORE_SCRIPTPARSER_H
#define OGRE_CORE_SCRIPTPARSER_H

#include "OgreMemoryAllocatorConfig.hpp"
#include "OgrePrerequisites.hpp"
#include "OgreScriptCompiler.hpp"
#include "OgreScriptLexer.hpp"

namespace Ogre {

    /** \addtogroup Core
    *  @{
    */
    /** \addtogroup General
    *  @{
    */

    class ScriptParser : public ScriptCompilerAlloc
    {
    public:
        static auto parse(const ScriptTokenList &tokens, StringView file) -> ConcreteNodeListPtr;
        static auto parseChunk(const ScriptTokenList &tokens, StringView file) -> ConcreteNodeListPtr;
    private:
        static auto getToken(ScriptTokenList::const_iterator i, ScriptTokenList::const_iterator end, int offset) -> const ScriptToken *;
        static auto skipNewlines(ScriptTokenList::const_iterator i, ScriptTokenList::const_iterator end) -> ScriptTokenList::const_iterator;
    };
    
    /** @} */
    /** @} */
}

#endif
