/*--------------------------------------------------------------------------*/
/*-------------------------- File test.cpp ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Main for testing TwoStageStochasticBlock.
 *
 * A TwoStageStochasticBlock instance is loaded from a netCDF file, a
 * BlockSolverConfig is applied that registers one or two Solver to it, the
 * Block is solved and the results are compared either between the two
 * Solver (when two are registered) or against a reference objective value
 * passed on the command line.
 *
 * The CLI mirrors that of tests/LagrangianDualSolver_UC/test.cpp, except
 * that for TwoStageStochasticBlock the second Solver is always a
 * LagrangianDualSolver (there is no PrimalProximalHeur counterpart yet);
 * the third positional argument is therefore kept as a placeholder for
 * symmetry with the LDS_UC batch invocation.
 *
 * \author Donato Meoli \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \author Antonio Frangioni \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Donato Meoli, Antonio Frangioni
 */
/*--------------------------------------------------------------------------*/
/*-------------------------------- MACROS ----------------------------------*/
/*--------------------------------------------------------------------------*/

#define LOG_LEVEL 2
// -1 = no log at all, not even pass/fail
// 0 = only pass/fail
// 1 = result of each test
// 2 = + solver log

#if( LOG_LEVEL >= 1 )
 #define LOG1( x ) std::cout << x
 #define CLOG1( y , x ) if( y ) std::cout << x

 #if( LOG_LEVEL >= 2 )
  #define LOG_ON_COUT 1
 #endif
#else
 #define LOG1( x )
 #define CLOG1( y , x )
#endif

/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <cmath>
#include <fstream>

#include "common_utils.h"

#include "TwoStageStochasticBlock.h"

#include "UCBlock.h"

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

using FunctionValue = Function::FunctionValue;

/*--------------------------------------------------------------------------*/
/*------------------------------- CONSTANTS --------------------------------*/
/*--------------------------------------------------------------------------*/

const char * const logF = "log.txt";

const FunctionValue INF = SMSpp_di_unipi_it::Inf< FunctionValue >();

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/

Block * TestBlock;  // the TwoStageStochasticBlock that is solved

// if not-NaN, the objective value of the (1st) Solver attached to the Block
// is compared against a reference value passed on the command line

// RefObjective is defined in common_utils.cpp (extern in common_utils.h)

const double RefTolerance = 1e-5;

bool ProxHeur = false;     // false = LagrangianDualSolver
                           // true  = PrimalProximalHeur


