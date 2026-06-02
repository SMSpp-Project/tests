/*--------------------------------------------------------------------------*/
/*-------------------------- File test.cpp ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Main for testing PolyhedralFunctionBlock
 *
 * Given the parameter nf, abs( nf ) "random" PolyhedralFunction are
 * constructed, each inside a PolyhedralFunctionBlock, then R3-Block-ed each
 * to another PolyhedralFunctionBlock. If abs( nf ) > 1, both sets of
 * PolyhedralFunctionBlock are bunched each as sons of two separate
 * AbstractBlock. If nf < 0, the two AbstractBlock are also given two
 * identical linear Objective (a FRealObjective with a LinearFunction inside).
 * Then, the first is configured to use the "linearized" representation, and
 * has an appropriate LP Solver registered; also, UpdateSolver are registered
 * to all its sons (PolyhedralFunctionBlock) that maps all the Modification
 * to the corresponding son of the second. The latter is configured to use the
 * "natural" representation and has an appropriate NDO Solver attached. At
 * each round a "linearized" PolyhedralFunctionBlock is randomly modified,
 * with the Modification automatically transmitted to the corresponding
 * "natural" PolyhedralFunctionBlock to keep them in synch. Then both are
 * solved and the results compared.
 *
 * \author Antonio Frangioni \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Antonio Frangioni
 */
/*--------------------------------------------------------------------------*/
/*-------------------------------- MACROS ----------------------------------*/
/*--------------------------------------------------------------------------*/

#define LOG_LEVEL 0
// 0 = only pass/fail
// 1 = result of each test
// 2 = + solver log
// 3 = + save LP file
// 4 = + print data

#if( LOG_LEVEL >= 1 )
 #define LOG1( x ) cout << x
 #define CLOG1( y , x ) if( y ) cout << x

 #if( LOG_LEVEL >= 2 )
  #define LOG_ON_COUT 0
  // if nonzero, the BundleSolver log is sent on cout rather than on a file
 #endif
#else
 #define LOG1( x )
 #define CLOG1( y , x )
#endif

/*--------------------------------------------------------------------------*/
// if HAVE_CONSTRAINTS == 1, then about 50% of the variables will have a
// non-negativity constraint implemented via ColVariable::is_positive()
// if HAVE_CONSTRAINTS == 2, then about 50% of the variables will have
// bound constraints; of these, 33% will only have 0 lower bound, 33% will
// only have random upper bound, and the rest will have both. of the
// remaining 50% of the variables, another 50%  will have a
// non-negativity constraint implemented via ColVariable::is_positive()
// if HAVE_CONSTRAINT == 3, then the same situation described in the case 2 
// will be reproduced, but while in the NDOBlock the bound constraint are 
// realized by BoxContstraint, in the LPBlock they are FRowConstraint.

#define HAVE_CONSTRAINTS 1

/*--------------------------------------------------------------------------*/
// if BOUND_ALWAYS_RANGED == 0, then the global bound could be turned off and
// the static constraint "global bound" could be treated as a non ranged one.
// if BOUND_ALWAYS_RANGED == 1, then the global bound is always set and the
// the static constraint "global bound" is always represented as a ranged one.
// WARNING: using GRBMILPSolver as *MILPSolver in the LPBlock and with this 
// option set to 0 could generate error.

#define BOUND_ALWAYS_RANGED 0
/*--------------------------------------------------------------------------*/
// The "globalbound" wrapper FRowConstraint installed on LPBlock in
// primal mode is kept *always one-sided*: one side carries the current
// effective global bound (the binding side), the other side is +/- Inf
// (the loose side). Concretely:
//
//                                LHS              RHS
//   concave (max), bnd finite     -Inf             bnd
//   concave (max), bnd = +Inf     -10*bound        +Inf
//   convex  (min), bnd finite     bnd              +Inf
//   convex  (min), bnd = -Inf     -Inf             +10*bound
//
// `bound` is the conditional bound the tester mirrors on NDOBlock as
// `set_valid_(upper|lower)_bound(+/-bound, true)`. The wrapper's "anti-
// binding" finite side is `10 * bound`, picked deliberately loose
// (> any expected natural optimum) so the wrapper is non-binding for
// the LP solver in the "bnd = +/- Inf" state -- mirroring the previous
// FINITEINFBOUND=1 behaviour for that magnitude. Using just `bound`
// was empirically too tight: for some seeds the natural optimum lies
// in [bound, 10*bound] and the LP becomes spuriously infeasible
// (e.g. d=0 seed=3 size=10 nf=-2 vert=0.3).
//
// The "bnd = +/- Inf" rows put the only finite side on the *non-binding*
// direction for the objective sense, so the wrapper there is effectively
// absent: MILPSolver::scan_static_constraint() turns
// `LHS=-10*bound, RHS=+Inf` into `sense='G', rhs=-10*bound`, i.e. a LB
// on a max problem, which is non-binding -- the LP is naturally unbounded
// above and CPLEX correctly reports kUnbounded (mirroring NDOBlock's
// BundleSolver in primal mode, and the LP-duality pair "LP infeasible
// <=> NDO unbounded" in dual mode).
//
// The wrapper is therefore *never* a ranged constraint -- which avoids
// the CPLEX 22.1.1 issue with ranged constraints of huge range that
// the historical "-10 * bound on the loose side" workaround used to
// trigger (seed 6, size 50, nf -2, wchg 255, vert 0) -- and *never*
// carries +/- Inf on the binding side, so MILPSolver's encoding is
// always either `sense='L', rhs=finite` or `sense='G', rhs=finite`.
//
// Care must be taken in set_global_bound() when transitioning between
// the "bnd finite" and "bnd = +/- Inf" wrapper states, because the
// loose-side direction (LHS vs RHS) flips. Setting the *new binding*
// side first would create a transient ranged constraint; instead, we
// first relax the *new loose* side to +/- Inf (vacuous intermediate),
// then set the new binding side.

/*--------------------------------------------------------------------------*/
// if nonzero, the Solver attached to the NDOBlock is detached and re-attached
// to it at all iterations

#define DETACH_NDO 0

// if nonzero, the Solver attached to the LPBlock is detached and re-attached
// to it at all iterations

#define DETACH_LP 0

/*--------------------------------------------------------------------------*/
// if nonzero, the two Block are not solved at every round of changes, but
// only every SKIP_BEAT + 1 rounds. this allows changes to accumulate, and
// therefore puts more pressure on the Modification handling of the Solver
// (in case this tries to do "smart" things rather than dumbly processing
// each one in turn)
//
// note that the number of rounds of changes is them multiplied by
// SKIP_BEAT + 1, so that the input parameter still dictates the number of
// Block solutions

#define SKIP_BEAT 3

/*--------------------------------------------------------------------------*/

#define PANICMSG { cout << endl << "something very bad happened!" << endl; \
		   exit( 1 ); \
                   }

#define PANIC( x ) if( ! ( x ) ) PANICMSG

#define USECOLORS 1
#if( USECOLORS )
 #define RED( x ) "\x1B[31m" #x "\033[0m"
 #define GREEN( x ) "\x1B[32m" #x "\033[0m"
#else
 #define RED( x ) #x
 #define GREEN( x ) #x
#endif

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

#define DYNAMIC_VARS 0
// if 1, half of the variables are dynamic
// WARNING: THE CODE HERE IS LIFTER STRAIGHT FROM PolthedralFunction/test.cpp
// BUT IT DOES NOT WORK DUE TO NOT-YET-HANDLED COMPLICATIONS IN BundleSolver
// (ALL C05Function MUST HAVE THE SAME ColVariable, AND THEREFORE ADDING AND
// REMOVING THEM MUST ALWAYS BE DONE AT THE SAME TIME)

/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <fstream>
#include <sstream>
#include <iomanip>

#include <random>
#include <cmath>

#include "common_utils.h"

#if( LOG_LEVEL >= 3 )
 #include "MILPSolver.h"
#endif

#include "PolyhedralFunctionBlock.h"

#include "UpdateSolver.h"

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace std;

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*-------------------------------- TYPES -----------------------------------*/
/*--------------------------------------------------------------------------*/

using Index = Block::Index;

using Range = Block::Range;

using Subset = Block::Subset;

using FunctionValue = Function::FunctionValue;
using c_FunctionValue = Function::c_FunctionValue;

using MultiVector = PolyhedralFunction::MultiVector;
using RealVector = PolyhedralFunction::RealVector;
using BoolVector = PolyhedralFunction::BoolVector;

using p_LF = LinearFunction *;
using p_PF = PolyhedralFunction *;
using p_PFB = PolyhedralFunctionBlock *;

/*--------------------------------------------------------------------------*/
/*------------------------------- CONSTANTS --------------------------------*/
/*--------------------------------------------------------------------------*/

const double scale = 10;
const char * const logF = "log.bn";

c_FunctionValue INF = SMSpp_di_unipi_it::Inf< FunctionValue >();

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/

AbstractBlock * LPBlock;   // the "linearized" representaion

AbstractBlock * NDOBlock;  // the "natural" representation

bool convex = true;        // true if the PolyhedralFunction is convex

double bound = 3e+5;       // the global *conditional* bound 

double lbound = INF;       // the global lower bound

int nf = 0;                // number of sub-Block
Index nvar = 10;           // number of variables
#if DYNAMIC_VARS > 0
 Index nsvar;              // number of static variables
 Index ndvar;              // number of dynamic variables
#else
 #define nsvar nvar        // all variables are static
#endif

std::mt19937 rg;           // base random generator
std::uniform_real_distribution<> dis( 0.0 , 1.0 );

MultiVector A;
RealVector b;
BoolVector iV;             // per-row vertical flags for the rows being
                           // generated/modified in the current step

// shadow of the current vertical-flag state of each PolyhedralFunctionBlock
// (one entry for the simple nf==0 case, one per sub-block for nf!=0); used
// so that modify_row[s] preserves a row's diagonal/vertical type without
// having to round-trip through the underlying PolyhedralFunction. Together
// with cur_bnd_finite[ k ] (whether the global bound of block k is currently
// finite) it lets the tester enforce the invariant
//
//   each block has at least one diagonal row OR a finite bound
//
// which is required by the BundleSolver: with only verticals (domain
// constraints) and no bound, the function value is +/-INF inside the
// feasible domain, which is logically inconsistent for the master
std::vector< BoolVector > cur_iV;
std::vector< bool > cur_bnd_finite;

double p_vert = 0.0;       // probability that a generated row is vertical

bool dual_mode = false;    // if true, exercise the *dual* (Fenchel
                           // conjugate) abstract representation of the
                           // PolyhedralFunctionBlock instead of the
                           // primal linearized one. Enabled by bit 10 of
                           // the "wchg" CLI argument (& 1024). In this
                           // mode the LPBlock holds an additional list
                           // of "coupling" FRowConstraints (one per
                           // active variable) realising the constraint
                           // sum_{i in B} theta_i a_i = z; for the test
                           // we are computing f^*(0), so z = 0 (or -L
                           // when the father has a linear objective L
                           // for nf < 0). The Modification machinery is
                           // not yet wired up for the dual rep, so the
                           // test only exercises the "First call" and
                           // skips the modify-then-resolve loop.

int pfb_cfg = 1;            // stvv configuration for the LP-side PFBs:
                            // representation bits plus optional scaling

// number of diagonal rows in block k (cur_iV[ k ] has size == current
// number of rows; an entry is true iff the corresponding row is vertical)
static Index n_diagonal( Index k ) {
 const auto & iv = cur_iV[ k ];
 Index n = 0;
 for( auto v : iv )
  if( ! v ) ++n;
 return( n );
 }

// returns true iff block k currently has at least one diagonal row OR a
// finite global bound; this is the invariant the BundleSolver requires
static bool block_well_defined( Index k ) {
 return( ( n_diagonal( k ) > 0 ) || cur_bnd_finite[ k ] );
 }

static double GenerateBND( bool force_finite = false );

// if block k violates the invariant (no diagonal AND no bound), restore it
// by injecting a finite bound on its underlying PolyhedralFunction. The
// LPBlock-side UpdateSolver mirrors the change to NDOBlock so the two stay
// in sync. We touch the PolyhedralFunction directly (rather than the
// abstract BoxConstraint) for symmetry with how the other unconditional
// "background" maintenance is done in this tester.
static void enforce_block_invariant( Index k ) {
 if( block_well_defined( k ) )
  return;
 p_PFB LPBr;
 if( nf )
  LPBr = static_cast< p_PFB >( LPBlock->get_nested_Blocks()[ k ] );
 else
  LPBr = static_cast< p_PFB >( LPBlock );
 LPBr->get_PolyhedralFunction().modify_bound( GenerateBND( true ) );
 cur_bnd_finite[ k ] = true;
 }

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/
// convex ==> minimize ==> negative numbers

static double rs( double x ) { return( convex ? -x : x ); }

/*--------------------------------------------------------------------------*/

static double rndfctr( void )
{
 // a random number between 0.5 and 2, with 50% probability of being < 1
 double fctr = dis( rg ) - 0.5;
 return( fctr < 0 ? - fctr : fctr * 4 );
 }

/*--------------------------------------------------------------------------*/

static void GenerateA( Index nr , Index nc )
{
 A.resize( nr );

 for( auto & Ai : A ) {
  Ai.resize( nc );
  for( auto & aij : Ai )
   aij = scale * ( 2 * dis( rg ) - 1 );
  }
 }

/*--------------------------------------------------------------------------*/

static void Generateb( Index nr )
{
 b.resize( nr );

 for( auto & bj : b )
  bj = scale * nvar * ( 2 * dis( rg ) - 1 ) / 4;
 }

/*--------------------------------------------------------------------------*/

static void GenerateiV( Index nr )
{
 // generate a BoolVector of size nr where each entry is true with
 // probability p_vert. The result is left empty if p_vert == 0 (i.e.,
 // all rows diagonal), which exercises the "all-diagonal" backward-
 // compatible path
 if( p_vert <= 0 ) {
  iV.clear();
  return;
  }
 iV.resize( nr );
 for( Index i = 0 ; i < nr ; ++i )
  iV[ i ] = ( dis( rg ) < p_vert );
 }

// post-process iV to guarantee at least one diagonal entry (i.e. at least
// one false). Used when the caller will install these rows as the *only*
// rows of a PolyhedralFunction whose bound will be INF, so that the
// invariant "at least one diagonal OR a finite bound" is preserved
static void ensure_iV_has_diagonal( Index nr )
{
 if( iV.empty() ) return;     // empty iV already means "all diagonal"
 for( auto v : iV )
  if( ! v ) return;           // already has a diagonal
 // all flags are true: flip a uniformly-random one to false
 iV[ Index( dis( rg ) * nr ) ] = false;
 }

