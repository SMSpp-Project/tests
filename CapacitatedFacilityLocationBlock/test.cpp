/*--------------------------------------------------------------------------*/
/*-------------------------- File test.cpp ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Main for testing CapacitatedFacilityLocationBlock
 *
 * A CapacitatedFacilityLocationBlock instance is loaded from a text file,
 * and the pair BlockConfig + BlockSolverConfig of -F [see
 * read_formulations()] is applied to it. Some of the Solver of the
 * BlockSolverConfig may be R3BSolver, each of which solves the Block through
 * an R3Block of it (possibly, but not necessarily, a copy), made, configured
 * and kept up to date with the changes of the original Block from its own
 * ComputeConfig.
 *
 * The Solver of the BlockSolverConfig are registered to the Block and
 * computed().
 *
 * According to the value of one parameter, this is all: the Solver are
 * supposed to be exact and the lower bounds of all of them, those that
 * solve the original Block and those that solve an R3Block, are compared.
 *
 * Otherwise, each R3BSolver is supposed to be solving some kind of
 * continuous relaxation and a certain number of rounds of a simple slope
 * scaling heuristic are ran on its R3Block, then the results (both lower and
 * upper bound) are compared with the optimal value of the first other Solver
 * (not expecting they be equal).
 *
 * This is possibly repeated a number of times in a loop where data of the
 * problem (fixed and transportation costs, demands, capacities) are modified
 * at random.
 *
 * IMPORTANT NOTES:
 *
 * 1) changing demands via the abstract representation in the FF is not
 *    currently supported by CapacitatedFacilityLocationBlock, so it not
 *    attempted here regardless to the setting of bit 7 of the wchg
 *    command-line parameter
 *
 * 2) changing the abstract representation (as dictated by bit 7 of the
 *    wchg command-line parameter) currently ONLY WORKS IF B1 IS IN THE
 *    STANDARD FORMULATION, SAVE FOR FIXING OPEN/CLOSED AND UNFIXING OF
 *    FACILITIES THAT SHOULD WORK IN ALL FORMULATIONS
 *
 * 3) if the Flow Formulation is selected and a MCFSolver is used to solve
 *    it, then it is likely necessary to set the absolute accuracies, since
 *    several MCFSolver won't work properly without properly setting the
 *    numerical tolerances EpsFlw and EpsCst. Furthermore, an issue with the
 *    MCF R3Block is that the corresponding relaxation is *not* equivalent to
 *    the continuous one if the facilities cost are negative. Indeed, in that
 *    relaxation one has
 *
 *      y = \sum_j x_{ij} / D_i
 *
 *    while usually it is
 *
 *      y >= \sum_j x_{ij} / D_i
 *
 *    This means that with a negative facility cost y_i will be put to 1 in
 *    the "normal" formulations, but it will not (unless \sum_j x_{ij} = D_i)
 *    in the MCF relaxation. A specific setting from the command line is
 *    provided to ensure that 1) absolute accuracies are set in the Solver
 *    of the R3B, and 2) no negative facility costs are ever generated. Not
 *    using this setting in the right case may cause tests to fail.
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

#if( LOG_LEVEL >= 1 )
 #define LOG1( x ) cout << x
 #define CLOG1( y , x ) if( y ) cout << x

 #if( LOG_LEVEL >= 2 )
  #define LOG_ON_COUT 1
  // if nonzero, the Solver log are sent on cout rather than on a file
 #endif
#else
 #define LOG1( x )
 #define CLOG1( y , x )
#endif

/*--------------------------------------------------------------------------*/
// if nonzero, the Solver attached to the original
// CapacitatedFacilityLocationBlock is detached and re-attached to it at all
// iterations

#define DETACH_1ST 0

// if nonzero, the Solver attached to the R3Block is detached and re-attached
// to it at all iterations

#define DETACH_2ND 0

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
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <fstream>
#include <sstream>

#include <iomanip>

#include <random>

#include <chrono>

#include "common_utils.h"

#include "CDASolver.h"

#include "R3BSolver.h"

#include "CapacitatedFacilityLocationBlock.h"

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace std;

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*-------------------------------- TYPES -----------------------------------*/
/*--------------------------------------------------------------------------*/

using Index = Block::Index;
using c_Index = Block::c_Index;

using Range = Block::Range;
using c_Range = Block::c_Range;

using Subset = Block::Subset;
using c_Subset = Block::c_Subset;

using FunctionValue = Function::FunctionValue;

/*--------------------------------------------------------------------------*/
/*------------------------------- CONSTANTS --------------------------------*/
/*--------------------------------------------------------------------------*/

static constexpr FunctionValue INF = Inf< FunctionValue >();

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/

CapacitatedFacilityLocationBlock * B1;  // the original Block

Index m;  // number of facilities
Index n;  // number of customers

Index niter = 0;  // how many iterations of Slope Scaling have to be done

double cmp_tol = 1e-5;  // relative tolerance used to compare the two bounds;
                        // 1e-5 by default, but a batch may pass a looser value
                        // (last command-line argument) when the two Solver
                        // legitimately compute *different* bounds (e.g. a
                        // Lagrangian dual vs a cut-strengthened LP)

char filetype = 'C';    // type of the input file
long int & seed = round_opts.seed;        // seed of the random generator
Index & wchg = round_opts.wchg;           // what to change, coded bit-wise
double & p_change = round_opts.p_change;  // probability of any single change
Index & n_change = round_opts.n_change;   // average number of elements to
                                          // change
Index & n_repeat = round_opts.n_repeat;   // number of changing rounds

std::mt19937 rg;               // base random generator
std::uniform_real_distribution<> dis( 0.0 , 1.0 );

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

