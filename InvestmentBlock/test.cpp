/*--------------------------------------------------------------------------*/
/*------------------------------ File test.cpp -----------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 *
 * This is a convenient tool for solving the investment problem defined by an
 * InvestmentBlock. The description of the InvestmentBlock must be given in a
 * netCDF file. This tool can be executed as follows:
 *
 *   ./IB_test [-o VALUE] [-p PATH] [-c PATH] [-x FILE ]
 *             -B FILE -S FILE <nc4-file>
 *
 * The only mandatory arguments are the netCDF file containing the description
 * of the InvestmentBlock and the solver configuration file indicated by the
 * -S option. This netCDF file can be either a BlockFile or a ProbFile. The
 * BlockFile can contain any number of child groups, each one describing an
 * InvestmentBlock. Every InvestmentBlock is then solved. The ProbFile can
 * also contain any number of child groups, each one having the description of
 * an InvestmentBlock alongside the description of a BlockConfig and a
 * BlockSolverConfig for the InvestmentBlock. Also in this case, every
 * InvestmentBlock is solved.
 *
 * The -c option specifies the prefix to the paths to all configuration
 * files. This means that if PATH is the value passed to the -c option, then
 * the name (or path) to each configuration file will be prepended by
 * PATH. The -p option specifies the prefix to the paths to all files
 * specified by the attribute "filename" in the input netCDF file.
 *
 * It is possible to provide an initial point (initial solution or initial
 * investment) through the -x option. This option must be followed by a file
 * containing the initial point. If there are N assets subject to investment,
 * then this file must contain N numbers, where the i-th number is the initial
 * value for the investment in the i-th asset. If this option is not used,
 * then the initial value x_i for the investment in the i-th asset is
 * determined as follows. If the lower bound l_i on the i-th investment is
 * finite, then x_i = l_i. Otherwise, if the upper bound u_i on the i-th
 * investment is finite, then x_i = u_i. Otherwise, if both bounds are not
 * finite, then x_i = 0.
 *
 * The -o option gives the reference value the first Solver is compared to.
 *
 * For a BlockFile, the -S option specifies the BlockSolverConfig of every
 * InvestmentBlock, and the -B option its BlockConfig; for a ProbFile, both
 * come from the file, and -B only concerns the inner Block. The -B file is
 * typically a "meta"-BlockConfig (see InnerBCfg.txt), which is dispatched to
 * the InvestmentBlock and inside the inner Block of its InvestmentFunction,
 * the UCBlock of each stage of an SDDPBlock included. The BlockConfig of the
 * InvestmentBlock is an OBlockConfig (see IBOCfg.txt), which reformulates the
 * bounds on the investment and gives the InvestmentFunction its
 * ComputeConfig (see IFCfg.txt), whose extra Configuration holds the
 * BlockSolverConfig of the inner Block: everything is said by the
 * configuration files, and the tester only reads and applies them.
 *
 * \author Rafael Durbano Lobato \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \author Antonio Frangioni \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \author Donato Meoli \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Rafael Durbano Lobato, Antonio Frangioni, Donato Meoli
 */
/*--------------------------------------------------------------------------*/
/*-------------------------------- MACROS ----------------------------------*/
/*--------------------------------------------------------------------------*/

#define LOG_LEVEL 2
// -1 = no log at all, not even pass/fail
// 0 = only pass/fail
// 1 = result of each test
// 2 = + solver log
// 3 = reserved
// 4 = reserved

#if( LOG_LEVEL >= 1 )
#define LOG1( x ) std::cout << x
#define CLOG1( y , x ) if( y ) std::cout << x

#if( LOG_LEVEL >= 2 )
#define LOG_ON_COUT 1
#endif
#else
#define LOG1( x )
#define CLOG1( y , x )
#endif

// USECOLORS / RED / GREEN: in common_utils.h
#include "common_utils.h"

/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <getopt.h>
#include <filesystem>
#include <cerrno>
#include <cstdlib>
#include <exception>
#include <typeinfo>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <functional>
#include <map>
#include <iomanip>
#include <iostream>
#include <queue>
#include <chrono>
#include <cmath>
#include <limits>
#include <list>

#include <Block.h>
#include <BlockSolverConfig.h>
#include <CDASolver.h>
#include <Solution.h>

#include <BatteryUnitBlock.h>
#include <BendersBlock.h>
#include <HydroSystemUnitBlock.h>
#include <IntermittentUnitBlock.h>
#include <NetworkBlock.h>
#include <SDDPBlock.h>
#include <SDDPGreedySolver.h>
#include <StochasticBlock.h>
#include <SDDPSolver.h>
#include <SlackUnitBlock.h>
#include <ThermalUnitBlock.h>
#include <UCBlock.h>

