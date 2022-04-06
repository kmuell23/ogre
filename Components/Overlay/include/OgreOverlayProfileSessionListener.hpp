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

#ifndef OGRE_COMPONENTS_OVERLAY_PROFILESESSIONLISTENER_H
#define OGRE_COMPONENTS_OVERLAY_PROFILESESSIONLISTENER_H

#include <list>

#include "OgrePrerequisites.hpp"
#include "OgreProfiler.hpp"

namespace Ogre  {
    class Overlay;
    class OverlayContainer;
    class OverlayElement;

    /** \addtogroup Optional
    *  @{
    */
    /** \addtogroup Overlays
    *  @{
    */

    /** Concrete impl. of the ProfileSessionListener which visualizes
        the profling results using overlays.
    */
    class OverlayProfileSessionListener : public ProfileSessionListener
    {
    public:
        enum DisplayMode
        {
            /// Display % frame usage on the overlay
            DISPLAY_PERCENTAGE,
            /// Display microseconds on the overlay
            DISPLAY_MICROSECONDS
        };

        OverlayProfileSessionListener();
        virtual ~OverlayProfileSessionListener();

        /// @see ProfileSessionListener::initializeSession
        virtual void initializeSession();

        /// @see ProfileSessionListener::finializeSession
        virtual void finializeSession();

        /// @see ProfileSessionListener::displayResults
        virtual void displayResults(const ProfileInstance& instance, ulong maxTotalFrameTime);

        /// @see ProfileSessionListener::changeEnableState
        virtual void changeEnableState(bool enabled);

        /** Set the size of the profiler overlay, in pixels. */
        void setOverlayDimensions(Real width, Real height);

        /** Set the position of the profiler overlay, in pixels. */
        void setOverlayPosition(Real left, Real top);

        auto getOverlayWidth() const -> Real;
        auto getOverlayHeight() const -> Real;
        auto getOverlayLeft() const -> Real;
        auto getOverlayTop() const -> Real;

        /// Set the display mode for the overlay.
        void setDisplayMode(DisplayMode d) { mDisplayMode = d; }

        /// Get the display mode for the overlay.
        auto getDisplayMode() const -> DisplayMode { return mDisplayMode; }

    private:
        typedef std::list<OverlayElement*> ProfileBarList;

        /** Prints the profiling results of each frame 
        @remarks Recursive, for all the little children. */
        void displayResults(ProfileInstance* instance, ProfileBarList::const_iterator& bIter, ulong& maxTimeClocks, Real& newGuiHeight, int& profileCount);

        /** An internal function to create the container which will hold our display elements*/
        auto createContainer() -> OverlayContainer*;

        /** An internal function to create a text area */
        auto createTextArea(const String& name, Real width, Real height, Real top, Real left, 
                                    uint fontSize, const String& caption, bool show = true) -> OverlayElement*;

        /** An internal function to create a panel */
        auto createPanel(const String& name, Real width, Real height, Real top, Real left, 
                                const String& materialName, bool show = true) -> OverlayElement*;

        /// Holds the display bars for each profile results
        ProfileBarList mProfileBars;

        /// The overlay which contains our profiler results display
        Overlay* mOverlay;

        /// The window that displays the profiler results
        OverlayContainer* mProfileGui;

        /// The height of each bar
        Real mBarHeight;

        /// The height of the stats window
        Real mGuiHeight;

        /// The width of the stats window
        Real mGuiWidth;

        /// The horz position of the stats window
        Real mGuiLeft;

        /// The vertical position of the stats window
        Real mGuiTop;

        /// The size of the indent for each profile display bar
        Real mBarIndent;

        /// The width of the border between the profile window and each bar
        Real mGuiBorderWidth;

        /// The width of the min, avg, and max lines in a profile display
        Real mBarLineWidth;

        /// The distance between bars
        Real mBarSpacing;

        /// The max number of profiles we can display
        uint mMaxDisplayProfiles;

        /// How to display the overlay
        DisplayMode mDisplayMode;
    };
}
#endif
