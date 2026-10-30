# test/compare_formulations

A very simple tester for testing different formulations of some problem.

This main loads a `Block` once per formulation, each given by a `BlockConfig`
of the list in the file given with `-F` (a `SimpleConfiguration< std::vector<
Configuration * > >`), and `BlockConfig`-ures each copy with its own, assumed
to produce a formulation of the same problem. Each copy gets the `Solver` of
the `BlockSolverConfig` that its formulation carries, when the formulation is a
pair `BlockConfig` + `BlockSolverConfig` (a `SimpleConfiguration< std::pair<
Configuration * , Configuration * > >`), and those of `-S` otherwise; all the
copies are solved and their `Solver` compared with each other, by the same
`SolveFormulations()` of `common_utils` that the other testers use when given
`-F`. Both the `BlockConfig` and the `BlockSolverConfig` can be "meta", i.e.,
actually a 'SimpleConfiguration< std::map< std::string , Configuration * > >'
that maps some 'classname()' into the `BlockConfig` or `BlockSolverConfig` that
must be 'apply()'-ed to it.

Examples of `:Block` that have different configurations that can be tested
in this way are [UCBlock](https://gitlab.com/smspp/ucblock),
[MMCFBlock](https://gitlab.com/smspp/mmcfblock) and
[CapacitatedFacilityLocationBlock](https://gitlab.com/smspp/capacitatedfacilitylocationblock).

The usage of the executable is the following:

       ./compare_formulations block_filename -F formulations [-S BlockSolverConfig] [-r reference]

e.g., `-F MetaForm.txt`, the list that the [batch](batch) compares, whose
two formulations are `MetaConfig1.txt` with `BSCfg1.txt` and `MetaConfig2.txt`
with `BSCfg2.txt`.

A [makefile](makefile) is also provided that builds the executable including
the [UCBlock](https://gitlab.com/smspp/ucblock) module, the
[MILPSolver](https://gitlab.com/smspp/milpsolver) module and all its
dependencies (and, obviously, the "core SMS++" library). Other `:Block`
and/or `:Solver` could be easily added. It should be similarly easy to edit
the [CMakeLists.txt](CMakeLists.txt) to add more modules.


## Authors

- **Antonio Frangioni**  
  Dipartimento di Informatica  
  Università di Pisa

## License

This code is provided free of charge under the [GNU Lesser General Public
License version 3.0](https://opensource.org/licenses/lgpl-3.0.html) -
see the [LICENSE](LICENSE) file for details.
