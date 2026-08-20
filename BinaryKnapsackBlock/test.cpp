/*--------------------------------------------------------------------------*/
/*--------------------------- File test.cpp --------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Main for testing BinaryKnapsackBlock, comparing the results of all the
 * Solvers attached to it: every exact Solver must agree on the optimal value,
 * every relaxation Solver must bracket it (see CrossCheckSolvers() and batches/batch and batch-mixed
 * for the cross-check of all the mathematically equivalent formulations).
 *
 * \author Federica Di Pasquale \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \author Antonio Frangioni \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \author Donato Meoli \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 */
/*--------------------------------------------------------------------------*/
/*------------------------------ MACROS ------------------------------------*/
/*--------------------------------------------------------------------------*/

#define STEP 3  // after modifications solve again at each multiple of STEP

#ifndef LOG_LEVEL
 #define LOG_LEVEL 0
#endif
// 0 = only pass/fail
// 1 = list of modifications (and per-solve timings)
// 2 = also print verbose header about main configuration at start

#if( LOG_LEVEL > 0 )
 #define LOG( x ) cout << x
#else
 #define LOG( x )
#endif

#define USECOLORS 1
#if( USECOLORS )
 #define RED( x ) "\x1B[31m" #x "\033[0m"
 #define GREEN( x ) "\x1B[32m" #x "\033[0m"
#else
 #define RED( x ) #x
 #define GREEN( x ) #x
#endif

/*--------------------------------------------------------------------------*/
/*----------------------------- INCLUDES -----------------------------------*/
/*--------------------------------------------------------------------------*/

#include "common_utils.h"

#include "BinaryKnapsackBlock.h"

#include <random>

#include <chrono>

#include <cstdlib>

#include <fstream>

/*--------------------------------------------------------------------------*/
/*------------------------------- USING ------------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace std;

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*------------------------------- TYPES ------------------------------------*/
/*--------------------------------------------------------------------------*/

using Index = Block::Index;
using c_Index = Block::c_Index;

using Range = Block::Range;
using c_Range = Block::c_Range;

using Subset = Block::Subset;
using c_Subset = Block::c_Subset;

/*--------------------------------------------------------------------------*/
/*----------------------------- CONSTANTS ----------------------------------*/
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/

BinaryKnapsackBlock * BKB;          // The Binary Knapsack Block

std::mt19937 rg;                       // random generator
std::uniform_real_distribution<> dis( 0.0 , 1.0 );

Index N = 100;                         // number of items

static constexpr Index rangeW = 100;   // range values of weights
static constexpr double rangeP = 100;  // range values of profits

// when have_ref is true (the Pisinger mode, see -C / run_pisinger()),
// CrossCheckSolvers additionally checks the optimum against the published
// reference value ref_opt
bool have_ref = false;
double ref_opt = 0;

/*--------------------------------------------------------------------------*/
/*----------------------------- FUNCTIONS ----------------------------------*/
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
// Generate a random Range of size m < N

Range generateRange( Index m )
{
 Range rng;
 rng.first = dis( rg ) * ( N - m );
 rng.second = rng.first + m;
 return( rng );
 }

/*--------------------------------------------------------------------------*/
// Generate a random Subset of size m < N

Subset generateSubset( Index m )
{
 Subset nms;
 
 Subset idx( N );            
 iota( idx.begin() , idx.end() , 0 );
 
 sample( idx.begin() , idx.end() , back_inserter( nms ) , m , rg );
 
 return( nms );
 }

/*--------------------------------------------------------------------------*/

