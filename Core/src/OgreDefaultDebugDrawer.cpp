// This file is part of the OGRE project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at https://www.ogre3d.org/licensing.
module;

#include <cstddef>

module Ogre.Core;

import :BlendMode;
import :Common;
import :DefaultDebugDrawer;
import :Frustum;
import :HardwareBuffer;
import :Material;
import :MaterialManager;
import :Matrix3;
import :Matrix4;
import :Node;
import :Pass;
import :Platform;
import :Prerequisites;
import :RenderOperation;
import :ResourceGroupManager;
import :SceneNode;
import :SharedPtr;
import :Technique;
import :Vector;

import <algorithm>;
import <array>;
import <vector>;

namespace Ogre
{
DefaultDebugDrawer::DefaultDebugDrawer() : mLines(""), mAxes("") {}

void DefaultDebugDrawer::preFindVisibleObjects(SceneManager* source,
                                               SceneManager::IlluminationRenderStage irs, Viewport* v)
{
    mDrawType = static_cast<DrawType>(0);

    if (source->getDisplaySceneNodes())
        mDrawType |= DrawType::AXES;
    if (source->getShowBoundingBoxes())
        mDrawType |= DrawType::WIREBOX;
}
void DefaultDebugDrawer::beginLines()
{
    if (mLines.getSections().empty())
    {
        const char* matName = "Ogre/Debug/LinesMat";
        auto mat = MaterialManager::getSingleton().getByName(matName, RGN_INTERNAL);
        if (!mat)
        {
            mat = MaterialManager::getSingleton().create(matName, RGN_INTERNAL);
            Pass* p = mat->getTechnique(0)->getPass(0);
            p->setLightingEnabled(false);
            p->setVertexColourTracking(TrackVertexColourEnum::AMBIENT);
        }
        mLines.setBufferUsage(HardwareBufferUsage::CPU_TO_GPU);
        mLines.begin(mat, RenderOperation::OperationType::LINE_LIST);
    }
    else if (mLines.getCurrentVertexCount() == 0)
        mLines.beginUpdate(0);
}
void DefaultDebugDrawer::drawWireBox(const AxisAlignedBox& aabb, const ColourValue& colour)
{
    beginLines();

    int base = mLines.getCurrentVertexCount();
    for (const auto& corner : aabb.getAllCorners())
    {
        mLines.position(corner);
        mLines.colour(colour);
    }
    // see AxisAlignedBox::getAllCorners
    int idx[] = {0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6, 6, 7, 7, 4, 0, 6, 1, 5, 2, 4, 3, 7};
    for (int i : idx)
        mLines.index(base + i);
}
void DefaultDebugDrawer::drawFrustum(const Frustum* frust)
{
    beginLines();

    int base = mLines.getCurrentVertexCount();
    for (const auto& corner : frust->getWorldSpaceCorners())
    {
        mLines.position(corner);
        mLines.colour(frust->getDebugColour());
    }
    // see ConvexBody::define
    int idx[] = {0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6, 6, 7, 7, 4, 2, 6, 1, 5, 0, 4, 3, 7};
    for (int i : idx)
        mLines.index(base + i);
}
void DefaultDebugDrawer::drawAxes(const Affine3& pose, float size)
{
    if (mAxes.getSections().empty())
    {
        const char* matName = "Ogre/Debug/AxesMat";
        auto mat = MaterialManager::getSingleton().getByName(matName, RGN_INTERNAL);
        if (!mat)
        {
            mat = MaterialManager::getSingleton().create(matName, RGN_INTERNAL);
            Pass* p = mat->getTechnique(0)->getPass(0);
            p->setLightingEnabled(false);
            p->setPolygonModeOverrideable(false);
            p->setVertexColourTracking(TrackVertexColourEnum::AMBIENT);
            p->setSceneBlending(SceneBlendType::TRANSPARENT_ALPHA);
            p->setCullingMode(CullingMode::NONE);
            p->setDepthWriteEnabled(false);
            p->setDepthCheckEnabled(false);
        }

        mAxes.setBufferUsage(HardwareBufferUsage::CPU_TO_GPU);
        mAxes.begin(mat);
    }
    else if (mAxes.getCurrentVertexCount() == 0)
        mAxes.beginUpdate(0);

    /* 3 axes, each made up of 2 of these (base plane = XY)
     *   .------------|\
     *   '------------|/
     */
    Vector3 basepos[7] =
    {
        // stalk
        Vector3{0, 0.05, 0},
        Vector3{0, -0.05, 0},
        Vector3{0.7, -0.05, 0},
        Vector3{0.7, 0.05, 0},
        // head
        Vector3{0.7, -0.15, 0},
        Vector3{1, 0, 0},
        Vector3{0.7, 0.15, 0}
    };

    ColourValue col[3] = {ColourValue{1, 0, 0, 0.8}, ColourValue{0, 1, 0, 0.8}, ColourValue{0, 0, 1, 0.8}};

    Matrix3 rot[6];

    // x-axis
    rot[0] = Matrix3::IDENTITY;
    rot[1].FromAxes(Vector3::UNIT_X, Vector3::NEGATIVE_UNIT_Z, Vector3::UNIT_Y);
    // y-axis
    rot[2].FromAxes(Vector3::UNIT_Y, Vector3::NEGATIVE_UNIT_X, Vector3::UNIT_Z);
    rot[3].FromAxes(Vector3::UNIT_Y, Vector3::UNIT_Z, Vector3::UNIT_X);
    // z-axis
    rot[4].FromAxes(Vector3::UNIT_Z, Vector3::UNIT_Y, Vector3::NEGATIVE_UNIT_X);
    rot[5].FromAxes(Vector3::UNIT_Z, Vector3::UNIT_X, Vector3::UNIT_Y);

    // 6 arrows
    for (size_t i = 0; i < 6; ++i)
    {
        uint32 base = mAxes.getCurrentVertexCount();
        // vertices
        for (const auto& p : basepos)
        {
            mAxes.position(pose * (rot[i] * p * size));
            mAxes.colour(col[i / 2]);
        }

        // indices
        mAxes.quad(base + 0, base + 1, base + 2, base + 3);
        mAxes.triangle(base + 4, base + 5, base + 6);
    }
}
void DefaultDebugDrawer::drawBone(const Node* node)
{
    drawAxes(node->_getFullTransform());
}
void DefaultDebugDrawer::drawSceneNode(const SceneNode* node)
{
    const auto& aabb = node->_getWorldAABB();
    //Skip all bounding boxes that are infinite.
    if (aabb.isInfinite()) {
        return;
    }
    if (!!(mDrawType & DrawType::AXES))
    {
        auto const hs = aabb.getHalfSize();
        float sz = std::min(hs[0], hs[1]);
        sz = std::min(sz, hs[2]);
        sz = std::max(sz, 1.0f);
        drawAxes(node->_getFullTransform(), sz);
    }

    if (node->getShowBoundingBox() || !!(mDrawType & DrawType::WIREBOX))
    {
        drawWireBox(aabb);
    }
}
void DefaultDebugDrawer::postFindVisibleObjects(SceneManager* source,
                                                SceneManager::IlluminationRenderStage irs, Viewport* v)
{
    auto queue = source->getRenderQueue();
    if (mLines.getCurrentVertexCount())
    {
        mLines.end();
        mLines._updateRenderQueue(queue);
    }
    if (mAxes.getCurrentVertexCount())
    {
        mAxes.end();
        mAxes._updateRenderQueue(queue);
    }

    if (mStatic)
    {
        mLines._updateRenderQueue(queue);
        mAxes._updateRenderQueue(queue);
    }
}

} /* namespace Ogre */