int main( int argc , char ** argv )
{
 std::set_terminate( smspp_terminate );

 // reading command line parameters - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 switch( argc ) {
  case( 5 ): Str2Sthg( argv[ 4 ] , RefObjective );
  case( 4 ): Str2Sthg( argv[ 3 ] , ProxHeur );
  case( 3 ):
  case( 2 ): break;
  default:
   std::cerr << "Usage: " << argv[ 0 ]
             << " TSSB-file [BSC-file ws ref]" << std::endl
             << "       BSC-file: BlockSolverConfig description [BSPar.txt]"
             << std::endl
             << "       ws:  0 = LagrangianDualSolver, 1 = PrimalProximalHeur"
                " [0]" << std::endl
             << "       ref: reference objective value to compare against "
                "[none]" << std::endl;
   return( 1 );
  }

 // read the Block- - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 TestBlock = Block::deserialize( argv[ 1 ] );
 if( ! TestBlock ) {
  std::cout << std::endl << "Block::deserialize() failed!" << std::endl;
  exit( 1 );
  }

 if( ! dynamic_cast< TwoStageStochasticBlock * >( TestBlock ) ) {
  std::cout << std::endl
            << "Error: deserialized Block is not a TwoStageStochasticBlock"
            << std::endl;
  delete TestBlock;
  exit( 1 );
  }

 // attach the Solver(s) to the Block - - - - - - - - - - - - - - - - - - - -

 // BSC may be a plain BlockSolverConfig or a meta-config
 // SimpleConfiguration< std::map< std::string , Configuration * > >;
 // s_config_Block() dispatches on the runtime type and clears the config(s)
 // for final cleanup.
 std::string bsc_fn = argc >= 3 ? argv[ 2 ] : "BSPar.txt";
 Configuration * bsc = Configuration::deserialize( bsc_fn );
 if( ! bsc ) {
  std::cerr << "Error: cannot load BSC from " << bsc_fn << std::endl;
  delete TestBlock;
  exit( 1 );
  }

 // apply the inner (meta-)BlockConfig (formulation) by classname recursively
 // over the whole Block tree: the scenarios contain UCBlocks whose
 // ThermalUnitBlock (and DCNetworkBlock) sit arbitrarily deep, and
 // b_config_Block reaches them all. The inner Solvers are NOT attached here:
 // they descend from the LagrangianDualSolver str_LagBF_BSCfg (InnerBSCfg.txt)
 // when bsc is applied below.
 if( auto ibc = Configuration::deserialize( "InnerBCfg.txt" ) ) {
  b_config_Block( TestBlock , ibc , "InnerBCfg.txt" );
  delete( ibc );
  }

 s_config_Block( TestBlock , bsc , bsc_fn );

 if( TestBlock->get_registered_solvers().empty() ) {
  std::cout << std::endl
            << "no Solver registered to the Block!" << std::endl;
  delete bsc;
  delete TestBlock;
  exit( 1 );
  }

 // open log-file - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 #if( LOG_LEVEL >= 2 )
  #if( LOG_ON_COUT )
   ( ( TestBlock->get_registered_solvers() ).back() )->set_log( &std::cout );
  #else
   std::ofstream LOGFile( logF , std::ofstream::out );
   if( ! LOGFile.is_open() )
    std::cerr << "Warning: cannot open log file " << logF << std::endl;
   else {
    LOGFile.setf( std::ios::scientific , std::ios::floatfield );
    LOGFile << std::setprecision( 10 );
    ( ( TestBlock->get_registered_solvers() ).back() )->set_log( &LOGFile );
    }
  #endif
 #endif

 // first solver call - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 LOG1( "First call: " );

 bool AllPassed;

 // TSSB always uses lower-bound producers (MILPSolver LP, LagrangianDualSolver);
 // ProxHeur is a placeholder kept for CLI symmetry with LDS_UC.
 const auto getter1 = ObjGetter::LowerBound;
 const auto getter2 = ProxHeur ? ObjGetter::UpperBound : ObjGetter::LowerBound;

 if( TestBlock->get_registered_solvers().size() > 1 ) {
  double fo1st = -INF;
  bool hs1st = false;
  double time1 = 0.0;
  long it1 = 0;
  AllPassed = SolveBoth( TestBlock , getter1 , getter2 , ProxHeur ,
                         RefTolerance , &fo1st , &hs1st , &time1 , &it1 );

  // optional additional check against reference objective
  if( ! std::isnan( RefObjective ) ) {
   if( hs1st )
    AllPassed &= CheckRefValue( fo1st , RefObjective , RefTolerance ,
                                time1 , it1 );
   else
    AllPassed = false;
   }
  }
 else {
  // single-solver case
  if( std::isnan( RefObjective ) )
   AllPassed = SolveBoth( TestBlock , getter1 );
  else
   AllPassed = SolveAndCheckRef( TestBlock , RefObjective , getter1 ,
                                 RefTolerance );
  }

 #if( LOG_LEVEL >= 0 )
  if( ! std::isnan( RefObjective ) ||
      ( TestBlock->get_registered_solvers().size() > 1 ) ) {
   if( AllPassed )
    std::cout << GREEN( All tests passed!! ) << std::endl;
   else
    std::cout << RED( Shit happened!! ) << std::endl;
   }
 #endif

 // destroy the Block - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 TestBlock->unregister_Solvers( true );
 delete( bsc );
 delete( TestBlock );

 return( AllPassed ? 0 : 1 );

 }  // end( main )

/*--------------------------------------------------------------------------*/
/*--------------------------- End File test.cpp ----------------------------*/
/*--------------------------------------------------------------------------*/
