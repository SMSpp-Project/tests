/*--------------------------------------------------------------------------*/
/*-------------------- File CFLScenarioGenerator.cpp ----------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Standalone scenario generator for Capacitated Facility Location (CFL)
 * problems. Generates and validates stochastic demand scenarios, saving them as
 * DiscreteScenarioSet in netCDF format
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
#include <random>
#include <span>
#include <string>

#include "BlockSolverConfig.h"
#include "CapacitatedFacilityLocationBlock.h"
#include "Configuration.h"
#include "Solver.h"

using namespace std;
using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*------------------------------ STRUCTURES --------------------------------*/
/*--------------------------------------------------------------------------*/

struct GeneratorConfig {
 string instance_path;             // Path to base CFL instance
 string output_path;               // Output path for scenarios
 int num_scenarios = 20;           // Number of scenarios to generate
 double variation_factor = 0.2;    // Variation factor for demands
 unsigned seed = 42;               // Random seed
 int verbose = 1;                  // Verbosity level
 double validation_timeout = 10.0; // Timeout for validation solving
 bool validate = true;             // Whether to validate scenarios
 bool validate_only = false;       // Only validate, don't generate scenarios
 string solver_config = "BSPar_HiGHS.txt"; // Solver configuration file
};

/*--------------------------------------------------------------------------*/
/*--------------------------- HELPER FUNCTIONS -----------------------------*/
/*--------------------------------------------------------------------------*/

void print_help( const char * program_name ) {
 cout << "Usage: " << program_name << " [options]" << endl;
 cout << "\nOptions:" << endl;
 cout << "  -i, --instance <path>     Path to base CFL instance file (required)"
      << endl;
 cout << "  -o, --output <path>       Output path for scenarios (default: "
  "<instance>_scenarios.nc4)"
      << endl;
 cout << "  -n, --scenarios <number>  Number of scenarios to generate "
  "(default: 20)"
      << endl;
 cout << "  -v, --variation <factor>  Variation factor for demands (default: "
  "0.2)"
      << endl;
 cout << "  -s, --seed <number>       Random seed (default: 42)" << endl;
 cout << "  --verbose <level>         Verbosity level 0-2 (default: 1)" << endl;
 cout << "  --no-validate            Skip scenario validation" << endl;
 cout << "  --validate-only          Only validate instance, don't generate "
  "scenarios"
      << endl;
 cout << "  --timeout <seconds>       Validation timeout per scenario "
  "(default: 10)"
      << endl;
 cout << "  --solver-config <path>    Solver configuration file (default: "
  "BSPar_HiGHS.txt)"
      << endl;
 cout << "  -h, --help               Show this help message" << endl;
 cout << "\nExamples:" << endl;
 cout << "  " << program_name << " -i cap41.nc4 -n 100 -v 0.3" << endl;
 cout << "  " << program_name
      << " --instance orlib/cap102.nc4 --scenarios 50 --seed 123" << endl;
 cout << "  " << program_name
      << " --instance cap41.nc4 --validate-only  # Just validate instance"
      << endl;
}

/*--------------------------------------------------------------------------*/