// return a random number in [ 0.5 , 2 ] so that the probability of being
// p > 1 is the same as the probability of being 1 / p < 1: in this way the
// modified numbers should, on average, retain the same order of magnitude
// of the original ones even after being modified very many times
// more properly, we want a value x \in [ 0.5 , 2 ] such that
//    P( x >= p ) == P( x <= 1 / p ) for all p \in [ 1 , 2 ].
// let us consider the distribution function is F( y ) = P( x <= y ):
// by definition,
//    P( a < x <= b ) == F( b ) - F( a )
// hence, we want
//    P( p <= x <= 2 ) == F( 2 ) - F( p ) ==
//    P( 1 / 2 <= x <= 1 / p ) == F( 1 / p ) - F( 1 / 2 )
// i.e.,
//    F( 2 ) - F( p ) == F( 1 / p ) - F( 1 / 2 )
// furthermore, F( 1 / 2 ) == 0 and F( 2 ) == 1, whence
//    1 - F( p ) == F( 1 / p )
// ... and this I don't seem to be able to solve, so let's hope the
// implementation below works

static double rndfctr( void )
{
 auto val = 2 * dis( rg );
 if( val < 1 )
  val = 1 / ( val + 1 );
 //!! ensure few digits after the point: this may help the MCFSolver that
 //!! have originally been constructed with integers in mind
 val = double( int( val * 1000 ) ) / 1000;

 return( val ); 
 }

/*--------------------------------------------------------------------------*/
// with probability 2 / 3 returns a random number in [ 0.5 , 2 ]  with the
// same property as that of rndfctr() (i.e., the probability of being p > 1
// is the same as the probability of being 1 / p < 1, while with the
// remaining probability 1 / 3 returns a random number in [ - 2 , - 0.5 ]
// with an analogous property; in other words, it tries to keep the
// modified numbers, on average, of the same order of magnitude of the
// original ones even after being modified very many times, while flipping
// their sign once in three calls

static double rndfctrn( void )
{
 auto val = rndfctr();
 return( dis( rg ) < 0.6666 ? val : - val );
 }

/*--------------------------------------------------------------------------*/
// if neg == true, returns vect[ rng ] scaled by random factors in [ -2 , 2 ]
// (using rndfctrn(), see comments there): this means that there is some
// chance that some element of vect[ rng ] is < 0 after the operation even if
// vect[ rng ] was all >= 0 before, and therefore is the "right" operation
// for costs (facility and transporation) since these may end up being < 0,
// e.g., in a Lagrangian approach. But this may have to be controlled for
// design costs in some cases (cf. the comments at the beginning), which is
// why by rather setting neg == false it returns vect[ rng ] scaled by random
// factors in [ 0.5 , 2 ]: (using rndfctr(), see comments there); this
// conserves positivity (if vect[ rng ] >= 0 before, this is still true after),
// and therefore is the "right" operation for demands and capacities that need
// always be >= 0

template< class T >
static vector< T > rndscale( const T * vect , Range rng , bool neg = false )
{
 vector< T > tmp( rng.second - rng.first );
 vect += rng.first;
 if( neg )
  for( auto & el : tmp )
   el = rndfctrn() * (*(vect++));
 else
  for( auto & el : tmp )
   el = rndfctr() * (*(vect++));

 return( tmp );
 }

/*--------------------------------------------------------------------------*/
// if neg == true, returns vect[ sbst ] scaled by random factors in [ -2 , 2 ]
// (using rndfctrn(), see comments there): this means that there is some
// chance that some element of vect[ sbst ] is < 0 after the operation even if
// vect[ sbst ] was all >= 0 before, and therefore is the "right" operation
// for costs (facility and transporation) since these may end up being < 0,
// e.g., in a Lagrangian approach. But this may have to be controlled for
// design costs in some cases (cf. the comments at the beginning), which is
// why by rather setting neg == false it returns vect[ sbst ] scaled by random
// factors in [ 0.5 , 2 ]: (using rndfctr(), see comments there); this
// conserves positivity (if vect[ sbst ] >= 0 before, this is still true
// after), and therefore is the "right" operation for demands and capacities
// that need always be >= 0

template< class T >
static vector< T > rndscale( const T * vect , c_Subset sbst ,
			     bool neg = false )
{
 vector< T > tmp( sbst.size() );
 auto vit = tmp.begin();
 if( neg )
  for( auto i : sbst )
   *(vit++) = rndfctrn() * vect[ i ];
 else
 for( auto i : sbst )
  *(vit++) = rndfctr() * vect[ i ];

 return( tmp );
 }

/*--------------------------------------------------------------------------*/

static LinearFunction * LF( Objective * obj )
{
 return( static_cast< LinearFunction * >( static_cast< FRealObjective *
					  >( obj )->get_function() ) );
 }

/*--------------------------------------------------------------------------*/

static LinearFunction * LF( Constraint * cnst )
{
 return( static_cast< LinearFunction * >( static_cast< FRowConstraint *
					  >( cnst )->get_function() ) );
 }

/*--------------------------------------------------------------------------*/

// adds k to all the elements of sbst ("shift the names by k")

static void SShift( Subset & sbst , Index k )
{
 for( auto & el : sbst )
  el += k;
 }

/*--------------------------------------------------------------------------*/

// helper: decode Solver return code into the "has-solution" flag expected by
// the canonical PrintResults() in common_utils.h

static inline bool has_solution( int rtrn )
{
 return( ( ( rtrn >= Solver::kOK ) && ( rtrn < Solver::kError )
           && ( rtrn != Solver::kUnbounded )
           && ( rtrn != Solver::kInfeasible ) )
         || ( rtrn == Solver::kLowPrecision ) );
 }

/*--------------------------------------------------------------------------*/

// the R3BSolver among the Solver of the original Block, in their order

static std::vector< R3BSolver * > R3B_Solvers( void )
{
 std::vector< R3BSolver * > S;
 for( auto slv : B1->get_registered_solvers() )
  if( auto r3s = dynamic_cast< R3BSolver * >( slv ) )
   S.push_back( r3s );
 return( S );
 }

/*--------------------------------------------------------------------------*/
// with DETACH_2ND, the inner Solver of Slvr2 is detached from its R3Block
// and re-attached to it

