/*--------------------------------------------------------------------------*/
/*-------------------------- File test.cpp ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Main for testing MultiStageStochasticBlock.
 *
 * The common part is that of every stochastic Block [see stochastic_test.h];
 * this one only adds, for each copy and after the Solver have run, the
 * round trip of its Solution [see CheckSolutionRoundTrip()].
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

#include <cstdio>
#include <iostream>
#include <memory>
#include <random>

#include "stochastic_test.h"

#include "MultiStageStochasticBlock.h"

#include <Solution.h>

/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/

// Permanent regression check of the MultiStageStochasticBlock Solution
// machinery. MSSB has no dedicated Solution class: it reuses the inherited
// TwoStageStochasticBlockSolution, recursively over the Block tree (here built
// from a shared scenario tree via views). After the Block has been solved, the
// Solution must survive a full round-trip without error:
//   s = get_Solution(); s->read( block );  s->serialize( file );
//   s2 = Solution::deserialize( file );     s2->write( block );
// The serialize( const std::string & ) overload is used on purpose: it sets
// the file-level SMS++_file_type attribute that Solution::deserialize requires
// (the bare serialize( NcFile & ) does not). Returns true iff the round-trip
// completes and deserialize yields a non-null Solution.

static bool CheckSolutionRoundTrip( Block * block )
{
 // a name of its own, since the batteries run in parallel in the same
 // directory and would otherwise write and read each other's file
 const std::string fn = "mssb_solution_roundtrip_" +
                        std::to_string( std::random_device()() ) + ".nc4";
 bool ok = false;
 try {
  std::unique_ptr< Solution > s1( block->get_Solution() );
  s1->read( block );
  s1->serialize( fn );

  std::unique_ptr< Solution > s2( Solution::deserialize( fn ) );
  if( ! s2 )
   std::cerr << "Solution round-trip: deserialize returned null" << std::endl;
  else {
   s2->write( block );   // restore the round-tripped Solution into the Block
   ok = true;
   }
  }
 catch( std::exception & e ) {
  std::cerr << "Solution round-trip error: " << e.what() << std::endl;
  }
 std::remove( fn.c_str() );
 return( ok );
 }

/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 StochasticTester t;
 t.classname = "MultiStageStochasticBlock";
 t.is_one = []( Block * b ) {
  return( dynamic_cast< MultiStageStochasticBlock * >( b ) != nullptr );
  };

 // permanent round-trip check of the MSSB Solution machinery (runs on every
 // instance, baked and shared-tree alike)
 t.check = CheckSolutionRoundTrip;

 return( stochastic_test( argc , argv , t ) );
 }

/*--------------------------------------------------------------------------*/
/*--------------------------- End File test.cpp ----------------------------*/
/*--------------------------------------------------------------------------*/
