/*--------------------------------------------------------------------------*/
/*-------------------------- File test.cpp ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Tester of the weights of a Block [see Block::set_objective_weight() and
 * Block::set_variable_weight()] in the :MILPSolver.
 *
 * A random tree of AbstractBlock three levels deep (the root, nson Block
 * under it and nson Block under each of these) is built twice out of the
 * same data: once with a random weight on the Objective and one on the
 * Variable of each Block, the root comprised, and once, the "twin", with
 * no weight, with the coefficients and the constant of each Objective
 * already multiplied by the product of the weights of the Objective along
 * the path up to the root, the root excluded, and with the coefficient of a
 * ColVariable of a son in a row of its father already multiplied by the
 * weight of the Variable of the son.
 * The Solver of the BlockSolverConfig are attached to both roots, and to
 * the first Block of the middle level of both trees, and each weighted Block
 * is checked against its twin: at the root the two have the same optimal
 * value, at the middle level that of the twin is that of the weighted Block
 * times the weight of the latter, which is not seen by a Solver attached to
 * it. Then, for a number of rounds, both trees are changed in the same way
 * (the twin by the coefficients multiplied as above) and solved again.
 *
 * Every Block has its own ColVariable, in a box which contains 0, and rows
 * among its own ColVariable and those of its sons, which 0 satisfies; the
 * Objective, to be minimized, is a LinearFunction or a convex DQuadFunction
 * (half and half), with a constant, and with -k 16 that of the first leaf is
 * a convex QuadFunction, while with -k 32 they are all LinearFunction, i.e.,
 * the tree is a linear program. What is changed is coded bit-wise in -k: 1
 * linear coefficients, 2 the constant, 4 linear and quadratic coefficients
 * of the DQuadFunction, 8 ColVariable taken out of and put back into an
 * Objective, 64 coefficients of the rows, 128 the weights of a Block (that
 * of the Variable, and that of the Objective of a leaf) while the Solver
 * are attached, which they have to handle [see
 * Solver::handles_weight_change()]. A change of the weights is checked to
 * be refused, with nothing changed, while a Solver that cannot handle it
 * is attached to an ancestor.
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

#include <cmath>
#include <iostream>
#include <random>
#include <vector>

#include "common_utils.h"

#include "AbstractBlock.h"
#include "FakeSolver.h"
#include "FRealObjective.h"
#include "FRowConstraint.h"
#include "LinearFunction.h"
#include "OneVarConstraint.h"
#include "QuadFunction.h"

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace std;

using namespace SMSpp_di_unipi_it;

using Index = Block::Index;

/*--------------------------------------------------------------------------*/
/*-------------------------------- TYPES -----------------------------------*/
/*--------------------------------------------------------------------------*/

/// the data of one row: lhs <= sum of coeff[ h ] * x[ var[ h ] ] <= rhs
struct RowData {
 std::vector< std::pair< int , Index > > var;  ///< (Block, index in it)
 std::vector< double > coeff;
 double lhs , rhs;
 };

/// the data of one Block of the tree, the same for the two trees
struct BlockData {
 int father;                  ///< index of the father, -1 for the root
 std::vector< int > sons;     ///< indices of the sons
 double weight;               ///< weight of its Objective (1 means not set)
 double vweight;              ///< weight of its Variable (1 means not set)
 double path;                 ///< product of the weights up to the root,
                              ///< the root excluded
 std::vector< double > lb , ub;
 int kind;                    ///< 0 linear, 1 DQuadFunction, 2 QuadFunction
 std::vector< double > lin , quad;
 std::vector< std::tuple< Index , Index , double > > offd;
 double cst;
 std::vector< RowData > rows;
 };

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/

Index nvar = 4;                  // ColVariable of each Block
Index nson = 2;                  // sons of each non-leaf Block
Index & wchg = round_opts.wchg;  // what is changed

std::mt19937 rg;
std::uniform_real_distribution<> dis( 0.0 , 1.0 );

std::vector< BlockData > tdata;   // the tree, the root first

// the Block of the two trees, in the order of tdata
std::vector< AbstractBlock * > wgt , twin;

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

static double lin_c( void ) { return( 2 * dis( rg ) - 1 ); }

