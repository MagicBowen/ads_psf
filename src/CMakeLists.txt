# ---- Src dependencies ----
set(CMAKE_CXX_STANDARD 14)

# CPMAddPackage(
#     NAME cub
#     GITHUB_REPOSITORY ccup/cub
#     GIT_TAG master
# )

# ---- Collect all src files except main.c ----

file(GLOB_RECURSE MAIN_FILE 
    "main.cc" "main.cpp"
)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "*.c" "*.C" "*.cc" "*.CC" "*.cpp" "*.CPP" "*.c++"
)

list(REMOVE_ITEM SOURCES ${MAIN_FILE})

# ---- Define library target ----

if (SHARED)
    set(LIB_TYPE SHARED)
    set(CMAKE_C_VISIBILITY_PRESET hidden)
else()
    set(LIB_TYPE STATIC)
endif()

add_library(${TARGET_LIB} ${LIB_TYPE} ${SOURCES})

# target_link_libraries(${TARGET_LIB} PUBLIC cub)

target_include_directories(${TARGET_LIB}
    PUBLIC ${PROJECT_SOURCE_DIR}/include
    PRIVATE ${PROJECT_SOURCE_DIR}/deps
)

set_target_properties(${TARGET_LIB} PROPERTIES CXX_STANDARD 14)

# ---- Add executable target ----

if (EXECUTABLE)
    set(EXEC_TARGET ${TARGET_LIB}_service)
    add_executable(${EXEC_TARGET} ${MAIN_FILE})
    target_link_libraries(${EXEC_TARGET} PRIVATE ${TARGET_LIB})
endif()
