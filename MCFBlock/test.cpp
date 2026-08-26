/*--------------------------------------------------------------------------*/
/*-------------------------- File test.cpp ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Main for testing MCFBlock and MCFSolver.
 *
 * Reads an instance of a MCF from a file (in either DIMACS or netCDF format)
 * in an MCFBlock. Two Solver (one supposed to the "physical", say some
 * MCFSolver, and the other "abstract", say a MILPSolver) are registered to
 * the MCFBlock and compute()-d, and the results are compared. The process is
 * repeated many times with the MCF problem randomly changed (costs /
 * capacities / deficits, arcs openings / closures, and arcs additions /
 * deletions). This tests both the MCFBlock and both Solver.
 *
 * \author Antonio Frangioni \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Antonio Frangioni
 */
/*--------------------------------------------------------------------------*/
/*------------------------------ DEFINES -----------------------------------*/
/*--------------------------------------------------------------------------*/

#define LOG_LEVEL 0
// 0 = only pass/fail
// 1 = result of each test

/*--------------------------------------------------------------------------*/
// several MCFSolver won't work properly without properly setting the
// numerical tolerances EpsFlw and EpsCst; if this macro is set to nonzero,
// the data of the problem is read and used to find the proper scaling
// factor needed to properly setting EpsFlw and EpsCst (via their "proxies"
// Solver::dblAbsAcc and CDASolver::dblAAccDSol, respectively). this is done
// in the first Solver registered to the MCFBlock, which is the one assumed
// to be is a MCFSolver

#define SET_EPS 0

/*--------------------------------------------------------------------------*/
// several MCFSolver won't work properly if costs and/or deficit/capacities
// are not integer. this macro allows to ensure that newly generated random
// costs and/or deficit/capacities are integer. It is coded bitwise:
// - SET_INT & 1 > 0: costs are integer
// - SET_INT & 2 > 0: deficit/capacities are integer
// note that this will not be much helpful unless the original instance data
// is integer to start with

#define SET_INT 3

#if SET_INT & 1
 #define intcost( x ) std::round( x )
#else
 #define intcost( x ) x
#endif

#if SET_INT & 2
 #define intflow( x ) std::round( x )
#else
 #define intflow( x ) x
#endif

/*--------------------------------------------------------------------------*/
// if nonzero, the 1st Solver attached to the MCFBlock is detached and
// re-attached to it at all iterations

#define DETACH_1ST 0

// if nonzero, the 2nd Solver attached to the MCFBlock is detached and
// re-attached to it at all iterations

#define DETACH_2ND 0

/*--------------------------------------------------------------------------*/
// if nonzero, the MCFBlock is not solved at every round of changes, but
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

#if( LOG_LEVEL >= 1 )
 #define LOG1( x ) std::cout << x
 #define CLOG1( y , x ) if( y ) std::cout << x
 #define CLOG2( y , x , z ) if( y ) std::cout << x; else std::cout << z
#else
 #define LOG1( x )
 #define CLOG1( y , x )
 #define CLOG2( y , x , z )
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
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <fstream>
#include <iomanip>

#include <random>

#include "common_utils.h"

#include "MCFBlock.h"
#if SET_EPS
 #include "CDASolver.h"
#endif

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*------------------------- TYPES & CONSTEXPRS -----------------------------*/
/*--------------------------------------------------------------------------*/

using Index = Block::Index;
using c_Index = Block::c_Index;
static constexpr Index IInf = SMSpp_di_unipi_it::Inf< Index >();

using FNumber = MCFBlock::FNumber;
static constexpr FNumber FInf = SMSpp_di_unipi_it::Inf< FNumber >();

using CNumber = MCFBlock::CNumber;
static constexpr CNumber CInf = SMSpp_di_unipi_it::Inf< CNumber >();

using Range = Block::Range;
using c_Range = Block::c_Range;

using Subset = Block::Subset;
using c_Subset = Block::c_Subset;

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/

MCFBlock * MCFB = nullptr;     // the MCFBlock

std::mt19937 rg;               // base random generator
std::uniform_real_distribution<> dis( 0.0 , 1.0 );

// set by SolveBoth(): did the just-solved instance admit a (primal) solution?
// used by the emergency feasibility-recovery mechanism in the main loop
bool last_round_feasible = true;

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
// return a random number in [ 0.5 , 2 ] so that the probability of being
// p > 1 is the same as the probability of being 1 / p < 1: in this way the
// modified numbers should, on average, retain the same order of magnitude
// of the original ones even after being modified very many times

