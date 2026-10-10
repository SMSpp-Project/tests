/*--------------------------------------------------------------------------*/
/*------------------------ File BlockCopySolver.cpp ------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Implementation of the class BlockCopySolver [see BlockCopySolver.h].
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

#include "BlockCopySolver.h"

#include "ColVariable.h"

#include "ThermalUnitBlock.h"

#include "common_utils.h"

#include <netcdf>

#include <map>

#include <vector>

/*--------------------------------------------------------------------------*/
/*-------------------------- NAMESPACE AND USING ---------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*-------------------------------- FACTORY ---------------------------------*/
/*--------------------------------------------------------------------------*/

SMSpp_insert_in_factory_cpp_0( BlockCopySolver );

/*--------------------------------------------------------------------------*/
/*------------------------------- FUNCTIONS --------------------------------*/
/*--------------------------------------------------------------------------*/

namespace {

const std::array< std::string , 2 > PAR_NAMES = { "strBlockCfg" ,
                                                  "strInnerBSC" };

const std::string EMPTY;

/// the ColVariable of each static group of b, by the name of the group
std::map< std::string , std::vector< ColVariable * > > columns( Block * b )
{
 std::map< std::string , std::vector< ColVariable * > > cols;
 for( const auto & g : b->get_static_variable_groups() ) {
  if( ! g )
   continue;
  auto & v = cols[ g->get_name() ];
  g->for_each( [ & ]( Variable & x ) {
   v.push_back( dynamic_cast< ColVariable * >( & x ) );
   } );
  }
 return( cols );
 }

}  // end( namespace )

/*--------------------------------------------------------------------------*/
/*--------------------- METHODS OF BlockCopySolver -------------------------*/
/*--------------------------------------------------------------------------*/

BlockCopySolver::~BlockCopySolver()
{
 clear_copy();
 }

/*--------------------------------------------------------------------------*/

void BlockCopySolver::clear_copy( void )
{
 if( f_bsc && f_copy ) {
  s_config_Block( f_copy , f_bsc , f_files[ 1 ] , false );
  }
 delete f_bsc;
 f_bsc = nullptr;
 f_inner = nullptr;
 delete f_copy;
 f_copy = nullptr;
 }

/*--------------------------------------------------------------------------*/

int BlockCopySolver::compute( bool changedvars )
{
 static const std::string fn = "BlockCopySolver::compute";

 clear_copy();
 f_status = kError;
 if( ! f_Block )
  throw( std::logic_error( fn + ": no Block" ) );
 if( f_files[ 1 ].empty() )
  throw( std::logic_error( fn + ": strInnerBSC not given" ) );

 // the copy: the Block written in a netCDF group in memory and read back
 int ncid;
 static unsigned int count = 0;
 const std::string name = "BlockCopySolver_" + std::to_string( count++ ) +
                          ".nc4";
 if( nc_create( name.c_str() , NC_DISKLESS | NC_NETCDF4 | NC_CLOBBER ,
                & ncid ) != NC_NOERR )
  throw( std::runtime_error( fn + ": cannot create a netCDF dataset in "
                             "memory" ) );
 try {
  auto g = netCDF::NcGroup( ncid ).addGroup( "Block" );
  f_Block->serialize( g );
  f_copy = Block::new_Block( g );
  }
 catch( ... ) {
  nc_close( ncid );
  throw;
  }
 nc_close( ncid );
 if( ! f_copy )
  throw( std::logic_error( fn + ": the copy of the Block is not read" ) );

 // what a UnitBlock is asked by its father, which the data do not say
 if( auto ub = dynamic_cast< UnitBlock * >( f_Block ) ) {
  auto cb = static_cast< UnitBlock * >( f_copy );
  cb->set_reserve_vars( ( ub->has_primary_reserve() ? 1 : 0 ) |
                        ( ub->has_secondary_reserve() ? 2 : 0 ) );
  cb->set_reactive_power( ub->has_reactive_power() );
  }

 // the formulation of the copy, and its abstract representation
 if( ! f_files[ 0 ].empty() ) {
  auto bc = Configuration::deserialize( f_files[ 0 ] );
  if( ! bc )
   throw( std::logic_error( fn + ": " + f_files[ 0 ] + " cannot be read" ) );
  b_config_Block( f_copy , bc , f_files[ 0 ] );
  delete bc;
  }
 f_copy->generate_abstract_variables();
 f_copy->generate_abstract_constraints();
 f_copy->generate_objective();

 // the fixed Variable of the Block, in the groups of the same name and size
 auto from = columns( f_Block );
 auto to = columns( f_copy );
 for( auto & [ nm , v ] : from ) {
  auto it = to.find( nm );
  if( ( it == to.end() ) || ( it->second.size() != v.size() ) )
   continue;
  for( std::size_t i = 0 ; i < v.size() ; ++i )
   if( v[ i ] && it->second[ i ] && v[ i ]->is_fixed() ) {
    it->second[ i ]->set_value( v[ i ]->get_value() );
    it->second[ i ]->is_fixed( true , eNoMod );
    }
  }

 // the Solver of the copy
 f_bsc = Configuration::deserialize( f_files[ 1 ] );
 if( ! f_bsc )
  throw( std::logic_error( fn + ": " + f_files[ 1 ] + " cannot be read" ) );
 s_config_Block( f_copy , f_bsc , f_files[ 1 ] );
 if( f_copy->get_registered_solvers().empty() )
  throw( std::logic_error( fn + ": " + f_files[ 1 ] + " registers no "
                           "Solver" ) );
 f_inner = f_copy->get_registered_solvers().front();
 f_status = f_inner->compute( changedvars );
 return( f_status );
 }

