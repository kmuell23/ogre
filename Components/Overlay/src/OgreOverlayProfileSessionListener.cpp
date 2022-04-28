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

#include "OgreOverlayProfileSessionListener.hpp"

#include <map>
#include <string>
#include <utility>

#include "OgreOverlay.hpp"
#include "OgreOverlayContainer.hpp"
#include "OgreOverlayElement.hpp"
#include "OgreOverlayManager.hpp"
#include "OgreStringConverter.hpp"
#include "OgreTimer.hpp"

namespace Ogre
{
    //-----------------------------------------------------------------------
    OverlayProfileSessionListener::OverlayProfileSessionListener() 
         
    = default;
    //-----------------------------------------------------------------------
    OverlayProfileSessionListener::~OverlayProfileSessionListener()
    {
        mProfileBars.clear();
    }
    //-----------------------------------------------------------------------
    void OverlayProfileSessionListener::initializeSession()
    {
        // create a new overlay to hold our Profiler display
        mOverlay = OverlayManager::getSingleton().create("Profiler");
        mOverlay->setZOrder(500);

        // this panel will be the main container for our profile bars
        mProfileGui = createContainer();

        // we create an initial pool of 50 profile bars
        for (uint i = 0; i < mMaxDisplayProfiles; ++i) 
        {
            // this is for the profile name and the number of times it was called in a frame
            OverlayElement* element = createTextArea(::std::format("profileText{}", StringConverter::toString(i)), 90, mBarHeight, mGuiBorderWidth + (mBarHeight + mBarSpacing) * i, 0, 14, "", false);
            mProfileGui->addChild(element);
            mProfileBars.push_back(element);

            // this indicates the current frame time
            element = createPanel(::std::format("currBar{}", StringConverter::toString(i)), 0, mBarHeight, mGuiBorderWidth + (mBarHeight + mBarSpacing) * i, mBarIndent, "Core/ProfilerCurrent", false);
            mProfileGui->addChild(element);
            mProfileBars.push_back(element);

            // this indicates the minimum frame time
            element = createPanel(::std::format("minBar{}", StringConverter::toString(i)), mBarLineWidth, mBarHeight, mGuiBorderWidth + (mBarHeight + mBarSpacing) * i, 0, "Core/ProfilerMin", false);
            mProfileGui->addChild(element);
            mProfileBars.push_back(element);

            // this indicates the maximum frame time
            element = createPanel(::std::format("maxBar{}", StringConverter::toString(i)), mBarLineWidth, mBarHeight, mGuiBorderWidth + (mBarHeight + mBarSpacing) * i, 0, "Core/ProfilerMax", false);
            mProfileGui->addChild(element);
            mProfileBars.push_back(element);

            // this indicates the average frame time
            element = createPanel(::std::format("avgBar{}", StringConverter::toString(i)), mBarLineWidth, mBarHeight, mGuiBorderWidth + (mBarHeight + mBarSpacing) * i, 0, "Core/ProfilerAvg", false);
            mProfileGui->addChild(element);
            mProfileBars.push_back(element);

            // this indicates the text of the frame time
            element = createTextArea(::std::format("statText{}", StringConverter::toString(i)), 20, mBarHeight, mGuiBorderWidth + (mBarHeight + mBarSpacing) * i, 0, 14, "", false);
            mProfileGui->addChild(element);
            mProfileBars.push_back(element);
        }

        // throw everything all the GUI stuff into the overlay and display it
        mOverlay->add2D(mProfileGui);
    }
    //-----------------------------------------------------------------------
    void OverlayProfileSessionListener::finializeSession()
    {
        auto* container = dynamic_cast<OverlayContainer*>(mProfileGui);
        if (container)
        {
            auto const& children = container->getChildren();

            for (auto it = children.begin(); it != children.end();)
            {
                OverlayElement* element = it->second;
                OverlayContainer* parent = element->getParent();
                if (parent)
                {
                    auto eraseIt = parent->removeChild(element->getName());
                    if (parent == container)
                        it = eraseIt;
                    else
                        ++it;
                }
                OverlayManager::getSingleton().destroyOverlayElement(element);
            }
        }
        if(mProfileGui)
            OverlayManager::getSingleton().destroyOverlayElement(mProfileGui);
        if(mOverlay)
            OverlayManager::getSingleton().destroy(mOverlay);           
            
        mProfileBars.clear();
    }
    //-----------------------------------------------------------------------
    void OverlayProfileSessionListener::displayResults(const ProfileInstance& root, ulong maxTotalFrameClocks)
    {
        Real newGuiHeight = mGuiHeight;
        int profileCount = 0;

        auto bIter = ::std::as_const(mProfileBars).begin();
        auto it = root.children.begin(), endit = root.children.end();
        for(;it != endit; ++it)
        {
            ProfileInstance* child = it->second;
            displayResults(child, bIter, maxTotalFrameClocks, newGuiHeight, profileCount);
        }
            
        // set the main display dimensions
        mProfileGui->setMetricsMode(GMM_PIXELS);
        mProfileGui->setHeight(newGuiHeight);
        mProfileGui->setWidth(mGuiWidth * 2 + 15);
        mProfileGui->setTop(5);
        mProfileGui->setLeft(5);

        // we hide all the remaining pre-created bars
        for (; bIter != mProfileBars.end(); ++bIter) 
        {
            (*bIter)->hide();
        }
    }
    //-----------------------------------------------------------------------
    void OverlayProfileSessionListener::displayResults(ProfileInstance* instance, ProfileBarList::const_iterator& bIter, ulong& maxClocks, Real& newGuiHeight, int& profileCount)
    {
        OverlayElement* g;

        // display the profile's name and the number of times it was called in a frame
        g = *bIter;
        ++bIter;
        g->show();
        g->setCaption(String(instance->name + ::std::format(" ({})", StringConverter::toString(instance->history.numCallsThisFrame) )));
        g->setLeft(10 + instance->hierarchicalLvl * 10.0f);


        // display the main bar that show the percentage of the frame time that this
        // profile has taken
        g = *bIter;
        ++bIter;
        g->show();
        // most of this junk has been set before, but we do this to get around a weird
        // Ogre gui issue (bug?)
        g->setMetricsMode(GMM_PIXELS);
        g->setHeight(mBarHeight);

        if (mDisplayMode == DISPLAY_PERCENTAGE)
            g->setWidth( (instance->history.currentClocksPercent) * mGuiWidth);
        else
            g->setWidth( ((long double)instance->history.currentClocks / (long double)maxClocks) * mGuiWidth);

        g->setLeft(mGuiWidth);
        g->setTop(mGuiBorderWidth + profileCount * (mBarHeight + mBarSpacing));



        // display line to indicate the minimum frame time for this profile
        g = *bIter;
        ++bIter;
        g->show();
        if(mDisplayMode == DISPLAY_PERCENTAGE)
            g->setLeft(mBarIndent + instance->history.minClocksPercent * mGuiWidth);
        else
            g->setLeft(mBarIndent + ((long double)instance->history.minClocks / (long double)maxClocks) * mGuiWidth);

        // display line to indicate the maximum frame time for this profile
        g = *bIter;
        ++bIter;
        g->show();
        if(mDisplayMode == DISPLAY_PERCENTAGE)
            g->setLeft(mBarIndent + instance->history.maxClocksPercent * mGuiWidth);
        else
            g->setLeft(mBarIndent + ((long double)instance->history.maxClocks / (long double)maxClocks) * mGuiWidth);

        // display line to indicate the average frame time for this profile
        g = *bIter;
        ++bIter;
        g->show();
        if(instance->history.totalCalls != 0)
        {
            if (mDisplayMode == DISPLAY_PERCENTAGE)
                g->setLeft(mBarIndent + (instance->history.totalClocksPercent / (long double)instance->history.totalCalls) * mGuiWidth);
            else
                g->setLeft(mBarIndent + (((long double)instance->history.totalClocks / (long double)instance->history.totalCalls) / (long double)maxClocks) * mGuiWidth);
        }
        else
            g->setLeft(mBarIndent);

        // display text
        g = *bIter;
        ++bIter;
        g->show();
        if (mDisplayMode == DISPLAY_PERCENTAGE)
        {
            g->setLeft(mBarIndent + instance->history.currentClocksPercent * mGuiWidth + 2);
            g->setCaption(::std::format("{:3.3}%",
                instance->history.currentClocksPercent * 100.0l));
        }
        else
        {
            g->setLeft(mBarIndent + ((long double)instance->history.currentClocks / (long double)maxClocks) * mGuiWidth + 2);
            g->setCaption(::std::format("{}ms", Timer::clocksToMilliseconds(instance->history.currentClocks)));
        }

        // we set the height of the display with respect to the number of profiles displayed
        newGuiHeight += mBarHeight + mBarSpacing;

        ++profileCount;

        // display children
        auto it = instance->children.begin(), endit = instance->children.end();
        for(;it != endit; ++it)
        {
            ProfileInstance* child = it->second;
            displayResults(child, bIter, maxClocks, newGuiHeight, profileCount);
        }
    }
    //-----------------------------------------------------------------------
    void OverlayProfileSessionListener::changeEnableState(bool enabled) 
    {
        if (enabled) 
        {
            mOverlay->show();
        }
        else 
        {
            mOverlay->hide();
        }
    }
    //-----------------------------------------------------------------------
    auto OverlayProfileSessionListener::createContainer() -> OverlayContainer*
    {
        OverlayContainer* container = (OverlayContainer*) 
            OverlayManager::getSingleton().createOverlayElement(
                "BorderPanel", "profiler");
        container->setMetricsMode(GMM_PIXELS);
        container->setMaterialName("Core/StatsBlockCenter");
        container->setHeight(mGuiHeight);
        container->setWidth(mGuiWidth * 2 + 15);
        container->setParameter("border_size", "1 1 1 1");
        container->setParameter("border_material", "Core/StatsBlockBorder");
        container->setParameter("border_topleft_uv", "0.0000 1.0000 0.0039 0.9961");
        container->setParameter("border_top_uv", "0.0039 1.0000 0.9961 0.9961");
        container->setParameter("border_topright_uv", "0.9961 1.0000 1.0000 0.9961");
        container->setParameter("border_left_uv","0.0000 0.9961 0.0039 0.0039");
        container->setParameter("border_right_uv","0.9961 0.9961 1.0000 0.0039");
        container->setParameter("border_bottomleft_uv","0.0000 0.0039 0.0039 0.0000");
        container->setParameter("border_bottom_uv","0.0039 0.0039 0.9961 0.0000");
        container->setParameter("border_bottomright_uv","0.9961 0.0039 1.0000 0.0000");
        container->setLeft(5);
        container->setTop(5);

        return container;
    }
    //-----------------------------------------------------------------------
    auto OverlayProfileSessionListener::createTextArea(const String& name, Real width, Real height, Real top, Real left, 
                                         uint fontSize, const String& caption, bool show) -> OverlayElement*
    {
        OverlayElement* textArea = OverlayManager::getSingleton().createOverlayElement("TextArea", name);
        textArea->setMetricsMode(GMM_PIXELS);
        textArea->setWidth(width);
        textArea->setHeight(height);
        textArea->setTop(top);
        textArea->setLeft(left);
        textArea->setParameter("font_name", "SdkTrays/Value");
        textArea->setParameter("char_height", StringConverter::toString(fontSize));
        textArea->setCaption(caption);
        textArea->setParameter("colour_top", "1 1 1");
        textArea->setParameter("colour_bottom", "1 1 1");

        if (show) {
            textArea->show();
        }
        else {
            textArea->hide();
        }

        return textArea;
    }
    //-----------------------------------------------------------------------
    auto OverlayProfileSessionListener::createPanel(const String& name, Real width, Real height, Real top, Real left, 
                                      const String& materialName, bool show) -> OverlayElement*
    {
        OverlayElement* panel = 
            OverlayManager::getSingleton().createOverlayElement("Panel", name);
        panel->setMetricsMode(GMM_PIXELS);
        panel->setWidth(width);
        panel->setHeight(height);
        panel->setTop(top);
        panel->setLeft(left);
        panel->setMaterialName(materialName);

        if (show) {
            panel->show();
        }
        else {
            panel->hide();
        }

        return panel;
    }
    //-----------------------------------------------------------------------
    void OverlayProfileSessionListener::setOverlayPosition(Real left, Real top)
    {
        mGuiLeft = left;
        mGuiTop = top;

        mProfileGui->setPosition(left, top);
    }
    //---------------------------------------------------------------------
    auto OverlayProfileSessionListener::getOverlayWidth() const -> Real
    {
        return mGuiWidth;
    }
    //---------------------------------------------------------------------
    auto OverlayProfileSessionListener::getOverlayHeight() const -> Real
    {
        return mGuiHeight;
    }
    //---------------------------------------------------------------------
    auto OverlayProfileSessionListener::getOverlayLeft() const -> Real
    {
        return mGuiLeft;
    }
    //---------------------------------------------------------------------
    auto OverlayProfileSessionListener::getOverlayTop() const -> Real
    {
        return mGuiTop;
    }
    //-----------------------------------------------------------------------
    void OverlayProfileSessionListener::setOverlayDimensions(Real width, Real height)
    {
        mGuiWidth = width;
        mGuiHeight = height;
        mBarIndent = mGuiWidth;

        mProfileGui->setDimensions(width, height);
    }
    //-----------------------------------------------------------------------
}
