/*--------------------------------------------------------------------------*/
/*-------------------- File UCScenarioGenerator.cpp -----------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Standalone scenario generator for Unit Commitment (UC) problems.
 * Generates stochastic scenarios for both demand (ActivePowerDemand) and
 * renewable generation (MaxPower) profiles, saving them as DiscreteScenarioSet
 * in netCDF format.
 *
 * The generator properly handles intermittent unit indexing by tracking
 * the actual unit indices in the UC instance, avoiding issues with
 * non-consecutive unit numbering.
 *
 * \author Benoît Tran \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Benoît Tran
 */
/*--------------------------------------------------------------------------*/

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <netcdf>
#include <random>

#include "BlockSolverConfig.h"
#include "Configuration.h"
#include "IntermittentUnitBlock.h"
#include "Solver.h"
#include "UCBlock.h"

using namespace std;
using namespace SMSpp_di_unipi_it;
namespace fs = std::filesystem;

/*--------------------------------------------------------------------------*/
/*------------------------------ STRUCTURES --------------------------------*/
/*--------------------------------------------------------------------------*/

struct UCGeneratorConfig {
 string instance_path;   // Path to base UC instance
 string output_path;     // Output path for scenarios
 int num_scenarios = 20; // Number of scenarios to generate
 double variation_factor =
   0.3;                        // Variation factor for both demand and renewable
 unsigned seed = 42;           // Random seed
 int verbose = 1;              // Verbosity level
 bool enable_demand = true;    // Enable demand uncertainty
 bool enable_renewable = true; // Enable renewable uncertainty
 double validation_timeout = 10.0; // Timeout for validation solving
 bool validate = false; // Whether to validate scenarios
 bool validate_only = false; // Only validate, don't generate scenarios
 string solver_config = "../BSPar_HiGHS.txt"; // Solver configuration file
};

/*--------------------------------------------------------------------------*/
/*--------------------------- HELPER FUNCTIONS -----------------------------*/
/*--------------------------------------------------------------------------*/

void print_help( const char * program_name ) {
 cout << "Usage: " << program_name << " [options]" << endl;
 cout << "\nOptions:" << endl;
 cout << "  -i, --instance <path>     Path to base UC instance file (required)"
      << endl;
 cout << "  -o, --output <path>       Output path for scenarios" << endl;
 cout << "  -n, --scenarios <number>  Number of scenarios to generate "
  "(default: 20)"
      << endl;
 cout << "  -v, --variation <factor>  Variation factor (default: 0.3)" << endl;
 cout << "  -s, --seed <number>       Random seed (default: 42)" << endl;
 cout << "  --no-demand              Disable demand uncertainty" << endl;
 cout << "  --no-maxpower            Disable renewable uncertainty" << endl;
 cout << "  --verbose <level>        Verbosity level 0-2 (default: 1)" << endl;
 cout << "  --validate               Enable scenario validation (WILL SEGFAULT with demand)"
      << endl;
 cout << "  --no-validate            Skip scenario validation" << endl;
 cout << "  --validate-only          Only validate instance, don't generate "
  "scenarios"
      << endl;
 cout << "  --timeout <seconds>      Validation timeout per scenario (default: "
  "10)"
      << endl;
 cout << "  --solver-config <path>   Solver configuration file (default: "
  "BSPar_HiGHS.txt)"
      << endl;
 cout << "  -h, --help               Show this help message" << endl;
 cout << "\nExamples:" << endl;
 cout << "  " << program_name << " -i EC_CO_Test.nc4 -n 100 -v 0.3" << endl;
 cout << "  " << program_name << " -i EC_NC_Test.nc4 --no-demand -n 50" << endl;
 cout << "  " << program_name << " -i EC_CO_Test.nc4 --no-maxpower -n 30"
      << endl;
 cout << "  " << program_name
      << " -i EC_CO_Test.nc4 --validate-only  # Just validate instance" << endl;
}

/*--------------------------------------------------------------------------*/

