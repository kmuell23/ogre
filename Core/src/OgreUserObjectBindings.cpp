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

import :Prerequisites;
import :UserObjectBindings;

import <any>;
import <map>;
import <memory>;
import <utility>;

namespace Ogre {
    static ::std::any constinit emptyAny;

    //--------------------------------------------------------------------------
    UserObjectBindings::UserObjectBindings(const UserObjectBindings& other)
    {
        if (other.mAttributes)
            mAttributes = std::make_unique<Attributes>(*other.mAttributes);
    }

    auto UserObjectBindings::swap(UserObjectBindings& rhs) -> UserObjectBindings&
    {
        std::swap(mAttributes, rhs.mAttributes);
        return *this;
    }

    auto UserObjectBindings::operator=(const UserObjectBindings& rhs) -> UserObjectBindings&
    {
        UserObjectBindings(rhs).swap(*this);
        return *this;
    }

    //-----------------------------------------------------------------------
    void UserObjectBindings::setUserAny( ::std::any const& anything )
    {
        // Allocate attributes on demand.
        if (!mAttributes)
            mAttributes = std::make_unique<Attributes>();

        mAttributes->mKeylessAny = anything;
    }

    //-----------------------------------------------------------------------
    auto UserObjectBindings::getUserAny() const noexcept -> ::std::any const&
    {
        // Allocate attributes on demand.
        if (!mAttributes)
            return emptyAny;

        return mAttributes->mKeylessAny;
    }

    //-----------------------------------------------------------------------
    void UserObjectBindings::setUserAny(std::string_view key, ::std::any const& anything)
    {
        // Allocate attributes on demand.
        if (!mAttributes)
            mAttributes = std::make_unique<Attributes>();

        // Case map doesn't exists.
        if (!mAttributes->mUserObjectsMap)
            mAttributes->mUserObjectsMap = std::make_unique<UserObjectsMap>();

        (*mAttributes->mUserObjectsMap)[key] = anything;
    }

    //-----------------------------------------------------------------------
    auto UserObjectBindings::getUserAny(std::string_view key) const -> ::std::any const&
    {
        if (!mAttributes)
            return emptyAny;

        // Case map doesn't exists.
        if (!mAttributes->mUserObjectsMap)
            return emptyAny;

        auto it = mAttributes->mUserObjectsMap->find(key);

        // Case user data found.
        if (it != mAttributes->mUserObjectsMap->end())
        {
            return it->second;
        }

        return emptyAny;
    }

    //-----------------------------------------------------------------------
    void UserObjectBindings::eraseUserAny(std::string_view key)
    {
        // Case attributes and map allocated.
        if (mAttributes && mAttributes->mUserObjectsMap)
        {
            auto it = mAttributes->mUserObjectsMap->find(key);

            // Case object found -> erase it from the map.
            if (it != mAttributes->mUserObjectsMap->end())
            {
                mAttributes->mUserObjectsMap->erase(it);
            }
        }
    }

    //-----------------------------------------------------------------------
    void UserObjectBindings::clear()
    {
        mAttributes.reset();
    }
}
