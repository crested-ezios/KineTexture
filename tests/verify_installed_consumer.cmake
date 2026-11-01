# Run as a CTest test after building the parent project. Each run keeps an
# isolated install and consumer build for diagnosis; no existing prefix is removed.
foreach(required IN ITEMS SOURCE_DIR BINARY_DIR GENERATOR)
    if(NOT DEFINED KINETEXTURE_${required} OR "${KINETEXTURE_${required}}" STREQUAL "")
        message(FATAL_ERROR "Missing KINETEXTURE_${required}")
    endif()
endforeach()

function(run_checked)
    execute_process(COMMAND ${ARGV} RESULT_VARIABLE result)
    if(NOT "${result}" STREQUAL "0")
        message(FATAL_ERROR "Installed-package check failed (${result}): ${ARGV}")
    endif()
endfunction()

string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef run_id)
set(check_dir "${KINETEXTURE_BINARY_DIR}/installed-consumer/${run_id}")
set(original_prefix "${check_dir}/original prefix")
set(relocated_prefix "${check_dir}/relocated prefix")
set(config_args)
if(NOT "${KINETEXTURE_CONFIG}" STREQUAL "")
    list(APPEND config_args --config "${KINETEXTURE_CONFIG}")
endif()

run_checked("${CMAKE_COMMAND}" --install "${KINETEXTURE_BINARY_DIR}"
    --prefix "${original_prefix}" ${config_args})
# Resolve the source before moving the directory; both ends must remain in this
# run's own build-tree scratch directory, including when paths contain spaces.
file(REAL_PATH "${check_dir}" resolved_check_dir)
file(REAL_PATH "${original_prefix}" resolved_original_prefix)
file(RELATIVE_PATH source_relative "${resolved_check_dir}" "${resolved_original_prefix}")
get_filename_component(resolved_destination "${relocated_prefix}" ABSOLUTE)
file(RELATIVE_PATH destination_relative "${resolved_check_dir}" "${resolved_destination}")
if(NOT source_relative STREQUAL "original prefix" OR
   NOT destination_relative STREQUAL "relocated prefix")
    message(FATAL_ERROR "Refusing to relocate a prefix outside this check's scratch directory")
endif()
file(RENAME "${original_prefix}" "${relocated_prefix}")

set(generator_args -G "${KINETEXTURE_GENERATOR}")
if(NOT "${KINETEXTURE_GENERATOR_PLATFORM}" STREQUAL "")
    list(APPEND generator_args -A "${KINETEXTURE_GENERATOR_PLATFORM}")
endif()
if(NOT "${KINETEXTURE_GENERATOR_TOOLSET}" STREQUAL "")
    list(APPEND generator_args -T "${KINETEXTURE_GENERATOR_TOOLSET}")
endif()
if(NOT "${KINETEXTURE_MAKE_PROGRAM}" STREQUAL "")
    list(APPEND generator_args "-DCMAKE_MAKE_PROGRAM=${KINETEXTURE_MAKE_PROGRAM}")
endif()

# No source-tree include directory or build-tree package is supplied to either
# independent project: find_package must resolve the relocated installation.
foreach(project IN ITEMS consumer two_bone_ik)
    if(project STREQUAL "consumer")
        set(source "${KINETEXTURE_SOURCE_DIR}/tests/consumer")
    else()
        set(source "${KINETEXTURE_SOURCE_DIR}/examples/two_bone_ik")
    endif()
    set(build "${check_dir}/${project}")
    run_checked("${CMAKE_COMMAND}" -S "${source}" -B "${build}"
        ${generator_args}
        "-DCMAKE_CXX_COMPILER=${KINETEXTURE_CXX_COMPILER}"
        "-DCMAKE_BUILD_TYPE=${KINETEXTURE_CONFIG}"
        "-DCMAKE_PREFIX_PATH=${relocated_prefix}"
        -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF
        -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF)
    run_checked("${CMAKE_COMMAND}" --build "${build}" ${config_args})
    run_checked("${KINETEXTURE_CTEST_COMMAND}" --test-dir "${build}"
        -C "${KINETEXTURE_CONFIG}" --output-on-failure)
endforeach()
message(STATUS "Relocated installed consumers passed: ${check_dir}")
