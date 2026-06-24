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

/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <sstream>

#include <random>

#include <algorithm>

#include <chrono>

#include <ctime>

#include <filesystem>

#include "BlockSolverConfig.h"

#include "SingleFlowDCRBlock.h"

#include "SingleFlowDCRBendersSolver.h"

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace std;

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*-------------------------------- TYPES -----------------------------------*/
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/*------------------------------- CONSTANTS --------------------------------*/
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/

SingleFlowDCRBlock * TestBlockDS = nullptr;  // the SingleFlowDCR that is solved
SingleFlowDCRBlock * TestBlock = nullptr;  
//SingleFlowDCRBlock * TestBlock_socp_CPX = nullptr;  
//SingleFlowDCRBlock * TestBlock_pc_CPX = nullptr;  
SingleFlowDCRBlock * TestBlock_socp_GRB = nullptr;  
SingleFlowDCRBlock * TestBlock_pc_GRB = nullptr;  
SingleFlowDCRBlock * TestBlock_ben = nullptr;  

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

int main( int argc , char **argv )
{

 BlockSolverConfig * bsc;

 int source, destination, commodity;
 int SN, EN, numberArc, numberComm, rtrn;
 int NComm, NNodes, NArcs;
 double rho, rho1, mtu, FlowBursts, FlowDeadline;
 double capacity, cost;
 string fn = argv[ 1 ];
 ofstream fout( fn.substr( 0 , fn.find_last_of( '.' ) ) + ".csv" ); 

 ifstream iNode( fn.substr( 0 ,  fn.find_last_of( '.' ) ) + ".nod" );
 ifstream iFile3( fn.substr( 0 , fn.find_last_of( '.' ) ) + ".param" );
 ifstream iFile2( fn.substr( 0 , fn.find_last_of( '.' ) ) + ".sup" );

 iNode >> NComm; 
 iNode >> NNodes;
 iNode >> NArcs;

 double dcr;
 double buffer1[ NComm ];
 double buffer2[ NComm ];
 double buffer3;

 if( iFile3.fail() ){
    ifstream iFile1( fn.substr( 0 , fn.find_last_of( '.' ) ) + ".dcr" );
    ofstream iParam( fn.substr( 0 , fn.find_last_of( '.' ) ) + ".param" );
    ofstream iDCR( fn.substr( 0 , fn.find_last_of( '.' ) ) + ".dcr.tmp" );
    iFile1.ignore(1, '\n');
    for( int j = 0 ; j < NArcs+NNodes ; j++ ){
      iFile1 >> dcr; 
      iDCR << dcr << endl; 
    }
    for( int j = 0 ; j < NComm ; j++ ){
      iFile1 >> buffer1[ j ] >> buffer2[ j ]; 
    }
    iFile1 >> buffer3; 
    iParam << buffer3 << std::endl;
    for( int j = 0 ; j < NComm ; j++ ){
      iParam << buffer1[ j ] << " " << buffer2[ j ] << endl;
    }
    iFile1.close();
    iParam.close();

    std::filesystem::rename( fn.substr( 0 , fn.find_last_of( '.' ) ) + ".dcr.tmp" , 
        fn.substr( 0 , fn.find_last_of( '.' ) ) + ".dcr" );
 }

 ifstream iFile1( fn.substr( 0 , fn.find_last_of( '.' ) ) + ".dcr" );
 iFile3.open(fn.substr( 0 , fn.find_last_of( '.' ) ) + ".param");
 
 iFile3 >> mtu;

 fout<<"nflow,feasible,"
        //<<"lb_SOCP_CPX,ub_SOCP_CPX,cpu_SOCP_CPX,lb_PC_CPX,ub_PC_CPX,cpu_PC_CPX,"
        <<"lb_SOCP_GRB,ub_SOCP_GRB,cpu_SOCP_GRB,lb_PC_GRB,ub_PC_GRB,cpu_PC_GRB,"
        <<"lb_Benders,ub_Benders,cpu_Benders,itBen,itLag"<<endl;    
 
 for( int j = 0 ; j < min( 100 , NComm ) ; j++ ){  

  ifstream netcdf_file( fn.substr( 0 , fn.find_last_of( '.' ) ) 
      + "_" + std::to_string( j+1 ) + ".nc4" );

  if( netcdf_file.fail() ){

    ifstream iFile1( fn.substr( 0 , fn.find_last_of( '.' ) ) + ".dcr" );
    ifstream iArc( fn.substr( 0 , fn.find_last_of( '.' ) ) + ".arc" );

    ofstream foutdmx( "output.dmx" );
    ofstream foutdcr( "output.dcr" );

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

    ifstream fndmx1( "output.dmx" );
    ifstream fndcr1( "output.dcr" );
    TestBlockDS->load( fndmx1 );
    Index NNode = TestBlockDS->get_NNodes();
    Index NArc = TestBlockDS->get_NArcs();
    TestBlockDS->load_dcr( fndcr1 , NNode , NArc );
    fndmx1.close();
    fndcr1.close();

    netCDF::NcFile dataFile( fn.substr( 0 , fn.find_last_of( '.' ) ) + 
        "_" + std::to_string( j+1 ) + ".nc4" , netCDF::NcFile::replace );
    netCDF::NcGroup block = dataFile.addGroup( "Block_0" );
    netCDF::NcAtt *att;
    auto int_type = netCDF::NcInt();
    dataFile.putAtt( "SMS++_file_type" , int_type , 1 );
    TestBlockDS->serialize( block );
    TestBlockDS->deserialize( block );
    dataFile.close();
  }
  /*
  TestBlock_socp_CPX = dynamic_cast< SingleFlowDCRBlock * >
        ( Block::deserialize( fn.substr( 0 , fn.find_last_of( '.' ) ) 
            + "_" + std::to_string( j+1 ) + ".nc4" ) );

  TestBlock_pc_CPX = dynamic_cast< SingleFlowDCRBlock * >
        ( Block::deserialize( fn.substr( 0 , fn.find_last_of( '.' ) ) 
            + "_" + std::to_string( j+1 ) + ".nc4" ) );
  */
  TestBlock_socp_GRB = dynamic_cast< SingleFlowDCRBlock * >
        ( Block::deserialize( fn.substr( 0 , fn.find_last_of( '.' ) ) 
            + "_" + std::to_string( j+1 ) + ".nc4" ) );

  TestBlock_pc_GRB = dynamic_cast< SingleFlowDCRBlock * >
        ( Block::deserialize( fn.substr( 0 , fn.find_last_of( '.' ) ) 
            + "_" + std::to_string( j+1 ) + ".nc4" ) );

  TestBlock_ben = dynamic_cast< SingleFlowDCRBlock * >
        ( Block::deserialize( fn.substr( 0 , fn.find_last_of( '.' ) ) 
            + "_" + std::to_string( j+1 ) + ".nc4" ) );
  /*
  auto c_socp_CPX = Configuration::deserialize( "BSPar_socp_CPX.txt" );
  auto bsc_socp_CPX = dynamic_cast< BlockSolverConfig * >( c_socp_CPX );

  auto c_pc_CPX = Configuration::deserialize( "BSPar_pc_CPX.txt" );
  auto bsc_pc_CPX = dynamic_cast< BlockSolverConfig * >( c_pc_CPX );
  */
  auto c_socp_GRB = Configuration::deserialize( "BSPar_socp_GRB.txt" );
  auto bsc_socp_GRB = dynamic_cast< BlockSolverConfig * >( c_socp_GRB );

  auto c_pc_GRB = Configuration::deserialize( "BSPar_pc_GRB.txt" );
  auto bsc_pc_GRB = dynamic_cast< BlockSolverConfig * >( c_pc_GRB );

  auto c_ben = Configuration::deserialize( "BSPar_ben.txt" );
  auto bsc_ben = dynamic_cast< BlockSolverConfig * >( c_ben );

  //bsc_socp_CPX->apply( TestBlock_socp_CPX );
  //bsc_pc_CPX->apply( TestBlock_pc_CPX );
  bsc_socp_GRB->apply( TestBlock_socp_GRB );
  bsc_pc_GRB->apply( TestBlock_pc_GRB );

  bsc_ben->apply( TestBlock_ben );

  auto dcrb_socp = dynamic_cast< BlockConfig * >
          ( Configuration::deserialize( "DCRCfgSOCP.txt" ) );

  auto dcrb_pc = dynamic_cast< BlockConfig * >
          ( Configuration::deserialize( "DCRCfgPC.txt" ) );

  //dcrb_pc->apply( TestBlock_pc_CPX );
  //dcrb_socp->apply( TestBlock_socp_CPX );

  dcrb_pc->apply( TestBlock_pc_GRB );
  dcrb_socp->apply( TestBlock_socp_GRB );

  dcrb_socp->apply( TestBlock_ben );

    bool feasible = TestBlock_ben->is_feasible_instance();

    //Solver * socp_CPX = TestBlock_socp_CPX->get_registered_solvers().front();
    //Solver * pc_CPX = TestBlock_pc_CPX->get_registered_solvers().front();
    Solver * socp_GRB = TestBlock_socp_GRB->get_registered_solvers().front();
    Solver * pc_GRB = TestBlock_pc_GRB->get_registered_solvers().front();
    Solver * ben = TestBlock_ben->get_registered_solvers().front();

    //bool optimal_socp_CPX = true;
    //bool optimal_pc_CPX = true;
    bool optimal_socp_GRB = true;
    bool optimal_pc_GRB = true;

    /*
        std::clock_t c_start = std::clock();
        int rtrn = socp_CPX->compute();
        double lbound = socp_CPX->get_lb();
        double ubound = socp_CPX->get_ub();
        double opt_val_socp_CPX = ubound;
        std::clock_t c_end = std::clock();
        double time =  1000.0 * (c_end-c_start) / CLOCKS_PER_SEC / 1000.0;

        if( ( ubound - lbound ) / ubound > 1e-6 )
          optimal_socp_CPX = false;

        if( feasible )
          fout<<j+1<<",true,"<<lbound<<","<<ubound<<","<<time;   
        else
          fout<<j+1<<",false,"<<lbound<<","<<ubound<<","<<time;   

        c_start = std::clock();
        rtrn = pc_CPX->compute();
        lbound = pc_CPX->get_lb();
        ubound = pc_CPX->get_ub();
        auto opt_val_pc_CPX = ubound;
        c_end = std::clock();
        time =  1000.0 * (c_end-c_start) / CLOCKS_PER_SEC / 1000.0;
        fout<<","<<lbound<<","<<ubound<<","<<time;   

        if( ( ubound - lbound ) / ubound > 1e-6 )
          optimal_pc_CPX = false;
    */
        std::clock_t c_start = std::clock();
        int rtrn = socp_GRB->compute();
        double lbound = socp_GRB->get_lb();
        double ubound = socp_GRB->get_ub();
        auto opt_val_socp_GRB = ubound;
        std::clock_t c_end = std::clock();
        double time =  1000.0 * (c_end-c_start) / CLOCKS_PER_SEC / 1000.0;

        if( ( ubound - lbound ) / ubound > 1e-6 )
          optimal_socp_GRB = false;

        if( feasible )                                                                                                                                                                                     
          fout<<j+1<<",true,"<<lbound<<","<<ubound<<","<<time;                                                                                                                                             
        else                                                                                                                                                                                               
          fout<<j+1<<",false,"<<lbound<<","<<ubound<<","<<time;

        c_start = std::clock();
        rtrn = pc_GRB->compute();
        lbound = pc_GRB->get_lb();
        ubound = pc_GRB->get_ub();
        auto opt_val_pc_GRB = ubound;
        c_end = std::clock();
        time =  1000.0 * (c_end-c_start) / CLOCKS_PER_SEC / 1000.0;
        fout<<","<<lbound<<","<<ubound<<","<<time;   

        if( ( ubound - lbound ) / ubound > 1e-6 )
          optimal_pc_GRB = false;
	
        c_start = std::clock();
        rtrn = ben->compute();
        lbound = ben->get_lb();
        ubound = ben->get_ub();
        c_end = std::clock();
        auto Ben_lb = lbound; 
        auto Ben_ub = ubound;
        time =  1000.0 * (c_end-c_start) / CLOCKS_PER_SEC / 1000.0;

        fout<<","<<lbound<<","<<ubound<<","<<time<<","
            <<dynamic_cast< SingleFlowDCRBendersSolver * >( ben )->get_BenIt()<<","
            <<dynamic_cast< SingleFlowDCRBendersSolver * >( ben )->get_LagIt(); 

        if( feasible && Ben_lb < 0 && ! dynamic_cast< SingleFlowDCRBendersSolver * >( ben )->is_DCR_feasible())
          std::cout << "ERROR in " << fn << " at flow " << j+1 << ": Ben_lb < 0 but DCR infeasible !" << std::endl;

        if( feasible && Ben_lb < 0 && dynamic_cast< SingleFlowDCRBendersSolver * >( ben )->is_DCR_feasible())
          std::cout << "ERROR in " << fn << " at flow " << j+1 << ": Ben_lb < 0 but DCR feasible !" << std::endl;

        if( ! feasible && dynamic_cast< SingleFlowDCRBendersSolver * >( ben )->is_DCR_feasible() )
          std::cout << "ERROR in " << fn << " at flow " << j+1 << ": not feasible for ERA-I but DCR feasible !" << std::endl;

        auto opt_val_socp = opt_val_socp_GRB;
        auto opt_val_pc = opt_val_pc_GRB;

        //if( ! TestBlock_pc_GRB->is_feasible() )
          //std::cout << j << " is infeasible" << std::endl;
        
        if( ( ( ( opt_val_pc - Ben_lb ) / opt_val_pc < -1e-2 || ( opt_val_pc - Ben_ub ) / opt_val_pc > 1e-2 ) )
          && ( ( opt_val_socp - Ben_lb ) / opt_val_socp < -1e-2 || ( opt_val_socp - Ben_ub ) / opt_val_socp > 1e-2 ) 
	    && ( optimal_socp_GRB || optimal_pc_GRB ) //&& optimal_socp_CPX && optimal_pc_CPX 
	    //&& abs( opt_val_socp_CPX - opt_val_pc_CPX ) / opt_val_socp_CPX < 1e-3 
	    //&& abs( opt_val_socp_CPX - opt_val_socp_GRB ) / opt_val_socp_CPX < 1e-3 
	    //&& abs( opt_val_socp_CPX - opt_val_pc_GRB ) / opt_val_socp_CPX < 1e-3 
          && feasible ){
            std::cout << "ERROR in " << fn << " at flow " << j+1 << ": " << 
                opt_val_socp << "," << opt_val_pc << " not in [" << Ben_lb << "," << Ben_ub << "]";
            if( dynamic_cast< SingleFlowDCRBendersSolver * >( ben )->is_DCR_feasible() )
              std::cout << " but it is DCR feasible ! ";
              std::cout << std::endl;
          }

  fout<<endl;
 }

 return( 0 );

 }  // end( main )

/*--------------------------------------------------------------------------*/
/*------------------------ End File test.cpp -------------------------------*/
/*--------------------------------------------------------------------------*/
