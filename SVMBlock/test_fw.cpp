/*--------------------------------------------------------------------------*/
/*---------------------------- File test_fw.cpp ----------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * FrankWolfeSolver on a feasible region that is not a polytope: the
 * least-squares fit of an affine model to a data set of the LIBSVM
 * repository, the weights and the bias being constrained in Euclidean balls.
 *
 * With x the n samples, augmented by a 1 for the bias, and y their targets,
 * the problem is
 *
 *   min { ( 1 / n ) || X w - y ||^2 : || w_j || <= R / sqrt( K ) ,
 *         j = 1 , ... , K }
 *
 * where w_1 , ... , w_K are K contiguous groups of the coordinates of w: with
 * K = 1 it is the least-squares fit in a ball, i.e. the constrained ("Ivanov")
 * form of ridge regression, with K > 1 the product of K balls, which is what
 * makes it a decomposition. Each group is a sub-Block of its own, holding its
 * ColVariable and the quadratic Constraint of its ball, and its oracle is
 * BallSolver, defined below, which minimizes a linear function over the ball
 * in closed form; the least-squares objective is the father one, a dense
 * QuadFunction. With R <= 0 the radius is - R times the norm of the
 * unconstrained least-squares fit, so that the Constraint is active.
 *
 * The father is solved by every Solver its BlockSolverConfig (-S) registers
 * to it, typically FrankWolfeSolver in its variants and a :MILPSolver solving
 * the quadratically constrained problem as a whole, all from the same point,
 * and the results are cross-checked by SolveAll(); the BlockSolverConfig of
 * the sub-Block (-L) registers their oracle.
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

#include "common_utils.h"

#include "AbstractBlock.h"
#include "DQuadFunction.h"
#include "FRealObjective.h"
#include "FRowConstraint.h"
#include "LinearFunction.h"
#include "OneVarConstraint.h"
#include "QuadFunction.h"
#include "SVCBlock.h"
#include "SVRBlock.h"

#include <cmath>
#include <fstream>
#include <vector>

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace std;
using namespace SMSpp_di_unipi_it;

using Index = Block::Index;
using Coefficient = DQuadFunction::Coefficient;

/*--------------------------------------------------------------------------*/
/*---------------------------- CLASS BallSolver ----------------------------*/
/*--------------------------------------------------------------------------*/
/// minimizes a linear function over an Euclidean ball, in closed form
/** The Block is a "ball": its ColVariable x are the static group "x", its
 * Constraint is the static FRowConstraint "ball", sum_i x_i^2 <= r^2, and its
 * Objective a LinearFunction c x + c0. The minimum is attained at
 * x = - r c / || c ||, and it is c0 - r || c ||; with c = 0 every point of
 * the ball is optimal, and the centre is given. Everything is read anew at
 * each compute(), hence the Modification are of no interest. */

class BallSolver : public Solver
{
 public:

 BallSolver( void ) : Solver() {}

 int compute( bool changedvars = true ) override;

 OFValue get_var_value( void ) override { return( f_value ); }

 OFValue get_lb( void ) override { return( f_value ); }

 OFValue get_ub( void ) override { return( f_value ); }

 bool has_var_solution( void ) override { return( f_has ); }

 void get_var_solution( Configuration * solc = nullptr ) override;

 void add_Modification( sp_Mod & mod ) override {}

 SMSpp_insert_in_factory_h;

 private:

 std::vector< std::pair< ColVariable * , double > > f_x;  ///< the minimum
 OFValue f_value = 0;                                     ///< its value
 bool f_has = false;                                      ///< computed
 };

SMSpp_insert_in_factory_cpp_0( BallSolver );

/*--------------------------------------------------------------------------*/

