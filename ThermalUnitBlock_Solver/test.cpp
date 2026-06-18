/*--------------------------------------------------------------------------*/
/*-------------------------- File test.cpp ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Main for testing ThermalUnitDPSolver.
 *
 * A ThermalUnitBlock instance is loaded from netCDF file, two different
 * Solver are registered to the ThermalUnitBlock, the second of which is
 * assumed to be a ThermalUnitDPSolver, the ThermalUnitBlock is solved by
 * the Solver and the results are compared. The ThermalUnitBlock is then
 * repeatedly randomly modified and re-solved several times, the results are
 * compared.
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
// 2 = + print optimal solutions

#define CHECK_SOLUTIONS 0
// coded bit-wise:
// bit 0: 1 = check feasibility of optimal solutions of Solver1
// bit 1: 1 = check feasibility of optimal solutions of Solver2
// bit 2: 1 = check that optimal solutions agree (dangerous, they may not)

#if( LOG_LEVEL >= 1 )
 #define LOG1( x ) std::cout << x
 #define CLOG1( y , x ) if( y ) std::cout << x
#else
 #define LOG1( x )
 #define CLOG1( y , x )
#endif

/*--------------------------------------------------------------------------*/
// if nonzero, the 1st Solver attached to the UCBlock is detached
// and re-attached to it at all iterations

#define DETACH_1ST 0

// if nonzero, the 2nd Solver attached to the UCBlock is detached and
// re-attached to it at all iterations

#define DETACH_2ND 0

/*--------------------------------------------------------------------------*/
// if nonzero, the Block is not solved at every round of changes, but only
// every SKIP_BEAT + 1 rounds. this allows changes to accumulate, and
// therefore puts more pressure on the Modification handling of the Solver
// (in case this tries to do "smart" things rather than dumbly processing
// each one in turn)
//
// note that the number of rounds of changes is them multiplied by
// SKIP_BEAT + 1, so that the input parameter still dictates the number of
// Block solutions

#define SKIP_BEAT 0

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

#include <sstream>

#include <chrono>

#include <random>

#include <iomanip>

#include "ThermalUnitBlock.h"

// NuclearUnitBlock derives from ThermalUnitBlock; this same tester also drives
// the nuclear instances (a NuclearUnitBlock deserialises and casts to a
// ThermalUnitBlock), printing the modulation indicators when present. The
// nuclear DP solver (NuclearUnitExtDPSolver) is selected through the
// BlockSolverConfig (BSCFG), not hard-wired here.
#include "NuclearUnitBlock.h"

#include "common_utils.h"

#include "FRealObjective.h"

#include "DQuadFunction.h"

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*-------------------------------- TYPES -----------------------------------*/
/*--------------------------------------------------------------------------*/

using Range = Block::Range;

using Subset = Block::Subset;

using FunctionValue = Function::FunctionValue;

/*--------------------------------------------------------------------------*/
/*------------------------------- CONSTANTS --------------------------------*/
/*--------------------------------------------------------------------------*/

static constexpr auto INF = Inf< FunctionValue >();

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/

ThermalUnitBlock * TUBlock;  // the ThermalUnitBlock

Index time_horizon;          // the length of the time horizon

std::vector< double > a;     // the quadratic cost coefficients
std::vector< double > b;     // the linear cost coefficients
std::vector< double > c;     // the fixed cost coefficients
//std::vector< double > l;     // the lower bounds on power production
std::vector< double > u;     // the upper bounds on power production

std::mt19937 rg;             // base random generator
std::uniform_real_distribution<> dis( 0.0 , 1.0 );

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

static Subset GenerateRand( Index m , Index k )
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

