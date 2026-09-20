/*--------------------------------------------------------------------------*/
/*---------------------------- File test_bds.cpp ---------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * The forms of one two-stage stochastic investment problem, one against the
 * other on the same data.
 *
 * On the instances the generator writes there are three of them: the
 * extensive form, given to a :MILPSolver, which is the reference; the ad hoc
 * Benders one, an InvestmentBlock whose inner Block is the whole
 * TwoStageStochasticBlock, its InvestmentFunction being the value function
 * and FIXING the design in every scenario rather than mapping it into the
 * right-hand side, so that it produces one aggregated linearization per
 * iteration; and the generic one, i.e., the structure
 * BendersDecompositionSolver asks for, with the here-and-now Variable in a
 * single copy in the root and one LP sub-Block per scenario coupled to them
 * by its capacity Constraint alone.
 *
 * On an instance that is on file, which carries the design replicated in the
 * scenarios and tied by the non-anticipativity Constraint, that structure is
 * assembled around the very Block the file gives [see benders_form()], and
 * whatever Solver a BlockSolverConfig names are cross-checked on it.
 *
 * Being formulations of one problem, the optima have to coincide, which is
 * what is checked; the iterations and the times are reported.
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

#include <chrono>
#include <cmath>
#include <iostream>
#include <netcdf>

#include "AbstractBlock.h"
#include "AbstractPath.h"
#include "BendersDecompositionSolver.h"
#include "BlockSolverConfig.h"
#include "ColVariable.h"
#include "FRealObjective.h"
#include "FRowConstraint.h"
#include "InvestmentBlock.h"
#include "InvestmentFunction.h"
#include "LinearFunction.h"
#include "Objective.h"
#include "OneVarConstraint.h"
#include "TwoStageStochasticBlock.h"

#include "common_utils.h"

using namespace SMSpp_di_unipi_it;
using namespace netCDF;

/*--------------------------------------------------------------------------*/
/*----------------------------- CONSTANTS ----------------------------------*/
/*--------------------------------------------------------------------------*/

// the relative gap at which two of these solves are the same number: the ad
// hoc form reads its objective back through a bundle whose required relative
// accuracy is 1e-4, hence nothing tighter can be asked of the comparison
static constexpr double tolerance = 1e-6;

/* The instance, the one in the repository unless another is named on the
 * command line: gen_investment.py writes them of whatever size. */

static std::string instance = "tssb_investment.nc4";

/*--------------------------------------------------------------------------*/
/*------------------------------- TYPES ------------------------------------*/
/*--------------------------------------------------------------------------*/

/// the instance, read once and used to build every form of it
/** Whatever the form, the data is the same, and it is read from the very file
 * the ad hoc form is deserialized from, so that the two cannot drift apart. */

struct Data {
 double cost;                       ///< cost of one unit of design
 double lb;                         ///< lower bound on the design
 double ub;                         ///< upper bound on the design
 std::vector< double > weight;      ///< one probability per scenario
 std::vector< std::vector< double > > demand;   ///< [ scenario ][ time ]
 std::vector< double > unit_max;    ///< max power, per unit of design
 std::vector< double > unit_cost;   ///< cost of the design-carrying unit
 std::vector< double > slack_max;   ///< max power of the slack
 std::vector< double > slack_cost;  ///< cost of the slack
 };

/*--------------------------------------------------------------------------*/
/*----------------------------- READING ------------------------------------*/
/*--------------------------------------------------------------------------*/

static std::vector< double > get_vec( const NcGroup & g ,
				      const std::string & name )
{
 auto v = g.getVar( name );
 std::size_t n = 1;
 for( int i = 0 ; i < v.getDimCount() ; ++i )
  n *= v.getDim( i ).getSize();
 std::vector< double > out( n );
 v.getVar( out.data() );
 return( out );
 }

/*--------------------------------------------------------------------------*/

