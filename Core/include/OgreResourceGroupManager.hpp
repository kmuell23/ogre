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

#include <ctime>

export module Ogre.Core:ResourceGroupManager;

export import :Archive;
export import :Common;
export import :DataStream;
export import :IteratorWrapper;
export import :MemoryAllocatorConfig;
export import :Platform;
export import :Prerequisites;
export import :SharedPtr;
export import :Singleton;
export import :StringVector;

export import <list>;
export import <map>;
export import <string>;
export import <utility>;
export import <vector>;

// If X11/Xlib.h gets included before this header (for example it happens when
// including wxWidgets and FLTK), Status is defined as an int which we don't
// want as we have an enum class named Status.
#ifdef Status
#undef Status
#endif

export
namespace Ogre {

    class ManualResourceLoader;
    class Resource;
    class ResourceManager;
    class ScriptLoader;

    /** \addtogroup Core
    *  @{
    */
    /** \addtogroup Resources
    *  @{
    */

    /// Default resource group name
    char const constexpr inline RGN_DEFAULT[] = "General";
    /// Internal resource group name (should be used by OGRE internal only)
    char const constexpr inline RGN_INTERNAL[] = "OgreInternal";
    /// Special resource group name which causes resource group to be automatically determined based on searching for the resource in all groups.
    char const constexpr inline RGN_AUTODETECT[] = "OgreAutodetect";

    /** This class defines an interface which is called back during
        resource group loading to indicate the progress of the load. 

        Resource group loading is in 2 phases - creating resources from 
        declarations (which includes parsing scripts), and loading
        resources. Note that you don't necessarily have to have both; it
        is quite possible to just parse all the scripts for a group (see
        ResourceGroupManager::initialiseResourceGroup, but not to 
        load the resource group. 
        The sequence of events is (* signifies a repeating item):
        <ul>
        <li>resourceGroupScriptingStarted</li>
        <li>scriptParseStarted (*)</li>
        <li>scriptParseEnded (*)</li>
        <li>resourceGroupScriptingEnded</li>
        <li>resourceGroupLoadStarted</li>
        <li>resourceLoadStarted (*)</li>
        <li>resourceLoadEnded (*)</li>
        <li>customStageStarted (*)</li>
        <li>customStageEnded (*)</li>
        <li>resourceGroupLoadEnded</li>
        <li>resourceGroupPrepareStarted</li>
        <li>resourcePrepareStarted (*)</li>
        <li>resourcePrepareEnded (*)</li>
        <li>resourceGroupPrepareEnded</li>
        </ul>

    */
    class ResourceGroupListener
    {
    public:
        virtual ~ResourceGroupListener() = default;

        /** This event is fired when a resource group begins parsing scripts.
        @note
            Remember that if you are loading resources through ResourceBackgroundQueue,
            these callbacks will occur in the background thread, so you should
            not perform any thread-unsafe actions in this callback if that's the
            case (check the group name / script name).
        @param groupName The name of the group 
        @param scriptCount The number of scripts which will be parsed
        */
        virtual void resourceGroupScriptingStarted(std::string_view groupName, size_t scriptCount) {}
        /** This event is fired when a script is about to be parsed.
            @param scriptName Name of the to be parsed
            @param skipThisScript A boolean passed by reference which is by default set to 
            false. If the event sets this to true, the script will be skipped and not
            parsed. Note that in this case the scriptParseEnded event will not be raised
            for this script.
        */
        virtual void scriptParseStarted(std::string_view scriptName, bool& skipThisScript) {}

        /** This event is fired when the script has been fully parsed.
        */
        virtual void scriptParseEnded(std::string_view scriptName, bool skipped) {}
        /** This event is fired when a resource group finished parsing scripts. */
        virtual void resourceGroupScriptingEnded(std::string_view groupName) {}

        /** This event is fired  when a resource group begins preparing.
        @param groupName The name of the group being prepared
        @param resourceCount The number of resources which will be prepared, including
            a number of stages required to prepare any linked world geometry
        */
        virtual void resourceGroupPrepareStarted(std::string_view groupName, size_t resourceCount)
                { (void)groupName; (void)resourceCount; }

        /** This event is fired when a declared resource is about to be prepared. 
        @param resource Weak reference to the resource prepared.
        */
        virtual void resourcePrepareStarted(const ResourcePtr& resource)
                { (void)resource; }

