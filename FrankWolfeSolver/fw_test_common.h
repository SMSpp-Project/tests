/*--------------------------------------------------------------------------*/
/*----------------------- File fw_test_common.h ----------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Scaffolding shared by the FrankWolfeSolver testers (test.cpp, test-mcf.cpp):
 * building a father AbstractBlock out of K copies of a leaf Block, collecting
 * the father-objective variables, and building a random father objective.
 *
 * \author Antonio Frangioni \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Antonio Frangioni
 */
/*--------------------------------------------------------------------------*/

#ifndef __fw_test_common
 #define __fw_test_common

#include "common_utils.h"

#include "AbstractBlock.h"
#include "FRealObjective.h"
#include "DQuadFunction.h"
#include "QuadFunction.h"
#include "PolyhedralFunction.h"
#include "ColVariable.h"

#include <random>
#include <string>
#include <vector>

/*--------------------------------------------------------------------------*/

namespace fwtest {

using namespace SMSpp_di_unipi_it;

using Index = Block::Index;
using FunctionValue = Function::FunctionValue;
using Coefficient = DQuadFunction::Coefficient;

/*--------------------------------------------------------------------------*/

/// a random number in [-1,1] drawn from rg
inline double rnd( std::mt19937 & rg )
{ std::uniform_real_distribution<> d( 0.0 , 1.0 ); return( 2 * d( rg ) - 1 ); }

/// a random number in [0,1) drawn from rg
inline double pos( std::mt19937 & rg )
{ std::uniform_real_distribution<> d( 0.0 , 1.0 ); return( d( rg ) ); }

/*--------------------------------------------------------------------------*/

/// collect the ColVariables that the father objective will be built over: the
/// named static-variable groups of @p b (if @p groups is non-empty), else the
/// variables active in @p b's Objective.
///
/// FrankWolfeSolver requires every father-Objective variable to be active in
/// some sub-Block Objective. For a Block whose objective spans every variable
/// (e.g. MCFBlock) collecting from the objective takes them all; for a Block
/// with variables outside its objective (e.g. ThermalUnitBlock, whose
/// formulation has auxiliary objective variables) the named groups pick the
/// "physical" variables a generic LMO actually sets and prices.

inline void collect_vars( Block * b , const std::vector< std::string > & groups ,
                          std::vector< ColVariable * > & vars )
{
 if( ! groups.empty() ) {
  for( const auto & name : groups ) {
   auto grp = b->get_static_variable_v< ColVariable >( name );
   if( ! grp )
    throw( std::invalid_argument( "collect_vars: no variable group named '" +
                                  name + "' in the sub-Block" ) );
   for( auto & v : *grp )
    vars.push_back( & v );
   }
  return;
  }

 auto obj = dynamic_cast< FRealObjective * >( b->get_objective() );
 if( ! obj )
  throw( std::invalid_argument( "collect_vars: child has no FRealObjective" ) );
 auto f = obj->get_function();
 const Index n = f->get_num_active_var();
 for( Index i = 0 ; i < n ; ++i )
  vars.push_back( static_cast< ColVariable * >( f->get_active_var( i ) ) );
 }

/*--------------------------------------------------------------------------*/

/// build a father AbstractBlock with @p k copies of the leaf Block read from
/// @p filename as its sub-Block, applying the BlockConfig @p bconf (if
/// non-empty) to each, and collecting the father-objective variables (see
/// collect_vars) into @p vars. No father objective is set.

inline AbstractBlock * build_father( const std::string & filename , int k ,
                                     const std::string & bconf ,
                                     const std::vector< std::string > & groups ,
                                     std::vector< ColVariable * > & vars )
{
 auto father = new AbstractBlock();
 vars.clear();
 for( int j = 0 ; j < k ; ++j ) {
  Block * child = Block::deserialize( filename , father );
  if( ! child )
   throw( std::invalid_argument( "build_father: cannot read Block from " +
                                 filename ) );
  if( ! bconf.empty() ) {
   Configuration * bc = Configuration::deserialize( bconf );
   b_config_Block( child , bc , bconf );
   delete bc;
   }
  child->generate_abstract_variables();
  child->generate_abstract_constraints();
  child->generate_objective();
  father->add_nested_Block( child );
  collect_vars( child , groups , vars );
  }
 return( father );
 }

/*--------------------------------------------------------------------------*/

/// random convex PolyhedralFunction data ( A , b ): max_r ( A_r.x + b_r )

inline void generate_poly( Index nv , int poly_rows , double scale ,
                           std::mt19937 & rg ,
                           PolyhedralFunction::MultiVector & A ,
                           PolyhedralFunction::RealVector & b )
{
 Index nr = poly_rows > 0 ? Index( poly_rows ) : nv + 1;
 A.assign( nr , PolyhedralFunction::RealVector( nv ) );
 b.assign( nr , 0 );
 for( Index r = 0 ; r < nr ; ++r ) {
  for( Index i = 0 ; i < nv ; ++i )
   A[ r ][ i ] = scale * rnd( rg );
  b[ r ] = scale * nv * rnd( rg ) / 4;
  }
 }

/*--------------------------------------------------------------------------*/

/// build a random father objective Function over @p vars: @p obj_type 0 a
/// DQuadFunction, 1 a QuadFunction, 2 a (convex) PolyhedralFunction

inline Function * make_father_objective( std::vector< ColVariable * > & vars ,
                                         int obj_type , double scale ,
                                         int poly_rows , std::mt19937 & rg )
{
 const Index nv = Index( vars.size() );

 if( obj_type == 0 ) {  // DQuadFunction: sum_i ( a_i x_i^2 + b_i x_i ), a_i > 0
  DQuadFunction::v_coeff_triple tr( nv );
  for( Index i = 0 ; i < nv ; ++i )
   tr[ i ] = std::make_tuple( vars[ i ] , Coefficient( scale * rnd( rg ) ) ,
                              Coefficient( scale * ( 0.5 + pos( rg ) ) ) );
  return( new DQuadFunction( std::move( tr ) ) );
  }

 if( obj_type == 1 ) {  // QuadFunction: off-diagonal terms (i+1,i), kept PSD by
                        // Gershgorin diagonal dominance ( 2 a_i >= sum |q| )
  QuadFunction::v_off_diag_term nd;
  std::vector< double > rowabs( nv , 0.0 );
  for( Index i = 0 ; i + 1 < nv ; ++i ) {
   double q = scale * 0.3 * rnd( rg );
   nd.push_back( std::make_tuple( i + 1 , i , Coefficient( q ) ) );
   rowabs[ i ]     += std::abs( q );
   rowabs[ i + 1 ] += std::abs( q );
   }
  DQuadFunction::v_coeff_triple tr( nv );
  for( Index i = 0 ; i < nv ; ++i )
   tr[ i ] = std::make_tuple( vars[ i ] , Coefficient( scale * rnd( rg ) ) ,
                              Coefficient( 0.5 * rowabs[ i ] +
                                           scale * ( 0.5 + pos( rg ) ) ) );
  return( new QuadFunction( std::move( tr ) , std::move( nd ) ) );
  }

 // obj_type == 2: convex PolyhedralFunction = max_r ( A_r . x + b_r )
 PolyhedralFunction::MultiVector A;
 PolyhedralFunction::RealVector b;
 generate_poly( nv , poly_rows , scale , rg , A , b );
 PolyhedralFunction::VarVector vv( vars.begin() , vars.end() );
 auto pf = new PolyhedralFunction( std::move( vv ) , std::move( A ) ,
                                   std::move( b ) , - Inf< FunctionValue >() );
 pf->set_is_convex( true , eNoMod );
 return( pf );
 }

/*--------------------------------------------------------------------------*/

}  // namespace fwtest

#endif  /* fw_test_common.h included */

/*--------------------------------------------------------------------------*/
/*------------------- End File fw_test_common.h ----------------------------*/
/*--------------------------------------------------------------------------*/