static double quad_c( void ) { return( 0.0625 + 0.1875 * dis( rg ) ); }

/*--------------------------------------------------------------------------*/
/// a weight in [ 0.1 , 10 ], log-uniform, or 1 (not set) one time in five

static double weight( void )
{
 if( dis( rg ) < 0.2 )
  return( 1 );
 return( std::pow( 10.0 , 2 * dis( rg ) - 1 ) );
 }

/*--------------------------------------------------------------------------*/

static int add_data( int father , double path )
{
 int b = tdata.size();
 tdata.emplace_back();
 auto & d = tdata.back();
 d.father = father;
 d.weight = weight();
 d.vweight = weight();
 d.path = ( father < 0 ) ? 1 : path * d.weight;
 for( Index i = 0 ; i < nvar ; ++i ) {
  d.lb.push_back( - 2 * dis( rg ) );
  d.ub.push_back( 2 * dis( rg ) );
  d.lin.push_back( lin_c() );
  d.quad.push_back( quad_c() );
  }
 d.kind = ( dis( rg ) < 0.5 ) ? 0 : 1;
 if( wchg & 32 )
  d.kind = 0;
 d.cst = 10 * lin_c();
 return( b );
 }

/*--------------------------------------------------------------------------*/
/// the data of the tree: the root, nson sons, nson sons of each of them

static void generate( void )
{
 int root = add_data( -1 , 1 );
 for( Index k = 0 ; k < nson ; ++k ) {
  int mid = add_data( root , 1 );
  tdata[ root ].sons.push_back( mid );
  for( Index h = 0 ; h < nson ; ++h ) {
   int leaf = add_data( mid , tdata[ mid ].path );
   tdata[ mid ].sons.push_back( leaf );
   }
  }

 // with -k 16 the first leaf has a convex QuadFunction, diagonally dominant
 if( ( wchg & 16 ) && ( ! ( wchg & 32 ) ) ) {
  auto & d = tdata[ tdata[ tdata[ 0 ].sons[ 0 ] ].sons[ 0 ] ];
  d.kind = 2;
  for( Index i = 0 ; i + 1 < nvar ; ++i )
   d.offd.emplace_back( i , i + 1 , 0.02 * lin_c() );
  }

 // the rows of each Block: on its own ColVariable and on those of its sons
 for( int b = 0 ; b < int( tdata.size() ) ; ++b ) {
  std::vector< int > among = tdata[ b ].sons;
  among.push_back( b );
  for( Index r = 0 ; r < nvar / 2 + 1 ; ++r ) {
   RowData row;
   for( int a : among )
    for( Index i = 0 ; i < nvar ; ++i )
     if( dis( rg ) < 0.5 ) {
      row.var.emplace_back( a , i );
      row.coeff.push_back( lin_c() );
      }
   if( row.var.empty() ) {
    row.var.emplace_back( b , 0 );
    row.coeff.push_back( 1 );
    }
   row.lhs = - dis( rg );   // 0 is always feasible
   row.rhs = dis( rg );
   tdata[ b ].rows.push_back( std::move( row ) );
   }
  }
 }

/*--------------------------------------------------------------------------*/
/// the ColVariable of a Block

static std::vector< ColVariable > & x_of( AbstractBlock * b )
{
 return( * b->get_static_variable_v< ColVariable >( "x" ) );
 }

/*--------------------------------------------------------------------------*/
/// the factor of a ColVariable of the Block a in a row of the Block b: the
/// weight of the Variable of a, a son of b, already in the coefficient of
/// the twin and seen by the Solver in the weighted tree

static double factor( int a , int b , bool is_twin )
{
 return( ( is_twin && ( a != b ) ) ? tdata[ a ].vweight : 1 );
 }

/*--------------------------------------------------------------------------*/
/// the LinearFunction of the r-th row of a Block

static LinearFunction * row_of( AbstractBlock * b , Index r )
{
 auto & rows = * b->get_static_constraint_v< FRowConstraint >( "rows" );
 return( static_cast< LinearFunction * >( rows[ r ].get_function() ) );
 }

/*--------------------------------------------------------------------------*/
/// builds a tree out of the data: the weighted one, or the twin

