/*--------------------------------------------------------------------------*/
/*-------------------------- File test.cpp ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Tester of the Solver of a MasterProblemBlock on synthetic master problems
 * of a bundle method.
 *
 * The master problem has n variables and K hard components, each a
 * PolyhedralFunctionBlock whose rows are cuts F_k( x ) >= b + g x and, with
 * -V, vertical rows b + g x <= 0, and it can have a lower bound on each
 * component (-L), a level (-l, the doubly stabilized master), a box on the
 * variables (-x) and a linear part (-b). The cuts are those that a bundle
 * method would give: at the stability centre x_bar each diagonal cut is
 * below the value F_k( x_bar ), i.e., it has a nonnegative linearization
 * error, and each vertical row holds.
 *
 * The file of -S is a SimpleConfiguration< std::vector< Configuration * > >
 * of BlockSolverConfig, each of which gets a MasterProblemBlock of its own
 * with the same data; all of them are solved, and each Solver is compared
 * with the first one in the status, in the value of the master problem and
 * in the step d; the value is computed by the tester out of d and of its
 * data, which d is also checked to satisfy (the vertical rows and the box).
 * A Solver that throws std::invalid_argument on the master problem, i.e.,
 * that does not handle it, is reported as "n.a." and left out from then
 * on. Then, for a number of rounds, the master problems are changed in the
 * same way and solved again; what is changed is coded bit-wise in -k: 1
 * cuts added, 2 cuts removed, 4 the centre moved to x_bar + d of the first
 * Solver (the serious step), with the values F_k( x_bar ) at the new
 * centre, 8 t, 16 the box, 32 the lower bounds, 64 the level and 128 the
 * linear part.
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
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

#include "common_utils.h"

#include "BlockSolverConfig.h"
#include "MasterProblemBlock.h"

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace std;

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/

static int nvar = 10;     ///< number of variables (-N)
static int ncmp = 1;      ///< number of hard components (-K)
static int ncut = 5;      ///< initial cuts of each component (-M)
static double pvert = 0;  ///< probability that a cut is vertical (-V)
static int lbmode = 0;    ///< 1: a lower bound on each component (-L)
static int level = 0;     ///< 1: the doubly stabilized master (-l)
static int boxmode = 0;   ///< 0 none, 1 half the variables, 2 all (-x)
static int linpart = 0;   ///< 1: a linear part b (-b)

static const double vtol = 1e-6;  ///< relative tolerance on the value
static const double dtol = 1e-5;  ///< relative tolerance on d

static std::mt19937 rg;
static std::uniform_real_distribution< double > U( -1 , 1 );
static std::uniform_real_distribution< double > Z( 0 , 1 );

/*--------------------------------------------------------------------------*/
// the data of the master problem, the same for all the Solver

struct Cut
{
 int k;                    ///< the component
 int slot;                 ///< the name of the cut in the master problem
 std::vector< double > g;  ///< the coefficients
 double b;                 ///< the constant
 bool vert;                ///< true if a vertical row
 };

static int bsize;                     ///< slots of each component
static std::vector< Cut > cuts;       ///< the cuts in the master problem
static std::vector< int > free_slot;  ///< the slots not in use
static std::vector< double > x_bar;   ///< the stability centre
static std::vector< double > F;       ///< F_k( x_bar )
static std::vector< double > LB;      ///< the lower bounds
static std::vector< double > L , Ub;   ///< the box
static std::vector< double > bvec;    ///< the linear part
static double t = 1;                  ///< the proximal parameter
static double lev = Inf< double >();  ///< the level

/*--------------------------------------------------------------------------*/
// one MasterProblemBlock for each BlockSolverConfig

struct Master
{
 MasterProblemBlock * mpb;
 Configuration * bsc;
 std::string name;
 bool na = false;  ///< true if its Solver does not handle the master
 int status = 0;
 double value = 0;  ///< the value of the master problem at d
 double viol = 0;   ///< the violation of the rows and of the box by d
 std::vector< double > d;
 };

static std::vector< Master > masters;

/*--------------------------------------------------------------------------*/

static double dot( const std::vector< double > & a ,
		   const std::vector< double > & x )
{
 double s = 0;
 for( int j = 0 ; j < int( x.size() ) ; ++j )
  s += a[ j ] * x[ j ];
 return( s );
 }

