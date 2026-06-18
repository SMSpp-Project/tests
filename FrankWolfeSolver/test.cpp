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

#include "common_utils.h"

#include "AbstractBlock.h"

#include "FRealObjective.h"

#include "DQuadFunction.h"

#include "QuadFunction.h"

#include "PolyhedralFunction.h"

#include "PolyhedralFunctionBlock.h"

#include "ColVariable.h"

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
std::vector< std::string > var_groups;  // -V : named static-variable groups to
                        // build the father objective over (empty = whole
                        // sub-Block objective). Lets the test pick the "physical"
                        // variables of a sub-Block (e.g. ThermalUnitBlock's
                        // "p_thermal","u_thermal") and ignore the formulation's
                        // auxiliary objective variables.

std::mt19937 rg;
std::uniform_real_distribution<> dis( 0.0 , 1.0 );

// a random number in [-1,1]
static inline double rnd( void ) { return( 2 * dis( rg ) - 1 ); }

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

// collect the ColVariables that are *active in the child's Objective* into vars.
//
// FrankWolfeSolver requires every father-Objective variable to be active in
// some sub-Block Objective, so the father objective must be built over exactly
// those variables --- not over *all* the Block's ColVariables. For a Block
// whose objective spans every variable (e.g. MCFBlock) the two coincide; for a
// Block with variables outside its objective (e.g. ThermalUnitBlock: only
// active_power / commitment / start_up / ... are priced, the formulation's
// auxiliary variables are not) collecting from the objective is the correct,
// and only viable, choice. The child Objective must already be generated.

