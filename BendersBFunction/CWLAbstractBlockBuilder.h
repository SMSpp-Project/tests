/*--------------------------------------------------------------------------*/
/*---------------------- File CWLAbstractBlockBuilder -------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Reads an instance of the Capacitated Warehouse Location problem and
 * produces an AbstractBlock associated with that instance.
 *
 * \author Rafael Durbano Lobato \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 * 
 * \author Enrico Calandrini \n
 *         Dipartimento di Matematica \n
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

#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>

/*--------------------------------------------------------------------------*/
/*--------------------------- NAMESPACE ------------------------------------*/
/*--------------------------------------------------------------------------*/

// namespace for the tests of the Structured Modeling System++ (SMS++)
namespace SMSpp_di_unipi_it { namespace tests {

/*--------------------------------------------------------------------------*/
/*--------------------------------- TYPES ----------------------------------*/
/*--------------------------------------------------------------------------*/

enum DecompositionType { eNone , eCustomer , eLocation };

/*--------------------------------------------------------------------------*/
/*-------------------------------- CLASSES ---------------------------------*/
/*--------------------------------------------------------------------------*/

struct CWLInstance {

 void set( int num_locations , int num_customers ) {
  this->num_locations = num_locations;
  this->num_customers = num_customers;
  capacity.resize( num_locations );
  fixed_cost.resize( num_locations );
  demand.resize( num_customers );
  cost.resize( num_locations );
  for( int i = 0 ; i < num_locations ; ++i ) {
   cost[ i ].resize( num_customers );
  }
 }