#include "InvestmentBlock.h"
#include "InvestmentFunction.h"

#ifdef USE_MPI
#include <boost/mpi/environment.hpp>
#endif

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*------------------- Investment-local CLI extensions ----------------------*/
/*--------------------------------------------------------------------------*/
// Globals + getopt extensions that go beyond the test baseline in
// tests/common_utils.h. Kept local because they replicate the tools/
// state-save / solution-I/O machinery, which is not relevant to other tests.

std::string state_in_file;       ///< State to be loaded into the Solver (-b)
std::string state_out_file;      ///< final State of the Solver (-a)
std::string sol_input;           ///< filename of input Solution (-I)
std::string sol_output;          ///< filename of output Solution (-O)
std::string sol_cfg_file;        ///< filename of output Solution Config (-C)
bool output_solution = false;    ///< true if solution has to be output (-o)
bool writeprob       = false;    ///< if the problem should be written back (-n)

/// parse the current optarg as a long int, returning -1 on parse error

inline long get_long_option( char * end = nullptr )
{
 errno = 0;
 long option = std::strtol( optarg , &end , 10 );
 if( ( ! optarg ) || ( ( option = std::strtol( optarg , &end , 10 ) ) ,
                       ( errno || ( end && *end ) ) ) )
  option = -1;
 return( option );
 }

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

void get_initial_Solution( Block * block )
{
 if( sol_input.empty() )
  return;

 if( auto initsol = Solution::deserialize( sol_input ) ) {
  initsol->write( block );
  delete initsol;
 }
 else
  std::cout << "Warning: input Solution " << sol_input << " invalid"
            << std::endl;
}

/*--------------------------------------------------------------------------*/

void get_initial_State( Solver * solver )
{
 if( state_in_file.empty() )
  return;

 try {
  auto state = State::deserialize( state_in_file );
  solver->put_State( *state );
  delete state;
 }
 catch( netCDF::exceptions::NcException & ) {
  std::cout << "Warning: State file " << state_in_file
            << " could not be loaded" << std::endl;
 }
 catch( const std::exception & e ) {
  std::cout << "Warning: error " << e.what()
            << " occurred while loading the Solver State" << std::endl;
 }
}

/*--------------------------------------------------------------------------*/

void write_final_Solution( Block * block , Configuration * cfg = nullptr ,
                           bool replace = false )
{
 if( sol_output.empty() )
  return;

 Configuration * outsolcfg = cfg;
 if( ( ! outsolcfg ) && ( ! sol_cfg_file.empty() ) )
  if( ! ( outsolcfg = Configuration::deserialize(
          resolve_with_prefix( conf_prefix , sol_cfg_file ) ) ) )
   std::cout << "Warning: output Solution Configuration "
             << sol_cfg_file << " invalid" << std::endl;

 if( auto sol = block->get_Solution( outsolcfg , false ) ) {
  sol->serialize( sol_output , replace );
  delete sol;
 }
 else
  std::cout << "Warning: output Solution empty" << std::endl;

 if( ! cfg )
  delete outsolcfg;
}

/*--------------------------------------------------------------------------*/

void write_final_State( Solver * solver , bool replace = false )
{
 if( state_out_file.empty() )
  return;

 try {
  solver->serialize_State( state_out_file , replace );
 }
 catch( netCDF::exceptions::NcException & ) {
  std::cout << "Warning: State file " << state_out_file
            << " could not be opened" << std::endl;
 }
 catch( const std::exception & e ) {
  std::cout << "Warning: error " << e.what()
            << " occurred while saving the Solver State" << std::endl;
 }
}

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/

bool AllPassed = true;

std::string initial_point_filename{};

// State to be loaded into the InvestmentBlock Solver
std::string solver_state_input_filename{};

// Prefix to the name of the file that will store the State of the
// InvestmentBlock Solver
std::string solver_state_output_filename{};

std::vector< double > initial_point;

/*--------------------------------------------------------------------------*/

const std::string my_short_opts = "o:x:";

const std::vector< option > my_long_opts = {
  { "ref-objective" ,            required_argument , nullptr , 'o' } ,
  { "initial-investment" ,       required_argument , nullptr , 'x' }
  };

const std::string my_help =
 "  -o, --ref-objective <value>     compare the first solver to a reference\n"
 "  -x, --initial-investment <file> initial investment\n";

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

static double get_solver_objective_value( Solver * solver ) {
 if( solver->has_var_solution() )
  return( solver->get_var_value() );

 return( solver->get_lb() );
}