/*--------------------------------------------------------------------------*/
// F_k( x_bar ): above all the diagonal cuts of k, and above its lower bound

static void set_values( void )
{
 for( int k = 0 ; k < ncmp ; ++k ) {
  double f = -Inf< double >();
  for( const auto & c : cuts )
   if( ( c.k == k ) && ( ! c.vert ) )
    f = std::max( f , c.b + dot( c.g , x_bar ) );
  if( f == -Inf< double >() )
   f = 0;
  f += Z( rg );
  if( ( LB.size() == std::size_t( ncmp ) ) && ( LB[ k ] > f ) )
   f = LB[ k ] + Z( rg );
  F[ k ] = f;
  }
 }

/*--------------------------------------------------------------------------*/
// a new cut of component k, with a nonnegative linearization error at x_bar
// if diagonal, holding at x_bar if vertical

static Cut new_cut( int k )
{
 Cut c;
 c.k = k;
 c.slot = free_slot.back();
 free_slot.pop_back();
 c.g.resize( nvar );
 for( auto & gj : c.g )
  gj = U( rg );
 c.vert = Z( rg ) < pvert;
 const double e = Z( rg );
 c.b = ( c.vert ? 0 : F[ k ] ) - dot( c.g , x_bar ) - e;
 return( c );
 }

/*--------------------------------------------------------------------------*/

static void new_box( void )
{
 L.assign( nvar , -Inf< double >() );
 Ub.assign( nvar , Inf< double >() );
 for( int j = 0 ; j < nvar ; ++j )
  if( ( boxmode == 2 ) || ( ( boxmode == 1 ) && ( j % 2 ) ) ) {
   L[ j ] = x_bar[ j ] - Z( rg );
   Ub[ j ] = x_bar[ j ] + Z( rg );
   }
 }

/*--------------------------------------------------------------------------*/

static void new_LB( void )
{
 LB.assign( ncmp , -Inf< double >() );
 if( lbmode )
  for( int k = 0 ; k < ncmp ; ++k )
   LB[ k ] = F[ k ] - 1 - 3 * Z( rg );
 }

/*--------------------------------------------------------------------------*/
// a level below the value at x_bar, which asks for a decrease

static void new_level( void )
{
 lev = Inf< double >();
 if( level ) {
  double f = 0;
  for( double fk : F )
   f += fk;
  lev = f - 0.1 - Z( rg );
  }
 }

/*--------------------------------------------------------------------------*/

static void generate( void )
{
 bsize = 3 * ncut + 10;
 free_slot.clear();
 for( int s = ncmp * bsize - 1 ; s >= 0 ; --s )
  free_slot.push_back( s );
 x_bar.resize( nvar );
 for( auto & xj : x_bar )
  xj = U( rg );
 F.assign( ncmp , 0 );
 set_values();
 for( int k = 0 ; k < ncmp ; ++k )
  for( int i = 0 ; i < ncut ; ++i )
   cuts.push_back( new_cut( k ) );
 set_values();
 new_LB();
 new_box();
 bvec.assign( nvar , 0 );
 if( linpart )
  for( auto & bj : bvec )
   bj = 0.5 * U( rg );
 t = 0.1 + 10 * Z( rg );
 new_level();
 }

/*--------------------------------------------------------------------------*/
// the MasterProblemBlock of a BlockSolverConfig, with the data above

static MasterProblemBlock * build( Configuration * bsc )
{
 auto mpb = new MasterProblemBlock();
 mpb->configure( true , ncmp * bsize , nvar , ncmp , {} , {} ,
		 level ? MasterProblemBlock::kDoublyStabilized
		       : MasterProblemBlock::kProximal );
 mpb->generate_abstract_variables();
 mpb->generate_abstract_constraints();
 if( linpart )
  mpb->set_linear_part( bvec );
 mpb->generate_objective();
 mpb->set_box( L , Ub );
 static_cast< BlockSolverConfig * >( bsc )->apply( mpb );
 mpb->set_t( t );
 mpb->set_reference( x_bar , F );
 for( int k = 0 ; k < ncmp ; ++k )
  mpb->set_LB( k , LB[ k ] );
 if( level )
  mpb->set_f_lev( lev );
 for( const auto & c : cuts )
  mpb->add_cut( c.k , c.slot , std::vector< double >( c.g ) , c.b , c.vert );
 return( mpb );
 }

/*--------------------------------------------------------------------------*/
// the same change on every master problem

