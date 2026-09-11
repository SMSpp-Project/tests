# AbstractBlock_mirror test

A tester for `AbstractBlock::mirror()`, i.e., for the copy of the abstract
representation of a Block that any Block has without having written a line
for it.

The tester builds the copy of two Block and checks three things: that the
copy is the same problem as the original, by solving the two with the same
`:MILPSolver` and comparing the optimal values; that solution information
moves from the copy back to the original; and that the copy follows the
original when this changes, which an `UpdateSolver` attached to the latter
does by mapping every `Modification` forward.

The two originals are an `AbstractBlock` carrying one object of each kind the
mirror knows, an inner Block whose `Constraint` is written in the `Variable`
of the father comprised, and a `BinaryKnapsackBlock`, which has a physical
representation of its own and generates its abstract one.

## Running the test

    ./AbstractBlock_mirror_test [ <solver> ]

with `<solver>` the name of the `:MILPSolver` to use, `CPXMILPSolver` by
default. The `batch` script runs the tester over every `:MILPSolver` that is
in the build.
