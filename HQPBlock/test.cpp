/*--------------------------------------------------------------------------*/
/*------------------------------ File test.cpp -----------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Tester of HQPBlock and HQPSolver: every HQPSolver that the
 * BlockSolverConfig given with -S registers to an HQPBlock (one for each
 * inner Solver and setting) solves the same hierarchy in the same run, and
 * each of them is compared level by level with the reference solution
 * computed in the null space of the equality tasks of the higher levels
 * [see reference.h], hence with all the others.
 *
 * Without an instance file, the hierarchies are built here:
 *
 * - random hierarchies of mixed levels;
 *
 * - a hierarchy with a level of equalities only, one of inequalities only,
 *   one with both and an empty one;
 *
 * - an inequality task that cannot be satisfied at its level (positive
 *   slack, of known value) and one already satisfied (zero slack);
 *
 * - dependent rows: a repeated row, a linear combination of rows and two
 *   equal rows asking for different values, in both kinds of tasks;
 *
 * - weights different from 1;
 *
 * - hard rows [see HQPBlock::HardRows], equalities and inequalities, some
 *   active at the point that the reference starts from, with levels that
 *   conflict with them (not in an HQPDistBlock, which does not split
 *   them);
 *
 * - a change of b, d, A (also out of the pattern) and C of the same
 *   HQPBlock between two compute(), as a step of a Model Predictive Control
 *   scheme, checked against the reference and against a new HQPBlock with
 *   the new data solved from scratch;
 *
 * - a run of 10 steps of Model Predictive Control on the same HQPBlock,
 *   each changing b, d, A and C a little, checked against the reference;
 *
 * - a small Model Predictive Control with its stage structure [see
 *   HQPBlock::StageData]: two robots, four stages, the dynamics, the limits
 *   and the smoothness of the inputs, a row between the robots and their
 *   goals.
 *
 * With -Q quad or -Q conic, a hierarchy whose hard rows are quadratic or
 * conic [see HQPBlock::HardRows], active at the solution of each level,
 * is built instead and solved by all the HQPSolver, which are compared
 * with each other only (as for a file with no reference, see below), the
 * null-space reference having no such rows. With -F, a list of
 * formulations, it is loaded into an HQPBlock once for each formulation,
 * which its BlockConfig gives the abstract representation of the
 * nonlinear hard rows (with a Function or by cuts, see
 * HQPBlock::generate_abstract_constraints()) and its BlockSolverConfig its
 * HQPSolver, all of them compared with each other, those of each
 * formulation following those of the previous one in the positions of -E;
 * the hard rows of the data are then also checked at the solution of each
 * HQPSolver [see HQPBlock::is_feasible()], with the tolerance of the
 * BlockConfig.
 *
 * The HQPSolver with intProxCenter == 1 solve exactly the problem of the
 * reference, hence also x is compared; for the others only the values of
 * the levels, which are unique, are.
 *
 * The configuration given with -S is either a BlockSolverConfig, applied to
 * the HQPBlock, or a map from the classname of a Block to its
 * BlockSolverConfig [see attach()]: with an entry for HQPDistBlock, each of
 * the hierarchies built here is also loaded into an HQPDistBlock, split
 * among 3 owners [see load()], and solved (and changed, in the steps of
 * Model Predictive Control) by the HQPSolver of that entry, which are
 * compared with the same reference; they follow those of the HQPBlock in
 * the positions of -E and -I. With -F, a list of formulations [see
 * get_formulations()], each hierarchy is instead loaded into an
 * HQPDistBlock once for each formulation, which its BlockConfig gives its
 * structure [see HQPDistBlock::set_structure()] and its BlockSolverConfig
 * its HQPSolver; those of each formulation follow those of the previous one
 * in the positions.
 *
 * With an instance file, written by tools/npz2nc4.py of HQPBlock, the
 * HQPBlock in it is solved, and the values of its levels (\f$ \| v_p \|^2 /
 * 2 + \| w_p \|^2 / 2 \f$) are compared with those at the solution of each
 * level that the file carries (group HQPReference, written by the code that
 * generated the instance) and with the reference with the regularization
 * of the file on the step and on the slacks; x is printed but not checked,
 * since with a small regularization it is fixed only to about the square
 * root of the accuracy of each solve; if the reference runs out of
 * iterations (its active-set method has no rule against cycling, which the
 * degenerate levels of the longer horizons need), the values are compared
 * with the file only. If the file of the previous step of
 * the same run is given with -P, the step is also reached by Modification:
 * the HQPBlock of the previous step is solved, changed into this one by
 * set_b(), set_d(), set_A() and set_C(), and solved again, as the Model
 * Predictive Control does. The HQPSolver run one after the other on the
 * same Block, and each of them starts from the x that the Block had when
 * it was attached and then from that of its own last solve, not from that
 * left by the HQPSolver before it.
 *
 * A file without the group HQPReference, e.g., one too large for the dense
 * reference, is solved by all the HQPSolver, which are compared with each
 * other only, the values of the levels of the first one that ends with
 * kOK being those of the others; if either of the two has dblSlackTol > 0,
 * the levels after the first one where the values differ are not unique
 * (each HQPSolver can trade a level for the next ones within the tolerance
 * on the fixed slacks), and the comparison stops there, the first level
 * having to agree. The time of each compute() is printed. If the file has
 * the group HQPPoint (variable X), a point given by some other code (e.g.,
 * by tools/trots2nc4.py of HQPBlock), the values of the levels of each
 * HQPSolver must not be lexicographically larger than those at that point.
 * A file whose Block is not an HQPBlock, e.g., the weighted sum of the
 * objectives of a case of TROTS as a Linear Program (tools/trots2nc4.py
 * --weighted), is solved by the Solver of the BlockSolverConfig of its
 * classname, which are compared with each other, none of them ending above
 * the value at the point of the file (group HQPPoint, variable Value).
 *
 * The iterations of the inner Solvers of each HQPSolver [see
 * HQPSolver::get_elapsed_iterations()] are summed over the solves of the
 * steps reached by Modification (those of the run of Model Predictive
 * Control of the hierarchies built here, the step from the file of -P),
 * and -I lists pairs i:j of positions of HQPSolver in the BlockSolverConfig
 * (from 1) such that i must take fewer of them than j: e.g., an inner
 * Solver that starts from the previous solve of its level against the same
 * one that starts each solve from scratch, whose values are compared with
 * the same reference.
 *
 * The values of the levels are compared with relative tolerance (absolute
 * below 1) 1e-6 on the hierarchies built here, 1e-5 for the HQPSolver that
 * keep the rows of the lower levels in the model and on the instance
 * files; -E gives the tolerance of each HQPSolver in the order of the
 * BlockSolverConfig, in place of these, for an inner Solver that does not
 * reach them [see the batteries]. On the hierarchies built here x is
 * compared with tolerance 1e-3 (1e-2 for the HQPSolver that keep the rows
 * of the lower levels in the model), and -X gives that of each HQPSolver in
 * the same order, for an inner Solver that solves each level only up to a
 * gap: with the regularization \f$ \varepsilon \f$, a gap \f$ \delta \f$
 * on the value of a level leaves x free by about \f$ \sqrt{ 2 \delta /
 * \varepsilon } \f$.
 *
 * An HQPSolver whose inner Solver is not in the build is reported and left
 * out; if none of them has one, the tester exits with 77, the status of a
 * test that had nothing to do. An HQPSolver whose inner Solver does not
 * apply to a hierarchy, i.e., throws std::invalid_argument (e.g., one that
 * needs the stage structure, on a hierarchy that has none), is reported and
 * left out for that hierarchy.
 *
 * \author Donato Meoli \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 */
/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <chrono>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <random>

#include "common_utils.h"

#include "reference.h"

/*--------------------------------------------------------------------------*/

using namespace hqp_test;

static std::string previous;   // the file of the previous step (-P)
static std::string qcqp_kind;  // quad or conic: the hierarchy of -Q

static int usable = 0;         // the most Solver that were run

// the position of each HQPSolver in the BlockSolverConfig, those of an
// HQPDistBlock after those of the HQPBlock
static std::map< const HQPSolver * , std::size_t > position;

// the number of HQPSolver of the HQPBlock, i.e., the first position of
// those of an HQPDistBlock
static std::size_t first_dist = 0;

