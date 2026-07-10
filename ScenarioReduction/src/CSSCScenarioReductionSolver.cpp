/*--------------------------------------------------------------------------*/
/*----------- File CSSCScenarioReductionSolver.cpp -----------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Implementation of CSSCScenarioReductionSolver: the Cost-Space Scenario
 * Clustering (CSSC), applied to reduce a pool of N scenarios of a
 * two-stage stochastic program down to K representative ones.
 *
 * \author Minh Duc Pham \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 */
/*--------------------------------------------------------------------------*/

#include "CSSCScenarioReductionSolver.h"

#include "AbstractBlock.h"
#include "ColVariable.h"
#include "FRealObjective.h"
#include "FRowConstraint.h"
#include "LinearFunction.h"
#include "Modification.h"
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <numeric>
#include <unordered_set>

using namespace SMSpp_di_unipi_it;

SMSpp_insert_in_factory_cpp_1( CSSCScenarioReductionSolver );

/*--------------------------------------------------------------------------*/

CSSCScenarioReductionSolver::~CSSCScenarioReductionSolver()
{
 delete f_milp_config;
}

/*--------------------------------------------------------------------------*/
/*---------------------------- set_Block ----------------------------------*/
/*--------------------------------------------------------------------------*/

void CSSCScenarioReductionSolver::set_Block( Block * block )
{
 std::lock_guard< std::recursive_mutex > lock( f_mutex );

 if( block == f_Block ) return;

 Solver::set_Block( block );
 if( ! f_Block ) return;

 auto * srb = dynamic_cast< ScenarioReductionBlock * >( f_Block );
 if( ! srb )
  throw std::invalid_argument(
   "CSSCScenarioReductionSolver::set_Block: "
   "block must be a ScenarioReductionBlock" );

 f_tssb = dynamic_cast< TwoStageStochasticBlock * >(
  srb->get_stochastic_block() );
 if( ! f_tssb )
  throw std::invalid_argument(
   "CSSCScenarioReductionSolver::set_Block: "
   "ScenarioReductionBlock must contain a TwoStageStochasticBlock" );

 f_stoch_applicator = dynamic_cast< StochasticBlock * >(
  srb->get_scenario_applicator() );
 if( ! f_stoch_applicator )
  throw std::invalid_argument(
   "CSSCScenarioReductionSolver::set_Block: "
   "ScenarioReductionBlock must contain a StochasticBlock scenario applicator" );

 auto * dss = dynamic_cast< DiscreteScenarioSet * >(
  srb->get_scenario_generator() );
 if( ! dss )
  throw std::invalid_argument(
   "CSSCScenarioReductionSolver::set_Block: "
   "ScenarioGenerator must be a DiscreteScenarioSet" );

 const int N = static_cast< int >( dss->get_poolSize() );

 // Build pool_map: relative index [0..N-1] -> absolute DSS scenario index
 const auto pool = dss->get_selected_scenarios();
 f_pool_map.assign( pool.begin() , pool.end() );

 f_N = N;

 // Uniform weights 1/N.
 f_weights.assign( N , 1.0 / static_cast< double >( N ) );

 // Pairwise Euclidean distances between scenario feature vectors
 const auto sz = dss->get_scenarioSize();
 f_dist.assign( N , std::vector< double >( N , 0.0 ) );
 for( int i = 0 ; i < N ; ++i )
  for( int j = 0 ; j < N ; ++j ) {
   double d2 = 0.0;
   for( std::size_t d = 0 ; d < sz ; ++d ) {
    double dd = dss->get_scenario_value( f_pool_map[ i ] , d )
              - dss->get_scenario_value( f_pool_map[ j ] , d );
    d2 += dd * dd;
    }
   f_dist[ i ][ j ] = std::sqrt( d2 );
   }

 reduced_atoms.assign( N , false );
 ind_red.clear();
 if( f_K > 0 ) ind_red.reserve( static_cast< std::size_t >( f_K ) );
 f_solution_value = 0.0;
}

/*--------------------------------------------------------------------------*/
/*---------------------------- compute ------------------------------------*/
/*--------------------------------------------------------------------------*/

