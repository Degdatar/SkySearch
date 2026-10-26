cmake_minimum_required(VERSION 3.24)
project(SkySearch VERSION 0.1.0 LANGUAGES CXX)

include(FetchContent)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Keep in sync with the CommonLibSSE-NG branch used by the SkyOS source tree.
FetchContent_Declare(
    CommonLibSSE
    GIT_REPOSITORY https://github.com/alandtse/CommonLibSSE-NG.git
    GIT_TAG ng
    GIT_SUBMODULES ""
    GIT_SHALLOW TRUE
)

set(BUILD_TESTS OFF CACHE BOOL "Disable CommonLibSSE-NG tests" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "Disable CommonLibSSE-NG tests" FORCE)
set(ENABLE_SKYRIM_VR OFF CACHE BOOL "SkySearch supports Skyrim SE/AE, not VR" FORCE)

# CommonLib is a build dependency; its own install rules must not be copied into
# the deployable SkySearch package.
set(CMAKE_SKIP_INSTALL_RULES ON)
FetchContent_MakeAvailable(CommonLibSSE)
set(CMAKE_SKIP_INSTALL_RULES OFF)
include("${commonlibsse_SOURCE_DIR}/cmake/CommonLibSSE.cmake")

add_commonlibsse_plugin(SkySearch
    NAME "SkySearch"
    AUTHOR "Degdatar"
    VERSION ${PROJECT_VERSION}
    SOURCES
        src/skysearch.cpp
        src/app_module_entry.cpp
)

target_include_directories(SkySearch PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(SkySearch PRIVATE cxx_std_23)
target_compile_definitions(SkySearch PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    SKYOS_SKYSEARCH_EXPORTS
)

# The DLL is installed as a Skyrim data tree. SkyOS owns the shared shell and
# calls the exported search/action functions from this optional plugin.
install(TARGETS SkySearch RUNTIME DESTINATION "Data/SKSE/Plugins")
install(DIRECTORY data/PrismaUI/views/SkySearch
    DESTINATION "Data/PrismaUI/views")
install(DIRECTORY data/SKSE/Plugins/SkyOS/Mods/SkySearch
    DESTINATION "Data/SKSE/Plugins/SkyOS/Mods")
install(DIRECTORY data/SKSE/Plugins/SkyOS/Mods/SkyViewer/SkySearch
    DESTINATION "Data/SKSE/Plugins/SkyOS/Mods/SkyViewer")

set(CPACK_PACKAGE_NAME "SkySearch")
set(CPACK_PACKAGE_VENDOR "SkyOS")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "SkySearch-${PROJECT_VERSION}-SkyrimSE-AE")
set(CPACK_GENERATOR "ZIP")
include(CPack)