// the formulation of the HQPDistBlock being solved (with -F), its
// BlockConfig and BlockSolverConfig, and the number of HQPSolver of each
// formulation, those of the next one following them in the positions
static std::size_t form = 0;
static Configuration * form_bcfg = nullptr;
static Configuration * form_bsc = nullptr;
static std::vector< std::size_t > form_count;
static std::string form_name;       // with a space after, if any

// the pairs of -I: the HQPSolver at the first position (from 0) must take
// fewer iterations than that at the second one
static std::vector< std::pair< std::size_t , std::size_t > > fewer;

// the tolerances on x of -X, by position (from 0), NaN where none is given
static std::vector< double > solver_xtol;

// the iterations of the inner Solvers of each HQPSolver, by position, in the
// solves of the steps reached by Modification, and whether there are any
static std::map< std::size_t , long > step_iter;
static bool stepped = false;

// the x from which each HQPSolver starts its next compute(): that of the
// Block when it is attached, then that of its own last solve, so that an
// inner Solver starting from the current x of the Block does not start
// from the solution of the HQPSolver before it
static std::map< const HQPSolver * , std::vector< double > > own_x;

// the Block of s as an HQPLevelBlock, nullptr if it is not one
static HQPLevelBlock * level_block( const HQPSolver * s )
{
 return( dynamic_cast< HQPLevelBlock * >( s->get_Block() ) );
 }

/*--------------------------------------------------------------------------*/
// adds the iterations of the last compute() of s to those of the steps

static void count_step( const HQPSolver * s )
{
 step_iter[ position.at( s ) ] += s->get_elapsed_iterations();
 stepped = true;
 }

/*--------------------------------------------------------------------------*/
// compute() of s in st, false if its inner Solver does not apply to the
// hierarchy what (std::invalid_argument), which is reported

static bool run( HQPSolver * s , const std::string & what , int & st )
{
 const auto it = own_x.find( s );
 if( it != own_x.end() )
  level_block( s )->set_x_values( it->second );
 try {
  st = s->compute();
  }
 catch( const std::invalid_argument & e ) {
  std::cout << what << " [" << label( s ) << "]: does not apply (" << e.what()
	    << "), skipped" << std::endl;
  return( false );
  }
 if( it != own_x.end() )
  level_block( s )->get_x_values( it->second );
 return( true );
 }

/*--------------------------------------------------------------------------*/
// the tolerance on the values of the levels that -E declares for s, in the
// order of the BlockSolverConfig (one value for all of them), dflt if none

static double vtol_of( const HQPSolver * s , double dflt )
{
 const auto it = position.find( s );
 if( it == position.end() )
  return( dflt );
 const std::size_t k = ( solver_eps.size() == 1 ) ? 0 : it->second;
 if( ( k < solver_eps.size() ) && ( ! std::isnan( solver_eps[ k ] ) ) )
  return( solver_eps[ k ] );
 return( dflt );
 }

/*--------------------------------------------------------------------------*/
// the tolerance on x that -X declares for s, in the order of the
// BlockSolverConfig (one value for all of them), dflt if none

static double xtol_of( const HQPSolver * s , double dflt )
{
 const auto it = position.find( s );
 if( it == position.end() )
  return( dflt );
 const std::size_t k = ( solver_xtol.size() == 1 ) ? 0 : it->second;
 if( ( k < solver_xtol.size() ) && ( ! std::isnan( solver_xtol[ k ] ) ) )
  return( solver_xtol[ k ] );
 return( dflt );
 }

/*--------------------------------------------------------------------------*/
/*--------------------- THE HIERARCHIES BUILT HERE -------------------------*/
/*--------------------------------------------------------------------------*/
// compares s with the reference with its center of the regularization: the
// values of the levels are well conditioned, x is not (the regularization
// is all that fixes it in some directions) and an interior-point inner
// Solver gives it to about 1e-4; the rows of the lower levels left in the
// model with free slacks make the Hessian singular, which the inner Solver
// deals with less accurately, so the tolerances are then wider; the one on
// the values can be given with -E, the one on x with -X

static int check( const std::string & what , HQPSolver * s ,
                  const RefSolution & ref , const RefSolution & ref0 )
{
 const bool lower = ! s->get_int_par( HQPSolver::intRelaxLower );
 return( compare( what , s ,
                  s->get_int_par( HQPSolver::intProxCenter ) ? ref : ref0 ,
                  vtol_of( s , lower ? 1e-5 : 1e-6 ) ,
                  xtol_of( s , lower ? 1e-2 : 1e-3 ) ) );
 }

/*--------------------------------------------------------------------------*/
// a random matrix with about density * size non-zero entries

static Mat random_matrix( std::mt19937 & g , int r , int c ,
                          double density )
{
 std::uniform_real_distribution< double > u( -1 , 1 ) , z( 0 , 1 );
 Mat M = Mat::Zero( r , c );
 for( int i = 0 ; i < r ; ++i )
  for( int j = 0 ; j < c ; ++j )
   if( z( g ) < density )
    M( i , j ) = u( g );
 return( M );
 }

/*--------------------------------------------------------------------------*/

static Vec random_vector( std::mt19937 & g , int r )
{
 std::uniform_real_distribution< double > u( -1 , 1 );
 Vec v( r );
 for( int i = 0 ; i < r ; ++i )
  v( i ) = u( g );
 return( v );
 }

/*--------------------------------------------------------------------------*/

static void add_level( Instance & in , Mat A , Vec b , Mat C , Vec d )
{
 in.A.push_back( std::move( A ) );
 in.b.push_back( std::move( b ) );
 in.C.push_back( std::move( C ) );
 in.d.push_back( std::move( d ) );
 }

/*--------------------------------------------------------------------------*/

static Instance random_instance( unsigned seed )
{
 std::mt19937 g( seed );
 Instance in;
 in.name = "random " + std::to_string( seed );
 in.n = 10;
 in.eps = 1e-3;
 std::uniform_int_distribution< int > rows( 0 , 4 );
 for( int p = 0 ; p < 5 ; ++p ) {
  const int m = rows( g ) , l = rows( g );
  // small right-hand sides of the inequalities, so that some are violated
  // at the solution of the higher levels
  add_level( in , random_matrix( g , m , in.n , 0.5 ) ,
             random_vector( g , m ) , random_matrix( g , l , in.n , 0.5 ) ,
             0.3 * random_vector( g , l ) );
  }
 return( in );
 }

/*--------------------------------------------------------------------------*/
// equalities only, inequalities only, both, nothing, equalities only

static Instance kinds_instance( void )
{
 std::mt19937 g( 77 );
 Instance in;
 in.name = "kinds of levels";
 in.n = 8;
 in.eps = 1e-3;
 add_level( in , random_matrix( g , 3 , 8 , 0.6 ) , random_vector( g , 3 ) ,
            Mat( 0 , 8 ) , Vec( 0 ) );
 add_level( in , Mat( 0 , 8 ) , Vec( 0 ) , random_matrix( g , 6 , 8 , 0.6 ) ,
            -0.5 * Vec::Ones( 6 ) );
 add_level( in , random_matrix( g , 2 , 8 , 0.6 ) , random_vector( g , 2 ) ,
            random_matrix( g , 3 , 8 , 0.6 ) , 0.1 * random_vector( g , 3 ) );
 add_level( in , Mat( 0 , 8 ) , Vec( 0 ) , Mat( 0 , 8 ) , Vec( 0 ) );
 add_level( in , Mat::Identity( 8 , 8 ) , random_vector( g , 8 ) ,
            Mat( 0 , 8 ) , Vec( 0 ) );
 return( in );
 }

/*--------------------------------------------------------------------------*/
// hard rows around a random point x0: 2 equalities and 5 inequalities, 2
// of them active at x0, one with no entries; then random levels, whose
// small right-hand sides make some of them conflict with the hard rows

static Instance hard_instance( void )
{
 std::mt19937 g( 91 );
 Instance in = random_instance( 91 );
 in.name = "hard rows";
 const int n = in.n;
 in.hx0 = random_vector( g , n );
 in.hA = random_matrix( g , 2 , n , 0.5 );
 in.hb = in.hA * in.hx0;
 in.hC = random_matrix( g , 5 , n , 0.5 );
 in.hC.row( 4 ).setZero();
 Vec margin( 5 );
 margin << 0 , 0 , 0.2 , 0.5 , 1;
 in.hd = in.hC * in.hx0 + margin;
 return( in );
 }

