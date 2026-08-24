# --------------------------------------------------------------------------
# CMakeLists.txt for tests/BundleSolverML
#
# Benchmark harness comparing two solver configurations over a split of
# instances. Built only when Torch is available, since BundleSolverML is
# itself only built into BundleSolver in that case.
# --------------------------------------------------------------------------

find_package(Torch QUIET HINTS ${Torch_ROOT})

if (Torch_FOUND)

    add_executable(BundleSolverML_bench test.cpp)

    target_link_libraries(BundleSolverML_bench PRIVATE
                          SMS++::BundleSolver
                          SMS++::UCBlock
                          SMS++::MMCFBlock
                          SMS++::LagrangianDualSolver
                          ${TORCH_LIBRARIES})

    target_compile_features(BundleSolverML_bench PUBLIC cxx_std_20)

    # on Windows the Torch DLLs have to sit next to the executable
    if (MSVC)
        file(GLOB TORCH_DLLS "${TORCH_INSTALL_PREFIX}/lib/*.dll")
        add_custom_command(TARGET BundleSolverML_bench POST_BUILD
                           COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                   ${TORCH_DLLS}
                                   $<TARGET_FILE_DIR:BundleSolverML_bench>
                           COMMENT "Copying Torch DLLs next to the executable")
    endif ()

endif ()

# --------------------------------------------------------------------------
