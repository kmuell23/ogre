// This file is part of the OGRE project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at https://www.ogre3d.org/licensing.

#ifndef OGRE_RENDERSYSTEMS_GLSUPPORT_RENDERTARGET_H
#define OGRE_RENDERSYSTEMS_GLSUPPORT_RENDERTARGET_H


namespace Ogre
{
class GLContext;
class GLFrameBufferObjectCommon;

class GLRenderTarget
{
public:
    virtual ~GLRenderTarget() = default;

    /** Get the GL context that needs to be active to render to this target
     */
    [[nodiscard]] virtual GLContext* getContext() const noexcept = 0;
    virtual GLFrameBufferObjectCommon* getFBO() noexcept { return nullptr; }
};
} // namespace Ogre

#endif // OGREGLRENDERTARGET_H