/*--------------------------------------------------------------------------*/
// level 0 fixes x_0 = 1 and x_1 = 0; level 1 asks x_0 <= 0 (it cannot:
// slack 1, value 1/2) and x_1 <= 5 (already satisfied: slack 0); level 2
// asks x_0 + x_2 <= -2, i.e., x_2 <= -3 (satisfiable: value 0), and level 3
// x_2 = 0, which the inequality of level 2 keeps at -3 (value 9/2)

static Instance slack_instance( void )
{
 Instance in;
 in.name = "infeasible and satisfied tasks";
 in.n = 4;
 in.eps = 1e-3;
 Mat A0 = Mat::Zero( 2 , 4 );
 A0( 0 , 0 ) = 1;
 A0( 1 , 1 ) = 1;
 add_level( in , A0 , Vec::Unit( 2 , 0 ) , Mat( 0 , 4 ) , Vec( 0 ) );
 Mat C1 = Mat::Zero( 2 , 4 );
 C1( 0 , 0 ) = 1;
 C1( 1 , 1 ) = 1;
 Vec d1( 2 );
 d1 << 0 , 5;
 add_level( in , Mat( 0 , 4 ) , Vec( 0 ) , C1 , d1 );
 Mat C2 = Mat::Zero( 1 , 4 );
 C2( 0 , 0 ) = 1;
 C2( 0 , 2 ) = 1;
 add_level( in , Mat( 0 , 4 ) , Vec( 0 ) , C2 , Vec::Constant( 1 , -2 ) );
 Mat A3 = Mat::Zero( 1 , 4 );
 A3( 0 , 2 ) = 1;
 add_level( in , A3 , Vec::Zero( 1 ) , Mat( 0 , 4 ) , Vec( 0 ) );
 return( in );
 }

/*--------------------------------------------------------------------------*/
// dependent rows: in level 0 a row repeated and the sum of two rows; in
// level 1 two equal rows asking for different values (least squares: their
// mean) and a repeated inequality; in level 2 the inequalities of level 1
// again, tighter

static Instance dependent_instance( void )
{
 std::mt19937 g( 5 );
 Instance in;
 in.name = "dependent rows";
 in.n = 6;
 in.eps = 1e-3;
 Mat A0 = random_matrix( g , 2 , 6 , 0.8 );
 Mat A0d( 4 , 6 );
 A0d << A0 , A0.row( 0 ) , A0.row( 0 ) + A0.row( 1 );
 Vec b0 = random_vector( g , 2 );
 Vec b0d( 4 );
 b0d << b0 , b0( 0 ) , b0( 0 ) + b0( 1 );
 add_level( in , A0d , b0d , Mat( 0 , 6 ) , Vec( 0 ) );
 Mat r = random_matrix( g , 1 , 6 , 0.8 );
 Mat A1( 2 , 6 );
 A1 << r , r;
 Vec b1( 2 );
 b1 << 1 , 3;
 Mat c = random_matrix( g , 1 , 6 , 0.8 );
 Mat C1( 2 , 6 );
 C1 << c , c;
 add_level( in , A1 , b1 , C1 , Vec::Constant( 2 , -1 ) );
 add_level( in , Mat( 0 , 6 ) , Vec( 0 ) , C1 , Vec::Constant( 2 , -2 ) );
 add_level( in , Mat::Identity( 6 , 6 ) , Vec::Zero( 6 ) , Mat( 0 , 6 ) ,
            Vec( 0 ) );
 return( in );
 }

/*--------------------------------------------------------------------------*/

static Instance weighted_instance( void )
{
 Instance in = random_instance( 11 );
 in.name = "weights";
 std::mt19937 g( 12 );
 std::uniform_real_distribution< double > u( 0.5 , 3 );
 for( int p = 0 ; p < in.levels() ; ++p ) {
  in.we.push_back( Vec( in.b[ p ].size() ) );
  in.wi.push_back( Vec( in.d[ p ].size() ) );
  for( auto & x : in.we.back() )
   x = u( g );
  for( auto & x : in.wi.back() )
   x = u( g );
  }
 return( in );
 }

/*--------------------------------------------------------------------------*/
// two robots in the plane over N = 4 stages, s_{k+1} = s_k + u_k / 4 from
// s_0 given (the right-hand sides of the dynamics), the columns as in the
// HO-MPC package: the inputs u_{j,k} first, then the states s_{j,k+1}, each
// block by robot and then by stage. Level 0 the dynamics, 1 |u| <= 1, 2 the
// smoothness |u_k - u_{k-1}| <= 0.3 (rows of stage k on u_{k-1}), 3 the
// first coordinate of robot 0 at least 0.5 above that of robot 1 at stage
// 2 (a row on s_2 only, of stage 1), 4 the goals at every stage, beyond
// what the limits allow at the first stages

static Instance stage_instance( void )
{
 Instance in;
 in.name = "stages";
 const int R = 2 , N = 4 , nu = 2 , ns = 2;
 in.n = R * N * ( nu + ns );
 in.eps = 1e-3;
 auto ucol = [ & ]( int j , int k , int i ) {
  return( j * N * nu + k * nu + i );
  };
 auto scol = [ & ]( int j , int k , int i ) {  // s_{j,k}, k >= 1
  return( R * N * nu + j * N * ns + ( k - 1 ) * ns + i );
  };

 auto & st = in.stages;
 st.num_stages = N;
 st.var_stage.assign( in.n , 0 );
 st.var_state.assign( in.n , 0 );
 for( int j = 0 ; j < R ; ++j )
  for( int k = 0 ; k < N ; ++k )
   for( int i = 0 ; i < 2 ; ++i ) {
    st.var_stage[ ucol( j , k , i ) ] = k;
    st.var_stage[ scol( j , k + 1 , i ) ] = k + 1;
    st.var_state[ scol( j , k + 1 , i ) ] = 1;
    }

 const double s0[ 2 ][ 2 ] = { { 0 , 0 } , { 0.3 , -0.2 } };
 const double goal[ 2 ][ 2 ] = { { -1.5 , 0.8 } , { 1.6 , 1.2 } };
 const int m = R * N * ns;
 Mat A0 = Mat::Zero( m , in.n ) , C1 = Mat::Zero( 2 * m , in.n );
 Mat C2 = Mat::Zero( 2 * m , in.n ) , C3 = Mat::Zero( 1 , in.n );
 Mat A4 = Mat::Zero( m , in.n );
 Vec b0 = Vec::Zero( m ) , b4( m );
 int r = 0 , c = 0;
 for( int j = 0 ; j < R ; ++j )
  for( int k = 0 ; k < N ; ++k )
   for( int i = 0 ; i < 2 ; ++i , ++r ) {
    A0( r , scol( j , k + 1 , i ) ) = 1;
    A0( r , ucol( j , k , i ) ) = -0.25;
    if( k )
     A0( r , scol( j , k , i ) ) = -1;
    else
     b0( r ) = s0[ j ][ i ];
    A4( r , scol( j , k + 1 , i ) ) = 1;
    b4( r ) = goal[ j ][ i ];
    for( int sg = 0 ; sg < 2 ; ++sg , ++c ) {
     C1( c , ucol( j , k , i ) ) = sg ? -1 : 1;
     if( k ) {
      C2( c , ucol( j , k , i ) ) = sg ? -1 : 1;
      C2( c , ucol( j , k - 1 , i ) ) = sg ? 1 : -1;
      }
     }
    }
 C3( 0 , scol( 1 , 2 , 0 ) ) = 1;
 C3( 0 , scol( 0 , 2 , 0 ) ) = -1;
 Vec d2 = Vec::Constant( 2 * m , 0.3 );
 add_level( in , A0 , b0 , Mat( 0 , in.n ) , Vec( 0 ) );
 add_level( in , Mat( 0 , in.n ) , Vec( 0 ) , C1 , Vec::Ones( 2 * m ) );
 add_level( in , Mat( 0 , in.n ) , Vec( 0 ) , C2 , d2 );
 add_level( in , Mat( 0 , in.n ) , Vec( 0 ) , C3 , Vec::Constant( 1 , -0.5 ) );
 add_level( in , A4 , b4 , Mat( 0 , in.n ) , Vec( 0 ) );
 return( in );
 }

