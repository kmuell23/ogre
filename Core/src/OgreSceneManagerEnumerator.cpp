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

import :Exception;
import :LogManager;
import :SceneManagerEnumerator;

import <ostream>;
import <utility>;

namespace Ogre {
class RenderSystem;

    //-----------------------------------------------------------------------
    template<> SceneManagerEnumerator* Singleton<SceneManagerEnumerator>::msSingleton = nullptr;
    auto SceneManagerEnumerator::getSingletonPtr() -> SceneManagerEnumerator*
    {
        return msSingleton;
    }
    auto SceneManagerEnumerator::getSingleton() -> SceneManagerEnumerator&
    {  
        assert( msSingleton );  return ( *msSingleton );  
    }

    //-----------------------------------------------------------------------
    SceneManagerEnumerator::SceneManagerEnumerator()
         
    {
        addFactory(&mDefaultFactory);

    }
    //-----------------------------------------------------------------------
    SceneManagerEnumerator::~SceneManagerEnumerator()
    {
        // Destroy all remaining instances
        // Really should have shutdown and unregistered by now, but catch here in case
        Instances instancesCopy = mInstances;
        for (auto & i : instancesCopy)
        {
            // destroy instances
            for(auto & mFactorie : mFactories)
            {
                if (mFactorie->getMetaData().typeName == i.second->getTypeName())
                {
                    mFactorie->destroyInstance(i.second);
                    mInstances.erase(i.first);
                    break;
                }
            }

        }
        mInstances.clear();

    }
    //-----------------------------------------------------------------------
    void SceneManagerEnumerator::addFactory(SceneManagerFactory* fact)
    {
        mFactories.push_back(fact);
        // add to metadata
        mMetaDataList.push_back(&fact->getMetaData());
        // Log
        LogManager::getSingleton().logMessage("SceneManagerFactory for type '" +
            fact->getMetaData().typeName + "' registered.");
    }
    //-----------------------------------------------------------------------
    void SceneManagerEnumerator::removeFactory(SceneManagerFactory* fact)
    {
        OgreAssert(fact, "Cannot remove a null SceneManagerFactory");

        // destroy all instances for this factory
        for (auto i = mInstances.begin(); i != mInstances.end(); )
        {
            SceneManager* instance = i->second;
            if (instance->getTypeName() == fact->getMetaData().typeName)
            {
                fact->destroyInstance(instance);
                auto deli = i++;
                mInstances.erase(deli);
            }
            else
            {
                ++i;
            }
        }
        // remove from metadata
        for (auto m = mMetaDataList.begin(); m != mMetaDataList.end(); ++m)
        {
            if(*m == &(fact->getMetaData()))
            {
                mMetaDataList.erase(m);
                break;
            }
        }
        mFactories.remove(fact);
    }
    //-----------------------------------------------------------------------
    auto SceneManagerEnumerator::getMetaData(const String& typeName) const -> const SceneManagerMetaData*
    {
        for (auto i : mMetaDataList)
        {
            if (typeName == i->typeName)
            {
                return i;
            }
        }

        OGRE_EXCEPT(Exception::ERR_ITEM_NOT_FOUND, 
            "No metadata found for scene manager of type '" + typeName + "'",
            "SceneManagerEnumerator::createSceneManager");

    }

