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

#include <cassert>

module Ogre.Core;

import :DepthBuffer;
import :Exception;
import :Image;
import :Log;
import :LogManager;
import :Profiler;
import :RenderTarget;
import :RenderTargetListener;
import :Root;
import :StringConverter;
import :Timer;
import :Viewport;

import <algorithm>;
import <ctime>;
import <iomanip>;
import <ostream>;
import <string>;
import <utility>;

namespace Ogre {
class Camera;

    RenderTarget::RenderTarget()
         
    {
        mTimer = Root::getSingleton().getTimer();
        resetStatistics();
    }

    RenderTarget::~RenderTarget()
    {
        // make a copy of the list to avoid crashes, the viewport destructor change the list
        ViewportList vlist = mViewportList;
        
        // Delete viewports
        for (auto & i : vlist)
        {
            fireViewportRemoved(i.second);
            delete i.second;
        }

        //DepthBuffer keeps track of us, avoid a dangling pointer
        detachDepthBuffer();


        // Write closing message
        LogManager::getSingleton().stream(LML_TRIVIAL)
            << "Render Target '" << mName << "' "
            << "Average FPS: " << mStats.avgFPS << " "
            << "Best FPS: " << mStats.bestFPS << " "
            << "Worst FPS: " << mStats.worstFPS; 

    }

    auto RenderTarget::getName() const -> const String&
    {
        return mName;
    }


    void RenderTarget::getMetrics(unsigned int& width, unsigned int& height)
    {
        width = mWidth;
        height = mHeight;
    }

    auto RenderTarget::getWidth() const -> unsigned int
    {
        return mWidth;
    }
    auto RenderTarget::getHeight() const -> unsigned int
    {
        return mHeight;
    }
    //-----------------------------------------------------------------------
    void RenderTarget::setDepthBufferPool( uint16 poolId )
    {
        if( mDepthBufferPoolId != poolId )
        {
            mDepthBufferPoolId = poolId;
            detachDepthBuffer();
        }
    }
    //-----------------------------------------------------------------------
    auto RenderTarget::getDepthBufferPool() const -> uint16
    {
        return mDepthBufferPoolId;
    }
    //-----------------------------------------------------------------------
    auto RenderTarget::getDepthBuffer() const -> DepthBuffer*
    {
        return mDepthBuffer;
    }
    //-----------------------------------------------------------------------
    auto RenderTarget::attachDepthBuffer( DepthBuffer *depthBuffer ) -> bool
    {
        bool retVal = false;

        if( (retVal = depthBuffer->isCompatible( this )) )
        {
            detachDepthBuffer();
            mDepthBuffer = depthBuffer;
            mDepthBuffer->_notifyRenderTargetAttached( this );
        }

        return retVal;
    }
    //-----------------------------------------------------------------------
    void RenderTarget::detachDepthBuffer()
    {
        if( mDepthBuffer )
        {
            mDepthBuffer->_notifyRenderTargetDetached( this );
            mDepthBuffer = nullptr;
        }
    }
    //-----------------------------------------------------------------------
    void RenderTarget::_detachDepthBuffer()
    {
        mDepthBuffer = nullptr;
    }

    void RenderTarget::updateImpl()
    {
        _beginUpdate();
        _updateAutoUpdatedViewports(true);
        _endUpdate();
    }

    void RenderTarget::_beginUpdate()
    {
        // notify listeners (pre)
        firePreUpdate();

        mStats.triangleCount = 0;
        mStats.batchCount = 0;
    }

    void RenderTarget::_updateAutoUpdatedViewports(bool updateStatistics)
    {
        // Go through viewports in Z-order
        // Tell each to refresh
        auto it = mViewportList.begin();
        while (it != mViewportList.end())
        {
            Viewport* viewport = (*it).second;
            if(viewport->isAutoUpdated())
            {
                _updateViewport(viewport,updateStatistics);
            }
            ++it;
        }
    }

