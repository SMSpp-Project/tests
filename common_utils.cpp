/*--------------------------------------------------------------------------*/
/*-------------------------- common_utils.cpp ------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Implementations of the utility functions declared in common_utils.h.
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

#include "common_utils.h"

#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <limits>
#include <list>
#include <map>
#include <typeinfo>

#include <SMSTypedefs.h>

#include <Configuration.h>

/*--------------------------------------------------------------------------*/
/*--------------------- MPI / UCX SAFE-DEFAULTS ----------------------------*/
/*--------------------------------------------------------------------------*/
/* Some SMS++ targets (InvestmentBlock, and the tools using SDDPBlock) pull
 * in libboost_mpi / libmpi transitively even when they never call
 * MPI_Init(). Where Open MPI / UCX are installed but no usable transport is
 * available (no IB, missing UCX vfs.sock, ...), their runtime can hang on
 * startup spinning on futex / X11 sockets, so TCP-only defaults are
 * pre-seeded here, before main(), with setenv( ... , 0 ): the third
 * argument is "overwrite = false", hence whoever has exported UCX_TLS /
 * OMPI_MCA_* already, as on a cluster with a real fabric, keeps control. */

namespace {

void set_default_env( const char * name , const char * value ) {
#ifdef _WIN32
 if( std::getenv( name ) == nullptr )
  _putenv_s( name , value );
#else
 setenv( name , value , 0 );
#endif
}

struct SmsppMpiSafeEnvInit {
 SmsppMpiSafeEnvInit() {
  set_default_env( "UCX_TLS"     , "tcp,self" );
  set_default_env( "OMPI_MCA_btl", "tcp,self" );
  set_default_env( "OMPI_MCA_pml", "ob1"      );
  // the hwloc GL component probes the GPU topology via XOpenDisplay(),
  // which may hang inside MPI_Init(); no SMS++ target has a use for it
  set_default_env( "HWLOC_COMPONENTS", "-gl"  );
  }
 };

static SmsppMpiSafeEnvInit smspp_mpi_safe_env_init_;

}  // anonymous namespace

/*--------------------------------------------------------------------------*/
/*----------------------- OUTPUT AND ERROR HANDLING ------------------------*/
/*--------------------------------------------------------------------------*/

// print an objective value, or the reason why there is none

void PrintResults( bool hs , int rtrn , double fo )
{
 if( hs ) {
  std::cout.setf( std::ios::scientific , std::ios::floatfield );
  std::cout << def << fo;
  }
 else
  if( rtrn == Solver::kInfeasible )
   std::cout << "    Unfeas";
  else
   if( rtrn == Solver::kUnbounded )
    std::cout << "      Unbounded";
   else
    std::cout << "      Error!";
 }

/*--------------------------------------------------------------------------*/
// print the exception that reached std::terminate(), then abort

void smspp_terminate( void )
{
 std::cerr << "Uncaught exception in executing SMS++:\n";
 try {
  std::rethrow_exception( std::current_exception() );
  }
 catch( const std::exception & e ) {
  std::cerr << "\tException type: " << typeid( e ).name() << "\n";
  std::cerr << "\tException message: " << e.what() << "\n";
  }
 catch( ... ) {
  std::cerr << "\tUnknown exception" << std::endl;
  }
 std::abort();  // or exit( 1 )
 }

/*--------------------------------------------------------------------------*/
/*----------------------------- CONFIGURATION ------------------------------*/
/*--------------------------------------------------------------------------*/

// apply a BlockConfig, or dispatch a meta one to the sub-Block it names

void b_config_Block( Block * block , Configuration * b_config ,
                     const std::string & fn )
{
 // std::list rather than std::vector since it's built by push_back and
 // only trasversed head-to-tail
 std::list< Block * > BFS;

 // handle the special case of a "meta" BlockConfig
 if( auto * mb =
     dynamic_cast< SimpleConfiguration< std::map< std::string ,
                                                  Configuration * > >
                                        * >( b_config ) ) {

  // construct the list of all Block inside block
  BFS.push_back( block );
  for( auto bit = BFS.begin() ; bit != BFS.end() ; ++bit )
   for( auto el : ( *bit )->get_nested_Blocks() )
    BFS.push_back( el );

  auto & map = mb->f_value;

  // now BlockConfig-ure all Block whose classname() matches
  for( auto b : BFS )
   if( auto bcit = map.find( b->classname() ); bcit != map.end() ) {
    if( auto bc = dynamic_cast< BlockConfig * >( bcit->second ) ) {
     auto cbc = bc->clone();
     cbc->apply( b );
     delete cbc;
     }
    else {
     std::cerr << "Error: meta-Configuration for :Block " << bcit->first
               << " in file " << fn << " is not a BlockConfig" << std::endl;
     exit( 1 );
     }
    }

  return;  // all done
  }

 if( auto * bc = dynamic_cast< BlockConfig * >( b_config ) ) {
  bc->apply( block );  // just apply() it
  return;              // all done
  }

 std::cerr << "Error: " << fn
           << " does not contain a valid [meta]BlockConfig" << std::endl;
 exit( 1 );

 }  // end( b_config_Block )