template< class F_t >
static void on_all( F_t f )
{
 for( auto & m : masters )
  f( m.mpb );
 }

/*--------------------------------------------------------------------------*/

static void change( void )
{
 const auto & r = round_opts;
 std::uniform_int_distribution< int > kk( 0 , ncmp - 1 );
 auto happens = [ & ]( Block::Index bit ) {
  return( ( r.wchg & bit ) && ( Z( rg ) < r.p_change ) );
  };

 // the serious step first, since the cuts are built at the centre
 if( happens( 4 ) && ( ! masters[ 0 ].na ) &&
     ( ( masters[ 0 ].status == Solver::kOK ) ||
       ( masters[ 0 ].status == Solver::kLowPrecision ) ) ) {
  // in the box, which the step of the Solver keeps only to its tolerance
  for( int j = 0 ; j < nvar ; ++j )
   x_bar[ j ] =
     std::clamp( x_bar[ j ] + masters[ 0 ].d[ j ] , L[ j ] , Ub[ j ] );
  set_values();
  on_all( [ & ]( MasterProblemBlock * m ) { m->set_reference( x_bar , F ); } );
  if( level ) {
   new_level();
   on_all( [ & ]( MasterProblemBlock * m ) { m->set_f_lev( lev ); } );
   }
  }

 if( happens( 2 ) )
  for( Block::Index h = 0 ; h < r.n_change ; ++h ) {
   if( cuts.empty() )
    break;
   std::uniform_int_distribution< int > ii( 0 , int( cuts.size() ) - 1 );
   const int i = ii( rg );
   const Cut c = cuts[ i ];
   // every component keeps a diagonal cut
   int diag = 0;
   for( const auto & o : cuts )
    diag += ( o.k == c.k ) && ( ! o.vert );
   if( ( ! c.vert ) && ( diag <= 1 ) )
    continue;
   on_all( [ & ]( MasterProblemBlock * m ) { m->remove_cut( c.k ,
							    c.slot ); } );
   free_slot.push_back( c.slot );
   cuts.erase( cuts.begin() + i );
   }

 if( happens( 1 ) )
  for( Block::Index h = 0 ; h < r.n_change ; ++h ) {
   if( free_slot.empty() )
    break;
   const Cut c = new_cut( kk( rg ) );
   on_all( [ & ]( MasterProblemBlock * m ) {
    m->add_cut( c.k , c.slot , std::vector< double >( c.g ) , c.b , c.vert );
    } );
   cuts.push_back( c );
   }

 if( happens( 8 ) ) {
  t = 0.1 + 10 * Z( rg );
  on_all( [ & ]( MasterProblemBlock * m ) { m->set_t( t ); } );
  }

 if( boxmode && happens( 16 ) ) {
  new_box();
  on_all( [ & ]( MasterProblemBlock * m ) { m->set_box( L , Ub ); } );
  }

 if( lbmode && happens( 32 ) ) {
  new_LB();
  on_all( [ & ]( MasterProblemBlock * m ) {
   for( int k = 0 ; k < ncmp ; ++k )
    m->set_LB( k , LB[ k ] );
   } );
  }

 if( level && happens( 64 ) ) {
  new_level();
  on_all( [ & ]( MasterProblemBlock * m ) { m->set_f_lev( lev ); } );
  }

 if( linpart && happens( 128 ) ) {
  for( auto & bj : bvec )
   bj = 0.5 * U( rg );
  on_all( [ & ]( MasterProblemBlock * m ) { m->set_linear_part( bvec ); } );
  }
 }

/*--------------------------------------------------------------------------*/
// the value of the master problem at the step d, out of the data of the
// tester and not of the Solver: b d + sum_k max( LB_k , max_i b_i + g_i x )
// + || d ||^2 / 2 t at x = x_bar + d, and the largest violation by x of a
// vertical row or of the box, relative to the largest | x_j | (and 1)