UCGeneratorConfig parse_arguments( int argc , char * argv[] ) {
 UCGeneratorConfig config;

 for(int i = 1; i < argc; ++i) {
  string arg = argv[ i ];

  if( arg == "-h" || arg == "--help" ) {
   print_help( argv[ 0 ] );
   exit( 0 );
  }
  else if( arg == "-i" || arg == "--instance" ) {
   if( i + 1 < argc ) {
    config.instance_path = argv[ ++i ];
   }
   else{
    cerr << "Missing value after " << arg << endl;
    exit( 1 );
   }
  }
  else if( arg == "-o" || arg == "--output" ) {
   if( i + 1 < argc ) {
    config.output_path = argv[ ++i ];
   }
   else{
    cerr << "Missing value after " << arg << endl;
    exit( 1 );
   }
  }
  else if( arg == "-n" || arg == "--scenarios" ) {
   if( i + 1 < argc ) {
    config.num_scenarios = stoi( argv[ ++i ] );
    if( config.num_scenarios <= 0 ) {
     cerr << "Number of scenarios must be positive" << endl;
     exit( 1 );
    }
   }
   else{
    cerr << "Missing value after " << arg << endl;
    exit( 1 );
   }
  }
  else if( arg == "-v" || arg == "--variation" ) {
   if( i + 1 < argc ) {
    config.variation_factor = stod( argv[ ++i ] );
    if( config.variation_factor < 0 ) {
     cerr << "Variation factor must be non-negative" << endl;
     exit( 1 );
    }
   }
   else{
    cerr << "Missing value after " << arg << endl;
    exit( 1 );
   }
  }
  else if( arg == "-s" || arg == "--seed" ) {
   if( i + 1 < argc ) {
    config.seed = stoi( argv[ ++i ] );
   }
   else{
    cerr << "Missing value after " << arg << endl;
    exit( 1 );
   }
  }
  else if( arg == "--no-demand" ) {
   config.enable_demand = false;
  }
  else if( arg == "--no-maxpower" ) {
   config.enable_renewable = false;
  }
  else if( arg == "--verbose" ) {
   if( i + 1 < argc ) {
    config.verbose = stoi( argv[ ++i ] );
    if( config.verbose < 0 || config.verbose > 3 ) {
     cerr << "Verbose level must be 0, 1, 2, or 3" << endl;
     exit( 1 );
    }
   }
   else{
    cerr << "Missing value after " << arg << endl;
    exit( 1 );
   }
  }
  else if( arg == "--validate" ) {
   config.validate = true;
  }
  else if( arg == "--no-validate" ) {
   config.validate = false;
  }
  else if( arg == "--validate-only" ) {
   config.validate_only = true;
   config.validate = true;
  }
  else if( arg == "--timeout" ) {
   if( i + 1 < argc ) {
    config.validation_timeout = stod( argv[ ++i ] );
   }
   else{
    cerr << "Missing value after " << arg << endl;
    exit( 1 );
   }
  }
  else if( arg == "--solver-config" ) {
   if( i + 1 < argc ) {
    config.solver_config = argv[ ++i ];
   }
   else{
    cerr << "Missing value after " << arg << endl;
    exit( 1 );
   }
  }
  else{
   cerr << "Unknown option: " << arg << endl;
   cerr << "Use -h or --help for usage information" << endl;
   exit( 1 );
  }
 }

 // Check required arguments
 if( config.instance_path.empty( )) {
  cerr << "Error: Instance path is required (-i or --instance)" << endl;
  cerr << "Use -h or --help for usage information" << endl;
  exit( 1 );
 }

 // Check that at least one uncertainty type is enabled
 if( ! config.enable_demand && ! config.enable_renewable ) {
  cerr << "Error: At least one uncertainty type must be enabled" << endl;
  cerr << "Cannot use both --no-demand and --no-maxpower" << endl;
  exit( 1 );
 }

 // Set default output path if not specified
 if( config.output_path.empty( )) {
  fs::path instance_file( config.instance_path );
  string suffix;
  if( config.enable_demand && config.enable_renewable ) {
   suffix = "_apdmp";
  }
  else if( config.enable_demand ) {
   suffix = "_apd";
  }
  else{
   suffix = "_mp";
  }
  // Save to centralized scenarios directory
  config.output_path = "../scenarios/UCBlock/" + instance_file.stem( ).string( )
    +
    suffix + "_scenarios.nc4";
 }

 return config;
}

/*--------------------------------------------------------------------------*/

struct UCData {
 vector< double > demand_profile;             // ActivePowerDemand flattened
 vector< vector< double > > renewable_profiles; // MaxPower for each renewable unit
 vector< int > renewable_unit_indices;        // Indices of intermittent units
 int num_periods = 0;
 int num_nodes = 0;
 int num_renewable_units = 0;
};

/*--------------------------------------------------------------------------*/

