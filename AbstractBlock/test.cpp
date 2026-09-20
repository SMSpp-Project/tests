/*--------------------------------------------------------------------------*/
/*-------------------------- File test.cpp ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Tester for AbstractBlock::mirror(), i.e., for the copy of the abstract
 * representation of a Block that any Block has without having written a line
 * for it.
 *
 * The copy is checked to be the same problem as the original, by solving the
 * two with the same Solver and comparing the optimal values; to move solution
 * information back to the original; and to follow the original when this
 * changes, which an UpdateSolver attached to the latter does by mapping every
 * Modification forward.
 *
 * Two originals are used: an AbstractBlock carrying one object of each kind
 * the mirror knows, inner Block comprised, and a BinaryKnapsackBlock, which
 * has a physical representation of its own and generates its abstract one.
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

#include <cmath>
#include <iomanip>
#include <iostream>

#include "AbstractBlock.h"
#include "BinaryKnapsackBlock.h"
#include "BlockSolverConfig.h"
#include "ColVariable.h"
#include "DQuadFunction.h"
#include "FRealObjective.h"
#include "FRowConstraint.h"
#include "LinearFunction.h"
#include "OneVarConstraint.h"
#include "Solver.h"
#include "UpdateSolver.h"

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*----------------------------- GLOBALS ------------------------------------*/
/*--------------------------------------------------------------------------*/

static int failed = 0;

static std::string solver_name = "CPXMILPSolver";

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

static void check( const std::string & what , double a , double b ,
                   double eps );

/*--------------------------------------------------------------------------*/
/// solves \p blck with the Solver the command line asks for
/** Attaches the Solver to \p blck, computes, and detaches it. Note that
 * clearing a BlockSolverConfig un-registers *every* Solver of the Block and
 * deletes it, so whoever holds one of its own takes it off first. */

static double solve( Block * blck )
{
 auto bsc = new BlockSolverConfig();
 bsc->add_ComputeConfig( std::string( solver_name ) , nullptr );
 bsc->apply( blck );

 if( blck->get_registered_solvers().empty() ) {
  std::cerr << "no Solver " << solver_name << " in the factory" << std::endl;
  std::exit( 1 );
  }

 auto slvr = blck->get_registered_solvers().back();
 slvr->compute();
 const double value = slvr->get_var_value();

 bsc->clear();
 bsc->apply( blck );
 delete bsc;

 return( value );
 }

/*--------------------------------------------------------------------------*/
/// solves the original and the copy and compares, keeping \p us attached

static void check_pair( const std::string & what , Block * orig , Block * copy ,
                        Solver * us )
{
 orig->unregister_Solver( us , false );
 const double a = solve( orig );
 const double b = solve( copy );
 orig->register_Solver( us );
 check( what , a , b , 1e-9 );
 }

/*--------------------------------------------------------------------------*/

static void check( const std::string & what , double a , double b ,
                   double eps )
{
 const bool ok = std::abs( a - b ) <= eps * std::max( 1.0 , std::abs( a ) );

 std::cout << std::left << std::setw( 36 ) << what << std::right
           << std::setw( 14 ) << std::setprecision( 10 ) << a
           << std::setw( 14 ) << b << ( ok ? "   OK" : "   *** DIFFERENT" )
           << std::endl;

 if( ! ok )
  ++failed;
 }

/*--------------------------------------------------------------------------*/
/// an AbstractBlock with one object of each kind the mirror knows
/** Static and dynamic Constraint, the OneVarConstraint that carry the bounds,
 * FRowConstraint over a LinearFunction, a quadratic Objective, and an inner
 * Block whose Constraint is written in the Variable of the father as well as
 * in its own, which is what a copy has to keep. Every Variable is continuous:
 * the Objective is quadratic, and a general-purpose Solver does not take an
 * integer Variable there. */

