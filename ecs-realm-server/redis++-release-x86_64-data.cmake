########### AGGREGATED COMPONENTS AND DEPENDENCIES FOR THE MULTI CONFIG #####################
#############################################################################################

list(APPEND redis-plus-plus_COMPONENT_NAMES redis++::redis++_static)
list(REMOVE_DUPLICATES redis-plus-plus_COMPONENT_NAMES)
if(DEFINED redis-plus-plus_FIND_DEPENDENCY_NAMES)
  list(APPEND redis-plus-plus_FIND_DEPENDENCY_NAMES hiredis)
  list(REMOVE_DUPLICATES redis-plus-plus_FIND_DEPENDENCY_NAMES)
else()
  set(redis-plus-plus_FIND_DEPENDENCY_NAMES hiredis)
endif()
set(hiredis_FIND_MODE "NO_MODULE")

########### VARIABLES #######################################################################
#############################################################################################
set(redis-plus-plus_PACKAGE_FOLDER_RELEASE "/home/woopinbells/.conan2/p/rediseddfda0b244cb/p")
set(redis-plus-plus_BUILD_MODULES_PATHS_RELEASE )


set(redis-plus-plus_INCLUDE_DIRS_RELEASE "${redis-plus-plus_PACKAGE_FOLDER_RELEASE}/include")
set(redis-plus-plus_RES_DIRS_RELEASE )
set(redis-plus-plus_DEFINITIONS_RELEASE )
set(redis-plus-plus_SHARED_LINK_FLAGS_RELEASE )
set(redis-plus-plus_EXE_LINK_FLAGS_RELEASE )
set(redis-plus-plus_OBJECTS_RELEASE )
set(redis-plus-plus_COMPILE_DEFINITIONS_RELEASE )
set(redis-plus-plus_COMPILE_OPTIONS_C_RELEASE )
set(redis-plus-plus_COMPILE_OPTIONS_CXX_RELEASE )
set(redis-plus-plus_LIB_DIRS_RELEASE "${redis-plus-plus_PACKAGE_FOLDER_RELEASE}/lib")
set(redis-plus-plus_BIN_DIRS_RELEASE )
set(redis-plus-plus_LIBRARY_TYPE_RELEASE STATIC)
set(redis-plus-plus_IS_HOST_WINDOWS_RELEASE 0)
set(redis-plus-plus_LIBS_RELEASE redis++)
set(redis-plus-plus_SYSTEM_LIBS_RELEASE pthread m)
set(redis-plus-plus_FRAMEWORK_DIRS_RELEASE )
set(redis-plus-plus_FRAMEWORKS_RELEASE )
set(redis-plus-plus_BUILD_DIRS_RELEASE )
set(redis-plus-plus_NO_SONAME_MODE_RELEASE FALSE)


# COMPOUND VARIABLES
set(redis-plus-plus_COMPILE_OPTIONS_RELEASE
    "$<$<COMPILE_LANGUAGE:CXX>:${redis-plus-plus_COMPILE_OPTIONS_CXX_RELEASE}>"
    "$<$<COMPILE_LANGUAGE:C>:${redis-plus-plus_COMPILE_OPTIONS_C_RELEASE}>")
set(redis-plus-plus_LINKER_FLAGS_RELEASE
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,SHARED_LIBRARY>:${redis-plus-plus_SHARED_LINK_FLAGS_RELEASE}>"
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,MODULE_LIBRARY>:${redis-plus-plus_SHARED_LINK_FLAGS_RELEASE}>"
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:${redis-plus-plus_EXE_LINK_FLAGS_RELEASE}>")


set(redis-plus-plus_COMPONENTS_RELEASE redis++::redis++_static)
########### COMPONENT redis++::redis++_static VARIABLES ############################################

set(redis-plus-plus_redis++_redis++_static_INCLUDE_DIRS_RELEASE "${redis-plus-plus_PACKAGE_FOLDER_RELEASE}/include")
set(redis-plus-plus_redis++_redis++_static_LIB_DIRS_RELEASE "${redis-plus-plus_PACKAGE_FOLDER_RELEASE}/lib")
set(redis-plus-plus_redis++_redis++_static_BIN_DIRS_RELEASE )
set(redis-plus-plus_redis++_redis++_static_LIBRARY_TYPE_RELEASE STATIC)
set(redis-plus-plus_redis++_redis++_static_IS_HOST_WINDOWS_RELEASE 0)
set(redis-plus-plus_redis++_redis++_static_RES_DIRS_RELEASE )
set(redis-plus-plus_redis++_redis++_static_DEFINITIONS_RELEASE )
set(redis-plus-plus_redis++_redis++_static_OBJECTS_RELEASE )
set(redis-plus-plus_redis++_redis++_static_COMPILE_DEFINITIONS_RELEASE )
set(redis-plus-plus_redis++_redis++_static_COMPILE_OPTIONS_C_RELEASE "")
set(redis-plus-plus_redis++_redis++_static_COMPILE_OPTIONS_CXX_RELEASE "")
set(redis-plus-plus_redis++_redis++_static_LIBS_RELEASE redis++)
set(redis-plus-plus_redis++_redis++_static_SYSTEM_LIBS_RELEASE pthread m)
set(redis-plus-plus_redis++_redis++_static_FRAMEWORK_DIRS_RELEASE )
set(redis-plus-plus_redis++_redis++_static_FRAMEWORKS_RELEASE )
set(redis-plus-plus_redis++_redis++_static_DEPENDENCIES_RELEASE hiredis::hiredis)
set(redis-plus-plus_redis++_redis++_static_SHARED_LINK_FLAGS_RELEASE )
set(redis-plus-plus_redis++_redis++_static_EXE_LINK_FLAGS_RELEASE )
set(redis-plus-plus_redis++_redis++_static_NO_SONAME_MODE_RELEASE FALSE)

# COMPOUND VARIABLES
set(redis-plus-plus_redis++_redis++_static_LINKER_FLAGS_RELEASE
        $<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,SHARED_LIBRARY>:${redis-plus-plus_redis++_redis++_static_SHARED_LINK_FLAGS_RELEASE}>
        $<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,MODULE_LIBRARY>:${redis-plus-plus_redis++_redis++_static_SHARED_LINK_FLAGS_RELEASE}>
        $<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:${redis-plus-plus_redis++_redis++_static_EXE_LINK_FLAGS_RELEASE}>
)
set(redis-plus-plus_redis++_redis++_static_COMPILE_OPTIONS_RELEASE
    "$<$<COMPILE_LANGUAGE:CXX>:${redis-plus-plus_redis++_redis++_static_COMPILE_OPTIONS_CXX_RELEASE}>"
    "$<$<COMPILE_LANGUAGE:C>:${redis-plus-plus_redis++_redis++_static_COMPILE_OPTIONS_C_RELEASE}>")