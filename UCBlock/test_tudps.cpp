/*--------------------------------------------------------------------------*/
/*-------------------------- File test_tudps.cpp ---------------------------*/
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
 * Given a list of BlockConfig (-F) rather than one (-B), the instance is read
 * once per element of the list, each copy is given its own BlockConfig, i.e.,
 * typically its own formulation, and the same BlockSolverConfig, each random
 * change is made to all the copies, and besides the two Solver of each copy
 * the copies are compared with each other: they are the same unit, hence
 * whatever their formulation they must have the same optimum.
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

#define CHECK_GET_SOLUTION 1
// 1 = check the Solution each Solver produces by itself, see
//     CheckGetSolution(); this is cheap and it is the only thing that a
//     "physical" Solver can offer to whoever has no abstract representation

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

#if CHECK_GET_SOLUTION

// check the Solution that the Solver produces by itself [see
// Solver::get_Solution()] against the one it writes into the Variable. the
// active power and the commitment are scribbled over in between, so that a
// Solution read out of the Variable rather than out of the Solver cannot pass
// unnoticed; only the round trip is checked, hence how accurate the Solver is
// does not matter here.
//
// the formulation-specific auxiliary variables are recomputed out of
// ( p , u ) on both sides of the comparison: writing the Solution back does
// it through ThermalUnitBlock::set_solution(), since those variables do not
// travel with the Solution, and the reference is taken after the same call,
// since a Solver that writes them itself only writes them as accurately as
// it solves. The perspective-cut variable is the case that matters: its cost
// is paid by the Objective, an approximate Solver leaves it above the p^2/u
// that the constraint fences, and the difference is worth its tolerance
// rather than anything about the round trip, which is what is checked here

static bool CheckGetSolution( Solver * slvr , const char * name )
{
 auto obj = ( static_cast< FRealObjective * >( TUBlock->get_objective() )
	      )->get_function();

 // the Solution holds (p, u), from which its write() derives the rest of
 // the schedule [see ThermalUnitBlock::set_solution()]: the state it is
 // compared with is completed in the same way, since the one a Solver leaves
 // may differ in what is derived, e.g., the epigraph of the perspective
 // cuts, which is only as tight as the cuts that were separated
 slvr->get_var_solution();
 TUBlock->set_solution();
 obj->compute();
 const auto ref = obj->get_value();

 auto sol = slvr->get_Solution();
 if( ! sol ) {
  std::cerr << "Error: " << name << " returned no Solution" << std::endl;
  return( false );
  }

 auto p = TUBlock->get_active_power( 0 );
 auto u = TUBlock->get_commitment( 0 );
 // a fixed Variable keeps its value [see ColVariable::set_value()], and it
 // is not scribbled over
 for( Index i = 0 ; i < time_horizon ; ++i , ++p , ++u ) {
  if( ! p->is_fixed() )
   p->set_value( -1 );
  if( ! u->is_fixed() )
   u->set_value( 0.5 );
  }

 // the modulation of a nuclear unit is scribbled over as well: it is in the
 // Solution [see NuclearUnitBlockSolution], hence the round trip must
 // restore it too
 std::vector< double > m;
 auto NUBlock = dynamic_cast< NuclearUnitBlock * >( TUBlock );
 if( NUBlock )
  if( auto mi = NUBlock->get_modulation() ) {
   m.resize( time_horizon );
   for( Index i = 0 ; i < time_horizon ; ++i , ++mi ) {
    m[ i ] = mi->get_value();
    if( ! mi->is_fixed() )
     mi->set_value( 0.5 );
    }
   }

 sol->write( TUBlock );
 delete sol;

 if( ! m.empty() ) {
  auto mi = NUBlock->get_modulation();
  for( Index i = 0 ; i < time_horizon ; ++i , ++mi )
   if( mi->get_value() != m[ i ] ) {
    std::cerr << "Error: " << name << " Solution has modulation "
	      << mi->get_value() << " instead of " << m[ i ] << " at "
	      << i << std::endl;
    return( false );
    }
  }

 obj->compute();
 const auto solval = obj->get_value();
 if( std::abs( ref - solval ) > 1e-6 * std::max( std::abs( ref ) ,
						 double( 1 ) ) ) {
  std::cerr.setf( std::ios::scientific , std::ios::floatfield );
  std::cerr << std::setprecision( 9 );
  std::cerr << "Error: " << name << " Solution is worth " << solval
	    << " but the one in the Variable is worth " << ref << std::endl;
  return( false );
  }

 return( true );
 }

