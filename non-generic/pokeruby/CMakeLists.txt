add_library(${DECOMP} STATIC
        symbols.c)

target_include_directories(${DECOMP} PUBLIC ${PROJECT_SOURCE_DIR}/generic)
target_include_directories(${DECOMP} PUBLIC ${PROJECT_SOURCE_DIR}/decomp/${DECOMP}/include)
target_include_directories(${DECOMP} PUBLIC ${PROJECT_SOURCE_DIR}/decomp/${DECOMP}/src)