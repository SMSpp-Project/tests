/*--------------------------------------------------------------------------*/
/*----------------------------- File test2.cpp -----------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * This file contains the implementation of a series of tests for the
 * linearizations that are produced by the BendersBFunction. It constructs a
 * number of simple linear programming problems and solve them by Benders
 * decomposition. It uses *MILPSolver for solving the inner problem and
 * requires a file called solver.txt containing the description of a
 * BlockSolverConfig of a Solver for the master problem.
 *
 * \author Rafael Durbano Lobato \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Rafael Durbano Lobato
 */

/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include "AbstractBlock.h"
#include "BendersBFunction.h"
#include "BlockSolverConfig.h"
#include "FRealObjective.h"
#include "FRowConstraint.h"
#include "LinearFunction.h"
#include "OneVarConstraint.h"

#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>

/*--------------------------------------------------------------------------*/
/*--------------------------- NAMESPACE ------------------------------------*/
/*--------------------------------------------------------------------------*/

// namespace for the tests of the Structured Modeling System++ (SMS++)
using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*--------------------------------- TYPES ----------------------------------*/
/*--------------------------------------------------------------------------*/

using matrix = std::vector< std::vector< double > >;

/*--------------------------------------------------------------------------*/
/*------------------------------- CONSTANTS --------------------------------*/
/*--------------------------------------------------------------------------*/

const auto inf = Inf< double >();

std::string solver_filename;

/*--------------------------------------------------------------------------*/
/*------------------------------- FUNCTIONS --------------------------------*/
/*--------------------------------------------------------------------------*/

BlockSolverConfig * build_solver_config
( const std::string & config_file_path ) {

 std::ifstream config_file( config_file_path );
 if( ! config_file.is_open() )
  throw std::invalid_argument( "BendersBFunction test: Error: cannot open "
                               "configuration file " + config_file_path );

 auto bsc = new BlockSolverConfig;
 config_file >> ( * bsc );
 config_file.close();
 return( bsc );
}

/*--------------------------------------------------------------------------*/

bool is_identity( const matrix & A ) {
 if( A.empty() ) return( false );
 for( Index i = 0 ; i < A.size() ; ++i ) {
  if( A[ i ].size() != A.size() )
   return( false );
  if( A[ i ][ i ] != 1 ) return( false );
  for( Index j = 0 ; j < A.size() ; ++j )
   if( i != j && A[ i ][ j ] != 0.0 )
    return( false );
 }
 return( true );
}

/*--------------------------------------------------------------------------*/

/*
 * min  d'x
 * s.t. l <= y <= u
 *      M1 y + b1 <= Ex <= M2 y + b2
 *      f1 <= Fx <= f2
 *      l_x <= x <= u_x
 */

AbstractBlock * build_LP( const std::vector< double > & l ,
                          const std::vector< double > & u ,
                          const std::vector< double > & d ,
                          const matrix & E = {} ,
                          const matrix & M1 = {} , const matrix & M2 = {} ,
                          const std::vector< double > & b1 = {} ,
                          const std::vector< double > & b2 = {} ,
                          const matrix & F = {} ,
                          const std::vector< double > & f1 = {} ,
                          const std::vector< double > & f2 = {} ,
                          const std::vector< double > & l_x = {} ,
                          const std::vector< double > & u_x = {} )
{
 if( l.size() != u.size() )
  throw( std::invalid_argument( "The vectors l and u must have the "
                                "same size." ) );
 if( b1.size() != M1.size() )
  throw( std::invalid_argument( "The number of rows of M1 must be equal to "
                                "the size of b1." ) );
 if( b2.size() != M2.size() )
  throw( std::invalid_argument( "The number of rows of M2 must be equal to "
                                "the size of b2." ) );
 if( l_x.size() != u_x.size() )
  throw( std::invalid_argument( "The vectors l_x and u_x must have the "
                                "same size." ) );

 if( l_x.size() != d.size() )
  throw( std::invalid_argument( "The vectors l_x and d must have the "
                                "same size." ) );

 auto lp = new AbstractBlock;

 std::vector< ColVariable > * x = nullptr;
 std::vector< ColVariable > * y = nullptr;

 if( ! l.empty() ) {

  // y variable

  y = new std::vector< ColVariable >( l.size() );

  // bounds on y
  for( Index i = 0 ; i < l.size() ; ++i ) {
   if( l[ i ] == - 1 && u[ i ] == 1 )
    ( *y )[ i ].is_unitary( true );
   else if( l[ i ] == 0 && u[ i ] == 1 ) {
    ( *y )[ i ].is_unitary( true );
    ( *y )[ i ].is_positive( true );
   }
   else if( l[ i ] == -1 && u[ i ] == 0 ) {
    ( *y )[ i ].is_unitary( true );
    ( *y )[ i ].is_negative( true );
   }
   else {
    if( l[ i ] == 0 )
     ( *y )[ i ].is_positive( true );
    if( u[ i ] == 0 )
     ( *y )[ i ].is_negative( true );
   }
  }
  lp->add_static_variable( * y , "y" );
 }

 if( ! d.empty() ) {
  x = new std::vector< ColVariable >( d.size() );

  // bounds on x
  auto bounds_x = new std::vector< BoxConstraint >( l_x.size() );
  for( Index i = 0 ; i < l_x.size() ; ++i ) {
   (*bounds_x)[ i ].set_variable( &(*x)[ i ] );
   (*bounds_x)[ i ].set_lhs( l_x[ i ] );
   (*bounds_x)[ i ].set_rhs( u_x[ i ] );
  }

  // bounds on x
  for( Index i = 0 ; i < l_x.size() ; ++i ) {
   if( l_x[ i ] == - 1 && u_x[ i ] == 1 )
    ( *x )[ i ].is_unitary( true );
   else if( l_x[ i ] == 0 && u_x[ i ] == 1 ) {
    ( *x )[ i ].is_unitary( true );
    ( *x )[ i ].is_positive( true );
   }
   else if( l_x[ i ] == -1 && u_x[ i ] == 0 ) {
    ( *x )[ i ].is_unitary( true );
    ( *x )[ i ].is_negative( true );
   }
   else {
    if( l_x[ i ] == 0 )
     ( *x )[ i ].is_positive( true );
    if( u_x[ i ] == 0 )
     ( *x )[ i ].is_negative( true );
   }
  }


  lp->add_static_variable( * x , "x" );
  lp->add_static_constraint( * bounds_x , "bounds-x" );
 }

 // Objective

 auto objective_function = new LinearFunction();
 for( Index i = 0 ; i < ( *x ).size() ; ++i )
  objective_function->add_variable( &(*x)[ i ] , d[ i ] );
 lp->set_objective( new FRealObjective( lp , objective_function ) );


 // Constraints in x only
 // f1 <= Fx <= f2

 if( ! f1.empty() ) {
  auto x_constraints = new std::vector< FRowConstraint >( f1.size() );
  for( Index i = 0 ; i < f1.size() ; ++i ) {
   auto function = new LinearFunction();
   for( Index j = 0 ; j < F[ i ].size() ; ++j )
    function->add_variable( &( *x )[ j ] , F[ i ][ j ] );
   ( *x_constraints )[ i ].set_function( function );
   ( *x_constraints )[ i ].set_lhs( f1[ i ] );
   ( *x_constraints )[ i ].set_rhs( f2[ i ] );
  }
  lp->add_static_constraint( *x_constraints , "x-only" );
 }

 // Constraints in x and y
 // M1 y + b1 <= Ex <= M2 y + b2

 auto num_lower_xy_constraints =
  std::count_if( b1.begin() , b1.end() , [](double d) { return( d > -inf ); } );

 if( num_lower_xy_constraints > 0 ) {
  auto lower_xy_constraints =
   new std::vector< FRowConstraint >( num_lower_xy_constraints );
  Index i = 0;
  for( const auto b : b1 ) {
   if( b == -inf ) continue;
   auto function = new LinearFunction();
   for( Index j = 0 ; j < E[ i ].size() ; ++j )
    function->add_variable( &(*x)[ j ] , E[ i ][ j ] );
   for( Index j = 0 ; j < M1[ i ].size() ; ++j )
    function->add_variable( &(*y)[ j ] , -M1[ i ][ j ] );
   ( *lower_xy_constraints )[ i ].set_function( function );
   ( *lower_xy_constraints )[ i ].set_lhs( b );
   ( *lower_xy_constraints )[ i ].set_rhs( inf );
   ++i;
  }
  lp->add_static_constraint( *lower_xy_constraints , "xy-lower" );
 }

 auto num_upper_xy_constraints =
  std::count_if( b2.begin() , b2.end() , [](double d) { return( d < inf ); } );

 if( num_upper_xy_constraints > 0 ) {
  auto upper_xy_constraints =
   new std::vector< FRowConstraint >( num_upper_xy_constraints );
  Index i = 0;
  for( const auto b : b2 ) {
   if( b == inf ) continue;
   auto function = new LinearFunction();
   for( Index j = 0 ; j < E[ i ].size() ; ++j )
    function->add_variable( &(*x)[ j ] , E[ i ][ j ] );
   for( Index j = 0 ; j < M2[ i ].size() ; ++j )
    function->add_variable( &(*y)[ j ] , -M2[ i ][ j ] );
   ( *upper_xy_constraints )[ i ].set_function( function );
   ( *upper_xy_constraints )[ i ].set_lhs( -inf );
   ( *upper_xy_constraints )[ i ].set_rhs( b );
   ++i;
  }
  lp->add_static_constraint( *upper_xy_constraints , "xy-upper");
 }

 return( lp );
}