/*--------------------------------------------------------------------------*/
// the same for a BlockSolverConfig, which is also clear()-ed for the final
// cleanup, see common_utils.h

void s_config_Block( Block * block , Configuration * s_config ,
                     const std::string & fn ,
                     bool clear_after )
{
 // std::list rather than std::vector since it's built by push_back and
 // only trasversed head-to-tail
 std::list< Block * > BFS;

 // handle the special case of a "meta" BlockSolverConfig
 if( auto * mb =
     dynamic_cast< SimpleConfiguration< std::map< std::string ,
                                                  Configuration * > >
                                        * >( s_config ) ) {

  // construct the list of all Block inside block
  BFS.push_back( block );
  for( auto bit = BFS.begin() ; bit != BFS.end() ; ++bit )
   for( auto el : ( *bit )->get_nested_Blocks() )
    BFS.push_back( el );

  auto & map = mb->f_value;

  // now BlockSolverConfig-ure all Block whose classname() matches
  for( auto b : BFS )
   if( auto bcit = map.find( b->classname() ); bcit != map.end() ) {
    if( auto bsc = dynamic_cast< BlockSolverConfig * >( bcit->second ) )
     bsc->apply( b );
    else {
     std::cerr << "Error: meta-Configuration for :Block " << bcit->first
               << " in file " << fn << " is not a BlockSolverConfig"
               << std::endl;
     exit( 1 );
     }
    }

  // finally, clear() all the BlockSolverConfig for final cleanup
  if( clear_after )
   for( auto & el : map )
    (el.second)->clear();

  return;  // all done
  }

 if( auto * bsc = dynamic_cast< BlockSolverConfig * >( s_config ) ) {
  bsc->apply( block );          // just apply() it
  if( clear_after )
   bsc->clear();                // clear() it for final cleanup
  return;                       // all done
  }

 std::cerr << "Error: " << fn
           << " does not contain a valid [meta]BlockSolverConfig"
           << std::endl;
 exit( 1 );

 }  // end( s_config_Block )

/*--------------------------------------------------------------------------*/
/*------------------------------ CROSS-CHECK -------------------------------*/
/*--------------------------------------------------------------------------*/

// where a Solver publishes its answer

double get_obj_value( Solver * slvr , ObjGetter g )
{
 switch( g ) {
  case ObjGetter::VarValue:   return( slvr->get_var_value() );
  case ObjGetter::LowerBound: return( slvr->get_lb() );
  case ObjGetter::UpperBound: return( slvr->get_ub() );
  }
 return( std::numeric_limits< double >::quiet_NaN() );  // unreachable
 }

/*--------------------------------------------------------------------------*/
// the canonical format of every value the tests print

std::string fmt_obj( double v )
{
 std::ostringstream os;
 os.setf( std::ios::scientific , std::ios::floatfield );
 os << std::setprecision( 7 ) << v;
 return( os.str() );
 }

/*--------------------------------------------------------------------------*/
// the tolerance Solver k is held to

double eps_of( std::size_t k , Solver * s , double dflt )
{
 // a single value of -E is the tolerance of every Solver, a list is
 // positional and its empty fields declare nothing
 double e = std::numeric_limits< double >::quiet_NaN();
 if( solver_eps.size() == 1 )
  e = solver_eps.front();
 else if( k < solver_eps.size() )
  e = solver_eps[ k ];

 if( ! std::isnan( e ) )
  return( e );

 // else the accuracy the Solver was asked for in its ComputeConfig
 return( s ? s->get_dbl_par( Solver::dblRelAcc ) : dflt );
 }

/*--------------------------------------------------------------------------*/
// the default reading of a Solver: the interval of its base contract

