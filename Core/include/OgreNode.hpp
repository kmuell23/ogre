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
export module Ogre.Core:Node;

export import :Math;
export import :Matrix3;
export import :Matrix4;
export import :MemoryAllocatorConfig;
export import :Platform;
export import :Prerequisites;
export import :Quaternion;
export import :String;
export import :Vector;

export import <algorithm>;
export import <set>;
export import <string_view>;
export import <vector>;

export
namespace Ogre {
    template <typename T> class VectorIterator;
    template <typename T> class ConstVectorIterator;
class Camera;

    /** \addtogroup Core
    *  @{
    */
    /** \addtogroup Scene
    *  @{
    */
    /** Class representing a general-purpose node an articulated scene graph.
    @remarks
        A node in the scene graph is a node in a structured tree. A node contains
        information about the transformation which will apply to
        it and all of it's children. Child nodes can have transforms of their own, which
        are combined with their parent's transformations.
    @par
        This is an abstract class - concrete classes are based on this for specific purposes,
        e.g. SceneNode, Bone
    */
    class Node : public NodeAlloc
    {
    public:
        /** Enumeration denoting the spaces which a transform can be relative to.
        */
        enum class TransformSpace
        {
            /// Transform is relative to the local space
            LOCAL,
            /// Transform is relative to the space of the parent node
            PARENT,
            /// Transform is relative to world space
            WORLD
        };
        using ChildNodeMap = std::vector<Node *>;
        using ChildNodeIterator = VectorIterator<ChildNodeMap>;
        using ConstChildNodeIterator = ConstVectorIterator<ChildNodeMap>;

        /** Listener which gets called back on Node events.
        */
        class Listener
        {
        public:
            Listener() = default;
            virtual ~Listener() = default;
            /** Called when a node gets updated.
            @remarks
                Note that this happens when the node's derived update happens,
                not every time a method altering it's state occurs. There may 
                be several state-changing calls but only one of these calls, 
                when the node graph is fully updated.
            */
            virtual void nodeUpdated(const Node*) {}
            /** Node is being destroyed */
            virtual void nodeDestroyed(const Node*) {}
            /** Node has been attached to a parent */
            virtual void nodeAttached(const Node*) {}
            /** Node has been detached from a parent */
            virtual void nodeDetached(const Node*) {}
        };

    protected:
        /// Pointer to parent node
        Node* mParent;
        /// Collection of pointers to direct children
        ChildNodeMap mChildren;

        using ChildUpdateSet = std::set<Node *>;
        /// List of children which need updating, used if self is not out of date but children are
        ChildUpdateSet mChildrenToUpdate;
        /// Friendly name of this node
        SmallString<16uz> mName;

        /// Flag to indicate own transform from parent is out of date
        mutable bool mNeedParentUpdate : 1;
        /// Flag indicating that all children need to be updated
        bool mNeedChildUpdate : 1;
        /// Flag indicating that parent has been notified about update request
        bool mParentNotified : 1;
        /// Flag indicating that the node has been queued for update
        bool mQueuedForUpdate : 1;
        /// Stores whether this node inherits orientation from it's parent
        bool mInheritOrientation : 1;
        /// Stores whether this node inherits scale from it's parent
        bool mInheritScale : 1;
        mutable bool mCachedTransformOutOfDate : 1;

        /// Stores the orientation of the node relative to it's parent.
        Quaternion mOrientation;
        /// Stores the position/translation of the node relative to its parent.
        Vector3 mPosition;
        /// Stores the scaling factor applied to this node
        Vector3 mScale;

        /// Cached derived transform as a 4x4 matrix
        mutable Affine3 mCachedTransform;

        /// Only available internally - notification of parent.
        virtual void setParent(Node* parent);

        /** Cached combined orientation.
        @par
            This member is the orientation derived by combining the
            local transformations and those of it's parents.
            This is updated when _updateFromParent is called by the
            SceneManager or the nodes parent.
        */
        mutable Quaternion mDerivedOrientation;

        /** Cached combined position.
        @par
            This member is the position derived by combining the
            local transformations and those of it's parents.
            This is updated when _updateFromParent is called by the
            SceneManager or the nodes parent.
        */
        mutable Vector3 mDerivedPosition;

        /** Cached combined scale.
        @par
            This member is the position derived by combining the
            local transformations and those of it's parents.
            This is updated when _updateFromParent is called by the
            SceneManager or the nodes parent.
        */
        mutable Vector3 mDerivedScale;

