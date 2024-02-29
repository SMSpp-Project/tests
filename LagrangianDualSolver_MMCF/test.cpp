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
 * \author Francesco Demelas \n
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
// if nonzero, the two Block are not solved at every round of changes, but
// only every SKIP_BEAT + 1 rounds. this allows changes to accumulate, and
// therefore puts more pressure on the Modification handling of the Solver
// (in case this tries to do "smart" things rather than dumbly processing
// each one in turn)
//
// note that the number of rounds of changes is them multiplied by
// SKIP_BEAT + 1, so that the input parameter still dictates the number of
// Block solutions

#define SKIP_BEAT 0

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
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <fstream>
#include <sstream>
#include <iomanip>

#include <random>

#include "BlockSolverConfig.h"

#include "CDASolver.h"

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
char **globalArgv;             // the main argv for a global use
int wprnt = 0;

std::mt19937 rg;               // base random generator
std::uniform_real_distribution<> dis( 0.0 , 1.0 );

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

template< class T >
static void Str2Sthg( const char* const str , T &sthg )
{
 istringstream( str ) >> sthg;
 }

/*----------------------------------------------------------------------------

static double rndfctr( void )
{
 // return a random number between 0.5 and 2, with 50% probability of being
 // < 1
 double fctr = dis( rg ) - 0.5;
 return( fctr < 0 ? - fctr : fctr * 4 );
 }

------------------------------------------------------------------------------

static Subset GenerateRand( Index m , Index k )
{
 // generate a sorted random k-vector of unique integers in 0 ... m - 1

 Subset rnd( m );
 std::iota( rnd.begin() , rnd.end() , 0 );
 std::shuffle( rnd.begin() , rnd.end() , rg );
 rnd.resize( k );
 sort( rnd.begin() , rnd.end() );

 return( std::move( rnd ) );
 }

----------------------------------------------------------------------------*/

static void PrintResults( bool hs , int rtrn , double fo )
{
 if( hs )
  cout << fo;
 else
  if( rtrn == Solver::kInfeasible )
   cout << "    Unfeas";
  else
   if( rtrn == Solver::kUnbounded )
    cout << "      Unbounded";
   else
    cout << "      Error!";
 }
 
/*-------------------------------------------------------------------------*/