SolverReading read_bounds( Solver * s , std::size_t k )
{
 return( SolverReading{ get_obj_value( s , ObjGetter::LowerBound ) ,
                        get_obj_value( s , ObjGetter::UpperBound ) ,
                        eps_of( k , s ) } );
 }

/*--------------------------------------------------------------------------*/
// the reading of a Solver that publishes an optimum and no interval

SolverClassifier exact_getter( ObjGetter g )
{
 return( [ g ]( Solver * s , std::size_t k ) -> SolverReading {
  return( SolverReading::exact( get_obj_value( s , g ) , eps_of( k , s ) ) );
  } );
 }

/*--------------------------------------------------------------------------*/

// whether the extended output is on, either via -v or via the `verbose`
// environment variable (`verbose=1 ./batch ...`, `verbose=1 ctest ...`),
// which the tests that parse their arguments positionally, and so do not
// understand -v, inherit as well
static bool tests_verbose()
{
 static const bool env_on = []() {
   const char * e = std::getenv( "verbose" );
   return e && ( e[ 0 ] != '\0' ) && ( std::string( e ) != "0" );
   }();
 return ( verbosity_level >= 1 ) || env_on;
}

/*--------------------------------------------------------------------------*/
// print the one line that reports an instance: timings, Solver values,
// reference and verdict

void print_instance_line( const std::vector< double > & times ,
                          const std::vector< std::string > & value_tokens ,
                          double ref ,
                          const std::string & verdict ,
                          double diff ,
                          bool always )
{
 // the detailed per-round line (times, solver values, verdict) of a test
 // that re-solves in a loop of modification rounds is "extended" output:
 // print it only when verbose, keeping the default output terse. The
 // one-per-instance cross-check line of SolveAll() (@p always) and any
 // failing comparison (KO) are always shown instead, so that what was
 // compared, and a failure, are visible without re-running in verbose mode
 if( ( ! always ) && ( ! tests_verbose() ) &&
     ( verdict.compare( 0 , 2 , "KO" ) != 0 ) )
  return;

 for( std::size_t k = 0 ; k < times.size() ; ++k )
  std::cout << ( k ? " - " : "" ) << fixd << times[ k ];

 std::cout << " | ";
 for( std::size_t k = 0 ; k < value_tokens.size() ; ++k )
  std::cout << ( k ? "  " : "" ) << "S" << k << " = " << value_tokens[ k ];

 if( ! std::isnan( ref ) ) {
  std::cout << "  ~ Ref = " << fmt_obj( ref );
  if( ! std::isnan( diff ) )
   std::cout << " (|diff| = " << fmt_obj( diff ) << ")";
  }

 std::cout << "  -> " << verdict << std::endl;
 }

/*--------------------------------------------------------------------------*/
// how a reading appears in that line

std::string reading_token( const SolverReading & r )
{
 const std::string lb = fmt_obj( r.lb ) , ub = fmt_obj( r.ub );

 // a Solver that closed the gap prints the optimum rather than an interval
 // whose two ends read the same
 if( lb == ub )
  return( lb );

 return( "[ " + lb + " , " + ub + " ]" );
 }

/*--------------------------------------------------------------------------*/
// A cross-check where every Solver reports infeasible, or every Solver
// reports unbounded, is a pass: they do agree. A run made mostly of such
// comparisons, however, passes without ever comparing a solution, so they
// are counted here and a warning is issued at exit if they dominate; no
// verdict and no exit code is touched, since a test can be legitimately
// infeasible.

namespace {
 struct MutualInfWatchdog {
  std::size_t n_total = 0;  // multi-Solver cross-checks performed
  std::size_t n_inf   = 0;  // ... of which unanimously infeasible (OK(e))
  std::size_t n_unb   = 0;  // ... of which unanimously unbounded  (OK(u))
  ~MutualInfWatchdog() {
   if( ( n_total >= 4 ) && ( 2 * ( n_inf + n_unb ) > n_total ) )
    std::cerr << ANSI_YELLOW << "[WARNING] " << ( n_inf + n_unb )
              << " of " << n_total << " multi-Solver comparisons were "
                 "unanimously " << ( n_unb > n_inf ? "unbounded" : "infeasible" )
              << ": this run passed mostly via solver agreement on the "
                 "absence of a solution, not by comparing solutions."
              << ANSI_RESET << "\n";
   }
  };
 MutualInfWatchdog mutual_inf_watchdog;
}