    //-----------------------------------------------------------------------
    auto SceneManagerEnumerator::createSceneManager(
        const String& typeName, const String& instanceName) -> SceneManager*
    {
        if (mInstances.find(instanceName) != mInstances.end())
        {
            OGRE_EXCEPT(Exception::ERR_DUPLICATE_ITEM, 
                "SceneManager instance called '" + instanceName + "' already exists",
                "SceneManagerEnumerator::createSceneManager");
        }

        SceneManager* inst = nullptr;
        for(auto & mFactorie : mFactories)
        {
            if (mFactorie->getMetaData().typeName == typeName)
            {
                if (instanceName.empty())
                {
                    // generate a name
                    StringStream s;
                    s << "SceneManagerInstance" << ++mInstanceCreateCount;
                    inst = mFactorie->createInstance(s.str());
                }
                else
                {
                    inst = mFactorie->createInstance(instanceName);
                }
                break;
            }
        }

        if (!inst)
        {
            // Error!
            OGRE_EXCEPT(Exception::ERR_ITEM_NOT_FOUND, 
                "No factory found for scene manager of type '" + typeName + "'",
                "SceneManagerEnumerator::createSceneManager");
        }

        /// assign rs if already configured
        if (mCurrentRenderSystem)
            inst->_setDestinationRenderSystem(mCurrentRenderSystem);

        mInstances[inst->getName()] = inst;
        
        return inst;
        

    }
    //-----------------------------------------------------------------------
    void SceneManagerEnumerator::destroySceneManager(SceneManager* sm)
    {
        OgreAssert(sm, "Cannot destroy a null SceneManager");

        // Erase instance from map
        mInstances.erase(sm->getName());

        // Find factory to destroy
        for(auto & mFactorie : mFactories)
        {
            if (mFactorie->getMetaData().typeName == sm->getTypeName())
            {
                mFactorie->destroyInstance(sm);
                break;
            }
        }

    }
    //-----------------------------------------------------------------------
    auto SceneManagerEnumerator::getSceneManager(const String& instanceName) const -> SceneManager*
    {
        auto i = mInstances.find(instanceName);
        if(i != mInstances.end())
        {
            return i->second;
        }
        else
        {
            OGRE_EXCEPT(Exception::ERR_ITEM_NOT_FOUND, 
                "SceneManager instance with name '" + instanceName + "' not found.",
                "SceneManagerEnumerator::getSceneManager");
        }

    }
    //---------------------------------------------------------------------
    auto SceneManagerEnumerator::hasSceneManager(const String& instanceName) const -> bool
    {
        return mInstances.find(instanceName) != mInstances.end();
    }

    //-----------------------------------------------------------------------
    auto SceneManagerEnumerator::getSceneManagers() const -> const SceneManagerEnumerator::Instances&
    {
        return mInstances;
    }
    //-----------------------------------------------------------------------
    void SceneManagerEnumerator::setRenderSystem(RenderSystem* rs)
    {
        mCurrentRenderSystem = rs;

        for (auto & mInstance : mInstances)
        {
            mInstance.second->_setDestinationRenderSystem(rs);
        }

    }
    //-----------------------------------------------------------------------
    void SceneManagerEnumerator::shutdownAll()
    {
        for (auto & mInstance : mInstances)
        {
            // shutdown instances (clear scene)
            mInstance.second->clearScene();            
        }

    }
    //-----------------------------------------------------------------------
    const String DefaultSceneManagerFactory::FACTORY_TYPE_NAME = "DefaultSceneManager";
    //-----------------------------------------------------------------------
    void DefaultSceneManagerFactory::initMetaData() const
    {
        mMetaData.typeName = FACTORY_TYPE_NAME;
        mMetaData.worldGeometrySupported = false;
    }
    //-----------------------------------------------------------------------
    auto DefaultSceneManagerFactory::createInstance(
        const String& instanceName) -> SceneManager*
    {
        return new DefaultSceneManager(instanceName);
    }
    //-----------------------------------------------------------------------
    //-----------------------------------------------------------------------
    DefaultSceneManager::DefaultSceneManager(const String& name)
        : SceneManager(name)
    {
    }
    //-----------------------------------------------------------------------
    DefaultSceneManager::~DefaultSceneManager()
    = default;
    //-----------------------------------------------------------------------
    auto DefaultSceneManager::getTypeName() const -> const String&
    {
        return DefaultSceneManagerFactory::FACTORY_TYPE_NAME;
    }
    //-----------------------------------------------------------------------


}
