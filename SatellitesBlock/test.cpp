/*--------------------------------------------------------------------------*/
/*-------------------------- File test.cpp ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Main for testing the Block of SatellitesBlock.
 *
 * An instance of the Satellite Constellation Design Problem is loaded into
 * the Block whose classname() is given with -b, i.e., ConstellationBlock or
 * DiscreteConstellationBlock (with the SatelliteBlock, resp.
 * DiscreteSatelliteBlock, of the satellites as sub-Block) or
 * MultiTargetBlock (with the SingleTargetBlock of the targets as sub-Block),
 * a BlockSolverConfig is applied that registers to it the Solver to be
 * compared, and SolveAll() cross-checks them against one another and, if
 * given with -r, against the optimum of the instance. The Solver are
 * typically a :MILPSolver on the abstract representation of the whole tree
 * and a LagrangianDualSolver relaxing the constraints that link the
 * sub-Block, whose sub-problems are solved by the ad hoc SatelliteSolver or
 * DiscreteSatelliteSolver or by a :MILPSolver; the Lagrangian dual of these
 * integer problems is a relaxation, which -R declares.
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
/*--------------------------------- MAIN -----------------------------------*/
/*--------------------------------------------------------------------------*/
// all is done by load_solve_test() of common_utils, the options being those
// it handles (the instance positional, -S / -F / -B / -c / -p / -D / -v / -E
// / -R and -r, the optimum of the instance, and -b, the classname() of the
// Block the instance is loaded into)

int main( int argc , char ** argv )
{
 return( load_solve_test( argc , argv , "SMS++ SatellitesBlock test.\n" ,
                          "ConstellationBlock" , true ) );
 }

/*--------------------------------------------------------------------------*/
/*-------------------------- End File test.cpp -----------------------------*/
/*--------------------------------------------------------------------------*/
