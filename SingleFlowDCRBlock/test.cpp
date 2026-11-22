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
/// the value of the native parameter of @p slvr with that name, 0 if none
/** The name is the one of CPLEX for a CPXMILPSolver and of Gurobi for a
 * GRBMILPSolver; a Solver of another class has none of these. */

static double native_par( Solver * slvr , const std::string & cpx ,
                          const std::string & grb )
{
 const std::string cls = slvr->classname();
 const std::string & name = ( cls.find( "CPX" ) != std::string::npos ) ?
                            cpx : ( cls.find( "GRB" ) != std::string::npos ) ?
                            grb : std::string();
 if( name.empty() )
  return( 0 );
 const auto idx = slvr->dbl_par_str2idx( name );
 if( idx == Inf< Solver::idx_type >() )
  return( 0 );
 return( slvr->get_dbl_par( idx ) );
 }

/*--------------------------------------------------------------------------*/
/// the tolerance up to which the solution of @p slvr has to meet the deadline
/** Relative to max( 1 , deadline ), as SingleFlowDCRBlock::delay_feasible()
 * reads it. A Solver that declares no feasibility tolerance (dblFAccSol 0,
 * e.g., one that builds its rates to meet the deadline) is held to 1e-9.
 * Otherwise the tolerances that govern its solution are read from it and
 * carried over the rows of the formulation where the delay is: the
 * deadline row (the sum of the fixed delays x[ i ] l[ i ], of the theta[ i ]
 * and of theta_min, at most D), the cone rows theta[ i ] r[ i ] >=
 * MTU x[ i ]^2 and theta_min r_min >= burst.
 *
 * - The feasibility tolerance tau_f (dblFAccSol) is an absolute violation of
 *   each row: tau_f on the deadline row, and, on a cone row, a delay
 *   MTU x[ i ]^2 / r[ i ] above theta[ i ] by tau_f / r[ i ] (burst / r_min
 *   above theta_min by tau_f / r_min).
 *
 * - The convergence tolerance tau_q of the barrier on the conic rows is a
 *   relative one: tau_q max( 1 , D ) on the deadline row, and on a cone row
 *   a violation tau_q max( 1 , MTU ) (tau_q max( 1 , burst ) for theta_min),
 *   hence a delay above theta[ i ] by that over r[ i ] (r_min).
 *
 * - The integrality tolerance tau_i lets x[ i ] be 1 - tau_i on an arc of
 *   the path, where the delay of the path is that at x[ i ] = 1: it grows
 *   by at most tau_i ( MTU / U[ i ] + l[ i ] + 2 MTU / r[ i ] ) on the arc.
 *
 * The bound is the sum of these, on the arcs the solution uses. */

static double deadline_tolerance( SingleFlowDCRBlock * block , Solver * slvr ,
                                  SingleFlowDCRBlock::c_Vec_double & X ,
                                  SingleFlowDCRBlock::c_Vec_double & R )
{
 c_double tau_f = slvr->get_dbl_par( Solver::dblFAccSol );
 if( ! ( tau_f > 0 ) )
  return( 1e-9 );
 c_double tau_q = native_par( slvr , "CPXPARAM_Barrier_QCPConvergeTol" ,
                              "BarQCPConvTol" );
 c_double tau_i = native_par( slvr , "CPXPARAM_MIP_Tolerances_Integrality" ,
                              "IntFeasTol" );

 c_double D = std::max( 1.0 , block->get_FlowDeadline() );
 c_double MTU = block->get_MTU();
 auto & LD = block->get_LinkDelays();
 auto & ND = block->get_NodeDelays();

 double inv = 0 , fix = 0 , rmin = Inf< double >();
 for( Index i = 0 ; i < block->get_NArcs() ; ++i )
  if( ( ! block->is_deleted( i ) ) && ( X[ i ] > 0.5 ) && ( R[ i ] > 0 ) ) {
   inv += 1 / R[ i ];
   rmin = std::min( rmin , R[ i ] );
   c_double Ui = block->get_U( i );
   fix += ( Ui < Inf< double >() ? MTU / Ui : 0 ) +
          ( LD.empty() ? 0 : LD[ i ] ) +
          ( ND.empty() ? 0 : ND[ block->get_SN( i ) - 1 ] ) + 2 * MTU / R[ i ];
   }
 c_double burst = block->get_FlowBurst();
 c_double imin = ( ( burst > 0 ) && ( rmin < Inf< double >() ) ) ?
                 1 / rmin : 0;

 c_double excess = tau_f * ( 1 + inv + imin ) +
                   tau_q * ( D + std::max( 1.0 , MTU ) * inv +
                             std::max( 1.0 , burst ) * imin ) +
                   tau_i * fix;

 return( std::max( 1e-9 , excess / D ) );
 }

/*--------------------------------------------------------------------------*/
/// the tolerance of the lower bound of @p slvr, absolute
/** A Solver that declares no feasibility tolerance (dblFAccSol 0) gives a
 * tight bound, up to 1e-9 relative; one that does is accurate up to the
 * relative and absolute gaps it declares (dblRelAcc, dblAbsAcc). */

