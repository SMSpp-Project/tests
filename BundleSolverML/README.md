# test/BundleSolverML

A tester which provides validation for `BundleSolverML`, the `BundleSolver`
variant whose step-size t is predicted by a neural network (implemented
with the LibTorch C++ API) instead of the classical rule-based heuristics,
trained online across successive solves.

This executable, given the input parameter n, constructs a "random" convex
`PolyhedralFunction` and puts it as the only `Objective` of an
`AbstractBlock`, otherwise "empty" save for the n `ColVariable` active in
the `PolyhedralFunction`.

The `AbstractBlock` is first solved by a standard `BundleSolver`, whose
optimal value is taken as the reference. Then a `BundleSolverML` is
attached and the `AbstractBlock` is repeatedly solved with it, calling
`Backward()` between successive solves (epochs) so that the network is
trained online; at each epoch the optimal value is compared with the
reference one. The tester also checks that the network is actually driving
the step-size (i.e., that `Heuristic()` records iterations), that
`Backward()` actually changes the network parameters, that the
`SaveModel()` / `LoadModel()` round-trip exactly restores the weights, and
that the shared-network mechanism (`set_shared_net()` / `get_shared_net()`
/ `clear_shared_net()`) correctly redirects the active network among
multiple `BundleSolverML` objects.

The usage of the executable is the following:

       ./BundleSolverML_test seed [nvar dens #epochs]
       nvar: number of variables [10]
       dens: rows / variables [4]
       #epochs: training epochs [5]

A batch file is provided that runs a small set of tests with different
sizes and seeds of the random generator; all these passing is a good sign
that no regressions have been done for the tested modules, and in
particular for `BundleSolverML`.

A makefile is also provided that builds the executable including the
`BundleSolver` module and all its dependencies, in particular `MILPSolver`
(and, obviously, the core SMS++ library). Note that `BundleSolverML` is
only compiled into `BundleSolver` if libTorch is available [see the
BundleSolver README], hence this test requires libTorch to be installed at
`$(Torch_ROOT)` (makefile builds) or findable by `find_package(Torch)`
(CMake builds).


## Authors

- **Francesca Demelas**  
  Laboratoire d'Informatique de Paris Nord  
  Université Sorbonne Paris Nord

- **Donato Meoli**  
  Dipartimento di Informatica  
  Università di Pisa

## License

This code is provided free of charge under the [GNU Lesser General Public
License version 3.0](https://opensource.org/licenses/lgpl-3.0.html) -
see the [LICENSE](LICENSE) file for details.
