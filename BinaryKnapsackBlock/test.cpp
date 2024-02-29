/*--------------------------------------------------------------------------*/
/*--------------------------- File test.cpp --------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Main for testing BinaryKnapsackBlock, comparing the results of two 
 * different Solvers attached to it.
 *
 * \author Federica Di Pasquale \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \author Antonio Frangioni \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 */
/*--------------------------------------------------------------------------*/
/*------------------------------ MACROS ------------------------------------*/
/*--------------------------------------------------------------------------*/

#define STEP 3  // after modifications solve again at each multiple of STEP

#define LOG_LEVEL 0
// 0 = only pass/fail
// 1 = list of modifications
// 2 = also print verbose header about main configuration at start

#if( LOG_LEVEL > 0 )
 #define LOG( x ) cout << x
#else
 #define LOG( x )
#endif

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

#include "BinaryKnapsackBlock.h"

#include "BlockSolverConfig.h"

#include <random>

/*--------------------------------------------------------------------------*/
/*------------------------------- USING ------------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace std;

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*------------------------------- TYPES ------------------------------------*/
/*--------------------------------------------------------------------------*/

using Index = Block::Index;
using c_Index = Block::c_Index;

using Range = Block::Range;
using c_Range = Block::c_Range;

using Subset = Block::Subset;
using c_Subset = Block::c_Subset;

/*--------------------------------------------------------------------------*/
/*----------------------------- CONSTANTS ----------------------------------*/
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/*------------------------------ GLOBALS -----------------------------------*/
/*--------------------------------------------------------------------------*/

BinaryKnapsackBlock * BKB;          // The Binary Knapsack Block

std::mt19937 rg;                       // random generator
std::uniform_real_distribution<> dis( 0.0 , 1.0 );

Index N = 100;                         // number of items

static constexpr Index rangeW = 100;   // range values of weights
static constexpr double rangeP = 100;  // range values of profits

/*--------------------------------------------------------------------------*/
/*----------------------------- FUNCTIONS ----------------------------------*/
/*--------------------------------------------------------------------------*/

template< class T >
static void Str2Sthg( const char * str , T & sthg ) {
 istringstream( str ) >> sthg;
 }

/*--------------------------------------------------------------------------*/
// Generate a random Range of size m < N

Range generateRange( Index m )
{
 Range rng;
 rng.first = dis( rg ) * ( N - m );
 rng.second = rng.first + m;
 return( rng );
 }

/*--------------------------------------------------------------------------*/
// Generate a random Subset of size m < N

Subset generateSubset( Index m )
{
 Subset nms;
 
 Subset idx( N );            
 iota( idx.begin() , idx.end() , 0 );
 
 sample( idx.begin() , idx.end() , back_inserter( nms ) , m , rg );
 
 return( nms );
 }

/*--------------------------------------------------------------------------*/

bool SolveBoth( void )
{
 // get the two Solvers - - - - - - - - - - - - - - - - - - - - - - - - - -
 // sort of assuming they are different, although they may not be
 auto Solver1 = BKB->get_registered_solvers().front();
 auto Solver2 = BKB->get_registered_solvers().back();

 // solve with both Solvers - - - - - - - - - - - - - - - - - - - - - - - -

 #if( LOG_LEVEL >= 1 )
  auto start = std::chrono::system_clock::now();
 #endif

 auto status1 = Solver1->compute();     

 #if( LOG_LEVEL >= 1 )
  auto end = std::chrono::system_clock::now();
  std::chrono::duration< double > elapsed = end - start;
  cout.setf( ios::scientific, ios::floatfield );
  cout << setprecision( 2 ) << elapsed.count();
  start = std::chrono::system_clock::now();
 #endif

 auto status2 = Solver2->compute();     

 #if( LOG_LEVEL >= 1 )
  end = std::chrono::system_clock::now();
  elapsed = end - start;
  cout.setf( ios::scientific, ios::floatfield );
  cout << setprecision( 2 ) << " - " << elapsed.count() << " - ";
 #endif

 // check status- - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  

 if( ( status1 == Solver::kInfeasible ) &&
     ( status2 == Solver::kInfeasible ) ) {
  LOG( "OK(u)" );
  return( true );
  }

 if( ( status1 == Solver::kInfeasible ) ||
     ( status2 == Solver::kInfeasible ) ) {
  cout << "Error: Solver1 ";
  if( status1 == Solver::kInfeasible ) cout << "in";
  cout << "feasible but Solver2 ";
  if( status2 == Solver::kInfeasible ) cout << "in";
  cout << "feasible";
  return( false );
  }

 // get optimal values- - - - - - - - - - - - - - - - - - - - - - - - - - -

 double Value1 = Solver1->get_var_value(); 

 double Value2 = Solver2->get_var_value();  

 // get solutions - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 Solver1->get_var_solution();
 
 double checksol = 0;
 for( Index i = 0 ; i < N ; ++i )
  checksol += BKB->get_x( i ) * BKB->get_Profit( i );

 if( abs( checksol - Value1 ) > 1e-06 ) {
  cerr << "Error computing solution Solver1: " << "checksol " << checksol
       << " != Value1 " << Value1;
  return( false );
  }

 Solver2->get_var_solution();

 checksol = 0;
 for( Index i = 0 ; i < N ; ++i )
  checksol += BKB->get_x( i ) * BKB->get_Profit( i );
 
 if( abs( checksol - Value2 ) > 1e-06 ) {
  cerr << "Error computing solution Solver2: " << "checksol " << checksol
       << " != Value2 " << Value2;
  return( false );
  }

 // compare optimal values- - - - - - - - - - - - - - - - - - - - - - - - - 
 
 double gap = ( Value2 - Value1 ) / max( abs( Value1 ) , 1.0 ) ;
 if( abs( gap ) < 2e-06 ) {
  LOG( "OK(f)" );
  return( true );
  }
 
 cout << "Error: Value1 = " << Value1 << ", Value2 = " << Value2;
 return( false );     
 } 

