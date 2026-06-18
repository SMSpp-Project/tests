/*--------------------------------------------------------------------------*/
/*------------------------ File test-mcf.cpp -------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * MCFBlock-specific tester for the FrankWolfeSolver Modification handling.
 *
 * Unlike the generic test.cpp (which only exercises *objective* Modification,
 * doable through the abstract FRealObjective and hence Block-agnostically),
 * this tester changes the *feasible region* of the sub-Block, which inherently
 * requires knowing the Block type: it builds K MCFBlock as the sub-Block of an
 * AbstractBlock father with a random DQuadFunction father objective, solves it
 * by both a FrankWolfeSolver and a :MILPSolver, then repeatedly changes the arc
 * capacities of a sub-Block (MCFBlock::chg_ucaps, borrowing the change logic of
 * tests/MCF_MILP) and re-solves, cross-checking the two optima. This exercises
 * the feasibility handling of FrankWolfeSolver (atoms that became infeasible
 * are dropped and the active set re-built / warm-started), see intHandleMod.
 *
 * \author Antonio Frangioni \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Antonio Frangioni
 */
/*--------------------------------------------------------------------------*/

#include "fw_test_common.h"   // collect_vars / build_father / make_father_objective

#include "MCFBlock.h"

#include <random>
#include <vector>

using namespace std;
using namespace SMSpp_di_unipi_it;

using Index = Block::Index;

int n_children = 2;     // -k
double obj_scale = 1.0; // -a
long seed = 1;          // -e
int mod_rounds = 5;     // -M : capacity-change rounds

std::mt19937 rg;

/*--------------------------------------------------------------------------*/

static bool process_specific_arg( int opt )
{
 switch( opt ) {
  case( 'k' ): Str2Sthg( optarg , n_children ); return( true );
  case( 'a' ): Str2Sthg( optarg , obj_scale );  return( true );
  case( 'e' ): Str2Sthg( optarg , seed );       return( true );
  case( 'M' ): Str2Sthg( optarg , mod_rounds ); return( true );
  default:                                       return( false );
  }
 }

