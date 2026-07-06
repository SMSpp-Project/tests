/*--------------------------------------------------------------------------*/
/*-------------------------- File test.cpp ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Generic test for FrankWolfeSolver.
 *
 * A "leaf" Block is read K times from a (netCDF) file given on the command
 * line; the K copies become the sub-Blocks of a father AbstractBlock. A random
 * father FRealObjective is then built over ALL the ColVariables of the K
 * sub-Blocks; its Function can be a DQuadFunction, a QuadFunction or a
 * PolyhedralFunction (option -o). The father Block is solved by every :Solver
 * registered to it via the BlockSolverConfig read from file (-S), typically a
 * :MILPSolver (monolithic) and a FrankWolfeSolver (decomposition), and the
 * results are cross-checked by SolveAll().
 *
 * Assumption (v1): each loaded Block is a "leaf" (no sub-Blocks), or at least
 * the father objective only depends on the variables of the *direct* children
 * (see frank-wolfe-design.md, 7.bis).
 *
 * \author Antonio Frangioni \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Antonio Frangioni
 */
/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include "fw_test_common.h"   // collect_vars / build_father / make_father_objective

#include "PolyhedralFunctionBlock.h"   // for the two-block Polyhedral path

#include <random>
#include <sstream>
#include <vector>

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace std;
using namespace SMSpp_di_unipi_it;

using Index = Block::Index;
using FunctionValue = Function::FunctionValue;
using Coefficient = DQuadFunction::Coefficient;

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/

// test-specific command-line options (set by process_specific_arg)
int n_children = 2;     // -k : number of sub-Block (copies of the input Block)
int obj_type = 0;       // -o : 0 = DQuadFunction, 1 = QuadFunction, 2 = Polyhedral
double obj_scale = 1.0; // -a : scale of the (random) father objective coefficients
long seed = 1;          // -e : random seed
int poly_rows = 0;      // -r : number of rows of the PolyhedralFunction (0 = nvar+1)
std::string refconf;    // -R : BlockSolverConfig for the reference (Poly test)
int mod_rounds = 0;     // -M : extra solve rounds, each perturbing the father
                        // (and a sub-Block) objective to exercise Modification
                        // handling (re-snapshot / re-cache on re-solve)
std::vector< std::string > var_groups;  // -V : named static-variable groups to
                        // build the father objective over (empty = whole
                        // sub-Block objective). Lets the test pick the "physical"
                        // variables of a sub-Block (e.g. ThermalUnitBlock's
                        // "p_thermal","u_thermal") and ignore the formulation's
                        // auxiliary objective variables.

std::mt19937 rg;

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/
// the father-building / objective scaffolding lives in fw_test_common.h
// (namespace fwtest), shared with test-mcf.cpp.

static bool process_specific_arg( int opt )
{
 switch( opt ) {
  case( 'k' ): Str2Sthg( optarg , n_children ); return( true );
  case( 'o' ): Str2Sthg( optarg , obj_type );   return( true );
  case( 'a' ): Str2Sthg( optarg , obj_scale );  return( true );
  case( 'e' ): Str2Sthg( optarg , seed );       return( true );
  case( 'r' ): Str2Sthg( optarg , poly_rows );  return( true );
  case( 'M' ): Str2Sthg( optarg , mod_rounds ); return( true );
  case( 'R' ): refconf = std::string( optarg ); return( true );
  case( 'V' ): {                                 // comma-separated group names
   std::string s( optarg ) , tok;
   std::stringstream ss( s );
   while( std::getline( ss , tok , ',' ) )
    if( ! tok.empty() )
     var_groups.push_back( tok );
   return( true );
   }
  default:                                       return( false );
  }
 }

