/*--------------------------------------------------------------------------*/
/*-------------------------- File test.cpp ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Main for testing LagrangianDualSolver with MMCFBlock
 *
 * An MMCFBlock instance is loaded from a text file, two different Solver are
 * registered to the MMCFBlock, the second of which is assumed to be a
 * LagrangianDualSolver, the MMCFBlock is solved by the Solver and the
 * results are compared.
 *
 * The tester has some parts for the future extension when the MMCFBlock is
 * repeatedly randomly modified and re-solved several times, but this is not
 * done yet.
 *
 * \author Francesca Demelas \n
 *         Laboratoire d'Informatique de Paris Nord \n
 *         Universite' Sorbonne Paris Nord \n
 *
 * \author Antonio Frangioni \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Antonio Frangioni
 */
/*--------------------------------------------------------------------------*/
/*-------------------------------- MACROS ----------------------------------*/
/*--------------------------------------------------------------------------*/

#define LOG_LEVEL 0
// 0 = only pass/fail
// 1 = result of each test
// 2 = + solver log
// 3 = + save LP file

#if( LOG_LEVEL >= 1 )
 #define LOG1( x ) cout << x
 #define CLOG1( y , x ) if( y ) cout << x

 #if( LOG_LEVEL >= 2 )
  #define LOG_ON_COUT 0
  // if nonzero, the 2nd Solver (LagrangianDualSolver) log is sent on cout
  // rather than on a file( bsc->get_SolverName( i ) == "BundleSolver" )
 #endif
#else
 #define LOG1( x )
 #define CLOG1( y , x )
#endif

/*--------------------------------------------------------------------------*/
// if nonzero, the 1st Solver attached to the UCBlock is detached
// and re-attached to it at all iterations

#define DETACH_1ST 0

// if nonzero, the 2nd Solver attached to the UCBlock is detached and
// re-attached to it at all iterations

#define DETACH_2ND 0

/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <fstream>
#include <sstream>

#include <iomanip>

#include <chrono>

#include <random>

#include "common_utils.h"

#include "CDASolver.h"

#include "LagrangianDualSolver.h"

#include "MMCFBlock.h"

//!!#include "MILPSolver.h"

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace std;

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*-------------------------------- TYPES -----------------------------------*/
/*--------------------------------------------------------------------------*/

using Index = Block::Index;
using c_Index = Block::c_Index;

using Range = Block::Range;
using c_Range = Block::c_Range;

using Subset = Block::Subset;
using c_Subset = Block::c_Subset;

using FunctionValue = Function::FunctionValue;

/*--------------------------------------------------------------------------*/
/*------------------------------- CONSTANTS --------------------------------*/
/*--------------------------------------------------------------------------*/

const char *const logF = "log.txt";

const FunctionValue INF = SMSpp_di_unipi_it::Inf< FunctionValue >();

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/

MMCFBlock * TestBlock;         // the [MMCF]Block that is solved
int wprnt = 0;

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/

static void PrintSol( CDASolver * slvr , bool first ,
		      double time ,  double objFunc )
{
 if( ! wprnt )
  return;

 std::string name( filename );
 name = name.substr( name.find_last_of( "/" ) + 1 , name.length() );
 name = name.substr( 0 , name.find( "." ) );
 if( first )
  name.append( "_1" );
 else
  name.append( "_2" );

 // start: reduced costs extraction

 if( wprnt & 1 ) {
  // for the second Solver only, read a Configuration for
  // get_dual_solution( ) from file
  Configuration * cfg = nullptr;
  if( ! first )
   cfg = Configuration::deserialize( "GetDualSolConfig.txt" );

  slvr->get_dual_solution( cfg );
  delete cfg;

  ofstream dualFile( "./redCosts/" + name + "-Dual.txt" );
  for( Index k = 0 ; k < TestBlock->get_NComm() ; ++k ) {
   for( Index i = 0 ; i < TestBlock->get_NNodes() ; ++i )
    dualFile << TestBlock->get_potential( k , i ) << " ";
   dualFile << "\n";
   }

  dualFile.close();
  }
  
 // primal solution extraction 
 if( wprnt & 2 ) {
  slvr->get_var_solution();

  ofstream primalFile( "./primals/" + name + "-Prim.txt" );
  for( Index k = 0 ; k < TestBlock->get_NComm() ; ++k ) {
   for( Index i = 0 ; i < TestBlock->get_NArcs() ; ++i  )
    primalFile << TestBlock->get_flow( k , i ) << " ";
   primalFile << "\n";   
   }

  primalFile.close();
  }

 if( wprnt & 4 ) {
  ofstream timeFile( "./times/" + name + "Sol-time.dat" );
  timeFile << time;
  timeFile << "\n";
  timeFile << objFunc <<"\n";
  timeFile.close();
  }
 }

/*--------------------------------------------------------------------------*/

