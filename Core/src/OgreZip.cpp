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

#include <sys/stat.h>
#include <zip.h>
// NOLINTBEGIN
#include <zip.h>

module Ogre.Core:Zip.Obj;

import :Archive;
import :DataStream;
import :Exception;
import :FileSystem;
import :Platform;
import :Prerequisites;
import :SharedPtr;
import :String;
import :StringVector;
import :Zip;

import <algorithm>;
import <cstddef>;
import <iosfwd>;
import <map>;
import <memory>;
import <string>;
import <utility>;
import <vector>;

// NOLINTEND
namespace Ogre {
namespace {
    class ZipArchive : public Archive
    {
    protected:
        /// Handle to root zip file
        zip_t* mZipFile{nullptr};
        MemoryDataStreamPtr mBuffer;
        /// File list (since zziplib seems to only allow scanning of dir tree once)
        FileInfoList mFileList;
    public:
        ZipArchive(const String& name, const String& archType, const uint8* externBuf = nullptr, size_t externBufSz = 0);
        ~ZipArchive() override;
        /// @copydoc Archive::isCaseSensitive
        [[nodiscard]]
        auto isCaseSensitive() const -> bool override { return true; }

        /// @copydoc Archive::load
        void load() override;
        /// @copydoc Archive::unload
        void unload() override;

        /// @copydoc Archive::open
        [[nodiscard]]
        auto open(const String& filename, bool readOnly = true) const -> DataStreamPtr override;

        /// @copydoc Archive::create
        auto create(const String& filename) -> DataStreamPtr override;

        /// @copydoc Archive::remove
        void remove(const String& filename) override;

        /// @copydoc Archive::list
        [[nodiscard]]
        auto list(bool recursive = true, bool dirs = false) const -> StringVectorPtr override;

        /// @copydoc Archive::listFileInfo
        [[nodiscard]]
        auto listFileInfo(bool recursive = true, bool dirs = false) const -> FileInfoListPtr override;

        /// @copydoc Archive::find
        [[nodiscard]]
        auto find(const String& pattern, bool recursive = true,
            bool dirs = false) const -> StringVectorPtr override;

        /// @copydoc Archive::findFileInfo
        [[nodiscard]]
        auto findFileInfo(const String& pattern, bool recursive = true,
            bool dirs = false) const -> FileInfoListPtr override;

        /// @copydoc Archive::exists
        [[nodiscard]]
        auto exists(const String& filename) const -> bool override;