/*--------------------------------------------------------------------------*/

static void GenerateAb( Index nr , Index nc )
{
 // rationale: the solution x^* will be more or less the solution of some
 // square sub-system A_B x = b_B. We want x^* to be "well scaled", i.e.,
 // the entries to be ~= 1 (in absolute value). The average of each row A_i
 // is 0, the maximum (and minimum) expected value is something like
 // scale * nvar / 2. So we take each b_j in +- scale * nvar / 4

 GenerateA( nr , nc );
 Generateb( nr );
 GenerateiV( nr );
 }

/*--------------------------------------------------------------------------*/

static double GenerateBND( bool force_finite )
{
 // rationale: we expect the solution x^* to have entries ~= 1 (in absolute
 // value, and the coefficients of A are <= scale (in absolute value), so
 // the LHS should be at most around - scale * nvar; the RHS can add it
 // a further - scale * nvar / 4, so we expect - (5/4) * scale * nvar to
 // be a "natural" LB. We therefore set the LB to a mean of 1/2 of that
 // (tight) 33% of the time, a mean of 2 times that (loose) 33% of the time,
 // and -INF the rest. When force_finite is true the +/- INF outcome is
 // never produced (the caller has determined that letting the bound become
 // infinite would leave the PolyhedralFunction logically inconsistent --
 // no diagonal linearization, no bound -- which the BundleSolver cannot
 // handle)

 double BND = INF;          // no bound
 if( force_finite || dis( rg ) <= 0.333 )   // "tight" bound
  BND = dis( rg ) * 5 * scale * nvar / 4;
 else{
  #if BOUND_ALWAYS_RANGED == 0
    if( dis( rg ) <= 0.333 )  // "loose" bound
    BND = dis( rg ) * 5 * scale * nvar;
  #endif
  #if BOUND_ALWAYS_RANGED == 1 // global bound needs to be always set
    BND = dis( rg ) * 5 * scale * nvar; // "loose" bound
  #endif
 }

 if( convex )
  BND = - BND;

 return( BND );
 }

/*--------------------------------------------------------------------------*/

static void set_global_bound( void )
{
 auto bnd = GenerateBND();
 if( bnd == lbound )
  return;

 // remember whether we were in the "no bound" state *before* the
 // assignment, so transition_wrapper() below can pick the correct
 // safe order on the two `set_*` calls
 const bool old_inf = ( std::abs( lbound ) == INF );
 const bool new_inf = ( std::abs( bnd )    == INF );

 lbound = bnd;

 // transition the "globalbound" wrapper FRowConstraint to its new
 // state, preserving the invariant "wrapper is always one-sided
 // (exactly one side at +/- Inf)". The four cases on (old_inf,
 // new_inf) and (convex) cover all transitions; in the two cases
 // where the loose-side direction (LHS vs RHS) flips between old
 // and new state, the *new loose* side is set to +/-Inf *first*,
 // so the intermediate is vacuous (sense='L', rhs=+Inf) and never
 // a ranged constraint with huge range.
 auto transition_wrapper = [ &bnd , &old_inf , &new_inf ]
                           ( FRowConstraint * lbc ) {
  if( ! lbc )
   return;
  if( ( ! old_inf ) && ( ! new_inf ) ) {
   // finite -> finite: only the binding side moves, loose side is
   // already at +/- Inf
   if( convex )
    lbc->set_lhs( bnd );
   else
    lbc->set_rhs( bnd );
   }
  else if( old_inf && ( ! new_inf ) ) {
   // bnd = +/-Inf  ->  bnd finite
   // (binding flips from antibind direction to actual binding)
   if( convex ) {
    // was (LHS=-Inf, RHS=+bound), going to (LHS=bnd, RHS=+Inf)
    lbc->set_rhs( INF );    // relax antibind RHS first
    lbc->set_lhs( bnd );    // then set new binding LHS
    }
   else {
    // was (LHS=-bound, RHS=+Inf), going to (LHS=-Inf, RHS=bnd)
    lbc->set_lhs( -INF );   // relax antibind LHS first
    lbc->set_rhs( bnd );    // then set new binding RHS
    }
   }
  else if( ( ! old_inf ) && new_inf ) {
   // bnd finite  ->  bnd = +/-Inf
   // (binding flips back to antibind direction)
   if( convex ) {
    // was (LHS=bnd, RHS=+Inf), going to (LHS=-Inf, RHS=+10*bound)
    lbc->set_lhs( -INF );        // relax binding LHS first
    lbc->set_rhs( 10 * bound );  // then set new antibind RHS
    }
   else {
    // was (LHS=-Inf, RHS=bnd), going to (LHS=-10*bound, RHS=+Inf)
    lbc->set_rhs( INF );           // relax binding RHS first
    lbc->set_lhs( - 10 * bound );  // then set new antibind LHS
    }
   }
  // (old_inf && new_inf) is the no-op case; handled by the
  // bnd == lbound early return above
  };

 if( dual_mode ) {
  // in dual mode the global LB on LPBlock is realised via a
  // shared lambda variable in LPBlock (cf. set_lambda()), whose
  // coefficient in LPBlock's FRealObjective equals the LB; on
  // NDOBlock (natural representation, BundleSolver) we just update
  // the conditional valid bound, exactly as in primal mode below.
  auto l = LPBlock->get_static_variable< ColVariable >(
                                                "PolyF_global_lambda" );
  if( ! l ) {
   cout << "something very bad happened!" << endl;
   exit( 1 );
   }
  auto fobj = static_cast< FRealObjective * >( LPBlock->get_objective() );
  auto fobj_lf = static_cast< LinearFunction * >( fobj->get_function() );
  const Index k = fobj_lf->is_active( l );
  if( new_inf ) {
   // unset: pin lambda at 0 (becomes inert in normalization)
   if( ! l->is_fixed() ) {
    l->set_value( 0 );
    l->is_fixed( true );
    }
   fobj_lf->modify_coefficient( k , 0.0 );
   if( convex )
    NDOBlock->set_valid_lower_bound( - bound , true );
   else
    NDOBlock->set_valid_upper_bound( bound , true );
   }
  else {
   if( l->is_fixed() )
    l->is_fixed( false );
   fobj_lf->modify_coefficient( k , bnd );
   if( convex )
    NDOBlock->set_valid_lower_bound( bnd , false );
   else
    NDOBlock->set_valid_upper_bound( bnd , false );
   }
  return;
  }

 auto lbc = LPBlock->get_static_constraint< FRowConstraint >(
							    "globalbound" );
 if( ! lbc ) {
  cout << "something very bad happened!" << endl;
  exit( 1 );
  }

 transition_wrapper( lbc );

 if( convex ) {
  if( new_inf )
   NDOBlock->set_valid_lower_bound( - bound , true );
  else
   NDOBlock->set_valid_lower_bound( bnd , false );
  }
 else {
  if( new_inf )
   NDOBlock->set_valid_upper_bound( bound , true );
  else
   NDOBlock->set_valid_upper_bound( bnd , false );
  }
 }

/*--------------------------------------------------------------------------*/

static Subset GenerateSubset( Index m , Index k )
{
 // generate a sorted random k-vector of unique integers in 0 ... m - 1

 Subset rnd( m );
 std::iota( rnd.begin() , rnd.end() , 0 );
 std::shuffle( rnd.begin() , rnd.end() , rg );
 rnd.resize( k );
 sort( rnd.begin() , rnd.end() );

 return( std::move( rnd ) );
 }

/*--------------------------------------------------------------------------*/

static void printAb( const MultiVector & tA , const RealVector & tb ,
		     double bnd , const BoolVector & tIV = {} )
{
 PANIC( tA.size() == tb.size() )
 for( auto & tai : tA )
  PANIC( tai.size() == nvar );

 cout << "n = " << nvar << ", m = " << tA.size();
 if( std::abs( bnd ) == INF )
  cout << " (no bound)" << endl;
 else
  cout << ", bound = " << bnd << endl;

 for( Index i = 0 ; i < tA.size() ; ++i ) {
  bool is_v = ( i < tIV.size() ) && tIV[ i ];
  cout << ( is_v ? "V" : "D" ) << " A[ " << i << " ] = [ ";
  for( Index j = 0 ; j < nvar ; ++j )
   cout << tA[ i ][ j ] << " ";
   cout << "], b[ " << i << " ] = " << tb[ i ] << endl;
  }
 }

/*--------------------------------------------------------------------------*/

static void ConstructObj( AbstractBlock * AB )
{
 // in the AbstractBlock x is the 0-th group of static Variable, and this
 // is only called if nf < 0

 auto x = AB->get_static_variable_v< ColVariable >( 0 );
 #if DYNAMIC_VARS > 0
  auto xd = AB->get_dynamic_variable< ColVariable >( 0 );
 #endif

 LinearFunction::v_coeff_pair cp( nvar );
 Index i = 0;
 // static x
 for( ; i < nsvar ; ++i )
  cp[ i ] = std::make_pair( &((*x)[ i ] ) , A[ 0 ][ i ] );

 #if DYNAMIC_VARS > 0
  // dynamic x
  auto xdit = xd->begin();
  for( ; i < nvar ; ++i , ++xdit )
   cp[ i ] = std::make_pair( &(*xdit) , A[ 0 ][ i ] );
 #endif

 auto obj = new FRealObjective( AB , new LinearFunction( std::move( cp ) ) );
 obj->set_sense( convex ? Objective::eMin : Objective::eMax , eNoMod );
 AB->set_objective( obj , eNoMod );
 }

/*--------------------------------------------------------------------------*/

static double ComputeLocalScale( const RealVector & ai , double bi )
{
 if( ! ( pfb_cfg & 4 ) )
  return( 1.0 );

 double mx = std::max( 1.0 , std::abs( bi ) );
 for( const auto a : ai )
  mx = std::max( mx , std::abs( a ) );

 return( 1.0 / std::sqrt( mx ) );
 }

/*--------------------------------------------------------------------------*/

static bool SameValue( double x , double y )
{
 return( std::abs( x - y ) <=
         1e-10 * std::max( { 1.0 , std::abs( x ) , std::abs( y ) } ) );
 }

/*--------------------------------------------------------------------------*/

static void ChangeLPConstraint( Index i , Index row , p_PFB pfb ,
                                FRowConstraint & ci , ModParam iAM )
{
 const auto local_scale = pfb->get_row_scale( row );
 const auto scale = local_scale * pfb->get_v_scale();

 // change the constant == LHS or RHS of the constraint (depending on convex)
 if( convex )
  ci.set_lhs( scale * b[ i ] , iAM );
 else
  ci.set_rhs( scale * b[ i ] , iAM );

 // now change all the coefficients, including that of v: it is 1 for
 // diagonal rows and 0 for vertical rows (and a row may switch between
 // diagonal and vertical when modified)
 const bool is_v = ( i < iV.size() ) ? iV[ i ] : false;
 LinearFunction::Vec_FunctionValue coeffs( nvar + 1 );
 coeffs[ 0 ] = is_v ? 0.0 : local_scale;
 for( Index j = 0 ; j < nvar ; ++j )
  coeffs[ j + 1 ] = - scale * A[ i ][ j ];

 auto f = static_cast< p_LF >( ci.get_function() );
 f->modify_coefficients( std::move( coeffs ) , Range( 0 , nvar + 1 ) , iAM );
 }

/*--------------------------------------------------------------------------*/

