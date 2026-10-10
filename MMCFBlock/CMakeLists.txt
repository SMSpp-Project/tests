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
    find_package(MCFClassSolver)
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
            COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_SOURCE_DIR}/data/Mnetgen/.generated
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
            COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_SOURCE_DIR}/data/Mnetgen/.generated
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

# ----- Data ---------------------------------------------------------------- #
# The Canad instances, which the batches read; their version in the Package
# Registry is the only one published, before the archives were versioned.
#
# The instances the tests read are, for each <fmt> of DATA_FORMATS, the
# archive <fmt>.tgz in the GitLab Package Registry of the project
# DATA_PROJECT, extracted into data/<fmt>. Nothing of this is built with the
# tests: extract_<DATA_PREFIX>_<fmt> downloads the archive (by building
# download_<DATA_PREFIX>_<fmt>) and extracts it, and it is built when the
# tests that read the instances are run, by a test that sets up the fixture
# <DATA_PREFIX>_<fmt>_extracted they require.
#
# A published version is never replaced: new data go out under a new
# version, written here first [see the upload scripts in data/], and the
# archive and the marker of its extraction carry it in their name, so that a
# tree holding the data of an older version (a cache of the CI, or a clone
# that has been extracted before) downloads and extracts again instead of
# taking them for the current ones.
set(DATA_VERSION latest)
set(DATA_PROJECT 15602443)
set(DATA_PREFIX mmcf)
set(DATA_FORMATS Canad)

foreach (fmt ${DATA_FORMATS})
    set(url "https://gitlab.com/api/v4/projects/${DATA_PROJECT}/packages/\
generic/${fmt}/${DATA_VERSION}/${fmt}.tgz")
    set(archive ${CMAKE_CURRENT_SOURCE_DIR}/data/${fmt}-${DATA_VERSION}.tgz)
    set(script
        ${CMAKE_CURRENT_BINARY_DIR}/download_${DATA_PREFIX}_${fmt}.cmake)
    # the marker of the extraction, on which what is made from the instances
    # depends too
    set(DATA_MARKER_${fmt}
        ${CMAKE_CURRENT_SOURCE_DIR}/data/.extracted-${fmt}-${DATA_VERSION})

    file(WRITE ${script} "
file(DOWNLOAD \"${url}\" \"${archive}\" SHOW_PROGRESS STATUS _status)
list(GET _status 0 _code)
list(GET _status 1 _message)
if(NOT _code EQUAL 0)
  # what is on disk is whatever the server answered with, and leaving it there
  # would let the build take it for the archive: the next attempt has to
  # download again rather than fail while extracting it
  file(REMOVE \"${archive}\")
  message(FATAL_ERROR \"cannot download ${url}: \${_message}\")
endif()
file(SIZE \"${archive}\" _size)
if(_size LESS 1024)
  file(REMOVE \"${archive}\")
  message(FATAL_ERROR \"${archive} came down with \${_size} bytes, so the \
server answered but not with the archive: try again later\")
endif()
")

    add_custom_command(
            OUTPUT ${archive}
            COMMAND ${CMAKE_COMMAND} -P ${script}
            COMMENT "Downloading ${fmt}.tgz from the GitLab Package Registry"
            VERBATIM)
    add_custom_target(download_${DATA_PREFIX}_${fmt} DEPENDS ${archive})

    add_custom_command(
            OUTPUT ${DATA_MARKER_${fmt}}
            COMMAND ${CMAKE_COMMAND} -E tar xzf ${archive}
            COMMAND ${CMAKE_COMMAND} -E touch ${DATA_MARKER_${fmt}}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data
            COMMENT "Extracting ${fmt}.tgz"
            VERBATIM
            DEPENDS ${archive})
    add_custom_target(extract_${DATA_PREFIX}_${fmt}
                      DEPENDS ${DATA_MARKER_${fmt}})
    # the archive is the output of a command in both targets: it has to be
    # downloaded once, before it is extracted
    add_dependencies(extract_${DATA_PREFIX}_${fmt}
                     download_${DATA_PREFIX}_${fmt})
endforeach ()

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

# the instances of the suite, downloaded and extracted when the tests that read
# them are run, not when they are built [see the section Data above]; the lock
# is shared by the tests that build a target of the data of the suite, which
# under ctest -j would write the same files at once
add_test(NAME extract_mmcf_Canad
         COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
                 --target extract_mmcf_Canad)
set_tests_properties(extract_mmcf_Canad PROPERTIES
                     FIXTURES_SETUP mmcf_Canad_extracted
                     RESOURCE_LOCK mmcf_data)

if (TARGET SMS++::MMCFBlock AND
    TARGET MMCFClass AND
    CPLEX_FOUND)

    add_executable(MMCF_test test_reference.cpp)
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
                             "mmcf_Canad_extracted;mnetgen_generated")
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

    # the flow relaxation of BSPar-2S.txt asks for MCFSolver< MCFSimplex >
    if (TARGET SMS++::MCFClassSolver)
        target_link_libraries(MMCFBlock_test PRIVATE SMS++::MCFClassSolver)
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
            set(BATCH_FIXTURES "mmcf_Canad_extracted;mnetgen_generated")
        elseif (BATCH_NAME STREQUAL "batches/batch-m")
            set(BATCH_FIXTURES "mnetgen_generated")
        else ()
            set(BATCH_FIXTURES "mmcf_Canad_extracted")
        endif ()
        set_tests_properties(MMCFBlock_test/${BATCH_NAME}
                             PROPERTIES FIXTURES_REQUIRED "${BATCH_FIXTURES}")
    endforeach ()
endif ()

# The third executable solves small Network Design instances (data-nd, a
# Canad instance with fewer commodities) with MMCFNetworkDesignBlock in the
# monolithic formulation and in the Benders one, whose cuts come from a
# LagrangianDualSolver of the MMCFBlock, and cross-checks the two, and the
# cuts with the cost of the flow.
if (TARGET SMS++::MMCFBlock AND
    TARGET SMS++::LagrangianDualSolver AND
    TARGET SMS++::BundleSolver AND
    TARGET SMS++::MILPSolver AND
    TARGET SMS++::MCFClassSolver)

    add_executable(MMCFND_test nd_test.cpp)
    target_link_libraries(MMCFND_test PRIVATE
                          SMS++::MMCFBlock
                          SMS++::LagrangianDualSolver
                          SMS++::BundleSolver
                          SMS++::MILPSolver
                          SMS++::MCFClassSolver)

    smspp_install(MMCFND_test)

    if (UNIX)
        add_test(NAME MMCFND_test/batches/batch-nd
                 COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/batches/batch-nd
                         ${CMAKE_CURRENT_BINARY_DIR}/MMCFND_test
                 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    endif ()
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