static double lb_tolerance( Solver * slvr , double lb )
{
 c_double scale = std::max( 1.0 , std::abs( lb ) );
 if( ! ( slvr->get_dbl_par( Solver::dblFAccSol ) > 0 ) )
  return( 1e-9 * scale );
 return( std::max( { 1e-9 * scale ,
                     slvr->get_dbl_par( Solver::dblRelAcc ) * scale ,
                     slvr->get_dbl_par( Solver::dblAbsAcc ) } ) );
 }

/*--------------------------------------------------------------------------*/
/// how much below the optimum the cost of the solution of @p slvr can be
/** A Solver that declares no feasibility tolerance hands out a solution of
 * the problem, whose cost is not below the optimum: 1e-9 relative. One that
 * declares a tolerance may leave its rows violated within all its
 * tolerances (feasibility, convergence of the barrier on the conic rows,
 * integrality), and its solution ( X , R ) can be cheaper than the optimum.
 * What these let it gain is read on the solution, rather than bounded row
 * by row: on its path P (x[ i ] = 1 on the arcs with X[ i ] > 1 / 2, as
 * x[ i ] = 1 - tau_i or a violated cone row shrink the delay that the rows
 * see but not the one of the path), R' is R clipped to the bounds
 * [ rho , U_i ] of the rates, of cost c( R ) + Delta, and e is the excess of
 * its delay over the deadline D. R' is a solution of the problem with
 * deadline D + e, so c( R' ) >= v_P( D + e ), the cheapest rates on P
 * within that deadline, and v_P( D + e ) >= v_P( D ) - Lambda e >=
 * opt - Lambda e, where Lambda bounds the multiplier of the deadline row of
 * min sum_P c_i r_i: in its KKT conditions an arc not at its capacity has
 * c_i >= lambda MTU / r_i^2 (more with the burst term on the arc of the
 * minimum rate), hence lambda <= c_i U_i^2 / MTU, and Lambda = max_P
 * c_i U_i^2 / MTU (+INF, i.e., no bound, if an arc of P has no capacity).
 * Hence c( R ) >= opt - Lambda e - Delta, and the margin is
 * Lambda e + Delta (Delta < 0 when R reserves more than U_i). */

static double cost_margin( SingleFlowDCRBlock * block , Solver * slvr ,
                           SingleFlowDCRBlock::c_Vec_double & X ,
                           SingleFlowDCRBlock::c_Vec_double & R ,
                           double cost )
{
 c_double tau_f = slvr->get_dbl_par( Solver::dblFAccSol );
 if( ! ( tau_f > 0 ) )
  return( 1e-9 * std::max( 1.0 , std::abs( cost ) ) );

 // the path, with x[ i ] = 1 on its arcs
 SingleFlowDCRBlock::Vec_double P( X.size() );
 for( Index i = 0 ; i < P.size() ; ++i )
  P[ i ] = X[ i ] > 0.5 ? 1 : 0;

 // R clipped to [ rho , U_i ] on P, and the change Delta of its cost
 SingleFlowDCRBlock::Vec_double Rc( R );
 double Delta = 0;
 for( Index i = 0 ; i < P.size() ; ++i )
  if( ( ! block->is_deleted( i ) ) && ( P[ i ] > 0 ) ) {
   Rc[ i ] = std::min( block->get_U( i ) ,
                       std::max( block->get_rho() , R[ i ] ) );
   Delta += block->get_C( i ) * ( Rc[ i ] - R[ i ] );
   }

 // e, as the smallest relative tolerance that accepts the delay of P
 double e = 0;
 if( ! block->delay_feasible( 0 , P , Rc ) ) {
  double lo = 1e-16 , hi = 1;
  if( ! block->delay_feasible( hi , P , Rc ) )  // the delay is not even
   return( Inf< double >() );                  // close: no bound
  for( int it = 0 ; it < 100 ; ++it ) {
   c_double mid = std::sqrt( lo * hi );
   ( block->delay_feasible( mid , P , Rc ) ? hi : lo ) = mid;
   }
  e = hi * std::max( 1.0 , block->get_FlowDeadline() );
  }

 c_double MTU = block->get_MTU();
 double Lambda = 0;
 for( Index i = 0 ; i < block->get_NArcs() ; ++i )
  if( ( ! block->is_deleted( i ) ) && ( X[ i ] > 0.5 ) ) {
   c_double Ui = block->get_U( i );
   c_double ci = block->get_C( i );
   if( e > 0 )
    Lambda = ( Ui < Inf< double >() ) ?
             std::max( Lambda , ci * Ui * Ui / MTU ) : Inf< double >();
   }

 return( ( e > 0 ? Lambda * e : 0 ) + Delta );
 }