GeneratorConfig parse_arguments( int argc , char * argv[] ) {
 GeneratorConfig config;

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
  else if( arg == "--verbose" ) {
   if( i + 1 < argc ) {
    config.verbose = stoi( argv[ ++i ] );
    if( config.verbose < 0 || config.verbose > 2 ) {
     cerr << "Verbose level must be 0, 1, or 2" << endl;
     exit( 1 );
    }
   }
   else{
    cerr << "Missing value after " << arg << endl;
    exit( 1 );
   }
  }
  else if( arg == "--no-validate" ) {
   config.validate = false;
  }
  else if( arg == "--validate-only" ) {
   config.validate_only = true;
   config.validate = true; // Ensure validation is enabled
  }
  else if( arg == "--timeout" ) {
   if( i + 1 < argc ) {
    config.validation_timeout = stod( argv[ ++i ] );
    if( config.validation_timeout <= 0 ) {
     cerr << "Timeout must be positive" << endl;
     exit( 1 );
    }
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

 // Set default output path if not specified
 if( config.output_path.empty( )) {
  filesystem::path instance_file( config.instance_path );
  // Save to centralized scenarios directory
  config.output_path =
    "../scenarios/CFL/" + instance_file.stem( ).string( ) + "_scenarios.nc4";
 }

 return config;
}

/*--------------------------------------------------------------------------*/

vector< vector< double > > generate_clustered_scenarios(
  const vector< double > & base_demands ,
  int num_scenarios ,
  double variation_factor ,
  unsigned seed ,
  int verbose ) {
 if( verbose >= 1 )
  cout << "Generating " << num_scenarios << " scenarios in 5 clusters..."
       << endl;

 int scenario_dim = base_demands.size( );
 vector< vector< double > > scenarios;

 mt19937 gen( seed );

 // Add the original scenario first
 scenarios.push_back( base_demands );

 // Determine scenarios per cluster (now 5 clusters)
 int scenarios_per_cluster = (num_scenarios - 1) / 5;
 int remaining = (num_scenarios - 1) % 5;

 // Cluster 0: Around original (small variation 10%)
 uniform_real_distribution<> cluster0_dist(
   1.0 - variation_factor * 0.5 ,
   1.0 + variation_factor * 0.5 );
 int cluster0_count = scenarios_per_cluster + (remaining > 0 ? 1 : 0);
 for(int s = 0; s < cluster0_count; ++s) {
  vector< double > scenario( scenario_dim );
  for(int i = 0; i < scenario_dim; ++i) {
   scenario[ i ] = base_demands[ i ] * cluster0_dist( gen );
  }
  scenarios.push_back( scenario );
 }
 if( remaining > 0 ) remaining--;

 // Cluster 1: High demand (1.3x to 1.5x)
 uniform_real_distribution<> cluster1_dist(
   1.0 + variation_factor * 1.5 ,
   1.0 + variation_factor * 2.5 );
 int cluster1_count = scenarios_per_cluster + (remaining > 0 ? 1 : 0);
 for(int s = 0; s < cluster1_count; ++s) {
  vector< double > scenario( scenario_dim );
  for(int i = 0; i < scenario_dim; ++i) {
   scenario[ i ] = base_demands[ i ] * cluster1_dist( gen );
  }
  scenarios.push_back( scenario );
 }
 if( remaining > 0 ) remaining--;

 // Cluster 2: Low demand (0.5x to 0.7x)
 uniform_real_distribution<> cluster2_dist(
   1.0 - variation_factor * 2.5 ,
   1.0 - variation_factor * 1.5 );
 int cluster2_count = scenarios_per_cluster + (remaining > 0 ? 1 : 0);
 for(int s = 0; s < cluster2_count; ++s) {
  vector< double > scenario( scenario_dim );
  for(int i = 0; i < scenario_dim; ++i) {
   scenario[ i ] = max( 0.0 , base_demands[ i ] * cluster2_dist( gen ));
  }
  scenarios.push_back( scenario );
 }
 if( remaining > 0 ) remaining--;

 // Cluster 3: Mixed/regional (half high, half low)
 uniform_real_distribution<> high_dist(
   1.0 + variation_factor * 1.0 ,
   1.0 + variation_factor * 2.0 );
 uniform_real_distribution<> low_dist(
   1.0 - variation_factor * 2.0 ,
   1.0 - variation_factor * 1.0 );
 int cluster3_count = scenarios_per_cluster + (remaining > 0 ? 1 : 0);
 for(int s = 0; s < cluster3_count; ++s) {
  vector< double > scenario( scenario_dim );
  for(int i = 0; i < scenario_dim; ++i) {
   if( i < scenario_dim / 2 ) {
    scenario[ i ] = base_demands[ i ] * high_dist( gen );
   }
   else{
    scenario[ i ] = max( 0.0 , base_demands[ i ] * low_dist( gen ));
   }
  }
  scenarios.push_back( scenario );
 }
 if( remaining > 0 ) remaining--;

 // Cluster 4: Very low demand (around 0.4x to 0.6x - about half)
 uniform_real_distribution<> cluster4_dist( 0.4 , 0.6 );
 int cluster4_count = scenarios_per_cluster + remaining;
 for(int s = 0; s < cluster4_count; ++s) {
  vector< double > scenario( scenario_dim );
  for(int i = 0; i < scenario_dim; ++i) {
   scenario[ i ] = base_demands[ i ] * cluster4_dist( gen );
  }
  scenarios.push_back( scenario );
 }

 if( verbose >= 2 ) {
  cout << "  Cluster 0 (normal): " << cluster0_count
       << " scenarios around original" << endl;
  cout << "  Cluster 1 (high): " << cluster1_count
       << " scenarios with high demand" << endl;
  cout << "  Cluster 2 (low): " << cluster2_count
       << " scenarios with low demand" << endl;
  cout << "  Cluster 3 (mixed): " << cluster3_count
       << " scenarios with regional variation" << endl;
  cout << "  Cluster 4 (very low): " << cluster4_count
       << " scenarios with very low demand (~50%)" << endl;
 }

 return scenarios;
}

/*--------------------------------------------------------------------------*/

bool validate_scenario(
  CapacitatedFacilityLocationBlock * cfl ,
  const vector< double > & original_demands ,
  const vector< double > & scenario_demands ,
  const GeneratorConfig & config ) {
 // Apply scenario demands
 cfl->chg_customer_demands( scenario_demands.begin( ));

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
  bsc->apply( cfl );

  if( ! cfl->get_registered_solvers( ).empty( )) {
   auto solver = cfl->get_registered_solvers( ).front( );

   if( solver ) {
    solver->set_par( Solver::dblMaxTime , config.validation_timeout );

    int result = solver->compute( false );

    // A scenario is treated as valid (feasible) unless the solver *proves* it
    // infeasible/unbounded.  Crucially, a wall-clock timeout (kStopTime) must
    // NOT be treated as invalid: otherwise the verdict depends on machine load
    // / solver threading / timing, which makes regeneration, and hence the
    // whole scenario set, non-deterministic across runs with the same seed.
    // Infeasibility, by contrast, is a deterministic mathematical fact.
    success = ( result != Solver::kInfeasible &&
                result != Solver::kUnbounded );

    if( result == Solver::kOK && config.verbose >= 2 ) {
     double obj = solver->get_ub( );
     cout << " (obj: " << fixed << setprecision( 2 ) << obj << ")";
    }
   }
  }
 } catch( const exception & e ) {
  if( config.verbose >= 1 ) {
   cerr << "Exception during validation: " << e.what( ) << endl;
  }
 }

 delete cfg;

 // Restore original demands
 cfl->chg_customer_demands( original_demands.begin( ));

 return success;
}

/*--------------------------------------------------------------------------*/

vector< double > regenerate_scenario(
  const vector< double > & base_demands ,
  int cluster ,
  mt19937 & gen ) {
 vector< double > new_scenario( base_demands.size( ));
 uniform_real_distribution<> mild_variation( 0.85 , 1.15 );

 // Generate based on cluster type with milder variation
 switch( cluster ) {
 case 0: // Normal cluster
  for(size_t i = 0; i < base_demands.size( ); ++i) {
   new_scenario[ i ] = base_demands[ i ] * mild_variation( gen );
  }
  break;

 case 1: // High demand cluster
  for(size_t i = 0; i < base_demands.size( ); ++i) {
   uniform_real_distribution<> dist( 1.1 , 1.3 );
   new_scenario[ i ] = base_demands[ i ] * dist( gen );
  }
  break;

 case 2: // Low demand cluster
  for(size_t i = 0; i < base_demands.size( ); ++i) {
   uniform_real_distribution<> dist( 0.7 , 0.9 );
   new_scenario[ i ] = base_demands[ i ] * dist( gen );
  }
  break;

 case 3: // Mixed cluster
  for(size_t i = 0; i < base_demands.size( ); ++i) {
   if( i < base_demands.size( ) / 2 ) {
    uniform_real_distribution<> dist( 1.05 , 1.2 );
    new_scenario[ i ] = base_demands[ i ] * dist( gen );
   }
   else{
    uniform_real_distribution<> dist( 0.8 , 0.95 );
    new_scenario[ i ] = base_demands[ i ] * dist( gen );
   }
  }
  break;

 case 4: // Very low demand cluster
  for(size_t i = 0; i < base_demands.size( ); ++i) {
   uniform_real_distribution<> dist( 0.45 , 0.55 );
   new_scenario[ i ] = base_demands[ i ] * dist( gen );
  }
  break;

 default: // Fallback to mild variation
  for(size_t i = 0; i < base_demands.size( ); ++i) {
   new_scenario[ i ] = base_demands[ i ] * mild_variation( gen );
  }
 }

 return new_scenario;
}

/*--------------------------------------------------------------------------*/

void save_scenarios_netcdf(
  const string & filename ,
  const vector< vector< double > > & scenarios ,
  const GeneratorConfig & config ) {
 if( config.verbose >= 1 ) {
  cout << "\nSaving scenarios to: " << filename << endl;
 }

 // Create directory if needed
 filesystem::path filepath( filename );
 if( filepath.has_parent_path( )) {
  filesystem::create_directories( filepath.parent_path( ));
 }

 // Create netCDF file
 netCDF::NcFile file( filename , netCDF::NcFile::replace );

 // Add metadata attributes
 file.putAtt( "generator" , "CFLScenarioGenerator" );
 file.putAtt( "instance" , config.instance_path );
 file.putAtt( "num_scenarios" , netCDF::NcInt( ) , config.num_scenarios );
 file.putAtt( "variation_factor" , netCDF::NcDouble( ) , config.variation_factor
   );
 file.putAtt( "seed" , netCDF::NcUint( ) , config.seed );

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
 }
}