static AbstractBlock * build_toy( void )
{
 auto ab = new AbstractBlock();
 ab->set_name( std::string( "toy" ) );

 auto x = new std::vector< ColVariable >( 4 );
 for( auto & var : *x )
  var.set_type( ColVariable::kContinuous , eNoMod );
 ab->add_static_variable( *x , "x" );

 auto bx = new std::vector< BoxConstraint >( 4 );
 for( Index i = 0 ; i < 4 ; ++i ) {
  (*bx)[ i ].set_variable( & (*x)[ i ] , eNoMod );
  (*bx)[ i ].set_lhs( 0 , eNoMod );
  (*bx)[ i ].set_rhs( 3 , eNoMod );
  }
 ab->add_static_constraint( *bx , "box" );

 auto nn = new std::vector< NNConstraint >( 1 );
 (*nn)[ 0 ].set_variable( & (*x)[ 0 ] , eNoMod );
 ab->add_static_constraint( *nn , "nonneg" );

 auto rows = new std::list< FRowConstraint >( 2 );
 auto it = rows->begin();
 it->set_function( new LinearFunction( { { & (*x)[ 0 ] , 1 } ,
                                         { & (*x)[ 1 ] , 1 } ,
                                         { & (*x)[ 2 ] , 1 } ,
                                         { & (*x)[ 3 ] , 1 } } ) , eNoMod );
 it->set_lhs( 4 , eNoMod );
 it->set_rhs( Inf< double >() , eNoMod );
 ++it;
 it->set_function( new LinearFunction( { { & (*x)[ 0 ] , 1 } ,
                                         { & (*x)[ 1 ] , -1 } } ) , eNoMod );
 it->set_lhs( - Inf< double >() , eNoMod );
 it->set_rhs( 1 , eNoMod );
 ab->add_dynamic_constraint( *rows , "rows" );

 auto of = new DQuadFunction( { { & (*x)[ 0 ] , 3 , 1 } ,
                                { & (*x)[ 1 ] , 2 , 0 } ,
                                { & (*x)[ 2 ] , -1 , 0.5 } ,
                                { & (*x)[ 3 ] , 1 , 0 } } );
 auto obj = new FRealObjective( ab , of );
 obj->set_sense( Objective::eMin , eNoMod );
 ab->set_objective( obj , eNoMod );

 auto in = new AbstractBlock( ab );
 in->set_name( std::string( "inner" ) );

 auto y = new std::vector< ColVariable >( 2 );
 for( auto & var : *y )
  var.set_type( ColVariable::kPosUnitary , eNoMod );
 in->add_static_variable( *y , "y" );

 auto irows = new std::vector< FRowConstraint >( 1 );
 (*irows)[ 0 ].set_function( new LinearFunction(
                              { { & (*y)[ 0 ] , 1 } , { & (*y)[ 1 ] , 1 } ,
                                { & (*x)[ 0 ] , -1 } } ) , eNoMod );
 (*irows)[ 0 ].set_lhs( - Inf< double >() , eNoMod );
 (*irows)[ 0 ].set_rhs( 0 , eNoMod );
 in->add_static_constraint( *irows , "link" );

 auto iof = new LinearFunction( { { & (*y)[ 0 ] , -2 } ,
                                  { & (*y)[ 1 ] , -3 } } );
 auto iobj = new FRealObjective( in , iof );
 iobj->set_sense( Objective::eMin , eNoMod );
 in->set_objective( iobj , eNoMod );

 ab->add_nested_Block( in );

 return( ab );
 }

/*--------------------------------------------------------------------------*/
/*-------------------------------- main() ----------------------------------*/
/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 if( argc > 1 )
  solver_name = argv[ 1 ];

 std::cout << std::left << std::setw( 36 ) << "case" << std::right
           << std::setw( 14 ) << "original" << std::setw( 14 ) << "mirror"
           << std::endl;

 {  // an AbstractBlock with an inner Block and a quadratic Objective - - - -

  auto orig = build_toy();
  auto copy = new AbstractBlock();
  copy->mirror( orig );

  for( auto & issue : copy->get_mirror_issues() )
   std::cerr << "  issue: " << issue << std::endl;

  check( "toy" , solve( orig ) , solve( copy ) , 1e-9 );

  // the solution of the copy written back into the original
  copy->mirror_write();
  double diff = 0;
  auto xo = orig->get_static_variable_v< ColVariable >( 0 );
  for( auto & var : *xo )
   diff += std::abs( var.get_value() -
                     copy->mirror_of( & var )->get_value() );
  check( "gap after mapping back" , 0 , diff , 0 );

  delete copy;
  delete orig;
  }

 {  // a Block that is not an AbstractBlock, and a change of it - - - - - - -

  auto bkb = new BinaryKnapsackBlock();
  const Index n = 12;
  BinaryKnapsackBlock::doubleVec weights( n ) , profits( n );
  for( Index i = 0 ; i < n ; ++i ) {
   weights[ i ] = 3 + ( ( 7 * i ) % 11 );
   profits[ i ] = 5 + ( ( 13 * i ) % 17 );
   }
  bkb->load( n , 30 , weights , profits );
  bkb->generate_abstract_variables();
  bkb->generate_abstract_constraints();
  bkb->generate_objective();

  auto copy = new AbstractBlock();
  copy->mirror( bkb );

  for( auto & issue : copy->get_mirror_issues() )
   std::cerr << "  issue: " << issue << std::endl;

  check( "knapsack" , solve( bkb ) , solve( copy ) , 1e-9 );

  /* An UpdateSolver attached to the original maps forward to the copy every
   * Modification the original issues, which is what keeps the two the same
   * problem with nobody writing a line. */

  auto us = new UpdateSolver( copy );
  bkb->register_Solver( us );

  if( auto cnst = bkb->get_static_constraint< FRowConstraint >( 0 ) ) {
   cnst->set_rhs( 20 );
   check_pair( "after the capacity is changed" , bkb , copy , us );
   }

  if( auto obj = dynamic_cast< FRealObjective * >( bkb->get_objective() ) )
   if( auto lf = dynamic_cast< LinearFunction * >( obj->get_function() ) ) {
    lf->modify_coefficient( 0 , 100 );
    check_pair( "after a profit is changed" , bkb , copy , us );
    }

  auto var = bkb->get_static_variable_v< ColVariable >( 0 );
  if( var && ( var->size() > 1 ) ) {
   (*var)[ 1 ].set_value( 0 );
   (*var)[ 1 ].is_fixed( true , eModBlck );
   check_pair( "after a Variable is fixed" , bkb , copy , us );
   }

  bkb->unregister_Solver( us , false );
  delete us;
  delete copy;
  delete bkb;
  }

 std::cout << ( failed ? "*** SOME TEST FAILED" : "All tests passed!!" )
           << std::endl;

 return( failed );
 }

/*--------------------------------------------------------------------------*/
/*---------------------------- End File test.cpp ---------------------------*/
/*--------------------------------------------------------------------------*/