static bool test_investment_solvers( InvestmentBlock * investment_block ) {
 try {
  auto investment_function = static_cast< InvestmentFunction * >(
   investment_block->get_function() );

  auto & solvers = investment_block->get_registered_solvers();

  if( solvers.empty() )
   throw( std::logic_error( "No solver has been registered." ) );

  if( sol_verbose )
   for( auto solver : solvers )
    if( solver )
     solver->set_log( &std::cout );

  // set initial Solution, if provided - - - - - - - - - - - - - - - - - - - -
  get_initial_Solution( investment_block );

  // load the given State, if provided - - - - - - - - - - - - - - - - - - - -
  auto first_solver = solvers.front();
  get_initial_State( first_solver );

#if( LOG_LEVEL >= 1 )
  auto start = std::chrono::system_clock::now();
#endif

  int rtrn1st = Solver::kOK;
  if( ! dryrun )
   rtrn1st = first_solver->compute();

#if( LOG_LEVEL >= 1 )
  auto end = std::chrono::system_clock::now();
  std::chrono::duration< double > elapsed = end - start;
  double time1 = elapsed.count();
#endif

  bool hs1st = ( ( ( rtrn1st >= Solver::kOK ) && ( rtrn1st < Solver::kError )
    && ( rtrn1st != Solver::kUnbounded )
    && ( rtrn1st != Solver::kInfeasible ) )
   || ( rtrn1st == Solver::kLowPrecision ) );

  double fo1st = hs1st
                  ? get_solver_objective_value( first_solver )
                  : -Inf< double >();

#if( LOG_LEVEL >= 1 )
  long it1 = first_solver->get_elapsed_iterations();
#endif

  bool all_passed = hs1st;

  // build readings for every registered Solver (each an exact optimum read
  // via get_solver_objective_value); Solver 0 was already solved above, the
  // rest are solved here. The cross-check verdict and the uniform per-instance
  // line, including the optional RefObjective, are produced by common_utils
  std::vector< Solver * > S( solvers.begin() , solvers.end() );
  const std::size_t M = S.size();
  std::vector< double > times( M , 0.0 );
  std::vector< std::string > toks( M );
  std::vector< SolverReading > rd( M );
  std::vector< bool > hsv( M , false );
  std::vector< int > statusv( M , Solver::kOK );

  auto tok = []( bool h , int rtrn , const SolverReading & r ) -> std::string {
   if( h )                               return( reading_token( r ) );
   if( rtrn == Solver::kInfeasible )     return( "Unfeas" );
   if( rtrn == Solver::kUnbounded )      return( "Unbounded" );
   return( "Error!" );
   };

  times[ 0 ] = time1; hsv[ 0 ] = hs1st; statusv[ 0 ] = rtrn1st;
  if( hs1st ) rd[ 0 ] = SolverReading::exact( fo1st , eps_of( 0 , S[ 0 ] ) );
  toks[ 0 ] = tok( hs1st , rtrn1st , rd[ 0 ] );

  for( std::size_t k = 1 ; k < M ; ++k ) {
   auto st = std::chrono::system_clock::now();
   if( ! dryrun )
    statusv[ k ] = S[ k ]->compute();
   auto en = std::chrono::system_clock::now();
   times[ k ] = std::chrono::duration< double >( en - st ).count();
   hsv[ k ] = ( ( ( statusv[ k ] >= Solver::kOK ) &&
                  ( statusv[ k ] < Solver::kError ) &&
                  ( statusv[ k ] != Solver::kUnbounded ) &&
                  ( statusv[ k ] != Solver::kInfeasible ) )
                || ( statusv[ k ] == Solver::kLowPrecision ) );
   if( hsv[ k ] )
    rd[ k ] = SolverReading::exact( get_solver_objective_value( S[ k ] ) ,
                                    eps_of( k , S[ k ] ) );
   toks[ k ] = tok( hsv[ k ] , statusv[ k ] , rd[ k ] );
   }

  std::string verdict;
  double diff;
  all_passed = cross_check( rd , hsv , statusv , RefObjective , 1e-5 ,
                            verdict , diff );
  print_instance_line( times , toks , RefObjective , verdict , diff );
  (void) it1;

#if( LOG_LEVEL >= 0 )
  if( all_passed )
   std::cout << GREEN( All tests passed!! ) << std::endl;
  else
   std::cout << RED( Shit happened!! ) << std::endl;
#endif

  // write final Solution, if required - - - - - - - - - - - - - - - - - - - -
  write_final_Solution( investment_block );

  // write final State, if required- - - - - - - - - - - - - - - - - - - - - -
  write_final_State( first_solver );

  if( output_solution ) { // display the solution
   if( first_solver->has_var_solution() ) {
    const auto solution_value = first_solver->get_var_value();
    std::cout << "Solution value: " << std::setprecision( 20 )
     << solution_value << std::endl;
    first_solver->get_var_solution();
    std::cout << "Solution: " << std::endl;
    const auto & variables = investment_block->get_variables();
    const auto & var_lb = investment_block->get_variable_lower_bound();
    const auto width = std::to_string( variables.size() ).size();
    for( Index i = 0 ; i < variables.size() ; ++i ) {
     auto value = variables[ i ].get_value();
     if( investment_block->get_reformulate_bounds() && ( i < var_lb.size() ) &&
      ( var_lb[ i ] > -Inf< double >() ) )
      value += var_lb[ i ];
     std::cout << std::setw( width ) << i << " " << value << std::endl;
    }
   }
   else
    std::cout << "No solution has been found" << std::endl;
  }

  return( all_passed );
 }
 catch( std::exception & e ) {
  std::cerr << e.what() << std::endl;
  throw;
 }
}

