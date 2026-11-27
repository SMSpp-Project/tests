# --------------------------------------------------------------------------- #
#    CMake file for tests/CSPBlock                                            #
# --------------------------------------------------------------------------- #

# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(CSPBlock)
endif ()

# Skip the suite if some module its labels declare is not in the build
smspp_require_labelled_modules()

# ----- Tests --------------------------------------------------------------- #
if (TARGET SMS++::CSPBlock)

    add_executable(CSPBlock_test test.cpp ../common_utils.cpp)
    target_include_directories(CSPBlock_test PRIVATE ..)
    target_link_libraries(CSPBlock_test PRIVATE SMS++::CSPBlock)

    smspp_install(CSPBlock_test)

    # the Solver of BSPar.txt run the MiniZinc driver, without which there is
    # no backend to compare
    find_program(MINIZINC minizinc)
    if (MINIZINC)
        # the instances of CSPBlock, downloaded and extracted when the tests
        # that read them are run, not when they are built [see the
        # CMakeLists.txt of the module]; the lock is shared by the tests that
        # build a target of the data of the module, which under ctest -j
        # would write the same files at once
        add_test(NAME extract_csp_fzn
                 COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
                         --target extract_csp_fzn)
        set_tests_properties(extract_csp_fzn PROPERTIES
                             FIXTURES_SETUP csp_fzn_extracted
                             RESOURCE_LOCK csp_data)

        # one test per family of instances, so that each stays within the
        # time limit of a test of the CI
        foreach (family rcpsp colouring golomb qcp)
            set(name CSPBlock_test/batches/batch-${family})
            add_test(NAME ${name}
                     COMMAND ${SMSpp_BASH}
                         "${CMAKE_CURRENT_SOURCE_DIR}/batches/batch"
                         "$<TARGET_FILE:CSPBlock_test>"
                         BSPar.txt "" ${family}
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
            set_tests_properties(${name} PROPERTIES
                                 FIXTURES_REQUIRED csp_fzn_extracted)
        endforeach ()
    else ()
        message(STATUS "Skipping the batteries of tests/CSPBlock: minizinc "
                       "not on the PATH")
    endif ()

endif ()

# Tag this directory's tests with their per-module CI labels
# (the label map lives in cmake/TestLabels.cmake).
smspp_label_tests()

# --------------------------------------------------------------------------- #
