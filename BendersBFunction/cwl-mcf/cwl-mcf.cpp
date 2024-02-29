/*--------------------------------------------------------------------------*/
/*--------------------------- File cwl-mcf.C -------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Simple main() for solving relaxations of Capacitated Warehouse Location
 * problems using a MCF solver under the MCFClass interface. It also
 * implements a simple slope-scaling approach whereby the cost of the
 * design variables is iteratively modified to take into account the level
 * of utilization of each warehouse in the previous continuous solution.
 *
 * \author Antonio Frangioni \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Antonio Frangioni
 */

/*--------------------------------------------------------------------------*/
/*------------------------------- MACROS -----------------------------------*/
/*--------------------------------------------------------------------------*/

#define WHICH_MCF 80

/** This macros select which solver (derived class of MCFClass) is used, and
    which solver-specific algorithmic parameters are set. The first four bits
   of the value can be used to set some special initialization for the solver.
   Possible values are:

     16    the RelaxIV solver
     +1    the Auction() initialization is used (if available)

     32    the MCFCplex solver
     +k    a value of k between 1 and 3 set the pricing rule used by the
           network simplex; 0 means automatic (the default)

     48    the MCFZIB solver
     +1    use the dual network simplex (by default, the primal is used)
     +2    use the first-element pricing rule (by default, Dantzig rule is
           used)
     +4    use the Multiple Partial Pricing rule (only for primal simplex)

     64    the CS2 solver.

     80    the MCFSimplex solver
     +1    use the dual network simplex (by default, the primal is used)
     +2    use the first-element pricing rule (by default, Candidate List
           Pivot Rule rule is used)
     +4    use the Dantzig's pricing rule
   */

/*--------------------------------------------------------------------------*/

#define LOOP_SIZE 100
// how many iterations to look back to see if this solution has been found yet
// if LOOP_SIZE == 0, no check is done

/*--------------------------------------------------------------------------*/

#define CLOSED_COSTS 1
// this macro controls how costs of "closed" arc is computed:
// == 0 ==> usual formula F[ i ] / Q[ i ]
// > 0  ==> use the cost at the previous iteration (that may, or may not,
//          be the same as above)

/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

// MCFClass header(s)- - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//- - - - - - - - - - - - - - - - - - - - - - - - - -- - - - - - - - - - - - -

#if( WHICH_MCF < 32 )

 #include "RelaxIV.h"
 #define MCFTYPE RelaxIV

#elif( WHICH_MCF < 48 )

 #include "MCFCplex.h"
 #define MCFTYPE MCFCplex

#elif( WHICH_MCF < 64 )

 #include "MCFZIB.h"
 #define MCFTYPE MCFZIB
 #define MCFZIBALG( x ) ( x & 1 ? false : true )
 #define MCFZIBPRC( x ) ( x & 2 ? MCFZIB::kFrstElA : \
                        ( x & 4 ? MCFZIB::kMltPrPr : MCFZIB::kDantzig ) )

#elif( WHICH_MCF < 80 )

 #include "CS2.h"1
 #define MCFTYPE CS2

#else

 #include "MCFSimplex.h"
 #define MCFTYPE MCFSimplex

#endif

// other includes- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

#include <cmath>
#include <fstream>
#include <sstream>

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace MCFClass_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*------------------------------- TYPES ------------------------------------*/
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/*----------------------------- CONSTANTS ----------------------------------*/
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/*----------------------------- FUNCTIONS ----------------------------------*/
/*--------------------------------------------------------------------------*/

template< class T >
static inline void str2val( const char* const str , T &sthg )
{
 std::istringstream( str ) >> sthg;
 }

/*--------------------------------------------------------------------------*/
/*-------------------------------- main() ----------------------------------*/
/*--------------------------------------------------------------------------*/

