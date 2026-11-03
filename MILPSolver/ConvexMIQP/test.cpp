/*--------------------------------------------------------------------------*/
/*-------------------------- File test.cpp ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Main for testing the :MILPSolver on strictly convex Mixed-Integer
 * Quadratic Programs.
 *
 * A random instance of
 *
 *     min { x' Q x + c' x : A x <= b , x_i integer for i < n1 }
 *
 * is generated from a seed as in section 4 of "A Feasible Active Set Method
 * with Reoptimization for Convex Quadratic Mixed-Integer Programming", SIAM
 * J. Optim. 26(3), 2016: Q = sum_i l_i v_i v_i' with the n eigenvalues l_i
 * uniform in [ 0 , 1 ] and the v_i orthonormalized out of n vectors with
 * entries uniform in [ -1 , 1 ], c uniform in [ -1 , 1 ], and either (a)
 * A and b uniform in [ -1 , 1 ] or (b) A uniform in [ 0 , 1 ] and b_i half
 * the sum of the row. It is built as an AbstractBlock, with a QuadFunction
 * in a FRealObjective and a LinearFunction in each FRowConstraint, a
 * BlockSolverConfig registers to it the Solver to be compared, and
 * SolveAll() cross-checks them against one another. The solution each
 * Solver writes is then checked on the data: it has to satisfy the rows,
 * the bounds and the integrality, and its value has to be the one the
 * Solver declares. The number of nodes of each :MILPSolver is printed.
 *
 * On top of the instances of the paper, the options give those that the
 * paper does not have: equality rows, ranged rows, bounds on the integer
 * and on the continuous variables, binary variables, a maximization (of
 * the negated objective, i.e., the same problem), and two kinds of
 * infeasible instances, one with an infeasible continuous relaxation and
 * one whose relaxation is feasible but which has no integer solution.
 *
 * \author Donato Meoli \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Donato Meoli
 */
/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <random>

#include "AbstractBlock.h"

#include "common_utils.h"

#include "FRealObjective.h"

#include "FRowConstraint.h"

#include "LinearFunction.h"

#include "MILPSolver.h"

#include "OneVarConstraint.h"

#include "QuadFunction.h"

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace std;

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*-------------------------------- TYPES -----------------------------------*/
/*--------------------------------------------------------------------------*/

using Index = Block::Index;

using coeff_pair = LinearFunction::coeff_pair;
using v_coeff_pair = LinearFunction::v_coeff_pair;

using coeff_triple = DQuadFunction::coeff_triple;
using v_coeff_triple = DQuadFunction::v_coeff_triple;

using off_diag_term = QuadFunction::off_diag_term;
using v_off_diag_term = QuadFunction::v_off_diag_term;

/*--------------------------------------------------------------------------*/
/*------------------------------- CONSTANTS --------------------------------*/
/*--------------------------------------------------------------------------*/

static constexpr double INF = Inf< double >();

static constexpr double TOL = 1e-5;  // of the comparisons of values

static constexpr double FTOL = 1e-5;  // of the feasibility of a solution

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/

// the options of the instance

Index n = 20;           // number of variables (-n)
double pint = 0.5;      // fraction of the integer ones (-i)
Index m = 1;            // number of rows of the paper (-m)
int type = 0;           // 0: rows of type (a), 1: rows of type (b) (-t)
Index neq = 0;          // how many of the rows are equalities (-q)
Index nrng = 0;         // how many of the rows are ranged (-w)
double ubnd = 0;        // bound of the integer variables, 0 = none (-u)
bool binary = false;    // whether the integer variables are binary (-b)
double cbnd = 0;        // bound of the continuous ones, 0 = none (-o)
bool maximize = false;  // whether the negated objective is maximized (-x)
int infeas = 0;         // 1: infeasible relaxation, 2: no integer point (-z)
long int seed = 0;      // seed of the random generator (-e)

// the instance

