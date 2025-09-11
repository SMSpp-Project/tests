# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(MMCFBlock)
    # Solvers
    find_package(BundleSolver)
    find_package(MILPSolver)
endif ()

find_package(CPLEX)

# ----- Subdirectories ------------------------------------------------------ #
add_subdirectory(MMCFClass)
add_subdirectory(gen/Mnetgen)

# ----- Custom commands and targets ----------------------------------------- #

# Custom command to run gen/batch with the mnetgen exe if the .generated file does not exist
if (UNIX)
    add_custom_command(
            OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/data/Mnetgen/.generated
            COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/gen/genbatch ${CMAKE_CURRENT_BINARY_DIR}/gen/Mnetgen/mnetgen
            COMMAND echo > ${CMAKE_CURRENT_SOURCE_DIR}/data/Mnetgen/.generated
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/gen
            COMMENT "Running mnetgen"
            VERBATIM
    )
elseif (WIN32)
    add_custom_command(
            OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/data/Mnetgen/.generated
            # using Git for Windows that includes Git Bash
            COMMAND "C:/Program Files/Git/bin/bash.exe"
            "${CMAKE_CURRENT_SOURCE_DIR}/gen/genbatch" "$<TARGET_FILE:mnetgen>"
            COMMAND echo > ${CMAKE_CURRENT_SOURCE_DIR}/data/Mnetgen/.generated
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/gen
            COMMENT "Running mnetgen"
            VERBATIM
    )
endif ()

# Create custom targets to run the nc4 generation
add_custom_target(run_mnetgen
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/data/Mnetgen/.generated)

# Ensure that run_mnetgen is executed after the build
add_dependencies(run_mnetgen mnetgen)

# ----- Tests --------------------------------------------------------------- #
if (TARGET SMS++::MMCFBlock AND
    TARGET MMCFClass AND
    CPLEX_FOUND)

    add_executable(MMCF_test main.cpp)
    target_link_libraries(MMCF_test PRIVATE
                          SMS++::MMCFBlock
                          MMCFClass)

    if (TARGET SMS++::BundleSolver)
        target_link_libraries(MMCF_test PRIVATE SMS++::BundleSolver)
    endif ()

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(MMCF_test PRIVATE SMS++::MILPSolver)
    endif ()

    # Create a test fixture to generate files by running run_mnetgen.
    # This test will build and run the run_mnetgen target
    add_test(NAME run_mnetgen
             COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target run_mnetgen)
    # Mark run_mnetgen as a fixture that sets up "mnetgen_generated"
    set_tests_properties(run_mnetgen PROPERTIES FIXTURES_SETUP mnetgen_generated)

    # Add a test for each batch file
    file(GLOB BATCH_FILES "${CMAKE_CURRENT_SOURCE_DIR}/batch")
    foreach (BATCH_FILE ${BATCH_FILES})
        # Remove the source directory prefix to get the batch name
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" BATCH_NAME ${BATCH_FILE})
        if (UNIX)
            add_test(NAME MMCF_test/${BATCH_NAME}
                     COMMAND bash ${BATCH_FILE} ${CMAKE_CURRENT_BINARY_DIR}/MMCF_test
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        elseif (WIN32)
            add_test(NAME MMCF_test/${BATCH_NAME}
                     # using Git for Windows that includes Git Bash
                     COMMAND "C:/Program Files/Git/bin/bash.exe"
                             "${BATCH_FILE}" "$<TARGET_FILE:MMCF_test>"
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        endif ()
        # Make each batch test require the mnetgen_generated fixture
        set_tests_properties(MMCF_test/${BATCH_NAME}
                             PROPERTIES FIXTURES_REQUIRED mnetgen_generated)
    endforeach ()
endif ()

# --------------------------------------------------------------------------- #