/*--------------------------------------------------------------------------*/
// the verdict on one instance, see common_utils.h for the criterion

bool cross_check( const std::vector< SolverReading > & rd ,
                  const std::vector< bool > & has_solution ,
                  const std::vector< int > & status ,
                  double ref , double tol ,
                  std::string & verdict_out , double & diff_out )
{
 diff_out = std::numeric_limits< double >::quiet_NaN();

 // a <= b up to the relative tolerance t
 auto le = []( double a , double b , double t ) {
  return( a - b <=
          t * std::max( double( 1 ) ,
                        std::max( std::abs( a ) , std::abs( b ) ) ) );
  };

 const std::size_t M = has_solution.size();
 std::size_t nFeas = 0 , nInf = 0 , nUnb = 0;
 for( std::size_t k = 0 ; k < M ; ++k ) {
  if( has_solution[ k ] )                        ++nFeas;
  else if( status[ k ] == Solver::kInfeasible )  ++nInf;
  else if( status[ k ] == Solver::kUnbounded )   ++nUnb;
  }

 // count genuine multi-Solver comparisons for the mutual-infeasibility
 // watchdog (see above)
 if( M >= 2 )
  ++mutual_inf_watchdog.n_total;

 // single Solver, no reference: just "did it find a solution?" (with the
 // lb <= ub sanity check when the reading is a bracket)
 if( ( M == 1 ) && std::isnan( ref ) ) {
  bool ok = has_solution[ 0 ];
  if( ok && std::isfinite( rd[ 0 ].lb ) && std::isfinite( rd[ 0 ].ub ) )
   ok = le( rd[ 0 ].lb , rd[ 0 ].ub , tol );
  verdict_out = ok ? "OK" : "KO";
  return( ok );
  }

 // unanimous infeasible / unbounded is a pass only when NO reference was given
 if( nFeas == 0 ) {
  if( std::isnan( ref ) && nInf == M ) {
   if( M >= 2 ) ++mutual_inf_watchdog.n_inf;
   verdict_out = "OK(e)"; return( true );
   }
  if( std::isnan( ref ) && nUnb == M ) {
   if( M >= 2 ) ++mutual_inf_watchdog.n_unb;
   verdict_out = "OK(u)"; return( true );
   }
  verdict_out = "KO";
  return( false );
  }

 // some feasible, some not: disagreement (an errored Solver counts as
 // disagreeing with the feasible ones, too)
 if( nFeas < M ) { verdict_out = "KO"; return( false ); }

 // all feasible: the agreement check of common_utils.h. The optimum is not
 // known, so what every Solver is measured against is the best knowledge
 // the whole set of them provides, i.e. the largest of the lower bounds and
 // the smallest of the upper bounds (the reference value, when given, being
 // one more Solver that has both)
 constexpr double INF = std::numeric_limits< double >::infinity();
 double best_lb = - INF , best_ub = INF;
 for( std::size_t k = 0 ; k < M ; ++k ) {
  best_lb = std::max( best_lb , rd[ k ].lb );
  best_ub = std::min( best_ub , rd[ k ].ub );
  }
 if( ! std::isnan( ref ) ) {
  best_lb = std::max( best_lb , ref );
  best_ub = std::min( best_ub , ref );
  }

 bool ok = true;
 double zstar = std::numeric_limits< double >::quiet_NaN();  // 1st claimed z*
 for( std::size_t k = 0 ; k < M ; ++k ) {
  const double lb = rd[ k ].lb , ub = rd[ k ].ub;

  // correctness, which every Solver owes whatever it promised: its bounds
  // cannot contradict the bounds of the others
  if( std::isfinite( ub ) && ( ! le( best_lb , ub , tol ) ) )
   ok = false;
  if( std::isfinite( lb ) && ( ! le( lb , best_ub , tol ) ) )
   ok = false;

  // quality, which only the Solver that delivered what it was asked owes:
  // each of its bounds has to be within its declared tolerance of the best
  // opposite one. A Solver that returns kLowPrecision, or that stopped on
  // any other condition, promised nothing and owes nothing here. The
  // tolerance is never taken below tol: under that the comparison would be
  // measuring the noise of the cross-check itself, and a Solver asked for
  // an accuracy that tight stops just outside it anyway, its own stopping
  // criterion not being normalized exactly like this one
  const double e = std::max( std::isnan( rd[ k ].eps ) ? tol : rd[ k ].eps ,
                             tol );
  if( ( status[ k ] == Solver::kOK ) && ( ! std::isinf( e ) ) ) {
   if( std::isfinite( ub ) && std::isfinite( best_lb )
       && ( ! le( ub , best_lb , e ) ) )
    ok = false;
   if( std::isfinite( lb ) && std::isfinite( best_ub )
       && ( ! le( best_ub , lb , e ) ) )
    ok = false;
   }

  // the optimum is pinned by the first Solver that bounds it on both sides
  if( std::isnan( zstar ) && std::isfinite( lb ) && std::isfinite( ub ) )
   zstar = rd[ k ].claimed();
  }

 if( ( ! std::isnan( zstar ) ) && ( ! std::isnan( ref ) ) )
  diff_out = std::abs( zstar - ref );

 verdict_out = ok ? ( std::isnan( zstar ) ? "OK" : "OK(f)" ) : "KO";
 return( ok );
 }