        /** Triggers the node to update it's combined transforms.
        @par
            This method is called internally by Ogre to ask the node
            to update it's complete transformation based on it's parents
            derived transform.
        */
        void _updateFromParent() const;

        /** Class-specific implementation of _updateFromParent.
        @remarks
            Splitting the implementation of the update away from the update call
            itself allows the detail to be overridden without disrupting the 
            general sequence of updateFromParent (e.g. raising events)
        */
        virtual void updateFromParentImpl() const;
    private:
        /// The position to use as a base for keyframe animation
        Vector3 mInitialPosition;
        /// The orientation to use as a base for keyframe animation
        Quaternion mInitialOrientation;
        /// The scale to use as a base for keyframe animation
        Vector3 mInitialScale;

        /** Node listener - only one allowed (no list) for size & performance reasons. */
        Listener* mListener;

        using QueuedUpdates = std::vector<Node *>;
        static QueuedUpdates msQueuedUpdates;

        /** Internal method for creating a new child node - must be overridden per subclass. */
        virtual auto createChildImpl() -> Node* = 0;

        /** Internal method for creating a new child node - must be overridden per subclass. */
        virtual auto createChildImpl(std::string_view name) -> Node* = 0;
    public:
        /// Constructor, should only be called by parent, not directly.
        Node();
        /** Constructor, should only be called by parent, not directly.
        @remarks
            Assigned a name.
        */
        Node(std::string_view name);

        virtual ~Node();  

        /** Returns the name of the node. */
        auto getName() const -> std::string_view { return mName.data; }

        /** Gets this node's parent (NULL if this is the root).
        */
        auto getParent() const noexcept -> Node* { return mParent; }

        /** Returns a quaternion representing the nodes orientation.
        */
        auto getOrientation() const noexcept -> const Quaternion & { return mOrientation; }

        /** Sets the orientation of this node via a quaternion.
        @remarks
            Orientations, unlike other transforms, are not always inherited by child nodes.
            Whether or not orientations affect the orientation of the child nodes depends on
            the setInheritOrientation option of the child. In some cases you want a orientating
            of a parent node to apply to a child node (e.g. where the child node is a part of
            the same object, so you want it to be the same relative orientation based on the
            parent's orientation), but not in other cases (e.g. where the child node is just
            for positioning another object, you want it to maintain it's own orientation).
            The default is to inherit as with other transforms.
        @par
            Note that rotations are oriented around the node's origin.
        */
        void setOrientation( const Quaternion& q );

        /// @overload
        void setOrientation( Real w, Real x, Real y, Real z);

        /** Resets the nodes orientation (local axes as world axes, no rotation).
        @remarks
            Orientations, unlike other transforms, are not always inherited by child nodes.
            Whether or not orientations affect the orientation of the child nodes depends on
            the setInheritOrientation option of the child. In some cases you want a orientating
            of a parent node to apply to a child node (e.g. where the child node is a part of
            the same object, so you want it to be the same relative orientation based on the
            parent's orientation), but not in other cases (e.g. where the child node is just
            for positioning another object, you want it to maintain it's own orientation).
            The default is to inherit as with other transforms.
        @par
            Note that rotations are oriented around the node's origin.
        */
        void resetOrientation();

        /** Sets the position of the node relative to it's parent.
        */
        void setPosition(const Vector3& pos);

        /// @overload
        void setPosition(Real x, Real y, Real z) { setPosition(Vector3{x, y, z}); }

        /** Gets the position of the node relative to it's parent.
        */
        auto getPosition() const noexcept -> const Vector3 & { return mPosition; }

        /** Sets the scaling factor applied to this node.
        @remarks
            Scaling factors, unlike other transforms, are not always inherited by child nodes.
            Whether or not scalings affect the size of the child nodes depends on the setInheritScale
            option of the child. In some cases you want a scaling factor of a parent node to apply to
            a child node (e.g. where the child node is a part of the same object, so you want it to be
            the same relative size based on the parent's size), but not in other cases (e.g. where the
            child node is just for positioning another object, you want it to maintain it's own size).
            The default is to inherit as with other transforms.
        @par
            Note that like rotations, scalings are oriented around the node's origin.
        */
        void setScale(const Vector3& scale);

        /// @overload
        void setScale(Real x, Real y, Real z) { setScale(Vector3{x, y, z}); }

        /** Gets the scaling factor of this node.
        */
        auto getScale() const noexcept -> const Vector3& { return mScale; }