    void RenderTarget::_endUpdate()
    {
         // notify listeners (post)
        firePostUpdate();

        // Update statistics (always on top)
        updateStats();
    }

    void RenderTarget::_updateViewport(Viewport* viewport, bool updateStatistics)
    {
        assert(viewport->getTarget() == this &&
                "RenderTarget::_updateViewport the requested viewport is "
                "not bound to the rendertarget!");

        fireViewportPreUpdate(viewport);
        viewport->update();
        if(updateStatistics)
        {
            mStats.triangleCount += viewport->_getNumRenderedFaces();
            mStats.batchCount += viewport->_getNumRenderedBatches();
        }
        fireViewportPostUpdate(viewport);
    }

    void RenderTarget::_updateViewport(int zorder, bool updateStatistics)
    {
        auto it = mViewportList.find(zorder);
        if (it != mViewportList.end())
        {
            _updateViewport((*it).second,updateStatistics);
        }
        else
        {
            OGRE_EXCEPT(Exception::ERR_ITEM_NOT_FOUND,"No viewport with given zorder : "
                + StringConverter::toString(zorder), "RenderTarget::_updateViewport");
        }
    }

    auto RenderTarget::addViewport(Camera* cam, int ZOrder, float left, float top ,
        float width , float height) -> Viewport*
    {       
        // Check no existing viewport with this Z-order
        auto it = mViewportList.find(ZOrder);

        if (it != mViewportList.end())
        {
            StringStream str;
            str << "Can't create another viewport for "
                << mName << " with Z-order " << ZOrder
                << " because a viewport exists with this Z-order already.";
            OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS, str.str(), "RenderTarget::addViewport");
        }
        // Add viewport to list
        // Order based on Z-order
        auto* vp = new Viewport(cam, this, left, top, width, height, ZOrder);

        mViewportList.emplace(ZOrder, vp);

        fireViewportAdded(vp);