/*--------------------------------------------------------------------------*/

void process_my_args( int argc , char ** argv ) {
 exe = get_filename( argv[ 0 ] );
 if( argc < 2 ) {
  std::cout << exe << ": no input file\n"
   << "Try " << exe << "' --help' for more information.\n";
  exit( 1 );
 }

 // Support the batch invocation:
 //   test <nc-file> <block-config> <solver-config> 0 <ref-objective>
 //
 // Semantics:
 // - load the InvestmentBlock from <nc-file>
 // - apply <block-config> as external BlockConfig
 // - apply <solver-config> as external BlockSolverConfig
 // - if a reference objective is provided, compare the first solver to it
 //
 // This branch is taken only when the first argument is not an option.
 if( argv[ 1 ][ 0 ] != '-' ) {
  filename = std::string( argv[ 1 ] );

  if( argc >= 3 )
   bconf_file = std::string( argv[ 2 ] );

  if( argc >= 4 )
   sconf_file = std::string( argv[ 3 ] );

  if( argc >= 6 )
   RefObjective = std::stod( argv[ 5 ] );

  bconf_file = resolve_with_prefix( conf_prefix , bconf_file );
  sconf_file = resolve_with_prefix( conf_prefix , sconf_file );

  return;
 }

 while( true ) { // options
  auto opt = getopt_long( argc , argv , short_opts.data() ,
                          long_opts.data() , nullptr );
  if( opt == -1 ) break;
  if( process_standard_arg( opt ) ) // if it is a standard one
   continue; // next

  switch( opt ) { // non-standard options
  case 'o' : RefObjective = std::stod( optarg );
   break;
  case 'x' : initial_point_filename = std::string( optarg );
   break;
  case '?' : // Unrecognized option
  default : std::cout << "Try " << exe << "' --help' for more information"
    << std::endl;
   exit( 1 );
  }
 } // end( while( true ) )

 if( optind < argc ) // last argument == [InvestmentBlock] filename
  filename = std::string( argv[ optind ] );
 else {
  std::cout << exe << ": no input file" << std::endl
   << "Try " << exe << "' --help' for more information" << std::endl;
  exit( 1 );
 }

 bconf_file = resolve_with_prefix( conf_prefix , bconf_file );
 sconf_file = resolve_with_prefix( conf_prefix , sconf_file );
} // end( process_my_args )

/*--------------------------------------------------------------------------*/

Block * get_uc_block( const SDDPBlock * sddp_block , Index stage ,
                      Index sub_block_index ) {
 auto benders_block = static_cast< BendersBlock * >(
  sddp_block->get_sub_Block( stage , sub_block_index )->get_inner_block() );

 auto objective = static_cast< FRealObjective * >(
  benders_block->get_objective() );

 auto benders_function = static_cast< BendersBFunction * >(
  objective->get_function() );
 return( benders_function->get_inner_block() );
}

/*--------------------------------------------------------------------------*/

bool update_hydro_unit( Block * previous_block , Block * block ,
                        Index stage ) {
 auto unit = dynamic_cast< HydroUnitBlock * >( block );
 auto previous_unit = dynamic_cast< HydroUnitBlock * >( previous_block );

 if( ( ! unit ) && ( ! previous_unit ) )
  return( false );

 if( ( ! unit ) || ( ! previous_unit ) )
  throw( std::logic_error( "test: UCBlocks at stages " +
   std::to_string( stage - 1 ) + " and " +
   std::to_string( stage ) +
   " do not have the same structure" ) );

 auto number_generators = previous_unit->get_number_generators();

 if( number_generators != unit->get_number_generators() )
  throw( std::logic_error( "test: HydroUnitBlock at stage " +
   std::to_string( stage - 1 ) + " has " +
   std::to_string( number_generators ) +
   ", but corresponding HydroUnitBlock at stage " +
   std::to_string( stage ) + " has " +
   std::to_string( unit->get_number_generators() )
  ) );

 const auto time_horizon = previous_unit->get_time_horizon();

 std::vector< double > flow_rate( number_generators );

 for( Index g = 0 ; g < number_generators ; ++g )
  flow_rate[ g ] =
   previous_unit->get_flow_rate( g , time_horizon - 1 )->get_value();

 unit->set_initial_flow_rate( flow_rate.cbegin() );

 return( true );
}

