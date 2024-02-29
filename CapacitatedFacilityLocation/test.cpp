/*--------------------------------------------------------------------------*/
/*-------------------------- File test.cpp ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Main for testing CapacitatedFacilityLocationBlock
 *
 * A CapacitatedFacilityLocationBlock instance is loaded from a text file,
 * then a R3Block (possibly, but not necessarily, a copy) is created.
 *
 * Two Solver are registered to the two Block and computed().
 *
 * According to the value of one parameter, this is all: the Solver are
 * supposed to be exact and the optimal values are compared.
 *
 * Otherwise, the Solver attached to the R3Block is supposed to be solving
 * some kind of continuous relaxation and a certain number of rounds of a
 * simple slope scaling heuristic are ran on that Block, then the results
 * (both lower and upper bound) are compared with the optimal value of the
 * other Solver (not expecting they be equal).
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

#define USECOLORS 1
#if( USECOLORS )
 #define RED( x ) "\x1B[31m" #x "\033[0m"
 #define GREEN( x ) "\x1B[32m" #x "\033[0m"
#else
 #define RED( x ) #x
 #define GREEN( x ) #x
#endif

/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <fstream>
#include <sstream>
#include <iomanip>

#include <random>

#include "BlockSolverConfig.h"

#include "CDASolver.h"

#include "UpdateSolver.h"

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
Block * B2;                             // the R3Block

Index m;  // number of facilities
Index n;  // number of customers

Configuration * r3bc;  // the R3Block Configuration

Index niter = 0;  // how many iterations of Slope Scaling have to be done

std::mt19937 rg;               // base random generator
std::uniform_real_distribution<> dis( 0.0 , 1.0 );

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

template< class T >
static void Str2Sthg( const char* const str , T &sthg )
{
 istringstream( str ) >> sthg;
 }

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
// modified numbers, on average, of the the same order of magnitude of the
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

static Subset GenerateRand( Index m , Index k , bool ord = true )
{
 // generate a (sorted) random k-vector of unique integers in 0 ... m - 1
 if( k > m ) {
  cerr << "error: GenerateRand( " << m << " , " << k << " )" << endl;
  exit( 1 );
  }

 Subset rnd( m );
 std::iota( rnd.begin() , rnd.end() , 0 );
 std::shuffle( rnd.begin() , rnd.end() , rg );
 rnd.resize( k );
 if( ord )
  sort( rnd.begin() , rnd.end() );

 return( rnd );
 }

/*--------------------------------------------------------------------------*/
// adds k to all the elements of sbst ("shift the names by k")

static void SShift( Subset & sbst , Index k )
{
 for( auto & el : sbst )
  el += k;
 }

/*--------------------------------------------------------------------------*/

static void PrintResults( int rtrn , double fo )
{
 if( rtrn == Solver::kInfeasible )
  cout << "    Unfeas";
 else
  cout << fo;
 }

/*--------------------------------------------------------------------------*/

