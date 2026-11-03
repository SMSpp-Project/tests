/*--------------------------------------------------------------------------*/
/*--------------------------- File test_prune.cpp --------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Unit test of PolyhedralFunction::remove_parallel_rows(), i.e., of the
 * geometric pruning of the rows of a PolyhedralFunction, which needs no
 * Solver: among two parallel rows the dominated one, i.e., the one with the
 * worse constant, has to go and the other has to stay.
 *
 * A PolyhedralFunction over R^2 is built whose rows grow in all directions,
 * plus a row parallel to (and dominated by) one of them and a constant row
 * below (above) all the others, which is inactive but parallel to nothing
 * and therefore not the business of this method. The check is that exactly
 * the dominated row is removed, in three cases: a convex function with the
 * dominated row after the dominating one, a convex function with the
 * dominated row before it, and a concave function, where the dominated row
 * is the one with the larger constant. The LP-based removal of the inactive
 * rows is tested with PolyhedralFunctionBlock, which owns it.
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
#include <string>

#include "Block.h"
#include "ColVariable.h"
#include "PolyhedralFunction.h"

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
// builds the PolyhedralFunction of rows ( A , b ), convex or concave, prunes
// its parallel rows and checks that exactly the row with constant b_gone is
// removed while the one with constant b_kept, which is parallel to nothing,
// survives

static bool check( const std::string & name ,
		   PolyhedralFunction::MultiVector && A ,
		   PolyhedralFunction::RealVector && b , bool convex ,
		   double b_gone , double b_kept )
{
 std::vector< ColVariable > x( 2 );
 PolyhedralFunction::VarVector xv{ & x[ 0 ] , & x[ 1 ] };

 const auto bound = convex ? - Inf< PolyhedralFunction::FunctionValue >()
                           : Inf< PolyhedralFunction::FunctionValue >();

 PolyhedralFunction f( std::move( xv ) , std::move( A ) , std::move( b ) ,
		       bound , convex );

 const auto n0 = f.get_nrows();
 f.remove_parallel_rows();
 const auto n1 = f.get_nrows();

 const auto & nb = f.get_b();
 const bool gone = std::find( nb.begin() , nb.end() , b_gone ) == nb.end();
 const bool kept = std::find( nb.begin() , nb.end() , b_kept ) != nb.end();

 const bool ok = ( n1 + 1 == n0 ) && gone && kept;
 std::cout << name << ": rows " << n0 << " -> " << n1
	   << ( ok ? " -> OK" : " -> FAIL" ) << std::endl;
 return( ok );
 }

/*--------------------------------------------------------------------------*/

int main( void )
{
 bool ok = true;

 // convex, f( x ) = max { rows }:
 //  0: x0      1: -x0     2: x1      3: -x1   ( grow in all directions )
 //  4: x0 - 3            ( parallel to row 0 and dominated by it )
 //  5: -9                ( inactive, but parallel to nothing )
 ok &= check( "convex, dominated row last" ,
	      { { 1 , 0 } , { -1 , 0 } , { 0 , 1 } , { 0 , -1 } , { 1 , 0 } ,
		{ 0 , 0 } } ,
	      { 0 , 0 , 0 , 0 , -3 , -9 } , true , -3 , -9 );

 // the same with the dominated row first, which is then the one of the
 // pair met as the first index
 ok &= check( "convex, dominated row first" ,
	      { { 1 , 0 } , { -1 , 0 } , { 0 , 1 } , { 0 , -1 } , { 1 , 0 } ,
		{ 0 , 0 } } ,
	      { -3 , 0 , 0 , 0 , 0 , -9 } , true , -3 , -9 );

 // concave, f( x ) = min { rows }: the dominated row of a parallel pair is
 // the one with the larger constant
 ok &= check( "concave" ,
	      { { 1 , 0 } , { -1 , 0 } , { 0 , 1 } , { 0 , -1 } , { 1 , 0 } ,
		{ 0 , 0 } } ,
	      { 0 , 0 , 0 , 0 , 3 , 9 } , false , 3 , 9 );

 return( ok ? 0 : 1 );
 }

/*--------------------------------------------------------------------------*/
/*------------------------ End File test_prune.cpp -------------------------*/
/*--------------------------------------------------------------------------*/