/*--------------------------------------------------------------------------*/

bool update_battery_unit( Block * previous_block , Block * block ,
                          Index stage ) {
 auto unit = dynamic_cast< BatteryUnitBlock * >( block );
 auto previous_unit = dynamic_cast< BatteryUnitBlock * >( previous_block );

 if( ( ! unit ) && ( ! previous_unit ) )
  return( false );

 if( ( ! unit ) || ( ! previous_unit ) )
  throw( std::logic_error( "test: UCBlocks at stages " +
   std::to_string( stage - 1 ) +
   " and " + std::to_string( stage ) +
   " do not have the same structure" ) );

 const auto time_horizon = previous_unit->get_time_horizon();

 std::vector< double > initial_power_data = {
  ( previous_unit->get_active_power( 0 ) + time_horizon - 1 )->get_value()
 };

 unit->set_initial_power( initial_power_data.cbegin() );

 std::vector< double > initial_storage_data = {
  previous_unit->get_storage_level()[ time_horizon - 1 ].get_value()
 };

 unit->set_initial_storage( initial_storage_data.cbegin() );

 return( true );
}

/*--------------------------------------------------------------------------*/

bool update_thermal_unit( Block * previous_block , Block * block ,
                          Index stage ) {
 auto previous_unit = dynamic_cast< ThermalUnitBlock * >( previous_block );
 auto unit = dynamic_cast< ThermalUnitBlock * >( block );

 if( ! unit && ! previous_unit )
  return( false );

 if( ! unit || ! previous_unit )
  throw( std::logic_error(
   "test: UCBlocks at stages " + std::to_string( stage - 1 ) +
   " and " + std::to_string( stage ) +
   " do not have the same structure." ) );

 const auto time_horizon = previous_unit->get_time_horizon();

 std::vector< double > active_power_data = {
  ( previous_unit->get_active_power( 0 ) + time_horizon - 1 )->get_value()
 };
 unit->set_initial_power( active_power_data.cbegin() );

 return( true );
}

/*--------------------------------------------------------------------------*/

void callback( SDDPBlock * sddp_block , Index stage , Index sub_block_index ) {
 if( stage == 0 )
  return;

 auto previous_uc_block = get_uc_block( sddp_block , stage - 1 ,
                                        sub_block_index );
 auto uc_block = get_uc_block( sddp_block , stage , sub_block_index );

 std::queue< Block * > blocks;
 blocks.push( uc_block );

 std::queue< Block * > previous_blocks;
 previous_blocks.push( previous_uc_block );

 while( ! blocks.empty() ) {
  auto block = blocks.front();
  blocks.pop();

  auto previous_block = previous_blocks.front();
  previous_blocks.pop();

  auto n = block->get_number_nested_Blocks();

  if( n != previous_block->get_number_nested_Blocks() )
   throw( std::logic_error( "test: UCBlocks at stages " +
    std::to_string( stage - 1 ) +
    " and " + std::to_string( stage ) +
    " do not have the same structure" ) );

  for( decltype( n ) i = 0 ; i < n ; ++i ) {
   blocks.push( block->get_nested_Block( i ) );
   previous_blocks.push( previous_block->get_nested_Block( i ) );
  }

  // the simulation passes to the next stage the volumes of the reservoirs,
  // and the state of the thermal and battery units as well
  if( ! update_hydro_unit( previous_block , block , stage ) )
   update_thermal_unit( previous_block , block , stage )
    || update_battery_unit( previous_block , block , stage );
 }
}

/*--------------------------------------------------------------------------*/

std::vector< double > get_default_initial_point( InvestmentBlock * block ) {
 block->generate_abstract_constraints();
 const auto & box_constraints = block->get_constraints();
 std::vector< double > initial_point( box_constraints.size() );
 for( Index i = 0 ; i < box_constraints.size() ; ++i )
  if( box_constraints[ i ].get_lhs() > -Inf< double >() )
   initial_point[ i ] = box_constraints[ i ].get_lhs();
  else if( box_constraints[ i ].get_rhs() < Inf< double >() )
   initial_point[ i ] = box_constraints[ i ].get_rhs();
  else
   initial_point[ i ] = 0;

 return( initial_point );
}

/*--------------------------------------------------------------------------*/

std::vector< double > load_initial_point( void ) {
 if( initial_point_filename.empty() )
  return {};

 std::ifstream file( initial_point_filename );

 // Make sure the file is open
 if( ! file.is_open() )
  throw( std::runtime_error( "It was not possible to open the file " +
   initial_point_filename ) );

 std::vector< double > initial_point;

 double component;
 while( file >> component )
  initial_point.push_back( component );

 return( initial_point );
}

