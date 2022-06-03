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
#ifndef OGRE_SAMPLES_SAMPLECONTEXT_H
#define OGRE_SAMPLES_SAMPLECONTEXT_H

#include "OgreApplicationContext.hpp"

#include "OgrePlugin.hpp"
#include "OgreProfiler.hpp"
#include "OgreRenderWindow.hpp"
#include "Sample.hpp"

namespace OgreBites
{
    /*=============================================================================
    | Base class responsible for setting up a common context for samples.
    | May be subclassed for specific sample types (not specific samples).
    | Allows one sample to run at a time, while maintaining a sample queue.
    =============================================================================*/
    class SampleContext : public ApplicationContext, public InputListener
    {
    public:
        Ogre::RenderWindow* mWindow{nullptr};

        SampleContext(const Ogre::String& appName = /*OGRE_VERSION_NAME*/"Tsathoggua")
        : ApplicationContext(appName) 
        {
            mCurrentSample = nullptr;
            mSamplePaused = false;
            mLastRun = false;
            mLastSample = nullptr;
        }

        virtual Sample* getCurrentSample() noexcept
        {
            return mCurrentSample;
        }

        /*-----------------------------------------------------------------------------
        | Quits the current sample and starts a new one.
        -----------------------------------------------------------------------------*/
        virtual void runSample(Sample* s)
        {
            if (mCurrentSample)
            {
                mCurrentSample->_shutdown();    // quit current sample
                mSamplePaused = false;          // don't pause the next sample
            }

            mWindow->removeAllViewports();                  // wipe viewports
            mWindow->resetStatistics();

            if (s)
            {
                // retrieve sample's required plugins and currently installed plugins
                Ogre::Root::PluginInstanceList ip = mRoot->getInstalledPlugins();
                Ogre::StringVector rp = s->getRequiredPlugins();

                for (auto j = rp.begin(); j != rp.end(); j++)
                {
                    bool found = false;
                    // try to find the required plugin in the current installed plugins
                    for (auto k = ip.begin(); k != ip.end(); k++)
                    {
                        if ((*k)->getName() == *j)
                        {
                            found = true;
                            break;
                        }
                    }
                    if (!found)  // throw an exception if a plugin is not found
                    {
                        OGRE_EXCEPT(Ogre::Exception::ERR_NOT_IMPLEMENTED, "Sample requires plugin: " + *j);
                    }
                }

                // test system capabilities against sample requirements
                s->testCapabilities(mRoot->getRenderSystem()->getCapabilities());
                s->setShaderGenerator(mShaderGenerator);
                s->_setup(mWindow, mFSLayer.get(), mOverlaySystem);   // start new sample
            }

            mCurrentSample = s;
        }

        /*-----------------------------------------------------------------------------
        | This function encapsulates the entire lifetime of the context.
        -----------------------------------------------------------------------------*/
        virtual void go(Sample* initialSample = nullptr, ulong frameCount = -1)
        {
            while (!mLastRun)
            {
                mLastRun = true;  // assume this is our last run

                initApp(frameCount);

                // restore the last sample if there was one or, if not, start initial sample
                if (!mFirstRun) recoverLastSample();
                else if (initialSample) runSample(initialSample);

                loadStartUpSample();
        
                if (mRoot->getRenderSystem() != nullptr)
                {
                    mRoot->startRendering();    // start the render loop
                }

                closeApp();

                mFirstRun = false;
            }
        }

        virtual void loadStartUpSample() {}

        bool isCurrentSamplePaused() noexcept
        {
            return !mCurrentSample || mSamplePaused;
        }

        virtual void pauseCurrentSample()
        {
            if (!isCurrentSamplePaused())
            {
                mSamplePaused = true;
                mCurrentSample->paused();
            }
        }

        virtual void unpauseCurrentSample()
        {
            if (mCurrentSample && mSamplePaused)
            {
                mSamplePaused = false;
                mCurrentSample->unpaused();
            }
        }
            
        /*-----------------------------------------------------------------------------
        | Processes frame started events.
        -----------------------------------------------------------------------------*/
        bool frameStarted(const Ogre::FrameEvent& evt) override
        {
            pollEvents();

            // manually call sample callback to ensure correct order
            return !isCurrentSamplePaused() ? mCurrentSample->frameStarted(evt) : true;
        }
            
        /*-----------------------------------------------------------------------------
        | Processes rendering queued events.
        -----------------------------------------------------------------------------*/
        bool frameRenderingQueued(const Ogre::FrameEvent& evt) override
        {
            // manually call sample callback to ensure correct order
            return !isCurrentSamplePaused() ? mCurrentSample->frameRenderingQueued(evt) : true;
        }
            
        /*-----------------------------------------------------------------------------
        | Processes frame ended events.
        -----------------------------------------------------------------------------*/
        bool frameEnded(const Ogre::FrameEvent& evt) noexcept override
        {
            // manually call sample callback to ensure correct order
            if (mCurrentSample && !mSamplePaused && !mCurrentSample->frameEnded(evt)) return false;
            // quit if window was closed
            if (mWindow->isClosed()) return false;
            // go into idle mode if current sample has ended
            if (mCurrentSample && mCurrentSample->isDone()) runSample(nullptr);

            return true;
        }