static void PrintSol( CDASolver * slvr , bool first ,
		      double time ,  double objFunc )
{
 if( ! wprnt )
  return;

 std::string name( globalArgv[ 1 ] );
 name = name.substr( name.find_last_of( "/" ) + 1 , name.length() );
 name = name.substr( 0 , name.find( "." ) );
 if( first )
  name.append( "_1" );
 else
  name.append( "_2" );

 // start: reduced costs extraction

 if( wprnt & 1 ) {
  slvr->get_dual_solution();

  ofstream solutionsFile( "./redCosts/" + name + "Sol-redCosts.dat" );
  for( Index k = 0 ; k < TestBlock->get_NComm() ; ++k ) {
   for( Index i = 0 ; i < TestBlock->get_NNodes() ; ++i )
    solutionsFile << TestBlock->get_potential( k , i ) << " ";
   solutionsFile << "\n";
   }

  solutionsFile.close();
  }
  
 // primal solution extraction 
 if( wprnt & 2 ) {
  slvr->get_var_solution();

  ofstream primalFile( "./primals/" + name + "-Prim.dat" );
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
 try {
  // solve with the 1st Solver- - - - - - - - - - - - - - - - - - - - - - - -
  auto Slvr1 = dynamic_cast< CDASolver *>(
			       TestBlock->get_registered_solvers().front() );
  if( ! Slvr1 ) {
   cout << "Error! First solver registred to TestBlock not a CDASolver";
   exit( 1 );
   }
  #if DETACH_1ST
   TestBlock->unregister_Solver( Slvr1 );
   TestBlock->register_Solver( Slvr1 , true );  // push it to the front
  #endif

  #if( LOG_LEVEL >= 3 )
   Slvr1->set_par( MILPSolver::strOutputFile , "LPBlock-CPXMILP.lp" );
  #endif

  auto start = std::chrono::system_clock::now();

  int rtrn1st = Slvr1->compute( false );
  bool hs1st = ( ( ( rtrn1st >= Solver::kOK ) && ( rtrn1st < Solver::kError )
                   && ( rtrn1st != Solver::kUnbounded )
                   && ( rtrn1st != Solver::kInfeasible ) )
                 || ( rtrn1st == Solver::kLowPrecision ) );
  double fo1st = hs1st ? Slvr1->get_var_value() : -INF;

  auto end = std::chrono::system_clock::now();
  std::chrono::duration< double > elapsed = end - start;
 
  PrintSol( Slvr1 , true , elapsed.count() , fo1st );

  #if( LOG_LEVEL >= 1 )
   cout.setf( ios::scientific, ios::floatfield );
   cout << setprecision( 2 ) << elapsed.count() << " - " << flush;
  #endif

  if( TestBlock->get_registered_solvers().size() == 1 ) {
   #if( LOG_LEVEL >= 1 )
    PrintResults( hs1st , rtrn1st , fo1st );
    cout << endl;
   #endif
   return( true );
   }

  // solve with the 2nd Solver- - - - - - - - - - - - - - - - - - - - - - - -
  auto Slvr2 = dynamic_cast< CDASolver *>(
			       TestBlock->get_registered_solvers().back() );
  if( ! Slvr2 ) {
   cout << "Error! Last solver registred to TestBlock not a CDASolver";
   exit( 1 );
   }
  #if DETACH_2ND
   TestBlock->unregister_Solver( Slvr2 );
   TestBlock->register_Solver( Slvr2 );  // push it to the back
  #endif

  start = std::chrono::system_clock::now();

  int rtrn2nd = Slvr2->compute( false );
  bool hs2nd = ( ( ( rtrn2nd >= Solver::kOK ) && ( rtrn2nd < Solver::kError )
                   && ( rtrn2nd != Solver::kUnbounded )
                   && ( rtrn2nd != Solver::kInfeasible ) )
                 || ( rtrn2nd == Solver::kLowPrecision ) );
  double fo2nd = hs2nd ? Slvr2->get_var_value() : -INF;

  end = std::chrono::system_clock::now();
  elapsed = end - start;

  PrintSol( Slvr2 , false , elapsed.count() , fo2nd );

  #if( LOG_LEVEL >= 1 )
   cout.setf( ios::scientific, ios::floatfield );
   cout << setprecision( 2 ) << elapsed.count();
  #endif

  if( hs1st && hs2nd && ( abs( fo1st - fo2nd ) <= 2e-7 *
			  max( double( 1 ) , max( abs( fo1st ) ,
						  abs( fo2nd ) ) ) ) ) {
   LOG1( " - OK(f)" << endl );
   return( true );
   }

  if( ( rtrn1st == Solver::kInfeasible ) &&
      ( rtrn2nd == Solver::kInfeasible ) ) {
   LOG1( " - OK(e)" << endl );
   return( true );
   }

  if( ( rtrn1st == Solver::kUnbounded ) &&
      ( rtrn2nd == Solver::kUnbounded ) ) {
   LOG1( " - OK(u)" << endl );
   return( true );
   }
    
  #if( LOG_LEVEL >= 1 )
   cout << " - " << setprecision( 7 );
   PrintResults( hs1st , rtrn1st , fo1st );
   cout << " - ";
   PrintResults( hs2nd , rtrn2nd , fo2nd );
   cout << endl;
  #endif

  return( false );
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

int main( int argc , char **argv )
{
 // reading command line parameters - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 
 globalArgv = argv;
 assert( SKIP_BEAT >= 0 );

 char filetype = 's';  // type of the input file;
 
 switch( argc ) {
  case( 4 ): wprnt = argv[ 3 ][ 0 ];  
  case( 3 ): filetype = argv[ 2 ][ 0 ];
  case( 2 ): break;
  default:   cerr << "Usage: " << argv[ 0 ] << " file_name [typ wprnt]"
		  << endl
		  << "        typ = s*, c, p, o, d, u, m (lower or uppercase)"  
		  << endl 
		  << "        wprnt: what print into a file, coded bit-wise [0]"
		  << endl 
		  << "         0 = nothing, 1 = duals,"
		  << endl
		  << "         2 = primal,  4 = time & objective value"
		  << endl;
             exit( 1 );
  }

 // read the Block- - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 TestBlock = new MMCFBlock;
 
 TestBlock->load( argv[ 1 ] , filetype );
 TestBlock->PreProcess();
 
 auto cfg = Configuration::deserialize( "BPar.txt" );
 if( BlockConfig * bc = dynamic_cast< BlockConfig * >( cfg ) )
  bc->apply( TestBlock );
 else {
  cerr << "Error: BPar.txt does not contain a BlockConfig" << endl;
  delete( cfg );
  exit( 1 );
  }
  
 delete( cfg );
 
 TestBlock->generate_abstract_variables();

 // attach the Solver(s) to the Block - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // do this by reading an appropriate BlockSolverConfig from file and
 // apply() it to the TestBlock; note that the BlockSolverConfig is
 // clear()-ed and kept to do the cleanup at the end

 BlockSolverConfig * bsc;
 {
  auto c = Configuration::deserialize( "BSPar.txt" );
  bsc = dynamic_cast< BlockSolverConfig * >( c );
  
  if( ! bsc ) {
   cerr << "Error: BSPar.txt does not contain a BlockSolverConfig" << endl;
   delete( c );
   exit( 1 );
   }

  bsc->apply( TestBlock );
  bsc->clear();

  if( TestBlock->get_registered_solvers().empty() ) {
   cout << endl << "no Solver registered to the Block!" << endl;
   exit( 1 );
   }
  }

 // open log-file - - - - - - - - - - -  - - - - - - - - - - - - - - - - - -
 //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 #if( LOG_LEVEL >= 2 )
  #if( LOG_ON_COUT )
   ((TestBlock->get_registered_solvers()).back())->set_log( &cout );
  #else
   ofstream LOGFile( logF , ofstream::out );
   if( ! LOGFile.is_open() )
    cerr << "Warning: cannot open log file """ << logF << """" << endl;
   else {
    LOGFile.setf( ios::scientific, ios::floatfield );
    LOGFile << setprecision( 10 );
    ((TestBlock->get_registered_solvers()).back())->set_log( &LOGFile );
    }
  #endif
 #endif

 // first solver call - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 bool AllPassed = SolveBoth();
 
 // main loop - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // now, for n_repeat times:
 // - up to n_change ... are ...
 // - up to n_change ... are ...
 // - up to n_change ... are ...
 // - up to n_change ... are ...
 //
 // then the TestBlock is re-solved with both Solver

 /*!!
 for( Index rep = 0 ; rep < n_repeat * ( SKIP_BEAT + 1 ) ; ) {
  LOG1( rep << ": ");

  // do stuff 1 - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 1 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = Index( dis( rg ) * n_change ) ) {
    LOG1( "... " << tochange << " ... - " );

    }

  // do stuff 2 - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 2 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = min( m - 1 , Index( dis( rg ) * n_change ) ) ) {
    LOG1( "... " << tochange << " ..." );

    
    if( dis( rg ) <= 0.5 ) {  // in 50% of the cases do a ranged change
     LOG1( "(r) - " );

     }
    else {  // in the other 50% of the cases, do a sparse change
     LOG1( "(s) - " );
     Subset nms( GenerateRand( m , tochange ) );

     }

    }

  // ...


  // if verbose, print out stuff- - - - - - - - - - - - - - - - - - - - - - -

  #if( LOG_LEVEL >= 3 )
   ((LPBlock->get_registered_solvers()).front())->set_par(
		                     MILPSolver::strOutputFile , "LPBlock-" +
		                     std::to_string( rep ) + ".lp" );
  #endif

  // finally, re-solve the problems- - - - - - - - - - - - - - - - - - - - -
  // ... every SKIP_BEAT + 1 rounds

  if( ! ( ++rep % ( SKIP_BEAT + 1 ) ) )
   AllPassed &= SolveBoth();
  #if( LOG_LEVEL >= 1 )
  else
   cout << endl;
  #endif

  }  // end( main loop )- - - - - - - - - - - - - - - - - - - - - - - - - - -
     // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     !!*/

 if( AllPassed )
  cout << GREEN( All tests passed!! ) << endl;
 else
  cout << RED( Shit happened!! ) << endl;
 
 // destroy the Block - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 // apply() the clear()-ed BlockSolverConfig to cleanup Solver
 bsc->apply( TestBlock );

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