bool CrossCheckSolvers( void )
{
 // cross-check ALL the registered Solver via the shared common_utils engine:
 // every exact one must agree on the optimal value z*, every relaxation
 // Solver must bracket it, and (in the Pisinger mode, have_ref) z* must match
 // the published optimum ref_opt. The engine also prints the uniform
 // per-instance line with every Solver value; what is added here is the
 // BinaryKnapsackBlock-specific self-consistency check, i.e., that the value
 // each Solver reports is the one its own solution has
 const auto & reg = BKB->get_registered_solvers();
 std::vector< Solver * > Solvers( reg.begin() , reg.end() );
 const std::size_t M = Solvers.size();
 if( M < 2 ) {
  cerr << "Error: CrossCheckSolvers needs at least two registered Solver";
  return( false );
  }

 // the reading of a Solver, its [ get_lb() , get_ub() ] interval and the
 // tolerance it is held to, lives in common_utils; the classifier is invoked
 // by SolveAll() only for the Solver that found a solution, so it is wrapped
 // here to record which ones the self-consistency check below can read a
 // primal x from: the infeasible ones have none, and neither has a Solver
 // that claims no optimum, i.e., the relaxation
 std::vector< char > feasible( M , 0 );
 std::vector< char > bracket( M , 0 );
 SolverClassifier classify =
  [ &feasible , &bracket ]( Solver * s , std::size_t k ) -> SolverReading {
   feasible[ k ] = 1;
   bracket[ k ] = std::isinf( eps_of( k , s ) );  // no optimum, hence no x
   return( read_bounds( s , k ) );
   };

 const double ref = have_ref ? ref_opt
                  : std::numeric_limits< double >::quiet_NaN();
 if( ! SolveAll( BKB , classify , ref , 2e-06 ) )
  return( false );

 // BinaryKnapsackBlock-specific self-consistency: every feasible exact Solver's
 // reported value must equal the value recomputed from its returned solution
 for( std::size_t k = 0 ; k < M ; ++k ) {
  if( ! feasible[ k ] )
   continue;                               // infeasible: no primal x to read
  if( bracket[ k ] )
   continue;                               // relaxations have no primal x
  const double value = Solvers[ k ]->get_var_value();
  Solvers[ k ]->get_var_solution();
  double checksol = 0;
  for( Index i = 0 ; i < N ; ++i )
   checksol += BKB->get_x( i ) * BKB->get_Profit( i );
  if( abs( checksol - value ) > 1e-06 * max( abs( value ) , 1.0 ) ) {
   cerr << "Error: Solver " << k << " solution value " << checksol
        << " != its reported value " << value;
   return( false );
   }
  }

 return( true );
 }

/*--------------------------------------------------------------------------*/
// a Pisinger benchmark instance read from a .csv (see read_pisinger())

struct PisingerInst {
 std::string name;
 Index N;
 double C, z;
 std::vector< double > W, P;
 };

/*--------------------------------------------------------------------------*/
// read every instance of a Pisinger .csv, each a block "knapPI_... / n N /
// c C / z Z / time T / <idx,profit,weight,xstar> lines / -----"

static std::vector< PisingerInst > read_pisinger( const std::string & path )
{
 std::vector< PisingerInst > out;
 std::ifstream in( path );
 std::string line;
 while( std::getline( in , line ) ) {
  if( line.compare( 0 , 6 , "knapPI" ) != 0 )
   continue;
  if( ! line.empty() && line.back() == '\r' )
   line.pop_back();
  PisingerInst pi;
  pi.name = line;
  unsigned nn = 0;
  std::getline( in , line ); std::sscanf( line.c_str() , "n %u" , & nn );
  std::getline( in , line ); std::sscanf( line.c_str() , "c %lf" , & pi.C );
  std::getline( in , line ); std::sscanf( line.c_str() , "z %lf" , & pi.z );
  std::getline( in , line );                      // "time ..."
  pi.N = nn; pi.W.resize( nn ); pi.P.resize( nn );
  for( Index i = 0 ; i < pi.N ; ++i ) {
   std::getline( in , line );
   long idx; double p, w;
   std::sscanf( line.c_str() , "%ld,%lf,%lf" , & idx , & p , & w );
   pi.P[ i ] = p; pi.W[ i ] = w;
   }
  out.push_back( std::move( pi ) );
  }
 return( out );
 }

/*--------------------------------------------------------------------------*/
// the Pisinger mode (-C <csv>): test every instance of the .csv with ALL the
// attached Solver (the CrossCheckSolvers() solver-vs-solver cross-check) AND against
// the published optimum z (solver-vs-reference), reusing one Block and one set
// of Solver across the whole class (the data of each instance is set with the
// chg_*() Modification, to which the Solver react)