/*--------------------------------------------------------------------------*/

/*
 * min  f( y )
 * s.t. l <= y <= u
 *
 * f( y ) = min  d'x
 *          s.t. M1 y + b1 <= Ex <= M2 y + b2
 *               f1 <= Fx <= f2
 */

AbstractBlock * build_Benders_decomposition(
 Solver * inner_block_solver ,
 bool invert_data_mapping_order ,
 const std::vector< double > & l ,
 const std::vector< double > & u ,
 const std::vector< double > & d ,
 const matrix & E = {} ,
 const matrix & M1 = {} , const matrix & M2 = {} ,
 const std::vector< double > & b1 = {} ,
 const std::vector< double > & b2 = {} ,
 const matrix & F = {} ,
 const std::vector< double > & f1 = {} ,
 const std::vector< double > & f2 = {} ,
 const std::vector< double > & l_x = {} ,
 const std::vector< double > & u_x = {} )
{
 if( l.size() != u.size() )
  throw( std::invalid_argument( "The vectors l and u must have the "
                                "same size." ) );
 if( b1.size() != M1.size() )
  throw( std::invalid_argument( "The number of rows of M1 must be equal to "
                                "the size of b1." ) );
 if( b2.size() != M2.size() )
  throw( std::invalid_argument( "The number of rows of M2 must be equal to "
                                "the size of b2." ) );
 if( b1.size() != b2.size() )
  throw( std::invalid_argument( "b1 and b2 must have the same size." ) );

 if( f1.size() != F.size() )
  throw( std::invalid_argument( "The number of rows of F must be equal to "
                                "the size of f1." ) );
 if( f1.size() != f2.size() )
  throw( std::invalid_argument( "f1 and f2 must have the same size." ) );

 if( l_x.size() != u_x.size() )
  throw( std::invalid_argument( "The vectors l_x and u_x must have the "
                                "same size." ) );
 if( l_x.size() != d.size() )
  throw( std::invalid_argument( "The vectors l_x and d must have the "
                                "same size." ) );

 auto master = new AbstractBlock;

 // y variable

 auto y = new std::vector< ColVariable >( l.size() );

 // bounds on y
 for( Index i = 0 ; i < l.size() ; ++i ) {
  if( l[ i ] == - 1 && u[ i ] == 1 )
   ( *y )[ i ].is_unitary( true );
  else if( l[ i ] == 0 && u[ i ] == 1 ) {
   ( *y )[ i ].is_unitary( true );
   ( *y )[ i ].is_positive( true );
  }
  else if( l[ i ] == -1 && u[ i ] == 0 ) {
   ( *y )[ i ].is_unitary( true );
   ( *y )[ i ].is_negative( true );
  }
  else {
   if( l[ i ] == 0 )
    ( *y )[ i ].is_positive( true );
   if( u[ i ] == 0 )
    ( *y )[ i ].is_negative( true );
  }
 }

 master->add_static_variable( *y , "y" );

 // inner Block of the BendersBFunction
 // f( z ) = min  d'x
 //          s.t. M1 z + b1 <= Ex <= M2 z + b2
 //               f1 <= Fx <= f2

 auto inner_block = new AbstractBlock;

 // Variables

 std::vector< ColVariable > * x = nullptr;
 if( ! d.empty() ) {
  x = new std::vector< ColVariable >( d.size() );

  // bounds on x
  auto bounds_x = new std::vector< BoxConstraint >( l_x.size() );
  for( Index i = 0 ; i < l_x.size() ; ++i ) {
   (*bounds_x)[ i ].set_variable( &(*x)[ i ] );
   (*bounds_x)[ i ].set_lhs( l_x[ i ] );
   (*bounds_x)[ i ].set_rhs( u_x[ i ] );
  }

  // bounds on x
  for( Index i = 0 ; i < l_x.size() ; ++i ) {
   if( l_x[ i ] == - 1 && u_x[ i ] == 1 )
    ( *x )[ i ].is_unitary( true );
   else if( l_x[ i ] == 0 && u_x[ i ] == 1 ) {
    ( *x )[ i ].is_unitary( true );
    ( *x )[ i ].is_positive( true );
   }
   else if( l_x[ i ] == -1 && u_x[ i ] == 0 ) {
    ( *x )[ i ].is_unitary( true );
    ( *x )[ i ].is_negative( true );
   }
   else {
    if( l_x[ i ] == 0 )
     ( *x )[ i ].is_positive( true );
    if( u_x[ i ] == 0 )
     ( *x )[ i ].is_negative( true );
   }
  }


  inner_block->add_static_variable( *x , "x" );
  inner_block->add_static_constraint( *bounds_x , "x" );
 }

 // Objective

 auto objective_function = new LinearFunction();
 for( Index i = 0 ; i < ( *x ).size() ; ++i )
  objective_function->add_variable( &( *x )[ i ] , d[ i ] );
 inner_block->set_objective( new FRealObjective( inner_block ,
                                                 objective_function ) );

 // Constraints in x only
 // f1 <= Fx <= f2

 if( ! f1.empty() ) {
  auto x_constraints = new std::vector< FRowConstraint >( f1.size() );
  for( Index i = 0 ; i < f1.size() ; ++i ) {
   auto function = new LinearFunction();
   for( Index j = 0 ; j < F[ i ].size() ; ++j )
    function->add_variable( &( *x )[ j ] , F[ i ][ j ] );
   ( *x_constraints )[ i ].set_function( function );
   ( *x_constraints )[ i ].set_lhs( f1[ i ] );
   ( *x_constraints )[ i ].set_rhs( f2[ i ] );
  }
  inner_block->add_static_constraint( *x_constraints );
 }

 // Constraints in x and y
 // M1 y + b1 <= Ex <= M2 y + b2

 std::vector< FRowConstraint > * xy_constraints = nullptr;

 if( ! b1.empty() ) {
  xy_constraints = new std::vector< FRowConstraint >( b1.size() );
  for( Index i = 0 ; i < b1.size() ; ++i ) {
   auto function = new LinearFunction();
   for( Index j = 0 ; j < E[ i ].size() ; ++j )
    function->add_variable( &( *x )[ j ] , E[ i ][ j ] );
   ( *xy_constraints )[ i ].set_function( function );
   ( *xy_constraints )[ i ].set_lhs( -inf );
   ( *xy_constraints )[ i ].set_rhs( +inf );
  }
  inner_block->add_static_constraint( *xy_constraints );
 }

 inner_block->register_Solver( inner_block_solver );

 // BendersBFunction

 auto benders_function = new BendersBFunction();

 std::vector< ColVariable * > benders_variables;
 benders_variables.reserve( ( *y ).size() );
 for( auto & y_i : *y )
  benders_variables.push_back( &y_i );

 benders_function->set_inner_block( inner_block );
 benders_function->set_variables( std::move( benders_variables ) );

 if( ! b1.empty() ) {
  for( Index i = 0 ; i < b1.size() ; ++i ) {

   if( ( b1[ i ] == b2[ i ] ) && ( M1[ i ] == M2[ i ] ) &&
       b1[ i ] > -inf ) {
    // equality constraint
    std::vector< double > Ai( M1[ i ] );
    benders_function->add_row( std::move( Ai ) , b1[ i ] ,
                               &(*xy_constraints)[ i ] ,
                               BendersBFunction::eBoth );
   }
   else {

    if( invert_data_mapping_order ) {

     if( b2[ i ] < inf ) {
      std::vector< double > Ai( M2[ i ] );
      benders_function->add_row( std::move( Ai ) , b2[ i ] ,
                                 &(*xy_constraints)[ i ] ,
                                 BendersBFunction::eRHS );
     }
     if( b1[ i ] > -inf ) {
      std::vector< double > Ai( M1[ i ] );
      benders_function->add_row( std::move( Ai ) , b1[ i ] ,
                                 &(*xy_constraints)[ i ] ,
                                 BendersBFunction::eLHS );
     }
    }
    else {
     if( b1[ i ] > -inf ) {
      std::vector< double > Ai( M1[ i ] );
      benders_function->add_row( std::move( Ai ) , b1[ i ] ,
                                 &(*xy_constraints)[ i ] ,
                                 BendersBFunction::eLHS );
     }
     if( b2[ i ] < inf ) {
      std::vector< double > Ai( M2[ i ] );
      benders_function->add_row( std::move( Ai ) , b2[ i ] ,
                                 &(*xy_constraints)[ i ] ,
                                 BendersBFunction::eRHS );
     }
    }
   }
  }
 }

 master->set_objective( new FRealObjective( master , benders_function ) );

 return( master );
}

