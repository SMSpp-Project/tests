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
/* Some SMS++ targets (InvestmentBlock and tools using SDDPBlock) pull in
 * libboost_mpi / libmpi transitively even when they never call MPI_Init().
 * On systems where Open MPI / UCX are installed but no usable transport is
 * available (no IB, missing UCX vfs.sock, ...), the MPI/UCX runtime can
 * hang on startup spinning on futex / X11 sockets.
 *
 * To make every SMS++ test work out-of-the-box after a fresh install, we
 * pre-seed safe TCP-only defaults with setenv(..., 0): the third argument
 * is "overwrite = false", so any user who has already exported UCX_TLS /
 * OMPI_MCA_* (e.g. on an HPC cluster with a real fabric) keeps full
 * control. This is executed before main() via a static initializer.    */

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
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

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

std::string fmt_obj( double v )
{
 std::ostringstream os;
 os.setf( std::ios::scientific , std::ios::floatfield );
 os << std::setprecision( 7 ) << v;
 return( os.str() );
 }

/*--------------------------------------------------------------------------*/

SolverClassifier exact_getter( ObjGetter g )
{
 return( [ g ]( Solver * s , std::size_t ) -> SolverReading {
  SolverReading r;
  r.kind  = SolverReading::Kind::Exact;
  r.value = get_obj_value( s , g );
  return( r );
  } );
 }

/*--------------------------------------------------------------------------*/

SolverClassifier exact_getters( std::vector< ObjGetter > getters ,
                                ObjGetter dflt )
{
 return( [ getters = std::move( getters ) , dflt ]
         ( Solver * s , std::size_t k ) -> SolverReading {
  SolverReading r;
  r.kind  = SolverReading::Kind::Exact;
  r.value = get_obj_value( s , k < getters.size() ? getters[ k ] : dflt );
  return( r );
  } );
 }

/*--------------------------------------------------------------------------*/

SolverClassifier relaxation_aware_getter( void )
{
 return( []( Solver * s , std::size_t ) -> SolverReading {
  SolverReading r;
  // a relaxation Solver (classname() containing "Relaxation") only brackets
  // z* in [ get_lb() , get_ub() ]; any other Solver is an exact optimum
  if( s->classname().find( "Relaxation" ) != std::string::npos ) {
   r.kind = SolverReading::Kind::Bracket;
   r.lb   = get_obj_value( s , ObjGetter::LowerBound );
   r.ub   = get_obj_value( s , ObjGetter::UpperBound );
   }
  else {
   r.kind  = SolverReading::Kind::Exact;
   r.value = get_obj_value( s , ObjGetter::VarValue );
   }
  return( r );
  } );
 }

/*--------------------------------------------------------------------------*/

// verbose is on either via -v (verbosity_level, only for tests that parse
// their options through process_args) or, uniformly for *all* tests regardless
// of how they parse their arguments, via the `verbose` environment variable
// (e.g. `verbose=1 ./batch ...` or `verbose=1 ctest ...`), which the child test
// processes inherit. The latter is what makes the verbose log consistent
// across the whole test suite, including the tests that read positional
// arguments by hand and so do not understand -v.
static bool tests_verbose()
{
 static const bool env_on = []() {
   const char * e = std::getenv( "verbose" );
   return e && ( e[ 0 ] != '\0' ) && ( std::string( e ) != "0" );
   }();
 return ( verbosity_level >= 1 ) || env_on;
}

