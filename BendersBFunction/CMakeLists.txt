# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(MCFBlock)
    # Solvers
    find_package(MCFClassSolver)
    find_package(BundleSolver)
    find_package(MILPSolver)
endif ()

# ----- Tests --------------------------------------------------------------- #
# BendersBFunction_test pairs the BendersBFunction with a min-cost flow built
# directly through MCFClass, so it needs the MCFClassSolver module (which
# carries MCFClass and pulls in MCFBlock); BendersBFunction_test2 only needs
# MCFBlock.
if (TARGET SMS++::MCFClassSolver)

    add_executable(BendersBFunction_test test.cpp
                                         cwl-mcf/cwl-mcf.cpp
                                         ../common_utils.cpp)
    target_include_directories(BendersBFunction_test PRIVATE ..)
    target_link_libraries(BendersBFunction_test PRIVATE SMS++::MCFClassSolver)

    if (TARGET SMS++::BundleSolver)
        target_link_libraries(BendersBFunction_test PRIVATE SMS++::BundleSolver)
    endif ()

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(BendersBFunction_test PRIVATE SMS++::MILPSolver)
    endif ()

    smspp_install(BendersBFunction_test)
endif ()

if (TARGET SMS++::MCFBlock)

    add_executable(BendersBFunction_test2 test2.cpp ../common_utils.cpp)
    target_include_directories(BendersBFunction_test2 PRIVATE ..)

    if (TARGET SMS++::BundleSolver)
        target_link_libraries(BendersBFunction_test2 PRIVATE SMS++::BundleSolver)
    endif ()

    if (TARGET SMS++::MILPSolver)
        target_link_libraries(BendersBFunction_test2 PRIVATE SMS++::MILPSolver)
    endif ()

    smspp_install(BendersBFunction_test2)

    add_test(NAME BendersBFunction_test2
             COMMAND BendersBFunction_test2 "BundlePar-cwl.txt"
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif ()

# Tag this directory's tests with their per-module CI labels
# (the label map lives in cmake/TestLabels.cmake).
smspp_label_tests()

# --------------------------------------------------------------------------- #