static Data read( const std::string & file )
{
 Data d;
 NcFile f( file , NcFile::read );

 auto inv = f.getGroup( "Block_0" );
 d.cost = get_vec( inv , "Cost" ).front();
 d.lb = get_vec( inv , "LowerBound" ).front();
 d.ub = get_vec( inv , "UpperBound" ).front();

 auto tssb = inv.getGroup( "InnerBlock" );
 auto set = tssb.getGroup( "DiscreteScenarioSet" );
 d.weight = get_vec( set , "PoolWeights" );

 auto sc = set.getVar( "Scenarios" );
 const auto K = sc.getDim( 0 ).getSize() , T = sc.getDim( 1 ).getSize();
 auto flat = get_vec( set , "Scenarios" );
 d.demand.resize( K );
 for( std::size_t k = 0 ; k < K ; ++k )
  d.demand[ k ].assign( flat.begin() + k * T , flat.begin() + ( k + 1 ) * T );

 auto uc = tssb.getGroup( "StochasticBlock" ).getGroup( "Block" );
 auto u0 = uc.getGroup( "UnitBlock_0" ) , u1 = uc.getGroup( "UnitBlock_1" );
 d.unit_max = get_vec( u0 , "MaxPower" );
 d.unit_cost = get_vec( u0 , "ActivePowerCost" );
 d.slack_max = get_vec( u1 , "MaxPower" );
 d.slack_cost = get_vec( u1 , "ActivePowerCost" );

 return( d );
 }

/*--------------------------------------------------------------------------*/
/*--------------------------- BLOCK BUILDERS -------------------------------*/
/*--------------------------------------------------------------------------*/

/* BundleSolver does not take a general bound l <= x <= u on the Variable of
 * the Block it is attached to, so the design is shifted: the Variable is
 * x' = x - l in [ 0 , u - l ], and l is added back wherever x appears, which
 * leaves a constant cost * l in the Objective. The ad hoc form does the very
 * same thing, through the BlockConfig of the InvestmentBlock, so the two keep
 * describing one problem. */

static std::vector< ColVariable > * make_x( const Data & d )
{
 auto x = new std::vector< ColVariable >( 1 );
 ( * x )[ 0 ].is_positive( true );
 ( * x )[ 0 ].set_value( 0 );
 return( x );
 }

/*--------------------------------------------------------------------------*/

// the box on the shifted design, the only Constraint of the master

static void add_design_bound( AbstractBlock * block , const Data & d ,
			      std::vector< ColVariable > * x )
{
 auto bnd = new std::vector< BoxConstraint >( 1 );
 ( * bnd )[ 0 ].set_variable( & ( * x )[ 0 ] );
 ( * bnd )[ 0 ].set_lhs( 0 );
 ( * bnd )[ 0 ].set_rhs( d.ub - d.lb );
 block->add_static_constraint( * bnd , "design bound" );
 }

/*--------------------------------------------------------------------------*/

/* add to block the dispatch of one scenario: the power of the design-carrying
 * unit and of the slack, the demand Constraint and the capacity Constraint,
 * the latter being the only place where the design Variable appears, and the
 * costs of the scenario, weighted by its probability, to obj */

