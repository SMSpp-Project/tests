/*--------------------------------------------------------------------------*/
/*----------------------------- File test.cpp ------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Main for testing different formulations of some problem.
 *
 * This main loads a Block twice. Then it Block-Config-ure each copy with a
 * different BlockConfig taken by two different files, assumed to produce
 * two different formulations of the same problem. Then it attaches two
 * Solver to the two copies of the Block (ideally identical, but this should
 * be irrelevant), solve both and compare the results.
 *
 * Also, a special "meta-configuration" mode is supported for both the
 * BlockConfig and BlockSolverConfig: if the specified Configuration is not
 * really a BlockConfig / BlockSolverConfig but rather a
 *
 *   SimpleConfiguration< std::map< std::string , Configuration * > >
 *
 * then this is interpreted as "the BlockConfig / BlockSolverConfig that
 * are to be apply()-ed to the Block / all its sub-Block that have that
 * specific classname()". That is, if the SimpleConfiguration< ... >
 * contains, say,
 *
 *    { { "UCBlock" , < pointer to BC1 > } ,
 *      { "DCNetworkBlock" , < pointer to BC2 > } }
 *
 * then the Block is scanned, and all its sub-Block (possibly, itself) that
 * are UCBlock are BlockConfig-ured with (a clone() to) BC1 while all the its
 * sub-Block (...) that are DCNetworkBlock are BlockConfig-ured with (...)
 * BC2; analogously for the BlockSolverConfig (except there is no need for
 * clone()-ing).
 *
 * \author Antonio Frangioni \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Antonio Frangioni
 */
/*--------------------------------------------------------------------------*/
/*----------------------------- INCLUDES -----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <chrono>
#include <cmath>
#include <limits>

#include "common_utils.h"

#include "RBlockConfig.h"

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

// RefObjective is defined in common_utils.cpp (extern in common_utils.h).
// If not-NaN, the objective value of the 1st Solver is compared against
// the reference value passed on the command line (argv[6]).

/*--------------------------------------------------------------------------*/
/*----------------------------- FUNCTIONS ----------------------------------*/
/*--------------------------------------------------------------------------*/

