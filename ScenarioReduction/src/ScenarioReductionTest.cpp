/*--------------------------------------------------------------------------*/
/*------------------ File ScenarioReductionSolve.cpp ------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Fully generic scenario-reduction solve program: reads a pre-built
 * TwoStageStochasticBlock (TSSB) file, holding the base problem, its N
 * scenarios, and their StaticAbstractPath (here-and-now variable) metadata,
 * runs a scenario-reduction method (a heuristic via ScenarioReductionSolver,
 * or CSSC via CSSCScenarioReductionSolver), solves the reduced K-scenario
 * problem, and reports the in-sample gap against the full N-scenario
 * solution.
 *
 * This file has NO dependency on any concrete Block type (no CFL, no UC
 * headers, no dynamic_cast to a problem-specific class anywhere): every
 * piece of problem-specific knowledge (what the first-stage variables are,
 * how scenario data maps onto the model) is read from the TSSB file itself,
 * via the generic AbstractPath mechanism. Building a TSSB file in the first
 * place is necessarily problem-specific and is the job of a separate
 * "generate" program (CFLScenarioGenerator, UCScenarioGenerator, ...); this
 * program only ever reads one.
 *
 * Usage:
 *   scenario_reduction_solve -i <tssb.nc4> -m <method> -r <K> -c <solver.txt>
 *
 * \author Minh Duc Pham \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 */
/*--------------------------------------------------------------------------*/

#include "BlockSolverConfig.h"
#include "ColVariable.h"
#include "CSSCScenarioReductionSolver.h"
#include "DiscreteScenarioSet.h"
#include "ScenarioReductionBlock.h"
#include "ScenarioReductionSolver.h"
#include "StochasticBlock.h"
#include "TwoStageStochasticBlock.h"

#include <chrono>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

#include <netcdf>
#include <netcdf.h>   // NC_STRING, NC_CHAR, nc_free_string

using namespace std;
using namespace SMSpp_di_unipi_it;

using Index = unsigned int;

