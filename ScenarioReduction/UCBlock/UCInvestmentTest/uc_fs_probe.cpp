/*--------------------------------------------------------------------------*/
/* TEMPORARY probe: compare TwoStageStochasticBlock::get_first_stage_variables()
 * (generic, StaticAbstractPath-based) against a hand-rolled UC design-variable
 * extractor, WITHOUT solving anything (deserialize + generate_abstract_variables
 * only). Used to get fast, current counts instead of waiting ~15-20 min for a
 * full run to reach the same comparison inside cssc_pick(). Safe to delete
 * once the get_first_stage_variables() investigation is done. */
/*--------------------------------------------------------------------------*/

#include "AbstractPath.h"
#include "ColVariable.h"
#include "TwoStageStochasticBlock.h"
#include "UCBlock.h"
#include "UnitBlock.h"
#include "IntermittentUnitBlock.h"
#include "BatteryUnitBlock.h"
#include "DesignNetworkBlock.h"

#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace SMSpp_di_unipi_it;
using Index = unsigned int;

static std::map< ColVariable * , std::string > g_label;

static std::vector< ColVariable * > uc_design_vars( Block * scenario_block )
{
 std::vector< ColVariable * > fs;
 auto * uc = dynamic_cast< UCBlock * >( scenario_block );
 if( ! uc )
  if( auto * sb = dynamic_cast< StochasticBlock * >( scenario_block ) )
   uc = dynamic_cast< UCBlock * >( sb->get_inner_block() );
 if( ! uc )
  throw std::runtime_error( "uc_design_vars: inner block is not a UCBlock" );

 int n_iub = 0 , n_iub_live = 0;
 int n_bub = 0 , n_bub_batt_live = 0 , n_bub_conv_live = 0;

 for( Index u = 0 ; u < uc->get_number_units() ; ++u ) {
  UnitBlock * ub = uc->get_unit_block( u );
  if( auto * iub = dynamic_cast< IntermittentUnitBlock * >( ub ) ) {
   n_iub++;
   if( iub->get_investment_cost() != 0 ) {
    fs.push_back( & iub->get_design() );
    g_label[ & iub->get_design() ] = "Intermittent[u=" + std::to_string(u) + "]";
    n_iub_live++;
    }
   }
  else if( auto * bub = dynamic_cast< BatteryUnitBlock * >( ub ) ) {
   n_bub++;
   if( bub->get_batt_investment_cost() != 0 ) {
    fs.push_back( & bub->get_batt_design() );
    g_label[ & bub->get_batt_design() ] = "Battery-batt[u=" + std::to_string(u) + "]";
    n_bub_batt_live++;
    }
   if( bub->get_conv_investment_cost() != 0 ) {
    fs.push_back( & bub->get_conv_design() );
    g_label[ & bub->get_conv_design() ] = "Battery-conv[u=" + std::to_string(u) + "]";
    n_bub_conv_live++;
    }
   }
  }

 int n_dnb = 0 , n_dnb_lines = 0;
 for( NetworkBlock * nb : uc->get_network_blocks() )
  if( auto * dnb = dynamic_cast< DesignNetworkBlock * >( nb ) ) {
   n_dnb++;
   Index line = 0;
   for( ColVariable & v : dnb->get_design() ) {
    fs.push_back( & v );
    g_label[ & v ] = "DesignNetworkBlock[line=" + std::to_string(line++) + "]";
    n_dnb_lines++;
    }
   }

 std::cout << "  IntermittentUnitBlock: total=" << n_iub
           << "  InvestmentCost!=0=" << n_iub_live << "\n"
           << "  BatteryUnitBlock: total=" << n_bub
           << "  batt-live=" << n_bub_batt_live
           << "  conv-live=" << n_bub_conv_live << "\n"
           << "  DesignNetworkBlock: total=" << n_dnb
           << "  design-vars(lines)=" << n_dnb_lines << "\n";

 return fs;
}