/*--------------------------------------------------------------------------*/

void test_linearization( Block * benders_block ,
                         const std::vector< std::vector< double > > & y_values ,
                         const std::vector< double > & solution_values ,
                         const std::vector< int > & status ,
                         const double optimal_value ) {

 auto benders_function = dynamic_cast< BendersBFunction * >
  ( dynamic_cast< FRealObjective * >
    ( benders_block->get_objective() )->get_function() );

 assert( benders_function );

 const auto num_y = benders_function->get_num_active_var();

 std::vector< double > optimal_solution( num_y );
 std::vector< ColVariable * > y( num_y );
 for( Index i = 0 ; i < y.size() ; ++i ) {
  y[ i ] = dynamic_cast< ColVariable * >( benders_function->get_active_var( i ) );
  assert( y[ i ] );
  optimal_solution[ i ] = y[ i ]->get_value();
 }

 const auto zero = std::vector< double >( num_y , 0 );

 for( Index i = 0 ; i < y_values.size() ; ++i ) {

  std::vector< double > g( num_y , std::numeric_limits< double >::quiet_NaN() );
  for( Index j = 0 ; j < num_y ; ++j )
   y[ j ]->set_value( y_values[ i ][ j ] );
  assert( benders_function->compute() == status[ i ] );
  assert( benders_function->get_value() == solution_values[ i ] );

  if( status[ i ] == Solver::kOK )
   assert( benders_function->has_linearization() );
  else
   assert( benders_function->has_linearization( false ) );

  auto alpha = benders_function->get_linearization_constant();
  benders_function->get_linearization_coefficients( g.data() );

  if( status[ i ] == Solver::kInfeasible ) {
   double gy = 0;
   for( Index j = 0 ; j < g.size() ; ++j )
    gy += g[ j ] * y[ j ]->get_value();
   assert( alpha + gy > 0 );
  }
  else if( status[ i ] == Solver::kOK ) {
   // test linearization on y
   double gy = 0;
   for( Index j = 0 ; j < g.size() ; ++j )
    gy += g[ j ] * y[ j ]->get_value();
   assert( solution_values[ i ] == alpha + gy );

   // test linearization on the optimal solution
   gy = 0;
   for( Index j = 0 ; j < g.size() ; ++j )
    gy += g[ j ] * optimal_solution[ j ];
   assert( alpha + gy <= optimal_value );
  }
 }
}

/*--------------------------------------------------------------------------*/

