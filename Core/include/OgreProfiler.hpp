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
/*

    Although the code is original, many of the ideas for the profiler were borrowed from 
"Real-Time In-Game Profiling" by Steve Rabin which can be found in Game Programming
Gems 1.

    This code can easily be adapted to your own non-Ogre project. The only code that is 
Ogre-dependent is in the visualization/logging routines and the use of the Timer class.

    Enjoy!

*/
export module Ogre.Core:Profiler;

export import :MemoryAllocatorConfig;
export import :Platform;
export import :Prerequisites;
export import :Singleton;

export import <algorithm>;
export import <map>;
export import <set>;
export import <string>;
export import <vector>;

export
namespace Ogre {
    class Timer;

    /** \addtogroup Core
    *  @{
    */
    /** \addtogroup General
    *  @{
    */
    /** List of reserved profiling masks
    */
    enum class ProfileGroupMask : uint32
    {
        /// User default profile
        USER_DEFAULT = 0x00000001,
        /// All in-built Ogre profiling will match this mask
        ALL = 0xFF000000,
        /// General processing
        GENERAL = 0x80000000,
        /// Culling
        CULLING = 0x40000000,
        /// Rendering
        RENDERING = 0x20000000
    };

    auto constexpr operator not(ProfileGroupMask value) -> bool
    {
        return not std::to_underlying(value);
    }

    auto constexpr operator bitand(ProfileGroupMask left, ProfileGroupMask right) -> ProfileGroupMask
    {
        return static_cast<ProfileGroupMask>
        (   std::to_underlying(left)
        bitand
            std::to_underlying(right)
        );
    }

    /** Represents the total timing information of a profile
        since profiles can be called more than once each frame
    */
    struct ProfileFrame 
    {

        /// The total amout of CPU clocks this profile has taken this frame
        ulong   frameClocks;

        /// The number of times this profile was called this frame
        uint    calls;

        /// The hierarchical level of this profile, 0 being the main loop
        uint    hierarchicalLvl;

    };

    /// Represents a history of each profile during the duration of the app
    struct ProfileHistory 
    {
        /// The current percentage of frame time this profile has taken
        long double currentClocksPercent;
        /// The maximum percentage of frame time this profile has taken
        long double maxClocksPercent;
        /// The minimum percentage of frame time this profile has taken
        long double minClocksPercent;
        /// The total percentage of frame time this profile has taken
        long double totalClocksPercent;

        /// The current frame time this profile has taken in microseconds
        ulong    currentClocks;
        /// The maximum frame time this profile has taken in microseconds
        ulong    maxClocks;
        /// The minimum frame time this profile has taken in microseconds
        ulong    minClocks;


        /// The total frame time this profile has taken in microseconds
        ulong    totalClocks;
        /// The total sum of all squares of the time taken in microseconds.
        ulong    sumOfSquareClocks;

        /// The total number of times this profile was called
        /// (used to calculate average)
        ulong   totalCalls;

        /// The number of times this profile has been called each frame
        uint    numCallsThisFrame;

        /// The hierarchical level of this profile, 0 being the root profile
        uint    hierarchicalLvl;

        [[nodiscard]] auto StandardDeviationMilliseconds() const -> long double;
    };

    /// Represents an individual profile call
    class ProfileInstance : public ProfilerAlloc
    {
        friend class Profiler;
    public:
        ProfileInstance();

        using ProfileChildren = std::map<std::string_view, ::std::unique_ptr<ProfileInstance>>;

        void logResults();
        void reset();

        inline auto watchForMax() -> bool { return history.currentClocksPercent == history.maxClocksPercent; }
        inline auto watchForMin() -> bool { return history.currentClocksPercent == history.minClocksPercent; }
        inline auto watchForLimit(long double limit, bool greaterThan = true) -> bool
        {
            if (greaterThan)
                return history.currentClocksPercent > limit;
            else
                return history.currentClocksPercent < limit;
        }

        auto watchForMax(std::string_view profileName) -> bool;
        auto watchForMin(std::string_view profileName) -> bool;
        auto watchForLimit(std::string_view profileName, long double limit, bool greaterThan = true) -> bool;
                                
        /// The name of the profile
        String          name;

        /// The name of the parent, null if root
        ProfileInstance* parent{nullptr};

        ProfileChildren children;

        ProfileFrame frame;
        ulong frameNumber{0};

        ProfileHistory history;

        /// The time this profile was started
        ulong           currentClock;

        /// Represents the total time of all child profiles to subtract
        /// from this profile
        ulong           accumClocks{0};

        /// The hierarchical level of this profile, 0 being the root profile
        uint            hierarchicalLvl{0};
    };

    /** ProfileSessionListener should be used to visualize profile results.
        Concrete impl. could be done using Overlay's but its not limited to 
        them you can also create a custom listener which sends the profile
        informtaion over a network.
    */
    class ProfileSessionListener
    {
    public:
        virtual ~ProfileSessionListener() = default;

