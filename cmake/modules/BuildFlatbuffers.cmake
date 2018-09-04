set(FLATBUFFERS_INSTALL_DIR ${CMAKE_THIRD_PARTY_DIR})

if(WIN32)
    set(_byproducts
        ${FLATBUFFERS_INSTALL_DIR}/bin/flatc.exe
    )

  ExternalProject_Add(
    flatbuffers
    GIT_REPOSITORY https://github.com/google/flatbuffers.git
    GIT_TAG 3c54fd964b6beae9a92955415568a001c9cea23d
    CMAKE_ARGS -G ${CMAKE_GENERATOR} -DCMAKE_BUILD_TYPE=Release -DFLATBUFFERS_BUILD_TESTS=OFF -DFLATBUFFERS_BUILD_FLATHASH=OFF -DFLATBUFFERS_BUILD_FLATLIB=OFF -DFLATBUFFERS_BUILD_FLATHASH=OFF -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    BUILD_BYPRODUCTS ${_byproducts}
    INSTALL_DIR ${FLATBUFFERS_INSTALL_DIR}
  )
  
  ExternalProject_Add_Step(
    flatbuffers
    copy-flatbuffers
    DEPENDEES download
    COMMAND ${CMAKE_SCRIPT_PATH}/robocopy.bat "<SOURCE_DIR>/src" "<INSTALL_DIR>/include/" "*.h"
    COMMAND ${CMAKE_SCRIPT_PATH}/robocopy.bat "<SOURCE_DIR>/src" "<INSTALL_DIR>/include/" "*.hpp"
  )
else()
    set(_byproducts
        ${FLATBUFFERS_INSTALL_DIR}/bin/flatc
    )
  ExternalProject_Add(
    flatbuffers
    GIT_REPOSITORY https://github.com/google/flatbuffers.git
    GIT_TAG 3c54fd964b6beae9a92955415568a001c9cea23d
    CMAKE_ARGS -G ${CMAKE_GENERATOR} -DCMAKE_BUILD_TYPE=Release -DFLATBUFFERS_BUILD_TESTS=OFF -DFLATBUFFERS_BUILD_FLATHASH=OFF -DFLATBUFFERS_BUILD_FLATLIB=OFF -DFLATBUFFERS_BUILD_FLATHASH=OFF -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    BUILD_BYPRODUCTS ${_byproducts}
    INSTALL_DIR ${FLATBUFFERS_INSTALL_DIR}
  )
endif()

ExternalProject_Get_Property(
  flatbuffers
  install_dir
)

set(FLATBUFFERS_INCLUDE_DIR "${install_dir}/include")
set(FLATBUFFERS_EXEC "${install_dir}/bin/flatc")

if(NOT TARGET FLATBUFFERS::FLATBUFFERS)
    add_library(FLATBUFFERS::FLATBUFFERS INTERFACE IMPORTED)
    add_dependencies(FLATBUFFERS::FLATBUFFERS flatbuffers)
    set_target_properties(FLATBUFFERS::FLATBUFFERS PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${FLATBUFFERSL_INCLUDE_DIRS}")
    set_target_properties(FLATBUFFERS::FLATBUFFERS PROPERTIES INTERFACE_LINK_LIBRARIES "${FLATBUFFERS_EXEC}")
endif()