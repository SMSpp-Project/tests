# test/SVMBlock

A tester for `SVMBlock` and for the ad hoc `SMOSolver`, and for the ability of
a general-purpose `:MILPSolver` and of `LagrangianDualSolver` to solve the SVM
training problem in each of the formulations the `Block` can generate.

A data set is generated out of a seed, a `SVCBlock` or a `SVRBlock` is loaded
with it and its abstract representation is generated for the problem that the
bit-wise value `wf` selects, together with the loss and with how the bias is
dealt with; with more than one chunk the training problem is rather given the
consensus structure that `set_structure()` builds. All the `:Solver` of the
given `BlockSolverConfig` are then registered to the `Block`, and the results
they obtain are cross-checked against each other: they all address the same
training problem, hence they all have to agree on its optimal value, however
it is written and however each of them gets there. That is the test. Since no
reference value is hard-wired, the `BlockSolverConfig` (`-S`) has to be given
explicitly.

Four `:Solver` are involved:

- a `:MILPSolver`, which reads the abstract representation, hence solves the
  dense quadratic program the Wolfe dual is, the diagonal one the training
  problem itself is, or, on the consensus structure, the whole of it;

- `SMOSolver`, which ignores the abstract representation altogether and solves
  the dual out of the data with Sequential Minimal Optimization, reporting the
  value of the objective of whichever formulation was generated;

- `LIBSVMSolver`, which hands the training problem over to LIBSVM, the
  reference implementation of the very algorithm `SMOSolver` implements, and
  is therefore the natural thing to check the latter against; it is only there
  when `SVMBlock` has been built with LIBSVM, and it only trains the problems
  LIBSVM can express, i.e., the linear loss with the bias not regularised and
  any kernel but the Laplacian one;

- `LagrangianDualSolver`, only on the consensus structure, which relaxes the
  consensus constraints tying the copies of the model of each chunk and solves
  the resulting Lagrangian dual with a `BundleSolver`, each subproblem being
  the SVM training problem of one chunk. The training problem being convex and
  the rewriting an exact one, the bound it converges to is the optimal value
  itself. Each subproblem can be given to a `:MILPSolver` or to a `SMOSolver`
  of its own: the multipliers of the relaxed constraints are the linear term
  of the primal of the chunk, which the latter folds into the dual it solves.

The usage of the executable is the following:

       ./SVM_test -S <BlockSolverConfig> [options] [file]
       -f, --wf <bits>    the loss and the bias, coded bit-wise [0]
                          1 = squared loss
                          2 = regularised bias
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
       -R, --reopt        also change the training problem under the Solver,
                          re-solving after each change
       -d, --data <file>  a data set in the sparse format of LIBSVM, which
                          replaces the generated one
       -Y, --kmemory <MB> memory the Gram matrix may take: under it the whole
                          matrix is built, over it its rows are computed and
                          cached [1024]
       -A, --noabstract   leave the Block in its physical representation,
                          which is all a Solver reading it needs
       -G, --grid <n>     train over a grid of n values of C, reporting the
                          total time and the iterations of each Solver [0]
       -P, --pgrid <n>    walk that grid at n values of the parameter of the
                          kernel as well [0]
       -O, --order <n>    in which order the grid is walked [0]: 0 = C
                          increasing, 1 = C decreasing, 2 = C back and forth,
                          3 = the kernel innermost
       -I, --incremental <n>
                          learn n more samples one at a time, timing each
                          Solver over the additions [0]

`./SVM_test --help` lists the standard options as well. If a `file` is given
it is read as a `SVMBlock` in netCDF format and no data set is generated.

Note that the training problem itself only exists for the linear kernel, the
only one whose feature map is the identity, and that `LagrangianDualSolver`
only applies to the consensus structure, whence the four `BlockSolverConfig`:
[BSPar.txt](BSPar.txt), a `:MILPSolver` and `SMOSolver`,
[BSPar-LSVM.txt](BSPar-LSVM.txt), the same two plus `LIBSVMSolver`,
[BSPar-LD.txt](BSPar-LD.txt), which adds `LagrangianDualSolver` handing each
chunk to a `:MILPSolver`, and [BSPar-LD-SMO.txt](BSPar-LD-SMO.txt), the same
one with each chunk handed to a `SMOSolver` instead.

There are therefore two batch files in [batches](batches), one per problem:
[batch-primal](batches/batch-primal), which sticks to the linear kernel, the
only one the training problem itself exists for, and sweeps the loss, the bias
and the granularity of the consensus structure, and
[batch-dual](batches/batch-dual), which sweeps the kernels, the loss, the bias
and the trade-off parameter. Each of them passes the `BlockConfig` naming its
own problem, so that what is being exercised is stated in the batch rather
than encoded in a flag. [batch-chunk](batches/batch-chunk) is the same sweep
as the former, restricted to the granularities where there is something to
relax, with each chunk solved by a `SMOSolver` of its own.