        /// Create the internal resources
        virtual void initializeSession() = 0;

        /// All internal resources should be deleted here
        virtual void finializeSession() = 0;

        /** If the profiler disables this listener then it
            should hide its panels (if any exists) or stop
            sending data over the network
        */
        virtual void changeEnableState(bool enabled) {}; 
        
        /// Here we get the real profiling information which we can use 
        virtual void displayResults(const ProfileInstance& instance, ulong maxTotalFrameClocks) {};
    };

    /** The profiler allows you to measure the performance of your code
        @remarks
            Do not create profiles directly from this unless you want a profile to last
            outside of its scope (i.e. the main game loop). For most cases, use the macro
            OgreProfile(name) and braces to limit the scope. You must enable the Profile
            before you can used it with setEnabled(true). If you want to disable profiling
            in Ogre, simply set the macro OGRE_PROFILING to 0.
        @author Amit Mathew (amitmathew (at) yahoo (dot) com)
        @todo resolve artificial cap on number of profiles displayed
        @todo fix display ordering of profiles not called every frame
    */
    class Profiler :
        public Singleton<Profiler>,
        public ProfilerAlloc
    {
        public:
            Profiler();
            ~Profiler();

            /** Sets the timer for the profiler */
            void setTimer(Timer* t);

            /** Retrieves the timer for the profiler */
            auto getTimer() noexcept -> Timer*;

            /** Begins a profile
            @remarks 
                Use the macro OgreProfileBegin(name) instead of calling this directly 
                so that profiling can be ignored in the release version of your app. 
            @remarks 
                You only use the macro (or this) if you want a profile to last outside
                of its scope (i.e. the main game loop). If you use this function, make sure you 
                use a corresponding OgreProfileEnd(name). Usually you would use the macro 
                OgreProfile(name). This function will be ignored for a profile that has been 
                disabled or if the profiler is disabled.
            @param profileName Must be unique and must not be an empty string
            @param groupID A profile group identifier, which can allow you to mask profiles
            */
            void beginProfile(std::string_view profileName, ProfileGroupMask groupID = ProfileGroupMask::USER_DEFAULT);

            /** Ends a profile
            @remarks 
                Use the macro OgreProfileEnd(name) instead of calling this directly so that
                profiling can be ignored in the release version of your app.
            @remarks
                This function is usually not called directly unless you want a profile to
                last outside of its scope. In most cases, using the macro OgreProfile(name) 
                which will call this function automatically when it goes out of scope. Make 
                sure the name of this profile matches its corresponding beginProfile name. 
                This function will be ignored for a profile that has been disabled or if the
                profiler is disabled.
            @param profileName Must be unique and must not be an empty string
            @param groupID A profile group identifier, which can allow you to mask profiles
            */
            void endProfile(std::string_view profileName, ProfileGroupMask groupID = ProfileGroupMask::USER_DEFAULT);

            /** Mark the beginning of a GPU event group
             @remarks Can be safely called in the middle of the profile.
             */
            void beginGPUEvent(std::string_view event);

            /** Mark the end of a GPU event group
             @remarks Can be safely called in the middle of the profile.
             */
            void endGPUEvent(std::string_view event);

            /** Mark a specific, ungrouped, GPU event
             @remarks Can be safely called in the middle of the profile.
             */
            void markGPUEvent(std::string_view event);

            /** Sets whether this profiler is enabled. Only takes effect after the
                the frame has ended.
                @remarks When this is called the first time with the parameter true,
                it initializes the GUI for the Profiler
            */
            void setEnabled(bool enabled);

            /** Gets whether this profiler is enabled */
            [[nodiscard]] auto getEnabled() const noexcept -> bool;

            /** Enables a previously disabled profile 
            @remarks Can be safely called in the middle of the profile.
            */
            void enableProfile(std::string_view profileName);

            /** Disables a profile
            @remarks Can be safely called in the middle of the profile.
            */
            void disableProfile(std::string_view profileName);

            /** Set the mask which all profiles must pass to be enabled. 
            */
            void setProfileGroupMask(ProfileGroupMask mask) { mProfileMask = mask; }
            /** Get the mask which all profiles must pass to be enabled. 
            */
            [[nodiscard]] auto getProfileGroupMask() const noexcept -> ProfileGroupMask { return mProfileMask; }

            /** Returns true if the specified profile reaches a new frame time maximum
            @remarks If this is called during a frame, it will be reading the results
            from the previous frame. Therefore, it is best to use this after the frame
            has ended.
            */
            auto watchForMax(std::string_view profileName) -> bool;

            /** Returns true if the specified profile reaches a new frame time minimum
            @remarks If this is called during a frame, it will be reading the results
            from the previous frame. Therefore, it is best to use this after the frame
            has ended.
            */
            auto watchForMin(std::string_view profileName) -> bool;

            /** Returns true if the specified profile goes over or under the given limit
                frame time
            @remarks If this is called during a frame, it will be reading the results
            from the previous frame. Therefore, it is best to use this after the frame
            has ended.
            @param profileName Must be unique and must not be an empty string
            @param limit A number between 0 and 1 representing the percentage of frame time
            @param greaterThan If true, this will return whether the limit is exceeded. Otherwise,
            it will return if the frame time has gone under this limit.
            */
            auto watchForLimit(std::string_view profileName, long double limit, bool greaterThan = true) -> bool;

            /** Outputs current profile statistics to the log */
            void logResults();

            /** Clears the profiler statistics */
            void reset();

            /** Sets the Profiler so the display of results are updated every n frames*/
            void setUpdateDisplayFrequency(uint freq);

            /** Gets the frequency that the Profiler display is updated */
            [[nodiscard]] auto getUpdateDisplayFrequency() const noexcept -> uint;

            /**
            @remarks
                Register a ProfileSessionListener from the Profiler
            @param listener
                A valid listener derived class
            */
            void addListener(ProfileSessionListener* listener);

            /**
            @remarks
                Unregister a ProfileSessionListener from the Profiler
            @param listener
                A valid listener derived class
            */
            void removeListener(ProfileSessionListener* listener);

            /// @copydoc Singleton::getSingleton()
            static auto getSingleton() noexcept -> Profiler&;
            /// @copydoc Singleton::getSingleton()
            static auto getSingletonPtr() noexcept -> Profiler*;

            [[nodiscard]] auto getCurrentCalls() const noexcept -> uint
            {
                return mCurrent->history.totalCalls;
            }
        private:
            friend class ProfileInstance;

            using TProfileSessionListener = std::vector<ProfileSessionListener *>;
            TProfileSessionListener mListeners;

            /** Initializes the profiler's GUI elements */
            void initialize();

            void displayResults();

            /** Processes frame stats for all of the mRoot's children */
            void processFrameStats();
            /** Processes specific ProfileInstance and it's children recursively.*/
            void processFrameStats(ProfileInstance* instance, ulong& maxFrameClocks);

            /** Handles a change of the profiler's enabled state*/
            void changeEnableState();

            // lol. Uses typedef; put's original container type in name.
            using DisabledProfileMap = std::set<std::string_view>;
            using ProfileChildren = ProfileInstance::ProfileChildren;

            ProfileInstance* mCurrent;
            ProfileInstance* mLast{nullptr};
            ProfileInstance mRoot;

            /// Holds the names of disabled profiles
            DisabledProfileMap mDisabledProfiles;

            /// Whether the GUI elements have been initialized
            bool mInitialized{false};

            /// The number of frames that must elapse before the current
            /// frame display is updated
            uint mUpdateDisplayFrequency{10};

            /// The number of elapsed frame, used with mUpdateDisplayFrequency
            uint mCurrentFrame{0};

            /// The timer used for profiling
            Timer* mTimer{nullptr};

            /// The total time each frame takes
            ulong mTotalFrameClocks{0};

            /// Whether this profiler is enabled
            bool mEnabled{true};

            /// Keeps track of the new enabled/disabled state that the user has requested
            /// which will be applied after the frame ends
            bool mNewEnableState{true};

            /// Mask to decide whether a type of profile is enabled or not
            ProfileGroupMask mProfileMask{0xFFFFFFFF};

            /// The max frame time recorded
            ulong mMaxTotalFrameClocks{0};

            /// Rolling average of clocks
            long double mAverageFrameClocks{0};
            bool mResetExtents{false};

    }; // end class

    /** An individual profile that will be processed by the Profiler
        @remarks
            Use the macro OgreProfile(name) instead of instantiating this profile directly
        @remarks
            We use this Profile to allow scoping rules to signify the beginning and end of
            the profile. Use the Profiler singleton (through the macro OgreProfileBegin(name)
            and OgreProfileEnd(name)) directly if you want a profile to last
            outside of a scope (i.e. the main game loop).
        @author Amit Mathew (amitmathew (at) yahoo (dot) com)
    */
    class Profile : public ProfilerAlloc
    {

    public:
        Profile(std::string_view profileName, ProfileGroupMask groupID = ProfileGroupMask::USER_DEFAULT)
            : mName(profileName), mGroupID(groupID)
        {
            Profiler::getSingleton().beginProfile(profileName, groupID);
        }
        ~Profile() { Profiler::getSingleton().endProfile(mName, mGroupID); }

    private:
        /// The name of this profile
        String mName;
        /// The group ID
        ProfileGroupMask mGroupID;
    };
    /** @} */
    /** @} */

} // end namespace