UCData load_uc_instance(
  const string & instance_path ,
  const UCGeneratorConfig & config ) {
 UCData data;

 if( config.verbose >= 1 ) {
  cout << "Loading UC instance: " << instance_path << endl;
 }

 try {
  netCDF::NcFile file( instance_path , netCDF::NcFile::read );
  auto block = file.getGroup( "Block_0" );

  // Load demand profile if needed
  if( config.enable_demand ) {
   auto demand_var = block.getVar( "ActivePowerDemand" );
   if( ! demand_var.isNull( )) {
    // Get dimensions
    auto dims = demand_var.getDims( );
    if( dims.size( ) != 2 ) {
     throw runtime_error( "ActivePowerDemand should have 2 dimensions" );
    }
    // ActivePowerDemand is [TimeHorizon, NumberNodes] in netCDF
    // But C++ API returns dimensions in reverse order
    data.num_periods = dims[ 1 ].getSize( ); // TimeHorizon (actually dims[1])
    data.num_nodes = dims[ 0 ].getSize( );   // NumberNodes (actually dims[0])


    // Read demand data safely
    size_t total_size = data.num_periods * data.num_nodes;
    if( total_size == 0 ) {
     throw runtime_error( "ActivePowerDemand has zero size" );
    }

    // Read data in time-major order (as stored in file)
    vector< double > temp_demand( total_size );
    demand_var.getVar( temp_demand.data( ));

    // Transpose to node-major order for UCBlock
    // File has [time][node], UCBlock expects [node][time]
    data.demand_profile.resize( total_size );
    for(size_t t = 0; t < data.num_periods; t++) {
     for(size_t n = 0; n < data.num_nodes; n++) {
      // Source: temp_demand[t * num_nodes + n]
      // Dest: demand_profile[n * num_periods + t]
      data.demand_profile[ n * data.num_periods + t ] =
        temp_demand[ t * data.num_nodes + n ];
     }
    }


    if( config.verbose >= 2 ) {
     cout << "  Loaded demand: " << data.num_periods << " periods × "
          << data.num_nodes << " nodes = " << data.demand_profile.size( )
          << " values" << endl;
    }
   }
   else{
    cerr << "Warning: No ActivePowerDemand found in instance" << endl;
   }
  }

  // Load renewable profiles if needed
  if( config.enable_renewable ) {
   // Find all IntermittentUnitBlock groups
   auto groups = block.getGroups( );
   for(const auto &[ name , group ] : groups) {
    if( name.find( "UnitBlock_" ) != string::npos ) {
     // Extract unit index from name (e.g., "UnitBlock_5" -> 5)
     int unit_index = -1;
     size_t underscore_pos = name.find( "_" );
     if( underscore_pos != string::npos ) {
      try {
       unit_index = stoi( name.substr( underscore_pos + 1 ));
      } catch( ... ) {
       continue;
      }
     }

     auto type_attr = group.getAtt( "type" );
     if( ! type_attr.isNull( )) {
      string unit_type;
      type_attr.getValues( unit_type );

      if( unit_type == "IntermittentUnitBlock" ) {
       auto max_power = group.getVar( "MaxPower" );
       if( ! max_power.isNull( )) {
        auto power_dims = max_power.getDims( );
        if( power_dims.empty( ) || power_dims[ 0 ].getSize( ) == 0 ) {
         if( config.verbose >= 2 ) {
          cout << "    Warning: Skipping IntermittentUnitBlock " << name
               << " with empty MaxPower" << endl;
         }
         continue;
        }

        size_t profile_size = power_dims[ 0 ].getSize( );
        vector< double > profile( profile_size );
        max_power.getVar( profile.data( ));
        data.renewable_profiles.push_back( profile );
        data.renewable_unit_indices.push_back( unit_index );
        data.num_renewable_units++;

        if( config.verbose >= 2 ) {
         cout << "    Found intermittent unit " << name << " at index " << unit_index << endl;
        }
       }
      }
     }
    }
   }

   if( config.verbose >= 2 ) {
    cout << "  Loaded renewable profiles: " << data.num_renewable_units
         << " units × " << data.num_periods << " periods" << endl;
   }
  }

 } catch( const netCDF::exceptions::NcException & e ) {
  cerr << "Error loading UC instance: " << e.what( ) << endl;
  exit( 1 );
 }

 return data;
}

/*--------------------------------------------------------------------------*/

vector< vector< double > > generate_demand_scenarios(
  const vector< double > & base_demand ,
  int num_clusters ,
  mt19937 & gen ,
  double variation_factor ) {

 vector< vector< double > > scenarios;

 // Cluster 1: High demand (1.2x to 1.4x)
 uniform_real_distribution<> high_dist( 1.2 , 1.4 );
 vector< double > high_scenario( base_demand.size( ));
 for(size_t i = 0; i < base_demand.size( ); ++i) {
  high_scenario[ i ] = base_demand[ i ] * high_dist( gen );
 }
 scenarios.push_back( high_scenario );

 // Cluster 2: Base demand (0.9x to 1.1x)
 uniform_real_distribution<> base_dist( 0.9 , 1.1 );
 vector< double > base_scenario( base_demand.size( ));
 for(size_t i = 0; i < base_demand.size( ); ++i) {
  base_scenario[ i ] = base_demand[ i ] * base_dist( gen );
 }
 scenarios.push_back( base_scenario );

 // Cluster 3: Low demand (0.6x to 0.8x)
 uniform_real_distribution<> low_dist( 0.6 , 0.8 );
 vector< double > low_scenario( base_demand.size( ));
 for(size_t i = 0; i < base_demand.size( ); ++i) {
  low_scenario[ i ] = base_demand[ i ] * low_dist( gen );
 }
 scenarios.push_back( low_scenario );

 return scenarios;
}

/*--------------------------------------------------------------------------*/

