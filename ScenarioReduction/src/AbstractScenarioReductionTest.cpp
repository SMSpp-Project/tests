/*--------------------------------------------------------------------------*/
/*---------------- File AbstractScenarioReductionTest.cpp -----------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Implementation of the AbstractScenarioReductionTest class.
 *
 * \author Benoît Tran \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Benoît Tran
 */
/*--------------------------------------------------------------------------*/

#include "AbstractScenarioReductionTest.h"

#include <cmath>
#include <filesystem>
#include <iomanip>

#include "Configuration.h"
#include "Solver.h"

using namespace std;
using namespace SMSpp_di_unipi_it;
using namespace ScenarioReductionTesting;

/*--------------------------------------------------------------------------*/
/*-------------------------- PUBLIC METHODS --------------------------------*/
/*--------------------------------------------------------------------------*/

AbstractScenarioReductionTest::AbstractScenarioReductionTest( ) {
 // Initialize ComputeConfig with default values
 config = new ComputeConfig( );

 // Standard SMS++ parameters
 config->set_par(
   std::string( "intLogVerb" ) ,
   0 );  // Verbosity level (standard SMS++)
 config->set_par(
   std::string( "dblMaxTime" ) ,
   -1.0 );  // Time limit (standard SMS++)

 // Test-specific integer parameters
 config->set_par(
   std::string( "intFullNumScenarios" ) ,
   0 );  // 0 = use all from file
 config->set_par( std::string( "intReducedNumScenarios" ) , 3 );
 config->set_par(
   std::string( "intUseWarmstart" ) ,
   0 );  // bool as int: 0=false, 1=true
 config->set_par( std::string( "intUseShuffle" ) , 0 );  // bool as int
 config->set_par( std::string( "intSaveResults" ) , 0 ); // bool as int
 config->set_par( std::string( "intLoadResults" ) , 0 ); // bool as int
 config->set_par( std::string( "intComputeVPI" ) , 0 );  // bool as int

 // Test-specific string parameters
 config->set_par( std::string( "strReductionMethod" ) , std::string(
  "dupacova" ));
 config->set_par( std::string( "strInstancePath" ) , std::string( "" ));
 config->set_par( std::string( "strScenarioFile" ) , std::string( "" ));
 config->set_par( std::string( "strCacheDir" ) , std::string( "./cache/" ));
 config->set_par(
   std::string( "strSolverConfig" ) ,
   std::string( "BSPar_HiGHS.txt" ));
}

/*--------------------------------------------------------------------------*/

AbstractScenarioReductionTest::~AbstractScenarioReductionTest( ) {
 delete config;
 config = nullptr;
}

/*--------------------------------------------------------------------------*/
// Helper methods to access ComputeConfig parameters

int AbstractScenarioReductionTest::get_int_config(
  const std::string & name ) const {
 for(const auto & p : config->int_pars) {
  if( p.first == name ) return p.second;
 }
 return 0; // default
}

double AbstractScenarioReductionTest::get_dbl_config(
  const std::string & name ) const {
 for(const auto & p : config->dbl_pars) {
  if( p.first == name ) return p.second;
 }
 return 0.0; // default
}

std::string AbstractScenarioReductionTest::get_str_config(
  const std::string & name ) const {
 for(const auto & p : config->str_pars) {
  if( p.first == name ) return p.second;
 }
 return ""; // default
}

// Removed - no longer need these wrapper functions

/*--------------------------------------------------------------------------*/

int AbstractScenarioReductionTest::run( int argc , char * argv[] ) {
 try {
  // Command-line arguments and saving the test's config
  parse_arguments( argc , argv );
  print_configuration( );

  // Load the base problem instance and scenarios
  load( );

  // Solve stochastic problem with the full set of scenarios
  solve_stochastic_problem( );

  // Optionally - compute anticipative solution with full scenarios
  solve_anticipative( );

  // Do scenario reduction
  solve_scenario_reduction( );

  // Solve stochastic problem with the reduced set of scenarios
  solve_stochastic_problem( );

  // Optionally - compute anticipative solution with reduced scenarios
  solve_anticipative( );

  // Optionally - save results to cache folder
  save_solutions_cache( );

  // Optionally - Print final results
  print_results( );

  return 0;

 } catch( const exception & e ) {
  cerr << "Error: " << e.what( ) << endl;
  return 1;
 }
}

/*--------------------------------------------------------------------------*/
/*------------------------ CONFIGURATION METHODS --------------------------*/
/*--------------------------------------------------------------------------*/