        /** This event is fired when the resource has been prepared. 
        */
        virtual void resourcePrepareEnded() {}
        /** This event is fired when a resource group finished preparing. */
        virtual void resourceGroupPrepareEnded(std::string_view groupName)
        { (void)groupName; }

        /** This event is fired  when a resource group begins loading.
        @param groupName The name of the group being loaded
        @param resourceCount The number of resources which will be loaded, including
            a number of custom stages required to load anything else
        */
        virtual void resourceGroupLoadStarted(std::string_view groupName, size_t resourceCount) {}
        /** This event is fired when a declared resource is about to be loaded. 
        @param resource Weak reference to the resource loaded.
        */
        virtual void resourceLoadStarted(const ResourcePtr& resource) {}
        /** This event is fired when the resource has been loaded. 
        */
        virtual void resourceLoadEnded() {}
        /** This event is fired when a custom loading stage
            is about to start. The number of stages required will have been 
            included in the resourceCount passed in resourceGroupLoadStarted.
        @param description Text description of what is about to be done
        */
        virtual void customStageStarted(std::string_view description){}
        /** This event is fired when a custom loading stage
            has been completed. The number of stages required will have been 
            included in the resourceCount passed in resourceGroupLoadStarted.
        */
        virtual void customStageEnded() {}
        /** This event is fired when a resource group finished loading. */
        virtual void resourceGroupLoadEnded(std::string_view groupName) {}
        /** This event is fired when a resource was just created.
        @param resource Weak reference to the resource created.
        */
        virtual void resourceCreated(const ResourcePtr& resource)
        { (void)resource; }
        /** This event is fired when a resource is about to be removed.
        @param resource Weak reference to the resource removed.
        */
        virtual void resourceRemove(const ResourcePtr& resource)
        { (void)resource; }
    };

    /**
    This class allows users to override resource loading behavior.

    By overriding this class' methods, you can change how resources
    are loaded and the behavior for resource name collisions.
    */
    class ResourceLoadingListener
    {
    public:
        virtual ~ResourceLoadingListener() = default;

        /** This event is called when a resource beings loading. */
        virtual auto resourceLoading(std::string_view name, std::string_view group, Resource *resource) noexcept -> DataStreamPtr { return nullptr; }

        /** This event is called when a resource stream has been opened, but not processed yet. 

            You may alter the stream if you wish or alter the incoming pointer to point at
            another stream if you wish.
        */
        virtual void resourceStreamOpened(std::string_view name, std::string_view group, Resource *resource, DataStreamPtr& dataStream) {}

        /** This event is called when a resource collides with another existing one in a resource manager

            @param resource the new resource that conflicts with an existing one
            @param resourceManager the according resource manager 
            @return false to skip registration of the conflicting resource and continue using the previous instance.
          */
        virtual auto resourceCollision(Resource *resource, ResourceManager *resourceManager) noexcept -> bool { return true; }
    };

    /** This singleton class manages the list of resource groups, and notifying
        the various resource managers of their obligations to load / unload
        resources in a group. It also provides facilities to monitor resource
        loading per group (to do progress bars etc), provided the resources 
        that are required are pre-registered.

        Defining new resource groups, and declaring the resources you intend to
        use in advance is optional, however it is a very useful feature. In addition, 
        if a ResourceManager supports the definition of resources through scripts, 
        then this is the class which drives the locating of the scripts and telling
        the ResourceManager to parse them. 
        
        @see @ref Resource-Management
    */
    class ResourceGroupManager : public Singleton<ResourceGroupManager>, public ResourceAlloc
    {
    public:
        /// same as @ref RGN_DEFAULT
        static std::string_view const constexpr DEFAULT_RESOURCE_GROUP_NAME = RGN_DEFAULT;
        /// same as @ref RGN_INTERNAL
        static std::string_view const constexpr INTERNAL_RESOURCE_GROUP_NAME = RGN_INTERNAL;
        /// same as @ref RGN_AUTODETECT
        static std::string_view const constexpr AUTODETECT_RESOURCE_GROUP_NAME = RGN_AUTODETECT;
        /// The number of reference counts held per resource by the resource system
        static const long RESOURCE_SYSTEM_NUM_REFERENCE_COUNTS;
        /// Nested struct defining a resource declaration
        struct ResourceDeclaration
        {
            std::string_view resourceName;
            std::string_view resourceType;
            ManualResourceLoader* loader;
            NameValuePairList parameters;
        };
        /// List of resource declarations
        using ResourceDeclarationList = std::list<ResourceDeclaration>;
        using ResourceManagerMap = std::map<std::string_view, ResourceManager *>;
        using ResourceManagerIterator = MapIterator<ResourceManagerMap>;
        /// Resource location entry
        struct ResourceLocation
        {
            /// Pointer to the archive which is the destination
            Archive* archive;
            /// Whether this location was added recursively
            bool recursive;
        };
        /// List of possible file locations
        using LocationList = std::vector<ResourceLocation>;

