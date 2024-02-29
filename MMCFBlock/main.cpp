/*--------------------------------------------------------------------------*/
/*----------------------------- File main.cpp ------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 *
 * Tester for MMCFBlock. It loads an MMCF instance from file, taking the
 * filename and the type from the command line, into both a MMCFBlock with
 * an appropriate Solver (say, CPXMILPSolver) attached, and into a
 * MMCFCplex object derived from MMCFClass. It solves the instance with
 * both and compares the results.
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
/*-------------------------------- MACROS ----------------------------------*/
/*--------------------------------------------------------------------------*/

#define LOG_LEVEL 0
// 0 = only pass/fail
// 1 = + solver log

/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <iostream>
#include <fstream>
#include <iomanip>

#include "MMCFBlock.h"
#include "BlockSolverConfig.h"

#include "Graph.h"
#include "MMCFCple.h"

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

using namespace MMCFClass_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*----------------------------- FUNCTIONS ----------------------------------*/
/*--------------------------------------------------------------------------*/

/*!!
template< class T >
static inline void str2val( const char* const str , T &sthg )
{
 istringstream( str ) >> sthg;
 }
 !!*/

/*--------------------------------------------------------------------------*/

 static void PrintResults( int rtrn , double lb , double ub )
{
 cout << "MMCFB: ";
 if( ( rtrn >= Solver::kOK ) && ( rtrn < Solver::kError ) )
  cout << std::setprecision( 8 ) <<   lb << ", " << ub;
 else
  if( rtrn == Solver::kInfeasible )
   cout << "Unfeas";
  else
   if( rtrn == Solver::kUnbounded )
    cout << "Unbounded";
   else
    cout << "Error!";
 }

/*--------------------------------------------------------------------------*/
/*----------------------------- CONSTANTS ----------------------------------*/
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
/*--------------------------------- Main -----------------------------------*/
/*--------------------------------------------------------------------------*/