#if DETACH_2ND
static void detach_2nd( R3BSolver * Slvr2 )
{
 auto B2 = Slvr2->get_R3B();
 B2->unregister_Solver( Slvr2->get_inner_Solver() );
 B2->register_Solver( Slvr2->get_inner_Solver() );  // push it to the back
 }
#endif

/*--------------------------------------------------------------------------*/

// runs the Slope Scaling with the R3BSolver Slvr2, whose lower bound is the
// first one that is valid; Slvr1 is the Solver of the original Block, which
// returned rtrn1st

static bool SlopeScaling( Solver * Slvr1 , int rtrn1st , R3BSolver * Slvr2 )
{
 #if DETACH_2ND
  detach_2nd( Slvr2 );
 #endif

 #if( LOG_LEVEL >= 1 )
  auto start = std::chrono::system_clock::now();
 #endif

 int rtrn2nd = Slvr2->compute( false );

 // the only way in which a CFL can be infeasible is if the sum of the
 // demands is larger than the sum of the capacities; we assume that this
 // also makes the R3B infeasible right away (anyway, it's either
 // infeasible now or never since the Slope Scaling only changes the
 // objective)
 if( ( rtrn1st == Solver::kInfeasible ) &&
     ( rtrn2nd == Solver::kInfeasible ) ) {
  LOG1( " - OK(e)" << endl );
  return( true );
  }

 // inhibit Modification in the first Solver, since they will all be
 // undone at the end
 Slvr1->inhibit_Modification();

 // the final return value
 bool OK = true;

 // only at the first iteration the lower bound is valid
 auto LB = Slvr2->get_lb();
 vector< FunctionValue > oldFO( niter );  // previous o.f. values
 auto UB = INF;   // best UB value found
 CapacitatedFacilityLocationBlock::CVector F = B1->get_Fixed_Costs();

 for( Index h = 0 ; ; ) {            // Slope Scaling loop
  if( rtrn2nd >= Solver::kError ) {  // error in the Solver
   #if( LOG_LEVEL >= 1 )
    cout << " - Error in B2" << endl;
   #endif
   OK = false;
   break;
   }

  Slvr2->get_var_solution();  // map the solution of the R3Block in B1

  CapacitatedFacilityLocationBlock::CntSolution y( m );  // read y
  B1->get_facility_solution( y.begin() );

  // do slope scaling. The idea is simple: because one expects that
  //
  //    y[ i ] = total warehouse utilization / Q[ i ]
  //
  // can be << 1 in the continuous solution, one can pay a lot less than
  // the true cost of F[ i ] to have flow using warehouse i; this makes for
  // a crappy bound and a huge gap with the rounded integer solution. Then,
  // one takes all the used warehouses (those for which y[ i ] > 0 in the
  // continuous solution, hence y[ i ] = 1 in the rounded one) and modifies
  // their cost so that *that level of warehouse utilization corresponds to
  // paying the full price F[ i ]*. This is simply obtained by setting the
  // cost to F[ i ] / y[ i ]. Because y[ i ] <= 1, this is >= than the
  // "standard" cost F[ i ] / Q[ i ]: hence, warehouses that are "open but
  // little used" are heavily penalized (relatively speaking) w.r.t. those
  // that are "open but used a lot" or "not open at all". Hopefully, this
  // will convince the continuous solution at the next round to avoid the
  // former and more fully using the ones that are used a lot.
  std::vector< bool > yb( m , false );  // meanwhile, round-up y
  auto NF = F;
  for( Index i = 0 ; i < m ; ++i )
   if( y[ i ] > 1e-6 ) {  // open warehouse
    NF[ i ] = F[ i ] / y[ i ];
    yb[ i ] = true;
    }

  // set the rounded y solution in B1 to compute the objective
  B1->set_facility_solution( yb.begin() );

  // at all iterations save the first one, restore the original costs.
  // for all iterations save the last one this is "temporary", just in
  // order to be able to compute the right objective value, so use eNoMod
  // to avoid that the Solver are informed of this. however, for the last
  // iteration this is permanent, and therefore use the standard eNoBlck
  if( h ) {
   auto iM = h < niter - 1 ? eNoMod : eNoBlck;
   B1->chg_facility_costs( Block::MF_dbl_sp( F ) , Block::INFRange , iM , iM );
   }

  // now compute the value of the new feasible solution
  if( auto NUB = B1->get_objective_value() ; NUB < UB )
   UB = NUB;

  auto fo2nd = Slvr2->get_var_value();  // get objective value

  #if( LOG_LEVEL >= 1 )
   // a few printouts
   cout << endl << h << ": relaxation = " << setprecision( 8 )
	  << fo2nd << ", heuristic = " << setprecision( 8 ) << UB
	  << ", gap = " << setprecision( 2 ) << ( UB - LB ) / LB;
  #endif

  // look back: if you find the same value of the relaxation the
  // algorithm is likely cycling, so force a stop
  if( any_of( oldFO.begin() , oldFO.begin() + h ,
		[ fo2nd ]( auto old ) {
		 return( abs( fo2nd - old ) <=
			 1e-6 * max( fo2nd , double( 1 ) ) );
		 } ) )
   break;

  oldFO[ h ] = fo2nd;  // record back value for later

  if( ++h >= niter )  // all attempts expended
   break;             // done

  // change the facility costs in B1: R3BSolver (whose Modification are
  // *not* inhibited) immediately forwards this to its R3Block
  B1->chg_facility_costs( Block::MF_dbl_sp( NF ) );

  #if DETACH_2ND
   detach_2nd( Slvr2 );
  #endif
  rtrn2nd = Slvr2->compute( false );  // solve again and iterate
 
  }  // end( Slope Scaling loop )

 // deinhibit Modification in the first Solver
 Slvr1->inhibit_Modification( false );

 #if( LOG_LEVEL >= 1 )
  auto end = std::chrono::system_clock::now();
  std::chrono::duration< double > elapsed = end - start;
  cout.setf( ios::scientific, ios::floatfield );
  cout << setprecision( 2 ) << " - " << elapsed.count() << endl;
 #endif

 return( OK );
 }