static void PrintSolution( void )
{
 std::cout.setf( std::ios::fixed );
 // std::cout.setf( std::ios::scientific , std::ios::floatfield );
 // std::cout << std::setprecision( 4 );

 auto p = TUBlock->get_active_power( 0 );
 std::cout << std::endl << "p = [ ";
 for( Index i = 0 ; ; ++p ) {
  std::cout << p->get_value();
  if( ++i >= time_horizon )
   break;
  else
   std::cout << ", ";
  }
 std::cout << " ]";

 auto u = TUBlock->get_commitment( 0 );
 std::cout << std::endl << "u = [ ";
 for( Index i = 0 ; ; ++u ) {
  std::cout << int( u->get_value() );
  if( ++i >= time_horizon )
   break;
  else
   std::cout << ", ";
  }
 std::cout << " ]" << std::endl;

 // modulation profile, only for a nuclear unit
 if( auto NUBlock = dynamic_cast< NuclearUnitBlock * >( TUBlock ) )
  if( auto m = NUBlock->get_modulation() ) {
   std::cout << "m = [ ";
   for( Index i = 0 ; ; ++m ) {
    std::cout << int( m->get_value() );
    if( ++i >= time_horizon )
     break;
    else
     std::cout << ", ";
    }
   std::cout << " ]" << std::endl;
   }
 }

/*--------------------------------------------------------------------------*/

#if( CHECK_SOLUTIONS & 4 )

static void GetP( std::vector< double > & P )
{
 auto p = TUBlock->get_active_power( 0 );
 for( auto & pi : P )
  pi = (p++)->get_value();
 }

static void GetU( std::vector< bool > & U )
{
 auto u = TUBlock->get_commitment( 0 );
 // apparently does not work for unfathomable reasons
 // for( auto & ui : U )
 // ui = (u++)->get_value();
 for( Index i = 0 ; i < U.size() ; ++i )
  U[ i ] = (u++)->get_value();
 }

#endif

/*--------------------------------------------------------------------------*/

static double fixed_cost( Index i )
{
 // returns the original fixed cost multiplied by a factor uniformly
 // distributed in [ -4 , 4 ]

 return( c[ i ] * ( 8 * dis( rg ) - 4 ) );
 }

/*--------------------------------------------------------------------------*/

static double quadratic_cost( Index i )
{
 // returns the original quadratic cost multiplied by a factor "uniformly
 // distributed" in [ 0.1 , 10 ]

 return( a[ i ] * pow( 10 , 2 * dis( rg ) - 1 ) );
 }

/*--------------------------------------------------------------------------*/

static double linear_cost( Index i )
{
 /* Randomly setting the linear cost is nontrivial, since 1UC problems have
  * an unfortunate tendency for producing "all 0" solutions with their
  * original costs. This is because a > 0, b > 0 and c > 0, so producing
  * power has a positive cost and there is no gain counter-balancing it.
  *
  * Random fixed costs can be negative (see fixed_cost()) so this provides
  * an incentive to the unit to produce, but typically one should set b < 0
  * so that also power production is convenient (least the unit is started
  * but always kept at the minimum).
  *
  * Since a > 0, the largest possible quadratic cost ist a u^2. To ensure
  * that producing energy is always more convenient than not producing
  * anything (p == 0 ==> cost == 0) one must have
  *
  *   a u^2 + b u < 0    ==>   b < - a u
  *
  * Notice that this just gives b < 0 if a == 0.
  *
  * The random value of b is therefore set as follows:
  *
  * - in 10% of the cases is equal to the original linear cost multiplied
  *   by a factor "uniformly distributed" in [ 0.1 , 10 ] (hence positive
  *   iff the original one was)
  *
  * - in all the remaining cases:
  *
  *   = if a ~= 0, then it is - | b | multiplied by a factor "uniformly
  *     distributed" in [ 0.1 , 10 ] (hence negative no matter what)
  *
  *   = else if is - a u is multiplied by a factor "uniformly distributed"
  *     in [ 4 , 1 / 4 ] (hence negative no matter what)
  *
  * Note, however, that a could have just changed prior to the call to
  * this function, so the current value in TUBlock is used rather than
  * the stored one. */

 auto ai = TUBlock->get_quad_term( i );

 return( dis( rg ) < 0.1
	 ? b[ i ] * pow( 10 , 2 * dis( rg ) - 1 )
   	 : ( abs( ai ) <= 1e-16
	     ? - abs( b[ i ] ) * pow( 10 , 2 * dis( rg ) - 1 )
	     : - ai * u[ i ] * 100 ) );
 }

