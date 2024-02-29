/*--------------------------------------------------------------------------*/
/*----------------------------- File main.cpp ------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Small main() for testing SimpleMILPBlock. It just creates one and loads it
 * from a stream; little more than a compilation check.
 *
 * \author Antonio Frangioni \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \author Enrico Gorgone \n
 *         Dipartimento di Matematica ed Informatica \n
 *         Universita' di Cagliari \n
 *
 * \copyright &copy; by Antonio Frangioni
 */
/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <iostream>
#include <fstream>
#include "BlockSolverConfig.h"

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace std;

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*----------------------------- CONSTANTS ----------------------------------*/
/*--------------------------------------------------------------------------*/

const char *const logF = "log.bn";

/*--------------------------------------------------------------------------*/
/*--------------------------------- Main -----------------------------------*/
/*--------------------------------------------------------------------------*/

int main( int argc , char **argv )
{
 if( argc > 3 ) {
  cerr << "Usage: " << argv[ 0 ]
       << " [LukFi_file_name BlockSolverConfig_file_name]" << endl;
  return( 1 );
  }

 ifstream ProbFile( argc < 2 ? "LukFiB.txt" : argv[ 1 ] );
 if( ! ProbFile.is_open() ) {
  cerr << "Error: cannot open input file "
       << ( argc < 2 ? "LukFiB.txt" : argv[ 1 ] ) << endl;
  return( 1 );
  }

 auto sLukFi = Block::new_Block( "LukFiBlock" );
 ProbFile >> *sLukFi;
 ProbFile.close();

 cout << *sLukFi;

 ProbFile.open( argc < 3 ? "BSC.txt" : argv[ 2 ] );
 if( ! ProbFile.is_open() ) {
  cerr << "Error: cannot open file " << ( argc < 3 ? "BSC.txt" : argv[ 2 ] )
       << endl;
  return( 1 );
  }

 BlockSolverConfig * bsc = new BlockSolverConfig;
 ProbFile >> *( bsc );
 ProbFile.close();

 bsc->apply( sLukFi );
 bsc->clear();

 auto slvr = (sLukFi->get_registered_solvers()).front();

 // open log-file - - - - - - - - - - -  - - - - - - - - - - - - - - - - - -
 //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 ofstream LOGFile( logF , ofstream::out );
 if( ! LOGFile.is_open() )
  cerr << "Warning: cannot open log file """ << logF << """" << endl;
 else
  slvr->set_log( &LOGFile );

 int rtrn = slvr->compute( false );

 LOGFile << std::endl << std::endl << "f* = "
	 << slvr->get_lb() << " (optimal value)" << std::endl;

 bsc->apply( sLukFi );

 delete bsc;

 delete sLukFi;

 return( 0 );
 }

/*--------------------------------------------------------------------------*/
/*------------------------- End File main.cpp ------------------------------*/
/*--------------------------------------------------------------------------*/

