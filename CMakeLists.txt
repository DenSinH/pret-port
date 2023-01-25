cmake_minimum_required(VERSION 3.21)
set(DECOMP pokeruby)
set(PROJECT_NAME ${DECOMP}-port)
project(${PROJECT_NAME} C CXX ASM)
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

set(CMAKE_C_STANDARD 17)


set(DECOMP_DIR ${DECOMP})

# find source files
set(DECOMP_SOURCE_RELATIVE_DIR "${PROJECT_SOURCE_DIR}/decomp/${DECOMP_DIR}/src")
file(GLOB_RECURSE DECOMP_SOURCE_FILES
        RELATIVE ${DECOMP_SOURCE_RELATIVE_DIR}
        ${DECOMP_SOURCE_RELATIVE_DIR}/*.c)

file(GLOB_RECURSE DECOMP_HEADER_FILES
        ${PROJECT_SOURCE_DIR}/decomp/${DECOMP_DIR}/include/*.h
        ${PROJECT_SOURCE_DIR}/decomp/${DECOMP_DIR}/src/*.h)

set(DECOMP_ASM_DATA_RELATIVE_DIR "${PROJECT_SOURCE_DIR}/decomp/${DECOMP_DIR}/data")
file(GLOB_RECURSE DECOMP_ASM_DATA_FILES
        RELATIVE ${DECOMP_ASM_DATA_RELATIVE_DIR}
        ${DECOMP_ASM_DATA_RELATIVE_DIR}/*.s)

set(DECOMP_ASM_SOUND_RELATIVE_DIR "${PROJECT_SOURCE_DIR}/decomp/${DECOMP_DIR}/sound")
file(GLOB_RECURSE DECOMP_ASM_SOUND_FILES
        RELATIVE ${DECOMP_ASM_SOUND_RELATIVE_DIR}
        ${DECOMP_ASM_SOUND_RELATIVE_DIR}/*.s)

file(GLOB_RECURSE DECOMP_ASM_INC_FILES "${PROJECT_SOURCE_DIR}/decomp/${DECOMP_DIR}/*.inc")

# find tools
add_subdirectory(pret-tools)

# todo: this is only for clang
add_link_options("LINKER:/SAFESEH:NO")

# create target
add_executable(${PROJECT_NAME}
        ${DECOMP_HEADER_FILES})

# libgba target for missing stdlib includes
file(GLOB_RECURSE LIBGBA_SOURCES "${PROJECT_SOURCE_DIR}/generic/*.c")
add_library(LIBGBA STATIC
        ${LIBGBA_SOURCES})

target_include_directories(LIBGBA PRIVATE
        "${PROJECT_SOURCE_DIR}/decomp/${DECOMP}/include"
        "${PROJECT_SOURCE_DIR}/generic"
        "${PROJECT_SOURCE_DIR}/generic/include")

# add decomp library for decomp-specific missing symbols
add_subdirectory(non-generic/${DECOMP})

# need 32 bits for GBA pointer casts to still work
# do this after including the subdirectory because we want to use 64 bits for the tools
#set(CMAKE_C_FLAGS "-m32")
set_target_properties(${PROJECT_NAME} PROPERTIES
        COMPILE_OPTIONS "-m32"
        LINK_FLAGS "-m32")
set_target_properties(${DECOMP} PROPERTIES
        COMPILE_OPTIONS "-m32"
        LINK_FLAGS "-m32")

# allow nonmatching functions (less asm)
target_compile_definitions(${PROJECT_NAME} PUBLIC
        NONMATCHING)

# todo: separate file
# pokeruby specific
target_compile_definitions(${PROJECT_NAME} PUBLIC
        ENGLISH
        DEBUG=0
        RUBY
        MODERN=1)


# compile options for target
target_compile_options(${PROJECT_NAME} PRIVATE
        -Wno-macro-redefined)
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE C)

target_include_directories(${PROJECT_NAME} PUBLIC
        ${PROJECT_SOURCE_DIR}/decomp/${DECOMP_DIR}/include)

# add additional compile definitions
if(MSVC)
    # add_definitions(/FI"foo.h")
    message(ERROR "Test this for MSVC")
else()
    if (EXISTS "${PROJECT_SOURCE_DIR}/non-generic/${DECOMP}/prepend.h")
        set(PREPROCESSING_PREPEND_STRING "-include \"${PROJECT_SOURCE_DIR}/generic/prepend.h\" -include \"${PROJECT_SOURCE_DIR}/non-generic/${DECOMP}/prepend.h\"")
    else()
        set(PREPROCESSING_PREPEND_STRING "-include \"${PROJECT_SOURCE_DIR}/generic/prepend.h\"")
    endif()
endif()
# process graphics files
# the conversions should all be specified in .mk files in the directories

# find all incbins present in code
set(GFXFINDALL "${PROJECT_SOURCE_DIR}/tools/gfxfindall.py")
list(TRANSFORM DECOMP_SOURCE_FILES
     PREPEND "${DECOMP_SOURCE_RELATIVE_DIR}/"
     OUTPUT_VARIABLE ABSOLUTE_DECOMP_SOURCE_FILES)
list(TRANSFORM DECOMP_ASM_DATA_FILES
     PREPEND "${DECOMP_ASM_DATA_RELATIVE_DIR}/"
     OUTPUT_VARIABLE ABSOLUTE_DECOMP_ASM_DATA_FILES)

set(SOURCE_LIST_FILE "${CMAKE_CURRENT_BINARY_DIR}/temp/source_list.txt")
string (REPLACE ";" "\n" ABSOLUTE_DECOMP_SOURCE_FILES_LIST "${ABSOLUTE_DECOMP_SOURCE_FILES}")
string (REPLACE ";" "\n" ABSOLUTE_DECOMP_ASM_DATA_FILES_LIST "${ABSOLUTE_DECOMP_ASM_DATA_FILES}")
string (REPLACE ";" "\n" DECOMP_ASM_INC_FILES_LIST "${DECOMP_ASM_INC_FILES}")
file(WRITE "${SOURCE_LIST_FILE}.tmp" "${ABSOLUTE_DECOMP_SOURCE_FILES_LIST}\n${ABSOLUTE_DECOMP_ASM_DATA_FILES_LIST}\n${DECOMP_ASM_INC_FILES_LIST}")
file(COPY_FILE "${SOURCE_LIST_FILE}.tmp" "${SOURCE_LIST_FILE}" ONLY_IF_DIFFERENT)

set(HEADER_LIST_FILE "${CMAKE_CURRENT_BINARY_DIR}/temp/header_list.txt")
string (REPLACE ";" "\n" DECOMP_HEADER_FILES_LIST "${DECOMP_HEADER_FILES}")
file(WRITE "${HEADER_LIST_FILE}.tmp" ${DECOMP_HEADER_FILES_LIST})
file(COPY_FILE "${HEADER_LIST_FILE}.tmp" "${HEADER_LIST_FILE}" ONLY_IF_DIFFERENT)

set(INCBIN_LIST_FILE "${CMAKE_CURRENT_BINARY_DIR}/temp/incbin_list.txt")

add_custom_command(
        OUTPUT "${INCBIN_LIST_FILE}"
        COMMAND python ${GFXFINDALL} "${SOURCE_LIST_FILE}" "${HEADER_LIST_FILE}" > "${INCBIN_LIST_FILE}"
        DEPENDS ${GFXFINDALL} ${ABSOLUTE_DECOMP_SOURCE_FILES} ${DECOMP_HEADER_FILES}
        COMMENT "Finding all graphics files"
        VERBATIM
)

add_custom_target(RUN_GFXFINDALL
        DEPENDS "${INCBIN_LIST_FILE}")

set(MK_FILE "${PROJECT_SOURCE_DIR}/decomp/${DECOMP_DIR}/Makefile")
#file(GLOB MK_FILES
#        ${PROJECT_SOURCE_DIR}/decomp/${DECOMP_DIR}/Makefile)
##        ${PROJECT_SOURCE_DIR}/decomp/${DECOMP_DIR}/*.mk)
#set(MK_FILE_LIST_FILE "${CMAKE_CURRENT_BINARY_DIR}/temp/mkfile_list.txt")

# write to file so the argument string will not be too long
#string (REPLACE ";" "\n" MK_FILES_LIST "${MK_FILES}")
#file(WRITE "${MK_FILE_LIST_FILE}.tmp" ${MK_FILES_LIST})
#file(COPY_FILE "${MK_FILE_LIST_FILE}.tmp" "${MK_FILE_LIST_FILE}" ONLY_IF_DIFFERENT)

set(INCFIXALL "${PROJECT_SOURCE_DIR}/tools/incfixall.py")
set(INCFIXALL_OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/incfixall.out")
#message(STATUS "INCFIXALL command \
#    python ${INCFIXALL} $<TARGET_FILE:GBAGFX> $<TARGET_FILE:RSFONT> $<TARGET_FILE:JSONPROC> $<TARGET_FILE:MAPJSON> \
#    \"${PROJECT_SOURCE_DIR}/decomp/${DECOMP_DIR}/\" \"${CMAKE_CURRENT_BINARY_DIR}\" \"${MK_FILE_LIST_FILE}\" \"${INCBIN_LIST_FILE}\"")

add_custom_command(
        OUTPUT "${INCFIXALL_OUTPUT_FILE}"
        COMMAND python ${INCFIXALL} $<TARGET_FILE:GBAGFX> $<TARGET_FILE:RSFONT> $<TARGET_FILE:JSONPROC> $<TARGET_FILE:MAPJSON>
                $<TARGET_FILE:AIF2PCM> $<TARGET_FILE:MID2AGB>
                "${PROJECT_SOURCE_DIR}/decomp/${DECOMP_DIR}/" "${CMAKE_CURRENT_BINARY_DIR}" "${MK_FILE}" "${INCBIN_LIST_FILE}"
                > "${INCFIXALL_OUTPUT_FILE}"
        DEPENDS ${INCFIXALL} GBAGFX RSFONT JSONPROC MAPJSON AIF2PCM MID2AGB
                RUN_GFXFINDALL "${MK_FILE}" ${INCBIN_LIST_FILE}
        COMMENT "Processing all include files"
        VERBATIM
)

add_custom_target(RUN_INCFIXALL
        DEPENDS "${INCFIXALL_OUTPUT_FILE}")

# preprocess source files
get_target_property(DECOMP_COMPILE_OPTIONS ${PROJECT_NAME} COMPILE_OPTIONS)
get_target_property(DECOMP_COMPILE_DEFINITIONS ${PROJECT_NAME} COMPILE_DEFINITIONS)
get_target_property(DECOMP_INCLUDE_DIRECTORIES ${PROJECT_NAME} INCLUDE_DIRECTORIES)

# Construct the compiler string for the include directories.
foreach(dir ${DECOMP_INCLUDE_DIRECTORIES})
    string(APPEND PREPROC_INCLUDE_STRING "-I${dir} ")
endforeach()
separate_arguments(PREPROC_INCLUDE_STRING UNIX_COMMAND ${PREPROC_INCLUDE_STRING})

# Construct the compiler string for the compile definitions
foreach(def ${DECOMP_COMPILE_DEFINITIONS})
    string(APPEND PREPROC_COMPILE_DEFINITION_STRING "-D${def} ")
endforeach()
separate_arguments(PREPROC_COMPILE_DEFINITION_STRING_SEPARATED UNIX_COMMAND ${PREPROC_COMPILE_DEFINITION_STRING})

foreach(source_file ${DECOMP_SOURCE_FILES})
    set(output_file "${CMAKE_CURRENT_BINARY_DIR}/src-preproc/${source_file}.ii")
    set(temp_file "${CMAKE_CURRENT_BINARY_DIR}/src-preproc/${source_file}.c")
    add_custom_command(
            OUTPUT "${output_file}"
            COMMAND "${CMAKE_C_COMPILER}" ${CMAKE_C_FLAGS} ${DECOMP_COMPILE_OPTIONS}
                    -o "${temp_file}"
                    -E "${DECOMP_SOURCE_RELATIVE_DIR}/${source_file}"
                    ${PREPROC_INCLUDE_STRING} ${PREPROC_COMPILE_DEFINITION_STRING_SEPARATED}
                    ${PREPROCESSING_PREPEND_STRING}
            COMMAND PREPROC "${temp_file}" "${PROJECT_SOURCE_DIR}/non-generic/${DECOMP}/charmap.txt" > "${output_file}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
            DEPENDS PREPROC "${DECOMP_SOURCE_RELATIVE_DIR}/${source_file}" RUN_INCFIXALL
                    ${PROJECT_SOURCE_DIR}/generic/prepend.h
            # todo: DEPENDS on inc files
            COMMENT "Preprocessing ${source_file} to ${output_file}"
    )

    target_sources(${PROJECT_NAME} PRIVATE "${output_file}")
    set_source_files_properties("${output_file}" PROPERTIES LANGUAGE C)
endforeach()

set(ASM_NONGENERIC_PREPEND_FILE "${PROJECT_SOURCE_DIR}/non-generic/${DECOMP}/prepend_asm.h")
if(EXISTS ${ASM_NONGENERIC_PREPEND_FILE})
    message(STATUS "Nongeneric assembly prepend file found: ${ASM_NONGENERIC_PREPEND_FILE}")
    set(ASM_PREPROCESSING_PREPEND_STRING "-include \"${ASM_NONGENERIC_PREPEND_FILE}\"")
else()
    set(ASM_PREPROCESSING_PREPEND_STRING "")
    set(ASM_NONGENERIC_PREPEND_FILE "")
endif()

foreach(source_file ${DECOMP_ASM_DATA_FILES})
    set(output_file "${CMAKE_CURRENT_BINARY_DIR}/asm-data-preproc/${source_file}.tmp.s")
    set(temp_file "${CMAKE_CURRENT_BINARY_DIR}/asm-data-preproc/${source_file}.c")

    add_custom_command(
            OUTPUT "${output_file}"
            COMMAND PREPROC "${DECOMP_ASM_DATA_RELATIVE_DIR}/${source_file}" "${PROJECT_SOURCE_DIR}/non-generic/${DECOMP}/charmap.txt" > "${temp_file}"
            COMMAND "${CMAKE_C_COMPILER}"
                    -o "${output_file}"
                    -E "${temp_file}"
                    ${PREPROC_INCLUDE_STRING}
                    ${ASM_PREPROCESSING_PREPEND_STRING}
                    # todo: decomp-specific
                    -DDEBUG=0
                    -DREVISION=1
                    -DNO_MUSIC=0
                    -Wno-unicode
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/decomp/${DECOMP}"
            DEPENDS PREPROC "${DECOMP_ASM_DATA_RELATIVE_DIR}/${source_file}"
                    ${ASM_NONGENERIC_PREPEND_FILE}
                    ${DECOMP_ASM_INC_FILES}
                    RUN_INCFIXALL
            COMMENT "Preprocessing ${source_file} to ${output_file}"
    )

    target_sources(${PROJECT_NAME} PRIVATE "${output_file}")
    set_source_files_properties("${output_file}" PROPERTIES
            LANGUAGE ASM
            INCLUDE_DIRECTORIES "${PROJECT_SOURCE_DIR}/decomp/${DECOMP}")
endforeach()

foreach(source_file ${DECOMP_ASM_SOUND_FILES})
    set(output_file "${CMAKE_CURRENT_BINARY_DIR}/asm-sound-preproc/${source_file}.tmp.s")
    set(temp_file "${CMAKE_CURRENT_BINARY_DIR}/asm-sound-preproc/${source_file}.c")

    add_custom_command(
            OUTPUT "${output_file}"
            COMMAND PREPROC "${DECOMP_ASM_SOUND_RELATIVE_DIR}/${source_file}" "${PROJECT_SOURCE_DIR}/non-generic/${DECOMP}/charmap.txt" > "${temp_file}"
            COMMAND "${CMAKE_C_COMPILER}"
                    -o "${output_file}"
                    -E "${temp_file}"
                    ${PREPROC_INCLUDE_STRING}
                    -I"${PROJECT_SOURCE_DIR}/decomp/${DECOMP}/sound"
                    ${ASM_PREPROCESSING_PREPEND_STRING}
                    # todo: decomp-specific
                    -DDEBUG=0
                    -DREVISION=1
                    -DNO_MUSIC=0
                    -Wno-unicode
            # this is a hack to get .include "MPlayDef.s" to work in PREPROC
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/decomp/${DECOMP}/include"
            DEPENDS PREPROC "${DECOMP_ASM_SOUND_RELATIVE_DIR}/${source_file}"
                    ${ASM_NONGENERIC_PREPEND_FILE}
                    ${DECOMP_ASM_INC_FILES}
                    RUN_INCFIXALL
            COMMENT "Preprocessing ${source_file} to ${output_file}"
    )

    target_sources(${PROJECT_NAME} PRIVATE "${output_file}")
    set_source_files_properties("${output_file}" PROPERTIES
            LANGUAGE ASM
            INCLUDE_DIRECTORIES "${PROJECT_SOURCE_DIR}/decomp/${DECOMP}")
endforeach()

#target_compile_options(${PROJECT_NAME} PRIVATE "-L")

target_link_libraries(${PROJECT_NAME} ${DECOMP})
target_link_libraries(${PROJECT_NAME} LIBGBA)