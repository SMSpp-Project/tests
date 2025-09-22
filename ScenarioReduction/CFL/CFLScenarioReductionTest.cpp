/*--------------------------------------------------------------------------*/
/*------------------ File CFLScenarioReductionTest.cpp --------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Implementation of CFL-specific scenario reduction test.
 *
 * \author Benoît Tran \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Benoît Tran
 */
/*--------------------------------------------------------------------------*/

#include "CFLScenarioReductionTest.h"

#include <fstream>
#include <random>

#include "Solver.h"

using namespace std;
using namespace SMSpp_di_unipi_it;
using namespace ScenarioReductionTesting;

/*--------------------------------------------------------------------------*/
/*---------------------- PURE VIRTUAL IMPLEMENTATIONS ----------------------*/
/*--------------------------------------------------------------------------*/

void CFLScenarioReductionTest::load_problem_instance( const string & path ) {
 if( get_int_config( "intLogVerb" ) >= 2 ) {
  cout << "  Loading CFL instance from: " << path << endl;
 }
 // Load the block
 Block * block = Block::deserialize( path );
 auto * base_cfl = dynamic_cast< CapacitatedFacilityLocationBlock * >(block);

 if( ! base_cfl ) {
  delete block;
  throw runtime_error( "Failed to load CFL instance from " + path );
 }
 // Set the base_block member (required by abstract class)
 base_block = base_cfl;

 // Extract dimensions
 num_facilities = base_cfl->get_NFacilities( );
 num_customers = base_cfl->get_NCustomers( );

 // Ensure single-sourcing for stochastic version
 if( ! base_cfl->get_UnSplittable( )) {
  base_cfl->chg_UnSplittable( true );
  if( get_int_config( "intLogVerb" ) >= 2 ) {
   cout << "  Set UnSplittable = true for single-sourcing" << endl;
  }
 }
 // Extract base data
 base_demands = extract_base_demands( );

 // Extract facility data for reference
 facility_costs.resize( num_facilities );
 facility_capacities.resize( num_facilities );
 for(size_t f = 0; f < num_facilities; ++f) {
  facility_costs[ f ] = base_cfl->get_Fixed_Cost( f );
  facility_capacities[ f ] = base_cfl->get_Capacity( f );
 }
 // Create the StochasticBlock parametrically
 stochastic_block = std::make_unique< StochasticBlock >( nullptr , base_block );

 // Create DataMapping for customer demands
 // Maps scenario data (all customer demands) to the CFL block's demands
 using SDM = SimpleDataMapping< Block::Range , Block::Range , double >;

 // Create range for all customer demands
 Block::Range demand_range( 0 , num_customers );

 // Get the function pointer from the methods factory
 // The function type matches chg_customer_demands(c_DV_it, Range, ModParam,
 // ModParam)
 using FuncType =
   Block::FunctionType< std::vector< double >::const_iterator , Block::Range >;
 auto chg_demands_func = Block::get_method< FuncType >(
  "CapacitatedFacilityLocationBlock::chg_customer_demands" );

 // Create the DataMapping
 auto demand_mapping = std::make_unique< SDM >(
  chg_demands_func ,
  base_block ,
  demand_range ,
  demand_range );

 // Add the data mapping to the StochasticBlock
 std::vector< std::unique_ptr< SimpleDataMappingBase > > mappings;
 mappings.push_back( std::move( demand_mapping ));
 stochastic_block->set_data_mappings( std::move( mappings ));

 if( get_int_config( "intLogVerb" ) >= 2 ) {
  cout << "  Loaded CFL with " << num_facilities << " facilities and "
       << num_customers << " customers" << endl;
  cout << "  Created StochasticBlock with demand mapping" << endl;
 }
}

/*--------------------------------------------------------------------------*/

