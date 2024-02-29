/*--------------------------------------------------------------------------*/
/*----------------------------- File test.cpp ------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Main for testing different formulations of some problem.
 *
 * This main loads a Block twice. Then it Block-Config-ure each copy with a
 * different BlockConfig taken by two different files, assumed to produce
 * two different formulations of the same problem. Then it attaches two
 * identical Solver to the two copies of the Block (by using the same
 * BlockSolverConfig), solve both and compare the results.
 *
 * \author Antonio Frangioni \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Antonio Frangioni
 */
/*--------------------------------------------------------------------------*/
/*------------------------------ MACROS ------------------------------------*/
/*--------------------------------------------------------------------------*/

#define USECOLORS 1
#if( USECOLORS )
 #define RED( x ) "\x1B[31m" #x "\033[0m"
 #define GREEN( x ) "\x1B[32m" #x "\033[0m"
#else
 #define RED( x ) #x
 #define GREEN( x ) #x
#endif

/*--------------------------------------------------------------------------*/
/*----------------------------- INCLUDES -----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <iomanip>

#include "RBlockConfig.h"

#include "BlockSolverConfig.h"

/*--------------------------------------------------------------------------*/
/*------------------------------- USING ------------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*------------------------------- TYPES ------------------------------------*/
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/*----------------------------- CONSTANTS ----------------------------------*/
/*--------------------------------------------------------------------------*/

static constexpr double INF = SMSpp_di_unipi_it::Inf< double >();

/*--------------------------------------------------------------------------*/
/*------------------------------ GLOBALS -----------------------------------*/
/*--------------------------------------------------------------------------*/

Block * Block1;
Block * Block2;

/*--------------------------------------------------------------------------*/
/*----------------------------- FUNCTIONS ----------------------------------*/
/*--------------------------------------------------------------------------*/

static void PrintResults( bool hs , int rtrn , double fo )
{
 if( hs )
  std::cout << fo;
 else
  if( rtrn == Solver::kInfeasible )
   std::cout << "    Unfeas";
  else
   if( rtrn == Solver::kUnbounded )
    std::cout << "      Unbounded";
   else
    std::cout << "      Error!";
 }

/*--------------------------------------------------------------------------*/

static bool SolveBoth( void ) 
{
 try {
  // solve with the 1st Solver- - - - - - - - - - - - - - - - - - - - - - - -
  auto Slvr1 = Block1->get_registered_solvers().front();

  auto start = std::chrono::system_clock::now();

  int rtrn1st = Slvr1->compute( false );
  bool hs1st = ( ( ( rtrn1st >= Solver::kOK ) && ( rtrn1st < Solver::kError )
                   && ( rtrn1st != Solver::kUnbounded )
                   && ( rtrn1st != Solver::kInfeasible ) )
                 || ( rtrn1st == Solver::kLowPrecision ) );
  double fo1st = hs1st ? Slvr1->get_var_value() : -INF;

  auto end = std::chrono::system_clock::now();
  std::chrono::duration< double > elapsed = end - start;
 
  std::cout.setf( std::ios::scientific, std::ios::floatfield );
  std::cout << std::setprecision( 2 ) << elapsed.count() << " - " << std::flush;

  // solve with the 2nd Solver- - - - - - - - - - - - - - - - - - - - - - - -
  auto Slvr2 = Block2->get_registered_solvers().front();

  start = std::chrono::system_clock::now();

  int rtrn2nd = Slvr2->compute( false );
  bool hs2nd = ( ( ( rtrn2nd >= Solver::kOK ) && ( rtrn2nd < Solver::kError )
                   && ( rtrn2nd != Solver::kUnbounded )
                   && ( rtrn2nd != Solver::kInfeasible ) )
                 || ( rtrn2nd == Solver::kLowPrecision ) );
  double fo2nd = hs2nd ? Slvr2->get_var_value() : -INF;

  end = std::chrono::system_clock::now();
  elapsed = end - start;

  std::cout.setf( std::ios::scientific, std::ios::floatfield );
  std::cout << std::setprecision( 2 ) << elapsed.count();

  if( hs1st && hs2nd && ( abs( fo1st - fo2nd ) <= 2e-7 *
			  std::max( double( 1 ) , std::max( abs( fo1st ) ,
						  abs( fo2nd ) ) ) ) ) {
   std::cout << " - OK(f)" << std::endl;
   return( true );
   }

  if( ( rtrn1st == Solver::kInfeasible ) &&
      ( rtrn2nd == Solver::kInfeasible ) ) {
   std::cout << " - OK(e)" << std::endl;
   return( true );
   }

  if( ( rtrn1st == Solver::kUnbounded ) &&
      ( rtrn2nd == Solver::kUnbounded ) ) {
   std::cout << " - OK(u)" << std::endl;
   return( true );
   }
    
  std::cout << " - " << std::setprecision( 7 );
  PrintResults( hs1st , rtrn1st , fo1st );
  std::cout << " - ";
  PrintResults( hs2nd , rtrn2nd , fo2nd );
  std::cout << std::endl;

  return( false );
  }
 catch( std::exception &e ) {
  std::cerr << e.what() << std::endl;
  exit( 1 );
  }
 catch(...) {
  std::cerr << "error: unknown exception thrown" << std::endl;
  exit( 1 );
  }
 }