void AbstractScenarioReductionTest::parse_arguments( int argc , char * argv[] )
{
 for(int i = 1; i < argc; ++i) {
  string arg = argv[ i ];

  // Help options
  if( arg == "-h" || arg == "--help" ) {
   print_help( argv[ 0 ] );
   exit( 0 );
  }
  // Instance path option
  else if( arg == "-i" || arg == "--instance" ) {
   if( i + 1 < argc ) {
    config->set_par( std::string("strInstancePath") , std::string(argv[ ++i ]) );
   }
   else{
    cerr << "Missing value after " << arg << endl;
    exit( 1 );
   }
  }
  // Scenario file option (for pre-generated scenarios)
  else if( arg == "-f" || arg == "--scenario-file" ) {
   if( i + 1 < argc ) {
    config->set_par( std::string("strScenarioFile") , std::string(argv[ ++i ]) );
   }
   else{
    cerr << "Missing value after " << arg << endl;
    exit( 1 );
   }
  }
  // Verbose options
  else if( arg == "-v" || arg == "--verbose" ) {
   if( i + 1 < argc ) {
    try {
     int verbose = stoi( argv[ ++i ] );
     if( verbose < 0 || verbose > 2 ) {
      cerr << "Verbose level must be 0, 1, or 2" << endl;
      exit( 1 );
     }
     config->set_par( std::string("intLogVerb") , verbose );
    } catch( const exception & e ) {
     cerr << "Invalid verbose level: " << argv[ i ] << endl;
     exit( 1 );
    }
   }
   else{
    cerr << "Missing value after " << arg << endl;
    exit( 1 );
   }
  }
  // Time limit option
  else if( arg == "-t" || arg == "--time" ) {
   if( i + 1 < argc ) {
    try {
     double time_limit = stod( argv[ ++i ] );
     if( time_limit <= 0 ) {
      cerr << "Time limit must be positive" << endl;
      exit( 1 );
     }
     config->set_par( std::string("dblMaxTime") , time_limit );
    } catch( const exception & e ) {
     cerr << "Invalid time limit: " << argv[ i ] << endl;
     exit( 1 );
    }
   }
   else{
    cerr << "Missing value after " << arg << endl;
    exit( 1 );
   }
  }
  // Number of full scenarios option
  else if( arg == "-n" || arg == "--full-scenarios" ) {
   if( i + 1 < argc ) {
    try {
     int full_num = stoi( argv[ ++i ] );
     if( full_num <= 0 ) {
      cerr << "Number of full scenarios must be positive" << endl;
      exit( 1 );
     }
     config->set_par( std::string("intFullNumScenarios") , full_num );
    } catch( const exception & e ) {
     cerr << "Invalid number of full scenarios: " << argv[ i ] << endl;
     exit( 1 );
    }
   }
   else{
    cerr << "Missing value after " << arg << endl;
    exit( 1 );
   }
  }
  // Number of reduced scenarios options
  else if( arg == "-r" || arg == "--reduced" ) {
   if( i + 1 < argc ) {
    try {
     int reduced_num = stoi( argv[ ++i ] );
     if( reduced_num <= 0 ) {
      cerr << "Number of reduced scenarios must be positive" << endl;
      exit( 1 );
     }
     config->set_par( std::string("intReducedNumScenarios") , reduced_num );
    } catch( const exception & e ) {
     cerr << "Invalid number of reduced scenarios: " << argv[ i ] << endl;
     exit( 1 );
    }
   }
   else{
    cerr << "Missing value after " << arg << endl;
    exit( 1 );
   }
  }
  // Reduction method option
  else if( arg == "-m" || arg == "--method" ) {
   if( i + 1 < argc ) {
    string method = argv[ ++i ];
    // Validate method
    if( method != "baseline" && method != "dupacova" && method != "bestfit" &&
      method != "firstfit" && method != "milp" ) {
     cerr << "Unknown reduction method: " << method << endl;
     cerr << "Valid methods: baseline, dupacova, bestfit, "
      "firstfit, milp"
          << endl;
     exit( 1 );
    }
    config->set_par( std::string("strReductionMethod") , std::move(method) );
   }
   else{
    cerr << "Missing value after " << arg << endl;
    exit( 1 );
   }
  }
  // Warmstart option
  else if( arg == "-w" || arg == "--warmstart" ) {
   if( i + 1 < argc ) {
    string val = argv[ ++i ];
    config->set_par( std::string("intUseWarmstart") , (val == "1" || val == "true") ? 1 : 0 );
   }
   else{
    cerr << "Missing value after " << arg << endl;
    exit( 1 );
   }
  }
  // Shuffle option
  else if( arg == "-S" || arg == "--shuffle" ) {
   if( i + 1 < argc ) {
    string val = argv[ ++i ];
    config->set_par( std::string("intUseShuffle") , (val == "1" || val == "true") ? 1 : 0 );
   }
   else{
    cerr << "Missing value after " << arg << endl;
    exit( 1 );
   }
  }
  // Cache directory option
  else if( arg == "-d" || arg == "--cache-dir" ) {
   if( i + 1 < argc ) {
    string cache_dir = argv[ ++i ];
    // Ensure cache_dir ends with /
    if( ! cache_dir.empty( ) && cache_dir.back( ) != '/' ) { cache_dir += '/'; }
    config->set_par( std::string("strCacheDir") , std::move(cache_dir) );
   }
   else{
    cerr << "Missing value after " << arg << endl;
    exit( 1 );
   }
  }
  // Save results option
  else if( arg == "-s" || arg == "--save-results" ) {
   config->set_par( std::string("intSaveResults") , 1 );
  }
  // Load results option
  else if( arg == "-L" || arg == "--load-results" ) {
   config->set_par( std::string("intLoadResults") , 1 );
  }
  // VPI option
  else if( arg == "-p" || arg == "--vpi" ) {
   config->set_par( std::string("intComputeVPI") , 1 );
  }
 }

 // Instance path is needed
 if( get_str_config( "strInstancePath" ).empty( )) {
  cerr << "Error: Instance path is required (-i or --instance)" << endl;
  print_help( argv[ 0 ] );
  exit( 1 );
 }
}

/*--------------------------------------------------------------------------*/

void AbstractScenarioReductionTest::print_help( const char * program_name ) {
 cout << "Usage: " << program_name << " [options]" << endl;
 cout << "\nGeneral Options:" << endl;
 cout << "  -i, --instance <path>     Path to problem instance file "
  "(required)"
      << endl;
 cout << "  -f, --scenario-file <path> Load pre-generated scenarios from file"
      << endl;
 cout << "  -v, --verbose <level>     Set verbosity level (0=silent, "
  "1=normal, 2=detailed)"
      << endl;
 cout << "  -t, --time <seconds>      Set solver time limit in seconds" << endl;
 cout << "  -h, --help                Show this help message" << endl;
 cout << "\nScenario Options:" << endl;
 cout << "  -n, --full-scenarios <N>  Set number of full scenarios to use"
      << endl;
 cout << "                            (Default: use all from loaded file)"
      << endl;
 cout << "  -r, --reduced <number>    Set number of reduced scenarios "
  "(default: 3)"
      << endl;
 cout << "\nReduction Method Options:" << endl;
 cout << "  -m, --method <name>       Scenario reduction method:" << endl;
 cout << "                            baseline, dupacova (default), "
  "bestfit, firstfit, milp"
      << endl;
 cout << "  -w, --warmstart <0|1>     Use warm start for local search "
  "(0=false, 1=true)"
      << endl;
 cout << "  -S, --shuffle <0|1>       Enable shuffling for FirstFit "
  "(0=false, 1=true)"
      << endl;
 cout << "\nResults Cache Options:" << endl;
 cout << "  -s, --save-results        Save solution results to cache files"
      << endl;
 cout << "  -L, --load-results        Load pre-computed results from cache"
      << endl;
 cout << "  -d, --cache-dir <path>    Specify cache directory (default: "
  "./cache/)"
      << endl;
 cout << "\nAnalysis Options:" << endl;
 cout << "  -p, --vpi                 Compute Value of Perfect Information"
      << endl;
}

