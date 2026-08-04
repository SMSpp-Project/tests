# test/SVMBlock

A tester for `SVMBlock` and for the ad hoc `SMOSolver`, and for the ability of
a general-purpose `:MILPSolver` and of `LagrangianDualSolver` to solve the SVM
training problem in each of the formulations the `Block` can generate.

A data set is generated out of a seed, a `SVCBlock` or a `SVRBlock` is loaded
with it and its abstract representation is generated for the problem that the
bit-wise value `wf` selects, together with the loss and with how the bias is
dealt with; with more than one chunk the training problem is rather rewritten
as the consensus reformulation that `make_consensus_Block()` assembles. All the `:Solver` of the given
`BlockSolverConfig` are then registered to the `Block`, and the results they
obtain are cross-checked against each other: they all address the same
training problem, hence they all have to agree on its optimal value, however
it is written and however each of them gets there. That is the test. Since no reference value is hard-wired, the
`BlockSolverConfig` (`-S`) has to be given explicitly.

Three `:Solver` are involved:

- a `:MILPSolver`, which reads the abstract representation, hence solves the
  dense quadratic program the Wolfe dual is, the diagonal one the training
  problem itself is, or, on the consensus rewriting, the whole of it;

- `SMOSolver`, which ignores the abstract representation altogether and solves
  the dual out of the data with Sequential Minimal Optimization, reporting the
  value of the objective of whichever formulation was generated;

- `LagrangianDualSolver`, only on the consensus rewriting, which relaxes the
  consensus constraints tying the copies of the model of each chunk and solves
  the resulting Lagrangian dual with a `BundleSolver`, each subproblem being
  the SVM training problem of one chunk. The training problem being convex and
  the rewriting an exact one, the bound it converges to is the optimal value
  itself.

The usage of the executable is the following:

       ./SVM_test -S <BlockSolverConfig> [options] [file]
       -f, --wf <bits>    what is encoded, coded bit-wise [0]
                          1  = the training problem itself, 0 = its Wolfe dual
                          +2 = squared loss
                          +4 = regularised bias
       -s, --nchunk <n>   rewrite the training problem as n chunks tied by
                          consensus constraints [1]
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

Note that the training problem itself only exists for the linear kernel, the
only one whose feature map is the identity, and that `LagrangianDualSolver`
only applies to the consensus rewriting, whence the two `BlockSolverConfig`:
[BSPar.txt](BSPar.txt), a `:MILPSolver` and `SMOSolver`, and
[BSPar-LD.txt](BSPar-LD.txt), a `:MILPSolver` and `LagrangianDualSolver`.
`SMOSolver` is not in the latter because it only attaches to a `SVMBlock`,
while what `make_consensus_Block()` assembles is not one.

There are therefore two batch files in [batches](batches), one per problem:
[batch-primal](batches/batch-primal), which sticks to the linear kernel, the
only one the training problem itself exists for, and sweeps the loss, the bias
and the granularity of the consensus rewriting, and
[batch-dual](batches/batch-dual), which sweeps the kernels, the loss, the bias
and the trade-off parameter. Each of them passes the `BlockConfig` naming its
own problem, so that what is being exercised is stated in the batch rather
than encoded in a flag.
