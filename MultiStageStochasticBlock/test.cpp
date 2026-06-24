/*--------------------------------------------------------------------------*/
/*-------------------------- File test.cpp ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Main for testing MultiStageStochasticBlock.
 *
 * A MultiStageStochasticBlock instance is loaded from a netCDF file, a
 * BlockSolverConfig is applied that registers one or two Solver to it, the
 * Block is solved and the results are compared either between the two
 * Solver (when two are registered) or against a reference objective value
 * passed on the command line.
 *
 * The CLI mirrors that of tests/TwoStageStochasticBlock/test.cpp: the
 * second Solver, when present, is a LagrangianDualSolver, and the third
 * positional argument is kept as a placeholder for symmetry with the batch
 * invocation.
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
#include <cstdio>
#include <fstream>
#include <memory>

#include "common_utils.h"

#include "MultiStageStochasticBlock.h"

#include "UCBlock.h"

#include <Solution.h>

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

Block * TestBlock;  // the MultiStageStochasticBlock that is solved

// if not-NaN, the objective value of the (1st) Solver attached to the Block
// is compared against a reference value passed on the command line

// RefObjective is defined in common_utils.cpp (extern in common_utils.h)

const double RefTolerance = 1e-5;

bool ProxHeur = false;     // false = LagrangianDualSolver
                           // true  = PrimalProximalHeur

/*--------------------------------------------------------------------------*/

// test-specific command-line options, appended to the standard ones handled
// by common_utils (the instance positional and -B / -S / -c / -p / -D / -v):
//   -w / --warm-start  : 0 = LagrangianDualSolver, 1 = PrimalProximalHeur
//   -r / --ref         : reference objective value to compare against

static bool process_specific_arg( int opt )
{
 switch( opt ) {
  case( 'w' ): Str2Sthg( optarg , ProxHeur );    return( true );
  case( 'r' ): Str2Sthg( optarg , RefObjective ); return( true );
  default:                                        return( false );
  }
 }

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
 const std::string fn = "mssb_solution_roundtrip.nc4";
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
 std::set_terminate( smspp_terminate );

 // reading command line parameters - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // the standard parameters (instance positional, -B BlockConfig, -S
 // BlockSolverConfig, -c/-p prefixes) are parsed centrally by common_utils;
 // the test only appends its own -w / -r options

 docopt_desc = "SMS++ MultiStageStochasticBlock test.\n";
 short_opts += "w:r:";
 const std::vector< option > my_opts = {
   { "warm-start" , required_argument , nullptr , 'w' } ,
   { "ref"        , required_argument , nullptr , 'r' } };
 long_opts.insert( std::prev( long_opts.end() ) ,
                   my_opts.begin() , my_opts.end() );
 help += "  -w, --warm-start <0|1>          0 = LagrangianDualSolver, "
         "1 = PrimalProximalHeur [0]\n"
         "  -r, --ref <value>               reference objective to compare "
         "against [none]\n";

 process_args( argc , argv , process_specific_arg );

 // the BlockSolverConfig (-S) is mandatory; the BlockConfig (-B, the inner
 // formulation) is optional and applied only if provided
 require_solver_config();

 // read the Block- - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 TestBlock = Block::deserialize( filename );
 if( ! TestBlock ) {
  std::cout << std::endl << "Block::deserialize() failed!" << std::endl;
  exit( 1 );
  }

 if( ! dynamic_cast< MultiStageStochasticBlock * >( TestBlock ) ) {
  std::cout << std::endl
            << "Error: deserialized Block is not a MultiStageStochasticBlock"
            << std::endl;
  delete TestBlock;
  exit( 1 );
  }

 // attach the Solver(s) to the Block - - - - - - - - - - - - - - - - - - - -

 // BSC may be a plain BlockSolverConfig or a meta-config
 // SimpleConfiguration< std::map< std::string , Configuration * > >;
 // s_config_Block() dispatches on the runtime type and clears the config(s)
 // for final cleanup.
 Configuration * bsc = Configuration::deserialize( sconf_file );
 if( ! bsc ) {
  std::cerr << "Error: cannot load BSC from " << sconf_file << std::endl;
  delete TestBlock;
  exit( 1 );
  }

 // apply the inner (meta-)BlockConfig (formulation) by classname recursively
 // over the whole Block tree: the leaves contain UCBlocks whose
 // ThermalUnitBlock (and DCNetworkBlock) sit arbitrarily deep, and
 // b_config_Block reaches them all. The inner Solvers are NOT attached here:
 // they descend from the LagrangianDualSolver str_LagBF_BSCfg (InnerBSCfg.txt)
 // when bsc is applied below.
 if( ! bconf_file.empty() )
  if( auto ibc = Configuration::deserialize( bconf_file ) ) {
   b_config_Block( TestBlock , ibc , bconf_file );
   delete( ibc );
   }

 s_config_Block( TestBlock , bsc , sconf_file );

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

 // MSSB uses lower-bound producers (MILPSolver LP, LagrangianDualSolver);
 // ProxHeur is a placeholder kept for CLI symmetry with the TSSB batch.
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

 // permanent round-trip check of the MSSB Solution machinery (runs on every
 // instance, baked and shared-tree alike)
 if( ! CheckSolutionRoundTrip( TestBlock ) )
  AllPassed = false;

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