static void add_scenario( AbstractBlock * block , const Data & d ,
			  std::size_t k , std::vector< ColVariable > * x ,
			  LinearFunction * obj )
{
 const auto T = d.demand[ k ].size();

 auto g = new std::vector< ColVariable >( T );
 for( auto & v : * g ) v.is_positive( true );
 block->add_static_variable( * g , "g" );

 auto s = new std::vector< ColVariable >( T );
 for( auto & v : * s ) v.is_positive( true );
 block->add_static_variable( * s , "s" );

 auto dem = new std::vector< FRowConstraint >( T );
 for( std::size_t t = 0 ; t < T ; ++t ) {
  auto f = new LinearFunction();
  f->add_variable( & ( * g )[ t ] , 1 );
  f->add_variable( & ( * s )[ t ] , 1 );
  ( * dem )[ t ].set_function( f );
  ( * dem )[ t ].set_both( d.demand[ k ][ t ] );
  }
 block->add_static_constraint( * dem , "demand" );

 auto cap = new std::vector< FRowConstraint >( T );
 for( std::size_t t = 0 ; t < T ; ++t ) {
  auto f = new LinearFunction();
  f->add_variable( & ( * g )[ t ] , 1 );
  f->add_variable( & ( * x )[ 0 ] , - d.unit_max[ t ] );
  ( * cap )[ t ].set_function( f );
  ( * cap )[ t ].set_lhs( -Inf< double >() );
  ( * cap )[ t ].set_rhs( d.unit_max[ t ] * d.lb );
  }
 block->add_static_constraint( * cap , "capacity" );

 auto slk = new std::vector< FRowConstraint >( T );
 for( std::size_t t = 0 ; t < T ; ++t ) {
  auto f = new LinearFunction();
  f->add_variable( & ( * s )[ t ] , 1 );
  ( * slk )[ t ].set_function( f );
  ( * slk )[ t ].set_lhs( -Inf< double >() );
  ( * slk )[ t ].set_rhs( d.slack_max[ t ] );
  }
 block->add_static_constraint( * slk , "slack capacity" );

 for( std::size_t t = 0 ; t < T ; ++t ) {
  obj->add_variable( & ( * g )[ t ] , d.weight[ k ] * d.unit_cost[ t ] );
  obj->add_variable( & ( * s )[ t ] , d.weight[ k ] * d.slack_cost[ t ] );
  }
 }

/*--------------------------------------------------------------------------*/

// the monolithic model, every scenario in one Block: the reference optimum

static AbstractBlock * build_monolithic( const Data & d )
{
 auto block = new AbstractBlock();
 auto x = make_x( d );
 block->add_static_variable( * x , "x" );
 add_design_bound( block , d , x );

 auto f = new LinearFunction();
 f->add_variable( & ( * x )[ 0 ] , d.cost );
 f->set_constant_term( d.cost * d.lb );
 for( std::size_t k = 0 ; k < d.demand.size() ; ++k )
  add_scenario( block , d , k , x , f );

 auto obj = new FRealObjective( block , f );
 obj->set_sense( Objective::eMin );
 block->set_objective( obj );
 return( block );
 }

/*--------------------------------------------------------------------------*/

// the 2-level model: the design in the root, one LP sub-Block per scenario

static AbstractBlock * build_structured( const Data & d )
{
 auto root = new AbstractBlock();
 auto x = make_x( d );
 root->add_static_variable( * x , "x" );
 add_design_bound( root , d , x );

 auto df = new LinearFunction();
 df->add_variable( & ( * x )[ 0 ] , d.cost );
 df->set_constant_term( d.cost * d.lb );
 auto robj = new FRealObjective( root , df );
 robj->set_sense( Objective::eMin );
 root->set_objective( robj );

 for( std::size_t k = 0 ; k < d.demand.size() ; ++k ) {
  auto sub = new AbstractBlock( root );
  auto sf = new LinearFunction();
  add_scenario( sub , d , k , x , sf );
  auto sobj = new FRealObjective( sub , sf );
  sobj->set_sense( Objective::eMin );
  sub->set_objective( sobj );
  root->add_nested_Block( sub );
  }

 return( root );
 }

/*--------------------------------------------------------------------------*/
/*------------------------------ SOLVING -----------------------------------*/
/*--------------------------------------------------------------------------*/

