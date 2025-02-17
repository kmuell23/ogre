#-------------------------------------------------------------------
# This file is part of the CMake build system for OGRE
#     (Object-oriented Graphics Rendering Engine)
# For the latest info, see http://www.ogre3d.org/
#
# The contents of this file are placed in the public domain. Feel
# free to make use of it in any way you like.
#-------------------------------------------------------------------

set(HEADER_FILES
    include/CubeMappingTest.hpp
    include/StencilShadowTest.hpp
    include/VTestPlugin.hpp)

set(SOURCE_FILES
    src/StencilShadowTest.cpp
    src/CubeMappingTest.cpp
    src/VTestPlugin.cpp)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/Samples/Common/include)
include_directories(${PROJECT_SOURCE_DIR}/Samples/Simple/include)
include_directories(${PROJECT_SOURCE_DIR}/Tests/VisualTests/Common/include)

add_module(
    VTests.hpp
PARTITION
    ${HEADER_FILES}
IMPLEMENTATION
    ${SOURCE_FILES}
)

target_link_libraries(Ogre.Tests.VisualTests.VTests PRIVATE ${OGRE_LIBRARIES})
#ogre_config_sample_lib(VTests)

if (APPLE AND NOT APPLE_IOS)
    # Set the INSTALL_PATH so that Samples can be installed in the application package
    set_target_properties(VTests
       PROPERTIES BUILD_WITH_INSTALL_RPATH 1
       INSTALL_NAME_DIR "@executable_path/../Plugins"
    )
endif()