void print_instance_line( const std::vector< double > & times ,
                          const std::vector< std::string > & value_tokens ,
                          double ref ,
                          const std::string & verdict ,
                          double diff )
{
 // the detailed per-round line (times, solver values, verdict) is "extended"
 // output: print it only when verbose, keeping the default output terse (just
 // the per-instance header from the batch and the final summary printed by the
 // test). Failing comparisons (KO) are always shown, so that a failure is
 // visible without re-running in verbose mode
 if( ( ! tests_verbose() ) && ( verdict.compare( 0 , 2 , "KO" ) != 0 ) )
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

std::string reading_token( const SolverReading & r )
{
 if( r.kind == SolverReading::Kind::Bracket )
  return( "[ " + fmt_obj( r.lb ) + " , " + fmt_obj( r.ub ) + " ]" );
 return( fmt_obj( r.value ) );
 }

/*--------------------------------------------------------------------------*/
/*--- mutual-infeasibility watchdog ----------------------------------------*/
/*--------------------------------------------------------------------------*/
// A multi-Solver cross-check where *all* Solvers report infeasible (or all
// unbounded) and no reference is given is accepted as a pass (OK(e) / OK(u)):
// the Solvers agree, so there is nothing to flag per se. However, if a whole
// run degenerates into such "everybody agrees it is infeasible" comparisons,
// the test is not really comparing any solution and silently passes for
// the wrong reason. We therefore count these cases and, at process exit, emit
// a warning on stderr if they dominate the run (without altering any verdict
// or exit code, so that legitimately-infeasible tests keep working).

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

bool cross_check( const std::vector< SolverReading > & rd ,
                  const std::vector< bool > & has_solution ,
                  const std::vector< int > & status ,
                  double ref , double tol ,
                  std::string & verdict_out , double & diff_out )
{
 diff_out = std::numeric_limits< double >::quiet_NaN();

 // a ~ b / a <= b within the relative tolerance
 auto within = [ tol ]( double a , double b ) {
  return( std::abs( a - b ) <=
          tol * std::max( double( 1 ) ,
                          std::max( std::abs( a ) , std::abs( b ) ) ) );
  };
 auto le = [ tol ]( double a , double b ) {
  return( a - b <=
          tol * std::max( double( 1 ) ,
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

 // single Solver, no reference: just "did it find a solution?"
 if( ( M == 1 ) && std::isnan( ref ) ) {
  bool ok = has_solution[ 0 ];
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

 // some feasible, some not: disagreement
 if( nInf > 0 || nUnb > 0 ) { verdict_out = "KO"; return( false ); }

 // all feasible: gather Exact / LowerBound / UpperBound / Bracket readings
 std::vector< double > exacts , lbs , ubs;
 for( std::size_t k = 0 ; k < M ; ++k )
  switch( rd[ k ].kind ) {
   case SolverReading::Kind::Exact:      exacts.push_back( rd[ k ].value );
                                         break;
   case SolverReading::Kind::LowerBound: lbs.push_back( rd[ k ].value );
                                         break;
   case SolverReading::Kind::UpperBound: ubs.push_back( rd[ k ].value );
                                         break;
   case SolverReading::Kind::Bracket:    lbs.push_back( rd[ k ].lb );
                                         ubs.push_back( rd[ k ].ub );
                                         break;
   }

 bool ok = true;

 // determine z*: from the Exact readings, else from ref, else from the bounds
 double zstar = std::numeric_limits< double >::quiet_NaN();
 if( ! exacts.empty() ) {
  zstar = exacts.front();
  for( double e : exacts )             // every Exact must agree on z*
   if( ! within( e , zstar ) )
    ok = false;
  }
 else if( ! std::isnan( ref ) )
  zstar = ref;

 if( ! std::isnan( zstar ) ) {
  for( double lb : lbs ) if( ! le( lb , zstar ) ) ok = false;  // LB <= z*
  for( double ub : ubs ) if( ! le( zstar , ub ) ) ok = false;  // z* <= UB
  if( ! std::isnan( ref ) && ! exacts.empty() ) {
   diff_out = std::abs( zstar - ref );
   if( ! within( zstar , ref ) ) ok = false;  // exact optimum must match Ref
   }
  }
 else {
  // no Exact reading and no Ref: the bounds must be mutually consistent
  if( ! lbs.empty() && ! ubs.empty() ) {
   double maxlb = lbs.front() , minub = ubs.front();
   for( double v : lbs ) maxlb = std::max( maxlb , v );
   for( double v : ubs ) minub = std::min( minub , v );
   if( ! le( maxlb , minub ) ) ok = false;
   }
  }

 verdict_out = ok ? ( exacts.empty() ? "OK" : "OK(f)" ) : "KO";
 return( ok );
 }

/*--------------------------------------------------------------------------*/

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
  const auto & reg = block->get_registered_solvers();
  std::vector< Solver * > S( reg.begin() , reg.end() );
  const std::size_t M = S.size();
  if( M == 0 ) {
   std::cout << "no Solver registered to the Block!" << std::endl;
   return( false );
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
    }
   else if( status[ k ] == Solver::kInfeasible )  tok[ k ] = "Unfeas";
   else if( status[ k ] == Solver::kUnbounded )   tok[ k ] = "Unbounded";
   else                                           tok[ k ] = "Error!";
   }

  // out-params from the first Solver - - - - - - - - - - - - - - - - - - - -
  if( out_fo1 )   *out_fo1   = hs[ 0 ] ? rd[ 0 ].value : -INF;
  if( out_hs1 )   *out_hs1   = hs[ 0 ];
  if( out_time1 ) *out_time1 = times[ 0 ];
  if( out_it1 )   *out_it1   = iters[ 0 ];

  // cross-check + uniform per-instance line - - - - - - - - - - - - - - - - -
  std::string verdict;
  double diff;
  bool ok = cross_check( rd , hs , status , ref , tol , verdict , diff );
  print_instance_line( times , tok , ref , verdict , diff );
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
               const std::vector< ObjGetter > & getters ,
               double tol ,
               double * out_fo1 ,
               bool   * out_hs1 ,
               double * out_time1 ,
               long   * out_it1 )
{
 return( SolveAll( block , exact_getters( getters ) , ref , tol ,
                   out_fo1 , out_hs1 , out_time1 , out_it1 ) );
 }

/*--------------------------------------------------------------------------*/

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
 // via g1, the second via g2. In the default (two-sided) mode both are Exact
 // optima that must agree; in the ProxHeur one-sided mode the first is a
 // lower bound and the second an upper bound, so the verdict becomes LB <= UB.
 SolverClassifier classify =
  [ g1 , g2 , one_sided_le ]( Solver * s , std::size_t k ) -> SolverReading {
   SolverReading r;
   if( one_sided_le ) {
    r.kind  = k ? SolverReading::Kind::UpperBound
                : SolverReading::Kind::LowerBound;
    r.value = get_obj_value( s , k ? g2 : g1 );
    }
   else {
    r.kind  = SolverReading::Kind::Exact;
    r.value = get_obj_value( s , k ? g2 : g1 );
    }
   return( r );
   };

 return( SolveAll( block , classify ,
                   std::numeric_limits< double >::quiet_NaN() , tol ,
                   out_fo1 , out_hs1 , out_time1 , out_it1 ) );
 }

/*--------------------------------------------------------------------------*/

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

bool SolveAndCheckRef( Block * block , double ref ,
                       ObjGetter g ,
                       double rel_tol )
{
 // single-Solver solve + reference check, expressed as SolveAll() of the only
 // registered Solver (read via g) against ref
 return( SolveAll( block , exact_getter( g ) , ref , rel_tol ) );
 }

/*--------------------------------------------------------------------------*/
/*--------------------- CLI baseline (opt-in) ------------------------------*/
/*--------------------------------------------------------------------------*/
// Globals (declared extern in common_utils.h).

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

// Minimal getopt baseline shared by tests that opt in. Tests that need
// extra switches override short_opts / long_opts / help in their main()
// before calling process_args().
std::string short_opts = "B:S:p:c:Dv::h";

std::vector< option > long_opts = {
 { "help"            , no_argument       , nullptr , 'h' } ,
 { "blockcfg"        , required_argument , nullptr , 'B' } ,
 { "solvercfg"       , required_argument , nullptr , 'S' } ,
 { "prefix"          , required_argument , nullptr , 'p' } ,
 { "configdir"       , required_argument , nullptr , 'c' } ,
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
 "  -D, --dryrun                    skip the compute() call\n"
 "  -v, --verbose[=N]               verbose output (0 = silent, 1 = basic, 2 = debug)\n";

/*--------------------------------------------------------------------------*/

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
  case 'D': dryrun = true; break;
  case 'v': {
   sol_verbose = true;
   verbosity_level = optarg ? std::atoi( optarg ) : 1;
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

void process_args( int argc , char ** argv )
{
 process_args( argc , argv , nullptr );
 }

/*--------------------------------------------------------------------------*/

void process_args( int argc , char ** argv , bool ( *custom_arg )( int opt ) )
{
 exe = get_filename( argv[ 0 ] );

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