//int main( int argc , char **argv )
double cwl_mcf( std::string file_name )
{
 // read the command-line parameters- - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 bool print = false;

 int slp_sclng_it = 0;     // max number of slope scaling iterations
 bool prn_y_var = false;   // true if the y's are printed
 bool prn_rd_cst = false;  // true if reduced costs of y's are printed
 /*
 std::string file_name = argv[ 1 ];
 switch( argc ) {

  case( 5 ): str2val( argv[ 4 ] , prn_rd_cst );

  case( 4 ): str2val( argv[ 3 ] , prn_y_var );

  case( 3 ): str2val( argv[ 2 ] , slp_sclng_it );

  case( 2 ): break;

  default:   std::cerr << "Usage: " << argv[ 0 ]
		  << " file_name [slp_sclng_it prn_y_var prn_rd_cst]"
		  << std::endl
		  << "       slp_sclng_it = 0 (default) max slope scaling iter"
		  << std::endl
		  << "       prn_y_var = 0 (default) print y variables"
		  << std::endl
		  << "       prn_rd_cst = 0 (default) print y's reduced costs"
		  << std::endl;
             return( 1 );
  }
 */

 // enter the try-block - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 double LB = -Inf< double >();     // correct lower bound

 try {

  // open problem file- - - - - - - - - - - - - - - - - - - - - - - - - - - -
  //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  ifstream ProbFile( file_name );
  if( ! ProbFile.is_open() ) {
   std::cerr << "Error: cannot open file " << file_name << std::endl;
   return( 1 );
   }

  // read instance- - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  int m;
  ProbFile >> m;
  if( ProbFile.fail() ) {
   std::cerr << "Error reading from file " << file_name << std::endl;
   return( 1 );
   }

  if( m <= 0 ) {
   std::cerr << "Error: number of warehouses must be positive" << std::endl;
   return( 1 );
   }

  int n;
  ProbFile >> n;
  if( ProbFile.fail() ) {
   std::cerr << "Error reading from file " << file_name << std::endl;
   return( 1 );
   }

  if( n <= 0 ) {
   std::cerr << "Error: number of customers must be positive" << std::endl;
   return( 1 );
   }

  double *Q = new double[ m ];  // capacity
  double *F = new double[ m ];  // fixed cost

  for( int i = 0 ; i < m ; i++ ) {  // for( each warehouse )
   ProbFile >> Q[ i ];
   if( ProbFile.fail() ) {
    std::cerr << "Error reading from file " << file_name << std::endl;
    return( 1 );
    }

   ProbFile >> F[ i ];
   if( ProbFile.fail() ) {
    std::cerr << "Error reading from file " << file_name << std::endl;
    return( 1 );
    }
   }  // end( for( each warehouse ) )

  double *D = new double[ n ];      // demand
  double *C = new double[ n * m ];  // C[ j * m + i ] is the cost between
                                    // customer j to warehouse i

  for( int j = 0 ; j < n ; j++ ) {  // for( each customer )
   ProbFile >> D[ j ];
   if( ProbFile.fail() ) {
    std::cerr << "Error reading from file " << file_name << std::endl;
    return( 1 );
    }

   for( int i = 0 ; i < m ; i++ ) {  // for( each warehouse )
    ProbFile >> C[ j * m + i ];
    if( ProbFile.fail() ) {
     std::cerr << "Error reading from file " << file_name << std::endl;
     return( 1 );
     }
    }  // end( for( each warehouse ) )
   }  // end( for( each customer ) )

  if( print )
   std::cout << "Solving instance " << file_name << " with " << m
        << " warehouses and " << n << " customers" << std::endl;

  // create solver and pass it the instance - - - - - - - - - - - - - - - - -
  //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  OPTtimers timer;
  timer.Start();

  MCFClass::Index NNodes = n + m + 1;
  // node 1: super source
  // nodes 2 ... m + 1: warehouses
  // nodes m + 2 ... n + m + 1: customers

  MCFClass::Index NArcs = m * ( n + 1 );
  // arcs 0 ... m - 1: from source to warehouses
  // arcs m ... m * ( n + 1 ) - 1: from warehouses to customers

  // MCFTYPE *MCF = new MCFTYPE( NNodes , NArcs );
  MCFTYPE *MCF = new MCFTYPE();

  MCFClass::CRow MCFC = new MCFClass::CNumber[ NArcs ];
  MCFClass::FRow MCFU = new MCFClass::FNumber[ NArcs ];
  MCFClass::FRow MCFB = new MCFClass::FNumber[ NNodes ];

  MCFClass::Index_Set MCFSn = new MCFClass::Index[ NArcs ];
  MCFClass::Index_Set MCFEn = new MCFClass::Index[ NArcs ];

  for( int i = 0 ; i <= m ; i++ )
   MCFB[ i ] = MCFClass::FNumber( 0 );

  for( int j = 0 ; j < n ; j++ ) {
   MCFB[ 0 ] -= MCFClass::FNumber( D[ j ] );
   MCFB[ m + 1 + j ] = MCFClass::FNumber( D[ j ] );
   }

  MCFClass::Index a = 0;

  for( int i = 0 ; i < m ; i++ , a++ ) {
   MCFSn[ a ] = 1;
   MCFEn[ a ] = i + 2;
   MCFU[ a ] = MCFClass::FNumber( Q[ i ] );
   MCFC[ a ] = MCFClass::CNumber( F[ i ] / Q[ i ] );
   }

  for( int j = 0 ; j < n ; j++ )
   for( int i = 0 ; i < m ; i++ , a++ ) {
    MCFSn[ a ] = i + 2;
    MCFEn[ a ] = m + 2 + j;
    MCFU[ a ] = MCFClass::FNumber( D[ j ] );
    // this is a fake capacity, but a reasonable one
    MCFC[ a ] = MCFClass::CNumber( C[ j * m + i ] / D[ j ] );
    // note: C[ j * m + i ] is, according to the manual, the cost of
    // "allocating all of the demand of j to warehouse i", thus the
    // flow unit cost over an arc has to be scaled by D[ j ]
    }

  MCF->LoadNet( NNodes , NArcs , NNodes , NArcs , MCFU , MCFC , MCFB ,
		MCFSn , MCFEn );

  // deallocate instance data - - - - - - - - - - - - - - - - - - - - - - - -
  //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  delete[] MCFEn;
  delete[] MCFSn;
  delete[] MCFB;
  delete[] MCFU;
  delete[] MCFC;

  /*!!
  ofstream dmx( "prob.dmx" );
  MCF->WriteMCF( dmx );
  */

  // but not all, because some are needed in the heuristic

  if( print )
   std::cout << "Model construction time " << timer.Read();

  // choose algorithmic parameters, if any- - - - - - - - - - - - - - - - - -
  //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  #if( WHICH_MCF < 32 )
   // RelaxIV
   #if( WHICH_MCF & 1 )
    MCF->SetPar( RelaxIV::kAuction , MCFClass:kYes );
   #endif
  #elif( WHICH_MCF < 48 )
   // MCFCplex
   #if( ( ( WHICH_MCF & 1 ) > 0 ) && ( ( WHICH_MCF & 1 ) < 4 )
    MCF->SetPar( CPX_PARAM_NETPPRIIND , int( WHICH_MCF & 32 ) );
   #endif
  #elif( WHICH_MCF < 64 )
   // MCFZIB
   MCF->SetAlg( MCFZIBALG( WHICH_MCF ) , MCFZIBPRC( WHICH_MCF ) );
  #elif( WHICH_MCF < 80 )
   // CS2: nothing to do
  #else
   // MCFSimplex
   #if( WHICH_MCF & 1 )
    MCF->SetPar( MCFSimplex::kAlgPrimal , MCFClass::kNo );
   #endif
   #if( ( WHICH_MCF & 6 ) == 2 )
    MCF->SetPar( MCFSimplex::kAlgPricing , MCFSimplex::kFirstEligibleArc );
   #elif( ( WHICH_MCF & 6 ) == 4 )
    MCF->SetPar( MCFSimplex::kAlgPricing , MCFSimplex::kDantzig );
   #endif
  #endif

  MCF->SetMCFTime();

  // (possibly) start the slope-scaling loop- - - - - - - - - - - - - - - - -
  //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  #if LOOP_SIZE
   double oldFO[ LOOP_SIZE ];                // o.f. value at previous rounds
   for( int l = 0 ; l < LOOP_SIZE ; l++ )
    oldFO[ l ] = Inf< double >();    // ... currently undefined
  #endif
  double bestUB = Inf< double >();   // best UB value found
  MCFClass::FRow X = new MCFClass::FNumber[ NArcs ];  // flow solution

  for( int itr = 0 ; ; itr++ ) {
   // solve the problem - - - - - - - - - - - - - - - - - - - - - - - - - - -

   MCF->SolveMCF();

   // output results- - - - - - - - - - - - - - - - - - - - - - - - - - - - -

   if( print && MCF->MCFGetStatus() != MCFClass::kOK ) {
    switch( MCF->MCFGetStatus() ) {
     case( MCFClass::kUnfeasible ):
      std::cout << "MCF problem unfeasible" << std::endl;
      break;
     case( MCFClass::kUnbounded ):
      std::cout << "MCF problem unbounded (what ?!?)" << std::endl;
      break;
     case( MCFClass::kUnSolved ):
      std::cout << "MCF solver not called (what ?!?)" << std::endl;
      break;
     case( MCFClass::kStopped ):
      std::cout << "MCF problem stopped (what ?!?)" << std::endl;
      break;
     default:  // that'd be MCFClass::kError for you, sir
      std::cout << "error in the MCF solver" << std::endl;
     }
    break;
    }

   // run a simple rounding heuristic - - - - - - - - - - - - - - - - - - - -

   MCF->MCFGetX( X );

   // arcs 0 ... m - 1: from source to warehouses
   // arcs m ... m * ( n + 1 ) - 1: from warehouses to customers

   MCFClass::Index a = 0;

   // round up design variables
   double FUB = 0;
   for( int i = 0 ; i < m ; i++ , a++ )
    if( X[ a ] > 1e-6 )
     FUB += F[ i ];

   // copy over transportation costs
   double CUB = 0;
   for( int j = 0 ; j < n ; j++ )
    for( int i = 0 ; i < m ; i++ , a++ )
     CUB += X[ a ] * C[ j * m + i ] / D[ j ];

   if( FUB + CUB < bestUB )
    bestUB = FUB + CUB;

   if( print ) {

   // print y variables - - - - - - - - - - - - - - - - - - - - - - - - - - -
   std::cout << setprecision( 6 );

   if( prn_y_var )
    for( int i = 0 ; i < m ; i++ )
     if( X[ i ] > 1e-6 )
      std::cout << "y[ " << i + 1 << " ] = " << X[ i ] / Q[ i ] << std::endl;

   // print reduced costs - - - - - - - - - - - - - - - - - - - - - - - - - -

   if( prn_rd_cst ) {
    MCFClass::CRow RC = new MCFClass::CNumber[ m ];

    MCF->MCFGetRC( RC , 0 , 0 , m );  // get reduced costs of y variables

    for( int i = 0 ; i < m ; i++ )
     if( ( RC[ i ] > 1e-6 ) || ( RC[ i ] < - 1e-6 ) )
      std::cout << "RC y[ " << i + 1 << " ] = " << RC[ i ] << std::endl;

    delete[] RC;
    }
   }

   // get optimal flow value- - - - - - - - - - - - - - - - - - - - - - - - -

   double FO = MCF->MCFGetFO();
   if( ! itr )  // at the first iteration (only) is a valid lower bound
    LB = FO;    // keep track

   // a few printouts - - - - - - - - - - - - - - - - - - - - - - - - - - - -

   if( print && slp_sclng_it )
    std::cout << itr << ": flow value = " << setprecision( 12 ) << FO
	 << ", heuristic value = " << setprecision( 12 ) << FUB + CUB
	 << std::endl << "    (" << FUB << " + " << CUB << "), gap = "
	 << setprecision( 4 ) << ( FUB + CUB - LB ) / LB << std::endl;

   // check termination - - - - - - - - - - - - - - - - - - - - - - - - - - -
   // look back LOOP_SIZE iterations: if you find the same value of the
   // flow the algorithm is likely cycling, so force a stop

   #if LOOP_SIZE
    for( int l = 0 ; l < LOOP_SIZE ; l++ )
      if( oldFO[ l ] < Inf< double >() )
       if( std::abs( FO - oldFO[ l ] ) <= 1e-6 * max( FO , double( 1 ) ) ) {
       itr = slp_sclng_it;
       break;
       }

    oldFO[ itr % LOOP_SIZE ] = FO;  // record flow value for later
   #endif

   if( itr >= slp_sclng_it )  // maximum number of iterations
    break;

   // do slope scaling- - - - - - - - - - - - - - - - - - - - - - - - - - - -
   // The idea is simple: because
   //
   //    y[ i ] = total warehouse utilization / Q[ i ]
   //
   // can be << 1 in the continuous solution, one can pay a lot less than
   // the true cost of F[ i ] to have flow using warehouse i; this makes for
   // a crappy bound and a huge gap with the rounded integer solution. Then,
   // one takes all the used warehouses (those for which y[ i ] > 0 in the
   // continuous solution, hence y[ i ] = 1 in the rounded one) and modifies
   // their cost so that *that level of warehouse utilization corresponds to
   // paying the full price F[ i ]*. This is simply obtained by setting the
   // cost to F[ i ] / y[ i ]. Because y[ i ] <= Q[ i ], this is >= than the
   // "standard" cost F[ i ] / Q[ i ]: hence, warehouses that are "open but
   // little used" are heavily penalized (relatively speaking) w.r.t. those
   // that are "open but used a lot" or "not open at all". Hopefully, this
   // will convince the flow at the next round to avoid the former and more
   // fully use the ones that are used a lot.

   MCFClass::CRow newC = new MCFClass::CNumber[ m ];  // new costs

   for( int i = 0 ; i < m ; i++ )
    if( X[ i ] > 1e-6 )
     newC[ i ] = MCFClass::CNumber( F[ i ] / X[ i ] );  // open warehouse
    else
     #if CLOSED_COSTS
      newC[ i ] = MCF->MCFCost( MCFClass::Index( i ) );  // closed warehouse
     #else
      newC[ i ] = MCFClass::CNumber( F[ i ] / Q[ i ] );  // closed warehouse
     #endif

   MCF->ChgCosts( newC , NULL , 0 , m );  // change the costs (of the y only)

   delete[] newC;

   }  // end( slope scaling loop )- - - - - - - - - - - - - - - - - - - - - -
      //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  if( print ) {

  if( LB > -Inf< double >() )
   std::cout << "Relaxation value = " << setprecision( 12 ) << LB << " ~ ";
  if( bestUB < Inf< double >() )
   std::cout << "heuristic value = " << setprecision( 12 ) << bestUB << std::endl;
  if( ( LB > -Inf< double >() ) &&
      ( bestUB < Inf< double >() ) )
   std::cout << "gap = " << setprecision( 4 ) << ( bestUB - LB ) / LB;

  double tu , ts;
  MCF->TimeMCF( tu , ts );
  std::cout << " ~ time = " << tu + ts << std::endl;

  }

  // cleanup- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  delete[] X;

  delete( MCF );

  delete[] C;
  delete[] D;
  delete[] F;
  delete[] Q;

  }  // end( try-block )

 // managing exceptions - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 catch( exception &e ) {
  std::cerr << e.what() << std::endl;
  return( 1 );
  }
 catch(...) {
  std::cerr << "Error: unknown exception thrown" << std::endl;
  return( 1 );
  }

 // the end - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 // return( 0 );
 return( LB );

 }  // end( main )

/*--------------------------------------------------------------------------*/
/*------------------------ End File cwl-mcf.C ------------------------------*/
/*--------------------------------------------------------------------------*/
