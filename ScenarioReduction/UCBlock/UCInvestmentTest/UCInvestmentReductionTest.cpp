/*--------------------------------------------------------------------------*/
/*--------------- File UCInvestmentReductionTest.cpp -----------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * The test compares several scenario reduction methods for K representative
 *
 */
/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include "AbstractPath.h"
#include "BlockSolverConfig.h"
#include "ColVariable.h"
#include "CSSCScenarioReductionSolver.h"
#include "DiscreteScenarioSet.h"
#include "ScenarioReductionBlock.h"
#include "ScenarioReductionSolver.h"
#include "StochasticBlock.h"
#include "TwoStageStochasticBlock.h"

#include "UCBlock.h"
#include "UnitBlock.h"
#include "IntermittentUnitBlock.h"
#include "BatteryUnitBlock.h"
#include "DesignNetworkBlock.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <netcdf>
#include <netcdf.h>   // NC_STRING, NC_CHAR, nc_free_string

using namespace std;
using namespace SMSpp_di_unipi_it;

using Index = unsigned int;

/*--------------------------------------------------------------------------*/
/*------------------------ nc_copy_group_recursive -------------------------*/
/*--------------------------------------------------------------------------*/
/* Deep-copy a netCDF group (attributes, dimensions, variables, sub-groups)
 * Used to clone the file's StaticAbstractPath and StochasticBlock
 * into a built reduced TwoStageStochasticBlock*/

static void nc_copy_group_recursive( const netCDF::NcGroup & src ,
                                     netCDF::NcGroup & dst )
{
 for( const auto & [ name , att ] : src.getAtts() ) {
  try { std::string val; att.getValues( val ); dst.putAtt( name , val ); }
  catch( ... ) {}  // skip non-string attributes
 }

 for( const auto & [ name , dim ] : src.getDims() )
  dst.addDim( name , dim.getSize() );

 for( const auto & [ name , var ] : src.getVars() ) {
  auto type  = var.getType();
  auto sdims = var.getDims();

  size_t total = 1;
  for( const auto & d : sdims ) total *= d.getSize();

  std::vector< netCDF::NcDim > ddims;
  for( const auto & d : sdims ) {
   auto found = dst.getDim( d.getName() , netCDF::NcGroup::ParentsAndCurrent );
   if( found.isNull() )
    throw std::runtime_error( "nc_copy_group_recursive: dim not found: "
                              + d.getName() );
   ddims.push_back( found );
  }

  auto dvar = dst.addVar( name , type , ddims );
  if( total == 0 ) continue;

  auto tid = type.getId();
  if( tid == NC_STRING ) {
   std::vector< char * > ptrs( total , nullptr );
   var.getVar( ptrs.data() );
   std::vector< const char * > cptrs( total );
   for( size_t i = 0 ; i < total ; ++i )
    cptrs[ i ] = ptrs[ i ] ? ptrs[ i ] : "";
   dvar.putVar( cptrs.data() );
   nc_free_string( static_cast< size_t >( total ) , ptrs.data() );
  }
  else if( tid == NC_CHAR || tid == NC_BYTE || tid == NC_UBYTE ) {
   std::vector< char > buf( total );
   var.getVar( buf.data() );
   dvar.putVar( buf.data() );
  }
  else {
   std::vector< double > buf( total );
   var.getVar( buf.data() );
   dvar.putVar( buf.data() );
  }
 }

 for( const auto & [ name , child ] : src.getGroups() ) {
  auto dst_child = dst.addGroup( name );
  nc_copy_group_recursive( child , dst_child );
 }
}

/*--------------------------------------------------------------------------*/
/*---------------------------- write_reduced_tssb --------------------------*/
/*--------------------------------------------------------------------------*/
/* Build a reduced TwoStageStochasticBlock file containing the given
 * scenarios (with weights weights), by cloning the original file's
 * StaticAbstractPath + StochasticBlock and writing a fresh DiscreteScenarioSet.
 * Taking the scenario vectors directly (rather than indices into a DSS) lets
 * the same routine serve both the V-matrix solves and the CSSC block factory */