static double solve( Block * block , const std::string & cfg , double & secs ,
		     long & iters , int & status )
{
 auto c = Configuration::deserialize( cfg );
 auto bsc = dynamic_cast< BlockSolverConfig * >( c );
 if( ! bsc ) {
  std::cerr << cfg << " is not a BlockSolverConfig" << std::endl;
  std::exit( 1 );
  }
 /* Whatever goes wrong, from the configuration being refused onwards, the
  * Block is given back what the Solver had taken from it before the error
  * travels on, so that the caller can go on using the Block. */

 auto give_back = [ & ]() { bsc->clear(); bsc->apply( block ); delete bsc; };

 double value;
 try {
  bsc->apply( block );

  auto solver = block->get_registered_solvers().front();

  auto t0 = std::chrono::steady_clock::now();
  status = solver->compute();
  secs = std::chrono::duration< double >(
                              std::chrono::steady_clock::now() - t0 ).count();
  iters = solver->get_elapsed_iterations();
  value = solver->get_var_value();
  }
 catch( ... ) { give_back(); throw; }

 give_back();
 return( value );
 }

/*--------------------------------------------------------------------------*/

/* Both Benders forms drive their master with a BundleSolver, and the master
 * keeps the subproblems hanging from it as sub-Block; a bundle that wants
 * each of its sub-Block to be a bare function Block refuses it in
 * set_Block(), and so does one that does not carry the required parameters.
 * In either case the form is skipped rather than failed. */

static double solve_by_bundle( Block * block , const std::string & cfg ,
			       double & secs , long & iters , int & status ,
			       bool & ran , double dflt )
{
 try {
  const auto value = solve( block , cfg , secs , iters , status );
  ran = true;
  return( value );
  }
 catch( const std::exception & e ) {
  std::cout << cfg << ": skipped, " << e.what() << std::endl;
  ran = false;
  return( dflt );
  }
 }

/*--------------------------------------------------------------------------*/

/* the ad hoc form: the InvestmentBlock is deserialized from the file, its
 * bound Constraint are reformulated into the shifted box that a BundleSolver
 * takes, and the BlockSolverConfig of the inner Block travels to the
 * InvestmentFunction as the "extra" Configuration it expects */

static double solve_ad_hoc( double & secs , long & iters , int & status ,
			    bool & ran , double dflt )
{
 auto root = Block::deserialize( instance );
 auto inv = dynamic_cast< InvestmentBlock * >( root );
 if( ! inv ) {
  std::cerr << instance << " does not hold an InvestmentBlock" << std::endl;
  std::exit( 1 );
  }

 auto config = new BlockConfig;
 config->f_static_constraints_Configuration =
                                        new SimpleConfiguration< int >( 1 );
 inv->set_BlockConfig( config );

 auto inner = dynamic_cast< BlockSolverConfig * >(
                       Configuration::deserialize( "InvBCfg.txt" ) );
 if( ! inner ) {
  std::cerr << "InvBCfg.txt is not a BlockSolverConfig" << std::endl;
  std::exit( 1 );
  }

 ComputeConfig cc;
 cc.f_extra_Configuration =
  new SimpleConfiguration< std::map< std::string , Configuration * > >
                                       ( { { "BlockSolverConfig" , inner } } );

 auto function = dynamic_cast< InvestmentFunction * >( inv->get_function() );
 if( ! function ) {
  std::cerr << "the InvestmentBlock has no InvestmentFunction" << std::endl;
  std::exit( 1 );
  }
 function->set_ComputeConfig( & cc );

 const auto value = solve_by_bundle( inv , "BSPar-Inv.txt" , secs , iters ,
				     status , ran , dflt );
 delete root;
 return( value );
 }

/*--------------------------------------------------------------------------*/
/*-------------------------------- MAIN ------------------------------------*/
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/*-------------- THE BENDERS FORM OF AN INSTANCE ON FILE -------------------*/
/*--------------------------------------------------------------------------*/

