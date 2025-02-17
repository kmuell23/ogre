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

#include <cstddef>

export module Ogre.Core:PatchMesh;

export import :HardwareBuffer;
export import :Mesh;
export import :PatchSurface;
export import :Prerequisites;
export import :Resource;

export
namespace Ogre {
class ResourceManager;
class VertexDeclaration;

    /** \addtogroup Core
    *  @{
    */
    /** \addtogroup LOD
    *  @{
    */
    /** Patch specialisation of Mesh. 
    @remarks
        Instances of this class should be created by calling MeshManager::createBezierPatch.
    */
    class PatchMesh : public Mesh
    {
    private:
        /// Internal surface definition
        PatchSurface mSurface;
        /// Vertex declaration, cloned from the input
        VertexDeclaration* mDeclaration{nullptr};
    public:
        /// Constructor
        PatchMesh(ResourceManager* creator, std::string_view name, ResourceHandle handle,
            std::string_view group);
        /// Update the mesh with new control points positions.
        void update(void* controlPointBuffer, size_t width, size_t height, 
                    size_t uMaxSubdivisionLevel, size_t vMaxSubdivisionLevel, 
                    PatchSurface::VisibleSide visibleSide);
        /// Define the patch, as defined in MeshManager::createBezierPatch
        void define(void* controlPointBuffer, 
            VertexDeclaration *declaration, size_t width, size_t height,
            size_t uMaxSubdivisionLevel = PatchSurface::AUTO_LEVEL, 
            size_t vMaxSubdivisionLevel = PatchSurface::AUTO_LEVEL,
            PatchSurface::VisibleSide visibleSide = PatchSurface::VisibleSide::FRONT,
            HardwareBuffer::Usage vbUsage = HardwareBuffer::STATIC_WRITE_ONLY,
            HardwareBuffer::Usage ibUsage = HardwareBuffer::DYNAMIC_WRITE_ONLY,
            bool vbUseShadow = false, bool ibUseShadow = false);

        /* Sets the current subdivision level as a proportion of full detail.
        @param factor Subdivision factor as a value from 0 (control points only) to 1 (maximum
            subdivision). */
        void setSubdivision(Real factor);
    private:
        void loadImpl() override;
        /// Overridden from Resource - do nothing (no disk caching)
        void prepareImpl() override {}

    };
    /** @} */
    /** @} */

}