/*--------------------------------------------------------------------------*/

int main( int argc , char **argv )
{
 // read command line parameters- - - - - - - - - - - - - - - - - - - - - - -

 if( argc < 2 ) {
  std::cerr << "Usage: " << argv[ 0 ]
       << " block_filename [cfg_1_filename cfg_1_filename]" << std::endl
       << "       default: RBlockConfig1.txt RBlockConfig1.txt" << std::endl;
  return( 1 );  
  }

 // load both Block out of the same netCDF file- - - - - - - - - - - - - - - - 

 Block1 = Block::deserialize( argv[ 1 ] );
 if( ! Block1 ) {
  std::cerr << "error: cannot load Block from " << argv[ 1 ] << std::endl;
  return( 1 );
  }

 Block2 = Block::deserialize( argv[ 1 ] );
 // this reasonably should not fail ...

 // load two BlockConfig from file- - - - - - - - - - - - - - - - - - - - - -

 auto cfg1 = dynamic_cast< BlockConfig * >(
	     Configuration::deserialize( argc >= 3 ? argv[ 2 ]
					           : "RBlockConfig1.txt" ) );
 if( ! cfg1 ) {
  std::cerr << "error: cannot load BlockConfig 1" << std::endl;
  return( 1 );
  }

 cfg1->apply( Block1 );
 delete( cfg1 );
 
 auto cfg2 = dynamic_cast< BlockConfig * >(
	     Configuration::deserialize( argc >= 4 ? argv[ 3 ]
					           : "RBlockConfig2.txt" ) );
 if( ! cfg2 ) {
  std::cerr << "error: cannot load BlockConfig 2" << std::endl;
  return( 1 );
  }

 cfg2->apply( Block2 );
 delete( cfg2 );

 // attach two identical Solver to both Block - - - - - - - - - - - - - - - -
 // do that via a BlockSolverConfig

 auto c = Configuration::deserialize( "BSCfg.txt" );
 auto bsc = dynamic_cast< BlockSolverConfig * >( c );
 if( ! bsc ) {
  std::cerr << "error: BSCfg.txt does not contain a BlockSolverConfig"
            << std::endl;
  exit( 1 );
  }

 bsc->apply( Block1 );

 if( Block1->get_registered_solvers().empty() ) {
  std::cerr << "Error: no Solver registered to Block1" << std::endl;
  exit( 1 );
  }

 bsc->apply( Block2 );
 // this reasonably should not fail ...

 bsc->clear();  
  
 // solve- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 auto ok = SolveBoth();
 if( ok )
  std::cout << GREEN( Test passed!! ) << std::endl;
 else
  std::cout << RED( Shit happened!! ) << std::endl;

 // clean - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

 bsc->apply( Block1 );
 delete( Block1 );

 bsc->apply( Block2 );
 delete( Block2 );

 delete( bsc );

 return( ok ? 0 : 1 );
 }

/*--------------------------------------------------------------------------*/
/*------------------------- End File test.cpp ------------------------------*/
/*--------------------------------------------------------------------------*/
