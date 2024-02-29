# ----- Tests --------------------------------------------------------------- #
if (TARGET SMS++::BundleSolver AND
    TARGET SMS++::MILPSolver)

    add_executable(PolyhedralFunctionBlock_test test.cpp)
    target_link_libraries(PolyhedralFunctionBlock_test PRIVATE
                          SMS++::BundleSolver
                          SMS++::MILPSolver)

    # This version copied all the *Par.txt files and the batch shell script
    # so it could run as a test from the build directory.

    # file(GLOB parFiles "*Par.txt")
    # foreach (parFile ${parFiles})
    #     configure_file(${parFile} ${CMAKE_CURRENT_BINARY_DIR} COPYONLY)
    # endforeach ()
    # configure_file(batch ${CMAKE_CURRENT_BINARY_DIR} COPYONLY)
    # add_test(NAME PolyhedralFunctionBlock_test
    #          COMMAND bash "${CMAKE_CURRENT_BINARY_DIR}/batch")

    # This version replicates the batch file in CMake.
    # This method was preferred because it allows to keep track
    # of the single tests via the CTest application and also
    # avoids copying all the auxiliary files.
    set(what 31)
    set(nf -10)
    foreach (size 2 10 50)
        foreach (dens 3 1.1)
            foreach (seed RANGE 0 19)
                add_test(NAME "PolyhedralFunctionBlock_test/[${seed},${what},${size},${dens},${nf}]"
                         COMMAND PolyhedralFunctionBlock_test ${seed} ${what} ${size} ${dens} ${nf}
                         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
            endforeach ()
        endforeach ()
    endforeach ()
endif ()

# --------------------------------------------------------------------------- #