/*--------------------------------------------------------------------------*/

void set_initial_point( InvestmentBlock * investment_block ) {
 // Generate the abstract variables so that we can set their values.

 investment_block->generate_abstract_variables();

 // Possibly load a given initial point.

 initial_point = load_initial_point();

 if( ! initial_point.empty() ) {
  // An initial point has been provided.

  const auto num_variables = investment_block->get_number_variables();
  if( initial_point.size() != num_variables )
   throw( std::logic_error( "The initial point has size " +
    std::to_string( initial_point.size() ) + ", but "
    "there are " + std::to_string( num_variables ) +
    " variables." ) );

  // the bounds are reformulated (or not) when the constraints are generated
  investment_block->generate_abstract_constraints();
  if( investment_block->get_reformulate_bounds() ) {
   // If variable bounds have been reformulated, the initial point must be
   // adjusted.

   const auto & var_lower_bound =
    investment_block->get_variable_lower_bound();
   for( Index i = 0 ; i < initial_point.size() ; ++i ) {
    if( ( i < var_lower_bound.size() ) &&
     ( var_lower_bound[ i ] > -Inf< double >() ) )
     initial_point[ i ] -= var_lower_bound[ i ];
   }
  }
 }
 else // Since no initial point has been provided, we use the default one.
  initial_point = get_default_initial_point( investment_block );

 // Finally, set the initial point.
 investment_block->set_variable_values( initial_point );
}

/*--------------------------------------------------------------------------*/

void set_log( SDDPBlock * sddp_block , std::ostream * output_stream ) {
 for( auto sub_block : sddp_block->get_nested_Blocks() ) {
  for( auto solver : sddp_block->get_registered_solvers() )
   if( solver )
    solver->set_log( output_stream );

  auto stochastic_block = static_cast< StochasticBlock * >( sub_block );
  auto benders_block = static_cast< BendersBlock * >(
   stochastic_block->get_nested_Blocks().front() );
  auto objective = static_cast< FRealObjective * >(
   benders_block->get_objective() );
  auto benders_function = static_cast< BendersBFunction * >(
   objective->get_function() );
  auto inner_block = benders_function->get_inner_block();

  for( auto solver : inner_block->get_registered_solvers() )
   if( solver )
    solver->set_log( output_stream );
 }
}


/*--------------------------------------------------------------------------*/
/// configures the inner Block of the InvestmentFunction
/** The inner Block of the InvestmentFunction, and the UCBlock of every stage
 * of an SDDPBlock, which sits behind a BendersBFunction, are out of reach of
 * the nested-Block BFS started at the InvestmentBlock: the given
 * "meta"-BlockConfig is dispatched to each of them here. */

void configure_inner_Blocks( InvestmentFunction * investment_function ,
                             Configuration * block_config ,
                             const std::string & name ) {
 for( auto block : investment_function->get_nested_Blocks() )
  if( auto sddp_block = dynamic_cast< SDDPBlock * >( block ) ) {
   for( Index t = 0 ; t < sddp_block->get_time_horizon() ; ++t )
    for( Index j = 0 ; j < sddp_block->get_num_sub_blocks_per_stage() ; ++j )
     b_config_Block( get_uc_block( sddp_block , t , j ) , block_config ,
                     name );
   }
  else
   b_config_Block( block , block_config , name );
}

/*--------------------------------------------------------------------------*/
/// checks that the InvestmentFunction has been given its Solver
/** The Solver of the inner Block of the InvestmentFunction come with the
 * ComputeConfig of the InvestmentFunction, which the OBlockConfig of the
 * InvestmentBlock gives to its Objective (see IBOCfg.txt). When the inner
 * Block is an SDDPBlock, the SDDPGreedySolver so registered, which simulate
 * the scenarios one stage after the other, are each given the callback()
 * that passes the final state of a stage to the next. */

void check_inner_Solvers( InvestmentFunction * investment_function ) {
 for( auto block : investment_function->get_nested_Blocks() ) {
  if( block->get_registered_solvers().empty() ) {
   std::cerr << "error: the inner Block of the InvestmentFunction has no "
             << "Solver: the BlockConfig of the InvestmentBlock must be an "
             << "OBlockConfig giving its InvestmentFunction a ComputeConfig "
             << "with a BlockSolverConfig (see IBOCfg.txt)" << std::endl;
   exit( 1 );
   }

  if( auto sddp_block = dynamic_cast< SDDPBlock * >( block ) )
   for( auto solver : sddp_block->get_registered_solvers() )
    if( auto greedy = dynamic_cast< SDDPGreedySolver * >( solver ) ) {
     const auto sub_block_index = Index( greedy->get_int_par(
                                     SDDPGreedySolver::intSubBlockIndex ) );
     greedy->set_callback( [ sddp_block , sub_block_index ]( Index stage ) {
                            callback( sddp_block , stage , sub_block_index );
                            } );
     }
  }
}

