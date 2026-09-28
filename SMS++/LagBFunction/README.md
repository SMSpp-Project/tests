# test/LagBFunction

A tester which provides very comprehensive tests for `LagBFunction`,
`PolyhedralFunctionBlock`, `PolyhedralFunction`, any `CDASolver` able to
handle `C05Function` in the objective (such as `BundleSolver`, for which
some specific provisions are made), any `CDASolver` able to handle Linear
Programs (such as `MILPSolver` and its derived classes `CPXMILPSolver` ,
`SCIPMILPSolver` and `GRBMILPSolver`), as well as for quite a lot of the mechanics of the "core"
SMS++ library.

This executable, given three input parameters n, k and p, construct two
mathematically equivalent `AbstractBlock`, called NDOBlock and LPBlock,
deemed to be solved by two different `CDASolver`; one able to handle
`C05Function` in the objective, and another just able to solve Linear
Programs.

The NDOBlock is constructed as follows:

 - k random `PolyhedralFunction` are constructed, each inside a
   `PolyhedralFunctionBlock`.

 - p random uncapacitated *max-cost* transportation problems (with
   *negative* costs) on complete bipartite n-graphs (n origins, n
   destinations) are constructed "by hand", each inside an `AbstractBlock`;
   the problems have balanced supply and demands, no directed cycles, and no
   capacities on a craftly chosen set of "crucial" arcs (see below), so as to
   ensure that they surely attain finite optimal solutions.

 - The above p `AbstractBlock` are inserted as inner `Block`, each inside a
   `LagBFunction`. If a command line parameter dictates that the
   `LagBFunction` will actually be computed (the `CDASolver` registered to
   NDOBlock does not have the "easy components" feature), an appropriate
   `Solver` (capable of solving LPs) is registered to all the inner `Block`
   of the `LagBFunction`.

 - The above p `LagBFunction` are put each inside the `FRealObjective` of a
   new `AbstractBlock`, otherwise empty.

 - The k `PolyhedralFunctionBlock` (configured to use "natural"
   representation) and the p `AbstractBlock` are inserted inside the single
   `AbstractBlock` NDOBlock, possibly with a linear function, to represent
   a problem of the form (for k = 1 and p = 2)

       min { l x + f(x) + max { ( B x + c_1 ) z_1 : E z_1 = b_1 , 0 <= z_1 }
                        + max { ( B x + c_2 ) z_2 : E z_2 = b_2 , 0 <= z_2 } }
 
   Assuming that arcs are ordered lexicographically (first all the ones
   outgoing node 0, ordered by tail node, then all the ones outgoing node
   1, ...), the matrix E has the form (ignoring bound constraints)

                 n^2
          | e^T  0  ...  0  |
       n  |  0  e^T ...  0  |
          |  :  :        :  |
          |  0  0   ... e^T |
          +-----------------+
       n  |  I  I   ...  I  |

   corresponding to constraints (with I = J =  { 0 ... n - 1 })

       \sum_{ j \in J } z[ i ][ j ] == s[ i ]   i \in I

       \sum_{ i \in I } z[ i ][ j ] == d[ j ]   j \in J

   with s[] and d[] being the vectors of supplies and demand. The matrix
   B has the form

             n^2
       n  |  I  I   ...  I  |

   corresponding to the fact that the cost of arc ( i , j ) is

         c[ i ][ j ] + x[ j ]

   i.e., x[ j ] is added to the cost of all arcs ingoing destination j.

 - Some arc ( i , j ) will have a finite upper bound u[ i ][ j ] >= 0,
   with the value 0 being possible (basically, fixing the variable).
   However, this immediately creates the risk that the transportation
   problem is unfeasible, which we don't want to handle. To avoid that,
   the following cunning plan has been devised:

   = none of the arcs ( i , i ) will ever have finite upper bound;

   = the i-th supply and demand will be equal: s[ i ] = d[ i ].

   This guarantees that satisfying the i-th supply/demand pair via the
   direct arc ( i , j ) is always possible, albeit is may easily not be
   the best choice due to the costs being random. Note that one may have
   also put any finite upper bound >= s[ i ] = d[ i ] for this to work,
   but if bounds and capacities are randomly changed then one should be
   careful to guarantee that this always holds; by not having the bound
   at all we guarantee that this can never be a problem, since we will
   never create a finite upper bound when an infinite one was (nor
   vice-versa, for that matter).

 - An appropriate `CDASolver` is attached to NDOBlock; this can in general
   be any `Solver` capable of solving it, but some specific provisions are
   made for `BundleSolver`, in particular when very verbose log is active.

 Then, an LP equivalent of NDOBlock is constructed into the different
 `AbstractBloc` LPBlock with the following steps:

 - The linear objective and the k `PolyhedralFunctionBlock` are just
   copied over, the latter using the R3Block mechanics

 - For the p `LagBFunction`, an LP equivalent is constructed by the
   following derivation:

       min { l x + f(x) + max { ( B x + c_1 ) z_1 : E z_1 = b_1  , 0 <= z_1 }
                        + max { ( B x + c_2 ) z_2 : E z_2 = b_2  , 0 <= z_2 }
            } =
       
       min { l x + f(x) + min { y_1 b_1 : y_1 E >= B x + c_1 }
                        + min { y_2 b_2 : y_2 E >= B x + c_2 } } =
       
       min { l x + f(x) + y_1 b_1 + y_2 b_2 :
             y_1 E >= B x + c_1 , y_2 E >= B x + c_2 }

   Since the transpose of E has the form

                  n        n
             | e 0 ... 0 | I |
       n^2   | 0 e ... 0 | I |
             | : :     : | : |
             | 0 0 ... e | I |

   this corresponds to variables yo[ i ] and yd[ j ] for each origin and
   destination, with costs s[ i ] and d[ j ] respectively, as well as
   constraints

       ys[ i ] + yd[ j ] - x[ j ] >= c[ i ][ j ]

   for all i \in I and j \in J. This works if the variable z[ i ][ j ] has
   *no* finite bound; if, instead, the constraint

       z[ i ][ j ] <= u[ i ][ j ]

   is present (with u[ i ][ j ] = 0 possible), then it has a dual variable
   w[ i ][ j ]; this means that a term u[ i ][ j ] * w[ i ][ j ] is added
   to the objective function, and that the constraint becomes

       ys[ i ] + yd[ j ] + w[ i ][ j ] - x[ j ] >= c[ i ][ j ]

       w[ i ][ j ] >= 0

   All this, however, is only correct for the convex case; in the concave
   one, the problem is rather

       max { l x + f(x) + min { ( B x + c_1 ) z_1 : E z_1 = b_1  , 0 <= z_1 }
                        + min { ( B x + c_2 ) z_2 : E z_2 = b_2  , 0 <= z_2 } }
   yielding

       max { l x + f(x) + y_1 b_1 + y_2 b_2 :
             y_1 E <= B x + c_1 , y_2 E <= B x + c_2 }

   and therefore the constraints are

       ys[ i ] + yd[ j ] - w[ i ][ j ] - x[ j ] <= c[ i ][ j ]

       w[ i ][ j ] >= 0

   assuming u[ i ][ j ] is finite, else without the "- w[ i ][ j ]" term
   and the w[ i ][ j ] variable; note having put a "-" to keep w[ i ][ j ]
   non-negative, since the natural sign of the dual variable of a <=
   constraint in a minimization LP is <= 0. However, this means that the
   corresponding term in the objective, that would ordinarily be

       + u[ i ][ j ] * w[ i ][ j ]

   must then become

       - u[ i ][ j ] * w[ i ][ j ]

   because w[ i ][ j ] has changed sign (w[ i ][ j ] ==> - w[ i ][ j ]).

 - The variables ys[ i ] and yd[ j ], the objective function and the
   constraints (linking them with x[]) are constructed manually into an
   `AbstractBlock` for each p.

 - An appropriate `CDASolver` is attached to LPBlock.

