# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    find_package(SMS++)
endif ()

# Skip the suite if some module its labels declare is not in the build
smspp_require_labelled_modules()

# ----- Tests --------------------------------------------------------------- #
add_executable(compare_formulations test.cpp ../common_utils.cpp)
target_include_directories(compare_formulations PRIVATE ..)
target_link_libraries(compare_formulations PRIVATE SMS++::SMS++)

# the Block whose formulations are compared and the Solver that solve them
# come from the factory: what is in the build is linked, as the makefile
# does, the batch comparing those of UCBlock with a :MILPSolver
foreach (module UCBlock MILPSolver)
    if (TARGET SMS++::${module})
        target_link_libraries(compare_formulations PRIVATE SMS++::${module})
    endif ()
endforeach ()

# Tag this directory's tests with their per-module CI labels
# (the label map lives in cmake/TestLabels.cmake).
smspp_label_tests()

# --------------------------------------------------------------------------- #