/*--------------------------------------------------------------------------*/

void process_prob_file( const netCDF::NcFile & file ) {
 // the inner Block, which the BlockConfig of the problem does not reach,
 // takes the -B "meta"-BlockConfig, if any
 Configuration * inner_block_config = nullptr;
 if( ! bconf_file.empty() ) {
  inner_block_config = Configuration::deserialize( bconf_file );
  if( ! inner_block_config ) {
   std::cerr << "error: cannot load BlockConfig " << bconf_file << std::endl;
   exit( 1 );
  }
 }

 auto problems = file.getGroups();

 for( auto & problem : problems ) { // for each problem descriptor:
  auto & problem_group = problem.second;

  // Deserialize the Block
  auto block_group = problem_group.getGroup( "Block" );
  auto block_type_att = block_group.getAtt( "type" );

  if( block_type_att.isNull() ) {
   std::cerr << "Attribute 'type' not found in the netCDF group "
    << block_group.getName() << std::endl;
   exit( 1 );
  }

  std::string block_type;
  block_type_att.getValues( block_type );

  if( block_type != "InvestmentBlock" ) {
   std::cerr << "The Block in the netCDF file " << block_type << " is "
    << block_type << ", but it must be an InvestmentBlock"
    << std::endl;
   exit( 1 );
  }

  auto investment_block = dynamic_cast< InvestmentBlock * >(
   Block::new_Block( block_group , nullptr ) );
  assert( investment_block );

  auto investment_function = static_cast< InvestmentFunction * >(
   investment_block->get_function() );

  // Configure the inner Block, then the InvestmentBlock, whose BlockConfig
  // gives the InvestmentFunction the Solver of the inner Block
  if( inner_block_config )
   configure_inner_Blocks( investment_function , inner_block_config ,
                           bconf_file );

  auto block_config_group = problem_group.getGroup( "BlockConfig" );
  auto block_config = dynamic_cast< BlockConfig * >(
   BlockConfig::new_Configuration( block_config_group ) );
  if( ! block_config )
   throw( std::logic_error( "BlockConfig group was not properly provided" ) );
  // the OBlockConfig gives its ComputeConfig to the Objective, which has to
  // be there already; the constraints wait for the BlockConfig, which says
  // whether the bounds are reformulated
  investment_block->generate_abstract_variables();
  investment_block->generate_objective();
  block_config->apply( investment_block );
  block_config->clear();

  check_inner_Solvers( investment_function );

  // Possibly set the initial point
  set_initial_point( investment_block );

  // Configure solver
  auto solver_config_group = problem_group.getGroup( "BlockSolver" );
  auto block_solver_config = dynamic_cast< BlockSolverConfig * >(
   BlockSolverConfig::new_Configuration( solver_config_group ) );
  if( ! block_solver_config )
   throw( std::logic_error( "BlockSolver group was not properly provided" ) );
  block_solver_config->apply( investment_block );
  block_solver_config->clear();

  std::cout << "Problem: " << problem.first << std::endl;

  // Set the output stream for the log of the inner Solvers

  for( auto block : investment_function->get_nested_Blocks() )
   if( auto sddp_block = dynamic_cast< SDDPBlock * >( block ) )
    set_log( sddp_block , &std::cout );
   else
    for( auto solver : block->get_registered_solvers() )
     if( solver )
      solver->set_log( &std::cout );

  // Solve
  AllPassed &= test_investment_solvers( investment_block );

  // Destroy the Block and the Configurations

  block_config->apply( investment_block );
  delete block_config;

  block_solver_config->apply( investment_block );
  delete block_solver_config;

  delete investment_block;
 }

 delete inner_block_config;
}

/*--------------------------------------------------------------------------*/

