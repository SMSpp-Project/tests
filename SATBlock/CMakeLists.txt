# --------------------------------------------------------------------------- #
#    CMake file for tests/SATBlock                                            #
# --------------------------------------------------------------------------- #

# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(SATBlock)
    # Solvers
    find_package(MILPSolver)
endif ()

# Skip the suite if some module its labels declare is not in the build
smspp_require_labelled_modules()

# ----- Tests --------------------------------------------------------------- #
if (TARGET SMS++::SATBlock)

    add_executable(SATBlock_test test.cpp ../common_utils.cpp)
    target_include_directories(SATBlock_test PRIVATE ..)
    target_link_libraries(SATBlock_test PRIVATE SMS++::SATBlock)

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(SATBlock_test PRIVATE SMS++::MILPSolver)
    endif ()

    smspp_install(SATBlock_test)

    # the batteries on the instances of the module need them downloaded and
    # extracted first, which this test does, building the target of the module
    # that does it
    add_test(NAME fetch_sat_data
             COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
                     --target fetch_sat_data)
    set_tests_properties(fetch_sat_data
                         PROPERTIES FIXTURES_SETUP sat_data_ready)

    # the generator of the instances made of groups of clauses, when it is a
    # target of this build, otherwise the one installed [see batch-groups]
    set(_satgen smspp_satgen)
    if (TARGET satgen)
        set(_satgen "$<TARGET_FILE:satgen>")
    endif ()

    foreach (BATCH_NAME batch-mse batch-groups)
        if (UNIX)
            add_test(NAME SATBlock_test/batches/${BATCH_NAME}
                     COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/batches/${BATCH_NAME}
                             ${CMAKE_CURRENT_BINARY_DIR}/SATBlock_test
                             BSPar.txt "" ${_satgen}
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        elseif (WIN32)
            add_test(NAME SATBlock_test/batches/${BATCH_NAME}
                     # using Git for Windows that includes Git Bash
                     COMMAND "C:/Program Files/Git/bin/bash.exe"
                             "${CMAKE_CURRENT_SOURCE_DIR}/batches/${BATCH_NAME}"
                             "$<TARGET_FILE:SATBlock_test>"
                             BSPar.txt "" ${_satgen}
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        endif ()
    endforeach ()
    set_tests_properties(SATBlock_test/batches/batch-mse
                         PROPERTIES FIXTURES_REQUIRED sat_data_ready)

endif ()

# Tag this directory's tests with their per-module CI labels
# (the label map lives in cmake/TestLabels.cmake).
smspp_label_tests()

# --------------------------------------------------------------------------- #