/*--------------------------------------------------------------------------*/
// the HQPSolver of the BlockSolverConfig that have an inner Solver; those
// that have none are reported, once; their positions are counted from that
// of the first HQPSolver of the kind of Block

static std::size_t first_of( const HQPBlock & ) { return( 0 ); }

static std::size_t first_of( const HQPDistBlock & )
{
 std::size_t first = first_dist;
 for( std::size_t f = 0 ; f < form ; ++f )
  first += form_count[ f ];
 return( first );
 }

/*--------------------------------------------------------------------------*/
// the BlockConfig of the formulation, if any, and the HQPSolver of the
// configuration of the Block

static void configure( HQPBlock & ) {}

static void configure( HQPDistBlock & block )
{
 if( form_bcfg )
  b_config_Block( & block , form_bcfg , fconf_file );
 }

static std::vector< HQPSolver * > attach_to( HQPBlock & block )
{
 return( attach( block , sconf_file ) );
 }

static std::vector< HQPSolver * > attach_to( HQPDistBlock & block )
{
 if( form_bsc )
  return( attach( block , form_bsc , fconf_file ) );
 return( attach( block , sconf_file ) );
 }

static std::vector< HQPSolver * > working( const std::vector< HQPSolver * > &
                                           sl , bool report ,
                                           std::size_t first = 0 )
{
 std::vector< HQPSolver * > res;
 if( ! first )
  first_dist = std::max( first_dist , sl.size() );
 else
  if( form_bsc ) {
   form_count.resize( std::max( form_count.size() , form + 1 ) , 0 );
   form_count[ form ] = std::max( form_count[ form ] , sl.size() );
   }
 for( std::size_t k = 0 ; k < sl.size() ; ++k ) {
  position[ sl[ k ] ] = first + k;
  if( auto hb = level_block( sl[ k ] ) )
   hb->get_x_values( own_x[ sl[ k ] ] );
  else
   own_x.erase( sl[ k ] );
  }
 for( auto s : sl )
  if( s->get_inner_Solver() )
   res.push_back( s );
  else
   if( report )
    std::cout << "[" << label( s ) << "]: no inner Solver of "
              << s->get_str_par( HQPSolver::strInnerBSC )
              << " in this build, skipped" << std::endl;
 usable = std::max( usable , int( res.size() ) );
 return( res );
 }

/*--------------------------------------------------------------------------*/
// solves in, loaded into a B, with the HQPSolver of the config for B,
// compares them with the reference, and returns the number of errors

template< class B >
static int solve_and_check( const Instance & in , bool report )
{
 const RefSolution ref = reference( in ) , ref0 = reference( in , false );
 if( ! ( ref.ok && ref0.ok ) ) {
  std::cout << in.name << ": the reference failed -> KO" << std::endl;
  return( 1 );
  }

 B block;
 load( block , in );
 configure( block );
 int errors = 0;
 for( auto s : working( attach_to( block ) , report , first_of( block ) ) ) {
  int st = 0;
  if( ! run( s , in.name , st ) )
   continue;
  if( st != Solver::kOK ) {
   std::cout << in.name << " [" << label( s ) << "]: status " << st
             << " -> KO" << std::endl;
   ++errors;
   continue;
   }
  errors += check( in.name , s , ref , ref0 );
  }

 block.unregister_Solvers( true );
 return( errors );
 }

/*--------------------------------------------------------------------------*/
// the expected values of the levels of slack_instance()

static int check_slack_values( void )
{
 Instance in = slack_instance();
 in.eps = 1e-9;
 const RefSolution ref = reference( in );
 const double expected[] = { 0 , 0.5 , 0 , 4.5 };
 int errors = 0;
 for( int p = 0 ; p < 4 ; ++p )
  if( std::abs( ref.value[ p ] - expected[ p ] ) > 1e-6 ) {
   std::cout << "reference of " << in.name << ": level " << p << " value "
             << ref.value[ p ] << " instead of " << expected[ p ]
             << " -> KO" << std::endl;
   ++errors;
   }
 if( ! errors )
  std::cout << "reference of " << in.name << ": values 0, 1/2, 0, 9/2 "
            << "-> OK" << std::endl;
 return( errors );
 }

/*--------------------------------------------------------------------------*/
// a step of Model Predictive Control: new b, d, A and C on the same B,
// compared with the reference and with a new B

template< class B >
static int mpc_step( void )
{
 Instance in = random_instance( 21 );
 in.name = form_name + "MPC step";
 B block;
 load( block , in );
 configure( block );
 const auto slvrs = working( attach_to( block ) , false ,
                             first_of( block ) );
 int errors = 0;
 int st0 = 0;
 for( auto s : slvrs )
  run( s , in.name , st0 );

 // the new data: b and d moved, A and C perturbed, an entry out of the
 // pattern of a row added and one of the pattern zeroed
 std::mt19937 g( 22 );
 Instance nw = in;
 nw.name = form_name + "MPC step, after the change";
 for( int p = 0 ; p < in.levels() ; ++p ) {
  nw.b[ p ] += 0.2 * random_vector( g , nw.b[ p ].size() );
  nw.d[ p ] += 0.2 * random_vector( g , nw.d[ p ].size() );
  for( auto * M : { & nw.A[ p ] , & nw.C[ p ] } ) {
   for( int i = 0 ; i < M->rows() ; ++i )
    for( int j = 0 ; j < M->cols() ; ++j )
     if( ( *M )( i , j ) != 0 )
      ( *M )( i , j ) *= 1 + 0.1 * random_vector( g , 1 )( 0 );
   if( M->rows() ) {
    for( int j = 0 ; j < M->cols() ; ++j )
     if( ( *M )( 0 , j ) == 0 ) {
      ( *M )( 0 , j ) = 0.5;
      break;
      }
    for( int j = M->cols() - 1 ; j >= 0 ; --j )
     if( ( *M )( M->rows() - 1 , j ) != 0 ) {
      ( *M )( M->rows() - 1 , j ) = 0;
      break;
      }
    }
   }
  std::vector< double > b( nw.b[ p ].data() ,
                           nw.b[ p ].data() + nw.b[ p ].size() );
  std::vector< double > d( nw.d[ p ].data() ,
                           nw.d[ p ].data() + nw.d[ p ].size() );
  block.set_b( p , b );
  block.set_d( p , d );
  block.set_A( p , nw.A[ p ].sparseView() );
  block.set_C( p , nw.C[ p ].sparseView() );
  }

 const RefSolution ref = reference( nw ) , ref0 = reference( nw , false );
 B fresh;
 load( fresh , nw );
 configure( fresh );
 const auto fslvrs = working( attach_to( fresh ) , false ,
                              first_of( fresh ) );

 for( std::size_t i = 0 ; i < slvrs.size() ; ++i ) {
  auto s = slvrs[ i ];
  int st = 0 , fst = 0;
  if( ! ( run( s , nw.name , st ) &&
	  run( fslvrs[ i ] , nw.name + ", from scratch" , fst ) ) )
   continue;
  if( ( st != Solver::kOK ) || ( fst != Solver::kOK ) ) {
   std::cout << nw.name << " [" << label( s ) << "]: status " << st
             << " (from scratch " << fst << ") -> KO" << std::endl;
   ++errors;
   continue;
   }
  errors += check( nw.name , s , ref , ref0 );
  errors += check( nw.name + ", from scratch" , fslvrs[ i ] , ref , ref0 );
  }

 block.unregister_Solvers( true );
 fresh.unregister_Solvers( true );
 return( errors );
 }

/*--------------------------------------------------------------------------*/
// a run of Model Predictive Control on the same B: each step moves b and d
// a little, as a reference moving along a trajectory, and A and C, as a
// model linearized at a moving point; each step is compared with the
// reference, and its iterations are those that -I compares

