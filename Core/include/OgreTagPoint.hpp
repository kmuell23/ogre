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
export module Ogre.Core:TagPoint;

export import :Bone;
export import :Matrix4;

export
namespace Ogre  {
class Entity;
class MovableObject;
class Skeleton;


    
    /** \addtogroup Core
    *  @{
    */
    /** \addtogroup Animation
    *  @{
    */
    /** A tagged point on a skeleton, which can be used to attach entities to on specific
        other entities.
    @remarks
        A Skeleton, like a Mesh, is shared between Entity objects and simply updated as required
        when it comes to rendering. However there are times when you want to attach another object
        to an animated entity, and make sure that attachment follows the parent entity's animation
        (for example, a character holding a gun in his / her hand). This class simply identifies
        attachment points on a skeleton which can be used to attach child objects. 
    @par
        The child objects themselves are not physically attached to this class; as it's name suggests
        this class just 'tags' the area. The actual child objects are attached to the Entity using the
        skeleton which has this tag point. Use the Entity::attachMovableObjectToBone method to attach
        the objects, which creates a new TagPoint on demand.
    */
    class TagPoint : public Bone
    {

    public:
        TagPoint(unsigned short handle, Skeleton* creator);

        auto getParentEntity() const -> Entity *;
        auto getChildObject() const noexcept -> MovableObject*;
        
        void setParentEntity(Entity *pEntity);
        void setChildObject(MovableObject *pObject);

        /** Tells the TagPoint whether it should inherit orientation from it's parent entity.
        @param inherit If true, this TagPoint's orientation will be affected by
            its parent entity's orientation. If false, it will not be affected.
        */
        void setInheritParentEntityOrientation(bool inherit);

        /** Returns true if this TagPoint is affected by orientation applied to the parent entity. 
        */
        auto getInheritParentEntityOrientation() const noexcept -> bool;

        /** Tells the TagPoint whether it should inherit scaling factors from it's parent entity.
        @param inherit If true, this TagPoint's scaling factors will be affected by
            its parent entity's scaling factors. If false, it will not be affected.
        */
        void setInheritParentEntityScale(bool inherit);

        /** Returns true if this TagPoint is affected by scaling factors applied to the parent entity. 
        */
        auto getInheritParentEntityScale() const noexcept -> bool;

        /** Gets the transform of parent entity. */
        auto getParentEntityTransform() const noexcept -> const Affine3&;

        /** Gets the transform of this node just for the skeleton (not entity) */
        auto _getFullLocalTransform() const -> const Affine3&;


        void needUpdate(bool forceParentUpdate = false) override;
        /** Overridden from Node in order to include parent Entity transform. */
        void updateFromParentImpl() const override;

    private:
        bool mInheritParentEntityOrientation{true};
        bool mInheritParentEntityScale{true};
        Entity *mParentEntity{nullptr};
        MovableObject *mChildObject{nullptr};
        mutable Affine3 mFullLocalTransform;
    };

    /** @} */
    /** @} */

} //namespace