/*--------------------------------------------------------------------------*/

Solver::OFValue BlockCopySolver::get_lb( void )
{
 return( f_inner ? f_inner->get_lb() : - Inf< OFValue >() );
 }

/*--------------------------------------------------------------------------*/

Solver::OFValue BlockCopySolver::get_ub( void )
{
 return( f_inner ? f_inner->get_ub() : Inf< OFValue >() );
 }

/*--------------------------------------------------------------------------*/

Solver::OFValue BlockCopySolver::get_var_value( void )
{
 return( f_inner ? f_inner->get_var_value() : Inf< OFValue >() );
 }

/*--------------------------------------------------------------------------*/

bool BlockCopySolver::has_var_solution( void )
{
 return( f_inner && f_inner->has_var_solution() );
 }

/*--------------------------------------------------------------------------*/

void BlockCopySolver::get_var_solution( Configuration * solc )
{
 if( ! f_inner )
  return;
 f_inner->get_var_solution( solc );
 auto from = columns( f_copy );
 auto to = columns( f_Block );
 for( auto & [ nm , v ] : from ) {
  auto it = to.find( nm );
  if( ( it == to.end() ) || ( it->second.size() != v.size() ) )
   continue;
  for( std::size_t i = 0 ; i < v.size() ; ++i )
   if( v[ i ] && it->second[ i ] )
    it->second[ i ]->set_value( v[ i ]->get_value() );
  }
 // the Variable of the formulation of the Block follow
 if( auto tub = dynamic_cast< ThermalUnitBlock * >( f_Block ) )
  tub->set_solution();
 }

/*--------------------------------------------------------------------------*/

Solution * BlockCopySolver::get_Solution( Configuration * solc )
{
 if( ! f_inner )
  return( nullptr );
 f_inner->get_var_solution();
 return( f_copy->get_Solution( solc , false ) );
 }

/*--------------------------------------------------------------------------*/

const std::string & BlockCopySolver::get_dflt_str_par( idx_type par ) const
{
 if( par >= Solver::get_num_str_par() )
  return( EMPTY );
 return( Solver::get_dflt_str_par( par ) );
 }

/*--------------------------------------------------------------------------*/

Solver::idx_type BlockCopySolver::str_par_str2idx( const std::string & name )
 const
{
 for( idx_type i = 0 ; i < PAR_NAMES.size() ; ++i )
  if( name == PAR_NAMES[ i ] )
   return( Solver::get_num_str_par() + i );
 return( Solver::str_par_str2idx( name ) );
 }

/*--------------------------------------------------------------------------*/

const std::string & BlockCopySolver::str_par_idx2str( idx_type idx ) const
{
 if( idx >= Solver::get_num_str_par() )
  return( PAR_NAMES.at( idx - Solver::get_num_str_par() ) );
 return( Solver::str_par_idx2str( idx ) );
 }

/*--------------------------------------------------------------------------*/

const std::string & BlockCopySolver::get_str_par( idx_type par ) const
{
 if( par >= Solver::get_num_str_par() )
  return( f_files.at( par - Solver::get_num_str_par() ) );
 return( Solver::get_str_par( par ) );
 }

/*--------------------------------------------------------------------------*/

void BlockCopySolver::set_par( idx_type par , std::string && value )
{
 if( par >= Solver::get_num_str_par() )
  f_files.at( par - Solver::get_num_str_par() ) = std::move( value );
 else
  Solver::set_par( par , std::move( value ) );
 }

/*--------------------------------------------------------------------------*/
/*---------------------- End File BlockCopySolver.cpp ----------------------*/
/*--------------------------------------------------------------------------*/
