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
export module Ogre.Tests.VisualTests.Common:TestBatch;

export import :ImageValidator;

export import Ogre.Core;

export import <utility>;

export
class TestBatch;

export
using TestBatchSet = std::set<TestBatch, std::greater<>>;

/** Represents the output from running a batch of tests
 *        (i.e. a single run of the TestContext) */
export
class TestBatch : public Ogre::GeneralAllocatedObject
{
public:

    // image files from this batch
    std::vector<Ogre::String> images;
    // This set's name (usually of the form: [TestPluginName]_[Timestamp])
    Ogre::String name;
    // name of the tets plugin 
    Ogre::String plugin;
    // version string
    Ogre::String version;
    // timestamp (when the batch was run)
    Ogre::String timestamp;
    // user comment made at the time of the run
    Ogre::String comment;
    // resolution the batch was run at
    size_t resolutionX;
    size_t resolutionY;

    /** Initialize based on a config file
     *        @param info Reference to loaded config file with details about the set 
     *        @param directory The full path to this set's directory */
    TestBatch(Ogre::ConfigFile& info, std::string_view directory):mDirectory(directory)
    {
        // fill out basic info
        std::string const res{ info.getSetting("Resolution","Info") };
        resolutionX = atoi(res.c_str());
        resolutionY = atoi(res.substr(res.find('x')+1).c_str());
        version = info.getSetting("Version","Info");
        timestamp = info.getSetting("Time","Info");
        comment = info.getSetting("Comment","Info");
        name = info.getSetting("Name","Info");
        // grab image names
        const Ogre::ConfigFile::SettingsMultiMap& tests = info.getSettings("Tests");
        for(const auto & test : tests)
            images.push_back(test.second);
    }

    /** Manually initialize a batch object
     *        @param batchName The name of the overall test batch
     *        @param pluginName The name of the test plugin being used 
     *        @param timestamp The time the test was begun
     *        @param resx The width of the render window used
     *        @param resy The height of the render window used 
     *        @param directory The directory this batch is saved to */
    TestBatch(std::string_view batchName, std::string_view pluginName,
        std::string_view t, size_t resx, size_t resy, std::string_view directory)
        :name(batchName)
        ,plugin(pluginName)
        ,timestamp(t)
        ,comment("")
        ,resolutionX(resx)
        ,resolutionY(resy)
        ,mDirectory(directory)
    {
        Ogre::StringStream ver;
        ver<</*OGRE_VERSION_MAJOR*/13<<"."<</*OGRE_VERSION_MINOR*/3<<" ("<<
            /*OGRE_VERSION_NAME*/"Tsathoggua"<<") "<</*OGRE_VERSION_SUFFIX*/"Modernized";
        version = ver.str();
    }

    /** Returns whether or not the passed in set is comparable
        this means they must have the same resolution and image (test)
        names. */
    [[nodiscard]] auto canCompareWith(const TestBatch& other) const -> bool
    {
        if (resolutionX != other.resolutionX ||
            resolutionY != other.resolutionY ||
            images.size() != other.images.size())
            return false;

        for (unsigned int i = 0; i < images.size(); ++i)
            if(images[i] != other.images[i])
                return false;

        return true;
    }

    /** Gets the full path to the image at the specificied index */
    [[nodiscard]] auto getImagePath(size_t index) const -> Ogre::String
    {
        return ::std::format("{}/{}.png", mDirectory , images[index] );
    }

    /** Does image comparison on all images between these two sets */
    [[nodiscard]] auto compare(const TestBatch& other) const -> ComparisonResultVector
    {
        ComparisonResultVector out;
        if (!canCompareWith(other))
        {
            out.clear();
        }
        else
        {
            ImageValidator validator = ImageValidator(mDirectory, other.mDirectory);
            for (const auto & image : images)
                out.push_back(validator.compare(image));
        }
        return out;
    }

    // write the config file for this set
    void writeConfig()
    {
        // save a small .cfg file with some details about the batch
        std::ofstream config;
        config.open(::std::format("{}/info.cfg", mDirectory));

        if (config.is_open())
        {
            config<<"[Info]\n";
            config<<"Name="<<name<<"\n";
            config<<"Time="<<timestamp<<"\n";
            config<<"Resolution="<<resolutionX<<"x"<<resolutionY<<"\n";
            config<<"Comment="<<comment<<"\n";
            config<<"Version="<<version<<"\n";
            config<<"[Tests]\n";

            // add entries for each image
            for (auto & image : images)
                config<<"Test="<<image<<"\n";

            config.close();
        }
    }

    /** Greater than operator, so they can be sorted chronologically */
    [[nodiscard]] auto operator<=>(const TestBatch& other) const noexcept -> ::std::weak_ordering
    {
        using namespace Ogre;
        // due to the way timestamps are formatted, lexicographical ordering will also be chronological
        return timestamp <=> other.timestamp;
    }

    /** Loads all test batches found in a directory and returns a reference counted ptr 
     *    to a set containing all the valid batches */
    static auto loadTestBatches(Ogre::String directory) -> TestBatchSet
    {
        TestBatchSet out;
        // use ArchiveManager to get a list of all subdirectories
        Ogre::Archive* testDir = Ogre::ArchiveManager::getSingleton().load(directory, "FileSystem", true);
        Ogre::StringVectorPtr tests = testDir->list(false, true);
        for (unsigned int i = 0; i < tests->size(); ++i)
        {
            Ogre::ConfigFile info;
            
            // look for info.cfg, if none found, must not be a batch directory
            try
            {
                info.load(::std::format("{}{}/info.cfg", directory, (*tests)[i]));
            }
            catch (Ogre::FileNotFoundException& e)
            {
                continue;
            }

            out.insert(TestBatch(info, directory + (*tests)[i]));
        }

        return out;
    }

private:

    Ogre::String mDirectory;

};