vector< vector< double > > generate_renewable_scenarios(
  const vector< vector< double > > & base_profiles ,
  int num_clusters ,
  mt19937 & gen ,
  double variation_factor ) {

 vector< vector< double > > scenarios;
 size_t total_size = base_profiles.size( ) *
   (base_profiles.empty( ) ? 0 : base_profiles[ 0 ].size( ));

 // Cluster 1: High renewable (sunny/windy - 1.2x to 1.5x)
 uniform_real_distribution<> high_dist( 1.2 , 1.5 );
 vector< double > high_scenario;
 high_scenario.reserve( total_size );
 for(const auto & profile : base_profiles) {
  for(double val : profile) {
   // Preserve zeros (night time for solar)
   high_scenario.push_back( val == 0 ? 0 : val * high_dist( gen ));
  }
 }
 scenarios.push_back( high_scenario );

 // Cluster 2: Base renewable (0.8x to 1.2x)
 uniform_real_distribution<> base_dist( 0.8 , 1.2 );
 vector< double > base_scenario;
 base_scenario.reserve( total_size );
 for(const auto & profile : base_profiles) {
  for(double val : profile) {
   base_scenario.push_back( val == 0 ? 0 : val * base_dist( gen ));
  }
 }
 scenarios.push_back( base_scenario );

 // Cluster 3: Low renewable (cloudy/calm - 0.3x to 0.7x)
 uniform_real_distribution<> low_dist( 0.3 , 0.7 );
 vector< double > low_scenario;
 low_scenario.reserve( total_size );
 for(const auto & profile : base_profiles) {
  for(double val : profile) {
   low_scenario.push_back( val == 0 ? 0 : val * low_dist( gen ));
  }
 }
 scenarios.push_back( low_scenario );

 return scenarios;
}

/*--------------------------------------------------------------------------*/

vector< vector< double > > combine_scenarios(
  const vector< vector< double > > & demand_scenarios ,
  const vector< vector< double > > & renewable_scenarios ,
  const UCGeneratorConfig & config ) {

 vector< vector< double > > combined_scenarios;

 // Always add base scenario first (original data)
 vector< double > base_scenario;
 if( ! demand_scenarios.empty( ) && ! renewable_scenarios.empty( )) {
  // Combine middle scenarios (base clusters)
  base_scenario.insert(
    base_scenario.end( ) ,
    demand_scenarios[ 1 ].begin( ) ,
    demand_scenarios[ 1 ].end( ));
  base_scenario.insert(
    base_scenario.end( ) ,
    renewable_scenarios[ 1 ].begin( ) ,
    renewable_scenarios[ 1 ].end( ));
 }
 else if( ! demand_scenarios.empty( )) {
  base_scenario = demand_scenarios[ 1 ]; // Base demand cluster
 }
 else{
  base_scenario = renewable_scenarios[ 1 ]; // Base renewable cluster
 }
 combined_scenarios.push_back( base_scenario );

 // Generate all combinations
 if( config.enable_demand && config.enable_renewable ) {
  // Cartesian product of demand and renewable scenarios
  for(const auto & demand : demand_scenarios) {
   for(const auto & renewable : renewable_scenarios) {
    if( & demand == & demand_scenarios[ 1 ] &&
      & renewable == & renewable_scenarios[ 1 ] ) {
     continue; // Skip base scenario (already added)
    }
    vector< double > scenario;
    scenario.reserve( demand.size( ) + renewable.size( ));
    scenario.insert( scenario.end( ) , demand.begin( ) , demand.end( ));
    scenario.insert( scenario.end( ) , renewable.begin( ) , renewable.end( ));
    combined_scenarios.push_back( scenario );
   }
  }
 }
 else if( config.enable_demand ) {
  // Only demand scenarios (skip the base already added)
  for(size_t i = 0; i < demand_scenarios.size( ); ++i) {
   if( i != 1 ) { // Skip base
    combined_scenarios.push_back( demand_scenarios[ i ] );
   }
  }
 }
 else{
  // Only renewable scenarios (skip the base already added)
  for(size_t i = 0; i < renewable_scenarios.size( ); ++i) {
   if( i != 1 ) { // Skip base
    combined_scenarios.push_back( renewable_scenarios[ i ] );
   }
  }
 }

 // Ensure we have the requested number of scenarios
 while( combined_scenarios.size( ) < static_cast< size_t >(config.num_scenarios)
   ) {
  // Duplicate scenarios with small random perturbations
  mt19937 gen( config.seed + combined_scenarios.size( ));
  uniform_real_distribution<> perturb( 0.95 , 1.05 );

  size_t idx = combined_scenarios.size( ) % (combined_scenarios.size( ) - 1) + 1
  ;
  vector< double > new_scenario = combined_scenarios[ idx ];
  for(double & val : new_scenario) {
   if( val != 0 ) { // Don't perturb zeros
    val *= perturb( gen );
   }
  }
  combined_scenarios.push_back( new_scenario );
 }

 // Trim to exact number if we have too many
 if( combined_scenarios.size( ) > static_cast< size_t >(config.num_scenarios)) {
  combined_scenarios.resize( config.num_scenarios );
 }

 return combined_scenarios;
}