static bool SolveBoth( void )
{
 // own solve loop (kept for the per-Solver PrintSol() side output and the
 // CDASolver check), then the shared common_utils cross_check + uniform line.
 // Every Solver is an exact optimum read via get_var_value(); they must agree
 // within 2e-7. The DETACH_* re-ordering is applied up front.
 try {
  #if DETACH_1ST
   { auto s = TestBlock->get_registered_solvers().front();
     TestBlock->unregister_Solver( s );
     TestBlock->register_Solver( s , true ); }   // push it to the front
  #endif
  #if DETACH_2ND
   { auto s = TestBlock->get_registered_solvers().back();
     TestBlock->unregister_Solver( s );
     TestBlock->register_Solver( s ); }          // push it to the back
  #endif

  const auto & reg = TestBlock->get_registered_solvers();
  std::vector< Solver * > S( reg.begin() , reg.end() );
  const std::size_t M = S.size();

  std::vector< int > status( M );
  std::vector< double > times( M );
  std::vector< bool > hs( M );
  std::vector< SolverReading > rd( M );
  std::vector< std::string > tok( M );

  for( std::size_t k = 0 ; k < M ; ++k ) {
   auto Slvr = dynamic_cast< CDASolver * >( S[ k ] );
   if( ! Slvr ) {
    cout << "Error! Solver registred to TestBlock not a CDASolver";
    exit( 1 );
    }
   #if( LOG_LEVEL >= 3 )
    if( k == 0 )
     Slvr->set_par( MILPSolver::strOutputFile , "LPBlock-CPXMILP.lp" );
   #endif

   auto start = std::chrono::system_clock::now();
   status[ k ] = Slvr->compute( false );
   auto end = std::chrono::system_clock::now();
   times[ k ] = std::chrono::duration< double >( end - start ).count();
   hs[ k ] = ( ( ( status[ k ] >= Solver::kOK )
                 && ( status[ k ] < Solver::kError )
                 && ( status[ k ] != Solver::kUnbounded )
                 && ( status[ k ] != Solver::kInfeasible ) )
               || ( status[ k ] == Solver::kLowPrecision ) );
   double fo = hs[ k ] ? Slvr->get_var_value() : -INF;

   PrintSol( Slvr , k == 0 , times[ k ] , fo );

   if( hs[ k ] ) {
    rd[ k ] = SolverReading::exact( fo , eps_of( k , Slvr ) );
    tok[ k ] = reading_token( rd[ k ] );
    }
   else if( status[ k ] == Solver::kInfeasible )  tok[ k ] = "Unfeas";
   else if( status[ k ] == Solver::kUnbounded )   tok[ k ] = "Unbounded";
   else                                           tok[ k ] = "Error!";
   }

  std::string verdict;
  double diff;
  bool ok = cross_check( rd , hs , status ,
                         std::numeric_limits< double >::quiet_NaN() ,
                         2e-7 , verdict , diff );
  print_instance_line( times , tok ,
                       std::numeric_limits< double >::quiet_NaN() ,
                       verdict , diff );
  return( ok );
  }
 catch( exception &e ) {
  cerr << e.what() << endl;
  exit( 1 );
  }
 catch(...) {
  cerr << "Error: unknown exception thrown" << endl;
  exit( 1 );
  }
 }

/*--------------------------------------------------------------------------*/

// test-specific command-line knobs, set by process_specific_arg(); the
// standard parameters (instance positional, -B BlockConfig, -S
// BlockSolverConfig, -c/-p prefixes) are handled centrally by common_utils
char filetype = 's';  // type of the input file

// number of times the instance is (re-)solved; 0 means "not set on the
// command line", in which case the value is taken from the inner Solver's
// intNTrainRounds parameter (if it has one) and otherwise defaults to 1. A
// BundleSolverML configured with intMLTrainOnline trains once per re-solve.
int n_epochs = 0;

/*--------------------------------------------------------------------------*/

static bool process_specific_arg( int opt )
{
 switch( opt ) {
  case( 't' ): filetype = optarg[ 0 ];        return( true );
  case( 'w' ): Str2Sthg( optarg , wprnt );    return( true );
  case( 'e' ): Str2Sthg( optarg , n_epochs ); return( true );
  default:                                    return( false );
  }
 }

/*--------------------------------------------------------------------------*/