/*--------------------------------------------------------------------------*/

void AbstractScenarioReductionTest::print_configuration( ) {
 if( get_int_config( "intLogVerb" ) >= 1 ) {
  cout << "\nTest Configuration" << endl;
  cout << "  Instance.........: " << get_str_config( "strInstancePath" ) << endl
  ;
  if( ! get_str_config( "strScenarioFile" ).empty( )) {
   cout << "  Scenario file...: " << get_str_config( "strScenarioFile" ) << endl
   ;
  }
  if( get_int_config( "intFullNumScenarios" ) > 0 ) {
   cout << "  Full scenarios..: " << get_int_config( "intFullNumScenarios" )
        << endl;
  }
  cout << "  Reduced scenarios: " << get_int_config( "intReducedNumScenarios" )
       << endl;
  cout << "  Reduction method.: " << get_str_config( "strReductionMethod" )
       << endl;
  cout << "  Verbose level....: " << get_int_config( "intLogVerb" ) << endl;
  if( get_dbl_config( "dblMaxTime" ) > 0 ) {
   cout << "  Time limit......: " << get_dbl_config( "dblMaxTime" ) <<
     " seconds"
        << endl;
  }
  if( get_int_config( "intSaveResults" ) || get_int_config( "intLoadResults" ))
  {
   cout << "  Cache directory.: " << get_str_config( "strCacheDir" ) << endl;
   if( get_int_config( "intSaveResults" ))
    cout << "  Saving results.: enabled" << endl;
   if( get_int_config( "intLoadResults" ))
    cout << "  Loading results from cache: enabled" << endl;
  }
  if( get_int_config( "intComputeVPI" )) {
   cout << "  Compute VPI....: enabled" << endl;
  }
 }

 if( get_int_config( "intLogVerb" ) >= 1 ) {
  cout << "=== Scenario Reduction Test  ===" << endl;
 }
}

/*--------------------------------------------------------------------------*/
/*-------------------------- WORKFLOW METHODS ------------------------------*/
/*--------------------------------------------------------------------------*/

void AbstractScenarioReductionTest::load( ) {
 if( get_int_config( "intLogVerb" ) >= 1 )
  cout << "\nLoading base instance" << endl;

 // Call problem-specific loading
 load_problem_instance( get_str_config( "strInstancePath" ));

 // Validate that the instance was loaded correctly
 if( ! base_block ) {
  throw runtime_error(
   "Failed to load base block from " + get_str_config( "strInstancePath" ));
 }

 // Validate that the stochastic block was created
 if( ! stochastic_block ) {
  throw runtime_error(
   "Failed to create stochastic block for " +
   get_str_config( "strInstancePath" ));
 }

 // Load scenarios from the scenario file
 if( get_int_config( "intLogVerb" ) >= 1 ) cout << "\nLoading scenarios" << endl
  ;

 // Determine which scenario file to use
 string scenario_file;
 if( ! get_str_config( "strScenarioFile" ).empty( )) {
  // User-specified scenario file
  scenario_file = get_str_config( "strScenarioFile" );
  if( get_int_config( "intLogVerb" ) >= 2 ) {
   cout << "  Using user-specified scenario file: " << scenario_file << endl;
  }
 }
 else{
  // Scenario file based on instance name
  scenario_file = get_scenario_file( get_str_config( "strInstancePath" ));
  if( get_int_config( "intLogVerb" ) >= 2 ) {
   cout << "  Using scenario file: " << scenario_file << endl;
  }
 }

 // Load scenarios using DiscreteScenarioSet::deserialize
 try {
  // Open the netCDF file
  netCDF::NcFile file( scenario_file , netCDF::NcFile::read );

  // Create a DiscreteScenarioSet and deserialize from the file
  scenario_set = std::make_unique< DiscreteScenarioSet >( );
  scenario_set->deserialize( file );
  file.close( );

  // Store dimensions
  size_t total_scenarios_in_file = scenario_set->get_nbScenarios( );
  dimension_scenario = scenario_set->get_scenarioSize( );

  // Initialize the scenario pool based on user specification
  if( get_int_config( "intFullNumScenarios" ) > 0 ) {
   // User specified -n option
   if( get_int_config( "intFullNumScenarios" ) <= total_scenarios_in_file ) {
    // Valid count specified - use init_representative_pool for deterministic
    // selection
    scenario_set->init_representative_pool(
      get_int_config( "intFullNumScenarios" ));
   }
   else{
    // User requested more scenarios than available
    if( get_int_config( "intLogVerb" ) >= 1 ) {
     cout << "  Warning: Requested " << get_int_config( "intFullNumScenarios" )
          << " scenarios but file only contains " << total_scenarios_in_file
          << endl;
     cout << "  Using all " << total_scenarios_in_file << " scenarios" << endl;
    }
    config->set_par( std::string("intFullNumScenarios") , static_cast<int>(total_scenarios_in_file) );
    // Still use init_representative_pool to maintain consistency
    scenario_set->init_representative_pool(
      get_int_config( "intFullNumScenarios" ));
   }
  }
  else{
   // No -n specified, use all scenarios with init_representative_pool for
   // baseline
   config->set_par( std::string("intFullNumScenarios") , static_cast<int>(total_scenarios_in_file) );
   scenario_set->init_representative_pool(
     get_int_config( "intFullNumScenarios" ));
  }

 } catch( const netCDF::exceptions::NcException & e ) {
  throw runtime_error(
   "Failed to load scenarios from: " + scenario_file + "\n\n" +
   "Scenarios must be pre-generated before running tests.\n" +
   "  1. Generate scenarios using the problem-specific generator\n" +
   "     (e.g., CFLScenarioGenerator for CFL problems)\n" +
   "  2. Place scenarios in: " + get_scenarios_directory( ) + "\n" +
   "  3. Use naming convention: <instance_name>_scenarios.nc4\n" +
   "  4. Or specify a custom scenario file with -f option\n\n" +
   "NetCDF error: " + e.what( ));
 } catch( const std::exception & e ) {
  throw runtime_error(
   "Failed to load scenarios from: " + scenario_file + "\n\n" +
   "Scenarios must be pre-generated. Use the problem-specific "
   "generator\n" +
   "or specify a valid scenario file with the -f option.\n\n" +
   "Error: " + e.what( ));
 }

 if( get_int_config( "intLogVerb" ) >= 1 ) {
  cout << "  Loaded instance" << endl;
  cout << "    Scenario dimension: " << get_scenario_dimension( ) << endl;
 }

 // Validate scenario counts after loading
 if( get_int_config( "intReducedNumScenarios" ) >
   get_int_config( "intFullNumScenarios" )) {
  throw runtime_error(
   "Error: Number of reduced scenarios (" +
   to_string( get_int_config( "intReducedNumScenarios" )) +
   ") cannot exceed total scenarios (" +
   to_string( get_int_config( "intFullNumScenarios" )) + ")" );
 }
}