        /** Tells the node whether it should inherit orientation from it's parent node.
        @remarks
            Orientations, unlike other transforms, are not always inherited by child nodes.
            Whether or not orientations affect the orientation of the child nodes depends on
            the setInheritOrientation option of the child. In some cases you want a orientating
            of a parent node to apply to a child node (e.g. where the child node is a part of
            the same object, so you want it to be the same relative orientation based on the
            parent's orientation), but not in other cases (e.g. where the child node is just
            for positioning another object, you want it to maintain it's own orientation).
            The default is to inherit as with other transforms.
        @param inherit If true, this node's orientation will be affected by its parent's orientation.
            If false, it will not be affected.
        */
        void setInheritOrientation(bool inherit);

        /** Returns true if this node is affected by orientation applied to the parent node. 
        @remarks
            Orientations, unlike other transforms, are not always inherited by child nodes.
            Whether or not orientations affect the orientation of the child nodes depends on
            the setInheritOrientation option of the child. In some cases you want a orientating
            of a parent node to apply to a child node (e.g. where the child node is a part of
            the same object, so you want it to be the same relative orientation based on the
            parent's orientation), but not in other cases (e.g. where the child node is just
            for positioning another object, you want it to maintain it's own orientation).
            The default is to inherit as with other transforms.
        @remarks
            See setInheritOrientation for more info.
        */
        auto getInheritOrientation() const noexcept -> bool { return mInheritOrientation; }

        /** Tells the node whether it should inherit scaling factors from it's parent node.
        @remarks
            Scaling factors, unlike other transforms, are not always inherited by child nodes.
            Whether or not scalings affect the size of the child nodes depends on the setInheritScale
            option of the child. In some cases you want a scaling factor of a parent node to apply to
            a child node (e.g. where the child node is a part of the same object, so you want it to be
            the same relative size based on the parent's size), but not in other cases (e.g. where the
            child node is just for positioning another object, you want it to maintain it's own size).
            The default is to inherit as with other transforms.
        @param inherit If true, this node's scale will be affected by its parent's scale. If false,
            it will not be affected.
        */
        void setInheritScale(bool inherit);

        /** Returns true if this node is affected by scaling factors applied to the parent node. 
        @remarks
            See setInheritScale for more info.
        */
        auto getInheritScale() const noexcept -> bool { return mInheritScale; }

        /** Scales the node, combining it's current scale with the passed in scaling factor. 
        @remarks
            This method applies an extra scaling factor to the node's existing scale, (unlike setScale
            which overwrites it) combining it's current scale with the new one. E.g. calling this 
            method twice with Vector3{2,2,2} would have the same effect as setScale(Vector3{4,4,4}) if
            the existing scale was 1.
        @par
            Note that like rotations, scalings are oriented around the node's origin.
        */
        void scale(const Vector3& scale);

        /// @overload
        void scale(Real x, Real y, Real z);

        /** Moves the node along the Cartesian axes.
        @par
            This method moves the node by the supplied vector along the
            world Cartesian axes, i.e. along world x,y,z
        @param d
            Vector with x,y,z values representing the translation.
        @param relativeTo
            The space which this transform is relative to.
        */
        void translate(const Vector3& d, TransformSpace relativeTo = TransformSpace::PARENT);
        /// @overload
        void translate(Real x, Real y, Real z, TransformSpace relativeTo = TransformSpace::PARENT)
        {
            translate(Vector3{x, y, z}, relativeTo);
        }
        /** Moves the node along arbitrary axes.
        @remarks
            This method translates the node by a vector which is relative to
            a custom set of axes.
        @param axes
            A 3x3 Matrix containing 3 column vectors each representing the
            axes X, Y and Z respectively. In this format the standard cartesian
            axes would be expressed as:
            <pre>
            1 0 0
            0 1 0
            0 0 1
            </pre>
            i.e. the identity matrix.
        @param move
            Vector relative to the axes above.
        @param relativeTo
            The space which this transform is relative to.
        */
        void translate(const Matrix3& axes, const Vector3& move, TransformSpace relativeTo = TransformSpace::PARENT)
        {
            translate(axes * move, relativeTo);
        }
        /// @overload
        void translate(const Matrix3& axes, Real x, Real y, Real z, TransformSpace relativeTo = TransformSpace::PARENT)
        {
            translate(axes, Vector3{x, y, z}, relativeTo);
        }

        /** Rotate the node around the Z-axis.
        */
        virtual void roll(const Radian& angle, TransformSpace relativeTo = TransformSpace::LOCAL)
        {
            rotate(Quaternion::FromAngleAndAxis(angle, Vector3::UNIT_Z), relativeTo);
        }

