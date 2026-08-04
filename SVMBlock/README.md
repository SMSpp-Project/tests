# test/SVMBlock

A tester for `SVMBlock` and for the ad hoc `SMOSolver`, and for the ability of
a general-purpose `:MILPSolver` and of `LagrangianDualSolver` to solve the SVM
training problem in each of the formulations the `Block` can generate.

A data set is generated out of a seed, a `SVCBlock` or a `SVRBlock` is loaded
with it and its abstract representation is generated in one of the
formulations, which the bit-wise value `wf` selects together with the loss and
with how the bias is dealt with. All the `:Solver` of the given
`BlockSolverConfig` are then registered to the `Block`, and the results they
obtain are cross-checked against each other: they all address the same
training problem, hence they all have to agree on its optimal value, whichever
formulation of it the abstract representation encodes and however each of them
gets there. That is the test. Since no reference value is hard-wired, the
`BlockSolverConfig` (`-S`) has to be given explicitly.

Three `:Solver` are involved:

- a `:MILPSolver`, which reads the abstract representation, hence solves the
  dense quadratic program the Wolfe dual is, the diagonal one the primal is,
  or, on the decomposed formulation, the whole consensus reformulation;

- `SMOSolver`, which ignores the abstract representation altogether and solves
  the dual out of the data with Sequential Minimal Optimization, reporting the
  value of the objective of whichever formulation was generated;

- `LagrangianDualSolver`, only on the decomposed formulation, which relaxes
  the consensus constraints tying the copies of the model of each chunk and
  solves the resulting Lagrangian dual with a `BundleSolver`, each subproblem
  being the SVM training problem of one chunk. The training problem being
  convex and the decomposed formulation an exact reformulation of it, the
  bound it converges to is the optimal value itself.

The usage of the executable is the following:

       ./SVM_test -S <BlockSolverConfig> [options] [file]
       -f, --wf <bits>    what formulation, coded bit-wise [0]
                          0-1 = 0 Wolfe dual, 1 primal, 2 decomposed
                          +4  = squared loss
                          +8  = regularised bias
       -s, --nchunk <n>   chunks of the decomposed formulation [4]
       -K, --kernel <n>   0 linear, 1 poly, 2 gaussian, 3 laplacian,
                          4 sigmoid [0]
       -g, --regress      regression instead of classification
       -N, --nsample <n>  number of samples [60]
       -M, --nfeature <n> number of features [4]
       -C, --parC <x>     trade-off parameter C [1]
       -E, --epsilon <x>  half-width of the insensitivity tube [0.1]
       -e, --seed <n>     pseudo-random generator seed [1]
       -n, --rounds <n>   how many rounds, each with its own data set [10]
       -t, --tol <x>      relative tolerance of the cross-check [1e-5]
       -r, --ref <x>      reference objective value

`./SVM_test --help` lists the standard options as well. If a `file` is given
it is read as a `SVMBlock` in netCDF format and no data set is generated.

Note that the primal formulations only exist for the linear kernel, the only
one whose feature map is the identity, and that `LagrangianDualSolver` only
applies to the decomposed one, whence the two `BlockSolverConfig`:
[BSPar.txt](BSPar.txt), a `:MILPSolver` and `SMOSolver`, and
[BSPar-LD.txt](BSPar-LD.txt), which adds `LagrangianDualSolver`.

The batch files in [batches](batches) sweep, respectively, every formulation
with the linear kernel (`batch`), every kernel on the Wolfe dual
(`batch-kernels`) and every granularity of the decomposition (`batch-decomposed`).
