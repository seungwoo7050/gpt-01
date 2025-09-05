# Load the debug and release variables
file(GLOB DATA_FILES "${CMAKE_CURRENT_LIST_DIR}/redis++-*-data.cmake")

foreach(f ${DATA_FILES})
    include(${f})
endforeach()

# Create the targets for all the components
foreach(_COMPONENT ${redis-plus-plus_COMPONENT_NAMES} )
    if(NOT TARGET ${_COMPONENT})
        add_library(${_COMPONENT} INTERFACE IMPORTED)
        message(${redis++_MESSAGE_MODE} "Conan: Component target declared '${_COMPONENT}'")
    endif()
endforeach()

if(NOT TARGET redis++::redis++_static)
    add_library(redis++::redis++_static INTERFACE IMPORTED)
    message(${redis++_MESSAGE_MODE} "Conan: Target declared 'redis++::redis++_static'")
endif()
# Load the debug and release library finders
file(GLOB CONFIG_FILES "${CMAKE_CURRENT_LIST_DIR}/redis++-Target-*.cmake")

foreach(f ${CONFIG_FILES})
    include(${f})
endforeach()