template< class B >
static int mpc_run( void )
{
 Instance in = random_instance( 31 );
 B block;
 load( block , in );
 configure( block );
 const auto slvrs = working( attach_to( block ) , false ,
                             first_of( block ) );
 int st0 = 0;
 for( auto s : slvrs )
  run( s , in.name , st0 );

 std::mt19937 g( 32 );
 int errors = 0;
 for( int k = 1 ; k <= 10 ; ++k ) {
  in.name = form_name + "MPC run, step " + std::to_string( k );
  for( int p = 0 ; p < in.levels() ; ++p ) {
   in.b[ p ] += 0.02 * random_vector( g , in.b[ p ].size() );
   in.d[ p ] += 0.02 * random_vector( g , in.d[ p ].size() );
   for( auto * M : { & in.A[ p ] , & in.C[ p ] } )
    for( int i = 0 ; i < M->rows() ; ++i )
     for( int j = 0 ; j < M->cols() ; ++j )
      if( ( *M )( i , j ) != 0 )
       ( *M )( i , j ) *= 1 + 0.01 * random_vector( g , 1 )( 0 );
   block.set_b( p , std::vector< double >( in.b[ p ].data() , in.b[ p ].data()
                                           + in.b[ p ].size() ) );
   block.set_d( p , std::vector< double >( in.d[ p ].data() , in.d[ p ].data()
                                           + in.d[ p ].size() ) );
   block.set_A( p , in.A[ p ].sparseView() );
   block.set_C( p , in.C[ p ].sparseView() );
   }

  const RefSolution ref = reference( in ) , ref0 = reference( in , false );
  for( auto s : slvrs ) {
   int st = 0;
   if( ! run( s , in.name , st ) )
    continue;
   count_step( s );
   if( st != Solver::kOK ) {
    std::cout << in.name << " [" << label( s ) << "]: status " << st
              << " -> KO" << std::endl;
    ++errors;
    continue;
    }
   errors += check( in.name , s , ref , ref0 );
   }
  }

 block.unregister_Solvers( true );
 return( errors );
 }

/*--------------------------------------------------------------------------*/
// all the hierarchies built here, loaded into a B

template< class B >
static int generated_in( const std::vector< Instance > & cases )
{
 int errors = 0;
 bool first = true;
 for( const auto & in : cases ) {
  errors += solve_and_check< B >( in , first );
  first = false;
  }

 if( usable ) {
  errors += mpc_step< B >();
  errors += mpc_run< B >();
  }

 return( errors );
 }

/*--------------------------------------------------------------------------*/
// all the hierarchies built here, in an HQPBlock and in an HQPDistBlock

static int generated( void )
{
 int errors = check_slack_values();

 std::vector< Instance > cases;
 for( unsigned seed = 1 ; seed <= 6 ; ++seed )
  cases.push_back( random_instance( seed ) );
 cases.push_back( kinds_instance() );
 cases.push_back( slack_instance() );
 cases.push_back( dependent_instance() );
 cases.push_back( weighted_instance() );
 cases.push_back( stage_instance() );

 // the hard rows, which an HQPDistBlock does not split
 std::vector< Instance > central = cases;
 central.push_back( hard_instance() );
 errors += generated_in< HQPBlock >( central );

 // the HQPDistBlock once for each formulation of -F, otherwise only if
 // the config has HQPSolver for it
 if( ! fconf_file.empty() ) {
  auto fv = get_formulations();
  for( form = 0 ; form < fv.configs.size() ; ++form ) {
   form_bcfg = fv.configs[ form ];
   form_bsc = solver_config_of( fv , form );
   form_name = fv.names[ form ] + " ";
   std::vector< Instance > named = cases;
   for( auto & in : named )
    in.name = fv.names[ form ] + " " + in.name;
   errors += generated_in< HQPDistBlock >( named );
   delete form_bsc;
   }
  form_bcfg = form_bsc = nullptr;
  form_name.clear();
  delete fv.owner;
  return( errors );
  }

 HQPDistBlock probe;
 load( probe , cases.front() );
 const bool dist = ! attach( probe , sconf_file ).empty();
 probe.unregister_Solvers( true );
 if( dist )
  errors += generated_in< HQPDistBlock >( cases );

 return( errors );
 }

/*--------------------------------------------------------------------------*/
/*------------------------- THE INSTANCE FILES -----------------------------*/
/*--------------------------------------------------------------------------*/

static constexpr double VTOL = 1e-5;  // default tolerance on the values

/*--------------------------------------------------------------------------*/
// the dense Instance of an HQPBlock

static Instance instance_of( const HQPBlock & block , const std::string & nm )
{
 Instance in;
 in.name = nm;
 in.n = block.get_num_var();
 in.eps = block.get_regularization();
 for( Block::Index p = 0 ; p < block.get_num_levels() ; ++p ) {
  in.A.push_back( Mat( block.get_A( p ) ) );
  in.C.push_back( Mat( block.get_C( p ) ) );
  const auto e = block.get_eq_start( p ) , i = block.get_ineq_start( p );
  in.b.push_back( Eigen::Map< const Vec >( block.get_b().data() + e ,
                                           block.get_num_eq_rows( p ) ) );
  in.d.push_back( Eigen::Map< const Vec >( block.get_d().data() + i ,
                                           block.get_num_ineq_rows( p ) ) );
  in.we.push_back( Eigen::Map< const Vec >( block.get_we().data() + e ,
                                            block.get_num_eq_rows( p ) ) );
  in.wi.push_back( Eigen::Map< const Vec >( block.get_wi().data() + i ,
                                            block.get_num_ineq_rows( p ) ) );
  }
 const auto & hr = block.get_hard_rows();
 if( hr.b.size() ) {
  in.hA = Mat( hr.A );
  in.hb = Eigen::Map< const Vec >( hr.b.data() , hr.b.size() );
  }
 if( hr.d.size() ) {
  in.hC = Mat( hr.C );
  in.hd = Eigen::Map< const Vec >( hr.d.data() , hr.d.size() );
  }
 return( in );
 }

/*--------------------------------------------------------------------------*/
// the solution of each level in the file, and the values of the levels

static RefSolution file_solution( const std::string & file ,
                                  const Instance & in )
{
 RefSolution sol;
 netCDF::NcFile f( file , netCDF::NcFile::read );
 auto g = f.getGroup( "HQPReference" );
 if( g.isNull() ) {
  sol.ok = false;
  return( sol );
  }
 Eigen::Matrix< double , Eigen::Dynamic , Eigen::Dynamic , Eigen::RowMajor >
  Xr( in.levels() , in.n );
 g.getVar( "X" ).getVar( Xr.data() );
 for( int p = 0 ; p < in.levels() ; ++p ) {
  Vec x = Xr.row( p ).transpose();
  const Vec v = in.we[ p ].cwiseProduct( in.A[ p ] * x - in.b[ p ] );
  const Vec w = in.wi[ p ].cwiseProduct( in.C[ p ] * x - in.d[ p ] )
   .cwiseMax( 0.0 );
  sol.value.push_back( ( v.squaredNorm() + w.squaredNorm() ) / 2 );
  sol.x.push_back( std::move( x ) );
  }
 return( sol );
 }

/*--------------------------------------------------------------------------*/
// solves with all the HQPSolver and compares; returns the number of errors

static int check_file( const std::string & what ,
                       const std::vector< HQPSolver * > & sl ,
                       const RefSolution & fsol , const RefSolution & ref ,
                       bool step = false )
{
 int errors = 0;
 for( auto s : sl ) {
  int st = 0;
  if( ! run( s , what , st ) )
   continue;
  if( step )
   count_step( s );
  if( st != Solver::kOK ) {
   std::cout << what << " [" << label( s ) << "]: status " << st
             << " -> KO" << std::endl;
   ++errors;
   continue;
   }
  // the tolerance on x is infinite: it is printed, not checked
  const double vtol = vtol_of( s , VTOL );
  errors += compare( what + " vs file" , s , fsol , vtol , 1e100 );
  if( ref.ok )
   errors += compare( what + " vs reference" , s , ref , vtol , 1e100 );
  }
 return( errors );
 }

/*--------------------------------------------------------------------------*/
// whether the file has the solution of each level (group HQPReference)

static bool has_reference( const std::string & file )
{
 netCDF::NcFile f( file , netCDF::NcFile::read );
 return( ! f.getGroup( "HQPReference" ).isNull() );
 }

/*--------------------------------------------------------------------------*/
// the point of the group HQPPoint of the file and the values of the levels
// of block there, computed on its sparse rows (ok == false if none)

