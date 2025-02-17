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

#include <gtest/gtest.h>
#include <climits>
#include <cstdlib>

module Ogre.Tests;

import :Core.RadixSort;

import Ogre.Core;

import <list>;
import <vector>;

using namespace Ogre;

// Register the test suite
//--------------------------------------------------------------------------
void RadixSortTests::SetUp()
{    srand(0);
    
}

//--------------------------------------------------------------------------
void RadixSortTests::TearDown()
{
}

//--------------------------------------------------------------------------
class FloatSortFunctor
{
public:
    auto operator()(const float& p) const -> float
    {
        return p;
    }
};

//--------------------------------------------------------------------------
class IntSortFunctor
{
public:
    auto operator()(const int& p) const -> int
    {
        return p;
    }
};

//--------------------------------------------------------------------------
class UnsignedIntSortFunctor
{
public:
    auto operator()(const unsigned int& p) const -> unsigned int
    {
        return p;
    }
};
//--------------------------------------------------------------------------
TEST_F(RadixSortTests,FloatVector)
{
    std::vector<float> container;
    FloatSortFunctor func;
    RadixSort<std::vector<float>, float, float> sorter;

    for (int i = 0; i < 1000; ++i)
    {
        container.push_back((float)Math::RangeRandom(-1e10, 1e10));
    }

    sorter.sort(container, func);

    auto v = container.begin();
    float lastValue = *v++;
    for (;v != container.end(); ++v)
    {
        EXPECT_TRUE(*v >= lastValue);
        lastValue = *v;
    }
}
//--------------------------------------------------------------------------
TEST_F(RadixSortTests,FloatList)
{
    std::list<float> container;
    FloatSortFunctor func;
    RadixSort<std::list<float>, float, float> sorter;

    for (int i = 0; i < 1000; ++i)
    {
        container.push_back((float)Math::RangeRandom(-1e10, 1e10));
    }

    sorter.sort(container, func);

    auto v = container.begin();
    float lastValue = *v++;
    for (;v != container.end(); ++v)
    {
        EXPECT_TRUE(*v >= lastValue);
        lastValue = *v;
    }
}
//--------------------------------------------------------------------------
TEST_F(RadixSortTests,UnsignedIntList)
{
    std::list<unsigned int> container;
    UnsignedIntSortFunctor func;
    RadixSort<std::list<unsigned int>, unsigned int, unsigned int> sorter;

    for (int i = 0; i < 1000; ++i)
    {
        container.push_back((unsigned int)(UINT_MAX * double(Math::UnitRandom())));
    }

    sorter.sort(container, func);

    auto v = container.begin();
    unsigned int lastValue = *v++;
    for (;v != container.end(); ++v)
    {
        EXPECT_TRUE(*v >= lastValue);
        lastValue = *v;
    }
}
//--------------------------------------------------------------------------
TEST_F(RadixSortTests,IntList)
{
    std::list<int> container;
    IntSortFunctor func;
    RadixSort<std::list<int>, int, int> sorter;

    for (int i = 0; i < 1000; ++i)
    {
        container.push_back((int)(UINT_MAX * double(Math::UnitRandom())) + INT_MIN);
    }

    sorter.sort(container, func);

    auto v = container.begin();
    int lastValue = *v++;
    for (;v != container.end(); ++v)
    {
        EXPECT_TRUE(*v >= lastValue);
        lastValue = *v;
    }
}
//--------------------------------------------------------------------------
TEST_F(RadixSortTests,UnsignedIntVector)
{
    std::vector<unsigned int> container;
    UnsignedIntSortFunctor func;
    RadixSort<std::vector<unsigned int>, unsigned int, unsigned int> sorter;

    for (int i = 0; i < 1000; ++i)
    {
        container.push_back((unsigned int)(UINT_MAX * double(Math::UnitRandom())));
    }

    sorter.sort(container, func);

    auto v = container.begin();
    unsigned int lastValue = *v++;
    for (;v != container.end(); ++v)
    {
        EXPECT_TRUE(*v >= lastValue);
        lastValue = *v;
    }
}
//--------------------------------------------------------------------------
TEST_F(RadixSortTests,IntVector)
{
    std::vector<int> container;
    IntSortFunctor func;
    RadixSort<std::vector<int>, int, int> sorter;

    for (int i = 0; i < 1000; ++i)
    {
        container.push_back((int)(UINT_MAX * double(Math::UnitRandom())) + INT_MIN);
    }

    sorter.sort(container, func);

    auto v = container.begin();
    int lastValue = *v++;
    for (;v != container.end(); ++v)
    {
        EXPECT_TRUE(*v >= lastValue);
        lastValue = *v;
    }
}
//--------------------------------------------------------------------------
