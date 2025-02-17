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
#include <cstring>

module Ogre.Tests;

import :Core.RootWithoutRenderSystemFixture;

import Ogre.Components.Bites;
import Ogre.Core;
import Ogre.PlugIns.STBICodec;

import <list>;
import <map>;
import <memory>;
import <random>;
import <string>;
import <utility>;
import <vector>;

using std::minstd_rand;

using namespace Ogre;

using CameraTests = RootWithoutRenderSystemFixture;
TEST_F(CameraTests,customProjectionMatrix)
{
    Camera cam("", nullptr);
    std::vector<Vector3> corners(cam.getWorldSpaceCorners(), cam.getWorldSpaceCorners() + 8);
    RealRect extents = cam.getFrustumExtents();
    cam.setCustomProjectionMatrix(true, cam.getProjectionMatrix());
    for(int j = 0; j < 8; j++)
        EXPECT_EQ(corners[j], cam.getWorldSpaceCorners()[j]);

    EXPECT_EQ(extents, cam.getFrustumExtents());
}
TEST(Root,shutdown)
{
    Root root("");
    OgreBites::StaticPluginLoader mStaticPluginLoader;
    mStaticPluginLoader.load();

    root.shutdown();
}
TEST(SceneManager,removeAndDestroyAllChildren)
{
    Root root("");
    SceneManager* sm = root.createSceneManager();
    sm->getRootSceneNode()->createChildSceneNode();
    sm->getRootSceneNode()->createChildSceneNode();
    sm->getRootSceneNode()->removeAndDestroyAllChildren();
}
static void createRandomEntityClones(Entity* ent, size_t cloneCount, const Vector3& min,

                                     const Vector3& max, SceneManager* mgr)
{
    // we want cross platform consistent sequence
    minstd_rand rng;

    for (size_t n = 0; n < cloneCount; ++n)
    {
        // Create a new node under the root.
        SceneNode* node = mgr->createSceneNode();
        // Random translate.
        Vector3 nodePos = max - min;
        nodePos.x *= double(rng()) / double(rng.max());
        nodePos.y *= double(rng()) / double(rng.max());
        nodePos.z *= double(rng()) / double(rng.max());
        nodePos += min;
        node->setPosition(nodePos);
        mgr->getRootSceneNode()->addChild(node);
        Entity* cloneEnt = ent->clone(StringConverter::toString(n));
        // Attach to new node.
        node->attachObject(cloneEnt);
    }
}

struct SceneQueryTest : public RootWithoutRenderSystemFixture {
    SceneManager* mSceneMgr;
    Camera* mCamera;
    SceneNode* mCameraNode;