/*--------------------------------------------------------------------------*/

void AbstractScenarioReductionTest::solve_scenario_reduction( ) {
 if( get_int_config( "intLogVerb" ) >= 1 ) {
  cout << "\nPerforming scenario reduction:" << endl;
  cout << "  Reducing from " << get_int_config( "intFullNumScenarios" ) <<
    " to "
       << get_int_config( "intReducedNumScenarios" ) << " scenarios using "
       << get_str_config( "strReductionMethod" ) << " method..." << endl;
 }

 // Perform scenario reduction IN-PLACE on scenario_set
 auto reduction_start = chrono::high_resolution_clock::now( );

 if( get_str_config( "strReductionMethod" ) == "baseline" ) {
  scenario_set->init_representative_pool(
    get_int_config( "intReducedNumScenarios" ));
 }
 else if(
   get_str_config( "strReductionMethod" ) == "milp" ||
   get_str_config( "strReductionMethod" ) == "optimal" ) {
  auto cfg = Configuration::deserialize( "BSConfig_SR_HiGHS.txt" );
  auto * bsc = dynamic_cast< BlockSolverConfig * >(cfg);
  if( bsc ) {
   scenario_set->set_config( nullptr , bsc );
   scenario_set->init_representative_pool(
     get_int_config( "intReducedNumScenarios" ));
   // Don't delete bsc - set_config takes ownership
  }
  else{
   scenario_set->init_representative_pool(
     get_int_config( "intReducedNumScenarios" ));
  }
 }
 else{
  // Update configuration for other methods
  update_SR_config(
    get_str_config( "strReductionMethod" ) ,
    get_int_config( "intUseWarmstart" ) ,
    get_int_config( "intUseShuffle" ));

  auto cfg = Configuration::deserialize( "BSConfig_SR.txt" );
  auto * bsc = dynamic_cast< BlockSolverConfig * >(cfg);
  if( bsc ) {
   scenario_set->set_config( nullptr , bsc ); // Transfers ownership
   scenario_set->init_representative_pool(
     get_int_config( "intReducedNumScenarios" ));
  }
  else{
   scenario_set->init_representative_pool(
     get_int_config( "intReducedNumScenarios" ));
  }
 }

 auto reduction_end = chrono::high_resolution_clock::now( );
 reduction_metrics.reduction_time_ms =
   chrono::duration_cast< chrono::milliseconds >(
  reduction_end - reduction_start )
   .count( );

 // Get selected indices
 auto selected_indices_span = scenario_set->get_selected_scenarios( );
 reduction_metrics.selected_indices.clear( );
 for(auto idx : selected_indices_span) {
  reduction_metrics.selected_indices.push_back( static_cast< int >(idx));
 }

 // Compute metrics
 reduction_metrics.ell = scenario_set->get_ell( );

 if( get_int_config( "intLogVerb" ) >= 1 ) {
  cout << "  Scenario reduction completed in "
       << reduction_metrics.reduction_time_ms << " ms" << endl;
  cout << "  Selected " << scenario_set->get_poolSize( ) << " scenarios" << endl
  ;
 }
}

/*--------------------------------------------------------------------------*/