        /** Rotate the node around the X-axis.
        */
        virtual void pitch(const Radian& angle, TransformSpace relativeTo = TransformSpace::LOCAL)
        {
            rotate(Quaternion::FromAngleAndAxis(angle, Vector3::UNIT_X), relativeTo);
        }

        /** Rotate the node around the Y-axis.
        */
        virtual void yaw(const Radian& angle, TransformSpace relativeTo = TransformSpace::LOCAL)
        {
            rotate(Quaternion::FromAngleAndAxis(angle, Vector3::UNIT_Y), relativeTo);
        }

        /** Rotate the node around an arbitrary axis.
        */
        void rotate(const Vector3& axis, const Radian& angle, TransformSpace relativeTo = TransformSpace::LOCAL)
        {
            rotate(Quaternion::FromAngleAndAxis(angle, axis), relativeTo);
        }

        /** Rotate the node around an aritrary axis using a Quarternion.
        */
        void rotate(const Quaternion& q, TransformSpace relativeTo = TransformSpace::LOCAL);

        /** Gets a matrix whose columns are the local axes based on
            the nodes orientation relative to it's parent. */
        auto getLocalAxes() const -> Matrix3;

        /** Creates an unnamed new Node as a child of this node.
        @param translate
            Initial translation offset of child relative to parent
        @param rotate
            Initial rotation relative to parent
        */
        virtual auto createChild(
            const Vector3& translate = Vector3::ZERO, 
            const Quaternion& rotate = Quaternion::IDENTITY ) -> Node*;

        /** Creates a new named Node as a child of this node.
        @remarks
            This creates a child node with a given name, which allows you to look the node up from 
            the parent which holds this collection of nodes.
        @param name Name of the Node to create
        @param translate
            Initial translation offset of child relative to parent
        @param rotate
            Initial rotation relative to parent
        */
        virtual auto createChild(std::string_view name, const Vector3& translate = Vector3::ZERO, const Quaternion& rotate = Quaternion::IDENTITY) -> Node*;

        /** Adds a (precreated) child scene node to this node. If it is attached to another node,
            it must be detached first.
        @param child The Node which is to become a child node of this one
        */
        void addChild(Node* child);

        /** Reports the number of child nodes under this one.
        @deprecated use getChildren()
        */
        auto numChildren() const -> uint16 { return static_cast< uint16 >( mChildren.size() ); }

        /** Gets a pointer to a child node.
        @remarks
            There is an alternate getChild method which returns a named child.
        @deprecated use getChildren()
        */
        auto getChild(unsigned short index) const -> Node*;

        /** Gets a pointer to a named child node.
        */
        auto getChild(std::string_view name) const -> Node*;

        /// List of sub-nodes of this Node
        auto getChildren() const noexcept -> const ChildNodeMap& { return mChildren; }

        /** Drops the specified child from this node. 
        @remarks
            Does not delete the node, just detaches it from
            this parent, potentially to be reattached elsewhere. 
            There is also an alternate version which drops a named
            child from this node.
        */
        virtual auto removeChild(unsigned short index) -> Node*;
        /// @overload
        virtual auto removeChild(Node* child) -> Node*;

        /** Drops the named child from this node. 
        @remarks
            Does not delete the node, just detaches it from
            this parent, potentially to be reattached elsewhere.
        */
        virtual auto removeChild(std::string_view name) -> Node*;
        /** Removes all child Nodes attached to this node. Does not delete the nodes, just detaches them from
            this parent, potentially to be reattached elsewhere.
        */
        virtual void removeAllChildren();
        
        /** Sets the final world position of the node directly.
        @remarks 
            It's advisable to use the local setPosition if possible
        */
        void _setDerivedPosition(const Vector3& pos);

        /** Sets the final world orientation of the node directly.
        @remarks 
            It's advisable to use the local setOrientation if possible, this simply does
            the conversion for you.
        */
        void _setDerivedOrientation(const Quaternion& q);

        /** Gets the orientation of the node as derived from all parents.
        */
        auto _getDerivedOrientation() const -> const Quaternion &;

        /** Gets the position of the node as derived from all parents.
        */
        auto _getDerivedPosition() const -> const Vector3 &;

        /** Gets the scaling factor of the node as derived from all parents.
        */
        auto _getDerivedScale() const -> const Vector3 &;

        /** Gets the full transformation matrix for this node.
        @remarks
            This method returns the full transformation matrix
            for this node, including the effect of any parent node
            transformations, provided they have been updated using the Node::_update method.
            This should only be called by a SceneManager which knows the
            derived transforms have been updated before calling this method.
            Applications using Ogre should just use the relative transforms.
        */
        auto _getFullTransform() const -> const Affine3&;

