/*--------------------------------------------------------------------------*/
/*--------------- File UCScenarioReductionTest.cpp ------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Implementation of the UC hook functions declared in
 * UCScenarioReductionTest.h, and main() wiring them into a ProblemHooks
 * consumed by run_scenario_reduction_test() (ScenarioReductionCommon.h)
 *
 * \author Benoît Tran \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \author Minh Duc Pham \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Benoît Tran
 */
/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include "UCScenarioReductionTest.h"

#include "DataMapping.h"
#include "DiscreteScenarioSet.h"
#include "ECNetworkBlock.h"
#include "IntermittentUnitBlock.h"
#include "ScenarioReductionBlock.h"
#include "StochasticBlock.h"
#include "ThermalUnitBlock.h"
#include "TwoStageStochasticBlock.h"
#include "UCBlock.h"

// CSSCScenarioReductionSolver is in include/CSSCScenarioReductionSolver.h
// (already included via UCScenarioReductionTest.h)

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <stdexcept>

#include <netcdf>
#include <netcdf.h>   // for NC_DOUBLE, NC_UINT, NC_STRING, nc_free_string

using namespace std;

namespace SMSpp_di_unipi_it {

/*--------------------------------------------------------------------------*/
/// expose TwoStageStochasticBlock::get_stochastic_block(), which became
/// protected in the SMS++ core header (off-limits to edit); the CSSC branch
/// only needs read access to the StochasticBlock applicator
struct TSSBExposer : public TwoStageStochasticBlock {
 StochasticBlock * expose_stochastic_block() const {
  return get_stochastic_block();
 }
};

/*--------------------------------------------------------------------------*/
/*----------------------- nc_copy_group_recursive --------------------------*/
/*--------------------------------------------------------------------------*/

static void nc_copy_group_recursive( const netCDF::NcGroup & src ,
                                      netCDF::NcGroup & dst )
{
 // 1. Group attributes (all string for UCBlock: :type=, :id=)
 for( const auto & [ name , att ] : src.getAtts() ) {
  try {
   std::string val; att.getValues( val ); dst.putAtt( name , val );
  } catch(...) {}  // skip non-string attrs
 }

 // 2. Dimensions (current group only)
 for( const auto & [ name , dim ] : src.getDims() )
  dst.addDim( name , dim.getSize() );

 // 3. Variables
 for( const auto & [ name , var ] : src.getVars() ) {
  auto type  = var.getType();
  auto sdims = var.getDims();

  size_t total = 1;
  for( const auto & d : sdims ) total *= d.getSize();

  // Resolve each source dim in dst or its parents
  std::vector< netCDF::NcDim > ddims;
  for( const auto & d : sdims ) {
   auto found = dst.getDim( d.getName() , netCDF::NcGroup::ParentsAndCurrent );
   if( found.isNull() )
    throw std::runtime_error( "nc_copy_group_recursive: dim not found: "
                               + d.getName() );
   ddims.push_back( found );
  }

  auto dvar = dst.addVar( name , type , ddims );
  if( total == 0 ) continue;

  auto tid = type.getId();
  if( tid == NC_STRING ) {
   // Variable-length strings: getVar allocates char* entries
   std::vector< char * > ptrs( total , nullptr );
   var.getVar( ptrs.data() );
   std::vector< const char * > cptrs( total );
   for( size_t i = 0 ; i < total ; ++i )
    cptrs[ i ] = ptrs[ i ] ? ptrs[ i ] : "";
   dvar.putVar( cptrs.data() );
   nc_free_string( static_cast< size_t >( total ) , ptrs.data() );
  }
  else if( tid == NC_CHAR || tid == NC_BYTE || tid == NC_UBYTE ) {
   std::vector< char > buf( total );
   var.getVar( buf.data() );
   dvar.putVar( buf.data() );
  }
  else {
   // All numeric types: netCDF converts to/from double transparently
   std::vector< double > buf( total );
   var.getVar( buf.data() );
   dvar.putVar( buf.data() );
  }
 }

 // 4. Recurse into child groups
 for( const auto & [ name , child ] : src.getGroups() ) {
  auto dst_child = dst.addGroup( name );
  nc_copy_group_recursive( child , dst_child );
 }
}

/*--------------------------------------------------------------------------*/
/*----------------------- get_intermittent_indices -------------------------*/
/*--------------------------------------------------------------------------*/

static std::vector< Index > get_intermittent_indices( UCBlock * uc )
{
 std::vector< Index > idx;
 const Index n = uc->get_number_units();
 for( Index u = 0 ; u < n ; ++u )
  if( dynamic_cast< IntermittentUnitBlock * >( uc->get_unit_block(u) ) )
   idx.push_back(u);
 return idx;
}

/*--------------------------------------------------------------------------*/
/*----------------------- infer_uncertainty_type ---------------------------*/
/*--------------------------------------------------------------------------*/

static UCState::UncertaintyType
infer_uncertainty_type( Block * base_block , const UCState & uc_state ,
                        size_t scenario_dim )
{
 auto * uc = dynamic_cast< UCBlock * >( base_block );
 if( !uc )
  throw std::runtime_error( "infer_uncertainty_type: base_block is not UCBlock" );

 const size_t T  = uc->get_time_horizon();
 const size_t nd = uc->get_number_nodes();
 const size_t ni = uc_state.intermittent_units.size();

 const size_t dim_demand    = nd * T;
 const size_t dim_renewable = ni * T;
 const size_t dim_both      = dim_demand + dim_renewable;

 if( scenario_dim == dim_demand    ) return UCState::UncertaintyType::kDemandOnly;
 if( scenario_dim == dim_renewable ) return UCState::UncertaintyType::kRenewableOnly;
 if( scenario_dim == dim_both      ) return UCState::UncertaintyType::kBoth;

 throw std::runtime_error(
  "infer_uncertainty_type: scenario dim " + std::to_string(scenario_dim) +
  " does not match demand (" + std::to_string(dim_demand) +
  "), renewable (" + std::to_string(dim_renewable) +
  "), or both (" + std::to_string(dim_both) + ")" );
}

/*--------------------------------------------------------------------------*/
/*----------------------- uc_load_problem_instance --------------------------*/
/*--------------------------------------------------------------------------*/

void uc_load_problem_instance(
  ScenarioReductionState & state , UCState & uc_state ,
  const std::string & path )
{
 if( get_int_config( state.config , "intLogVerb" ) >= 2 )
  cout << "  Loading UC instance from: " << path << "\n";

 // Strip optional "TSSB_" prefix from filename
 std::string base_path = path;
 {
  size_t slash = path.rfind('/');
  std::string dir  = (slash != std::string::npos) ? path.substr(0,slash+1) : "";
  std::string file = (slash != std::string::npos) ? path.substr(slash+1)   : path;
  if( file.starts_with("TSSB_") )
   base_path = dir + file.substr(5);
 }

 Block * raw = Block::deserialize( base_path );
 auto * uc   = dynamic_cast< UCBlock * >( raw );
 if( !uc ) { delete raw;
  throw std::runtime_error( "Failed to load UCBlock from " + base_path ); }

 state.base_block       = uc;          // non-owning; uc is owned by Block factory
 uc_state.instance_file_path = base_path;   // saved for EC Block copy workaround

 uc_state.num_time_periods  = uc->get_time_horizon();
 uc_state.num_nodes         = uc->get_number_nodes();
 uc_state.intermittent_units = get_intermittent_indices( uc );

 if( get_int_config( state.config , "intLogVerb" ) >= 2 ) {
  cout << "    Time periods:  " << uc_state.num_time_periods   << "\n"
       << "    Nodes:         " << uc_state.num_nodes          << "\n"
       << "    Units:         " << uc->get_number_units() << "\n"
       << "    Intermittent:  " << uc_state.intermittent_units.size() << "\n";
 }

 // Create a StochasticBlock wrapper (needed by apply_scenario_to_block)
 state.stochastic_block = std::make_unique< StochasticBlock >( nullptr , state.base_block );
}

/*--------------------------------------------------------------------------*/
/*---------------------------- build_tssb ----------------------------------*/
/*--------------------------------------------------------------------------*/
/* Internal helper (not in the header): builds a TSSB from an explicit DSS.
 * Used both by uc_build_tssb_for_current_pool() (full/reduced solves) and by
 * uc_create_srb()'s CSSC branch (needs a TSSB over the current pool to hand
 * to the ScenarioReductionBlock as its stochastic_block/applicator)*/

static std::unique_ptr< TwoStageStochasticBlock >
build_tssb( UCBlock * uc , DiscreteScenarioSet * dss , const std::string & tmp ,
           UCState::UncertaintyType utype , const UCState & uc_state )
{
 using UncertaintyType = UCState::UncertaintyType;

 const int N  = static_cast< int >( dss->get_poolSize() );
 const int T  = static_cast< int >( uc->get_time_horizon() );
 const int nd = static_cast< int >( uc->get_number_nodes() );
 const auto & intermittent = uc_state.intermittent_units;
 const int ni = static_cast< int >( intermittent.size() );

 {
  netCDF::NcFile f( tmp , netCDF::NcFile::replace );
  auto g = f.addGroup("TwoStageStochasticBlock");
  g.putAtt("type" , "TwoStageStochasticBlock");
  g.addDim("NumberScenarios" , N);

  // ---- StaticAbstractPath: commitment variables of ThermalUnitBlocks ----
  // One 2-node path per ThermalUnitBlock: 'B' navigates to the unit's own
  // nested Block (group index u, matching get_unit_block(u) == v_Block[u]),
  // then 'V' selects the whole "u_thermal" (commitment) static variable
  // group in one shot via a range (element_index=0, range_index=T). Named
  // group lookup ("u_thermal") is used instead of a numeric index so this
  // does not depend on ThermalUnitBlock's variable registration order
  {
   std::vector< Index > thermal_units;
   for( Index u = 0 ; u < uc->get_number_units() ; ++u )
    if( dynamic_cast< ThermalUnitBlock * >( uc->get_unit_block(u) ) )
     thermal_units.push_back( u );
   const int num_thermal = static_cast< int >( thermal_units.size() );

   auto pg    = g.addGroup("StaticAbstractPath");
   auto pdim  = pg.addDim("PathDim"        , num_thermal );
   auto tldim = pg.addDim("PathTotalLength" , num_thermal * 2 );

   std::vector< unsigned int > starts( num_thermal );
   for( int k = 0 ; k < num_thermal ; ++k )
    starts[ k ] = static_cast< unsigned int >( k * 2 );

   std::vector< char > node_types( num_thermal * 2 );
   std::vector< std::string > group_names( num_thermal * 2 );
   std::vector< unsigned int > elem_idx( num_thermal * 2 , 0 );
   std::vector< unsigned int > range_idx( num_thermal * 2 , 0 );

   for( int k = 0 ; k < num_thermal ; ++k ) {
    node_types[ 2*k ]     = 'B';
    group_names[ 2*k ]    = std::to_string( thermal_units[ k ] );
    // element/range indices for the 'B' node are unused (Block selection
    // uses only the group index), left at 0.

    node_types[ 2*k + 1 ]  = 'V';
    group_names[ 2*k + 1 ] = "u_thermal";
    elem_idx[ 2*k + 1 ]    = 0;
    range_idx[ 2*k + 1 ]   = static_cast< unsigned int >( T );
    }

   pg.addVar("PathStart"     , netCDF::NcUint() , pdim  ).putVar(starts.data());
   pg.addVar("PathNodeTypes" , netCDF::NcChar() , tldim ).putVar(node_types.data());
   {
    auto gv = pg.addVar("PathGroupIndices" , netCDF::NcString() , tldim );
    std::vector< const char * > cptrs( group_names.size() );
    for( size_t i = 0 ; i < group_names.size() ; ++i )
     cptrs[ i ] = group_names[ i ].c_str();
    gv.putVar( cptrs.data() );
   }
   pg.addVar("PathElementIndices" , netCDF::NcUint() , tldim ).putVar(elem_idx.data());
   pg.addVar("PathRangeIndices"   , netCDF::NcUint() , tldim ).putVar(range_idx.data());
  }

  // ---- StochasticBlock: inner UCBlock + DataMappings --------------------
  {
   auto sg = g.addGroup("StochasticBlock");
   sg.putAtt("type","StochasticBlock");

   auto bg = sg.addGroup("Block");

   // EC instances hit a bug in ECNetworkBlock::serialize (v_ActiveDemand.size()
   // returns NumberIntervals=96 instead of NumberNodes=10). Detect by checking
   // for NumberIntervals in Block_0 of the source file, then copy directly
   bool use_copy = false;
   if( !uc_state.instance_file_path.empty() ) {
    netCDF::NcFile probe( uc_state.instance_file_path , netCDF::NcFile::read );
    auto grp = probe.getGroup("Block_0");
    use_copy = !grp.isNull() && !grp.getDim("NumberIntervals").isNull();
   }
   if( use_copy ) {
    netCDF::NcFile src( uc_state.instance_file_path , netCDF::NcFile::read );
    nc_copy_group_recursive( src.getGroup("Block_0") , bg );
   } else {
    uc->serialize(bg);
   }

   int num_mappings = 0;
   if( utype == UncertaintyType::kDemandOnly    ) num_mappings = 1;
   if( utype == UncertaintyType::kRenewableOnly ) num_mappings = ni;
   if( utype == UncertaintyType::kBoth          ) num_mappings = 1 + ni;

   auto ndm = sg.addDim("NumberDataMappings", num_mappings);

   std::vector< char > dtypes(num_mappings,'D');
   std::vector< char > callers(num_mappings,'B');
   sg.addVar("DataType",netCDF::NcChar(),ndm).putVar(dtypes.data());
   sg.addVar("Caller"  ,netCDF::NcChar(),ndm).putVar(callers.data());

   std::vector< std::string > fn;
   if( utype == UncertaintyType::kDemandOnly ||
       utype == UncertaintyType::kBoth )
    fn.push_back("UCBlock::set_active_power_demand");
   if( utype == UncertaintyType::kRenewableOnly ||
       utype == UncertaintyType::kBoth )
    for( int k = 0 ; k < ni ; ++k )
     fn.push_back("IntermittentUnitBlock::set_maximum_power");
   {
    auto fv = sg.addVar("FunctionName",netCDF::NcString(),ndm);
    for( int m = 0 ; m < num_mappings ; ++m )
     fv.putVar({static_cast<size_t>(m)}, fn[m]);
   }

   // SetSize: all zeros, both SetFrom and SetTo are Range for every mapping
   std::vector< unsigned int > ss_vec( 2 * num_mappings , 0u );
   auto ssd = sg.addDim("SetSizeDim", ss_vec.size());
   sg.addVar("SetSize",netCDF::NcUint(),ssd).putVar(ss_vec.data());

   // SetElements: 4 values per mapping -> {from_start, from_end, to_start, to_end}
   // Scenario vector layout:
   //   [demand: nd*T values][renewable_0: T values]...[renewable_{ni-1}: T values]
   //   (demand part only present for kDemandOnly / kBoth)
   std::vector< unsigned int > se_vec;
   {
    const unsigned int T_u  = static_cast< unsigned int >( T );
    const unsigned int ndT  = static_cast< unsigned int >( nd * T );
    unsigned int roff = 0u;   // offset into scenario vector where renewables start

    if( utype == UncertaintyType::kDemandOnly ||
        utype == UncertaintyType::kBoth ) {
     se_vec.push_back(0);    se_vec.push_back(ndT);   // SetFrom = [0, nd*T)
     se_vec.push_back(0);    se_vec.push_back(ndT);   // SetTo   = [0, nd*T)
     roff = ndT;
    }
    if( utype == UncertaintyType::kRenewableOnly ||
        utype == UncertaintyType::kBoth )
     for( int k = 0 ; k < ni ; ++k ) {
      se_vec.push_back( roff + k * T_u     );  // SetFrom start
      se_vec.push_back( roff + (k+1) * T_u );  // SetFrom end
      se_vec.push_back( 0 );                    // SetTo start
      se_vec.push_back( T_u );                  // SetTo end
     }
   }
   auto sed = sg.addDim("SetElementsDim", se_vec.size());
   sg.addVar("SetElements",netCDF::NcUint(),sed).putVar(se_vec.data());

   // Vectorized "AbstractPath" group for all DataMappings (required by
   // SimpleDataMappingBase::pre_deserialize, which looks for exactly this name)
   // - demand mapping:    empty path (length 0), caller is UCBlock itself
   // - renewable mapping: length-1 'G' path to the IntermittentUnitBlock child
   {
    std::vector< unsigned int > path_starts;
    std::vector< char >         node_types;
    std::vector< unsigned int > group_idxs;
    std::vector< unsigned int > elem_idxs;

    unsigned int cur = 0;
    if( utype == UncertaintyType::kDemandOnly ||
        utype == UncertaintyType::kBoth ) {
     path_starts.push_back( cur );          // empty path: length 0
    }
    if( utype == UncertaintyType::kRenewableOnly ||
        utype == UncertaintyType::kBoth ) {
     for( int k = 0 ; k < ni ; ++k ) {
      path_starts.push_back( cur );
      node_types.push_back( 'B' );    // 'B' = navigate to child Block
      group_idxs.push_back( static_cast< unsigned int >( intermittent[k] ) );
      elem_idxs.push_back( 0 );       // element_index meaningless for 'B' nodes
      ++cur;
     }
    }

    const size_t num_paths  = path_starts.size();
    const size_t total_len  = node_types.size();

    auto apg     = sg.addGroup( "AbstractPath" );
    auto ap_pdim = apg.addDim( "PathDim"        , num_paths  );
    auto ap_tdim = apg.addDim( "PathTotalLength" , total_len  );

    apg.addVar( "PathStart"         , netCDF::NcUint() , ap_pdim )
       .putVar( path_starts.data() );

    // PathRangeIndices intentionally omitted, 'B' nodes have no range
    // deserializer defaults to Inf for absent PathRangeIndices
    if( total_len > 0 ) {
     apg.addVar( "PathNodeTypes"      , netCDF::NcChar() , ap_tdim )
        .putVar( node_types.data() );
     apg.addVar( "PathGroupIndices"   , netCDF::NcUint() , ap_tdim )
        .putVar( group_idxs.data() );
     apg.addVar( "PathElementIndices" , netCDF::NcUint() , ap_tdim )
        .putVar( elem_idxs.data() );
    } else {
     apg.addVar( "PathNodeTypes"      , netCDF::NcChar() , ap_tdim );
     apg.addVar( "PathGroupIndices"   , netCDF::NcUint() , ap_tdim );
     apg.addVar( "PathElementIndices" , netCDF::NcUint() , ap_tdim );
    }
   }
  }

  // ---- DiscreteScenarioSet ---------------------------------------------
  {
   auto dg = g.addGroup("DiscreteScenarioSet");
   dss->serialize(dg);
  }
 }  // NcFile closed

 auto tssb = std::make_unique< TwoStageStochasticBlock >();
 {
  netCDF::NcFile f(tmp, netCDF::NcFile::read);
  tssb->deserialize( f.getGroup("TwoStageStochasticBlock") );
 }
 std::remove(tmp.c_str());
 return tssb;
}

/*--------------------------------------------------------------------------*/
/*----------------------- uc_build_tssb_for_current_pool --------------------*/
/*--------------------------------------------------------------------------*/

std::unique_ptr< TwoStageStochasticBlock >
uc_build_tssb_for_current_pool(
  ScenarioReductionState & state , UCState & uc_state , const std::string & tmp )
{
 auto * uc = dynamic_cast< UCBlock * >( state.base_block );
 if( !uc )
  throw std::runtime_error( "uc_build_tssb_for_current_pool: base_block is not UCBlock" );

 // Infer type from current scenario_set dimension (set during load())
 auto utype = infer_uncertainty_type(
  state.base_block , uc_state , state.scenario_set->get_scenarioSize() );

 return build_tssb( uc , state.scenario_set.get() , tmp , utype , uc_state );
}

/*--------------------------------------------------------------------------*/
/*------------------------------- uc_run_cssc -------------------------------*/
/*--------------------------------------------------------------------------*/

void uc_run_cssc(
  ScenarioReductionState & state , UCState & uc_state ,
  ScenarioReductionBlock * srb , BlockSolverConfig * bsc , int K )
{
 auto * uc_base = dynamic_cast< UCBlock * >( state.base_block );
 if( ! uc_base )
  throw std::runtime_error( "run_cssc: base_block is not UCBlock" );

 auto solver = std::make_unique< CSSCScenarioReductionSolver >();
 solver->set_milp_config( bsc );  // takes ownership of bsc
 solver->set_nb_reduced( K );
 solver->set_Block( srb );

 // No set_var_extractor call: the solver's generic AbstractPath-based
 // fallback reads the commitment variables directly from the TSSB's
 // StaticAbstractPath (see build_tssb()'s 'B'+'V' path per ThermalUnitBlock),
 // verified to match the old hand-rolled extractor exactly

 // UCBlock cannot  tell its Solver about every incremental data
 // change, so CSSC must use reload mode: after each set_data(), the whole
 // model is reloaded from scratch
 solver->set_fix_with_modification( false );

 // When the StochasticBlock was deserialized, each DataMapping's target was
 // resolved as an AbstractPath relative to the file. The demand mapping has
 // an empty path (it targets the UCBlock itself), and resolving an empty
 // path returns null. So we point every mapping's target at the actual live
 // Block here, otherwise set_data() would call a setter on a null pointer
 // and crash
 auto * stoch = dynamic_cast< StochasticBlock * >(
  srb->get_scenario_applicator() );
 if( ! stoch )
  throw std::runtime_error(
   "run_cssc: needs a StochasticBlock applicator" );
 if( auto * inner = stoch->get_inner_block() )
  for( const auto & m : stoch->get_data_mappings() )
   m->set_caller_from_reference( inner );

 solver->compute();
 solver->get_var_solution();
}

/*--------------------------------------------------------------------------*/
/*------------------------------ uc_create_srb ------------------------------*/
/*--------------------------------------------------------------------------*/

std::unique_ptr< ScenarioReductionBlock >
uc_create_srb(
  ScenarioReductionState & state , UCState & uc_state ,
  int K , const std::string & method )
{
 auto * uc = dynamic_cast< UCBlock * >( state.base_block );
 if( !uc )
  throw std::runtime_error( "create_srb: base_block is not UCBlock" );

 // Refresh uncertainty type from current scenario_set
 uc_state.uncertainty_type = infer_uncertainty_type(
  state.base_block , uc_state , state.scenario_set->get_scenarioSize() );

 // Build ScenarioReductionBlock
 // Both the heuristics (ScenarioReductionSolver) and CSSC
 // (CSSCScenarioReductionSolver) read the scenario data directly from the
 // DiscreteScenarioSet held by the ScenarioReductionBlock
 auto srb = std::make_unique< ScenarioReductionBlock >();
 srb->set_scenario_generator( state.scenario_set.get() );

 // CSSC needs TSSB + applicator
 if( method == "cssc" ) {
  uc_state.srb_tssb = build_tssb( uc , state.scenario_set.get() ,
                                  "/tmp/uc_srb_tssb.nc4" ,
                                  uc_state.uncertainty_type , uc_state );

  auto * stoch_app = dynamic_cast< StochasticBlock * >(
   static_cast< TSSBExposer * >( uc_state.srb_tssb.get() )->expose_stochastic_block() );
  if( !stoch_app )
   throw std::runtime_error(
    "create_srb: TwoStageStochasticBlock has no StochasticBlock applicator" );

  srb->set_stochastic_block( uc_state.srb_tssb.get() );
  srb->set_scenario_applicator( stoch_app );
 } else {
  uc_state.srb_tssb.reset();  // not needed for heuristics
 }

 return srb;
}

}  // namespace SMSpp_di_unipi_it