        /// @copydoc Archive::getModifiedTime
        [[nodiscard]]
        auto getModifiedTime(const String& filename) const -> time_t override;
    };
}
    //-----------------------------------------------------------------------
    ZipArchive::ZipArchive(const String& name, const String& archType, const uint8* externBuf, size_t externBufSz)
        : Archive(name, archType) 
    {
        if(externBuf)
            mBuffer = std::make_shared<MemoryDataStream>(const_cast<uint8*>(externBuf), externBufSz);
    }
    //-----------------------------------------------------------------------
    ZipArchive::~ZipArchive()
    {
        unload();
    }
    //-----------------------------------------------------------------------
    void ZipArchive::load()
    {
        if (!mZipFile)
        {
            if(!mBuffer)
                mBuffer = std::make_shared<MemoryDataStream>(_openFileStream(mName, std::ios::binary));

            mZipFile = zip_stream_open((const char*)mBuffer->getPtr(), mBuffer->size(), 0, 'r');

            // Cache names
            int n = zip_entries_total(mZipFile);
            for (int i = 0; i < n; ++i) {
                FileInfo info;
                info.archive = this;

                zip_entry_openbyindex(mZipFile, i);

                info.filename = zip_entry_name(mZipFile);
                // Get basename / path
                StringUtil::splitFilename(info.filename, info.basename, info.path);

                // Get sizes
                info.uncompressedSize = zip_entry_size(mZipFile);
                info.compressedSize = zip_entry_comp_size(mZipFile);

                if (zip_entry_isdir(mZipFile))
                {
                    info.filename = info.filename.substr(0, info.filename.length() - 1);
                    StringUtil::splitFilename(info.filename, info.basename, info.path);
                    // Set compressed size to -1 for folders; anyway nobody will check
                    // the compressed size of a folder, and if he does, its useless anyway
                    info.compressedSize = size_t(-1);
                }

                zip_entry_close(mZipFile);
                mFileList.push_back(info);
            }
        }
    }
    //-----------------------------------------------------------------------
    void ZipArchive::unload()
    {
        if (mZipFile)
        {
            zip_close(mZipFile);
            mZipFile = nullptr;
            mFileList.clear();
            mBuffer.reset();
        }
    
    }
    //-----------------------------------------------------------------------
    auto ZipArchive::open(const String& filename, bool readOnly) const -> DataStreamPtr
    {
        // zip is not threadsafe
        String lookUpFileName = filename;

        bool open = zip_entry_open(mZipFile, lookUpFileName.c_str(), true) == 0;

        if (!open)
        {
            OGRE_EXCEPT(Exception::ERR_FILE_NOT_FOUND, "could not open "+lookUpFileName);
        }

        // Construct & return stream
        auto ret = std::make_shared<MemoryDataStream>(zip_entry_size(mZipFile));

        if(zip_entry_noallocread(mZipFile, ret->getPtr(), ret->size()) < 0)
            OGRE_EXCEPT(Exception::ERR_FILE_NOT_FOUND, "could not read "+lookUpFileName);
        zip_entry_close(mZipFile);

        return ret;
    }
    //---------------------------------------------------------------------
    auto ZipArchive::create(const String& filename) -> DataStreamPtr
    {
        OGRE_EXCEPT(Exception::ERR_NOT_IMPLEMENTED, "Modification of zipped archives is not implemented");
    }
    //---------------------------------------------------------------------
    void ZipArchive::remove(const String& filename)
    {
        OGRE_EXCEPT(Exception::ERR_NOT_IMPLEMENTED, "Modification of zipped archives is not implemented");
    }
    //-----------------------------------------------------------------------
    auto ZipArchive::list(bool recursive, bool dirs) const -> StringVectorPtr
    {
        StringVectorPtr ret = StringVectorPtr(new StringVector());

        FileInfoList::const_iterator i, iend;
        iend = mFileList.end();
        for (i = mFileList.begin(); i != iend; ++i)
            if ((dirs == (i->compressedSize == size_t (-1))) &&
                (recursive || i->path.empty()))
                ret->push_back(i->filename);

        return ret;
    }
    //-----------------------------------------------------------------------
    auto ZipArchive::listFileInfo(bool recursive, bool dirs) const -> FileInfoListPtr
    {
        auto* fil = new FileInfoList();
        FileInfoList::const_iterator i, iend;
        iend = mFileList.end();
        for (i = mFileList.begin(); i != iend; ++i)
            if ((dirs == (i->compressedSize == size_t (-1))) &&
                (recursive || i->path.empty()))
                fil->push_back(*i);

        return FileInfoListPtr(fil);
    }
    //-----------------------------------------------------------------------
    auto ZipArchive::find(const String& pattern, bool recursive, bool dirs) const -> StringVectorPtr
    {
        StringVectorPtr ret = StringVectorPtr(new StringVector());
        // If pattern contains a directory name, do a full match
        bool full_match = (pattern.find ('/') != String::npos) ||
                          (pattern.find ('\\') != String::npos);
        bool wildCard = pattern.find('*') != String::npos;
            
        FileInfoList::const_iterator i, iend;
        iend = mFileList.end();
        for (i = mFileList.begin(); i != iend; ++i)
            if ((dirs == (i->compressedSize == size_t (-1))) &&
                (recursive || full_match || wildCard))
                // Check basename matches pattern (zip is case insensitive)
                if (StringUtil::match(full_match ? i->filename : i->basename, pattern, false))
                    ret->push_back(i->filename);

        return ret;
    }
    //-----------------------------------------------------------------------
    auto ZipArchive::findFileInfo(const String& pattern, 
        bool recursive, bool dirs) const -> FileInfoListPtr
    {
        FileInfoListPtr ret = FileInfoListPtr(new FileInfoList());
        // If pattern contains a directory name, do a full match
        bool full_match = (pattern.find ('/') != String::npos) ||
                          (pattern.find ('\\') != String::npos);
        bool wildCard = pattern.find('*') != String::npos;

        FileInfoList::const_iterator i, iend;
        iend = mFileList.end();
        for (i = mFileList.begin(); i != iend; ++i)
            if ((dirs == (i->compressedSize == size_t (-1))) &&
                (recursive || full_match || wildCard))
                // Check name matches pattern (zip is case insensitive)
                if (StringUtil::match(full_match ? i->filename : i->basename, pattern, false))
                    ret->push_back(*i);

        return ret;
    }
    //-----------------------------------------------------------------------
    auto ZipArchive::exists(const String& filename) const -> bool
    {       
        String cleanName = filename;

        return std::find_if(mFileList.begin(), mFileList.end(), [&cleanName](const Ogre::FileInfo& fi) {
                   return fi.filename == cleanName;
               }) != mFileList.end();
    }
    //---------------------------------------------------------------------
    auto ZipArchive::getModifiedTime(const String& filename) const -> time_t
    {
        // Zziplib doesn't yet support getting the modification time of individual files
        // so just check the mod time of the zip itself
        struct stat tagStat;
        bool ret = (stat(mName.c_str(), &tagStat) == 0);

        if (ret)
        {
            return tagStat.st_mtime;
        }
        else
        {
            return 0;
        }

    }
    //-----------------------------------------------------------------------
    //  ZipArchiveFactory
    //-----------------------------------------------------------------------
    auto ZipArchiveFactory::createInstance( const String& name, bool readOnly ) -> Archive *
    {
        if(!readOnly)
            return nullptr;

        return new ZipArchive(name, getType());
    }
    //-----------------------------------------------------------------------
    auto ZipArchiveFactory::getType() const -> const String&
    {
        static String name = "Zip";
        return name;
    }
    //-----------------------------------------------------------------------
    //-----------------------------------------------------------------------
    //  EmbeddedZipArchiveFactory
    //-----------------------------------------------------------------------
    //-----------------------------------------------------------------------
    /// a struct to hold embedded file data
    struct EmbeddedFileData
    {
        const uint8 * fileData;
        size_t fileSize;
        size_t curPos;
        bool isFileOpened;
        EmbeddedZipArchiveFactory::DecryptEmbeddedZipFileFunc decryptFunc;
    };
    //-----------------------------------------------------------------------
    /// A type for a map between the file names to file index
    using EmbbedFileDataList = std::map<String, EmbeddedFileData>;

