# Avoid multiple calls to find_package to append duplicated properties to the targets
include_guard()########### VARIABLES #######################################################################
#############################################################################################
set(redis-plus-plus_FRAMEWORKS_FOUND_RELEASE "") # Will be filled later
conan_find_apple_frameworks(redis-plus-plus_FRAMEWORKS_FOUND_RELEASE "${redis-plus-plus_FRAMEWORKS_RELEASE}" "${redis-plus-plus_FRAMEWORK_DIRS_RELEASE}")

set(redis-plus-plus_LIBRARIES_TARGETS "") # Will be filled later


######## Create an interface target to contain all the dependencies (frameworks, system and conan deps)
if(NOT TARGET redis-plus-plus_DEPS_TARGET)
    add_library(redis-plus-plus_DEPS_TARGET INTERFACE IMPORTED)
endif()

set_property(TARGET redis-plus-plus_DEPS_TARGET
             APPEND PROPERTY INTERFACE_LINK_LIBRARIES
             $<$<CONFIG:Release>:${redis-plus-plus_FRAMEWORKS_FOUND_RELEASE}>
             $<$<CONFIG:Release>:${redis-plus-plus_SYSTEM_LIBS_RELEASE}>
             $<$<CONFIG:Release>:hiredis::hiredis>)

####### Find the libraries declared in cpp_info.libs, create an IMPORTED target for each one and link the
####### redis-plus-plus_DEPS_TARGET to all of them
conan_package_library_targets("${redis-plus-plus_LIBS_RELEASE}"    # libraries
                              "${redis-plus-plus_LIB_DIRS_RELEASE}" # package_libdir
                              "${redis-plus-plus_BIN_DIRS_RELEASE}" # package_bindir
                              "${redis-plus-plus_LIBRARY_TYPE_RELEASE}"
                              "${redis-plus-plus_IS_HOST_WINDOWS_RELEASE}"
                              redis-plus-plus_DEPS_TARGET
                              redis-plus-plus_LIBRARIES_TARGETS  # out_libraries_targets
                              "_RELEASE"
                              "redis-plus-plus"    # package_name
                              "${redis-plus-plus_NO_SONAME_MODE_RELEASE}")  # soname

# FIXME: What is the result of this for multi-config? All configs adding themselves to path?
set(CMAKE_MODULE_PATH ${redis-plus-plus_BUILD_DIRS_RELEASE} ${CMAKE_MODULE_PATH})

########## COMPONENTS TARGET PROPERTIES Release ########################################

    ########## COMPONENT redis++::redis++_static #############

        set(redis-plus-plus_redis++_redis++_static_FRAMEWORKS_FOUND_RELEASE "")
        conan_find_apple_frameworks(redis-plus-plus_redis++_redis++_static_FRAMEWORKS_FOUND_RELEASE "${redis-plus-plus_redis++_redis++_static_FRAMEWORKS_RELEASE}" "${redis-plus-plus_redis++_redis++_static_FRAMEWORK_DIRS_RELEASE}")

        set(redis-plus-plus_redis++_redis++_static_LIBRARIES_TARGETS "")

        ######## Create an interface target to contain all the dependencies (frameworks, system and conan deps)
        if(NOT TARGET redis-plus-plus_redis++_redis++_static_DEPS_TARGET)
            add_library(redis-plus-plus_redis++_redis++_static_DEPS_TARGET INTERFACE IMPORTED)
        endif()

        set_property(TARGET redis-plus-plus_redis++_redis++_static_DEPS_TARGET
                     APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                     $<$<CONFIG:Release>:${redis-plus-plus_redis++_redis++_static_FRAMEWORKS_FOUND_RELEASE}>
                     $<$<CONFIG:Release>:${redis-plus-plus_redis++_redis++_static_SYSTEM_LIBS_RELEASE}>
                     $<$<CONFIG:Release>:${redis-plus-plus_redis++_redis++_static_DEPENDENCIES_RELEASE}>
                     )

        ####### Find the libraries declared in cpp_info.component["xxx"].libs,
        ####### create an IMPORTED target for each one and link the 'redis-plus-plus_redis++_redis++_static_DEPS_TARGET' to all of them
        conan_package_library_targets("${redis-plus-plus_redis++_redis++_static_LIBS_RELEASE}"
                              "${redis-plus-plus_redis++_redis++_static_LIB_DIRS_RELEASE}"
                              "${redis-plus-plus_redis++_redis++_static_BIN_DIRS_RELEASE}" # package_bindir
                              "${redis-plus-plus_redis++_redis++_static_LIBRARY_TYPE_RELEASE}"
                              "${redis-plus-plus_redis++_redis++_static_IS_HOST_WINDOWS_RELEASE}"
                              redis-plus-plus_redis++_redis++_static_DEPS_TARGET
                              redis-plus-plus_redis++_redis++_static_LIBRARIES_TARGETS
                              "_RELEASE"
                              "redis-plus-plus_redis++_redis++_static"
                              "${redis-plus-plus_redis++_redis++_static_NO_SONAME_MODE_RELEASE}")


        ########## TARGET PROPERTIES #####################################
        set_property(TARGET redis++::redis++_static
                     APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                     $<$<CONFIG:Release>:${redis-plus-plus_redis++_redis++_static_OBJECTS_RELEASE}>
                     $<$<CONFIG:Release>:${redis-plus-plus_redis++_redis++_static_LIBRARIES_TARGETS}>
                     )

        if("${redis-plus-plus_redis++_redis++_static_LIBS_RELEASE}" STREQUAL "")
            # If the component is not declaring any "cpp_info.components['foo'].libs" the system, frameworks etc are not
            # linked to the imported targets and we need to do it to the global target
            set_property(TARGET redis++::redis++_static
                         APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                         redis-plus-plus_redis++_redis++_static_DEPS_TARGET)
        endif()

        set_property(TARGET redis++::redis++_static APPEND PROPERTY INTERFACE_LINK_OPTIONS
                     $<$<CONFIG:Release>:${redis-plus-plus_redis++_redis++_static_LINKER_FLAGS_RELEASE}>)
        set_property(TARGET redis++::redis++_static APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES
                     $<$<CONFIG:Release>:${redis-plus-plus_redis++_redis++_static_INCLUDE_DIRS_RELEASE}>)
        set_property(TARGET redis++::redis++_static APPEND PROPERTY INTERFACE_LINK_DIRECTORIES
                     $<$<CONFIG:Release>:${redis-plus-plus_redis++_redis++_static_LIB_DIRS_RELEASE}>)
        set_property(TARGET redis++::redis++_static APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS
                     $<$<CONFIG:Release>:${redis-plus-plus_redis++_redis++_static_COMPILE_DEFINITIONS_RELEASE}>)
        set_property(TARGET redis++::redis++_static APPEND PROPERTY INTERFACE_COMPILE_OPTIONS
                     $<$<CONFIG:Release>:${redis-plus-plus_redis++_redis++_static_COMPILE_OPTIONS_RELEASE}>)


    ########## AGGREGATED GLOBAL TARGET WITH THE COMPONENTS #####################
    set_property(TARGET redis++::redis++_static APPEND PROPERTY INTERFACE_LINK_LIBRARIES redis++::redis++_static)

########## For the modules (FindXXX)
set(redis-plus-plus_LIBRARIES_RELEASE redis++::redis++_static)
