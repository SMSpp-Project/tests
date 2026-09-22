# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(UCBlock)
    # Solvers
    find_package(LagrangianDualSolver)
    find_package(BundleSolver)
    find_package(MILPSolver)
    find_package(FrankWolfeSolver)
endif ()

# Skip the suite if some module its labels declare is not in the build
smspp_require_labelled_modules()

# ----- Tests --------------------------------------------------------------- #
if (TARGET SMS++::UCBlock)

    add_executable(UCBlock_test test.cpp ../common_utils.cpp)
    target_include_directories(UCBlock_test PRIVATE ..)
    target_link_libraries(UCBlock_test PRIVATE SMS++::UCBlock)

    if (TARGET SMS++::LagrangianDualSolver)
        target_link_libraries(UCBlock_test PRIVATE SMS++::LagrangianDualSolver)
    endif ()

    if (TARGET SMS++::BundleSolver)
        target_link_libraries(UCBlock_test PRIVATE SMS++::BundleSolver)
    endif ()

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(UCBlock_test PRIVATE SMS++::MILPSolver)
    endif ()

    smspp_install(UCBlock_test)

    # the pollutant budget constraints, on instances the tester writes itself
    if (TARGET SMS++::MILPSolver)
        add_test(NAME UCBlock_test/pollutant
                 COMMAND UCBlock_test --pollutant)
    endif ()

    # the scale factor of a unit, on instances the tester writes itself; the
    # model is checked with no Solver at all, the optimum and the DP Solvers
    # only when a :MILPSolver is in the build
    add_test(NAME UCBlock_test/scale
             COMMAND UCBlock_test --scale)

    # Create a test fixture to generate .nc4 files by running extract_uc_nc4.
    # This test will build and run the extract_uc_nc4 target
    add_test(NAME extract_uc_nc4
             COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target extract_uc_nc4)
    # Mark extract_uc_nc4 as a fixture that sets up "uc_data_extracted"
    set_tests_properties(extract_uc_nc4
                         PROPERTIES FIXTURES_SETUP uc_data_extracted)

    # Register every batch: each one covers a family of instances (academic,
    # plan4res, energy community, PyPSA, AC OPF) and cross-checks all the
    # Solver its BlockSolverConfig attaches, so they are independent and can be
    # run, selected and timed one by one.
    file(GLOB BATCH_FILES "${CMAKE_CURRENT_SOURCE_DIR}/batches/*")
    foreach (BATCH_FILE ${BATCH_FILES})
        # Remove the source directory prefix to get the batch name
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" BATCH_NAME ${BATCH_FILE})
        if (UNIX)
            add_test(NAME UCBlock_test/${BATCH_NAME}
                     COMMAND bash ${BATCH_FILE} ${CMAKE_CURRENT_BINARY_DIR}/UCBlock_test
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        elseif (WIN32)
            add_test(NAME UCBlock_test/${BATCH_NAME}
                     # using Git for Windows that includes Git Bash
                     COMMAND "C:/Program Files/Git/bin/bash.exe"
                             "${BATCH_FILE}" "$<TARGET_FILE:UCBlock_test>"
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        endif ()
        # Make each batch test require the uc_data_extracted fixture
        set_tests_properties(UCBlock_test/${BATCH_NAME}
                             PROPERTIES FIXTURES_REQUIRED uc_data_extracted)
    endforeach ()

    # The Frank-Wolfe decomposition of a father Block whose leaves are the
    # ThermalUnitBlock of this suite, each with its ThermalUnitDPSolver as the
    # Linear Minimization Oracle: since the dynamic programme plus the
    # Perspective Cuts characterize the convex hull of the integer solutions
    # of a unit, the Dantzig-Wolfe value Frank-Wolfe computes must be the
    # perspective bound a monolithic :MILPSolver gives, which is what the
    # batteries of the single units check on a few of them. The tester is the
    # generic one (../fw_test.cpp), the same the MCFBlock suite builds on its
    # own Block, and its configurations are those of FW/.
    if (TARGET SMS++::FrankWolfeSolver)
        add_executable(UCBlock_FW_test ../fw_test.cpp ../common_utils.cpp)
        target_include_directories(UCBlock_FW_test PRIVATE ..)
        target_link_libraries(UCBlock_FW_test PRIVATE
                              SMS++::FrankWolfeSolver SMS++::UCBlock)

        if (TARGET SMS++::MILPSolver)
            target_link_libraries(UCBlock_FW_test PRIVATE SMS++::MILPSolver)
        endif ()

        smspp_install(UCBlock_FW_test)
    endif ()

    # The specialised dynamic programming solver of ThermalUnitBlock against a
    # :MILPSolver on the same unit: it reads the instances of this suite and
    # compares two solvers of this module's Block, hence it is one of the
    # tests of UCBlock and not a suite of its own. Its batteries live apart,
    # in batches-tub, because they walk the instances that carry one unit
    # alone rather than a whole unit commitment.
    add_executable(TUDPS_test test_tudps.cpp ../common_utils.cpp)
    target_include_directories(TUDPS_test PRIVATE ..)
    target_link_libraries(TUDPS_test PRIVATE SMS++::UCBlock)

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(TUDPS_test PRIVATE SMS++::MILPSolver)
    endif ()

    smspp_install(TUDPS_test)

    # Each battery of batches-tub walks a family of the instances that carry
    # one unit alone, thermal or nuclear, and runs it with every Solver that
    # applies to it: the dynamic programme against the :MILPSolver with this
    # tester, and, where the family is one Frank-Wolfe can be posed on, the
    # decomposition of a father of K copies of the unit with the tester above,
    # which is why each of them is given both.
    file(GLOB TUB_BATCH_FILES "${CMAKE_CURRENT_SOURCE_DIR}/batches-tub/*")
    foreach (BATCH_FILE ${TUB_BATCH_FILES})
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" BATCH_NAME ${BATCH_FILE})
        add_batch_test(TUDPS_test ${BATCH_NAME} UCBlock_FW_test)
        set_tests_properties(TUDPS_test/${BATCH_NAME}
                             PROPERTIES FIXTURES_REQUIRED uc_data_extracted)
    endforeach ()

endif ()

# The scenario generator of this Block and the battery that reduces what it
# writes: the reduction is asked of ScenarioReductionSolver, but what it is
# asked of is the unit commitment instances, hence it belongs to this suite.
# The generator reads a UCBlock instance, perturbs the demand and the
# generation of its intermittent units, and writes both the scenario set and
# the TwoStageStochasticBlock the battery then reduces and solves with the
# tester of ScenarioReductionSolver, which is generic and lives in that
# module; the battery is therefore run with the two of them and not with the
# tester of this suite.
if (TARGET SMS++::UCBlock AND TARGET SMS++::StochasticBlock)

    add_executable(UCScenarioGenerator
                   UCScenarioGenerator.cpp ../common_utils.cpp)
    target_include_directories(UCScenarioGenerator PRIVATE ..)
    target_link_libraries(UCScenarioGenerator PRIVATE
                          SMS++::UCBlock
                          SMS++::StochasticBlock)

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(UCScenarioGenerator PRIVATE SMS++::MILPSolver)
    endif ()

    smspp_install(UCScenarioGenerator)

    if (TARGET SMS++::ScenarioReductionSolver)
        add_batch_test(UCScenarioGenerator batches-scenred/batch
                       ScenarioReductionSolver_test)
        set_tests_properties(UCScenarioGenerator/batches-scenred/batch
                             PROPERTIES FIXTURES_REQUIRED uc_data_extracted)
    endif ()
endif ()

    # The benchmark of the machine-learning driven bundle on the instances of
    # this Block: the harness is the generic one (../ml_bench.cpp), which
    # reads whichever instance it is given, here the netCDF ones of this
    # module. It is built only where Torch is, BundleSolverML being
    # built into BundleSolver only in that case, and it is no test: it trains
    # a network and measures, hence it is run by hand.
    find_package(Torch QUIET HINTS ${Torch_ROOT})
    if (Torch_FOUND AND TARGET SMS++::BundleSolverML)

        add_executable(UCBlock_ML_bench ../ml_bench.cpp)
        target_include_directories(UCBlock_ML_bench PRIVATE ..)
        target_link_libraries(UCBlock_ML_bench PRIVATE
                              SMS++::BundleSolverML
                              SMS++::BundleSolver
                              SMS++::LagrangianDualSolver
                              SMS++::UCBlock
                              ${TORCH_LIBRARIES})
        target_compile_features(UCBlock_ML_bench PUBLIC cxx_std_20)

        # on Windows the Torch DLLs have to sit next to the executable
        if (MSVC)
            file(GLOB TORCH_DLLS "${TORCH_INSTALL_PREFIX}/lib/*.dll")
            add_custom_command(TARGET UCBlock_ML_bench POST_BUILD
                               COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                       ${TORCH_DLLS}
                                       $<TARGET_FILE_DIR:UCBlock_ML_bench>
                               COMMENT "Copying Torch DLLs next to it")
        endif ()

        smspp_install(UCBlock_ML_bench)
    endif ()

# Tag this directory's tests with their per-module CI labels
# (the label map lives in cmake/TestLabels.cmake).
smspp_label_tests()

# --------------------------------------------------------------------------- #