/*--------------------------------------------------------------------------*/

int main( int argc , char **argv )
{
 // reading command line parameters - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 long int seed = 123123;                // seed
 Index wchg = 127;                      // what to change, coded bit-wise
 Index n_repeat = 100;                  // number of repetitions
 double delta = 0.01;                   // capacity parameter
 double nW = 0.1;                       // percentage of negative weights
 double nP = 0.1;                       // percentage of positive weights
 double nI = 0.5;                       // percentage of integer variables
 double nM = 0.2;                       // max percentage of items to modify
 // for small knapsacks nM * N may be too small (always 0 or 1 at most)
 // minM defines the minimum absolute number of items that can be modified
 int minM = 10;

 switch( argc ) {
  case( 10 ): Str2Sthg( argv[ 9 ] , nM );
  case( 9 ): Str2Sthg( argv[ 8 ] , nI );
  case( 8 ): Str2Sthg( argv[ 7 ] , nP );
  case( 7 ): Str2Sthg( argv[ 6 ] , nW );
  case( 6 ): Str2Sthg( argv[ 5 ] , delta );
  case( 5 ): Str2Sthg( argv[ 4 ] , n_repeat );
  case( 4 ): Str2Sthg( argv[ 3 ] , N );
  case( 3 ): Str2Sthg( argv[ 2 ] , wchg );
  case( 2 ): Str2Sthg( argv[ 1 ] , seed );
             break;
  default: cerr << "Usage: " << argv[ 0 ]
		<< " seed [wchg N n_repeat delta nW nP nI nM]"
        << endl << "       wchg: what to change, coded bit-wise [127]"
	<< endl << "             1 = change sense, 2 = change capacity "
        << endl << "             3 = change profits, 4 = change weights"
        << endl << "             5 = fix, 6 = unfix, 7 = change integrality"
	<< endl << "       N: number of variables [100]"
        << endl << "       n_repeat: number of repetitions [100]"
        << endl << "       delta: Capacity parameter [0.01]"
        << endl << "       nW: percentage of negative weights [0.1]"
        << endl << "       nP: percentage of negative profits [0.1]"
        << endl << "       nI: percentage of integer variables [0.5]"
        << endl << "       nM: max percentage of items to modify [0.2]"
        << endl; 
   return( 1 );
  }

 // sanity checks - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 
 if( ( delta < 0 ) || ( delta > 1 ) ) {
  cerr << "error: delta must be in [ 0 , 1 ]" << endl;
  exit( 1 ); 
  }

 if( ( nW < 0 ) || ( nW > 1 ) ) {
  cerr << "error: nW must be in [ 0 , 1 ]" << endl;
  exit( 1 );
  }

 if( ( nP < 0 ) || ( nP > 1 ) ) {
  cerr << "error: nP must be in [ 0 , 1 ]" << endl;
  exit( 1 );
  }

 if( ( nI < 0 ) || ( nI > 1 ) ) {
  cerr << "error: nI must be in [ 0 , 1 ]" << endl;
  exit( 1 );
  }

 if( ( nM < 0 ) || ( nM > 1 ) ) {
  cerr << "error: nI must be in [ 0 , 1 ]" << endl;
  exit( 1 );
  }

 const int minW =  - int( nW * rangeW );
 const int maxW = minW + rangeW;

 const double minP = - nP * rangeP;
 const double maxP = minP + rangeP;

 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // seed the pseudo-random number generator     

 rg.seed( seed );

 // print verbose header- - - - - - - - - - - - - - - - - - - - - - - - - - - 

 #if( LOG_LEVEL > 1 )
  cout << "seed = " << seed << " ~ N = " << N << " ~ n_repeat " << n_repeat
       << " ~ delta = " << delta << endl;
  cout << "W in [ " << minW << " , " << maxW << " ] ~ P in [ " << minP
       << " , " << maxP << " ] ~ nI = " << nI << endl;

  cout << endl << "Modifications: " << endl;
  if( wchg & 1 )                   
   cout << " - Objective Sense" << endl;
  if( wchg & 2 )
   cout << " - Capacity" << endl; 
  if( wchg & 4 )                   
   cout << " - Profits" << endl;
  if( wchg & 8 )
   cout << " - Weights" << endl; 
  if( wchg & 16 )                   
   cout << " - Fix" << endl;
  if( wchg & 32 )
   cout << " - Unfix" << endl;
  if( wchg & 64 )
   cout << " - Integrality";
  cout << endl << endl;
 #endif
 
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // create the BinaryKnapsackBlock- - - - - - - - - - - - - - - - - - - - - -

 BKB = new BinaryKnapsackBlock();            

 // generate instance - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

 // generate weights from a uniform int distribution
 uniform_int_distribution<> dist_W( minW , maxW );
 
 // generate integrality from a uniform int distribution
 uniform_int_distribution<> dist_I( 0 , 1 );

 // generate profits from a uniform real distribution
 uniform_real_distribution<> dist_P( minP , maxP );

 vector< double > W( N );             // vector of weights
 vector< double > P( N );             // vector of profits
 vector< bool > I( N );               // vector of integrality
 double C;                            // Capacity of the Knapsack

 int totWp = 0;                       // total sum of the positive weights
 int totWn = 0;                       // total sum of the negative weights

 for( Index i = 0 ; i < N ; i++ ) {
  W[ i ] = dist_W( rg );      
  P[ i ] = dist_P( rg );
  I[ i ] = ( dist_I( rg ) < nI );   
  if( W[ i ] > 0 )                     // update totWn and totWp
   totWp += W[ i ];      
  else
   totWn += W[ i ];      
  }
 
 // generate the Capacity from a uniform real distribution
 uniform_real_distribution<> dist_C( totWn , 
                                     totWn + delta * ( totWp - totWn ) );
 C = dist_C( rg );

 // load the Binary Knapsack instance- - - - - - - - - - - - - - - - - - - -
 
 if( nI < 1 )
  BKB->load( N , C , std::move( W ) , std::move( P ) , std::move( I ) );
 else
  BKB->load( N , C , std::move( W ) , std::move( P ) );
 
 // attach two Solver to the BinaryKnapsackBlock- - - - - - - - - - - - - - - 
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // do it by using a single a BlockSolverConfig, read from file
 
 auto bsc = dynamic_cast< BlockSolverConfig * >(
		     Configuration::deserialize( "BinaryKnapsackPar.txt" ) );
 if( ! bsc ) {
  cerr << "Error: configuration file not a BlockSolverConfig" << endl;
  exit( 1 );    
  }

 bsc->apply( BKB );
 bsc->clear();  // keep the clear()-ed BlockSolverConfig for final cleanup

 // check Solvers - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 if( BKB->get_registered_solvers().empty() ) {
  cerr << "Error: BlockSolverConfig did not register any Solver" << endl;
  exit( 1 );    
  }
  
 // get Objective and Constraint- - - - - - - - - - - - - - - - - - - - - - -
 
 auto obj = BKB->get_objective< FRealObjective >();
 
 auto cnst = BKB->get_static_constraint< FRowConstraint >( 0 );

 // get the corresponding linear functions

 auto lfobj = dynamic_cast< LinearFunction * >( obj->get_function() );
 if( ! lfobj ) {
  cerr << "Error: cannot get the Objective LinearFunction" << endl;
  exit( 1 ); 
  }

 auto lfcnst = dynamic_cast< LinearFunction * >( cnst->get_function() );
 if( ! lfcnst ) {
  cerr << "Error: cannot get the Constraint LinearFunction" << endl;
  exit( 1 ); 
  }

 // first call- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 LOG( "0: " );
 bool AllPassed = SolveBoth();
 
 // modifications loop- - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 for( Index i = 1 ; i <= n_repeat * STEP ; ++i ) {
  LOG( endl << i << ": " );
  
  // change the sense of the objective - - - - - - - - - - - - - - - - - - -
  if( ( wchg & 1 ) && ( dis( rg ) < 0.3 ) ) {
   LOG( "sense" );

   if( dis( rg ) < 0.5 )
    BKB->set_objective_sense( 1 - BKB->get_objective_sense() );   // PR
   else {
    obj->set_sense( 1 - BKB->get_objective_sense() );             // AR
    LOG( "(A)" );
    }
   LOG( " ~ " );
   }

  // change the Capacity of the Knapsack- - - - - - - - - - - - - - - - - - -
  if( wchg & 2 && dis( rg ) < 0.3 ) {
   LOG( "C" );

   C = dist_C( rg );
   
   if( dis( rg ) < 0.5 )
    BKB->chg_capacity( C );     // PR
   else {
    cnst->set_rhs( C );         // AR
    LOG( "(A)" );
    }
   LOG( " ~ " );
   }                   

  // change Profits (range or subset) - - - - - - - - - - - - - - - - - - - -
  if( wchg & 4 && dis( rg ) < 0.3 ) {

   Index m = dis( rg ) * max( int( nM * N ) , minM ); // n. of items to modify
   m = min( m , N );
   if( m ) {
    LOG( "P" );

    vector< double > nP( m );                 // generate new profits
    for( auto & p : nP )  
     p = dist_P( rg ); 

    if( dis( rg ) < 0.5 ) {                   // ranged modification
     Range rng = generateRange( m );

     if( dis( rg ) < 0.5 ) {
      BKB->chg_profits( nP.begin() , rng );              // PR
      LOG( "(R)" );
      }
     else {
      lfobj->modify_coefficients( std::move( nP ) , rng );    // AR
      LOG( "(AR)" );
      }
     }
    else {                                    // or subset modification
     Subset nms = generateSubset( m ); 
     if( dis( rg ) < 0.5 ) {
      BKB->chg_profits( nP.begin() , std::move( nms ) );              // PR
      LOG( "(S)" );
      }
     else {
      lfobj->modify_coefficients( std::move( nP ) , std::move( nms ) );    // AR
      LOG( "(AS)" );
      }
     }
    LOG( " ~ " );
    }                   
  }
  // change Weights (range or subset) - - - - - - - - - - - - - - - - - - - -
  if( wchg & 8 && dis( rg ) < 0.3 ) {

   Index m = dis( rg ) * max( int( nM * N ) , minM ); // n. of items to modify
   m = min( m , N );
   if( m ) {
    LOG( "W" );

    vector< double > nW( m );                 // generate new weights
    for( auto & w : nW )  
     w = dist_W( rg );

    if( dis( rg ) < 0.5 ) {                   // ranged modification
     Range rng = generateRange( m );

     if( dis( rg ) < 0.5 ) {
      BKB->chg_weights( nW.begin() , rng );              // PR
      LOG( "(R)" );
      }
     else {
      lfcnst->modify_coefficients( std::move( nW ) , rng );   // AR
      LOG( "(AR)" );
      }
     }
    else {                                    // or subset modification
     Subset nms = generateSubset( m ); 

     if( dis( rg ) < 0.5 ) {
      BKB->chg_weights( nW.begin() , std::move( nms ) );              // PR
      LOG( "(S)" );
      }
     else {
      lfcnst->modify_coefficients( std::move( nW ) , std::move( nms ) );   // AR
      LOG( "(AS)" );
      }
     }
    LOG( " ~ " );
    }
   }
   
  // Fix (range or subset)- - - - - - - - - - - - - - - - - - - - - - - - - -
  if( wchg & 16 && dis( rg ) < 0.3 ) {

   Index m = dis( rg ) * max( int( nM * N ) , minM ); // n. of items to modify
   m = min( m , N );
   if( m ) {
    LOG( "F" );

    vector< bool > nX( m );
    for( Index i = 0 ; i < m ; i++ )          // generate new x values
     nX[ i ] = ( dis( rg ) < 0.5 ) ? false : true;
    auto nXit = nX.begin();

    if( dis( rg ) < 0.5 ) {                   // ranged modification
     Range rng = generateRange( m );

     if( dis( rg ) < 0.5 ) {                  // PR 
      BKB->fix_x( nXit , rng ); 
      LOG( "(R)" );
      }
     else {                                   // AR
      for( Index j = rng.first ; j < rng.second ; j++ ) {
       auto x = BKB->get_Var( j );
       if( ! x->is_fixed() ) {
	x->set_value( *nXit++ );
	x->is_fixed( true );   
        }
       }
      LOG( "(AR)" );
      }
     }
    else {                                    // or subset modification
     Subset nms = generateSubset( m ); 

     if( dis( rg ) < 0.5 ) {                 // PR
      BKB->fix_x( nXit , std::move( nms ) );
      LOG( "(S)" );
      }
     else {                                  // AR
      for( auto j : nms ) {
       auto x = BKB->get_Var( j );
       if( ! x->is_fixed() ) {
	x->set_value( *nXit++ );
	x->is_fixed( true );   
        }
       }
      LOG( "(AS)" );
      }
     }
    LOG( " ~ " );
    }
   }
  // Unfix (range or subset)- - - - - - - - - - - - - - - - - - - - - - - - -
  if( wchg & 32 && dis( rg ) < 0.3 ) {

   Index m = dis( rg ) * max( int( nM * N ) , minM ); // n. of items to modify
   m = min( m , N );
   if( m ) {
    LOG( "U" );

    if( dis( rg ) < 0.5 ) {                   // ranged modification
     Range rng = generateRange( m );

     if( dis( rg ) < 0.5 ) {                  // PR
      BKB->unfix_x( rng );
      LOG( "(R)" );
      }
     else {                                   // AR
      for( Index j = rng.first ; j < rng.second ; j++ )
       BKB->get_Var( j )->is_fixed( false );
      LOG( "(AR)" );
      }
     }
    else {                                    // or subset modification    
     Subset nms = generateSubset( m ); 

     if( dis( rg ) < 0.5 ) {                  // PR
      BKB->unfix_x( std::move( nms ) );
      LOG( "(S)" );
      }
     else {                                   // AR
      for( auto j : nms )
       BKB->get_Var( j )->is_fixed( false );
      LOG( "(AS)" );
      }
     }
    LOG( " ~ " );
    }
   }
  // change Integrality (range or subset) - - - - - - - - - - - - - - - - - -
  if( wchg & 64 && dis( rg ) < 0.3 ) {

   Index m = dis( rg ) * max( int( nM * N ) , minM ); // n. of items to modify
   m = min( m , N );
   if( m ) {
    LOG( "I" );

    vector< bool > nI( m );               // generate new integrality vector
    for( Index i = 0 ; i < m ; i++ )
     nI[ i ] = ( dist_I( rg ) <= 0.5 );

    if( dis( rg ) < 0.5 ) {               // ranged modification
     Range rng = generateRange( m );

     if( dis( rg ) < 0.5 ) {              // PR
      BKB->chg_integrality( nI.begin() , rng );
      LOG( "(R)" );
      }
     else {                               // AR
      auto nIit = nI.begin();
      for( Index j = rng.first ; j < rng.second ; ++j )
       BKB->get_Var( j )->set_type( *(nIit++) ? ColVariable::kBinary
				              : ColVariable::kPosUnitary );
      LOG( "(AR)" );
      }
     }
    else {                                // or subset modification
     Subset nms = generateSubset( m ); 

     if( dis( rg ) < 0.5 ) {              // PR
      BKB->chg_integrality( nI.begin() , std::move( nms ) );
      LOG( "(S)" );
      }
     else {                               // AR
      auto nIit = nI.begin();
      for( auto j : nms )
       BKB->get_Var( j )->set_type( *(nIit++) ? ColVariable::kBinary
				              : ColVariable::kPosUnitary );
      LOG( "(AS)" );
      }
     }
    LOG( " ~ " );
    }
   }
  // finally, re-solve - - - - - - - - - - - - - - - - - - - - - - - - - - -
 
  if( ! ( i % STEP ) )
   AllPassed &= SolveBoth();

  }  // end( main loop ) - - - - - - - - - - - - - - - - - - - - - - - - - -
     //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 LOG( endl );
 if( AllPassed )
  cout << GREEN( All tests passed!! ) << endl;
 else
  cout << RED( Errors happened!! ) << endl;    

 // final cleanup - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

 bsc->apply( BKB );  // remove the Solver by apply()-ing the clear()-ed bsc

 delete( bsc );      // delete the BlockSolverConfig

 delete( BKB );      // delete the BinaryKnapsackBlock

 // all done- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

 return( AllPassed ? 0 : 1 );

 }  // end( main )

/*--------------------------------------------------------------------------*/
/*------------------------ End File test.cpp -------------------------------*/
/*--------------------------------------------------------------------------*/
