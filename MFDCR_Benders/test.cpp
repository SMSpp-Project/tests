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

#define LOG_LEVEL 0
// 0 = only pass/fail
// 1 = result of each test
// 2 = + solver log

#if( LOG_LEVEL >= 1 )
 #define LOG1( x ) cout << x
 #define CLOG1( y , x ) if( y ) cout << x

 #if( LOG_LEVEL >= 2 )
  #define LOG_ON_COUT 1
  // if nonzero, the NDO Solver log is sent on cout rather than on a file
 #endif
#else
 #define LOG1( x )
 #define CLOG1( y , x )
#endif

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
#include <filesystem>  

#include <chrono>
#include <ctime>

#include "BlockSolverConfig.h"
#include "MultiFlowDCRBlock.h"

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace std;
using namespace SMSpp_di_unipi_it;

MultiFlowDCRBlock * MDCRB = nullptr;    // original MultiFlowDCRBlock
MultiFlowDCRBlock * MDCRB_ben = nullptr;    // original MultiFlowDCRBlock
MultiFlowDCRBlock * MDCRB_milp = nullptr;    // original MultiFlowDCRBlock

/*--------------------------------------------------------------------------*/

int main( int argc , char **argv )
{
  namespace stdfs = std::filesystem;
  std::string file = argv[ 1 ];
  std::ifstream iNode( file.substr(0,file.find_last_of('.'))+".nod" );
  int commodity, node, arc;

  //std::ofstream output_cpu( file.substr(0,file.find_last_of('.'))+"_cpu.txt" );
  //std::ofstream output_gap( file.substr(0,file.find_last_of('.'))+"_gap.txt" );
  //std::ofstream output( file.substr(0,file.find_last_of('.'))+".txt" );

  ofstream fout_csv( file.substr( 0 , file.find_last_of( '.' ) ) + ".csv" );

  fout_csv<<"nflow,"
        <<"lb_EXACT,ub_EXACT,cpu_EXACT,"
        <<"lb_GRB,ub_GRB,cpu_GRB,"
        <<"lb_Benders,ub_Benders,cpu_Benders,max_ERROR,max_GAP"<<endl;
  
  if( ! iNode.is_open() )
   throw( std::invalid_argument( "can't open file .nod" ) );

  std::ifstream iNode1( file.substr(0,file.find_last_of('.'))+"-1.nod" );
  if( ! iNode1.good() ){
    stdfs::path p = stdfs::current_path();
    stdfs::rename(file.substr(0,file.find_last_of('.'))+".nod", file.substr(0,file.find_last_of('.'))+"-1.nod");
    std::ifstream iNode2( file.substr(0,file.find_last_of('.'))+"-1.nod" );
    iNode2 >> commodity;
    iNode2 >> node >> arc;
    iNode2.close();
  } else {
    iNode1 >> commodity;
    iNode1 >> node >> arc;
    iNode1.close();
  }

  int index = 0;
  int k = 0;
  int mtu;
  string buffer1[commodity];

  std::ifstream iDCR1( file.substr(0,file.find_last_of('.'))+"-1.dcr" );
  if( ! iDCR1.good() ){
    stdfs::path p = stdfs::current_path();
    stdfs::rename(file.substr(0,file.find_last_of('.'))+".dcr", file.substr(0,file.find_last_of('.'))+"-1.dcr");
    std::ofstream iDCR( file.substr(0,file.find_last_of('.'))+".dcr" );
    std::ifstream iDCR1( file.substr(0,file.find_last_of('.'))+"-1.dcr" );
    std::string buffer;
    getline(iDCR1, buffer);
    iDCR << buffer << '\n';
    while(index<arc+node) {  
      getline(iDCR1, buffer);
      iDCR << buffer << '\n';
      index +=1 ;
    }
    while(index<arc+node+commodity) {  
      getline(iDCR1, buffer1[k]);
      k += 1;
      index += 1;
    }
    iDCR1 >> mtu;
  } else {
    stdfs::path p = stdfs::current_path();
    std::ofstream iDCR( file.substr(0,file.find_last_of('.'))+".dcr" );
    std::ifstream iDCR1( file.substr(0,file.find_last_of('.'))+"-1.dcr" );
    std::string buffer;
    getline(iDCR1, buffer);
    iDCR << buffer << '\n';
    while(index<arc+node) {  
      getline(iDCR1, buffer);
      iDCR << buffer << '\n';
      index +=1 ;
    }
    while(index<arc+node+commodity) {  
      getline(iDCR1, buffer1[k]);
      k += 1;
      index += 1;
    }
    iDCR1 >> mtu;
  }
  std::ofstream iParam( file.substr(0,file.find_last_of('.'))+".param" );
  iParam << mtu << "\n";
  for(index=0; index<commodity; index++) {  
    iParam << buffer1[index] << '\n';
  }
  iParam.close();
  int j;

  double max_GAP = 0.0;
  double max_ERROR = 0.0;

  for(j=1; j<=5; ++j){
  //for(j=1; j<std::min(500,commodity+1); j+=std::min(500,commodity+1)/10){
    std::cout << "commodities: " << j << std::endl;
    std::ofstream fout( file );
    fout << j << " " << node << " " << arc << " " << arc;  
    fout.close();

    MDCRB = dynamic_cast< MultiFlowDCRBlock * >( Block::new_Block( "MultiFlowDCRBlock" ) );
    assert( MDCRB );

    MDCRB->load( argv[ 1 ] );

    MDCRB_ben = dynamic_cast< MultiFlowDCRBlock * >( Block::new_Block( "MultiFlowDCRBlock" ) );
    MDCRB_milp = dynamic_cast< MultiFlowDCRBlock * >( Block::new_Block( "MultiFlowDCRBlock" ) );

    MDCRB_ben->load( argv[ 1 ] );
    MDCRB_milp->load( argv[ 1 ] );

    // check Solvers - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    auto c = Configuration::deserialize( "BSPar_exc.txt" );
    auto bsc = dynamic_cast< BlockSolverConfig * >( c );
    if( ! bsc ) {
      cerr << "Error: BSPar.txt not a BlockSolverConfig" << endl;
      delete( c );
      exit( 1 );
    }

    auto c_ben = Configuration::deserialize( "BSPar_ben.txt" );
    auto bsc_ben = dynamic_cast< BlockSolverConfig * >( c_ben );

    auto c_milp = Configuration::deserialize( "BSPar_milp.txt" );
    auto bsc_milp = dynamic_cast< BlockSolverConfig * >( c_milp );

    bsc->apply( MDCRB );
    bsc_ben->apply( MDCRB_ben );
    bsc_milp->apply( MDCRB_milp );

    double auxLB;
    double auxLB_LDS;

    Solver * slvr = MDCRB->get_registered_solvers().back();
    Solver * milp = MDCRB_milp->get_registered_solvers().back();
    Solver * ben = MDCRB_ben->get_registered_solvers().back();

        std::clock_t c_start = std::clock();
        auto rtrn = slvr->compute();
        auto lbound = slvr->get_lb();
        auto ubound = slvr->get_ub();
        std::clock_t c_end = std::clock();
        auto time =  1000.0 * (c_end-c_start) / CLOCKS_PER_SEC / 1000.0;
        std::cout << "OPTIMAL VALUE SOLUTION: " << ubound << "\n";
        std::cout << "CPU EXACT: " << time << "\n";
        auxLB = lbound;

      if( slvr->has_var_solution() ){

        fout_csv<<j+1<<","<<lbound<<","<<ubound<<","<<time;

        c_start = std::clock();
        rtrn = milp->compute();
        lbound = milp->get_lb();
        ubound = milp->get_ub();
        c_end = std::clock();
        time =  1000.0 * (c_end-c_start) / CLOCKS_PER_SEC / 1000.0;
        std::cout << "LAGRANGIAN LB MILP: " << lbound << "\n";
        std::cout << "LAGRANGIAN UB MILP: " << ubound << "\n";
        std::cout << "CPU MILP: " << time  << "\n";
        auxLB_LDS = lbound;

        fout_csv<<","<<lbound<<","<<ubound<<","<<time;

        c_start = std::clock();
        rtrn = ben->compute();
        lbound = ben->get_lb();
        ubound = ben->get_ub();
        c_end = std::clock();
        time =  1000.0 * (c_end-c_start) / CLOCKS_PER_SEC / 1000.0;
        std::cout << "LAGRANGIAN LB Benders: " << lbound << "\n";
        std::cout << "LAGRANGIAN UB Benders: " << ubound << "\n";
        std::cout << "CPU Benders: " << time  << "\n";

        max_ERROR = max( max_ERROR , std::abs( auxLB - lbound ) / lbound );
        max_GAP = max( max_GAP , std::abs( auxLB_LDS - lbound ) / lbound );

        std::cout << "\nMAX ERROR = " << max_ERROR << "\n";
        std::cout << "\nMAX GAP = " << max_GAP << "\n\n";

        fout_csv<<","<<lbound<<","<<ubound<<","<<time<<","<<max_ERROR<<","<<max_GAP<<endl;
      }
    }

  return 0;
 }  // end( main )

/*--------------------------------------------------------------------------*/
/*------------------------ End File test.cpp -------------------------------*/
/*--------------------------------------------------------------------------*/
