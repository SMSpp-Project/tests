/*--------------------------------------------------------------------------*/
/*--------------- File UCScenarioReductionTest.cpp ------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Implementation of UCScenarioReductionTest.
 *
 * \author Benoît Tran \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
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
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <stdexcept>

#include <netcdf>
#include <netcdf.h>   // for NC_DOUBLE, NC_UINT, NC_STRING, nc_free_string

using namespace std;
using namespace SMSpp_di_unipi_it;
using namespace ScenarioReductionTesting;

/*--------------------------------------------------------------------------*/
/// expose TwoStageStochasticBlock::get_stochastic_block(), which became
/// protected in the SMS++ core header (off-limits to edit); the CSSC branch
/// only needs read access to the StochasticBlock applicator.
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

std::vector< Index >
UCScenarioReductionTest::get_intermittent_indices( UCBlock * uc )
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

UCScenarioReductionTest::UncertaintyType
UCScenarioReductionTest::infer_uncertainty_type( size_t scenario_dim ) const
{
 auto * uc = dynamic_cast< UCBlock * >( base_block );
 if( !uc )
  throw std::runtime_error( "infer_uncertainty_type: base_block is not UCBlock" );

 const size_t T  = uc->get_time_horizon();
 const size_t nd = uc->get_number_nodes();
 const size_t ni = intermittent_units_.size();

 const size_t dim_demand    = nd * T;
 const size_t dim_renewable = ni * T;
 const size_t dim_both      = dim_demand + dim_renewable;

 if( scenario_dim == dim_demand    ) return UncertaintyType::kDemandOnly;
 if( scenario_dim == dim_renewable ) return UncertaintyType::kRenewableOnly;
 if( scenario_dim == dim_both      ) return UncertaintyType::kBoth;

 throw std::runtime_error(
  "infer_uncertainty_type: scenario dim " + std::to_string(scenario_dim) +
  " does not match demand (" + std::to_string(dim_demand) +
  "), renewable (" + std::to_string(dim_renewable) +
  "), or both (" + std::to_string(dim_both) + ")" );
}

/*--------------------------------------------------------------------------*/
/*----------------------- load_problem_instance ----------------------------*/
/*--------------------------------------------------------------------------*/

void UCScenarioReductionTest::load_problem_instance( const std::string & path )
{
 if( get_int_config("intLogVerb") >= 2 )
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

 base_block          = uc;          // non-owning; uc is owned by Block factory
 instance_file_path_ = base_path;   // saved for EC Block copy workaround

 num_time_periods_  = uc->get_time_horizon();
 num_nodes_         = uc->get_number_nodes();
 intermittent_units_ = get_intermittent_indices( uc );

 if( get_int_config("intLogVerb") >= 2 ) {
  cout << "    Time periods:  " << num_time_periods_   << "\n"
       << "    Nodes:         " << num_nodes_          << "\n"
       << "    Units:         " << uc->get_number_units() << "\n"
       << "    Intermittent:  " << intermittent_units_.size() << "\n";
 }

 // Create a StochasticBlock wrapper (needed by apply_scenario_to_block)
 stochastic_block = std::make_unique< StochasticBlock >( nullptr , base_block );
}

/*--------------------------------------------------------------------------*/
/*---------------------------- build_tssb ----------------------------------*/
/*--------------------------------------------------------------------------*/

