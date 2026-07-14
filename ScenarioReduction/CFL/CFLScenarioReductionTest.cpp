/*--------------------------------------------------------------------------*/
/*---------------- File CFLScenarioReductionTest.cpp -----------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Implementation of CFL-specific scenario reduction test.
 *
 * \author Benoît Tran \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \author Minh Duc Pham \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Benoît Tran
 */
/*--------------------------------------------------------------------------*/  
#include "CapacitatedFacilityLocationBlock.h"
#include "CSSCScenarioReductionSolver.h"
#include "ScenarioReductionSolver.h"
#include "DiscreteScenarioSet.h"
#include "ScenarioReductionBlock.h"
#include "TwoStageStochasticBlock.h"
#include "BlockSolverConfig.h"
#include "StochasticBlock.h"
#include "DataMapping.h"

#include <chrono>
#include <iostream>
#include <string>
#include <cstdio>

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/// expose TwoStageStochasticBlock::get_stochastic_block(), which became
/// protected in the SMS++ core header (off-limits to edit); the CSSC branch
/// only needs read access to the StochasticBlock applicator
struct TSSBExposer : public TwoStageStochasticBlock {
 StochasticBlock * expose_stochastic_block() const {
  return get_stochastic_block();
 }
};

/*--------------------------------------------------------------------------*/

static void print_usage( const char * prog ) {
 std::cout
  << "Usage: " << prog << " [options]\n"
  << "  -i <file>   CFL instance netCDF file (required)\n"
  << "  -f <file>   Scenario netCDF file (required)\n"
  << "  -r <int>    Number of representative scenarios (default: 5)\n"
  << "  -n <int>    Limit total scenarios to N (default: all)\n"
  << "  -c <file>   Solver config file (default: BSPar_CPLEX.txt)\n"
  << "  -m <method> dupacova|bestfit|firstfit|baseline|cssc (default: dupacova)\n"
  << "  -v <int>    Verbosity (default: 0)\n"
  << std::endl;
}

/*--------------------------------------------------------------------------*/
/// Build TSSB from base_cfl + dss via netCDF serialization

static std::unique_ptr< TwoStageStochasticBlock >
build_tssb( CapacitatedFacilityLocationBlock * cfl ,
            DiscreteScenarioSet * dss ,
            const std::string & tmp = "/tmp/tssb_tmp.nc4" )
{
 const int N  = static_cast< int >( dss->get_poolSize() );
 const int nc = static_cast< int >( cfl->get_NCustomers() );
 const int nf = static_cast< int >( cfl->get_NFacilities() );

 {
  netCDF::NcFile f( tmp , netCDF::NcFile::replace );
  auto g = f.addGroup( "TwoStageStochasticBlock" );
  g.putAtt( "type" , "TwoStageStochasticBlock" );
  g.addDim( "NumberScenarios" , N );

  // StaticAbstractPath: y[] are the here-and-now (first-stage) variables.
  //
  // One path with a single 'V' node selecting the whole y[] group:
  //   group 0 (= "y", the first static variable group of the CFLB),
  //   element 0 .. range nf.
  // AbstractPath::get_number_elements computes the element count as
  // (range - element), so range must be nf (an *exclusive* end). Encoding
  // one path per facility with PathRangeIndices[i] == PathElementIndices[i]
  // gives (i - i) == 0 elements per path and resolves to zero first-stage
  // variables, so generate_abstract_constraints builds no non-anticipativity
  // constraints and the extensive form silently degenerates into a
  // wait-and-see (anticipative) model
  {
   auto pg = g.addGroup( "StaticAbstractPath" );
   auto pdim  = pg.addDim( "PathDim" , 1 );
   auto tldim = pg.addDim( "PathTotalLength" , 1 );
   unsigned int u0 = 0 , unf = static_cast< unsigned int >( nf );
   char vtype = 'V';
   pg.addVar( "PathStart"          , netCDF::NcUint() , pdim  ).putVar( &u0 );
   pg.addVar( "PathNodeTypes"      , netCDF::NcChar() , tldim ).putVar( &vtype );
   pg.addVar( "PathGroupIndices"   , netCDF::NcUint() , tldim ).putVar( &u0 );
   pg.addVar( "PathElementIndices" , netCDF::NcUint() , tldim ).putVar( &u0 );
   pg.addVar( "PathRangeIndices"   , netCDF::NcUint() , tldim ).putVar( &unf );
  }

  // StochasticBlock: inner CFLB + DataMapping for customer demands
  {
   auto sg = g.addGroup( "StochasticBlock" );
   sg.putAtt( "type" , "StochasticBlock" );

   auto bg = sg.addGroup( "Block" );
   cfl->serialize( bg );

   auto ndm = sg.addDim( "NumberDataMappings" , 1 );
   char dt = 'D'; sg.addVar( "DataType"   , netCDF::NcChar() , ndm ).putVar( &dt );
   char cl = 'B'; sg.addVar( "Caller"     , netCDF::NcChar() , ndm ).putVar( &cl );

   std::string fn = "CapacitatedFacilityLocationBlock::chg_customer_demands";
   sg.addVar( "FunctionName" , netCDF::NcString() , ndm ).putVar( {0} , &fn );

   auto ssd = sg.addDim( "SetSizeDim" , 2 );
   std::vector< unsigned int > ss = { 0 , 0 };
   sg.addVar( "SetSize" , netCDF::NcUint() , ssd ).putVar( ss.data() );

   unsigned char ord = 0;
   sg.addVar( "Ordered" , netCDF::NcUbyte() , ndm ).putVar( &ord );

   auto sed = sg.addDim( "SetElementsDim" , 4 );
   std::vector< unsigned int > se = { 0 , (unsigned int)nc , 0 , (unsigned int)nc };
   sg.addVar( "SetElements" , netCDF::NcUint() , sed ).putVar( se.data() );

   // AbstractPath (empty = Block itself)
   auto apg = sg.addGroup( "AbstractPath" );
   auto apdim  = apg.addDim( "PathDim" , 1 );
   auto aptldim = apg.addDim( "PathTotalLength" , 0 );
   unsigned int ps = 0;
   apg.addVar( "PathStart"         , netCDF::NcUint() , apdim   ).putVar( &ps );
   apg.addVar( "PathNodeTypes"     , netCDF::NcChar() , aptldim );
   apg.addVar( "PathGroupIndices"  , netCDF::NcUint() , aptldim );
   apg.addVar( "PathElementIndices", netCDF::NcUint() , aptldim );
   apg.addVar( "PathRangeIndices"  , netCDF::NcUint() , aptldim );
  }

  // DiscreteScenarioSet
  {
   auto dg = g.addGroup( "DiscreteScenarioSet" );
   dss->serialize( dg );
  }
 }

 auto tssb = std::make_unique< TwoStageStochasticBlock >();
 {
  netCDF::NcFile f( tmp , netCDF::NcFile::read );
  tssb->deserialize( f.getGroup( "TwoStageStochasticBlock" ) );
 }
 std::remove( tmp.c_str() );
 return tssb;
}