int BallSolver::compute( bool changedvars )
{
 lock();
 f_has = false;
 f_x.clear();

 auto ab = dynamic_cast< AbstractBlock * >( get_Block() );
 if( ! ab ) {
  unlock();
  throw( std::invalid_argument( "BallSolver::compute: not an AbstractBlock" ) );
  }

 auto obj = dynamic_cast< FRealObjective * >( ab->get_objective() );
 auto lf = obj ? dynamic_cast< LinearFunction * >( obj->get_function() )
               : nullptr;
 auto ball = ab->get_static_constraint< FRowConstraint >( "ball" );
 if( ( ! lf ) || ( ! ball ) ) {
  unlock();
  throw( std::invalid_argument( "BallSolver::compute: not a ball" ) );
  }

 const double r = std::sqrt( std::max( 0.0 , double( ball->get_rhs() ) ) );
 double nc = 0;
 for( const auto & p : lf->get_v_var() )
  nc += p.second * p.second;
 nc = std::sqrt( nc );

 const double sgn = ( obj->get_sense() == Objective::eMax ) ? -1 : 1;
 for( const auto & p : lf->get_v_var() )
  f_x.emplace_back( p.first , nc > 0 ? - sgn * r * p.second / nc : 0.0 );

 f_value = lf->get_constant_term() - sgn * r * nc;
 f_has = true;
 unlock();
 return( kOK );
 }

/*--------------------------------------------------------------------------*/

void BallSolver::get_var_solution( Configuration * solc )
{
 for( auto & p : f_x )
  p.first->set_value( p.second );
 }

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/

int n_groups = 1;         // -k : how many balls the coordinates are dealt to
double radius = -0.5;     // -r : the radius, or minus its ratio to || w_ls ||
bool regression = false;  // -g : the targets are real numbers
std::string lmo_conf;     // -L : the BlockSolverConfig of the sub-Block
int problem = 0;          // -P : 0 the least-squares fit in balls, 1 the
                          //      Wolfe dual of the SVM on boxes
int kernel = 0;           // -K : 0 linear, 2 Gaussian (the dual only)
double parC = 1;          // -C : the bound on the multipliers (the dual only)

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

static bool process_specific_arg( int opt )
{
 switch( opt ) {
  case( 'k' ): Str2Sthg( optarg , n_groups ); return( true );
  case( 'r' ): Str2Sthg( optarg , radius );   return( true );
  case( 'g' ): regression = true;             return( true );
  case( 'L' ): lmo_conf = std::string( optarg ); return( true );
  case( 'P' ): Str2Sthg( optarg , problem );  return( true );
  case( 'K' ): Str2Sthg( optarg , kernel );   return( true );
  case( 'C' ): Str2Sthg( optarg , parC );     return( true );
  default:     return( false );
  }
 }

/*--------------------------------------------------------------------------*/
/// the norm of the unconstrained least-squares fit, by Cholesky

static double ls_norm( std::vector< double > H , std::vector< double > g ,
                       Index d )
{
 // H w = g with H = X'X / n positive semidefinite: a tiny regularization
 // makes it definite, which only matters for a rank-deficient data set
 for( Index i = 0 ; i < d ; ++i )
  H[ i * d + i ] += 1e-10 * ( 1 + H[ i * d + i ] );

 for( Index j = 0 ; j < d ; ++j ) {
  double s = H[ j * d + j ];
  for( Index k = 0 ; k < j ; ++k )
   s -= H[ j * d + k ] * H[ j * d + k ];
  H[ j * d + j ] = std::sqrt( std::max( s , 1e-300 ) );
  for( Index i = j + 1 ; i < d ; ++i ) {
   double t = H[ i * d + j ];
   for( Index k = 0 ; k < j ; ++k )
    t -= H[ i * d + k ] * H[ j * d + k ];
   H[ i * d + j ] = t / H[ j * d + j ];
   }
  }
 for( Index i = 0 ; i < d ; ++i ) {       // L z = g
  for( Index k = 0 ; k < i ; ++k )
   g[ i ] -= H[ i * d + k ] * g[ k ];
  g[ i ] /= H[ i * d + i ];
  }
 for( Index i = d ; i-- > 0 ; ) {         // L' w = z
  for( Index k = i + 1 ; k < d ; ++k )
   g[ i ] -= H[ k * d + i ] * g[ k ];
  g[ i ] /= H[ i * d + i ];
  }
 double nw = 0;
 for( auto v : g )
  nw += v * v;
 return( std::sqrt( nw ) );
 }

/*--------------------------------------------------------------------------*/
/// the least-squares fit in balls: the father, its balls and their Variable