/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 std::set_terminate( smspp_terminate );

 docopt_desc = "SMS++ FrankWolfeSolver MCF Modification test.\n";
 short_opts += "k:a:e:M:";
 const std::vector< option > my_opts = {
   { "children"  , required_argument , nullptr , 'k' } ,
   { "scale"     , required_argument , nullptr , 'a' } ,
   { "seed"      , required_argument , nullptr , 'e' } ,
   { "modrounds" , required_argument , nullptr , 'M' } };
 long_opts.insert( std::prev( long_opts.end() ) ,
                   my_opts.begin() , my_opts.end() );
 help += "  -k, --children <K>   number of MCFBlock sub-Block copies [2]\n"
         "  -a, --scale <s>      scale of the random father objective [1]\n"
         "  -e, --seed <n>       random seed [1]\n"
         "  -M, --modrounds <n>  random cost/capacity Modification rounds [5]\n";

 process_args( argc , argv , process_specific_arg );

 if( filename.empty() ) {
  cerr << "Error: no MCFBlock instance file given" << endl;
  exit( 1 );
  }
 require_solver_config();
 rg.seed( seed );

 // build the father: K MCFBlock copies (shared scaffolding) + a random
 // DQuadFunction objective over all their (flow) ColVariables
 std::vector< ColVariable * > vars;
 auto father = fwtest::build_father( filename , n_children , "" , {} , vars );
 if( vars.empty() ) {
  cerr << "Error: the sub-Block have no ColVariable" << endl;
  exit( 1 );
  }

 // the sub-Block must be MCFBlock: we change their arc capacities natively
 std::vector< MCFBlock * > children;
 for( auto b : father->get_nested_Blocks() ) {
  auto m = dynamic_cast< MCFBlock * >( b );
  if( ! m ) {
   cerr << "Error: a sub-Block is not an MCFBlock" << endl;
   exit( 1 );
   }
  children.push_back( m );
  }

 auto obj = new FRealObjective( father ,
            fwtest::make_father_objective( vars , 0 , obj_scale , 0 , rg ) );
 obj->set_sense( Objective::eMin , eNoMod );
 father->set_objective( obj );

 Configuration * bsc = Configuration::deserialize( sconf_file );
 if( ! bsc ) {
  cerr << "Error: cannot load BSC from " << sconf_file << endl;
  exit( 1 );
  }
 s_config_Block( father , bsc , sconf_file );
 if( father->get_registered_solvers().empty() ) {
  cerr << "no Solver registered to the father Block!" << endl;
  exit( 1 );
  }

 bool ok = SolveAll( father , exact_getter( ObjGetter::VarValue ) ,
                     std::numeric_limits< double >::quiet_NaN() , 1e-5 );

 // randomized Modification rounds (in the style of tests/MCF_MILP): each round
 // randomly changes the arc costs (an *objective* change -> sub-Block-objective
 // Modification) and/or the arc capacities (a *feasible-region* change), of the
 // first sub-Block, always perturbing from the *original* values (not
 // compounding, which would drift towards infeasibility). This exercises both
 // the objective handling (c0 re-snapshot / atom-cost recompute) and the
 // feasibility handling (infeasible-atom drop) of FrankWolfeSolver.
 //
 // Every chg_* is issued with BOTH the physical and the abstract Modification
 // (eModBlck, eModBlck): the physical one notifies the native LMO (MCFSolver),
 // the abstract one the monolithic :MILPSolver and FrankWolfeSolver. The chg_*
 // default (eNoBlck) updates the data but issues no Modification, leaving the
 // solvers' cached formulations stale. Capacities are kept clean integers >= 1
 // (MCFSimplex is sensitive to many-digit capacities, see tests/MCF_MILP).
 auto mcf = children.front();
 Index na = mcf->get_NStaticArcs();
 MCFBlock::Vec_FNumber orig_caps( na );
 MCFBlock::Vec_CNumber orig_costs( na );
 for( Index a = 0 ; a < na ; ++a ) {
  orig_caps[ a ]  = mcf->get_U( a );
  orig_costs[ a ] = mcf->get_C( a );
  }

 // the first sub-Block's flow ColVariables, in arc order, are the first na
 // entries collected into vars; used to close/reopen arcs (variable fixing)
 std::vector< bool > closed( na , false );

 for( int r = 0 ; ( r < mod_rounds ) && ok ; ++r ) {
  int what = int( 4 * fwtest::pos( rg ) );  // 0 cost, 1 cap, 2 both, 3 fix-toggle
  const char * desc = what == 0 ? "cost" : ( what == 1 ? "cap" :
                      ( what == 2 ? "cost+cap" : "fix" ) );

  if( ( what == 0 ) || ( what == 2 ) ) {           // change costs (objective)
   MCFBlock::Vec_CNumber costs( na );
   for( Index a = 0 ; a < na ; ++a )
    costs[ a ] = std::round( double( orig_costs[ a ] ) *
                             ( 0.7 + 0.6 * fwtest::pos( rg ) ) );
   mcf->chg_costs( costs.begin() , MCFBlock::Range( 0 , na ) , eModBlck ,
                   eModBlck );
   }
  if( ( what == 1 ) || ( what == 2 ) ) {           // change caps (feasibility)
   MCFBlock::Vec_FNumber caps( na );
   for( Index a = 0 ; a < na ; ++a )
    caps[ a ] = MCFBlock::FNumber( std::max( 1.0 ,
                   std::round( double( orig_caps[ a ] ) *
                               ( 0.85 + 0.45 * fwtest::pos( rg ) ) ) ) );
   mcf->chg_ucaps( caps.begin() , MCFBlock::Range( 0 , na ) , eModBlck ,
                   eModBlck );
   }
  if( what == 3 ) {                                // toggle one arc's fix status
   Index a = Index( na * fwtest::pos( rg ) ) % na; // (a VariableMod: closing an
   if( ! closed[ a ] ) {                           //  arc shrinks the region,
    vars[ a ]->set_value( 0 );                     //  reopening relaxes it)
    vars[ a ]->is_fixed( true );
    closed[ a ] = true;
    }
   else {
    vars[ a ]->is_fixed( false );
    closed[ a ] = false;
    }
   }

  bool okr = SolveAll( father , exact_getter( ObjGetter::VarValue ) ,
                       std::numeric_limits< double >::quiet_NaN() , 1e-5 );
  cout << "  Round " << ( r + 1 ) << " (" << desc << "): "
       << ( okr ? "ok" : "MISMATCH" ) << endl;
  ok = ok && okr;
  }

 cout << ( ok ? GREEN( All tests passed!! ) : RED( Shit happened!! ) ) << endl;

 s_config_Block( father , bsc );
 delete bsc;
 delete father;
 return( ok ? 0 : 1 );
 }

/*--------------------------------------------------------------------------*/
/*---------------------- End File test-mcf.cpp -----------------------------*/
/*--------------------------------------------------------------------------*/
