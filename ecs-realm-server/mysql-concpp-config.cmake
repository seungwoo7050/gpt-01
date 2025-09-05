########## MACROS ###########################################################################
#############################################################################################

# Requires CMake > 3.15
if(${CMAKE_VERSION} VERSION_LESS "3.15")
    message(FATAL_ERROR "The 'CMakeDeps' generator only works with CMake >= 3.15")
endif()

if(mysql-concpp_FIND_QUIETLY)
    set(mysql-concpp_MESSAGE_MODE VERBOSE)
else()
    set(mysql-concpp_MESSAGE_MODE STATUS)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/cmakedeps_macros.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/mysql-concppTargets.cmake)
include(CMakeFindDependencyMacro)

check_build_type_defined()

foreach(_DEPENDENCY ${mysql-connector-cpp_FIND_DEPENDENCY_NAMES} )
    # Check that we have not already called a find_package with the transitive dependency
    if(NOT ${_DEPENDENCY}_FOUND)
        find_dependency(${_DEPENDENCY} REQUIRED ${${_DEPENDENCY}_FIND_MODE})
    endif()
endforeach()

set(mysql-concpp_VERSION_STRING "9.2.0")
set(mysql-concpp_INCLUDE_DIRS ${mysql-connector-cpp_INCLUDE_DIRS_RELEASE} )
set(mysql-concpp_INCLUDE_DIR ${mysql-connector-cpp_INCLUDE_DIRS_RELEASE} )
set(mysql-concpp_LIBRARIES ${mysql-connector-cpp_LIBRARIES_RELEASE} )
set(mysql-concpp_DEFINITIONS ${mysql-connector-cpp_DEFINITIONS_RELEASE} )


# Only the last installed configuration BUILD_MODULES are included to avoid the collision
foreach(_BUILD_MODULE ${mysql-connector-cpp_BUILD_MODULES_PATHS_RELEASE} )
    message(${mysql-concpp_MESSAGE_MODE} "Conan: Including build module from '${_BUILD_MODULE}'")
    include(${_BUILD_MODULE})
endforeach()