static bool run_pisinger( const std::string & csv , const std::string & sconf )
{
 auto insts = read_pisinger( csv );
 if( insts.empty() ) {
  cerr << "Error: no instance read from " << csv << endl;
  return( false );
  }

 // build the Block from the first instance (Pisinger is pure 0-1: all integer)
 BKB = new BinaryKnapsackBlock();
 BKB->load( insts[ 0 ].N , insts[ 0 ].C ,
            std::vector< double >( insts[ 0 ].W ) ,
            std::vector< double >( insts[ 0 ].P ) );
 BKB->generate_abstract_variables();
 BKB->generate_abstract_constraints();
 BKB->generate_objective();

 Configuration * bsc = Configuration::deserialize( sconf );
 if( ! bsc ) {
  cerr << "Error: cannot load BSC from " << sconf << endl;
  return( false );
  }
 s_config_Block( BKB , bsc , sconf );
 if( BKB->get_registered_solvers().empty() ) {
  cerr << "Error: BlockSolverConfig did not register any Solver" << endl;
  return( false );
  }

 bool AllPassed = true;
 have_ref = true;
 for( std::size_t k = 0 ; k < insts.size() ; ++k ) {
  N = insts[ k ].N;
  BKB->chg_weights( insts[ k ].W.begin() );       // all the weights
  BKB->chg_profits( insts[ k ].P.begin() );       // all the profits
  BKB->chg_capacity( insts[ k ].C );
  ref_opt = insts[ k ].z;
  LOG( insts[ k ].name << ": " );
  AllPassed &= CrossCheckSolvers();
  }
 have_ref = false;

 if( AllPassed )
  cout << GREEN( All tests passed!! ) << endl;
 else
  cout << RED( Errors happened!! ) << endl;

 s_config_Block( BKB , bsc );
 delete bsc;
 delete BKB;
 return( AllPassed );
 }

/*--------------------------------------------------------------------------*/

// test-specific command-line knobs, set by process_specific_arg(); the
// standard parameter (-S BlockSolverConfig) is handled centrally by
// common_utils. This tester GENERATES its own BinaryKnapsackBlock from the
// seed, so it takes no instance positional (filename_optional = true).
// (N is declared as a global above.)
long int seed = 123123;     // seed
Index wchg = 127;           // what to change, coded bit-wise
Index n_repeat = 100;       // number of repetitions
double delta = 0.01;        // capacity parameter
double nW = 0.1;            // percentage of negative weights
double nP = 0.1;            // percentage of negative profits
double nI = 0.5;            // percentage of integer variables
double nM = 0.2;            // max percentage of items to modify
std::string pisinger_csv;   // if set (-C), test these Pisinger instances vs z

/*--------------------------------------------------------------------------*/

static bool process_specific_arg( int opt )
{
 switch( opt ) {
  case( 'e' ): Str2Sthg( optarg , seed );      return( true );
  case( 'k' ): Str2Sthg( optarg , wchg );      return( true );
  case( 'N' ): Str2Sthg( optarg , N );         return( true );
  case( 'n' ): Str2Sthg( optarg , n_repeat );  return( true );
  case( 'd' ): Str2Sthg( optarg , delta );     return( true );
  case( 'W' ): Str2Sthg( optarg , nW );        return( true );
  case( 'P' ): Str2Sthg( optarg , nP );        return( true );
  case( 'i' ): Str2Sthg( optarg , nI );        return( true );
  case( 'M' ): Str2Sthg( optarg , nM );        return( true );
  case( 'C' ): pisinger_csv = optarg;          return( true );
  default:                                     return( false );
  }
 }

/*--------------------------------------------------------------------------*/

