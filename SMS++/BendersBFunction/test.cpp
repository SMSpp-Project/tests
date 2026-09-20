/*--------------------------------------------------------------------------*/
/*----------------------------- File test.cpp ------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * This file contains the implementation of a test for the
 * BendersBFunction. The test consists in solving relaxations of the
 * Capacitated Warehouse Location (CWL) problem by Benders
 * decomposition. BundleSolver is used to solve the master problem and
 * *MILPSolver is used to solve the inner problem. The program requires as
 * argument the path to a directory containing instances of the CWL
 * problem. Each file in that directory is assumed to contain an instance of
 * the CWL problem, except if they are named manual.txt or readme.txt. A file
 * containing an instance of the CWL problem must have the following
 * format. The first line must contain the number of locations (L) and the
 * number of customers (C) (in that order). Each of the next L lines is
 * associated with one location and must contain the capacity of that location
 * and the fixed cost of opening a warehouse at that location. Next, there
 * must be C blocks of lines, each of them associated with a customer and
 * containing L+1 lines. The i-th of these blocks must contain the demand of
 * the i-th customer followed by L lines, the j-th one containing the unit
 * cost of serving customer j by the warehouse at location i.
 *
 * \author Rafael Durbano Lobato \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 * 
 * \author Enrico Calandrini \n
 *         Dipartimento di Matematica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Rafael Durbano Lobato
 */

/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include "common_utils.h"

#include "AbstractBlock.h"
#include "CWLAbstractBlockBuilder.h"

#include "cwl-mcf/cwl-mcf.h"

#include <chrono>
#include <iostream>
#include <iomanip>

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;
using namespace SMSpp_di_unipi_it::tests;

/*--------------------------------------------------------------------------*/
/*-------------------------- AUXILIARY TYPES -------------------------------*/
/*--------------------------------------------------------------------------*/

enum SolverType { MILPSolver , BundleSolver };

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

BlockSolverConfig * build_solver_config
( const std::string & config_file_path ) {

 std::ifstream config_file( config_file_path );
 if( ! config_file.is_open() )
  throw( std::invalid_argument( "BendersBFunction test: Error: cannot open "
                                "configuration file " + config_file_path ) );

 auto bsc = new BlockSolverConfig;
 config_file >> ( * bsc );
 config_file.close();
 return( bsc );
}

/*--------------------------------------------------------------------------*/

int solve_with_BundleSolver( std::filesystem::path file_path ,
                             bool continuous_relaxation ,
                             double * solution_value ) {
 auto inner_block = new AbstractBlock();

 // Add a *MILPSolver to inner_block; BSC may be a plain BlockSolverConfig
 // or a meta-config SimpleConfiguration< std::map< std::string ,
 // Configuration * > >, dispatched by s_config_Block().
 std::string lpbsc_fn = "LPPar_innerBlock.txt";
 Configuration * lpbsc = Configuration::deserialize( lpbsc_fn );
 if( ! lpbsc ) {
  std::cerr << "Error: cannot load BSC from " << lpbsc_fn << std::endl;
  exit( 1 );
  }
 s_config_Block( inner_block , lpbsc , lpbsc_fn );

 auto inner_block_solver = ( inner_block->get_registered_solvers() ).front();
 //inner_block_solver->set_par( MILPSolver::strOutputFile , "lp.txt" )

 auto block = build_CWL_block_with_Benders_decomposition
   ( file_path , continuous_relaxation , inner_block_solver );

 // Configuring the CWL Block to produce RowConstraintSolution
 BlockConfig block_config;
 block_config.f_solution_Configuration = new SimpleConfiguration< int >( 1 );
 block_config.apply( block );

 // Solver configuration; pass through s_config_Block() so a meta-config
 // SimpleConfiguration< std::map< std::string , Configuration * > > would
 // also be supported should build_solver_config() be generalised in the
 // future to read one.
 auto block_solver_config = build_solver_config( "BundlePar-cwl.txt" );
 s_config_Block( block , block_solver_config , "BundlePar-cwl.txt" );

 auto solver = block->get_registered_solvers().front();

 auto status = solver->compute();

 if( solver->has_var_solution() )
  *solution_value = solver->get_var_value();

 s_config_Block( block , block_solver_config );
 delete( block_solver_config );
 delete( block );
 return( status );
}