    private:
        /// Map of resource types (strings) to ResourceManagers, used to notify them to load / unload group contents
        ResourceManagerMap mResourceManagerMap;

        /// Map of loading order (Real) to ScriptLoader, used to order script parsing
        using ScriptLoaderOrderMap = std::multimap<Real, ScriptLoader *>;
        ScriptLoaderOrderMap mScriptLoaderOrderMap;

        using ResourceGroupListenerList = std::vector<ResourceGroupListener *>;
        ResourceGroupListenerList mResourceGroupListenerList;

        ResourceLoadingListener *mLoadingListener{nullptr};

        /// Resource index entry, resourcename->location 
        using ResourceLocationIndex = std::map<std::string, Archive*, std::less<>>;

        /// List of resources which can be loaded / unloaded
        using LoadUnloadResourceList = std::list<ResourcePtr>;
        /// Resource group entry
        struct ResourceGroup
        {
            enum class Status
            {
                UNINITIALSED = 0,
                INITIALISING = 1,
                INITIALISED = 2,
                LOADING = 3,
                LOADED = 4
            };
            /// Group name
            std::string name;
            /// Group status
            Status groupStatus;
            /// List of possible locations to search
            LocationList locationList;
            /// Index of resource names to locations, built for speedy access (case sensitive archives)
            ResourceLocationIndex resourceIndexCaseSensitive;

            /// Pre-declared resources, ready to be created
            ResourceDeclarationList resourceDeclarations;
            /// Created resources which are ready to be loaded / unloaded
            // Group by loading order of the type (defined by ResourceManager)
            // (e.g. skeletons and materials before meshes)
            using LoadResourceOrderMap = std::map<Real, LoadUnloadResourceList>;
            LoadResourceOrderMap loadResourceOrderMap;
            uint32 customStageCount;
            // in global pool flag - if true the resource will be loaded even a different   group was requested in the load method as a parameter.
            bool inGlobalPool;

            void addToIndex(std::string_view filename, Archive* arch);
            void removeFromIndex(std::string_view filename, Archive* arch);
            void removeFromIndex(Archive* arch);

        };
        /// Map from resource group names to groups
        using ResourceGroupMap = std::map<std::string, ResourceGroup*, std::less<>>;
        ResourceGroupMap mResourceGroupMap;

        /// Group name for world resources
        String mWorldGroupName;

        /** Parses all the available scripts found in the resource locations
        for the given group, for all ResourceManagers.
        @remarks
            Called as part of initialiseResourceGroup
        */
        void parseResourceGroupScripts(ResourceGroup* grp) const;
        /** Create all the pre-declared resources.
        @remarks
            Called as part of initialiseResourceGroup
        */
        void createDeclaredResources(ResourceGroup* grp);
        /** Adds a created resource to a group. */
        void addCreatedResource(ResourcePtr& res, ResourceGroup& group) const;
        /** Get resource group */
        [[nodiscard]] auto getResourceGroup(std::string_view name, bool throwOnFailure = false) const -> ResourceGroup*;
        /** Drops contents of a group, leave group there, notify ResourceManagers. */
        void dropGroupContents(ResourceGroup* grp);
        /** Delete a group for shutdown - don't notify ResourceManagers. */
        void deleteGroup(ResourceGroup* grp);
        /// Internal find method for auto groups
        [[nodiscard]] auto
        resourceExistsInAnyGroupImpl(std::string_view filename) const -> std::pair<Archive*, ResourceGroup*>;
        /// Internal event firing method
        void fireResourceGroupScriptingStarted(std::string_view groupName, size_t scriptCount) const;
        /// Internal event firing method
        void fireScriptStarted(std::string_view scriptName, bool &skipScript) const;
        /// Internal event firing method
        void fireScriptEnded(std::string_view scriptName, bool skipped) const;
        /// Internal event firing method
        void fireResourceGroupScriptingEnded(std::string_view groupName) const;
        /// Internal event firing method
        void fireResourceGroupLoadStarted(std::string_view groupName, size_t resourceCount) const;
        /// Internal event firing method
        void fireResourceLoadStarted(const ResourcePtr& resource) const;
        /// Internal event firing method
        void fireResourceLoadEnded() const;
        /// Internal event firing method
        void fireResourceGroupLoadEnded(std::string_view groupName) const;
        /// Internal event firing method
        void fireResourceGroupPrepareStarted(std::string_view groupName, size_t resourceCount) const;
        /// Internal event firing method
        void fireResourcePrepareStarted(const ResourcePtr& resource) const;
        /// Internal event firing method
        void fireResourcePrepareEnded() const;
        /// Internal event firing method
        void fireResourceGroupPrepareEnded(std::string_view groupName) const;
        /// Internal event firing method
        void fireResourceCreated(const ResourcePtr& resource) const;
        /// Internal event firing method
        void fireResourceRemove(const ResourcePtr& resource) const;
        /** Internal modification time retrieval */
        auto resourceModifiedTime(ResourceGroup* group, std::string_view filename) const -> std::filesystem::file_time_type;