Solver * build_inner_block_solver() {
 auto inner_block = new AbstractBlock();

 // Add a *MILPSolver to inner_block
  BlockSolverConfig * lpbsc;
  {
   auto c = Configuration::deserialize( "LPPar_innerBlock.txt" );
   lpbsc = dynamic_cast< BlockSolverConfig * >( c );
   if( ! lpbsc ) {
    std::cerr << "Error: LPPar_innerBlock.txt does not contain a BlockSolverConfig" << std::endl;
    delete( c );
    exit( 1 );
    }
   }

 lpbsc->apply( inner_block );
 lpbsc->clear();

 auto inner_block_solver = (inner_block->get_registered_solvers()).front();
 
 return( inner_block_solver );
}

/*--------------------------------------------------------------------------*/

void test( bool invert ) {
 /*
  * min  x1 + x2 + x3
  * s.t. -1 <= x_i <= 1,      i = 1,2,3
  *       0.5 <= -x1 + x3 <= 1 + y1 + y2
  */

 int num_x = 3;

 std::vector< double > l = { -10 , -10 };
 std::vector< double > u = {  10 ,  10 };

 std::vector< double > d( num_x , 1.0 );

 matrix E = { { -1 , 0 , 1 } };

 matrix M1 = { { 0 , 0 } };
 matrix M2 = { { 1 , 1 } };

 std::vector< double > b1 = { 0.5 } ;
 std::vector< double > b2 = { 1.0 };

 matrix F = { { 1.0 , 0.0 , 0.0 } ,
              { 0.0 , 1.0 , 0.0 } ,
              { 0.0 , 0.0 , 1.0 } };
 std::vector< double > f1 = { -1 , -1 , -1 };
 std::vector< double > f2 = {  1 ,  1 ,  1 };

 std::vector< double > l_x( num_x , -1 );
 std::vector< double > u_x( num_x ,  1 );

 auto lp = build_LP( l , u , d , E , M1, M2 , b1 , b2,
                     F , f1 , f2 , l_x , u_x );

 // Add a *MILPSolver to lp
  BlockSolverConfig * lpbsc;
  {
   auto c = Configuration::deserialize( "LPPar.txt" );
   lpbsc = dynamic_cast< BlockSolverConfig * >( c );
   if( ! lpbsc ) {
    std::cerr << "Error: LPPar.txt does not contain a BlockSolverConfig" << std::endl;
    delete( c );
    exit( 1 );
    }
   }

 lpbsc->apply( lp );
 lpbsc->clear();

 auto solver = ( lp->get_registered_solvers()).front();
 assert( solver->compute() == Solver::kOK );
 assert( solver->has_var_solution() );
 auto optimal_value = solver->get_var_value();
 assert( optimal_value == - 2.5 );
 delete( lp );
 delete( solver );

 auto inner_block_solver = build_inner_block_solver();
 auto benders_block = build_Benders_decomposition
  ( inner_block_solver , invert , l , u , d , E , M1, M2 , b1 , b2, F , f1 , f2 ,
    l_x, u_x );

 // Solve

 auto block_solver_config = build_solver_config( solver_filename );
 block_solver_config->apply( benders_block );
 auto bundle_solver = benders_block->get_registered_solvers().front();
 assert( bundle_solver->compute() == Solver::kOK );
 assert( bundle_solver->has_var_solution() );
 assert( bundle_solver->get_var_value() == optimal_value );
 bundle_solver->get_var_solution();

 // Test linearizations

 std::vector< std::vector< double > > y_values =
  { { 0 , 0 } , { 1 , 0 } , { 0 , 1 } , { -1 , 0.5 } };
 std::vector< double > solution_values( y_values.size() , optimal_value );
 std::vector< int > status( y_values.size() , Solver::kOK );

 test_linearization( benders_block , y_values , solution_values , status ,
                     optimal_value );

 block_solver_config->clear();
 block_solver_config->apply( benders_block );
 delete( block_solver_config );
 delete( benders_block );
 delete( inner_block_solver );
}

/*--------------------------------------------------------------------------*/

void test2( bool invert ) {
 /*
  * min  10*x1 + x5
  * s.t. x0 = 1
  *      x0 - x1 = y1
  *      x2 = 1
  *      x2 = y2
  *      x1 - x4 = 0
  *      -x5 = - y1
  *      x3 - x6 = y2
  *      -x3 + x4 + x5 + x6 = 0
  *      x >= 0
  */

 int num_x = 7;

 std::vector< double > l = {   0 ,   0 };
 std::vector< double > u = { inf , inf };

 std::vector< double > l_x( num_x ,   0 );
 std::vector< double > u_x( num_x , inf );

 std::vector< double > d = { 0 , 10 , 0 , 1 , 0 , 0 , 0 };

 matrix M1 = { { 0 , 0 } , { 1 , 0 } , { 0 , 1 } , { -1 , 0 } , { 0 , 1 } };
 matrix M2 = M1;
 matrix E = { { 1 ,  0 , 0 , 0 , 0 ,  0 ,  0 } ,
              { 1 , -1 , 0 , 0 , 0 ,  0 ,  0 } ,
              { 0 ,  0 , 1 , 0 , 0 ,  0 ,  0 } ,
              { 0 ,  0 , 0 , 0 , 0 , -1 ,  0 } ,
              { 0 ,  0 , 0 , 1 , 0 ,  0 , -1 } };

 std::vector< double > b1 = { 1 , 0 , 0 , 0 , 0 } ;
 std::vector< double > b2 = b1;

 matrix F = { { 0 , 0 , 1 ,  0 ,  0 , 0 , 0 } ,
              { 0 , 1 , 0 ,  0 , -1 , 0 , 0 } ,
              { 0 , 0 , 0 , -1 ,  1 , 1 , 1 } };
 std::vector< double > f1 = { 1 , 0 , 0 };
 std::vector< double > f2 = f1;

 auto lp = build_LP( l , u , d , E , M1 , M2 , b1 , b2 ,
                     F , f1 , f2 , l_x , u_x );

 // Add a *MILPSolver to lp
  BlockSolverConfig * lpbsc;
  {
   auto c = Configuration::deserialize( "LPPar.txt" );
   lpbsc = dynamic_cast< BlockSolverConfig * >( c );
   if( ! lpbsc ) {
    std::cerr << "Error: LPPar.txt does not contain a BlockSolverConfig" << std::endl;
    delete( c );
    exit( 1 );
    }
   }

 lpbsc->apply( lp );
 lpbsc->clear();

 auto solver = ( lp->get_registered_solvers()).front();
 assert( solver->compute() == Solver::kOK );
 assert( solver->has_var_solution() );
 auto optimal_value = solver->get_var_value();
 assert( optimal_value == 1.0 );
 delete( lp );
 delete( solver );

 // Solve

 auto inner_block_solver = build_inner_block_solver();
 auto benders_block = build_Benders_decomposition
  ( inner_block_solver , invert , l , u , d , E , M1 , M2 , b1 , b2 ,
    F , f1 , f2 , l_x , u_x );

 auto block_solver_config = build_solver_config( solver_filename );
 block_solver_config->apply( benders_block );
 block_solver_config->clear();

 auto bundle_solver = benders_block->get_registered_solvers().front();
 assert( bundle_solver->compute() == Solver::kOK );
 assert( bundle_solver->has_var_solution() );
 assert( bundle_solver->get_var_value() == optimal_value );
 bundle_solver->get_var_solution();

 // Test linearizations

 std::vector< std::vector< double > > y_values =
  { { 0 , 0 } , { 1 , 0 } , { 0 , 1 } , { 1 , 1 } };
 std::vector< double > solution_values = { inf , inf , 11 , 1 };
 std::vector< int > status = { Solver::kInfeasible , Solver::kInfeasible ,
  Solver::kOK , Solver::kOK };

 test_linearization( benders_block , y_values , solution_values , status ,
                     optimal_value );

 block_solver_config->clear();
 block_solver_config->apply( benders_block );
 delete( block_solver_config );
 delete( benders_block );
 delete( inner_block_solver );
}