/*--------------------------------------------------------------------------*/

int solve_with_MILPSolver( std::filesystem::path file_path ,
                           bool continuous_relaxation ,
                           double * solution_value ) {
 auto block = build_CWL_block( file_path , continuous_relaxation );

 // Add a *MILPSolver to block; BSC may be a plain BlockSolverConfig or a
 // meta-config SimpleConfiguration< std::map< std::string ,
 // Configuration * > >, dispatched by s_config_Block().
 std::string lpbsc_fn = "LPPar.txt";
 Configuration * lpbsc = Configuration::deserialize( lpbsc_fn );
 if( ! lpbsc ) {
  std::cerr << "Error: cannot load BSC from " << lpbsc_fn << std::endl;
  exit( 1 );
  }
 s_config_Block( block , lpbsc , lpbsc_fn );

 auto solver = ( block->get_registered_solvers() ).front();
 auto status = solver->compute();
 if( solver->has_var_solution() )
  *solution_value = solver->get_var_value();
 delete( block );
 return( status );
}

/*--------------------------------------------------------------------------*/

void compare( std::string data_dir_path ,
              SolverType solver_type = SolverType::BundleSolver ,
              double epsilon = 1.0e-6 ) {

 const bool continuous_relaxation = true;

 for( const auto & file :
       std::filesystem::directory_iterator( data_dir_path ) ) {

  auto file_path = file.path();

  if( file_path.filename() == "manual.txt" ||
      file_path.filename() == "readme.txt" )
   continue;

  double solution_value = 0;
  int status;

  std::cout << "Solving instance " << file_path.filename() << ": ";
  std::flush( std::cout );

  auto start = std::chrono::system_clock::now();
  if( solver_type == SolverType::MILPSolver )
   status = solve_with_MILPSolver( file_path , continuous_relaxation ,
                                   & solution_value );
  else if( solver_type == SolverType::BundleSolver )
   status = solve_with_BundleSolver( file_path , continuous_relaxation ,
                                     & solution_value );
  else {
   std::cerr << "\nUnknown Solver type: " << solver_type << std::endl;
   exit( 1 );
  }
  auto end = std::chrono::system_clock::now();
  double t = std::chrono::duration< double >( end - start ).count();

  // uniform per-instance line: the Solver value (S0) against the cwl-mcf
  // reference; the "Solving instance <name>: " prefix above plays the role of
  // the batch prefix
  double ref = std::numeric_limits< double >::quiet_NaN();
  std::string tok , verdict;
  if( status != ThinComputeInterface::kOK ) {
   tok = "Error!"; verdict = "KO";
   }
  else {
   ref = cwl_mcf( file_path.string() );
   auto diff = std::abs( solution_value - ref );
   auto max_diff = std::max( epsilon , epsilon *
                             std::min( abs( solution_value ) , abs( ref ) ) );
   tok = fmt_obj( solution_value );
   verdict = ( diff > max_diff ) ? "KO" : "OK";
   }

  print_instance_line( { t } , { tok } , ref , verdict );
 }
}


int main( int argc , char ** argv )
{
 // override the default terminate handler to print the exception message
 std::set_terminate( smspp_terminate );

 // reading command line parameters - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 if( argc < 2 ) {
  std::cerr << "The path to the directory containing the instance files " <<
   "must be provided as argument." << std::endl;
  std::cerr << "Usage: " << argv[ 0 ] << " PATH" << std::endl;
  return( 1 );
 }

 std::string path = argv[ 1 ];

 std::cout << "***** LP formulation test *****" << std::endl;
 compare( path , SolverType::MILPSolver );

 std::cout << "***** Benders decomposition test *****" << std::endl;
 compare( path , SolverType::BundleSolver );

 return( 0 );
}

/*--------------------------------------------------------------------------*/
/*--------------------------- End File test.cpp ----------------------------*/
/*--------------------------------------------------------------------------*/