        /** Find out if the named file exists in a group. Internal use only
         @param group Pointer to the resource group
         @param filename Fully qualified name of the file to test for
         */
        auto resourceExists(ResourceGroup* group, std::string_view filename) const -> Archive*;

        /** Open resource with optional searching in other groups if it is not found. Internal use only */
        auto openResourceImpl(std::string_view resourceName,
            std::string_view groupName,
            bool searchGroupsIfNotFound,
            Resource* resourceBeingLoaded,
            bool throwOnFailure = true) const -> DataStreamPtr;

        /// Stored current group - optimisation for when bulk loading a group
        ResourceGroup* mCurrentGroup{nullptr};
    public:
        ResourceGroupManager();
        virtual ~ResourceGroupManager();

        /** Create a resource group.

        @param name The name to give the resource group.
        @param inGlobalPool if true the resource will be loaded even a different
            group was requested in the load method as a parameter.
        @see @ref Resource-Management
        */
        void createResourceGroup(std::string_view name, bool inGlobalPool = false);


        /** Initialises a resource group.
        @note 
            When you call Root::initialise, all resource groups that have already been
            created are automatically initialised too. Therefore you do not need to 
            call this method for groups you define and set up before you call 
            Root::initialise. However, since one of the most useful features of 
            resource groups is to set them up after the main system initialisation
            has occurred (e.g. a group per game level), you must remember to call this
            method for the groups you create after this.

        @param name The name of the resource group to initialise
        @see @ref Resource-Management
        */
        void initialiseResourceGroup(std::string_view name);

        /** Initialise all resource groups which are yet to be initialised.
        @see #initialiseResourceGroup
        */
        void initialiseAllResourceGroups();

        /** Prepares a resource group.

            Prepares any created resources which are part of the named group.
            Note that resources must have already been created by calling
            ResourceManager::createResource, or declared using declareResource() or
            in a script (such as .material and .overlay). The latter requires
            that initialiseResourceGroup() has been called.
        
            When this method is called, this class will callback any ResourceGroupListener
            which have been registered to update them on progress. 
        @param name The name of the resource group to prepare.
        */
        void prepareResourceGroup(std::string_view name);

        /** Loads a resource group.

            Loads any created resources which are part of the named group.
            Note that resources must have already been created by calling
            ResourceManager::createResource, or declared using declareResource() or
            in a script (such as .material and .overlay). The latter requires
            that initialiseResourceGroup() has been called.
        
            When this method is called, this class will callback any ResourceGroupListeners
            which have been registered to update them on progress. 
        @param name The name of the resource group to load.
        */
        void loadResourceGroup(std::string_view name);

        /** Unloads a resource group.

            This method unloads all the resources that have been declared as
            being part of the named resource group. Note that these resources
            will still exist in their respective ResourceManager classes, but
            will be in an unloaded state. If you want to remove them entirely,
            you should use clearResourceGroup() or destroyResourceGroup().
        @param name The name to of the resource group to unload.
        @param reloadableOnly If set to true, only unload the resource that is
            reloadable. Because some resources isn't reloadable, they will be
            unloaded but can't load them later. Thus, you might not want to them
            unloaded. Or, you might unload all of them, and then populate them
            manually later.
            @see Resource::isReloadable for resource is reloadable.
        */
        void unloadResourceGroup(std::string_view name, bool reloadableOnly = true);