static RefSolution file_point( const std::string & file ,
                               const HQPBlock & block )
{
 RefSolution pt;
 netCDF::NcFile f( file , netCDF::NcFile::read );
 auto g = f.getGroup( "HQPPoint" );
 if( g.isNull() ) {
  pt.ok = false;
  return( pt );
  }
 Vec x( block.get_num_var() );
 g.getVar( "X" ).getVar( x.data() );
 for( Block::Index p = 0 ; p < block.get_num_levels() ; ++p ) {
  const auto e = block.get_eq_start( p ) , i = block.get_ineq_start( p );
  const auto me = block.get_num_eq_rows( p );
  const auto mi = block.get_num_ineq_rows( p );
  const Vec v = Eigen::Map< const Vec >( block.get_we().data() + e , me )
   .cwiseProduct( Vec( block.get_A( p ) * x ) -
                  Eigen::Map< const Vec >( block.get_b().data() + e , me ) );
  const Vec w = Eigen::Map< const Vec >( block.get_wi().data() + i , mi )
   .cwiseProduct( Vec( block.get_C( p ) * x ) -
                  Eigen::Map< const Vec >( block.get_d().data() + i , mi ) )
   .cwiseMax( 0.0 );
  pt.value.push_back( ( v.squaredNorm() + w.squaredNorm() ) / 2 );
  }
 pt.x.push_back( std::move( x ) );
 return( pt );
 }

/*--------------------------------------------------------------------------*/
// checks that the values of the levels of s are not lexicographically
// larger than those at the point pt, the first level where they differ by
// more than vtol (relative) deciding; returns the number of errors

static int not_worse( const std::string & what , HQPSolver * s ,
                      const RefSolution & pt , double vtol )
{
 for( std::size_t p = 0 ; p < pt.value.size() ; ++p ) {
  const double vs = s->get_level_value( p );
  if( rel( vs , pt.value[ p ] ) <= vtol )
   continue;
  const bool ok = vs < pt.value[ p ];
  std::cout << what << " [" << label( s ) << "]: level " << p << " value "
            << vs << ( ok ? " < " : " > " ) << pt.value[ p ]
            << " at the point" << ( ok ? " -> OK" : " -> KO" ) << std::endl;
  return( ok ? 0 : 1 );
  }
 std::cout << what << " [" << label( s ) << "]: the values at the point "
           << "-> OK" << std::endl;
 return( 0 );
 }

/*--------------------------------------------------------------------------*/
// compares the levels solved by s with those of the first HQPSolver that
// ended with kOK, whose dblSlackTol is tau0; with tau the largest of the
// two dblSlackTol, a level after the first one where the values differ by
// more than vtol (relative) is not unique if tau > 0, since each HQPSolver
// fixes the slacks of the previous levels up to tau and the two can trade
// a level for the next ones: then the comparison stops there, OK if that
// level is not the first one; x is not compared; returns the number of
// errors

static int compare_peer( const std::string & what , HQPSolver * s ,
                         const RefSolution & first , double tau0 ,
                         double vtol )
{
 const double tau = std::max( tau0 ,
                              s->get_dbl_par( HQPSolver::dblSlackTol ) );
 if( tau <= 0 )
  return( compare( what , s , first , vtol , 1e100 ) );
 const std::size_t P = first.value.size();
 if( s->get_num_solved_levels() != P ) {
  std::cout << what << " [" << label( s ) << "]: "
            << s->get_num_solved_levels() << " levels solved out of " << P
            << " -> KO" << std::endl;
  return( 1 );
  }
 std::size_t p = 0;
 while( ( p < P ) &&
        ( rel( s->get_level_value( p ) , first.value[ p ] ) <= vtol ) )
  ++p;
 const bool ok = ( p > 0 );
 std::cout << what << " [" << label( s ) << "]: ";
 if( p == P )
  std::cout << "all the levels agree";
 else
  std::cout << "the levels agree up to " << p << ", where "
            << s->get_level_value( p ) << " vs " << first.value[ p ]
            << " (not unique with dblSlackTol " << tau << ")";
 std::cout << ( ok ? " -> OK" : " -> KO" ) << std::endl;
 if( ( p < P ) && ok )
  for( std::size_t h = p + 1 ; h < P ; ++h )
   std::cout << "   level " << h << ": " << s->get_level_value( h )
             << " vs " << first.value[ h ] << std::endl;
 return( ok ? 0 : 1 );
 }

/*--------------------------------------------------------------------------*/
// solves with all the HQPSolver, printing the time of each compute(), and
// compares each one with the first that ends with kOK [see compare_peer()],
// with the point pt, if any, and by also( s ), if given, which returns the
// number of its errors; returns the number of errors

static int check_peers( const std::string & what ,
                        const std::vector< HQPSolver * > & sl ,
                        const RefSolution & pt ,
                        const std::function< int( HQPSolver * ) > & also =
                        nullptr )
{
 int errors = 0;
 RefSolution first;
 std::string yard;
 double tau0 = 0;
 for( auto s : sl ) {
  int st = 0;
  const auto t0 = std::chrono::steady_clock::now();
  if( ! run( s , what , st ) )
   continue;
  const std::chrono::duration< double > dt =
   std::chrono::steady_clock::now() - t0;
  std::cout << what << " [" << label( s ) << "]: status " << st << ", "
            << s->get_elapsed_iterations() << " iterations, " << dt.count()
            << " s" << std::endl;
  if( st != Solver::kOK ) {
   std::cout << what << " [" << label( s ) << "]: status " << st
             << " -> KO" << std::endl;
   ++errors;
   continue;
   }
  const double vtol = vtol_of( s , VTOL );
  if( yard.empty() ) {
   yard = label( s );
   tau0 = s->get_dbl_par( HQPSolver::dblSlackTol );
   for( Block::Index p = 0 ; p < s->get_num_solved_levels() ; ++p ) {
    first.value.push_back( s->get_level_value( p ) );
    const auto & x = s->get_level_x( p );
    first.x.push_back( Eigen::Map< const Vec >( x.data() , x.size() ) );
    std::cout << "   level " << p << ": " << first.value.back()
              << std::endl;
    }
   }
  else
   errors += compare_peer( what + " vs " + yard , s , first , tau0 ,
                           vtol );
  if( pt.ok )
   errors += not_worse( what , s , pt , vtol );
  if( also )
   errors += also( s );
  }
 if( yard.empty() ) {
  std::cout << what << ": no HQPSolver ended with kOK -> KO" << std::endl;
  ++errors;
  }
 return( errors );
 }

/*--------------------------------------------------------------------------*/
// a file whose Block is not an HQPBlock, e.g., the weighted sum of a case
// of TROTS (tools/trots2nc4.py --weighted of HQPBlock): the Solver of the
// BlockSolverConfig of its classname that this build has are compared with
// each other, and none may end above the value at the point of the file
// (group HQPPoint, variable Value), if any; returns the number of errors

static int check_other( const std::string & nm , Block * block )
{
 using Meta = SimpleConfiguration< std::map< std::string ,
                                             Configuration * > >;
 std::unique_ptr< Configuration > c( Configuration::deserialize(
                                                         sconf_file ) );
 auto bsc = dynamic_cast< BlockSolverConfig * >( c.get() );
 if( auto m = dynamic_cast< Meta * >( c.get() ) ) {
  const auto it = m->f_value.find( block->classname() );
  bsc = ( it == m->f_value.end() ) ? nullptr :
        dynamic_cast< BlockSolverConfig * >( it->second );
  }
 if( ! bsc ) {
  std::cout << nm << ": no BlockSolverConfig of " << block->classname()
            << " in " << sconf_file << " -> KO" << std::endl;
  return( 1 );
  }
 drop_missing_Solvers( bsc , sconf_file );
 bsc->apply( block );
 usable = std::max( usable , int( block->get_registered_solvers().size() ) );

 const double nan = std::numeric_limits< double >::quiet_NaN();
 double v = nan;
 bool hs = false;
 int errors = SolveAll( block , exact_getter( ObjGetter::VarValue ) , nan ,
                        VTOL , & v , & hs , nullptr , nullptr , bsc ) ? 0
                                                                      : 1;
 netCDF::NcFile f( filename , netCDF::NcFile::read );
 auto g = f.getGroup( "HQPPoint" );
 if( ( ! g.isNull() ) && ( ! g.getVar( "Value" ).isNull() ) ) {
  double pv = nan;
  g.getVar( "Value" ).getVar( & pv );
  const bool ok = hs && ( v <= pv + VTOL * std::max( 1.0 , std::abs( pv ) ) );
  std::cout << nm << " [" << block->classname() << "]: value " << v
            << ( ok ? " <= " : " > " ) << pv << " at the point"
            << ( ok ? " -> OK" : " -> KO" ) << std::endl;
  errors += ! ok;
  }
 block->unregister_Solvers( true );
 return( errors );
 }

