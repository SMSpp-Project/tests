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

#include "AbstractBlock.h"
#include "BlockSolverConfig.h"
#include "CWLAbstractBlockBuilder.h"

#include "cwl-mcf/cwl-mcf.h"

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
  throw std::invalid_argument( "BendersBFunction test: Error: cannot open "
                               "configuration file " + config_file_path );

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

 // Add a *MILPSolver to inner_block
  BlockSolverConfig * lpbsc;
  {
   auto c = Configuration::deserialize( "LPPar_innerBlock.txt" );
   lpbsc = dynamic_cast< BlockSolverConfig * >( c );
   if( ! lpbsc ) {
    std::cerr << "Error: LPPar_innerBlock.txt does not contain a BlockSolverConfig" << std::endl;
    delete( c );
    exit( 1 );
    }
   }

 lpbsc->apply( inner_block );
 lpbsc->clear();

 auto inner_block_solver = (inner_block->get_registered_solvers()).front();
 //inner_block_solver->set_par( MILPSolver::strOutputFile , "lp.txt" )

 auto block = build_CWL_block_with_Benders_decomposition
   ( file_path , continuous_relaxation , inner_block_solver );

 // Configuring the CWL Block to produce RowConstraintSolution
 BlockConfig block_config;
 block_config.f_solution_Configuration = new SimpleConfiguration< int >( 1 );
 block_config.apply( block );

 // Solver configuration
 auto block_solver_config = build_solver_config( "BundlePar-cwl.txt" );
 block_solver_config->apply( block );
 block_solver_config->clear();

 auto solver = block->get_registered_solvers().front();

 auto status = solver->compute();

 if( solver->has_var_solution() )
  *solution_value = solver->get_var_value();

 block_solver_config->apply( block );
 delete( block_solver_config );
 delete( block );
 return( status );
}

/*--------------------------------------------------------------------------*/

int solve_with_MILPSolver( std::filesystem::path file_path ,
                           bool continuous_relaxation ,
                           double * solution_value ) {
 auto block = build_CWL_block( file_path , continuous_relaxation );

 // Add a *MILPSolver to block
  BlockSolverConfig * lpbsc;
  {
   auto c = Configuration::deserialize( "LPPar.txt" );
   lpbsc = dynamic_cast< BlockSolverConfig * >( c );
   if( ! lpbsc ) {
    std::cerr << "Error: LPPar.txt does not contain a BlockSolverConfig" << std::endl;
    delete( c );
    exit( 1 );
    }
   }

 lpbsc->apply( block );
 lpbsc->clear();

 auto solver = (block->get_registered_solvers()).front();
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

  if( status != ThinComputeInterface::kOK )
   std::cout << "FAILED" << std::endl;
  else {
   auto cwl_mcf_value = cwl_mcf( file_path.string() );
   auto diff = std::abs( solution_value - cwl_mcf_value );
   auto max_diff = std::max( epsilon , epsilon *
                             std::min( abs( solution_value ),
                                       abs( cwl_mcf_value ) ) );

   if( diff > max_diff ) {
    std::cout << "FAILED" << std::endl;
    std::cout << "  Solution found:    " << std::setprecision( 20 )
              << solution_value << std::endl;
    std::cout << "  Expected solution: " << std::setprecision( 20 )
              << cwl_mcf_value << std::endl;
   }
   else
    std::cout << "OK" << std::endl;
  }
 }
}

/*--------------------------------------------------------------------------*/

int main( int argc, char ** argv ) {

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