static bool SolveBoth( double * out_fo1 = nullptr ,
                       bool   * out_hs1 = nullptr )
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

  if( out_fo1 ) *out_fo1 = fo1st;
  if( out_hs1 ) *out_hs1 = hs1st;

  auto end = std::chrono::system_clock::now();
  std::chrono::duration< double > elapsed = end - start;

  std::cout.setf( std::ios::scientific, std::ios::floatfield );
  std::cout << std::setprecision( 2 ) << elapsed.count() << " - "
	    << std::flush;

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
 // override the default terminate handler to print the exception message
 std::set_terminate( smspp_terminate );

 // read command line parameters- - - - - - - - - - - - - - - - - - - - - - -

 if( argc < 2 ) {
  std::cerr << "Usage: " << argv[ 0 ]
	    << " block_filename [BlockConfig1 BlockConfig2 "
	    << "BlockSolverConfig1 BlockSolverConfig2 RefObj]"
	    << std::endl
	    << "       default filenames: RBlockConfig1.txt RBlockConfig2.txt"
	    << " BSCfg1.txt BSCfg2.txt" << std::endl
	    << "       (if BSCfg1 is given but BSCfg2 is not, they are "
	    << "the same)"  << std::endl
	    << "       RefObj: optional reference objective; if passed, fo1st"
	    << " is compared against it with relative tolerance 1e-5"
	    << std::endl;
  return( 1 );
  }

 // read optional reference objective- - - - - - - - - - - - - - - - - - - -

 if( argc >= 7 )
  Str2Sthg( argv[ 6 ] , RefObjective );

 // load both Block out of the same netCDF file- - - - - - - - - - - - - - - -

 Block1 = Block::deserialize( argv[ 1 ] );
 if( ! Block1 ) {
  std::cerr << "error: cannot load Block from " << argv[ 1 ] << std::endl;
  return( 1 );
  }

 Block2 = Block::deserialize( argv[ 1 ] );
 // this reasonably should not fail ...

 // load two BlockConfig from file- - - - - - - - - - - - - - - - - - - - - -

 std::string fn1 = argc >= 3 ? argv[ 2 ] : "RBlockConfig1.txt";
 auto cfg1 = Configuration::deserialize( fn1 );
 if( ! cfg1 ) {
  std::cerr << "error: cannot load BlockConfig " << fn1 << std::endl;
  return( 1 );
  }

 b_config_Block( Block1 , cfg1 , fn1 );
 delete( cfg1 );

 std::string fn2 = argc >= 4 ? argv[ 3 ] : "RBlockConfig2.txt";
 auto cfg2 = Configuration::deserialize( fn2 );
 if( ! cfg2 ) {
  std::cerr << "error: cannot load BlockConfig " << fn2 << std::endl;
  return( 1 );
  }

 b_config_Block( Block2 , cfg2 , fn2 );
 delete( cfg2 );

 // load two BlockSolverConfig from file- - - - - - - - - - - - - - - - - - -

 fn1 = argc >= 5 ? argv[ 4 ] : "BSCfg1.txt";
 if( ! ( cfg1 = Configuration::deserialize( fn1 ) ) ) {
  std::cerr << "error: cannot load BlockSolverConfig " << fn1 << std::endl;
  return( 1 );
  }

 s_config_Block( Block1 , cfg1 , fn1 );
 if( Block1->get_registered_solvers().empty() ) {
  std::cerr << "Error: no Solver registered to Block1" << std::endl;
  exit( 1 );
  }

 fn2 = argc >= 6 ? argv[ 5 ] : ( argc >= 5 ? fn1 : "BSCfg2.txt" );
 if( ! ( cfg2 = Configuration::deserialize( fn2 ) ) ) {
  std::cerr << "error: cannot load BlockSolverConfig " << fn2  << std::endl;
  return( 1 );
  }

 s_config_Block( Block2 , cfg2 , fn2 );
 if( Block2->get_registered_solvers().empty() ) {
  std::cerr << "Error: no Solver registered to Block2" << std::endl;
  exit( 1 );
  }

 // solve- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 double fo1 = -INF;
 bool   hs1 = false;
 auto ok = SolveBoth( &fo1 , &hs1 );

 // optional reference-objective check- - - - - - - - - - - - - - - - - - - -

 if( ok && ! std::isnan( RefObjective ) ) {
  if( ! hs1 ) {
   std::cout << "Cannot check Ref: Solver1 returned no solution"
             << std::endl;
   ok = false;
   }
  else {
   double maxv = std::max( double( 1 ) ,
                           std::max( std::abs( fo1 ) ,
                                     std::abs( RefObjective ) ) );
   double diff = std::abs( fo1 - RefObjective );
   double tol = 1e-5 * maxv;
   bool ref_ok = ( diff <= tol );

   std::cout << def << fo1
             << " ~ Ref = " << def << RefObjective
             << " (|diff| = " << def << diff
             << ( ref_ok ? ", OK" : ", KO" ) << ")" << std::endl;

   if( ! ref_ok ) ok = false;
   }
  }

 if( ok )
  std::cout << GREEN( Test passed!! ) << std::endl;
 else
  std::cout << RED( Shit happened!! ) << std::endl;

 // clean - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

 s_config_Block( Block1 , cfg1 );  // cfg1 has been clear()-ed before
 delete( Block1 );
 delete( cfg1 );

 s_config_Block( Block2 , cfg2 );  // cfg2 has been clear()-ed before
 delete( Block2 );
 delete( cfg2 );

 return( ok ? 0 : 1 );
 }

/*--------------------------------------------------------------------------*/
/*------------------------- End File test.cpp ------------------------------*/
/*--------------------------------------------------------------------------*/