void CFLScenarioReductionTest::create_twostage_netcdf( const string & filename )
{
 if( get_int_config( "intLogVerb" ) >= 2 ) {
  cout << "  Creating two-stage stochastic CFL in " << filename << endl;
 }
 try {
  // Create netCDF file
  netCDF::NcFile file( filename , netCDF::NcFile::replace );

  if( get_int_config( "intLogVerb" ) >= 2 ) {
   cout << "  NetCDF file opened for writing" << endl;
  }
  // Create root group for TwoStageStochasticBlock
  auto tssGroup = file.addGroup( "TwoStageStochasticBlock" );

  // Add NumberScenarios dimension at TwoStageStochasticBlock level
  // Use poolSize to get the current number of scenarios (after reduction if
  // applicable)
  size_t num_scenarios = scenario_set->get_poolSize( );
  tssGroup.addDim( "NumberScenarios" , num_scenarios );

  // Add first-stage block (CFL)
  auto firstStageGroup = tssGroup.addGroup( "FirstStageBlock" );
  dynamic_cast< CapacitatedFacilityLocationBlock * >(base_block)
  ->serialize( firstStageGroup );

  // Add StaticAbstractPath for first-stage variables (facility opening
  // decisions)
  size_t nf = num_facilities; // Number of facilities
  auto staticPathGroup = tssGroup.addGroup( "StaticAbstractPath" );
  auto staticPathDim = staticPathGroup.addDim( "PathDim" , nf );
  auto staticTotalLengthDim = staticPathGroup.addDim( "PathTotalLength" , nf );

  vector< unsigned int > indices( nf );
  for(size_t i = 0; i < nf; ++i)
   indices[ i ] = i;
  staticPathGroup.addVar( "PathStart" , netCDF::NcUint( ) , staticPathDim )
  .putVar( indices.data( ));
  staticPathGroup
  .addVar( "PathNodeTypes" , netCDF::NcChar( ) , staticTotalLengthDim )
  .putVar( vector< char >( nf , 'V' ).data( ));
  staticPathGroup
  .addVar( "PathGroupIndices" , netCDF::NcUint( ) , staticTotalLengthDim )
  .putVar( vector< unsigned int >( nf , 0 ).data( ));
  staticPathGroup
  .addVar( "PathElementIndices" , netCDF::NcUint( ) , staticTotalLengthDim )
  .putVar( indices.data( ));
  staticPathGroup
  .addVar( "PathRangeIndices" , netCDF::NcUint( ) , staticTotalLengthDim )
  .putVar( indices.data( ));

  // Create StochasticBlock group
  auto stochGroup = tssGroup.addGroup( "StochasticBlock" );

  // Add StochasticBlock attributes
  stochGroup.putAtt( "type" , "StochasticBlock" );

  // Add the inner Block (another CFL block for second stage)
  auto innerBlockGroup = stochGroup.addGroup( "Block" );
  dynamic_cast< CapacitatedFacilityLocationBlock * >(base_block)
  ->serialize( innerBlockGroup );

  // Add DiscreteScenarioSet at TwoStageStochasticBlock level
  auto dssGroup = tssGroup.addGroup( "DiscreteScenarioSet" );
  scenario_set->serialize( dssGroup );

  // Add DataMapping structure to tell StochasticBlock how to apply scenario
  // data
  auto numDataMappings = stochGroup.addDim( "NumberDataMappings" , 1 );

  // DataType: 'D' for double
  char dataType = 'D';
  stochGroup.addVar( "DataType" , netCDF::NcChar( ) , numDataMappings )
  .putVar( & dataType );

  // Caller: 'B' for Block
  char caller = 'B';
  stochGroup.addVar( "Caller" , netCDF::NcChar( ) , numDataMappings )
  .putVar( & caller );

  // Function name for changing customer demands
  string functionName =
    "CapacitatedFacilityLocationBlock::chg_customer_demands";
  stochGroup.addVar( "FunctionName" , netCDF::NcString( ) , numDataMappings )
  .putVar( { 0 } , & functionName );

  // SetSize: [0, 0] means Range for both SetFrom and SetTo
  auto setSizeDim = stochGroup.addDim( "SetSizeDim" , 2 );
  vector< unsigned int > setSize = { 0 , 0 }; // 0 = Range type
  stochGroup.addVar( "SetSize" , netCDF::NcUint( ) , setSizeDim )
  .putVar( setSize.data( ));

  // Ordered flag (for subsets, not used for ranges)
  unsigned char ordered = 0;
  stochGroup.addVar( "Ordered" , netCDF::NcUbyte( ) , numDataMappings )
  .putVar( & ordered );

  // SetElements: [from_start, from_end, to_start, to_end]
  // Map from indices [0, num_customers) to [0, num_customers)
  auto setElementsDim = stochGroup.addDim( "SetElementsDim" , 4 );
  vector< unsigned int > setElements = {
   0 ,
   static_cast< unsigned int >(num_customers) ,
   0 ,
   static_cast< unsigned int >(num_customers) };
  stochGroup.addVar( "SetElements" , netCDF::NcUint( ) , setElementsDim )
  .putVar( setElements.data( ));

  // Add AbstractPath for DataMapping (empty path means the Block itself)
  auto abstractPathGroup = stochGroup.addGroup( "AbstractPath" );
  abstractPathGroup.addDim( "PathDim" , 1 );
  auto pathTotalLength = abstractPathGroup.addDim( "PathTotalLength" , 0 );
  unsigned int pathStart = 0;
  abstractPathGroup
  .addVar(
    "PathStart" ,
    netCDF::NcUint( ) ,
    abstractPathGroup.getDim( "PathDim" ))
  .putVar( & pathStart );
  abstractPathGroup.addVar( "PathNodeTypes" , netCDF::NcChar( ) ,
    pathTotalLength );
  abstractPathGroup.addVar(
    "PathGroupIndices" ,
    netCDF::NcUint( ) ,
    pathTotalLength );
  abstractPathGroup.addVar(
    "PathElementIndices" ,
    netCDF::NcUint( ) ,
    pathTotalLength );
  abstractPathGroup.addVar(
    "PathRangeIndices" ,
    netCDF::NcUint( ) ,
    pathTotalLength );

  // Set TwoStageStochasticBlock attributes
  tssGroup.putAtt( "type" , "TwoStageStochasticBlock" );

  file.close( );

  if( get_int_config( "intLogVerb" ) >= 2 ) {
   cout << "  NetCDF file closed successfully" << endl;
  }
 } catch( const netCDF::exceptions::NcException & e ) {
  throw runtime_error( "Failed to create two-stage netCDF: " + string( e.what( )
   ));
 }
}

/*--------------------------------------------------------------------------*/
/*--------------------------- PRIVATE METHODS ------------------------------*/
/*--------------------------------------------------------------------------*/

vector< double > CFLScenarioReductionTest::extract_base_demands( ) {
 vector< double > demands( num_customers );
 auto * cfl = dynamic_cast< CapacitatedFacilityLocationBlock * >(base_block);
 for(size_t i = 0; i < num_customers; ++i) {
  demands[ i ] = cfl->get_Demand( i );
 }
 return demands;
}

/*--------------------------------------------------------------------------*/
/*------------------ End File CFLScenarioReductionTest.cpp -----------------*/
/*--------------------------------------------------------------------------*/
