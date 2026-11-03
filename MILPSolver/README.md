# tests/MILPSolver

The tests posed on the objects a `:MILPSolver` is asked to handle, one
directory each: [QuadFunction](QuadFunction), the quadratic objectives and
constraints of the QPLib instances, read into an `AbstractBlock` and solved
against the optimum each of them is published with, and
[ConvexMIQP](ConvexMIQP), random strictly convex MIQPs generated from a seed,
on which the `FASTQPAMILPSolver` is cross-checked against the backends.

They are here, and not in `MILPSolver/test`, because they ask for the
cross-check machinery of [`common_utils`](../common_utils.h), which is of this
repository; the unit tests of the module, which do not, are in
`MILPSolver/test`.