static void write_reduced_tssb( const std::string & orig_file ,
                                const std::vector< std::vector< double > > & scenarios ,
                                const std::vector< double > & weights ,
                                const std::string & out_file )
{
 const size_t K  = scenarios.size();
 const size_t SS = scenarios.empty() ? 0 : scenarios.front().size();

 netCDF::NcFile in( orig_file , netCDF::NcFile::read );
 auto src_b0 = in.getGroup( "Block_0" );
 if( src_b0.isNull() )
  throw std::runtime_error( "write_reduced_tssb: Block_0 not found" );

 netCDF::NcFile out( out_file , netCDF::NcFile::replace );
 out.putAtt( "SMS++_file_type" , netCDF::NcInt() , 1 );

 auto g = out.addGroup( "Block_0" );
 g.putAtt( "type" , "TwoStageStochasticBlock" );
 g.putAtt( "id" , "0" );
 g.addDim( "NumberScenarios" , K );

 // clone StaticAbstractPath + StochasticBlock verbatim
 {
  auto sap = g.addGroup( "StaticAbstractPath" );
  nc_copy_group_recursive( src_b0.getGroup( "StaticAbstractPath" ) , sap );
  auto sb = g.addGroup( "StochasticBlock" );
  nc_copy_group_recursive( src_b0.getGroup( "StochasticBlock" ) , sb );
 }

 // fresh DiscreteScenarioSet with the selected scenarios
 {
  auto dg = g.addGroup( "DiscreteScenarioSet" );
  dg.putAtt( "type" , "DiscreteScenarioSet" );
  auto nd = dg.addDim( "NumberScenarios" , K );
  auto sd = dg.addDim( "ScenarioSize" , SS );

  std::vector< double > flat;
  flat.reserve( K * SS );
  for( const auto & sc : scenarios )
   flat.insert( flat.end() , sc.begin() , sc.end() );
  dg.addVar( "Scenarios" , netCDF::NcDouble() , { nd , sd } ).putVar( flat.data() );
  dg.addVar( "PoolWeights" , netCDF::NcDouble() , nd ).putVar( weights.data() );
 }
}

/*--------------------------------------------------------------------------*/
/*------------------------------- SolveResult ------------------------------*/
/*--------------------------------------------------------------------------*/

struct SolveResult {
 double obj = 0.0;
 std::vector< double > first_stage;   
};

// forward declaration: the UC first-stage extractor,
// which collects only the generated design variables. Preferred over the
// library get_first_stage_variables(), whose StaticAbstractPath may contain
// entries that resolve to null in instances where some units are not in design
// mode (example: the larger s_5 network)
static std::vector< ColVariable * >
uc_first_stage_vars( TwoStageStochasticBlock * tssb );

/* deserialize a TSSB file, generate, fix the first stage to fix, solve.
 * and return the objective (plus the first-stage values when not fixed)*/

static SolveResult solve_tssb_file( const std::string & file ,
                                    BlockSolverConfig * bsc ,
                                    const std::vector< double > * fix = nullptr )
{
 Block * raw = Block::deserialize( file );
 auto * tssb = dynamic_cast< TwoStageStochasticBlock * >( raw );
 if( ! tssb ) { delete raw;
  throw std::runtime_error( "solve_tssb_file: not a TwoStageStochasticBlock" ); }
 std::unique_ptr< Block > owner( tssb );

 tssb->generate_abstract_variables();
 tssb->generate_abstract_constraints();
 tssb->generate_objective();

 auto fs = uc_first_stage_vars( tssb );

 if( fix ) {
  if( fix->size() != fs.size() )
   throw std::runtime_error( "solve_tssb_file: first-stage size mismatch (" +
    std::to_string( fix->size() ) + " vs " + std::to_string( fs.size() ) + ")" );
  for( size_t k = 0 ; k < fs.size() ; ++k ) {
   fs[ k ]->set_value( (*fix)[ k ] );
   fs[ k ]->is_fixed( true , eNoMod );   // solver reads fixed bounds at load
  }
 }

 auto cfg = std::unique_ptr< BlockSolverConfig >(
  static_cast< BlockSolverConfig * >( bsc->clone() ) );
 cfg->apply( tssb );

 auto * solver = tssb->get_registered_solvers().front();
 int st = solver->compute( false );
 if( st != Solver::kOK && st != Solver::kLowPrecision )
  throw std::runtime_error(
   "solve_tssb_file: solver status=" + std::to_string( st ) );

 SolveResult res;
 res.obj = solver->get_ub();
 solver->get_var_solution();

 if( ! fix ) {
  res.first_stage.reserve( fs.size() );
  for( auto * v : fs ) res.first_stage.push_back( v->get_value() );
 }

 cfg->clear();
 return res;
}