/*--------------------------------------------------------------------------*/

void test3( bool invert ) {

 /*
  * min  x0
  * s.t. x0 = 1
  *      x0 = y0
  *      x >= 0
  */

 int num_x = 1;

 std::vector< double > l = { 0 };
 std::vector< double > u = { inf };

 std::vector< double > l_x( num_x ,   0 );
 std::vector< double > u_x( num_x , inf );

 std::vector< double > d = { 1 };

 matrix M1 = { { 1 } };
 matrix M2 = M1;
 matrix E = { { 1 } };

 std::vector< double > b1 = { 0 } ;
 std::vector< double > b2 = b1;

 matrix F = { { 1 } };
 std::vector< double > f1 = { 1 };
 std::vector< double > f2 = f1;

 auto lp = build_LP( l , u , d , E , M1 , M2 , b1 , b2 ,
                     F , f1 , f2 , l_x , u_x );

 // Add a *MILPSolver to lp
  BlockSolverConfig * lpbsc;
  {
   auto c = Configuration::deserialize( "LPPar.txt" );
   lpbsc = dynamic_cast< BlockSolverConfig * >( c );
   if( ! lpbsc ) {
    std::cerr << "Error: LPPar.txt does not contain a BlockSolverConfig" << std::endl;
    delete( c );
    exit( 1 );
    }
   }

 lpbsc->apply( lp );
 lpbsc->clear();

 auto solver = ( lp->get_registered_solvers()).front();
 assert( solver->compute() == Solver::kOK );
 assert( solver->has_var_solution() );
 auto optimal_value = solver->get_var_value();
 assert( optimal_value == 1.0 );
 delete( lp );
 delete( solver );

 // Solve

 auto inner_block_solver = build_inner_block_solver();
 auto benders_block = build_Benders_decomposition
  ( inner_block_solver , invert , l , u , d , E , M1 , M2 , b1 , b2 ,
    F , f1 , f2 , l_x , u_x );

 auto block_solver_config = build_solver_config( solver_filename );
 block_solver_config->apply( benders_block );
 block_solver_config->clear();

 auto bundle_solver = benders_block->get_registered_solvers().front();
 assert( bundle_solver->compute() == Solver::kOK );
 assert( bundle_solver->has_var_solution() );
 assert( bundle_solver->get_var_value() == optimal_value );
 bundle_solver->get_var_solution();

 // Test linearizations

 std::vector< std::vector< double > > y_values = { { 0 } , { 1 } };
 std::vector< double > solution_values = { inf , 1 };
 std::vector< int > status = { Solver::kInfeasible , Solver::kOK };

 test_linearization( benders_block , y_values , solution_values , status ,
                     optimal_value );

 block_solver_config->clear();
 block_solver_config->apply( benders_block );
 delete( block_solver_config );
 delete( benders_block );
 delete( inner_block_solver );
}

/*--------------------------------------------------------------------------*/

void test4( bool invert ) {

 /*
  * min  x0
  * s.t. x0 = 1
  *      x0 = y0
  *      x >= 0
  *
  * with BendersBFunction handling both constraints
  */

 int num_x = 1;

 std::vector< double > l = { 0 };
 std::vector< double > u = { inf };

 std::vector< double > l_x( num_x ,   0 );
 std::vector< double > u_x( num_x , inf );

 std::vector< double > d = { 1 };

 matrix M1 = { { 0 } , { 1 } };
 matrix M2 = M1;
 matrix E = { { 1 } , { 1 } };

 std::vector< double > b1 = { 1 , 0 } ;
 std::vector< double > b2 = b1;

 matrix F = { };
 std::vector< double > f1 = { };
 std::vector< double > f2 = f1;

 auto lp = build_LP( l , u , d , E , M1 , M2 , b1 , b2 ,
                     F , f1 , f2 , l_x , u_x );

 // Add a *MILPSolver to lp
  BlockSolverConfig * lpbsc;
  {
   auto c = Configuration::deserialize( "LPPar.txt" );
   lpbsc = dynamic_cast< BlockSolverConfig * >( c );
   if( ! lpbsc ) {
    std::cerr << "Error: LPPar.txt does not contain a BlockSolverConfig" << std::endl;
    delete( c );
    exit( 1 );
    }
   }

 lpbsc->apply( lp );
 lpbsc->clear();

 auto solver = ( lp->get_registered_solvers()).front();
 assert( solver->compute() == Solver::kOK );
 assert( solver->has_var_solution() );
 auto optimal_value = solver->get_var_value();
 assert( optimal_value == 1.0 );
 delete( lp );
 delete( solver );

 // Solve

 auto inner_block_solver = build_inner_block_solver();
 auto benders_block = build_Benders_decomposition
  ( inner_block_solver , invert , l , u , d , E , M1 , M2 , b1 , b2 ,
    F , f1 , f2 , l_x , u_x );

 auto block_solver_config = build_solver_config( solver_filename );
 block_solver_config->apply( benders_block );
 block_solver_config->clear();

 auto bundle_solver = benders_block->get_registered_solvers().front();
 assert( bundle_solver->compute() == Solver::kOK );
 assert( bundle_solver->has_var_solution() );
 assert( bundle_solver->get_var_value() == optimal_value );
 bundle_solver->get_var_solution();

 // Test linearizations

 std::vector< std::vector< double > > y_values = { { 0 } , { 1 } };
 std::vector< double > solution_values = { inf , 1 };
 std::vector< int > status = { Solver::kInfeasible , Solver::kOK };

 test_linearization( benders_block , y_values , solution_values , status ,
                     optimal_value );

 block_solver_config->clear();
 block_solver_config->apply( benders_block );
 delete( block_solver_config );
 delete( benders_block );
 delete( inner_block_solver );
}

/*--------------------------------------------------------------------------*/