/*--------------------------------------------------------------------------*/
// the HQPBlock of a file, nullptr if there is none

static HQPBlock * read_block( const std::string & file )
{
 auto b = Block::deserialize( file );
 auto h = dynamic_cast< HQPBlock * >( b );
 if( ! h )
  delete b;
 return( h );
 }

/*--------------------------------------------------------------------------*/
// the instance of filename, and its step from the previous one if given

static int from_file( void )
{
 const std::string nm = std::filesystem::path( filename ).stem().string();
 auto any = Block::deserialize( filename );
 auto block = dynamic_cast< HQPBlock * >( any );
 if( ! block ) {
  if( ! any ) {
   std::cout << nm << ": cannot read the Block -> KO" << std::endl;
   return( 1 );
   }
  const int errors = check_other( nm , any );
  delete any;
  return( errors );
  }

 int errors = 0;
 if( ! has_reference( filename ) ) {
  if( ! previous.empty() )
   std::cout << nm << ": no HQPReference, -P ignored" << std::endl;
  errors = check_peers( nm , working( attach( *block , sconf_file ) , true ) ,
                        file_point( filename , *block ) );
  block->unregister_Solvers( true );
  delete block;
  return( errors );
  }

 Instance in = instance_of( *block , nm );
 const RefSolution fsol = file_solution( filename , in );
 if( ! fsol.ok ) {
  std::cout << nm << ": no solution in the file -> KO" << std::endl;
  delete block;
  return( 1 );
  }
 // the solution of level 0 in the file satisfies the hard rows, if any
 if( block->has_hard_rows() && in.levels() )
  in.hx0 = fsol.x[ 0 ];
 const RefSolution ref = reference( in , true , in.eps );
 // the solution in the file is a reference of its own
 if( ! ref.ok )
  std::cout << nm << ": the null-space reference runs out of iterations, "
	    << "the HQPSolver are compared with the file only" << std::endl;

 errors += check_file( nm , working( attach( *block , sconf_file ) , true ) ,
                       fsol , ref );

 // the same step reached by Modification of the HQPBlock of the previous
 if( ! previous.empty() ) {
  auto prev = read_block( previous );
  if( ! prev ) {
   std::cout << previous << ": cannot read the HQPBlock -> KO" << std::endl;
   ++errors;
   }
  else {
   const auto psl = working( attach( *prev , sconf_file ) , false );
   int st0 = 0;
   for( auto s : psl )
    run( s , previous , st0 );
   for( Block::Index p = 0 ; p < block->get_num_levels() ; ++p ) {
    const auto e = block->get_eq_start( p ) , i = block->get_ineq_start( p );
    prev->set_b( p , std::vector< double >( block->get_b().begin() + e ,
                     block->get_b().begin() + e +
                     block->get_num_eq_rows( p ) ) );
    prev->set_d( p , std::vector< double >( block->get_d().begin() + i ,
                     block->get_d().begin() + i +
                     block->get_num_ineq_rows( p ) ) );
    prev->set_A( p , block->get_A( p ) );
    prev->set_C( p , block->get_C( p ) );
    }
   const auto & hr = block->get_hard_rows();
   if( hr.b.size() ) {
    prev->set_hard_b( hr.b );
    prev->set_hard_A( hr.A );
    }
   if( hr.d.size() ) {
    prev->set_hard_d( hr.d );
    prev->set_hard_C( hr.C );
    }
   errors += check_file( nm + " after the step" , psl , fsol , ref , true );
   prev->unregister_Solvers( true );
   delete prev;
   }
  }

 block->unregister_Solvers( true );
 delete block;
 return( errors );
 }

/*--------------------------------------------------------------------------*/
/*---------------------------- THE ITERATIONS ------------------------------*/
/*--------------------------------------------------------------------------*/
// prints the iterations of each HQPSolver in the steps reached by
// Modification and checks the pairs of -I; returns the number of errors

static int check_iterations( void )
{
 if( ! stepped ) {
  if( ! fewer.empty() )
   std::cout << "no step reached by Modification: the iterations are not "
             << "compared" << std::endl;
  return( 0 );
  }

 for( const auto & [ k , it ] : step_iter )
  std::cout << "iterations of the HQPSolver " << k + 1 << " in the steps "
            << "reached by Modification: " << it << std::endl;

 int errors = 0;
 for( const auto & [ i , j ] : fewer ) {
  const auto a = step_iter.find( i ) , b = step_iter.find( j );
  if( ( a == step_iter.end() ) || ( b == step_iter.end() ) ) {
   std::cout << "HQPSolver " << i + 1 << " and " << j + 1 << ": not both "
             << "with an inner Solver, iterations not compared" << std::endl;
   continue;
   }
  const bool ok = a->second < b->second;
  std::cout << "iterations in the steps: HQPSolver " << i + 1 << " "
            << a->second << ( ok ? " < " : " >= " ) << "HQPSolver " << j + 1
            << " " << b->second << ( ok ? " -> OK" : " -> KO" ) << std::endl;
  if( ! ok )
   ++errors;
  }
 return( errors );
 }

/*--------------------------------------------------------------------------*/
/*--------------------------------- MAIN -----------------------------------*/
/*--------------------------------------------------------------------------*/
// the pairs i:j of -I, separated by commas, positions from 1

static void parse_fewer( const std::string & arg )
{
 fewer.clear();
 std::size_t pos = 0;
 while( pos < arg.size() ) {
  std::size_t next = arg.find( ',' , pos );
  if( next == std::string::npos )
   next = arg.size();
  const std::string pair = arg.substr( pos , next - pos );
  const auto colon = pair.find( ':' );
  std::size_t i = 0 , j = 0 , ri = 0 , rj = 0;
  try {
   if( colon != std::string::npos ) {
    i = std::stoul( pair.substr( 0 , colon ) , & ri );
    j = std::stoul( pair.substr( colon + 1 ) , & rj );
    }
   }
  catch( ... ) {
   i = 0;
   }
  if( ( colon == std::string::npos ) || ( ri != colon ) ||
      ( rj != pair.size() - colon - 1 ) || ( i == 0 ) || ( j == 0 ) )
   throw( std::invalid_argument( "-I: " + pair + " is not a pair i:j of "
                                 "positions from 1" ) );
  fewer.emplace_back( i - 1 , j - 1 );
  pos = next + 1;
  }
 }

/*--------------------------------------------------------------------------*/
// the tolerances of -X, separated by commas, an empty one for the default

static void parse_xtol( const std::string & arg )
{
 solver_xtol.clear();
 std::size_t pos = 0;
 for( ; ; ) {
  std::size_t next = arg.find( ',' , pos );
  if( next == std::string::npos )
   next = arg.size();
  const std::string v = arg.substr( pos , next - pos );
  double t = std::numeric_limits< double >::quiet_NaN();
  if( ! v.empty() ) {
   std::size_t r = 0;
   try {
    t = std::stod( v , & r );
    }
   catch( ... ) {
    r = 0;
    }
   if( ( r != v.size() ) || ( ! ( t > 0 ) ) )
    throw( std::invalid_argument( "-X: " + v + " is not a positive "
                                  "tolerance" ) );
   }
  solver_xtol.push_back( t );
  if( next == arg.size() )
   break;
  pos = next + 1;
  }
 }

/*--------------------------------------------------------------------------*/
// with -Q quad or -Q conic: a hierarchy in 4 columns whose levels ask for
// points outside the set of its hard rows, which are therefore active at
// the solution of each level: x >= -10 and, with quad, the balls
// || x - ( 1 , 1 , 0 , 0 ) ||^2 <= 4 and x_0^2 + 3 x_1^2 <= 5, with conic,
// sum_k exp( x_k - 1 ) <= 3 and || x ||_3 <= 2.5

