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
export module Ogre.Components.Bites:ApplicationContext;

export import :ApplicationContextBase;
export import :Input;

export
namespace OgreBites
{
    /** \addtogroup Optional
    *  @{
    */
    /** \addtogroup Bites
    *  @{
    */
    class ApplicationContextSDL : public ApplicationContextBase
    {
    protected:
        void _destroyWindow(const NativeWindowPair& win) override;
    public:
        explicit ApplicationContextSDL(std::string_view appName = "Ogre3D");

        void setWindowGrab(NativeWindowType* win, bool grab) override;
        [[nodiscard]] auto getDisplayDPI() const noexcept -> float override;
        void shutdown() override;
        void pollEvents() override;
        void addInputListener(NativeWindowType* win, InputListener* lis) override;
        void removeInputListener(NativeWindowType* win, InputListener* lis) override;
        auto
        createWindow(std::string_view name, uint32_t w = 0, uint32_t h = 0,
                     Ogre::NameValuePairList miscParams = Ogre::NameValuePairList()) -> NativeWindowPair override;

        using ApplicationContextBase::setWindowGrab;
        using ApplicationContextBase::addInputListener;
        using ApplicationContextBase::removeInputListener;
    };

    using ApplicationContext = ApplicationContextSDL;

    /** @} */
    /** @} */
}
