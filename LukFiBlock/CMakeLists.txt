find_package(BundleSolver)
add_executable(LukFi_test main.cpp)
target_link_libraries(LukFi_test PRIVATE
                      SMS++::LukFiBlock
                      SMS++::BundleSolver)