/*--------------------------------------------------------------------------*/
// solve an instance with every Solver, cross-check them, report

bool SolveAll( Block * block ,
               const SolverClassifier & classify ,
               double ref ,
               double tol ,
               double * out_fo1 ,
               bool   * out_hs1 ,
               double * out_time1 ,
               long   * out_it1 )
{
 constexpr double INF = std::numeric_limits< double >::has_infinity
                        ? std::numeric_limits< double >::infinity()
                        : std::numeric_limits< double >::max();

 try {
  // the Solver the BlockSolverConfig attached to this Block, in its order,
  // which is the order the -E tolerances are positional on; the Solver that
  // these in turn attach to the inner Block are theirs, not part of the
  // comparison
  const auto & reg = block->get_registered_solvers();
  std::vector< Solver * > S( reg.begin() , reg.end() );
  const std::size_t M = S.size();
  if( M == 0 ) {
   std::cout << "no Solver registered to the Block!" << std::endl;
   return( false );
   }

  // with -v 2, before solving, print what each Solver was actually given:
  // the index space of a Solver that wraps another one extends over that of
  // the wrapped one, so this shows the parameters of the inner Solver too
  if( verbosity_level >= 2 )
   for( std::size_t k = 0 ; k < M ; ++k ) {
    std::cout << std::endl << "--- parameters of Solver " << k << " ("
              << S[ k ]->classname() << ")" << std::endl;
    S[ k ]->print_parameters( std::cout );
    }

  // solve every Solver, timing each, then read the feasible ones - - - - - - -
  std::vector< int >    status( M );
  std::vector< double > times( M );
  std::vector< long >   iters( M );
  std::vector< bool >   hs( M );
  std::vector< SolverReading > rd( M );
  std::vector< std::string > tok( M );
  for( std::size_t k = 0 ; k < M ; ++k ) {
   auto start = std::chrono::system_clock::now();
   status[ k ] = S[ k ]->compute( false );
   auto end = std::chrono::system_clock::now();
   times[ k ] = std::chrono::duration< double >( end - start ).count();
   iters[ k ] = S[ k ]->get_elapsed_iterations();
   hs[ k ] = ( ( ( status[ k ] >= Solver::kOK )
                 && ( status[ k ] < Solver::kError )
                 && ( status[ k ] != Solver::kUnbounded )
                 && ( status[ k ] != Solver::kInfeasible ) )
               || ( status[ k ] == Solver::kLowPrecision ) );
   if( hs[ k ] ) {
    rd[ k ] = classify( S[ k ] , k );
    tok[ k ] = reading_token( rd[ k ] );
    // a Solver that did not return kOK did not deliver what it was asked
    // and is therefore only held to correctness: say so in the line, since
    // it is what its numbers are worth
    if( status[ k ] == Solver::kLowPrecision )  tok[ k ] += " (lowP)";
    else if( status[ k ] != Solver::kOK )       tok[ k ] += " (stop)";
    }
   else if( status[ k ] == Solver::kInfeasible )  tok[ k ] = "Unfeas";
   else if( status[ k ] == Solver::kUnbounded )   tok[ k ] = "Unbounded";
   else                                           tok[ k ] = "Error!";
   }

  // out-params from the first Solver - - - - - - - - - - - - - - - - - - - -
  if( out_fo1 )   *out_fo1   = hs[ 0 ] ? rd[ 0 ].claimed() : -INF;
  if( out_hs1 )   *out_hs1   = hs[ 0 ];
  if( out_time1 ) *out_time1 = times[ 0 ];
  if( out_it1 )   *out_it1   = iters[ 0 ];

  // cross-check + uniform per-instance line - - - - - - - - - - - - - - - - -
  std::string verdict;
  double diff;
  bool ok = cross_check( rd , hs , status , ref , tol , verdict , diff );
  print_instance_line( times , tok , ref , verdict , diff , true );
  return( ok );
  }
 catch( std::exception & e ) {
  std::cerr << e.what() << std::endl;
  exit( 1 );
  }
 catch( ... ) {
  std::cerr << "Error: unknown exception thrown" << std::endl;
  exit( 1 );
  }
 }