int main( int argc , char **argv )
{
 // override the default terminate handler to print the exception message
 std::set_terminate( smspp_terminate );

 // reading command line parameters - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // standard params (instance positional + -B + -S) are parsed by
 // common_utils; the test only appends its own knobs

 docopt_desc = "SMS++ LagrangianDualSolver-on-MMCFBlock test.\n";
 short_opts += "t:w:e:";
 const std::vector< option > my_opts = {
   { "type"   , required_argument , nullptr , 't' } ,
   { "wprnt"  , required_argument , nullptr , 'w' } ,
   { "epochs" , required_argument , nullptr , 'e' } };
 long_opts.insert( std::prev( long_opts.end() ) ,
                   my_opts.begin() , my_opts.end() );
 help += "  -t, --type <c>                  input file type "
         "(s*,c,p,o,d,u,m) [s]\n"
         "  -w, --wprnt <bits>              what to print to file, bit-wise "
         "[0]:\n"
         "                                    1 duals, 2 primal, "
         "4 time & objective value\n"
         "  -e, --epochs <n>                times the instance is re-solved "
         "(training rounds);\n"
         "                                  overrides the inner Solver's "
         "intNTrainRounds [1]\n";

 process_args( argc , argv , process_specific_arg );

 // both the BlockConfig (-B) and the BlockSolverConfig (-S) must be provided
 // explicitly: the test never falls back to a hardcoded default Configuration
 require_block_config();
 require_solver_config();

 // read the Block- - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 TestBlock = new MMCFBlock;

 TestBlock->load( filename , filetype );
 TestBlock->PreProcess();

 // BC may be a plain BlockConfig or a meta-config
 // SimpleConfiguration< std::map< std::string , Configuration * > >;
 // b_config_Block() dispatches on the runtime type.
 Configuration * cfg = Configuration::deserialize( bconf_file );
 if( ! cfg ) {
  cerr << "Error: cannot load BC from " << bconf_file << endl;
  exit( 1 );
  }
 b_config_Block( TestBlock , cfg , bconf_file );

 delete( cfg );

 TestBlock->generate_abstract_variables();

 // attach the Solver(s) to the Block - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // do this by reading an appropriate BlockSolverConfig from file and
 // apply() it to the TestBlock; note that the BlockSolverConfig is
 // clear()-ed and kept to do the cleanup at the end.
 // BSC may be a plain BlockSolverConfig or a meta-config
 // SimpleConfiguration< std::map< std::string , Configuration * > >;
 // s_config_Block() dispatches on the runtime type and clears the config(s)
 // for final cleanup.

 Configuration * bsc = Configuration::deserialize( sconf_file );
 if( ! bsc ) {
  cerr << "Error: cannot load BSC from " << sconf_file << endl;
  exit( 1 );
  }
 s_config_Block( TestBlock , bsc , sconf_file );

 if( TestBlock->get_registered_solvers().empty() ) {
  cout << endl << "no Solver registered to the Block!" << endl;
  exit( 1 );
  }

 // open log-file - - - - - - - - - - -  - - - - - - - - - - - - - - - - - -
 //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 #if( LOG_LEVEL >= 2 )
  #if( LOG_ON_COUT )
   ( ( TestBlock->get_registered_solvers() ).back() )->set_log( &cout );
  #else
   ofstream LOGFile( logF , ofstream::out );
   if( ! LOGFile.is_open() )
    cerr << "Warning: cannot open log file """ << logF << """" << endl;
   else {
    LOGFile.setf( ios::scientific, ios::floatfield );
    LOGFile << setprecision( 10 );
    ( ( TestBlock->get_registered_solvers() ).back() )->set_log( &LOGFile );
    }
  #endif
 #endif

 // solver call(s) - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // the instance is solved n_rounds times: this is a no-op repetition for a
 // plain Solver, but it drives online training when the inner Solver is a
 // BundleSolverML with intMLTrainOnline set. The count comes from -e if given,
 // else from the inner Solver's intNTrainRounds parameter, else 1.

 int n_rounds = 1;
 if( n_epochs > 0 )
  n_rounds = n_epochs;
 else if( auto * lds = dynamic_cast< LagrangianDualSolver * >(
			    TestBlock->get_registered_solvers().back() ) )
  if( Solver * inner = lds->get_inner_Solver() ) {
   const auto idx = inner->int_par_str2idx( "intNTrainRounds" );
   if( idx < inner->get_num_int_par() )
    n_rounds = inner->get_int_par( idx );
   }

 bool AllPassed = true;
 for( int r = 0 ; r < n_rounds ; ++r ) {
  CLOG1( n_rounds > 1 , "[round " << r << "] " );
  AllPassed &= SolveBoth();
  }

 if( AllPassed )
  cout << GREEN( All tests passed!! ) << endl;
 else
  cout << RED( Shit happened!! ) << endl;
 
 // destroy the Block - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 // apply() the clear()-ed BlockSolverConfig (or meta-config) to cleanup Solver
 s_config_Block( TestBlock , bsc );

 // then delete the BlockSolverConfig
 delete( bsc );

 // finally the AbstractBlock can be deleted
 delete( TestBlock );

 // terminate - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 return( AllPassed ? 0 : 1 );

 }  // end( main )

/*--------------------------------------------------------------------------*/
/*------------------------ End File test.cpp -------------------------------*/
/*--------------------------------------------------------------------------*/