/*--------------------------------------------------------------------------*/

bool validate_scenario(
  UCBlock * uc_block ,
  const UCData & original_data ,
  const vector< double > & scenario ,
  const UCGeneratorConfig & config ,
  bool is_base_scenario = false ) {

 if( config.verbose >= 3 ) {
  cout << "\n  [validate_scenario] is_base=" << is_base_scenario
       << ", demand_size=" << original_data.demand_profile.size( )
       << ", periods=" << original_data.num_periods
       << ", nodes=" << original_data.num_nodes << endl;
 }

 // Check UCBlock state
 if( config.verbose >= 3 ) {
  cout << "  [DEBUG] UCBlock state check:" << endl;
  cout << "    UCBlock address: " << uc_block << endl;
  cout << "    Number of nodes: " << uc_block->get_number_nodes( ) << endl;
  cout << "    Time horizon: " << uc_block->get_time_horizon( ) << endl;
 }

 // If this is the base scenario validation, skip applying scenarios
 // since the UCBlock already has the correct base values
 if( ! is_base_scenario ) {
  // Apply scenario to UCBlock
  // First, extract demand and renewable parts from the combined scenario
  size_t demand_size = original_data.demand_profile.size( );
  size_t renewable_total_size =
    original_data.num_renewable_units * original_data.num_periods;

  // Apply demand changes if demand uncertainty is enabled
  if( config.enable_demand && demand_size > 0 ) {
   vector< double > scenario_demand(
     scenario.begin( ) ,
     scenario.begin( ) + demand_size );


   // Pass all data (nodes * periods) with full range
   uc_block->set_active_power_demand(
     scenario_demand.begin( ) ,
     Block::Range( 0 , demand_size ));

  }

  // Apply renewable changes if renewable uncertainty is enabled
  if( config.enable_renewable && original_data.num_renewable_units > 0 ) {
   // Get the renewable portion of the scenario
   size_t start_idx = config.enable_demand ? demand_size : 0;

   // Apply to each intermittent unit using the correct indices
   for(size_t idx = 0; idx < original_data.renewable_unit_indices.size(); ++idx) {
    int unit_index = original_data.renewable_unit_indices[idx];

    // Get the specific unit block by index
    auto * unit_block = dynamic_cast< IntermittentUnitBlock * >(
      uc_block->get_unit_block( unit_index ));

    if( unit_block ) {
     // Extract this unit's profile from the scenario
     size_t profile_start = start_idx + (idx * original_data.num_periods);
     vector< double > unit_profile(
       scenario.begin( ) + profile_start ,
       scenario.begin( ) + profile_start + original_data.num_periods );

     // Use time range for set_maximum_power
     unit_block->set_maximum_power(
       unit_profile.begin( ) ,
       Block::Range( 0 , original_data.num_periods ));

    } else {
     cerr << "Warning: Could not get IntermittentUnitBlock at index "
          << unit_index << endl;
    }
   }
  }
 } // end if (!is_base_scenario)

 // Load solver configuration
 auto cfg = Configuration::deserialize( config.solver_config );
 if( ! cfg ) {
  if( config.verbose >= 1 ) {
   cerr << "Failed to load solver configuration: " << config.solver_config
        << endl;
  }
  return false;
 }

 auto * bsc = dynamic_cast< BlockSolverConfig * >(cfg);
 if( ! bsc ) {
  delete cfg;
  if( config.verbose >= 1 ) {
   cerr << "Configuration is not a BlockSolverConfig" << endl;
  }
  return false;
 }

 bool success = false;

 try {
  bsc->apply( uc_block );

  // Get and configure the solver
  if( ! uc_block->get_registered_solvers( ).empty( )) {
   auto solver = uc_block->get_registered_solvers( ).front( );

   if( solver ) {
    // Set timeout
    solver->set_par( Solver::dblMaxTime , config.validation_timeout );

    // Solve
    int result = solver->compute( false );

    // Check if feasible
    if( result == Solver::kOK ) {
     success = true;
     if( config.verbose >= 3 ) {
      double obj = solver->get_ub( );
      cout << " (obj: " << fixed << setprecision( 2 ) << obj << ")";
     }
    }
   }
  }
 } catch( const exception & e ) {
  if( config.verbose >= 2 ) {
   cerr << "Solver exception: " << e.what( ) << endl;
  }
 }

 delete bsc;

 // Restore original values (only if we modified them)
 if( ! is_base_scenario ) {
  if( config.enable_demand ) {
   uc_block->set_active_power_demand(
     original_data.demand_profile.begin( ) ,
     Block::Range( 0 , original_data.num_periods ));
  }

  if( config.enable_renewable && original_data.num_renewable_units > 0 ) {
   // Restore using the correct unit indices
   for(size_t idx = 0; idx < original_data.renewable_unit_indices.size(); ++idx) {
    int unit_index = original_data.renewable_unit_indices[idx];
    auto * unit_block = dynamic_cast< IntermittentUnitBlock * >(
      uc_block->get_unit_block( unit_index ));
    if( unit_block && idx < original_data.renewable_profiles.size( )) {
     unit_block->set_maximum_power(
       original_data.renewable_profiles[ idx ].begin( ) ,
       Block::Range( 0 , original_data.num_periods ));
    }
   }
  }
 }

 return success;
}

