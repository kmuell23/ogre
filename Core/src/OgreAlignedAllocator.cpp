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

import :AlignedAllocator;
import :Bitwise;
import :Platform;

import <new>;

/**
*
* |___2___|3|_________5__________|__6__|
* ^         ^
* 1         4
*
* 1 -> Pointer to start of the block allocated by new.
* 2 -> Gap used to get 4 aligned on given alignment
* 3 -> Byte offset between 1 and 4
* 4 -> Pointer to the start of data block.
* 5 -> Data block.
* 6 -> Wasted memory at rear of data block.
*/
namespace Ogre {

    /** Allocate memory with given alignment.
        @param
            size The size of memory need to allocate.
        @param
            alignment The alignment of result pointer, must be power of two
            and in range [1, 128].
        @return
            The allocated memory pointer.
        @par
            On failure, exception will be throw.
    */
    auto AlignedMemory::allocate(size_t size, size_t alignment) -> void*
    {
        assert(0 < alignment && alignment <= 128 && Bitwise::isPO2(alignment));

        return new (::std::align_val_t{alignment}) unsigned char[size];
    }
    //---------------------------------------------------------------------
    auto AlignedMemory::allocate(size_t size) -> void*
    {
        return allocate(size, SIMD_ALIGNMENT);
    }
    //---------------------------------------------------------------------
    void AlignedMemory::deallocate(void* p, size_t alignment)
    {
        if (p)
        {
            ::operator delete[](static_cast<unsigned char*>(p), ::std::align_val_t{alignment}) ;
        }
    }

}
