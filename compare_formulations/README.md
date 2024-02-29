# test/compare_formulations

A very simple tester for testing different formulations of some problem.

This main loads a `Block` twice. Then it `BlockConfig`-ure each copy with
a different `BlockConfig`, taken by two different files, assumed to
produce two different formulations of the same problem. Then it attaches
two identical `Solver` to the two copies of the `Block` (by using the
same `BlockSolverConfig`), solve both and compare the results.

Examples of `:Block` that have different configurations that can be tested
in this way are [UCBlock](https://gitlab.com/smspp/ucblock),
[MMCFBlock](https://gitlab.com/smspp/mmcfblock) and
[CapacitatedFacilityLocationBlock](https://gitlab.com/smspp/capacitatedfacilitylocationblock).

The usage of the executable is the following:

       ./compare_formulations block_filename [cfg_1_filename cfg_1_filename]
       default: RBlockConfig1.txt RBlockConfig1.txt

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
