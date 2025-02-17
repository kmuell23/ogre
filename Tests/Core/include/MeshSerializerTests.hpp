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

export module Ogre.Tests:Core.MeshSerializer;

export import Ogre.Core;

export import <memory>;
export import <unordered_map>;

using namespace Ogre;

export
class MeshSerializerTests : public ::testing::Test
{

protected:
    MeshPtr mMesh;
    MeshPtr mOrigMesh;
    String mMeshFullPath;
    String mSkeletonFullPath;
    SkeletonPtr mSkeleton;
    Real mErrorFactor;
    ::std::unique_ptr<FileSystemLayer> mFSLayer;
    ::std::unique_ptr<ResourceGroupManager> mResGroupMgr;
    ::std::unique_ptr<LodStrategyManager> mLodMgr;
    ::std::unique_ptr<HardwareBufferManager> mHardMgr;
    ::std::unique_ptr<MeshManager> mMeshMgr;
    ::std::unique_ptr<SkeletonManager> mSkelMgr;
    ::std::unique_ptr<ArchiveFactory> mArchiveFactory;
    ::std::unique_ptr<ArchiveManager> mArchiveMgr;
    ::std::unique_ptr<MaterialManager> mMatMgr;

public:
    void SetUp() override;
    void TearDown() override;
    void testMesh(MeshVersion version);
    void assertMeshClone(Mesh* a, Mesh* b, MeshVersion version = MeshVersion::LATEST);
    void assertVertexDataClone(VertexData* a, VertexData* b, MeshVersion version = MeshVersion::LATEST);
    void assertIndexDataClone(IndexData* a, IndexData* b, MeshVersion version = MeshVersion::LATEST);
    void assertEdgeDataClone(EdgeData* a, EdgeData* b, MeshVersion version = MeshVersion::LATEST);
    void assertLodUsageClone(const MeshLodUsage& a, const MeshLodUsage& b, MeshVersion version = MeshVersion::LATEST);

    template<typename T>
    auto isContainerClone(T& a, T& b) -> bool;
    template<typename K, typename V, typename H, typename E>
    auto isHashMapClone(const std::unordered_map<K, V, H, E>& a, const std::unordered_map<K, V, H, E>& b) -> bool;

    void getResourceFullPath(const ResourcePtr& resource, String& outPath);
    auto copyFile(std::string_view srcPath, std::string_view dstPath) -> bool;
    auto isLodMixed(const Mesh* pMesh) -> bool;
    auto isEqual(Real a, Real b) -> bool;
    auto isEqual(const Vector3& a, const Vector3& b) -> bool;
};