void test5( bool invert ) {
 /*
  * min  x0 - x1
  * s.t. x0 + x1 <= y + 1
  *      x0 - x1 >= -2y
  *      -1 <= x <= 1
  */

 int num_x = 2;

 std::vector< double > l = { 0 };
 std::vector< double > u = { inf };

 std::vector< double > l_x( num_x , -1 );
 std::vector< double > u_x( num_x ,  1 );

 std::vector< double > d = { 1 , -1 };

 matrix M1 = { { 0 } , { -2 } };
 matrix M2 = { { 1 } , {  0 } };
 matrix E = { {  1 , 1 } , { 1 , -1 } };

 std::vector< double > b1 = { -inf , 0 };
 std::vector< double > b2 = {    1 , inf };

 matrix F = { };
 std::vector< double > f1 = { };
 std::vector< double > f2 = f1;

 auto lp = build_LP( l , u , d , E , M1 , M2 , b1 , b2 ,
                     F , f1 , f2 , l_x , u_x );

 // Add a *MILPSolver to lp
  BlockSolverConfig * lpbsc;
  {
   auto c = Configuration::deserialize( "LPPar.txt" );
   lpbsc = dynamic_cast< BlockSolverConfig * >( c );
   if( ! lpbsc ) {
    std::cerr << "Error: LPPar.txt does not contain a BlockSolverConfig" << std::endl;
    delete( c );
    exit( 1 );
    }
   }

 lpbsc->apply( lp );
 lpbsc->clear();

 auto solver = ( lp->get_registered_solvers()).front();
 assert( solver->compute() == Solver::kOK );
 assert( solver->has_var_solution() );
 auto optimal_value = solver->get_var_value();
 assert( optimal_value == -2.0 );
 delete( lp );
 delete( solver );

 // Solve

 auto inner_block_solver = build_inner_block_solver();
 auto benders_block = build_Benders_decomposition
  ( inner_block_solver , invert , l , u , d , E , M1 , M2 , b1 , b2 ,
    F , f1 , f2 , l_x , u_x );

 auto block_solver_config = build_solver_config( solver_filename );
 block_solver_config->apply( benders_block );
 block_solver_config->clear();

 auto bundle_solver = benders_block->get_registered_solvers().front();
 assert( bundle_solver->compute() == Solver::kOK );
 assert( bundle_solver->has_var_solution() );
 assert( bundle_solver->get_var_value() == optimal_value );
 bundle_solver->get_var_solution();

 // Test linearizations

 std::vector< std::vector< double > > y_values = { { -1 } , { 0 } , { 1 } };
 std::vector< double > solution_values = { 2 , 0 , -2 };
 std::vector< int > status = { Solver::kOK , Solver::kOK , Solver::kOK };

 test_linearization( benders_block , y_values , solution_values , status ,
                     optimal_value );

 block_solver_config->clear();
 block_solver_config->apply( benders_block );
 delete( block_solver_config );
 delete( benders_block );
 delete( inner_block_solver );
}

/*--------------------------------------------------------------------------*/

void test6( bool invert ) {
 /*
  * min  x0 + x1 + x2 - x3
  * s.t.
  *      x1 - x2 - x4 <= 10
  *      x4 - x3 + x0 >= 5
  *      x0 - x2 + x3 = y + 3
  *      0 <= xi      , i = 0, 1, 2
  *      0 <= xi      , i = 3, 4
  *      x3 <= 10
  */

 int num_x = 5;

 std::vector< double > l = { 0 };
 std::vector< double > u = { inf };

 std::vector< double > l_x( num_x , 0 );
 //std::vector< double > u_x = { 1 , 1 , 1 , inf , inf };
 std::vector< double > u_x = { inf , inf , inf , inf , inf };

 std::vector< double > d = { 1 , 1 , 1 , -1 , 0 };

 matrix M1 = { { 1 } };
 matrix M2 = M1;
 matrix E = { {  1 , 0 , -1 , 1, 0 } };

 std::vector< double > b1 = { 3 };
 std::vector< double > b2 = b1;

 matrix F = { { 0 , 1, -1 , 0 , -1 } , { 1 , 0 , 0 , -1 , 1 } , { 0 , 0 , 0 , 1 , 0 } };
 std::vector< double > f1 = { -inf , 5 , -inf };
 std::vector< double > f2 = { 10 , inf , 1 };

 auto lp = build_LP( l , u , d , E , M1 , M2 , b1 , b2 ,
                     F , f1 , f2 , l_x , u_x );

 // Add a *MILPSolver to lp
  BlockSolverConfig * lpbsc;
  {
   auto c = Configuration::deserialize( "LPPar.txt" );
   lpbsc = dynamic_cast< BlockSolverConfig * >( c );
   if( ! lpbsc ) {
    std::cerr << "Error: LPPar.txt does not contain a BlockSolverConfig" << std::endl;
    delete( c );
    exit( 1 );
    }
   }

 lpbsc->apply( lp );
 lpbsc->clear();

 auto solver = ( lp->get_registered_solvers()).front();
 assert( solver->compute() == Solver::kOK );
 assert( solver->has_var_solution() );
 auto optimal_value = solver->get_var_value();
 assert( optimal_value == 1 );
 delete( lp );
 delete( solver );

 // Solve

 auto inner_block_solver = build_inner_block_solver();
 auto benders_block = build_Benders_decomposition
  ( inner_block_solver , invert , l , u , d , E , M1 , M2 , b1 , b2 ,
    F , f1 , f2 , l_x , u_x );

 auto block_solver_config = build_solver_config( solver_filename );
 block_solver_config->apply( benders_block );
 block_solver_config->clear();

 auto bundle_solver = benders_block->get_registered_solvers().front();
 assert( bundle_solver->compute() == Solver::kOK );
 assert( bundle_solver->has_var_solution() );
 assert( bundle_solver->get_var_value() == optimal_value );
 bundle_solver->get_var_solution();

 // Test linearizations

 std::vector< std::vector< double > > y_values = { { 0 } };
 std::vector< double > solution_values = { 1 };
 std::vector< int > status = { Solver::kOK };

 test_linearization( benders_block , y_values , solution_values , status ,
                     optimal_value );

 block_solver_config->clear();
 block_solver_config->apply( benders_block );
 delete( block_solver_config );
 delete( benders_block );
 delete( inner_block_solver );
}

/*--------------------------------------------------------------------------*/