void AbstractScenarioReductionTest::solve_stochastic_problem( ) {
 if( get_int_config( "intLogVerb" ) >= 1 )
  cout << "\nSolving stochastic problem" << endl;

 size_t current_scenarios = scenario_set->get_poolSize( );

 // Try to load cached results if requested
 if( get_int_config( "intLoadResults" )) {
  try {
   if( current_scenarios == get_int_config( "intFullNumScenarios" )) {
    full_result =
      load_solution_cache( get_str_config( "strCacheDir" ) + "full_result.nc4" )
    ;
   }
   else{
    reduced_result = load_solution_cache(
     get_str_config( "strCacheDir" ) + "reduced_result.nc4" );
   }
   if( get_int_config( "intLogVerb" ) >= 1 ) {
    cout << "  Loaded pre-computed results from cache" << endl;
   }
   return;
  } catch( const exception & e ) {
   if( get_int_config( "intLogVerb" ) >= 1 ) {
    cout << "  Could not load cached results, computing..." << endl;
   }
  }
 }

 // Solve the problem with current scenario pool
 if( get_int_config( "intLogVerb" ) >= 1 ) {
  cout << "  Solving stochastic problem with " << current_scenarios
       << " scenarios:" << endl;
 }

 // Create two-stage stochastic problem for current scenarios
 auto tss_block = create_twostage_block( );

 // Solve and store in appropriate result
 if( current_scenarios == get_int_config( "intFullNumScenarios" )) {
  // This is the full problem
  full_result = compute_extensive_form( tss_block.get( ));
 }
 else{
  // This is the reduced problem
  reduced_result = compute_extensive_form( tss_block.get( ));
 }
}

/*--------------------------------------------------------------------------*/
void AbstractScenarioReductionTest::solve_anticipative( ) {
 if( ! get_int_config(
  "intComputeVPI" )) {       // We were not asked to do it so move on
  return;
 }

 if( get_int_config( "intLogVerb" ) >= 1 )
  cout << "\nComputing anticipative solutions:" << endl;

 // Try to load cached anticipative results if requested
 if( get_int_config( "intLoadResults" )) {
  try {
   anticipative_full = load_solution_cache(
    get_str_config( "strCacheDir" ) + "anticipative_full.nc4" );
   anticipative_reduced = load_solution_cache(
    get_str_config( "strCacheDir" ) + "anticipative_reduced.nc4" );
   if( get_int_config( "intLogVerb" ) >= 1 ) {
    cout << "  Loaded pre-computed anticipative solutions from cache" << endl;
   }
  } catch( const exception & e ) {
   if( get_int_config( "intLogVerb" ) >= 1 ) {
    cout << "  Could not load cached anticipative solutions, "
     "computing..."
         << endl;
   }
   // Fall through to compute
  }
 }

 // Ensure pool is initialized
 if( ! scenario_set->is_pool_initialized( )) {
  throw runtime_error( "solve_anticipative: pool not initialized" );
 }

 // Determine if current pool is full or reduced based on size
 size_t current_pool_size = scenario_set->get_poolSize( );
 bool is_full_pool =
   (current_pool_size == get_int_config( "intFullNumScenarios" ));
 bool is_reduced_pool =
   (current_pool_size == get_int_config( "intReducedNumScenarios" ));

 // Reset pool to beginning for clean iteration
 scenario_set->reset_pool( );

 // Compute anticipative solution for current pool and store in appropriate
 // variable
 if( is_full_pool ) {
  // Computing for full scenario set
  if( ! anticipative_full.solved ) {
   if( get_int_config( "intLogVerb" ) >= 1 ) {
    cout << "  Computing anticipative solution for full scenario set ("
         << current_pool_size << " scenarios)..." << endl;
   }
   anticipative_full = solve_anticipative_solution( );
  }
 }
 else if( is_reduced_pool ) {
  // Computing for reduced scenario set
  if( ! anticipative_reduced.solved ) {
   if( get_int_config( "intLogVerb" ) >= 1 ) {
    cout << "  Computing anticipative solution for reduced scenario set ("
         << current_pool_size << " scenarios)..." << endl;
   }
   anticipative_reduced = solve_anticipative_solution( );
  }
 }
 else{
  // Pool size doesn't match expected - this might be intentional in some tests
  if( get_int_config( "intLogVerb" ) >= 1 ) {
   cout << "  Warning: Current pool size (" << current_pool_size
        << ") doesn't match full (" << get_int_config( "intFullNumScenarios" )
        << ") or reduced (" << get_int_config( "intReducedNumScenarios" )
        << ") - skipping anticipative computation" << endl;
  }
  return;
 }

 // Report the solution that was just computed
 if( get_int_config( "intLogVerb" ) >= 1 ) {
  if( is_full_pool && anticipative_full.solved ) {
   cout << "  Full anticipative objective: " << fixed << setprecision( 2 )
        << anticipative_full.objective << endl;
  }
  else if( is_reduced_pool && anticipative_reduced.solved ) {
   cout << "  Reduced anticipative objective: " << fixed << setprecision( 2 )
        << anticipative_reduced.objective << endl;
  }
 }
}

/*--------------------------------------------------------------------------*/