/*--------------------------------------------------------------------------*/

bool SolveAll( Block * block ,
               double ref ,
               double tol ,
               double * out_fo1 ,
               bool   * out_hs1 ,
               double * out_time1 ,
               long   * out_it1 )
{
 return( SolveAll( block , read_bounds , ref , tol ,
                   out_fo1 , out_hs1 , out_time1 , out_it1 ) );
 }

/*--------------------------------------------------------------------------*/
// SolveAll() of one or two Solver read via the given getters

bool SolveBoth( Block * block ,
                ObjGetter g1 ,
                ObjGetter g2 ,
                bool one_sided_le ,
                double tol ,
                double * out_fo1 ,
                bool   * out_hs1 ,
                double * out_time1 ,
                long   * out_it1 )
{
 // SolveBoth is the M <= 2 special case of SolveAll: the first Solver is read
 // via g1, the second via g2. In the default (two-sided) mode both claim an
 // optimum that must agree; in the ProxHeur one-sided mode the first is a
 // lower bound and the second an upper bound, so the verdict becomes LB <= UB.
 SolverClassifier classify =
  [ g1 , g2 , one_sided_le ]( Solver * s , std::size_t k ) -> SolverReading {
   const double v = get_obj_value( s , k ? g2 : g1 );
   if( ! one_sided_le )
    return( SolverReading::exact( v , eps_of( k , s ) ) );

   // the one-sided bounds claim nothing unless -E says how tight they are
   const double e = eps_of( k , nullptr ,
                            std::numeric_limits< double >::infinity() );
   return( k ? SolverReading::upper_bound( v , e )
             : SolverReading::lower_bound( v , e ) );
   };

 return( SolveAll( block , classify ,
                   std::numeric_limits< double >::quiet_NaN() , tol ,
                   out_fo1 , out_hs1 , out_time1 , out_it1 ) );
 }

/*--------------------------------------------------------------------------*/
// compare a value against the reference one and report

bool CheckRefValue( double fo , double ref ,
                    double rel_tol ,
                    double time1 , long iters )
{
 double maxv = std::max( double( 1 ) ,
                         std::max( std::abs( fo ) , std::abs( ref ) ) );
 double diff = std::abs( fo - ref );
 double tol = rel_tol * maxv;

 bool OK = ( diff <= tol );

 std::cout << fixd << time1 << "\t" << iters << "\t"
           << def << fo
           << " ~ Ref = " << def << ref
           << " (|diff| = " << def << diff
           << ( OK ? ", OK" : ", KO" ) << ")" << std::endl;

 return( OK );
 }

/*--------------------------------------------------------------------------*/
// solve with the only Solver and compare against the reference value

bool SolveAndCheckRef( Block * block , double ref ,
                       ObjGetter g ,
                       double rel_tol )
{
 // single-Solver solve + reference check, expressed as SolveAll() of the only
 // registered Solver (read via g) against ref
 return( SolveAll( block , exact_getter( g ) , ref , rel_tol ) );
 }

/*--------------------------------------------------------------------------*/
/*------------------------------ COMMAND LINE ------------------------------*/
/*--------------------------------------------------------------------------*/
// the globals declared extern in common_utils.h

std::string docopt_desc;

std::string exe;
std::string filename;
std::string bconf_file;
std::string sconf_file;
std::string block_prefix;
std::string conf_prefix;

bool sol_verbose = false;
bool dryrun      = false;

int verbosity_level = 0;

double RefObjective = std::numeric_limits< double >::quiet_NaN();

std::vector< double > solver_eps;

// the getopt baseline shared by the tests that opt in; those that need
// extra switches override short_opts / long_opts / help in their main()
// before calling process_args()
std::string short_opts = "B:S:p:c:E:Dv::h";

