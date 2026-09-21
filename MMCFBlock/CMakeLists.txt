# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(MMCFBlock)
    # Solvers
    find_package(BundleSolver)
    find_package(MILPSolver)
    find_package(LagrangianDualSolver)
    find_package(MCFLemonSolver)
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

# Fetch the Canad instances: download Canad.tgz from the GitLab Package
# Registry and extract it under data/; the fetch_canad_data target is the
# test fixture's only dependency.
# Download Canad.tgz from GitLab Package Registry if it doesn't exist
set(CANAD_URL "https://gitlab.com/api/v4/projects/15602443/packages/generic/Canad/latest/Canad.tgz")
set(CANAD_ARCHIVE ${CMAKE_CURRENT_SOURCE_DIR}/data/Canad.tgz)
set(CANAD_SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/download_canad.cmake)

file(WRITE ${CANAD_SCRIPT} "
file(DOWNLOAD \"${CANAD_URL}\" \"${CANAD_ARCHIVE}\" SHOW_PROGRESS)
")

add_custom_command(
        OUTPUT ${CANAD_ARCHIVE}
        COMMAND ${CMAKE_COMMAND} -E echo "Downloading Canad.tgz from GitLab Package Registry..."
        COMMAND ${CMAKE_COMMAND} -P ${CANAD_SCRIPT}
        COMMENT "Downloading Canad.tgz"
        VERBATIM)

add_custom_target(download_canad
        DEPENDS ${CANAD_ARCHIVE})

# Custom command to extract the Canad.tgz archive if the .extracted file does not exist
if (UNIX)
    set(TAR_FLAGS --warning=no-unknown-keyword -xzf)
elseif (WIN32)
    set(TAR_FLAGS xzf)
endif ()

add_custom_command(
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/data/Canad/.extracted
        COMMAND tar ${TAR_FLAGS} ${CMAKE_CURRENT_SOURCE_DIR}/data/Canad.tgz
        COMMAND echo > ${CMAKE_CURRENT_SOURCE_DIR}/data/Canad/.extracted
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data
        COMMENT "Extracting Canad.tgz"
        VERBATIM
        DEPENDS download_canad)

# The .dat instances are ready as soon as the archive is extracted: this is
# the terminal target the test fixtures depend on (see also
# tests/MMCFBlock).
add_custom_target(fetch_canad_data
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/data/Canad/.extracted)

# Ensure that the extraction is executed after the download
add_dependencies(fetch_canad_data download_canad)

# Skip the suite if some module its labels declare is not in the build
smspp_require_labelled_modules()

# ----- Tests --------------------------------------------------------------- #
# Create a test fixture to generate files by running run_mnetgen.
# This test will build and run the run_mnetgen target
add_test(NAME run_mnetgen
         COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target run_mnetgen)
# Mark run_mnetgen as a fixture that sets up "mnetgen_generated"
set_tests_properties(run_mnetgen
                     PROPERTIES FIXTURES_SETUP mnetgen_generated)

# Create a test fixture to download the Canad instances by running
# fetch_canad_data. This test will build and run the fetch_canad_data target
add_test(NAME fetch_canad_data
         COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target fetch_canad_data)
# Mark fetch_canad_data as a fixture that sets up "canad_fetched"
set_tests_properties(fetch_canad_data
                     PROPERTIES FIXTURES_SETUP canad_fetched)

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

    # the ML batch asks for BundleSolverML by name, and the class is in the
    # factory only if its library is linked [see BundleSolver/CMakeLists.txt]
    if (TARGET SMS++::BundleSolverML)
        target_link_libraries(MMCF_test PRIVATE SMS++::BundleSolverML)
    endif ()

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(MMCF_test PRIVATE SMS++::MILPSolver)
    endif ()

    smspp_install(MMCF_test)

    # the reference tester's batch (block-vs-MMCFCplex), which lives apart
    # from those of the cross-check tester below because it is run with this
    # tester and not with that one
    file(GLOB BATCH_FILES "${CMAKE_CURRENT_SOURCE_DIR}/batches-ref/*")
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
        # Make each batch test require the data fixtures it reads
        set_tests_properties(MMCF_test/${BATCH_NAME}
                             PROPERTIES FIXTURES_REQUIRED
                             "canad_fetched;mnetgen_generated")
    endforeach ()
endif ()

# ----- Cross-check tester -------------------------------------------------- #
# The second executable solves the same MMCFBlock instances with every Solver
# of its BlockSolverConfig (a :MILPSolver and a LagrangianDualSolver) and
# cross-checks them, reusing the data fixtures set up above.
if (TARGET SMS++::MMCFBlock)

    add_executable(MMCFBlock_test test.cpp ../common_utils.cpp)
    target_include_directories(MMCFBlock_test PRIVATE ..)
    target_link_libraries(MMCFBlock_test PRIVATE SMS++::MMCFBlock)

    if (TARGET SMS++::LagrangianDualSolver)
        target_link_libraries(MMCFBlock_test PRIVATE SMS++::LagrangianDualSolver)
    endif ()

    if (TARGET SMS++::BundleSolver)
        target_link_libraries(MMCFBlock_test PRIVATE SMS++::BundleSolver)
    endif ()

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(MMCFBlock_test PRIVATE SMS++::MILPSolver)
    endif ()

    if (TARGET SMS++::MCFLemonSolver)
        target_link_libraries(MMCFBlock_test PRIVATE SMS++::MCFLemonSolver)
    endif ()

    # batchML asks for BundleSolverML by name, and the class is in the
    # factory only if its library is linked [see BundleSolver/CMakeLists.txt]
    if (TARGET SMS++::BundleSolverML)
        target_link_libraries(MMCFBlock_test PRIVATE SMS++::BundleSolverML)
    endif ()

    smspp_install(MMCFBlock_test)

    # The ML batch (batchML) drives the inner BundleSolverML, which is only
    # registered in the Solver factory when BundleSolver is built with Torch;
    # register it as a test only in that case.
    find_package(Torch QUIET HINTS ${Torch_ROOT})
    set(ML_BATCH "")
    if (Torch_FOUND)
        set(ML_BATCH "${CMAKE_CURRENT_SOURCE_DIR}/batches/batchML")
    endif ()

    # Add a test for each batch file
    file(GLOB BATCH_FILES "${CMAKE_CURRENT_SOURCE_DIR}/batches/batch"
                          "${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-c"
                          "${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-k"
                          "${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-m"
                          ${ML_BATCH})
    foreach (BATCH_FILE ${BATCH_FILES})
        # Remove the source directory prefix to get the batch name
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" BATCH_NAME ${BATCH_FILE})
        if (UNIX)
            add_test(NAME MMCFBlock_test/${BATCH_NAME}
                     COMMAND bash ${BATCH_FILE} ${CMAKE_CURRENT_BINARY_DIR}/MMCFBlock_test
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        elseif (WIN32)
            add_test(NAME MMCFBlock_test/${BATCH_NAME}
                     # using Git for Windows that includes Git Bash
                     COMMAND "C:/Program Files/Git/bin/bash.exe"
                             "${BATCH_FILE}" "$<TARGET_FILE:MMCFBlock_test>"
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        endif ()
        # Make each batch test require the data fixtures it reads
        if (BATCH_NAME STREQUAL "batches/batch")
            set(BATCH_FIXTURES "canad_fetched;mnetgen_generated")
        elseif (BATCH_NAME STREQUAL "batches/batch-m")
            set(BATCH_FIXTURES "mnetgen_generated")
        else ()
            set(BATCH_FIXTURES "canad_fetched")
        endif ()
        set_tests_properties(MMCFBlock_test/${BATCH_NAME}
                             PROPERTIES FIXTURES_REQUIRED "${BATCH_FIXTURES}")
    endforeach ()
endif ()

    # The benchmark of the machine-learning driven bundle on the instances of
    # this Block: the harness is the generic one (../ml_bench.cpp), which
    # reads whichever instance it is given, and this suite compiles into it
    # the two text formats of the module (ML_BENCH_TEXT) and the splits of
    # splits-mnetgen. It is built only where Torch is, BundleSolverML being
    # built into BundleSolver only in that case, and it is no test: it trains
    # a network and measures, hence it is run by hand.
    find_package(Torch QUIET HINTS ${Torch_ROOT})
    if (Torch_FOUND AND TARGET SMS++::BundleSolverML)

        add_executable(MMCFBlock_ML_bench ../ml_bench.cpp)
        target_include_directories(MMCFBlock_ML_bench PRIVATE ..)
        target_compile_definitions(MMCFBlock_ML_bench PRIVATE ML_BENCH_TEXT=1)
        target_link_libraries(MMCFBlock_ML_bench PRIVATE
                              SMS++::BundleSolverML
                              SMS++::BundleSolver
                              SMS++::LagrangianDualSolver
                              SMS++::MMCFBlock
                              ${TORCH_LIBRARIES})
        target_compile_features(MMCFBlock_ML_bench PUBLIC cxx_std_20)

        # on Windows the Torch DLLs have to sit next to the executable
        if (MSVC)
            file(GLOB TORCH_DLLS "${TORCH_INSTALL_PREFIX}/lib/*.dll")
            add_custom_command(TARGET MMCFBlock_ML_bench POST_BUILD
                               COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                       ${TORCH_DLLS}
                                       $<TARGET_FILE_DIR:MMCFBlock_ML_bench>
                               COMMENT "Copying Torch DLLs next to it")
        endif ()

        smspp_install(MMCFBlock_ML_bench)
    endif ()

# Tag this directory's tests with their per-module CI labels
# (the label map lives in cmake/TestLabels.cmake).
smspp_label_tests()

# --------------------------------------------------------------------------- #
