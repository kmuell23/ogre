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

import :Codec;
import :Exception;
import :SharedPtr;
import :String;
import :StringConverter;

import <any>;
import <format>;
import <utility>;

namespace Ogre {

    std::map<std::string_view, Codec * > Codec::msMapCodecs;

    Codec::~Codec() = default;

    auto Codec::encode(::std::any const& input) const -> DataStreamPtr
    {
        OGRE_EXCEPT(ExceptionCodes::NOT_IMPLEMENTED, ::std::format("{} - encoding to memory not supported", getType()));
        return {};
    }

    void Codec::encodeToFile(::std::any const& input, std::string_view outFileName) const
    {
        OGRE_EXCEPT(ExceptionCodes::NOT_IMPLEMENTED, ::std::format("{} - encoding to file not supported", getType()));
    }

    auto Codec::getExtensions() -> std::vector<std::string_view>
    {
        std::vector<std::string_view> result;
        result.reserve(msMapCodecs.size());
        for (auto & msMapCodec : msMapCodecs)
        {
            result.push_back(msMapCodec.first);
        }
        return result;
    }

    void Codec::registerCodec(Codec* pCodec)
    {
        auto ret = msMapCodecs.emplace(pCodec->getType(), pCodec);
        if (!ret.second)
            OGRE_EXCEPT(ExceptionCodes::DUPLICATE_ITEM, ::std::format("{} already has a registered codec", pCodec->getType()));
    }

    auto Codec::getCodec(std::string_view extension) -> Codec*
    {
        String lwrcase{extension};
        StringUtil::toLowerCase(lwrcase);
        auto i = msMapCodecs.find(lwrcase);
        if (i == msMapCodecs.end())
        {
            String formats_str;
            if(msMapCodecs.empty())
                formats_str = "There are no formats supported (no codecs registered).";
            else
                formats_str = ::std::format("Supported formats are: {}.", getExtensions() );

            OGRE_EXCEPT(ExceptionCodes::ITEM_NOT_FOUND,
                        ::std::format("Can not find codec for '{}' format.\n", extension ) + formats_str);
        }

        return i->second;

    }

    auto Codec::getCodec(char *magicNumberPtr, size_t maxbytes) -> Codec*
    {
        for (auto & [key, value] : msMapCodecs)
        {
            auto ext = value->magicNumberToFileExt(magicNumberPtr, maxbytes);
            if (!ext.empty())
            {
                // check codec type matches
                // if we have a single codec class that can handle many types, 
                // and register many instances of it against different types, we
                // can end up matching the wrong one here, so grab the right one
                if (ext == value->getType())
                    return value;
                else
                    return getCodec(ext);
            }
        }

        return nullptr;

    }

}