        /** Unload all resources which are not referenced by any other object.

            This method behaves like unloadResourceGroup(), except that it only
            unloads resources in the group which are not in use, ie not referenced
            by other objects. This allows you to free up some memory selectively
            whilst still keeping the group around (and the resources present,
            just not using much memory).
        @param name The name of the group to check for unreferenced resources
        @param reloadableOnly If true (the default), only unloads resources
            which can be subsequently automatically reloaded
        */
        void unloadUnreferencedResourcesInGroup(std::string_view name, 
            bool reloadableOnly = true);

        /** Clears a resource group. 
        @remarks
            This method unloads all resources in the group, but in addition it
            removes all those resources from their ResourceManagers, and then 
            clears all the members from the list. That means after calling this
            method, there are no resources declared as part of the named group
            any more. Resource locations still persist though.
        @param name The name to of the resource group to clear.
        */
        void clearResourceGroup(std::string_view name);
        
        /** Destroys a resource group, clearing it first, destroying the resources
            which are part of it, and then removing it from
            the list of resource groups. 
        @param name The name of the resource group to destroy.
        */
        void destroyResourceGroup(std::string_view name);

        /** Checks the status of a resource group.
        @remarks
            Looks at the state of a resource group.
            If initialiseResourceGroup has been called for the resource
            group return true, otherwise return false.
        @param name The name to of the resource group to access.
        */
        [[nodiscard]] auto isResourceGroupInitialised(std::string_view name) const -> bool;

        /** Checks the status of a resource group.
        @remarks
            Looks at the state of a resource group.
            If loadResourceGroup has been called for the resource
            group return true, otherwise return false.
        @param name The name to of the resource group to access.
        */
        [[nodiscard]] auto isResourceGroupLoaded(std::string_view name) const -> bool;

        /*** Verify if a resource group exists
        @param name The name of the resource group to look for
        */
        [[nodiscard]] auto resourceGroupExists(std::string_view name) const -> bool;

        /** Adds a location to the list of searchable locations for a
            Resource type.

            @param
                name The name of the location, e.g. './data' or
                '/compressed/gamedata.zip'
            @param
                locType A string identifying the location type, e.g.
                'FileSystem' (for folders), 'Zip' etc. Must map to a
                registered plugin which deals with this type (FileSystem and
                Zip should always be available)
            @param
                resGroup the resource group which this location
                should apply to; defaults to the General group which applies to
                all non-specific resources.
            @param
                recursive If the resource location has a concept of recursive
                directory traversal, enabling this option will mean you can load
                resources in subdirectories using only their unqualified name.
                The default is to disable this so that resources in subdirectories
                with the same name are still unique.
            @param readOnly whether the Archive is read only
            @see @ref Resource-Management
        */
        void addResourceLocation(std::string_view name, std::string_view locType, 
            std::string_view resGroup = DEFAULT_RESOURCE_GROUP_NAME, bool recursive = false, bool readOnly = true);
        /** Removes a resource location from the search path. */ 
        void removeResourceLocation(std::string_view name, 
            std::string_view resGroup = DEFAULT_RESOURCE_GROUP_NAME);
        /** Verify if a resource location exists for the given group. */ 
        [[nodiscard]] auto resourceLocationExists(std::string_view name, 
            std::string_view resGroup = DEFAULT_RESOURCE_GROUP_NAME) const -> bool;

        /** Declares a resource to be a part of a resource group, allowing you 
            to load and unload it as part of the group.

        @param name The resource name. 
        @param resourceType The type of the resource. Ogre comes preconfigured with 
            a number of resource types: 
            <ul>
            <li>Font</li>
            <li>GpuProgram</li>
            <li>HighLevelGpuProgram</li>
            <li>Material</li>
            <li>Mesh</li>
            <li>Skeleton</li>
            <li>Texture</li>
            </ul>
            .. but more can be added by plugin ResourceManager classes.
        @param groupName The name of the group to which it will belong.
        @param loadParameters A list of name / value pairs which supply custom
            parameters to the resource which will be required before it can 
            be loaded. These are specific to the resource type.
        @see @ref Resource-Management
        */
        void declareResource(std::string_view name, std::string_view resourceType,
            std::string_view groupName = DEFAULT_RESOURCE_GROUP_NAME,
            const NameValuePairList& loadParameters = NameValuePairList());
        /** @copydoc declareResource
            @param loader Pointer to a ManualResourceLoader implementation which will
            be called when the Resource wishes to load. If supplied, the resource
            is manually loaded, otherwise it'll loading from file automatic.
            @note We don't support declare manually loaded resource without loader
                here, since it's meaningless.
        */
        void declareResource(std::string_view name, std::string_view resourceType,
            std::string_view groupName, ManualResourceLoader* loader,
            const NameValuePairList& loadParameters = NameValuePairList());
        /** Undeclare a resource.
        @remarks
            Note that this will not cause it to be unloaded
            if it is already loaded, nor will it destroy a resource which has 
            already been created if initialiseResourceGroup has been called already.
            Only unloadResourceGroup / clearResourceGroup / destroyResourceGroup 
            will do that. 
        @param name The name of the resource. 
        @param groupName The name of the group this resource was declared in. 
        */
        void undeclareResource(std::string_view name, std::string_view groupName);

