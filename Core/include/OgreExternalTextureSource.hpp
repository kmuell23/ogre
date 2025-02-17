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

 

export module Ogre.Core:ExternalTextureSource;

/***************************************************************************
OgreExternalTextureSource.h  -  
    Base class that texture plugins need to derive from. This provides the hooks
    necessary for a plugin developer to easily extend the functionality of dynamic textures.
    It makes creation/destruction of dynamic textures more streamlined. While the plugin
    will need to talk with Ogre for the actual modification of textures, this class allows
    easy integration with Ogre apps. Material script files can be used to aid in the 
    creation of dynamic textures. Functionality can be added that is not defined here
    through the use of the base dictionary. For an example of how to use this class and the
    string interface see ffmpegVideoPlugIn.

-------------------
date                 : Jan 1 2004
email                : pjcast@yahoo.com
***************************************************************************/
export import :Prerequisites;
export import :ResourceGroupManager;
export import :StringInterface;

export
namespace Ogre
{
    /** \addtogroup Core
    *  @{
    */
    /** \addtogroup Materials
    *  @{
    */
    /** Enum for type of texture play mode */
    enum class eTexturePlayMode
    {
        Pause = 0,         /// Video starts out paused
        Play_ASAP = 1,     /// Video starts playing as soon as possible
        Play_Looping = 2   /// Video Plays Instantly && Loops
    };

    /** IMPORTANT: **Plugins must override default dictionary name!** 
    Base class that texture plugins derive from. Any specific 
    requirements that the plugin needs to have defined before 
    texture/material creation must be define using the stringinterface
    before calling create defined texture... or it will fail, though, it 
    is up to the plugin to report errors to the log file, or raise an 
    exception if need be. */
    class ExternalTextureSource : public StringInterface
    {
    public:
        /** Constructor */
        ExternalTextureSource();
        /** Virtual destructor */
        ~ExternalTextureSource() override = default;

        //--------------------------------------------------------//
        //Base Functions that work with Command String Interface... Or can be called
        //manually to create video through code 

        /// Sets an input file name - if needed by plugin
        void setInputName( std::string_view sIN ) { mInputFileName = sIN; }
        /// Gets currently set input file name
        [[nodiscard]] auto getInputName( ) const noexcept -> std::string_view { return mInputFileName; }
        /// Sets the frames per second - plugin may or may not use this
        void setFPS( int iFPS ) { mFramesPerSecond = iFPS; }
        /// Gets currently set frames per second
        [[nodiscard]] auto getFPS( ) const noexcept -> int { return mFramesPerSecond; }
        /// Sets a play mode
        void setPlayMode( eTexturePlayMode eMode )  { mMode = eMode; }
        /// Gets currently set play mode
        [[nodiscard]] auto getPlayMode() const noexcept -> eTexturePlayMode { return mMode; }

        /// Used for attaching texture to Technique, State, and texture unit layer
        void setTextureTecPassStateLevel( int t, int p, int s ) 
                { mTechniqueLevel = t;mPassLevel = p;mStateLevel = s; }
        /// Get currently selected Texture attribs.
        void getTextureTecPassStateLevel( int& t, int& p, int& s ) const
                {t = mTechniqueLevel;   p = mPassLevel; s = mStateLevel;}
        
        /** Call from derived classes to ensure the dictionary is setup */
        void addBaseParams();

        /** Returns the string name of this Plugin (as set by the Plugin)*/
        [[nodiscard]] auto getPluginStringName( ) const noexcept -> std::string_view { return mPluginName; }
        /** Returns dictionary name */
        [[nodiscard]] auto getDictionaryStringName( ) const noexcept -> std::string_view { return mDictionaryName; }

        //Pure virtual functions that plugins must Override
        /** Call this function from manager to init system */
        virtual auto initialise() -> bool = 0;
        /** Shuts down Plugin */
        virtual void shutDown() = 0;

        /** Creates a texture into an already defined material or one that is created new
        (it's up to plugin to use a material or create one)
        Before calling, ensure that needed params have been defined via the stringInterface
        or regular methods */
        virtual void createDefinedTexture( std::string_view sMaterialName,
            std::string_view groupName = ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME) = 0;
        /** What this destroys is dependent on the plugin... See specific plugin
        doc to know what is all destroyed (normally, plugins will destroy only
        what they created, or used directly - ie. just texture unit) */
        virtual void destroyAdvancedTexture( std::string_view sTextureName,
            std::string_view groupName = ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME) = 0;

    protected:
        /// String Name of this Plugin
        String mPluginName;
    
        //------ Vars used for setting/getting dictionary stuff -----------//
        eTexturePlayMode mMode;
        
        String mInputFileName;
        
        bool mUpdateEveryFrame;
        
        int mFramesPerSecond,
            mTechniqueLevel{0},
            mPassLevel{0}, 
            mStateLevel{0};
        //------------------------------------------------------------------//

    protected:
        /** The string name of the dictionary name - each plugin
        must override default name */
        String mDictionaryName;
    };
    /** @} */
    /** @} */
}