    void SetUp() override {
        RootWithoutRenderSystemFixture::SetUp();

        mSceneMgr = mRoot->createSceneManager();
        mCamera = mSceneMgr->createCamera("Camera");
        mCameraNode = mSceneMgr->getRootSceneNode()->createChildSceneNode();
        mCameraNode->attachObject(mCamera);
        mCameraNode->setPosition(0,0,500);
        mCameraNode->lookAt(Vector3{0, 0, 0}, Node::TransformSpace::PARENT);

        // Create a set of random balls
        Entity* ent = mSceneMgr->createEntity("501", "sphere.mesh", "General");

        // stick one at the origin so one will always be hit by ray
        mSceneMgr->getRootSceneNode()->createChildSceneNode()->attachObject(ent);
        createRandomEntityClones(ent, 500, Vector3{-2500,-2500,-2500}, Vector3{2500,2500,2500}, mSceneMgr);

        mSceneMgr->_updateSceneGraph(mCamera);
    }
};
TEST_F(SceneQueryTest,Intersection)
{
    auto intersectionQuery = mSceneMgr->createIntersectionQuery();

    int expected[][2] = {
        {0, 391},   {1, 8},     {117, 128}, {118, 171}, {118, 24},  {121, 72},  {121, 95},
        {132, 344}, {14, 227},  {14, 49},   {144, 379}, {151, 271}, {153, 28},  {164, 222},
        {169, 212}, {176, 20},  {179, 271}, {185, 238}, {190, 47},  {193, 481}, {201, 210},
        {205, 404}, {235, 366}, {239, 3},   {250, 492}, {256, 67},  {26, 333},  {260, 487},
        {263, 272}, {265, 319}, {265, 472}, {270, 45},  {284, 329}, {289, 405}, {316, 80},
        {324, 388}, {334, 337}, {336, 436}, {34, 57},   {340, 440}, {342, 41},  {348, 82},
        {35, 478},  {372, 412}, {380, 460}, {398, 92},  {417, 454}, {432, 99},  {448, 79},
        {498, 82},  {72, 77}
    };

    IntersectionSceneQueryResult& results = intersectionQuery->execute();
    EXPECT_EQ(results.movables2movables.size(), sizeof(expected)/sizeof(expected[0]));

    int i = 0;
    for (auto & thepair : results.movables2movables)
    {
        // printf("{%d, %d},", StringConverter::parseInt(thepair.first->getName()), StringConverter::parseInt(thepair.second->getName()));
        ASSERT_EQ(expected[i][0], StringConverter::parseInt(thepair.first->getName()));
        ASSERT_EQ(expected[i][1], StringConverter::parseInt(thepair.second->getName()));
        i++;
    }
    // printf("\n");
}
TEST_F(SceneQueryTest, Ray) {
    auto rayQuery = mSceneMgr->createRayQuery(mCamera->getCameraToViewportRay(0.5, 0.5));
    rayQuery->setSortByDistance(true, 2);

    RaySceneQueryResult& results = rayQuery->execute();

    ASSERT_EQ("501", results[0].movable->getName());
    ASSERT_EQ("397", results[1].movable->getName());
}
TEST(MaterialSerializer, Basic)
{
    Root root;
    DefaultTextureManager texMgr;

    String group = "General";

    auto mat = std::make_shared<Material>(nullptr, "Material Name", 0, group);
    auto pass = mat->createTechnique()->createPass();
    auto tus = pass->createTextureUnitState();
    tus->setContentType(TextureUnitState::ContentType::SHADOW);
    tus->setName("Test TUS");
    pass->setAmbient(ColourValue::Green);

    pass->createTextureUnitState("TextureName");

    // export to string
    MaterialSerializer ser;
    ser.queueForExport(mat);
    std::string str{ser.getQueuedAsString()};

    // printf("%s\n", str.c_str());

    // load again
    DataStreamPtr stream = std::make_shared<MemoryDataStream>("memory.material", &str[0], str.size());
    MaterialManager::getSingleton().parseScript(stream, group);

    auto mat2 = MaterialManager::getSingleton().getByName("Material Name", group);
    ASSERT_TRUE(mat2);
    EXPECT_EQ(mat2->getTechniques().size(), mat->getTechniques().size());
    EXPECT_EQ(mat2->getTechniques()[0]->getPasses()[0]->getAmbient(), ColourValue::Green);
    EXPECT_EQ(mat2->getTechniques()[0]->getPasses()[0]->getTextureUnitState(0)->getName(),
              "Test TUS");
    EXPECT_EQ(mat2->getTechniques()[0]->getPasses()[0]->getTextureUnitState("Test TUS")->getContentType(),
              TextureUnitState::ContentType::SHADOW);
    EXPECT_EQ(mat2->getTechniques()[0]->getPasses()[0]->getTextureUnitState(1)->getTextureName(),
              "TextureName");
}
TEST(Image, FlipV)
{
    ResourceGroupManager mgr;
    STBIImageCodec::startup();
    ConfigFile cf;
    cf.load(FileSystemLayer(/*OGRE_VERSION_NAME*/"Tsathoggua").getConfigFilePath("resources.cfg"));
    auto testPath = cf.getSettings("Tests").begin()->second;

    Image ref;
    ref.load(Root::openFileStream(::std::format("{}/decal1vflip.png", testPath)), "png");

    Image img;
    img.load(Root::openFileStream(::std::format("{}/decal1.png", testPath)), "png");
    img.flipAroundX();

    // img.save(::std::format("{}/decal1vflip.png", testPath));

    STBIImageCodec::shutdown();
    ASSERT_TRUE(!memcmp(img.getData(), ref.getData(), ref.getSize()));
}
TEST(Image, Resize)
{
    ResourceGroupManager mgr;
    STBIImageCodec::startup();
    ConfigFile cf;
    cf.load(FileSystemLayer(/*OGRE_VERSION_NAME*/"Tsathoggua").getConfigFilePath("resources.cfg"));
    auto testPath = cf.getSettings("Tests").begin()->second;

    Image ref;
    ref.load(Root::openFileStream(::std::format("{}/decal1small.png", testPath)), "png");

    Image img;
    img.load(Root::openFileStream(::std::format("{}/decal1.png", testPath)), "png");
    img.resize(128, 128);

    //img.save(::std::format("{}/decal1small.png", testPath));

    STBIImageCodec::shutdown();
    ASSERT_TRUE(!memcmp(img.getData(), ref.getData(), ref.getSize()));
}
TEST(Image, Combine)
{
    ResourceGroupManager mgr;
    FileSystemArchiveFactory fs;
    ArchiveManager amgr;
    amgr.addArchiveFactory(&fs);
    STBIImageCodec::startup();
    ConfigFile cf;
    cf.load(FileSystemLayer(/*OGRE_VERSION_NAME*/"Tsathoggua").getConfigFilePath("resources.cfg"));
    mgr.addResourceLocation(::std::format("{}/../materials/textures", cf.getSettings("General").begin()->second), fs.getType());
    mgr.initialiseAllResourceGroups();

    auto testPath = cf.getSettings("Tests").begin()->second;
    Image ref;
    ref.load(Root::openFileStream(::std::format("{}/rockwall_flare.png", testPath)), "png");

    Image combined;
    // pick 2 files that are the same size, alpha texture will be made greyscale
    combined.loadTwoImagesAsRGBA("rockwall.tga", "flare.png", RGN_DEFAULT, PixelFormat::BYTE_RGBA);

    // combined.save(::std::format("{}/rockwall_flare.png", testPath));
    STBIImageCodec::shutdown();
    ASSERT_TRUE(!memcmp(combined.getData(), ref.getData(), ref.getSize()));
}

