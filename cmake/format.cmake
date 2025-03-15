# Function to find all source files in the project
function(get_all_source_files _source_files)
    file(GLOB_RECURSE _tmp_sources
        ${PROJECT_SOURCE_DIR}/src/*.cpp
        ${PROJECT_SOURCE_DIR}/src/*.hpp
        ${PROJECT_SOURCE_DIR}/src/*.h
        ${PROJECT_SOURCE_DIR}/include/*.hpp
        ${PROJECT_SOURCE_DIR}/include/*.h
    )
    set(${_source_files} ${_tmp_sources} PARENT_SCOPE)
endfunction()

# Find clang-format executable
find_program(CLANG_FORMAT_EXE NAMES clang-format)

# Add format target
if(CLANG_FORMAT_EXE)
    # Get all source files
    get_all_source_files(ALL_SOURCE_FILES)

    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXE} -i ${ALL_SOURCE_FILES}
        COMMENT "Formatting source code..."
        VERBATIM
    )

    add_custom_target(check-format
        COMMAND ${CLANG_FORMAT_EXE} --dry-run -Werror ${ALL_SOURCE_FILES}
        COMMENT "Checking format compliance..."
        VERBATIM
    )
else()
    message(WARNING "clang-format not found, format target will not be available")
endif()

# Function to enable auto-formatting for a target
function(enable_auto_format target)
    if(CLANG_FORMAT_EXE)
        get_target_property(TARGET_SOURCES ${target} SOURCES)
        add_custom_command(
            TARGET ${target}
            PRE_BUILD
            COMMAND ${CLANG_FORMAT_EXE} -i ${TARGET_SOURCES}
            COMMENT "Auto-formatting ${target} sources..."
            VERBATIM
        )
    endif()
endfunction()