void AbstractScenarioReductionTest::print_results( ) {
 if( get_int_config( "intLogVerb" ) < 1 ) { // We were not asked to do it
  return;
 }
 cout << "\n========== RESULTS ==========" << endl;

 // Problem info
 cout << "Instance..........: " << get_str_config( "strInstancePath" ) << endl;
 cout << "Scenario dimension: " << get_scenario_dimension( ) << endl;

 // Scenario info
 cout << "\nScenarios" << endl;
 cout << "  Full............: " << get_int_config( "intFullNumScenarios" )
      << endl;
 cout << "  Reduced.........: " << get_int_config( "intReducedNumScenarios" )
      << endl;
 cout << "  Reduction method: " << get_str_config( "strReductionMethod" ) <<
   endl;

 // Timing
 cout << "\nTiming" << endl;
 cout << "  Reduction time.: " << reduction_metrics.reduction_time_ms << " ms"
      << endl;
 if( full_result.solved ) {
  cout << "  Full solve time: " << full_result.time_ms << " ms" << endl;
 }
 if( reduced_result.solved ) {
  cout << "  Reduced solve t: " << reduced_result.time_ms << " ms" << endl;
 }

 // Objectives
 cout << "\nObjectives" << endl;
 if( full_result.solved ) {
  cout << "  Full...: " << fixed << setprecision( 2 ) << full_result.objective
       << endl;
 }
 else{
  cout << "  Full...: NOT SOLVED" << endl;
 }

 if( reduced_result.solved ) {
  cout << "  Reduced: " << fixed << setprecision( 2 ) << reduced_result.
    objective
       << endl;
 }
 else{
  cout << "  Reduced: NOT SOLVED" << endl;
 }

 if( full_result.solved && reduced_result.solved ) {
  double obj_diff = abs( reduced_result.objective - full_result.objective );
  double rel_diff = obj_diff / abs( full_result.objective ) * 100.0;
  cout << "  Diff...: " << obj_diff << " (" << rel_diff << "%)" << endl;
 }

 // Value of Perfect Information (if computed)
 if( get_int_config( "intComputeVPI" ) &&
   (anticipative_full.solved || anticipative_reduced.solved)) {
  cout << "\nValue of Perfect Information" << endl;

  // VPI = Stochastic objective - Anticipative objective
  // (cost of not having perfect information)
  if( anticipative_full.solved && full_result.solved ) {
   double vpi_full = full_result.objective - anticipative_full.objective;
   cout << "  Full scenarios" << endl;
   cout << "    Stochastic......: " << fixed << setprecision( 2 )
        << full_result.objective << endl;
   cout << "    Anticipative....: " << fixed << setprecision( 2 )
        << anticipative_full.objective << endl;
   cout << "    Value Perf. Info: " << fixed << setprecision( 2 ) << vpi_full;
   if( vpi_full < 0 ) {
    cout << " (WARNING: negative VPI indicates potential issue)";
   }
   cout << endl;
  }

  if( anticipative_reduced.solved && reduced_result.solved ) {
   double vpi_reduced =
     reduced_result.objective - anticipative_reduced.objective;
   cout << "  Reduced scenarios" << endl;
   cout << "    Stochastic..: " << fixed << setprecision( 2 )
        << reduced_result.objective << endl;
   cout << "    Anticipative: " << fixed << setprecision( 2 )
        << anticipative_reduced.objective << endl;
   cout << "    VPI.........: " << fixed << setprecision( 2 ) << vpi_reduced;
   if( vpi_reduced < 0 ) {
    cout << " (WARNING: negative VPI indicates potential issue)";
   }
   cout << endl;
  }
 }

 // Selected scenarios
 if( get_int_config( "intLogVerb" ) >= 2 ) {
  cout << "\nSelected scenarios and weights:" << endl;
  // Get the pool weights directly (no need to re-run scenario reduction)
  auto pool_weights = scenario_set->get_pool_weights( );

  // Print aligned indices and weights
  cout << "  Indices: ";
  for(int idx : reduction_metrics.selected_indices) {
   cout << setw( 8 ) << idx;
  }
  cout << endl;

  cout << "  Weights: ";
  for(double weight : pool_weights) {
   cout << setw( 8 ) << fixed << setprecision( 4 ) << weight;
  }
  cout << endl;
 }

 cout << "\n=============================" << endl;
}

/*--------------------------------------------------------------------------*/
/*------------------------- HELPER METHODS ---------------------------------*/
/*--------------------------------------------------------------------------*/

Block *AbstractScenarioReductionTest::get_base_block( ) { return base_block; }

/*--------------------------------------------------------------------------*/

void AbstractScenarioReductionTest::apply_scenario_to_block(
  const std::vector< double > & scenario ) {
 if( ! stochastic_block ) {
  throw runtime_error( "StochasticBlock not initialized - ensure "
   "TwoStageStochasticBlock is created first" );
 }

 // Get the DataMappings from the StochasticBlock
 const auto & data_mappings = stochastic_block->get_data_mappings( );

 // Save the original caller (the inner block of StochasticBlock)
 Block * original_caller = stochastic_block->get_inner_block( );

 // Apply each mapping to the base block
 for(auto & mapping : data_mappings) {
  mapping->set_caller( base_block );
  mapping->set_data( scenario.begin( ) , eNoBlck , eNoBlck );
 }

 // Restore the original caller for all mappings
 for(auto & mapping : data_mappings) {
  mapping->set_caller( original_caller );
 }
}

std::unique_ptr< TwoStageStochasticBlock > AbstractScenarioReductionTest::
create_twostage_block( ) {
 // Create temporary netCDF with current scenario_set state
 string temp_file = "temp_tss.nc4";
 create_twostage_netcdf( temp_file ); // Uses internal scenario_set
 // Load as TwoStageStochasticBlock
 netCDF::NcFile file( temp_file , netCDF::NcFile::read );
 auto tssGroup = file.getGroup( "TwoStageStochasticBlock" );
 if( tssGroup.isNull( )) {
  throw std::runtime_error(
   "Failed to get TwoStageStochasticBlock group from file" );
 }
 auto tss_block = std::make_unique< TwoStageStochasticBlock >( );
 tss_block->deserialize( tssGroup );
 file.close( );

 remove( temp_file.c_str( ));
 return tss_block;
}

