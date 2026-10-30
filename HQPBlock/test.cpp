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
 * - a change of b, d, A (also out of the pattern) and C of the same
 *   HQPBlock between two compute(), as a step of a Model Predictive Control
 *   scheme, checked against the reference and against a new HQPBlock with
 *   the new data solved from scratch;
 *
 * - a run of 10 steps of Model Predictive Control on the same HQPBlock,
 *   each changing b, d, A and C a little, checked against the reference.
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
 * root of the accuracy of each solve. If the file of the previous step of
 * the same run is given with -P, the step is also reached by Modification:
 * the HQPBlock of the previous step is solved, changed into this one by
 * set_b(), set_d(), set_A() and set_C(), and solved again, as the Model
 * Predictive Control does.
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
 * test that had nothing to do.
 *
 * \author Donato Meoli \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 */
/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <filesystem>
#include <map>
#include <random>

#include "common_utils.h"

#include "reference.h"

/*--------------------------------------------------------------------------*/

using namespace hqp_test;

static std::string previous;   // the file of the previous step (-P)

static int usable = 0;         // the most HQPSolver with an inner Solver

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

/*--------------------------------------------------------------------------*/
// adds the iterations of the last compute() of s to those of the steps

static void count_step( const HQPSolver * s )
{
 step_iter[ position.at( s ) ] += s->get_elapsed_iterations();
 stepped = true;
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
 for( std::size_t k = 0 ; k < sl.size() ; ++k )
  position[ sl[ k ] ] = first + k;
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
  const int st = s->compute();
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
 for( auto s : slvrs )
  s->compute();

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
  const int st = s->compute();
  const int fst = fslvrs[ i ]->compute();
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
 for( auto s : slvrs )
  s->compute();

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
   const int st = s->compute();
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

 errors += generated_in< HQPBlock >( cases );

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
  const int st = s->compute();
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
  errors += compare( what + " vs reference" , s , ref , vtol , 1e100 );
  }
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
 auto block = read_block( filename );
 if( ! block ) {
  std::cout << nm << ": cannot read the HQPBlock -> KO" << std::endl;
  return( 1 );
  }

 int errors = 0;
 const Instance in = instance_of( *block , nm );
 const RefSolution fsol = file_solution( filename , in );
 const RefSolution ref = reference( in , true , in.eps );
 if( ! ( fsol.ok && ref.ok ) ) {
  std::cout << nm << ": no solution in the file, or the reference failed"
            << " -> KO" << std::endl;
  delete block;
  return( 1 );
  }

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
   for( auto s : psl )
    s->compute();
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
// the options of this tester: -P, the file of the previous step, -I, the
// pairs whose iterations are compared, and -X, the tolerances on x

static bool process_specific_arg( int opt )
{
 switch( opt ) {
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
 short_opts += "P:I:X:";
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
         "                                  for the default\n";
 filename_optional = true;
 process_args( argc , argv , process_specific_arg );
 require_solver_config();

 int errors = filename.empty() ? generated() : from_file();
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