static double value_of( const std::vector< double > & d , double & viol )
{
 std::vector< double > x( x_bar );
 double v = 0 , dd = 0;
 for( int j = 0 ; j < nvar ; ++j ) {
  x[ j ] += d[ j ];
  v += bvec[ j ] * d[ j ];
  dd += d[ j ] * d[ j ];
  viol = std::max( { viol , L[ j ] - x[ j ] , x[ j ] - Ub[ j ] } );
  }
 double xn = 1;
 for( double xj : x )
  xn = std::max( xn , std::abs( xj ) );
 std::vector< double > m( LB );
 for( const auto & c : cuts ) {
  const double r = c.b + dot( c.g , x );
  if( c.vert )
   viol = std::max( viol , r );
  else
   m[ c.k ] = std::max( m[ c.k ] , r );
  }
 for( double mk : m )
  v += mk;
 viol /= xn;  // relative to the size of x
 return( v + dd / ( 2 * t ) );
 }

/*--------------------------------------------------------------------------*/
// solves all the master problems and compares them with the first one

static bool solve( const std::string & what )
{
 for( auto & m : masters ) {
  if( m.na )
   continue;
  try {
   m.status = m.mpb->solve_master();
   }
  catch( const std::invalid_argument & e ) {
   m.na = true;
   if( verbosity_level > 0 )
    cout << m.name << " n.a.: " << e.what() << endl;
   continue;
   }
  const bool ok =
    ( m.status == Solver::kOK ) || ( m.status == Solver::kLowPrecision );
  m.d = ok ? m.mpb->get_d_vector() : std::vector< double >();
  m.viol = 0;
  m.value = ok ? value_of( m.d , m.viol ) : std::nan( "" );
  }

 const auto & ref = masters[ 0 ];
 const bool rok = ( ! ref.na ) && ( ( ref.status == Solver::kOK ) ||
				    ( ref.status == Solver::kLowPrecision ) );
 bool AllPassed = true;
 std::string verdict;
 cout << what << ":";
 for( const auto & m : masters ) {
  cout << " " << m.name << " = ";
  if( m.na ) {
   cout << "n.a.";
   continue;
   }
  const bool ok =
    ( m.status == Solver::kOK ) || ( m.status == Solver::kLowPrecision );
  if( ok )
   cout << setprecision( 10 ) << m.value;
  else
   cout << "status " << m.status;
  if( ok && ( m.viol > dtol ) )
   cout << " (violation " << m.viol << ")";
  bool same = ( ! ok ) || ( m.viol <= dtol );
  if( &m == &ref ) {
   if( ! same )
    verdict += " KO";
   AllPassed &= same;
   continue;
   }
  same = same && ( ok == rok ) && ( ok || ( m.status == ref.status ) );
  if( same && ok ) {
   same = std::abs( m.value - ref.value ) <=
	  vtol * std::max( 1.0 , std::abs( ref.value ) );
   double dd = 0 , dn = 0;
   for( int j = 0 ; j < nvar ; ++j ) {
    dd = std::max( dd , std::abs( m.d[ j ] - ref.d[ j ] ) );
    dn = std::max( dn , std::abs( ref.d[ j ] ) );
    }
   same = same && ( dd <= dtol * std::max( 1.0 , dn ) );
   }
  verdict += same ? " OK" : " KO";
  AllPassed &= same;
  }
 cout << " ->" << ( verdict.empty() ? " nothing to compare" : verdict )
      << endl;
 return( AllPassed );
 }

/*--------------------------------------------------------------------------*/

static bool process_specific_arg( int opt )
{
 if( process_round_option( opt ) )
  return( true );
 switch( opt ) {
  case( 'N' ):
   Str2Sthg( optarg , nvar );
   return( true );
  case( 'K' ):
   Str2Sthg( optarg , ncmp );
   return( true );
  case( 'M' ):
   Str2Sthg( optarg , ncut );
   return( true );
  case( 'V' ):
   Str2Sthg( optarg , pvert );
   return( true );
  case( 'L' ):
   Str2Sthg( optarg , lbmode );
   return( true );
  case( 'l' ):
   Str2Sthg( optarg , level );
   return( true );
  case( 'x' ):
   Str2Sthg( optarg , boxmode );
   return( true );
  case( 'b' ):
   Str2Sthg( optarg , linpart );
   return( true );
  default:
   return( false );
  }
 }

