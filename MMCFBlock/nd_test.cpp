/*--------------------------------------------------------------------------*/
/*--------------------------- File nd_test.cpp -----------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Main for testing MMCFNetworkDesignBlock: the same instance of the
 * Multicommodity Min-Cost Flow Network Design problem is solved in the
 * monolithic formulation, typically by a MILPSolver, and in the Benders one,
 * typically by a MILPSolver whose lazy-constraint callback separates the
 * Benders cuts out of a LagrangianDualSolver of the hidden copy of the
 * MMCFBlock, and the two optimal values are compared. Then, for some random
 * y, the value of the Benders cut at y is compared with the cost of the flow
 * at y, i.e., the value of the monolithic formulation with the y fixed
 * minus the fixed costs: the cut must never be above it, and it should be
 * close to it.
 *
 * Usage:
 *
 *   MMCFND_test [-t frmt] -m mono-BlockConfig -M mono-BlockSolverConfig
 *               -b Benders-BlockConfig -B Benders-BlockSolverConfig
 *               [-r samples] [-e seed] [-g gap] < instance >
 *
 * frmt is the format of MMCFBlock::load() ('s' for the Canad instances, the
 * default), gap the relative tolerance of the comparisons (1e-5 by default).
 *
 * \author Donato Meoli \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Donato Meoli
 */
/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include "MMCFNetworkDesignBlock.h"

#include "BlockSolverConfig.h"
#include "Solver.h"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>

#include <unistd.h>

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

using Index = Block::Index;

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

namespace {

/// loads the instance in a MMCFNetworkDesignBlock, with its BlockConfig and
/// BlockSolverConfig read from file

MMCFNetworkDesignBlock * build( const std::string & inst , char frmt ,
				const std::string & bcf ,
				const std::string & scf )
{
 auto blk = new MMCFNetworkDesignBlock;
 std::ifstream f( inst );
 if( ! f.is_open() )
  throw( std::invalid_argument( "cannot open " + inst ) );
 blk->load( f , frmt );

 auto bc = dynamic_cast< BlockConfig * >( Configuration::deserialize( bcf ) );
 if( ! bc )
  throw( std::invalid_argument( bcf + " is not a BlockConfig" ) );
 bc->apply( blk );
 delete bc;

 blk->generate_abstract_variables();
 blk->generate_abstract_constraints();
 blk->generate_objective();

 auto bsc = dynamic_cast< BlockSolverConfig * >(
                                           Configuration::deserialize( scf ) );
 if( ! bsc )
  throw( std::invalid_argument( scf + " is not a BlockSolverConfig" ) );
 bsc->apply( blk );
 delete bsc;

 if( blk->get_registered_solvers().empty() )
  throw( std::logic_error( "no Solver for the MMCFNetworkDesignBlock" ) );
 return( blk );
 }

/*--------------------------------------------------------------------------*/

bool close( double a , double b , double gap ) {
 return( std::abs( a - b ) <= gap * std::max( 1.0 , std::abs( b ) ) );
 }

/*--------------------------------------------------------------------------*/

void cleanup( MMCFNetworkDesignBlock * blk ) {
 blk->unregister_Solvers( true );  // and deletes them
 delete blk;
 }

}  // end( anonymous namespace )