void process_block_file( const netCDF::NcFile & file ) {
 auto blocks = file.getGroups();

 // the BlockConfig: an OBlockConfig for the InvestmentBlock, or a
 // "meta"-BlockConfig with one [see InnerBCfg.txt]
 if( bconf_file.empty() ) {
  std::cerr << "error: a BlockConfig for the InvestmentBlock must be given "
            << "(see InnerBCfg.txt)" << std::endl;
  exit( 1 );
 }
 auto given_block_config = Configuration::deserialize( bconf_file );
 if( ! given_block_config ) {
  std::cerr << "error: cannot load BlockConfig " << bconf_file << std::endl;
  exit( 1 );
 }

 // Load BlockSolverConfig from file
 Configuration * solver_config = Configuration::deserialize( sconf_file );
 if( ! solver_config ) {
  std::cerr << "error: cannot load BlockSolverConfig " << sconf_file
            << std::endl;
  exit( 1 );
 }

 // For each Block descriptor
 for( auto block_description : blocks ) {
  // Deserialize the Block
  auto block_type_att = block_description.second.getAtt( "type" );
  if( block_type_att.isNull() ) {
   std::cerr << "The netCDF attribute 'type' was not found in the netCDF "
             << "group " << block_description.second.getName() << std::endl;
   exit( 1 );
  }

  std::string block_type;
  block_type_att.getValues( block_type );

  if( block_type != "InvestmentBlock" ) {
   std::cerr << "The Block in the netCDF file " << block_type << " is "
             << block_type << ", but it must be an InvestmentBlock"
             << std::endl;
   exit( 1 );
  }

  auto investment_block = dynamic_cast< InvestmentBlock * >(
   Block::new_Block( block_description.second , nullptr ) );

  // deserialize gives up, having said why, when a type it needs is not in the
  // Block factory, which is what happens when that type is not linked in
  if( ! investment_block ) {
   std::cerr << "error: cannot build the InvestmentBlock of " << filename
             << std::endl;
   exit( 1 );
   }

  auto investment_function = static_cast< InvestmentFunction * >(
   investment_block->get_function() );

  // Configure the inner Block, then the InvestmentBlock, whose (O)BlockConfig
  // gives the InvestmentFunction the Solver of the inner Block: a plain
  // BlockConfig only concerns the InvestmentBlock, while a "meta"-BlockConfig
  // is also dispatched inside the InvestmentFunction, since the nested-Block
  // BFS cannot cross the Function boundary
  if( ! dynamic_cast< BlockConfig * >( given_block_config ) )
   configure_inner_Blocks( investment_function , given_block_config ,
                           bconf_file );
  // the OBlockConfig gives its ComputeConfig to the Objective, which has to
  // be there already; the constraints wait for the BlockConfig, which says
  // whether the bounds are reformulated
  investment_block->generate_abstract_variables();
  investment_block->generate_objective();
  b_config_Block( investment_block , given_block_config , bconf_file );

  check_inner_Solvers( investment_function );

  // Possibly set the initial point
  set_initial_point( investment_block );

  // Finally, apply the Solver configuration
  s_config_Block( investment_block , solver_config , sconf_file );

  if( investment_block->get_registered_solvers().empty() ) {
   std::cerr << "Error: no Solver registered to InvestmentBlock"
             << std::endl;
   exit( 1 );
  }

  // Set the output stream for the log of the inner Solvers
  for( auto block : investment_function->get_nested_Blocks() )
   if( auto sddp_block = dynamic_cast< SDDPBlock * >( block ) )
    set_log( sddp_block , &std::cout );
   else
    for( auto solver : block->get_registered_solvers() )
     if( solver )
      solver->set_log( &std::cout );

  // Solve
  AllPassed &= test_investment_solvers( investment_block );

  // Cleanup solver attachments/configurations
  s_config_Block( investment_block , solver_config );

  delete investment_block;
 }

 delete given_block_config;
 delete solver_config;
}

/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv ) {
 // override the default terminate handler to print the exception message
 std::set_terminate( smspp_terminate );

 // append new options to default ones- - - - - - - - - - - - - - - - - - - -
 // note that the local options are inserted right before the last (nullptr)
 // record in long_opts

#ifdef USE_MPI
 boost::mpi::environment env( argc , argv );
#endif

 docopt_desc = "SMS++ investment solver\n";
 short_opts.append( my_short_opts );
 long_opts.insert( std::prev( long_opts.end() ) ,
                   my_long_opts.begin() , my_long_opts.end() );
 help.append( my_help );

 // process command-line arguments- - - - - - - - - - - - - - - - - - - - - -

 process_my_args( argc , argv );

 // open the file - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 // the BlockSolverConfig (-S) must be provided explicitly: the test never
 // falls back to a hardcoded default Configuration
 require_solver_config();

 netCDF::NcFile file;
 auto type = read_open_netCDF( file , filename );

 // process the file- - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 switch( type ) {
 case eProbFile : std::cout << filename << " is a problem file, "
   << "ignoring Block/Solver Configuration(s)..."
   << std::endl;
  process_prob_file( file );
  break;
 case eBlockFile : std::cout << filename << " is a Block file" << std::endl;
  process_block_file( file );
  break;
 default : std::cerr << filename << " is not a valid SMS++ file"
   << std::endl;
  exit( 1 );
 }

 return( AllPassed ? 0 : 1 );
} // end( main )

/*--------------------------------------------------------------------------*/
/*------------------------ End File test.cpp -------------------------------*/
/*--------------------------------------------------------------------------*/
