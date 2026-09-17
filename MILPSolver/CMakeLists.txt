# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    # Blocks
    find_package(SMS++)
    # Solvers
    find_package(MILPSolver)
endif ()

# Skip the suite if some module its labels declare is not in the build
smspp_require_labelled_modules()

# ----- Tests --------------------------------------------------------------- #
if (TARGET SMS++::MILPSolver)

    add_executable(MILPSolver_groups_test groups.cpp)
    target_link_libraries(MILPSolver_groups_test PRIVATE SMS++::MILPSolver)

    smspp_install(MILPSolver_groups_test)

    add_test(NAME MILPSolver_test/groups
             COMMAND MILPSolver_groups_test
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    # ----- Labels ---------------------------------------------------------- #
    smspp_label_tests()

endif ()