static void load_qcqp( HQPBlock & block )
{
 using SM = HQPBlock::SparseMatrix;
 const HQPBlock::Index n = 4;
 Mat C0( 2 , n ) , A1( 2 , n ) , A2( n , n );
 C0 << -1 , -1 , 0 , 0 ,          // level 0: x_0 + x_1 >= 6
        0 , 0 , -1 , 0;           //          x_2 >= 3
 A1 << 1 , -1 , 0 , 0 ,           // level 1: x_0 - x_1 = 4
       0 , 0 , 1 , 1;             //          x_2 + x_3 = -5
 A2.setIdentity();                // level 2: x = ( 3 , -3 , 3 , 3 )
 std::vector< SM > A = { SM( 0 , n ) , A1.sparseView() , A2.sparseView() };
 std::vector< SM > C = { C0.sparseView() , SM( 0 , n ) , SM( 0 , n ) };
 std::vector< HQPBlock::Vec_double > b = { {} , { 4 , -5 } ,
                                           { 3 , -3 , 3 , 3 } };
 std::vector< HQPBlock::Vec_double > d = { { -6 , -3 } , {} , {} };

 HQPBlock::HardRows hr;
 Mat I( n , n );
 I.setIdentity();
 hr.C = SM( ( -I ).sparseView() );
 hr.d.assign( n , 10 );
 if( qcqp_kind == "quad" ) {
  Mat Q1( n , n );
  Q1.setZero();
  Q1( 0 , 0 ) = 2;
  Q1( 1 , 1 ) = 6;
  hr.Q = { SM( ( 2 * I ).sparseView() ) , SM( Q1.sparseView() ) };
  Mat Cq( 2 , n );
  Cq << -2 , -2 , 0 , 0 ,
         0 , 0 , 0 , 0;
  hr.Cq = Cq.sparseView();
  hr.eq = { 2 , 5 };
  }
 else {
  hr.SE = { SM( I.sparseView() ) };
  hr.SEb = { HQPBlock::Vec_double( n , -1 ) };
  hr.SEc = { HQPBlock::Vec_double( n , 1 ) };
  hr.SEl = SM( 1 , n );
  hr.se = { 3 };
  hr.PN = { SM( I.sparseView() ) };
  hr.PNb = { HQPBlock::Vec_double( n , 0 ) };
  hr.PNp = { 3 };
  hr.PNs = { 1 };
  hr.PNl = SM( 1 , n );
  hr.pn = { 2.5 };
  }

 block.load( n , std::move( A ) , std::move( b ) , std::move( C ) ,
             std::move( d ) , {} , {} , 1e-6 , {} , {} , std::move( hr ) );
 }

/*--------------------------------------------------------------------------*/
// the hierarchy of -Q, solved by all the HQPSolver, which are compared with
// each other only, the null-space reference having no such rows: with -F,
// in an HQPBlock for each formulation, configured by its BlockConfig and
// solved by the HQPSolver of its BlockSolverConfig, and the hard rows of
// the data are checked at the solution of each HQPSolver [see
// HQPBlock::is_feasible()] with the tolerance of the BlockConfig; without
// -F, in one HQPBlock with the HQPSolver of -S

static int generated_qcqp( void )
{
 RefSolution none;
 none.ok = false;
 const std::string nm = qcqp_kind + " hard rows";

 if( fconf_file.empty() ) {
  HQPBlock block;
  load_qcqp( block );
  const int errors = check_peers( nm , working( attach( block ,
                                                        sconf_file ) ,
                                                true ) , none );
  block.unregister_Solvers( true );
  return( errors );
  }

 auto fv = get_formulations();
 std::vector< std::unique_ptr< HQPBlock > > blocks;
 std::vector< Configuration * > bscs;
 std::vector< HQPSolver * > all;
 std::size_t first = 0;  // the position of the first HQPSolver of k
 for( std::size_t k = 0 ; k < fv.configs.size() ; ++k ) {
  blocks.push_back( std::make_unique< HQPBlock >() );
  auto & block = *blocks.back();
  load_qcqp( block );
  b_config_Block( & block , fv.configs[ k ] , fconf_file );
  bscs.push_back( solver_config_of( fv , k ) );
  const auto sl = attach( block , bscs.back() , fconf_file );
  for( auto s : working( sl , true , first ) )
   all.push_back( s );
  first += sl.size();
  }

 // the hard rows at the x of the last level of s
 const auto feasible = [ & ]( HQPSolver * s ) -> int {
  const bool ok = static_cast< HQPBlock * >( s->get_Block() )->is_feasible();
  std::cout << nm << " [" << label( s ) << "]: the hard rows at its "
            << "solution" << ( ok ? " -> OK" : " -> KO" ) << std::endl;
  return( ok ? 0 : 1 );
  };
 const int errors = check_peers( nm , all , none , feasible );

 for( auto & b : blocks )
  b->unregister_Solvers( true );
 for( auto c : bscs )
  delete c;
 delete fv.owner;
 return( errors );
 }

/*--------------------------------------------------------------------------*/
// the options of this tester: -P, the file of the previous step, -I, the
// pairs whose iterations are compared, -X, the tolerances on x, and -Q,
// the hierarchies with quadratic or conic hard rows

static bool process_specific_arg( int opt )
{
 switch( opt ) {
  case( 'Q' ):
   qcqp_kind = optarg;
   if( ( qcqp_kind != "quad" ) && ( qcqp_kind != "conic" ) )
    throw( std::invalid_argument( "-Q takes quad or conic" ) );
   return( true );
  case( 'P' ):
   previous = optarg;
   return( true );
  case( 'I' ):
   parse_fewer( optarg );
   return( true );
  case( 'X' ):
   parse_xtol( optarg );
   return( true );
  default:
   return( false );
  }
 }

/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 docopt_desc = "SMS++ HQPBlock test: the HQPSolver of the BlockSolverConfig "
               "against the\nnull-space reference, on the hierarchies built "
               "here or on the instance file.\n";
 short_opts += "P:I:X:Q:";
 long_opts.insert( std::prev( long_opts.end() ) ,
                   { "qcqp" , required_argument , nullptr , 'Q' } );
 long_opts.insert( std::prev( long_opts.end() ) ,
                   { "previous" , required_argument , nullptr , 'P' } );
 long_opts.insert( std::prev( long_opts.end() ) ,
                   { "fewer" , required_argument , nullptr , 'I' } );
 long_opts.insert( std::prev( long_opts.end() ) ,
                   { "xtol" , required_argument , nullptr , 'X' } );
 help += "  -P, --previous <file>           the instance of the previous "
         "step, also\n"
         "                                  changed into this one by "
         "Modification\n"
         "  -I, --fewer <i>:<j>[,...]       the HQPSolver at position i "
         "(from 1) takes\n"
         "                                  fewer iterations than that at "
         "j in the steps\n"
         "                                  reached by Modification\n"
         "  -X, --xtol <tol>[,...]          the tolerance on x of each "
         "HQPSolver, in the\n"
         "                                  order of the BlockSolverConfig, "
         "an empty one\n"
         "                                  for the default\n"
         "  -Q, --qcqp <quad|conic>         the hierarchy with quadratic or "
         "conic hard\n"
         "                                  rows built here, the HQPSolver "
         "compared with\n"
         "                                  each other only\n";
 filename_optional = true;
 process_args( argc , argv , process_specific_arg );
 if( qcqp_kind.empty() || fconf_file.empty() )
  require_solver_config();

 int errors = ( ! qcqp_kind.empty() ) ? generated_qcqp()
              : ( filename.empty() ? generated() : from_file() );
 errors += check_iterations();

 if( ( ! errors ) && ( ! usable ) ) {
  std::cout << "no HQPSolver with an inner Solver: nothing to do"
            << std::endl;
  return( 77 );
  }

 std::cout << ( errors ? "HQPBlock: FAILED" : "HQPBlock: all tests passed" )
           << std::endl;
 return( errors ? 1 : 0 );
 }

/*--------------------------------------------------------------------------*/
/*---------------------------- End File test.cpp ---------------------------*/
/*--------------------------------------------------------------------------*/