/*--------------------------------------------------------------------------*/
/*-------------------------------- main() ----------------------------------*/
/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 std::set_terminate( smspp_terminate );

 docopt_desc = "SMS++ Solver of a MasterProblemBlock test.\n";
 filename_optional = true;
 round_opts = { 0 , 255 , 20 , 3 , 0.5 };
 add_round_options( "                                    1 cuts added, "
		    "2 cuts removed,\n"
		    "                                    4 serious step, "
		    "8 t, 16 box,\n"
		    "                                    32 lower bounds, "
		    "64 level,\n"
		    "                                    128 linear part\n" );
 short_opts += "N:K:M:V:L:l:x:b:";
 const std::vector< option > my_opts = {
  { "nvar" , required_argument , nullptr , 'N' } ,
  { "ncmp" , required_argument , nullptr , 'K' } ,
  { "ncut" , required_argument , nullptr , 'M' } ,
  { "pvert" , required_argument , nullptr , 'V' } ,
  { "lb" , required_argument , nullptr , 'L' } ,
  { "level" , required_argument , nullptr , 'l' } ,
  { "box" , required_argument , nullptr , 'x' } ,
  { "linear" , required_argument , nullptr , 'b' }
  };
 long_opts.insert( std::prev( long_opts.end() ) , my_opts.begin() ,
		   my_opts.end() );
 help += "  -N, --nvar <n>                  variables [10]\n"
	 "  -K, --ncmp <n>                  hard components [1]\n"
	 "  -M, --ncut <n>                  initial cuts of each "
	 "component [5]\n"
	 "  -V, --pvert <p>                 probability that a cut is "
	 "vertical [0]\n"
	 "  -L, --lb <0|1>                  a lower bound on each "
	 "component [0]\n"
	 "  -l, --level <0|1>               the doubly stabilized master "
	 "[0]\n"
	 "  -x, --box <0|1|2>               box on no, half, all the "
	 "variables [0]\n"
	 "  -b, --linear <0|1>              a linear part [0]\n";
 process_args( argc , argv , process_specific_arg );
 require_solver_config();

 if( ( nvar < 1 ) || ( ncmp < 1 ) || ( ncut < 1 ) ) {
  cout << "error: nvar, ncmp and ncut must be positive" << endl;
  return( 1 );
  }

 auto c = Configuration::deserialize( sconf_file );
 auto list =
   dynamic_cast< SimpleConfiguration< std::vector< Configuration * > > * >(
     c );
 if( ! list ) {
  cerr << "Error: " << sconf_file << " is not a list of BlockSolverConfig"
       << endl;
  delete c;
  return( 1 );
  }

 rg.seed( round_opts.seed );
 generate();

 for( auto * bsc : list->f_value ) {
  if( ! dynamic_cast< BlockSolverConfig * >( bsc ) ) {
   cerr << "Error: an entry of " << sconf_file << " is not a "
	<< "BlockSolverConfig" << endl;
   delete c;
   return( 1 );
   }
  // a BlockSolverConfig with a Solver that this build has not (e.g.,
  // BDMinQuadSolver, an optional module) is left out, and said
  bool there = true;
  for( const auto & name :
       static_cast< BlockSolverConfig * >( bsc )->get_SolverNames() )
   if( ! Solver::has_Solver( name ) ) {
    cout << name << " is not in this build, left out" << endl;
    there = false;
    }
  if( ! there )
   continue;
  auto mpb = build( bsc );
  if( mpb->get_registered_solvers().empty() ) {
   delete mpb;  // no Solver of this build
   continue;
   }
  masters.push_back(
    { mpb , bsc , mpb->get_registered_solvers().front()->classname() } );
  }
 if( masters.size() < 2 ) {
  cout << "fewer than two Solver in this build: nothing to compare" << endl;
  for( auto & m : masters )
   delete m.mpb;
  delete c;
  return( 77 );
  }

 bool AllPassed = solve( "start" );
 for( Block::Index rep = 0 ; rep < round_opts.n_repeat ; ++rep ) {
  change();
  AllPassed &= solve( "round " + std::to_string( rep ) );
  }

 // clang-format off
 if( AllPassed )
  cout << GREEN( All tests passed!! ) << endl;
 else
  cout << RED( Shit happened!! ) << endl;
 // clang-format on

 // detach the Solver, then delete the master problems
 for( auto & m : masters ) {
  static_cast< BlockSolverConfig * >( m.bsc )->clear();
  static_cast< BlockSolverConfig * >( m.bsc )->apply( m.mpb );
  delete m.mpb;
  }
 delete c;

 return( AllPassed ? 0 : 1 );
 }

/*--------------------------------------------------------------------------*/
/*---------------------------- End File test.cpp ---------------------------*/
/*--------------------------------------------------------------------------*/
