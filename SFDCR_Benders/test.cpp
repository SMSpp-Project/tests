/*--------------------------------------------------------------------------*/
/*-------------------------- File test.cpp ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Main for testing 
 *
 * \author Antonio Frangioni \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \author Laura Galli \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \author Luca Mencarelli \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Antonio Frangioni
 */
/*--------------------------------------------------------------------------*/
/*-------------------------------- MACROS ----------------------------------*/
/*--------------------------------------------------------------------------*/

#define LOG_LEVEL 2
// 0 = only pass/fail
// 1 = result of each test
// 2 = + solver log

#if( LOG_LEVEL >= 1 )
 #define LOG1( x ) cout << x
 #define CLOG1( y , x ) if( y ) cout << x

 #if( LOG_LEVEL >= 2 )
  #define LOG_ON_COUT 1
  // if nonzero, the 2nd Solver (LagrangianDualSolver) log is sent on cout
  // rather than on a file
 #endif
#else
 #define LOG1( x )
 #define CLOG1( y , x )
#endif

/*--------------------------------------------------------------------------*/
// if nonzero, the 1st Solver attached to the AbstractBlock is detached
// and re-attached to it at all iterations

#define DETACH_1ST 0

// if nonzero, the 2nd Solver attached to the AbstractBlock is detached and
// re-attached to it at all iterations

#define DETACH_2ND 0

/*--------------------------------------------------------------------------*/
// if nonzero, the AbstractBlock is not solved at every round of changes, but
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

#include <sstream>

#include <random>

#include <algorithm>

#include "BlockSolverConfig.h"

#include "SingleFlowDCRBlock.h"

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
using c_FunctionValue = Function::c_FunctionValue;
using Vec_FunctionValue = LinearFunction::Vec_FunctionValue;

using RHSValue = RowConstraint::RHSValue;

/*--------------------------------------------------------------------------*/
/*------------------------------- CONSTANTS --------------------------------*/
/*--------------------------------------------------------------------------*/

const char *const logF = "log.txt";

static constexpr FunctionValue INF = Inf< RHSValue >();

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/

SingleFlowDCRBlock * TestBlockDS = nullptr;  // the SingleFlowDCR that is solved
SingleFlowDCRBlock * TestBlock = nullptr;  // the SingleFlowDCR that is solved

Index wchg = 15;            // parameters of what is done

std::mt19937 rg;            // base random generator
std::uniform_real_distribution<> dis( 0.0 , 1.0 );

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

template< class T >
static void Str2Sthg( const char* const str , T &sthg )
{
 istringstream( str ) >> sthg;
 }

/*--------------------------------------------------------------------------*/

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

/*--------------------------------------------------------------------------*/

static void PrintResults( bool hs , int rtrn , double lb , double ub )
{
 if( hs )
  cout << lb << " " << ub;
 else
  if( rtrn == Solver::kInfeasible )
   cout << "    Unfeas";
  else
   if( rtrn == Solver::kUnbounded )
    cout << "      Unbounded";
   else
    cout << "      Error!";
 }

/*--------------------------------------------------------------------------*/