static void collect_vars( Block * b , std::vector< ColVariable * > & vars )
{
 // if explicit variable-group names were given (-V), build the father over
 // exactly those named static-variable groups (the "physical" variables a
 // generic LMO sets and prices); this keeps the father off any formulation
 // auxiliary objective variables (so the abstract->physical objective-change
 // translation of the sub-Block stays well-defined, see the scatter Subset
 // path in FrankWolfeSolver). Otherwise build it over the whole sub-Block
 // objective.
 if( ! var_groups.empty() ) {
  for( const auto & name : var_groups ) {
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

// build the random father objective Function over the given variables

static Function * make_father_objective( std::vector< ColVariable * > & vars )
{
 const Index nv = Index( vars.size() );

 if( obj_type == 0 ) {  // DQuadFunction: sum_i ( a_i x_i^2 + b_i x_i ), a_i > 0
  DQuadFunction::v_coeff_triple tr( nv );
  for( Index i = 0 ; i < nv ; ++i )
   tr[ i ] = std::make_tuple( vars[ i ] ,
                              Coefficient( obj_scale * rnd() ) ,        // b_i
                              Coefficient( obj_scale * ( 0.5 + dis( rg ) ) ) ); // a_i
  return( new DQuadFunction( std::move( tr ) ) );
  }

 if( obj_type == 1 ) {  // QuadFunction: add off-diagonal terms (i+1,i), kept
                        // PSD by Gershgorin diagonal dominance ( 2 a_i >= sum |q| )
  QuadFunction::v_off_diag_term nd;
  std::vector< double > rowabs( nv , 0.0 );
  for( Index i = 0 ; i + 1 < nv ; ++i ) {
   double q = obj_scale * 0.3 * rnd();
   nd.push_back( std::make_tuple( i + 1 , i , Coefficient( q ) ) );
   rowabs[ i ]     += std::abs( q );
   rowabs[ i + 1 ] += std::abs( q );
   }
  DQuadFunction::v_coeff_triple tr( nv );
  for( Index i = 0 ; i < nv ; ++i )
   tr[ i ] = std::make_tuple( vars[ i ] ,
                              Coefficient( obj_scale * rnd() ) ,
                              Coefficient( 0.5 * rowabs[ i ] +
                                           obj_scale * ( 0.5 + dis( rg ) ) ) );
  return( new QuadFunction( std::move( tr ) , std::move( nd ) ) );
  }

 // obj_type == 2: convex PolyhedralFunction = max_r ( A_r . x + b_r )
 Index nr = poly_rows > 0 ? Index( poly_rows ) : nv + 1;
 PolyhedralFunction::VarVector vv( vars.begin() , vars.end() );
 PolyhedralFunction::MultiVector A( nr , PolyhedralFunction::RealVector( nv ) );
 PolyhedralFunction::RealVector b( nr );
 for( Index r = 0 ; r < nr ; ++r ) {
  for( Index i = 0 ; i < nv ; ++i )
   A[ r ][ i ] = obj_scale * rnd();
  b[ r ] = obj_scale * nv * rnd() / 4;
  }
 auto pf = new PolyhedralFunction( std::move( vv ) , std::move( A ) ,
                                   std::move( b ) , - Inf< FunctionValue >() );
 pf->set_is_convex( true , eNoMod );
 return( pf );
 }

/*--------------------------------------------------------------------------*/

// build a father AbstractBlock with K copies of the input Block as sub-Blocks,
// collecting all their ColVariables; no objective is set

static AbstractBlock * build_mcf_father( std::vector< ColVariable * > & vars )
{
 auto father = new AbstractBlock();
 vars.clear();
 for( int k = 0 ; k < n_children ; ++k ) {
  Block * child = Block::deserialize( filename , father );
  if( ! child ) {
   cerr << "Error: cannot read Block from " << filename << endl;
   exit( 1 );
   }
  if( ! bconf_file.empty() ) {
   Configuration * bc = Configuration::deserialize( bconf_file );
   b_config_Block( child , bc , bconf_file );
   delete bc;
   }
  child->generate_abstract_variables();
  child->generate_abstract_constraints();
  child->generate_objective();
  father->add_nested_Block( child );
  collect_vars( child , vars );
  }
 return( father );
 }

/*--------------------------------------------------------------------------*/

// generate a random convex PolyhedralFunction data ( A , b ): max_r ( A_r.x + b_r )

static void generate_poly( Index nv , PolyhedralFunction::MultiVector & A ,
                           PolyhedralFunction::RealVector & b )
{
 Index nr = poly_rows > 0 ? Index( poly_rows ) : nv + 1;
 A.assign( nr , PolyhedralFunction::RealVector( nv ) );
 b.assign( nr , 0 );
 for( Index r = 0 ; r < nr ; ++r ) {
  for( Index i = 0 ; i < nv ; ++i )
   A[ r ][ i ] = obj_scale * rnd();
  b[ r ] = obj_scale * nv * rnd() / 4;
  }
 }

/*--------------------------------------------------------------------------*/

static bool process_specific_arg( int opt )
{
 switch( opt ) {
  case( 'k' ): Str2Sthg( optarg , n_children ); return( true );
  case( 'o' ): Str2Sthg( optarg , obj_type );   return( true );
  case( 'a' ): Str2Sthg( optarg , obj_scale );  return( true );
  case( 'e' ): Str2Sthg( optarg , seed );       return( true );
  case( 'r' ): Str2Sthg( optarg , poly_rows );  return( true );
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
 short_opts += "k:o:a:e:r:R:V:";
 const std::vector< option > my_opts = {
   { "children" , required_argument , nullptr , 'k' } ,
   { "objtype"  , required_argument , nullptr , 'o' } ,
   { "scale"    , required_argument , nullptr , 'a' } ,
   { "seed"     , required_argument , nullptr , 'e' } ,
   { "rows"     , required_argument , nullptr , 'r' } ,
   { "refconf"  , required_argument , nullptr , 'R' } ,
   { "vargroups", required_argument , nullptr , 'V' } };
 long_opts.insert( std::prev( long_opts.end() ) ,
                   my_opts.begin() , my_opts.end() );
 help += "  -k, --children <K>   number of sub-Block copies [2]\n"
         "  -o, --objtype <t>    father objective: 0 DQuad, 1 Quad, 2 Poly [0]\n"
         "  -a, --scale <s>      scale of the random father objective [1]\n"
         "  -e, --seed <n>       random seed [1]\n"
         "  -r, --rows <m>       PolyhedralFunction rows [nvar+1]\n"
         "  -R, --refconf <f>    reference (MILP) BlockSolverConfig, Poly test\n"
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
  auto father = build_mcf_father( vars );
  if( vars.empty() ) {
   cerr << "Error: the sub-Block have no ColVariable" << endl;
   exit( 1 );
   }

  auto obj = new FRealObjective( father , make_father_objective( vars ) );
  obj->set_sense( Objective::eMin , eNoMod );
  father->set_objective( obj );

  Configuration * bsc = Configuration::deserialize( sconf_file );
  if( ! bsc ) {
   cerr << "Error: cannot load BSC from " << sconf_file << endl;
   exit( 1 );
   }
  s_config_Block( father , bsc , sconf_file );
  if( father->get_registered_solvers().empty() ) {
   cout << endl << "no Solver registered to the father Block!" << endl;
   exit( 1 );
   }

  bool ok = SolveAll( father , exact_getter( ObjGetter::VarValue ) ,
                      std::numeric_limits< double >::quiet_NaN() , 1e-5 );
  cout << ( ok ? GREEN( All tests passed!! ) : RED( Shit happened!! ) ) << endl;

  s_config_Block( father , bsc );
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
 auto father1 = build_mcf_father( vars1 );
 auto father2 = build_mcf_father( vars2 );
 if( vars1.empty() || ( vars1.size() != vars2.size() ) ) {
  cerr << "Error: the two Block copies do not match" << endl;
  exit( 1 );
  }
 Index nv = Index( vars1.size() );

 PolyhedralFunction::MultiVector A;
 PolyhedralFunction::RealVector b;
 generate_poly( nv , A , b );
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
