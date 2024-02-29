# ----- BendersBFunction_test ----------------------------------------------- #
if (TARGET SMS++::BundleSolver AND
    TARGET SMS++::MILPSolver AND
    TARGET MCFClass::MCFClass)

    add_executable(BendersBFunction_test test.cpp
                   cwl-mcf/cwl-mcf.cpp)
    target_link_libraries(BendersBFunction_test PRIVATE
                          SMS++::BundleSolver
                          MCFClass::MCFClass
                          SMS++::MILPSolver)

    if ( (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND
          CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9") OR
         (CMAKE_CXX_COMPILER_ID MATCHES "GNU" AND
          CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9" ) )
    target_link_libraries(BendersBFunction_test PRIVATE
                              stdc++fs)
    endif ()

    add_executable(BendersBFunction_test2 test2.cpp)

    target_link_libraries(BendersBFunction_test2 PRIVATE
                          SMS++::BundleSolver
                          SMS++::MILPSolver)

    add_test(NAME BendersBFunction_test2
             COMMAND BendersBFunction_test2 "BundlePar-cwl.txt"
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif ()

# --------------------------------------------------------------------------- #
