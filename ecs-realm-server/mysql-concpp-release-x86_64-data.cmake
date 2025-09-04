########### AGGREGATED COMPONENTS AND DEPENDENCIES FOR THE MULTI CONFIG #####################
#############################################################################################

set(mysql-connector-cpp_COMPONENT_NAMES "")
if(DEFINED mysql-connector-cpp_FIND_DEPENDENCY_NAMES)
  list(APPEND mysql-connector-cpp_FIND_DEPENDENCY_NAMES protobuf OpenSSL ZLIB lz4 zstd)
  list(REMOVE_DUPLICATES mysql-connector-cpp_FIND_DEPENDENCY_NAMES)
else()
  set(mysql-connector-cpp_FIND_DEPENDENCY_NAMES protobuf OpenSSL ZLIB lz4 zstd)
endif()
set(protobuf_FIND_MODE "NO_MODULE")
set(OpenSSL_FIND_MODE "NO_MODULE")
set(ZLIB_FIND_MODE "NO_MODULE")
set(lz4_FIND_MODE "NO_MODULE")
set(zstd_FIND_MODE "NO_MODULE")

########### VARIABLES #######################################################################
#############################################################################################
set(mysql-connector-cpp_PACKAGE_FOLDER_RELEASE "/home/woopinbells/.conan2/p/mysql6688fc51cdb87/p")
set(mysql-connector-cpp_BUILD_MODULES_PATHS_RELEASE )


set(mysql-connector-cpp_INCLUDE_DIRS_RELEASE "${mysql-connector-cpp_PACKAGE_FOLDER_RELEASE}/include")
set(mysql-connector-cpp_RES_DIRS_RELEASE )
set(mysql-connector-cpp_DEFINITIONS_RELEASE "-DMYSQL_STATIC"
			"-DSTATIC_CONCPP")
set(mysql-connector-cpp_SHARED_LINK_FLAGS_RELEASE )
set(mysql-connector-cpp_EXE_LINK_FLAGS_RELEASE )
set(mysql-connector-cpp_OBJECTS_RELEASE )
set(mysql-connector-cpp_COMPILE_DEFINITIONS_RELEASE "MYSQL_STATIC"
			"STATIC_CONCPP")
set(mysql-connector-cpp_COMPILE_OPTIONS_C_RELEASE )
set(mysql-connector-cpp_COMPILE_OPTIONS_CXX_RELEASE )
set(mysql-connector-cpp_LIB_DIRS_RELEASE "${mysql-connector-cpp_PACKAGE_FOLDER_RELEASE}/lib")
set(mysql-connector-cpp_BIN_DIRS_RELEASE )
set(mysql-connector-cpp_LIBRARY_TYPE_RELEASE STATIC)
set(mysql-connector-cpp_IS_HOST_WINDOWS_RELEASE 0)
set(mysql-connector-cpp_LIBS_RELEASE mysqlcppconnx-static)
set(mysql-connector-cpp_SYSTEM_LIBS_RELEASE m pthread dl resolv)
set(mysql-connector-cpp_FRAMEWORK_DIRS_RELEASE )
set(mysql-connector-cpp_FRAMEWORKS_RELEASE )
set(mysql-connector-cpp_BUILD_DIRS_RELEASE )
set(mysql-connector-cpp_NO_SONAME_MODE_RELEASE FALSE)


# COMPOUND VARIABLES
set(mysql-connector-cpp_COMPILE_OPTIONS_RELEASE
    "$<$<COMPILE_LANGUAGE:CXX>:${mysql-connector-cpp_COMPILE_OPTIONS_CXX_RELEASE}>"
    "$<$<COMPILE_LANGUAGE:C>:${mysql-connector-cpp_COMPILE_OPTIONS_C_RELEASE}>")
set(mysql-connector-cpp_LINKER_FLAGS_RELEASE
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,SHARED_LIBRARY>:${mysql-connector-cpp_SHARED_LINK_FLAGS_RELEASE}>"
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,MODULE_LIBRARY>:${mysql-connector-cpp_SHARED_LINK_FLAGS_RELEASE}>"
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:${mysql-connector-cpp_EXE_LINK_FLAGS_RELEASE}>")


set(mysql-connector-cpp_COMPONENTS_RELEASE )