void test7( bool invert ) {
 /*
  * min  x1 - 5x2
  * s.t.
  *      x0 = y
  *      - x1 + 5x2 - x3 <= 3
  *      x3 <= 1
  *      0 <= xi <= 1 , i = 1, 2
  *      0 <= xi      , i = 3, 4
  */

 std::vector< double > l = { 0 };
 std::vector< double > u = { inf };

 std::vector< double > l_x = { -inf , -inf , 0 , 0   };
 std::vector< double > u_x = {  inf ,  inf , 1 , inf };

 std::vector< double > d = { 0 , 1 , -5 , 0 };

 matrix M1 = { { 1 } };
 matrix M2 = M1;
 matrix E = { {  1 , 0 , 0 , 0 } };

 std::vector< double > b1 = { 0 };
 std::vector< double > b2 = b1;

 matrix F = { { 0 , -1 , 5 , -1 } , { 0 , 0 , 0 , 1 } , { 0 , 1 , 0 , 0 } };
 std::vector< double > f1 = { -inf , -inf , 1 };
 std::vector< double > f2 = {    3 ,    1 , inf };

 auto lp = build_LP( l , u , d , E , M1 , M2 , b1 , b2 ,
                     F , f1 , f2 , l_x , u_x );

 // Add a *MILPSolver to lp
  BlockSolverConfig * lpbsc;
  {
   auto c = Configuration::deserialize( "LPPar.txt" );
   lpbsc = dynamic_cast< BlockSolverConfig * >( c );
   if( ! lpbsc ) {
    std::cerr << "Error: LPPar.txt does not contain a BlockSolverConfig" << std::endl;
    delete( c );
    exit( 1 );
    }
   }

 lpbsc->apply( lp );
 lpbsc->clear();

 auto solver = ( lp->get_registered_solvers()).front();
 assert( solver->compute() == Solver::kOK );
 assert( solver->has_var_solution() );
 auto optimal_value = solver->get_var_value();
 assert( optimal_value == -4 );
 delete( lp );
 delete( solver );

 // Solve

 auto inner_block_solver = build_inner_block_solver();
 auto benders_block = build_Benders_decomposition
  ( inner_block_solver , invert , l , u , d , E , M1 , M2 , b1 , b2 ,
    F , f1 , f2 , l_x , u_x );

 auto block_solver_config = build_solver_config( solver_filename );
 block_solver_config->apply( benders_block );
 block_solver_config->clear();

 auto bundle_solver = benders_block->get_registered_solvers().front();
 assert( bundle_solver->compute() == Solver::kOK );
 assert( bundle_solver->has_var_solution() );
 assert( bundle_solver->get_var_value() == optimal_value );
 bundle_solver->get_var_solution();

 // Test linearizations

 std::vector< std::vector< double > > y_values = { { 0 } };
 std::vector< double > solution_values = { optimal_value };
 std::vector< int > status = { Solver::kOK };

 test_linearization( benders_block , y_values , solution_values , status ,
                     optimal_value );

 block_solver_config->clear();
 block_solver_config->apply( benders_block );
 delete( block_solver_config );
 delete( benders_block );
 delete( inner_block_solver );
}

/*--------------------------------------------------------------------------*/

void test8( bool invert ) {
 /*
  * min  x
  * s.t.
  *      1 <= x <= y + 0
  */

 std::vector< double > l = { 0 };
 std::vector< double > u = { inf };

 std::vector< double > l_x = { -inf };
 std::vector< double > u_x = {  inf };

 std::vector< double > d = { 1 };

 matrix M1 = { { 0 } };
 matrix M2 = { { 1 } };
 matrix E  = { { 1 } };

 std::vector< double > b1 = { 1 };
 std::vector< double > b2 = { 0 };

 matrix F = { };
 std::vector< double > f1 = { };
 std::vector< double > f2 = f1;

 auto lp = build_LP( l , u , d , E , M1 , M2 , b1 , b2 ,
                     F , f1 , f2 , l_x , u_x );

 // Add a *MILPSolver to lp
  BlockSolverConfig * lpbsc;
  {
   auto c = Configuration::deserialize( "LPPar.txt" );
   lpbsc = dynamic_cast< BlockSolverConfig * >( c );
   if( ! lpbsc ) {
    std::cerr << "Error: LPPar.txt does not contain a BlockSolverConfig" << std::endl;
    delete( c );
    exit( 1 );
    }
   }

 lpbsc->apply( lp );
 lpbsc->clear();

 auto solver = ( lp->get_registered_solvers()).front();
 assert( solver->compute() == Solver::kOK );
 assert( solver->has_var_solution() );
 auto optimal_value = solver->get_var_value();
 assert( optimal_value == 1 );
 delete( lp );
 delete( solver );

 // Solve

 auto inner_block_solver = build_inner_block_solver();
 auto benders_block = build_Benders_decomposition
  ( inner_block_solver , invert , l , u , d , E , M1 , M2 , b1 , b2 ,
    F , f1 , f2 , l_x , u_x );

 auto block_solver_config = build_solver_config( solver_filename );
 block_solver_config->apply( benders_block );
 block_solver_config->clear();

 /* // TODO Uncomment when *MILPSolver is ready to deal with l > u bounds
 auto bundle_solver = benders_block->get_registered_solvers().front();
 assert( bundle_solver->compute() == Solver::kOK );
 assert( bundle_solver->has_var_solution() );
 assert( bundle_solver->get_var_value() == optimal_value );
 bundle_solver->get_var_solution();

 block_solver_config->clear();
 block_solver_config->apply( benders_block );
 delete( block_solver_config );
 */

 // Test linearizations

 auto benders_function = dynamic_cast< BendersBFunction * >
  ( dynamic_cast< FRealObjective * >
    ( benders_block->get_objective() )->get_function() );

 assert( benders_function );

 auto y = dynamic_cast< ColVariable * >( benders_function->get_active_var( 0 ) );
 y->set_value( 1 );

 std::vector< std::vector< double > > y_values = { { 1 } };
 std::vector< double > solution_values = { optimal_value };
 std::vector< int > status = { Solver::kOK };

 test_linearization( benders_block , y_values , solution_values , status ,
                     optimal_value );

 delete( benders_block );
 delete( inner_block_solver );
}

/*--------------------------------------------------------------------------*/

void test9( bool invert ) {
 /*
  * min  x
  * s.t.
  *      1 <= x <= -y + 2
  */

 std::vector< double > l = { 0 };
 std::vector< double > u = { inf };

 std::vector< double > l_x = { -inf };
 std::vector< double > u_x = {  inf };

 std::vector< double > d = { 1 };

 matrix M1 = { { 0 } };
 matrix M2 = { { -1 } };
 matrix E  = { { 1 } };

 std::vector< double > b1 = { 1 };
 std::vector< double > b2 = { 2 };

 matrix F = { };
 std::vector< double > f1 = { };
 std::vector< double > f2 = f1;

 auto lp = build_LP( l , u , d , E , M1 , M2 , b1 , b2 ,
                     F , f1 , f2 , l_x , u_x );

 // Add a *MILPSolver to lp
  BlockSolverConfig * lpbsc;
  {
   auto c = Configuration::deserialize( "LPPar.txt" );
   lpbsc = dynamic_cast< BlockSolverConfig * >( c );
   if( ! lpbsc ) {
    std::cerr << "Error: LPPar.txt does not contain a BlockSolverConfig" << std::endl;
    delete( c );
    exit( 1 );
    }
   }

 lpbsc->apply( lp );
 lpbsc->clear();

 auto solver = ( lp->get_registered_solvers()).front();
 assert( solver->compute() == Solver::kOK );
 assert( solver->has_var_solution() );
 auto optimal_value = solver->get_var_value();
 assert( optimal_value == 1 );
 delete( lp );
 delete( solver );

 // Solve

 auto inner_block_solver = build_inner_block_solver();
 auto benders_block = build_Benders_decomposition
  ( inner_block_solver , invert , l , u , d , E , M1 , M2 , b1 , b2 ,
    F , f1 , f2 , l_x , u_x );

 auto block_solver_config = build_solver_config( solver_filename );
 block_solver_config->apply( benders_block );
 block_solver_config->clear();

 /* // TODO Uncomment when *MILPSolver is ready to deal with l > u bounds
 auto bundle_solver = benders_block->get_registered_solvers().front();
 assert( bundle_solver->compute() == Solver::kOK );
 assert( bundle_solver->has_var_solution() );
 assert( bundle_solver->get_var_value() == optimal_value );
 bundle_solver->get_var_solution();

 block_solver_config->clear();
 block_solver_config->apply( benders_block );
 delete( block_solver_config );
 */

 // Test linearizations

 auto benders_function = dynamic_cast< BendersBFunction * >
  ( dynamic_cast< FRealObjective * >
    ( benders_block->get_objective() )->get_function() );

 assert( benders_function );

 auto y = dynamic_cast< ColVariable * >( benders_function->get_active_var( 0 ) );
 y->set_value( 1 );

 std::vector< std::vector< double > > y_values = { { 1 } };
 std::vector< double > solution_values = { optimal_value };
 std::vector< int > status = { Solver::kOK };

 test_linearization( benders_block , y_values , solution_values , status ,
                     optimal_value );

 delete( benders_block );
 delete( inner_block_solver );
}

