# Avoid multiple calls to find_package to append duplicated properties to the targets
include_guard()########### VARIABLES #######################################################################
#############################################################################################
set(cli11_FRAMEWORKS_FOUND_RELEASE "") # Will be filled later
conan_find_apple_frameworks(cli11_FRAMEWORKS_FOUND_RELEASE "${cli11_FRAMEWORKS_RELEASE}" "${cli11_FRAMEWORK_DIRS_RELEASE}")

set(cli11_LIBRARIES_TARGETS "") # Will be filled later


######## Create an interface target to contain all the dependencies (frameworks, system and conan deps)
if(NOT TARGET cli11_DEPS_TARGET)
    add_library(cli11_DEPS_TARGET INTERFACE IMPORTED)
endif()

set_property(TARGET cli11_DEPS_TARGET
             APPEND PROPERTY INTERFACE_LINK_LIBRARIES
             $<$<CONFIG:Release>:${cli11_FRAMEWORKS_FOUND_RELEASE}>
             $<$<CONFIG:Release>:${cli11_SYSTEM_LIBS_RELEASE}>
             $<$<CONFIG:Release>:>)

####### Find the libraries declared in cpp_info.libs, create an IMPORTED target for each one and link the
####### cli11_DEPS_TARGET to all of them
conan_package_library_targets("${cli11_LIBS_RELEASE}"    # libraries
                              "${cli11_LIB_DIRS_RELEASE}" # package_libdir
                              "${cli11_BIN_DIRS_RELEASE}" # package_bindir
                              "${cli11_LIBRARY_TYPE_RELEASE}"
                              "${cli11_IS_HOST_WINDOWS_RELEASE}"
                              cli11_DEPS_TARGET
                              cli11_LIBRARIES_TARGETS  # out_libraries_targets
                              "_RELEASE"
                              "cli11"    # package_name
                              "${cli11_NO_SONAME_MODE_RELEASE}")  # soname

# FIXME: What is the result of this for multi-config? All configs adding themselves to path?
set(CMAKE_MODULE_PATH ${cli11_BUILD_DIRS_RELEASE} ${CMAKE_MODULE_PATH})

########## GLOBAL TARGET PROPERTIES Release ########################################
    set_property(TARGET CLI11::CLI11
                 APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                 $<$<CONFIG:Release>:${cli11_OBJECTS_RELEASE}>
                 $<$<CONFIG:Release>:${cli11_LIBRARIES_TARGETS}>
                 )

    if("${cli11_LIBS_RELEASE}" STREQUAL "")
        # If the package is not declaring any "cpp_info.libs" the package deps, system libs,
        # frameworks etc are not linked to the imported targets and we need to do it to the
        # global target
        set_property(TARGET CLI11::CLI11
                     APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                     cli11_DEPS_TARGET)
    endif()

    set_property(TARGET CLI11::CLI11
                 APPEND PROPERTY INTERFACE_LINK_OPTIONS
                 $<$<CONFIG:Release>:${cli11_LINKER_FLAGS_RELEASE}>)
    set_property(TARGET CLI11::CLI11
                 APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES
                 $<$<CONFIG:Release>:${cli11_INCLUDE_DIRS_RELEASE}>)
    # Necessary to find LINK shared libraries in Linux
    set_property(TARGET CLI11::CLI11
                 APPEND PROPERTY INTERFACE_LINK_DIRECTORIES
                 $<$<CONFIG:Release>:${cli11_LIB_DIRS_RELEASE}>)
    set_property(TARGET CLI11::CLI11
                 APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS
                 $<$<CONFIG:Release>:${cli11_COMPILE_DEFINITIONS_RELEASE}>)
    set_property(TARGET CLI11::CLI11
                 APPEND PROPERTY INTERFACE_COMPILE_OPTIONS
                 $<$<CONFIG:Release>:${cli11_COMPILE_OPTIONS_RELEASE}>)

########## For the modules (FindXXX)
set(cli11_LIBRARIES_RELEASE CLI11::CLI11)
