# tests/MCFBlock

The suite of `MCFBlock`: every `:Solver` that can solve a min-cost flow
problem, on the instances of the module, and the Frank-Wolfe decomposition of
a father `Block` whose leaves are those same `MCFBlock`.

`MCFBlock_test` reads an instance (in DIMACS or netCDF) into a `MCFBlock`,
attaches to it every `:Solver` its `BlockSolverConfig` names, and cross-checks
what they return while the instance changes: costs, capacities and deficits,
arcs opened and closed, arcs added and deleted. What each change is, and which
of them a run makes, is the bit-wise code of its `-k`. [BSPar.txt](BSPar.txt)
names a `MCFSolver< MCFSimplex >`, a `MCFSolver< RelaxIV >`, the four LEMON
algorithms of `MCFLemonSolver` on the graph that can change and a
`:MILPSolver`; [BSPar-large.txt](BSPar-large.txt) leaves out the two LEMON
algorithms that are slow on the large instances, CycleCanceling and
CapacityScaling.

The batteries are in [batches](batches), one per family of instances, and each
of them runs its family with every tester that applies to it:

- [batch-small](batches/batch-small), the small instances of `MCFClassSolver`,
  static and with dynamic arcs, with `BSPar.txt`: the fast one, and the one
  that walks every code path of the decomposition;

- [batch](batches/batch) and [batch-dense](batches/batch-dense), the `net` and
  `goto` families of the module, the second one being the large instances,
  with `BSPar-large.txt`.

The configurations of the decomposition are in [FW](FW), which says what each
of them is for.


## Authors

- **Antonio Frangioni**  
  Dipartimento di Informatica  
  Università di Pisa


## License

This code is provided free of charge under the [GNU Lesser General Public
License version 3.0](https://opensource.org/licenses/lgpl-3.0.html) -
see the [LICENSE](LICENSE) file for details.
