/*--------------------------------------------------------------------------*/
/*-------------------------- File test.cpp ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Main for testing CSPBlock.
 *
 * A constraint satisfaction or optimization problem, written in FlatZinc,
 * is loaded into a CSPBlock, a BlockSolverConfig is applied that registers
 * to it the Solver to be compared, and SolveAll() cross-checks them against
 * one another and, if given with -r, against the optimum of the instance
 * (0 for a satisfiable one, which has no objective).
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
                          "SMS++ CSPBlock test.\n" ,
                          "CSPBlock" , false ) );
 }

/*--------------------------------------------------------------------------*/
/*-------------------------- End File test.cpp -----------------------------*/
/*--------------------------------------------------------------------------*/