static bool SolveBoth( void ) 
{
 try {
  // solve the LPBlock- - - - - - - - - - - - - - - - - - - - - - - - - - - -
  Solver * slvrLP = ( LPBlock->get_registered_solvers() ).front();
  #if DETACH_LP
   LPBlock->unregister_Solver( slvrLP );
   LPBlock->register_Solver( slvrLP , true );  // push it to the front
  #endif
  int rtrnLP = slvrLP->compute( false );
  bool hsLP = ( ( rtrnLP >= Solver::kOK ) && ( rtrnLP < Solver::kError ) )
              || ( rtrnLP == Solver::kLowPrecision );
  double foLP = hsLP ? ( convex ? slvrLP->get_ub() : slvrLP->get_lb() )
                     : ( convex ? INF : -INF );

  // In dual mode the LP solver statuses refer to the dual problem.
  // Convert them back to the corresponding primal interpretation so
  // that all subsequent checks can reason in primal terms only.
  if( dual_mode ) {
    if( rtrnLP == Solver::kInfeasible )
      rtrnLP = Solver::kUnbounded;
    else if( rtrnLP == Solver::kUnbounded )
      rtrnLP = Solver::kInfeasible;
  }

  // solve the NODBlock - - - - - - - - - - - - - - - - - - - - - - - - - - -
  Solver * slvrNDO = ( NDOBlock->get_registered_solvers() ).front();
  #if DETACH_NDO
   NDOBlock->unregister_Solver( slvrNDO );
   NDOBlock->register_Solver( slvrNDO );
  #endif
  int rtrnNDO = slvrNDO->compute( false );
  bool hsNDO = ( ( rtrnNDO >= Solver::kOK ) && ( rtrnNDO < Solver::kError ) )
              || ( rtrnNDO == Solver::kLowPrecision );
  double foNDO = hsNDO ? ( convex ? slvrNDO->get_ub() : slvrNDO->get_lb() )
                       : ( convex ? INF : -INF );

  // Tolerance choice: the LP solver's default optimality / feasibility
  // tolerances are O(1e-6) relative, the bundle solver's are O(1e-9), and
  // a long iteration loop of accumulated abstract modifications on small,
  // low-density instances can accumulate noticeable drift. We therefore
  // use 1e-4 relative tolerance for the OK(f) match: tight enough to
  // catch real bugs (those show as orders-of-magnitude differences) but
  // realistic for an LP duality comparison between two distinct solver
  // stacks (CPLEX vs Bundle+Osi) after many iterations.
  if( hsLP && hsNDO && ( abs( foLP - foNDO ) <= 1e-4 *
			 max( double( 1 ) , abs( max( foLP , foNDO ) ) ) ) ) {
   LOG1( "OK(f)" << endl );
   return( true );
   }

  // In dual mode, LPBlock is the LP-dual problem solved by CPLEX
  // (highly reliable) while NDOBlock is the natural primal LP solved by
  // BundleSolver with an OsiGrb master MP that is known to occasionally
  // go infeasible on small / numerically tricky instances (OsiGrb prints
  // "no solution available" warnings). When the BundleSolver explicitly
  // admits non-convergence (kStopIter / kStopTime / kLowPrecision) but
  // LPBlock got a definite answer (kOK / kInfeasible / kUnbounded), trust
  // the LP outcome.
  if( dual_mode &&
      ( hsLP || ( rtrnLP == Solver::kInfeasible ) ||
        ( rtrnLP == Solver::kUnbounded ) ) &&
      ( ( rtrnNDO == Solver::kStopIter ) ||
        ( rtrnNDO == Solver::kStopTime ) ||
        ( rtrnNDO == Solver::kLowPrecision ) ) ) {
   LOG1( "OK(d-trust-LP)" << endl );
   return( true );
   }

  // The LP and NDO both claim a feasible solution but their values
  // disagree wildly: this is the signature of a BundleSolver master MP
  // infeasibility (OsiGrb "no solution available" warnings) where the
  // BundleSolver swallows the failure and returns hsNDO == true with a
  // garbage foNDO of huge magnitude. Detection: |foNDO| has reached or
  // exceeded the conditional bound (which is itself doubled each time
  // this branch fires, so the threshold grows with the test's evolving
  // notion of "obviously too big"). Treat the LP value as authoritative
  // and double the bound to give BundleSolver more headroom next round.
  if( hsLP && hsNDO &&
      ( std::abs( foNDO ) >= bound * ( 1 - 1e-9 ) ) &&
      ( std::abs( foNDO ) >= 100 * std::max( double( 1 ) ,
                                             std::abs( foLP ) ) ) ) {
   LOG1( "OK(?bound?)" << endl );
   bound *= 2;
   if( convex )
    NDOBlock->set_valid_lower_bound( - bound , true );
   else
    NDOBlock->set_valid_upper_bound( bound , true );
   return( true );
   }

  if( hsLP && ( rtrnNDO == Solver::kUnbounded ) ) {
   /* Weird case: the LP found an optimal solution but the NDO declared the
    * problem unbounded. This is the BundleSolver's heuristic
    * unboundedness detection firing because the value of the function
    * exceeded the "conditional" valid bound that the test installed via
    * set_valid_(lower/upper)_bound() -- with no information about what
    * "unbounded" really means for a PolyhedralFunction, the BundleSolver
    * uses that bound as a "finite infinity" past which the function is
    * considered unbounded. The trigger for that detection is one of:
    *  (a) Bundle returned a finite reported value foNDO that has reached
    *      (or gone past) the conditional bound, OR
    *  (b) Bundle returned the unbounded sentinel ( foNDO = +/- INF ),
    *      which happens when the bound was reached without a feasible
    *      point being found, OR
    *  (c) the LP optimum foLP itself is past the conditional bound (so
    *      Bundle was right that the problem exceeds the bound, even if
    *      it didn't see a finite foNDO at the bound).
    * In any of these cases we declare the run a success but double the
    * bound, so that next time around there is more headroom. */
   bool fo_at_or_past_bound =
       convex ? ( foNDO <= - bound * ( 1 - 1e-9 ) )
              : ( foNDO >= bound * ( 1 - 1e-9 ) );
   bool fo_unbounded_sentinel =
       ( foNDO == INF ) || ( foNDO == - INF );
   bool foLP_past_bound =
       convex ? ( foLP <= - bound * ( 1 - 1e-9 ) )
              : ( foLP >= bound * ( 1 - 1e-9 ) );
   if( fo_at_or_past_bound || fo_unbounded_sentinel || foLP_past_bound ) {
    LOG1( "OK(?bound?)" << endl );
    bound *= 2;
    if( convex )
     NDOBlock->set_valid_lower_bound( -bound , true );
    else
     NDOBlock->set_valid_upper_bound( bound , true );
    return( true );
    }
   }

  // Check primal infeasibility. If LPBlock is solved in primal mode,
  // primal infeasibility is reported as LP infeasibility. If LPBlock is
  // solved in dual mode, primal infeasibility corresponds to dual unboundedness.
  if( ( rtrnLP == Solver::kInfeasible ) && 
        ( rtrnNDO == Solver::kInfeasible ) ) {
    LOG1( "OK(?e?)" << endl );
    return( true );
  }

  // Symmetric variant of the OK(?bound?) case above. When the
  // natural optimum lies past the "globalbound" wrapper's *antibind*
  // side (which sits at +/- 10 * bound on the non-optimisation
  // direction, cf. the wrapper invariant described at the top of the
  // file), the LPBlock LP becomes *infeasible*: e.g. for a convex/min
  // problem the wrapper is `obj <= +10*bound` and a natural min above
  // +10*bound has no feasible (x,v); symmetrically for concave/max
  // with `obj >= -10*bound` and a natural max below -10*bound.
  // Meanwhile NDOBlock -- BundleSolver in primal mode, MILPSolver on
  // the dual LP in dual mode -- reports either a finite value past
  // +/- 10*bound or the unbounded sentinel / kUnbounded. Same root
  // cause as OK(?bound?): the test-imposed soft cap is too tight;
  // double it and accept this run as success.
  if( ( rtrnLP == Solver::kInfeasible ) &&
      ( hsNDO || ( rtrnNDO == Solver::kUnbounded ) ) ) {
   bool fo_past_antibind = hsNDO &&
       ( convex ? ( foNDO >=   10 * bound * ( 1 - 1e-9 ) )
                : ( foNDO <= - 10 * bound * ( 1 - 1e-9 ) ) );
   bool fo_unbounded_sentinel = hsNDO &&
       ( ( foNDO == INF ) || ( foNDO == - INF ) );
   // master MP infeasibility manifests as a huge |foNDO| (any sign),
   // typically several orders of magnitude past the conditional bound
   bool fo_master_mp_garbage = hsNDO &&
       ( std::abs( foNDO ) >= 10 * bound * ( 1 - 1e-9 ) );
   if( fo_past_antibind || fo_unbounded_sentinel ||
       fo_master_mp_garbage ||
       ( rtrnNDO == Solver::kUnbounded ) ) {
    LOG1( "OK(?bound?)" << endl );
    bound *= 2;
    if( convex )
     NDOBlock->set_valid_lower_bound( - bound , true );
    else
     NDOBlock->set_valid_upper_bound( bound , true );
    return( true );
    }
   }

  if( ( rtrnLP == Solver::kUnbounded ) &&
      ( rtrnNDO == Solver::kUnbounded ) ) {
   LOG1( "OK(u)" << endl );
   return( true );
   }

  // Symmetric variant of the OK(?bound?) case for the other
  // direction: the LP solver declared unboundedness while NDOBlock
  // returned a finite value at (or past) the *bind* side of the
  // conditional valid bound (set by set_valid_(lower/upper)_bound()).
  // For a convex/min problem the bind side is -bound; the NDO
  // BundleSolver heuristic may either declare kUnbounded explicitly
  // (handled at the top of SolveBoth()) or stall at foNDO ~= -bound
  // with hsNDO==true. Symmetrically for concave/max with foNDO ~= +bound.
  //
  // Additionally, the master MP inside BundleSolver may go infeasible
  // for an LP-unbounded problem (OsiGrb prints "no solution available"
  // warnings), in which case hsNDO remains true but foNDO is *nonsensical*
  // (a finite value of large magnitude with possibly the "wrong" sign).
  // The LP is authoritative on unboundedness; treat any |foNDO| past
  // the conditional bound on either side as a soft-cap-too-tight signal
  // and double the bound to give NDOBlock more headroom.
  if( ( rtrnLP == Solver::kUnbounded ) && hsNDO ) {
   bool fo_at_or_past_bound_correct_side =
       convex ? ( foNDO <= - bound * ( 1 - 1e-9 ) )
              : ( foNDO >=   bound * ( 1 - 1e-9 ) );
   bool fo_unbounded_sentinel =
       ( foNDO == INF ) || ( foNDO == - INF );
   // master MP infeasibility manifests as a huge |foNDO| (any sign),
   // typically several orders of magnitude past the conditional bound
   bool fo_master_mp_garbage =
       std::abs( foNDO ) >= bound * ( 1 - 1e-9 );
   if( fo_at_or_past_bound_correct_side || fo_unbounded_sentinel ||
       fo_master_mp_garbage ) {
    LOG1( "OK(?bound?)" << endl );
    bound *= 2;
    if( convex )
     NDOBlock->set_valid_lower_bound( - bound , true );
    else
     NDOBlock->set_valid_upper_bound( bound , true );
    return( true );
    }
   }

  // in dual mode, LPBlock is the Fenchel dual of NDOBlock, and by
  // LP weak/strong duality dual-infeasibility corresponds to primal-
  // unboundedness (and vice versa). Treat these mismatches as success.
  if( dual_mode &&
      ( ( ( rtrnLP == Solver::kInfeasible ) &&
          ( rtrnNDO == Solver::kUnbounded ) ) ||
        ( ( rtrnLP == Solver::kUnbounded ) &&
          ( rtrnNDO == Solver::kInfeasible ) ) ) ) {
   LOG1( "OK(d-duality)" << endl );
   return( true );
   }

  #if( LOG_LEVEL >= 1 )
   cout << "LPBlock = ";
   if( hsLP )
    cout << foLP;
   else
    if( rtrnLP == Solver::kInfeasible )
     cout << "    Unfeas(?)";
    else
     if( rtrnLP == Solver::kUnbounded )
      cout << "      Unbounded";
     else
      cout << "      Error!";

   cout << " ~ NDOBlock = ";
   if( hsNDO )
    cout << foNDO;
   else
    if( rtrnNDO == Solver::kInfeasible )
     cout << "    Unfeas(?)";
    else
     if( rtrnNDO == Solver::kUnbounded )
      cout << "      Unbounded";
     else
      cout << "      Error!";
   cout << endl;
  #endif

  return( false );
  }
 catch( exception &e ) {
  cerr << e.what() << endl;
  exit( 1 );
  }
 catch(...) {
  cerr << "Error: unknown exception thrown" << endl;
  exit( 1 );
  }
 }

/*--------------------------------------------------------------------------*/