std::vector< option > long_opts = {
 { "help"            , no_argument       , nullptr , 'h' } ,
 { "blockcfg"        , required_argument , nullptr , 'B' } ,
 { "solvercfg"       , required_argument , nullptr , 'S' } ,
 { "prefix"          , required_argument , nullptr , 'p' } ,
 { "configdir"       , required_argument , nullptr , 'c' } ,
 { "eps"             , required_argument , nullptr , 'E' } ,
 { "dryrun"          , no_argument       , nullptr , 'D' } ,
 { "verbose"         , optional_argument , nullptr , 'v' } ,
 { nullptr           , no_argument       , nullptr , 0   }
 };

std::string help =
 "  -h, --help                      print this help\n"
 "  -B, --blockcfg <file>           Block Configuration\n"
 "  -S, --solvercfg <file>          Solver Configuration\n"
 "  -p, --prefix <path>             the prefix for all Block filenames\n"
 "  -c, --configdir <path>          the prefix for all Config filenames\n"
 "  -E, --eps <e[,e,...]>           optimality tolerance of each Solver, in\n"
 "                                  the order of the BlockSolverConfig, the\n"
 "                                  empty field being its own dblRelAcc; one\n"
 "                                  value applies to all, inf claims nothing\n"
 "                                  beyond a valid [ get_lb() , get_ub() ]\n"
 "  -D, --dryrun                    skip the compute() call\n"
 "  -v, --verbose[=N]               verbose output (0 = silent, 1 = basic, 2 = debug)\n";

/*--------------------------------------------------------------------------*/
// open an SMS++ nc4 file and check that it is one

int read_open_netCDF( netCDF::NcFile & f , std::string fn )
{
 if( ! block_prefix.empty() && ! fn.empty()
     && fn.front() != '/' && fn.front() != '\\' )
  fn = block_prefix + fn;

 try {
  f.open( fn , netCDF::NcFile::read );
  }
 catch( netCDF::exceptions::NcException & ) {
  std::cerr << exe << ": cannot open nc4 file " << fn << std::endl;
  exit( 1 );
  }

 netCDF::NcGroupAtt gtype = f.getAtt( "SMS++_file_type" );
 if( gtype.isNull() ) {
  std::cerr << exe << ": " << fn << " is not an SMS++ nc4 file" << std::endl;
  exit( 1 );
  }

 int type;
 gtype.getValues( &type );

 if( ( type != eProbFile ) && ( type != eBlockFile ) ) {
  std::cerr << exe << ": " << fn << " is not a valid SMS++ file" << std::endl;
  exit( 1 );
  }

 return( type );
 }

/*--------------------------------------------------------------------------*/
// print the usage

void docopt( void )
{
 // http://docopt.org
 std::cout << docopt_desc << std::endl;
 std::cout << "Usage:" << std::endl
           << "  " << exe << " [options] <file>" << std::endl
           << "  " << exe << " -h | --help" << std::endl << std::endl
           << "Options:" << std::endl << help << std::endl;
 }

/*--------------------------------------------------------------------------*/
// read the tolerances of -E, a comma-separated list whose fields are either
// a number or, case-insensitively, "inf"
static void parse_eps_list( const std::string & arg )
{
 solver_eps.clear();
 std::size_t pos = 0;
 while( pos <= arg.size() ) {
  const std::size_t next = arg.find( ',' , pos );
  std::string field = arg.substr( pos , next == std::string::npos
                                        ? std::string::npos : next - pos );
  pos = ( next == std::string::npos ) ? arg.size() + 1 : next + 1;

  // trim the blanks that a quoted argument may carry
  const auto b = field.find_first_not_of( " \t" );
  const auto e = field.find_last_not_of( " \t" );
  field = ( b == std::string::npos ) ? "" : field.substr( b , e - b + 1 );

  // an empty field declares nothing for that Solver, but holds its place
  if( field.empty() ) {
   solver_eps.push_back( std::numeric_limits< double >::quiet_NaN() );
   continue;
   }

  std::string lc;
  for( auto c : field )
   lc += char( std::tolower( (unsigned char) c ) );

  if( ( lc == "inf" ) || ( lc == "+inf" ) || ( lc == "infinity" ) ) {
   solver_eps.push_back( std::numeric_limits< double >::infinity() );
   continue;
   }

  try {
   solver_eps.push_back( std::stod( field ) );
   }
  catch( ... ) {
   std::cerr << exe << ": invalid tolerance '" << field << "' in -E"
             << std::endl;
   exit( 1 );
   }
  }
 }

