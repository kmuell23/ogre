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

module Ogre.Tests;

import :Core.EdgeBuilder;

import Ogre.Core;

import <memory>;

// Register the test suite
//--------------------------------------------------------------------------
void EdgeBuilderTests::SetUp()
{   
   mBufMgr = new DefaultHardwareBufferManager();
}

//--------------------------------------------------------------------------
void EdgeBuilderTests::TearDown()
{
    delete mBufMgr;
}
//--------------------------------------------------------------------------
TEST_F(EdgeBuilderTests,SingleIndexBufSingleVertexBuf)
{
    /* This tests the edge builders ability to find shared edges in the simple case
    of a single index buffer referencing a single vertex buffer
    */
    VertexData vd;
    IndexData id;

    // Test pyramid
    vd.vertexCount = 4;
    vd.vertexStart = 0;
    vd.vertexDeclaration = HardwareBufferManager::getSingleton().createVertexDeclaration();
    vd.vertexDeclaration->addElement(0, 0, VertexElementType::FLOAT3, VertexElementSemantic::POSITION);
    HardwareVertexBufferSharedPtr vbuf = HardwareBufferManager::getSingleton().createVertexBuffer(sizeof(float)*3, 4, HardwareBuffer::STATIC,true);
    vd.vertexBufferBinding->setBinding(0, vbuf);
    auto* pFloat = static_cast<float*>(vbuf->lock(HardwareBuffer::LockOptions::DISCARD));
    *pFloat++ = 0  ; *pFloat++ = 0  ; *pFloat++ = 0  ;
    *pFloat++ = 50 ; *pFloat++ = 0  ; *pFloat++ = 0  ;
    *pFloat++ = 0  ; *pFloat++ = 100; *pFloat++ = 0  ;
    *pFloat++ = 0  ; *pFloat++ = 0  ; *pFloat++ = -50;
    vbuf->unlock();

    id.indexBuffer = HardwareBufferManager::getSingleton().createIndexBuffer(
        HardwareIndexBuffer::IndexType::_16BIT, 12, HardwareBuffer::STATIC, true);
    id.indexCount = 12;
    id.indexStart = 0;
    auto* pIdx = static_cast<unsigned short*>(id.indexBuffer->lock(HardwareBuffer::LockOptions::DISCARD));
    *pIdx++ = 0; *pIdx++ = 1; *pIdx++ = 2;
    *pIdx++ = 0; *pIdx++ = 2; *pIdx++ = 3;
    *pIdx++ = 1; *pIdx++ = 3; *pIdx++ = 2;
    *pIdx++ = 0; *pIdx++ = 3; *pIdx++ = 1;
    id.indexBuffer->unlock();

    EdgeListBuilder edgeBuilder;
    edgeBuilder.addVertexData(&vd);
    edgeBuilder.addIndexData(&id);
    EdgeData* edgeData = edgeBuilder.build();

    // Should be only one group, since only one vertex buffer
    EXPECT_TRUE(edgeData->edgeGroups.size() == 1);
    // 4 triangles
    EXPECT_TRUE(edgeData->triangles.size() == 4);
    EdgeData::EdgeGroup& eg = edgeData->edgeGroups[0];
    // 6 edges
    EXPECT_TRUE(eg.edges.size() == 6);

    delete edgeData;
}
//--------------------------------------------------------------------------
TEST_F(EdgeBuilderTests,MultiIndexBufSingleVertexBuf)
{
    /* This tests the edge builders ability to find shared edges when there are
    multiple index sets (submeshes) using a single vertex buffer.
    */
    VertexData vd;
    IndexData id[4];

    // Test pyramid
    vd.vertexCount = 4;
    vd.vertexStart = 0;
    vd.vertexDeclaration = HardwareBufferManager::getSingleton().createVertexDeclaration();
    vd.vertexDeclaration->addElement(0, 0, VertexElementType::FLOAT3, VertexElementSemantic::POSITION);
    HardwareVertexBufferSharedPtr vbuf = HardwareBufferManager::getSingleton().createVertexBuffer(sizeof(float)*3, 4, HardwareBuffer::STATIC,true);
    vd.vertexBufferBinding->setBinding(0, vbuf);
    auto* pFloat = static_cast<float*>(vbuf->lock(HardwareBuffer::LockOptions::DISCARD));
    *pFloat++ = 0  ; *pFloat++ = 0  ; *pFloat++ = 0  ;
    *pFloat++ = 50 ; *pFloat++ = 0  ; *pFloat++ = 0  ;
    *pFloat++ = 0  ; *pFloat++ = 100; *pFloat++ = 0  ;
    *pFloat++ = 0  ; *pFloat++ = 0  ; *pFloat++ = -50;
    vbuf->unlock();

    id[0].indexBuffer = HardwareBufferManager::getSingleton().createIndexBuffer(
        HardwareIndexBuffer::IndexType::_16BIT, 3, HardwareBuffer::STATIC, true);
    id[0].indexCount = 3;
    id[0].indexStart = 0;
    auto* pIdx = static_cast<unsigned short*>(id[0].indexBuffer->lock(HardwareBuffer::LockOptions::DISCARD));
    *pIdx++ = 0; *pIdx++ = 1; *pIdx++ = 2;
    id[0].indexBuffer->unlock();

    id[1].indexBuffer = HardwareBufferManager::getSingleton().createIndexBuffer(
        HardwareIndexBuffer::IndexType::_16BIT, 3, HardwareBuffer::STATIC, true);
    id[1].indexCount = 3;
    id[1].indexStart = 0;
    pIdx = static_cast<unsigned short*>(id[1].indexBuffer->lock(HardwareBuffer::LockOptions::DISCARD));
    *pIdx++ = 0; *pIdx++ = 2; *pIdx++ = 3;
    id[1].indexBuffer->unlock();

    id[2].indexBuffer = HardwareBufferManager::getSingleton().createIndexBuffer(
        HardwareIndexBuffer::IndexType::_16BIT, 3, HardwareBuffer::STATIC, true);
    id[2].indexCount = 3;
    id[2].indexStart = 0;
    pIdx = static_cast<unsigned short*>(id[2].indexBuffer->lock(HardwareBuffer::LockOptions::DISCARD));
    *pIdx++ = 1; *pIdx++ = 3; *pIdx++ = 2;
    id[2].indexBuffer->unlock();

    id[3].indexBuffer = HardwareBufferManager::getSingleton().createIndexBuffer(
        HardwareIndexBuffer::IndexType::_16BIT, 3, HardwareBuffer::STATIC, true);
    id[3].indexCount = 3;
    id[3].indexStart = 0;
    pIdx = static_cast<unsigned short*>(id[3].indexBuffer->lock(HardwareBuffer::LockOptions::DISCARD));
    *pIdx++ = 0; *pIdx++ = 3; *pIdx++ = 1;
    id[3].indexBuffer->unlock();

    EdgeListBuilder edgeBuilder;
    edgeBuilder.addVertexData(&vd);
    edgeBuilder.addIndexData(&id[0]);
    edgeBuilder.addIndexData(&id[1]);
    edgeBuilder.addIndexData(&id[2]);
    edgeBuilder.addIndexData(&id[3]);
    EdgeData* edgeData = edgeBuilder.build();

    // Should be only one group, since only one vertex buffer
    EXPECT_TRUE(edgeData->edgeGroups.size() == 1);
    // 4 triangles
    EXPECT_TRUE(edgeData->triangles.size() == 4);
    EdgeData::EdgeGroup& eg = edgeData->edgeGroups[0];
    // 6 edges
    EXPECT_TRUE(eg.edges.size() == 6);

    delete edgeData;
}
//--------------------------------------------------------------------------
TEST_F(EdgeBuilderTests,MultiIndexBufMultiVertexBuf)
{
    /* This tests the edge builders ability to find shared edges when there are
    both multiple index sets (submeshes) each using a different vertex buffer
    (not using shared geometry).
    */

    VertexData vd[4];
    IndexData id[4];

    // Test pyramid
    vd[0].vertexCount = 3;
    vd[0].vertexStart = 0;
    vd[0].vertexDeclaration = HardwareBufferManager::getSingleton().createVertexDeclaration();
    vd[0].vertexDeclaration->addElement(0, 0, VertexElementType::FLOAT3, VertexElementSemantic::POSITION);
    HardwareVertexBufferSharedPtr vbuf = HardwareBufferManager::getSingleton().createVertexBuffer(sizeof(float)*3, 3, HardwareBuffer::STATIC,true);
    vd[0].vertexBufferBinding->setBinding(0, vbuf);
    auto* pFloat = static_cast<float*>(vbuf->lock(HardwareBuffer::LockOptions::DISCARD));
    *pFloat++ = 0  ; *pFloat++ = 0  ; *pFloat++ = 0  ;
    *pFloat++ = 50 ; *pFloat++ = 0  ; *pFloat++ = 0  ;
    *pFloat++ = 0  ; *pFloat++ = 100; *pFloat++ = 0  ;
    vbuf->unlock();

    vd[1].vertexCount = 3;
    vd[1].vertexStart = 0;
    vd[1].vertexDeclaration = HardwareBufferManager::getSingleton().createVertexDeclaration();
    vd[1].vertexDeclaration->addElement(0, 0, VertexElementType::FLOAT3, VertexElementSemantic::POSITION);
    vbuf = HardwareBufferManager::getSingleton().createVertexBuffer(sizeof(float)*3, 3, HardwareBuffer::STATIC,true);
    vd[1].vertexBufferBinding->setBinding(0, vbuf);
    pFloat = static_cast<float*>(vbuf->lock(HardwareBuffer::LockOptions::DISCARD));
    *pFloat++ = 0  ; *pFloat++ = 0  ; *pFloat++ = 0  ;
    *pFloat++ = 0  ; *pFloat++ = 100; *pFloat++ = 0  ;
    *pFloat++ = 0  ; *pFloat++ = 0  ; *pFloat++ = -50;
    vbuf->unlock();

    vd[2].vertexCount = 3;
    vd[2].vertexStart = 0;
    vd[2].vertexDeclaration = HardwareBufferManager::getSingleton().createVertexDeclaration();
    vd[2].vertexDeclaration->addElement(0, 0, VertexElementType::FLOAT3, VertexElementSemantic::POSITION);
    vbuf = HardwareBufferManager::getSingleton().createVertexBuffer(sizeof(float)*3, 3, HardwareBuffer::STATIC,true);
    vd[2].vertexBufferBinding->setBinding(0, vbuf);
    pFloat = static_cast<float*>(vbuf->lock(HardwareBuffer::LockOptions::DISCARD));
    *pFloat++ = 50 ; *pFloat++ = 0  ; *pFloat++ = 0  ;
    *pFloat++ = 0  ; *pFloat++ = 100; *pFloat++ = 0  ;
    *pFloat++ = 0  ; *pFloat++ = 0  ; *pFloat++ = -50;
    vbuf->unlock();

    vd[3].vertexCount = 3;
    vd[3].vertexStart = 0;
    vd[3].vertexDeclaration = HardwareBufferManager::getSingleton().createVertexDeclaration();
    vd[3].vertexDeclaration->addElement(0, 0, VertexElementType::FLOAT3, VertexElementSemantic::POSITION);
    vbuf = HardwareBufferManager::getSingleton().createVertexBuffer(sizeof(float)*3, 3, HardwareBuffer::STATIC,true);
    vd[3].vertexBufferBinding->setBinding(0, vbuf);
    pFloat = static_cast<float*>(vbuf->lock(HardwareBuffer::LockOptions::DISCARD));
    *pFloat++ = 0  ; *pFloat++ = 0  ; *pFloat++ = 0  ;
    *pFloat++ = 50 ; *pFloat++ = 0  ; *pFloat++ = 0  ;
    *pFloat++ = 0  ; *pFloat++ = 0  ; *pFloat++ = -50;
    vbuf->unlock();

    id[0].indexBuffer = HardwareBufferManager::getSingleton().createIndexBuffer(
        HardwareIndexBuffer::IndexType::_16BIT, 3, HardwareBuffer::STATIC, true);
    id[0].indexCount = 3;
    id[0].indexStart = 0;
    auto* pIdx = static_cast<unsigned short*>(id[0].indexBuffer->lock(HardwareBuffer::LockOptions::DISCARD));
    *pIdx++ = 0; *pIdx++ = 1; *pIdx++ = 2;
    id[0].indexBuffer->unlock();

    id[1].indexBuffer = HardwareBufferManager::getSingleton().createIndexBuffer(
        HardwareIndexBuffer::IndexType::_16BIT, 3, HardwareBuffer::STATIC, true);
    id[1].indexCount = 3;
    id[1].indexStart = 0;
    pIdx = static_cast<unsigned short*>(id[1].indexBuffer->lock(HardwareBuffer::LockOptions::DISCARD));
    *pIdx++ = 0; *pIdx++ = 1; *pIdx++ = 2;
    id[1].indexBuffer->unlock();

    id[2].indexBuffer = HardwareBufferManager::getSingleton().createIndexBuffer(
        HardwareIndexBuffer::IndexType::_16BIT, 3, HardwareBuffer::STATIC, true);
    id[2].indexCount = 3;
    id[2].indexStart = 0;
    pIdx = static_cast<unsigned short*>(id[2].indexBuffer->lock(HardwareBuffer::LockOptions::DISCARD));
    *pIdx++ = 0; *pIdx++ = 2; *pIdx++ = 1;
    id[2].indexBuffer->unlock();

    id[3].indexBuffer = HardwareBufferManager::getSingleton().createIndexBuffer(
        HardwareIndexBuffer::IndexType::_16BIT, 3, HardwareBuffer::STATIC, true);
    id[3].indexCount = 3;
    id[3].indexStart = 0;
    pIdx = static_cast<unsigned short*>(id[3].indexBuffer->lock(HardwareBuffer::LockOptions::DISCARD));
    *pIdx++ = 0; *pIdx++ = 2; *pIdx++ = 1;
    id[3].indexBuffer->unlock();

    EdgeListBuilder edgeBuilder;
    edgeBuilder.addVertexData(&vd[0]);
    edgeBuilder.addVertexData(&vd[1]);
    edgeBuilder.addVertexData(&vd[2]);
    edgeBuilder.addVertexData(&vd[3]);
    edgeBuilder.addIndexData(&id[0], 0);
    edgeBuilder.addIndexData(&id[1], 1);
    edgeBuilder.addIndexData(&id[2], 2);
    edgeBuilder.addIndexData(&id[3], 3);
    EdgeData* edgeData = edgeBuilder.build();

    // Should be 4 groups
    EXPECT_TRUE(edgeData->edgeGroups.size() == 4);
    // 4 triangles
    EXPECT_TRUE(edgeData->triangles.size() == 4);
    // 6 edges in total
    EXPECT_TRUE(
        (edgeData->edgeGroups[0].edges.size() +
        edgeData->edgeGroups[1].edges.size() +
        edgeData->edgeGroups[2].edges.size() +
        edgeData->edgeGroups[3].edges.size())
                    == 6);

    delete edgeData;
}
//--------------------------------------------------------------------------
