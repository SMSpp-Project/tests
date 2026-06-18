# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(MCFBlock)
    find_package(UCBlock)
    # Solvers
    find_package(MILPSolver)
    find_package(MCFClassSolver)
    find_package(FrankWolfeSolver)
endif ()

# ----- Tests --------------------------------------------------------------- #
if (TARGET SMS++::FrankWolfeSolver)

    add_executable(FWS_test test.cpp ../common_utils.cpp)
    target_include_directories(FWS_test PRIVATE ..)
    target_link_libraries(FWS_test PRIVATE SMS++::FrankWolfeSolver)

    # MCFBlock + MCFClassSolver provide the MCFBlock sub-Block and its LMO
    if (TARGET SMS++::MCFBlock)
        target_link_libraries(FWS_test PRIVATE SMS++::MCFBlock)
    endif ()
    if (TARGET SMS++::MCFClassSolver)
        target_link_libraries(FWS_test PRIVATE SMS++::MCFClassSolver)
    endif ()

    # UCBlock provides the ThermalUnitBlock sub-Block and its ThermalUnitDPSolver
    if (TARGET SMS++::UCBlock)
        target_link_libraries(FWS_test PRIVATE SMS++::UCBlock)
    endif ()

    # MILPSolver provides the monolithic :MILPSolver used as the cross-check
    if (TARGET SMS++::MILPSolver)
        target_link_libraries(FWS_test PRIVATE SMS++::MILPSolver)
    endif ()

    smspp_install(FWS_test)

    # the fast regression suite (cross-checks vs a :MILPSolver on small MCFBlock
    # instances); the script defaults its data/config paths relative to the
    # source directory, and takes the executable as its first argument
    if (UNIX)
        add_test(NAME FWS_test/regression
                 COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/regression
                         ${CMAKE_CURRENT_BINARY_DIR}/FWS_test
                 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    elseif (WIN32)
        add_test(NAME FWS_test/regression
                 # using Git for Windows that includes Git Bash
                 COMMAND "C:/Program Files/Git/bin/bash.exe"
                         "${CMAKE_CURRENT_SOURCE_DIR}/regression"
                         "$<TARGET_FILE:FWS_test>"
                 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    endif ()
endif ()

# --------------------------------------------------------------------------- #