static void build( std::vector< AbstractBlock * > & tree , bool is_twin )
{
 tree.assign( tdata.size() , nullptr );
 for( int b = 0 ; b < int( tdata.size() ) ; ++b ) {
  auto & d = tdata[ b ];
  auto blk = ( d.father < 0 ) ? new AbstractBlock()
                              : new AbstractBlock( tree[ d.father ] );
  tree[ b ] = blk;
  if( d.father >= 0 )
   tree[ d.father ]->add_nested_Block( blk );

  auto x = new std::vector< ColVariable >( nvar );
  blk->add_static_variable( *x , "x" );
  auto box = new std::vector< BoxConstraint >( nvar );
  for( Index i = 0 ; i < nvar ; ++i ) {
   ( *box )[ i ].set_variable( & ( *x )[ i ] );
   ( *box )[ i ].set_lhs( d.lb[ i ] );
   ( *box )[ i ].set_rhs( d.ub[ i ] );
   }
  blk->add_static_constraint( *box , "box" );

  const double s = is_twin ? d.path : 1;
  Function * f;
  if( d.kind == 0 ) {
   LinearFunction::v_coeff_pair vp;
   for( Index i = 0 ; i < nvar ; ++i )
    vp.emplace_back( & ( *x )[ i ] , s * d.lin[ i ] );
   f = new LinearFunction( std::move( vp ) , s * d.cst );
   }
  else {
   DQuadFunction::v_coeff_triple vt;
   for( Index i = 0 ; i < nvar ; ++i )
    vt.emplace_back( & ( *x )[ i ] , s * d.lin[ i ] , s * d.quad[ i ] );
   if( d.kind == 1 )
    f = new DQuadFunction( std::move( vt ) , s * d.cst );
   else {
    QuadFunction::v_off_diag_term od;
    for( auto & t : d.offd )
     od.emplace_back( std::get< 0 >( t ) , std::get< 1 >( t ) ,
                      s * std::get< 2 >( t ) );
    f = new QuadFunction( std::move( vt ) , std::move( od ) , s * d.cst );
    }
   }
  auto obj = new FRealObjective();
  obj->set_function( f );
  obj->set_sense( Objective::eMin , eNoMod );
  blk->set_objective( obj );

  if( ! is_twin ) {
   blk->set_objective_weight( d.weight );
   blk->set_variable_weight( d.vweight );
   }
  }

 // the rows, once all the ColVariable are there
 for( int b = 0 ; b < int( tdata.size() ) ; ++b ) {
  auto rows = new std::vector< FRowConstraint >( tdata[ b ].rows.size() );
  for( Index r = 0 ; r < rows->size() ; ++r ) {
   auto & rd = tdata[ b ].rows[ r ];
   LinearFunction::v_coeff_pair vp;
   for( Index h = 0 ; h < rd.var.size() ; ++h ) {
    auto & x = x_of( tree[ rd.var[ h ].first ] );
    vp.emplace_back( & x[ rd.var[ h ].second ] ,
                     rd.coeff[ h ] * factor( rd.var[ h ].first , b ,
                                             is_twin ) );
    }
   ( *rows )[ r ].set_function( new LinearFunction( std::move( vp ) ) );
   ( *rows )[ r ].set_lhs( rd.lhs );
   ( *rows )[ r ].set_rhs( rd.rhs );
   }
  tree[ b ]->add_static_constraint( *rows , "rows" );
  }
 }

/*--------------------------------------------------------------------------*/
/// the Function of the Objective of a Block, and the same as each kind

static Function * fn_of( AbstractBlock * b )
{
 return( static_cast< FRealObjective * >( b->get_objective() )->
                                                            get_function() );
 }

static DQuadFunction * dq_of( AbstractBlock * b )
{
 return( dynamic_cast< DQuadFunction * >( fn_of( b ) ) );
 }

static LinearFunction * lf_of( AbstractBlock * b )
{
 return( dynamic_cast< LinearFunction * >( fn_of( b ) ) );
 }

/*--------------------------------------------------------------------------*/
/// the index in "x" of the i-th active ColVariable of the Objective of b

