# ----- Requirements -------------------------------------------------------- #
# Nothing of its own: this directory groups the tests posed on the objects a
# :MILPSolver is asked to handle, one directory each, and every one of them
# states what it needs. They are here, and not in MILPSolver/test, because
# they ask for the cross-check machinery of common_utils.

# ----- Tests --------------------------------------------------------------- #
add_subdirectory(QuadFunction)

# --------------------------------------------------------------------------- #
