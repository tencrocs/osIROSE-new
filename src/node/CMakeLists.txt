
link_directories(${EXTERNAL_BINARY_DIR})

set(PROJECT NodeServer)

project(${PROJECT})

if (${CMAKE_CXX_COMPILER_ID} STREQUAL GNU)
  add_compiler_flags(-Wl,-Map=${PROJECT}.map)
endif()

set( SOURCES_MAIN
  src/main )

set( SOURCES_HEADER
  include/nodeserver.h
  include/nodeclient.h
  include/nodeisc.h )

set( SOURCES_BASE
  src/nodeserver.cpp
  src/nodeclient.cpp
  src/nodeisc.cpp )

add_executable(${PROJECT} ${SOURCES_MAIN} ${SOURCES_BASE} ${SOURCES_HEADER})

target_include_directories(${PROJECT} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
set_target_properties(${PROJECT} PROPERTIES LINK_FLAGS "${${CMAKE_CXX_COMPILER_ID}_LINK_FLAGS}")

target_link_libraries(${PROJECT} ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(${PROJECT} CURL::CURL)

if (MSVC)
  set_target_properties(${PROJECT} PROPERTIES COMPILE_FLAGS "/wd4503")
  target_link_libraries(${PROJECT} wsock32.lib)
endif()

add_dependencies(${PROJECT} RoseCommon)
target_link_libraries(${PROJECT} RoseCommon)

generate_symbol_data(${PROJECT})

if (COMMAND cotire)
  cotire(${PROJECT})
endif()

SET_PROPERTY(TARGET ${PROJECT}                PROPERTY FOLDER "Servers")