SolutionResult AbstractScenarioReductionTest::compute_extensive_form(
  TwoStageStochasticBlock * tss_block ) {
 SolutionResult result;

 try {
  // Configure solver
  auto * bsc = dynamic_cast< BlockSolverConfig * >(
    Configuration::deserialize( get_str_config( "strSolverConfig" )));

  if( ! bsc ) { throw runtime_error( "Failed to load solver configuration" ); }
  bsc->apply( tss_block );

  // Get solver
  if( ! tss_block->get_registered_solvers( ).empty( )) {
   auto solver = tss_block->get_registered_solvers( ).front( );

   // Set time limit if specified
   if( get_dbl_config( "dblMaxTime" ) > 0 ) {
    solver->set_par( Solver::dblMaxTime , get_dbl_config( "dblMaxTime" ));
   }

   // Solve
   auto solve_start = chrono::high_resolution_clock::now( );
   int solve_result = solver->compute( false );
   auto solve_end = chrono::high_resolution_clock::now( );

   result.time_ms =
     chrono::duration_cast< chrono::milliseconds >( solve_end - solve_start )
     .count( );

   if( solve_result == Solver::kOK ) {
    double obj = solver->get_ub( );

    // TwoStageStochasticBlock already weights objectives by probabilities
    result.objective = obj;
    result.solved = true;

    if( get_int_config( "intLogVerb" ) >= 2 ) {
     cout << "    Objective (expected): " << fixed << setprecision( 2 )
          << result.objective << endl;
     cout << "    Time: " << result.time_ms << " ms" << endl;
    }
   }
   else{
    if( get_int_config( "intLogVerb" ) >= 1 ) {
     cout << "    Failed to solve (status: " << solve_result;

     // Add human-readable status description
     if( solve_result == Solver::kInfeasible ) {
      cout << " - Problem is infeasible";
     }
     else if( solve_result == Solver::kUnbounded ) {
      cout << " - Problem is unbounded";
     }
     else if( solve_result == Solver::kError ) {
      cout << " - Solver error";
     }
     else if( solve_result == Solver::kStopTime ) {
      cout << " - Time limit reached";
     }
     else if( solve_result == Solver::kStopIter ) {
      cout << " - Iteration limit reached";
     }
     else if( solve_result == Solver::kLowPrecision ) {
      cout << " - Low precision solution";
     }

     cout << ")" << endl;
    }
   }
  }

  delete bsc;

 } catch( const exception & e ) {
  if( get_int_config( "intLogVerb" ) >= 1 ) {
   cout << "    Error: " << e.what( ) << endl;
  }
 }

 return result;
}

/*--------------------------------------------------------------------------*/

SolutionResult AbstractScenarioReductionTest::solve_anticipative_solution( ) {
 SolutionResult result;

 // Get the already-selected scenario indices (no need to re-run reduction)
 auto selected_indices = scenario_set->get_selected_scenarios( );
 auto pool_weights = scenario_set->get_pool_weights( );

 // Get scenario dimension
 size_t scenario_size = scenario_set->get_scenarioSize( );

 // Solve each selected scenario independently
 for(size_t i = 0; i < selected_indices.size( ); ++i) {
  auto scenario_idx = selected_indices[ i ];

  // Build the scenario vector from individual values
  vector< double > scenario( scenario_size );
  for(size_t j = 0; j < scenario_size; ++j) {
   scenario[ j ] = scenario_set->get_scenario_value( scenario_idx , j );
  }

  // Apply scenario to block
  apply_scenario_to_block( scenario );

  // Solve
  auto [ obj , solved ] = solve( get_base_block( ));

  if( solved ) {
   result.scenario_objectives.push_back( obj );
  }
  else{
   // If any scenario fails, the anticipative solution fails
   result.solved = false;
   return result;
  }
 }

 // Compute weighted expected value using the pool weights
 if( ! result.scenario_objectives.empty( )) {
  double weighted_sum = 0.0;
  for(size_t i = 0; i < result.scenario_objectives.size( ); ++i) {
   weighted_sum += result.scenario_objectives[ i ] * pool_weights[ i ];
  }
  result.objective = weighted_sum;
  result.solved = true;
 }

 return result;
}

/*--------------------------------------------------------------------------*/

pair< double , bool > AbstractScenarioReductionTest::solve( Block * block ) {
 try {
  // Configure solver
  auto cfg = Configuration::deserialize( get_str_config( "strSolverConfig" ));
  auto * bsc = dynamic_cast< BlockSolverConfig * >(cfg);

  if( ! bsc ) {
   delete cfg;
   return { 0.0 , false };
  }

  bsc->apply( block );

  // Get solver
  if( ! block->get_registered_solvers( ).empty( )) {
   auto solver = block->get_registered_solvers( ).front( );

   // Set time limit if specified
   if( get_dbl_config( "dblMaxTime" ) > 0 ) {
    cout << get_dbl_config( "dblMaxTime" ) << "[DEBUG]" << endl;
    solver->set_par( Solver::dblMaxTime , get_dbl_config( "dblMaxTime" ));
   }

   // Solve
   int result = solver->compute( false );

   if( result == Solver::kOK ) {
    double obj = solver->get_ub( );
    delete cfg;
    return { obj , true };
   }
  }

  delete cfg;
 } catch( const exception & e ) {
  // Solver failed
 }

 return { 0.0 , false };
}

/*--------------------------------------------------------------------------*/
/*------------------------- I/O METHODS ------------------------------------*/
/*--------------------------------------------------------------------------*/

vector< vector< double > > AbstractScenarioReductionTest::
load_scenarios_from_file(
  const string & filename ) {
 vector< vector< double > > scenarios;

 try {
  netCDF::NcFile file( filename , netCDF::NcFile::read );

  // Check for DiscreteScenarioSet structure
  auto scenDim = file.getDim( "NumberScenarios" );
  auto sizeDim = file.getDim( "ScenarioSize" );

  if( scenDim.isNull( ) || sizeDim.isNull( )) {
   throw runtime_error(
    "File doesn't contain valid DiscreteScenarioSet dimensions" );
  }

  size_t num_scenarios = scenDim.getSize( );
  size_t scenario_size = sizeDim.getSize( );

  // Load scenario data
  auto scenVar = file.getVar( "Scenarios" );
  if( scenVar.isNull( )) {
   throw runtime_error( "File doesn't contain Scenarios variable" );
  }

  scenarios.resize( num_scenarios );
  for(size_t s = 0; s < num_scenarios; ++s) {
   scenarios[ s ].resize( scenario_size );
   scenVar.getVar( { s , 0 } , { 1 , scenario_size } , scenarios[ s ].data( ));
  }

  file.close( );

 } catch( const netCDF::exceptions::NcException & e ) {
  throw runtime_error(
   "NetCDF error reading scenario file: " + string( e.what( )));
 }

 return scenarios;
}

/*--------------------------------------------------------------------------*/