/*--------------------------------------------------------------------------*/

// solves the original Block with all its Solver, those that solve an
// R3Block of it among them: with no Slope Scaling, the lower bounds that all
// of them give are bounds on the same optimum and are compared with each
// other, otherwise the Slope Scaling is run with each R3BSolver in turn

static bool SolveBoth( void )
{
 try {
  #if DETACH_1ST
   {
    auto s1 = B1->get_registered_solvers().front();
    B1->unregister_Solver( s1 );
    B1->register_Solver( s1 , true );  // push it to the front
    }
  #endif

  // the Solver, in the order of the comparison
  const auto & RS = B1->get_registered_solvers();
  std::vector< Solver * > S( RS.begin() , RS.end() );

  if( niter ) {  // the Slope Scaling- - - - - - - - - - - - - - - - - - - -
   // those that do not solve an R3Block are computed, the first giving the
   // status that the Slope Scaling is compared with
   std::vector< Solver * > S1;
   for( auto slv : S )
    if( ! dynamic_cast< R3BSolver * >( slv ) )
     S1.push_back( slv );
   if( S1.empty() ) {
    cerr << "Error: the Slope Scaling needs a Solver of the original Block "
	 << "that is not an R3BSolver" << endl;
    exit( 1 );
    }

   int rtrn1st = Solver::kError;
   for( std::size_t k = 0 ; k < S1.size() ; ++k ) {
    const int r = S1[ k ]->compute( false );
    if( ! k )
     rtrn1st = r;
    }

   bool OK = true;
   for( auto r3s : R3B_Solvers() )
    OK &= SlopeScaling( S1.front() , rtrn1st , r3s );
   return( OK );
   }

  // solve once and compare lower bounds- - - - - - - - - - - - - - - - - - -

  // each bound is an exact lower bound that must agree with the others
  // (cmp_tol); the verdict and the uniform per-instance line are those of
  // common_utils
  const std::size_t M = S.size();
  std::vector< SolverReading > rd( M );
  std::vector< bool > hs( M );
  std::vector< int > status( M );
  std::vector< std::string > tok( M );
  for( std::size_t k = 0 ; k < M ; ++k ) {
   status[ k ] = S[ k ]->compute( false );
   hs[ k ] = has_solution( status[ k ] );
   if( hs[ k ] ) {
    rd[ k ] = SolverReading::exact( S[ k ]->get_lb() , eps_of( k , S[ k ] ) );
    tok[ k ] = reading_token( rd[ k ] );
    }
   else if( status[ k ] == Solver::kInfeasible )  tok[ k ] = "Unfeas";
   else if( status[ k ] == Solver::kUnbounded )   tok[ k ] = "Unbounded";
   else                                           tok[ k ] = "Error!";
   }

  std::string verdict;
  double diff;
  bool ok = cross_check( rd , hs , status ,
                         std::numeric_limits< double >::quiet_NaN() ,
                         cmp_tol , verdict , diff );
  print_instance_line( std::vector< double >( M , 0.0 ) , tok ,
                       std::numeric_limits< double >::quiet_NaN() ,
                       verdict );
  return( ok );
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

// test-specific command-line knobs, set by process_specific_arg(); the
// formulations (-F) are those of common_utils

static bool process_specific_arg( int opt )
{
 if( process_round_option( opt ) )
  return( true );
 switch( opt ) {
  case( 't' ): filetype = optarg[ 0 ];           return( true );
  case( 's' ): Str2Sthg( optarg , niter );       return( true );
  case( 'T' ): Str2Sthg( optarg , cmp_tol );     return( true );
  default:                                       return( false );
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

 docopt_desc = "SMS++ CapacitatedFacilityLocationBlock test.\n";
 round_opts = { 1 , 31 , 40 , 10 , 0.5 };
 add_round_options(
  "                                    0 facility cost, 1 transport\n"
  "                                    cost, 2 capacities, 3 demands,\n"
  "                                    4 close, 5 re-open, 6 fix-open\n"
  "                                    facilities, 7 split/unsplit,\n"
  "                                    8 (+256) abstract\n"
  "                                    representation, 9 (+512) set\n"
  "                                    eps & no negative design\n" );
 short_opts += "t:s:T:";
 const std::vector< option > my_opts = {
   { "type"   , required_argument , nullptr , 't' } ,
   { "slope"  , required_argument , nullptr , 's' } ,
   { "cmptol" , required_argument , nullptr , 'T' } };
 long_opts.insert( std::prev( long_opts.end() ) ,
                   my_opts.begin() , my_opts.end() );
 help += "  -t, --type <c>                  type of the instance file, C, F\n"
         "                                  or L, ignored for a .nc4 [C]\n"
         "  -s, --slope <n>                 Slope Scaling iterations [0]\n"
         "  -T, --cmptol <x>                relative tolerance of the\n"
         "                                  comparison of the bounds [1e-5]\n"
         "\n"
         "The formulation (-F) is the original Block, a pair of its\n"
         "BlockConfig and BlockSolverConfig; an R3Block of it is solved by\n"
         "an R3BSolver of the BlockSolverConfig.\n";

 process_args( argc , argv , process_specific_arg );

 if( fconf_file.empty() ) {
  cerr << "Error: the formulations must be given (did you forget -F?)" << endl;
  exit( 1 );
  }

 rg.seed( seed );  // seed the pseudo-random number generator

 // the formulation: the original Block - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 auto vl = read_formulations( fconf_file );
 auto & V = vl->f_value;
 auto b1p = dynamic_cast< SimpleConfiguration< std::pair< Configuration * ,
                                       Configuration * > > * >( V.front() );
 if( ( V.size() != 1 ) || ( ! b1p ) || ( ! b1p->f_value.second ) ) {
  cerr << "Error: " << fconf_file << " must list the original Block only, a "
       << "pair BlockConfig + BlockSolverConfig" << endl;
  exit( 1 );
  }

 // read the Block- - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 string fn( filename );
 if( ( fn.size() >= 4 ) && ( fn.substr( fn.size() - 4 , 4 ) == ".nc4" ) ) {
  B1 = dynamic_cast< CapacitatedFacilityLocationBlock * >(
					         Block::deserialize( fn ) );
  if( ! B1 ) {
   cerr << "Error: " << fn
	<< " does not contain a CapacitatedFacilityLocationBlock" << endl;
   return( 1 );
   }
  }
 else {
  B1 = new CapacitatedFacilityLocationBlock;
  load_Block_or_exit( B1 , fn , filetype );
  }

 m = B1->get_NFacilities();  // record number of facilities
 n = B1->get_NCustomers();   // record number of customers

 b_config_Block( B1 , b1p->f_value.first , fconf_file );

 // ensure that B1 already has all its sub-Block ready when the Solver
 // is registered, for sub-Block "appearing" during the call to
 // generate_abstract_variables() may confuse them (say, B1 may be
 // lock()-ed but the sub-Block would not be, which creates problems)
 // probably a Block::set_configuration() would be better
 B1->generate_abstract_variables();

 // attach the Solver to the Block- - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // do this by applying a clone() of the BlockSolverConfig; note that it is
 // clear()-ed and kept to do the cleanup at the end; each R3BSolver makes
 // its R3Block when it is attached

 auto as_bsc = []( Configuration * c ) {
  auto bsc = dynamic_cast< BlockSolverConfig * >( c->clone() );
  if( ! bsc ) {
   cerr << "Error: " << fconf_file << " has a BlockSolverConfig that is not"
        << endl;
   exit( 1 );
   }
  return( bsc );
  };

 BlockSolverConfig * bsc1 = as_bsc( b1p->f_value.second );

 // the Solver this build does not have are left out, with a warning
 s_config_Block( B1 , bsc1 , fconf_file );

 if( B1->get_registered_solvers().empty() ) {
  cout << endl << "no Solver registered to B1!" << endl;
  exit( 1 );
  }

 const auto R3S = R3B_Solvers();

 // compute and set numerical tolerances in the Solver of the R3Block - - - -
 //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 if( wchg & 512 ) {
  auto TC = B1->get_Transportation_Costs().data();
  auto c_abs = std::abs( *std::max_element( TC , TC + n * m ,
					    []( auto a , auto b ) {
					     return( std::abs( a ) <
						     std::abs( b ) ); } ) );
  auto & CD = B1->get_Demands();
  auto d_abs = std::abs( *std::max_element( CD.begin() , CD.end() ,
					    []( auto a , auto b ) {
					     return( std::abs( a ) <
						     std::abs( b ) ); } ) );
  for( Index i = 0 ; i < m ; ++i ) {
   auto fi = std::abs( B1->get_Fixed_Cost( i ) );
   auto ci = std::abs( B1->get_Capacity( i ) );
   if( fi > d_abs )
    d_abs = fi;
   if( fi / ci > c_abs )
    c_abs = fi / ci;
   }

  static constexpr double BA = 1e-12;  // base accuracy

  // every Solver of every R3Block, all of them being compared
  for( auto r3s : R3S )
   for( auto B2S : r3s->get_R3B()->get_registered_solvers() ) {
    B2S->set_par( Solver::dblAbsAcc ,
		  BA * std::max( d_abs , double( 1 ) ) );
    B2S->set_par( CDASolver::dblAAccDSol ,
		  BA * std::max( c_abs , double( 1 ) ) );
    }
  }

 // open log-file - - - - - - - - - - -  - - - - - - - - - - - - - - - - - -
 //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 #if( LOG_LEVEL >= 2 )
  #if( LOG_ON_COUT )
   ( ( B1->get_registered_solvers() ).front() )->set_log( &cout );
   if( ! R3S.empty() )
    R3S.front()->get_inner_Solver()->set_log( &cout );
  #else
   ofstream LOGFile1( "log1.txt" , ofstream::out );
   if( ! LOGFile1.is_open() )
    cerr << "Warning: cannot open log file log1.txt" << endl;
   else {
    LOGFile1.setf( ios::scientific, ios::floatfield );
    LOGFile1 << setprecision( 10 );
    ( ( B1->get_registered_solvers() ).front() )->set_log( & LOGFile1 );
    }

   ofstream LOGFile2( "log2.txt" , ofstream::out );
   if( ! LOGFile2.is_open() )
    cerr << "Warning: cannot open log file log2.txt" << endl;
   else
    if( ! R3S.empty() ) {
     LOGFile2.setf( ios::scientific, ios::floatfield );
     LOGFile2 << setprecision( 10 );
     R3S.front()->get_inner_Solver()->set_log( & LOGFile2 );
     }
  #endif
 #endif

 // first solver call - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 bool AllPassed = SolveBoth();

 // main loop - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // now, for n_repeat times:
 //
 // - up to n_change facility costs are changed by multiplying the current
 //   ones by a random number in [ 0.5 , 2 ]
 //
 // - up to n_change transportation costs are changed by multiplying the
 //   current ones by a random number in [ 0.5 , 2 ]
 //
 // - up to n_change facility capacities are changed by multiplying the
 //   current ones by a random number in [ 0.5 , 2 ]; note that the
 //   aggregate capacity should remain more or less the same, which should
 //   help in keeping the CFL instance feasible
 //
 // - up to n_change customer demands are changed by multiplying the
 //   current ones by a random number in [ 0.5 , 2 ]; note that the
 //   aggregate demand should remain more or less the same, which should
 //   help in keeping the CFL instance feasible
 //
 // - up to n_change facilities are closed
 //
 // - up to n_change facilities are re-opened (made unfixed-open)
 //
 // - up to n_change facilities are fixed-open
 //
 // - in 33% of the case, the problem type (split/unsplit) is randomly set
 //
 // then the B1 and B2 are re-solved with their Solver
 //
 // IMPORTANT NOTE: changing the abstract representation currently
 //                 ONLY WORKS IF B1 IS IN THE STANDARD FORMULATION

 for( Index rep = 0 ; rep < n_repeat * ( SKIP_BEAT + 1 ) ; ) {
  LOG1( rep << ": ");

  if( ! AllPassed ) {
   std::ofstream f( "CFL1.txt" );
   B1->print( f , 'C' );
   f.close();
   for( std::size_t j = 0 ; j < R3S.size() ; ++j ) {
    f.open( "CFL" + std::to_string( j + 2 ) + ".txt" );
    R3S[ j ]->get_R3B()->print( f , 'C' );
    f.close();
    }
   break;
   }

  // change facilities costs- - - - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 1 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = Index( dis( rg ) * min( m , n_change ) ) ) {
    LOG1( "changed " << tochange << " f-costs" );

    if( tochange == 1 ) {     // change a single element
     auto i = Index( dis( rg ) * ( m - 1 ) );
     auto NC = B1->get_Fixed_Cost( i );
     if( wchg & 512 )
      NC *= rndfctr();   // ensure no negative fixed costs
     else
      NC *= rndfctrn();  // negative fixed costs alowed

    if( ( wchg & 256 ) && ( dis( rg ) < 0.5 ) ) {
     // change via abstract representation
     LOG1( "(a)" );
     LF( B1->get_objective() )->modify_coefficient( i , NC );
     }
    else  // change via call to chg_* method
     B1->chg_facility_cost( NC , i );
     }
    else
     if( dis( rg ) <= 0.5 ) {  // in 50% of the cases do a ranged change
      Range rng;
      rng.first = dis( rg ) * ( m - tochange );
      rng.second = rng.first  + tochange;
      auto NC = rndscale( B1->get_Fixed_Costs().data() , rng ,
			  ! ( wchg & 512 ) );

      if( ( wchg & 256 ) && ( dis( rg ) < 0.5 ) ) {
       // change via abstract representation
       LOG1( "(a)" );
       LF( B1->get_objective() )->modify_coefficients( std::move( NC ) , rng );
       }
      else  // change via call to chg_* method
       B1->chg_facility_costs( Block::MF_dbl_sp( NC ) , rng );
      }
     else {                    // in the others do a sparse change
      bool ord = ( dis( rg ) < 0.5 );
      auto sbst = GenerateRand( m , tochange , rg , ord );
      auto NC = rndscale( B1->get_Fixed_Costs().data() , sbst ,
			  ! ( wchg & 512 ) );
      LOG1( "(s" );
      if( ! ord )
       LOG1( ",u" );

      if( ( wchg & 256 ) && ( dis( rg ) < 0.5 ) ) {
       // change via abstract representation
       LOG1( ",a" );
       LF( B1->get_objective() )->modify_coefficients( std::move( NC ) ,
					      std::move( sbst ) , ord );
       }
      else  // change via call to chg_* method
       B1->chg_facility_costs( Block::MF_dbl_sp( NC ) , std::move( sbst ) ,
                               ord );

      LOG1( ")" );
      }

    LOG1( " - " );
    }

  // change transportation costs- - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 2 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = Index( dis( rg ) * min( n * m , n_change ) ) ) {
    LOG1( "changed " << tochange << " t-costs" );

    if( tochange == 1 ) {     // change a single element
     auto i = Index( dis( rg ) * ( n * m - 1 ) );
     auto NC = rndfctr() * B1->get_Transportation_Cost( i / n , i % n );

     if( ( wchg & 256 ) && ( dis( rg ) < 0.5 ) ) {
      // change via abstract representation
      LOG1( "(a)" );
      LF( B1->get_objective() )->modify_coefficient( m + i , NC );
      }
     else  // change via call to chg_* method
      B1->chg_transportation_cost( NC , i );
     }
    else
     if( dis( rg ) <= 0.5 ) {  // in 50% of the cases do a ranged change
      Range rng;
      rng.first = dis( rg ) * ( n * m - tochange );
      rng.second = rng.first  + tochange;
      auto NC = rndscale( B1->get_Transportation_Costs().data() , rng , true );

      if( ( wchg & 256 ) && ( dis( rg ) < 0.5 ) ) {
       // change via abstract representation
       LOG1( "(a)" );
       rng.first += m;
       rng.second += m;
       LF( B1->get_objective() )->modify_coefficients( std::move( NC ) , rng );
       }
      else  // change via call to chg_* method
       B1->chg_transportation_costs( Block::MF_dbl_sp( NC ) , rng );
      }
     else {                    // in the others do a sparse change
      bool ord = ( dis( rg ) < 0.5 );
      auto sbst = GenerateRand( n * m , tochange , rg , ord );
      auto NC = rndscale( B1->get_Transportation_Costs().data() , sbst ,
			  true );
      LOG1( "(s" );
      if( ! ord )
       LOG1( ",u" );

      if( ( wchg & 256 ) && ( dis( rg ) < 0.5 ) ) {
       // change via abstract representation
       LOG1( ",a" );
       SShift( sbst , m );
       LF( B1->get_objective() )->modify_coefficients( std::move( NC ) ,
						       std::move( sbst ) ,
						       ord );
       }
      else  // change via call to chg_* method
       B1->chg_transportation_costs( Block::MF_dbl_sp( NC ) ,
                                     std::move( sbst ) , ord );

      LOG1( ")" );
      }

    LOG1( " - " );
    }

  // change facilities capacities - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 4 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = Index( dis( rg ) * min( m , n_change ) ) ) {
    LOG1( "changed " << tochange << " capacities" );

    std::vector< FRowConstraint > * cap = nullptr;
    ModParam iM = eModBlck;
    if( ( wchg & 256 ) && ( dis( rg ) < 0.5 ) ) {
     cap = B1->get_static_constraint_v< FRowConstraint >( "cap" );
     if( tochange > 1 )
      iM = Observer::make_par( iM , B1->open_channel() );
     }
    
    if( tochange == 1 ) {     // change a single element
     auto i = Index( dis( rg ) * ( m - 1 ) );
     auto NC = rndfctr() * B1->get_Capacity( i );

     if( cap ) {  // change via abstract representation
      LOG1( "(a)" );
      LF( & (*cap)[ i ] )->modify_coefficient( n , - NC );
      }
     else         // change via call to chg_* method
      B1->chg_facility_capacity( NC , i );
     }
    else 
     if( dis( rg ) <= 0.5 ) {  // in 50% of the cases do a ranged change
      Range rng;
      rng.first = dis( rg ) * ( m - tochange );
      rng.second = rng.first  + tochange;
      auto NC = rndscale( B1->get_Capacities().data() , rng );

      if( cap ) {  // change via abstract representation
       LOG1( "(a)" );
       auto NCit = NC.begin();
       for( Index i = rng.first ; i < rng.second ; ++i )
	LF( & (*cap)[ i ] )->modify_coefficient( n , - *(NCit++) , iM );
       }
      else         // change via call to chg_* method
       B1->chg_facility_capacities( Block::MF_dbl_sp( NC ) , rng );
      }
     else {                    // in the others do a sparse change
      bool ord = ( dis( rg ) < 0.5 );
      auto sbst = GenerateRand( m , tochange , rg , ord );
      auto NC = rndscale( B1->get_Capacities().data() , sbst );
      LOG1( "(s" );
      if( ! ord )
       LOG1( ",u" );

      if( cap ) {  // change via abstract representation
       LOG1( ",a" );
       auto NCit = NC.begin();
       for( Index i : sbst )
	LF( & (*cap)[ i ] )->modify_coefficient( n , - *(NCit++) , iM );
       }
      else       // change via call to chg_* method
       B1->chg_facility_capacities( Block::MF_dbl_sp( NC ) ,
                                    std::move( sbst ) , ord );

      LOG1( ")" );
      }

    if( auto chnl = Observer::par2chnl( iM ) )  // a channel had been opened
     B1->close_channel( chnl );       // close it now

    LOG1( " - " );
    }

  // change customer demands- - - - - - - - - - - - - - - - - - - - - - - - -
  // note: changing demands via the abstract representation in the FF is not
  //       currently supported by CapacitatedFacilityLocationBlock, so it
  //       not attempted here

  if( ( wchg & 8 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = Index( dis( rg ) * min( n , n_change ) ) ) {
    LOG1( "changed " << tochange << " demands" );

    if( tochange == 1 ) {     // change a single element
     auto j = Index( dis( rg ) * ( n - 1 ) );
     auto ND = rndfctr() * B1->get_Demand( j );
     B1->chg_customer_demand( ND , j );
     }
    else
     if( dis( rg ) <= 0.5 ) {  // in 50% of the cases do a ranged change
      Range rng;
      rng.first = dis( rg ) * ( n - tochange );
      rng.second = rng.first  + tochange;
      auto NC = rndscale( B1->get_Demands().data() , rng );
      B1->chg_customer_demands( Block::MF_dbl_sp( NC ) , rng );
      }
     else {                    // in the others do a sparse change
      bool ord = ( dis( rg ) < 0.5 );
      auto sbst = GenerateRand( n , tochange , rg , ord );
      auto NC = rndscale( B1->get_Demands().data() , sbst );
      LOG1( "(s" );
      if( ! ord )
       LOG1( ",u" );

      B1->chg_customer_demands( Block::MF_dbl_sp( NC ) , std::move( sbst ) ,
                                ord );
      LOG1( ")" );
      }

    LOG1( " - " );
    }

  // close facilities - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 16 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = Index( dis( rg ) * min( m , n_change ) ) ) {
    LOG1( "closed " << tochange << " facilities" );

    ModParam iM = eModBlck;
    bool abs = false;
    if( ( wchg & 256 ) && ( tochange > 1 ) && ( dis( rg ) < 0.5 ) ) {
     abs = true;
     iM = Observer::make_par( iM , B1->open_channel() );
     }

    if( tochange == 1 ) {     // change a single element
     auto i = Index( dis( rg ) * ( m - 1 ) );

     if( abs ) {
      // change via abstract representation
      LOG1( "(a)" );
      auto yi = B1->get_y( i );
      if( ! yi->is_fixed() ) {
       yi->set_value( 0 );
       yi->is_fixed( true );
       }
      }
     else  // change via call to method
      B1->close_facility( i );
     }
    else
     if( dis( rg ) <= 0.5 ) {  // in 50% of the cases do a ranged change
      Range rng;
      rng.first = dis( rg ) * ( m - tochange );
      rng.second = rng.first  + tochange;

      if( abs ) {  // change via abstract representation
       LOG1( "(a)" );
       for( Index i = rng.first ; i < rng.second ; ++i ) {
	auto yi = B1->get_y( i );
	if( ! yi->is_fixed() ) {
	 yi->set_value( 0 );
	 yi->is_fixed( true , iM );
         }
        }
       }
      else       // change via call to method
       B1->close_facilities( rng );
      }
     else {                    // in the others do a sparse change
      bool ord = ( dis( rg ) < 0.5 );
      auto sbst = GenerateRand( m , tochange , rg , ord );
      LOG1( "(s" );
      if( ! ord )
       LOG1( ",u" );

      if( abs ) {  // change via abstract representation
       LOG1( ",a" );
       for( Index i : sbst ) {
	auto yi = B1->get_y( i );
	if( ! yi->is_fixed() ) {
	 yi->set_value( 0 );
	 yi->is_fixed( true , iM );
         }
        }
       }
      else     // change via call to chg_* method
       B1->close_facilities( std::move( sbst ) , ord );

      LOG1( ")" );
      }

    if( auto chnl = Observer::par2chnl( iM ) )  // a channel had been opened
     B1->close_channel( chnl );       // close it now

    LOG1( " - " );
    }

  // open facilities- - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 32 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = Index( dis( rg ) * min( m , n_change ) ) ) {
    LOG1( "opened " << tochange << " facilities" );

    ModParam iM = eModBlck;
    bool abs = false;
    if( ( wchg & 256 ) && ( tochange > 1 ) && ( dis( rg ) < 0.5 ) ) {
     abs = true;
     iM = Observer::make_par( iM , B1->open_channel() );
     }

    if( tochange == 1 ) {     // change a single element
     auto i = Index( dis( rg ) * ( m - 1 ) );

     if( abs ) {  // change via abstract representation
      LOG1( "(a)" );
      B1->get_y( i )->is_fixed( false );
      }
     else  // change via call to method
      B1->open_facility( i );
     }
    else
     if( dis( rg ) <= 0.5 ) {  // in 50% of the cases do a ranged change
      Range rng;
      rng.first = dis( rg ) * ( m - tochange );
      rng.second = rng.first  + tochange;

      if( abs ) {  // change via abstract representation
       LOG1( "(a)" );
       for( Index i = rng.first ; i < rng.second ; ++i )
	B1->get_y( i )->is_fixed( false , iM );
       }
      else       // change via call to method
       B1->open_facilities( rng );
      }
     else {                    // in the others do a sparse change
      bool ord = ( dis( rg ) < 0.5 );
      auto sbst = GenerateRand( m , tochange , rg , ord );
      LOG1( "(s" );
      if( ! ord )
       LOG1( ",u" );

      if( abs ) {  // change via abstract representation
       LOG1( ",a" );
       for( Index i : sbst )
	B1->get_y( i )->is_fixed( false , iM );
       }
      else     // change via call to chg_* method
       B1->open_facilities( std::move( sbst ) , ord );

      LOG1( ")" );
      }

    if( auto chnl = Observer::par2chnl( iM ) )  // a channel had been opened
     B1->close_channel( chnl );       // close it now

    LOG1( " - " );
    }

  // fix-open facilities- - - - - - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 64 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = Index( dis( rg ) * min( m , n_change ) ) ) {
    LOG1( "fix-open " << tochange << " facilities" );

    ModParam iM = eModBlck;
    bool abs = false;
    if( ( wchg & 256 ) && ( tochange > 1 ) && ( dis( rg ) < 0.5 ) ) {
     abs = true;
     iM = Observer::make_par( iM , B1->open_channel() );
     }

    if( tochange == 1 ) {     // change a single element
     auto i = Index( dis( rg ) * ( m - 1 ) );

     if( abs ) {  // change via abstract representation
      LOG1( "(a)" );
      auto yi = B1->get_y( i );
      if( ! yi->is_fixed() ) {
       yi->set_value( 1 );
       yi->is_fixed( true );
       }
      }
     else  // change via call to method
      B1->fix_open_facility( i );
     }
    else
     if( dis( rg ) <= 0.5 ) {  // in 50% of the cases do a ranged change
      Range rng;
      rng.first = dis( rg ) * ( m - tochange );
      rng.second = rng.first  + tochange;

      if( abs ) {  // change via abstract representation
       LOG1( "(a)" );
       for( Index i = rng.first ; i < rng.second ; ++i ) {
	auto yi = B1->get_y( i );
	if( ! yi->is_fixed() ) {
	 yi->set_value( 1 );
	 yi->is_fixed( true , iM );
         }
        }
       }
      else       // change via call to method
       B1->fix_open_facilities( rng );
      }
     else {                    // in the others do a sparse change
      bool ord = ( dis( rg ) < 0.5 );
      auto sbst = GenerateRand( m , tochange , rg , ord );
      LOG1( "(s" );
      if( ! ord )
       LOG1( ",u" );

      if( abs ) {  // change via abstract representation
       LOG1( ",a" );
       for( Index i : sbst ) {
	auto yi = B1->get_y( i );
	if( ! yi->is_fixed() ) {
	 yi->set_value( 1 );
	 yi->is_fixed( true , iM );
         }
        }
       }
      else     // change via call to chg_* method
       B1->fix_open_facilities( std::move( sbst ) , ord );

      LOG1( ")" );
      }

    if( auto chnl = Observer::par2chnl( iM ) )  // a channel had been opened
     B1->close_channel( chnl );       // close it now

    LOG1( " - " );
    }

  // change problem type (split/unsplit)- - - - - - - - - - - - - - - - - - -
  // this is only supported via the physical interface

  if( ( wchg & 128 ) && ( dis( rg ) <= 0.33 ) ) {
   LOG1( "change problem type ~ " );

   B1->chg_UnSplittable( dis( rg ) < 0.5  );
   }

  // finally, re-solve the problems- - - - - - - - - - - - - - - - - - - - -
  // ... every SKIP_BEAT + 1 rounds

  if( ! ( ++rep % ( SKIP_BEAT + 1 ) ) )
   AllPassed &= SolveBoth();
  #if( LOG_LEVEL >= 1 )
  else
   cout << endl;
  #endif

  }  // end( main loop )- - - - - - - - - - - - - - - - - - - - - - - - - - -
     // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 if( AllPassed )
  cout << GREEN( All tests passed!! ) << endl;
 else
  cout << RED( Shit happened!! ) << endl;
 
 // destroy the Blocks- - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // apply() the clear()-ed BlockSolverConfig to cleanup the Solver, each
 // R3BSolver deleting its R3Block, then delete it and the Block
 bsc1->apply( B1 );
 delete( bsc1 );
 delete( B1 );

 // and the formulation
 delete( vl );

 // terminate - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 return( AllPassed ? 0 : 1 );

 }  // end( main )

/*--------------------------------------------------------------------------*/
/*------------------------ End File test.cpp -------------------------------*/
/*--------------------------------------------------------------------------*/