/*--------------------------------------------------------------------------*/

vector< double > regenerate_scenario(
  const UCData & base_data ,
  int cluster ,
  mt19937 & gen ,
  const UCGeneratorConfig & config ) {

 vector< double > new_scenario;
 uniform_real_distribution<> mild_variation( 0.85 , 1.15 );

 // Generate demand part if enabled
 if( config.enable_demand ) {
  vector< double > demand_scenario( base_data.demand_profile.size( ));

  switch( cluster ) {
  case 0: // Normal cluster
   for(size_t i = 0; i < base_data.demand_profile.size( ); ++i) {
    demand_scenario[ i ] = base_data.demand_profile[ i ] * mild_variation( gen )
    ;
   }
   break;

  case 1: // High demand cluster
   for(size_t i = 0; i < base_data.demand_profile.size( ); ++i) {
    uniform_real_distribution<> dist( 1.1 , 1.3 );
    demand_scenario[ i ] = base_data.demand_profile[ i ] * dist( gen );
   }
   break;

  case 2: // Low demand cluster
   for(size_t i = 0; i < base_data.demand_profile.size( ); ++i) {
    uniform_real_distribution<> dist( 0.7 , 0.9 );
    demand_scenario[ i ] = base_data.demand_profile[ i ] * dist( gen );
   }
   break;
  }

  new_scenario.insert(
    new_scenario.end( ) ,
    demand_scenario.begin( ) ,
    demand_scenario.end( ));
 }

 // Generate renewable part if enabled
 if( config.enable_renewable ) {
  for(const auto & base_profile : base_data.renewable_profiles) {
   vector< double > renewable_scenario( base_profile.size( ));

   switch( cluster ) {
   case 0: // Normal cluster
    for(size_t i = 0; i < base_profile.size( ); ++i) {
     renewable_scenario[ i ] = base_profile[ i ] * mild_variation( gen );
    }
    break;

   case 1: // High renewable cluster
    for(size_t i = 0; i < base_profile.size( ); ++i) {
     uniform_real_distribution<> dist( 1.1 , 1.3 );
     renewable_scenario[ i ] = base_profile[ i ] * dist( gen );
    }
    break;

   case 2: // Low renewable cluster
    for(size_t i = 0; i < base_profile.size( ); ++i) {
     uniform_real_distribution<> dist( 0.7 , 0.9 );
     renewable_scenario[ i ] = base_profile[ i ] * dist( gen );
    }
    break;
   }

   new_scenario.insert(
     new_scenario.end( ) ,
     renewable_scenario.begin( ) ,
     renewable_scenario.end( ));
  }
 }

 return new_scenario;
}

/*--------------------------------------------------------------------------*/