/*--------------------------------------------------------------------------*/
/// checks the Solution that each Solver produces
/** For every Solver that has one, asks for the Solution and checks that
 * SingleFlowDCRBlock::is_sol_feasible() accepts it, with the tolerance the
 * BlockConfig declares, that the cost it gives is the value the Solver
 * declares and, for a Solver that does not solve a relaxation, that it
 * meets the deadline up to its tolerance [see deadline_tolerance()] and
 * costs at least each lower bound less its tolerance [see lb_tolerance()]
 * and the margin of the solution [see cost_margin()]. The Variable of the
 * Block are scrambled first: a Solver that fills its Solution by writing
 * there and reading it back rather than out of its own data structures
 * produces garbage, and this is what catches it. @p relax
 * says which of the Solver solve a relaxation, as the formulation of
 * @p block declares it [see attach_formulation_Solvers()], and @p form is
 * the name of the formulation, to be written before that of the Solver when
 * there is more than one. */

static bool check_solutions( SingleFlowDCRBlock * block , double tol ,
                             const std::vector< bool > & relax_of ,
                             const std::string & form )
{
 bool ok = true;
 std::size_t k = 0;

 // the lower bounds, each less its tolerance, which the cost of a solution
 // of the problem cannot be below: the largest of them, and its Solver
 double best_lb = - Inf< double >();
 std::size_t best_k = 0 , h = 0;
 for( auto slvr : block->get_registered_solvers() ) {
  c_double lb = slvr->get_lb();
  if( ( lb > - Inf< double >() ) && ( lb < Inf< double >() ) &&
      ( lb - lb_tolerance( slvr , lb ) > best_lb ) ) {
   best_lb = lb - lb_tolerance( slvr , lb );
   best_k = h;
   }
  ++h;
  }

 for( auto slvr : block->get_registered_solvers() ) {
  const std::string me = ( form.empty() ? "" : form + " " ) + "S" +
                         std::to_string( k );
  const bool relax = ( k < relax_of.size() ) && relax_of[ k ];
  ++k;

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

  // a solution of the problem meets the deadline, up to a tolerance much
  // tighter than that of the Block, and then costs at least the optimum
  if( ! relax ) {
   auto & X = dsol->get_x();
   auto & R = dsol->get_r();
   c_double dtol = deadline_tolerance( block , slvr , X , R );
   if( ! block->delay_feasible( dtol , X , R ) ) {
    // by how much, as the smallest relative tolerance that would accept it
    double lo = dtol , hi = 1;
    for( int it = 0 ; it < 60 ; ++it ) {
     c_double mid = std::sqrt( lo * hi );
     ( block->delay_feasible( mid , X , R ) ? hi : lo ) = mid;
     }
    std::cout << me << ": " << RED( delay above the deadline ) << " by "
              << hi << " (relative), with a tolerance of " << dtol
              << std::endl;
    ok = false;
    }
   // the Solver of the solution may also leave its rows violated, and
   // then its cost below the optimum [see cost_margin()]
   c_double margin = cost_margin( block , slvr , X , R , cost );
   if( cost < best_lb - margin ) {
    std::cout << me << ": " << RED( Solution cost ) << " = "
              << fmt_obj( cost ) << " below the lower bound of S" << best_k
              << " less its tolerance, " << fmt_obj( best_lb ) << ", by "
              << fmt_obj( best_lb - cost ) << ", more than the margin of"
              << " the solution, " << fmt_obj( margin ) << std::endl;
    ok = false;
    }
   }

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

 if( n_nodes < 2 ) {
  std::cerr << "Error: at least two nodes are needed" << std::endl;
  exit( 1 );
  }

 // the formulations of the BlockConfig: the list of -F, the one of -B, or none
 // [see get_formulations()]
 auto forms = get_formulations( true );

 // the Block, once per formulation: either the given netCDF file or a random
 // instance, the same for all of them - - - - - - - - - - - - - - - - - - -

 std::vector< Block * > blocks;
 std::vector< Configuration * > sconfs;
 std::vector< std::vector< bool > > relax;
 for( std::size_t k = 0 ; k < forms.configs.size() ; ++k ) {
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

  // configure it - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  if( forms.configs[ k ] )
   b_config_Block( block , forms.configs[ k ] , forms.file );

  auto sconf = solver_config_of( forms , k );
  relax.push_back( attach_formulation_Solvers( block , sconf , forms , k ) );

  blocks.push_back( block );
  sconfs.push_back( sconf );
  }

 delete( forms.owner );

 // solve them and cross-check what the Solver say- - - - - - - - - - - - -

 c_double tol = 1e-5;
 bool ok = SolveFormulations( blocks , forms.names , read_bounds ,
                              RefObjective , tol , {} , relax );

 // then check the Solution they produce - - - - - - - - - - - - - - - - - -

 for( std::size_t k = 0 ; k < blocks.size() ; ++k )
  ok &= check_solutions( static_cast< SingleFlowDCRBlock * >( blocks[ k ] ) ,
                         1e-4 , relax[ k ] ,
                         blocks.size() > 1 ? forms.names[ k ] : "" );

 // clean up- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 for( std::size_t k = 0 ; k < blocks.size() ; ++k ) {
  s_config_Block( blocks[ k ] , sconfs[ k ] );  // clear()-ed before
  delete( sconfs[ k ] );
  delete( blocks[ k ] );
  }

 return( ok ? 0 : 1 );

 }  // end( main )

/*--------------------------------------------------------------------------*/
/*------------------------ End File test.cpp -------------------------------*/
/*--------------------------------------------------------------------------*/