        /** Open a single resource by name and return a DataStream
            pointing at the source of the data.
        @param resourceName The name of the resource to locate.
            Even if resource locations are added recursively, you
            must provide a fully qualified name to this method. You
            can find out the matching fully qualified names by using the
            find() method if you need to.
        @param groupName The name of the resource group; this determines which
            locations are searched.
            If you're loading a @ref Resource using #RGN_AUTODETECT, you **must**
            also provide the resourceBeingLoaded parameter to enable the
            group membership to be changed
        @param resourceBeingLoaded Optional pointer to the resource being
            loaded, which you should supply if you want
        @param throwOnFailure throw an exception. Returns nullptr otherwise
        @return Shared pointer to data stream containing the data, will be
            destroyed automatically when no longer referenced
        */
        auto openResource(std::string_view resourceName,
                                   std::string_view groupName = DEFAULT_RESOURCE_GROUP_NAME,
                                   Resource* resourceBeingLoaded = nullptr,
                                   bool throwOnFailure = true) const -> DataStreamPtr
        {
            return openResourceImpl(resourceName, groupName, false,
                                    resourceBeingLoaded, throwOnFailure);
        }

        /** Open all resources matching a given pattern (which can contain
            the character '*' as a wildcard), and return a collection of 
            DataStream objects on them.
        @param pattern The pattern to look for. If resource locations have been
            added recursively, subdirectories will be searched too so this
            does not need to be fully qualified.
        @param groupName The resource group; this determines which locations
            are searched.
        @return Shared pointer to a data stream list , will be
            destroyed automatically when no longer referenced
        */
        [[nodiscard]] auto openResources(std::string_view pattern,
            std::string_view groupName = DEFAULT_RESOURCE_GROUP_NAME) const -> DataStreamList;
        
        /** List all files in a resource group with accompanying information.
        @param groupName The name of the group
        @param dirs If true, directory names will be returned instead of file names
        @return A list of structures detailing quite a lot of information about
        all the files in the archive.
        */
        [[nodiscard]] auto listResourceFileInfo(std::string_view groupName, bool dirs = false) const -> FileInfoListPtr;

        /** Find all file or directory names matching a given pattern in a
            resource group.
        @note
        This method only returns filenames, you can also retrieve other
        information using findFileInfo.
        @param groupName The name of the group
        @param pattern The pattern to search for; wildcards (*) are allowed
        @param dirs Set to true if you want the directories to be listed
            instead of files
        @return A list of filenames matching the criteria, all are fully qualified
        */
        [[nodiscard]] auto findResourceNames(std::string_view groupName, std::string_view pattern,
            bool dirs = false) const -> StringVectorPtr;

        /** Find out if the named file exists in a group. 
        @param group The name of the resource group
        @param filename Fully qualified name of the file to test for
        */
        [[nodiscard]] auto resourceExists(std::string_view group, std::string_view filename) const -> bool;
        
        /** Find out if the named file exists in any group. 
        @param filename Fully qualified name of the file to test for
        */
        [[nodiscard]] auto resourceExistsInAnyGroup(std::string_view filename) const -> bool;

        /** Find the group in which a resource exists.
        @param filename Fully qualified name of the file the resource should be
            found as
        @return Name of the resource group the resource was found in. An
            exception is thrown if the group could not be determined.
        */
        [[nodiscard]] auto findGroupContainingResource(std::string_view filename) const -> std::string_view ;

