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

#include <cstdlib>
#include <ctime>

module Ogre.Core;

import :Log;
import :Prerequisites;
import :StringConverter;

import <algorithm>;
import <iomanip>;
import <iostream>;
import <string>;
import <vector>;

// LogMessageLevel + LoggingLevel > OGRE_LOG_THRESHOLD = message logged
enum {
OGRE_LOG_THRESHOLD = 4
};

namespace {
    const char* RED = "\x1b[31;1m";
    const char* YELLOW = "\x1b[33;1m";
    const char* RESET = "\x1b[0m";
}

namespace Ogre
{
    //-----------------------------------------------------------------------
    Log::Log( std::string_view name, bool debuggerOutput, bool suppressFile ) : 
         mDebugOut(debuggerOutput),
        mSuppressFile(suppressFile),  mLogName(name) 
    {
        if (!mSuppressFile)
        {
            mLog.open(name.data());
        }

        char* val = getenv("OGRE_MIN_LOGLEVEL");
        int min_lml;
        if(val && StringConverter::parse(val, min_lml))
            setMinLogLevel(LogMessageLevel(min_lml));

        if(mDebugOut)
        {
            val = getenv("TERM");
            mTermHasColours = val && String(val).find("xterm") != String::npos;
        }
    }
    //-----------------------------------------------------------------------
    Log::~Log()
    {
        if (!mSuppressFile)
        {
            mLog.close();
        }
    }

    //-----------------------------------------------------------------------
    void Log::logMessage( std::string_view message, LogMessageLevel lml, bool maskDebug )
    {
        if (lml >= mLogLevel)
        {
            bool skipThisMessage = false;
            for(auto & mListener : mListeners)
                mListener->messageLogged( message, lml, maskDebug, mLogName, skipThisMessage);
            
            if (!skipThisMessage)
            {
                if (mDebugOut && !maskDebug)
                {
                    std::ostream& os = int(lml) >= int(LogMessageLevel::Warning) ? std::cerr : std::cout;

                    if(mTermHasColours) {
                        if(lml == LogMessageLevel::Warning)
                            os << YELLOW;
                        if(lml == LogMessageLevel::Critical)
                            os << RED;
                    }

                    os << message;

                    if(mTermHasColours) {
                        os << RESET;
                    }

                    os << std::endl;
                }

                // Write time into log
                if (!mSuppressFile)
                {
                    if (mTimeStamp)
                    {
                        struct tm *pTime;
                        time_t ctTime; time(&ctTime);
                        pTime = localtime( &ctTime );
                        mLog << std::setw(2) << std::setfill('0') << pTime->tm_hour
                            << ":" << std::setw(2) << std::setfill('0') << pTime->tm_min
                            << ":" << std::setw(2) << std::setfill('0') << pTime->tm_sec
                            << ": ";
                    }
                    mLog << message << std::endl;

                    // Flush stcmdream to ensure it is written (incase of a crash, we need log to be up to date)
                    mLog.flush();
                }
            }
        }
    }
    
    //-----------------------------------------------------------------------
    void Log::setTimeStampEnabled(bool timeStamp)
    {
        mTimeStamp = timeStamp;
    }

    //-----------------------------------------------------------------------
    void Log::setDebugOutputEnabled(bool debugOutput)
    {
        mDebugOut = debugOutput;
    }

    void Log::setMinLogLevel(LogMessageLevel lml)
    {
        mLogLevel = lml;
    }

    //-----------------------------------------------------------------------
    void Log::addListener(LogListener* listener)
    {
        if (std::ranges::find(mListeners, listener) == mListeners.end())
            mListeners.push_back(listener);
    }

    //-----------------------------------------------------------------------
    void Log::removeListener(LogListener* listener)
    {
        auto i = std::ranges::find(mListeners, listener);
        if (i != mListeners.end())
            mListeners.erase(i);
    }
    //---------------------------------------------------------------------
    auto Log::stream(LogMessageLevel lml, bool maskDebug) -> Log::Stream 
    {
        return {this, lml, maskDebug};

    }
}