static bool SolveBoth( void ) 
{
 try {
  // solve with the 1st Solver- - - - - - - - - - - - - - - - - - - - - - - -
  Solver * Slvr1 = TestBlock->get_registered_solvers().front();
  #if DETACH_1ST
   TestBlock->unregister_Solver( Slvr1 );
   TestBlock->register_Solver( Slvr1 , true );  // push it to the front
  #endif
  int rtrn1st = Slvr1->compute( false );
  if( ! TestBlock->is_feasible_flow() )
    rtrn1st = Solver::kInfeasible;
  bool hs1st = ( ( ( rtrn1st >= Solver::kOK ) && ( rtrn1st < Solver::kError )
                   && ( rtrn1st != Solver::kUnbounded )
                   && ( rtrn1st != Solver::kInfeasible ) )
                 || ( rtrn1st == Solver::kLowPrecision ) );
  double fo1stlb = hs1st ? Slvr1->get_lb() : -INF;
  double fo1stub = hs1st ? Slvr1->get_ub() : INF;

  if( TestBlock->get_registered_solvers().size() == 1 ) {
   #if( LOG_LEVEL >= 1 )
    cout << "Solver = ";
    PrintResults( hs1st , rtrn1st , fo1stlb , fo1stub );
    cout << endl;
   #endif
   return( true );
   }

  // solve with the 2nd Solver- - - - - - - - - - - - - - - - - - - - - - - -
  Solver * Slvr2 = TestBlock->get_registered_solvers().back();
  #if DETACH_2ND
   TestBlock->unregister_Solver( Slvr2 );
   TestBlock->register_Solver( Slvr2 );  // push it to the back
  #endif
  int rtrn2nd = Slvr2->compute( false );

<<<<<<< Updated upstream
=======
  double fo2ndlb = Slvr2->get_lb();
  double fo2ndub = Slvr2->get_ub();
  double fo1stval = Slvr1->get_var_value();
  bool OKfo;

  if( fo2ndub > 1e200 && fo2ndlb < -1e200 )
    rtrn2nd = Solver::kInfeasible;
  
  hs1st = ( (fo1stub - fo1stlb)/ fo1stub < 1e-6 ) && hs1st;

>>>>>>> Stashed changes
  bool hs2nd = ( ( ( rtrn2nd >= Solver::kOK ) && ( rtrn2nd < Solver::kError )
                   && ( rtrn2nd != Solver::kUnbounded )
                   && ( rtrn2nd != Solver::kInfeasible ) )
                 || ( rtrn2nd == Solver::kLowPrecision ) );

  double fo2ndlb = hs2nd ? Slvr2->get_lb() : -INF;
  double fo2ndub = hs2nd ? Slvr2->get_ub() : INF;
  double fo1stval = Slvr1->get_var_value();
  bool OKfo;

  if( fo2ndub == Inf<double>() && fo2ndlb == Inf<double>() )
    rtrn2nd = Solver::kInfeasible;

  if( fo1stub == 0 && fo1stlb > fo1stub )
    rtrn1st = Solver::kInfeasible;

  if( hs1st ) {
    OKfo = ( ( fo1stub - fo2ndlb ) / fo1stub >= -1e-4 &&
      ( fo1stlb - fo2ndub ) / fo1stlb <= 1e-4 );
   }

  if( hs1st && hs2nd && OKfo ) {
   LOG1( "OK(f)" << std::endl);
   ///LOG1( fo1stval << " " << fo2ndlb << " " << fo2ndub << std::endl );
   return( true );
   }

  if( ( rtrn1st == Solver::kInfeasible ) &&
      ( rtrn2nd == Solver::kInfeasible ) ) {
    LOG1( "OK(e)" << endl );
    return( true );
    }

  if( ( rtrn1st == Solver::kUnbounded ) &&
      ( rtrn2nd == Solver::kUnbounded ) ) {
   LOG1( "OK(u)" << endl );
   return( true );
   }

  if( ( rtrn1st == Solver::kError ) || ( (fo1stub - fo1stlb)/ fo1stub > 1e-6 ) ) {
   LOG1( "OK(err)" << endl );
   return( true );
   }

  #if( LOG_LEVEL >= 0 )
   cout << "Solver1 = ";
   PrintResults( hs1st , rtrn1st , fo1stlb , fo1stub );

   cout << " ~ Solver2 = ";
   PrintResults( hs2nd , rtrn2nd , fo2ndlb , fo2ndub );
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
/// Custom terminate function to print the exception message

void smspp_terminate( void )
{
 std::cerr << "Uncaught exception in executing SMS++:\n";
 try {
  std::rethrow_exception( std::current_exception() );
  }
 catch( const std::exception & e ) {
  std::cerr << "\tException type: " << typeid( e ).name() << "\n";
  std::cerr << "\tException message: " << e.what() << "\n";
  }
 catch( ... ) {
  std::cerr << "\tUnknown exception" << std::endl;
  }
 std::abort();  // or exit(1)
 }

/*--------------------------------------------------------------------------*/

int main( int argc , char **argv )
{

 Index n_flow = 5;
 double p_change = 0.5;
 Index n_repeat = 5;

 bool AllPassed = true;
 BlockSolverConfig * bsc;

 // override the default terminate handler to print the exception message
 std::set_terminate( smspp_terminate );

 // reading command line parameters - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 
 assert( SKIP_BEAT >= 0 );

 int source, destination, commodity;
 int SN, EN, numberArc, numberComm;
 int NComm, NNodes, NArcs;
 double rho, rho1, mtu, FlowBursts, FlowDeadline;
 double capacity, cost;
 string fn = argv[ 1 ];

 ifstream iNode( fn.substr( 0 , fn.find_last_of( '.' ) ) + ".nod" );
 ifstream iFile3( fn.substr( 0 ,fn.find_last_of( '.' ) ) + ".param" );
 ifstream iFile2( fn.substr( 0 ,fn.find_last_of( '.' ) ) + ".sup" );

 iNode >> NComm; 
 iNode >> NNodes;
 iNode >> NArcs;

 iFile3 >> mtu;
 
 for(int j = 0; j < n_flow; j++){

  ifstream netcdf_file( fn + "_" + std::to_string( j ) + ".nc4" );

  if( netcdf_file.fail() ){

    ifstream iFile1( fn.substr(0,fn.find_last_of('.')) + ".dcr" );
    ifstream iArc( fn.substr(0,fn.find_last_of('.')) + ".arc" );

    ofstream foutdmx("output.dmx");
    ofstream foutdcr("output.dcr");

    iFile2 >> source;
    iFile2 >> commodity;
    iFile2 >> rho;
    iFile2 >> destination;
    iFile2 >> commodity;
    iFile2 >> rho1;

    iFile3 >> FlowBursts >> FlowDeadline;

    while (!iFile1.eof()) {
    string buffer;
    getline(iFile1, buffer);
    foutdcr << buffer << '\n';
    }
    iFile1.close();
    
    foutdcr << FlowBursts << '\n';
    foutdcr << FlowDeadline  << '\n';
    foutdcr << mtu  << '\n';
    foutdcr << rho  << '\n';
    foutdcr.close();

    foutdmx << "p min " << NNodes << " " << NArcs << "\n";
    foutdmx << "n " << source << " 1\n";
    foutdmx << "n " << destination << " -1\n";
    
    for(int i = 0; i < NArcs; i++){
    iArc >> numberArc;
    iArc >> SN;
    iArc >> EN;
    iArc >> numberComm;
    iArc >> cost;
    iArc >> capacity;
    iArc >> numberArc;
    foutdmx << "a " << SN << " " << EN << " -1 " << capacity << " " << cost << "\n";
    }

    foutdmx.close();
    iArc.close();

    TestBlockDS = dynamic_cast< SingleFlowDCRBlock * >( Block::new_Block( "SingleFlowDCRBlock" ) );

    ifstream fndmx1("output.dmx");
    ifstream fndcr1("output.dcr");
    TestBlockDS->load( fndmx1 );
    Index NNode = TestBlockDS->get_NNodes();
    Index NArc = TestBlockDS->get_NArcs();
    TestBlockDS->load_dcr( fndcr1 , NNode , NArc );
    fndmx1.close();
    fndcr1.close();

    netCDF::NcFile dataFile( fn + "_" + std::to_string( j ) + ".nc4" , netCDF::NcFile::replace );
    netCDF::NcGroup block = dataFile.addGroup( "Block_0" );
    netCDF::NcAtt *att;
    auto int_type = netCDF::NcInt();
    dataFile.putAtt( "SMS++_file_type" , int_type , 1 );
    TestBlockDS->serialize( block );
    TestBlockDS->deserialize( block );
    dataFile.close();
  }

  TestBlock = dynamic_cast< SingleFlowDCRBlock * >
        ( Block::deserialize( fn + "_" + std::to_string( j ) + ".nc4" ) );

  Index NNodes = TestBlock->get_NNodes();
  Index NArcs = TestBlock->get_NArcs();
  
  if( ! TestBlock ) {
    std::cout << "Block::deserialize() failed!" << std::endl;
  exit( 1 );
  }

 // attach the Solver(s) to the Block - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // do this by reading an appropriate BlockSolverConfig from file and
 // apply() it to the TestBlock; note that the BlockSolverConfig is
 // clear()-ed and kept to do the cleanup at the end

 {
  auto c = Configuration::deserialize( "BSPar-2S.txt" );
  bsc = dynamic_cast< BlockSolverConfig * >( c );
  if( ! bsc ) {
   cerr << "Error: BSPar-2S.txt not a BlockSolverConfig" << endl;
   delete( c );
   exit( 1 );
   }

  bsc->apply( TestBlock );
  bsc->clear();

  if( TestBlock->get_registered_solvers().empty() ) {
   cout << endl << "no Solver registered to the Block!" << endl;
   exit( 1 );
   }

  // load the BlockConfig for SigleFlowDCRBlock
  auto dcrc = Configuration::deserialize( "DCRCfg.txt" );
  auto dcrbc = dynamic_cast< BlockConfig * >( dcrc );
  if( ! dcrbc ) {
   std::cerr << "Error: DCRCfg.txt does not contain a BlockSolverConfig"
             << std::endl;
   delete( c );
   delete( dcrc );
   exit( 1 );
   }
   dcrbc->apply( TestBlock );
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

 std::uniform_int_distribution<> distr( 0 , NNodes - 1 );

 // the two Solver are called to re-solve the SingleFlowDCRBlock
 for( Index rep = 0 ; rep < n_repeat * ( SKIP_BEAT + 1 ) ; ) {
  LOG1( rep << " - " << fn << "[" << j << "]: " );

  // change costs - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  if( ( wchg & 1 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = Index( dis( rg ) * NArcs ) ) {
    LOG1( "changed " << tochange << " obj coeffs" );
    std::vector< double > NC( tochange );
    auto it = NC.begin();
    for( auto & nc : NC )
     nc = dis( rg );
    
     if( dis( rg ) <= 0.5 ) {  // in 50% of the cases do a ranged change
      LOG1( "(r) - " );
      Index strt = dis( rg ) * ( NArcs - tochange );
      Index stp = strt + tochange;
      if( tochange == 1 )
       if( dis( rg ) < 0.5 )
        TestBlock->chg_cost( NC.front() , strt );
       else
        static_cast< LinearFunction * >(static_cast< FRealObjective * >( TestBlock->get_objective())
          ->get_function())->modify_coefficient( NC.front() , strt );
      else
       if( dis( rg ) < 0.5 )
        TestBlock->chg_costs( it , Range( strt , stp ) );
       else 
        static_cast< LinearFunction * >(static_cast< FRealObjective * >( TestBlock->get_objective())
          ->get_function())->modify_coefficients( std::move( NC ) , Range( strt , stp ) );
     }
     else {  // in the other 50% of the cases, do a sparse change
      LOG1( "(s) - " );
      Subset nms( GenerateRand( NArcs , tochange ) );
      if( tochange == 1 )
       if( dis( rg ) < 0.5 )
        TestBlock->chg_cost( NC.front() , nms.front() );
       else
        static_cast< LinearFunction * >(static_cast< FRealObjective * >( TestBlock->get_objective())
          ->get_function())->modify_coefficient( NC.front() , nms.front() );
      else
       if( dis( rg ) < 0.5 )
        TestBlock->chg_costs( it , std::move( nms ) );
       else
        static_cast< LinearFunction * >(static_cast< FRealObjective * >( TestBlock->get_objective())
          ->get_function())->modify_coefficients( std::move( NC ) , std::move( nms ) );
     }
   }

   // close arcs - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   if( ( ( wchg & 2 ) && dis( rg ) <= p_change ) )
    if( Index toclose = Index( dis( rg ) * NArcs ) ) {
      LOG1( "closed " << toclose << " arcs" );
      if( dis( rg ) <= 0.5 ) {  // in 50% of the cases do a ranged change
        LOG1( "(r) - " );
        Index strt = dis( rg ) * ( NArcs - toclose );
        Index stp = strt + toclose;
        if( toclose == 1 )
          if( dis( rg ) < 0.5 )
            TestBlock->close_arc( strt );
          else {
            auto xa = TestBlock->i2p_x( strt );
            xa->set_value( 0 );
            xa->is_fixed( true );
          }
        else
          if( dis( rg ) < 0.5 )
            TestBlock->close_arcs( Range( strt , stp ) );
          else {
            for( Index i = strt ; i < stp ; ++i ){
              auto xa = TestBlock->i2p_x( i );
              xa->set_value( 0 );
              xa->is_fixed( true );
            }
          }
      }
      else {  // in the other 50% of the cases, do a sparse change
        LOG1( "(s) - " );
        Subset nms( GenerateRand( NArcs , toclose ) );
        if( toclose == 1 )
          if( dis( rg ) < 0.5 )
            TestBlock->close_arc( nms.front() );
          else {
            auto xa = TestBlock->i2p_x( nms.front() );
            xa->set_value( 0 );
            xa->is_fixed( true );
          }
        else
          if( dis( rg ) < 0.5 )
            TestBlock->close_arcs( std::move( nms ) );
          else {
            for( auto nit = nms.begin() ; nit != nms.end() ; ++nit ){
              auto xa = TestBlock->i2p_x( *nit );
              xa->set_value( 0 );
              xa->is_fixed( true );
            }
          }
      }
    }

   // change s-t - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   if( ( ( wchg & 3 ) && dis( rg ) <= p_change ) ){
    LOG1( "changed s-t nodes - " );
    if( ( wchg & 2 ) && ( dis( rg ) <= p_change ) ){
      Index news = distr( rg );
      Index newt = news;
      while( newt == news )
        newt = distr( rg );
      TestBlock->chg_st( news , newt );
    }
  }

  // finally, re-solve the problems- - - - - - - - - - - - - - - - - - - - -
  // ... every SKIP_BEAT + 1 rounds

  if( ! ( ++rep % ( SKIP_BEAT + 1 ) ) )
   AllPassed &= SolveBoth();
  #if( LOG_LEVEL >= 1 )
  else
   cout << endl;
  #endif
  }
 }  // end( main loop )- - - - - - - - - - - - - - - - - - - - - - - - - - -
     // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


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