Index n1;                      // number of integer variables
vector< double > Q;            // the Hessian / 2, n x n, row-major
vector< double > cv;           // the linear term
vector< vector< double > > A;  // the rows
vector< double > lhs , rhs;     // their sides
vector< double > xlb , xub;     // the bounds of the variables

std::mt19937 rg;  // base random generator

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

// the data of section 4 of the paper, then the rows of the edge cases

static void generate( void )
{
 n1 = Index( std::round( pint * n ) );
 n1 = std::min( n1 , n );

 // Q = sum_i l_i v_i v_i', the v_i orthonormalized by Gram-Schmidt
 vector< vector< double > > V( n , vector< double >( n ) );
 for( auto & v : V )
  for( auto & e : v )
   e = rnd( rg );
 for( Index i = 0 ; i < n ; ++i ) {
  for( Index k = 0 ; k < i ; ++k ) {
   double d = 0;
   for( Index j = 0 ; j < n ; ++j )
    d += V[ i ][ j ] * V[ k ][ j ];
   for( Index j = 0 ; j < n ; ++j )
    V[ i ][ j ] -= d * V[ k ][ j ];
   }
  double nn = 0;
  for( auto e : V[ i ] )
   nn += e * e;
  nn = std::sqrt( nn );
  for( auto & e : V[ i ] )
   e /= nn;
  }

 Q.assign( n * n , 0 );
 for( Index i = 0 ; i < n ; ++i ) {
  const double li = pos( rg );
  for( Index r = 0 ; r < n ; ++r )
   for( Index s = 0 ; s < n ; ++s )
    Q[ r * n + s ] += li * V[ i ][ r ] * V[ i ][ s ];
  }

 cv.resize( n );
 for( auto & e : cv )
  e = rnd( rg );

 A.assign( m , vector< double >( n ) );
 lhs.assign( m , -INF );
 rhs.resize( m );
 for( Index i = 0 ; i < m ; ++i ) {
  if( type == 0 ) {
   for( auto & e : A[ i ] )
    e = rnd( rg );
   rhs[ i ] = rnd( rg );
   }
  else {
   double s = 0;
   for( auto & e : A[ i ] )
    s += ( e = pos( rg ) );
   rhs[ i ] = s / 2;
   }
  }

 // the first neq rows are equalities, the last nrng ones are ranged
 for( Index i = 0 ; i < std::min( neq , m ) ; ++i )
  lhs[ i ] = rhs[ i ];
 for( Index i = m - std::min( nrng , m ) ; i < m ; ++i )
  if( lhs[ i ] == -INF )
   lhs[ i ] = rhs[ i ] - 1;

 // the bounds
 xlb.assign( n , -INF );
 xub.assign( n , INF );
 for( Index j = 0 ; j < n ; ++j )
  if( j < n1 ) {
   if( binary ) {
    xlb[ j ] = 0;
    xub[ j ] = 1;
    }
   else if( ubnd > 0 ) {
    xlb[ j ] = -ubnd;
    xub[ j ] = ubnd;
    }
   }
  else if( cbnd > 0 ) {
   xlb[ j ] = -cbnd;
   xub[ j ] = cbnd;
   }

 // the infeasible instances
 if( infeas == 1 ) {  // a' x <= -1 and a' x >= 1
  vector< double > a( n );
  for( auto & e : a )
   e = rnd( rg );
  A.push_back( a );
  lhs.push_back( -INF );
  rhs.push_back( -1 );
  A.push_back( a );
  lhs.push_back( 1 );
  rhs.push_back( INF );
  }
 if( infeas == 2 ) {  // 2 x_0 - 2 x_1 = 1 on two integer variables
  vector< double > a( n , 0 );
  a[ 0 ] = 2;
  a[ 1 ] = -2;
  A.push_back( a );
  lhs.push_back( 1 );
  rhs.push_back( 1 );
  }
 }

/*--------------------------------------------------------------------------*/

// the AbstractBlock of the instance

