# tests/MILPSolver

The tests posed on the objects a `:MILPSolver` is asked to handle, one
directory each: for now [QuadFunction](QuadFunction), the quadratic objectives
and constraints of the QPLib instances, read into an `AbstractBlock` and
solved against the optimum each of them is published with.

They are here, and not in `MILPSolver/test`, because they ask for the
cross-check machinery of [`common_utils`](../common_utils.h), which is of this
repository; the unit tests of the module, which do not, are in
`MILPSolver/test`.
