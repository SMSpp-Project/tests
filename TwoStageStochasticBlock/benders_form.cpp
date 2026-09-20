/*--------------------------------------------------------------------------*/
/*------------------------- File benders_form.cpp --------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Implementation of benders_form(), which assembles the Benders form of a
 * two-stage stochastic instance around the Block a file gives.
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
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include "benders_form.h"

#include "AbstractPath.h"
#include "ColVariable.h"
#include "FRealObjective.h"
#include "FRowConstraint.h"
#include "LinearFunction.h"
#include "Objective.h"
#include "OneVarConstraint.h"

#include <list>

/*--------------------------------------------------------------------------*/
/*-------------------------- NAMESPACE & USING -----------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

using Index = Block::Index;

/*--------------------------------------------------------------------------*/
/*-------------------------- THE BENDERS FORM ------------------------------*/
/*--------------------------------------------------------------------------*/

AbstractBlock * benders_form( TwoStageStochasticBlock * tssb )
{
 const Index L = tssb->get_number_leaves();
 const auto & paths = tssb->get_paths_to_static_here_and_now_vars();

 std::vector< std::vector< ColVariable * > > xk( L );
 for( Index l = 0 ; l < L ; ++l ) {
  auto leaf = tssb->get_leaf_block( l );
  if( ! leaf )
   return( nullptr );

  for( const auto & p : paths ) {
   const auto nv = p->get_number_elements< ColVariable >( leaf );
   auto e = p->get_element< ColVariable >( leaf );
   for( Index j = 0 ; j < nv ; ++j )
    xk[ l ].push_back( e + j );
   }
  }

 const Index n = xk[ 0 ].size();
 if( ! n )
  return( nullptr );

 for( Index l = 1 ; l < L ; ++l )
  if( xk[ l ].size() != n )
   return( nullptr );

 auto root = new AbstractBlock();

 /* the box of a design Variable is not on the Variable: the units state it
  * as a OneVarConstraint of their own, which is one of the "active stuff" of
  * the Variable, so the bounds are collected from there and intersected with
  * whatever the Variable itself declares */

 auto x = new std::vector< ColVariable >( n );
 auto bnd = new std::vector< BoxConstraint >( n );

 for( Index j = 0 ; j < n ; ++j ) {
  auto lo = xk[ 0 ][ j ]->get_lb() , up = xk[ 0 ][ j ]->get_ub();
  for( Index a = 0 ; a < xk[ 0 ][ j ]->get_num_active() ; ++a )
   if( auto ovc = dynamic_cast< OneVarConstraint * >(
                                         xk[ 0 ][ j ]->get_active( a ) ) ) {
    lo = std::max( lo , double( ovc->get_lhs() ) );
    up = std::min( up , double( ovc->get_rhs() ) );
    }

  ( *bnd )[ j ].set_variable( & ( *x )[ j ] );
  ( *bnd )[ j ].set_lhs( lo );
  ( *bnd )[ j ].set_rhs( up );
  }

 root->add_static_variable( *x , "x" );
 root->add_static_constraint( *bnd , "design bound" );

 // the cost of the design, taken out of the Objective of every leaf
 auto rlf = new LinearFunction();
 std::vector< double > cost( n , 0 );

 for( Index l = 0 ; l < L ; ++l )
  for( Index j = 0 ; j < n ; ++j ) {
   auto var = xk[ l ][ j ];
   for( Index a = 0 ; a < var->get_num_active() ; ++a ) {
    auto obj = dynamic_cast< FRealObjective * >( var->get_active( a ) );
    if( ! obj )
     continue;

    if( auto lf = dynamic_cast< LinearFunction * >( obj->get_function() ) ) {
     const auto & vv = lf->get_v_var();
     for( Index i = 0 ; i < vv.size() ; ++i )
      if( vv[ i ].first == var ) {
       cost[ j ] += vv[ i ].second;
       lf->remove_variable( i );
       break;
       }
     }
    break;
    }
   }

 /* Every design Variable enters the Objective of the master, those that cost
  * nothing with a zero coefficient: a bundle solving the master in its
  * sparse mode asks its linear part to cover all the Variable the value
  * functions are active in, and they are active in all of them. */

 for( Index j = 0 ; j < n ; ++j )
  rlf->add_variable( & ( *x )[ j ] , cost[ j ] );

 auto robj = new FRealObjective( root , rlf );
 robj->set_sense( Objective::eMin );
 root->set_objective( robj );

 // one subproblem per leaf, the wrapper carrying the coupling
 for( Index l = 0 ; l < L ; ++l ) {
  auto leaf = tssb->get_leaf_block( l );
  auto sub = new AbstractBlock( root );

  /* the wrapper adds nothing to the Objective of the leaf, but it has to say
   * which way the subproblem goes: a Block with no Objective answers eUndef,
   * and the sign of the cuts is read off that */

  auto sof = new FRealObjective();
  sof->set_function( new LinearFunction() );
  sof->set_sense( Objective::eMin );
  sub->set_objective( sof );

  auto cns = new std::list< FRowConstraint >( n );
  Index j = 0;
  for( auto & c : *cns ) {
   auto lf = new LinearFunction();
   lf->add_variable( xk[ l ][ j ] , 1 );
   lf->add_variable( & ( *x )[ j ] , -1 );
   c.set_function( lf );
   c.set_lhs( - Inf< double >() );
   c.set_rhs( 0 );
   ++j;
   }

  sub->add_dynamic_constraint( *cns , "link" );
  leaf->set_f_Block( sub );
  sub->add_nested_Block( leaf );
  root->add_nested_Block( sub );
  }

 return( root );
 }

/*--------------------------------------------------------------------------*/
/*----------------------- End File benders_form.cpp ------------------------*/
/*--------------------------------------------------------------------------*/
