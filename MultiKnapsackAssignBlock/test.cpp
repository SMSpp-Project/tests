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
/*--------------------------------- MAIN -----------------------------------*/
/*--------------------------------------------------------------------------*/
// all is done by load_solve_test() of common_utils, the options being those
// it handles (the instance positional, -S / -F / -B / -c / -p / -D / -v / -E
// / -R and -r, the optimum of the instance)

int main( int argc , char ** argv )
{
 return( load_solve_test( argc , argv ,
                          "SMS++ MultiKnapsackAssignBlock test.\n" ,
                          "MultiKnapsackAssignBlock" , false ) );
 }

/*--------------------------------------------------------------------------*/
/*-------------------------- End File test.cpp -----------------------------*/
/*--------------------------------------------------------------------------*/