/*--------------------------------------------------------------------------*/
/*------------------------ nc_copy_group_recursive --------------------------*/
/*--------------------------------------------------------------------------*/
/* Deep-copy a netCDF group (attributes, dimensions, variables, sub-groups).
 * Used to clone a TSSB file's StaticAbstractPath + StochasticBlock verbatim
 * into a freshly-built reduced-scenario TSSB file: this is pure byte-level
 * netCDF copying, blind to what any of it means. */

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
  else if( tid == NC_CHAR ) {
   // NC_CHAR is the only text numeric-ish type netCDF-cxx4's char*
   // get/putVar overload accepts; NC_BYTE/NC_UBYTE are true NUMERIC types
   // and must go through the generic numeric branch below, or the library
   // throws "Attempt to convert between text & numbers"
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
/*----------------------------- write_reduced_tssb --------------------------*/
/*--------------------------------------------------------------------------*/
/* Build a reduced TSSB file containing exactly the given scenarios (with the
 * given weights), by cloning the original file's StaticAbstractPath and
 * StochasticBlock groups verbatim and writing a fresh DiscreteScenarioSet.
 * Generic: works for any TSSB regardless of the underlying problem type */

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

 {
  auto sap = g.addGroup( "StaticAbstractPath" );
  nc_copy_group_recursive( src_b0.getGroup( "StaticAbstractPath" ) , sap );
  auto sb = g.addGroup( "StochasticBlock" );
  nc_copy_group_recursive( src_b0.getGroup( "StochasticBlock" ) , sb );
 }

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
/*------------------------------- solve_tssb_file ---------------------------*/
/*--------------------------------------------------------------------------*/
/* Deserialize a TSSB file, generate its abstract representation, solve, and
 * return the objective. Generic: get_first_stage_variables() reads the
 * TSSB's own StaticAbstractPath, no problem-specific extractor needed */

static double solve_tssb_file( const std::string & file , BlockSolverConfig * bsc )
{
 Block * raw = Block::deserialize( file );
 auto * tssb = dynamic_cast< TwoStageStochasticBlock * >( raw );
 if( ! tssb ) { delete raw;
  throw std::runtime_error( "solve_tssb_file: not a TwoStageStochasticBlock" ); }
 std::unique_ptr< Block > owner( tssb );

 tssb->generate_abstract_variables();
 tssb->generate_abstract_constraints();
 tssb->generate_objective();

 auto cfg = std::unique_ptr< BlockSolverConfig >(
  static_cast< BlockSolverConfig * >( bsc->clone() ) );
 cfg->apply( tssb );

 auto * solver = tssb->get_registered_solvers().front();
 int st = solver->compute( false );
 if( st != Solver::kOK && st != Solver::kLowPrecision )
  throw std::runtime_error(
   "solve_tssb_file: solver status=" + std::to_string( st ) );

 double obj = solver->get_ub();
 cfg->clear();
 return obj;
}

/*--------------------------------------------------------------------------*/
/*--------------------------------- Reduction -------------------------------*/
/*--------------------------------------------------------------------------*/
/* Result of a reduction method: which absolute scenario indices were picked
 * as representatives, and which representative every scenario is assigned
 * to (same indexing as DiscreteScenarioSet) */

struct Reduction {
 std::vector< Index > selected;      // K representative indices
 std::vector< Index > assignment;    // N entries, one per scenario
 std::vector< double > weights;      // K aggregated weights, sum to 1
};

/*--------------------------------------------------------------------------*/
/*------------------------------- heuristic_reduce ---------------------------*/
/*--------------------------------------------------------------------------*/
/* Baseline / Dupacova / BestFit / FirstFit via ScenarioReductionSolver.
 * Generic: reads scenario vectors + weights straight from the DSS, no
 * knowledge of the underlying Block needed at all */

static Reduction heuristic_reduce( DiscreteScenarioSet * dss , int K , int algo )
{
 auto srb = std::make_unique< ScenarioReductionBlock >();
 srb->set_scenario_generator( dss );

 auto srs = std::make_unique< ScenarioReductionSolver >();
 srs->set_nb_reduced( K );
 srs->set_algorithm( algo );
 srs->set_Block( srb.get() );
 srs->compute();
 srs->get_var_solution();

 const auto & sol = srb->get_solution();
 if( sol.selected_indices.empty() )
  throw std::runtime_error( "heuristic_reduce: no representative selected" );

 Reduction r;
 r.selected   = sol.selected_indices;
 r.assignment = sol.assignments;
 r.weights    = sol.weights;
 return r;
}

/*--------------------------------------------------------------------------*/
/*--------------------------------- cssc_reduce ------------------------------*/
/*--------------------------------------------------------------------------*/
/* CSSC via CSSCScenarioReductionSolver, reading a pre-built TSSB file.
 * Generic: no set_var_extractor() call, the solver's own AbstractPath-based
 * fallback (generic_var_extractor) identifies the here-and-now variables
 * directly from the TSSB, whatever problem it describes */

static Reduction cssc_reduce( const std::string & tssb_file ,
                              DiscreteScenarioSet * dss , int K ,
                              BlockSolverConfig * bsc )
{
 Block * raw = Block::deserialize( tssb_file );
 auto * tssb = dynamic_cast< TwoStageStochasticBlock * >( raw );
 if( ! tssb ) { delete raw;
  throw std::runtime_error( "cssc_reduce: not a TwoStageStochasticBlock" ); }
 std::unique_ptr< Block > owner( tssb );

 // Deserialize a standalone StochasticBlock (the file's own Block_0/
 // StochasticBlock group) to serve as the scenario applicator required by
 // CSSCScenarioReductionSolver::set_Block
 std::unique_ptr< Block > stoch_owner;
 {
  netCDF::NcFile f( tssb_file , netCDF::NcFile::read );
  auto b0 = f.getGroup( "Block_0" );
  auto sg = b0.getGroup( "StochasticBlock" );
  stoch_owner.reset( Block::new_Block( sg , nullptr ) );
 }
 auto * stoch = dynamic_cast< StochasticBlock * >( stoch_owner.get() );
 if( ! stoch )
  throw std::runtime_error( "cssc_reduce: no StochasticBlock applicator" );

 // Re-resolve every DataMapping caller against this StochasticBlock's own
 // inner block: an empty-path mapping (caller is the inner Block itself)
 // resolves to null when freshly deserialized standalone
 if( auto * inner = stoch->get_inner_block() )
  for( const auto & m : stoch->get_data_mappings() )
   m->set_caller_from_reference( inner );

 auto srb = std::make_unique< ScenarioReductionBlock >();
 srb->set_scenario_generator( dss );
 srb->set_stochastic_block( tssb );
 srb->set_scenario_applicator( stoch );

 auto solver = std::make_unique< CSSCScenarioReductionSolver >();
 solver->set_milp_config( static_cast< BlockSolverConfig * >( bsc->clone() ) );
 solver->set_nb_reduced( K );
 solver->set_fix_with_modification( false );
 solver->set_Block( srb.get() );
 solver->compute();
 solver->get_var_solution();

 const auto & sol = srb->get_solution();
 if( sol.selected_indices.empty() )
  throw std::runtime_error( "cssc_reduce: no representative selected" );

 Reduction r;
 r.selected   = sol.selected_indices;
 r.assignment = sol.assignments;
 r.weights    = sol.weights;
 return r;
}

/*--------------------------------------------------------------------------*/
/*----------------------------------- main -----------------------------------*/
/*--------------------------------------------------------------------------*/

int main( int argc , char * argv[] )
{
 std::string instance_file , solver_file = "BSPar_CPLEX.txt" , method = "cssc";
 int K = 5;

 for( int i = 1 ; i < argc ; ++i ) {
  std::string a( argv[ i ] );
  if(      a == "-i" && i + 1 < argc ) instance_file = argv[ ++i ];
  else if( a == "-c" && i + 1 < argc ) solver_file   = argv[ ++i ];
  else if( a == "-m" && i + 1 < argc ) method        = argv[ ++i ];
  else if( a == "-r" && i + 1 < argc ) K             = std::atoi( argv[ ++i ] );
 }
 if( instance_file.empty() ) {
  std::cerr << "Usage: " << argv[ 0 ]
            << " -i <tssb.nc4> [-m <method>] [-r <K>] [-c <solver.txt>]\n"
            << "  method: baseline | dupacova | bestfit | firstfit | cssc\n";
  return 1;
 }

 try {
  std::cout << "=== Generic scenario-reduction solve (K=" << K << ") ===\n";
  std::cout << "Instance: " << instance_file << "\n";
  std::cout << "Method:   " << method << "\n";

  auto bsc = std::unique_ptr< BlockSolverConfig >(
   static_cast< BlockSolverConfig * >(
    Configuration::deserialize( solver_file ) ) );
  if( ! bsc ) throw std::runtime_error( "Failed to load solver config" );

  // ---- load a standalone DiscreteScenarioSet from the file --------------
  auto dss = std::make_unique< DiscreteScenarioSet >();
  {
   netCDF::NcFile f( instance_file , netCDF::NcFile::read );
   auto b0 = f.getGroup( "Block_0" );
   auto dg = b0.getGroup( "DiscreteScenarioSet" );
   dss->deserialize( dg );
  }
  dss->init_representative_pool();
  const Index N = dss->get_nbScenarios();
  std::cout << "Scenarios N = " << N << ", scenario size = "
            << dss->get_scenarioSize() << "\n";

  // ---- v*: full N-scenario objective -------------------------------------
  std::cout << "\nSolving full TSS (v*)...\n";
  const double v_star = solve_tssb_file( instance_file , bsc.get() );
  std::cout << "  v* = " << std::fixed << std::setprecision(2) << v_star << "\n";

  // ---- reduction ----------------------------------------------------------
  std::cout << "\nReducing to K=" << K << " via " << method << "...\n";
  auto t0 = std::chrono::steady_clock::now();

  Reduction red;
  if( method == "cssc" )
   red = cssc_reduce( instance_file , dss.get() , K , bsc.get() );
  else {
   int algo;
   if(      method == "baseline" ) algo = 0;
   else if( method == "dupacova" ) algo = 1;
   else if( method == "bestfit"  ) algo = 2;
   else if( method == "firstfit" ) algo = 3;
   else throw std::invalid_argument( "unknown method: " + method );
   red = heuristic_reduce( dss.get() , K , algo );
  }

  const double red_ms = std::chrono::duration< double , std::milli >(
   std::chrono::steady_clock::now() - t0 ).count();

  // ---- build + solve the reduced TSSB -------------------------------------
  std::vector< std::vector< double > > sel_scen;
  sel_scen.reserve( red.selected.size() );
  for( auto idx : red.selected ) {
   std::vector< double > v( dss->get_scenarioSize() );
   for( size_t d = 0 ; d < v.size() ; ++d ) v[ d ] = dss->get_scenario_value( idx , d );
   sel_scen.push_back( std::move( v ) );
  }
  const std::string red_file = "/tmp/scenario_reduction_solve_reduced.nc4";
  write_reduced_tssb( instance_file , sel_scen , red.weights , red_file );
  const double reduced_obj = solve_tssb_file( red_file , bsc.get() );
  std::remove( red_file.c_str() );

  const double gap = std::abs( reduced_obj - v_star ) /
   ( std::abs( v_star ) > 1e-12 ? std::abs( v_star ) : 1.0 );

  std::cout << "\n=== Results ===\n";
  std::cout << "Selected scenarios:";
  for( auto idx : red.selected ) std::cout << " " << idx;
  std::cout << "\n";
  std::cout << "Full  (N=" << N << "): " << std::scientific << std::setprecision(3)
            << v_star << "\n";
  std::cout << "Reduced (K=" << K << "): " << reduced_obj
            << "  (" << std::fixed << std::setprecision(1) << red_ms << " ms)\n";
  std::cout << "Gap: " << std::fixed << std::setprecision(4) << gap * 100.0
            << "%\n";

  return 0;
 }
 catch( const std::exception & e ) {
  std::cerr << "Error: " << e.what() << "\n";
  return 1;
 }
}

/*--------------------------------------------------------------------------*/
/*----------------- End File ScenarioReductionSolve.cpp ---------------------*/
/*--------------------------------------------------------------------------*/