static AbstractBlock * build_balls( SVMBlock * svm , BlockSolverConfig * lbsc ,
                                    std::vector< ColVariable * > & w )
{
 const Index n = svm->get_NSamples();
 const Index m = svm->get_NFeatures();
 const Index d = m + 1;                    // the bias is the last coordinate
 const auto & X = svm->get_X();
 const auto & y = svm->get_y();

 // H = X'X / n and g = X'y / n, X with a column of 1 appended
 std::vector< double > H( d * d , 0 ) , g( d , 0 );
 double yy = 0;
 std::vector< double > row( d );
 for( Index s = 0 ; s < n ; ++s ) {
  for( Index i = 0 ; i < m ; ++i )
   row[ i ] = X[ s * m + i ];
  row[ m ] = 1;
  for( Index i = 0 ; i < d ; ++i ) {
   if( row[ i ] == 0 )
    continue;
   g[ i ] += row[ i ] * y[ s ];
   for( Index j = 0 ; j <= i ; ++j )
    H[ i * d + j ] += row[ i ] * row[ j ];
   }
  yy += y[ s ] * y[ s ];
  }
 for( Index i = 0 ; i < d ; ++i ) {
  g[ i ] /= n;
  for( Index j = 0 ; j <= i ; ++j )
   H[ j * d + i ] = ( H[ i * d + j ] /= n );
  }
 yy /= n;

 const double nls = ls_norm( H , g , d );
 const double R = ( radius > 0 ) ? radius : - radius * nls;
 cout << "data set " << get_filename( filename ) << ": " << n
      << " samples, " << m << " features, || w_ls || = " << nls
      << ", R = " << R << ", " << n_groups << " ball(s)" << endl;

 // the father and its balls - - - - - - - - - - - - - - - - - - - - - - - - -

 const Index K = Index( std::max( 1 , std::min( n_groups , int( d ) ) ) );
 const double r2 = R * R / K;
 auto father = new AbstractBlock();
 w.assign( d , nullptr );

 for( Index k = 0 , beg = 0 ; k < K ; ++k ) {
  const Index end = beg + ( d - beg ) / ( K - k );
  auto sb = new AbstractBlock( father );

  auto x = new std::vector< ColVariable >( end - beg );
  sb->add_static_variable( *x , "x" );

  DQuadFunction::v_coeff_triple sq;
  LinearFunction::v_coeff_pair zero;
  for( Index i = beg ; i < end ; ++i ) {
   w[ i ] = & ( *x )[ i - beg ];
   sq.emplace_back( w[ i ] , 0 , 1 );
   zero.emplace_back( w[ i ] , 0 );
   }
  auto ball = new FRowConstraint( sb , - Inf< double >() , r2 ,
                                  new DQuadFunction( std::move( sq ) ) );
  sb->add_static_constraint( *ball , "ball" );

  auto obj = new FRealObjective( sb , new LinearFunction( std::move( zero ) ) );
  obj->set_sense( Objective::eMin , eNoMod );
  sb->set_objective( obj );

  father->add_nested_Block( sb );
  s_config_Block( sb , lbsc , lmo_conf , false );
  beg = end;
  }

 // ( 1 / n ) || X w - y ||^2 = w' H w - 2 g' w + y'y / n
 DQuadFunction::v_coeff_triple tr( d );
 QuadFunction::v_off_diag_term nd;
 for( Index i = 0 ; i < d ; ++i ) {
  tr[ i ] = std::make_tuple( w[ i ] , Coefficient( - 2 * g[ i ] ) ,
                             Coefficient( H[ i * d + i ] ) );
  for( Index j = 0 ; j < i ; ++j )
   if( H[ i * d + j ] != 0 )
    nd.push_back( std::make_tuple( i , j , Coefficient( 2 * H[ i * d + j ] ) ) );
  }
 auto obj = new FRealObjective( father ,
                                new QuadFunction( std::move( tr ) ,
                                                  std::move( nd ) , yy ) );
 obj->set_sense( Objective::eMin , eNoMod );
 father->set_objective( obj );
 return( father );
 }

/*--------------------------------------------------------------------------*/
/// the Wolfe dual of the SVM on boxes: the father, its boxes and their Variable
/** With the bias regularised the dual has no equality constraint, and it is
 *  min { a' Q a / 2 - 1' a : 0 <= a <= C }, Q_kl = y_k y_l ( K( x_k , x_l )
 *  + 1 ); the samples are dealt to n_groups boxes, whose oracle minimizes a
 *  linear function over a box [see BoxSolver]. */

