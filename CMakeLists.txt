cmake_minimum_required(VERSION 3.20)
project(KineTexture LANGUAGES CXX)
include(GNUInstallDirs)

add_library(kinetexture_core INTERFACE)
add_library(KineTexture::Core ALIAS kinetexture_core)
set_target_properties(kinetexture_core PROPERTIES EXPORT_NAME Core)
target_compile_features(kinetexture_core INTERFACE cxx_std_20)
target_include_directories(kinetexture_core INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

option(KINETEXTURE_BUILD_GKCLIP_TOOLS "Build native development-only gkclip export targets" ON)
if(KINETEXTURE_BUILD_GKCLIP_TOOLS)
    foreach(tool IN ITEMS gkclip2vmd gkclip2smpl)
        add_executable(${tool} tools/gkclip_export/main.cpp)
        target_link_libraries(${tool} PRIVATE KineTexture::Core)
        target_compile_definitions(${tool} PRIVATE GK_EXPORT_NAME="${tool}")
        if(tool STREQUAL "gkclip2vmd")
            target_compile_definitions(${tool} PRIVATE GK_EXPORT_VMD=1)
        else()
            target_compile_definitions(${tool} PRIVATE GK_EXPORT_VMD=0)
        endif()
        if(MSVC)
            target_compile_options(${tool} PRIVATE /W4 /utf-8)
        else()
            target_compile_options(${tool} PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        install(TARGETS ${tool} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    endforeach()
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(kinetexture_core_tests tests/test_core.cpp)
    target_link_libraries(kinetexture_core_tests PRIVATE KineTexture::Core)
    if(MSVC)
        target_compile_options(kinetexture_core_tests PRIVATE /W4 /utf-8)
    else()
        target_compile_options(kinetexture_core_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME kinetexture.core COMMAND kinetexture_core_tests)
    add_executable(kinetexture_clean1_tests tests/test_clean1.cpp)
    target_link_libraries(kinetexture_clean1_tests PRIVATE KineTexture::Core)
    if(MSVC)
        target_compile_options(kinetexture_clean1_tests PRIVATE /W4 /utf-8)
    endif()
    add_test(NAME kinetexture.clean1 COMMAND kinetexture_clean1_tests)
    add_executable(kinetexture_native_tests tests/test_native.cpp)
    target_link_libraries(kinetexture_native_tests PRIVATE KineTexture::Core)
    if(MSVC)
        target_compile_options(kinetexture_native_tests PRIVATE /W4 /utf-8)
    else()
        target_compile_options(kinetexture_native_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME kinetexture.native COMMAND kinetexture_native_tests)
    add_test(NAME kinetexture.installed_package
        COMMAND ${CMAKE_COMMAND}
            "-DKINETEXTURE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DKINETEXTURE_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
            "-DKINETEXTURE_CONFIG=$<CONFIG>"
            "-DKINETEXTURE_GENERATOR=${CMAKE_GENERATOR}"
            "-DKINETEXTURE_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
            "-DKINETEXTURE_GENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
            "-DKINETEXTURE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
            "-DKINETEXTURE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
            "-DKINETEXTURE_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_installed_consumer.cmake)
    foreach(export_test IN ITEMS gkclip_vmd smpl_export gkclip_adapter gkclip_presets)
        add_executable(kinetexture_${export_test}_tests tests/test_${export_test}.cpp)
        target_link_libraries(kinetexture_${export_test}_tests PRIVATE KineTexture::Core)
        if(MSVC)
            target_compile_options(kinetexture_${export_test}_tests PRIVATE /W4 /utf-8)
        else()
            target_compile_options(kinetexture_${export_test}_tests PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        add_test(NAME kinetexture.${export_test} COMMAND kinetexture_${export_test}_tests)
    endforeach()
    if(KINETEXTURE_BUILD_GKCLIP_TOOLS)
        find_package(Python3 COMPONENTS Interpreter QUIET)
        if(Python3_Interpreter_FOUND)
            add_test(NAME kinetexture.gkclip_cli COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_gkclip_cli.py
                $<TARGET_FILE:gkclip2vmd> $<TARGET_FILE:gkclip2smpl>)
        endif()
    endif()
    # The reviewed export may include the legacy codec as a separate adapter.
    # The Core target never depends on it.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/adapters/gakumas/GkMotionData.hpp")
        add_executable(kinetexture_gkclip_e2e tests/kinetexture/gkclip_e2e.cpp)
        target_link_libraries(kinetexture_gkclip_e2e PRIVATE KineTexture::Core)
        target_include_directories(kinetexture_gkclip_e2e PRIVATE adapters/gakumas)
        if(MSVC)
            target_compile_options(kinetexture_gkclip_e2e PRIVATE /utf-8)
        endif()
        add_test(NAME kinetexture.gkclip_e2e COMMAND kinetexture_gkclip_e2e
            "${CMAKE_CURRENT_SOURCE_DIR}/demo/motion.gkclip")
    endif()
endif()

include(CMakePackageConfigHelpers)
install(TARGETS kinetexture_core EXPORT KineTextureTargets)
install(DIRECTORY src/KineTexture/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/KineTexture
    FILES_MATCHING PATTERN "*.hpp")
install(EXPORT KineTextureTargets NAMESPACE KineTexture::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/KineTexture)
configure_package_config_file(src/KineTexture/cmake/KineTextureConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/KineTextureConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/KineTexture)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/KineTextureConfig.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/KineTexture)
install(FILES src/KineTexture/LICENSE src/KineTexture/NOTICE
    DESTINATION ${CMAKE_INSTALL_DATADIR}/KineTexture)
install(FILES src/KineTexture/README.md src/KineTexture/CHANNEL_SEMANTICS.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/KineTexture)