        return vp;
    }
    //-----------------------------------------------------------------------
    void RenderTarget::removeViewport(int ZOrder)
    {
        auto it = mViewportList.find(ZOrder);

        if (it != mViewportList.end())
        {
            fireViewportRemoved(it->second);
            delete (*it).second;
            mViewportList.erase(ZOrder);
        }
    }

    void RenderTarget::removeAllViewports()
    {
        // make a copy of the list to avoid crashes, the viewport destructor change the list
        ViewportList vlist = mViewportList;

        for (auto & it : vlist)
        {
            fireViewportRemoved(it.second);
            delete it.second;
        }

        mViewportList.clear();

    }

    void RenderTarget::resetStatistics()
    {
        mStats.avgFPS = 0.0;
        mStats.bestFPS = 0.0;
        mStats.lastFPS = 0.0;
        mStats.worstFPS = 999.0;
        mStats.triangleCount = 0;
        mStats.batchCount = 0;
        mStats.bestFrameTime = 999999;
        mStats.worstFrameTime = 0;
        mStats.vBlankMissCount = -1;

        mLastTime = mTimer->getMilliseconds();
        mLastSecond = mLastTime;
        mFrameCount = 0;
    }

    void RenderTarget::updateStats()
    {
        ++mFrameCount;
        unsigned long thisTime = mTimer->getMilliseconds();

        // check frame time
        unsigned long frameTime = thisTime - mLastTime ;
        mLastTime = thisTime ;

        mStats.bestFrameTime = std::min(mStats.bestFrameTime, frameTime);
        mStats.worstFrameTime = std::max(mStats.worstFrameTime, frameTime);

        // check if new second (update only once per second)
        if (thisTime - mLastSecond > 1000) 
        { 
            // new second - not 100% precise
            mStats.lastFPS = (float)mFrameCount / (float)(thisTime - mLastSecond) * 1000.0f;

            if (mStats.avgFPS == 0)
                mStats.avgFPS = mStats.lastFPS;
            else
                mStats.avgFPS = (mStats.avgFPS + mStats.lastFPS) / 2; // not strictly correct, but good enough

            mStats.bestFPS = std::max(mStats.bestFPS, mStats.lastFPS);
            mStats.worstFPS = std::min(mStats.worstFPS, mStats.lastFPS);

            mLastSecond = thisTime ;
            mFrameCount  = 0;

        }

    }

    void RenderTarget::getCustomAttribute(const String& name, void* pData)
    {
        OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS, "Attribute not found. " + name, " RenderTarget::getCustomAttribute");
    }
    //-----------------------------------------------------------------------
    void RenderTarget::addListener(RenderTargetListener* listener)
    {
        if (std::find(mListeners.begin(), mListeners.end(), listener) == mListeners.end())
            mListeners.push_back(listener);
    }
    //-----------------------------------------------------------------------
    void RenderTarget::insertListener(RenderTargetListener* listener, const unsigned int pos)
    {
        // if the position is larger than the list size we just set the listener at the end of the list
        if (pos > mListeners.size())
            mListeners.push_back(listener);
        else
            mListeners.insert(mListeners.begin() + pos, listener);
    }
    //-----------------------------------------------------------------------
    void RenderTarget::removeListener(RenderTargetListener* listener)
    {
        auto i = std::find(mListeners.begin(), mListeners.end(), listener);
        if (i != mListeners.end())
            mListeners.erase(i);
    }
    //-----------------------------------------------------------------------
    void RenderTarget::removeAllListeners()
    {
        mListeners.clear();
    }
    //-----------------------------------------------------------------------
    void RenderTarget::firePreUpdate()
    {
        RenderTargetEvent evt;
        evt.source = this;

        RenderTargetListenerList::iterator i, iend;
        i = mListeners.begin();
        iend = mListeners.end();
        for(; i != iend; ++i)
        {
            (*i)->preRenderTargetUpdate(evt);
        }


    }
    //-----------------------------------------------------------------------
    void RenderTarget::firePostUpdate()
    {
        RenderTargetEvent evt;
        evt.source = this;

        RenderTargetListenerList::iterator i, iend;
        i = mListeners.begin();
        iend = mListeners.end();
        for(; i != iend; ++i)
        {
            (*i)->postRenderTargetUpdate(evt);
        }
    }
    //-----------------------------------------------------------------------
    auto RenderTarget::getNumViewports() const -> unsigned short
    {
        return (unsigned short)mViewportList.size();

    }
    //-----------------------------------------------------------------------
    auto RenderTarget::getViewport(unsigned short index) -> Viewport*
    {
        assert (index < mViewportList.size() && "Index out of bounds");

        auto i = mViewportList.begin();
        while (index--)
            ++i;
        return i->second;
    }
    //-----------------------------------------------------------------------
    auto RenderTarget::getViewportByZOrder(int ZOrder) -> Viewport*
    {
        auto i = mViewportList.find(ZOrder);
        if(i == mViewportList.end())
        {
            OGRE_EXCEPT(Exception::ERR_ITEM_NOT_FOUND,"No viewport with given Z-order: "
                + StringConverter::toString(ZOrder), "RenderTarget::getViewportByZOrder");
        }
        return i->second;
    }
    //-----------------------------------------------------------------------
    auto RenderTarget::hasViewportWithZOrder(int ZOrder) -> bool
    {
        auto i = mViewportList.find(ZOrder);
        return i != mViewportList.end();
    }
    //-----------------------------------------------------------------------
    auto RenderTarget::isActive() const -> bool
    {
        return mActive;
    }
    //-----------------------------------------------------------------------
    void RenderTarget::setActive( bool state )
    {
        mActive = state;
    }
    //-----------------------------------------------------------------------
    void RenderTarget::fireViewportPreUpdate(Viewport* vp)
    {
        RenderTargetViewportEvent evt;
        evt.source = vp;

        RenderTargetListenerList::iterator i, iend;
        i = mListeners.begin();
        iend = mListeners.end();
        for(; i != iend; ++i)
        {
            (*i)->preViewportUpdate(evt);
        }
    }
    //-----------------------------------------------------------------------
    void RenderTarget::fireViewportPostUpdate(Viewport* vp)
    {
        RenderTargetViewportEvent evt;
        evt.source = vp;

        RenderTargetListenerList::iterator i, iend;
        i = mListeners.begin();
        iend = mListeners.end();
        for(; i != iend; ++i)
        {
            (*i)->postViewportUpdate(evt);
        }
    }
    //-----------------------------------------------------------------------
    void RenderTarget::fireViewportAdded(Viewport* vp)
    {
        RenderTargetViewportEvent evt;
        evt.source = vp;

        RenderTargetListenerList::iterator i, iend;
        i = mListeners.begin();
        iend = mListeners.end();
        for(; i != iend; ++i)
        {
            (*i)->viewportAdded(evt);
        }
    }
    //-----------------------------------------------------------------------
    void RenderTarget::fireViewportRemoved(Viewport* vp)
    {
        RenderTargetViewportEvent evt;
        evt.source = vp;

        // Make a temp copy of the listeners
        // some will want to remove themselves as listeners when they get this
        RenderTargetListenerList tempList = mListeners;

        RenderTargetListenerList::iterator i, iend;
        i = tempList.begin();
        iend = tempList.end();
        for(; i != iend; ++i)
        {
            (*i)->viewportRemoved(evt);
        }
    }
    //-----------------------------------------------------------------------
    auto RenderTarget::writeContentsToTimestampedFile(const String& filenamePrefix, const String& filenameSuffix) -> String
    {
        struct tm *pTime;
        time_t ctTime; time(&ctTime);
        pTime = localtime( &ctTime );
        // use ISO 8601 order
        StringStream oss;
        oss << filenamePrefix
            << std::put_time(pTime, "%Y%m%d_%H%M%S")
            << std::setw(3) << std::setfill('0') << (mTimer->getMilliseconds() % 1000)
            << filenameSuffix;
        String filename = oss.str();
        writeContentsToFile(filename);
        return filename;

    }
    //-----------------------------------------------------------------------
    void RenderTarget::writeContentsToFile(const String& filename)
    {
        Image img(suggestPixelFormat(), mWidth, mHeight);

        PixelBox pb = img.getPixelBox();
        copyContentsToMemory(pb, pb);

        img.save(filename);
    }
    //-----------------------------------------------------------------------
    void RenderTarget::_notifyCameraRemoved(const Camera* cam)
    {
        ViewportList::iterator i, iend;
        iend = mViewportList.end();
        for (i = mViewportList.begin(); i != iend; ++i)
        {
            Viewport* v = i->second;
            if (v->getCamera() == cam)
            {
                // disable camera link
                v->setCamera(nullptr);
            }
        }
    }
    //-----------------------------------------------------------------------
    void RenderTarget::setAutoUpdated(bool autoup)
    {
        mAutoUpdate = autoup;
    }
    //-----------------------------------------------------------------------
    auto RenderTarget::isAutoUpdated() const -> bool
    {
        return mAutoUpdate;
    }
    //-----------------------------------------------------------------------
    auto RenderTarget::isPrimary() const -> bool
    {
        // RenderWindow will override and return true for the primary window
        return false;
    }  
	//-----------------------------------------------------------------------
    auto RenderTarget::isStereoEnabled() const -> bool
    {
        return mStereoEnabled;
    }
    //-----------------------------------------------------------------------
    void RenderTarget::update(bool swap)
    {
        Ogre::Profiler::getSingleton().beginGPUEvent(getName());
        // call implementation
        updateImpl();


        if (swap)
        {
            // Swap buffers
            swapBuffers();
        }
        Ogre::Profiler::getSingleton().endGPUEvent(getName());
    }
    

}        