int main( int argc , char * argv[] )
{
 if( argc < 2 ) {
  std::cerr << "usage: " << argv[ 0 ] << " <instance.nc>\n";
  return 1;
  }

 Block * raw = Block::deserialize( argv[ 1 ] );
 auto * tssb = dynamic_cast< TwoStageStochasticBlock * >( raw );
 if( ! tssb ) {
  std::cerr << "not a TwoStageStochasticBlock\n";
  return 1;
  }
 std::unique_ptr< Block > owner( tssb );

 tssb->generate_abstract_variables();

 auto canon = tssb->get_first_stage_variables();
 auto ucfs  = uc_design_vars( tssb->get_sub_Block( 0 ) );

 std::set< ColVariable * > a( canon.begin() , canon.end() );
 std::set< ColVariable * > b( ucfs.begin() , ucfs.end() );

 std::cout << "\ncanon (get_first_stage_variables) = " << canon.size()
           << "   (unique=" << a.size() << ")\n";
 std::cout << "ucfs  (uc_design_vars, cost-filtered) = " << ucfs.size()
           << "   (unique=" << b.size() << ")\n";
 std::cout << ( a == b ? "[MATCH]" : "[DIFFER]" ) << "\n";

 // how many of canon are NOT in ucfs (i.e. "dead" vars the generic path
 // includes but the cost-filtered extractor excludes)?
 int extra = 0;
 for( auto * v : a )
  if( ! b.count( v ) )
   extra++;
 std::cout << "canon entries absent from ucfs (\"dead\"/non-live vars) = "
           << extra << "\n";
 for( auto * v : a )
  if( ! b.count( v ) ) {
   if( ! v ) { std::cout << "  in canon, NOT in ucfs: ptr=NULL\n"; continue; }
   std::cout << "  in canon, NOT in ucfs: ptr=" << v
             << "  value=" << v->get_value()
             << "  is_fixed=" << v->is_fixed()
             << "  type=" << v->get_type() << "\n";
   }
 for( auto * v : b )
  if( ! a.count( v ) ) {
   if( ! v ) { std::cout << "  in ucfs, NOT in canon: ptr=NULL\n"; continue; }
   std::cout << "  in ucfs, NOT in canon: " << g_label[ v ]
             << "  ptr=" << v
             << "  value=" << v->get_value()
             << "  is_fixed=" << v->is_fixed()
             << "  type=" << v->get_type() << "\n";
   }
 int n_null_in_canon_raw = 0;
 for( auto * v : canon )
  if( ! v ) n_null_in_canon_raw++;
 std::cout << "NULL entries in raw canon list = " << n_null_in_canon_raw << "\n";

 // find duplicates in the raw canon vector (448 raw vs 438 unique => 10 dup)
 std::map< ColVariable * , int > count;
 for( auto * v : canon )
  count[ v ]++;
 int dup_groups = 0 , dup_total = 0;
 for( auto & [ v , c ] : count )
  if( c > 1 ) {
   dup_groups++;
   dup_total += c;
   if( dup_groups <= 15 )
    std::cout << "  DUP ptr=" << v << " appears " << c
              << " times in canon (value="
              << ( v ? std::to_string( v->get_value() ) : std::string( "NULL" ) )
              << ")\n";
   }
 std::cout << "duplicate groups in canon = " << dup_groups
           << "  (accounting for " << dup_total << " raw entries)\n";

 // Per-path resolution: which path index resolves to NULL / multi-element?
 Block * s0 = tssb->get_first_stage_block( 0 );
 const auto & paths = tssb->get_paths_to_static_here_and_now_vars();
 std::cout << "\n=== per-path resolution ( " << paths.size() << " paths ) ===\n";
 int pi = 0;
 for( const auto & p : paths ) {
  Index ne = 0;
  ColVariable * e0 = nullptr;
  try {
   ne = p->get_number_elements< ColVariable >( s0 );
   e0 = p->get_element< ColVariable >( s0 );
   } catch( std::exception & ex ) {
   std::cout << "  path[" << pi << "] THREW: " << ex.what() << "\n";
   pi++; continue;
   }
  if( e0 == nullptr || ne != 1 )
   std::cout << "  path[" << pi << "] num_elem=" << ne
             << " first_ptr=" << e0
             << ( e0 ? "" : "  <== NULL" ) << "\n";
  pi++;
  }

 // Locate the DesignNetworkBlock (the parent of the failing x_network paths)
 // and report the element_index each FAILING path requests vs how many design
 // lines actually exist. reference for get_last_node = that block, since the
 // last node's group name "x_network" is resolved against its parent block.
 DesignNetworkBlock * dnb = nullptr;
 {
  auto * uc = dynamic_cast< UCBlock * >( s0 );
  if( ! uc )
   if( auto * sb = dynamic_cast< StochasticBlock * >( s0 ) )
    uc = dynamic_cast< UCBlock * >( sb->get_inner_block() );
  if( uc )
   for( NetworkBlock * nb : uc->get_network_blocks() )
    if( auto * d = dynamic_cast< DesignNetworkBlock * >( nb ) ) { dnb = d; break; }
  }
 if( dnb ) {
  std::cout << "\n=== DesignNetworkBlock: x_network has "
            << dnb->get_design().size() << " design lines (valid idx 0.."
            << ( dnb->get_design().size() - 1 ) << ") ===\n";
  pi = 0;
  for( const auto & p : paths ) {
   ColVariable * e0 = nullptr;
   try { e0 = p->get_element< ColVariable >( s0 ); } catch( ... ) {}
   if( e0 == nullptr ) {
    try {
     auto node = p->get_last_node( dnb );
     std::cout << "  FAILING path[" << pi << "] last-node type='"
               << (char) node.type
               << "' element_index=" << node.element_index
               << " range_index=" << (long long) node.range_index << "\n";
     } catch( std::exception & ex ) {
     std::cout << "  FAILING path[" << pi << "] get_last_node threw: "
               << ex.what() << "\n";
     }
    }
   pi++;
   }
  }

 return 0;
}