int CSSCScenarioReductionSolver::compute( bool /*changedvars*/ )
{
 std::lock_guard< std::recursive_mutex > lock( f_mutex );

 if( ! get_Block() ) return kError;

 if( f_K <= 0 || f_K > f_N )
  throw std::logic_error(
   "CSSCScenarioReductionSolver::compute: "
   "call set_nb_reduced(K) with 0 < K <= N before compute()" );

 if( ! f_milp_config )
  throw std::logic_error(
   "CSSCScenarioReductionSolver::compute: "
   "call set_milp_config() before compute()" );

 if( ! f_var_extractor )
  throw std::logic_error(
   "CSSCScenarioReductionSolver::compute: "
   "call set_var_extractor() before compute()" );

 mod_clear();

 std::cout << "CSSC: reducing " << f_N << " -> " << f_K
           << " scenarios" << std::endl;

 f_V_matrix = compute_generic_V_matrix();
 solve_cssc_milp( f_V_matrix );

 return kOK;
}

/*--------------------------------------------------------------------------*/
/*------------------- compute_generic_V_matrix ----------------------------*/
/*--------------------------------------------------------------------------*/

// Post-processing diagnostics for the V matrix: infeasible-pair count, a 4x4
// corner sample for a quick visual sanity check, and the max deviation from
// V[0][0] (near-zero deviation means every scenario has almost the same
// optimal cost, in which case CSSC has no real signal to cluster on and its
// choice of representatives is effectively arbitrary)
static void log_V_diagnostics( const std::vector< std::vector< double > > & V ,
                               int n )
{
 int inf_count = 0;
 for( int i = 0 ; i < n ; ++i )
  for( int j = 0 ; j < n ; ++j )
   if( V[ i ][ j ] < 0.0 ) ++inf_count;
 if( inf_count > 0 )
  std::cout << "  NOTE: " << inf_count
            << " off-diagonal LP(s) infeasible -> x_ij forced to 0 in MILP\n";

 double max_dev = 0.0;
 const double v00 = V[0][0];
 for( int i = 0 ; i < n ; ++i )
  for( int j = 0 ; j < n ; ++j )
   if( V[i][j] >= 0.0 )
    max_dev = std::max( max_dev , std::abs( V[i][j] - v00 ) );
 const int diag = std::min( n , 4 );
 std::cout << "  V matrix sample (first " << diag << "×" << diag << " corner):\n";
 for( int i = 0 ; i < diag ; ++i ) {
  for( int j = 0 ; j < diag ; ++j )
   if( V[i][j] < 0.0 )
    std::cout << std::setw(14) << "INFEAS";
   else
    std::cout << std::setw(14) << std::fixed << std::setprecision(2) << V[i][j];
  std::cout << "\n";
  }
 std::cout << "  max_deviation=" << max_dev
           << ( max_dev < 1.0 ? "  [FLAT: CSSC selects arbitrarily]"
                              : "  [has variation: CSSC meaningful]" )
           << "\n\n";

 int neg_count = 0;
 for( int j = 0 ; j < n ; ++j )
  for( int i = 0 ; i < n ; ++i )
   if( i != j && V[j][i] >= 0.0 && V[j][i] - V[j][j] < -1e-6 ) ++neg_count;
}

