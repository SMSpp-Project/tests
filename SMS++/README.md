# tests/SMS++

The tests posed on the objects of the core library, one directory per object:
the `AbstractBlock` (the box-structured one whose Lagrangian dual is computed,
the copy its `mirror()` makes and the round trip of a linear program through a
file), `BoxSolver`, `LagBFunction`, `BendersBFunction`, `PolyhedralFunction`
and `PolyhedralFunctionBlock`.

What is tested here is the core, and yet none of these can live in
`SMS++/test`: each of them asks either for a `:Solver` of a module the core
does not depend on, or for the cross-check machinery of
[`common_utils`](../common_utils.h), which is of this repository. The unit
tests that need neither are in `SMS++/test`, in the module.
