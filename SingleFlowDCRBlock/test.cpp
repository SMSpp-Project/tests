/*--------------------------------------------------------------------------*/
/*-------------------------- File test.cpp ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Tester of SingleFlowDCRBlock.
 *
 * Builds a random single-flow Delay-Constrained Routing instance (or reads
 * one out of a netCDF file), hands it to all the Solver that the
 * BlockSolverConfig registers to it and cross-checks what they answer with
 * SolveAll(); the exact and the heuristic ones can therefore be mixed
 * freely, each being held to what it declares [see @ref solver_eps].
 *
 * The instance is generated around a random source-sink path whose arcs are
 * given enough capacity to meet the deadline, so that it is always feasible
 * and the optimum is finite; the deadline is otherwise tight enough for the
 * delay constraint to matter, which is the point of the exercise.
 *
 * On top of the values, the Solution that each Solver produces [see
 * Solver::get_Solution()] is checked: the routing and the reserved rates it
 * carries have to satisfy the constraints of the DCR problem, which
 * SingleFlowDCRBlock::is_sol_feasible() answers without looking at the
 * Variable of the Block at all, and the cost they give has to be the value
 * the Solver declares. The Variable are deliberately scrambled before
 * asking, so that a Solver that builds its Solution out of them rather than
 * out of its own data structures is caught.
 *
 * \author Antonio Frangioni \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \author Donato Meoli \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Antonio Frangioni, Donato Meoli
 */
/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include "common_utils.h"

#include "SingleFlowDCRBlock.h"

#include <algorithm>
#include <memory>
#include <string>
#include <numeric>
#include <random>
#include <sstream>

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

using Index = Block::Index;
using Subset = Block::Subset;
using c_double = const double;

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/

Index n_nodes = 10;   ///< number of nodes of the random instance (-n)
Index n_arcs = 30;    ///< number of arcs of the random instance (-m)
long seed = 1;        ///< seed of the random instance (-e)
double tightness = 1.2;
///< the deadline is this times the delay of the "guaranteed" path (-t)

std::mt19937 rg;      ///< the random generator of the instance

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/
/// a random double uniformly distributed in [ lo , hi ]

static double rnd( double lo , double hi )
{
 return( std::uniform_real_distribution< double >( lo , hi )( rg ) );
 }

/*--------------------------------------------------------------------------*/
/// builds a random (feasible) single-flow DCR instance
/** Constructs a random DCR instance and loads it into block. The network is
 * a random simple path from node 1 to node n_nodes, which is what makes the
 * instance feasible, plus random extra arcs; every arc gets a random cost,
 * a random capacity of at least the sustained rate rho, and random
 * propagation/processing delays. The deadline is the delay that the
 * "guaranteed" path incurs when every one of its arcs reserves its whole
 * capacity, times tightness: with tightness >= 1 there is at least one
 * feasible routing, and the closer to 1 it is the fewer of them there
 * are. */