#endif

/*--------------------------------------------------------------------------*/

// what the 1st Solver, the reference, gives on one copy of the unit: the
// copies are compared on it [see CrossCheck()]

struct Reading {
 double time = 0;                 // how long it took
 int rtrn = Solver::kError;       // what compute() returned
 bool hs = false;                 // whether it has a solution
 double fo = 0;                   // the value of the solution, if any
 };

/*--------------------------------------------------------------------------*/

// the token that stands for a result in the report

static std::string token( bool hs , int rtrn , double fo )
{
 if( hs )                              return( fmt_obj( fo ) );
 if( rtrn == Solver::kInfeasible )     return( "Unfeas" );
 if( rtrn == Solver::kUnbounded )      return( "Unbounded" );
 return( "Error!" );
 }

/*--------------------------------------------------------------------------*/

// solves the unit TUBlock with both its Solver and compares them; @p names
// are what the two are called in the report, the default numbering if empty,
// and @p rd receives what the 1st one gave

static bool SolveBoth( const std::vector< std::string > & names ,
		       Reading & rd )
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
  rd = { t1 , rtrn1st , hs1st , fo1st };
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

  #if CHECK_GET_SOLUTION
   if( hs1st && Slvr1->has_var_solution() )
    if( ! CheckGetSolution( Slvr1 , "Solver1" ) )
     return( false );
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

  #if CHECK_GET_SOLUTION
   if( hs2nd && Slvr2->has_var_solution() )
    if( ! CheckGetSolution( Slvr2 , "Solver2" ) )
     return( false );
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
     if( std::abs( p1[ i ] - p2[ i ] ) > 1e-6 * std::max( std::abs( p1[ i ] ) ,
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

  print_instance_line(
   { t1 , t2 } ,
   { token( hs1st , rtrn1st , fo1st ) , token( hs2nd , rtrn2nd , fo2nd ) } ,
   std::numeric_limits< double >::quiet_NaN() , verdict ,
   std::numeric_limits< double >::quiet_NaN() , false , names );
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
long int & seed = round_opts.seed;
Index & wchg = round_opts.wchg;
int wf = 1;          // the formulation of the unit TUBlock, read back from
                      // its BlockConfig
double & p_change = round_opts.p_change;
Index & n_change = round_opts.n_change;
Index & n_repeat = round_opts.n_repeat;
double sd_cost = 0;   // the shut-down cost given to the unit, if any
bool must_run = false;   // the unit is on at every instant of the horizon

const char * qcost_env = nullptr;  // the TUDPS_QCOST environment variable

/*--------------------------------------------------------------------------*/

// one copy of the unit per variant of its BlockConfig: each copy is read
// from the same file, given its own BlockConfig and the same
// BlockSolverConfig, and receives the same changes, so that the formulations
// are compared with each other as the Solver of each of them are

struct Variant {
 ThermalUnitBlock * tub;   // the copy of the unit
 Configuration * bsc;      // its BlockSolverConfig, kept for the cleanup
 int wf;                   // its formulation, read back from its BlockConfig
 std::string name;         // what it is called in the report
 };

std::vector< Variant > variants;

/*--------------------------------------------------------------------------*/

// makes @p v the unit that the functions above work on

static void use_variant( const Variant & v )
{
 TUBlock = v.tub;
 wf = v.wf;
 }

/*--------------------------------------------------------------------------*/

// the name of the formulation @p f, as in the TUBCfg-<name>.txt that give it

static std::string formulation_name( int f )
{
 static const char * const nm[] = { "3bin" , "T" , "pt" , "DP" , "SU" , "SD" ,
				     "SUSD" , "?" };
 return( std::string( nm[ f & 7 ] ) + ( ( f & 8 ) ? "-PC" : "" ) );
 }

/*--------------------------------------------------------------------------*/

static bool process_specific_arg( int opt )
{
 if( process_round_option( opt ) )
  return( true );
 switch( opt ) {
  case( 'd' ): Str2Sthg( optarg , sd_cost );   return( true );
  case( 'u' ): must_run = true;                return( true );
  default:                                     return( false );
  }
 }

/*--------------------------------------------------------------------------*/

// reads a copy of the unit, applies to it the (meta-)BlockConfig @p bconf
// read from @p fn and gives it what the command line and the environment ask
// for, building its abstract representation

static ThermalUnitBlock * read_unit( Configuration * bconf ,
				     const std::string & fn )
{
 auto block = Block::deserialize( filename );
 if( ! block ) {
  std::cout << std::endl << "Block::deserialize() failed!" << std::endl;
  exit( 1 );
  }

 auto tub = dynamic_cast< ThermalUnitBlock * >( block );
 if( ! tub ) {
  std::cout << std::endl << "The deserialized Block is not a ThermalUnitBlock"
            << std::endl;
  exit( 1 );
  }

 // the formulation of the unit comes from the BlockConfig
 b_config_Block( tub , bconf , fn );

 // enable primary + secondary spinning-reserve variables: when the unit is
 // solved standalone there is no UCBlock parent to do it, so the reserve
 // variables/constraints would otherwise never be generated. This is a
 // no-op for instances with no PrimaryRho/SecondaryRho data.
 tub->set_reserve_vars( 3 );

 // env-gated validation of commitment-gated reactive power: with TUDPS_QCOST
 // set, enable q[t] (the standalone unit has no UCBlock parent to do it) and,
 // below, price it with a constant dualized reactive term, so the DP-vs-MILP
 // check exercises the box q in [Qmin_off+Qmin_on u, Qmax_off+Qmax_on u].
 if( qcost_env )
  tub->set_reactive_power( true );

 // a unit that is on before the horizon and whose minimum up time is longer
 // than it never switches within it, which is how a generator that PyPSA
 // does not commit is written; the minimum times decide the structure of the
 // model, hence they are set before the Variable are generated
 if( must_run ) {
  const std::vector< int > on( 1 , 1 );
  tub->set_init_updown_time( on.begin() , Range( 0 , 1 ) );
  tub->set_min_up_down_time( tub->get_time_horizon() + 1 , 1 );
  }

 // the shut-down cost, if any, is given to the unit before the Objective is
 // generated: the shut-down variables enter it only if the unit pays
 // anything to shut down
 if( sd_cost != 0 ) {
  // the first instants in which the commitment is fixed, and therefore have
  // no shut-down variable, are those the initial state imposes
  const int tau0 = tub->get_init_up_down_time();
  const Index init_t =
   tau0 > 0 ? ( Index( tau0 ) >= tub->get_min_up_time()
                ? 0 : tub->get_min_up_time() - Index( tau0 ) )
            : ( Index( - tau0 ) >= tub->get_min_down_time()
                ? 0 : tub->get_min_down_time() - Index( - tau0 ) );
  std::vector< double > sdc( tub->get_time_horizon() - init_t , sd_cost );
  tub->set_shutdown_costs( sdc.begin() ,
                           Range( init_t , tub->get_time_horizon() ) );
  }

 // env-gated: price the spinning reserves with a constant (negative) cost, so
 // the reserve-rewarded multi-piece per-period cost path of the DP solvers is
 // exercised (for profiling the reserve overhead of the base solver). The
 // cost is given before the Objective is generated: a reserve whose cost is
 // zero then is not in it [see ThermalUnitBlock::generate_objective()],
 // and after the Variable are, which the cost needs
 tub->generate_abstract_variables();
 if( const char * rc = std::getenv( "TUDPS_RESCOST" ) ) {
  std::vector< double > rcv( tub->get_time_horizon() , std::atof( rc ) );
  tub->set_primary_spinning_reserve_cost( rcv.begin() ,
                                          Range( 0 , rcv.size() ) );
  tub->set_secondary_spinning_reserve_cost( rcv.begin() ,
                                            Range( 0 , rcv.size() ) );
  }

 tub->generate_objective( nullptr );

 // env-gated check of the fixed Variable of the operating rules: with
 // TUDPS_FIXMOD = k set, one modulation Variable out of k is fixed to 0 and
 // the one at the middle of the horizon to 1, which any instance can do (a
 // unit is never forced to modulate, and a modulation may move the output
 // by nothing), so that the DP and the MILP solve the same restricted
 // problem and must still agree
 if( const char * fixmod_env = std::getenv( "TUDPS_FIXMOD" ) )
  if( auto NUBlock = dynamic_cast< NuclearUnitBlock * >( tub ) )
   if( auto m = NUBlock->get_modulation() ) {
    const Index hor = NUBlock->get_time_horizon();
    const Index k = std::max( 1 , std::atoi( fixmod_env ) );
    for( Index t = 0 ; t < hor ; t += k )
     if( ! m[ t ].is_fixed() ) {
      m[ t ].set_value( 0 );
      m[ t ].is_fixed( true , eNoMod );
      }
    if( ! m[ hor / 2 ].is_fixed() ) {
     m[ hor / 2 ].set_value( 1 );
     m[ hor / 2 ].is_fixed( true , eNoMod );
     }
    }

 if( qcost_env ) {
  std::vector< double > qcost( tub->get_time_horizon() ,
                               std::atof( qcost_env ) );
  tub->set_reactive_linear_term( qcost.begin() ,
                                 Range( 0 , tub->get_time_horizon() ) );
  }

 return( tub );
 }

/*--------------------------------------------------------------------------*/

// the Objective of the unit TUBlock

static DQuadFunction * objective_function( void )
{
 auto obj = TUBlock->get_objective();
 assert( obj );
 auto fro = dynamic_cast< FRealObjective * >( obj );
 assert( fro );
 auto of = dynamic_cast< DQuadFunction * >( fro->get_function() );
 assert( of );
 return( of );
 }

/*--------------------------------------------------------------------------*/

// one random change of the costs, drawn once and made to every copy of the
// unit: either the range [ strt , strt + newcsts.size() ) or the subset nms
// of the instants, and whether it is made via the abstract representation

struct Change {
 std::vector< double > newcsts;
 bool ranged = false;
 Index strt = 0;
 Subset nms;
 bool abstr = false;
 };

/*--------------------------------------------------------------------------*/

// draws a change of @p tochange costs, each computed by @p cost; in 50% of
// the cases it is a ranged change, in the others a sparse change

static Change draw_change( Index tochange , double ( *cost )( Index ) )
{
 Change chg;
 chg.newcsts.resize( tochange );
 chg.ranged = ( dis( rg ) <= 0.5 );
 if( chg.ranged ) {
  chg.strt = dis( rg ) * ( time_horizon - tochange );
  for( Index i = 0 ; i < tochange ; ++i )
   chg.newcsts[ i ] = cost( chg.strt + i );
  }
 else {
  chg.nms = GenerateRand( time_horizon , tochange , rg );
  for( Index i = 0 ; i < tochange ; ++i )
   chg.newcsts[ i ] = cost( chg.nms[ i ] );
  }
 chg.abstr = ( wchg & 128 ) && ( dis( rg ) < 0.5 );
 return( chg );
 }

/*--------------------------------------------------------------------------*/

// makes the change @p chg of the fixed costs to the unit TUBlock

static void change_fixed_costs( Change chg )
{
 auto of = objective_function();
 const Index tochange = chg.newcsts.size();

 if( chg.ranged ) {
  if( chg.abstr ) {
   // change via abstract representation
   // note that while this is a range of fixed costs, but the
   // corresponding variables may "scattered around" the objective and
   // therefore it becomes a Subset; yet, we check that if Subset
   // actually is a Range and in case convert it
   LOG1( "(r,a) - " );

   Subset nms( tochange );
   for( Index i = 0 ; i < tochange ; ++i )
    nms[ i ] = of->is_active( TUBlock->get_commitment( 0 ) +
			      ( chg.strt + i ) );

   std::sort( nms.begin() , nms.end() );
   if( nms.back() - nms.front() + 1 == nms.size() )
    of->modify_linear_coefficients( std::move( chg.newcsts ) ,
				    Range( nms.front() , nms.back() + 1 ) );
   else
    of->modify_linear_coefficients( std::move( chg.newcsts ) ,
				    std::move( nms ) );
   }
  else {  // change via call to set_* method
   LOG1( "(r) - " );
   TUBlock->set_const_term( chg.newcsts.begin() ,
			    Range( chg.strt , chg.strt + tochange ) );
   }
  }
 else {
  if( chg.abstr ) {
   // change via abstract representation
   LOG1( "(s,a) - " );

   for( Index i = 0 ; i < tochange ; ++i )
    chg.nms[ i ] = of->is_active( TUBlock->get_commitment( 0 ) +
				  chg.nms[ i ] );

   of->modify_linear_coefficients( std::move( chg.newcsts ) ,
				   std::move( chg.nms ) , false );
   }
  else {  // change via call to set_* method
   LOG1( "(s) - " );
   TUBlock->set_const_term( chg.newcsts.begin() , std::move( chg.nms ) ,
			    true );
   }
  }
 }

/*--------------------------------------------------------------------------*/

// makes the change @p chg of the quadratic coefficients to the unit TUBlock

static void change_quadratic_costs( Change chg )
{
 auto of = objective_function();
 const Index tochange = chg.newcsts.size();

 if( chg.ranged ) {
  if( chg.abstr ) {
   // change via abstract representation
   // note that while this is a range of fixed costs, but the
   // corresponding variables may "scattered around" the objective and
   // therefore it becomes a Subset; yet, we check that if Subset
   // actually is a Range and in case convert it
   LOG1( "(r,a) - " );

   if( wf & 8 ) {
    // with perspective cuts the quadratic term is the *linear*
    // coefficient of the cut variables; only the tbin/T/pt formulations
    // have them time-indexed and active in the Objective, for the others
    // (DP/SU/SD/SUSD) fall back to the physical setter
    if( auto cut = ( wf & 7 ) <= 2 ? TUBlock->get_cut() : nullptr ) {
     Subset nms( tochange );
     for( Index i = 0 ; i < tochange ; ++i )
      nms[ i ] = of->is_active( cut + ( chg.strt + i ) );

     std::sort( nms.begin() , nms.end() );
     if( nms.back() - nms.front() + 1 == nms.size() )
      of->modify_linear_coefficients( std::move( chg.newcsts ) ,
				      Range( nms.front() ,
					     nms.back() + 1 ) );
     else
      of->modify_linear_coefficients( std::move( chg.newcsts ) ,
				      std::move( nms ) );
     }
    else
     TUBlock->set_quad_term( chg.newcsts.begin() ,
			     Range( chg.strt , chg.strt + tochange ) );
    }
   else {
    std::vector< double > lincsts( tochange );
    for( Index i = 0 ; i < tochange ; ++i )
     lincsts[ i ] = TUBlock->get_linear_term( chg.strt + i );

    Subset nms( tochange );
    for( Index i = 0 ; i < tochange ; ++i )
     nms[ i ] = of->is_active( TUBlock->get_active_power( 0 )
			       + ( chg.strt + i ) );

    std::sort( nms.begin() , nms.end() );
    if( nms.back() - nms.front() + 1 == nms.size() )
     of->modify_terms( chg.newcsts.begin() , lincsts.begin() ,
		       Range( nms.front() , nms.back() + 1 ) );
    else
     of->modify_terms( chg.newcsts.begin() , lincsts.begin() ,
		       std::move( nms ) );
    }
   }
  else {  // change via call to set_* method
   LOG1( "(r) - " );
   TUBlock->set_quad_term( chg.newcsts.begin() ,
			   Range( chg.strt , chg.strt + tochange ) );
   }
  }
 else {
  if( chg.abstr ) {
   // change via abstract representation
   LOG1( "(s,a) - " );

   if( wf & 8 ) {
    // with perspective cuts the quadratic term is the *linear*
    // coefficient of the cut variables; only the tbin/T/pt formulations
    // have them time-indexed and active in the Objective, for the others
    // (DP/SU/SD/SUSD) fall back to the physical setter
    if( auto cut = ( wf & 7 ) <= 2 ? TUBlock->get_cut() : nullptr ) {
     for( Index i = 0 ; i < tochange ; ++i )
      chg.nms[ i ] = of->is_active( cut + chg.nms[ i ] );

     of->modify_linear_coefficients( std::move( chg.newcsts ) ,
				     std::move( chg.nms ) , false );
     }
    else
     TUBlock->set_quad_term( chg.newcsts.begin() , std::move( chg.nms ) ,
			     false );
    }
   else {
    std::vector< double > lincsts( tochange );
    for( Index i = 0 ; i < tochange ; ++i )
     lincsts[ i ] = TUBlock->get_linear_term( chg.nms[ i ] );

    for( Index i = 0 ; i < tochange ; ++i )
     chg.nms[ i ] = of->is_active( TUBlock->get_active_power( 0 ) +
				   chg.nms[ i ] );

    of->modify_terms( chg.newcsts.begin() , lincsts.begin() ,
		      std::move( chg.nms ) , false );
    }
   }
  else {  // change via call to set_* method
   LOG1( "(s) - " );
   TUBlock->set_quad_term( chg.newcsts.begin() , std::move( chg.nms ) ,
			   false );
   }
  }
 }

/*--------------------------------------------------------------------------*/

// makes the change @p chg of the linear coefficients to the unit TUBlock

static void change_linear_costs( Change chg )
{
 auto of = objective_function();
 const Index tochange = chg.newcsts.size();

 if( chg.ranged ) {
  if( chg.abstr ) {
   // change via abstract representation
   // note that while this is a range of fixed costs, but the
   // corresponding variables may "scattered around" the objective and
   // therefore it becomes a Subset; yet, we check that if Subset
   // actually is a Range and in case convert it
   LOG1( "(r,a) - " );

   Subset nms( tochange );
   for( Index i = 0 ; i < tochange ; ++i )
    nms[ i ] = of->is_active( TUBlock->get_active_power( 0 )
			      + ( chg.strt + i ) );

   std::sort( nms.begin() , nms.end() );
   if( nms.back() - nms.front() + 1 == nms.size() )
    of->modify_linear_coefficients( std::move( chg.newcsts ) ,
				    Range( nms.front() , nms.back() + 1 ) );
   else
    of->modify_linear_coefficients( std::move( chg.newcsts ) ,
				    std::move( nms ) );
   }
  else {  // change via call to set_* method
   LOG1( "(r) - " );
   TUBlock->set_linear_term( chg.newcsts.begin() ,
			     Range( chg.strt , chg.strt + tochange ) );
   }
  }
 else {
  if( chg.abstr ) {
   // change via abstract representation
   LOG1( "(s,a) - " );

   for( Index i = 0 ; i < tochange ; ++i )
    chg.nms[ i ] = of->is_active( TUBlock->get_active_power( 0 ) +
				  chg.nms[ i ] );

   of->modify_linear_coefficients( std::move( chg.newcsts ) ,
				   std::move( chg.nms ) , false );
   }
  else {  // change via call to set_* method
   LOG1( "(s) - " );
   TUBlock->set_linear_term( chg.newcsts.begin() , std::move( chg.nms ) ,
			     false );
   }
  }
 }

/*--------------------------------------------------------------------------*/

// compares what the 1st Solver gave on each copy of the unit, i.e., the
// formulations with each other: they are the same unit, hence they must have
// the same optimum, up to the tolerance with which the two Solver of each
// copy are compared [see SolveBoth()], or be all infeasible or all unbounded

static bool CrossCheck( const std::vector< Reading > & rd )
{
 if( rd.size() < 2 )
  return( true );

 bool all_hs = true;
 bool all_inf = true;
 bool all_unb = true;
 for( const auto & r : rd ) {
  all_hs &= r.hs;
  all_inf &= ( r.rtrn == Solver::kInfeasible );
  all_unb &= ( r.rtrn == Solver::kUnbounded );
  }

 bool ok = false;
 std::string verdict = "KO";
 if( all_hs ) {
  ok = true;
  for( const auto & r : rd )
   if( std::abs( r.fo - rd.front().fo ) > 1e-4 *
       std::max( double( 1 ) , std::max( std::abs( r.fo ) ,
					 std::abs( rd.front().fo ) ) ) )
    ok = false;
  if( ok )
   verdict = "OK(f)";
  }
 else
  if( all_inf ) {
   ok = true; verdict = "OK(e)";
   }
  else
   if( all_unb ) {
    ok = true; verdict = "OK(u)";
    }

 std::vector< double > times;
 std::vector< std::string > tokens;
 std::vector< std::string > names;
 for( std::size_t k = 0 ; k < rd.size() ; ++k ) {
  times.push_back( rd[ k ].time );
  tokens.push_back( token( rd[ k ].hs , rd[ k ].rtrn , rd[ k ].fo ) );
  names.push_back( variants[ k ].name );
  }
 print_instance_line( times , tokens ,
		      std::numeric_limits< double >::quiet_NaN() , verdict ,
		      std::numeric_limits< double >::quiet_NaN() , false ,
		      names );
 return( ok );
 }

/*--------------------------------------------------------------------------*/

// solves every copy of the unit with its two Solver, then compares the
// copies with each other

static bool SolveCopies( void )
{
 bool ok = true;
 std::vector< Reading > rd( variants.size() );
 for( std::size_t k = 0 ; k < variants.size() ; ++k ) {
  use_variant( variants[ k ] );
  // with one copy the report stays the one of the two Solver
  std::vector< std::string > names;
  if( variants.size() > 1 )
   names = { variants[ k ].name + " S0" , variants[ k ].name + " S1" };
  if( ! SolveBoth( names , rd[ k ] ) )
   ok = false;
  }

 if( ! CrossCheck( rd ) )
  ok = false;

 return( ok );
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
 round_opts = { 0 , 135 , 100 , 10 , 0.6 };
 add_round_options(
  "                                    1 fixed costs, 2 quadratic\n"
  "                                    coefficients, 4 linear\n"
  "                                    coefficients,\n"
  "                                    128 also via abstract\n"
  "                                    representation\n" );
 short_opts += "d:u";
 const std::vector< option > my_opts = {
   { "sdcost"  , required_argument , nullptr , 'd' } ,
   { "mustrun" , no_argument , nullptr , 'u' } };
 long_opts.insert( std::prev( long_opts.end() ) ,
                   my_opts.begin() , my_opts.end() );
 help += "  -d, --sdcost <v>                shut-down cost of the unit at\n"
         "                                  each instant, the instance\n"
         "                                  having none [0]\n"
         "  -u, --mustrun                   the unit is on at every instant,\n"
         "                                  as a generator PyPSA does not\n"
         "                                  commit\n";

 process_args( argc , argv , process_specific_arg );

 // the BlockSolverConfig (-S) and the BlockConfig (-B), which gives the
 // formulation of the unit (e.g., TUBCfg-T.txt), or the list of them (-F),
 // must be provided explicitly: the test never falls back to a hardcoded
 // default Configuration [see get_BlockConfig_variants()]

 rg.seed( seed );  // seed the pseudo-random number generator

 qcost_env = std::getenv( "TUDPS_QCOST" );

 // read the Block, once per variant - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // the formulation of each copy comes from its BlockConfig, and it is read
 // back, as the changes below depend on it: 0 3bin, 1 T, 2 pt, 3 DP, 4 SU,
 // 5 SD, 6 SUSD, +8 also Perspective Cuts

 auto bv = get_BlockConfig_variants();

 for( std::size_t k = 0 ; k < bv.configs.size() ; ++k ) {
  auto tub = read_unit( bv.configs[ k ] , bv.file );

  int f = 1;  // the T formulation, the one of a unit given no BlockConfig
  if( auto bc = tub->get_BlockConfig() )
   if( auto sc = dynamic_cast< SimpleConfiguration< int > * >(
					bc->f_static_variables_Configuration ) )
    f = sc->f_value;

  variants.push_back( { tub , nullptr , f ,
			std::to_string( k ) + ":" + formulation_name( f ) } );
  }

 // save some original data of the ThermalUnitBlock - - - - - - - - - - - - -
 // they are the same in every copy

 use_variant( variants.front() );
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

 // attach the Solver(s) to each copy of the ThermalUnitBlock - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // do this by reading an appropriate BlockSolverConfig from file and
 // apply() it to the copy; note that the BlockSolverConfig is clear()-ed
 // and kept to do the cleanup at the end, hence each copy reads its own.
 // BSC may be a plain BlockSolverConfig or a meta-config
 // SimpleConfiguration< std::map< std::string , Configuration * > >;
 // s_config_Block() dispatches on the runtime type and clears the config(s)
 // for final cleanup.

 for( auto & v : variants ) {
  v.bsc = solver_config_of( bv , &v - variants.data() );
  s_config_Block( v.tub , v.bsc , sconf_file );

  if( v.tub->get_registered_solvers().size() < 2 ) {
   std::cout << std::endl << "too few Solver registered to the Block"
             << std::endl;
   exit( 1 );
   }
  }

 delete bv.owner;

 // first solver call - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 LOG1( "First call: " );

 bool AllPassed = SolveCopies();
 Index n_solved = 1;

 // main loop - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // now, for n_repeat times:
 // - up to n_change constant terms are changed
 // - up to n_change linear terms are changed
 // - up to n_change quadratic terms are changed
 //
 // then the two Solver are called to re-solve each copy of the unit. Each
 // change is drawn once and made to every copy, so that the pseudo-random
 // sequence, and therefore what each copy is asked, is that of a run with
 // that copy alone

 for( Index rep = 0 ; rep < n_repeat * ( SKIP_BEAT + 1 ) ; ) {
  if( ! AllPassed )
   break;

  LOG1( rep << ": ");

  // change fixed costs - - - - - - - - - - - - - - - - - - - - - - - - - - -
  if( ( wchg & 1 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = std::min( time_horizon ,
                                  Index( dis( rg ) * n_change ) ) ) {
    LOG1( "changed " << tochange << " fixed costs" );
    const auto chg = draw_change( tochange , fixed_cost );
    for( const auto & v : variants ) {
     use_variant( v );
     change_fixed_costs( chg );
     }
    }

  // change quadratic coefficients- - - - - - - - - - - - - - - - - - - - - -
  if( ( wchg & 2 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = std::min( time_horizon ,
                                  Index( dis( rg ) * n_change ) ) ) {
    LOG1( "changed " << tochange << " quadratic coeffs" );
    const auto chg = draw_change( tochange , quadratic_cost );
    for( const auto & v : variants ) {
     use_variant( v );
     change_quadratic_costs( chg );
     }
    }

  // change linear coefficients - - - - - - - - - - - - - - - - - - - - - - -
  // linear_cost() reads the current quadratic coefficients, which are the
  // same in every copy, from the first one
  if( ( wchg & 4 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = std::min( time_horizon ,
                                  Index( dis( rg ) * n_change ) ) ) {
    LOG1( "changed " << tochange << " linear coeffs" );
    use_variant( variants.front() );
    const auto chg = draw_change( tochange , linear_cost );
    for( const auto & v : variants ) {
     use_variant( v );
     change_linear_costs( chg );
     }
    }

  // finally, re-solve the problems- - - - - - - - - - - - - - - - - - - - -
  // ... every SKIP_BEAT + 1 rounds

  if( ! ( ++rep % ( SKIP_BEAT + 1 ) ) ) {
   if( ! SolveCopies() )
    AllPassed = false;
   ++n_solved;
   }
  #if( LOG_LEVEL >= 1 )
  else
   std::cout << std::endl;
  #endif

  }  // end( main loop )- - - - - - - - - - - - - - - - - - - - - - - - - - -
     // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 // with several copies, which formulations have been compared, and how many
 // times
 if( AllPassed && ( variants.size() > 1 ) ) {
  std::cout << "formulations";
  for( const auto & v : variants )
   std::cout << " " << v.name;
  std::cout << " agree in " << n_solved << " solves" << std::endl;
  }

 if( AllPassed )
  std::cout << GREEN( All tests passed!! ) << std::endl;
 else
  std::cout << RED( Shit happened!! ) << std::endl;

 // destroy objects and vectors - - - - - - - - - - - - - - - - - - - - - - - 
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 for( auto & v : variants ) {
  // apply() the clear()-ed BlockSolverConfig (or meta-config) to cleanup
  // Solver, then delete it and the copy of the unit
  s_config_Block( v.tub , v.bsc );
  delete( v.bsc );
  delete( v.tub );
  }

 // terminate - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 return( AllPassed ? 0 : 1 );

 }  // end( main )

/*--------------------------------------------------------------------------*/
/*-------------------------- File test_tudps.cpp ---------------------------*/
/*--------------------------------------------------------------------------*/
