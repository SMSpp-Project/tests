/*--------------------------------------------------------------------------*/
/*--------------------------- File test_prune.cpp --------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Unit test of PolyhedralFunctionBlock::remove_redundant_rows(), i.e., of
 * the removal of all the redundant rows of the PolyhedralFunction of the
 * Block: the parallel (dominated) ones, which it leaves to
 * PolyhedralFunction::remove_parallel_rows(), and the inactive ones, which
 * it finds by solving an LP over the epigraph with the Solver of the
 * BlockSolverConfig in LPPar.txt.
 *
 * A convex PolyhedralFunction is built with a known parallel (dominated) row
 * and a known inactive row, and the pruning is checked to remove exactly
 * them. The geometric pruning alone is tested with PolyhedralFunction.
 *
 * \author Antonio Frangioni \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \author Donato Meoli \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Antonio Frangioni, Donato Meoli
 */
/*--------------------------------------------------------------------------*/

#include <algorithm>
#include <iostream>

#include "BlockSolverConfig.h"
#include "ColVariable.h"
#include "PolyhedralFunction.h"
#include "PolyhedralFunctionBlock.h"

using namespace SMSpp_di_unipi_it;

int main( void )
{
 // f( x ) = max { rows } over R^2, convex:
 //  0: x0      1: -x0     2: x1      3: -x1   ( grow in all directions )
 //  4: x0 - 3            ( parallel to row 0 and dominated by it )
 //  5: -9                ( parallel to nothing, below the others )
 std::vector< ColVariable > x( 2 );
 PolyhedralFunction::VarVector xv{ & x[ 0 ] , & x[ 1 ] };
 PolyhedralFunction::MultiVector A{ { 1 , 0 } , { -1 , 0 } , { 0 , 1 } ,
				    { 0 , -1 } , { 1 , 0 } , { 0 , 0 } };
 PolyhedralFunction::RealVector b{ 0 , 0 , 0 , 0 , -3 , -9 };

 PolyhedralFunction f( std::move( xv ) , std::move( A ) , std::move( b ) ,
		       - Inf< PolyhedralFunction::FunctionValue >() , true );

 const auto n0 = f.get_nrows();
 std::cout << "rows: " << n0;

 // both the parallel row ( row 4 ) and the inactive one ( row 5 ) go
 auto c = Configuration::deserialize( "LPPar.txt" );
 auto cfg = dynamic_cast< BlockSolverConfig * >( c );
 if( ! cfg ) { std::cerr << "LPPar.txt not a BlockSolverConfig\n";
               delete c;
               return( 1 ); }
 PolyhedralFunctionBlock pfb( nullptr , & f );
 pfb.remove_redundant_rows( cfg );
 const auto n1 = f.get_nrows();
 std::cout << " -> after redundant: " << n1 << std::endl;
 cfg->clear();
 delete cfg;

 // what is left are the four rows that grow in all directions
 const auto & nb = f.get_b();
 const bool ok = ( n0 == 6 ) && ( n1 == 4 ) &&
                 std::all_of( nb.begin() , nb.end() ,
			      []( double bi ) { return( bi == 0 ); } );
 std::cout << ( ok ? "-> OK ( parallel + inactive rows removed )"
		   : "-> FAIL" ) << std::endl;
 return( ok ? 0 : 1 );
 }

/*--------------------------------------------------------------------------*/
/*------------------------ End File test_prune.cpp -------------------------*/
/*--------------------------------------------------------------------------*/