 int num_locations, num_customers;
 std::vector< double > capacity;
 std::vector< double > fixed_cost;
 std::vector< double > demand;
 std::vector< std::vector< double > > cost;
};

/*--------------------------------------------------------------------------*/
/*------------------------------- FUNCTIONS --------------------------------*/
/*--------------------------------------------------------------------------*/

CWLInstance read_cwl_instance( std::filesystem::path file_path ) {
 std::ifstream stream( file_path );
 if( ! stream.is_open() )
  throw std::invalid_argument
   ( "File " + file_path.string() + " could not be opened." );

 CWLInstance instance;
 int num_locations , num_customers;

 stream >> num_locations >> num_customers;

 if( stream.fail() || stream.bad() )
  throw std::runtime_error( "Error while reading file " + file_path.string() );

 instance.set( num_locations , num_customers );

 for( int i = 0 ; i < num_locations ; ++i ) {
  stream >> instance.capacity[ i ] >> instance.fixed_cost[ i ];
  if( stream.fail() || stream.bad() )
   throw std::runtime_error( "Error while reading file " + file_path.string() );
 }

 for( int j = 0 ; j < num_customers ; ++j ) {
  stream >> instance.demand[ j ];
  if( stream.fail() || stream.bad() )
   throw std::runtime_error( "Error while reading file " + file_path.string() );
  for( int i = 0 ; i < num_locations ; ++i ) {
   stream >> instance.cost[ i ][ j ];
   if( stream.fail() || stream.bad() )
    throw std::runtime_error( "Error while reading file " +
                              file_path.string() );
  }
 }

 return( instance );
}

/*--------------------------------------------------------------------------*/

AbstractBlock * build_CWL_block( std::filesystem::path file_path ,
                                 bool continuous_relaxation = false ) {

 auto instance = read_cwl_instance( file_path );

 auto block = new AbstractBlock();

 // Variables

 auto y = new std::vector< ColVariable >( instance.num_locations );
 for( auto & y_i : * y ) {
  y_i.is_unitary( true );
  y_i.is_positive( true );
  if( ! continuous_relaxation )
   y_i.is_integer( true );
 }

 block->add_static_variable( * y );

 using array_type = typename boost::multi_array< ColVariable , 2 >;
 boost::array< typename array_type::index , 2 > shape =
  { instance.num_locations , instance.num_customers };
 auto x = new array_type( shape );

 auto p_x = x->data();
 for( array_type::size_type k = 0 ; k < x->num_elements() ; ++k , ++p_x )
  p_x->is_positive( true );

 block->add_static_variable( * x );

 // Constraints

 {
  auto demand_fulfillment =
   new std::vector< FRowConstraint >( instance.num_customers );
  for( int j = 0 ; j < instance.num_customers ; ++j ) {
   auto function = new LinearFunction();
   for( int i = 0 ; i < instance.num_locations ; ++i ) {
    function->add_variable( & ( * x )[ i ][ j ] , 1 );
   }
   ( * demand_fulfillment )[ j ].set_function( function );
   ( * demand_fulfillment )[ j ].set_both( 1 );
  }
  block->add_static_constraint( * demand_fulfillment );
 }

 {
  auto capacity_constraints =
   new std::vector< FRowConstraint >( instance.num_locations );
  for( int i = 0 ; i < instance.num_locations ; ++i ) {
   auto function = new LinearFunction();
   for( int j = 0 ; j < instance.num_customers ; ++j ) {
    function->add_variable( & ( * x )[ i ][ j ] , instance.demand[ j ] );
   }
   function->add_variable( & ( * y )[ i ] , - instance.capacity[ i ] );
   ( * capacity_constraints )[ i ].set_function( function );
   ( * capacity_constraints )[ i ].set_rhs( 0 );
  }
  block->add_static_constraint( * capacity_constraints );
 }

 // Objective function

 {
  auto function = new LinearFunction();
  for( int i = 0 ; i < instance.num_locations ; ++i ) {
   function->add_variable( & ( * y )[ i ] , instance.fixed_cost[ i ] );
   for( int j = 0 ; j < instance.num_customers ; ++j ) {
    function->add_variable( & ( * x )[ i ][ j ] , instance.cost[ i ][ j ] );
   }
  }
  auto objective = new FRealObjective( block , function );
  objective->set_sense( Objective::eMin );
  block->set_objective( objective );
 }

 return( block );
}


/*--------------------------------------------------------------------------*/

AbstractBlock * build_customer_Block( const CWLInstance & instance , int j ) {

 auto block = new AbstractBlock();

 // Variables

 auto x = new std::vector< ColVariable >( instance.num_locations );
 for( auto & x_i : * x )
  x_i.is_positive( true );

 block->add_static_variable( * x );

 // Constraint

 {
  auto demand_fulfillment = new FRowConstraint();
  auto function = new LinearFunction();
  for( int i = 0 ; i < instance.num_locations ; ++i ) {
   function->add_variable( & ( * x )[ i ] , 1 );
  }
  demand_fulfillment->set_function( function );
  demand_fulfillment->set_both( 1 );
  block->add_static_constraint( * demand_fulfillment );
 }

 // Objective function

 {
  auto function = new LinearFunction();
  for( int i = 0 ; i < instance.num_locations ; ++i )
   function->add_variable( & ( * x )[ i ] , instance.cost[ i ][ j ] );
  auto objective = new FRealObjective( block , function );
  objective->set_sense( Objective::eMin );
  block->set_objective( objective );
 }

 return( block );

}

/*--------------------------------------------------------------------------*/

BendersBFunction * build_decomposition_by_customer
( const CWLInstance & instance , std::vector< ColVariable * > && y ) {

 auto block = new AbstractBlock();

 auto & nested_Blocks = block->access_nested_Blocks();
 nested_Blocks.reserve( instance.num_customers );
 for( int j = 0 ; j < instance.num_customers ; ++j ) {
  auto customer_block = build_customer_Block( instance , j );

  // Add a *MILPSolver to customer_block
  BlockSolverConfig * lpbsc;
  {
   auto c = Configuration::deserialize( "LPPar_AbstractBlock.txt" );
   lpbsc = dynamic_cast< BlockSolverConfig * >( c );
   if( ! lpbsc ) {
    std::cerr << "Error: LPPar_AbstractBlock.txt does not contain a BlockSolverConfig" << std::endl;
    delete( c );
    exit( 1 );
    }
   }

 lpbsc->apply( customer_block );
 lpbsc->clear();

 nested_Blocks.push_back( customer_block );
 }

 // Constraints

 auto capacity_constraints =
  new std::vector< FRowConstraint >( instance.num_locations );
 {
  for( int i = 0 ; i < instance.num_locations ; ++i ) {
   auto function = new LinearFunction();
   for( int j = 0 ; j < instance.num_customers ; ++j ) {
    auto & x = * nested_Blocks[ j ]->get_static_variable_v< ColVariable >( 0 );
    function->add_variable( & x[ i ] , instance.demand[ j ] );
   }
   ( * capacity_constraints )[ i ].set_function( function );
   ( * capacity_constraints )[ i ].set_rhs( 0 );
  }
  block->add_static_constraint( * capacity_constraints );
 }

 // BendersBFunction

 auto benders_function = new BendersBFunction();

 benders_function->set_inner_block( block );
 benders_function->set_variables( std::move( y ) );

 {
  for( int i = 0 ; i < instance.num_locations ; ++i ) {
   std::vector< double > Ai( instance.num_locations , 0 );
   Ai[ i ] = instance.capacity[ i ];
   benders_function->add_row( std::move( Ai ) , 0 ,
                              & ( * capacity_constraints )[ i ] ,
                              BendersBFunction::eRHS );
  }
 }

 return( benders_function );
}

/*--------------------------------------------------------------------------*/

AbstractBlock * build_location_Block( const CWLInstance & instance , int i ) {

 auto block = new AbstractBlock();

 // Variables

 auto x = new std::vector< ColVariable >( instance.num_customers );
 for( auto & x_j : * x )
  x_j.is_positive( true );

 block->add_static_variable( * x );

 // Constraint

 {
  auto capacity_constraint = new FRowConstraint();
  auto function = new LinearFunction();
  for( int j = 0 ; j < instance.num_customers ; ++j ) {
   function->add_variable( & ( * x )[ j ] , instance.demand[ j ] );
  }
  capacity_constraint->set_function( function );
  capacity_constraint->set_rhs( 0 );
  block->add_static_constraint( * capacity_constraint );
 }

 // Objective function

 {
  auto function = new LinearFunction();
  for( int j = 0 ; j < instance.num_customers ; ++j )
   function->add_variable( & ( * x )[ j ] , instance.cost[ i ][ j ] );
  auto objective = new FRealObjective( block , function );
  objective->set_sense( Objective::eMin );
  block->set_objective( objective );
 }

 return( block );

}

/*--------------------------------------------------------------------------*/

BendersBFunction * build_decomposition_by_location
( const CWLInstance & instance , std::vector< ColVariable * > && y ) {

 auto block = new AbstractBlock();

 auto & nested_Blocks = block->access_nested_Blocks();
 nested_Blocks.reserve( instance.num_locations );
 for( int i = 0 ; i < instance.num_locations ; ++i ) {
  auto location_block = build_location_Block( instance , i );

  // Add a *MILPSolver to location_block
  BlockSolverConfig * lpbsc;
  {
   auto c = Configuration::deserialize( "LPPar_AbstractBlock.txt" );
   lpbsc = dynamic_cast< BlockSolverConfig * >( c );
   if( ! lpbsc ) {
    std::cerr << "Error: LPPar_AbstractBlock.txt does not contain a BlockSolverConfig" << std::endl;
    delete( c );
    exit( 1 );
    }
   }

  lpbsc->apply( location_block );
  lpbsc->clear();
  nested_Blocks.push_back( location_block );
 }

 // Constraints

 {
  auto demand_fulfillment =
   new std::vector< FRowConstraint >( instance.num_customers );
  for( int j = 0 ; j < instance.num_customers ; ++j ) {
   auto function = new LinearFunction();
   auto & x = * nested_Blocks[ j ]->get_static_variable_v< ColVariable >( 0 );
   for( int i = 0 ; i < instance.num_locations ; ++i ) {
    function->add_variable( & x[ i ] , 1 );
   }
   ( * demand_fulfillment )[ j ].set_function( function );
   ( * demand_fulfillment )[ j ].set_both( 1 );
  }
  block->add_static_constraint( * demand_fulfillment );
 }

 // BendersBFunction

 auto benders_function = new BendersBFunction();

 benders_function->set_inner_block( block );
 benders_function->set_variables( std::move( y ) );

 {
  for( int i = 0 ; i < instance.num_locations ; ++i ) {
   auto capacity_constraint =
    nested_Blocks[ i ]->get_static_constraint< FRowConstraint >( 0 );
   std::vector< double > Ai( instance.num_locations , 0 );
   Ai[ i ] = instance.capacity[ i ];
   benders_function->add_row( std::move( Ai ) , 0 , capacity_constraint ,
                              BendersBFunction::eRHS );
  }
 }

 return( benders_function );
}

/*--------------------------------------------------------------------------*/

BendersBFunction * build_Benders_function( const CWLInstance & instance ,
                                           std::vector< ColVariable * > && y ,
                                           Solver * solver ) {

 const bool use_capacity_slack = false;
 const double capacity_slack_cost = 1.0e+5;
 ColVariable * capacity_slack;

 auto block = new AbstractBlock();

 // Variables

 using array_type = typename boost::multi_array< ColVariable , 2 >;
 boost::array< typename array_type::index , 2 > shape =
  { instance.num_locations , instance.num_customers };
 auto x = new array_type( shape );

 auto p_x = x->data();
 for( array_type::size_type k = 0 ; k < x->num_elements() ; ++k , ++p_x )
  p_x->is_positive( true );

 block->add_static_variable( * x );

 if( use_capacity_slack ) {
  capacity_slack = new ColVariable();
  capacity_slack->is_positive( true );
  block->add_static_variable( * capacity_slack );
 }

 // Constraints
 {
  auto demand_fulfillment =
   new std::vector< FRowConstraint >( instance.num_customers );
  for( int j = 0 ; j < instance.num_customers ; ++j ) {
   auto function = new LinearFunction();
   for( int i = 0 ; i < instance.num_locations ; ++i ) {
    function->add_variable( & ( * x )[ i ][ j ] , 1 );
   }
   ( * demand_fulfillment )[ j ].set_function( function );
   ( * demand_fulfillment )[ j ].set_both( 1 );
  }
  block->add_static_constraint( * demand_fulfillment );
 }

 auto capacity_constraints =
  new std::vector< FRowConstraint >( instance.num_locations );
 {
  for( int i = 0 ; i < instance.num_locations ; ++i ) {
   auto function = new LinearFunction();
   for( int j = 0 ; j < instance.num_customers ; ++j ) {
    function->add_variable( & ( * x )[ i ][ j ] , instance.demand[ j ] );
   }
   if( use_capacity_slack )
    function->add_variable( capacity_slack , - 1.0 );
   ( * capacity_constraints )[ i ].set_function( function );
   ( * capacity_constraints )[ i ].set_lhs( -Inf< double >() );
   ( * capacity_constraints )[ i ].set_rhs( 0 );
  }
  block->add_static_constraint( * capacity_constraints );
 }

 // Objective function

 {
  auto function = new LinearFunction();
  if( use_capacity_slack )
   function->add_variable( capacity_slack , capacity_slack_cost );
  for( int i = 0 ; i < instance.num_locations ; ++i )
   for( int j = 0 ; j < instance.num_customers ; ++j )
    function->add_variable( & ( * x )[ i ][ j ] , instance.cost[ i ][ j ] );
  auto objective = new FRealObjective( block , function );
  objective->set_sense( Objective::eMin );
  block->set_objective( objective );
 }

 block->register_Solver( solver );

 // BendersBFunction

 auto benders_function = new BendersBFunction();

 benders_function->set_inner_block( block );
 benders_function->set_variables( std::move( y ) );

 {
  for( int i = 0 ; i < instance.num_locations ; ++i ) {
   std::vector< double > Ai( instance.num_locations , 0 );
   Ai[ i ] = instance.capacity[ i ];
   benders_function->add_row( std::move( Ai ) , 0 ,
                              & ( * capacity_constraints )[ i ] ,
                              BendersBFunction::eRHS );
  }
 }

 return( benders_function );
}

/*--------------------------------------------------------------------------*/

BendersBFunction * build_Benders_function
( const CWLInstance & instance , std::vector< ColVariable * > && y ,
  Solver * solver , DecompositionType decomposition_type ) {

 if( decomposition_type == eNone ) {
  return( build_Benders_function( instance , std::move( y ) , solver ) );
 }
 else if( decomposition_type == eCustomer ) {
  return( build_decomposition_by_customer( instance , std::move( y ) ) );
 }
 else if( decomposition_type == eLocation ) {
  return( build_decomposition_by_location( instance , std::move( y ) ) );
 }
 else {
  throw( std::invalid_argument( "build_Benders_function: invalid decomposition "
                                "type: " + std::to_string( decomposition_type) ) );
 }
}

/*--------------------------------------------------------------------------*/

AbstractBlock * build_Benders_master_block
( const CWLInstance & instance , bool continuous_relaxation ,
  std::vector< ColVariable * > & p_y ) {

 auto block = new AbstractBlock();

 // Variables

 p_y.clear();
 p_y.reserve( instance.num_locations );
 auto y = new std::vector< ColVariable >( instance.num_locations );
 for( auto & y_i : * y ) {
  y_i.is_unitary( true );
  y_i.is_positive( true );
  p_y.push_back( & y_i );
  if( ! continuous_relaxation )
   y_i.is_integer( true );
 }

 block->add_static_variable( * y );

 // Objective function

 {
  auto function = new LinearFunction();
  for( int i = 0 ; i < instance.num_locations ; ++i )
   function->add_variable( & ( * y )[ i ] , instance.fixed_cost[ i ] );
  auto objective = new FRealObjective( block , function );
  objective->set_sense( Objective::eMin );
  block->set_objective( objective );
 }

 return( block );
}

/*--------------------------------------------------------------------------*/

AbstractBlock * build_CWL_block_with_Benders_decomposition
( std::filesystem::path file_path , bool continuous_relaxation = false ,
  Solver * inner_block_solver = nullptr) {

 auto instance = read_cwl_instance( file_path );
 std::vector< ColVariable * > y;
 auto master_block = build_Benders_master_block( instance ,
                                                 continuous_relaxation , y );
 auto benders_function = build_Benders_function( instance , std::move( y ) ,
                                                 inner_block_solver );
 auto & nested_blocks = master_block->access_nested_Blocks();
 assert( nested_blocks.empty() );
 nested_blocks.push_back( new AbstractBlock( master_block ) );
 auto objective = new FRealObjective( nested_blocks[ 0 ] , benders_function );
 objective->set_sense( Objective::eMin );
 nested_blocks[ 0 ]->set_objective( objective );

 return( master_block );
}

} }   // end( namespace SMSpp_di_unipi_it )

/*--------------------------------------------------------------------------*/
/*------------------- End File CWLAbstractBlockBuilder.h -------------------*/
/*--------------------------------------------------------------------------*/