static AbstractBlock * build( void )
{
 auto AB = new AbstractBlock();

 auto x = new std::vector< ColVariable >( n );
 for( Index j = 0 ; j < n1 ; ++j )
  ( *x )[ j ].is_integer( true , eNoMod );
 AB->add_static_variable( *x , "x" );

 // the bounds, as BoxConstraint of the variables that have any
 Index nb = 0;
 for( Index j = 0 ; j < n ; ++j )
  if( ( xlb[ j ] > -INF ) || ( xub[ j ] < INF ) )
   ++nb;
 auto box = new std::vector< BoxConstraint >( nb );
 auto bit = box->begin();
 for( Index j = 0 ; j < n ; ++j )
  if( ( xlb[ j ] > -INF ) || ( xub[ j ] < INF ) ) {
   bit->set_variable( &( *x )[ j ] , eNoMod );
   bit->set_lhs( xlb[ j ] , eNoMod );
   bit->set_rhs( xub[ j ] , eNoMod );
   ++bit;
   }
 AB->add_static_constraint( *box , "box" );

 // the rows
 const Index mm = A.size();
 auto rows = new std::vector< FRowConstraint >( mm );
 for( Index i = 0 ; i < mm ; ++i ) {
  v_coeff_pair vp;
  for( Index j = 0 ; j < n ; ++j )
   if( A[ i ][ j ] != 0 )
    vp.push_back( coeff_pair( &( *x )[ j ] , A[ i ][ j ] ) );
  ( *rows )[ i ].set_function( new LinearFunction( std::move( vp ) ) ,
			       eNoMod );
  ( *rows )[ i ].set_lhs( lhs[ i ] , eNoMod );
  ( *rows )[ i ].set_rhs( rhs[ i ] , eNoMod );
  }
 AB->add_static_constraint( *rows , "rows" );

 // the Objective: x' Q x + c' x is sum_j Q_jj x_j^2 + c_j x_j plus
 // sum_{i < j} 2 Q_ij x_i x_j, negated if maximized
 const double sgn = maximize ? -1 : 1;
 v_coeff_triple vt( n );
 for( Index j = 0 ; j < n ; ++j )
  vt[ j ] = coeff_triple( &( *x )[ j ] , sgn * cv[ j ] ,
			  sgn * Q[ j * n + j ] );
 v_off_diag_term nd;
 for( Index i = 0 ; i < n ; ++i )
  for( Index j = i + 1 ; j < n ; ++j )
   if( Q[ i * n + j ] != 0 )
    nd.push_back( off_diag_term( i , j , sgn * 2 * Q[ i * n + j ] ) );

 auto obj = new FRealObjective();
 obj->set_function( new QuadFunction( std::move( vt ) , std::move( nd ) ) );
 obj->set_sense( maximize ? Objective::eMax : Objective::eMin , eNoMod );
 AB->set_objective( obj );

 return( AB );
 }

/*--------------------------------------------------------------------------*/

// the violation of the solution x of the instance, what it is in where,
// and its value in val

static double violation( const vector< double > & x , double & val ,
			 string & where )
{
 double viol = 0;
 auto upd = [ & ]( double v , const string & w ) {
  if( v > viol ) {
   viol = v;
   where = w;
   }
  };

 for( Index j = 0 ; j < n ; ++j ) {
  upd( xlb[ j ] - x[ j ] , "the lower bound of x" + to_string( j ) );
  upd( x[ j ] - xub[ j ] , "the upper bound of x" + to_string( j ) );
  if( j < n1 )
   upd( std::abs( x[ j ] - std::round( x[ j ] ) ) ,
	"the integrality of x" + to_string( j ) );
  }

 for( Index i = 0 ; i < A.size() ; ++i ) {
  double ax = 0;
  for( Index j = 0 ; j < n ; ++j )
   ax += A[ i ][ j ] * x[ j ];
  const double sc =
    std::max( { 1.0 , std::abs( lhs[ i ] < -INF / 2 ? 0 : lhs[ i ] ) ,
		std::abs( rhs[ i ] > INF / 2 ? 0 : rhs[ i ] ) } );
  if( lhs[ i ] > -INF )
   upd( ( lhs[ i ] - ax ) / sc , "the lhs of row " + to_string( i ) );
  if( rhs[ i ] < INF )
   upd( ( ax - rhs[ i ] ) / sc , "the rhs of row " + to_string( i ) );
  }

 val = 0;
 for( Index i = 0 ; i < n ; ++i ) {
  double qx = 0;
  for( Index j = 0 ; j < n ; ++j )
   qx += Q[ i * n + j ] * x[ j ];
  val += x[ i ] * ( qx + cv[ i ] );
  }
 if( maximize )
  val = -val;

 return( viol );
 }