/*--------------------------------------------------------------------------*/
/*------------------------------- main -------------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

int main( int argc , char * argv[] )
{
 UCState uc_state;
 ProblemHooks hooks;
 hooks.problem_type = "UC";

 hooks.load_problem_instance =
  [ & ]( ScenarioReductionState & state , const std::string & path ) {
   uc_load_problem_instance( state , uc_state , path );
  };
 hooks.create_srb =
  [ & ]( ScenarioReductionState & state , int K , const std::string & method ) {
   return uc_create_srb( state , uc_state , K , method );
  };
 hooks.build_tssb_for_current_pool =
  [ & ]( ScenarioReductionState & state , const std::string & tmp ) {
   return uc_build_tssb_for_current_pool( state , uc_state , tmp );
  };
 hooks.run_cssc =
  [ & ]( ScenarioReductionState & state , ScenarioReductionBlock * srb ,
         BlockSolverConfig * bsc , int K ) {
   uc_run_cssc( state , uc_state , srb , bsc , K );
  };
 hooks.get_scenarios_directory = [] { return std::string( "../scenarios/UCBlock/" ); };

 return run_scenario_reduction_test( argc , argv , hooks );
}

/*--------------------------------------------------------------------------*/
/*-------------- End File UCScenarioReductionTest.cpp ---------------------*/
/*--------------------------------------------------------------------------*/
