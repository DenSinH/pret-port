# we know that these tools work in pret, so we might as well just compile optimized
# versions, without warnings because they will likely be run often
add_compile_options(-O3 -DNDEBUG -w)

# PREPROC
file(GLOB PREPROC_SOURCE_FILES
        preproc/*.cpp)
add_executable(PREPROC
        ${PREPROC_SOURCE_FILES})
set_target_properties(PREPROC PROPERTIES LINKER_LANGUAGE CXX)

# GBAGFX
find_package(PNG REQUIRED)

file(GLOB GBAGFX_SOURCE_FILES
        gbagfx/*.c)
add_executable(GBAGFX
        ${GBAGFX_SOURCE_FILES})
target_include_directories(GBAGFX PRIVATE ${PNG_INCLUDE_DIR})
set_target_properties(GBAGFX PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(GBAGFX ${PNG_LIBRARY} ${ZLIB_LIBRARY})

# RSFONT
file(GLOB RSFONT_SOURCE_FILES
        rsfont/*.c)
add_executable(RSFONT
        ${RSFONT_SOURCE_FILES})
target_include_directories(RSFONT PRIVATE ${PNG_INCLUDE_DIR})
target_compile_options(RSFONT PRIVATE -Wno-deprecated-declarations)
set_target_properties(RSFONT PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(RSFONT ${PNG_LIBRARY} ${ZLIB_LIBRARY})

# JSONPROC
file(GLOB JSONPROC_SOURCE_FILES
        jsonproc/*.cpp)
add_executable(JSONPROC
        ${JSONPROC_SOURCE_FILES})
target_include_directories(JSONPROC PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/jsonproc)
target_compile_options(JSONPROC PRIVATE -Wno-deprecated-declarations)
set_target_properties(JSONPROC PROPERTIES LINKER_LANGUAGE CXX)

# MAPJSON
file(GLOB MAPJSON_SOURCE_FILES
        mapjson/*.cpp)
add_executable(MAPJSON
        ${MAPJSON_SOURCE_FILES})
set_target_properties(MAPJSON PROPERTIES LINKER_LANGUAGE CXX)

# AIF2PCM
file(GLOB AIF2PCM_SOURCE_FILES
        aif2pcm/*.c)
add_executable(AIF2PCM
        ${AIF2PCM_SOURCE_FILES})
set_target_properties(AIF2PCM PROPERTIES LINKER_LANGUAGE C)

# MID2AGB
file(GLOB MID2AGB_SOURCE_FILES
        mid2agb/*.cpp)
add_executable(MID2AGB
        ${MID2AGB_SOURCE_FILES})
set_target_properties(MID2AGB PROPERTIES LINKER_LANGUAGE CXX)