static Index position( AbstractBlock * b , Index i )
{
 auto v = static_cast< ColVariable * >( fn_of( b )->get_active_var( i ) );
 return( v - & x_of( b )[ 0 ] );
 }

/*--------------------------------------------------------------------------*/
/// one round of random changes of the Block b of both trees

static void change( int b )
{
 // what is changed is printed with -v
 auto say = [ b ]( const std::string & what , Index i , double v ,
                   double q = 0 ) {
  if( verbosity_level > 0 )
   cout << "  Block " << b << ": " << what << " " << i << " " << v << " "
        << q << endl;
  };
 auto & d = tdata[ b ];
 auto W = wgt[ b ];
 auto T = twin[ b ];
 const double s = d.path;

 if( ( wchg & 1 ) && ( dis( rg ) < round_opts.p_change ) ) {
  auto nav = fn_of( T )->get_num_active_var();
  for( Index c = 0 ; c < round_opts.n_change && nav ; ++c ) {
   Index i = Index( dis( rg ) * nav ) % nav;
   double v = lin_c();
   say( "linear" , i , v );
   if( d.kind == 0 ) {
    lf_of( W )->modify_coefficient( i , v );
    lf_of( T )->modify_coefficient( i , s * v );
    }
   else {
    dq_of( W )->modify_linear_coefficient( i , v );
    dq_of( T )->modify_linear_coefficient( i , s * v );
    }
   }
  }

 if( ( wchg & 2 ) && ( dis( rg ) < round_opts.p_change ) ) {
  double v = 10 * lin_c();
  say( "constant" , 0 , v );
  if( d.kind == 0 ) {
   lf_of( W )->set_constant_term( v );
   lf_of( T )->set_constant_term( s * v );
   }
  else {
   dq_of( W )->set_constant_term( v );
   dq_of( T )->set_constant_term( s * v );
   }
  }

 if( ( wchg & 4 ) && ( d.kind == 1 ) &&
     ( dis( rg ) < round_opts.p_change ) ) {
  auto nav = fn_of( T )->get_num_active_var();
  for( Index c = 0 ; c < round_opts.n_change && nav ; ++c ) {
   Index i = Index( dis( rg ) * nav ) % nav;
   double l = lin_c() , q = quad_c();
   say( "term" , i , l , q );
   dq_of( W )->modify_term( i , l , q );
   dq_of( T )->modify_term( i , s * l , s * q );
   }
  }

 // a ColVariable out of the Objective, or one back in; not for the
 // QuadFunction, whose off-diagonal terms would go with it
 if( ( wchg & 8 ) && ( d.kind != 2 ) &&
     ( dis( rg ) < round_opts.p_change ) ) {
  auto nav = fn_of( T )->get_num_active_var();
  if( ( nav > 1 ) && ( ( nav == nvar ) || ( dis( rg ) < 0.5 ) ) ) {
   Index i = Index( dis( rg ) * nav ) % nav;
   say( "remove" , i , 0 );
   if( d.kind == 0 ) {
    lf_of( W )->remove_variable( i );
    lf_of( T )->remove_variable( i );
    }
   else {
    dq_of( W )->remove_variable( i );
    dq_of( T )->remove_variable( i );
    }
   }
  else
   if( nav < nvar ) {  // put back the first one that is not there
    std::vector< bool > in( nvar , false );
    for( Index i = 0 ; i < nav ; ++i )
     in[ position( T , i ) ] = true;
    Index j = 0;
    while( in[ j ] )
     ++j;
    double l = lin_c() , q = quad_c();
    say( "add" , j , l , q );
    if( d.kind == 0 ) {
     lf_of( W )->add_variable( & x_of( W )[ j ] , l );
     lf_of( T )->add_variable( & x_of( T )[ j ] , s * l );
     }
    else {
     dq_of( W )->add_variable( & x_of( W )[ j ] , l , q );
     dq_of( T )->add_variable( & x_of( T )[ j ] , s * l , s * q );
     }
    }
  }

 // a coefficient of a row, multiplied in the twin by the weight of the
 // Variable of the Block the ColVariable belongs to
 if( ( wchg & 64 ) && ( dis( rg ) < round_opts.p_change ) ) {
  for( Index c = 0 ; c < round_opts.n_change ; ++c ) {
   Index r = Index( dis( rg ) * d.rows.size() ) % d.rows.size();
   auto & rd = d.rows[ r ];
   Index h = Index( dis( rg ) * rd.var.size() ) % rd.var.size();
   double v = lin_c();
   say( "row coefficient" , h , v , r );
   rd.coeff[ h ] = v;
   row_of( W , r )->modify_coefficient( h , v );
   row_of( T , r )->modify_coefficient( h , v *
                                        factor( rd.var[ h ].first , b ,
                                                true ) );
   }
  }

 // the weights of a Block with the Solver attached: that of its Variable,
 // which the twin has in the coefficients of the rows of the father, and,
 // for a leaf with no QuadFunction, that of its Objective, which the twin
 // has in the coefficients and the constant of its Objective
 if( ( wchg & 128 ) && ( d.father >= 0 ) &&
     ( dis( rg ) < round_opts.p_change ) ) {
  const double sv = weight();
  say( "weight of the Variable" , 0 , sv );
  W->set_variable_weight( sv );
  d.vweight = sv;
  auto & fd = tdata[ d.father ];
  for( Index r = 0 ; r < fd.rows.size() ; ++r )
   for( Index h = 0 ; h < fd.rows[ r ].var.size() ; ++h )
    if( fd.rows[ r ].var[ h ].first == b )
     row_of( twin[ d.father ] , r )->modify_coefficient( h ,
                                              fd.rows[ r ].coeff[ h ] * sv );

  if( d.sons.empty() && ( d.kind != 2 ) ) {
   const double w = weight();
   const double ratio = w / d.weight;
   say( "weight of the Objective" , 0 , w );
   W->set_objective_weight( w );
   d.weight = w;
   d.path *= ratio;
   auto nav = fn_of( T )->get_num_active_var();
   if( d.kind == 0 ) {
    auto lf = lf_of( T );
    for( Index i = 0 ; i < nav ; ++i )
     lf->modify_coefficient( i , ratio * lf->get_coefficient( i ) );
    lf->set_constant_term( ratio * lf->get_constant_term() );
    }
   else {
    auto dq = dq_of( T );
    for( Index i = 0 ; i < nav ; ++i )
     dq->modify_term( i , ratio * dq->get_linear_coefficient( i ) ,
                      ratio * dq->get_quadratic_coefficient( i ) );
    dq->set_constant_term( ratio * dq->get_constant_term() );
    }
   }
  }
 }