/*--------------------------------------------------------------------------*/
// the command line, so that an option can look at the argument that follows
// it even when getopt does not hand it over [see the 'v' case below]

static int f_argc = 0;
static char ** f_argv = nullptr;

// whether the whole string is made of digits, so that it is the level of -v
// and not the argument that follows it

static bool is_number( const char * s )
{
 if( ( ! s ) || ( ! *s ) )
  return( false );
 for( ; *s ; ++s )
  if( ( *s < '0' ) || ( *s > '9' ) )
   return( false );
 return( true );
 }

/*--------------------------------------------------------------------------*/
// one standard option, or false if it is not one

bool process_standard_arg( int opt )
{
 switch( opt ) {
  case 'B': bconf_file = std::string( optarg ); break;
  case 'S': sconf_file = std::string( optarg ); break;
  case 'p': {
   block_prefix = normalize_prefix( std::string( optarg ) );
   Block::set_filename_prefix( std::string( block_prefix ) );
   break;
   }
  case 'c': conf_prefix = normalize_prefix( std::string( optarg ) );
            // also hand the -c prefix to Configuration, so filenames referenced
            // from inside a Configuration file (the "*filename" includes and the
            // strInnerBSC / str_LagBF_BSCfg meta-config chains) are resolved
            // against it, exactly as Block::set_filename_prefix() does for -p
            Configuration::set_filename_prefix( std::string( conf_prefix ) );
            break;
  case 'E': parse_eps_list( std::string( optarg ) ); break;
  case 'D': dryrun = true; break;
  case 'v': {
   sol_verbose = true;
   // the level can be attached (-v2) or, since that is what one naturally
   // writes, separate (-v 2): getopt only gives the attached form, so the
   // next argument is taken when it is a number
   if( optarg )
    verbosity_level = std::atoi( optarg );
   else if( f_argv && ( optind < f_argc ) && is_number( f_argv[ optind ] ) )
    verbosity_level = std::atoi( f_argv[ optind++ ] );
   else
    verbosity_level = 1;
   break;
   }
  case 'h': docopt(); exit( 0 );
  case '?':
  default:  return( false );
  }
 return( true );
 }

/*--------------------------------------------------------------------------*/

bool filename_optional = false;

/*--------------------------------------------------------------------------*/
// the -S and -B that a test cannot do without

void require_solver_config( void )
{
 if( sconf_file.empty() )
  throw( std::invalid_argument(
   "a BlockSolverConfig must be provided (did you forget the -S option?)" ) );
 }

/*--------------------------------------------------------------------------*/

void require_block_config( void )
{
 if( bconf_file.empty() )
  throw( std::invalid_argument(
   "a BlockConfig must be provided (did you forget the -B option?)" ) );
 }

/*--------------------------------------------------------------------------*/
// parse the command line, with or without test-specific options

void process_args( int argc , char ** argv )
{
 process_args( argc , argv , nullptr );
 }

/*--------------------------------------------------------------------------*/

void process_args( int argc , char ** argv , bool ( *custom_arg )( int opt ) )
{
 exe = get_filename( argv[ 0 ] );
 f_argc = argc;
 f_argv = argv;

 while( true ) {  // options
  const auto opt = getopt_long( argc , argv , short_opts.data() ,
                                long_opts.data() , nullptr );
  if( opt == -1 ) break;

  // test-specific options are processed first: a test that re-defines one
  // of the standard letters means its own
  if( custom_arg && custom_arg( opt ) )  // test-specific option
   continue;                             // next

  if( process_standard_arg( opt ) )      // if it is a standard one
   continue;                             // next

  std::cout << "Try '" << exe << " --help' for more information"
            << std::endl;
  exit( 1 );
  }

 if( optind < argc )  // last positional argument == [Block] instance file
  filename = std::string( argv[ optind ] );
 else if( ! filename_optional ) {
  std::cout << exe << ": no input file" << std::endl
            << "Try '" << exe << " --help' for more information" << std::endl;
  exit( 1 );
  }
 }

/*--------------------------------------------------------------------------*/
/*------------------------ End common_utils.cpp ----------------------------*/
/*--------------------------------------------------------------------------*/