/*--------------------------------------------------------------------------*/

static bool SolveBoth( void )
{
 #if( CHECK_SOLUTIONS & 4 )
  std::vector< double > p1( time_horizon );
  std::vector< bool > u1( time_horizon );
  std::vector< double > p2( time_horizon );
  std::vector< bool > u2( time_horizon );
 #endif

 #if( ( LOG_LEVEL > 1 ) || ( CHECK_SOLUTIONS > 0 ) )
  auto obj = ( static_cast< FRealObjective * >( TUBlock->get_objective() )
	       )->get_function();
 #endif

 try {
  // solve with the 1st Solver- - - - - - - - - - - - - - - - - - - - - - - -
  Solver * Slvr1 = TUBlock->get_registered_solvers().front();
  #if DETACH_1ST
   TUBlock->unregister_Solver( Slvr1 );
   TUBlock->register_Solver( Slvr1 , true );  // push it to the front
  #endif
  auto start1 = std::chrono::system_clock::now();
  int rtrn1st = Slvr1->compute( false );
  auto end1 = std::chrono::system_clock::now();
  double t1 = std::chrono::duration< double >( end1 - start1 ).count();
  bool hs1st = ( ( ( rtrn1st >= Solver::kOK ) && ( rtrn1st < Solver::kError )
                   && ( rtrn1st != Solver::kUnbounded )
                   && ( rtrn1st != Solver::kInfeasible ) )
                 || ( rtrn1st == Solver::kLowPrecision ) );
  double fo1st = Slvr1->get_var_value();
  #if( ( LOG_LEVEL > 1 ) || ( CHECK_SOLUTIONS > 0 ) )
   if( hs1st ) {
    if( ! Slvr1->has_var_solution() ) {
     std::cerr << "Error: Solver1 has not found any solution" << std::endl;
     exit( 1 );
     }
    Slvr1->get_var_solution();
    #if( LOG_LEVEL > 1 )
     PrintSolution();
    #endif
    #if( CHECK_SOLUTIONS & 1 )
     if( ! TUBlock->is_feasible() ) {
      std::cerr << "Error: Solver1 solution is not feasible" << std::endl;
      exit( 1 );
      }
    #endif
    obj->compute();
    auto solval = obj->get_value();
    if( abs( fo1st - solval ) > 1e-8 * std::max( abs( fo1st ) ,
                                                 double( 1 ) ) ) {
     std::cerr.setf( std::ios::scientific , std::ios::floatfield );
     std::cerr << std::setprecision( 9 );
     std::cerr << "Error: Solver1 reports value " << fo1st
	  << " but solution value is " << solval << std::endl;
     exit( 1 );
     }
    #if( CHECK_SOLUTIONS & 4 )
     GetP( p1 );
     GetU( u1 );
    #endif
    }
  #endif

  // solve with the 2nd Solver- - - - - - - - - - - - - - - - - - - - - - - -
  Solver * Slvr2 = TUBlock->get_registered_solvers().back();
  #if DETACH_2ND
   TUBlock->unregister_Solver( Slvr2 );
   TUBlock->register_Solver( Slvr2 );  // push it to the back
  #endif
  auto start2 = std::chrono::system_clock::now();
  int rtrn2nd = Slvr2->compute( false );
  auto end2 = std::chrono::system_clock::now();
  double t2 = std::chrono::duration< double >( end2 - start2 ).count();

  bool hs2nd = ( ( ( rtrn2nd >= Solver::kOK ) && ( rtrn2nd < Solver::kError )
                   && ( rtrn2nd != Solver::kUnbounded )
                   && ( rtrn2nd != Solver::kInfeasible ) )
                 || ( rtrn2nd == Solver::kLowPrecision ) );
  double fo2nd = hs2nd ? Slvr2->get_var_value() : -INF;
  #if( ( LOG_LEVEL > 1 ) || ( CHECK_SOLUTIONS > 0 ) )
   if( hs2nd ) {
    if( ! Slvr2->has_var_solution() ) {
     std::cerr << "Error: Solver2 has not found any solution" << std::endl;
     exit( 1 );
     }
    Slvr2->get_var_solution();
    #if( LOG_LEVEL > 1 )
     PrintSolution();
    #endif
    #if( CHECK_SOLUTIONS & 2 )
     if( ! TUBlock->is_feasible() ) {
      std::cerr << "Error: Solver2 solution is not feasible" << std::endl;
      //exit( 1 );
      }
    #endif
    obj->compute();
    auto solval = obj->get_value();
    if( abs( fo2nd - solval ) > 1e-8 * std::max( abs( fo2nd ) ,
                                                 double( 1 ) ) ) {
     std::cerr.setf( std::ios::scientific , std::ios::floatfield );
     std::cerr << std::setprecision( 9 );
     std::cerr << "Error: Solver2 reports value " << fo2nd
	  << " but solution value is " << solval << std::endl;
     exit( 1 );
     }
    #if( CHECK_SOLUTIONS & 4 )
     GetP( p2 );
     GetU( u2 );
    #endif
    }
  #endif

  // this being a MIQP, the "abstract" Solver will have a limited
  // precision. in particular, variable lower bound constraints like
  // p >= l u can be slightly violated (with p ending up a bit lower
  // than l) due to either u being, say, 0.999999 or the constraint
  // being violated up to the accuracy tolerated by the solver,
  // yielding things like 127.999999 vs 128.000000 and thereby a final
  // var_value() slightly lower than that of the ThermalUnitDPSolver.
  // which is why the relatively loose tolerance of 2e-6 here
  //!!  if( hs1st && hs2nd && ( abs( fo1st - fo2nd ) <= 2e-6 *
  //!!  emergency version with 1e-4 to find big errors
  // bespoke verdict (kept intact, including the CHECK_SOLUTIONS dispatch
  // comparison), restructured to a single exit that prints the unified line
  bool ok = false;
  std::string verdict = "KO";
  bool decided = false;
  if( hs1st && hs2nd && ( abs( fo1st - fo2nd ) <= 1e-4 *
			  std::max( double( 1 ) , std::max( abs( fo1st ) ,
						  abs( fo2nd ) ) ) ) ) {
   ok = true; verdict = "OK(f)"; decided = true;

   #if( CHECK_SOLUTIONS & 4 )
    for( Index i = 0 ; i < time_horizon ; ++i ) {
     if( abs( p1[ i ] - p2[ i ] ) > 1e-6 * max( abs( p1[ i ] ) ,
						double( 1 ) ) ) {
      std::cerr << "p1[ " << i << " ] = " << p1[ i ] << " != p2[ " << i
	   << " ] = " << p2[ i ] << std::endl;
      return( false );
      }

     if( u1[ i ] != u2[ i ] ) {
      std::cerr << "u1[ " << i << " ] = " << u1[ i ] << " != u2[ " << i
	   << " ] = " << u2[ i ] << std::endl;
      return( false );
      }
     }
   #endif

   }

  if( ( ! decided ) && ( rtrn1st == Solver::kInfeasible ) &&
      ( rtrn2nd == Solver::kInfeasible ) ) {
   ok = true; verdict = "OK(e)"; decided = true;
   }

  if( ( ! decided ) && ( rtrn1st == Solver::kUnbounded ) &&
      ( rtrn2nd == Solver::kUnbounded ) ) {
   ok = true; verdict = "OK(u)"; decided = true;
   }

  {
   auto tok = []( bool hs , int rtrn , double fo ) -> std::string {
    if( hs )                              return( fmt_obj( fo ) );
    if( rtrn == Solver::kInfeasible )     return( "Unfeas" );
    if( rtrn == Solver::kUnbounded )      return( "Unbounded" );
    return( "Error!" );
    };
   print_instance_line(
    { t1 , t2 } ,
    { tok( hs1st , rtrn1st , fo1st ) , tok( hs2nd , rtrn2nd , fo2nd ) } ,
    std::numeric_limits< double >::quiet_NaN() , verdict );
   }
  return( ok );
  }
 catch( std::exception &e ) {
  std::cerr << e.what() << std::endl;
  exit( 1 );
  }
 catch(...) {
  std::cerr << "Error: unknown exception thrown" << std::endl;
  exit( 1 );
  }
 }