/*--------------------------------------------------------------------------*/
/// solves the twin, then the weighted Block against it

static bool check( AbstractBlock * W , AbstractBlock * T , double scale ,
                   const std::string & where )
{
 cout << where << " twin:     ";
 double fo = 0;
 bool hs = false;
 bool ok = SolveAll( T , std::numeric_limits< double >::quiet_NaN() , 1e-6 ,
                     & fo , & hs );
 cout << where << " weighted: ";
 if( ! hs ) {
  cout << "the twin has no optimal value" << endl;
  return( false );
  }
 return( SolveAll( W , fo / scale , 1e-6 ) && ok );
 }

/*--------------------------------------------------------------------------*/
/// a Solver that applies the weights of the Variable [see
/// Solver::handles_variable_weight()] but cannot handle a change of them

class StaticWeights : public FakeSolver {
 public:
 [[nodiscard]] bool handles_variable_weight( void ) const override {
  return( true );
  }
 };

/*--------------------------------------------------------------------------*/

static bool process_specific_arg( int opt )
{
 if( process_round_option( opt ) )
  return( true );
 switch( opt ) {
  case( 'N' ): Str2Sthg( optarg , nvar ); return( true );
  case( 's' ): Str2Sthg( optarg , nson ); return( true );
  default:                                return( false );
  }
 }