        /** Internal method to update the Node.
        @note
            Updates this node and any relevant children to incorporate transforms etc.
            Don't call this yourself unless you are writing a SceneManager implementation.
        @param updateChildren
            If @c true, the update cascades down to all children. Specify false if you wish to
            update children separately, e.g. because of a more selective SceneManager implementation.
        @param parentHasChanged
            This flag indicates that the parent transform has changed,
            so the child should retrieve the parent's transform and combine
            it with its own even if it hasn't changed itself.
        */
        virtual void _update(bool updateChildren, bool parentHasChanged);

        /** Sets a listener for this Node.
        @remarks
            Note for size and performance reasons only one listener per node is
            allowed.
        */
        void setListener(Listener* listener) { mListener = listener; }
        
        /** Gets the current listener for this Node.
        */
        auto getListener() const noexcept -> Listener* { return mListener; }
        

        /** Sets the current transform of this node to be the 'initial state' ie that
            position / orientation / scale to be used as a basis for delta values used
            in keyframe animation.
        @remarks
            You never need to call this method unless you plan to animate this node. If you do
            plan to animate it, call this method once you've loaded the node with it's base state,
            ie the state on which all keyframes are based.
        @par
            If you never call this method, the initial state is the identity transform, ie do nothing.
        */
        void setInitialState();

        /** Resets the position / orientation / scale of this node to it's initial state, see setInitialState for more info. */
        void resetToInitialState();

        /** Gets the initial position of this node, see setInitialState for more info. 
        @remarks
            Also resets the cumulative animation weight used for blending.
        */
        auto getInitialPosition() const noexcept -> const Vector3& { return mInitialPosition; }
        
        /** Gets the local position, relative to this node, of the given world-space position */
        auto convertWorldToLocalPosition( const Vector3 &worldPos ) -> Vector3;

        /** Gets the world position of a point in the node local space
            useful for simple transforms that don't require a child node.*/
        auto convertLocalToWorldPosition( const Vector3 &localPos ) -> Vector3;

        /** Gets the local direction, relative to this node, of the given world-space direction */
        auto convertWorldToLocalDirection( const Vector3 &worldDir, bool useScale ) -> Vector3;

        /** Gets the world direction of a point in the node local space
            useful for simple transforms that don't require a child node.*/
        auto convertLocalToWorldDirection( const Vector3 &localDir, bool useScale ) -> Vector3;

        /** Gets the local orientation, relative to this node, of the given world-space orientation */
        auto convertWorldToLocalOrientation( const Quaternion &worldOrientation ) -> Quaternion;

        /** Gets the world orientation of an orientation in the node local space
            useful for simple transforms that don't require a child node.*/
        auto convertLocalToWorldOrientation( const Quaternion &localOrientation ) -> Quaternion;

        /** Gets the initial orientation of this node, see setInitialState for more info. */
        auto getInitialOrientation() const noexcept -> const Quaternion& { return mInitialOrientation; }

        /** Gets the initial position of this node, see setInitialState for more info. */
        auto getInitialScale() const noexcept -> const Vector3& { return mInitialScale; }

        /** Helper function, get the squared view depth.  */
        auto getSquaredViewDepth(const Camera* cam) const -> Real;

        /** To be called in the event of transform changes to this node that require it's recalculation.
        @remarks
            This not only tags the node state as being 'dirty', it also requests it's parent to 
            know about it's dirtiness so it will get an update next time.
        @param forceParentUpdate Even if the node thinks it has already told it's
            parent, tell it anyway
        */
        virtual void needUpdate(bool forceParentUpdate = false);
        /** Called by children to notify their parent that they need an update.
        @param child The child Node to be updated
        @param forceParentUpdate Even if the node thinks it has already told it's
            parent, tell it anyway
        */
        void requestUpdate(Node* child, bool forceParentUpdate = false);
        /** Called by children to notify their parent that they no longer need an update. */
        void cancelUpdate(Node* child);

        /** Queue a 'needUpdate' call to a node safely.
        @remarks
            You can't call needUpdate() during the scene graph update, e.g. in
            response to a Node::Listener hook, because the graph is already being 
            updated, and update flag changes cannot be made reliably in that context. 
            Call this method if you need to queue a needUpdate call in this case.
        */
        static void queueNeedUpdate(Node* n);
        /** Process queued 'needUpdate' calls. */
        static void processQueuedUpdates();
    };
    /** @} */
    /** @} */

} // namespace Ogre