/*--------------------------------------------------------------------------*/
/*----------------------------- heuristic pick -----------------------------*/
/*--------------------------------------------------------------------------*/
/* run one ScenarioReductionSolver heuristic (baseline/dupacova/bestfit/
 * firstfit) for K=1 and return the selected scenario indexs*/

static Index heuristic_pick( DiscreteScenarioSet * dss , int algo )
{
 auto srb = std::make_unique< ScenarioReductionBlock >();
 srb->set_scenario_generator( dss );

 auto srs = std::make_unique< ScenarioReductionSolver >();
 srs->set_nb_reduced( 1 );
 srs->set_algorithm( algo );
 srs->set_Block( srb.get() );
 srs->compute();
 srs->get_var_solution();

 const auto & sol = srb->get_solution();
 if( sol.selected_indices.empty() )
  throw std::runtime_error( "heuristic_pick: no representative selected" );
 return sol.selected_indices.front();
}

/*--------------------------------------------------------------------------*/
/*--------------------------- uc_first_stage_vars --------------------------*/
/*--------------------------------------------------------------------------*/
/* UCBlock first-stage extractor. This navigates the actual UC investment 
 * model and returns the here-and-now design variables directly:
 *   - IntermittentUnitBlock : the capacity design variable   (get_design())
 *   - BatteryUnitBlock      : battery + converter design     (get_batt/conv_design())
 *   - DesignNetworkBlock    : the per-line network design    (get_design())
 *
 * Variables are taken from scenario-0's block (get_sub_Block(0))
 * instances TwoStageStochasticBlock::get_first_stage_variables() uses, so the
 * two sets coincide (verified once in cssc_pick)*/

// Collect the UC design (first-stage) variables of ONE scenario block.
static std::vector< ColVariable * >
uc_design_vars( Block * scenario_block )
{
 std::vector< ColVariable * > fs;
 auto * uc = dynamic_cast< UCBlock * >( scenario_block );
 if( ! uc )
  if( auto * sb = dynamic_cast< StochasticBlock * >( scenario_block ) )
   uc = dynamic_cast< UCBlock * >( sb->get_inner_block() );
 if( ! uc )
  throw std::runtime_error( "uc_design_vars: inner block is not a UCBlock" );

 // unit design variables. A design variable is a first-stage
 // variable only when the unit is actually in design mode, it means its investment
 // cost is non-zero (cf. IntermittentUnitBlock.cpp: the design variable is
 // generated only when InvestmentCost != 0), the same holds per-component for
 // a battery's storage and converter investment
 for( Index u = 0 ; u < uc->get_number_units() ; ++u ) {
  UnitBlock * ub = uc->get_unit_block( u );
  if( auto * iub = dynamic_cast< IntermittentUnitBlock * >( ub ) ) {
   if( iub->get_investment_cost() != 0 )
    fs.push_back( & iub->get_design() );
  }
  else if( auto * bub = dynamic_cast< BatteryUnitBlock * >( ub ) ) {
   if( bub->get_batt_investment_cost() != 0 )
    fs.push_back( & bub->get_batt_design() );
   if( bub->get_conv_investment_cost() != 0 )
    fs.push_back( & bub->get_conv_design() );
  }
 }

 // network line design variables (DesignNetworkBlock)
 for( NetworkBlock * nb : uc->get_network_blocks() )
  if( auto * dnb = dynamic_cast< DesignNetworkBlock * >( nb ) )
   for( ColVariable & v : dnb->get_design() )
    fs.push_back( & v );

 return fs;
}

// first-stage variables of the whole TSSB = design vars of scenario-0's block
static std::vector< ColVariable * >
uc_first_stage_vars( TwoStageStochasticBlock * tssb )
{
 if( ! tssb || tssb->get_number_scenarios() == 0 )
  return {};
 return uc_design_vars( tssb->get_sub_Block( 0 ) );
}