        /** Find all files or directories matching a given pattern in a group
            and get some detailed information about them.
        @param group The name of the resource group
        @param pattern The pattern to search for; wildcards (*) are allowed
        @param dirs Set to true if you want the directories to be listed
            instead of files
        @return A list of file information structures for all files matching 
        the criteria.
        */
        [[nodiscard]] auto findResourceFileInfo(std::string_view group, std::string_view pattern,
            bool dirs = false) const -> FileInfoListPtr;

        /** Retrieve the modification time of a given file */
        [[nodiscard]] auto resourceModifiedTime(std::string_view group, std::string_view filename) const -> std::filesystem::file_time_type;

        /** Find all resource location names matching a given pattern in a
            resource group.
        @param groupName The name of the group
        @param pattern The pattern to search for; wildcards (*) are allowed
        @return A list of resource locations matching the criteria
        */
        [[nodiscard]] auto findResourceLocation(std::string_view groupName, std::string_view pattern) const -> StringVectorPtr;

        /** Create a new resource file in a given group.
        @remarks
            This method creates a new file in a resource group and passes you back a 
            writeable stream. 
        @param filename The name of the file to create
        @param groupName The name of the group in which to create the file
        @param overwrite If true, an existing file will be overwritten, if false
            an error will occur if the file already exists
        @param locationPattern If the resource group contains multiple locations, 
            then usually the file will be created in the first writable location. If you 
            want to be more specific, you can include a location pattern here and 
            only locations which match that pattern (as determined by StringUtil::match)
            will be considered candidates for creation.
        */
        auto createResource(std::string_view filename, std::string_view groupName = DEFAULT_RESOURCE_GROUP_NAME, 
            bool overwrite = false, std::string_view locationPattern = BLANKSTRING) -> DataStreamPtr;

        /** Delete a single resource file.
        @param filename The name of the file to delete. 
        @param groupName The name of the group in which to search
        @param locationPattern If the resource group contains multiple locations, 
            then usually first matching file found in any location will be deleted. If you 
            want to be more specific, you can include a location pattern here and 
            only locations which match that pattern (as determined by StringUtil::match)
            will be considered candidates for deletion.
        */
        void deleteResource(std::string_view filename, std::string_view groupName = DEFAULT_RESOURCE_GROUP_NAME, 
            std::string_view locationPattern = BLANKSTRING);

        /** Delete all matching resource files.
        @param filePattern The pattern (see StringUtil::match) of the files to delete. 
        @param groupName The name of the group in which to search
        @param locationPattern If the resource group contains multiple locations, 
            then usually all matching files in any location will be deleted. If you 
            want to be more specific, you can include a location pattern here and 
            only locations which match that pattern (as determined by StringUtil::match)
            will be considered candidates for deletion.
        */
        void deleteMatchingResources(std::string_view filePattern, std::string_view groupName = DEFAULT_RESOURCE_GROUP_NAME, 
            std::string_view locationPattern = BLANKSTRING);

        /** Adds a ResourceGroupListener which will be called back during 
            resource loading events. 
        */
        void addResourceGroupListener(ResourceGroupListener* l);
        /** Removes a ResourceGroupListener */
        void removeResourceGroupListener(ResourceGroupListener* l);

        /** Sets the resource group that 'world' resources will use.
        @remarks
            This is the group which should be used by SceneManagers implementing
            world geometry when looking for their resources. Defaults to the 
            DEFAULT_RESOURCE_GROUP_NAME but this can be altered.
        */
        void setWorldResourceGroupName(std::string_view groupName) {mWorldGroupName = groupName;}

        /// Gets the resource group that 'world' resources will use.
        [[nodiscard]] auto getWorldResourceGroupName() const noexcept -> std::string_view { return mWorldGroupName; }

        /** Declare the number custom loading stages for a resource group

        This allows you to include them in a loading progress report.
        @param group The name of the resource group
        @param stageCount The number of extra stages
        @see #addResourceGroupListener
        @see #_notifyCustomStageStarted
        @see #_notifyCustomStageEnded
        */
        void setCustomStagesForResourceGroup(std::string_view group, uint32 stageCount);

        auto getCustomStagesForResourceGroup(std::string_view group) -> uint32;

            /** Checks the status of a resource group.
        @remarks
            Looks at the state of a resource group.
            If loadResourceGroup has been called for the resource
            group return true, otherwise return false.
        @param name The name to of the resource group to access.
        */
        [[nodiscard]] auto isResourceGroupInGlobalPool(std::string_view name) const -> bool;