static bool SolveBoth( void ) 
{
 try {
  // solve with the 1st Solver- - - - - - - - - - - - - - - - - - - - - - - -
  //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  auto Slvr1 = B1->get_registered_solvers().front();
  #if DETACH_1ST
   B1->unregister_Solver( Slvr1 );
   B1->register_Solver( Slvr1 , true );  // push it to the front
  #endif

  #if( LOG_LEVEL >= 1 )
   auto start = std::chrono::system_clock::now();
  #endif

  int rtrn1st = Slvr1->compute( false );
  double fo1st = Slvr1->get_lb();  // only compare lower bounds

  #if( LOG_LEVEL >= 1 )
   auto end = std::chrono::system_clock::now();
   std::chrono::duration< double > elapsed = end - start;
   cout.setf( ios::scientific, ios::floatfield );
   cout << setprecision( 2 ) << elapsed.count();
  #endif

  // solve with the 2nd Solver- - - - - - - - - - - - - - - - - - - - - - - -
  //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  auto Slvr2 = B2->get_registered_solvers().front();
  #if DETACH_2ND
   B2->unregister_Solver( Slvr2 );
   B2->register_Solver( Slvr2 );  // push it to the back
  #endif

  #if( LOG_LEVEL >= 1 )
   start = std::chrono::system_clock::now();
  #endif

  int rtrn2nd = Slvr2->compute( false );

  if( ! niter ) {  // solve once and compare results- - - - - - - - - - - - -

   double fo2nd = Slvr2->get_lb();  // only compare lower bounds

   #if( LOG_LEVEL >= 1 )
    end = std::chrono::system_clock::now();
    elapsed = end - start;
    cout.setf( ios::scientific, ios::floatfield );
    cout << setprecision( 2 ) << " - " << elapsed.count();
   #endif

   if( abs( fo1st - fo2nd ) <
       1e-5 *  max( double( 1 ) , max( abs( fo1st ) , abs( fo2nd ) ) ) ) {
    LOG1( " - OK(f)" << endl );
    return( true );
    }

   if( ( rtrn1st == Solver::kInfeasible ) &&
       ( rtrn2nd == Solver::kInfeasible ) ) {
    LOG1( " - OK(e)" << endl );
    return( true );
    }

   #if( LOG_LEVEL >= 1 )
    cout << " - " << setprecision( 7 );
    PrintResults( rtrn1st , fo1st );
    cout << " - ";
    PrintResults( rtrn2nd , fo2nd );
    cout << endl;
   #endif

   return( false );
   }
  else {  // run the Slope Scaling- - - - - - - - - - - - - - - - - - - - - -

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

    Slvr2->get_var_solution();           // get the full solution in B2

    B1->map_back_solution( B2 , r3bc );  // map it all back in B1

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
     B1->chg_facility_costs( F.begin() , Block::INFRange , iM , iM );
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

    // change the facility costs in B1: thanks to the UpdateSolver (whose
    // Modification are *not* inhibited) this is immediately forwarded to B2
    B1->chg_facility_costs( NF.begin() );

    #if DETACH_2ND
     B2->unregister_Solver( Slvr2 );
     B2->register_Solver( Slvr2 );  // push it to the back
    #endif
    rtrn2nd = Slvr2->compute( false );  // solve again and iterate
 
    }  // end( Slope Scaling loop )

   // deinhibit Modification in the first Solver
   Slvr1->inhibit_Modification( false );

   #if( LOG_LEVEL >= 1 )
    end = std::chrono::system_clock::now();
    elapsed = end - start;
    cout.setf( ios::scientific, ios::floatfield );
    cout << setprecision( 2 ) << " - " << elapsed.count() << endl;
   #endif

   return( OK );
   }
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
 // reading command line parameters - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 assert( SKIP_BEAT >= 0 );

 char filetype = 'C';  // type of the input file;
 long int seed = 1;
 unsigned int wchg = 31;
 double p_change = 0.5;
 Index n_change = 10;
 Index n_repeat = 40;

 switch( argc ) {
  case( 9 ): Str2Sthg( argv[ 8 ] , p_change );
  case( 8 ): Str2Sthg( argv[ 7 ] , n_change );
  case( 7 ): Str2Sthg( argv[ 6 ] , n_repeat );
  case( 6 ): Str2Sthg( argv[ 5 ] , wchg );
  case( 5 ): Str2Sthg( argv[ 4 ] , seed );
  case( 4 ): Str2Sthg( argv[ 3 ] , niter );
  case( 3 ): filetype = argv[ 2 ][ 0 ];
  case( 2 ): break;
  default:   cerr << "Usage: " << argv[ 0 ]
		  << " name [typ niter seed wchg #rounds #chng %chng]"
		  << endl
		  << "      typ = [C], F, L, ignored if name ends in .nc4"
		  << endl 
		  << "      niter: how many Slope Scaling iterations [0]"
		  << endl 
		  << "      seed: seed for the random number generator [1]"
		  << endl 
		  << "      wchg: what to change, coded bit-wise "
		  << endl
		  << "            0 = facility cost, 1 = transportation cost"
 		  << endl
		  << "            2 = capacities, 3 = demands"
 		  << endl
		  << "            4 = close, 5 = re-open, 6 = fix-open fac."
		  << endl
		  << "            7 = change problem type (split/unsplit)"
		  << endl
		  << "            8 (+256) = change abstract representation"
		  << endl
		  << "            9 (+512) = set eps & no negative design"
		  << endl
		  << "      #rounds: number of changing rounds [40]"
		  << endl
		  << "      #chng: average number of elements to change [10]"
		  << endl
		  << "      %chng: probability of any single change [0.5]"
		  << endl;
             return( 1 );
  }

 rg.seed( seed );  // seed the pseudo-random number generator     

 // read the Block- - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 string fn( argv[ 1 ] );
 if( fn.substr( fn.size() - 4 , 4 ) == ".nc4" ) {
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
  B1->Block::load( fn , filetype );
  // why the Block:: should be necessary evades me, but it seems it is
  }

 m = B1->get_NFacilities();  // record number of facilities
 n = B1->get_NCustomers();   // record number of customers

 // read the R3Block Configuration
 r3bc = Configuration::deserialize( "R3BCfg.txt" );

 // make the R3Block
 B2 = B1->get_R3_Block( r3bc );

 auto cfg = Configuration::deserialize( "BPar1.txt" );
 if( BlockConfig * bc = dynamic_cast< BlockConfig * >( cfg ) )
  bc->apply( B1 );
 else {
  cerr << "Error: BPar1.txt does not contain a BlockConfig" << endl;
  exit( 1 );
  }

 // ensure that B1 already has all its sub-Block ready when the Solver
 // is registered, for sub-Block "appearing" during the call to
 // generate_abstract_variables() may confuse them (say, B1 may be
 // lock()-ed but the sub-Block would not be, which creates problems)
 // probably a Block::set_configuration() would be better
 B1->generate_abstract_variables();

 cfg = Configuration::deserialize( "BPar2.txt" );
 if( BlockConfig * bc = dynamic_cast< BlockConfig * >( cfg ) )
  bc->apply( B2 );
 else {
  cerr << "Error: BPar2.txt does not contain a BlockConfig" << endl;
  exit( 1 );
  }

 // see above fow why this is needed
 B2->generate_abstract_variables();

 delete( cfg );

 // attach the Solver to the Blocks - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // do this by reading appropriate BlockSolverConfig from file and
 // apply() them to B1 and B2; note that the BlockSolverConfig are
 // clear()-ed and kept to do the cleanup at the end

 BlockSolverConfig * bsc1;
 {
  auto c = Configuration::deserialize( "BSPar1.txt" );
  bsc1 = dynamic_cast< BlockSolverConfig * >( c );
  
  if( ! bsc1 ) {
   cerr << "Error: BSPar1.txt does not contain a BlockSolverConfig" << endl;
   delete( c );
   exit( 1 );
   }

  bsc1->apply( B1 );
  bsc1->clear();

  if( B1->get_registered_solvers().empty() ) {
   cout << endl << "no Solver registered to B1!" << endl;
   exit( 1 );
   }
  }

 // separately register an UpdateSolver that forwards to B2
 auto US = new UpdateSolver( B2 , r3bc );
 B1->register_Solver( US );

 BlockSolverConfig * bsc2;
 {
  auto c = Configuration::deserialize( "BSPar2.txt" );
  bsc2 = dynamic_cast< BlockSolverConfig * >( c );

  if( ! bsc2 ) {
   cerr << "Error: BSPar2.txt does not contain a BlockSolverConfig" << endl;
   delete( c );
   exit( 1 );
   }

  bsc2->apply( B2 );
  bsc2->clear();

  if( B2->get_registered_solvers().empty() ) {
   cout << endl << "no Solver registered to B2!" << endl;
   exit( 1 );
   }
  }

 // compute and set numerical tolerances in the B2 (MCF)Solver - - - - - - -
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

  auto B2S = B2->get_registered_solvers().front();

  static constexpr double BA = 1e-12;  // base accuracy

  B2S->set_par( Solver::dblAbsAcc ,
		BA * std::max( d_abs , double( 1 ) ) );
  B2S->set_par( CDASolver::dblAAccDSol ,
		BA * std::max( c_abs , double( 1 ) ) ); 
  }

 // open log-file - - - - - - - - - - -  - - - - - - - - - - - - - - - - - -
 //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 #if( LOG_LEVEL >= 2 )
  #if( LOG_ON_COUT )
   ((B1->get_registered_solvers()).front())->set_log( &cout );
   ((B2->get_registered_solvers()).front())->set_log( &cout );
  #else
   ofstream LOGFile1( "log1.txt" , ofstream::out );
   if( ! LOGFile1.is_open() )
    cerr << "Warning: cannot open log file log1.txt" << endl;
   else {
    LOGFile1.setf( ios::scientific, ios::floatfield );
    LOGFile1 << setprecision( 10 );
    ((B1->get_registered_solvers()).front())->set_log( & LOGFile1 );
    }

   ofstream LOGFile2( "log2.txt" , ofstream::out );
   if( ! LOGFile2.is_open() )
    cerr << "Warning: cannot open log file log2.txt" << endl;
   else {
    LOGFile2.setf( ios::scientific, ios::floatfield );
    LOGFile2 << setprecision( 10 );
    ((B2->get_registered_solvers()).front())->set_log( & LOGFile2 );
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
   f.open( "CFL2.txt" );
   B2->print( f , 'C' );
   f.close();
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
       B1->chg_facility_costs( NC.begin() , rng );
      }
     else {                    // in the others do a sparse change
      bool ord = ( dis( rg ) < 0.5 );
      auto sbst = GenerateRand( m , tochange , ord );
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
       B1->chg_facility_costs( NC.begin() , std::move( sbst ) , ord );

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
       B1->chg_transportation_costs( NC.begin() , rng );
      }
     else {                    // in the others do a sparse change
      bool ord = ( dis( rg ) < 0.5 );
      auto sbst = GenerateRand( n * m , tochange , ord );
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
       B1->chg_transportation_costs( NC.begin() , std::move( sbst ) , ord );

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
       B1->chg_facility_capacities( NC.begin() , rng );
      }
     else {                    // in the others do a sparse change
      bool ord = ( dis( rg ) < 0.5 );
      auto sbst = GenerateRand( m , tochange , ord );
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
       B1->chg_facility_capacities( NC.begin() , std::move( sbst ) , ord );

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
      B1->chg_customer_demands( NC.begin() , rng );
      }
     else {                    // in the others do a sparse change
      bool ord = ( dis( rg ) < 0.5 );
      auto sbst = GenerateRand( n , tochange , ord );
      auto NC = rndscale( B1->get_Demands().data() , sbst );
      LOG1( "(s" );
      if( ! ord )
       LOG1( ",u" );

      B1->chg_customer_demands( NC.begin() , std::move( sbst ) , ord );
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
      auto sbst = GenerateRand( m , tochange , ord );
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
      auto sbst = GenerateRand( m , tochange , ord );
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
      auto sbst = GenerateRand( m , tochange , ord );
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
 // for B1 the UpdateSolver was manually registered, so it has to be manually
 // un-registered
 B1->unregister_Solver( US );
 delete( US );

 // apply() the clear()-ed BlockSolverConfig to cleanup Solver
 bsc2->apply( B2 );
 bsc1->apply( B1 );

 // then delete the BlockSolverConfig
 delete( bsc2 );
 delete( bsc1 );

 // finally the Block can be deleted
 delete( B2 );
 delete( B1 );

 // terminate - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 return( AllPassed ? 0 : 1 );

 }  // end( main )

/*--------------------------------------------------------------------------*/
/*------------------------ End File test.cpp -------------------------------*/
/*--------------------------------------------------------------------------*/
