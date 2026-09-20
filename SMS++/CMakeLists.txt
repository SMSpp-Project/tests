# --------------------------------------------------------------------------- #
#    The tests posed on the objects of the core library                       #
#                                                                             #
#    One directory per object: what is tested here is the core, but each of   #
#    these testers needs either a :Solver of a module the core does not       #
#    depend on, or the cross-check machinery of common_utils, hence it        #
#    cannot live in SMS++/test.                                               #
# --------------------------------------------------------------------------- #

add_subdirectory(AbstractBlock)
add_subdirectory(BoxSolver)

add_subdirectory(LagBFunction)
add_subdirectory(BendersBFunction)

add_subdirectory(PolyhedralFunction)
add_subdirectory(PolyhedralFunctionBlock)

# --------------------------------------------------------------------------- #
