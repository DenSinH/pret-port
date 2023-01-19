# PREPROC
file(GLOB PREPROC_SOURCE_FILES
        preproc/*.cpp)
add_executable(PREPROC
        ${PREPROC_SOURCE_FILES})
set_target_properties(PREPROC PROPERTIES LINKER_LANGUAGE CXX)

# GBAGFX
find_package(PNG REQUIRED)

message(STATUS "Found PNG library at ${PNG_LIBRARY}")
file(GLOB GBAGFX_SOURCE_FILES
        gbagfx/*.c)
add_executable(GBAGFX
        ${GBAGFX_SOURCE_FILES})
target_include_directories(GBAGFX PRIVATE ${PNG_INCLUDE_DIR})
set_target_properties(GBAGFX PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(GBAGFX ${PNG_LIBRARY} ${ZLIB_LIBRARY})