    namespace {
    /// A static list to store the embedded files data
    EmbbedFileDataList *gEmbeddedFileDataList;
    } // namespace {
    //-----------------------------------------------------------------------
    EmbeddedZipArchiveFactory::EmbeddedZipArchiveFactory() = default;
    EmbeddedZipArchiveFactory::~EmbeddedZipArchiveFactory() = default;
    //-----------------------------------------------------------------------
    auto EmbeddedZipArchiveFactory::createInstance( const String& name, bool readOnly ) -> Archive *
    {
        auto it = gEmbeddedFileDataList->find(name);
        if(it == gEmbeddedFileDataList->end())
            return nullptr;

        // TODO: decryptFunc

        return new ZipArchive(name, getType(), it->second.fileData, it->second.fileSize);
    }
    void EmbeddedZipArchiveFactory::destroyInstance(Archive* ptr)
    {
        removeEmbbeddedFile(ptr->getName());
        ZipArchiveFactory::destroyInstance(ptr);
    }
    //-----------------------------------------------------------------------
    auto EmbeddedZipArchiveFactory::getType() const -> const String&
    {
        static String name = "EmbeddedZip";
        return name;
    }
    //-----------------------------------------------------------------------
    void EmbeddedZipArchiveFactory::addEmbbeddedFile(const String& name, const uint8 * fileData, 
                                        size_t fileSize, DecryptEmbeddedZipFileFunc decryptFunc)
    {
        static bool needToInit = true;
        if(needToInit)
        {
            needToInit = false;

            // we can't be sure when global variables get initialized
            // meaning it is possible our list has not been init when this
            // function is being called. The solution is to use local
            // static members in this function an init the pointers for the
            // global here. We know for use that the local static variables
            // are create in this stage.
            static EmbbedFileDataList sEmbbedFileDataList;
            gEmbeddedFileDataList = &sEmbbedFileDataList;
        }

        EmbeddedFileData newEmbeddedFileData;
        newEmbeddedFileData.curPos = 0;
        newEmbeddedFileData.isFileOpened = false;
        newEmbeddedFileData.fileData = fileData;
        newEmbeddedFileData.fileSize = fileSize;
        newEmbeddedFileData.decryptFunc = decryptFunc;
        gEmbeddedFileDataList->emplace(name, newEmbeddedFileData);
    }
    //-----------------------------------------------------------------------
    void EmbeddedZipArchiveFactory::removeEmbbeddedFile( const String& name )
    {
        gEmbeddedFileDataList->erase(name);
    }
}