/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 std::set_terminate( smspp_terminate );

 docopt_desc = "SMS++ weights of a Block in :MILPSolver test.\n";
 filename_optional = true;
 round_opts = { 0 , 15 , 10 , 3 , 0.5 };
 add_round_options( "                                    1 linear coeffs, "
                    "2 constant, 4 quadratic coeffs,\n"
                    "                                    8 ColVariable out "
                    "of / into the Objective,\n"
                    "                                    16 a QuadFunction "
                    "in the first leaf,\n"
                    "                                    32 all the "
                    "Objective linear,\n"
                    "                                    64 coefficients "
                    "of the rows,\n"
                    "                                    128 weights with "
                    "the Solver attached\n" );
 short_opts += "N:s:";
 const std::vector< option > my_opts = {
   { "nvar" , required_argument , nullptr , 'N' } ,
   { "nson" , required_argument , nullptr , 's' } };
 long_opts.insert( std::prev( long_opts.end() ) ,
                   my_opts.begin() , my_opts.end() );
 help += "  -N, --nvar <n>                  ColVariable of each Block [4]\n"
         "  -s, --nson <n>                  sons of each Block [2]\n";
 process_args( argc , argv , process_specific_arg );
 require_solver_config();

 if( ( nvar < 1 ) || ( nson < 1 ) ) {
  cout << "error: nvar and nson must be positive" << endl;
  return( 1 );
  }

 rg.seed( round_opts.seed );
 generate();
 build( wgt , false );
 build( twin , true );

 // the Solver on the two roots and on the first Block of the middle level
 const int mid = tdata[ 0 ].sons[ 0 ];
 std::vector< std::pair< Block * , Configuration * > > att;
 for( Block * b : { static_cast< Block * >( wgt[ 0 ] ) ,
                    static_cast< Block * >( twin[ 0 ] ) ,
                    static_cast< Block * >( wgt[ mid ] ) ,
                    static_cast< Block * >( twin[ mid ] ) } ) {
  auto bsc = Configuration::deserialize( sconf_file );
  if( ! bsc ) {
   cerr << "Error: cannot load BSC from " << sconf_file << endl;
   return( 1 );
   }
  s_config_Block( b , bsc , sconf_file );
  if( b->get_registered_solvers().empty() ) {
   cout << "no Solver registered to the Block" << endl;
   return( 77 );
   }
  att.emplace_back( b , bsc );
  }

 // a change of the weights is refused, and nothing changes, while a
 // Solver attached to an ancestor cannot handle it
 auto fake = new StaticWeights();
 wgt[ 0 ]->register_Solver( fake );
 bool refused = false;
 try { wgt[ mid ]->set_weight( 2 ); }
 catch( const std::logic_error & ) { refused = true; }
 bool AllPassed = refused &&
  ( wgt[ mid ]->get_objective_weight() == tdata[ mid ].weight ) &&
  ( wgt[ mid ]->get_variable_weight() == tdata[ mid ].vweight );
 if( ! AllPassed )
  cout << "set_weight() not refused, or the weights changed, with a Solver "
          "that cannot handle it attached to an ancestor" << endl;
 wgt[ 0 ]->unregister_Solver( fake , true );

 AllPassed &= check( wgt[ 0 ] , twin[ 0 ] , 1 , "root" );
 AllPassed &= check( wgt[ mid ] , twin[ mid ] , tdata[ mid ].weight ,
                     "middle" );

 for( Index rep = 0 ; rep < round_opts.n_repeat ; ++rep ) {
  if( verbosity_level > 0 )
   cout << "round " << rep << endl;
  for( int b = 0 ; b < int( tdata.size() ) ; ++b )
   change( b );
  AllPassed &= check( wgt[ 0 ] , twin[ 0 ] , 1 , "root" );
  AllPassed &= check( wgt[ mid ] , twin[ mid ] , tdata[ mid ].weight ,
                      "middle" );
  }

 if( AllPassed )
  cout << GREEN( All tests passed!! ) << endl;
 else
  cout << RED( Shit happened!! ) << endl;

 // detach the Solver, sons first, then delete the trees
 for( auto it = att.rbegin() ; it != att.rend() ; ++it ) {
  s_config_Block( it->first , it->second );
  delete it->second;
  }
 delete wgt[ 0 ];
 delete twin[ 0 ];

 return( AllPassed ? 0 : 1 );
 }

/*--------------------------------------------------------------------------*/
/*---------------------------- End File test.cpp ---------------------------*/
/*--------------------------------------------------------------------------*/
