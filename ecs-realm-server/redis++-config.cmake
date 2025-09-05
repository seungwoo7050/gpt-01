########## MACROS ###########################################################################
#############################################################################################

# Requires CMake > 3.15
if(${CMAKE_VERSION} VERSION_LESS "3.15")
    message(FATAL_ERROR "The 'CMakeDeps' generator only works with CMake >= 3.15")
endif()

if(redis++_FIND_QUIETLY)
    set(redis++_MESSAGE_MODE VERBOSE)
else()
    set(redis++_MESSAGE_MODE STATUS)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/cmakedeps_macros.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/redis++Targets.cmake)
include(CMakeFindDependencyMacro)

check_build_type_defined()

foreach(_DEPENDENCY ${redis-plus-plus_FIND_DEPENDENCY_NAMES} )
    # Check that we have not already called a find_package with the transitive dependency
    if(NOT ${_DEPENDENCY}_FOUND)
        find_dependency(${_DEPENDENCY} REQUIRED ${${_DEPENDENCY}_FIND_MODE})
    endif()
endforeach()

set(redis++_VERSION_STRING "1.3.8")
set(redis++_INCLUDE_DIRS ${redis-plus-plus_INCLUDE_DIRS_RELEASE} )
set(redis++_INCLUDE_DIR ${redis-plus-plus_INCLUDE_DIRS_RELEASE} )
set(redis++_LIBRARIES ${redis-plus-plus_LIBRARIES_RELEASE} )
set(redis++_DEFINITIONS ${redis-plus-plus_DEFINITIONS_RELEASE} )


# Only the last installed configuration BUILD_MODULES are included to avoid the collision
foreach(_BUILD_MODULE ${redis-plus-plus_BUILD_MODULES_PATHS_RELEASE} )
    message(${redis++_MESSAGE_MODE} "Conan: Including build module from '${_BUILD_MODULE}'")
    include(${_BUILD_MODULE})
endforeach()


