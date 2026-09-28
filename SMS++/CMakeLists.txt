# ----- Requirements -------------------------------------------------------- #
# Nothing of its own: this directory groups the tests posed on the objects of
# the core library, one directory each, and every one of them states what it
# needs. What is tested here is the core, but each of these testers asks
# either for a :Solver of a module the core does not depend on, or for the
# cross-check machinery of common_utils, hence none of them can live in
# SMS++/test.

# ----- Tests --------------------------------------------------------------- #
add_subdirectory(AbstractBlock)
add_subdirectory(BoxSolver)

add_subdirectory(LagBFunction)
add_subdirectory(BendersBFunction)

add_subdirectory(PolyhedralFunction)
add_subdirectory(PolyhedralFunctionBlock)

# --------------------------------------------------------------------------- #