/*--------------------------------------------------------------------------*/

void test10( bool invert ) {
 /*
  * min  x
  * s.t.
  *    2y - 1 <= x <= -y + 2
  *     y >= 0
  */

 std::vector< double > l = { 0 };
 std::vector< double > u = { inf };

 std::vector< double > l_x = { -inf };
 std::vector< double > u_x = {  inf };

 std::vector< double > d = { 1 };

 matrix M1 = { {  2 } };
 matrix M2 = { { -1 } };
 matrix E  = { { 1 } };

 std::vector< double > b1 = { -1 };
 std::vector< double > b2 = { 2 };

 matrix F = { };
 std::vector< double > f1 = { };
 std::vector< double > f2 = f1;

 auto lp = build_LP( l , u , d , E , M1 , M2 , b1 , b2 ,
                     F , f1 , f2 , l_x , u_x );

 // Add a *MILPSolver to lp
  BlockSolverConfig * lpbsc;
  {
   auto c = Configuration::deserialize( "LPPar.txt" );
   lpbsc = dynamic_cast< BlockSolverConfig * >( c );
   if( ! lpbsc ) {
    std::cerr << "Error: LPPar.txt does not contain a BlockSolverConfig" << std::endl;
    delete( c );
    exit( 1 );
    }
   }

 lpbsc->apply( lp );
 lpbsc->clear();

 auto solver = ( lp->get_registered_solvers()).front();
 assert( solver->compute() == Solver::kOK );
 assert( solver->has_var_solution() );
 auto optimal_value = solver->get_var_value();
 assert( optimal_value == -1 );
 delete( lp );
 delete( solver );

 // Solve

 auto inner_block_solver = build_inner_block_solver();
 auto benders_block = build_Benders_decomposition
  ( inner_block_solver , invert , l , u , d , E , M1 , M2 , b1 , b2 ,
    F , f1 , f2 , l_x , u_x );

 auto block_solver_config = build_solver_config( solver_filename );
 block_solver_config->apply( benders_block );
 block_solver_config->clear();

 /* // TODO Uncomment when *MILPSolver is ready to deal with l > u bounds
 auto bundle_solver = benders_block->get_registered_solvers().front();
 assert( bundle_solver->compute() == Solver::kOK );
 assert( bundle_solver->has_var_solution() );
 assert( bundle_solver->get_var_value() == optimal_value );
 bundle_solver->get_var_solution();

 block_solver_config->clear();
 block_solver_config->apply( benders_block );
 delete( block_solver_config );
 */

 // Test linearizations

 auto benders_function = dynamic_cast< BendersBFunction * >
  ( dynamic_cast< FRealObjective * >
    ( benders_block->get_objective() )->get_function() );

 assert( benders_function );

 auto y = dynamic_cast< ColVariable * >( benders_function->get_active_var( 0 ) );
 y->set_value( 1 );

 std::vector< std::vector< double > > y_values = { { 1 } };
 std::vector< double > solution_values = { 1 };
 std::vector< int > status = { Solver::kOK };

 test_linearization( benders_block , y_values , solution_values , status ,
                     1 );

 delete( benders_block );
 delete( inner_block_solver );
}

/*--------------------------------------------------------------------------*/

void test11( bool invert ) {
 /*
  * min  x
  * s.t.
  *   -y + 2 <= x <= 2y - 1
  *    y >= 0
  */

 std::vector< double > l = { 0 };
 std::vector< double > u = { inf };

 std::vector< double > l_x = { -inf };
 std::vector< double > u_x = {  inf };

 std::vector< double > d = { 1 };

 matrix M1 = { { -1 } };
 matrix M2 = { {  2 } };
 matrix E  = { {  1 } };

 std::vector< double > b1 = {  2 };
 std::vector< double > b2 = { -1 };

 matrix F = { };
 std::vector< double > f1 = { };
 std::vector< double > f2 = f1;

 auto inner_block_solver = build_inner_block_solver();
 auto benders_block = build_Benders_decomposition
  ( inner_block_solver , invert , l , u , d , E , M1 , M2 , b1 , b2 ,
    F , f1 , f2 , l_x , u_x );

 // Test linearizations

 auto benders_function = dynamic_cast< BendersBFunction * >
  ( dynamic_cast< FRealObjective * >
    ( benders_block->get_objective() )->get_function() );

 assert( benders_function );

 auto y = dynamic_cast< ColVariable * >( benders_function->get_active_var( 0 ) );
 y->set_value( 1 );

 std::vector< std::vector< double > > y_values = { { 1 } };
 std::vector< double > solution_values = { 1 };
 std::vector< int > status = { Solver::kOK };

 test_linearization( benders_block , y_values , solution_values , status , 1 );

 delete( benders_block );
 delete( inner_block_solver );
}

/*--------------------------------------------------------------------------*/

void run( bool invert ) {
 test( invert );
 test2( invert );
 test3( invert );
 test4( invert );
 test5( invert );
 test6( invert );
 test7( invert );
 test8( invert );
 test9( invert );
 test10( invert );
 test11( invert );
}

/*--------------------------------------------------------------------------*/

int main( int argc, char ** argv ) {
 if( argc < 2 ) {
  std::cerr << "The path to the file containing the description of a "
   "BlockSolverConfig of the Solver for the master problem must be provided "
   "as argument." << std::endl;
  std::cerr << "Usage: " << argv[ 0 ] << " PATH" << std::endl;
  return( 1 );
 }

 solver_filename = argv[ 1 ];

 run( false );
 run( true );
 return( 0 );
}

/*--------------------------------------------------------------------------*/
/*--------------------------- End File test2.cpp ----------------------------*/
/*--------------------------------------------------------------------------*/