/*--------------------------------------------------------------------------*/
/*--------------------------------- MAIN -----------------------------------*/
/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 char frmt = 's';
 std::string mb , ms , bb , bs;
 int samples = 5 , seed = 0;
 double gap = 1e-5;

 for( int opt ; ( opt = getopt( argc , argv , "t:m:M:b:B:r:e:g:" ) ) != -1 ; )
  switch( opt ) {
   case 't': frmt = optarg[ 0 ]; break;
   case 'm': mb = optarg; break;
   case 'M': ms = optarg; break;
   case 'b': bb = optarg; break;
   case 'B': bs = optarg; break;
   case 'r': samples = std::atoi( optarg ); break;
   case 'e': seed = std::atoi( optarg ); break;
   case 'g': gap = std::atof( optarg ); break;
   default:
    std::cerr << "usage: " << argv[ 0 ] << " [-t frmt] -m BC -M BSC -b BC"
              << " -B BSC [-r samples] [-e seed] [-g gap] instance"
              << std::endl;
    return( 1 );
   }
 if( ( optind >= argc ) || mb.empty() || ms.empty() || bb.empty() ||
     bs.empty() ) {
  std::cerr << argv[ 0 ] << ": the instance and the four Configuration "
            << "are required" << std::endl;
  return( 1 );
  }
 const std::string inst( argv[ optind ] );

 std::cout << std::setprecision( 10 );
 bool ok = true;

 // the two formulations - - - - - - - - - - - - - - - - - - - - - - - - - -
 auto mono = build( inst , frmt , mb , ms );
 auto bend = build( inst , frmt , bb , bs );
 if( ( mono->get_formulation() != MMCFNetworkDesignBlock::kMonolithic ) ||
     ( bend->get_formulation() != MMCFNetworkDesignBlock::kBenders ) ) {
  std::cerr << "the BlockConfig give the wrong formulations" << std::endl;
  return( 1 );
  }

 auto smono = mono->get_registered_solvers().front();
 auto sbend = bend->get_registered_solvers().front();

 const int st1 = smono->compute();
 const double v1 = smono->get_var_value();
 std::cout << inst << " | mono = " << v1 << " (" << st1 << ")" << std::endl;
 const int st2 = sbend->compute();
 const double v2 = sbend->get_var_value();
 const bool same = ( st1 == Solver::kOK ) && ( st2 == Solver::kOK ) &&
                   close( v2 , v1 , gap );
 ok &= same;
 std::cout << inst << " | Benders = " << v2 << " (" << st2 << ") -> "
           << ( same ? "OK" : "KO" ) << std::endl;

 // the cuts at random y- - - - - - - - - - - - - - - - - - - - - - - - - -
 const Index m = mono->get_MMCFBlock()->get_NArcs();
 const auto & F = mono->get_MMCFBlock()->get_F();
 std::mt19937 rg( seed );
 std::bernoulli_distribution open( 0.8 );
 for( int r = 0 ; r < samples ; ++r ) {
  std::vector< double > y( m );
  double fy = 0;
  for( Index j = 0 ; j < m ; ++j ) {
   y[ j ] = open( rg ) ? 1 : 0;
   fy += F[ j ] * y[ j ];
   }

  // the cost of the flow at y: the monolithic formulation with y fixed
  for( Index j = 0 ; j < m ; ++j ) {
   auto yj = mono->get_y( j );
   if( yj->is_fixed() )
    yj->is_fixed( false );
   yj->set_value( y[ j ] );
   yj->is_fixed( true );
   }
  const int st = smono->compute();
  if( st == Solver::kInfeasible ) {
   std::cout << "  y" << r << ": infeasible, skipped" << std::endl;
   continue;
   }
  if( st != Solver::kOK ) {
   std::cout << "  y" << r << ": status " << st << " -> KO" << std::endl;
   ok = false;
   continue;
   }
  const double phi = smono->get_var_value() - fy;

  // the Benders cut there
  for( Index j = 0 ; j < m ; ++j )
   bend->get_y( j )->set_value( y[ j ] );
  bend->separate_Benders_cut( -1 );
  const double cv = bend->get_last_cut_value();

  const bool valid = cv <= phi + gap * std::max( 1.0 , std::abs( phi ) );
  const bool tight = close( cv , phi , 1e-3 );
  ok &= valid;
  std::cout << "  y" << r << ": flow = " << phi << " cut = " << cv
            << " Solver = " << bend->get_last_Solver_value() << " -> "
            << ( valid ? ( tight ? "OK" : "OK (loose)" ) : "KO" ) << std::endl;
  }

 cleanup( mono );
 cleanup( bend );

 std::cout << ( ok ? "OK" : "KO" ) << std::endl;
 return( ok ? 0 : 1 );
 }

/*--------------------------------------------------------------------------*/
/*-------------------------- End File nd_test.cpp --------------------------*/
/*--------------------------------------------------------------------------*/