struct UsePreviousResourceLoadingListener : public ResourceLoadingListener
{
    auto resourceCollision(Resource *resource, ResourceManager *resourceManager) noexcept -> bool override { return false; }
};

using ResourceLoading = RootWithoutRenderSystemFixture;
TEST_F(ResourceLoading, CollsionUseExisting)
{
    UsePreviousResourceLoadingListener listener;
    ResourceGroupManager::getSingleton().setLoadingListener(&listener);

    MaterialPtr mat = MaterialManager::getSingleton().create("Collision", "Tests");
    EXPECT_TRUE(mat);
    EXPECT_FALSE(MaterialManager::getSingleton().create("Collision", "Tests"));
    EXPECT_FALSE(mat->clone("Collision"));

    MeshPtr mesh = MeshManager::getSingleton().create("Collision", "Tests");
    EXPECT_TRUE(mesh);
    EXPECT_FALSE(MeshManager::getSingleton().create("Collision", "Tests"));
    EXPECT_FALSE(mesh->clone("Collision"));

    EXPECT_TRUE(SkeletonManager::getSingleton().create("Collision", "Tests"));
    EXPECT_FALSE(SkeletonManager::getSingleton().create("Collision", "Tests"));

    EXPECT_TRUE(CompositorManager::getSingleton().create("Collision", "Tests"));
    EXPECT_FALSE(CompositorManager::getSingleton().create("Collision", "Tests"));

    EXPECT_TRUE(HighLevelGpuProgramManager::getSingleton().createProgram(
        "Collision", "Tests", "null", GpuProgramType::VERTEX_PROGRAM));
    EXPECT_FALSE(HighLevelGpuProgramManager::getSingleton().createProgram(
        "Collision", "Tests", "null", GpuProgramType::VERTEX_PROGRAM));
}

struct DeletePreviousResourceLoadingListener : public ResourceLoadingListener
{
    auto resourceCollision(Resource* resource, ResourceManager* resourceManager) noexcept -> bool override
    {
        resourceManager->remove(resource->getName(), resource->getGroup());
        return true;
    }
};
TEST_F(ResourceLoading, CollsionDeleteExisting)
{
    DeletePreviousResourceLoadingListener listener;
    ResourceGroupManager::getSingleton().setLoadingListener(&listener);
    ResourceGroupManager::getSingleton().createResourceGroup("EmptyGroup", false);

    MaterialPtr mat = MaterialManager::getSingleton().create("Collision", "EmptyGroup");
    EXPECT_TRUE(mat);
    EXPECT_TRUE(MaterialManager::getSingleton().create("Collision", "EmptyGroup"));
    EXPECT_TRUE(mat->clone("Collision"));
}

