/*--------------------------------------------------------------------------*/
/*----------------------- File test_multiflow.cpp --------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Tester of MultiFlowDCRBlock.
 *
 * Reads a multi-flow Delay-Constrained Routing instance out of a netCDF
 * file, i.e., a set of flows each of which is a SingleFlowDCRBlock, tied by
 * the capacity of the arcs they share, hands it to all the Solver that the
 * BlockSolverConfig registers to it and cross-checks what they answer with
 * SolveAll(). Each Solver is read as the interval [ get_lb() , get_ub() ]
 * that contains the optimum, hence the exact ones, e.g., a :MILPSolver on
 * the formulation that holds all the flows, and the relaxations, e.g., a
 * LagrangianDualSolver that relaxes the shared capacities and solves one
 * SingleFlowDCRBlock per flow, whose bound is at most the optimum, are
 * mixed freely, each being held to what it declares [see @ref solver_eps].
 *
 * The Lagrangian dual of the DCR problem is not tight in general, hence a
 * relaxation that is declared exact on an instance only is by its -E
 * tolerance of that instance; what has to hold on every instance is that the
 * bounds of two Lagrangian Solver that differ only by how they solve the
 * flows are the same, and at most the optimum.
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

#include "common_utils.h"

#include "MultiFlowDCRBlock.h"

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

using c_double = const double;

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/
/// no option of its own, the instance being always read from a file

static bool process_specific_arg( int opt )
{
 return( false );
 }

/*--------------------------------------------------------------------------*/
/*-------------------------------- main() ----------------------------------*/
/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 std::set_terminate( smspp_terminate );

 docopt_desc = "SMS++ MultiFlowDCRBlock test.\n";

 process_args( argc , argv , process_specific_arg );
 require_solver_config();

 // the Block, out of the given netCDF file - - - - - - - - - - - - - - - - -

 auto block = dynamic_cast< MultiFlowDCRBlock * >(
                                        Block::deserialize( filename ) );
 if( ! block ) {
  std::cerr << "Error: " << filename
            << " does not contain a MultiFlowDCRBlock" << std::endl;
  exit( 1 );
  }

 // configure it- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 if( ! bconf_file.empty() )
  b_config_Block( block , Configuration::deserialize( bconf_file ) ,
                  bconf_file );

 auto sconf = Configuration::deserialize( sconf_file );
 s_config_Block( block , sconf , sconf_file );

 // solve it and cross-check what the Solver say - - - - - - - - - - - - - -

 c_double tol = 1e-5;
 bool ok = SolveAll( block , RefObjective , tol );

 // clean up- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 s_config_Block( block , sconf );  // sconf has been clear()-ed before
 delete( sconf );
 delete( block );

 return( ok ? 0 : 1 );

 }  // end( main )

/*--------------------------------------------------------------------------*/
/*--------------------- End File test_multiflow.cpp ------------------------*/
/*--------------------------------------------------------------------------*/
