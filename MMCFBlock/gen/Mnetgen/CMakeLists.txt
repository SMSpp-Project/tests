# --------------------------------------------------------------------------- #
#    CMake file for MMCFBlock/gen/Mnetgen                                     #
# --------------------------------------------------------------------------- #

# This is needed for setting the runtime path when installing.
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# ----- mnetgen ------------------------------------------------------------- #
add_executable(mnetgen mnetgen.C)

# ----- Install instructions ------------------------------------------------ #
include(GNUInstallDirs)
install(TARGETS mnetgen
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# --------------------------------------------------------------------------- #
