/*--------------------------------------------------------------------------*/
/*-------------------------- File test.cpp ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Main for testing the capacity of an AbstractBlock to solve a quadratic 
 * problem.
 *
 * \author Enrico Calandrini \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Enrico Calandrini
 */
/*--------------------------------------------------------------------------*/
/*-------------------------------- MACROS ----------------------------------*/
/*--------------------------------------------------------------------------*/

#define LOG1( x ) cout << x
#define CLOG1( y , x ) if( y ) cout << x

#define LOG_LEVEL 0
// 0 = only final result of the model
// 1 = + save LP file

/*--------------------------------------------------------------------------*/

#define PANIC( x ) if( ! ( x ) ) PANICMSG

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

#include "AbstractBlock.h"

#include "BlockSolverConfig.h"

#include "MILPSolver.h"

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace std;

using namespace SMSpp_di_unipi_it;

using FunctionValue = Function::FunctionValue;

/*--------------------------------------------------------------------------*/
/*-------------------------------- TYPES -----------------------------------*/
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/*------------------------------- CONSTANTS --------------------------------*/
/*--------------------------------------------------------------------------*/

const FunctionValue INF = SMSpp_di_unipi_it::Inf< FunctionValue >();

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/

AbstractBlock * LPBlock;   // the problem expressed as an LP

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/

template< class T >
static void Str2Sthg( const char* const str , T& sthg )
{
 istringstream( str ) >> sthg;
 }

/*--------------------------------------------------------------------------*/

static bool search_opt( std::string file_name , double* opt_value , char type )
{
  // Read all the available optimal solution from the file list_problems.txt
  std::ifstream file;
  file.open( "available_instances.txt" );
  auto max = std::numeric_limits< std::streamsize >::max();


  /*---------------------------------------*/
  /* HERE WE ARE STARTING TO READ THE FILE */
  /*---------------------------------------*/
  
  // Eat initial comments
  while( file.peek() == file.widen( '\\' )  || 
            file.peek() ==  '\n' ) {
    file.ignore( max, '\n' );
  }

  std::string word;
  file >> word;
  bool opt_found = false;

  // Now we should be reading lp files name until we reach the END word
  while( word != "END" ) {
    if( word == file_name ) {
      // We found the right file

      // If we want the optimal value of the continuous relaxation, skip the
      // first entry (solution of integer version)
      if( type == 'C')
        file >> word;

      std::string value;
      file >> value;
      if( value == "?" )
        // The instance is available but the optimal value is unknown
        return( false );

      *opt_value = std::stod( value );
      opt_found = true;
      word = "END";
    }
    else{
      file.ignore( max, '\n' );
      file >> word;
    }
  }

  return( opt_found );
 }

/*--------------------------------------------------------------------------*/