static void build_random_instance( SingleFlowDCRBlock * block )
{
 c_double MTU = 1;
 c_double rho = 1;
 c_double burst = 1;

 // the nodes of the "guaranteed" path: 1, then a random subset of the
 // internal nodes in random order, then n_nodes
 Subset path( n_nodes - 2 );
 std::iota( path.begin() , path.end() , 2 );
 std::shuffle( path.begin() , path.end() , rg );
 path.resize( std::min( Index( path.size() ) , std::max( Index( 1 ) ,
                                                         n_nodes / 2 ) ) );
 path.insert( path.begin() , 1 );
 path.push_back( n_nodes );

 if( n_arcs < path.size() - 1 )
  n_arcs = path.size() - 1;

 Subset SN( n_arcs ) , EN( n_arcs );
 std::vector< double > U( n_arcs ) , C( n_arcs ) , LD( n_arcs );

 // the arcs of the path first, so that their index is known
 for( Index i = 0 ; i < path.size() - 1 ; ++i ) {
  SN[ i ] = path[ i ];
  EN[ i ] = path[ i + 1 ];
  }

 // then the random ones, avoiding loops (parallel arcs are fine)
 for( Index i = path.size() - 1 ; i < n_arcs ; ++i ) {
  do {
   SN[ i ] = std::uniform_int_distribution< Index >( 1 , n_nodes )( rg );
   EN[ i ] = std::uniform_int_distribution< Index >( 1 , n_nodes )( rg );
   } while( SN[ i ] == EN[ i ] );
  }

 std::vector< double > ND( n_nodes );
 for( Index i = 0 ; i < n_nodes ; ++i )
  ND[ i ] = rnd( 0 , 0.1 );

 for( Index i = 0 ; i < n_arcs ; ++i ) {
  U[ i ] = rho * rnd( 1 , 4 );
  C[ i ] = rnd( 1 , 10 );
  LD[ i ] = rnd( 0 , 0.1 );
  }

 // the delay of the "guaranteed" path when every arc of it reserves its
 // whole capacity: the packetization and burst terms MTU / r + MTU / U,
 // plus the fixed per-arc and per-node ones
 double delay = 0;
 double rmin = Inf< double >();
 for( Index i = 0 ; i < path.size() - 1 ; ++i ) {
  delay += 2 * ( MTU / U[ i ] ) + LD[ i ] + ND[ SN[ i ] - 1 ];
  rmin = std::min( rmin , U[ i ] );
  }
 delay += burst / rmin;

 // write the instance out in the DIMACS-like format the Block reads, which
 // is also the way the source and the sink of the flow are told apart (a
 // positive supply is the source, a negative one is the sink); the data
 // are written with all their digits, so that the deadline computed above
 // is the one of the instance the Block ends up with
 std::ostringstream dmx;
 dmx << std::setprecision( 16 );
 dmx << "p min " << n_nodes << " " << n_arcs << "\n";
 dmx << "n " << 1 << " 1\n";
 dmx << "n " << n_nodes << " -1\n";
 for( Index i = 0 ; i < n_arcs ; ++i )
  dmx << "a " << SN[ i ] << " " << EN[ i ] << " 0 " << U[ i ] << " "
      << C[ i ] << "\n";

 // and the DCR-specific data: the node delays, the arc delays, then the
 // burst, the deadline, the MTU and rho
 std::ostringstream dcr;
 dcr << std::setprecision( 16 );
 for( Index i = 0 ; i < n_nodes ; ++i )
  dcr << ND[ i ] << "\n";
 for( Index i = 0 ; i < n_arcs ; ++i )
  dcr << LD[ i ] << "\n";
 dcr << burst << "\n" << tightness * delay << "\n" << MTU << "\n"
     << rho << "\n";

 std::istringstream idmx( dmx.str() );
 block->load( idmx );

 std::istringstream idcr( dcr.str() );
 block->load_dcr( idcr , block->get_NNodes() , block->get_NArcs() );

 }  // end( build_random_instance )

/*--------------------------------------------------------------------------*/
/// the cost of the solution held by sol, +INF if it holds no rates

static double cost_of( SingleFlowDCRBlock * block , DCRSolution * sol )
{
 auto & R = sol->get_r();
 if( R.size() < block->get_NArcs() )
  return( Inf< double >() );

 double cost = 0;
 for( Index i = 0 ; i < block->get_NArcs() ; ++i )
  if( ! block->is_deleted( i ) )
   cost += block->get_C( i ) * R[ i ];

 return( cost );
 }

/*--------------------------------------------------------------------------*/
/// checks the Solution that each Solver produces
/** For every Solver that has one, asks for the Solution and checks that
 * SingleFlowDCRBlock::is_sol_feasible() accepts it, with the tolerance the
 * BlockConfig declares, and that the cost it gives is the value the Solver
 * declares. The Variable of the Block are scrambled first: a Solver that
 * fills its Solution by writing there and reading it back rather than out
 * of its own data structures produces garbage, and this is what catches
 * it. */

static bool check_solutions( SingleFlowDCRBlock * block , double tol )
{
 bool ok = true;
 std::size_t k = 0;

 for( auto slvr : block->get_registered_solvers() ) {
  const std::string me = "S" + std::to_string( k );
  const bool relax = is_relaxation( k++ );

  if( ! slvr->has_var_solution() )
   continue;

  // scramble the Variable, if they are there at all
  if( block->get_NArcs() )
   for( Index i = 0 ; i < block->get_NArcs() ; ++i ) {
    block->set_x( i , -1 );
    block->set_r( i , -1 );
    }

  std::unique_ptr< Solution > sol( slvr->get_Solution() );
  auto dsol = dynamic_cast< DCRSolution * >( sol.get() );
  if( ! dsol ) {
   std::cout << me << ": " << RED( not a DCRSolution ) << std::endl;
   ok = false;
   continue;
   }

  // a Solver that solves a relaxation of the problem is not asked for a
  // feasible solution of the problem: what it writes in its Solution is a
  // point of the relaxation [see the -R option]. The tolerance of the
  // check is the one the BlockConfig declares
  if( ( ! relax ) && ( ! block->is_sol_feasible( dsol ) ) ) {
   // say which of the four checks is the one that fails, which is the
   // first thing anybody would ask; the tolerance here is the one of the
   // test, the Block having already spoken with the one of its config
   auto & X = dsol->get_x();
   auto & R = dsol->get_r();
   std::cout << me << ": " << RED( infeasible Solution ) << " (";
   if( ! block->flow_feasible( tol , X ) )
    std::cout << " flow";
   if( ! block->bound_feasible( tol , X , R ) )
    std::cout << " bounds";
   if( ! block->link_feasible( tol , X , R ) ) {
    std::cout << " rates";
    // the first arc whose reserved rate does not go with its routing:
    // where exactly it goes wrong is the next thing anybody would ask
    for( Index i = 0 ; i < block->get_NArcs() ; ++i ) {
     if( block->is_deleted( i ) )
      continue;
     c_double Ui = block->get_U( i );
     if( ( R[ i ] > Ui * X[ i ] + tol ) ||
         ( R[ i ] < block->get_rho() * X[ i ] - tol ) ) {
      std::cout << " [arc " << i << ": x = " << X[ i ] << " , r = "
                << R[ i ] << " , U = " << Ui << " , rho = "
                << block->get_rho() << " ]";
      break;
      }
     }
    }
   if( ! block->delay_feasible( tol , X , R ) )
    std::cout << " delay";
   std::cout << " )" << std::endl;
   ok = false;
   }

  c_double cost = cost_of( block , dsol );
  c_double declared = slvr->get_var_value();
  if( std::abs( cost - declared ) >
      tol * std::max( double( 1 ) , std::abs( declared ) ) ) {
   std::cout << me << ": " << RED( Solution cost ) << " = "
             << fmt_obj( cost ) << " but the Solver declares "
             << fmt_obj( declared ) << std::endl;
   ok = false;
   }

  // put the solution back into the Variable, so that whoever looks at
  // them next finds what the Solver found
  dsol->write( block );
  }

 return( ok );
 }

