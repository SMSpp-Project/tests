/*--------------------------------------------------------------------------*/
/*-------------------------- File test.cpp ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Main for testing TwoStageStochasticBlock.
 *
 * The common part is that of every stochastic Block [see stochastic_test.h];
 * this one only adds -b, which attaches the Solver to the Benders form of
 * the instance [see TwoStageStochasticBlock::get_Benders_form()] rather than
 * to the Block read: the two describe the same problem, but a Solver that
 * keeps the here-and-now Variable in a master of its own needs them in a
 * single copy in the root, which the extensive form on file does not have.
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
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <iostream>
#include <map>

#include "stochastic_test.h"

#include "AbstractBlock.h"

#include "TwoStageStochasticBlock.h"

/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 static bool BenForm = false;  // the Solver attached to the Benders form

 // the TwoStageStochasticBlock of each Benders form, which the form is given
 // back to before it is deleted
 static std::map< Block * , TwoStageStochasticBlock * > owner;

 StochasticTester t;
 t.classname = "TwoStageStochasticBlock";
 t.is_one = []( Block * b ) {
  return( dynamic_cast< TwoStageStochasticBlock * >( b ) != nullptr );
  };
 t.short_opts = "b";
 t.long_opts = { { "benders" , no_argument , nullptr , 'b' } };
 t.help = "  -b, --benders                   solve the Benders form of the "
          "instance [off]\n";
 t.handle = []( int opt ) {
  if( opt != 'b' )
   return( false );
  BenForm = true;
  return( true );
  };

 /* The Benders form is assembled around the Block the file gives, and it is
  * that Block, not this one, that the Solver are attached to: the two
  * describe the same problem, so a :MILPSolver reading the assembled one
  * whole is still solving the extensive form and is the reference of the
  * cross-check. */
 t.transform = []( Block * b ) -> Block * {
  if( ! BenForm )
   return( b );

  b->generate_abstract_variables();
  b->generate_abstract_constraints();
  b->generate_objective();

  auto tssb = static_cast< TwoStageStochasticBlock * >( b );
  Block * root = tssb->get_Benders_form();
  if( ! root )
   std::cout << std::endl
             << "Error: the Block declares no here-and-now Variable, hence "
                "there is no Benders form of it" << std::endl;
  else
   owner[ root ] = tssb;
  return( root );
  };

 // the Benders form holds the leaves of the TwoStageStochasticBlock, which
 // have to go back to it before either is deleted
 t.release = []( Block * b ) {
  auto it = owner.find( b );
  if( it == owner.end() ) {
   delete b;
   return;
   }
  b->unregister_Solvers( true );
  it->second->give_back_Benders_form( static_cast< AbstractBlock * >( b ) );
  delete it->second;
  owner.erase( it );
  };

 return( stochastic_test( argc , argv , t ) );
 }

/*--------------------------------------------------------------------------*/
/*--------------------------- End File test.cpp ----------------------------*/
/*--------------------------------------------------------------------------*/