// Build the N×N cost-space matrix V, where V[i][j] is the cost of applying the
// optimal first-stage solution of scenario i (y*_i) to scenario j's data.
// A single inner Block is generated once and reused for the whole loop, each
// scenario is loaded via incremental injection (StochasticBlock::set_data(),
// optionally followed by the registered DataInjector)
std::vector< std::vector< double > >
CSSCScenarioReductionSolver::compute_generic_V_matrix()
{
 const int n = f_N;

 auto * srb = dynamic_cast< ScenarioReductionBlock * >( f_Block );
 auto * dss = dynamic_cast< DiscreteScenarioSet * >(
  srb->get_scenario_generator() );
 const auto scenario_size = dss->get_scenarioSize();

 std::vector< std::vector< double > > V( n , std::vector< double >( n , 0.0 ) );

 Block * inner = f_stoch_applicator->get_inner_block();
 inner->generate_abstract_variables();
 inner->generate_abstract_constraints();
 inner->generate_objective();

 // Identify here-and-now variables via the caller-provided extractor.
 // Called after generate_abstract_variables() so pointers are stable
 std::vector< ColVariable * > fs_vars = f_var_extractor( inner );
 if( fs_vars.empty() )
  throw std::runtime_error(
   "CSSCScenarioReductionSolver::compute_generic_V_matrix: "
   "VarExtractor returned no variables" );

 std::cout << "  Step 1: building " << n << "×" << n
           << " V matrix  (" << fs_vars.size()
           << " first-stage variables)..." << std::endl;

 // Attach solver once, stays for the entire V-matrix loop so that every
 // set_data() call propagates modifications to the solver correctly
 auto * sub_cfg = static_cast< BlockSolverConfig * >( f_milp_config->clone() );
 sub_cfg->apply( inner );
 Solver * sub_solver = inner->get_registered_solvers().empty()
  ? nullptr : inner->get_registered_solvers().front();
 if( ! sub_solver ) {
  sub_cfg->clear(); delete sub_cfg;
  throw std::runtime_error(
   "CSSCScenarioReductionSolver::compute_generic_V_matrix: "
   "failed to attach solver to inner block" );
  }

 // Save original types, build continuous-relaxed types for the off-diagonal LP.
 // For kBinary(11) -> kPosUnitary(10): clear the integer bit.
 // This lets the solver treat fixed 0/1 vars as continuous bounds, avoiding
 // unnecessary MIP branching in the off-diagonal solves
 std::vector< ColVariable::var_type > orig_types( fs_vars.size() );
 std::vector< ColVariable::var_type > relax_types( fs_vars.size() );
 for( std::size_t k = 0 ; k < fs_vars.size() ; ++k ) {
  orig_types[ k ]  = fs_vars[ k ]->get_type();
  relax_types[ k ] = static_cast< ColVariable::var_type >(
   static_cast< int >( orig_types[ k ] ) & ~1 );
  }

 std::vector< double > y_star( fs_vars.size() );

 auto inject_scenario = [ & ]( const std::vector< double > & data ) {
  if( f_fix_with_mod ) {
   f_stoch_applicator->set_data( data , eNoBlck , eNoBlck );
   if( f_data_injector ) f_data_injector( inner , data );
  } else {
   f_stoch_applicator->set_data( data , eModBlck , eModBlck );
   inner->add_Modification( std::make_shared< NBModification >( inner ) );
  }
  };

 // Main an opportinity cost matrix (V-matrix) loop. For each row i:
 //   (a) solve scenario i on its own (diagonal V[i][i]), recording its
 //       optimal here-and-now solution y*_i;
 //   (b) fix the here-and-now variables at y*_i;
 //   (c) for every other scenario j, inject j's data and re-solve with y*_i
 //       fixed (off-diagonal V[i][j]), this measures how much scenario i's
 //       decision would cost if scenario j were the one that occurred;
 //   (d) unfix, ready for the next row
 for( int i = 0 ; i < n ; ++i ) {

  // (a) Inject scenario i and solve as MIP (original binary types).
  {
   std::vector< double > data( scenario_size );
   for( std::size_t d = 0 ; d < scenario_size ; ++d )
    data[ d ] = dss->get_scenario_value( f_pool_map[ i ] , d );
   inject_scenario( data );
  }

  {
   const int st = sub_solver->compute();
   if( st != Solver::kOK && st != Solver::kLowPrecision )
    throw std::runtime_error(
     "CSSCScenarioReductionSolver::compute_generic_V_matrix: "
     "MIP diagonal solve failed (status=" + std::to_string( st ) +
     ") for scenario i=" + std::to_string( i ) );
  }
  sub_solver->get_var_solution();
  V[ i ][ i ] = sub_solver->get_var_value();

  // Save MIP-optimal first-stage values.
  for( std::size_t k = 0 ; k < fs_vars.size() ; ++k )
   y_star[ k ] = fs_vars[ k ]->get_value();

  // (b) Relax first-stage vars to continuous and fix them at y*_i.
  //     Relaxing binary->continuous lets us fix at a slightly-off-integer y*_i
  //     (MIP solver tolerance) without spurious infeasibility, and avoids
  //     branching on the fixed vars in the off-diagonal LP
  for( std::size_t k = 0 ; k < fs_vars.size() ; ++k )
   fs_vars[ k ]->set_type( relax_types[ k ] , eNoMod );
  for( std::size_t k = 0 ; k < fs_vars.size() ; ++k )
   fs_vars[ k ]->set_value( y_star[ k ] );
  for( auto * v : fs_vars )
   v->is_fixed( true , eModBlck );

  // (c) Off-diagonal: inject scenario j, solve with y* fixed.
  for( int j = 0 ; j < n ; ++j ) {
   if( j == i ) continue;
   std::vector< double > data_j( scenario_size );
   for( std::size_t d = 0 ; d < scenario_size ; ++d )
    data_j[ d ] = dss->get_scenario_value( f_pool_map[ j ] , d );
   inject_scenario( data_j );
   const int st = sub_solver->compute();
   if( st == Solver::kOK || st == Solver::kLowPrecision ) {
    sub_solver->get_var_solution();
    V[ i ][ j ] = sub_solver->get_var_value();
    } else {
    static int first_inf_msg = 0;
    if( ++first_inf_msg <= 3 )
     std::cout << "    [DIAG] V[" << i << "][" << j
               << "] infeasible (status=" << st << ")\n";
    V[ i ][ j ] = -1.0;  // sentinel: x_ij forced to 0 in MILP
    }
   }

  // (d) Unfix and restore the original (binary) types for the next row's
  //     diagonal MIP. Type restore is silent (eNoMod), mirroring the silent
  //     relax in (b), the next row's inject_scenario() call is what makes the
  //     solver observe the restored types
  for( auto * v : fs_vars )
   v->is_fixed( false , eModBlck );
  for( std::size_t k = 0 ; k < fs_vars.size() ; ++k )
   fs_vars[ k ]->set_type( orig_types[ k ] , eNoMod );

  std::cout << "    row " << (i+1) << "/" << n
            << "  V[i][i]=" << std::fixed << std::setprecision(2)
            << V[i][i] << std::endl;
  }

 sub_cfg->clear(); delete sub_cfg;

 log_V_diagnostics( V , n );
 return V;
}

