# --------------------------------------------------------------------------- #
add_library(MMCFClass)
target_compile_features(MMCFClass PUBLIC cxx_std_14)

target_sources(MMCFClass PRIVATE Graph/Graph.C)
target_include_directories(MMCFClass PUBLIC
                           Graph
                           MMCFClas
                           OPTUtils)

if (CPLEX_FOUND)
    target_sources(MMCFClass PRIVATE MMCFCplex/MMCFCple.C)
    target_include_directories(MMCFClass PUBLIC MMCFCplex)
    target_link_libraries(MMCFClass PUBLIC CPLEX::Cplex)
endif ()
# --------------------------------------------------------------------------- #