std::unique_ptr< TwoStageStochasticBlock >
UCScenarioReductionTest::build_tssb( UCBlock * uc ,
                                      DiscreteScenarioSet * dss ,
                                      const std::string & tmp ,
                                      UncertaintyType utype ) const
{
 const int N  = static_cast< int >( dss->get_poolSize() );
 const int T  = static_cast< int >( uc->get_time_horizon() );
 const int nd = static_cast< int >( uc->get_number_nodes() );
 const auto & intermittent = intermittent_units_;
 const int ni = static_cast< int >( intermittent.size() );

 {
  netCDF::NcFile f( tmp , netCDF::NcFile::replace );
  auto g = f.addGroup("TwoStageStochasticBlock");
  g.putAtt("type" , "TwoStageStochasticBlock");
  g.addDim("NumberScenarios" , N);

  // ---- StaticAbstractPath: commitment variables of ThermalUnitBlocks ----
  {
   int num_thermal = 0;
   for( Index u = 0 ; u < uc->get_number_units() ; ++u )
    if( dynamic_cast< ThermalUnitBlock * >( uc->get_unit_block(u) ) )
     num_thermal++;
   const int path_len = num_thermal * T;

   auto pg    = g.addGroup("StaticAbstractPath");
   auto pdim  = pg.addDim("PathDim"        , 1);
   auto tldim = pg.addDim("PathTotalLength" , path_len);

   std::vector< unsigned int > starts = { 0 };
   pg.addVar("PathStart"          , netCDF::NcUint() , pdim  ).putVar(starts.data());
   pg.addVar("PathNodeTypes"      , netCDF::NcChar() , tldim )
     .putVar( std::vector< char >(path_len,'V').data() );
   pg.addVar("PathGroupIndices"   , netCDF::NcUint() , tldim )
     .putVar( std::vector< unsigned int >(path_len,0).data() );
   std::vector< unsigned int > idx(path_len);
   std::iota(idx.begin(),idx.end(),0);
   pg.addVar("PathElementIndices" , netCDF::NcUint() , tldim ).putVar(idx.data());
   pg.addVar("PathRangeIndices"   , netCDF::NcUint() , tldim ).putVar(idx.data());
  }

  // ---- StochasticBlock: inner UCBlock + DataMappings --------------------
  {
   auto sg = g.addGroup("StochasticBlock");
   sg.putAtt("type","StochasticBlock");

   auto bg = sg.addGroup("Block");

   // EC instances hit a bug in ECNetworkBlock::serialize (v_ActiveDemand.size()
   // returns NumberIntervals=96 instead of NumberNodes=10). Detect by checking
   // for NumberIntervals in Block_0 of the source file, then copy directly.
   bool use_copy = false;
   if( !instance_file_path_.empty() ) {
    netCDF::NcFile probe( instance_file_path_ , netCDF::NcFile::read );
    auto grp = probe.getGroup("Block_0");
    use_copy = !grp.isNull() && !grp.getDim("NumberIntervals").isNull();
   }
   if( use_copy ) {
    netCDF::NcFile src( instance_file_path_ , netCDF::NcFile::read );
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

   // SetElements: 4 values per mapping → {from_start, from_end, to_start, to_end}
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

    // PathRangeIndices intentionally omitted, 'B' nodes have no range;
    // deserializer defaults to Inf for absent PathRangeIndices.
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
/*----------------------- build_tssb_for_current_pool ----------------------*/
/*--------------------------------------------------------------------------*/

std::unique_ptr< TwoStageStochasticBlock >
UCScenarioReductionTest::build_tssb_for_current_pool( const std::string & tmp )
{
 auto * uc = dynamic_cast< UCBlock * >( base_block );
 if( !uc )
  throw std::runtime_error( "build_tssb_for_current_pool: base_block is not UCBlock" );

 // Infer type from current scenario_set dimension (set during load())
 UncertaintyType utype = infer_uncertainty_type(
  scenario_set->get_scenarioSize() );

 return build_tssb( uc , scenario_set.get() , tmp , utype );
}

/*--------------------------------------------------------------------------*/
/*------------------------------- run_cssc ---------------------------------*/
/*--------------------------------------------------------------------------*/

void UCScenarioReductionTest::run_cssc( ScenarioReductionBlock * srb ,
                                         BlockSolverConfig      * bsc ,
                                         int                      K   )
{
 auto * uc_base = dynamic_cast< UCBlock * >( base_block );
 if( ! uc_base )
  throw std::runtime_error( "run_cssc: base_block is not UCBlock" );
 const Index T = static_cast< Index >( uc_base->get_time_horizon() );

 auto solver = std::make_unique< CSSCScenarioReductionSolver >();
 solver->set_milp_config( bsc );  // takes ownership of bsc
 solver->set_nb_reduced( K );
 solver->set_Block( srb );

 // UC VarExtractor: commitment variables u[t] of each ThermalUnitBlock
 // No DataInjector needed, set_data() triggers NBModification for UCBlock
 solver->set_var_extractor( [T]( Block * inner ) -> std::vector< ColVariable * > {
  auto * uc = dynamic_cast< UCBlock * >( inner );
  if( ! uc ) return {};
  std::vector< ColVariable * > vars;
  const Index num_units = uc->get_number_units();
  for( Index u = 0 ; u < num_units ; ++u ) {
   auto * tub = dynamic_cast< ThermalUnitBlock * >( uc->get_unit_block(u) );
   if( tub ) {
    ColVariable * cv = tub->get_commitment( 0 );
    if( cv )
     for( Index t = 0 ; t < T ; ++t )
      vars.push_back( cv + t );
    }
   }
  return vars;
  } );

 // All uncertainty types use a fresh inner UCBlock per V-matrix cell.  This is
 // forced by two independent limitations of the reused-block incremental path:
 //   (1) demand reaches the model via UCBlock::set_active_power_demand, whose
 //       incremental abstract update has an out-of-bounds index bug in
 //       ECNetworkBlock (number_nodes != number_intervals)
 //   (2) fixing the first-stage commitment on a live block needs a VariableMod,
 //       which ThermalUnitBlock rejects ("VariableMod not supported").
 // Baking the scenario into a fresh block, fixing the commitment with eNoBlck,
 // and reading everything through the constraint-generation path sidesteps
 // both.  Cost: N^2 deserializations (heavy; OOMs for N >~ 30, a memory leak
 // in the per-cell rebuild still needs addressing to scale further)
 solver->set_fix_with_modification( false );

 const size_t nd    = num_nodes_;
 const size_t Tloc  = num_time_periods_;
 const auto   inter = intermittent_units_;
 const auto   utype = uncertainty_type_;
 const std::string ipath = instance_file_path_;

 solver->set_block_factory(
  [ ipath , nd , Tloc , inter , utype ]( const std::vector< double > & scen )
    -> std::unique_ptr< Block > {
   Block * raw = Block::deserialize( ipath );
   auto * uc   = dynamic_cast< UCBlock * >( raw );
   if( ! uc ) { delete raw;
    throw std::runtime_error( "UC block_factory: deserialized block is not "
                              "a UCBlock (" + ipath + ")" ); }
   size_t roff = 0;
   if( utype == UncertaintyType::kDemandOnly ||
       utype == UncertaintyType::kBoth ) {
    uc->set_active_power_demand(
     scen.begin() , Block::Range( 0 , nd * Tloc ) , eNoBlck , eNoBlck );
    roff = nd * Tloc;
    }
   if( utype == UncertaintyType::kRenewableOnly ||
       utype == UncertaintyType::kBoth ) {
    for( size_t k = 0 ; k < inter.size() ; ++k ) {
     auto * iub = dynamic_cast< IntermittentUnitBlock * >(
      uc->get_unit_block( inter[ k ] ) );
     if( iub )
      iub->set_maximum_power(
       scen.begin() + roff + k * Tloc ,
       Block::Range( 0 , Tloc ) , eNoBlck , eNoBlck );
     }
    }
   return std::unique_ptr< Block >( uc );
   } );

 solver->compute();
 solver->get_var_solution();
}

/*--------------------------------------------------------------------------*/
/*------------------------------ create_srb --------------------------------*/
/*--------------------------------------------------------------------------*/

std::unique_ptr< ScenarioReductionBlock >
UCScenarioReductionTest::create_srb( int K , const std::string & method )
{
 auto * uc = dynamic_cast< UCBlock * >( base_block );
 if( !uc )
  throw std::runtime_error( "create_srb: base_block is not UCBlock" );

 // Refresh uncertainty type from current scenario_set
 uncertainty_type_ = infer_uncertainty_type( scenario_set->get_scenarioSize() );

 // Build ScenarioReductionBlock 
 // Both the heuristics (ScenarioReductionSolver) and CSSC
 // (CSSCScenarioReductionSolver) read the scenario data directly from the
 // DiscreteScenarioSet held by the ScenarioReductionBlock
 auto srb = std::make_unique< ScenarioReductionBlock >();
 srb->set_scenario_generator( scenario_set.get() );

 // CSSC needs TSSB + applicator 
 if( method == "cssc" ) {
  srb_tssb_ = build_tssb( uc , scenario_set.get() ,
                           "/tmp/uc_srb_tssb.nc4" , uncertainty_type_ );

  auto * stoch_app = dynamic_cast< StochasticBlock * >(
   static_cast< TSSBExposer * >( srb_tssb_.get() )->expose_stochastic_block() );
  if( !stoch_app )
   throw std::runtime_error(
    "create_srb: TwoStageStochasticBlock has no StochasticBlock applicator" );

  srb->set_stochastic_block( srb_tssb_.get() );
  srb->set_scenario_applicator( stoch_app );
 } else {
  srb_tssb_.reset();  // not needed for heuristics
 }

 return srb;
}

/*--------------------------------------------------------------------------*/
/*------------------------------- main -------------------------------------*/
/*--------------------------------------------------------------------------*/

int main( int argc , char * argv[] )
{
 ScenarioReductionTesting::UCScenarioReductionTest test;
 return test.run( argc , argv );
}

/*--------------------------------------------------------------------------*/
/*-------------- End File UCScenarioReductionTest.cpp ---------------------*/
/*--------------------------------------------------------------------------*/