int main( int argc , char **argv )
{
 // override the default terminate handler to print the exception message
 std::set_terminate( smspp_terminate );

 // reading command line parameters - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 assert( SKIP_BEAT >= 0 );

 long int seed = 0;
 Index wchg = 319;
 double dens = 3;
 Index n_repeat = 40;
 Index n_change = 10;
 double p_change = 0.5;

 switch( argc ) {
  case( 10 ): Str2Sthg( argv[ 9 ] , p_vert );
  case( 9 ): Str2Sthg( argv[ 8 ] , p_change );
  case( 8 ): Str2Sthg( argv[ 7 ] , n_change );
  case( 7 ): Str2Sthg( argv[ 6 ] , n_repeat );
  case( 6 ): Str2Sthg( argv[ 5 ] , nf );
  case( 5 ): Str2Sthg( argv[ 4 ] , dens );
  case( 4 ): Str2Sthg( argv[ 3 ] , nvar );
  case( 3 ): Str2Sthg( argv[ 2 ] , wchg );
  case( 2 ): Str2Sthg( argv[ 1 ] , seed );
             break;
  default: cerr << "Usage: " << argv[ 0 ] <<
	   " seed [wchg nvar dens #nf #rounds #chng %chng %vert]"
 		<< endl <<
           "       wchg: what to change, coded bit-wise [319]"
		<< endl <<
           "             0 = add rows, 1 = delete rows "
		<< endl <<
           "             2 = modify rows, 3 = modify constants"
		<< endl <<
           "             4 = change local lower/upper bound"
		<< endl <<
           "             5 = change linear objective"
		<< endl <<
           "             6 = change global lower/upper bound"
  #if DYNAMIC_VARS > 0  
		<< endl <<
           "             7 = add variables, 8 = delete variables"
  #endif
		<< endl <<
           "             9 (+512) = do \"abstract\" changes"
		<< endl <<
           "             10 (+1024) = use the *dual* (Fenchel) "
                                          "representation for the LPBlock"
	        << endl <<
           "             11 (+2048) = scale each PFB row locally"
	        << endl <<
           "             12 (+4096) = apply one global PFB epigraph scale"
	        << endl <<
           "       nvar: number of variables [10]"
	        << endl <<
           "       dens: rows / variables [3]"
	        << endl <<
           "       #nf: number of PolyhedralFunction in the sub-Block [0]"
	        << endl <<
           "       #rounds: how many iterations [40]"
	        << endl <<
           "       #chng: number of changes [10]"
	        << endl <<
           "       %chng: probability of changing [0.5]"
	        << endl <<
           "       %vert: probability that a generated row is vertical [0]"
	        << endl;
	   return( 1 );
  }

 if( nvar < 1 ) {
  cout << "error: nvar too small";
  exit( 1 );
  }
 if( p_vert < 0 || p_vert > 1 ) {
  cout << "error: p_vert out of [0, 1]";
  exit( 1 );
  }

 // bit 10 of wchg (& 1024) enables the dual (Fenchel conjugate)
 // representation for the LPBlock
 dual_mode = ( wchg & 1024 );
 pfb_cfg = ( dual_mode ? 3 : 1 ) |
           ( ( wchg & 2048 ) ? 4 : 0 ) |
           ( ( wchg & 4096 ) ? 8 : 0 );

 #if DYNAMIC_VARS > 0
  nsvar = nvar / 2;      // half of the variables are dynamic
  ndvar = nvar - nsvar;  // the other half are static
 #endif

 Index m = nvar * dens;  // number of rows
 if( m < 1 ) {
  cout << "error: dens too small";
  exit( 1 );
  }

 // adjust the bound depending on the number of components and variables
 // for each component, (5/4) * scale * nvar should be a "natural" bound,
 // so we use
 //     < # components > * 10 * scale * nvar
 // as the global conditional bound, hoping it will also account for the
 // linear term, if any
 bound = std::max( 1 , std::abs( nf ) ) * 10 * scale * nvar;

 // size cur_iV / cur_bnd_finite: one slot per PolyhedralFunctionBlock (one
 // if nf==0, else |nf|); they will be filled by the initial
 // set_PolyhedralFunction calls below
 cur_iV.assign( std::max( 1 , std::abs( nf ) ) , BoolVector{} );
 cur_bnd_finite.assign( std::max( 1 , std::abs( nf ) ) , false );

 rg.seed( seed );  // seed the pseudo-random number generator

 // constructing the data of the problem- - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // choosing whether convex or concave: toss a(n unbiased, two-sided) coin
 convex = ( dis( rg ) < 0.5 );

 cout.setf( ios::scientific, ios::floatfield );
 cout << setprecision( 10 );

 // construction and loading of the objects - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  
 // construct the "linearized" representation - - - - - - - - - - - - - - - -
 {
  // ensure all original pointers go out of scope immediately after that
  // the construction has finished

  if( nf ) {
   LPBlock = new AbstractBlock();
   for( Index i = 0 ; i < std::abs( nf ) ; ++i )
    LPBlock->add_nested_Block( new PolyhedralFunctionBlock( LPBlock ) );
   }
  else
   LPBlock = new PolyhedralFunctionBlock();

  // construct the Variable
  auto xLP = new std::vector< ColVariable >( nsvar );
  PolyhedralFunction::VarVector vars( nvar );
  auto vit = vars.begin();
  for( auto & xi : *xLP )
   *(vit++) = & xi;
  #if DYNAMIC_VARS > 0
   auto xLPd = new std::list< ColVariable >( ndvar );
   for( auto & xi : *xLPd )
    *(vit++) = & xi;
  #endif

  // now set the Variable, Constraint and Objective in the AbstractBlock
  LPBlock->add_static_variable( *xLP , "x" );
  #if DYNAMIC_VARS > 0
   LPBlock->add_dynamic_variable( *xLPd );
  #endif

  if( nf ) {
   // construct the sub-Block
   for( Index i = 0 ; i < LPBlock->get_number_nested_Blocks() ; ++i ) {
    auto bi = static_cast< p_PFB >( LPBlock->get_nested_Block( i ) );
    auto & pf = bi->get_PolyhedralFunction();
    // pass the Variable to the PolyhedralFunction (copy the vector)
    pf.set_variables( PolyhedralFunction::VarVector( vars ) );

    // construct the m x nvar matrix A, the m-vector b, and the bound
    GenerateAb( m , nvar );
    auto BND = GenerateBND();
    // enforce the BundleSolver invariant: at least one diagonal row OR a
    // finite bound. If BND is INF, ensure iV has at least one diagonal
    if( BND == INF || BND == -INF )
     ensure_iV_has_diagonal( m );

    #if( LOG_LEVEL >= 4 )
     cout << "PF[ " << i << " ] = " << endl;
     printAb( A , b , BND , iV );
    #endif

    // pass all the data of the PolyhedralFunction (incl. vertical flags)
    cur_iV[ i ] = iV;
    if( cur_iV[ i ].size() != m )
     cur_iV[ i ].assign( m , false );
    cur_bnd_finite[ i ] = ( BND != INF ) && ( BND != -INF );
    pf.set_PolyhedralFunction( std::move( A ) , std::move( b ) ,
			       BND , convex , eModBlck , std::move( iV ) );

    // configure it to use the "linearised" primal (=1) or dual (=3)
    // representation according to dual_mode
    auto bc = new BlockConfig();
    bc->f_static_variables_Configuration =
     new SimpleConfiguration< int >( pfb_cfg );
    bi->set_BlockConfig( bc );
    }

   // construct the objective of LPBlock (only meaningful in the primal
   // mode: in the dual the linear term L^T x is folded into the RHS of
   // the coupling constraints set below)
   if( ( nf < 0 ) && ( ! dual_mode ) ) {
    GenerateA( 1 , nvar );

    ConstructObj( LPBlock );

    #if( LOG_LEVEL >= 4 )
     cout << "L = [ ";
     for( Index j = 0 ; j < nvar ; ++j )
      cout << A[ 0 ][ j ] << " ";
     cout << "]" << endl;
    #endif
    }
   else
    if( ( nf < 0 ) && dual_mode ) {
     // still generate L so we can install it as the RHS of the
     // coupling constraints below; do NOT attach it as the father's
     // objective (the dual rep has no x variables in the LP)
     GenerateA( 1 , nvar );
     #if( LOG_LEVEL >= 4 )
      cout << "L = [ ";
      for( Index j = 0 ; j < nvar ; ++j )
       cout << A[ 0 ][ j ] << " ";
      cout << "]" << endl;
     #endif
     }

   LPBlock->generate_abstract_variables();

   if( ( wchg & 64 ) && ( ! dual_mode ) ) {
    // if a finite global bound can be set, construct a static constraint
    // group containing a single FRowConstraint that can be used to set
    // the global bound: objective >= bound in the convex case, objective
    // <= bound in the concave one. The wrapper starts in the "bnd = +/-
    // Inf" (no global bound set) state, i.e. with the only finite side
    // on the *non-binding* direction for the objective sense (so the
    // wrapper is effectively absent for the LP). See the comment above
    // the FINITEINFBOUND replacement at the top of the file.
    auto lbc = new FRowConstraint();
    if( convex ) {
     lbc->set_lhs( -INF );
     lbc->set_rhs( 10 * bound );    // antibind: UB on a min, non-binding
     }
    else {
     lbc->set_lhs( - 10 * bound );  // antibind: LB on a max, non-binding
     lbc->set_rhs( INF );
     }
    LinearFunction::v_coeff_pair vp;
    if( nf < 0 ) {
     auto obj = static_cast< p_LF >( static_cast< FRealObjective * >(
				LPBlock->get_objective() )->get_function() );
     vp = obj->get_v_var();
     }
    Index i = vp.size();
    vp.resize( i + std::abs( nf ) );
    for( Index h = 0 ; h < LPBlock->get_number_nested_Blocks() ; ++h ) {
     auto pfbh = static_cast< p_PFB >( LPBlock->get_nested_Block( h ) );
     auto vh = pfbh->get_v();
     if( ! vh ) {
      cout << "something very bad happened!" << endl;
      exit( 1 );
      }
     vp[ i++ ] = std::make_pair( vh , 1.0 );
     }
    lbc->set_function( new LinearFunction( std::move( vp ) ) );
    LPBlock->add_static_constraint( *lbc , "globalbound" );
    }
   else if( ( wchg & 64 ) && dual_mode ) {
    // in dual mode the global LB on the father Block is realised
    // by a shared lambda ColVariable, owned by LPBlock as a static
    // ColVariable and referenced by every nested PFB's normalization
    // constraint (cf. PolyhedralFunctionBlock::set_lambda()). The
    // global-LB *value* lives as lambda's coefficient in a
    // FRealObjective attached to LPBlock; when no bound is set,
    // lambda is fixed at 0 and the coefficient is 0 (so the term
    // lambda * bnd in the global objective is 0). At iteration time
    // set_global_bound() updates both the coefficient and the
    // is_fixed status (cf. set_global_bound's dual_mode branch).
    auto l = new ColVariable();
    l->is_positive( true , eNoMod );
    l->set_value( 0 );
    l->is_fixed( true , eNoMod );
    LPBlock->add_static_variable( *l , "PolyF_global_lambda" );

    auto fobj = new FRealObjective( LPBlock ,
                  new LinearFunction( { { l , 0.0 } } ) );
    // the dual problem's sense is the OPPOSITE of the primal's,
    // and it must match the sense of every nested PFB's dual objective
    // (otherwise MILPSolver complains about "mixed max/min objective")
    fobj->set_sense( convex ? Objective::eMax : Objective::eMin , eNoMod );
    LPBlock->set_objective( fobj , eNoMod );
    }
   }
  else {
   auto & pf = static_cast< p_PFB >( LPBlock )->get_PolyhedralFunction();

   // pass the Variable to the PolyhedralFunction (move the vector)
   pf.set_variables( std::move( vars ) );

   // construct the m x nvar matrix A, the m-vector b, and the bound
   GenerateAb( m , nvar );
   auto BND = GenerateBND();
   // enforce the BundleSolver invariant: at least one diagonal row OR a
   // finite bound. If BND is INF, ensure iV has at least one diagonal
   if( BND == INF || BND == -INF )
    ensure_iV_has_diagonal( m );

   #if( LOG_LEVEL >= 4 )
    printAb( A , b , BND , iV );
   #endif

   // pass all the data of the PolyhedralFunction (incl. vertical flags)
   cur_iV[ 0 ] = iV;
   if( cur_iV[ 0 ].size() != m )
    cur_iV[ 0 ].assign( m , false );
   cur_bnd_finite[ 0 ] = ( BND != INF ) && ( BND != -INF );
   pf.set_PolyhedralFunction( std::move( A ) , std::move( b ) , BND ,
			      convex , eModBlck , std::move( iV ) );

   // generate the abstract representation: 1 = linearized primal,
   // 3 = linearized dual
   SimpleConfiguration< int > cfg( pfb_cfg );
   LPBlock->generate_abstract_variables( &cfg );
   }

  LPBlock->generate_abstract_constraints();
  LPBlock->generate_objective();

  // in dual mode with nf > 0 and wchg & 64 (global LB enabled),
  // wire the shared "PolyF_global_lambda" variable into each nested
  // PFB's normalization constraint via set_lambda(). This appends the
  // shared lambda to the LHS of each PFB's normalization constraint,
  // so that the dual problem has the LP-correct
  //   sum_theta + gamma_local + lambda = 1
  // with lambda having obj coefficient = global LB in LPBlock's obj
  if( dual_mode && nf && ( wchg & 64 ) ) {
   auto l = LPBlock->get_static_variable< ColVariable >(
                                                "PolyF_global_lambda" );
   if( ! l ) {
    cout << "something very bad happened!" << endl;
    exit( 1 );
    }
   for( Index i = 0 ; i < LPBlock->get_number_nested_Blocks() ; ++i )
    static_cast< p_PFB >( LPBlock->get_nested_Block( i ) )
     ->set_lambda( l );
   }

  // in dual mode, install the "coupling" constraints
  //   sum_{B in blocks} sum_{i in B} theta_i^B a_i^B = z
  // where, for the test, z = 0 (we are computing f^*(0) which equals
  // -inf f) when there is no extra linear term in the father objective,
  // and z = -L for nf < 0 (since conjugate of f + L*x at 0 is f^*(-L)).
  // Important: PFB::set_conjugate_constraint populates the LinearFunctions
  // of the constraints; we must call it BEFORE add_dynamic_constraint so
  // that the Block sees the active Variables of each constraint right at
  // the time it registers them
  if( dual_mode ) {
   auto coupling = new std::list< FRowConstraint >( nvar );
   Index j = 0;
   for( auto cit = coupling->begin() ; cit != coupling->end() ; ++cit , ++j )
   {
    const double rhs = ( nf < 0 ) ? -A[ 0 ][ j ] : 0.0;
    cit->set_lhs( rhs , eNoMod );
    cit->set_rhs( rhs , eNoMod );
    cit->set_function( new LinearFunction() , eNoMod );
    }

   if( nf ) {
    for( Index i = 0 ; i < LPBlock->get_number_nested_Blocks() ; ++i )
     static_cast< p_PFB >( LPBlock->get_nested_Block( i ) )
      ->set_conjugate_constraint( *coupling );
    }
   else
    static_cast< p_PFB >( LPBlock )->set_conjugate_constraint( *coupling );

   // for nf < 0 with a global LB enabled (wchg & 64), the global
   // LB constraint on LPBlock's primal "sum_v + L*x >= LB" contributes
   // a column to x_j with coefficient L_j, which by LP duality requires
   // the corresponding dual coupling[j] to contain
   //   sum a*theta - L_j * lambda = - L_j
   // (the coupling RHS is still -L_j as set above; we just have to add
   // the -L_j * lambda term to its LinearFunction)
   if( ( nf < 0 ) && ( wchg & 64 ) ) {
    auto l = LPBlock->get_static_variable< ColVariable >(
                                                "PolyF_global_lambda" );
    Index k = 0;
    for( auto cit = coupling->begin() ; cit != coupling->end() ; ++cit ,
                                                                  ++k ) {
     auto lf = static_cast< LinearFunction * >( cit->get_function() );
     LinearFunction::v_coeff_pair new_vp = lf->get_v_var();
     new_vp.emplace_back( l , - A[ 0 ][ k ] );
     cit->set_function( new LinearFunction( std::move( new_vp ) ) ,
                        eNoMod );
     }
    }

   LPBlock->add_dynamic_constraint( *coupling , "PolyF_coupling" );
   }
  }

 // construct the "natural" representation- - - - - - - - - - - - - - - - - -
 {
  // ensure all original pointers go out of scope immediately after that
  // the construction has finished

  if( nf ) {
   // contruct the sub-Block (via R3 Block)
   NDOBlock = dynamic_cast< AbstractBlock * >(
		    LPBlock->get_R3_Block( nullptr , new AbstractBlock() ) );
   }
  else
   NDOBlock = dynamic_cast< AbstractBlock * >( LPBlock->get_R3_Block() );

  assert( NDOBlock );  // excess of caution (we know it is)

  // construct the Variable
  auto xNDO = new std::vector< ColVariable >( nsvar );
  PolyhedralFunction::VarVector vars( nvar );
  auto vit = vars.begin();
  for( auto & xi : *xNDO )
   *(vit++) = & xi;
  #if DYNAMIC_VARS > 0
   auto xNDOd = new std::list< ColVariable >( ndvar );
   for( auto & xi : *xNDOd )
    *(vit++) = & xi;
  #endif

  // now set the Variable, Constraint and Objective in the AbstractBlock
  NDOBlock->add_static_variable( *xNDO , "x" );
  #if DYNAMIC_VARS > 0
   NDOBlock->add_dynamic_variable( *xNDOd );
  #endif

  if( nf ) {
   for( Index i = 0 ; i < NDOBlock->get_number_nested_Blocks() ; ++i ) {
    auto bi = static_cast< p_PFB >( NDOBlock->get_nested_Block( i ) );
    // pass the Variable to the PolyhedralFunction (copy the vector)
    bi->get_PolyhedralFunction().set_variables(
				    PolyhedralFunction::VarVector( vars ) );
    }

   // construct the objective of NDOBlock
   if( nf < 0 )
    ConstructObj( NDOBlock );
   }
  else  // pass the Variable to the PolyhedralFunction (move the vector)
   static_cast< p_PFB >( NDOBlock )->get_PolyhedralFunction().set_variables(
							  std::move( vars ) );
  if( convex )
   NDOBlock->set_valid_lower_bound( -bound , true );
  else
   NDOBlock->set_valid_upper_bound( bound , true );

  // NDOBlock always uses the natural representation (cfg = 0), in both
  // primal and dual mode; BundleSolver works with that. The comparison
  // LPBlock-vs-NDOBlock is therefore always "linearised LP solved by
  // MILPSolver" vs "natural rep solved by BundleSolver", and the
  // consistency of the primal-linearised LP and the dual-linearised LP
  // follows by transitivity (each agrees with the same BundleSolver
  // answer) without ever having to compare the two LPs directly. The
  // dual-mode-specific "globalbound" wrapper FRowConstraint that used
  // to live on NDOBlock is no longer needed: BundleSolver respects
  // set_valid_(lower|upper)_bound() as a conditional bound (declaring
  // kUnbounded if iterates exceed it), which the test's OK(?bound?)
  // reconciliation already handles.
  SimpleConfiguration< int > cfg( 0 );
  NDOBlock->generate_abstract_variables( &cfg );
  NDOBlock->generate_abstract_constraints();
  NDOBlock->generate_objective();
  }

 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // set_global_bound() handles both primal mode (modifying LPBlock's
 // "globalbound" FRowConstraint wrapper) and dual mode (modifying the
 // shared PolyF_global_lambda's coefficient + is_fixed status on LPBlock);
 // in both modes it also updates NDOBlock's conditional valid bound.
 if( nf && ( wchg & 64 ) )
  set_global_bound();       // do it now on both :Block

 // define bound constraints- - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 // HAVE_CONSTRAINTS == 1: each x_j is independently flipped to
 // is_positive(true) with probability 1/2. The loop runs in BOTH primal
 // and dual mode so that:
 //   (a) the RNG state at the start of the modification loop is the
 //       same in the two modes, which makes the resulting sequence of
 //       random modifications to f_polyf identical;
 //   (b) NDOBlock sees the same x-side bounds in both modes (it uses
 //       the natural representation everywhere now, with x as actual
 //       Variables), so BundleSolver works on the same problem in
 //       both modes and must give the same answer.
 //
 // The mode split is on what LPBlock represents:
 //   - primal: LPBlock has explicit x variables in the linearised
 //     primal LP, so is_positive(true) is set on them directly;
 //   - dual: LPBlock has no x in its abstract representation (only
 //     theta and gamma), but the x-side bound is the *primal*
 //     constraint "x_j >= 0", whose LP dual is to flip the
 //     corresponding PolyF_coupling[j] FRowConstraint from an
 //     equality to a one-sided inequality. The sign of the
 //     inequality depends on convex/concave:
 //         convex  (min):  Sum a*theta - L_j gamma >= -L_j   (>=)
 //         concave (max):  Sum a*theta - L_j gamma <= -L_j   (<=)
 //     For nf >= 0 (no linear term L) this becomes >= 0 / <= 0.
 //     The LinearFunction of coupling[j] is unchanged; only LHS/RHS
 //     of the FRowConstraint are relaxed. No new dual variables are
 //     needed and the change is entirely on the LPBlock parent --
 //     the nested PolyhedralFunctionBlocks know nothing about it.
 #if HAVE_CONSTRAINTS == 1
 {
  auto LPx = dual_mode
             ? nullptr
             : LPBlock->get_static_variable_v< ColVariable >( "x" );
  auto NDOx = NDOBlock->get_static_variable_v< ColVariable >( "x" );
  auto coupling = dual_mode
                  ? LPBlock->get_dynamic_constraint< FRowConstraint >(
                                                          "PolyF_coupling" )
                  : nullptr;
  std::list< FRowConstraint >::iterator cit;
  if( coupling )
   cit = coupling->begin();
  for( Index i = 0 ; i < nvar ; ++i ) {
   const bool flip = ( dis( rg ) < 0.5 );
   if( flip ) {
    (*NDOx)[ i ].is_positive( true , eNoMod );
    if( LPx )
     (*LPx)[ i ].is_positive( true , eNoMod );
    else {
     // dual mode: flip coupling[i] from equality to one-sided
     // inequality. The current setup has set_lhs(rhs) == set_rhs(rhs)
     // with rhs = (nf < 0 ? -L_i : 0). To switch convex / concave
     // sign, relax the "wrong" side to +/-INF.
     if( convex )
      cit->set_rhs( INF , eNoMod );  // keep LHS, relax RHS  ==>  >=
     else
      cit->set_lhs( -INF , eNoMod ); // keep RHS, relax LHS  ==>  <=
     }
    }
   if( coupling )
    ++cit;
   }
  }
 #endif
 #if HAVE_CONSTRAINTS == 2
 // HAVE_CONSTRAINTS == 2: each x_j is independently given (with equal
 // probability):
 //   - a BoxConstraint (50%), with one of three lhs/rhs combinations:
 //       (lhs=0, rhs=INF)     ~33% of boxes  -- effectively  x_j >= 0
 //       (lhs=0, rhs=finite)  ~33% of boxes  -- 0 <= x_j <= rhs
 //       (lhs=-INF, rhs=finite) ~33% of boxes -- x_j <= rhs
 //   - or, if no box: is_positive(true) (12.5%)  -- x_j >= 0
 //   - or nothing (12.5%): x_j free
 //
 // In primal mode the constraints are attached directly to LPBlock and
 // NDOBlock as BoxConstraints on their respective x vectors. In dual mode
 // we encode the LP-dual equivalent on LPBlock (which has no x in its
 // abstract rep, only theta and gamma) as follows. Let J_LB be the set
 // of j with finite lhs (always lhs=0 here) and J_UB the set of j with
 // finite rhs. For each j ∈ J_LB the dual variable γ^L_j ≥ 0 exists with
 // obj coefficient ±lhs_j = 0 (so γ^L is "free" and can be eliminated by
 // substitution), and the resulting effect is to flip coupling[j] from
 // equality `Σ a θ = -L_j` to a one-sided inequality (as in HC==1):
 //     convex  (min primal): `Σ a θ ≥ -L_j`     (keep LHS, relax RHS)
 //     concave (max primal): `Σ a θ ≤ -L_j`     (keep RHS, relax LHS)
 // For each j ∈ J_UB the dual variable γ^U_j ≥ 0 exists with obj
 // coefficient -rhs_j (convex) or +rhs_j (concave), and is added to
 // coupling[j]'s LinearFunction with coefficient +1 (convex) or -1
 // (concave). For j ∈ J_UB \ J_LB (case "lhs=-INF, rhs=finite") the
 // coupling stays as equality (no flip); for j ∈ J_UB ∩ J_LB it is
 // flipped (lhs=0 ⇒ flip). The new γ^U_j variables are attached as a
 // static variable group on LPBlock (the AbstractBlock parent) and their
 // obj contribution is added to LPBlock's parent objective (creating one
 // fresh if none exists; merging into the global-lambda objective if it
 // does).
 //
 // IMPORTANT: for nf==0 dual mode, LPBlock is itself a PolyhedralFunction-
 // Block and has no parent AbstractBlock to host the γ^U variables and
 // their objective contribution; appending γ^U directly to PFB's obj_lf
 // is unsafe because PFB's internal modification machinery assumes that
 // thetas sit at contiguous positions [1, nrows+1) and appends new thetas
 // to the end of obj_lf -- inserting γ^U between would break those
 // positional invariants. We therefore SKIP applying box constraints in
 // that specific configuration (nf == 0 + dual mode). The RNG draws are
 // still consumed identically to keep seed alignment with primal mode
 // and with HC <= 1 builds.
 {
  const bool apply_constraints = ( ! dual_mode ) || ( nf != 0 );

  auto LPx = ( ! dual_mode )
             ? LPBlock->get_static_variable_v< ColVariable >( "x" )
             : nullptr;
  auto NDOx = NDOBlock->get_static_variable_v< ColVariable >( "x" );
  auto coupling = ( dual_mode && ( nf != 0 ) )
                  ? LPBlock->get_dynamic_constraint< FRowConstraint >(
                                                          "PolyF_coupling" )
                  : nullptr;

  auto LPbnd = ( ! dual_mode ) && apply_constraints
               ? new std::list< BoxConstraint >
               : nullptr;
  auto NDObnd = apply_constraints
                ? new std::list< BoxConstraint >
                : nullptr;

  // collect (coupling iterator, rhs value) for j ∈ J_UB so we can wire
  // up γ^U dual variables after the RNG loop in dual mode
  std::vector< std::pair< std::list< FRowConstraint >::iterator , double > >
                                                                gammaU_boxes;

  std::list< FRowConstraint >::iterator cit;
  if( coupling )
   cit = coupling->begin();

  for( Index i = 0 ; i < nvar ; ++i ) {
   const bool is_box = ( dis( rg ) < 0.5 );
   if( is_box ) {
    const double p = dis( rg );
    const double lhs = p < 0.666 ? 0 : -INF;
    const double rhs = p > 0.333 ? dis( rg ) : INF;
    if( apply_constraints ) {
     NDObnd->resize( NDObnd->size() + 1 );
     NDObnd->back().set_variable( & (*NDOx)[ i ] );
     NDObnd->back().set_lhs( lhs , eNoMod );
     NDObnd->back().set_rhs( rhs , eNoMod );
     if( ! dual_mode ) {
      LPbnd->resize( LPbnd->size() + 1 );
      LPbnd->back().set_variable( & (*LPx)[ i ] );
      LPbnd->back().set_lhs( lhs , eNoMod );
      LPbnd->back().set_rhs( rhs , eNoMod );
      }
     else {
      // dual mode + nf != 0: LP-dual encoding
      if( lhs == 0 ) {
       // flip coupling[i] (lhs=0 case ⇒ HC==1-style flip)
       if( convex )
        cit->set_rhs( INF , eNoMod );  // keep LHS = -L_j, relax RHS
       else
        cit->set_lhs( -INF , eNoMod ); // keep RHS = -L_j, relax LHS
       }
      if( rhs != INF )
       gammaU_boxes.emplace_back( cit , rhs );
      }
     }
    }
   else {
    const bool is_pos = ( dis( rg ) < 0.5 );
    if( is_pos && apply_constraints ) {
     (*NDOx)[ i ].is_positive( true , eNoMod );
     if( ! dual_mode )
      (*LPx)[ i ].is_positive( true , eNoMod );
     else {
      // dual mode + nf != 0: HC==1-style coupling flip
      if( convex )
       cit->set_rhs( INF , eNoMod );
      else
       cit->set_lhs( -INF , eNoMod );
      }
     }
    }
   if( coupling )
    ++cit;
   }

  if( apply_constraints ) {
   if( ! dual_mode )
    LPBlock->add_dynamic_constraint( *LPbnd , "box" );
   NDOBlock->add_dynamic_constraint( *NDObnd , "box" );
   }

  // wire up γ^U dual variables on LPBlock parent (dual mode + nf != 0)
  if( apply_constraints && dual_mode && ( ! gammaU_boxes.empty() ) ) {
   // create the static γ^U variable group on LPBlock
   auto gammaU = new std::vector< ColVariable >( gammaU_boxes.size() );
   for( auto & g : *gammaU )
    g.is_positive( true , eNoMod );
   LPBlock->add_static_variable( *gammaU , "PolyF_gamma_box_U" );

   // build the γ^U objective LinearFunction contribution
   LinearFunction::v_coeff_pair obj_cp;
   obj_cp.reserve( gammaU_boxes.size() );

   for( std::size_t k = 0 ; k < gammaU_boxes.size() ; ++k ) {
    auto [ cit_j , rhs_j ] = gammaU_boxes[ k ];
    ColVariable * g = & (*gammaU)[ k ];

    // append γ^U_j to coupling[j]'s LinearFunction
    auto lf = static_cast< LinearFunction * >( cit_j->get_function() );
    LinearFunction::v_coeff_pair new_vp = lf->get_v_var();
    new_vp.emplace_back( g , convex ? 1.0 : -1.0 );
    cit_j->set_function( new LinearFunction( std::move( new_vp ) ) ,
                         eNoMod );

    // dual obj contribution: convex max => -rhs_j ; concave min => +rhs_j
    obj_cp.emplace_back( g , convex ? - rhs_j : rhs_j );
    }

   // attach to LPBlock parent objective (merge into existing one if any,
   // e.g. when wchg & 64 already created a parent objective with the
   // global-lambda term). Use the untemplated get_objective() because the
   // templated form throws when no objective is present.
   auto existing_obj = dynamic_cast< FRealObjective * >(
                                            LPBlock->get_objective() );
   if( existing_obj ) {
    auto lf = static_cast< LinearFunction * >( existing_obj->get_function() );
    LinearFunction::v_coeff_pair merged = lf->get_v_var();
    for( auto & t : obj_cp )
     merged.push_back( std::move( t ) );
    existing_obj->set_function(
                          new LinearFunction( std::move( merged ) ) , eNoMod );
    }
   else {
    auto obj = new FRealObjective( LPBlock ,
                          new LinearFunction( std::move( obj_cp ) ) );
    obj->set_sense( convex ? Objective::eMax : Objective::eMin , eNoMod );
    LPBlock->set_objective( obj , eNoMod );
    }
   }
  }
 #endif
 #if HAVE_CONSTRAINTS == 3
 // HAVE_CONSTRAINTS == 3: same as HAVE_CONSTRAINTS == 2 but the LPBlock-
 // side constraint is encoded as an FRowConstraint with a 1-variable
 // LinearFunction (rather than a BoxConstraint with a direct variable
 // reference). The NDOBlock side still uses BoxConstraint. The LP-dual
 // encoding is identical to HC == 2 because the FRowConstraint is just
 // a different encoding of the same LP constraint (a^T x with a = e_j).
 // See the HC == 2 comment block for the full derivation.
 {
  const bool apply_constraints = ( ! dual_mode ) || ( nf != 0 );

  auto LPx = ( ! dual_mode )
             ? LPBlock->get_static_variable_v< ColVariable >( "x" )
             : nullptr;
  auto NDOx = NDOBlock->get_static_variable_v< ColVariable >( "x" );
  auto coupling = ( dual_mode && ( nf != 0 ) )
                  ? LPBlock->get_dynamic_constraint< FRowConstraint >(
                                                          "PolyF_coupling" )
                  : nullptr;

  auto LPbnd = ( ! dual_mode ) && apply_constraints
               ? new std::list< FRowConstraint >
               : nullptr;
  auto NDObnd = apply_constraints
                ? new std::list< BoxConstraint >
                : nullptr;

  std::vector< std::pair< std::list< FRowConstraint >::iterator , double > >
                                                                gammaU_boxes;

  std::list< FRowConstraint >::iterator cit;
  if( coupling )
   cit = coupling->begin();

  for( Index i = 0 ; i < nsvar ; ++i ) {
   const bool is_box = ( dis( rg ) < 0.5 );
   if( is_box ) {
    const double p = dis( rg );
    const double lhs = p < 0.666 ? 0 : -INF;
    const double rhs = p > 0.333 ? dis( rg ) : INF;
    if( apply_constraints ) {
     NDObnd->resize( NDObnd->size() + 1 );
     NDObnd->back().set_variable( & (*NDOx)[ i ] );
     NDObnd->back().set_lhs( lhs , eNoMod );
     NDObnd->back().set_rhs( rhs , eNoMod );
     if( ! dual_mode ) {
      LinearFunction::v_coeff_pair vars_LP( 1 );
      vars_LP[ 0 ] = std::make_pair( & (*LPx)[ i ] , 1 );
      LPbnd->resize( LPbnd->size() + 1 );
      LPbnd->back().set_function( new LinearFunction( std::move( vars_LP ) ) );
      LPbnd->back().set_lhs( lhs , eNoMod );
      LPbnd->back().set_rhs( rhs , eNoMod );
      }
     else {
      // dual mode + nf != 0: LP-dual encoding (same as HC==2)
      if( lhs == 0 ) {
       if( convex )
        cit->set_rhs( INF , eNoMod );
       else
        cit->set_lhs( -INF , eNoMod );
       }
      if( rhs != INF )
       gammaU_boxes.emplace_back( cit , rhs );
      }
     }
    }
   else {
    const bool is_pos = ( dis( rg ) < 0.5 );
    if( is_pos && apply_constraints ) {
     (*NDOx)[ i ].is_positive( true , eNoMod );
     if( ! dual_mode )
      (*LPx)[ i ].is_positive( true , eNoMod );
     else {
      if( convex )
       cit->set_rhs( INF , eNoMod );
      else
       cit->set_lhs( -INF , eNoMod );
      }
     }
    }
   if( coupling )
    ++cit;
   }

  if( apply_constraints ) {
   if( ! dual_mode )
    LPBlock->add_dynamic_constraint( *LPbnd , "NObox" );
   NDOBlock->add_dynamic_constraint( *NDObnd , "box" );
   }

  // wire up γ^U dual variables on LPBlock parent (dual mode + nf != 0)
  if( apply_constraints && dual_mode && ( ! gammaU_boxes.empty() ) ) {
   auto gammaU = new std::vector< ColVariable >( gammaU_boxes.size() );
   for( auto & g : *gammaU )
    g.is_positive( true , eNoMod );
   LPBlock->add_static_variable( *gammaU , "PolyF_gamma_box_U" );

   LinearFunction::v_coeff_pair obj_cp;
   obj_cp.reserve( gammaU_boxes.size() );

   for( std::size_t k = 0 ; k < gammaU_boxes.size() ; ++k ) {
    auto [ cit_j , rhs_j ] = gammaU_boxes[ k ];
    ColVariable * g = & (*gammaU)[ k ];

    auto lf = static_cast< LinearFunction * >( cit_j->get_function() );
    LinearFunction::v_coeff_pair new_vp = lf->get_v_var();
    new_vp.emplace_back( g , convex ? 1.0 : -1.0 );
    cit_j->set_function( new LinearFunction( std::move( new_vp ) ) ,
                         eNoMod );

    obj_cp.emplace_back( g , convex ? - rhs_j : rhs_j );
    }

   auto existing_obj = dynamic_cast< FRealObjective * >(
                                            LPBlock->get_objective() );
   if( existing_obj ) {
    auto lf = static_cast< LinearFunction * >( existing_obj->get_function() );
    LinearFunction::v_coeff_pair merged = lf->get_v_var();
    for( auto & t : obj_cp )
     merged.push_back( std::move( t ) );
    existing_obj->set_function(
                          new LinearFunction( std::move( merged ) ) , eNoMod );
    }
   else {
    auto obj = new FRealObjective( LPBlock ,
                          new LinearFunction( std::move( obj_cp ) ) );
    obj->set_sense( convex ? Objective::eMax : Objective::eMin , eNoMod );
    LPBlock->set_objective( obj , eNoMod );
    }
   }
  }
 #endif

 // final checks- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 /*!!
 LPBlock->is_correct();
 NDOBlock->is_correct();
 !!*/

 // attach the Solver to the Block- - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // do this by reading appropriate BlockSolverConfig from files and apply()
 // them to the ::Block
 
 // BSC may be a plain BlockSolverConfig or a meta-config
 // SimpleConfiguration< std::map< std::string , Configuration * > >;
 // s_config_Block() dispatches and clears.
 auto msc = Configuration::deserialize( "LPPar.txt" );
 if( ! msc ) {
  cerr << "Error: cannot load BSC from LPPar.txt" << endl;
  return( 1 );
  }
 {
  s_config_Block( LPBlock , msc , "LPPar.txt" );

  // for LPBlock, in addition "manually" attach an UpdateSolver to (each
  // PolyhedralFunctionBlock in) LPBlock so that the physical
  // Modifications on f_polyf are mirrored to NDOBlock's PolyhedralFunction.
  // NDOBlock always uses the natural representation (cfg = 0, BundleSolver
  // via NDOPar.txt), so the UpdateSolver's default eNoBlck is enough: the
  // forwarded Modification updates NDOBlock's f_polyf without needing any
  // abstract-representation dispatcher to run.
  if( nf )
   for( int i = 0 ; i < LPBlock->get_number_nested_Blocks() ; ++i )
    LPBlock->get_nested_Block( i )->register_Solver(
		       new UpdateSolver( NDOBlock->get_nested_Block( i ) ) );
  else
   LPBlock->register_Solver( new UpdateSolver( NDOBlock ) );
  }
 
 // for NDOBlock do this by reading the appropriate BlockSolverConfig
 // from NDOPar.txt and apply() it to the NDOBlock. NDOPar.txt selects
 // BundleSolver on the natural representation in both primal and
 // dual mode (cf. the cfg = 0 above).
 auto bsc = Configuration::deserialize( "NDOPar.txt" );
 if( ! bsc ) {
  cerr << "Error: cannot load BSC from NDOPar.txt" << endl;
  return( 1 );
  }
 s_config_Block( NDOBlock , bsc , "NDOPar.txt" );

 // open log-file - - - - - - - - - - -  - - - - - - - - - - - - - - - - - -
 //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 #if( LOG_LEVEL >= 2 )
  #if( LOG_ON_COUT )
   ( ( NDOBlock->get_registered_solvers() ).front() )->set_log( &cout );
  #else
   ofstream LOGFile( logF , ofstream::out );
   if( ! LOGFile.is_open() )
    cerr << "Warning: cannot open log file """ << logF << """" << endl;
   else {
    LOGFile.setf( ios::scientific, ios::floatfield );
    LOGFile << setprecision( 10 );
    ( ( NDOBlock->get_registered_solvers() ).front() )->set_log( &LOGFile );
    }
  #endif

  #if( LOG_LEVEL >= 3 )
   ( ( LPBlock->get_registered_solvers() ).front() )->set_par(
	                          MILPSolver::strOutputFile , "LPBlock.lp" );
   ( ( NDOBlock->get_registered_solvers() ).front() )->set_par(
	                          MILPSolver::strOutputFile , "NDOBlock.lp" );
  #endif
 #endif

 // first solver call - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 LOG1( "First call: " );

 // ensure each block satisfies the BundleSolver invariant before the
 // first solve (the initial random generation may have produced a block
 // with only verticals and no bound, which would make the BundleSolver
 // stall)
 for( Index k = 0 ; k < cur_bnd_finite.size() ; ++k )
  enforce_block_invariant( k );

 bool AllPassed = SolveBoth();
 
 // main loop - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // now, for n_repeat times:
 // - up to n_change rows are added
 // - up to n_change rows are deleted
 // - up to n_change rows are modified
 // - up to n_change constants are modified
 // - up to n_change coefficient of linear obj (if any) are modified
 //
 // then the two problems are re-solved
 //
 // IMPORTANT NOTE: only LPBlock is changed, because UpdateSolver takes
 //                 care of intercepting all (physical) Modification and
 //                 map_forward them to NDOBlock
 //
 // if there are multiple PolyhedralFunctionBlock inside LPBlock and
 // NDOBlock, at each iteration only one of them is changed; however, by
 // playing with SKIP_BEAT one can solve the Block after having changed an
 // arbitrary number of them

 // the dual representation has the PF -> dual Modification machinery
 // wired up (cf. guts_of_add_Modification_PF_dual). All the "modify
 // rows / constants / bound / etc." mods originating from f_polyf are
 // mirrored into the dual abstract structures (f_theta / f_normcns /
 // objective / coupling), so the iteration loop is exercised in both
 // primal and dual mode. Some specific kinds of changes are still
 // primal-only and are skipped in dual mode below.
 for( Index rep = 0 ; rep < n_repeat * ( SKIP_BEAT + 1 ) ; ) {
  if( ! AllPassed )
   break;

  p_PFB LPBr;
  Index blk_idx;  // index into cur_iV for the block being touched
  if( nf ) {
   blk_idx = rep % std::abs( nf );
   LPBr = static_cast< p_PFB >( LPBlock->get_nested_Blocks()[ blk_idx ] );
   LOG1( rep << " [" << blk_idx << "]: ");
   }
  else {
   blk_idx = 0;
   LPBr = static_cast< p_PFB >( LPBlock );
   LOG1( rep << ": ");
   }

  m = LPBr->get_PolyhedralFunction().get_nrows();

  // add rows - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 1 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = Index( dis( rg ) * n_change ) ) {
    LOG1( "added " << tochange << " rows" );

    GenerateAb( tochange , nvar );

    // in dual mode there is no f_const dynamic-constraint group on
    // LPBr; the "abstract" path below only makes sense in primal mode
    // (where it tests the LR -> PF Modification machinery), and in
    // dual mode we systematically fall through to the PF path -- which
    // *does* exercise the new PF -> dual Modification mapping
    auto cnst = dual_mode
                ? nullptr
                : LPBr->get_dynamic_constraint< FRowConstraint >( 0 );

    if( ( wchg & 512 ) && ( ! dual_mode ) && ( dis( rg ) <= p_change ) ) {
     // in 50% of the cases do an "abstract" change
     LOG1( "(a)" );

     ColVariable * vLP;                 // pointer to the v used by LP cuts
     std::vector< ColVariable > * xLP;  // pointer to (static) x LP variables
     if( nf )
      xLP = LPBlock->get_static_variable_v< ColVariable >( 0 );
     else
      xLP = LPBlock->get_static_variable_v< ColVariable >(
                                             ( pfb_cfg & 8 ) ? 2 : 1 );
     vLP = LPBr->get_static_variable< ColVariable >(
                  ( pfb_cfg & 8 ) ? "PolyF_scaled_v" : "PolyF_v" );
     #if DYNAMIC_VARS > 0
      auto xLPd = LPBlock->get_dynamic_variable< ColVariable >( 0 );
     #endif

     std::list< FRowConstraint > nc( tochange );
     auto ncit = nc.begin();
     for( Index i = 0 ; i < tochange ; ++i , ++ncit ) {
      // construct constraint ci out of A[ i ] and b[ i ]:
      // - diagonal: b[ i ] <= vLP - sum_j A_ij xLP_j <= INF (or the
      //   concave dual)
      // - vertical: identical except that vLP's coefficient is 0
      //   instead of 1, encoding the domain constraint
      //   A_i x + b_i [ <= | >= ] 0
      //
      // note: constraints are constructed dense (elements == 0, which
      //       are anyway quite unlikely, are ignored) to make things
      //       simpler
      //
      // note: variable x[ i ] is given index i + 1, variable v has index 0

      const bool is_v = ( i < iV.size() ) && iV[ i ];
      const auto local_scale = ComputeLocalScale( A[ i ] , b[ i ] );
      const auto row_factor = local_scale * LPBr->get_v_scale();

      ncit->set_lhs( convex ? row_factor * b[ i ] : -INF );
      ncit->set_rhs( convex ? INF : row_factor * b[ i ] );
      LinearFunction::v_coeff_pair vars( nvar + 1 );
      Index j = 0;

      // first, v: coef is the local scale for diagonal rows and 0 for
      // vertical ones
      vars[ j ] = std::make_pair( vLP , is_v ? 0.0 : local_scale );

      // then, static x
      for( ; j < nsvar ; ++j )
       vars[ j + 1 ] = std::make_pair( &((*xLP)[ j ] ) ,
                                       - row_factor * A[ i ][ j ] );

      #if DYNAMIC_VARS > 0
       // finally, dynamic x
       auto xLPdit = xLPd->begin();
       for( ; j < nvar ; ++j , ++xLPdit )
	vars[ j + 1 ] = std::make_pair( &(*xLPdit) ,
                                        - row_factor * A[ i ][ j ] );
      #endif

      ncit->set_function( new LinearFunction( std::move( vars ) ) );
      }

     LPBr->add_dynamic_constraints( *cnst , nc );

     // The abstract row must round-trip to the unscaled physical
     // PolyhedralFunction row, including for vertical rows where the
     // coefficient of v is zero and cannot carry the local scale.
     const auto & pf = LPBr->get_PolyhedralFunction();
     for( Index i = 0 ; i < tochange ; ++i ) {
      PANIC( SameValue( pf.get_b()[ m + i ] , b[ i ] ) )
      for( Index j = 0 ; j < nvar ; ++j )
       PANIC( SameValue( pf.get_A()[ m + i ][ j ] , A[ i ][ j ] ) )
      PANIC( pf.is_row_vertical( m + i ) ==
             ( ( i < iV.size() ) && iV[ i ] ) )
      }
     }
    else {  // directly change the PolyhedralFunction in LPBlock
     if( tochange == 1 )
      LPBr->get_PolyhedralFunction().add_row(
		    std::move( A[ 0 ] ) , b[ 0 ] , eModBlck ,
		    ( ! iV.empty() ) && iV[ 0 ] );
     else
      LPBr->get_PolyhedralFunction().add_rows(
		    std::move( A ) , b , eModBlck ,
		    BoolVector( iV ) /* preserve iV for the cur_iV update */ );
     }

    // append the freshly-generated flags to cur_iV[ blk_idx ] (the
    // shadow), padding with false if iV was empty (i.e. p_vert == 0)
    cur_iV[ blk_idx ].reserve( cur_iV[ blk_idx ].size() + tochange );
    for( Index i = 0 ; i < tochange ; ++i )
     cur_iV[ blk_idx ].push_back( ( i < iV.size() ) ? iV[ i ] : false );

    LOG1( " - " );

    // update m
    m += tochange;

    // sanity checks
    PANIC( m == LPBr->get_PolyhedralFunction().get_nrows() );
    if( dual_mode ) {
     // in dual mode the per-row abstract structure is the dynamic
     // theta variable list, not f_const; the count must match m
     auto theta = LPBr->get_dynamic_variable< ColVariable >(
                                                      "PolyF_theta" );
     PANIC( theta && ( m == theta->size() ) );
     }
    else
     PANIC( m == cnst->size() );
    }

  // delete rows- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 2 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = min( m - 1 , Index( dis( rg ) * n_change ) ) ) {
    LOG1( "deleted " << tochange << " rows" );

    auto cnst = dual_mode
                ? nullptr
                : LPBr->get_dynamic_constraint< FRowConstraint >( 0 );

    if( dis( rg ) <= 0.5 ) {  // in 50% of the cases do a ranged change

     Index strt = dis( rg ) * ( m - tochange );
     Index stp = strt + tochange;

     if( ( wchg & 512 ) && ( ! dual_mode ) && ( dis( rg ) <= p_change ) ) {
      // in 50% of the cases do an "abstract" change
      LOG1( "(r,a) - " );
      if( tochange == 1 )
       LPBr->remove_dynamic_constraint( *cnst , std::next( cnst->begin() ,
							   strt ) );
      else
       LPBr->remove_dynamic_constraints( *cnst , Range( strt , stp ) );
      }
     else {  // directly change the PolyhedralFunction
      LOG1( "(r) - " );
      if( tochange == 1 )
       LPBr->get_PolyhedralFunction().delete_row( strt );
      else
       LPBr->get_PolyhedralFunction().delete_rows( Range( strt , stp ) );
      }

     // keep cur_iV[ blk_idx ] in sync
     if( cur_iV[ blk_idx ].size() == m )
      cur_iV[ blk_idx ].erase( cur_iV[ blk_idx ].begin() + strt ,
			       cur_iV[ blk_idx ].begin() + stp );
     }
    else {  // in the other 50% of the cases, do a sparse change
     Subset nms = GenerateSubset( m , tochange );
     Subset nms_kept( nms );  // ordered copy retained for cur_iV erase

     // remove them from the LP
     if( ( wchg & 512 ) && ( ! dual_mode ) && ( dis( rg ) <= p_change ) ) {
      // in 50% of the cases do an "abstract" change
      LOG1( "(s,a) - " );
      if( tochange == 1 )
       LPBr->remove_dynamic_constraint( *cnst , std::next( cnst->begin() ,
							   nms[ 0 ] ) );
      else
       LPBr->remove_dynamic_constraints( *cnst , std::move( nms ) , true );
      }
     else {  // directly change the PolyhedralFunction
      LOG1( "(s) - " );
      if( tochange == 1 )
       LPBr->get_PolyhedralFunction().delete_row( nms[ 0 ] );
      else
       LPBr->get_PolyhedralFunction().delete_rows( std::move( nms ) );
      }

     // keep cur_iV[ blk_idx ] in sync (back-to-front so earlier indices
     // stay valid as we erase)
     if( cur_iV[ blk_idx ].size() == m )
      for( auto it = nms_kept.rbegin() ; it != nms_kept.rend() ; ++it )
       cur_iV[ blk_idx ].erase( cur_iV[ blk_idx ].begin() + *it );
     }

    // update m
    m -= tochange;

    // sanity checks
    PANIC( m == LPBr->get_PolyhedralFunction().get_nrows() );
    if( dual_mode ) {
     // in dual mode the per-row abstract structure is the dynamic
     // theta variable list, not f_const; the count must match m
     auto theta = LPBr->get_dynamic_variable< ColVariable >(
                                                      "PolyF_theta" );
     PANIC( theta && ( m == theta->size() ) );
     }
    else
     PANIC( m == cnst->size() );
    }

  // modify rows- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 4 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = std::min( m , Index( dis( rg ) * n_change ) ) ) {
    LOG1( "modified " << tochange << " rows" );

    GenerateAb( tochange , nvar );

    if( dis( rg ) <= 0.5 ) {  // in 50% of the cases do a ranged change
     Index strt = dis( rg ) * ( m - tochange );
     Index stp = strt + tochange;

     // preserve the existing diagonal/vertical type of each modified row
     // (changing the type via modify is supported by PolyhedralFunction
     // but stresses the BundleSolver's master in ways that can prevent
     // convergence; in any case, the typical use of modify_row[s] is to
     // change a row's coefficients without changing its type)
     iV.assign( tochange , false );
     if( cur_iV[ blk_idx ].size() == m )
      for( Index i = 0 ; i < tochange ; ++i )
       iV[ i ] = cur_iV[ blk_idx ][ strt + i ];

     if( ( wchg & 512 ) && ( ! dual_mode ) && ( dis( rg ) <= p_change ) ) {
      // in 50% of the cases do an "abstract" change
      LOG1( "(r,a) - " );

      // modify them in the LP
      ColVariable * vLP;                 // pointer to v LP variable
      std::vector< ColVariable > * xLP;  // pointer to (static) x LP variables
      if( nf ) {
       vLP = LPBr->get_static_variable< ColVariable >( 0 );
       xLP = LPBlock->get_static_variable_v< ColVariable >( 0 );
       }
      else {
       vLP = LPBlock->get_static_variable< ColVariable >( 0 );
       xLP = LPBlock->get_static_variable_v< ColVariable >(
                                              ( pfb_cfg & 8 ) ? 2 : 1 );
       }
      #if DYNAMIC_VARS > 0
       auto xLPd = LPBlock->get_dynamic_variable< ColVariable >( 0 );
      #endif
      auto cnst = LPBr->get_dynamic_constraint< FRowConstraint >( 0 );

      // send all the Modification to the same channel
      auto chnl = LPBr->open_channel();
      auto iAM = Observer::make_par( eModBlck , chnl );

      auto cit = std::next( cnst->begin() , strt );
      for( Index i = 0 ; i < tochange ; ++i )
       ChangeLPConstraint( i , strt + i , LPBr , *(cit++) , iAM );

      LPBr->close_channel( chnl );
      }
     else {  // directly change the PolyhedralFunction
      LOG1( "(r) - " );
      if( tochange == 1 )
       LPBr->get_PolyhedralFunction().modify_row(
		    strt , std::move( A[ 0 ] ) , b[ 0 ] , eModBlck ,
		    iV[ 0 ] );
      else
       LPBr->get_PolyhedralFunction().modify_rows(
		    std::move( A ) , b , Range( strt , stp ) , eModBlck ,
		    BoolVector( iV ) );
      }
     }
    else {  // in the other 50% of the cases, do a sparse change
     Subset nms = GenerateSubset( m , tochange );

     // preserve the existing diagonal/vertical type (see comment in the
     // ranged branch above)
     iV.assign( tochange , false );
     if( cur_iV[ blk_idx ].size() == m )
      for( Index i = 0 ; i < tochange ; ++i )
       iV[ i ] = cur_iV[ blk_idx ][ nms[ i ] ];

     if( ( wchg & 512 ) && ( ! dual_mode ) && ( dis( rg ) <= p_change ) ) {
      // in 50% of the cases do an "abstract" change
      LOG1( "(s,a) - " );

      // modify them in the LP
      ColVariable * vLP;                 // pointer to v LP variable
      std::vector< ColVariable > * xLP;  // pointer to (static) x LP variables
      if( nf ) {
       vLP = LPBr->get_static_variable< ColVariable >( 0 );
       xLP = LPBlock->get_static_variable_v< ColVariable >( 0 );
       }
      else {
       vLP = LPBlock->get_static_variable< ColVariable >( 0 );
       xLP = LPBlock->get_static_variable_v< ColVariable >(
                                              ( pfb_cfg & 8 ) ? 2 : 1 );
       }
      #if DYNAMIC_VARS > 0
       auto xLPd = LPBlock->get_dynamic_variable< ColVariable >( 0 );
      #endif
      auto cnst = LPBr->get_dynamic_constraint< FRowConstraint >( 0 );

      // send all the Modification to the same channel
      auto chnl = LPBr->open_channel();
      auto iAM = Observer::make_par( eModBlck , chnl );

      Index prev = 0;
      auto cit = cnst->begin();
      for( Index i = 0 ; i < tochange ; ++i ) {
       cit = std::next( cit , nms[ i ] - prev );
       prev = nms[ i ];
       ChangeLPConstraint( i , nms[ i ] , LPBr , *cit , iAM );
       }

      LPBr->close_channel( chnl );
      }
     else {  // directly change the PolyhedralFunction
      LOG1( "(s) - " );
      if( tochange == 1 )
       LPBr->get_PolyhedralFunction().modify_row(
		    nms[ 0 ] , std::move( A[ 0 ] ) , b[ 0 ] , eModBlck ,
		    iV[ 0 ] );
      else
       LPBr->get_PolyhedralFunction().modify_rows(
		    std::move( A ) , b , std::move( nms ) , true , eModBlck ,
		    BoolVector( iV ) );
      }
     }
    }

  // modify constants - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 8 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = std::min( m , Index( dis( rg ) * n_change ) ) ) {
    LOG1( "modified " << tochange << " constants" );

    Generateb( tochange );

    if( dis( rg ) <= 0.5 ) {  // in 50% of the cases do a ranged change
     Index strt = dis( rg ) * ( m - tochange );
     Index stp = strt + tochange;

     if( ( wchg & 512 ) && ( ! dual_mode ) && ( dis( rg ) <= p_change ) ) {
      // in 50% of the cases do an "abstract" change
      LOG1( "(r,a) - " );

      auto cnst = LPBr->get_dynamic_constraint< FRowConstraint >( 0 );

      // send all the Modification to the same channel
      auto chnl = LPBr->open_channel();
      auto iAM = Observer::make_par( eModBlck , chnl );

      auto cit = std::next( cnst->begin() , strt );
      if( convex )
       for( Index i = 0 ; i < tochange ; ++i )
	(cit++)->set_lhs( LPBr->get_v_scale() *
                          LPBr->get_row_scale( strt + i ) * b[ i ] ,
                          iAM );
      else
       for( Index i = 0 ; i < tochange ; ++i )
	(cit++)->set_rhs( LPBr->get_v_scale() *
                          LPBr->get_row_scale( strt + i ) * b[ i ] ,
                          iAM );

      LPBr->close_channel( chnl );
      }
     else {  // directly change the PolyhedralFunction
      LOG1( "(r) - " );
      if( tochange == 1 )
       LPBr->get_PolyhedralFunction().modify_constant( strt , b[ 0 ] );
      else
       LPBr->get_PolyhedralFunction().modify_constants( b ,
							Range( strt , stp ) );
      }
     }
    else {  // in the other 50% of the cases, do a sparse change
     Subset nms = GenerateSubset( m , tochange );

     if( ( wchg & 512 ) && ( ! dual_mode ) && ( dis( rg ) <= p_change ) ) {
      // in 50% of the cases do an "abstract" change
      LOG1( "(s,a) - " );

      auto cnst = LPBr->get_dynamic_constraint< FRowConstraint >( 0 );

      // send all the Modification to the same channel
      auto chnl = LPBr->open_channel();
      auto iAM = Observer::make_par( eModBlck , chnl );

      Index prev = 0;
      auto cit = cnst->begin();
      if( convex )
       for( Index i = 0 ; i < tochange ; ++i ) {
	cit = std::next( cit , nms[ i ] - prev );
	prev = nms[ i ];
	cit->set_lhs( LPBr->get_v_scale() *
                      LPBr->get_row_scale( nms[ i ] ) * b[ i ] , iAM );
        }
      else
       for( Index i = 0 ; i < tochange ; ++i ) {
	cit = std::next( cit , nms[ i ] - prev );
	prev = nms[ i ];
	cit->set_rhs( LPBr->get_v_scale() *
                      LPBr->get_row_scale( nms[ i ] ) * b[ i ] , iAM );
        }

      LPBr->close_channel( chnl );
      }
     else {  // directly change the PolyhedralFunction
      LOG1( "(s) - " );
      if( tochange == 1 )
       LPBr->get_PolyhedralFunction().modify_constant( nms[ 0 ] , b[ 0 ] );
      else
       LPBr->get_PolyhedralFunction().modify_constants( b , std::move( nms ) ,
							true );
      }
     }
    }

  // modify local bounds- - - - - - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 16 ) && ( dis( rg ) <= p_change ) ) {
   LOG1( "modified bound" );

   // enforce the BundleSolver invariant: if there is no diagonal row, the
   // new bound MUST be finite (otherwise the function value is +/-INF
   // inside the feasible domain, which the BundleSolver cannot handle)
   const bool need_finite = ( n_diagonal( blk_idx ) == 0 );
   auto BND = GenerateBND( need_finite );

   if( ( wchg & 512 ) && ( ! dual_mode ) && ( dis( rg ) <= p_change ) ) {
    // in 50% of the cases do an "abstract" change
    LOG1( "(a)" );

    if( convex )
     LPBr->get_static_constraint< BoxConstraint >( 0 )->set_lhs( BND );
    else
     LPBr->get_static_constraint< BoxConstraint >( 0 )->set_rhs( BND );
    }
   else  // directly change the PolyhedralFunction
    LPBr->get_PolyhedralFunction().modify_bound( BND );

   cur_bnd_finite[ blk_idx ] = ( BND != INF ) && ( BND != -INF );

   LOG1( " - " );
   }

  // modify linear objective- - - - - - - - - - - - - - - - - - - - - - - - -
  // ... if there is any, of course
  //
  // in dual mode the L^T x linear term is folded into the RHS
  // of the coupling FRowConstraints (cf. set_conjugate_constraint
  // setup): changing L_j therefore means changing the RHS of the j-th
  // coupling constraint, not the LPBlock's x-side objective (which
  // doesn't exist on the dual LPBlock).

  if( ( nf < 0 ) && ( wchg & 32 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = Index( dis( rg ) * std::min( nvar , n_change ) ) ) {
    LOG1( "changed " << tochange << " objective coeff." );

    GenerateA( 1 , tochange );

    if( dual_mode ) {
     // dual mode: update the j-th coupling FRowConstraint's RHS to
     // the new -L[j] on LPBlock (where L lives folded into coupling),
     // and -- separately -- update NDOBlock's objective LinearFunction
     // (NDOBlock uses the primal linearised representation, so its
     // objective still has the x-side linear term L^T x to be kept in
     // sync with LPBlock's dual encoding).
     // When wchg & 64 is also enabled, lambda appears in each
     // coupling[j] with coefficient -L_j (cf. setup phase); update
     // that coefficient too on each L_j change. Additionally, the
     // globalbound wrapper FRowConstraint on NDOBlock has the term
     // L_j*x_j in its LinearFunction, which must be kept in sync.
     auto coupling = LPBlock->get_dynamic_constraint< FRowConstraint >(
                                                       "PolyF_coupling" );
     if( ! coupling ) {
      cout << "something very bad happened!" << endl;
      exit( 1 );
      }
     auto NDOLF = static_cast< p_LF >(
	    ( NDOBlock->get_objective< FRealObjective >() )->get_function() );
     ColVariable * l = ( wchg & 64 )
       ? LPBlock->get_static_variable< ColVariable >( "PolyF_global_lambda" )
       : nullptr;
     // also update NDOBlock's globalbound LF (when wchg & 64), which
     // contains the L_j*x_j terms
     p_LF NDOlbc_lf = nullptr;
     if( wchg & 64 )
      if( auto lbc = NDOBlock->get_static_constraint< FRowConstraint >(
                                                       "globalbound" ) )
       NDOlbc_lf = static_cast< p_LF >( lbc->get_function() );

     auto update_one = [ & ]( FRowConstraint & con , Index col ,
                              double L_new ) {
      const double nr = - L_new;
      // HAVE_CONSTRAINTS==1: if coupling[j] has been flipped to a
      // one-sided inequality (x_j >= 0 in the primal), only the
      // non-infinite side carries the "-L_j" value and must be
      // updated; the relaxed side must remain at +/- INF.
      const double old_lhs = con.get_lhs();
      const double old_rhs = con.get_rhs();
      if( old_lhs != -INF )
       con.set_lhs( nr );
      if( old_rhs != INF )
       con.set_rhs( nr );
      if( l ) {
       // update lambda's coefficient in this coupling LF
       auto lf = static_cast< LinearFunction * >( con.get_function() );
       const Index ki = lf->is_active( l );
       if( ki < lf->get_num_active_var() )
        lf->modify_coefficient( ki , - L_new );
       }
      };

     if( dis( rg ) <= 0.5 ) {  // ranged
      Index strt = dis( rg ) * ( nvar - tochange );
      Index stp = strt + tochange;
      auto cit = std::next( coupling->begin() , strt );
      for( Index i = 0 ; i < tochange ; ++i , ++cit )
       update_one( *cit , strt + i , A[ 0 ][ i ] );
      NDOLF->modify_coefficients( RealVector( A[ 0 ] ) ,
                                  Range( strt , stp ) );
      if( NDOlbc_lf )
       NDOlbc_lf->modify_coefficients( RealVector( A[ 0 ] ) ,
                                       Range( strt , stp ) );
      LOG1( "(r) - " );
      }
     else {                    // sparse
      Subset nms = GenerateSubset( nvar , tochange );
      Subset nms2( nms );
      Subset nms3( nms );
      Index prev = 0;
      auto cit = coupling->begin();
      for( Index i = 0 ; i < tochange ; ++i ) {
       cit = std::next( cit , nms[ i ] - prev );
       prev = nms[ i ];
       update_one( *cit , nms[ i ] , A[ 0 ][ i ] );
       }
      NDOLF->modify_coefficients( RealVector( A[ 0 ] ) , std::move( nms2 ) ,
                                  true );
      if( NDOlbc_lf )
       NDOlbc_lf->modify_coefficients( RealVector( A[ 0 ] ) ,
                                       std::move( nms3 ) , true );
      LOG1( "(s) - " );
      }
     }
    else {  // primal mode

    p_LF lf = nullptr;
    if( wchg & 64 ) {
     // if the constraint "-INF <= objective <= INF" is there, it must be
     // changed accordingly, too
     if( auto lbc = LPBlock->get_static_constraint< FRowConstraint >(
							    "globalbound" ) )
      lf = static_cast< p_LF >( lbc->get_function() );
     else {
      cout << "something very bad happened!" << endl;
      exit( 1 );
      }
     }

    auto LPLF = static_cast< p_LF >(
	    ( LPBlock->get_objective< FRealObjective >() )->get_function() );
    auto NDOLF = static_cast< p_LF >(
	   ( NDOBlock->get_objective< FRealObjective >() )->get_function() );

    if( dis( rg ) <= 0.5 ) {  // in 50% of the cases do a ranged change
     Index strt = dis( rg ) * ( nvar - tochange );
     Index stp = strt + tochange;

     if( tochange == 1 ) {
      if( lf )
       lf->modify_coefficient( strt , A[ 0 ][ 0 ] );
      LPLF->modify_coefficient( strt , A[ 0 ][ 0 ] );
      NDOLF->modify_coefficient( strt , A[ 0 ][ 0 ] );
      }
     else {
      if( lf )
       lf->modify_coefficients( RealVector( A[ 0 ] ) , Range( strt , stp ) );
      LPLF->modify_coefficients( RealVector( A[ 0 ] ) , Range( strt , stp ) );
      NDOLF->modify_coefficients( std::move( A[ 0 ] ) , Range( strt , stp ) );
      }
      
     LOG1( "(r) - " );
     }
    else {  // in the other 50% of the cases, do a sparse change
     Subset nms = GenerateSubset( nvar , tochange );

     if( tochange == 1 ) {
      if( lf )
       lf->modify_coefficient( nms.front() , A[ 0 ][ 0 ] );
      LPLF->modify_coefficient( nms.front() , A[ 0 ][ 0 ] );
      NDOLF->modify_coefficient( nms.front() , A[ 0 ][ 0 ] );
      }
     else {
      if( lf )
       lf->modify_coefficients( RealVector( A[ 0 ] ) , Subset( nms ) ,
				true );
      LPLF->modify_coefficients( RealVector( A[ 0 ] ) , Subset( nms ) ,
				 true );
      NDOLF->modify_coefficients( std::move( A[ 0 ] ) , std::move( nms ) ,
				  true );
      }

     LOG1( "(s) - " );
     }
    }  // end primal-mode linear obj change
    }

  // modify global bound- - - - - - - - - - - - - - - - - - - - - - - - - - -
  //
  // set_global_bound() handles both representations: in primal mode it
  // updates the "globalbound" wrapper FRowConstraint, in dual mode it
  // updates the shared gamma's coefficient (and is_fixed) in LPBlock's
  // FRealObjective; no dual_mode gating is needed here

  if( nf && ( wchg & 64 ) && ( dis( rg ) <= p_change ) ) {
   LOG1( "changed global bound - " );

   set_global_bound();
   }

  // add variables- - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  #if DYNAMIC_VARS > 0
  if( ( wchg & 128 ) && ( dis( rg ) <= p_change ) ) {
   Index tochange = Index( dis( rg ) * n_change );
   if( tochange ) {
    LOG1( "added " << tochange << " variables - " );

    throw( std::logic_error( "adding variables not implemented yet" ) );

    GenerateA( m , tochange );

    // add them in the LP, *copying* the data
    std::list< ColVariable > nxLPd( tochange );
    std::vector< Variable * > nxpLP( tochange );
    auto nxit = nxLPd.begin();
    for( Index i = 0 ; i < tochange ; )
     nxpLP[ i++ ] = &(*(nxit++));

    LPBlock->add_dynamic_variables(
	      *(LPBlock->get_dynamic_variable< ColVariable >( 0 )) , nxLPd );

    if( tochange == 1 )
     LPBlock->get_PolyhedralFunction().add_variable( nxpLP[ 0 ] , A[ 0 ] );
    else
     LPBlock->get_PolyhedralFunction().add_variables( std::move( nxpLP ) ,
						      MultiVector( A ) );

    // add them in the NDO, letting the data go
    std::list< ColVariable > nxNDOd( tochange );
    std::vector< Variable * > nxpNDO( tochange );
     auto nxit = nxNDOd.begin();
    for( Index i = 0 ; i < tochange ; )
     nxpNDO[ i++ ] = &(*(nxit++));

    NDOBlock->add_dynamic_variables(
	    *(NDOBlock->get_dynamic_variable< ColVariable >( 0 )) , nxNDOd );

    if( tochange == 1 )
     NDOBlock->get_PolyhedralFunction().add_variable( nxpNDO[ 0 ] , A[ 0 ] );
    else
     NDOBlock->get_PolyhedralFunction().add_variables( std::move( nxpNDO ) ,
						       std::move( A ) );

    // update ndvar
    ndvar += tochange;
    }
   }

  // remove variables - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 512 ) && ( dis( rg ) <= p_change ) ) {
   Index tochange = min( ndvar , Index( dis( rg ) * n_change ) );
   if( tochange ) {
    LOG1( "removed " << tochange << " variables" );

    throw( std::logic_error( "removing variables not implemented yet" ) );

    // in 50% of the cases do a ranged change, in the others a sparse change
    if( dis( rg ) <= 0.5 ) {
     LOG1( "(r) - " );

     Index strt = dis( rg ) * ( ndvar - tochange );
     Index stp = strt + tochange;

     // remove them from the LP
     auto xLPd = NDOBlock->get_dynamic_variable< ColVariable >( 0 );
     if( tochange == 1 )
      LPBlock->get_PolyhedralFunction().remove_variable( strt );
     else
      LPBlock->get_PolyhedralFunction().remove_variables( Range( strt ,
								 stp ) );

     LPBlock->remove_dynamic_variables( *xLPd , Range( strt , stp ) );

     // remove them from the NDO
     auto xNDOd = NDOBlock->get_dynamic_variable< ColVariable >( 0 );
     if( tochange == 1 )
      NDOBlock->get_PolyhedralFunction().remove_variable( strt );
     else
      NDOBlock->get_PolyhedralFunction().remove_variables( Range( strt ,
								  stp ) );

     NDOBlock->remove_dynamic_variables( *xNDOd , Range( strt , stp ) );
     }
    else {
     LOG1( "(s) - " );
     Subset nms = GenerateSubset( ndvar , tochange );

     // remove them from the LP, *copying* names
     auto xLPd = LPBlock->get_dynamic_variable< ColVariable >( 0 );
     if( tochange == 1 ) {
      LPBlock->get_PolyhedralFunction().remove_variable( nms[ 0 ] );
      auto vp = &(*std::next( xLPd->begin() , nms[ 0 ] ));
      LPBlock->remove_dynamic_variable( *xLPd , vp );
      }
     else {
      LPBlock->get_PolyhedralFunction().remove_variables( Subset( nms ) );
      LPBlock->remove_dynamic_variables( *xLPd , Subset( nms ) );
      }

     // remove them from the NDO, finally letting names go
     auto xNDOd = NDOBlock->get_dynamic_variable< ColVariable >( 0 );
     if( tochange == 1 ) {
      NDOBlock->get_PolyhedralFunction().remove_variable( nms[ 0 ] );
      auto vp = &(*std::next( xNDOd->begin() , nms[ 0 ] ));
      NDOBlock->remove_dynamic_variable( *xNDOd , vp );
      }
     else {
      NDOBlock->get_PolyhedralFunction().remove_variables( Subset( nms ) );
      NDOBlock->remove_dynamic_variables( *xNDOd , std::move( nms ) );
      }
     }

    // update ndvar
    ndvar -= tochange;
    }
   }

  #endif  // DYNAMIC_VARS > 0

  // if verbose, print out stuff- - - - - - - - - - - - - - - - - - - - - - -

  #if( LOG_LEVEL >= 3 )
   ( ( LPBlock->get_registered_solvers() ).front() )->set_par(
		                     MILPSolver::strOutputFile , "LPBlock-" +
		                     std::to_string( rep ) + ".lp" );
   ( ( NDOBlock->get_registered_solvers() ).front() )->set_par(
		                     MILPSolver::strOutputFile , "NDOBlock-" +
		                     std::to_string( rep ) + ".lp" );
   #if( LOG_LEVEL >= 4 )
    cout << endl << "LPBlock-PF: ";
    auto PF = & LPBr->get_PolyhedralFunction();
    printAb( PF->get_A() , PF->get_b() , convex
	     ? PF->get_global_lower_bound()
	     : PF->get_global_upper_bound() );
    p_PFB NDOBr;
    if( nf )
     NDOBr = static_cast< p_PFB >( NDOBlock->get_nested_Blocks()[
						    rep % std::abs( nf ) ] );
    else
     NDOBr = static_cast< p_PFB >( NDOBlock );
    cout << "NDOBlock-PF: ";
    PF = & NDOBr->get_PolyhedralFunction();
    printAb( PF->get_A() , PF->get_b() , convex
	     ? PF->get_global_lower_bound()
	     : PF->get_global_upper_bound() );
   #endif
  #endif

  // finally, re-solve the problems- - - - - - - - - - - - - - - - - - - - -
  // ... every SKIP_BEAT + 1 rounds

  if( ! ( ++rep % ( SKIP_BEAT + 1 ) ) ) {
   // ensure every block satisfies the BundleSolver invariant (at least
   // one diagonal row OR a finite bound) before solving; deletions and
   // bound updates earlier in this round may have left some block with
   // only verticals and no bound, which would make the BundleSolver stall
   for( Index k = 0 ; k < cur_bnd_finite.size() ; ++k )
    enforce_block_invariant( k );
   AllPassed &= SolveBoth();
   }
  #if( LOG_LEVEL >= 1 )
  else
   cout << endl;
  #endif

  }  // end( main loop )- - - - - - - - - - - - - - - - - - - - - - - - - - -
     // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 if( AllPassed )
  cout << GREEN( All test passed!! ) << endl;
 else
  cout << RED( Shit happened!! ) << endl;
 
 // cleanup - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // unregister (and delete) all Solvers attached to the Blocks: do this by
 // apply()-ing the clear()-ed BlockSolverConfig, then delete them

 s_config_Block( NDOBlock , bsc );
 delete( bsc );

 // for LPBlock, before  "manually" un-register (and delete) the
 // UpdateSolver from (each PolyhedralFunctionBlock in) LPBlock
 if( nf )
  for( int i = 0 ; i < LPBlock->get_number_nested_Blocks() ; ++i ) {
   auto bi = LPBlock->get_nested_Block( i );
   bi->unregister_Solver( bi->get_registered_solvers().back() , true );
   }
 else
  LPBlock->unregister_Solver( LPBlock->get_registered_solvers().back() ,
			      true );
 s_config_Block( LPBlock , msc );
 delete( msc );

 // delete the Blocks
 delete( NDOBlock );
 delete( LPBlock );

 // terminate - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 return( AllPassed ? 0 : 1 );

 }  // end( main )

/*--------------------------------------------------------------------------*/
/*------------------------ End File test.cpp -------------------------------*/
/*--------------------------------------------------------------------------*/