/* An instance written in its extensive form has the here-and-now Variable
 * replicated in every scenario and tied by the non-anticipativity Constraint
 * of the stochastic Block, while BendersDecompositionSolver asks for them in
 * a single copy in the root, for one sub-Block per subproblem, and for the
 * coupling written as Constraint of the sub-Block where those Variable
 * appear linearly. No file format carries the latter: AbstractBlock only
 * deserializes a .lp/.mps model, and a model in a file of its own cannot
 * name the Variable of a sub-Block. The shape is therefore assembled here,
 * around the very Block the file gives:
 *
 * - the here-and-now AbstractPath of the stochastic Block resolve, in each
 *   leaf, to the design Variable of that leaf; one copy of them goes in the
 *   root, bounded as the leaves bound theirs;
 *
 * - each leaf goes under a wrapper AbstractBlock whose Constraint say that
 *   the leaf uses at most what the root buys, i.e., x^l - x <= 0, a leaf not
 *   letting a Constraint be added to it from the outside; the wrapper is the
 *   subproblem, and the leaf keeps its data, its Objective and its weight;
 *
 * - the cost of the design is moved from the leaves to the root: a leaf that
 *   pays for the capacity it is given has a value function that is not
 *   monotone in it, and the coupling duals the master reads its cuts from
 *   lose the sign a capacity coupling has.
 *
 * What comes out is the same problem, hence a :MILPSolver attached to it
 * reads the extensive form and is the reference of the cross-check. */

static AbstractBlock * benders_form( TwoStageStochasticBlock * tssb )
{
 const Index L = tssb->get_number_leaves();
 const auto & paths = tssb->get_paths_to_static_here_and_now_vars();

 std::vector< std::vector< ColVariable * > > xk( L );
 for( Index l = 0 ; l < L ; ++l ) {
  auto leaf = tssb->get_leaf_block( l );
  if( ! leaf )
   return( nullptr );

  for( const auto & p : paths ) {
   const auto nv = p->get_number_elements< ColVariable >( leaf );
   auto e = p->get_element< ColVariable >( leaf );
   for( Index j = 0 ; j < nv ; ++j )
    xk[ l ].push_back( e + j );
   }
  }

 const Index n = xk[ 0 ].size();
 if( ! n )
  return( nullptr );

 for( Index l = 1 ; l < L ; ++l )
  if( xk[ l ].size() != n )
   return( nullptr );

 auto root = new AbstractBlock();

 /* the box of a design Variable is not on the Variable: the units state it
  * as a OneVarConstraint of their own, which is one of the "active stuff" of
  * the Variable, so the bounds are collected from there and intersected with
  * whatever the Variable itself declares */

 auto x = new std::vector< ColVariable >( n );
 auto bnd = new std::vector< BoxConstraint >( n );

 for( Index j = 0 ; j < n ; ++j ) {
  auto lo = xk[ 0 ][ j ]->get_lb() , up = xk[ 0 ][ j ]->get_ub();
  for( Index a = 0 ; a < xk[ 0 ][ j ]->get_num_active() ; ++a )
   if( auto ovc = dynamic_cast< OneVarConstraint * >(
                                         xk[ 0 ][ j ]->get_active( a ) ) ) {
    lo = std::max( lo , double( ovc->get_lhs() ) );
    up = std::min( up , double( ovc->get_rhs() ) );
    }

  ( *bnd )[ j ].set_variable( & ( *x )[ j ] );
  ( *bnd )[ j ].set_lhs( lo );
  ( *bnd )[ j ].set_rhs( up );
  }

 root->add_static_variable( *x , "x" );
 root->add_static_constraint( *bnd , "design bound" );

 // the cost of the design, taken out of the Objective of every leaf
 auto rlf = new LinearFunction();
 std::vector< double > cost( n , 0 );

 for( Index l = 0 ; l < L ; ++l )
  for( Index j = 0 ; j < n ; ++j ) {
   auto var = xk[ l ][ j ];
   for( Index a = 0 ; a < var->get_num_active() ; ++a ) {
    auto obj = dynamic_cast< FRealObjective * >( var->get_active( a ) );
    if( ! obj )
     continue;

    if( auto lf = dynamic_cast< LinearFunction * >( obj->get_function() ) ) {
     const auto & vv = lf->get_v_var();
     for( Index i = 0 ; i < vv.size() ; ++i )
      if( vv[ i ].first == var ) {
       cost[ j ] += vv[ i ].second;
       lf->remove_variable( i );
       break;
       }
     }
    break;
    }
   }

 for( Index j = 0 ; j < n ; ++j )
  if( cost[ j ] )
   rlf->add_variable( & ( *x )[ j ] , cost[ j ] );

 auto robj = new FRealObjective( root , rlf );
 robj->set_sense( Objective::eMin );
 root->set_objective( robj );

 // one subproblem per leaf, the wrapper carrying the coupling
 for( Index l = 0 ; l < L ; ++l ) {
  auto leaf = tssb->get_leaf_block( l );
  auto sub = new AbstractBlock( root );

  /* the wrapper adds nothing to the Objective of the leaf, but it has to say
   * which way the subproblem goes: a Block with no Objective answers eUndef,
   * and the sign of the cuts is read off that */

  auto sof = new FRealObjective();
  sof->set_function( new LinearFunction() );
  sof->set_sense( Objective::eMin );
  sub->set_objective( sof );

  auto cns = new std::list< FRowConstraint >( n );
  Index j = 0;
  for( auto & c : *cns ) {
   auto lf = new LinearFunction();
   lf->add_variable( xk[ l ][ j ] , 1 );
   lf->add_variable( & ( *x )[ j ] , -1 );
   c.set_function( lf );
   c.set_lhs( - Inf< double >() );
   c.set_rhs( 0 );
   ++j;
   }

  sub->add_dynamic_constraint( *cns , "link" );
  leaf->set_f_Block( sub );
  sub->add_nested_Block( leaf );
  root->add_nested_Block( sub );
  }

 return( root );
 }

