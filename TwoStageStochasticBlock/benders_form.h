/*--------------------------------------------------------------------------*/
/*-------------------------- File benders_form.h ---------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * The Benders form of a two-stage stochastic instance, assembled around the
 * Block the file gives.
 *
 * An instance written in its extensive form has the here-and-now Variable
 * replicated in every scenario and tied by the non-anticipativity Constraint
 * of the stochastic Block, while BendersDecompositionSolver asks for them in
 * a single copy in the root, for one sub-Block per subproblem, and for the
 * coupling written as Constraint of the sub-Block where those Variable
 * appear linearly. No file format carries the latter: AbstractBlock only
 * deserializes a .lp/.mps model, and a model in a file of its own cannot
 * name the Variable of a sub-Block, hence the shape is assembled here.
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

#ifndef __TESTS_BENDERS_FORM
 #define __TESTS_BENDERS_FORM

/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include "AbstractBlock.h"

#include "TwoStageStochasticBlock.h"

/*--------------------------------------------------------------------------*/
/*------------------------------- FUNCTIONS --------------------------------*/
/*--------------------------------------------------------------------------*/

/// assemble the Benders form of @p tssb around it
/** Returns a new AbstractBlock in which:
 *
 * - the here-and-now AbstractPath of @p tssb resolve, in each leaf, to the
 *   design Variable of that leaf; one copy of them goes in the root, bounded
 *   as the leaves bound theirs;
 *
 * - each leaf goes under a wrapper AbstractBlock whose Constraint say that
 *   the leaf uses at most what the root buys, i.e., x^l - x <= 0, a leaf not
 *   letting a Constraint be added to it from the outside; the wrapper is the
 *   subproblem, and the leaf keeps its data, its Objective and its weight;
 *
 * - the cost of the design is moved from the leaves to the root: a leaf that
 *   pays for the capacity it is given has a value function that is not
 *   monotone in it, and the coupling duals the master reads its cuts from
 *   lose the sign a capacity coupling has.
 *
 * What comes out is the same problem, hence a :MILPSolver attached to it
 * reads the extensive form and is the reference of any cross-check on it.
 * The leaves of @p tssb become sub-Block of the wrappers while @p tssb still
 * lists them among its own, so the Block that is returned is the one to use
 * and to delete, and @p tssb is not to be deleted after it.
 *
 * Returns nullptr if @p tssb declares no here-and-now Variable, or if its
 * leaves do not agree on how many they are. */

SMSpp_di_unipi_it::AbstractBlock * benders_form(
                    SMSpp_di_unipi_it::TwoStageStochasticBlock * tssb );

/*--------------------------------------------------------------------------*/

#endif  /* benders_form.h included */

/*--------------------------------------------------------------------------*/
/*------------------------ End File benders_form.h -------------------------*/
/*--------------------------------------------------------------------------*/
