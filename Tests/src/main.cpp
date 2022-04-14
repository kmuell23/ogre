#include <gtest/gtest.h>

#include "OgreLog.h"
#include "OgreLogManager.h"

int main(int argc, char *argv[])
{
    Ogre::LogManager logMgr{};
    logMgr.createLog("OgreTest.log", true, false);
    logMgr.setMinLogLevel(Ogre::LML_TRIVIAL);

    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