int main( int argc , char **argv )
{
 // override the default terminate handler to print the exception message
 std::set_terminate( smspp_terminate );

 // reading command line parameters - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // the standard parameter (-S) is parsed by common_utils; the test only
 // appends its own knobs and reads no instance file (it generates one)

 // for small knapsacks nM * N may be too small (always 0 or 1 at most):
 // minM is the minimum absolute number of items that can be modified
 int minM = 10;

 docopt_desc = "SMS++ BinaryKnapsackBlock test.\n";
 filename_optional = true;
 short_opts += "e:k:N:n:d:W:P:i:M:C:";
 const std::vector< option > my_opts = {
   { "seed"   , required_argument , nullptr , 'e' } ,
   { "wchg"   , required_argument , nullptr , 'k' } ,
   { "nvar"   , required_argument , nullptr , 'N' } ,
   { "rounds" , required_argument , nullptr , 'n' } ,
   { "delta"  , required_argument , nullptr , 'd' } ,
   { "nW"     , required_argument , nullptr , 'W' } ,
   { "nP"     , required_argument , nullptr , 'P' } ,
   { "nI"     , required_argument , nullptr , 'i' } ,
   { "nM"     , required_argument , nullptr , 'M' } ,
   { "csv"    , required_argument , nullptr , 'C' } };
 long_opts.insert( std::prev( long_opts.end() ) ,
                   my_opts.begin() , my_opts.end() );
 help += "  -e, --seed <n>                  pseudo-random generator seed\n"
         "  -k, --wchg <bits>               what to change, bit-wise [127]:\n"
         "                                    1 sense, 2 capacity, 4 profits,\n"
         "                                    8 weights, 16 fix, 32 unfix,\n"
         "                                    64 integrality\n"
         "  -N, --nvar <n>                  number of variables [100]\n"
         "  -n, --rounds <n>                number of repetitions [100]\n"
         "  -d, --delta <x>                 capacity parameter [0.01]\n"
         "  -W, --nW <x>                    fraction of negative weights "
         "[0.1]\n"
         "  -P, --nP <x>                    fraction of negative profits "
         "[0.1]\n"
         "  -i, --nI <x>                    fraction of integer variables "
         "[0.5]\n"
         "  -M, --nM <x>                    max fraction of items to modify "
         "[0.2]\n"
         "  -C, --csv <file>                Pisinger .csv: test its instances "
         "against their published optimum z\n";

 process_args( argc , argv , process_specific_arg );

 // the BlockSolverConfig (-S) must be provided explicitly: the test never
 // falls back to a hardcoded default Configuration
 require_solver_config();

 // Pisinger mode: instead of generating a random instance and mutating it,
 // run all the attached Solver on every instance of the given .csv, checking
 // they agree with one another AND with the published optimum (see
 // run_pisinger())
 if( ! pisinger_csv.empty() )
  return( run_pisinger( pisinger_csv , sconf_file ) ? 0 : 1 );

 // sanity checks - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 
 if( ( delta < 0 ) || ( delta > 1 ) ) {
  cerr << "error: delta must be in [ 0 , 1 ]" << endl;
  exit( 1 ); 
  }

 if( ( nW < 0 ) || ( nW > 1 ) ) {
  cerr << "error: nW must be in [ 0 , 1 ]" << endl;
  exit( 1 );
  }

 if( ( nP < 0 ) || ( nP > 1 ) ) {
  cerr << "error: nP must be in [ 0 , 1 ]" << endl;
  exit( 1 );
  }

 if( ( nI < 0 ) || ( nI > 1 ) ) {
  cerr << "error: nI must be in [ 0 , 1 ]" << endl;
  exit( 1 );
  }

 if( ( nM < 0 ) || ( nM > 1 ) ) {
  cerr << "error: nI must be in [ 0 , 1 ]" << endl;
  exit( 1 );
  }

 const int minW =  - int( nW * rangeW );
 const int maxW = minW + rangeW;

 const double minP = - nP * rangeP;
 const double maxP = minP + rangeP;

 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // seed the pseudo-random number generator     

 rg.seed( seed );

 // print verbose header- - - - - - - - - - - - - - - - - - - - - - - - - - - 

 #if( LOG_LEVEL > 1 )
  cout << "seed = " << seed << " ~ N = " << N << " ~ n_repeat " << n_repeat
       << " ~ delta = " << delta << endl;
  cout << "W in [ " << minW << " , " << maxW << " ] ~ P in [ " << minP
       << " , " << maxP << " ] ~ nI = " << nI << endl;

  cout << endl << "Modifications: " << endl;
  if( wchg & 1 )                   
   cout << " - Objective Sense" << endl;
  if( wchg & 2 )
   cout << " - Capacity" << endl; 
  if( wchg & 4 )                   
   cout << " - Profits" << endl;
  if( wchg & 8 )
   cout << " - Weights" << endl; 
  if( wchg & 16 )                   
   cout << " - Fix" << endl;
  if( wchg & 32 )
   cout << " - Unfix" << endl;
  if( wchg & 64 )
   cout << " - Integrality";
  cout << endl << endl;
 #endif
 
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // create the BinaryKnapsackBlock- - - - - - - - - - - - - - - - - - - - - -

 BKB = new BinaryKnapsackBlock();            

 // generate instance - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

 // generate weights from a uniform int distribution
 uniform_int_distribution<> dist_W( minW , maxW );
 
 // generate integrality from a uniform int distribution
 uniform_int_distribution<> dist_I( 0 , 1 );

 // generate profits from a uniform real distribution
 uniform_real_distribution<> dist_P( minP , maxP );

 vector< double > W( N );             // vector of weights
 vector< double > P( N );             // vector of profits
 vector< bool > I( N );               // vector of integrality
 double C;                            // Capacity of the Knapsack

 int totWp = 0;                       // total sum of the positive weights
 int totWn = 0;                       // total sum of the negative weights

 for( Index i = 0 ; i < N ; i++ ) {
  W[ i ] = dist_W( rg );      
  P[ i ] = dist_P( rg );
  I[ i ] = ( dist_I( rg ) < nI );   
  if( W[ i ] > 0 )                     // update totWn and totWp
   totWp += W[ i ];      
  else
   totWn += W[ i ];      
  }
 
 // generate the Capacity from a uniform real distribution
 uniform_real_distribution<> dist_C( totWn , 
                                     totWn + delta * ( totWp - totWn ) );
 C = dist_C( rg );

 // load the Binary Knapsack instance- - - - - - - - - - - - - - - - - - - -
 
 if( nI < 1 )
  BKB->load( N , C , std::move( W ) , std::move( P ) , std::move( I ) );
 else
  BKB->load( N , C , std::move( W ) , std::move( P ) );

 // build the abstract representation (Objective + Constraint) up front, so
 // that the test works also with solver configurations that do not trigger it
 // themselves (e.g. the pure-DP benchmark config with no :MILPSolver); the
 // generate_abstract_*() are idempotent, hence a no-op when already built
 BKB->generate_abstract_variables();
 BKB->generate_abstract_constraints();
 BKB->generate_objective();

 // attach two Solver to the BinaryKnapsackBlock- - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // do it by using a single a BlockSolverConfig, read from file
 // BSC may be a plain BlockSolverConfig or a meta-config
 // SimpleConfiguration< std::map< std::string , Configuration * > >;
 // s_config_Block() dispatches on the runtime type and clears the config(s)
 // for final cleanup.

 Configuration * bsc = Configuration::deserialize( sconf_file );
 if( ! bsc ) {
  cerr << "Error: cannot load BSC from " << sconf_file << endl;
  exit( 1 );
  }
 s_config_Block( BKB , bsc , sconf_file );

 // check Solvers - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 if( BKB->get_registered_solvers().empty() ) {
  cerr << "Error: BlockSolverConfig did not register any Solver" << endl;
  exit( 1 );    
  }
  
 // get Objective and Constraint- - - - - - - - - - - - - - - - - - - - - - -
 
 auto obj = BKB->get_objective< FRealObjective >();
 
 auto cnst = BKB->get_static_constraint< FRowConstraint >( 0 );

 // get the corresponding linear functions

 auto lfobj = dynamic_cast< LinearFunction * >( obj->get_function() );
 if( ! lfobj ) {
  cerr << "Error: cannot get the Objective LinearFunction" << endl;
  exit( 1 ); 
  }

 auto lfcnst = dynamic_cast< LinearFunction * >( cnst->get_function() );
 if( ! lfcnst ) {
  cerr << "Error: cannot get the Constraint LinearFunction" << endl;
  exit( 1 ); 
  }

 // first call- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 LOG( "0: " );
 bool AllPassed = CrossCheckSolvers();
 
 // modifications loop- - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 for( Index i = 1 ; i <= n_repeat * STEP ; ++i ) {
  LOG( endl << i << ": " );
  
  // change the sense of the objective - - - - - - - - - - - - - - - - - - -
  if( ( wchg & 1 ) && ( dis( rg ) < 0.3 ) ) {
   LOG( "sense" );

   if( dis( rg ) < 0.5 )
    BKB->set_objective_sense( 1 - BKB->get_objective_sense() );   // PR
   else {
    obj->set_sense( 1 - BKB->get_objective_sense() );             // AR
    LOG( "(A)" );
    }
   LOG( " ~ " );
   }

  // change the Capacity of the Knapsack- - - - - - - - - - - - - - - - - - -
  if( wchg & 2 && dis( rg ) < 0.3 ) {
   LOG( "C" );

   C = dist_C( rg );
   
   if( dis( rg ) < 0.5 )
    BKB->chg_capacity( C );     // PR
   else {
    cnst->set_rhs( C );         // AR
    LOG( "(A)" );
    }
   LOG( " ~ " );
   }                   

  // change Profits (range or subset) - - - - - - - - - - - - - - - - - - - -
  if( wchg & 4 && dis( rg ) < 0.3 ) {

   Index m = dis( rg ) * max( int( nM * N ) , minM ); // n. of items to modify
   m = min( m , N );
   if( m ) {
    LOG( "P" );

    vector< double > nP( m );                 // generate new profits
    for( auto & p : nP )  
     p = dist_P( rg ); 

    if( dis( rg ) < 0.5 ) {                   // ranged modification
     Range rng = generateRange( m );

     if( dis( rg ) < 0.5 ) {
      BKB->chg_profits( nP.begin() , rng );              // PR
      LOG( "(R)" );
      }
     else {
      lfobj->modify_coefficients( std::move( nP ) , rng );    // AR
      LOG( "(AR)" );
      }
     }
    else {                                    // or subset modification
     Subset nms = generateSubset( m ); 
     if( dis( rg ) < 0.5 ) {
      BKB->chg_profits( nP.begin() , std::move( nms ) );              // PR
      LOG( "(S)" );
      }
     else {
      lfobj->modify_coefficients( std::move( nP ) , std::move( nms ) );    // AR
      LOG( "(AS)" );
      }
     }
    LOG( " ~ " );
    }                   
  }
  // change Weights (range or subset) - - - - - - - - - - - - - - - - - - - -
  if( wchg & 8 && dis( rg ) < 0.3 ) {

   Index m = dis( rg ) * max( int( nM * N ) , minM ); // n. of items to modify
   m = min( m , N );
   if( m ) {
    LOG( "W" );

    vector< double > nW( m );                 // generate new weights
    for( auto & w : nW )  
     w = dist_W( rg );

    if( dis( rg ) < 0.5 ) {                   // ranged modification
     Range rng = generateRange( m );

     if( dis( rg ) < 0.5 ) {
      BKB->chg_weights( nW.begin() , rng );              // PR
      LOG( "(R)" );
      }
     else {
      lfcnst->modify_coefficients( std::move( nW ) , rng );   // AR
      LOG( "(AR)" );
      }
     }
    else {                                    // or subset modification
     Subset nms = generateSubset( m ); 

     if( dis( rg ) < 0.5 ) {
      BKB->chg_weights( nW.begin() , std::move( nms ) );              // PR
      LOG( "(S)" );
      }
     else {
      lfcnst->modify_coefficients( std::move( nW ) , std::move( nms ) );   // AR
      LOG( "(AS)" );
      }
     }
    LOG( " ~ " );
    }
   }
   
  // Fix (range or subset)- - - - - - - - - - - - - - - - - - - - - - - - - -
  if( wchg & 16 && dis( rg ) < 0.3 ) {

   Index m = dis( rg ) * max( int( nM * N ) , minM ); // n. of items to modify
   m = min( m , N );
   if( m ) {
    LOG( "F" );

    vector< bool > nX( m );
    for( Index i = 0 ; i < m ; i++ )          // generate new x values
     nX[ i ] = ( dis( rg ) < 0.5 ) ? false : true;
    auto nXit = nX.begin();

    if( dis( rg ) < 0.5 ) {                   // ranged modification
     Range rng = generateRange( m );

     if( dis( rg ) < 0.5 ) {                  // PR 
      BKB->fix_x( nXit , rng ); 
      LOG( "(R)" );
      }
     else {                                   // AR
      for( Index j = rng.first ; j < rng.second ; j++ ) {
       auto x = BKB->get_Var( j );
       if( ! x->is_fixed() ) {
	x->set_value( *nXit++ );
	x->is_fixed( true );   
        }
       }
      LOG( "(AR)" );
      }
     }
    else {                                    // or subset modification
     Subset nms = generateSubset( m ); 

     if( dis( rg ) < 0.5 ) {                 // PR
      BKB->fix_x( nXit , std::move( nms ) );
      LOG( "(S)" );
      }
     else {                                  // AR
      for( auto j : nms ) {
       auto x = BKB->get_Var( j );
       if( ! x->is_fixed() ) {
	x->set_value( *nXit++ );
	x->is_fixed( true );   
        }
       }
      LOG( "(AS)" );
      }
     }
    LOG( " ~ " );
    }
   }
  // Unfix (range or subset)- - - - - - - - - - - - - - - - - - - - - - - - -
  if( wchg & 32 && dis( rg ) < 0.3 ) {

   Index m = dis( rg ) * max( int( nM * N ) , minM ); // n. of items to modify
   m = min( m , N );
   if( m ) {
    LOG( "U" );

    if( dis( rg ) < 0.5 ) {                   // ranged modification
     Range rng = generateRange( m );

     if( dis( rg ) < 0.5 ) {                  // PR
      BKB->unfix_x( rng );
      LOG( "(R)" );
      }
     else {                                   // AR
      for( Index j = rng.first ; j < rng.second ; j++ )
       BKB->get_Var( j )->is_fixed( false );
      LOG( "(AR)" );
      }
     }
    else {                                    // or subset modification    
     Subset nms = generateSubset( m ); 

     if( dis( rg ) < 0.5 ) {                  // PR
      BKB->unfix_x( std::move( nms ) );
      LOG( "(S)" );
      }
     else {                                   // AR
      for( auto j : nms )
       BKB->get_Var( j )->is_fixed( false );
      LOG( "(AS)" );
      }
     }
    LOG( " ~ " );
    }
   }
  // change Integrality (range or subset) - - - - - - - - - - - - - - - - - -
  if( wchg & 64 && dis( rg ) < 0.3 ) {

   Index m = dis( rg ) * max( int( nM * N ) , minM ); // n. of items to modify
   m = min( m , N );
   if( m ) {
    LOG( "I" );

    vector< bool > nI( m );               // generate new integrality vector
    for( Index i = 0 ; i < m ; i++ )
     nI[ i ] = ( dist_I( rg ) <= 0.5 );

    if( dis( rg ) < 0.5 ) {               // ranged modification
     Range rng = generateRange( m );

     if( dis( rg ) < 0.5 ) {              // PR
      BKB->chg_integrality( nI.begin() , rng );
      LOG( "(R)" );
      }
     else {                               // AR
      auto nIit = nI.begin();
      for( Index j = rng.first ; j < rng.second ; ++j )
       BKB->get_Var( j )->set_type( *(nIit++) ? ColVariable::kBinary
				              : ColVariable::kPosUnitary );
      LOG( "(AR)" );
      }
     }
    else {                                // or subset modification
     Subset nms = generateSubset( m ); 

     if( dis( rg ) < 0.5 ) {              // PR
      BKB->chg_integrality( nI.begin() , std::move( nms ) );
      LOG( "(S)" );
      }
     else {                               // AR
      auto nIit = nI.begin();
      for( auto j : nms )
       BKB->get_Var( j )->set_type( *(nIit++) ? ColVariable::kBinary
				              : ColVariable::kPosUnitary );
      LOG( "(AS)" );
      }
     }
    LOG( " ~ " );
    }
   }
  // finally, re-solve - - - - - - - - - - - - - - - - - - - - - - - - - - -
 
  if( ! ( i % STEP ) )
   AllPassed &= CrossCheckSolvers();

  }  // end( main loop ) - - - - - - - - - - - - - - - - - - - - - - - - - -
     //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 LOG( endl );
 if( AllPassed )
  cout << GREEN( All tests passed!! ) << endl;
 else
  cout << RED( Errors happened!! ) << endl;    

 // final cleanup - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

 s_config_Block( BKB , bsc );  // remove the Solver by re-apply()-ing the
                               // clear()-ed bsc (or meta-config)

 delete( bsc );      // delete the BlockSolverConfig

 delete( BKB );      // delete the BinaryKnapsackBlock

 // all done- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

 return( AllPassed ? 0 : 1 );

 }  // end( main )

/*--------------------------------------------------------------------------*/
/*------------------------ End File test.cpp -------------------------------*/
/*--------------------------------------------------------------------------*/