        /*-----------------------------------------------------------------------------
        | Processes window size change event. Adjusts mouse's region to match that
        | of the window. You could also override this method to prevent resizing.
        -----------------------------------------------------------------------------*/
        void windowResized(Ogre::RenderWindow* rw) override
        {
            // manually call sample callback to ensure correct order
            if (!isCurrentSamplePaused()) mCurrentSample->windowResized(rw);
        }

        // window event callbacks which manually call their respective sample callbacks to ensure correct order

        void windowMoved(Ogre::RenderWindow* rw) override
        {
            if (!isCurrentSamplePaused()) mCurrentSample->windowMoved(rw);
        }

        bool windowClosing(Ogre::RenderWindow* rw) noexcept override
        {
            if (!isCurrentSamplePaused()) return mCurrentSample->windowClosing(rw);
            return true;
        }

        void windowClosed(Ogre::RenderWindow* rw) override
        {
            if (!isCurrentSamplePaused()) mCurrentSample->windowClosed(rw);
        }

        void windowFocusChange(Ogre::RenderWindow* rw) override
        {
            if (!isCurrentSamplePaused()) mCurrentSample->windowFocusChange(rw);
        }

        // keyboard and mouse callbacks which manually call their respective sample callbacks to ensure correct order

        bool keyPressed(const KeyDownEvent& evt) noexcept override
        {
            // Ignore repeated signals from key being held down.
            if (evt.repeat) return true;

            if (!isCurrentSamplePaused()) return mCurrentSample->keyPressed(evt);
            return true;
        }

        bool keyReleased(const KeyUpEvent& evt) noexcept override
        {
            if (!isCurrentSamplePaused()) return mCurrentSample->keyReleased(evt);
            return true;
        }

        bool touchMoved(const TouchFingerMotionEvent& evt) noexcept override
        {
            if (!isCurrentSamplePaused())
                return mCurrentSample->touchMoved(evt);
            return true;
        }

        bool mouseMoved(const MouseMotionEvent& evt) noexcept override
        {
            if (!isCurrentSamplePaused())
                return mCurrentSample->mouseMoved(evt);
            return true;
        }

        bool touchPressed(const TouchFingerDownEvent& evt) noexcept override
        {
            if (!isCurrentSamplePaused())
                return mCurrentSample->touchPressed(evt);
            return true;
        }

        bool mousePressed(const MouseButtonDownEvent& evt) noexcept override
        {
            if (!isCurrentSamplePaused())
                return mCurrentSample->mousePressed(evt);
            return true;
        }

        bool touchReleased(const TouchFingerUpEvent& evt) noexcept override
        {
            if (!isCurrentSamplePaused())
                return mCurrentSample->touchReleased(evt);
            return true;
        }

        bool mouseReleased(const MouseButtonUpEvent& evt) noexcept override
        {
            if (!isCurrentSamplePaused())
                return mCurrentSample->mouseReleased(evt);
            return true;
        }

        bool mouseWheelRolled(const MouseWheelEvent& evt) noexcept override
        {
            if (!isCurrentSamplePaused())
                return mCurrentSample->mouseWheelRolled(evt);
            return true;
        }

        bool textInput (const TextInputEvent& evt) noexcept override
        {
            if (!isCurrentSamplePaused ())
                return mCurrentSample->textInput (evt);
            return true;
        }

        bool isFirstRun() noexcept { return mFirstRun; }
        void setFirstRun(bool flag) { mFirstRun = flag; }
        bool isLastRun() noexcept { return mLastRun; }
        void setLastRun(bool flag) { mLastRun = flag; }
    protected:
        /*-----------------------------------------------------------------------------
        | Reconfigures the context. Attempts to preserve the current sample state.
        -----------------------------------------------------------------------------*/
        void reconfigure(const Ogre::String& renderer, Ogre::NameValuePairList& options) override
        {
            // save current sample state
            mLastSample = mCurrentSample;
            if (mCurrentSample) mCurrentSample->saveState(mLastSampleState);

            mLastRun = false;             // we want to go again with the new settings
            ApplicationContext::reconfigure(renderer, options);
        }

        /*-----------------------------------------------------------------------------
        | Recovers the last sample after a reset. You can override in the case that
        | the last sample is destroyed in the process of resetting, and you have to
        | recover it through another means.
        -----------------------------------------------------------------------------*/
        virtual void recoverLastSample()
        {
            runSample(mLastSample);
            mLastSample->restoreState(mLastSampleState);
            mLastSample = nullptr;
            mLastSampleState.clear();
        }

        /*-----------------------------------------------------------------------------
        | Cleans up and shuts down the context.
        -----------------------------------------------------------------------------*/
        void shutdown() override
        {
            if (mCurrentSample)
            {
                mCurrentSample->_shutdown();
                mCurrentSample = nullptr;
            }

            ApplicationContext::shutdown();
        }
        
        Sample* mCurrentSample;         // The active sample (0 if none is active)
        bool mSamplePaused;             // whether current sample is paused
        bool mLastRun;                  // whether or not this is the final run
        Sample* mLastSample;            // last sample run before reconfiguration
        Ogre::NameValuePairList mLastSampleState;     // state of last sample
    };
}

#endif