/*--------------------------------------------------------------------------*/

void save_scenarios_text(
  const string & filename ,
  const vector< vector< double > > & scenarios ,
  const GeneratorConfig & config ) {
 string txt_file = filename.substr( 0 , filename.find_last_of( '.' )) + ".txt";

 if( config.verbose >= 1 ) {
  cout << "Also saving scenarios as text to: " << txt_file << endl;
 }

 ofstream out( txt_file );
 if( ! out.is_open( )) {
  cerr << "Failed to create text file: " << txt_file << endl;
  return;
 }

 out << "# CFL Scenario Set" << endl;
 out << "# Generated from: " << config.instance_path << endl;
 out << "# Number of scenarios: " << scenarios.size( ) << endl;
 out << "# Scenario size: " << scenarios[ 0 ].size( ) << endl;
 out << "# Variation factor: " << config.variation_factor << endl;
 out << "# Seed: " << config.seed << endl;
 out << "#" << endl;

 // Write scenarios
 for(size_t s = 0; s < scenarios.size( ); ++s) {
  out << "# Scenario " << s << endl;
  for(size_t i = 0; i < scenarios[ s ].size( ); ++i) {
   out << scenarios[ s ][ i ];
   if( i < scenarios[ s ].size( ) - 1 ) out << " ";
  }
  out << endl;
 }

 out.close( );

 if( config.verbose >= 1 ) { cout << "Text file saved successfully" << endl; }
}

