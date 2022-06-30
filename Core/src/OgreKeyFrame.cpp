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
#include "OgreKeyFrame.hpp"

#include "OgreAnimationTrack.hpp"

namespace Ogre
{
    //---------------------------------------------------------------------
    KeyFrame::KeyFrame(const AnimationTrack* parent, Real time) 
        : mTime(time), mParentTrack(parent)
    {
    }
    //---------------------------------------------------------------------
    KeyFrame* KeyFrame::_clone(AnimationTrack* newParent) const
    {
        return new KeyFrame(newParent, mTime);
    }
    //---------------------------------------------------------------------
    NumericKeyFrame::NumericKeyFrame(const AnimationTrack* parent, Real time)
        :KeyFrame(parent, time)
    {
    }
    //---------------------------------------------------------------------
    const AnyNumeric& NumericKeyFrame::getValue() const noexcept
    {
        return mValue;
    }
    //---------------------------------------------------------------------
    void NumericKeyFrame::setValue(const AnyNumeric& val)
    {
        mValue = val;
    }
    //---------------------------------------------------------------------
    KeyFrame* NumericKeyFrame::_clone(AnimationTrack* newParent) const
    {
        auto* newKf = new NumericKeyFrame(newParent, mTime);
        newKf->mValue = mValue;
        return newKf;
    }
    //---------------------------------------------------------------------
    TransformKeyFrame::TransformKeyFrame(const AnimationTrack* parent, Real time)
        :KeyFrame(parent, time), mTranslate(Vector3::ZERO), 
        mScale(Vector3::UNIT_SCALE), mRotate(Quaternion::IDENTITY) 
    {
    }
    //---------------------------------------------------------------------
    void TransformKeyFrame::setTranslate(const Vector3& trans)
    {
        mTranslate = trans;
        if (mParentTrack)
            mParentTrack->_keyFrameDataChanged();
    }
    //---------------------------------------------------------------------
    const Vector3& TransformKeyFrame::getTranslate() const noexcept
    {
        return mTranslate;
    }
    //---------------------------------------------------------------------
    void TransformKeyFrame::setScale(const Vector3& scale)
    {
        mScale = scale;
        if (mParentTrack)
            mParentTrack->_keyFrameDataChanged();
    }
    //---------------------------------------------------------------------
    const Vector3& TransformKeyFrame::getScale() const noexcept
    {
        return mScale;
    }
    //---------------------------------------------------------------------
    void TransformKeyFrame::setRotation(const Quaternion& rot)
    {
        mRotate = rot;
        if (mParentTrack)
            mParentTrack->_keyFrameDataChanged();
    }
    //---------------------------------------------------------------------
    const Quaternion& TransformKeyFrame::getRotation() const noexcept
    {
        return mRotate;
    }
    //---------------------------------------------------------------------
    KeyFrame* TransformKeyFrame::_clone(AnimationTrack* newParent) const
    {
        auto* newKf = new TransformKeyFrame(newParent, mTime);
        newKf->mTranslate = mTranslate;
        newKf->mScale = mScale;
        newKf->mRotate = mRotate;
        return newKf;
    }
    //---------------------------------------------------------------------
    VertexMorphKeyFrame::VertexMorphKeyFrame(const AnimationTrack* parent, Real time)
        : KeyFrame(parent, time)
    {
    }
    //---------------------------------------------------------------------
    void VertexMorphKeyFrame::setVertexBuffer(const HardwareVertexBufferSharedPtr& buf)
    {
        mBuffer = buf;
    }
    //---------------------------------------------------------------------
    const HardwareVertexBufferSharedPtr& 
    VertexMorphKeyFrame::getVertexBuffer() const noexcept
    {
        return mBuffer;
    }
    //---------------------------------------------------------------------
    KeyFrame* VertexMorphKeyFrame::_clone(AnimationTrack* newParent) const
    {
        auto* newKf = new VertexMorphKeyFrame(newParent, mTime);
        newKf->mBuffer = mBuffer;
        return newKf;
    }   
    //---------------------------------------------------------------------
    VertexPoseKeyFrame::VertexPoseKeyFrame(const AnimationTrack* parent, Real time)
        :KeyFrame(parent, time)
    {
    }
    //---------------------------------------------------------------------
    void VertexPoseKeyFrame::addPoseReference(ushort poseIndex, Real influence)
    {
        mPoseRefs.push_back(PoseRef(poseIndex, influence));
    }
    //---------------------------------------------------------------------
    void VertexPoseKeyFrame::updatePoseReference(ushort poseIndex, Real influence)
    {
        for (auto & mPoseRef : mPoseRefs)
        {
            if (mPoseRef.poseIndex == poseIndex)
            {
                mPoseRef.influence = influence;
                return;
            }
        }
        // if we got here, we didn't find it
        addPoseReference(poseIndex, influence);

    }
    //---------------------------------------------------------------------
    void VertexPoseKeyFrame::removePoseReference(ushort poseIndex)
    {
        for (auto i = mPoseRefs.begin(); i != mPoseRefs.end(); ++i)
        {
            if (i->poseIndex == poseIndex)
            {
                mPoseRefs.erase(i);
                return;
            }
        }
    }
    //---------------------------------------------------------------------
    void VertexPoseKeyFrame::removeAllPoseReferences()
    {
        mPoseRefs.clear();
    }
    //---------------------------------------------------------------------
    const VertexPoseKeyFrame::PoseRefList& 
    VertexPoseKeyFrame::getPoseReferences() const noexcept
    {
        return mPoseRefs;
    }

    //---------------------------------------------------------------------
    KeyFrame* VertexPoseKeyFrame::_clone(AnimationTrack* newParent) const
    {
        auto* newKf = new VertexPoseKeyFrame(newParent, mTime);
        // By-value copy ok
        newKf->mPoseRefs = mPoseRefs;
        return newKf;
    }   
    //---------------------------------------------------------------------
    void VertexPoseKeyFrame::_applyBaseKeyFrame(const VertexPoseKeyFrame* base)
    {
        // We subtract the matching pose influences in the base keyframe from the
        // influences in this keyframe
        for (auto & myPoseRef : mPoseRefs)
        {
            Real baseInfluence = 0.0f;
            for (const PoseRef& basePoseRef : base->getPoseReferences())
            {
                if (basePoseRef.poseIndex == myPoseRef.poseIndex)
                {
                    baseInfluence = basePoseRef.influence;
                    break;
                }
            }
            
            myPoseRef.influence -= baseInfluence;
            
        }
        
    }


}