        /** Shutdown all ResourceManagers, performed as part of clean-up. */
        void shutdownAll();


        /** Internal method for registering a ResourceManager (which should be
            a singleton). Creators of plugins can register new ResourceManagers
            this way if they wish.
        @remarks
            ResourceManagers that wish to parse scripts must also call 
            _registerScriptLoader.
        @param resourceType String identifying the resource type, must be unique.
        @param rm Pointer to the ResourceManager instance.
        */
        void _registerResourceManager(std::string_view resourceType, ResourceManager* rm);

        /** Internal method for unregistering a ResourceManager.
        @remarks
            ResourceManagers that wish to parse scripts must also call 
            _unregisterScriptLoader.
        @param resourceType String identifying the resource type.
        */
        void _unregisterResourceManager(std::string_view resourceType);

        /** Get the registered resource managers.
        */
        [[nodiscard]] auto getResourceManagers() const noexcept -> const ResourceManagerMap& { return mResourceManagerMap; }

        /** Internal method for registering a ScriptLoader.
        @remarks ScriptLoaders parse scripts when resource groups are initialised.
        @param su Pointer to the ScriptLoader instance.
        */
        void _registerScriptLoader(ScriptLoader* su);

        /** Internal method for unregistering a ScriptLoader.
        @param su Pointer to the ScriptLoader instance.
        */
        void _unregisterScriptLoader(ScriptLoader* su);

        /** Method used to directly query for registered script loaders.
        @param pattern The specific script pattern (e.g. *.material) the script loader handles
        */
        [[nodiscard]] auto _findScriptLoader(std::string_view pattern) const -> ScriptLoader *;

        /** Internal method for getting a registered ResourceManager.
        @param resourceType String identifying the resource type.
        */
        [[nodiscard]] auto _getResourceManager(std::string_view resourceType) const -> ResourceManager*;

        /** Internal method called by ResourceManager when a resource is created.
        @param res Weak reference to resource
        */
        void _notifyResourceCreated(ResourcePtr& res) const;

        /** Internal method called by ResourceManager when a resource is removed.
        @param res Weak reference to resource
        */
        void _notifyResourceRemoved(const ResourcePtr& res) const;

        /** Internal method to notify the group manager that a resource has
            changed group (only applicable for autodetect group) */
        void _notifyResourceGroupChanged(std::string_view oldGroup, Resource* res) const;

        /** Internal method called by ResourceManager when all resources 
            for that manager are removed.
        @param manager Pointer to the manager for which all resources are being removed
        */
        void _notifyAllResourcesRemoved(ResourceManager* manager) const;

        /** Notify this manager that one custom loading stage has been started

        User code should call this method the number of times equal to the value declared
        in #setCustomStagesForResourceGroup.
        */
        void _notifyCustomStageStarted(std::string_view description) const;
        /** Notify this manager that one custom loading stage has been completed

        User code should call this method the number of times equal to the value declared
        #setCustomStagesForResourceGroup.
        */
        void _notifyCustomStageEnded() const;

        /** Get a list of the currently defined resource groups. 
        @note This method intentionally returns a copy rather than a reference in
            order to avoid any contention issues in multithreaded applications.
        @return A copy of list of currently defined groups.
        */
        [[nodiscard]] auto getResourceGroups() const -> std::vector<std::string_view>;
        /** Get the list of resource declarations for the specified group name. 
        @note This method intentionally returns a copy rather than a reference in
            order to avoid any contention issues in multithreaded applications.
        @param groupName The name of the group
        @return A copy of list of currently defined resources.
        */
        [[nodiscard]] auto getResourceDeclarationList(std::string_view groupName) const -> ResourceDeclarationList;

        /** Get the list of resource locations for the specified group name.
        @param groupName The name of the group
        @return The list of resource locations associated with the given group.
        */      
        [[nodiscard]] auto getResourceLocationList(std::string_view groupName) const -> const LocationList&;

        /// Sets a new loading listener
        void setLoadingListener(ResourceLoadingListener *listener);
        /// Returns the current loading listener
        [[nodiscard]] auto getLoadingListener() const -> ResourceLoadingListener *;

        /// @copydoc Singleton::getSingleton()
        static auto getSingleton() noexcept -> ResourceGroupManager&;
        /// @copydoc Singleton::getSingleton()
        static auto getSingletonPtr() noexcept -> ResourceGroupManager*;

    };
    /** @} */
    /** @} */
}