string AbstractScenarioReductionTest::get_scenario_file(
  const string & instance_path ) const {
 // Extract instance name from path
 filesystem::path p( instance_path );
 string instance_name = p.stem( ).string( ); // Get filename without extension

 // Get the scenarios directory from the subclass
 string scenarios_dir = get_scenarios_directory( );

 // Construct the scenario file path
 // Expected convention: scenarios are named as instance_name_scenarios.nc4
 string scenario_file = scenarios_dir + instance_name + "_scenarios.nc4";

 return scenario_file;
}

/*--------------------------------------------------------------------------*/

bool AbstractScenarioReductionTest::update_SR_config(
  const string & method ,
  bool warmstart ,
  bool shuffle ) {
 try {
  ifstream in( "BSConfig_SR.txt" );
  if( ! in.is_open( )) return false;

  stringstream buffer;
  string line;

  while( getline( in , line )) {
   // Update intPar_ScenRedAlg based on method
   if( line.find( "intPar_ScenRedAlg" ) != string::npos ) {
    if( method == "dupacova" ) {
     line = "intPar_ScenRedAlg = 1";
    }
    else if( method == "bestfit" ) {
     line = "intPar_ScenRedAlg = 2";
    }
    else if( method == "firstfit" ) {
     line = "intPar_ScenRedAlg = 3";
    }
   }
   // Update warm start parameter
   else if( line.find( "intWarmStart" ) != string::npos ) {
    line = "intWarmStart = " + to_string( warmstart ? 1 : 0 );
   }
   // Update shuffle parameter
   else if( line.find( "boolShuffleFF" ) != string::npos ) {
    line = "boolShuffleFF = " + to_string( shuffle ? 1 : 0 );
   }

   buffer << line << '\n';
  }

  in.close( );

  ofstream out( "BSConfig_SR.txt" );
  out << buffer.str( );
  out.close( );

  return true;

 } catch( const exception & e ) { return false; }
}

/*--------------------------------------------------------------------------*/

void AbstractScenarioReductionTest::save_solution_cache(
  const string & filename ,
  const SolutionResult & result ) {
 if( get_int_config( "intLogVerb" ) >= 2 ) {
  cout << "    Saving solution result to cache: " << filename << endl;
 }

 try {
  // Create directory if needed
  filesystem::path filepath( filename );
  if( filepath.has_parent_path( )) {
   filesystem::create_directories( filepath.parent_path( ));
  }

  // Create netCDF file
  netCDF::NcFile file( filename , netCDF::NcFile::replace );

  // Save scalar values as attributes
  file.putAtt( "objective" , netCDF::NcDouble( ) , result.objective );
  file.putAtt( "solved" , netCDF::NcInt( ) , result.solved ? 1 : 0 );
  file.putAtt( "time_ms" , netCDF::NcInt64( ) , result.time_ms );

  // Save scenario objectives if present
  if( ! result.scenario_objectives.empty( )) {
   auto dim = file.addDim( "NumScenarios" , result.scenario_objectives.size( ));
   auto var = file.addVar( "ScenarioObjectives" , netCDF::NcDouble( ) , dim );
   var.putVar( result.scenario_objectives.data( ));
  }

  file.close( );

  if( get_int_config( "intLogVerb" ) >= 2 ) {
   cout << "      Saved solution with objective=" << result.objective
        << ", time=" << result.time_ms << "ms" << endl;
  }

 } catch( const exception & e ) {
  cerr << "Warning: Failed to save solution cache: " << e.what( ) << endl;
 }
}

/*--------------------------------------------------------------------------*/

void AbstractScenarioReductionTest::save_solutions_cache( ) {
 if( ! get_int_config( "intSaveResults" )) { return; }

 save_solution_cache(
   get_str_config( "strCacheDir" ) + "full_result.nc4" ,
   full_result );
 save_solution_cache(
   get_str_config( "strCacheDir" ) + "reduced_result.nc4" ,
   reduced_result );
 save_solution_cache(
   get_str_config( "strCacheDir" ) + "anticipative_full.nc4" ,
   anticipative_full );
 save_solution_cache(
   get_str_config( "strCacheDir" ) + "anticipative_reduced.nc4" ,
   anticipative_reduced );

 if( get_int_config( "intLogVerb" ) >= 1 ) {
  cout << "  Saved results to cache" << endl;
 }
}

/*--------------------------------------------------------------------------*/

SolutionResult AbstractScenarioReductionTest::load_solution_cache(
  const string & filename ) {
 if( get_int_config( "intLogVerb" ) >= 2 ) {
  cout << "    Loading solution result from cache: " << filename << endl;
 }

 if( ! filesystem::exists( filename )) {
  throw runtime_error( "Cache file not found: " + filename );
 }

 SolutionResult result;

 netCDF::NcFile file( filename , netCDF::NcFile::read );

 // Load scalar values from attributes
 file.getAtt( "objective" ).getValues( & result.objective );

 int solved_int;
 file.getAtt( "solved" ).getValues( & solved_int );
 result.solved = (solved_int != 0);

 file.getAtt( "time_ms" ).getValues( & result.time_ms );

 // Load scenario objectives if present
 try {
  auto var = file.getVar( "ScenarioObjectives" );
  if( ! var.isNull( )) {
   auto dim = file.getDim( "NumScenarios" );
   size_t num_scenarios = dim.getSize( );
   result.scenario_objectives.resize( num_scenarios );
   var.getVar( result.scenario_objectives.data( ));
  }
 } catch( ... ) {
  // No scenario objectives in file
 }

 file.close( );

 if( get_int_config( "intLogVerb" ) >= 2 ) {
  cout << "      Loaded solution with objective=" << result.objective
       << ", time=" << result.time_ms << "ms" << endl;
 }

 return result;
}

/*--------------------------------------------------------------------------*/
/*-------------------- End File AbstractScenarioReductionTest.cpp ---------*/
/*--------------------------------------------------------------------------*/