/*--------------------------------------------------------------------------*/

static double solve_tssb( TwoStageStochasticBlock * tssb ,
                           BlockSolverConfig * bsc )
{
 tssb->generate_abstract_variables();
 tssb->generate_abstract_constraints();
 tssb->generate_objective();

 auto cfg = std::unique_ptr< BlockSolverConfig >(
  static_cast< BlockSolverConfig * >( bsc->clone() ) );
 cfg->apply( tssb );

 auto * solver = tssb->get_registered_solvers().front();
 if( solver->compute() != Solver::kOK )
  throw std::runtime_error( "solve_tssb: solver failed" );

 double obj = solver->get_ub();
 cfg->clear();
 return obj;
}

/*--------------------------------------------------------------------------*/

int main( int argc , char * argv[] )
{
 std::string instance_file , scenario_file , solver_file = "BSPar_CPLEX.txt";
 std::string method = "dupacova";
 int K = 5 , verbosity = 0 , N_limit = 0;

 for( int i = 1 ; i < argc ; ++i ) {
  std::string a( argv[i] );
  if(      a == "-i" && i+1 < argc ) instance_file = argv[++i];
  else if( a == "-f" && i+1 < argc ) scenario_file = argv[++i];
  else if( a == "-r" && i+1 < argc ) K             = std::stoi( argv[++i] );
  else if( a == "-n" && i+1 < argc ) N_limit       = std::stoi( argv[++i] );
  else if( a == "-c" && i+1 < argc ) solver_file   = argv[++i];
  else if( a == "-m" && i+1 < argc ) method        = argv[++i];
  else if( a == "-v" && i+1 < argc ) verbosity     = std::stoi( argv[++i] );
  else if( a == "-h" ) { print_usage( argv[0] ); return 0; }
 }

 if( instance_file.empty() || scenario_file.empty() ) {
  std::cerr << "Error: -i and -f are required\n";
  print_usage( argv[0] ); return 1;
 }

 std::cout << "=== CFL Scenario Reduction Test ===" << std::endl;
 std::cout << "Instance:  " << instance_file << std::endl;
 std::cout << "Scenarios: " << scenario_file << std::endl;
 std::cout << "Method:    " << method        << std::endl;
 std::cout << "K:         " << K             << std::endl;

 try {

  // 1. Load CFL instance
  std::cout << "\n[1] Loading CFL instance..." << std::endl;
  auto * cfl_ptr = dynamic_cast< CapacitatedFacilityLocationBlock * >(
   Block::deserialize( instance_file ) );
  if( ! cfl_ptr ) throw std::runtime_error( "Failed to load CFLB" );
  std::unique_ptr< CapacitatedFacilityLocationBlock > cfl( cfl_ptr );
  cfl->chg_UnSplittable( true );
  std::cout << "    Facilities: " << cfl->get_NFacilities()
            << "  Customers: "   << cfl->get_NCustomers() << std::endl;

  // 2. Load scenarios
  std::cout << "\n[2] Loading scenarios..." << std::endl;
  auto dss = std::make_unique< DiscreteScenarioSet >();
  {
   netCDF::NcFile f( scenario_file , netCDF::NcFile::read );
   dss->deserialize( f );
  }
  const int N_all = static_cast< int >( dss->get_nbScenarios() );
  const int N = ( N_limit > 0 && N_limit < N_all ) ? N_limit : N_all;
  std::cout << "    N = " << N << " scenarios" << std::endl;
  dss->init_representative_pool( static_cast< DiscreteScenarioSet::ScenarioIndex >( N ) );

  // Load solver config
  auto bsc = std::unique_ptr< BlockSolverConfig >(
   static_cast< BlockSolverConfig * >(
    Configuration::deserialize( solver_file ) ) );
  if( ! bsc ) throw std::runtime_error( "Failed to load solver config" );

  // 3. Solve full TSS
  std::cout << "\n[3] Solving full TSS (N=" << N << ")..." << std::endl;
  auto tssb_full = build_tssb( cfl.get() , dss.get() , "/tmp/tssb_full.nc4" );
  auto t0 = std::chrono::steady_clock::now();
  double full_obj = solve_tssb( tssb_full.get() , bsc.get() );
  double full_time = std::chrono::duration< double >(
   std::chrono::steady_clock::now() - t0 ).count();
  std::cout << "    Objective: " << full_obj
            << "  Time: " << full_time << "s" << std::endl;

  // 4. Scenario reduction
  std::cout << "\n[4] Scenario reduction (" << method
            << ", K=" << K << ")..." << std::endl;
  auto t2 = std::chrono::steady_clock::now();

  // ScenarioReductionBlock: holds scenario generator + solution.
  // CSSCScenarioReductionSolver reads N/weights/distances directly
  // from the DiscreteScenarioSet, no synthetic CFLB needed
  auto srb = std::make_unique< ScenarioReductionBlock >();
  srb->set_scenario_generator( dss.get() );
  // Both CSSC and the heuristics (ScenarioReductionSolver) read the scenario
  // data directly from the DiscreteScenarioSet, no synthetic CFLB needed

  if( method == "cssc" ) {
   auto tssb_cssc = build_tssb( cfl.get() , dss.get() , "/tmp/tssb_cssc.nc4" );

   auto * stoch_app =
    static_cast< TSSBExposer * >( tssb_cssc.get() )->expose_stochastic_block();
   if( ! stoch_app )
    throw std::runtime_error(
     "CSSC: TwoStageStochasticBlock has no StochasticBlock applicator." );

   struct StochAppGuard {
    StochasticBlock * app;
    ~StochAppGuard() { if( app ) app->set_inner_block( nullptr , false ); }
   } guard{ stoch_app };

   srb->set_stochastic_block( tssb_cssc.get() );
   srb->set_scenario_applicator( stoch_app );

   auto cssc = std::make_unique< CSSCScenarioReductionSolver >();
   cssc->set_milp_config( static_cast< BlockSolverConfig * >( bsc->clone() ) );
   cssc->set_nb_reduced( K );
   cssc->set_Block( srb.get() );

   // No set_var_extractor call: the solver's generic AbstractPath-based
   // fallback reads the here-and-now y[] variables directly from the TSSB,
   // verified to match the old hand-rolled extractor exactly.

   // CFL-specific DataInjector: set_data() alone does not notify the registered
   // solver because the DataMapping uses eNoMod flags.  This explicit call sends
   // eModBlck so CPLEX/HiGHS sees the updated demands before computing
   cssc->set_data_injector( []( Block * inner,
                                const std::vector< double > & data ) {
    auto * cflb = dynamic_cast< CapacitatedFacilityLocationBlock * >( inner );
    if( ! cflb ) return;
    using CIdx = CapacitatedFacilityLocationBlock::Index;
    cflb->chg_customer_demands( data.begin() ,
     Block::Range( CIdx(0) , CIdx( data.size() ) ) , eNoMod , eModBlck );
   } );

   cssc->compute();
   cssc->get_var_solution();
  }
  else {
   // Generic distribution-driven heuristics: read the DiscreteScenarioSet
   // directly from the ScenarioReductionBlock (no synthetic CFLB needed)
   auto heur = std::make_unique< ScenarioReductionSolver >();
   heur->set_nb_reduced( K );
   if(      method == "baseline" ) heur->set_algorithm( 0 );
   else if( method == "dupacova" ) heur->set_algorithm( 1 );
   else if( method == "bestfit" )  heur->set_algorithm( 2 );
   else if( method == "firstfit" ) heur->set_algorithm( 3 );
   else throw std::invalid_argument( "Unknown method: " + method );
   heur->set_Block( srb.get() );
   heur->compute();
   heur->get_var_solution();
  }

  // Reduction-algorithm cost: time from the start of step 4 until the
  // reduction is done (for CSSC this includes building the N x N V matrix and
  // solving the partitioning MILP; for the heuristics it is just the distance
  // computation + selection).  This is the genuine "computational cost" of the
  // reduction method, distinct from the time to solve the reduced TSS in step 5
  double algo_time = std::chrono::duration< double >(
   std::chrono::steady_clock::now() - t2 ).count();
  std::cout << "    Reduction time: " << algo_time << "s" << std::endl;

  // 5. Solve reduced TSS
  // Build a new DSS containing only the K selected scenarios with
  // aggregated weights. build_tssb uses get_nbScenarios() so we must
  // pass a DSS that has exactly K scenarios, not the full N
  std::cout << "\n[5] Solving reduced TSS (K=" << K << ")..." << std::endl;
  {
   // Read solution directly from ScenarioReductionBlock
   const auto & sol = srb->get_solution();
   if( ! sol.is_set() )
    throw std::runtime_error( "Solver did not write solution into SRB" );
   const auto & sel = sol.selected_indices;
   const auto & wts = sol.weights;
   const auto sz    = dss->get_scenarioSize();
   const int  Ksel  = static_cast< int >( sel.size() );

   std::cout << "    DEBUG: Ksel=" << Ksel << " sel=[" ;
   for( int p = 0 ; p < std::min(Ksel,10) ; ++p )
    std::cout << sel[p] << " ";
   std::cout << "]" << std::endl;

   std::vector< std::vector< double > > red_sc;
   std::vector< double >                red_wt;
   red_sc.reserve( Ksel );
   red_wt.reserve( Ksel );
   for( int p = 0 ; p < Ksel ; ++p ) {
    std::vector< double > sc( sz );
    for( DiscreteScenarioSet::ScenarioSize d = 0 ; d < sz ; ++d )
     sc[ d ] = dss->get_scenario_value( sel[p] , d );
    red_sc.push_back( sc );
    red_wt.push_back( wts[p] );
   }
   // Normalise weights
   double wsum = 0.0;
   for( double w : red_wt ) wsum += w;
   if( wsum > 0.0 ) for( double & w : red_wt ) w /= wsum;

   auto dss_red = std::make_unique< DiscreteScenarioSet >();
   dss_red->load_from_memory( red_sc , red_wt );
   dss_red->init_representative_pool(
    static_cast< DiscreteScenarioSet::ScenarioIndex >( Ksel ) );

   std::cout << "    DEBUG: dss_red has "
             << dss_red->get_nbScenarios() << " scenarios, "
             << "poolSize=" << dss_red->get_poolSize() << std::endl;

   auto tssb_red = build_tssb( cfl.get() , dss_red.get() , "/tmp/tssb_red.nc4" );
   auto t4 = std::chrono::steady_clock::now();
   double red_obj = solve_tssb( tssb_red.get() , bsc.get() );
   double red_time = std::chrono::duration< double >(
    std::chrono::steady_clock::now() - t4 ).count();
   std::cout << "    Objective: " << red_obj
             << "  Time: " << red_time << "s" << std::endl;

   // 6. Results
   // Reset stream formatting: the CSSC solver leaves std::cout in std::fixed
   // mode (it prints Wasserstein/discrepancy with std::fixed and never restores
   // the flags), which would otherwise make the cssc rows print with different
   // decimals than the heuristic rows even though the values are identical

   std::cout << std::defaultfloat;
   std::cout << "\n=== Results ===" << std::endl;
   std::cout << "Full TSS  (N=" << N << "): " << full_obj
             << "  (" << full_time << "s)" << std::endl;
   std::cout << "Reduced TSS (K=" << K << "): " << red_obj
             << "  (" << red_time << "s)" << std::endl;
   double err     = std::abs( red_obj - full_obj );
   double gap_pct = 100.0 * err / std::abs( full_obj );
   std::cout << "Gap (absolute): " << err
             << "  (" << gap_pct << "%)" << std::endl;
  }  // end reduced TSS scope
  return 0;

 } catch( const std::exception & e ) {
  std::cerr << "Error: " << e.what() << std::endl;
  return 1;
 }
 return 0;
}