/*--------------------------------------------------------------------------*/

/* The cross-check on an instance that is on file: the Benders form of it is
 * assembled, the BlockSolverConfig attaches to it whatever Solver it names,
 * which is a :MILPSolver reading the extensive form and the Benders Solver
 * reading the structure, and SolveAll() computes them all and checks that
 * they agree, with the reference objective value as one more of them. */

static bool run_on_file( const std::string & file , const std::string & bcfg ,
                         const std::string & scfg , double ref )
{
 auto blk = Block::deserialize( file );
 if( ! blk ) {
  std::cout << file << ": cannot be read" << std::endl;
  return( false );
  }

 auto tssb = dynamic_cast< TwoStageStochasticBlock * >( blk );
 if( ! tssb )
  if( auto inv = dynamic_cast< InvestmentBlock * >( blk ) )
   if( auto f = dynamic_cast< InvestmentFunction * >( inv->get_function() ) )
    if( ! f->get_nested_Blocks().empty() )
     tssb = dynamic_cast< TwoStageStochasticBlock * >(
                                        f->get_nested_Blocks().front() );

 if( ! tssb ) {
  std::cout << file << ": no stochastic Block in it" << std::endl;
  delete blk;
  return( false );
  }

 if( ! bcfg.empty() )
  if( auto bc = Configuration::deserialize( bcfg ) )
   b_config_Block( blk , bc , bcfg );

 blk->generate_abstract_variables();
 blk->generate_abstract_constraints();
 blk->generate_objective();

 auto root = benders_form( tssb );
 if( ! root ) {
  std::cout << file << ": no here-and-now Variable to put in the master"
            << std::endl;
  delete blk;
  return( false );
  }

 auto sc = Configuration::deserialize( scfg );
 if( ! sc ) {
  std::cout << scfg << ": cannot be read" << std::endl;
  delete blk;
  return( false );
  }

 // s_config_Block() clears the configuration after applying it, so the same
 // object un-registers the Solver when it is applied again at the end
 s_config_Block( root , sc , scfg );

 const bool ok = SolveAll( root , ref );

 s_config_Block( root , sc );
 delete sc;

 return( ok );
 }