static double rndfctr( void )
{
 auto val = 2 * dis( rg );
 if( val < 1 )
  val = 1 / ( val + 1 );
 #if SET_EPS
  //!! ensure few digits after the point: this may help the MCFSolver that
  //!! have originally been constructed with integers in mind
  val = double( int( val * 1000 ) ) / 1000;
 #endif

 return( val ); 
 }

/*--------------------------------------------------------------------------*/

static LinearFunction * LF( Objective * obj )
{
 return( static_cast< LinearFunction * >( static_cast< FRealObjective *
					  >( obj )->get_function() ) );
 }

/*--------------------------------------------------------------------------*/
// generate a (sorted) random k-vector of unique integers in 0 ... m - 1

static Subset GenerateRand( Index m , Index k , bool ord = true )
{
 if( k > m ) {
  std::cerr << "error: GenerateRand( " << m << " , " << k << " )"
	    << std::endl;
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
// remove any element >= m from the given Subset

static void Compact( Subset & nms , Index m )
{
 auto it = nms.begin();
 while( ( it != nms.end() ) && ( *it < m ) )
  ++it;

 if( it == nms.end() )
  return;

 for( auto nit = it ; ++nit != nms.end() ; )
  if( *nit < m )
   *(it++) = *nit;

 nms.resize( std::distance( nms.begin() , it ) );
 }

/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/// checks that the solution of each Solver is a flow of the cost it reports
/** Asks every registered Solver that has one for its solution [see
 * Solver::get_Solution()] and checks that what comes back is a feasible flow
 * of the very cost the Solver reports: the bounds of each arc, the flow
 * conservation at each node and the cost.
 *
 * Comparing the values alone, which is what the cross-check does, cannot see
 * a solution that is right in value but wrong in the flows: a permutation of
 * the arcs, say, leaves the cost untouched as long as the permuted flow is
 * still a flow of the same cost. This is exactly what happened, unnoticed for
 * a long time, with a :Solver reporting the flows in the order of its own
 * internal enumeration rather than in that of the MCFBlock. */

static bool CheckFlows( void )
{
 const Index m = MCFB->get_NArcs();
 const Index n = MCFB->get_NNodes();

 bool ok = true;
 Index k = 0;

 for( auto slv : MCFB->get_registered_solvers() ) {
  const Index kk = k++;

  if( ! slv->has_var_solution() )  // nothing to check
   continue;

  auto sol = dynamic_cast< MCFSolution * >( slv->get_Solution() );
  if( ( ! sol ) || sol->get_x().empty() ) {  // no flows in there
   delete sol;
   continue;
   }

  auto & x = sol->get_x();

  // the tolerances are relative to the size of the flow
  double scale = 1;
  for( Index a = 0 ; a < m ; ++a )
   if( ! MCFB->is_deleted( a ) )
    scale = std::max( scale , std::abs( x[ a ] ) );
  const double eps = 1e-9 * scale;

  // the bounds of each arc, a closed one being bound to 0 - - - - - - - - -
  for( Index a = 0 ; a < m ; ++a ) {
   if( MCFB->is_deleted( a ) )
    continue;
   const double ua = MCFB->is_closed( a ) ? 0 : MCFB->get_U( a );
   if( ( x[ a ] < - eps ) || ( x[ a ] > ua + eps ) ) {
    std::cout << std::endl << "S" << kk << ": flow " << x[ a ]
              << " out of [ 0 , " << ua << " ] on arc " << a;
    ok = false;
    break;
    }
   }

  // the flow conservation at each node - - - - - - - - - - - - - - - - - - -
  MCFBlock::Vec_FNumber dfct( n , 0 );
  for( Index a = 0 ; a < m ; ++a ) {
   if( MCFB->is_deleted( a ) )
    continue;
   dfct[ MCFB->get_SN( a ) - 1 ] -= x[ a ];
   dfct[ MCFB->get_EN( a ) - 1 ] += x[ a ];
   }

  for( Index i = 0 ; i < n ; ++i )
   if( std::abs( dfct[ i ] - MCFB->get_B( i ) ) > eps ) {
    std::cout << std::endl << "S" << kk << ": deficit " << dfct[ i ]
              << " instead of " << MCFB->get_B( i ) << " at node " << i;
    ok = false;
    break;
    }

  // the cost of the flow is the value the Solver reports - - - - - - - - - -
  double cost = 0;
  for( Index a = 0 ; a < m ; ++a )
   if( ! MCFB->is_deleted( a ) )
    cost += MCFB->get_C( a ) * x[ a ];

  const double v = slv->get_var_value();
  if( std::abs( cost - v ) > 5e-7 * std::max( double( 1 ) , std::abs( v ) ) ) {
   std::cout << std::endl << "S" << kk << ": the flow costs " << cost
             << " but the value is " << v;
   ok = false;
   }

  delete sol;
  }

 return( ok );

 }  // end( CheckFlows )

/*--------------------------------------------------------------------------*/

static bool SolveBoth( void )
{
 // optional Solver re-ordering before the cross-check (front/back), then defer
 // to the common_utils engine: every registered Solver is solved, the uniform
 // per-instance line with all values is printed, and the two exact optima must
 // agree (tol 5e-7); the all-infeasible case is accepted as OK(e)
 #if DETACH_1ST
  auto Slvr1 = MCFB->get_registered_solvers().front();
  MCFB->unregister_Solver( Slvr1 );
  MCFB->register_Solver( Slvr1 , true );  // push it to the front
 #endif
 #if DETACH_2ND
  auto Slvr2 = MCFB->get_registered_solvers().back();
  MCFB->unregister_Solver( Slvr2 );
  MCFB->register_Solver( Slvr2 );  // push it to the back
 #endif

 bool hs1 = false;
 bool ok = SolveAll( MCFB , exact_getter( ObjGetter::VarValue ) ,
                     std::numeric_limits< double >::quiet_NaN() ,
                     5e-7 , nullptr , & hs1 );
 last_round_feasible = hs1;  // the 1st (physical) Solver found a solution

 /* The cross-check only compares the values: the solutions themselves are
  * checked here, one Solver at a time, against the data of the MCFBlock. */
 ok &= CheckFlows();

 return( ok );
 }

/*--------------------------------------------------------------------------*/
// emergency feasibility-recovery helpers: roll specific pieces of the
// instance data back to the values saved right after deserialization (with
// which the instance is assumed feasible). See the main loop for the staging.

static void restore_deficits( const MCFBlock::Vec_FNumber & orig )
{
 // deficits are per-node and the node count never changes in this test, so
 // the saved vector always aligns; this undoes the deficit drift that is the
 // usual cause of permanent infeasibility
 MCFB->chg_dfcts( orig.begin() , Range( 0 , orig.size() ) );
 }

/*--------------------------------------------------------------------------*/

static void restore_capacities( const MCFBlock::Vec_FNumber & orig )
{
 // restore the original capacity of every arc that still exists at its
 // original index, undoing the capacity drift. Arc indices are stable except
 // for deleted slots (skipped here) and slots later reused by add_arc (only
 // possible when arcs were deleted first, i.e. never in the no-deletion runs);
 // a reused slot gets an arbitrary-but-harmless value. With no deletions the
 // whole capacity vector is restored exactly, so combined with the deficit
 // restore and the arc re-opening the original instance is fully recovered.
 if( orig.empty() )  // uncapacitated instance: nothing to restore
  return;
 Index m = std::min( Index( orig.size() ) , MCFB->get_NArcs() );
 for( Index i = 0 ; i < m ; ++i )
  if( ! MCFB->is_deleted( i ) )
   MCFB->chg_ucap( orig[ i ] , i );
 }

/*--------------------------------------------------------------------------*/

// test-specific command-line knobs, set by process_specific_arg(); the
// standard parameters (instance positional, -S BlockSolverConfig, -c/-p
// prefixes) are handled centrally by common_utils
long int seed = 1;
unsigned int wchg = 255;
double p_change = 0.5;
Index n_change = 10;
Index n_repeat = 40;

/*--------------------------------------------------------------------------*/

static bool process_specific_arg( int opt )
{
 switch( opt ) {
  case( 'e' ): Str2Sthg( optarg , seed );      return( true );
  case( 'k' ): Str2Sthg( optarg , wchg );      return( true );
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

 docopt_desc = "SMS++ MCFBlock-vs-MILP test.\n";
 short_opts += "e:k:n:m:q:";
 const std::vector< option > my_opts = {
   { "seed"    , required_argument , nullptr , 'e' } ,
   { "wchg"    , required_argument , nullptr , 'k' } ,
   { "rounds"  , required_argument , nullptr , 'n' } ,
   { "nchng"   , required_argument , nullptr , 'm' } ,
   { "pchng"   , required_argument , nullptr , 'q' } };
 long_opts.insert( std::prev( long_opts.end() ) ,
                   my_opts.begin() , my_opts.end() );
 help += "  -e, --seed <n>                  pseudo-random generator seed [1]\n"
         "  -k, --wchg <bits>               what to change, bit-wise [255]:\n"
         "                                    1 costs, 2 capacities, "
         "4 deficits, 8 close arcs,\n"
         "                                    16 re-open arcs, 32 delete arcs, "
         "64 create arcs,\n"
         "                                    128 use abstract representation\n"
         "  -n, --rounds <n>                number of changing rounds [40]\n"
         "  -m, --nchng <n>                 avg number of elements to change "
         "[10]\n"
         "  -q, --pchng <p>                 probability of any single change "
         "[0.5]\n";

 process_args( argc , argv , process_specific_arg );

 // the BlockSolverConfig (-S) must be provided explicitly: the test never
 // falls back to a hardcoded default Configuration
 require_solver_config();

 // construction and loading of the objects - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 std::string fn( filename );
 if( fn.substr( fn.size() - 4 , 4 ) == ".nc4" ) {
  MCFB = dynamic_cast< MCFBlock * >( Block::deserialize( fn ) );
  if( ! MCFB ) {
   std::cerr << "Error: " << fn << " does not contain a MCFBlock"
	     << std::endl;
   return( 1 );
   }
  }
 else {
  MCFB = new MCFBlock;
  MCFB->Block::load( fn );
  // why the Block:: should be necessary evades me, but it seems it is
  }

 // attach the Solver(s) to the MCFBlock- - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // do this by reading an appropriate BlockSolverConfig from file and
 // apply() it to the MCFBlock; note that the BlockSolverConfig is
 // clear()-ed and kept to do the cleanup at the end.
 // BSC may be a plain BlockSolverConfig or a meta-config
 // SimpleConfiguration< std::map< std::string , Configuration * > >;
 // s_config_Block() dispatches on the runtime type and clears the config(s)
 // for final cleanup.

 Configuration * bsc = Configuration::deserialize( sconf_file );
 if( ! bsc ) {
  std::cerr << "Error: cannot load BSC from " << sconf_file << std::endl;
  return( 1 );
  }
 s_config_Block( MCFB , bsc , sconf_file );

 if( MCFB->get_registered_solvers().size() < 2 ) {
  std::cout << "too few Solver registered to MCFB!" << std::endl;
  return( 1 );
  }

 // save the original deficits and capacities, with which the instance is
 // assumed feasible: used by the emergency feasibility-recovery mechanism in
 // the main loop (see there). Taken right after deserialization, before any
 // change is applied.
 const MCFBlock::Vec_FNumber orig_dfct( MCFB->get_B() );
 const MCFBlock::Vec_FNumber orig_ucap( MCFB->get_U() );

 // compute min/max cost & max deficit- - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 if( n_change > MCFB->get_NArcs() )
  n_change = MCFB->get_NArcs();

 CNumber c_max = 0;
 CNumber c_min = 0;
 CNumber c_abs = 0;
 auto & C = MCFB->get_C();
 if( ! C.empty() ) {
  auto mm = std::minmax_element( C.begin() , C.end() );
  c_min = *(mm.first);
  c_max = *(mm.second);  
  c_abs = std::abs( *std::max_element( C.begin() , C.end() ,
				       []( auto a , auto b ) {
					return( std::abs( a ) <
						std::abs( b ) ); } ) );
  }

 CNumber b_abs = 0;
 auto & B = MCFB->get_B();
 if( ! B.empty() )
  b_abs = *std::max_element( B.begin() , B.end() );

 bool nzdfct = ( b_abs > 0 );

 FNumber u_max = FInf;
 FNumber u_min = FInf;
 FNumber u_avg = FInf;
 auto & U = MCFB->get_U();
 if( ! U.empty() ) {
  auto mm = std::minmax_element( U.begin() , U.end() );
  u_min = *(mm.first);
  u_max = *(mm.second);  
  u_avg = std::accumulate( U.begin() , U.end() , 0 ) / MCFB->get_NArcs();
  }

 // set epsilons in MCFSolver - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 #if( SET_EPS )
 {
  auto MCFS = MCFB->get_registered_solvers().front();

  static constexpr double BA = 1e-12;  // base accuracy

  MCFS->set_par( Solver::dblAbsAcc ,
		 BA * std::max( std::max( b_abs , u_max ) , double( 1 ) ) );
  MCFS->set_par( CDASolver::dblAAccDSol ,
		 BA * std::max( c_abs , double( 1 ) ) ); 
  }
 #endif

 // first solver call - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 bool AllPassed = SolveBoth();

 // number of consecutive infeasible solves; drives the staged emergency
 // feasibility recovery in the main loop. Seeded from the very first solve.
 unsigned int consec_infeas = last_round_feasible ? 0 : 1;

 // main loop - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // now, for n_repeat times:
 // - up tp n_change costs are changed
 // - up to n_change capacities are changed
 // - if the problem is not a circulation problem, 2 deficits are modified
 //   (adding and subtracting the same number), otherwise two opposite
 //   deficits are created in two random nodes
 // - up to n_change arcs are closed
 // - up to n_change arcs are re-opened
 // - up to n_change arcs are deleted (either "in the middle" or "at the
 //   end"
 // - up to n_change arcs are added (wherever they fall)
 // this is done for SKIP_BEAT + 1 times, then the two problems are
 // re-compute()-d and their results compared

 rg.seed( seed );  // seed the pseudo-random number generator

 for( Index rep = 0 ; rep < n_repeat * ( SKIP_BEAT + 1 ) ; ) {
  if( ! AllPassed ) {
   std::ofstream f( "mcf.dmx" );
   MCFB->print( f , 'C' );
   f.close();
   break;
   }

  LOG1( rep << ": ");

  // change costs - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 1 ) && ( dis( rg ) <= p_change ) ) {
   auto tochange = std::max( Index( 1 ) , Index( dis( rg ) * n_change ) );

   LOG1( tochange << " cost" );

   auto lf = ( ( wchg & 128 ) && ( dis( rg ) < 0.5 ) )
           ? LF( MCFB->get_objective() ) : nullptr;

   if( tochange == 1 ) {
    CNumber newcst = intcost( c_min + CNumber( dis( rg ) * ( c_max - c_min ) ) );
    Index arc = Index( dis( rg ) * ( MCFB->get_NArcs() - 1 ) );

    if( lf ) {  // change via abstract representation
     LOG1( "(a)" );
     lf->modify_coefficient( arc , newcst );
     }
    else  // change via call to chg_* method
     MCFB->chg_cost( newcst , arc );

    LOG1( " - " );
    }
   else {
    MCFBlock::Vec_CNumber newcsts( tochange );
    for( Index i = 0 ; i < tochange ; ++i )
     newcsts[ i ] = intcost( c_min + CNumber( dis( rg ) * ( c_max - c_min ) ) );

    // in 50% of the cases do a ranged change, in the others a sparse change
    if( dis( rg ) <= 0.5 ) {
     Index strt = dis( rg ) * ( MCFB->get_NArcs() - tochange );
     Index stp = strt + tochange;

     if( lf ) {  // change via abstract representation
      LOG1( "s(r,a) - " );
      lf->modify_coefficients( std::move( newcsts ) , Range( strt , stp ) );
      }
     else {  // change via call to chg_* method
      MCFB->chg_costs( newcsts.begin() , Range( strt , stp ) );
      LOG1( "s(r) - " );
      }
     }
    else {
     bool ord = ( dis( rg ) < 0.5 );
     auto nms = GenerateRand( MCFB->get_NArcs() , tochange , ord );

     if( lf ) {  // change via abstract representation
      lf->modify_coefficients( std::move( newcsts ) , std::move( nms ) ,
			       ord );
      CLOG2( ord , "s(s,a) - " , "s(s,a,u) - " );
      }
     else {  // change via call to chg_* method
      MCFB->chg_costs( newcsts.begin() , std::move( nms ) , ord );
      CLOG2( ord , "s(s) - " , "s(s,u) - " );
      }
     }
    }
   }  // end( if( change costs ) )

  // change capacities- - - - - - - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 2 ) && ( dis( rg ) <= p_change ) ) {
   auto tochange = std::max( Index( 1 ) , Index( dis( rg ) * n_change ) );
   LOG1( tochange << " capacit" );

   if( tochange == 1 ) {
    auto arc = Index( dis( rg ) * ( MCFB->get_NArcs() - 1 ) );
    auto newcap = intflow( MCFB->get_U( arc ) * rndfctr() );

    if( ( wchg & 128 ) && ( dis( rg ) < 0.5 ) ) {
     // change via abstract representation
     LOG1( "y(a) - " );
     MCFB->i2p_ub( arc )->set_rhs( newcap );
     }
    else {  // change via call to chg_* method
     MCFB->chg_ucap( newcap , arc );
     LOG1( "y - " );
     }
    }
   else {
    MCFBlock::Vec_FNumber newcaps( tochange );

    // in 50% of the cases do a ranged change, in the others a sparse change
    if( dis( rg ) <= 0.5 ) {
     Index strt = dis( rg ) * ( MCFB->get_NArcs() - tochange );
     Index stp = strt + tochange;
     for( Index i = 0 ; i < tochange ; ++i )
      newcaps[ i ] = intflow( MCFB->get_U( i + strt ) * rndfctr() );

     if( ( wchg & 128 ) && ( dis( rg ) < 0.5 ) ) {
      // change via abstract representation, sending to a new channel
      LOG1( "ies(a,r) - " );
      auto chnl = MCFB->open_channel();
      auto modpar = Observer::make_par( eModBlck , chnl );
      for( Index i = 0 ; i < tochange ; ++i )
       MCFB->i2p_ub( i + strt )->set_rhs( newcaps[ i ] , modpar );
      MCFB->close_channel( chnl );
      }
     else {  // change via call to chg_* method
      MCFB->chg_ucaps( newcaps.begin() , Range( strt , stp ) );
      LOG1( "ies(r) - " );
      }
     }
    else {
     bool ord = ( dis( rg ) < 0.5 );
     auto nms = GenerateRand( MCFB->get_NArcs() , tochange , ord );
     auto ncit = newcaps.begin();
     for( auto i : nms )
      *(ncit++) = intflow( MCFB->get_U( i ) * rndfctr() );

     if( ( wchg & 128 ) && ( dis( rg ) < 0.5 ) ) {
      // change via abstract representation, sending to a new channel
      LOG1( "ies(a,s) - " );
      auto chnl = MCFB->open_channel();
      auto modpar = Observer::make_par( eModBlck , chnl );
      for( Index i = 0 ; i < tochange ; ++i )
       MCFB->i2p_ub( nms[ i ] )->set_rhs( newcaps[ i ] , modpar );
      MCFB->close_channel( chnl );
      }
     else {  // change via call to chg_* method
      MCFB->chg_ucaps( newcaps.begin() , std::move( nms ) , ord );
      CLOG2( ord , "ies(s) - " , "iess(s,u) - " );
      }
     }
    }
   }  // end( if( change capacities ) )

  // change deficits- - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 4 ) && ( dis( rg ) <= p_change ) ) {
   LOG1( "2 deficits" );

   Index posn = 0;
   Index negn = 0;
   FNumber posd = NAN;
   FNumber negd = NAN;
   auto n = MCFB->get_NNodes();

   if( nzdfct ) {  // if there are nonzero deficits
    auto & dfcts = MCFB->get_B();

    do
     posn = Index( dis( rg ) * n );  // select node with positive
    while( dfcts[ posn ] <= 0 );     // deficit (one must exist)
    posd = dfcts[ posn ];

    do
     negn = Index( dis( rg ) * n );  // select node with negative
    while( dfcts[ negn ] >= 0 );     // deficit (one must exist)
    negd = dfcts[ negn ];
    }
   else {
    posn = Index( dis( rg ) * n );   // just select at random
    negn = Index( dis( rg ) * n );
    posd = negd = 0;
    nzdfct = true;
    }

   FNumber Dlt = intflow( u_avg * 2 * dis( rg ) );
   if( dis( rg ) <= 0.5 ) {  // in 50% of cases up, in 50% of cases down
    posd += Dlt;
    negd -= Dlt;
    }
   else {
    Dlt = intflow( std::min( Dlt , std::max( std::max( posd , - negd ) / 2 ,
					     double( 1 ) ) ) );
    posd -= Dlt;
    negd += Dlt;
    }

   // pack the two Modification into a new channel
   auto chnl = MCFB->open_channel();
   auto modpar = Observer::make_par( eModBlck , chnl );

   if( ( wchg & 128 ) && ( dis( rg ) < 0.5 ) ) {
    // change via abstract representation
    LOG1( "(a)" );
    MCFB->i2p_e( posn )->set_both( posd , modpar );
    MCFB->i2p_e( negn )->set_both( negd , modpar );
    }
   else {  // change via call to chg_* method
    // note that eModBlck makes no sense for a physical Modification,
    // but MCFBlock is supposed to take care of this
    MCFB->chg_dfct( posd , posn , modpar , modpar );
    MCFB->chg_dfct( negd , negn , modpar , modpar );
    }

   MCFB->close_channel( chnl );

   LOG1( " - " );

   }  // end( change deficits )

  // closing arcs- - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 8 ) && ( dis( rg ) <= p_change ) ) {
   // "slightly" over-provision tochange to account for the fact that some
   // arcs may end up not being closed since they are either closed already
   // or deleted
   auto tochange = std::max( Index( 1 ) ,
			     std::min( MCFB->get_NArcs() ,
				       Index( dis( rg ) * 1.5 * n_change ) )
			     );

   // in 50% of the cases do a ranged change, in the others a sparse change
   if( dis( rg ) <= 0.5 ) {
    Index strt = dis( rg ) * ( MCFB->get_NArcs() - tochange );
    Index stp = strt + tochange;

    LOG1( tochange << " close" );

    if( ( wchg & 128 ) && ( dis( rg ) < 0.5 ) ) {
     // change via abstract representation, sending to a new channel
     LOG1( "(a) - " );
     auto modpar = MCFB->open_if_needed( eModBlck , tochange );
     for( Index i = strt ; i < stp ; ++i ) {
      auto *x = MCFB->i2p_x( i );
      x->set_value( 0 );
      x->is_fixed( true , modpar );
      }
     MCFB->close_if_needed( modpar , tochange );
     }
    else {  // change via call to close method
     MCFB->close_arcs( Range( strt , stp ) );
     LOG1( " - " );
     }
    }
   else {
    bool ord = ( dis( rg ) < 0.5 );
    auto nms = GenerateRand( MCFB->get_NArcs() , tochange , ord );
    for( auto & i : nms )
     if( MCFB->is_deleted( i ) || MCFB->is_closed( i ) ) {
      i = IInf;
      --tochange;
      }

    if( tochange != nms.size() )
     Compact( nms , MCFB->get_NArcs()  );

    LOG1( tochange << " close" );

    if( ( wchg & 128 ) && ( dis( rg ) < 0.5 ) ) {
     // change via abstract representation
     CLOG2( ord , "(s,a) - " , "(s,a,u) - " );
     auto modpar = MCFB->open_if_needed( eModBlck , tochange );
     for( auto i : nms ) {
      auto *x = MCFB->i2p_x( i );
      x->set_value( 0 );
      x->is_fixed( true , modpar );
      }
     MCFB->close_if_needed( modpar , tochange );
     }
    else {  // change via call to close method
     CLOG2( ord , "(s) - " , "(s,u) - " );
     MCFB->close_arcs( std::move( nms ) , ord );
     }
    }
   }

  // re-opening arcs - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 16 ) && ( dis( rg ) <= p_change ) ) {
  // "slightly" over-provision tochange to account for the fact that some
   // arcs may end up not being opened since they are either open already
   // or deleted
   auto tochange = std::max( Index( 1 ) ,
			     std::min( MCFB->get_NArcs() ,
				       Index( dis( rg ) * 1.5 * n_change ) )
			     );

   // in 50% of the cases do a ranged change, in the others a sparse change
   if( dis( rg ) <= 0.5 ) {
    Index strt = dis( rg ) * ( MCFB->get_NArcs() - tochange );
    Index stp = strt + tochange;

    LOG1( tochange << " open" );

    if( ( wchg & 128 ) && ( dis( rg ) < 0.5 ) ) {
     // change via abstract representation, sending to a new channel
     LOG1( "(a) - " );
     auto modpar = MCFB->open_if_needed( eModBlck , tochange );
     for( Index i = strt ; i < stp ; ++i )
      if( ! MCFB->is_deleted( i ) )
       MCFB->i2p_x( i )->is_fixed( false , modpar );
     MCFB->close_if_needed( modpar , tochange );
     }
    else {  // change via call to open method
     MCFB->open_arcs( Range( strt , stp ) );
     LOG1( " - " );
     }
    }
   else {
    bool ord = ( dis( rg ) < 0.5 );
    auto nms = GenerateRand( MCFB->get_NArcs() , tochange , ord );
    for( auto & i : nms )
     if( MCFB->is_deleted( i ) || ( ! MCFB->is_closed( i ) ) ) {
      i = IInf;
      --tochange;
      }

    if( tochange ) {
     if( tochange != nms.size() )
      Compact( nms , MCFB->get_NArcs() );

     LOG1( tochange << " open" );

     if( ( wchg & 128 ) && ( dis( rg ) < 0.5 ) ) {
      // change via abstract representation
      CLOG2( ord , "(s,a) - " , "(s,a,u) - " );
      auto modpar = MCFB->open_if_needed( eModBlck , tochange );
      for( auto i : nms )
       MCFB->i2p_x( i )->is_fixed( false , modpar );
      MCFB->close_if_needed( modpar , tochange );
      }
     else {  // change via call to open method
      CLOG2( ord , "(s) - " , "(s,u) - " );
      MCFB->open_arcs( std::move( nms ) , ord );
      }
     }
    }
   }

  // deleting arcs - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  Index da = MCFB->get_NArcs() - MCFB->get_NStaticArcs();
  if( ( da > 0 ) && ( wchg & 32 ) && ( dis( rg ) <= p_change ) ) {
   // "slightly" over-provision tochange to account for the fact that some
   // arcs may end up not being deleted since they are either deleted already
   auto tochange = std::max( Index( 1 ) ,
			     std::min( da ,
				       Index( dis( rg ) * 1.3 * n_change ) )
			     );

   if( dis( rg ) < 0.5 ) {  // delete somewhere in the middle
    auto nsa =  MCFB->get_NStaticArcs();
    bool ord = ( dis( rg ) < 0.5 );
    auto nms = GenerateRand( da , tochange , ord );
    for( auto i : nms ) {
     i += nsa;
     if( MCFB->is_deleted( i ) )
      --tochange;
     else
      MCFB->remove_arc( i );
     }

    LOG1( tochange << " delete(m" );
    CLOG2( ord , ") - " , ",u) - " );
    }
   else {  // delete at the end
    Index changed = 0;
    for( auto i = MCFB->get_NArcs() ; --i >= MCFB->get_NStaticArcs() ; ) {
     if( MCFB->is_deleted( i ) )
      continue;
     if( dis( rg ) <= 0.13 )
      break;

     MCFB->remove_arc( i );
     if( ++changed >= tochange )
      break;
     }

    LOG1( changed << " delete(e) - " );
    }
   }

  // creating new arcs - - - - - - - - - - - - - - - - - - - - - - - - - - -

  da = MCFB->get_NArcs() - MCFB->get_NStaticArcs();
  if( ( da > 0 ) && ( wchg & 64 ) && ( dis( rg ) <= p_change ) ) {
   Index changed = 0;
   Index afterend = 0;
   while( changed < n_change ) {
    if( dis( rg ) <= 0.10 )
     break;

    // random sn != en
    Index sn = 0;
    Index en = 0;
    auto n = MCFB->get_NNodes();
    do {
     sn = dis( rg ) * n + 1;
     en = dis( rg ) * n + 1;
     } while( sn == en );

    // random cost in [ - c_max , c_max ]
    auto cst = intcost( c_max * ( 1 - 2 * dis( rg ) ) );

    // random capacity in [ u_min , 1.5 * ( u_avg - u_min ) ]
    auto cap = intflow( 1.5 * ( u_avg - u_min ) * dis( rg ) + u_min );

    auto arc = MCFB->add_arc( sn , en , cst , cap );  // try to add
    if( arc == IInf )  // there was no space
     break;            // no sense to try again

    ++changed;
    if( arc == MCFB->get_NArcs() - 1 )
     ++afterend;
    }

   CLOG1( changed , "create " << changed << "(" << afterend << ") - " );
   }

  // finally, re-solve the problems- - - - - - - - - - - - - - - - - - - - -
  // ... every SKIP_BEAT + 1 rounds

  if( ! ( ++rep % ( SKIP_BEAT + 1 ) ) ) {
   // emergency feasibility recovery - - - - - - - - - - - - - - - - - - - - -
   // if recent solves were infeasible, progressively roll the instance back
   // toward the original (feasible) data *before* re-solving, so the rollback
   // overrides this cycle's changes. Stage 1 (>= 1 consecutive infeasible
   // solve) restores the original deficits, the usual culprit; stage 2 also
   // restores the original static-arc capacities; stage 3 also re-opens every
   // closed arc. With deficits + capacities + all arcs restored the original
   // feasible region is recovered, so feasibility should return (barring
   // original dynamic arcs permanently deleted meanwhile, which cannot be
   // brought back). The staging both isolates the culprit and self-heals.
   if( consec_infeas >= 1 ) restore_deficits( orig_dfct );
   if( consec_infeas >= 2 ) restore_capacities( orig_ucap );
   if( consec_infeas >= 3 ) MCFB->open_arcs();

   AllPassed &= SolveBoth();

   consec_infeas = last_round_feasible ? 0 : ( consec_infeas + 1 );
   }
  #if( LOG_LEVEL >= 1 )
  else
   std::cout << std::endl;
  #endif

  }  // end( main loop )- - - - - - - - - - - - - - - - - - - - - - - - - - -
     // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 if( AllPassed )
  std::cout << GREEN( All test passed!! ) << std::endl;
 else
  std::cout << RED( Shit happened!! ) << std::endl;

 // destroy objects - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 // apply() the clear()-ed BlockSolverConfig (or meta-config) to cleanup Solver
 s_config_Block( MCFB , bsc );

 // then delete the BlockSolverConfig
 delete( bsc );

 // finally the MCFBlock can be deleted
 delete( MCFB );

 // terminate - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 return( AllPassed ? 0 : 1 );

 }  // end( main )

/*--------------------------------------------------------------------------*/
/*------------------------ End File test.cpp -------------------------------*/
/*--------------------------------------------------------------------------*/