static AbstractBlock * build_dual( SVMBlock * svm , BlockSolverConfig * lbsc ,
                                   std::vector< ColVariable * > & a )
{
 const Index n = svm->get_NSamples();
 const Index m = svm->get_NFeatures();
 const auto & X = svm->get_X();
 const auto & y = svm->get_y();
 const double gamma = 1.0 / m;          // the Gaussian kernel, if any

 auto kv = [ & ]( Index k , Index l ) {
  const double * xk = X.data() + k * m;
  const double * xl = X.data() + l * m;
  double s = 0;
  if( kernel == 2 ) {
   for( Index i = 0 ; i < m ; ++i )
    s += ( xk[ i ] - xl[ i ] ) * ( xk[ i ] - xl[ i ] );
   return( std::exp( - gamma * s ) );
   }
  for( Index i = 0 ; i < m ; ++i )
   s += xk[ i ] * xl[ i ];
  return( s );
  };

 cout << "data set " << get_filename( filename ) << ": " << n
      << " samples, " << m << " features, the dual with the "
      << ( kernel == 2 ? "Gaussian" : "linear" ) << " kernel, C = " << parC
      << ", " << n_groups << " box(es)" << endl;

 const Index K = Index( std::max( 1 , std::min( n_groups , int( n ) ) ) );
 auto father = new AbstractBlock();
 a.assign( n , nullptr );

 for( Index k = 0 , beg = 0 ; k < K ; ++k ) {
  const Index end = beg + ( n - beg ) / ( K - k );
  auto sb = new AbstractBlock( father );

  auto x = new std::vector< ColVariable >( end - beg );
  sb->add_static_variable( *x , "x" );

  auto box = new std::vector< BoxConstraint >( end - beg );
  LinearFunction::v_coeff_pair zero;
  for( Index i = beg ; i < end ; ++i ) {
   a[ i ] = & ( *x )[ i - beg ];
   ( *box )[ i - beg ].set_variable( a[ i ] );
   ( *box )[ i - beg ].set_lhs( 0 );
   ( *box )[ i - beg ].set_rhs( parC );
   zero.emplace_back( a[ i ] , 0 );
   }
  sb->add_static_constraint( *box , "box" );

  auto obj = new FRealObjective( sb , new LinearFunction( std::move( zero ) ) );
  obj->set_sense( Objective::eMin , eNoMod );
  sb->set_objective( obj );

  father->add_nested_Block( sb );
  s_config_Block( sb , lbsc , lmo_conf , false );
  beg = end;
  }

 // a' Q a / 2 - 1' a: Q_kk / 2 on the diagonal, Q_kl once off it
 DQuadFunction::v_coeff_triple tr( n );
 QuadFunction::v_off_diag_term nd;
 for( Index k = 0 ; k < n ; ++k ) {
  tr[ k ] = std::make_tuple( a[ k ] , Coefficient( -1 ) ,
                             Coefficient( ( kv( k , k ) + 1 ) / 2 ) );
  for( Index l = 0 ; l < k ; ++l ) {
   const double q = y[ k ] * y[ l ] * ( kv( k , l ) + 1 );
   if( q != 0 )
    nd.push_back( std::make_tuple( k , l , Coefficient( q ) ) );
   }
  }
 auto obj = new FRealObjective( father ,
                                new QuadFunction( std::move( tr ) ,
                                                  std::move( nd ) ) );
 obj->set_sense( Objective::eMin , eNoMod );
 father->set_objective( obj );
 return( father );
 }

