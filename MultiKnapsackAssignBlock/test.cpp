/*--------------------------------------------------------------------------*/
/*-------------------------- File test.cpp ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Main for testing MultiKnapsackAssignBlock.
 *
 * An instance of the Multiple Knapsack Assignment Problem is loaded into a
 * MultiKnapsackAssignBlock, a BlockSolverConfig is applied that registers
 * to it the Solver to be compared, and SolveAll() cross-checks them against
 * one another and, if given with -r, against the optimum of the instance.
 * The Solver are typically a :MILPSolver on the abstract representation of
 * the whole tree and a LagrangianDualSolver relaxing the constraints that
 * link the BinaryKnapsackBlock sub-Block, whose sub-problems are solved by
 * a Solver of BinaryKnapsackBlock; the Lagrangian dual of this integer
 * problem is a relaxation, which -R declares.
 *
 * \author Donato Meoli \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Donato Meoli
 */
/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <iostream>

#include "common_utils.h"

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

// the options of this test, on top of those common_utils handles (the
// instance positional and -S / -c / -p / -D / -v / -E / -R):
//   -r / --ref   : the optimum of the instance, to compare against

static bool process_specific_arg( int opt )
{
 switch( opt ) {
  case( 'r' ): Str2Sthg( optarg , RefObjective );     return( true );
  default:                                            return( false );
  }
 }

/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 std::set_terminate( smspp_terminate );

 docopt_desc = "SMS++ MultiKnapsackAssignBlock test.\n";
 short_opts += "r:";
 const std::vector< option > my_opts = {
   { "ref" , required_argument , nullptr , 'r' } };
 long_opts.insert( std::prev( long_opts.end() ) ,
                   my_opts.begin() , my_opts.end() );
 help += "  -r, --ref <value>               the optimum of the instance "
         "[none]\n";

 process_args( argc , argv , process_specific_arg );
 require_solver_config();

 auto block = Block::new_Block( "MultiKnapsackAssignBlock" );
 if( ! block ) {
  std::cerr << "Error: MultiKnapsackAssignBlock not present in Block factory"
            << std::endl;
  exit( 1 );
  }

 block->load( filename );

 Configuration * bsc = Configuration::deserialize( sconf_file );
 if( ! bsc ) {
  std::cerr << "Error: cannot load " << sconf_file << std::endl;
  delete block;
  exit( 1 );
  }

 s_config_Block( block , bsc , sconf_file , false );

 if( block->get_registered_solvers().empty() ) {
  // none of the Solver it names is in this build: nothing to do
  delete bsc;
  delete block;
  return( 77 );
  }

 bool ok = SolveAll( block , RefObjective , 1e-6 , nullptr , nullptr ,
                     nullptr , nullptr ,
                     dynamic_cast< BlockSolverConfig * >( bsc ) );

 // detach the Solver by applying the cleared BlockSolverConfig, then free
 bsc->clear();
 s_config_Block( block , bsc , sconf_file );
 delete bsc;
 delete block;

 return( ok ? 0 : 1 );
 }

/*--------------------------------------------------------------------------*/
/*-------------------------- End File test.cpp -----------------------------*/
/*--------------------------------------------------------------------------*/
