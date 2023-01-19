cmake_minimum_required(VERSION 3.21)
set(DECOMP pokeruby)
set(PROJECT_NAME ${DECOMP}-port)
project(${PROJECT_NAME} C CXX)
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

set(CMAKE_C_STANDARD 17)


set(DECOMP_DIR ${DECOMP})

# find source files
set(DECOMP_SOURCE_RELATIVE_DIR "${PROJECT_SOURCE_DIR}/decomp/${DECOMP_DIR}/src")
file(GLOB_RECURSE DECOMP_SOURCE_FILES
        RELATIVE ${DECOMP_SOURCE_RELATIVE_DIR}
        ${PROJECT_SOURCE_DIR}/decomp/${DECOMP_DIR}/src/*.c)

file(GLOB_RECURSE DECOMP_HEADER_FILES
        ${PROJECT_SOURCE_DIR}/decomp/${DECOMP_DIR}/include/*.h)
file(GLOB_RECURSE DECOMP_ASM_FILES
        ${PROJECT_SOURCE_DIR}/decomp/${DECOMP_DIR}/data/*.s)

# find tools
add_subdirectory(pret-tools)

# need 32 bits for GBA pointer casts to still work
# do this after including the subdirectory because we want to use 64 bits for the tools
set(CMAKE_C_FLAGS "-m32")
set(CMAKE_EXE_LINKER_FLAGS "-m32")

# create target
add_executable(${PROJECT_NAME}
        ${DECOMP_HEADER_FILES}
)

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
    # GCC or Clang
    add_definitions(-include ${PROJECT_SOURCE_DIR}/non-generic/${DECOMP}/prepend.h)
endif()

# process graphics files
# the conversions should all be specified in .mk files in the directories
file(GLOB MK_FILES ${PROJECT_SOURCE_DIR}/decomp/${DECOMP_DIR}/*.mk)
set(GFXFIXALL "${PROJECT_SOURCE_DIR}/tools/gfxfixall.py")
message(STATUS GBAGFX)
add_custom_target(
        RUN_GFXFIXALL
        COMMAND python ${GFXFIXALL} ${GBAGFX} ${MK_FILES}
        DEPENDS ${GFXFIXALL} ${MK_FILES} GBAGFX
        COMMENT "Finding and converting all graphics files"
        VERBATIM
)
add_dependencies(${PROJECT_NAME} RUN_GFXFIXALL)

# preprocess source files
get_target_property(compile_options ${PROJECT_NAME} COMPILE_OPTIONS)
get_target_property(include_directories ${PROJECT_NAME} INCLUDE_DIRECTORIES)

# Construct the compiler string for the include directories.
foreach(dir ${include_directories})
    string(APPEND PREPROC_INCLUDE_STRING "-I${dir} ")
endforeach()
separate_arguments(PREPROC_INCLUDE_STRING UNIX_COMMAND ${PREPROC_INCLUDE_STRING})

foreach(source_file ${DECOMP_SOURCE_FILES})
    set(output_file "${CMAKE_CURRENT_BINARY_DIR}/src-preproc/${source_file}.ii")
    set(temp_file "${CMAKE_CURRENT_BINARY_DIR}/src-preproc/${source_file}.c")
    message(STATUS "${source_file} -> ${output_file}")
    add_custom_command(
            OUTPUT "${output_file}"
            COMMAND "${CMAKE_C_COMPILER}" ${CMAKE_C_FLAGS} ${compile_options}
                    -o "${temp_file}"
                    -E "${DECOMP_SOURCE_RELATIVE_DIR}/${source_file}"
                    ${PREPROC_INCLUDE_STRING}
            COMMAND PREPROC "${temp_file}" "${PROJECT_SOURCE_DIR}/non-generic/${DECOMP}/charmap.txt" > "${output_file}"
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/decomp/${DECOMP_DIR}"
            DEPENDS PREPROC "${DECOMP_SOURCE_RELATIVE_DIR}/${source_file}"
            COMMENT "Preprocessing ${source_file} to ${output_file}"
            VERBATIM
    )

    target_sources(${PROJECT_NAME} PRIVATE "${output_file}")
    set_source_files_properties("${output_file}" PROPERTIES LANGUAGE C)
endforeach()


# todo: separate file
# pokeruby specific
add_compile_definitions(ENGLISH)