void save_scenarios_netcdf(
  const string & filename ,
  const vector< vector< double > > & scenarios ,
  const UCGeneratorConfig & config ,
  const UCData & data ) {

 if( config.verbose >= 1 ) {
  cout << "\nSaving scenarios to: " << filename << endl;
 }

 // Create directory if needed
 fs::path filepath( filename );
 if( filepath.has_parent_path( )) {
  fs::create_directories( filepath.parent_path( ));
 }

 // Create netCDF file
 netCDF::NcFile file( filename , netCDF::NcFile::replace );

 // Add metadata attributes
 file.putAtt( "generator" , "UCScenarioGenerator" );
 file.putAtt( "instance" , config.instance_path );
 file.putAtt( "num_scenarios" , netCDF::NcInt( ) , config.num_scenarios );
 file.putAtt( "variation_factor" , netCDF::NcDouble( ) , config.variation_factor
   );
 file.putAtt( "seed" , netCDF::NcUint( ) , config.seed );
 file.putAtt( "demand_enabled" , netCDF::NcInt( ) , config.enable_demand ? 1 : 0
   );
 file.putAtt(
   "renewable_enabled" ,
   netCDF::NcInt( ) ,
   config.enable_renewable ? 1 : 0 );

 // Add component sizes
 if( config.enable_demand ) {
  file.putAtt(
    "demand_size" ,
    netCDF::NcInt( ) ,
    static_cast< int >(data.demand_profile.size( )));
 }
 if( config.enable_renewable ) {
  file.putAtt( "renewable_units" , netCDF::NcInt( ) , data.num_renewable_units )
  ;
  file.putAtt(
    "renewable_size" ,
    netCDF::NcInt( ) ,
    data.num_renewable_units * data.num_periods );
 }

 // Add timestamp
 auto now = chrono::system_clock::now( );
 auto time_t = chrono::system_clock::to_time_t( now );
 string timestamp = ctime( & time_t );
 timestamp.pop_back( ); // Remove newline
 file.putAtt( "generated_at" , timestamp );

 // Add dimensions
 auto scenDim = file.addDim( "NumberScenarios" , scenarios.size( ));
 auto sizeDim = file.addDim( "ScenarioSize" , scenarios[ 0 ].size( ));

 // Add scenario data
 auto scenVar =
   file.addVar( "Scenarios" , netCDF::NcDouble( ) , { scenDim , sizeDim } );
 for(size_t s = 0; s < scenarios.size( ); ++s) {
  scenVar.putVar( { s , 0 } , { 1 , scenarios[ 0 ].size( ) } , scenarios[ s ].
    data( ));
 }

 // Add uniform probabilities
 auto probVar = file.addVar( "Probabilities" , netCDF::NcDouble( ) , scenDim );
 vector< double > probs( scenarios.size( ) , 1.0 / scenarios.size( ));
 probVar.putVar( probs.data( ));

 if( config.verbose >= 1 ) {
  cout << "Successfully saved " << scenarios.size( ) << " scenarios" << endl;
  cout << "Scenario size: " << scenarios[ 0 ].size( ) << " values" << endl;
  if( config.enable_demand && config.enable_renewable ) {
   cout << "  Demand: " << data.demand_profile.size( ) << " values" << endl;
   cout << "  Renewable: " << (data.num_renewable_units * data.num_periods)
        << " values" << endl;
  }
 }
}

/*--------------------------------------------------------------------------*/
/*--------------------------------- MAIN -----------------------------------*/
/*--------------------------------------------------------------------------*/

