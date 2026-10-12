/*--------------------------------------------------------------------------*/
/*------------------------- File stochastic_test.h -------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * The main of the testers of the stochastic Block, i.e., TwoStageStochastic
 * Block and MultiStageStochasticBlock, which differ only in the class of the
 * Block they read and in what they do to it or check of it on top.
 *
 * An instance is loaded from a netCDF file once per variant of its
 * BlockConfig [see get_BlockConfig_variants()], i.e., once if -B or no
 * BlockConfig is given; the (meta-)BlockConfig is applied by classname over
 * the whole tree, a BlockSolverConfig registers one or more Solver to the
 * copy and they are solved and compared: with one copy, between the two
 * Solver if two are registered, else against the reference objective value
 * of -r, every Solver being read by its lower bound unless -R declares the
 * Solver that solve a relaxation, in which case all of them are read as the
 * interval of their bounds [see SolveAll()]; with several copies, as
 * SolveVariants() does, the copies being the same problem.
 *
 * \author Antonio Frangioni \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \author Donato Meoli \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Antonio Frangioni, Donato Meoli
 */
/*--------------------------------------------------------------------------*/
/*----------------------------- DEFINITIONS --------------------------------*/
/*--------------------------------------------------------------------------*/

#ifndef __stochastic_test
 #define __stochastic_test

/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <functional>
#include <string>
#include <vector>

#include <getopt.h>

#include "Block.h"

/*--------------------------------------------------------------------------*/

namespace SMSpp_di_unipi_it {

/// what a tester of a stochastic Block says of it
/** The class of the Block it reads, for the messages, how to tell that the
 * Block read is one, and what it adds to the common part: options of its
 * own, a Block that the Solver are attached to in place of the one read,
 * and a check of each copy after the Solver have run. */

struct StochasticTester {
 std::string classname;                        ///< e.g. "TwoStageStochasticBlock"
 std::function< bool( Block * ) > is_one;      ///< the Block read is one
 std::string short_opts;                       ///< its own options
 std::vector< option > long_opts;              ///< and their long form
 std::string help;                             ///< and their help
 std::function< bool( int ) > handle;          ///< handles one of them
 std::function< Block * ( Block * ) > transform = nullptr;
 ///< the Block the Solver are attached to, nullptr to give up; not given,
 ///< the Block read
 std::function< bool( Block * ) > check = nullptr;
 ///< a check of each copy after the Solver have run, if any
 };

/// the main of a tester of a stochastic Block, returning its exit code
/** -w and -r are the options of all of them: the reading of the second
 * Solver (0 its lower bound, 1 its upper bound, e.g., of a
 * PrimalProximalHeur) and the reference objective value. */

int stochastic_test( int argc , char ** argv , StochasticTester & t );

}  // end( namespace SMSpp_di_unipi_it )

#endif  /* stochastic_test.h included */

/*--------------------------------------------------------------------------*/
/*----------------------- End File stochastic_test.h -----------------------*/
/*--------------------------------------------------------------------------*/