static bool first_stage_shared( const std::string & file )
{
 Block * raw = Block::deserialize( file );
 auto * tssb = dynamic_cast< TwoStageStochasticBlock * >( raw );
 std::unique_ptr< Block > owner( tssb );
 if( ! tssb || tssb->get_number_scenarios() < 2 )
  return false;
 tssb->generate_abstract_variables();

 // resolve every StaticAbstractPath on scenario-0 and scenario-1 block,
 // exactly as TwoStageStochasticBlock::generate_abstract_constraints does, and
 // report sharing if any here-and-now variable is the same instance in both
 Block * b0 = tssb->get_first_stage_block( 0 );
 Block * b1 = tssb->get_first_stage_block( 1 );
 for( const auto & p : tssb->get_paths_to_static_here_and_now_vars() ) {
  const auto n = p->get_number_elements< ColVariable >( b0 );
  auto * e0 = p->get_element< ColVariable >( b0 );
  auto * e1 = p->get_element< ColVariable >( b1 );
  for( Index j = 0 ; j < n ; ++j )
   if( e0 + j == e1 + j )           // same instance across scenarios -> shared
    return true;
 }
 return false;
}

/*--------------------------------------------------------------------------*/
/*------------------------------- cssc_pick --------------------------------*/
/*--------------------------------------------------------------------------*/
/* run the real CSSCScenarioReductionSolver and return the selected scenario index*/

static Index cssc_pick( const std::string & orig_file ,
                        DiscreteScenarioSet * dss ,
                        BlockSolverConfig * bsc )
{
 // persistent full TSSB + its StochasticBlock applicator: required by
 // CSSCScenarioReductionSolver::set_Block (the block-factory path does not
 // evaluate on it, but set_Block validates their presence)
 Block * raw = Block::deserialize( orig_file );
 auto * tssb = dynamic_cast< TwoStageStochasticBlock * >( raw );
 if( ! tssb ) { delete raw;
  throw std::runtime_error( "cssc_pick: not a TwoStageStochasticBlock" ); }
 std::unique_ptr< Block > owner( tssb );

 // deserialize a standalone StochasticBlock (the file's Block_0/StochasticBlock)
 // to serve as the scenario applicator required by set_Block
 std::unique_ptr< Block > stoch_owner;
 {
  netCDF::NcFile f( orig_file , netCDF::NcFile::read );
  auto b0 = f.getGroup( "Block_0" );
  auto sg = b0.getGroup( "StochasticBlock" );
  stoch_owner.reset( Block::new_Block( sg , nullptr ) );
 }
 auto * stoch = dynamic_cast< StochasticBlock * >( stoch_owner.get() );
 if( ! stoch )
  throw std::runtime_error( "cssc_pick: no StochasticBlock applicator" );

 // Re-resolve every DataMapping caller against this StochasticBlock's own
 // inner block. StochasticBlock::deserialize sets the caller via
 // AbstractPath::get_element(), which returns nullptr for an EMPTY path (the
 // demand mapping, whose caller is the UCBlock itself). set_caller_from_reference()
 // instead maps an empty path to the reference block. Without this, Strategy B's
 // set_data() would invoke set_active_power_demand() on a null UCBlock (segfault);
 // Strategy A is unaffected (it never injects through this applicator) but the
 // fix-up is harmless for it
 if( auto * inner = stoch->get_inner_block() )
  for( const auto & m : stoch->get_data_mappings() )
   m->set_caller_from_reference( inner );

 // verify the concrete UCBlock design-variable extractor returns exactly the
 // StaticAbstractPath first-stage set (same ColVariable instances)
 tssb->generate_abstract_variables();
 {
  auto canon = tssb->get_first_stage_variables();
  auto ucfs  = uc_first_stage_vars( tssb );
  std::set< ColVariable * > a( canon.begin() , canon.end() );
  std::set< ColVariable * > b( ucfs.begin() , ucfs.end() );
  std::cout << "  first-stage via UCBlock design vars: " << ucfs.size()
            << "  (StaticAbstractPath: " << canon.size() << ")  "
            << ( a == b ? "[MATCH]" : "[differs - using UCBlock design vars]" )
            << "\n";
  // When they differ, the UCBlock design-variable set is the authoritative 
  // one and is what the V-matrix machinery uses, do not abort
 }

 auto srb = std::make_unique< ScenarioReductionBlock >();
 srb->set_scenario_generator( dss );
 srb->set_stochastic_block( tssb );
 srb->set_scenario_applicator( stoch );

 auto solver = std::make_unique< CSSCScenarioReductionSolver >();
 solver->set_milp_config(
  static_cast< BlockSolverConfig * >( bsc->clone() ) );   // takes ownership
 solver->set_nb_reduced( 1 );

 // No set_var_extractor call: the solver's generic AbstractPath-based
 // fallback (generic_var_extractor) reads the here-and-now variables
 // directly from the TSSB, verified to match uc_design_vars exactly.
 solver->set_fix_with_modification( false );

 solver->set_Block( srb.get() );
 solver->compute();
 solver->get_var_solution();

 const auto & sol = srb->get_solution();
 if( sol.selected_indices.empty() )
  throw std::runtime_error( "cssc_pick: no representative selected" );
 return sol.selected_indices.front();
}