/*--------------------------------------------------------------------------*/

// test-specific command-line knobs, set by process_specific_arg(); the
// standard parameters (instance positional, -S BlockSolverConfig, -c/-p
// prefixes) are handled centrally by common_utils. The same tester can thus
// drive different solver configurations (e.g. ThermalUnitExtDPSolver vs the
// nuclear NuclearUnitExtDPSolver) just by passing a different -S file.
long int seed = 0;
Index wchg = 135;
int wf = 1;
double p_change = 0.6;
Index n_change = 10;
Index n_repeat = 100;

/*--------------------------------------------------------------------------*/

static bool process_specific_arg( int opt )
{
 switch( opt ) {
  case( 'e' ): Str2Sthg( optarg , seed );      return( true );
  case( 'k' ): Str2Sthg( optarg , wchg );      return( true );
  case( 'f' ): Str2Sthg( optarg , wf );        return( true );
  case( 'n' ): Str2Sthg( optarg , n_repeat );  return( true );
  case( 'm' ): Str2Sthg( optarg , n_change );  return( true );
  case( 'q' ): Str2Sthg( optarg , p_change );  return( true );
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
 // standard params (instance positional + -S) are parsed by common_utils;
 // the test only appends its own knobs

 assert( SKIP_BEAT >= 0 );

 docopt_desc = "SMS++ ThermalUnitBlock Solver test.\n";
 short_opts += "e:k:f:n:m:q:";
 const std::vector< option > my_opts = {
   { "seed"   , required_argument , nullptr , 'e' } ,
   { "wchg"   , required_argument , nullptr , 'k' } ,
   { "wf"     , required_argument , nullptr , 'f' } ,
   { "rounds" , required_argument , nullptr , 'n' } ,
   { "nchng"  , required_argument , nullptr , 'm' } ,
   { "pchng"  , required_argument , nullptr , 'q' } };
 long_opts.insert( std::prev( long_opts.end() ) ,
                   my_opts.begin() , my_opts.end() );
 help += "  -e, --seed <n>                  pseudo-random generator seed [0]\n"
         "  -k, --wchg <bits>               what to change, bit-wise [135]:\n"
         "                                    1 fixed costs, 2 quadratic\n"
         "                                    coefficients, 4 linear\n"
         "                                    coefficients,\n"
         "                                    128 also via abstract\n"
         "                                    representation\n"
         "  -f, --wf <bits>                 what formulation [1]:\n"
         "                                    0 3bin, 1 T, 2 pt, 3 DP,\n"
         "                                    4 SU, 5 SD, 6 SUSD;\n"
         "                                    +8 also use perspective cuts\n"
         "  -n, --rounds <n>                how many iterations [100]\n"
         "  -m, --nchng <n>                 number of changes [10]\n"
         "  -q, --pchng <p>                 probability of changing [0.6]\n";

 process_args( argc , argv , process_specific_arg );

 // the BlockSolverConfig (-S) must be provided explicitly: the test never
 // falls back to a hardcoded default Configuration
 require_solver_config();

 rg.seed( seed );  // seed the pseudo-random number generator

 // read the Block- - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 auto block = Block::deserialize( filename );
 if( ! block ) {
  std::cout << std::endl << "Block::deserialize() failed!" << std::endl;
  exit( 1 );
  }

 TUBlock = dynamic_cast< ThermalUnitBlock * >( block );
 if( ! TUBlock ) {
  std::cout << std::endl << "The deserialized Block is not a ThermalUnitBlock"
            << std::endl;
  exit( 1 );
  }

 auto bc = new BlockConfig;
 bc->f_static_variables_Configuration = new SimpleConfiguration< int >( wf );
 TUBlock->set_BlockConfig( bc );

 // enable primary + secondary spinning-reserve variables: when the unit is
 // solved standalone there is no UCBlock parent to do it, so the reserve
 // variables/constraints would otherwise never be generated. This is a
 // no-op for instances with no PrimaryRho/SecondaryRho data.
 TUBlock->set_reserve_vars( 3 );

 TUBlock->generate_abstract_variables();
 TUBlock->generate_objective( nullptr );

 // save some original data of the ThermalUnitBlock - - - - - - - - - - - - -

 time_horizon = TUBlock->get_time_horizon();
 a.resize( time_horizon );
 b.resize( time_horizon );
 c.resize( time_horizon );
 // l.resize( time_horizon );
 u.resize( time_horizon );
 for( Index i = 0 ; i < time_horizon ; ++i ) {
  a[ i ] = TUBlock->get_quad_term( i );
  b[ i ] = TUBlock->get_linear_term( i );
  c[ i ] = TUBlock->get_const_term( i );
  // l[ i ] = TUBlock->get_operational_min_power( i );
  u[ i ] = TUBlock->get_operational_max_power( i );
  }

 // attach the Solver(s) to the ThermalUnitBlock- - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // do this by reading an appropriate BlockSolverConfig from file and
 // apply() it to the BoxBlock; note that the BlockSolverConfig is
 // clear()-ed and kept to do the cleanup at the end.
 // BSC may be a plain BlockSolverConfig or a meta-config
 // SimpleConfiguration< std::map< std::string , Configuration * > >;
 // s_config_Block() dispatches on the runtime type and clears the config(s)
 // for final cleanup.

 Configuration * bsc = Configuration::deserialize( sconf_file );
 if( ! bsc ) {
  std::cerr << "Error: cannot load BSC from " << sconf_file << std::endl;
  exit( 1 );
  }
 s_config_Block( TUBlock , bsc , sconf_file );

 if( TUBlock->get_registered_solvers().size() < 2 ) {
  std::cout << std::endl << "too few Solver registered to the Block"
            << std::endl;
  exit( 1 );
  }

 // first solver call - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 LOG1( "First call: " );

 bool AllPassed = SolveBoth();

 // main loop - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // now, for n_repeat times:
 // - up to n_change constant terms are changed
 // - up to n_change linear terms are changed
 // - up to n_change quadratic terms are changed
 //
 // then the two Solver are called to re-solve the BoxBlock

 for( Index rep = 0 ; rep < n_repeat * ( SKIP_BEAT + 1 ) ; ) {
  if( ! AllPassed )
   break;

  LOG1( rep << ": ");

  DQuadFunction * of;
  {
   auto obj = TUBlock->get_objective();
   assert( obj );
   auto fro = dynamic_cast< FRealObjective * >( obj );
   assert( fro );
   of = dynamic_cast< DQuadFunction * >( fro->get_function() );
   assert( of );
   }

  // change fixed costs - - - - - - - - - - - - - - - - - - - - - - - - - - -
  if( ( wchg & 1 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = std::min( time_horizon ,
                                  Index( dis( rg ) * n_change ) ) ) {
    LOG1( "changed " << tochange << " fixed costs" );

    std::vector< double >newcsts( tochange );

    // in 50% of the cases do a ranged change, in the others a sparse change
    if( dis( rg ) <= 0.5 ) {
     Index strt = dis( rg ) * ( time_horizon - tochange );
     Index stp = strt + tochange;

     for( Index i = 0 ; i < tochange ; ++i )
      newcsts[ i ] = fixed_cost( strt + i );

     if( ( wchg & 128 ) && ( dis( rg ) < 0.5 ) ) {
      // change via abstract representation
      // note that while this is a range of fixed costs, but the
      // corresponding variables may "scattered around" the objective and
      // therefore it becomes a Subset; yet, we check that if Subset
      // actually is a Range and in case convert it
      LOG1( "(r,a) - " );

      Subset nms( tochange );
      for( Index i = 0 ; i < tochange ; ++i )
       nms[ i ] = of->is_active( TUBlock->get_commitment( 0 ) + ( strt + i ) );

      std::sort( nms.begin() , nms.end() );
      if( nms.back() - nms.front() + 1 == nms.size() )
       of->modify_linear_coefficients( std::move( newcsts ) ,
				       Range( nms.front() , nms.back() + 1 ) );
      else
       of->modify_linear_coefficients( std::move( newcsts ) ,
				       std::move( nms ) );
      }
     else {  // change via call to set_* method
      LOG1( "(r) - " );
      TUBlock->set_const_term( newcsts.begin() , Range( strt , stp ) );
      }
     }
    else {
     Subset nms( GenerateRand( time_horizon , tochange ) );

     for( Index i = 0 ; i < tochange ; ++i )
      newcsts[ i ] = fixed_cost( nms[ i ] );

     if( ( wchg & 128 ) && ( dis( rg ) < 0.5 ) ) {
      // change via abstract representation
      LOG1( "(s,a) - " );

      for( Index i = 0 ; i < tochange ; ++i )
       nms[ i ] = of->is_active( TUBlock->get_commitment( 0 ) + nms[ i ] );

      of->modify_linear_coefficients( std::move( newcsts ) ,
				      std::move( nms ) , false );
      }
     else {  // change via call to set_* method
      LOG1( "(s) - " );
      TUBlock->set_const_term( newcsts.begin() , std::move( nms ) , true );
      }
     }
    }

  // change quadratic coefficients- - - - - - - - - - - - - - - - - - - - - -
  if( ( wchg & 2 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = std::min( time_horizon ,
                                  Index( dis( rg ) * n_change ) ) ) {
    LOG1( "changed " << tochange << " quadratic coeffs" );

    std::vector< double > newcsts( tochange );

    // in 50% of the cases do a ranged change, in the others a sparse change
    if( dis( rg ) <= 0.5 ) {
     Index strt = dis( rg ) * ( time_horizon - tochange );
     Index stp = strt + tochange;

     for( Index i = 0 ; i < tochange ; ++i )
      newcsts[ i ] = quadratic_cost( strt + i );

     if( ( wchg & 128 ) && ( dis( rg ) < 0.5 ) ) {
      // change via abstract representation
      // note that while this is a range of fixed costs, but the
      // corresponding variables may "scattered around" the objective and
      // therefore it becomes a Subset; yet, we check that if Subset
      // actually is a Range and in case convert it
      LOG1( "(r,a) - " );

      std::vector< double > lincsts( tochange );
      for( Index i = 0 ; i < tochange ; ++i )
       lincsts[ i ] = TUBlock->get_linear_term( strt + i );

      Subset nms( tochange );
      for( Index i = 0 ; i < tochange ; ++i )
       nms[ i ] = of->is_active( TUBlock->get_active_power( 0 )
				 + ( strt + i ) );

      std::sort( nms.begin() , nms.end() );
      if( nms.back() - nms.front() + 1 == nms.size() )
       of->modify_terms( newcsts.begin() , lincsts.begin() ,
			 Range( nms.front() , nms.back() + 1 ) );
      else
       of->modify_terms( newcsts.begin() , lincsts.begin() ,
			 std::move( nms ) );
      }
     else {  // change via call to set_* method
      LOG1( "(r) - " );
      TUBlock->set_quad_term( newcsts.begin() , Range( strt , stp ) );
      }
     }
    else {
     Subset nms( GenerateRand( time_horizon , tochange ) );

     for( Index i = 0 ; i < tochange ; ++i )
      newcsts[ i ] = quadratic_cost( nms[ i ] );

     if( ( wchg & 128 ) && ( dis( rg ) < 0.5 ) ) {
      // change via abstract representation
      LOG1( "(s,a) - " );

      std::vector< double > lincsts( tochange );
      for( Index i = 0 ; i < tochange ; ++i )
       lincsts[ i ] = TUBlock->get_linear_term( nms[ i ] );

      for( Index i = 0 ; i < tochange ; ++i )
       nms[ i ] = of->is_active( TUBlock->get_active_power( 0 ) + nms[ i ] );

      of->modify_terms( newcsts.begin() , lincsts.begin() ,
			std::move( nms ) , false );
      }
     else {  // change via call to set_* method
      LOG1( "(s) - " );
      TUBlock->set_quad_term( newcsts.begin() , std::move( nms ) , false );
      }
     }
    }

  // change linear coefficients - - - - - - - - - - - - - - - - - - - - - - -
  if( ( wchg & 4 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = std::min( time_horizon ,
                                  Index( dis( rg ) * n_change ) ) ) {
    LOG1( "changed " << tochange << " linear coeffs" );

    std::vector< double > newcsts( tochange );

    // in 50% of the cases do a ranged change, in the others a sparse change
    if( dis( rg ) <= 0.5 ) {
     Index strt = dis( rg ) * ( time_horizon - tochange );
     Index stp = strt + tochange;

     for( Index i = 0 ; i < tochange ; ++i )
      newcsts[ i ] = linear_cost( strt + i );

     if( ( wchg & 128 ) && ( dis( rg ) < 0.5 ) ) {
      // change via abstract representation
      // note that while this is a range of fixed costs, but the
      // corresponding variables may "scattered around" the objective and
      // therefore it becomes a Subset; yet, we check that if Subset
      // actually is a Range and in case convert it
      LOG1( "(r,a) - " );

      Subset nms( tochange );
      for( Index i = 0 ; i < tochange ; ++i )
       nms[ i ] = of->is_active( TUBlock->get_active_power( 0 )
				 + ( strt + i ) );

      std::sort( nms.begin() , nms.end() );
      if( nms.back() - nms.front() + 1 == nms.size() )
       of->modify_linear_coefficients( std::move( newcsts ) ,
				       Range( nms.front() , nms.back() + 1 ) );
      else
       of->modify_linear_coefficients( std::move( newcsts ) ,
				       std::move( nms ) );
      }
     else {  // change via call to set_* method
      LOG1( "(r) - " );
      TUBlock->set_linear_term( newcsts.begin() , Range( strt , stp ) );
      }
     }
    else {
     Subset nms( GenerateRand( time_horizon , tochange ) );

     for( Index i = 0 ; i < tochange ; ++i )
      newcsts[ i ] = linear_cost( nms[ i ] );

     if( ( wchg & 128 ) && ( dis( rg ) < 0.5 ) ) {
      // change via abstract representation
      LOG1( "(s,a) - " );

      for( Index i = 0 ; i < tochange ; ++i )
       nms[ i ] = of->is_active( TUBlock->get_active_power( 0 ) + nms[ i ] );

      of->modify_linear_coefficients( std::move( newcsts ) ,
				      std::move( nms ) , false );
      }
     else {  // change via call to set_* method
      LOG1( "(s) - " );
      TUBlock->set_linear_term( newcsts.begin() , std::move( nms ) , false );
      }
     }
    }

  // finally, re-solve the problems- - - - - - - - - - - - - - - - - - - - -
  // ... every SKIP_BEAT + 1 rounds

  if( ! ( ++rep % ( SKIP_BEAT + 1 ) ) ) {
   if( ! SolveBoth() )
    AllPassed = false;
   }
  #if( LOG_LEVEL >= 1 )
  else
   std::cout << std::endl;
  #endif

  }  // end( main loop )- - - - - - - - - - - - - - - - - - - - - - - - - - -
     // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 if( AllPassed )
  std::cout << GREEN( All tests passed!! ) << std::endl;
 else
  std::cout << RED( Shit happened!! ) << std::endl;

 // destroy objects and vectors - - - - - - - - - - - - - - - - - - - - - - - 
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 // apply() the clear()-ed BlockSolverConfig (or meta-config) to cleanup Solver
 s_config_Block( TUBlock , bsc );

 // then delete the BlockSolverConfig
 delete( bsc );

 // delete the Block
 delete( TUBlock );

 // terminate - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 return( AllPassed ? 0 : 1 );

 }  // end( main )

/*--------------------------------------------------------------------------*/
/*------------------------ End File test.cpp -------------------------------*/
/*--------------------------------------------------------------------------*/