With `-R` the training problem is also *changed* under the `Solver`, and
re-solved after each change: the trade-off parameter, the loss, the bias, the
half-width of the insensitivity tube, one target and finally a whole new data
set. The `Solver` are not detached in between, hence each of them has to make
the right sense of the `Modification` the `SVMBlock` issues, `SMOSolver`
re-optimizing out of the physical representation and the `:MILPSolver` reading
the abstract one, which the `SVMBlock` keeps up to date; that the two keep
agreeing after every change is the test. This is what
[batch-reopt](batches/batch-reopt) sweeps over both problems and all the
kernels.

With `-G n` the same data set is instead trained over and over with a grid of
`n` values of `C`, i.e. the model selection one actually runs: every `:Solver`
sees the same sequence of `Modification` and makes of them what it can, and
what is reported, per `:Solver`, is the total time over the whole grid against
the one of a `:Solver` of the same kind attached anew at each point, hence
knowing nothing. The difference is what re-optimization is worth, and it grows
with the resolution of the grid, since the finer it is the closer two
consecutive problems are: on 300 samples and 20 features, over four orders of
magnitude of `C`, `SMOSolver` saves 7% of the time on 7 values, 25% on 13 and
42% on 25. This is what [batch-grid](batches/batch-grid) sweeps.

[batch-libsvm](batches/batch-libsvm) is the three-way cross-check, i.e., the
same sweep of the dual restricted to what LIBSVM can be asked, with the three
`:Solver` attached at once: they have to agree on the optimal value, whichever
representation each of them reads. It is only registered with CTest when
`SVMBlock` has been built with LIBSVM.

Finally, [batch-liblinear](batches/batch-liblinear) is the same cross-check on
the other side of the problem: LIBLINEAR has the linear kernel only and the
regularised bias only, which is what LIBSVM has not, but it has both losses,
which LIBSVM has not either, so the two batches together cover the four
formulations. LIBLINEAR caps its dual solvers at 300 iterations whatever
tolerance they are given: a run that stops there is reported as
`kLowPrecision`, and the cross-check holds it to the only thing it then
claims, i.e., that the optimum is not above the value it returns. It is only
registered with CTest when `SVMBlock` has been built with LIBLINEAR.

## The Frank-Wolfe test

`SVM_FW_test` ([test_fw.cpp](test_fw.cpp)) uses the data sets of the suite
for two problems that `FrankWolfeSolver` decomposes, each built as a father
`AbstractBlock` whose `Objective` is a dense `QuadFunction` and whose
sub-`Block` are the feasible region, every `:Solver` of the father being
cross-checked against the others from the same starting point:

- with `-P 0`, the least-squares fit of an affine model in Euclidean balls,
  i.e. the constrained form of ridge regression, the weights and the bias
  being dealt to `-k` balls of radius R / sqrt( k ) each: a feasible region
  that is not a polytope, whose oracle is the `BallSolver` of the tester, a
  linear function being minimized over a ball in closed form; with `-r` not
  positive, R is that fraction of the norm of the unconstrained fit, so that
  the balls are active;

- with `-P 1`, the Wolfe dual of the SVM on a box, the bias being
  regularised so that there is no equality constraint, with the linear or
  the Gaussian kernel (`-K`) and the bound `-C`, whose oracle is
  `BoxSolver`.

The usage of the executable is

       ./SVM_FW_test -S <BlockSolverConfig> -L <BlockSolverConfig> [options]
                     <data set>
       -P, --problem <p>  0 the fit in balls, 1 the SVM dual [0]
       -k, --groups <K>   balls the coordinates are dealt to [1]
       -r, --radius <R>   radius of the whole; if <= 0, - R times the norm
                          of the unconstrained fit [-0.5]
       -g, --regress      the targets are real numbers
       -K, --kernel <k>   the dual only: 0 linear, 2 Gaussian [0]
       -C, --parC <x>     the dual only: the bound C [1]

where `-S` is the `BlockSolverConfig` of the father,
[FWBSCfg.txt](FWBSCfg.txt), which registers a `:MILPSolver` as the reference
and `FrankWolfeSolver` in five variants of its direction, and `-L` that of
each sub-`Block`, [BallBSCfg.txt](BallBSCfg.txt) or
[BoxBSCfg.txt](BoxBSCfg.txt). `FrankWolfeSolver` is read by its
[ `get_lb()` , `get_ub()` ], the reference by its value: a run that stops at
its cap of iterations is held to its bounds, which is what the certificate of
a direction other than the gradient is about. The battery is
[batch-fw](batches/batch-fw); on the box the variants do not reach the
accuracy they are asked for in the iterations they are given, and they are
held to their bounds only (`-E inf`).