/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 std::set_terminate( smspp_terminate );

 docopt_desc = "SMS++ FrankWolfeSolver on a least-squares fit in balls.\n";
 short_opts += "k:r:gL:P:K:C:";
 const std::vector< option > my_opts = {
   { "groups"  , required_argument , nullptr , 'k' } ,
   { "radius"  , required_argument , nullptr , 'r' } ,
   { "regress" , no_argument       , nullptr , 'g' } ,
   { "lmocfg"  , required_argument , nullptr , 'L' } ,
   { "problem" , required_argument , nullptr , 'P' } ,
   { "kernel"  , required_argument , nullptr , 'K' } ,
   { "parC"    , required_argument , nullptr , 'C' } };
 long_opts.insert( std::prev( long_opts.end() ) ,
                   my_opts.begin() , my_opts.end() );
 help += "  -k, --groups <K>     balls the coordinates are dealt to [1]\n"
         "  -r, --radius <R>     radius of the whole; if <= 0, - R times\n"
         "                       the norm of the unconstrained fit [-0.5]\n"
         "  -g, --regress        the targets are real numbers\n"
         "  -L, --lmocfg <f>     BlockSolverConfig of the sub-Block\n"
         "  -P, --problem <p>    0 the fit in balls, 1 the SVM dual [0]\n"
         "  -K, --kernel <k>     the dual only: 0 linear, 2 Gaussian [0]\n"
         "  -C, --parC <x>       the dual only: the bound C [1]\n";

 process_args( argc , argv , process_specific_arg );

 if( filename.empty() ) {
  cerr << "Error: no data set given" << endl;
  exit( 1 );
  }
 require_solver_config();
 if( lmo_conf.empty() ) {
  cerr << "Error: no BlockSolverConfig of the sub-Block given (-L)" << endl;
  exit( 1 );
  }
 if( regression && ( problem == 1 ) ) {
  cerr << "Error: the dual is the one of the classification (no -g)" << endl;
  exit( 1 );
  }

 // the data set, read by the SVMBlock that knows its format - - - - - - - - -

 auto svm = static_cast< SVMBlock * >(
  Block::new_Block( regression ? "SVRBlock" : "SVCBlock" ) );
 {
  std::ifstream in( filename );
  if( ! in.is_open() ) {
   cerr << "Error: cannot open the data set " << filename << endl;
   exit( 1 );
   }
  svm->load( in , 'l' );
  }

 // one BlockSolverConfig for all the sub-Block, applied to each of them and
 // cleared only once, at the end, to unregister their Solver
 auto lbsc = dynamic_cast< BlockSolverConfig * >(
                              Configuration::deserialize( lmo_conf ) );
 if( ! lbsc ) {
  cerr << "Error: cannot load the BSC of the sub-Block from " << lmo_conf
       << endl;
  exit( 1 );
  }

 std::vector< ColVariable * > w;
 auto father = ( problem == 1 ) ? build_dual( svm , lbsc , w )
                                : build_balls( svm , lbsc , w );
 lbsc->clear();

 // the Solver of the father, all from the same point - - - - - - - - - - - -

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
 apply_solver_verbosity( father );

 Solution * start = father->get_Solution( nullptr , false );
 // FrankWolfeSolver read by its [ get_lb() , get_ub() ]: a run that stops
 // at its cap of iterations is then held to the bounds it gives, which are
 // what the certificate of a bundle direction is about; the reference by its
 // value, a :MILPSolver giving no meaningful lower bound on a QCP
 const SolverClassifier value = exact_getter( ObjGetter::VarValue );
 const SolverClassifier classify = [ & ]( Solver * s , std::size_t k ) {
  return( ( s->classname() == "FrankWolfeSolver" ) ? read_bounds( s , k )
                                                   : value( s , k ) );
  };
 bool ok = SolveAll( father , classify ,
                     std::numeric_limits< double >::quiet_NaN() , 1e-5 ,
                     nullptr , nullptr , nullptr , nullptr , nullptr ,
                     start );
 delete start;

 // the norm of what the last Solver has left, to tell an active ball
 if( problem == 0 ) {
  double nw = 0;
  for( auto v : w )
   nw += v->get_value() * v->get_value();
  cout << "|| w || = " << std::sqrt( nw ) << endl;
  }

 s_config_Block( father , bsc );
 delete bsc;
 for( auto sb : father->get_nested_Blocks() )
  s_config_Block( sb , lbsc , "" , false );
 delete lbsc;
 delete father;
 delete svm;

 return( ok ? 0 : 1 );
 }

/*--------------------------------------------------------------------------*/
/*--------------------------- End File test_fw.cpp -------------------------*/
/*--------------------------------------------------------------------------*/