/*--------------------------------------------------------------------------*/

static bool process_specific_arg( int opt )
{
 switch( opt ) {
  case( 'n' ): Str2Sthg( optarg , n_nodes );   return( true );
  case( 'm' ): Str2Sthg( optarg , n_arcs );    return( true );
  case( 'e' ): Str2Sthg( optarg , seed );      return( true );
  case( 't' ): Str2Sthg( optarg , tightness ); return( true );
  default:                                     return( false );
  }
 }

/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 std::set_terminate( smspp_terminate );

 docopt_desc = "SMS++ SingleFlowDCRBlock test.\n";
 short_opts += "n:m:e:t:";
 const std::vector< option > my_opts = {
   { "nodes"     , required_argument , nullptr , 'n' } ,
   { "arcs"      , required_argument , nullptr , 'm' } ,
   { "seed"      , required_argument , nullptr , 'e' } ,
   { "tightness" , required_argument , nullptr , 't' } };
 long_opts.insert( std::prev( long_opts.end() ) ,
                   my_opts.begin() , my_opts.end() );
 help += "  -n, --nodes <n>      nodes of the random instance [10]\n"
         "  -m, --arcs <m>       arcs of the random instance [30]\n"
         "  -e, --seed <n>       seed of the random instance [1]\n"
         "  -t, --tightness <t>  deadline over the delay of the\n"
         "                       guaranteed path, >= 1 [1.2]\n";

 // the instance can be generated, so no input file is needed
 filename_optional = true;

 process_args( argc , argv , process_specific_arg );
 require_solver_config();

 if( n_nodes < 2 ) {
  std::cerr << "Error: at least two nodes are needed" << std::endl;
  exit( 1 );
  }

 // the Block: either the given netCDF file or a random instance - - - - - -

 SingleFlowDCRBlock * block;
 if( ! filename.empty() ) {
  block = dynamic_cast< SingleFlowDCRBlock * >(
                                       Block::deserialize( filename ) );
  if( ! block ) {
   std::cerr << "Error: " << filename
             << " does not contain a SingleFlowDCRBlock" << std::endl;
   exit( 1 );
   }
  }
 else {
  rg.seed( seed );
  block = dynamic_cast< SingleFlowDCRBlock * >(
                       Block::new_Block( "SingleFlowDCRBlock" ) );
  build_random_instance( block );
  }

 // configure it- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 if( ! bconf_file.empty() )
  b_config_Block( block , Configuration::deserialize( bconf_file ) ,
                  bconf_file );

 auto sconf = Configuration::deserialize( sconf_file );
 s_config_Block( block , sconf , sconf_file );

 // solve it and cross-check what the Solver say - - - - - - - - - - - - - -

 c_double tol = 1e-5;
 bool ok = SolveAll( block , RefObjective , tol );

 // then check the Solution they produce - - - - - - - - - - - - - - - - - -

 ok &= check_solutions( block , 1e-4 );

 // clean up- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 s_config_Block( block , sconf );  // sconf has been clear()-ed before
 delete( sconf );
 delete( block );

 return( ok ? 0 : 1 );

 }  // end( main )

/*--------------------------------------------------------------------------*/
/*------------------------ End File test.cpp -------------------------------*/
/*--------------------------------------------------------------------------*/