using TextureTests = RootWithoutRenderSystemFixture;
TEST_F(TextureTests, Blank)
{
    auto mat = std::make_shared<Material>(nullptr, "Material Name", 0, "Group");
    auto tus = mat->createTechnique()->createPass()->createTextureUnitState();

    EXPECT_EQ(tus->isBlank(), true);
    EXPECT_EQ(tus->getTextureName(), "");
    EXPECT_EQ(tus->getTextureType(), TextureType::_2D);
    EXPECT_EQ(tus->getNumMipmaps(), TextureMipmap::DEFAULT);
    EXPECT_EQ(tus->getDesiredFormat(), PixelFormat::UNKNOWN);
    EXPECT_EQ(tus->getFrameTextureName(0), "");
    EXPECT_EQ(tus->getGamma(), 1.0f);
    EXPECT_EQ(tus->isHardwareGammaEnabled(), false);
}
TEST(GpuSharedParameters, align)
{
    Root root("");
    GpuSharedParameters params("dummy");

    // trivial case
    params.addConstantDefinition("a", GpuConstantType::FLOAT1);
    EXPECT_EQ(params.getConstantDefinition("a").logicalIndex, 0uz);

    // 16 byte alignment
    params.addConstantDefinition("b", GpuConstantType::FLOAT4);
    EXPECT_EQ(params.getConstantDefinition("b").logicalIndex, 16uz);

    // break alignment again
    params.addConstantDefinition("c", GpuConstantType::FLOAT1);
    EXPECT_EQ(params.getConstantDefinition("c").logicalIndex, 32uz);

    // 16 byte alignment
    params.addConstantDefinition("d", GpuConstantType::MATRIX_4X4);
    EXPECT_EQ(params.getConstantDefinition("d").logicalIndex, 48uz);
}

using HighLevelGpuProgramTest = RootWithoutRenderSystemFixture;
TEST_F(HighLevelGpuProgramTest, resolveIncludes)
{
    auto mat = MaterialManager::getSingleton().create("Dummy", RGN_DEFAULT);

    auto& rgm = ResourceGroupManager::getSingleton();
    rgm.addResourceLocation(".", "FileSystem", RGN_DEFAULT, false, false);

    // recursive inclusion
    String bar = "World";
    rgm.createResource("bar.cg", RGN_DEFAULT)->write(bar.c_str(), bar.size());
    String foo = "Hello\n#include <bar.cg>\n";
    rgm.createResource("foo.cg", RGN_DEFAULT)->write(foo.c_str(), foo.size());
    const char* src = "#include <foo.cg>";

    String res = HighLevelGpuProgram::_resolveIncludes(src, mat.get(), "main.cg", true);
    rgm.deleteResource("foo.cg", RGN_DEFAULT);
    rgm.deleteResource("bar.cg", RGN_DEFAULT);

    String ref = "#line 1  \"foo.cg\"\n"
                 "Hello\n"
                 "#line 1  \"bar.cg\"\n"
                 "World\n"
                 "#line 3 \"foo.cg\"";

    ASSERT_EQ(res.substr(0, ref.size()), ref);
}
TEST(Math, TriangleRayIntersection)
{
    Vector3 tri[3] = {{-1, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    auto ray = Ray{{0, 0.5, 1}, {0, 0, -1}};

    EXPECT_TRUE(Math::intersects(ray, tri[0], tri[1], tri[2], true, true).first);
    EXPECT_TRUE(Math::intersects(ray, tri[0], tri[1], tri[2], true, false).first);
    EXPECT_FALSE(Math::intersects(ray, tri[0], tri[1], tri[2], false, true).first);
    EXPECT_FALSE(Math::intersects(ray, tri[0], tri[1], tri[2], false, false).first);

    ray = Ray{{0, 0.5, -1}, {0, 0, 1}};

    EXPECT_TRUE(Math::intersects(ray, tri[0], tri[1], tri[2], true, true).first);
    EXPECT_FALSE(Math::intersects(ray, tri[0], tri[1], tri[2], true, false).first);
    EXPECT_TRUE(Math::intersects(ray, tri[0], tri[1], tri[2], false, true).first);
    EXPECT_FALSE(Math::intersects(ray, tri[0], tri[1], tri[2], false, false).first);
}

using SkeletonTests = RootWithoutRenderSystemFixture;
TEST_F(SkeletonTests, linkedSkeletonAnimationSource)
{
    auto sceneMgr = mRoot->createSceneManager();
    auto entity = sceneMgr->createEntity("jaiqua.mesh");
    entity->getSkeleton()->addLinkedSkeletonAnimationSource("ninja.skeleton");
    entity->refreshAvailableAnimationState();
    EXPECT_TRUE(entity->getAnimationState("Stealth")); // animation from ninja.sekeleton
}