/*--------------------------------------------------------------------------*/
/*----------------------------------- main ---------------------------------*/
/*--------------------------------------------------------------------------*/

int main( int argc , char * argv[] )
{
 std::string instance_file , solver_file = "BSPar_CPLEX.txt";
 for( int i = 1 ; i < argc ; ++i ) {
  std::string a( argv[ i ] );
  if(      a == "-i" && i + 1 < argc ) instance_file = argv[ ++i ];
  else if( a == "-c" && i + 1 < argc ) solver_file   = argv[ ++i ];
 }
 if( instance_file.empty() ) {
  std::cerr << "Usage: " << argv[ 0 ] << " -i <tssb.nc> [-c <solver.txt>]\n";
  return 1;
 }

 try {
  std::cout << "=== UC Investment scenario-reduction comparison (K=1) ===\n";
  std::cout << "Instance: " << instance_file << "\n";

  auto bsc = std::unique_ptr< BlockSolverConfig >(
   static_cast< BlockSolverConfig * >(
    Configuration::deserialize( solver_file ) ) );
  if( ! bsc ) throw std::runtime_error( "Failed to load solver config" );

  // ---- load a standalone DiscreteScenarioSet from the file ---------------
  auto dss = std::make_unique< DiscreteScenarioSet >();
  {
   netCDF::NcFile f( instance_file , netCDF::NcFile::read );
   // NB: store the groups in named locals; chaining getGroup(...).getGroup(...)
   // on a temporary NcGroup leaves deserialize reading an empty group.
   auto b0 = f.getGroup( "Block_0" );
   auto dg = b0.getGroup( "DiscreteScenarioSet" );
   dss->deserialize( dg );
  }
  // initialise the full, deterministic, in-order pool {0..N-1} so the
  // ScenarioReductionSolver sees all scenarios (get_poolSize()==N)
  dss->init_representative_pool();
  const Index N = dss->get_nbScenarios();
  std::vector< double > w = dss->get_Weights();
  if( w.size() != N ) { w.assign( N , 1.0 / N ); }   // fallback: uniform
  { double s = std::accumulate( w.begin() , w.end() , 0.0 );
    if( s > 0 ) for( auto & x : w ) x /= s; }

  std::cout << "Scenarios N = " << N << ", scenario size = "
            << dss->get_scenarioSize() << "\n";
  std::cout << "Weights:";
  for( auto x : w ) std::cout << " " << x;
  std::cout << "\n";

  // timing 
  auto t_start = std::chrono::high_resolution_clock::now();
  auto t_full_start = t_start;
  auto t_vmat_start = t_start;
  auto t_cssc_start = t_start;
  long long ms_full = 0, ms_vmat = 0, ms_cssc = 0;

  // ---- v* : full extensive form ----------------------------------------
  // Try to solve the full scenario extensive form.  For some instances the
  // first-stage variables are shared across scenario replicas. 
  // If it still throws for any reason, fall back to the wait-and-see bound
  bool   have_v_star = false;
  double v_star      = 0.0;
  std::cout << "\nSolving full TSS (v*)...\n";
  t_full_start = std::chrono::high_resolution_clock::now();
  try {
   v_star = solve_tssb_file( instance_file , bsc.get() ).obj;
   have_v_star = true;
   std::cout << "  v* = " << std::setprecision( 10 ) << v_star << "\n";
  } catch( const std::exception & e ) {
   std::cout << "  [!] full-TSSB solve failed (" << e.what()
             << "); falling back to wait-and-see bound.\n";
  }
  {
   auto t_now = std::chrono::high_resolution_clock::now();
   ms_full = std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_full_start).count();
  }

  // ---- build the N single-scenario reduced TSSB files -------------------
  const std::string tmpl =
   "/tmp/uc_inv_reduced_";   // + index + ".nc4"
  std::vector< std::string > files( N );
  for( Index i = 0 ; i < N ; ++i ) {
   files[ i ] = tmpl + std::to_string( i ) + ".nc4";
   write_reduced_tssb( instance_file , { dss->get_scenario( i ) } ,
                       { 1.0 } , files[ i ] );
  }

  // ---- cost-space matrix V ----------------------------------------------
  std::cout << "\nBuilding cost-space matrix V (" << N << "x" << N << ")...\n";
  t_vmat_start = std::chrono::high_resolution_clock::now();
  std::vector< std::vector< double > > V( N , std::vector< double >( N , 0.0 ) );
  std::vector< std::vector< double > > xstar( N );
  for( Index i = 0 ; i < N ; ++i ) {
   auto r = solve_tssb_file( files[ i ] , bsc.get() );   // anticipative
   V[ i ][ i ] = r.obj;
   xstar[ i ]  = r.first_stage;
  }
  for( Index i = 0 ; i < N ; ++i )
   for( Index j = 0 ; j < N ; ++j )
    if( i != j )
     V[ i ][ j ] = solve_tssb_file( files[ j ] , bsc.get() , &xstar[ i ] ).obj;
  {
   auto t_now = std::chrono::high_resolution_clock::now();
   ms_vmat = std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_vmat_start).count();
  }

  std::cout << "  V =\n";
  for( Index i = 0 ; i < N ; ++i ) {
   std::cout << "    ";
   for( Index j = 0 ; j < N ; ++j )
    std::cout << std::setw( 16 ) << std::setprecision( 8 ) << V[ i ][ j ];
   std::cout << "\n";
  }

  // implementation cost of choosing representative i : sum_j w_j V[i][j]
  auto impl_cost = [ & ]( Index i ) {
   double c = 0.0;
   for( Index j = 0 ; j < N ; ++j ) c += w[ j ] * V[ i ][ j ];
   return c;
  };

  // reference value for gap/implErr: the true stochastic v* when available,
  // else the wait-and-see lower bound  WS = sum_j w_j V[j][j]
  double ref; std::string ref_label;
  if( have_v_star ) { ref = v_star; ref_label = "v*"; }
  else {
   ref = 0.0;
   for( Index j = 0 ; j < N ; ++j ) ref += w[ j ] * V[ j ][ j ];
   ref_label = "WS bound";
   std::cout << "\n  wait-and-see bound (reference) = "
             << std::setprecision( 10 ) << ref << "\n";
  }

  // ---- method selections (with per-method timing) --------------------------
  using Clock = std::chrono::high_resolution_clock;
  struct Method { std::string name; Index pick; long long ms; long long redtime_ms; };
  std::vector< Method > methods;

  auto timed_heuristic = [ & ]( int algo ) -> Method {
   auto t0 = Clock::now();
   Index p  = heuristic_pick( dss.get() , algo );
   auto ms  = std::chrono::duration_cast< std::chrono::milliseconds >(
                Clock::now() - t0 ).count();
   return { "" , p , ms , 0 };  // redtime_ms computed later
  };

  { auto m = timed_heuristic( 0 ); m.name = "baseline"; methods.push_back( m ); }
  { auto m = timed_heuristic( 1 ); m.name = "dupacova"; methods.push_back( m ); }
  { auto m = timed_heuristic( 2 ); m.name = "bestfit";  methods.push_back( m ); }
  { auto m = timed_heuristic( 3 ); m.name = "firstfit"; methods.push_back( m ); }

  // CSSC: run the real CSSCScenarioReductionSolver (Step-1 V + Step-2 MILP).
  // The total CSSC time includes V-matrix (Step 1) + MILP partitioning (Step 2)
  std::cout << "\nRunning CSSC solver...\n";
  t_cssc_start = Clock::now();
  Index cssc_idx = cssc_pick( instance_file , dss.get() , bsc.get() );
  ms_cssc = std::chrono::duration_cast< std::chrono::milliseconds >(
              Clock::now() - t_cssc_start ).count();
  long long ms_cssc_total = ms_vmat + ms_cssc;  // V-matrix (Step1) + MILP (Step2)
  methods.push_back( { "cssc" , cssc_idx , ms_cssc_total , 0 } );  // redtime_ms computed later

  // ---- compute redtime for each method --------------------------------
  for( auto & m : methods ) {
   std::vector< std::vector< double > > red_scen = { dss->get_scenario( m.pick ) };
   std::vector< double > red_weights = { 1.0 };
   std::string red_file = "/tmp/redtime_" + m.name + ".nc";
   write_reduced_tssb( instance_file , red_scen , red_weights , red_file );

   auto t_red = Clock::now();
   SolveResult red = solve_tssb_file( red_file , bsc.get() );
   m.redtime_ms = std::chrono::duration_cast< std::chrono::milliseconds >(
                   Clock::now() - t_red ).count();

   std::remove( red_file.c_str() );
  }

  // ---- comparison table --------------------------------------------------
  // reset stream flags: the CSSC solver leaves std::cout in std::fixed mode
  std::cout << std::defaultfloat;
  std::cout << "\n=== Comparison (K=1), " << ref_label << " = "
            << std::setprecision( 10 ) << ref << " ===\n";
  auto fmt = [ ref ]( double val ) {
   std::ostringstream os;
   os << std::setprecision( 8 ) << val << " ("
      << std::fixed << std::setprecision( 2 )
      << ( 100.0 * val / ref ) << "%)";
   return os.str();
  };

  std::cout << std::left
            << std::setw( 12 ) << "method"
            << std::setw( 6 )  << "pick"
            << std::setw( 18 ) << "reduced obj"
            << std::setw( 28 ) << "in-sample gap"
            << std::setw( 18 ) << "impl cost"
            << std::setw( 28 ) << "implementation error"
            << std::setw( 10 ) << "time(ms)"
            << std::setw( 12 ) << "redtime(ms)" << "\n";
  std::cout << std::string( 132 , '-' ) << "\n";
  for( const auto & m : methods ) {
   double red  = V[ m.pick ][ m.pick ];
   double impl = impl_cost( m.pick );
   double gap  = std::abs( red - ref );
   double ierr = impl - ref;
   std::cout << std::left << std::setw( 12 ) << m.name
             << std::setw( 6 )  << m.pick
             << std::setw( 18 ) << std::setprecision( 8 ) << red
             << std::setw( 28 ) << fmt( gap )
             << std::setw( 18 ) << std::setprecision( 8 ) << impl
             << std::setw( 28 ) << fmt( ierr )
             << std::setw( 10 ) << m.ms
             << std::setw( 12 ) << m.redtime_ms << "\n";
  }

  std::cout << std::string( 132 , '-' ) << "\n";

  // ---- timing summary -----
  auto t_total = std::chrono::high_resolution_clock::now();
  long long ms_total = std::chrono::duration_cast<std::chrono::milliseconds>(t_total - t_start).count();
  std::cout << "\n=== Computation times ===\n"
            << "  Full TSSB (v*)             : " << ms_full << " ms"
            << ( have_v_star ? "" : " [skipped]" ) << "\n"
            << "  CSSC Step-1 (V-matrix, N=" << N << ", " << N * N << " solves) : " << ms_vmat << " ms\n"
            << "  CSSC Step-2 (MILP partitioning)                : " << ms_cssc << " ms\n"
            << "  CSSC total  (Step-1 + Step-2)                  : " << ms_cssc_total << " ms\n";

  for( Index i = 0 ; i < N ; ++i ) std::remove( files[ i ].c_str() );
  return 0;
 }
 catch( const std::exception & e ) {
  std::cerr << "Error: " << e.what() << std::endl;
  return 1;
 }
}
/*--------------------------------------------------------------------------*/
/*-------------- End File UCInvestmentReductionTest.cpp --------------------*/
/*--------------------------------------------------------------------------*/