int main( int argc , char **argv )
{
 // reading command line parameters - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 if( argc < 2 ) {
  cerr << "Usage: " << argv[ 0 ] << " file_name [typ]" << endl
       << "        typ = s*, c, p, o, d, u, m (lower or uppercase)" << endl;
  return( 1 );
  }

 char filetype = 's';  // type of the input file;
 if( argc > 2 )
  filetype = argv[ 2 ][ 0 ];

 // read the Block- - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 auto MMCFb = new MMCFBlock;
 MMCFb->load( argv[ 1 ] , filetype );
 MMCFb->PreProcess();

 {
  auto cfg = Configuration::deserialize( "BPar.txt" );
  if( BlockConfig * bc = dynamic_cast< BlockConfig * >( cfg ) )
   bc->apply( MMCFb );
  else {
   cerr << "Error: BPar.txt does not contain a BlockConfig" << endl;
   exit( 1 );
   }
  delete( cfg );
  }

 MMCFb->generate_abstract_variables();

 // attach the Solver to the Block- - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 BlockSolverConfig * bsc;
 {
  auto c = Configuration::deserialize( "BSPar.txt" );
  bsc = dynamic_cast< BlockSolverConfig * >( c );
  if( ! bsc ) {
   cerr << "Error: configuration file not a BlockSolverConfig" << endl;
   delete( c );
   exit( 1 );
   }

  bsc->apply( MMCFb );
  bsc->clear();
  }

 if( MMCFb->get_registered_solvers().empty() ) {
  cout << endl << "no Solver registered to the Block!" << endl;
  exit( 1 );
  }

 Solver * slvr = (MMCFb->get_registered_solvers()).front();

 // open log-file - - - - - - - - - - -  - - - - - - - - - - - - - - - - - -
 //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 #if LOG_LEVEL > 0
  ofstream LOGFile( "logMMCFB.txt" , ofstream::out );
  if( ! LOGFile.is_open() )
   cerr << "Warning: cannot open log file logMMCFB.txt" << endl;
  else
   slvr->set_log( &LOGFile );
 #endif

 std::clock_t c_start = std::clock();
 int rtrn = slvr->compute( false );
 double lb_value = slvr->get_lb();
 double ub_value = slvr->get_ub();
 double time = double( std::clock() - c_start ) / double( CLOCKS_PER_SEC );

 // cleanup MMCFBlock and its Solver
 bsc->apply( MMCFb );
 delete( bsc );
 delete( MMCFb );

 // read and modify the problem - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 Graph *Gh = new Graph( argv[ 1 ] , filetype );
 Gh->PreProcess();

 // allocate the solver - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 MMCFCplex *mmcf = new MMCFCplex( Gh , nullptr );

 // set tolerance  - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 mmcf->SetCplexParam( CPX_PARAM_EPOPT , 1e-8 );
 mmcf->SetCplexParam( CPX_PARAM_EPGAP , 1e-8 );

 // pass the number of threads - - - - - - - - - - - - - - - - - - - - - - -

 mmcf->SetCplexParam( CPX_PARAM_THREADS , 1 );

 // pass Log- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 #if LOG_LEVEL > 0
  ofstream LOGCPX( "logMMCFC.txt" , ofstream::out );
  if( ! LOGCPX.is_open() )
   cerr << "Warning: cannot open log file logMMCFC.txt" << endl;
  else
   mmcf->SetMMCFLog( &LOGCPX , 2 );
 #endif

 // free some memory- - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 delete( Gh );

 // set the timers on - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 mmcf->SetMMCFTime();

 // solve the problem - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 MMCFClass::MMCFStatus Status = mmcf->SolveMMCF();

 // get the results - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 double tu , ts;
 mmcf->TimeMMCF( tu , ts );       // get the running time

 MMCFClass::FONumber OV1 = mmcf->GetPVal();      // get the primal value
 MMCFClass::FONumber OV2 = mmcf->GetDVal();      // get the dual value

 // clean up- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 delete( mmcf );

 // output the results- - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 cout << time << " - " << tu + ts  << ": ";

 switch( Status ) {
  case( MMCFClass::kStopped ):
  case( MMCFClass::kOK ):
   if( ( ( rtrn >= Solver::kOK ) && ( rtrn < Solver::kError ) ) &&
       ( abs( lb_value - OV1 ) <= 1e-8 *
	 max( double( 1 ) , max( abs( lb_value ) , abs( OV1 ) ) ) ) &&
       ( abs( ub_value - OV2 ) <= 1e-8 *
	 max( double( 1 ) , max( abs( ub_value ) , abs( OV2 ) ) ) ) ) {
    cout << "OK(f) - " << GREEN( Test passed!! ) << endl;
    return( 0 );
    }

   PrintResults( rtrn , lb_value , ub_value );
   cout << "- MMCFC: " << OV1 << ", "<< OV2 << " - ";
   break;
  case( MMCFClass::kUnfeasible ):
   if( rtrn == Solver::kInfeasible ) {
    cout << "OK(e) - " << GREEN( Test passed!! ) << endl;
    return( 0 );
    }

   PrintResults( rtrn , lb_value , ub_value );
   cout << "- MMCFC: Unfeas - ";
   break;
  case( MMCFClass::kUnbounded ) :
   if( rtrn == Solver::kUnbounded ) {
    cout << "OK(u) - " << GREEN( Test passed!! ) << endl;
    return( 0 );
    }

   PrintResults( rtrn , lb_value , ub_value );
   cout << "- MMCFC: Unbounded - ";
   break;
  default :
   PrintResults( rtrn , lb_value , ub_value );
   cout << "- MMCFC: Error! - ";
   }

 cout << RED( Shit happened!! ) << endl;
 return( 1 );
 }

/*--------------------------------------------------------------------------*/
/*------------------------- End File main.cpp ------------------------------*/
/*--------------------------------------------------------------------------*/