/*--------------------------------------------------------------------------*/
/*--------------------------------- MAIN -----------------------------------*/
/*--------------------------------------------------------------------------*/

int main( int argc , char * argv[] ) {
 try {
  // Parse command line arguments
  GeneratorConfig config = parse_arguments( argc , argv );

  // Print configuration
  if( config.verbose >= 1 ) {
   if( config.validate_only ) {
    cout << "CFL Instance Validator" << endl;
    cout << "=====================" << endl;
    cout << "Instance: " << config.instance_path << endl;
    cout << "Timeout: " << config.validation_timeout << "s" << endl;
   }
   else{
    cout << "CFL Scenario Generator" << endl;
    cout << "======================" << endl;
    cout << "Instance: " << config.instance_path << endl;
    cout << "Output: " << config.output_path << endl;
    cout << "Scenarios: " << config.num_scenarios << endl;
    cout << "Variation: " << config.variation_factor << endl;
    cout << "Seed: " << config.seed << endl;
    if( config.validate ) {
     cout << "Validation: enabled (timeout: " << config.validation_timeout
          << "s)" << endl;
    }
    else{
     cout << "Validation: disabled" << endl;
    }
   }
   cout << endl;
  }

  // Load base CFL instance
  if( config.verbose >= 1 ) cout << "Loading base instance..." << endl;

  Block * base_block = Block::deserialize( config.instance_path );
  auto * cfl = dynamic_cast< CapacitatedFacilityLocationBlock * >(base_block);

  if( ! cfl ) {
   delete base_block;
   throw runtime_error(
    "Failed to load CFL instance from " + config.instance_path );
  }

  // Ensure single-sourcing
  if( ! cfl->get_UnSplittable( )) {
   cfl->chg_UnSplittable( true );
   if( config.verbose >= 1 ) cout << "Converted to single-sourcing" << endl;
  }

  // Extract problem dimensions
  int nf = cfl->get_NFacilities( );
  int nc = cfl->get_NCustomers( );

  if( config.verbose >= 1 ) {
   cout << "Problem size: " << nf << " facilities, " << nc << " customers"
        << endl;
  }

  // If validate-only mode, just validate the instance and exit
  if( config.validate_only ) {
   if( config.verbose >= 1 ) cout << "\nValidating instance..." << endl;

   // Extract base demands for validation
   vector< double > base_demands( nc );
   for(int i = 0; i < nc; ++i) {
    base_demands[ i ] = cfl->get_Demand( i );
   }

   bool valid = validate_scenario( cfl , base_demands , base_demands , config );

   if( config.verbose >= 1 ) {
    if( valid ) {
     cout << "Instance is VALID (feasible with single-sourcing)" << endl;
    }
    else{
     cout << "Instance is INVALID (infeasible or timeout with single-sourcing)"
          << endl;
    }
   }

   delete cfl;
   return valid ? 0 : 1;
  }

  // Extract base demands
  vector< double > base_demands( nc );
  for(int i = 0; i < nc; ++i) {
   base_demands[ i ] = cfl->get_Demand( i );
  }

  // Generate scenarios
  if( config.verbose >= 1 ) cout << "\nGenerating scenarios..." << endl;

  vector< vector< double > > scenarios = generate_clustered_scenarios(
   base_demands ,
   config.num_scenarios ,
   config.variation_factor ,
   config.seed ,
   config.verbose );

  // Validate scenarios if requested
  if( config.validate ) {
   if( config.verbose >= 1 ) cout << "\nValidating scenarios..." << endl;

   int invalid_count = 0;
   vector< int > cluster_assignment( scenarios.size( ));
   cluster_assignment[ 0 ] = -1; // Original scenario

   // Assign clusters to scenarios (now 5 clusters)
   int idx = 1;
   int scenarios_per_cluster = (config.num_scenarios - 1) / 5;
   int remaining = (config.num_scenarios - 1) % 5;

   for(int c = 0; c < 5; ++c) {
    int count = scenarios_per_cluster + (remaining > c ? 1 : 0);
    for(int i = 0; i < count && idx < scenarios.size( ); ++i) {
     cluster_assignment[ idx++ ] = c;
    }
   }

   // Validate and regenerate if needed
   mt19937 gen( config.seed + 1000 ); // Different seed for regeneration

   for(size_t s = 0; s < scenarios.size( ); ++s) {
    if( config.verbose >= 2 ) {
     cout << "  Validating scenario " << s << "...";
    }

    bool valid = validate_scenario( cfl , base_demands , scenarios[ s ] , config
     );

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
      // Try regenerating up to 5 times at current cluster level
      int regen_attempts = 0;

      while( ! valid && regen_attempts < max_regen_attempts ) {
       scenarios[ s ] =
         regenerate_scenario( base_demands , cluster_assignment[ s ] , gen );

       valid = validate_scenario( cfl , base_demands , scenarios[ s ] , config )
       ;
       regen_attempts++;

       if( ! valid && config.verbose >= 2 ) {
        cout << " Regeneration attempt " << regen_attempts
             << " failed at cluster " << cluster_assignment[ s ] << endl;
       }
      }

      // If still invalid and not at bottom cluster, move down
      if( ! valid && cluster_assignment[ s ] > 0 ) {
       if( config.verbose >= 2 ) {
        cout << " Moving from cluster " << cluster_assignment[ s ]
             << " to cluster " << (cluster_assignment[ s ] - 1) << endl;
       }
       cluster_assignment[ s ]--;
      }
      else if( ! valid ) {
       // We're at the bottom cluster and still can't generate valid scenario
       break;
      }
     }

     if( ! valid ) {
      cerr << "Warning: Could not generate valid scenario for scenario " << s
           << " even after trying all lower demand clusters" << endl;
     }
    }
   }

   if( config.verbose == 1 ) {
    cout << endl; // Newline after progress dots
   }

   if( config.verbose >= 1 && invalid_count > 0 ) {
    cout << "Regenerated " << invalid_count << " invalid scenarios" << endl;
   }
  }

  // Save scenarios
  save_scenarios_netcdf( config.output_path , scenarios , config );

  // Also save as text for inspection
  if( config.verbose >= 1 ) {
   save_scenarios_text( config.output_path , scenarios , config );
  }

  // Clean up
  delete cfl;

  if( config.verbose >= 1 ) {
   cout << "\nScenario generation completed successfully" << endl;
  }

  return 0;

 } catch( const exception & e ) {
  cerr << "Error: " << e.what( ) << endl;
  return 1;
 }
}
