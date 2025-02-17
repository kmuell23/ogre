// This file is part of the OGRE project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at https://www.ogre3d.org/licensing.
module;

#include <cstdlib>

module Ogre.Components.Bites;

import :ApplicationContextBase;
import :ConfigDialog;
import :Input;
import :SGTechniqueResolverListener;
import :WindowEventUtilities;

import Ogre.Components.Overlay;
import Ogre.Components.RTShaderSystem;
import Ogre.Core;

import <ios>;
import <map>;
import <ranges>;
import <string>;
import <type_traits>;

namespace OgreBites {

static const char constexpr SHADER_CACHE_FILENAME[] = "cache.bin";

ApplicationContextBase::ApplicationContextBase(std::string_view appName)
{
    mAppName = appName;
    mFSLayer = ::std::make_unique<Ogre::FileSystemLayer>(mAppName);

    if (char* val = getenv("OGRE_CONFIG_DIR"))
    {
        Ogre::String configDir = Ogre::StringUtil::standardisePath(val);
        mFSLayer->setConfigPaths({ configDir });
    }

    mRoot = nullptr;
    mOverlaySystem = nullptr;
    mFirstRun = true;

    mMaterialMgrListener = nullptr;
    mShaderGenerator = nullptr;
}

void ApplicationContextBase::initApp(ulong frameCount)
{
    createRoot(frameCount);
    if (!oneTimeConfig()) return;

    // if the context was reconfigured, set requested renderer
    if (!mFirstRun) mRoot->setRenderSystem(mRoot->getRenderSystemByName(mNextRenderer));

    setup();
}

void ApplicationContextBase::closeApp()
{
    shutdown();
    if (mRoot)
    {
        mRoot->saveConfig();

        delete mRoot;
        mRoot = nullptr;
    }

    mStaticPluginLoader.unload();
}

auto ApplicationContextBase::initialiseRTShaderSystem() -> bool
{
    if (Ogre::RTShader::ShaderGenerator::initialize())
    {
        mShaderGenerator = Ogre::RTShader::ShaderGenerator::getSingletonPtr();

        // Create and register the material manager listener if it doesn't exist yet.
        if (!mMaterialMgrListener) {
            mMaterialMgrListener = new SGTechniqueResolverListener(mShaderGenerator);
            Ogre::MaterialManager::getSingleton().addListener(mMaterialMgrListener);
        }

        return true;
    }
    return false;
}

void ApplicationContextBase::setRTSSWriteShadersToDisk(bool write)
{
    if(!write) {
        mShaderGenerator->setShaderCachePath("");
        return;
    }

    // Set shader cache path.
    auto subdir = "RTShaderCache";

    auto path = mFSLayer->getWritablePath(subdir);
    if (!Ogre::FileSystemLayer::fileExists(path))
    {
        Ogre::FileSystemLayer::createDirectory(path);
    }
    mShaderGenerator->setShaderCachePath(path);
}

void ApplicationContextBase::destroyRTShaderSystem()
{
    //mShaderGenerator->removeAllShaderBasedTechniques();
    //mShaderGenerator->flushShaderCache();

    // Restore default scheme.
    Ogre::MaterialManager::getSingleton().setActiveScheme(Ogre::MaterialManager::DEFAULT_SCHEME_NAME);

    // Unregister the material manager listener.
    if (mMaterialMgrListener != nullptr)
    {
        Ogre::MaterialManager::getSingleton().removeListener(mMaterialMgrListener);
        delete mMaterialMgrListener;
        mMaterialMgrListener = nullptr;
    }

    // Destroy RTShader system.
    if (mShaderGenerator != nullptr)
    {
        Ogre::RTShader::ShaderGenerator::destroy();
        mShaderGenerator = nullptr;
    }
}

void ApplicationContextBase::setup()
{
    mRoot->initialise(false);
    createWindow(mAppName);

    locateResources();
    initialiseRTShaderSystem();
    loadResources();

    // adds context as listener to process context-level (above the sample level) events
    mRoot->addFrameListener(this);
}

void ApplicationContextBase::createRoot(ulong frameCount)
{
    Ogre::String pluginsPath;

    mRoot = new Ogre::Root(pluginsPath, mFSLayer->getWritablePath("ogre.cfg"),
                                mFSLayer->getWritablePath("ogre.log"), frameCount);

    mStaticPluginLoader.load();

    mOverlaySystem = new Ogre::OverlaySystem();
}

auto ApplicationContextBase::oneTimeConfig() -> bool
{
    if(mRoot->getAvailableRenderers().empty())
    {
        Ogre::LogManager::getSingleton().logError("No RenderSystems available");
        return false;
    }

    if (!mRoot->restoreConfig()) {
        return mRoot->showConfigDialog(OgreBites::getNativeConfigDialog());
    }
    return true;
}

void ApplicationContextBase::createDummyScene()
{
    mWindows[0].render->removeAllViewports();
    Ogre::SceneManager* sm = mRoot->createSceneManager("DefaultSceneManager", "DummyScene");
    sm->addRenderQueueListener(mOverlaySystem);
    Ogre::Camera* cam = sm->createCamera("DummyCamera");
    sm->getRootSceneNode()->attachObject(cam);
    mWindows[0].render->addViewport(cam);

    // Initialize shader generator.
    // Must be before resource loading in order to allow parsing extended material attributes.
    if (!initialiseRTShaderSystem())
    {
        OGRE_EXCEPT(Ogre::ExceptionCodes::FILE_NOT_FOUND,
                    "Shader Generator Initialization failed - Core shader libs path not found",
                    "ApplicationContextBase::createDummyScene");
    }

    mShaderGenerator->addSceneManager(sm);
}

void ApplicationContextBase::destroyDummyScene()
{
    if(!mRoot->hasSceneManager("DummyScene"))
        return;

    Ogre::SceneManager*  dummyScene = mRoot->getSceneManager("DummyScene");

    mShaderGenerator->removeSceneManager(dummyScene);

    dummyScene->removeRenderQueueListener(mOverlaySystem);
    mWindows[0].render->removeAllViewports();
    mRoot->destroySceneManager(dummyScene);
}

void ApplicationContextBase::addInputListener(NativeWindowType* win, InputListener* lis)
{
    mInputListeners.insert(std::make_pair(0, lis));
}


void ApplicationContextBase::removeInputListener(NativeWindowType* win, InputListener* lis)
{
    mInputListeners.erase(std::make_pair(0, lis));
}

auto ApplicationContextBase::frameRenderingQueued(const Ogre::FrameEvent& evt) -> bool
{
    for(const auto & mInputListener : mInputListeners) {
        mInputListener.second->frameRendered(evt);
    }

    return true;
}

auto ApplicationContextBase::createWindow(std::string_view name, Ogre::uint32 w, Ogre::uint32 h, Ogre::NameValuePairList miscParams) -> NativeWindowPair
{
    NativeWindowPair ret = {nullptr, nullptr};

    if(!mWindows.empty())
    {
        // additional windows should reuse the context
        miscParams["currentGLContext"] = "true";
    }

    auto p = mRoot->getRenderSystem()->getRenderWindowDescription();
    miscParams.insert(p.miscParams.begin(), p.miscParams.end());
    p.miscParams = miscParams;
    p.name = name;

    if(w > 0 && h > 0)
    {
        p.width = w;
        p.height= h;
    }

    ret.render = mRoot->createRenderWindow(p);

    mWindows.push_back(ret);

    WindowEventUtilities::_addRenderWindow(ret.render);

    return ret;
}

void ApplicationContextBase::destroyWindow(std::string_view name)
{
    for (auto it = mWindows.begin(); it != mWindows.end(); ++it)
    {
        if (it->render->getName() != name)
            continue;
        _destroyWindow(*it);
        mWindows.erase(it);
        return;
    }

    OGRE_EXCEPT(Ogre::ExceptionCodes::INVALIDPARAMS, ::std::format("No window named '{}'", name));
}

void ApplicationContextBase::_destroyWindow(const NativeWindowPair& win)
{
    mRoot->destroyRenderWindow(win.render);
}

void ApplicationContextBase::_fireInputEvent(const Event& event, uint32_t windowID) const
{
    struct Visitor
    {
        InputListener& l;

        void operator()(::std::monostate const&) const noexcept
        {   /* nothing for everything else*/ }

        void operator()(KeyDownEvent const& event) const noexcept
        {   l.keyPressed(event);    }

        void operator()(KeyUpEvent const& event) const noexcept
        {   l.keyReleased(event);    }

        void operator()(MouseButtonDownEvent const& event) const noexcept
        {   l.mousePressed(event);  }

        void operator()(MouseButtonUpEvent const& event) const noexcept
        {   l.mouseReleased(event); }

        void operator()(MouseWheelEvent const& event) const noexcept
        {   l.mouseWheelRolled(event);  }

        void operator()(MouseMotionEvent const& event) const noexcept
        {   l.mouseMoved(event);    }

        void operator()(TouchFingerDownEvent const& event) const noexcept
        {   // for finger down we have to move the pointer first
            l.touchMoved(::std::bit_cast<TouchFingerMotionEvent>(event));
            l.touchPressed(event);
        }

        void operator()(TouchFingerUpEvent const& event) const noexcept
        {   l.touchReleased(event); }

        void operator()(TouchFingerMotionEvent const& event) const noexcept
        {   l.touchMoved(event);    }

        void operator()(TextInputEvent const& event) const noexcept
        {   l.textInput(event); }

        void operator()(AxisEvent const& event) const noexcept
        {   l.axisMoved(event); }

        void operator()(ButtonDownEvent const& event) const noexcept
        {   l.buttonPressed(event); }

        void operator()(ButtonUpEvent const& event) const noexcept
        {   l.buttonReleased(event);  }
    };

    for(const auto & mInputListener : mInputListeners)
    {
        // gamepad events are not window specific
        if(mInputListener.first != windowID && event.index() <= Event{TextInputEvent{}}.index()) continue;

        Visitor const visitor {*mInputListener.second};
        ::std::visit(visitor, event);
    }
}

auto ApplicationContextBase::getDefaultMediaDir() -> Ogre::String
{
    return Ogre::FileSystemLayer::resolveBundlePath(getenv("OGRE_MEDIA_DIR"));
}

void ApplicationContextBase::locateResources()
{
    auto& rgm = Ogre::ResourceGroupManager::getSingleton();
    // load resource paths from config file
    Ogre::ConfigFile cf;
    Ogre::String resourcesPath = mFSLayer->getConfigFilePath("resources.cfg");

    if (Ogre::FileSystemLayer::fileExists(resourcesPath))
    {
        Ogre::LogManager::getSingleton().logMessage(::std::format("Parsing '{}'", resourcesPath));
        cf.load(resourcesPath);
    }
    else
    {
        rgm.addResourceLocation(getDefaultMediaDir(), "FileSystem", Ogre::RGN_DEFAULT);
    }

    // go through all specified resource groups
    for(auto const& [sec, settings] : cf.getSettingsBySection()) {

        // go through all resource paths
        for (auto const& [type, value] : settings)
        {
            std::string arch{ value };
            Ogre::StringUtil::trim(arch);
            if (arch.empty() || arch[0] == '.')
            {
                // resolve relative path with regards to configfile
                std::string_view baseDir, filename;
                Ogre::StringUtil::splitFilename(resourcesPath, filename, baseDir);
                arch = std::format("{}{}", baseDir, arch);
            }

            arch = Ogre::FileSystemLayer::resolveBundlePath(arch);

            if((type == "Zip" || type == "FileSystem") && !Ogre::FileSystemLayer::fileExists(arch))
            {
                Ogre::LogManager::getSingleton().logWarning(::std::format("resource location '{}' does not exist - skipping", arch));
                continue;
            }

            rgm.addResourceLocation(arch, type, sec);
        }
    }

    if(rgm.getResourceLocationList(Ogre::RGN_INTERNAL).empty())
    {
        const auto& mediaDir = getDefaultMediaDir();
        // add default locations
        rgm.addResourceLocation(::std::format("{}/Main", mediaDir), "FileSystem", Ogre::RGN_INTERNAL);

        rgm.addResourceLocation(::std::format("{}/RTShaderLib/GLSL", mediaDir), "FileSystem", Ogre::RGN_INTERNAL);
        rgm.addResourceLocation(::std::format("{}/RTShaderLib/HLSL_Cg", mediaDir), "FileSystem", Ogre::RGN_INTERNAL);
    }
}

void ApplicationContextBase::loadResources()
{
    Ogre::ResourceGroupManager::getSingleton().initialiseAllResourceGroups();
}

void ApplicationContextBase::reconfigure(std::string_view renderer, Ogre::NameValuePairList &options)
{
    mNextRenderer = renderer;
    Ogre::RenderSystem* rs = mRoot->getRenderSystemByName(renderer);

    // set all given render system options
    for (auto & option : options)
    {
        rs->setConfigOption(option.first, option.second);
    }

    mRoot->queueEndRendering();   // break from render loop
}

void ApplicationContextBase::shutdown()
{
    const auto& gpuMgr = Ogre::GpuProgramManager::getSingleton();
    if (gpuMgr.getSaveMicrocodesToCache() && gpuMgr.isCacheDirty())
    {
        Ogre::String path = mFSLayer->getWritablePath(SHADER_CACHE_FILENAME);
        std::fstream outFile(path.c_str(), std::ios::out | std::ios::binary);

        if (outFile.is_open())
        {
            Ogre::LogManager::getSingleton().logMessage(::std::format("Writing shader cache to {}", path));
            Ogre::DataStreamPtr ostream(new Ogre::FileStreamDataStream(path, &outFile, false));
            gpuMgr.saveMicrocodeCache(ostream);
        }
        else
            Ogre::LogManager::getSingleton().logWarning(::std::format("Cannot open shader cache for writing {}", path));
    }

    // Destroy the RT Shader System.
    destroyRTShaderSystem();

    for(auto & mWindow : std::ranges::reverse_view(mWindows))
    {
        _destroyWindow(mWindow);
    }
    mWindows.clear();

    if (mOverlaySystem)
    {
        delete mOverlaySystem;
    }

    mInputListeners.clear();
}

void ApplicationContextBase::pollEvents()
{
    // just avoid "window not responding"
    WindowEventUtilities::messagePump();
}

}