/*--------------------------------------------------------------------------*/
/*------------------------- solve_cssc_milp --------------------------------*/
/*--------------------------------------------------------------------------*/
// CSSC MILP faithful to Keutchayan et al. (eq. 24-29).
// The absolute value is taken OUTSIDE the per-cluster sum (epigraph variables
// t_j), so deviations of opposite sign cancel within a cluster, the essential
// "mid-ground" property that lets CSSC group scenarios far apart in outcome
// space but close in cost space:
//
//   min  (1/N) * sum_j t_j                                    (eq. 24)
//   s.t. t_j >=  sum_{i: V[j][i]>=0} (V[j,i]-V[j,j]) x_ij      (eq. 25)
//        t_j >= -sum_{i: V[j][i]>=0} (V[j,i]-V[j,j]) x_ij      (eq. 26)
//        x_ij <= u_j,  x_jj = u_j    ∀i,j                      (eq. 27)
//        sum_j x_ij = 1,  sum_j u_j = K                        (eq. 28)
//        x_ij, u_j ∈ {0,1},  t_j >= 0 continuous               (eq. 29)
//        x_ij = 0  for infeasible pairs (V[j][i] < 0)

void CSSCScenarioReductionSolver::solve_cssc_milp(
 const std::vector< std::vector< double > > & V )
{
 const int n = f_N;
 const int K = f_K;

 std::cout << "  Step 2: MILP partitioning (N=" << n << ", K=" << K
           << ")..." << std::endl;

 auto milp_block = std::make_unique< AbstractBlock >();

 auto * x_vars_p = new std::vector< ColVariable >( n * n );
 auto * u_vars_p = new std::vector< ColVariable >( n );
 auto * t_vars_p = new std::vector< ColVariable >( n );   // per-cluster |·| epigraph
 auto & x_vars   = *x_vars_p;
 auto & u_vars   = *u_vars_p;
 auto & t_vars   = *t_vars_p;

 for( int i = 0 ; i < n * n ; ++i ) x_vars[i].set_type( ColVariable::kBinary );
 for( int j = 0 ; j < n    ; ++j ) u_vars[j].set_type( ColVariable::kBinary );
 // t_j are continuous and ≥ 0 (enforced implicitly by t_j ≥ ±sum below).
 for( int j = 0 ; j < n    ; ++j ) t_vars[j].set_type( ColVariable::kContinuous );

 milp_block->add_static_variable( x_vars );
 milp_block->add_static_variable( u_vars );
 milp_block->add_static_variable( t_vars );

 // Fix x_ij = 0 for infeasible pairs (V[j][i] < 0 sentinel).
 {
  int inf_pairs = 0;
  for( int j = 0 ; j < n ; ++j )
   for( int i = 0 ; i < n ; ++i )
    if( i != j && V[j][i] < 0.0 ) {
     LinearFunction::v_coeff_pair terms;
     terms.emplace_back( &x_vars[i*n+j] , 1.0 );
     auto lf  = std::make_unique< LinearFunction >( std::move(terms) );
     auto con = std::make_unique< FRowConstraint >();
     con->set_lhs( -Inf< double >() ); con->set_rhs( 0.0 );
     con->set_function( lf.release() , eNoMod );
     milp_block->add_static_constraint( *con.release() );
     ++inf_pairs;
     }
  if( inf_pairs > 0 )
   std::cout << "  Fixing " << inf_pairs << " infeasible x_ij = 0\n";
 }

 // x_ij <= u_j  (eq. 27a)
 for( int i = 0 ; i < n ; ++i )
  for( int j = 0 ; j < n ; ++j ) {
   LinearFunction::v_coeff_pair terms;
   terms.emplace_back( &x_vars[i*n+j] ,  1.0 );
   terms.emplace_back( &u_vars[j]     , -1.0 );
   auto lf  = std::make_unique< LinearFunction >( std::move(terms) );
   auto con = std::make_unique< FRowConstraint >();
   con->set_lhs( -Inf< double >() ); con->set_rhs( 0.0 );
   con->set_function( lf.release() , eNoMod );
   milp_block->add_static_constraint( *con.release() );
   }

 // x_jj = u_j  (eq. 27b)
 for( int j = 0 ; j < n ; ++j ) {
  LinearFunction::v_coeff_pair terms;
  terms.emplace_back( &x_vars[j*n+j] ,  1.0 );
  terms.emplace_back( &u_vars[j]     , -1.0 );
  auto lf  = std::make_unique< LinearFunction >( std::move(terms) );
  auto con = std::make_unique< FRowConstraint >();
  con->set_lhs( 0.0 ); con->set_rhs( 0.0 );
  con->set_function( lf.release() , eNoMod );
  milp_block->add_static_constraint( *con.release() );
  }

 // sum_j x_ij = 1  (eq. 28a)
 for( int i = 0 ; i < n ; ++i ) {
  LinearFunction::v_coeff_pair terms;
  for( int j = 0 ; j < n ; ++j )
   terms.emplace_back( &x_vars[i*n+j] , 1.0 );
  auto lf  = std::make_unique< LinearFunction >( std::move(terms) );
  auto con = std::make_unique< FRowConstraint >();
  con->set_lhs( 1.0 ); con->set_rhs( 1.0 );
  con->set_function( lf.release() , eNoMod );
  milp_block->add_static_constraint( *con.release() );
  }

 // sum_j u_j = K  (eq. 28b)
 {
  LinearFunction::v_coeff_pair terms;
  for( int j = 0 ; j < n ; ++j )
   terms.emplace_back( &u_vars[j] , 1.0 );
  auto lf  = std::make_unique< LinearFunction >( std::move(terms) );
  auto con = std::make_unique< FRowConstraint >();
  con->set_lhs( static_cast< double >(K) );
  con->set_rhs( static_cast< double >(K) );
  con->set_function( lf.release() , eNoMod );
  milp_block->add_static_constraint( *con.release() );
  }

 // Absolute-value linearization (Keutchayan et al. eq. 25-26).  For each
 // candidate representative j let
 //     S_j = sum_{i: feasible} (V[j][i] - V[j][j]) * x_ij
 // be the *signed* clustering discrepancy of cluster j (the goodness of fit:
 // how well representative j's cost reproduces the aggregate cost of its
 // members, note the absolute value is taken outside the sum, so members
 // whose deviations have opposite signs cancel, which is the essential
 // "mid-ground" property of CSSC).  We model t_j >= |S_j| with two rows:
 //     t_j - S_j >= 0      (t_j >=  S_j)
 //     t_j + S_j >= 0      (t_j >= -S_j)
 // Infeasible pairs (V[j][i] < 0) are excluded from S_j (their x_ij is 0)
 for( int j = 0 ; j < n ; ++j ) {
  // t_j - S_j >= 0
  LinearFunction::v_coeff_pair pos;
  pos.emplace_back( &t_vars[j] , 1.0 );
  for( int i = 0 ; i < n ; ++i ) {
   if( V[j][i] < 0.0 ) continue;
   const double c = V[j][i] - V[j][j];
   if( std::abs( c ) > 1e-15 )
    pos.emplace_back( &x_vars[i*n+j] , -c );
   }
  {
   auto lf  = std::make_unique< LinearFunction >( std::move(pos) );
   auto con = std::make_unique< FRowConstraint >();
   con->set_lhs( 0.0 ); con->set_rhs( Inf< double >() );
   con->set_function( lf.release() , eNoMod );
   milp_block->add_static_constraint( *con.release() );
   }
  // t_j + S_j >= 0
  LinearFunction::v_coeff_pair neg;
  neg.emplace_back( &t_vars[j] , 1.0 );
  for( int i = 0 ; i < n ; ++i ) {
   if( V[j][i] < 0.0 ) continue;
   const double c = V[j][i] - V[j][j];
   if( std::abs( c ) > 1e-15 )
    neg.emplace_back( &x_vars[i*n+j] , c );
   }
  {
   auto lf  = std::make_unique< LinearFunction >( std::move(neg) );
   auto con = std::make_unique< FRowConstraint >();
   con->set_lhs( 0.0 ); con->set_rhs( Inf< double >() );
   con->set_function( lf.release() , eNoMod );
   milp_block->add_static_constraint( *con.release() );
   }
  }

 // Objective: min (1/N) * sum_j t_j   (Keutchayan et al. eq. 24)
 {
  const double inv_n = 1.0 / static_cast< double >(n);
  LinearFunction::v_coeff_pair terms;
  for( int j = 0 ; j < n ; ++j )
   terms.emplace_back( &t_vars[j] , inv_n );
  auto lf  = std::make_unique< LinearFunction >( std::move(terms) );
  auto obj = std::make_unique< FRealObjective >( milp_block.get() , lf.release() );
  obj->set_sense( Objective::eMin );
  milp_block->set_objective( obj.release() );
  }

 milp_block->generate_abstract_variables();

 auto * milp_cfg = static_cast< BlockSolverConfig * >( f_milp_config->clone() );
 milp_cfg->apply( milp_block.get() );

 Solver * milp_solver = milp_block->get_registered_solvers().empty()
  ? nullptr : milp_block->get_registered_solvers().front();
 if( ! milp_solver ) {
  milp_cfg->clear(); delete milp_cfg;
  throw std::runtime_error(
   "CSSCScenarioReductionSolver::solve_cssc_milp: no solver attached" );
  }

 // Bound the abs-of-sum MILP so a weak LP relaxation cannot make it hang;
 // on time-out we accept the best incumbent found (a valid clustering)
 if( f_milp_time_limit > 0.0 )
  milp_solver->set_par( Solver::dblMaxTime , f_milp_time_limit );

 const int status = milp_solver->compute();
 const bool have_feasible =
  ( status == Solver::kOK ) || ( status == Solver::kLowPrecision ) ||
  ( ( status == Solver::kStopTime || status == Solver::kStopIter ) &&
    milp_solver->get_var_value() < Inf< double >() );
 if( ! have_feasible ) {
  milp_cfg->clear(); delete milp_cfg;
  throw std::runtime_error(
   "CSSCScenarioReductionSolver::solve_cssc_milp: MILP failed (status "
   + std::to_string( status ) + ")" );
  }
 if( status == Solver::kStopTime || status == Solver::kStopIter )
  std::cout << "  [WARN] Step-2 MILP hit its limit (status=" << status
            << "); using best incumbent (clustering valid, optimality not "
               "proven).  Raise set_milp_time_limit() for a tighter solution.\n";

 milp_solver->get_var_solution();
 const double cssc_obj = milp_solver->get_var_value();
 milp_cfg->clear(); delete milp_cfg;

 std::cout << "  CSSC discrepancy (MILP obj) = "
           << std::fixed << std::setprecision(4) << cssc_obj << std::endl;

 // Extract representatives (u_j = 1)
 ind_red.clear();
 f_representatives.clear();
 for( int j = 0 ; j < n ; ++j )
  if( u_vars[j].get_value() > 0.5 )
   ind_red.push_back( static_cast< Index >(j) );
 f_representatives = ind_red;

 // Fallback: if MILP solution is fractional, take K highest u_j values
 if( static_cast< int >( ind_red.size() ) != K ) {
  std::vector< int > all(n);
  std::iota( all.begin() , all.end() , 0 );
  std::sort( all.begin() , all.end() , [&]( int a , int b ){
   return u_vars[a].get_value() > u_vars[b].get_value();
   } );
  ind_red.clear();
  for( int k = 0 ; k < K ; ++k )
   ind_red.push_back( static_cast< Index >( all[k] ) );
  std::sort( ind_red.begin() , ind_red.end() );
  f_representatives = ind_red;
  }

 // Assignments: x_ij = 1 -> scenario i assigned to representative j
 f_assignments.assign( n , ind_red[0] );
 for( int i = 0 ; i < n ; ++i )
  for( int j = 0 ; j < n ; ++j )
   if( x_vars[i*n+j].get_value() > 0.5 ) {
    f_assignments[ i ] = static_cast< Index >( j );
    break;
    }

 std::unordered_set< Index > rep_set( ind_red.begin() , ind_red.end() );
 indices_to_choose.clear();
 for( int i = 0 ; i < n ; ++i )
  if( ! rep_set.count( static_cast< Index >(i) ) )
   indices_to_choose.push_back( static_cast< Index >(i) );

 update_reduced_atoms();

 // Wasserstein distance using Euclidean distances between scenario vectors
 double wasserstein = 0.0;
 for( int i = 0 ; i < n ; ++i ) {
  double min_d = std::numeric_limits< double >::infinity();
  for( auto r : ind_red )
   min_d = std::min( min_d , f_dist[ i ][ r ] );
  wasserstein += f_weights[ i ] * min_d;
  }
 f_solution_value = wasserstein;

}

