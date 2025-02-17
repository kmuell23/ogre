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
export module Ogre.Core:Log;

export import :Common;
export import :MemoryAllocatorConfig;
export import :Prerequisites;

export import <algorithm>;
export import <fstream>;
export import <string>;
export import <vector>;

export
namespace Ogre {

    /** \addtogroup Core
    *  @{
    */
    /** \addtogroup General
    *  @{
    */

    /// @deprecated use LogMessageLevel instead
    enum class LoggingLevel
    {
        LL_LOW = 1,
        LL_NORMAL = 2,
        LL_BOREME = 3
    };

    /** The importance of a logged message.
    */
    enum class LogMessageLevel
    {
        Trivial = 1,
        Normal = 2,
        Warning = 3,
        Critical = 4
    };

    /** @remarks Pure Abstract class, derive this class and register to the Log to listen to log messages */
    class LogListener
    {
    public:
        virtual ~LogListener() = default;

        /**
        @remarks
            This is called whenever the log receives a message and is about to write it out
        @param message
            The message to be logged
        @param lml
            The message level the log is using
        @param maskDebug
            If we are printing to the console or not
        @param logName
            The name of this log (so you can have several listeners for different logs, and identify them)
        @param skipThisMessage
            If set to true by the messageLogged() implementation message will not be logged
        */
        virtual void messageLogged( std::string_view message, LogMessageLevel lml, bool maskDebug, std::string_view logName, bool& skipThisMessage ) = 0;
    };


    /**
        Log class for writing debug/log data to files.

        You can control the default log level through the `OGRE_MIN_LOGLEVEL` environment variable.
        Here, the value 1 corresponds to #LogMessageLevel::Trivial etc.
        @note Should not be used directly, but trough the LogManager class.
    */
    class Log : public LogAlloc
    {
    private:
        std::ofstream   mLog;
        LogMessageLevel mLogLevel{LogMessageLevel::Normal};
        bool            mDebugOut;
        bool            mSuppressFile;
        bool            mTimeStamp{true};
        String          mLogName;
        bool            mTermHasColours{false};

        using mtLogListener = std::vector<LogListener *>;
        mtLogListener mListeners;
    public:

        class Stream;

        /**
        @remarks
            Usual constructor - called by LogManager.
        */
        Log( std::string_view name, bool debugOutput = true, bool suppressFileOutput = false);

        /**
        @remarks
        Default destructor.
        */
        ~Log();

        /// Return the name of the log
        auto getName() const noexcept -> std::string_view { return mLogName; }
        /// Get whether debug output is enabled for this log
        auto isDebugOutputEnabled() const noexcept -> bool { return mDebugOut; }
        /// Get whether file output is suppressed for this log
        auto isFileOutputSuppressed() const noexcept -> bool { return mSuppressFile; }
        /// Get whether time stamps are printed for this log
        auto isTimeStampEnabled() const noexcept -> bool { return mTimeStamp; }

        /** Log a message to the debugger and to log file (the default is
            "<code>OGRE.log</code>"),
        */
        void logMessage( std::string_view message, LogMessageLevel lml = LogMessageLevel::Normal, bool maskDebug = false );

        /** Get a stream object targeting this log. */
        auto stream(LogMessageLevel lml = LogMessageLevel::Normal, bool maskDebug = false) -> Stream;

        /**
        @remarks
            Enable or disable outputting log messages to the debugger.
        */
        void setDebugOutputEnabled(bool debugOutput);

        /// set the minimal #LogMessageLevel for a message to be logged
        void setMinLogLevel(LogMessageLevel lml);
        /**
        @remarks
            Enable or disable time stamps.
        */
        void setTimeStampEnabled(bool timeStamp);
        /** Gets the level of the log detail.
        */
        auto getMinLogLevel() const noexcept -> LogMessageLevel { return mLogLevel; }
        /**
        @remarks
            Register a listener to this log
        @param listener
            A valid listener derived class
        */
        void addListener(LogListener* listener);

        /**
        @remarks
            Unregister a listener from this log
        @param listener
            A valid listener derived class
        */
        void removeListener(LogListener* listener);

        /** Stream object which targets a log.
        @remarks
            A stream logger object makes it simpler to send various things to 
            a log. You can just use the operator<< implementation to stream 
            anything to the log, which is cached until a Stream::Flush is
            encountered, or the stream itself is destroyed, at which point the 
            cached contents are sent to the underlying log. You can use Log::stream()
            directly without assigning it to a local variable and as soon as the
            streaming is finished, the object will be destroyed and the message
            logged.
        @par
            You can stream control operations to this object too, such as 
            std::setw() and std::setfill() to control formatting.
        @note
            Each Stream object is not thread safe, so do not pass it between
            threads. Multiple threads can hold their own Stream instances pointing
            at the same Log though and that is threadsafe.
        */
        class Stream
        {
        private:
            Log* mTarget;
            LogMessageLevel mLevel;
            bool mMaskDebug;
            using BaseStream = StringStream;
            BaseStream mCache;

        public:

            /// Simple type to indicate a flush of the stream to the log
            struct Flush {};

            Stream(Log* target, LogMessageLevel lml, bool maskDebug)
                :mTarget(target), mLevel(lml), mMaskDebug(maskDebug)
            {

            }
            // move constructor
            Stream(Stream&& rhs) = default;

            ~Stream()
            {
                // flush on destroy
                if (mCache.tellp() > 0)
                {
                    mTarget->logMessage(mCache.str(), mLevel, mMaskDebug);
                }
            }

            template <typename T>
            auto operator<< (const T& v) -> Stream&
            {
                if constexpr(std::is_enum_v<T>)
                    mCache << std::to_underlying(v);
                else
                    mCache << v;
                return *this;
            }

            auto operator<< (const Flush& v) -> Stream&
            {
                                (void)v;
                mTarget->logMessage(mCache.str(), mLevel, mMaskDebug);
                mCache.str("");
                return *this;
            }


        };

    };
    /** @} */
    /** @} */
}
