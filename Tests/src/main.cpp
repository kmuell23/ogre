module;

#include <gtest/gtest.h>

module Ogre.Tests;

import Ogre.Core;

auto main(int argc, char *argv[]) -> int
{
    auto* logMgr = new Ogre::LogManager();
    logMgr->createLog("OgreTest.log", true, false);
    logMgr->setMinLogLevel(Ogre::LML_TRIVIAL);

    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