/*--------------------------------------------------------------------------*/
/*------------------------- get_var_solution ------------------------------*/
/*--------------------------------------------------------------------------*/

void CSSCScenarioReductionSolver::get_var_solution( Configuration * /*solc*/ )
{
 std::lock_guard< std::recursive_mutex > lock( f_mutex );

 if( ! f_Block )
  throw std::logic_error(
   "CSSCScenarioReductionSolver::get_var_solution: no Block set" );

 auto * srb = dynamic_cast< ScenarioReductionBlock * >( f_Block );
 if( ! srb )
  throw std::logic_error(
   "CSSCScenarioReductionSolver::get_var_solution: not a SRB" );

 ScenarioReductionBlockSolution sol;
 sol.selected_indices.reserve( f_representatives.size() );
 for( auto r : f_representatives )
  sol.selected_indices.push_back( f_pool_map[ r ] );

 sol.assignments.resize( static_cast< std::size_t >( f_N ) );
 for( int i = 0 ; i < f_N ; ++i )
  sol.assignments[ i ] = f_pool_map[ f_assignments[ i ] ];

 sol.weights.resize( f_representatives.size() , 0.0 );
 for( int i = 0 ; i < f_N ; ++i ) {
  const Index rep_abs = sol.assignments[ i ];
  auto it = std::find( sol.selected_indices.begin() ,
                       sol.selected_indices.end() , rep_abs );
  if( it != sol.selected_indices.end() ) {
   const std::size_t pos =
    std::distance( sol.selected_indices.begin() , it );
   sol.weights[ pos ] += f_weights[ i ];
   }
  }

 srb->set_solution( sol );
}

/*--------------------------------------------------------------------------*/
/*------- End File CSSCScenarioReductionSolver.cpp -----------------*/
/*--------------------------------------------------------------------------*/