/*--------------------------------------------------------------------------*/

static bool process_specific_arg( int opt )
{
 switch( opt ) {
  case( 'n' ):
   Str2Sthg( optarg , n );
   return( true );
  case( 'i' ):
   Str2Sthg( optarg , pint );
   return( true );
  case( 'm' ):
   Str2Sthg( optarg , m );
   return( true );
  case( 't' ):
   Str2Sthg( optarg , type );
   return( true );
  case( 'q' ):
   Str2Sthg( optarg , neq );
   return( true );
  case( 'w' ):
   Str2Sthg( optarg , nrng );
   return( true );
  case( 'u' ):
   Str2Sthg( optarg , ubnd );
   return( true );
  case( 'b' ):
   binary = true;
   return( true );
  case( 'o' ):
   Str2Sthg( optarg , cbnd );
   return( true );
  case( 'x' ):
   maximize = true;
   return( true );
  case( 'z' ):
   Str2Sthg( optarg , infeas );
   return( true );
  case( 'e' ):
   Str2Sthg( optarg , seed );
   return( true );
  default:
   return( false );
  }
 }

/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 std::set_terminate( smspp_terminate );

 // reading command line parameters - - - - - - - - - - - - - - - - - - - - -

 docopt_desc = "SMS++ tester of the :MILPSolver on convex MIQPs.\n";
 filename_optional = true;
 short_opts += "n:i:m:t:q:w:u:bo:xz:e:";
 const std::vector< option > my_opts = {
  { "nvar" , required_argument , nullptr , 'n' } ,
  { "intfrac" , required_argument , nullptr , 'i' } ,
  { "nrows" , required_argument , nullptr , 'm' } ,
  { "type" , required_argument , nullptr , 't' } ,
  { "equalities" , required_argument , nullptr , 'q' } ,
  { "ranged" , required_argument , nullptr , 'w' } ,
  { "intbound" , required_argument , nullptr , 'u' } ,
  { "binary" , no_argument , nullptr , 'b' } ,
  { "contbound" , required_argument , nullptr , 'o' } ,
  { "maximize" , no_argument , nullptr , 'x' } ,
  { "infeasible" , required_argument , nullptr , 'z' } ,
  { "seed" , required_argument , nullptr , 'e' }
  };
 long_opts.insert( std::prev( long_opts.end() ) , my_opts.begin() ,
		   my_opts.end() );
 help +=
   "  -n, --nvar <n>                  number of variables [20]\n"
   "  -i, --intfrac <p>               fraction of integer variables [0.5]\n"
   "  -m, --nrows <m>                 number of rows [1]\n"
   "  -t, --type <t>                  0: A, b in [-1, 1], 1: A in [0, 1]\n"
   "                                  and b half the rows [0]\n"
   "  -q, --equalities <k>            the first k rows are equalities [0]\n"
   "  -w, --ranged <k>                the last k rows are ranged [0]\n"
   "  -u, --intbound <u>              integer variables in [-u, u], 0 for\n"
   "                                  none [0]\n"
   "  -b, --binary                    integer variables binary\n"
   "  -o, --contbound <u>             continuous variables in [-u, u], 0\n"
   "                                  for none [0]\n"
   "  -x, --maximize                  maximize the negated objective\n"
   "  -z, --infeasible <k>            1: infeasible relaxation, 2: no\n"
   "                                  integer solution [0]\n"
   "  -e, --seed <s>                  seed of the random generator [0]\n";

 process_args( argc , argv , process_specific_arg );
 require_solver_config();

 if( ( n < 2 ) || ( pint < 0 ) || ( pint > 1 ) ) {
  cerr << "error: n must be at least 2 and the fraction in [ 0 , 1 ]" << endl;
  return( 1 );
  }

 rg.seed( seed );
 generate();

 if( ( infeas == 2 ) && ( ( n1 < 2 ) || ( ( ubnd <= 0 ) && ( ! binary ) ) ) ) {
  cerr << "error: -z 2 needs two bounded integer variables" << endl;
  return( 1 );
  }

 cout << "n = " << n << ", n1 = " << n1 << ", m = " << A.size() << ", type "
      << ( type == 0 ? "a" : "b" ) << ", seed " << seed
      << ( maximize ? ", max" : ", min" ) << endl;

 auto block = build();

 // attach the Solver - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 Configuration * bsc = Configuration::deserialize( sconf_file );
 if( ! bsc ) {
  cerr << "Error: cannot load BSC from " << sconf_file << endl;
  return( 1 );
  }
 s_config_Block( block , bsc , sconf_file );

 if( block->get_registered_solvers().empty() ) {
  delete bsc;
  delete block;
  return( 77 );
  }

 apply_solver_verbosity( block );

 // solve and cross-check - - - - - - - - - - - - - - - - - - - - - - - - - -
 // each Solver is read by its bounds, and the solution it writes is checked
 // on the data of the instance

 bool solutions_ok = true;
 std::map< std::size_t , long > nodes;
 auto classify = [ & ]( Solver * s , std::size_t k ) {
  auto reading = read_bounds( s , k );

  if( auto ms = dynamic_cast< MILPSolver * >( s ) )
   try {
    nodes[ k ] = ms->get_explored_nodes();
    }
   catch( std::exception & ) {
    }

  if( s->has_var_solution() ) {
   s->get_var_solution();
   auto xv = block->get_static_variable_v< ColVariable >( "x" );
   vector< double > x( n );
   for( Index j = 0 ; j < n ; ++j )
    x[ j ] = ( *xv )[ j ].get_value();
   double val;
   string where;
   const double viol = violation( x , val , where );
   const double dec = s->get_var_value();
   if( viol > FTOL ) {
    cout << "Solver " << k << ": its solution violates " << where << " by "
	 << viol << endl;
    solutions_ok = false;
    }
   if( std::abs( val - dec ) > TOL * std::max( 1.0 , std::abs( val ) ) ) {
    cout << "Solver " << k << ": its solution is worth " << val << ", not "
	 << dec << endl;
    solutions_ok = false;
    }
   }

  return( reading );
  };

 bool ok = SolveAll( block , classify ,
		     std::numeric_limits< double >::quiet_NaN() , TOL );

 cout << "nodes:";
 const auto & slvs = block->get_registered_solvers();
 std::size_t k = 0;
 for( auto s : slvs ) {
  cout << " " << s->classname() << " ";
  if( nodes.count( k ) )
   cout << nodes[ k ];
  else
   cout << "-";
  ++k;
  }
 cout << endl;

 ok = ok && solutions_ok;
 if( ok )
  cout << GREEN( All tests passed!! ) << endl;
 else
  cout << RED( Shit happened!! ) << endl;

 // destroy objects - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 s_config_Block( block , bsc );
 delete bsc;
 delete block;

 return( ok ? 0 : 1 );

 }  // end( main )

/*--------------------------------------------------------------------------*/
/*------------------------ End File test.cpp -------------------------------*/
/*--------------------------------------------------------------------------*/