/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 std::set_terminate( smspp_terminate );

 docopt_desc = "SMS++ FrankWolfeSolver generic test.\n";
 short_opts += "k:o:a:e:r:R:V:M:";
 const std::vector< option > my_opts = {
   { "children" , required_argument , nullptr , 'k' } ,
   { "objtype"  , required_argument , nullptr , 'o' } ,
   { "scale"    , required_argument , nullptr , 'a' } ,
   { "seed"     , required_argument , nullptr , 'e' } ,
   { "rows"     , required_argument , nullptr , 'r' } ,
   { "refconf"  , required_argument , nullptr , 'R' } ,
   { "modrounds", required_argument , nullptr , 'M' } ,
   { "vargroups", required_argument , nullptr , 'V' } };
 long_opts.insert( std::prev( long_opts.end() ) ,
                   my_opts.begin() , my_opts.end() );
 help += "  -k, --children <K>   number of sub-Block copies [2]\n"
         "  -o, --objtype <t>    father objective: 0 DQuad, 1 Quad, 2 Poly [0]\n"
         "  -a, --scale <s>      scale of the random father objective [1]\n"
         "  -e, --seed <n>       random seed [1]\n"
         "  -r, --rows <m>       PolyhedralFunction rows [nvar+1]\n"
         "  -R, --refconf <f>    reference (MILP) BlockSolverConfig, Poly test\n"
         "  -M, --modrounds <n>  extra re-solve rounds, each perturbing the\n"
         "                       father and a sub-Block objective (tests the\n"
         "                       Modification handling) [0]\n"
         "  -V, --vargroups <l>  comma-separated names of the sub-Block static\n"
         "                       variable groups to build the father over\n"
         "                       (default: the whole sub-Block objective)\n";

 process_args( argc , argv , process_specific_arg );

 if( filename.empty() ) {
  cerr << "Error: no Block instance file given" << endl;
  exit( 1 );
  }
 require_solver_config();

 rg.seed( seed );

 // ----- single-block path ( DQuad / Quad ): both Solver via -S, SolveAll -----

 if( obj_type != 2 ) {
  std::vector< ColVariable * > vars;
  auto father = fwtest::build_father( filename , n_children , bconf_file , var_groups , vars );
  if( vars.empty() ) {
   cerr << "Error: the sub-Block have no ColVariable" << endl;
   exit( 1 );
   }

  auto obj = new FRealObjective( father ,
            fwtest::make_father_objective( vars , obj_type , obj_scale , poly_rows , rg ) );
  obj->set_sense( Objective::eMin , eNoMod );
  father->set_objective( obj );

  Configuration * bsc = Configuration::deserialize( sconf_file );
  if( ! bsc ) {
   cerr << "Error: cannot load BSC from " << sconf_file << endl;
   exit( 1 );
   }
  s_config_Block( father , bsc , sconf_file );
  // optional reference solver(s) via -R, registered *additively* so SolveAll
  // cross-checks them against the solver(s) under test in -S. Without -R the
  // single solver in -S runs alone: this is how FrankWolfeSolver is profiled
  // without the (possibly very slow) reference solve. The solver log can be set
  // straight from the ComputeConfig via the standard strLogFileName parameter.
  Configuration * rbsc = nullptr;
  if( ! refconf.empty() ) {
   rbsc = Configuration::deserialize( refconf );
   if( ! rbsc ) {
    cerr << "Error: cannot load reference BSC from " << refconf << endl;
    exit( 1 );
    }
   s_config_Block( father , rbsc , refconf );
   }
  if( father->get_registered_solvers().empty() ) {
   cout << endl << "no Solver registered to the father Block!" << endl;
   exit( 1 );
   }
  fwtest::apply_solver_verbosity( father );  // -v drives Solver::intLogVerb

  bool ok = SolveAll( father , exact_getter( ObjGetter::VarValue ) ,
                      std::numeric_limits< double >::quiet_NaN() , 1e-5 );

  // Modification rounds: perturb the father objective (linear coefficients)
  // and the first sub-Block objective, then re-solve. This exercises the
  // FrankWolfeSolver Modification handling: the cross-check stays valid only
  // if FrankWolfeSolver re-caches the father quadratic structure and
  // re-snapshots the sub-Block c0 (rather than reusing stale information).
  auto perturb = [ & ]( Function * f ) {
   if( ! f )
    return;
   Index n = f->get_num_active_var();
   Function::Vec_FunctionValue nc( n );
   for( Index i = 0 ; i < n ; ++i )
    nc[ i ] = obj_scale * fwtest::rnd( rg );
   if( auto dq = dynamic_cast< DQuadFunction * >( f ) )
    dq->modify_linear_coefficients( std::move( nc ) , Function::Range( 0 , n ) );
   else if( auto lf = dynamic_cast< LinearFunction * >( f ) )
    lf->modify_coefficients( std::move( nc ) , Function::Range( 0 , n ) );
   };

  for( int r = 0 ; ( r < mod_rounds ) && ok ; ++r ) {
   perturb( obj->get_function() );                     // father objective
   // perturb the first sub-Block objective only when the father is built over
   // the whole sub-Block objective (var_groups empty): otherwise the sub-Block
   // objective carries formulation-auxiliary variables (e.g. ThermalUnitBlock's
   // perspective cuts) that a full-range coefficient change would not be able
   // to bridge to the physical data
   if( var_groups.empty() ) {
    auto & sb = father->get_nested_Blocks();
    if( auto fo = dynamic_cast< FRealObjective * >(     // first sub-Block obj
                                       sb.front()->get_objective() ) )
     perturb( fo->get_function() );
    }
   bool okr = SolveAll( father , exact_getter( ObjGetter::VarValue ) ,
                        std::numeric_limits< double >::quiet_NaN() , 1e-5 );
   cout << "  Modification round " << ( r + 1 ) << ": "
        << ( okr ? "ok" : "MISMATCH" ) << endl;
   ok = ok && okr;
   }

  cout << ( ok ? GREEN( All tests passed!! ) : RED( Shit happened!! ) ) << endl;

  s_config_Block( father , bsc );
  if( rbsc ) { s_config_Block( father , rbsc ); delete rbsc; }
  delete bsc;
  delete father;
  return( ok ? 0 : 1 );
  }

 // ----- two-block path ( Polyhedral ): FW on copy1 vs MILP on copy2 ----------
 // copy1 has the PolyhedralFunction as its father Objective (for FW); copy2 has
 // the *same* PolyhedralFunction inside a (linearized) PolyhedralFunctionBlock,
 // solved monolithically by a :MILPSolver. F-W on a nonsmooth objective has no
 // global-convergence guarantee, so we check that its bracket [ lb , ub ] is
 // valid (contains the true optimum); equality is a bonus.

 if( refconf.empty() ) {
  cerr << "Error: the Polyhedral test needs the reference config (-R)" << endl;
  exit( 1 );
  }

 std::vector< ColVariable * > vars1 , vars2;
 auto father1 = fwtest::build_father( filename , n_children , bconf_file , var_groups , vars1 );
 auto father2 = fwtest::build_father( filename , n_children , bconf_file , var_groups , vars2 );
 if( vars1.empty() || ( vars1.size() != vars2.size() ) ) {
  cerr << "Error: the two Block copies do not match" << endl;
  exit( 1 );
  }
 Index nv = Index( vars1.size() );

 PolyhedralFunction::MultiVector A;
 PolyhedralFunction::RealVector b;
 fwtest::generate_poly( nv , poly_rows , obj_scale , rg , A , b );
 const FunctionValue NEGINF = - Inf< FunctionValue >();

 // copy1: the PolyhedralFunction as the father Objective
 {
  PolyhedralFunction::VarVector vv( vars1.begin() , vars1.end() );
  auto pf = new PolyhedralFunction( std::move( vv ) ,
                                    PolyhedralFunction::MultiVector( A ) ,
                                    PolyhedralFunction::RealVector( b ) , NEGINF );
  pf->set_is_convex( true , eNoMod );
  auto obj = new FRealObjective( father1 , pf );
  obj->set_sense( Objective::eMin , eNoMod );
  father1->set_objective( obj );
 }

 // copy2: the same PolyhedralFunction inside a (linearized) PolyhedralFunctionBlock
 {
  auto pfb = new PolyhedralFunctionBlock( father2 );
  auto & pf = pfb->get_PolyhedralFunction();
  pf.set_variables( PolyhedralFunction::VarVector( vars2.begin() , vars2.end() ) );
  PolyhedralFunction::BoolVector iV( A.size() , false );
  pf.set_PolyhedralFunction( PolyhedralFunction::MultiVector( A ) ,
                             PolyhedralFunction::RealVector( b ) , NEGINF , true ,
                             eNoMod , std::move( iV ) );
  auto bc = new BlockConfig();   // 1 = "linearized primal" ( MILP-digestible )
  bc->f_static_variables_Configuration = new SimpleConfiguration< int >( 1 );
  pfb->set_BlockConfig( bc );
  pfb->generate_abstract_variables();
  pfb->generate_abstract_constraints();
  pfb->generate_objective();
  father2->add_nested_Block( pfb );
 }

 // register FrankWolfeSolver on copy1 (-S), the :MILPSolver on copy2 (-R)
 Configuration * bsc1 = Configuration::deserialize( sconf_file );
 Configuration * bsc2 = Configuration::deserialize( refconf );
 if( ( ! bsc1 ) || ( ! bsc2 ) ) {
  cerr << "Error: cannot load the BlockSolverConfig(s)" << endl;
  exit( 1 );
  }
 s_config_Block( father1 , bsc1 , sconf_file );
 s_config_Block( father2 , bsc2 , refconf );
 if( father1->get_registered_solvers().empty() ||
     father2->get_registered_solvers().empty() ) {
  cerr << "Error: no Solver registered" << endl;
  exit( 1 );
  }
 fwtest::apply_solver_verbosity( father1 );  // -v drives Solver::intLogVerb
 fwtest::apply_solver_verbosity( father2 );

 Solver * fwslv = father1->get_registered_solvers().front();
 Solver * mlslv = father2->get_registered_solvers().front();

 fwslv->compute( false );
 mlslv->compute( false );
 double fw_val   = fwslv->get_var_value();
 double fw_lb    = fwslv->get_lb();
 double milp_val = mlslv->get_var_value();   // the true optimum

 double tol = 1e-5 * std::max( 1.0 , std::abs( milp_val ) );
 bool converged  = std::abs( fw_val - milp_val ) <= tol;
 bool bracket_ok = ( fw_lb <= milp_val + tol ) && ( milp_val <= fw_val + tol );

 cout << def << "FW val=" << fw_val << " lb=" << fw_lb
      << "  MILP(opt)=" << milp_val << "  -> "
      << ( converged ? "CONVERGED"
                     : ( bracket_ok ? "bracket-OK (not converged)" : "KO" ) )
      << endl;
 cout << ( bracket_ok ? GREEN( All tests passed!! ) : RED( Shit happened!! ) )
      << endl;

 s_config_Block( father1 , bsc1 );
 s_config_Block( father2 , bsc2 );
 delete bsc1; delete bsc2; delete father1; delete father2;
 return( bracket_ok ? 0 : 1 );
 }

/*--------------------------------------------------------------------------*/
/*------------------------ End File test.cpp -------------------------------*/
/*--------------------------------------------------------------------------*/