After all this is done, the NDOBlock and LPBlock are solved with the
registered `Solver` and the results (termination status and objective
value, if applicable) are compared.

The PolyhedralFunction and/or the costs and demands (not supplies) of the
uncapacitated transportation problems are then repeatedly randomly
modified "in the same way", and re-solved several times; each time the
results of the two `Solver` are compared.

The usage of the executable is the following:

       ./LagBFunction_test seed [wchg nvar #nf #nt dens #rounds #chng %chng]
       wchg: what to change, coded bit-wise [511]
             0 = add rows, 1 = delete rows 
             2 = modify rows, 3 = modify constants
             4 = change global lower/upper bound
             5 = modify costs, 6 = modify demands
             7 = modify flow bounds
             8 = change linear objective
       nvar: number of variables [10]
       |#nf|: number of PolyFunction (< 0: linear function) [1]
       |#nt|: number of transportation (< 0: easy comp.) [1]
       dens: rows / variables [3]
       #rounds: how many iterations [40]
       #chng: number of changes [10]
       %chng: probability of changing [0.5]

A batch file is provided that runs a largish (but typically terminating
within half an hour) set of tests with different sizes and seeds of the
random generator; all these passing is a good sign that no regressions
have been done for the tested modules.

A makefile is also provided that builds the executable including the
BundleSolver module and all its dependencies, in particular MILPSolver
(and, obviously, the core SMS++ library).


## Authors

- **Antonio Frangioni**  
  Dipartimento di Informatica  
  Università di Pisa

## License

This code is provided free of charge under the [GNU Lesser General Public
License version 3.0](https://opensource.org/licenses/lgpl-3.0.html) -
see the [LICENSE](LICENSE) file for details.