static bool SolveModel( bool is_found , double opt_value ) 
{
 try {
  // solve the LPBlock- - - - - - - - - - - - - - - - - - - - - - - - - - - -
  Solver * slvrLP = ( LPBlock->get_registered_solvers() ).front();
  int rtrnLP = slvrLP->compute( false );
  bool hsLP = ( ( rtrnLP >= Solver::kOK ) && ( rtrnLP < Solver::kError ) )
              || ( rtrnLP == Solver::kLowPrecision );
  double foLP = hsLP ? ( slvrLP->get_ub() ) : ( INF );

  if( is_found )
    LOG1( "The known optimal solution of the model is : " << opt_value << endl );
  else
    LOG1( "No known optimal solution available" << endl );
  
  if( rtrnLP == Solver::kStopTime ) {
    double boundLP = hsLP ? ( slvrLP->get_lb() ) : ( INF );
    LOG1( "Optimization stopped due to time limit." << endl );

    if( opt_value == 0 && abs(foLP) <= 1e-4 ) {
      LOG1( "The best solution found is " << foLP << endl );

      return( true );
    }
    
    LOG1( "The best solution found is " << foLP << " and the best bound is " 
      << boundLP << endl );

    if( ( opt_value <= foLP + abs(foLP)*1e-4 && opt_value >= boundLP ) || 
          ( opt_value >= foLP - abs(foLP)*1e-4 && opt_value <= boundLP ) )
      return( true );
    return( false );
  }
  
  if( hsLP ) {
   LOG1( "The returned value of the solution found is : " << foLP << endl );
   
   if( is_found && abs( foLP - opt_value ) >= 1e-3 * std::max( double( 1 ) , 
        std::max( abs( foLP ) , abs( opt_value ) ) ) )
    return( false );
   return( true );
  }

  if( rtrnLP == Solver::kInfeasible ) {
    LOG1( "The model hs been proven to be infeasible" << endl );

    if( is_found )
      return( false );
    else
      return( true );
    }

  if( rtrnLP == Solver::kUnbounded ) {
   LOG1( "The model hs been proven to be unbounded" << endl );
   
   if( is_found )
      return( false );
   return( true );
  }

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
/// Custom terminate function to print the exception message

void smspp_terminate( void ) {

 std::cerr << "Uncaught exception in executing SMS++:\n";
 try {
  std::rethrow_exception( std::current_exception() );
 }
 catch( const std::exception & e ) {
  std::cerr << "\tException type: " << typeid( e ).name() << "\n";
  std::cerr << "\tException message: " << e.what() << "\n";
 } catch( ... ) {
  std::cerr << "\tUnknown exception" << std::endl;
 }
 std::abort(); // or exit(1)
}

/*--------------------------------------------------------------------------*/

int main( int argc , char **argv )
{
 // override the default terminate handler to print the exception message
 std::set_terminate( smspp_terminate );

 // reading command line parameters - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 cout.setf( ios::scientific, ios::floatfield );
 cout << setprecision( 10 );

 std::string file_name;
 double opt_value = 0;
 char type = 'I';
 bool is_opt_found = false;

 // construction and loading of the objects - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 switch( argc ) {
  case( 4 ): Str2Sthg( argv[ 3 ] , opt_value );
    is_opt_found = true;
  case( 3 ): Str2Sthg( argv[ 2 ] , type );
  case( 2 ): Str2Sthg( argv[ 1 ] , file_name );
             break;
  default: cerr << "Usage: " << argv[ 0 ] <<
	   " file_name : name of the lp file to be loaded"
    << std::endl <<
	   "       type: solve the Integer version or the Continuous Relaxation [I]"
    << std::endl <<
     "       opt_value: optimal known value of the model [0]"
	        << std::endl;
	   return( 1 );
  }

 if( argc < 4 ) {
  // We have to retrieve optimal value, if available
  is_opt_found = search_opt( file_name , &opt_value , type );
 }

 // construct the LP problem by reading the mps file specified  - - - - - - - 
 {
  LPBlock = new AbstractBlock();
  
  std::ifstream file;
  file.open( file_name );
  LPBlock->load( file , 'L' );
 }

 // attach the Solver to the Block- - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // for both Block do this by reading an appropriate BlockSolverConfig from
 // file and apply() it to the Block; note that the BlockSolverConfig are
 // clear()-ed and kept to do the cleanup at the end

 BlockSolverConfig * lpbsc;
 {
  auto c = ( type == 'C' ) ? Configuration::deserialize( "LPPar_C.txt" ) :
    Configuration::deserialize( "LPPar_I.txt" );
  lpbsc = dynamic_cast< BlockSolverConfig * >( c );
  if( ! lpbsc ) {
   cerr << "Error: LPPar.txt does not contain a BlockSolverConfig" << endl;
   delete( c );
   exit( 1 );
   }
  }

 lpbsc->apply( LPBlock );
 lpbsc->clear();

 // Solve the continuous relaxation, if required
 if( type == 'C' )
  ( ( LPBlock->get_registered_solvers() ).front() )->set_par(
	                         MILPSolver::intRelaxIntVars , 1 );

 // open log-file - - - - - - - - - - -  - - - - - - - - - - - - - - - - - -
 //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // write the .lp of the model loaded in order to compare it with the
 // previous one
 #if( LOG_LEVEL >= 1 )
    ( ( LPBlock->get_registered_solvers() ).front() )->set_par(
	                         MILPSolver::strOutputFile , "Read_Model.lp" );
 #endif

 // first solver call - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 bool is_solved = SolveModel( is_opt_found , opt_value );

 if( is_solved )
  cout << GREEN( Test passed!! ) << endl;
 else
  cout << RED( Shit happened!! ) << endl;
 
 // destroy the Block - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 // apply() the clear()-ed BlockSolverConfig to cleanup Solver
 lpbsc->apply( LPBlock );

 // then delete the BlockSolverConfig
 delete( lpbsc );

 // delete the Blocks
 delete( LPBlock );

 // terminate - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 return( is_solved ? 0 : 1 );

 }  // end( main )

/*--------------------------------------------------------------------------*/
/*------------------------- End File test.cpp ------------------------------*/
/*--------------------------------------------------------------------------*/