int main( int argc , char * argv[] ) {
 try {
  // Parse command line arguments
  UCGeneratorConfig config = parse_arguments( argc , argv );

  // Print configuration
  if( config.verbose >= 1 ) {
   cout << "UC Scenario Generator" << endl;
   cout << "=====================" << endl;
   cout << "Instance: " << config.instance_path << endl;
   if( ! config.validate_only ) {
    cout << "Output: " << config.output_path << endl;
    cout << "Scenarios: " << config.num_scenarios << endl;
    cout << "Variation: " << config.variation_factor << endl;
    cout << "Seed: " << config.seed << endl;
   }
   cout << "Uncertainty types:" << endl;
   cout << "  Demand: " << (config.enable_demand ? "enabled" : "disabled")
        << endl;
   cout << "  Renewable: " << (config.enable_renewable ? "enabled" : "disabled")
        << endl;
   if( config.validate ) {
    cout << "Validation: "
         << (config.validate_only ? "validate-only mode" : "enabled") << endl;
    cout << "Solver config: " << config.solver_config << endl;
    cout << "Timeout: " << config.validation_timeout << " seconds" << endl;
   }
   cout << endl;
  }

  // Load UC instance data
  UCData data = load_uc_instance( config.instance_path , config );

  // Load UCBlock for validation if needed
  UCBlock * uc_block = nullptr;
  if( config.validate ) {
   if( config.verbose >= 1 ) {
    cout << "\nLoading UCBlock for validation..." << endl;
   }

   uc_block = new UCBlock( );

   // Open the netCDF file and get the Block_0 group
   netCDF::NcFile nc_file( config.instance_path , netCDF::NcFile::read );
   auto block_group = nc_file.getGroup( "Block_0" );
   if( block_group.isNull( )) {
    throw runtime_error( "No Block_0 group found in " + config.instance_path );
   }
   uc_block->deserialize( block_group );

   // Validate base instance
   if( config.verbose >= 1 ) { cout << "Validating base instance..."; }

   // Create base scenario (just the original data)
   vector< double > base_scenario;
   if( config.enable_demand ) {
    base_scenario.insert(
      base_scenario.end( ) ,
      data.demand_profile.begin( ) ,
      data.demand_profile.end( ));
   }
   if( config.enable_renewable ) {
    for(const auto & profile : data.renewable_profiles) {
     base_scenario.insert( base_scenario.end( ) , profile.begin( ) , profile.end
        ( ));
    }
   }

   bool valid = validate_scenario(
    uc_block ,
    data ,
    base_scenario ,
    config ,
    true );   // true = is_base_scenario
   if( config.verbose >= 1 ) {
    if( valid ) {
     cout << " VALID" << endl;
    }
    else{
     cout << " INVALID (infeasible or timeout)" << endl;
    }
   }

   if( config.validate_only ) {
    delete uc_block;
    return valid ? 0 : 1;
   }
  }

  // Initialize random generator
  mt19937 gen( config.seed );

  // Generate demand scenarios
  vector< vector< double > > demand_scenarios;
  if( config.enable_demand ) {
   if( config.verbose >= 1 ) {
    cout << "\nGenerating demand scenarios (3 clusters)..." << endl;
   }
   demand_scenarios = generate_demand_scenarios(
    data.demand_profile ,
    3 ,
    gen ,
    config.variation_factor );
  }

  // Generate renewable scenarios
  vector< vector< double > > renewable_scenarios;
  if( config.enable_renewable ) {
   if( config.verbose >= 1 ) {
    cout << "Generating renewable scenarios (3 clusters)..." << endl;
   }
   renewable_scenarios = generate_renewable_scenarios(
    data.renewable_profiles ,
    3 ,
    gen ,
    config.variation_factor );
  }

  // Combine scenarios
  if( config.verbose >= 1 ) { cout << "Combining scenarios..." << endl; }
  vector< vector< double > > combined_scenarios =
    combine_scenarios( demand_scenarios , renewable_scenarios , config );

  // Track cluster assignments for regeneration
  vector< int > cluster_assignment;
  for(int i = 0; i < 3; ++i) {  // 3 clusters
   int scenarios_per_cluster = config.num_scenarios / 3;
   if( i == 2 ) scenarios_per_cluster += config.num_scenarios % 3;
   for(int j = 0; j < scenarios_per_cluster; ++j) {
    cluster_assignment.push_back( i );
   }
  }

  // Validate scenarios if enabled
  if( config.validate && uc_block ) {
   if( config.verbose >= 1 ) {
    cout << "\nValidating generated scenarios..." << endl;
   }

   int invalid_count = 0;
   for(size_t s = 0; s < combined_scenarios.size( ); ++s) {
    if( config.verbose >= 2 ) {
     cout << "  Validating scenario " << s << "...";
    }

    bool valid =
      validate_scenario( uc_block , data , combined_scenarios[ s ] , config ,
     false );

    if( valid ) {
     if( config.verbose >= 2 ) {
      cout << " OK" << endl;
     }
     else if( config.verbose >= 1 ) {
      cout << "." << flush;
     }
    }
    else{
     invalid_count++;
     if( config.verbose >= 2 ) {
      cout << " INVALID - regenerating" << endl;
     }
     else if( config.verbose >= 1 ) {
      cout << "!" << flush;
     }

     // Keep trying to generate a valid scenario
     bool valid = false;
     const int max_regen_attempts = 5;

     // Try each cluster from current down to 0
     while( ! valid && cluster_assignment[ s ] >= 0 ) {
      int regen_attempts = 0;

      while( ! valid && regen_attempts < max_regen_attempts ) {
       combined_scenarios[ s ] =
         regenerate_scenario( data , cluster_assignment[ s ] , gen , config );

       valid = validate_scenario(
        uc_block ,
        data ,
        combined_scenarios[ s ] ,
        config ,
        false );
       regen_attempts++;

       if( ! valid && config.verbose >= 2 ) {
        cout << "    Regeneration attempt " << regen_attempts
             << " failed at cluster " << cluster_assignment[ s ] << endl;
       }
      }

      // If still invalid and not at bottom cluster, move down
      if( ! valid && cluster_assignment[ s ] > 0 ) {
       if( config.verbose >= 2 ) {
        cout << "    Moving from cluster " << cluster_assignment[ s ]
             << " to cluster " << (cluster_assignment[ s ] - 1) << endl;
       }
       cluster_assignment[ s ]--;
      }
      else if( ! valid ) {
       break;
      }
     }

     if( ! valid ) {
      cerr << "\nWarning: Could not generate valid scenario for scenario " << s
           << " even after trying all lower demand clusters" << endl;
     }
    }
   }

   if( config.verbose >= 1 && invalid_count > 0 ) {
    cout << "\n\nValidation summary: " << invalid_count
         << " scenarios were invalid and regenerated" << endl;
   }
   else if( config.verbose >= 1 ) {
    cout << "\n\nAll scenarios validated successfully!" << endl;
   }
  }

  // Save scenarios
  if( config.verbose >= 1 ) {
   cout << "\nSaving scenarios to: " << config.output_path << endl;
  }
  save_scenarios_netcdf( config.output_path , combined_scenarios , config , data
    );

  if( config.verbose >= 1 ) { cout << "\nGeneration complete" << endl; }

  // Clean up
  if( uc_block ) { delete uc_block; }

 } catch( const exception & e ) {
  cerr << "Error: " << e.what( ) << endl;
  return 1;
 }

 return 0;
}

/*--------------------------------------------------------------------------*/
/*------------------ End File UCScenarioGenerator.cpp ---------------------*/
/*--------------------------------------------------------------------------*/