/*--------------------------------------------------------------------------*/
/*--------------------------------- MAIN -----------------------------------*/
/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 /* An instance is read from file whenever a BlockSolverConfig is named on
  * the command line: that is the cross-check on the Benders form of an
  * instance written in its extensive form. With the instance alone, or with
  * none, the three forms of the investment problem the generator writes are
  * compared instead. */

 { std::string file , bcfg , scfg;
   double ref = std::numeric_limits< double >::quiet_NaN();

   for( int i = 1 ; i < argc ; ++i ) {
    const std::string a = argv[ i ];
    if( ( a == "-S" ) && ( i + 1 < argc ) )
     scfg = argv[ ++i ];
    else
     if( ( a == "-B" ) && ( i + 1 < argc ) )
      bcfg = argv[ ++i ];
     else
      if( ( a == "-r" ) && ( i + 1 < argc ) )
       ref = std::atof( argv[ ++i ] );
      else
       if( file.empty() )
        file = a;
    }

   if( ! scfg.empty() )
    return( run_on_file( file , bcfg , scfg , ref ) ? 0 : 1 );

   if( ! file.empty() )
    instance = file;
   }

 auto d = read( instance );
 std::cout << d.demand.size() << " scenarios, " << d.demand[ 0 ].size()
	   << " time steps, design cost " << d.cost << " in [ " << d.lb
	   << " , " << d.ub << " ]" << std::endl;

 double t_ref , t_bds = 0 , t_inv = 0;
 long i_ref , i_bds = 0 , i_inv = 0;
 int s_ref , s_bds = 0 , s_inv = 0;
 bool has_bds , has_inv;

 auto mono = build_monolithic( d );
 const auto ref = solve( mono , "BDSSCfg.txt" , t_ref , i_ref , s_ref );

 auto bend = build_structured( d );
 const auto bds = solve_by_bundle( bend , "BSPar-BDS.txt" , t_bds ,
				   i_bds , s_bds , has_bds , ref );

 const auto inv = solve_ad_hoc( t_inv , i_inv , s_inv , has_inv , ref );

 std::cout.precision( 12 );
 std::cout << "\nmonolithic        " << ref << "  status " << s_ref
	   << "  in " << t_ref << " s";
 if( has_bds )
  std::cout << "\nBenders, generic  " << bds << "  status " << s_bds
	    << "  in " << t_bds << " s, " << i_bds << " iterations";
 if( has_inv )
  std::cout << "\nBenders, ad hoc   " << inv << "  status " << s_inv
	    << "  in " << t_inv << " s, " << i_inv << " iterations";
 std::cout << std::endl;

 const auto scale = std::max( 1.0 , std::abs( ref ) );
 bool ok = true;

 if( std::abs( bds - ref ) > tolerance * scale ) {
  std::cout << "ERROR: the generic Benders form is off by "
	    << std::abs( bds - ref ) / scale << std::endl;
  ok = false;
  }

 if( std::abs( inv - ref ) > tolerance * scale ) {
  std::cout << "ERROR: the ad hoc Benders form is off by "
	    << std::abs( inv - ref ) / scale << std::endl;
  ok = false;
  }

 if( ! ok )
  std::cout << "\nthe forms do not describe the same problem" << std::endl;
 else
  if( has_bds && has_inv )
   std::cout << "\nOK, the two forms agree with the extensive optimum"
	     << std::endl;
  else
   std::cout << "\nOK, what could be run agrees with the extensive optimum"
	     << std::endl;

 delete mono;
 delete bend;
 return( ok ? 0 : 1 );
 }

/*--------------------------------------------------------------------------*/
/*------------------------- End File test_bds.cpp -------------------------*/
/*--------------------------------------------------------------------------*/
