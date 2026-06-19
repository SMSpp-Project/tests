# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    find_package(SMS++)
endif ()

# ----- Tests --------------------------------------------------------------- #
add_executable(compare_formulations test.cpp ../common_utils.cpp)
target_include_directories(compare_formulations PRIVATE ..)
target_link_libraries(compare_formulations PRIVATE SMS++::SMS++)

# Tag this directory's tests with their per-module CI labels
# (the label map lives in cmake/TestLabels.cmake).
smspp_label_tests()

# --------------------------------------------------------------------------- #
