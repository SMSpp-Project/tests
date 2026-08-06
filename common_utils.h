/*--------------------------------------------------------------------------*/
/*--------------------------- common_utils.h -------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Common utilities shared by the SMS++ test programs.
 *
 * Three groups of helpers:
 *
 * - output and parsing: the ostream manipulators def() and fixd(), the
 *   string-to-T parser Str2Sthg(), the pretty-printer of Solver return codes
 *   PrintResults(), and the std::terminate handler smspp_terminate();
 *
 * - configuration: b_config_Block() and s_config_Block() apply a BlockConfig
 *   or a BlockSolverConfig to a Block, dispatching to the nested sub-Block
 *   when the Configuration is a meta-configuration (see there);
 *
 * - the cross-check: SolveAll() computes every Solver registered to a Block,
 *   reads each one as the interval [ get_lb() , get_ub() ] that, by the base
 *   Solver contract, contains the optimum, and checks each of them against
 *   the best bounds the whole set provides (a reference objective value,
 *   when known, being one more Solver): correctness always, and the quality
 *   it declares when it says it delivered it. See cross_check() for the
 *   criterion and @ref solver_eps for where that declaration comes from.
 *   The result is one uniform log line per instance, printed by
 *   print_instance_line(). Tests that need their own solve loop call
 *   cross_check() and print_instance_line() directly, so verdict and log
 *   format are implemented once anyway.
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

#ifndef __TESTS_COMMON_UTILS
#define __TESTS_COMMON_UTILS

/*--------------------------------------------------------------------------*/
/*------------------------------ MACROS ------------------------------------*/
/*--------------------------------------------------------------------------*/

#ifndef USECOLORS
 #define USECOLORS 1
#endif
#if( USECOLORS )
 #define RED( x ) "\x1B[31m" #x "\033[0m"
 #define GREEN( x ) "\x1B[32m" #x "\033[0m"
 #define YELLOW( x ) "\x1B[33m" #x "\033[0m"
 // raw on/off codes, for colouring runtime (non-literal) messages
 #define ANSI_YELLOW "\x1B[33m"
 #define ANSI_RESET  "\033[0m"
#else
 #define RED( x ) #x
 #define GREEN( x ) #x
 #define YELLOW( x ) #x
 #define ANSI_YELLOW ""
 #define ANSI_RESET  ""
#endif

/*--------------------------------------------------------------------------*/
/*----------------------------- INCLUDES -----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <getopt.h>
#include <filesystem>

#include <cmath>
#include <cstddef>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include <Block.h>
#include <BlockSolverConfig.h>

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/
/** @name Global variables shared across SMS++ tests that opt into the
 *        getopt-style command line. Tests that parse argv positionally can
 *        ignore them entirely. */
/// @{

extern std::string docopt_desc;     ///< test description; set by the test before calling docopt()

extern std::string exe;             ///< name of the executable file
extern std::string filename;        ///< input filename (last positional argv)
extern std::string bconf_file;      ///< BlockConfig filename (-B)
extern std::string sconf_file;      ///< BlockSolverConfig filename (-S)
extern std::string block_prefix;    ///< prefix for all Block filenames (-p)
extern std::string conf_prefix;     ///< prefix for all Configuration filenames (-c)

extern bool sol_verbose;            ///< if the Solver should be verbose
extern bool dryrun;                 ///< if compute() need not really be called (-D)

extern int verbosity_level;         ///< verbosity level (0 = silent, >0 = verbose output, -v)

/// reference objective for the ref-vs-Solver comparison; defaults to NaN
/** Tests set this directly (positional argv or test-specific option); the
 *  default getopt baseline does not parse it because individual tests use
 *  different conventions for the option letter. */
extern double RefObjective;

/// the optimality tolerances declared by -E, NaN where none was declared
/** The gap that each Solver is allowed to leave, positionally with respect
 *  to the registration order (= the Solver order in the BlockSolverConfig),
 *  with the empty field meaning "not declared here":
 *
 *      -E ,,5e-2
 *
 *  A single value applies to every Solver, and eps = inf declares nothing
 *  beyond the base contract, i.e. only that [ get_lb() , get_ub() ] contains
 *  the optimum, which is also what a Solver returning kLowPrecision is held
 *  to whatever its tolerance. -E is only needed where the accuracy a Solver
 *  was asked for is not what it can be held to, which is the case of the
 *  heuristics; see eps_of(). */
extern std::vector< double > solver_eps;

/// the optimality tolerance of Solver @p k
/** What -E declares for @p k if it declares anything, else the accuracy
 *  Solver @p s was asked for, i.e. its dblRelAcc, which is the number that
 *  counts for a Solver that delivers what it is asked. @p dflt is returned
 *  when there is neither: pass no Solver and infinity for a reading that
 *  claims nothing unless -E says otherwise, such as the one-sided bound of
 *  a heuristic, whose dblRelAcc drives its stopping condition and says
 *  nothing about the quality of the solution it returns. */

double eps_of( std::size_t k , Solver * s = nullptr ,
               double dflt = std::numeric_limits< double >::quiet_NaN() );

/*--------------------------------------------------------------------------*/
/// default short command-line options string used by process_standard_arg()
/** Tests that need extra options override this (and long_opts / help) in
 *  their main() before calling process_args(), then handle the
 *  test-specific switches in their own dispatcher. */
extern std::string short_opts;

/// default long command-line options vector

extern std::vector< option > long_opts;

/// default help text printed by docopt()

extern std::string help;

/// @}
/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

/// set precision for long floats (7 digits) in scientific notation

inline std::ostream & def( std::ostream & os )
{
 os.setf( std::ios::scientific , std::ios::floatfield );
 os << std::setprecision( 7 );
 return( os );
 }

/*--------------------------------------------------------------------------*/
/// set precision for short floats (4 digits) in fixed notation

inline std::ostream & fixd( std::ostream & os )
{
 os.setf( std::ios::fixed , std::ios::floatfield );
 os << std::setprecision( 4 );
 return( os );
 }

/*--------------------------------------------------------------------------*/
/// parse a C-string into a value of type T via std::istringstream

template< class T >
inline void Str2Sthg( const char * const str , T & sthg )
{
 std::istringstream( str ) >> sthg;
 }

/*--------------------------------------------------------------------------*/
/// pretty-print a Solver result (objective value or kInfeasible/kUnbounded/error)
/** @param hs   has-solution flag (the caller has decoded the Solver return code)
 *  @param rtrn the raw Solver return code (used only when @p hs is false)
 *  @param fo   the objective value (used only when @p hs is true)
 *
 *  Uses the def manipulator for the numeric output so the same format is shared
 *  by every test program. */

void PrintResults( bool hs , int rtrn , double fo );

/*--------------------------------------------------------------------------*/
/// std::terminate handler that prints the active exception and aborts
/** Install with std::set_terminate( smspp_terminate ); at the top of main(). */

void smspp_terminate( void );

/*--------------------------------------------------------------------------*/
/// apply a (meta-)BlockConfig to a Block
/** If @p b_config is a plain BlockConfig, simply apply() it to @p block.
 *
 *  If it is a
 *
 *    SimpleConfiguration< std::map< std::string , Configuration * > >
 *
 *  scan @p block and every nested sub-Block (BFS, including @p block itself),
 *  and for every Block whose classname() matches a key in the map apply()
 *  a clone() of the corresponding BlockConfig.
 *
 *  Anything else: print an error citing @p fn and exit(1). */

void b_config_Block( Block * block , Configuration * b_config ,
                     const std::string & fn );

/*--------------------------------------------------------------------------*/
/// apply a (meta-)BlockSolverConfig to a Block, optionally clear() for cleanup
/** Mirror of b_config_Block() for BlockSolverConfig. The differences are:
 *
 *  - no clone() is required since BlockSolverConfig::apply() does not transfer
 *    ownership;
 *
 *  - after apply(), every BlockSolverConfig is clear()-ed (or, in the
 *    meta-mode, every value of the map is clear()-ed) so the same object can
 *    be re-applied at the end of main() to unregister the Solver and free
 *    the resources. Pass @p clear_after = false to skip this clear() when
 *    the same BlockSolverConfig is applied multiple times (e.g., re-attach
 *    in a test loop) and a single deferred clear() is done by the caller.
 *
 *  @p fn defaults to the empty string so the same call can be reused at
 *  cleanup time without a filename to cite in error messages. */

void s_config_Block( Block * block , Configuration * s_config ,
                     const std::string & fn = "" ,
                     bool clear_after = true );

/*--------------------------------------------------------------------------*/
/// which Solver getter to use when extracting the objective value
/** Different Solver families "return" their result through different methods:
 *  MILPSolver-like Solver expose the optimum via get_var_value();
 *  LagrangianDualSolver computes a lower bound on a minimization problem;
 *  PrimalProximalHeuristic computes an upper bound. The enum lets the
 *  comparison helpers be parametric over this choice. */

enum class ObjGetter {
 VarValue ,    ///< Solver::get_var_value()
 LowerBound ,  ///< Solver::get_lb()
 UpperBound    ///< Solver::get_ub()
 };

/*--------------------------------------------------------------------------*/
/// extract the objective value from a Solver using the chosen getter
/** Not const-correct on the Solver pointer because SMS++ Solver::get_lb() /
 *  get_ub() / get_var_value() are not marked const. */

double get_obj_value( Solver * slvr , ObjGetter g );

/*--------------------------------------------------------------------------*/
/// format an objective value the canonical way (scientific, 7 digits)

std::string fmt_obj( double v );

/*--------------------------------------------------------------------------*/
/// how a single Solver's result enters the per-instance cross-check
/** The interval [ lb , ub ] that the Solver claims contains the optimum,
 *  plus the gap eps it is allowed to leave.
 *
 *  Both bounds are valid by the base Solver contract, so they are compared
 *  against those of the other Solver at face value. eps is instead a claim
 *  about the quality of the answer, and it is what a heuristic returning
 *  ( -inf , ub ], or a relaxation returning [ lb , +inf ), is held to: how
 *  close its one bound has to come to the best opposite bound anybody
 *  proved, see cross_check(). eps = inf claims nothing, eps = NaN means
 *  "the tolerance the cross-check is called with". */

struct SolverReading {
 double lb  = - std::numeric_limits< double >::infinity();  ///< z* >= lb
 double ub  =   std::numeric_limits< double >::infinity();  ///< z* <= ub
 double eps =   std::numeric_limits< double >::quiet_NaN(); ///< allowed gap

 /// an optimum @p v claimed up to @p e
 static SolverReading exact( double v ,
                             double e =
                              std::numeric_limits< double >::quiet_NaN() )
 { return( SolverReading{ v , v , e } ); }

 /// a lower bound on z*, tight up to @p e (by default, no claim at all)
 static SolverReading lower_bound( double v , double e = s_inf() )
 { return( SolverReading{ v , s_inf() , e } ); }

 /// an upper bound on z*, tight up to @p e (by default, no claim at all)
 static SolverReading upper_bound( double v , double e = s_inf() )
 { return( SolverReading{ - s_inf() , v , e } ); }

 /// the value the Solver reports, i.e. its finite bound (lb first)

 double claimed( void ) const
 { return( std::isfinite( lb ) ? lb : ub ); }

 private:

 static constexpr double s_inf( void )
 { return( std::numeric_limits< double >::infinity() ); }
 };

/*--------------------------------------------------------------------------*/
/// classify+read a Solver: maps (Solver*, index) to a SolverReading
/** Called by SolveAll() once per feasible Solver, after it has compute()d, to
 *  decide how to read and cross-check its result. The default is
 *  read_bounds(), which needs no knowledge of the Solver; tests with needs
 *  (a value that only get_var_value() exposes, or extra per-Solver
 *  bookkeeping) wrap it or pass their own. */

using SolverClassifier =
 std::function< SolverReading ( Solver * , std::size_t ) >;

/*--------------------------------------------------------------------------*/
/// default reading of Solver @p k: its [ get_lb() , get_ub() ] and its -E eps
/** No Solver type or name is ever inspected: a Solver that closes the gap
 *  gives a point, a pure relaxation gives [ lb , +inf ), a pure heuristic
 *  gives ( -inf , ub ], and a Solver that found nothing gives the whole line.
 *  How tight each one is held to be is not deduced here but declared by
 *  @ref solver_eps, because exactness is a property of the configuration and
 *  of the instance, not of the class: a LagrangianDualSolver is exact only
 *  when the relaxation it solves happens to be tight. */

SolverReading read_bounds( Solver * s , std::size_t k );

/*--------------------------------------------------------------------------*/
/// classifier reading every Solver via @p g as a claimed optimum
/** For the Solver that publish their result only through get_var_value(),
 *  which the [ get_lb() , get_ub() ] interval of read_bounds() would not
 *  see. The -E tolerance of each Solver still applies. */

SolverClassifier exact_getter( ObjGetter g = ObjGetter::VarValue );

/*--------------------------------------------------------------------------*/
/// print the uniform per-instance log line
/** Prints "<t0> - <t1> - ... | S0 = <tok0>  S1 = <tok1>  ... [ ~ Ref = <r>
 *  (|diff| = <d>) ]  -> <verdict>". @p times and @p value_tokens must have the
 *  same length (one entry per Solver); a Ref is printed only if @p ref is not
 *  NaN, and the "(|diff| = ...)" detail only if @p diff is also not NaN. Used
 *  both by SolveAll() and by the tests that keep their own solve loop (so every
 *  test prints the same line).
 *
 *  With @p always == false the line is only printed when verbose (the -v
 *  option or the `verbose` environment variable) or on a KO verdict: this is
 *  what the tests that re-solve in a loop of modification rounds use, so
 *  their default output stays terse. SolveAll() passes true instead, so the
 *  one cross-check line per instance is always visible. */

void print_instance_line( const std::vector< double > & times ,
                          const std::vector< std::string > & value_tokens ,
                          double ref ,
                          const std::string & verdict ,
                          double diff =
                           std::numeric_limits< double >::quiet_NaN() ,
                          bool always = false );

/*--------------------------------------------------------------------------*/
/// format a SolverReading as a value token ("v" or "[ lb , ub ]")
/** The bounds are printed as the Solver returned them, the tolerance it
 *  declares playing no part here. */

std::string reading_token( const SolverReading & r );

/*--------------------------------------------------------------------------*/
/// cross-check per-Solver readings against each other and an optional reference
/** Pure verdict logic shared by SolveAll() and by tests that run their own
 *  solve loop (so the cross-check is implemented once). For each Solver k:
 *  @p has_solution[k] says whether it found a solution, @p status[k] is its
 *  return code (for infeasible/unbounded parity), and @p rd[k] is its reading
 *  (consulted only when has_solution[k]).
 *
 *  Feasibility must be unanimous: with no @p ref, a single Solver passes iff
 *  it found a solution, all-infeasible passes as OK(e) and all-unbounded as
 *  OK(u); any mix (or any errored Solver) is KO.
 *
 *  All feasible is the agreement check between Solver. The optimum is not
 *  known, so what each of them is measured against is the best knowledge the
 *  whole set provides, i.e. \f$ LB^* \f$ the largest of the lower bounds and
 *  \f$ UB^* \f$ the smallest of the upper bounds, @p ref being one more
 *  Solver returning \f$[ ref , ref ]\f$. Every Solver owes correctness, i.e.
 *  that its bounds do not contradict the others,
 *
 *  \f[
 *   ub_k \geq LB^* \;\; , \;\; lb_k \leq UB^*
 *  \f]
 *
 *  up to @p tol, which is the numerical noise floor. A Solver that returns
 *  kOK, i.e. that says it delivered what it was asked, also owes quality,
 *  each of its bounds being within its declared \f$\varepsilon_k\f$ of the
 *  best opposite one,
 *
 *  \f[
 *   ub_k - LB^* \leq \varepsilon_k s \;\; , \;\;
 *   UB^* - lb_k \leq \varepsilon_k s \;\; , \;\;
 *   s = \max( 1 , | \cdot | )
 *  \f]
 *
 *  where \f$\varepsilon_k\f$ is never taken smaller than @p tol, below
 *  which the comparison would only measure the noise of the cross-check
 *  itself. This is what holds a heuristic to the quality, and a relaxation
 *  to the gap, that it claims: an exact Solver returns \f$[ v , v ]\f$ and
 *  both reduce to agreeing with everybody else on the optimum. A Solver that
 *  returns kLowPrecision, or that stopped on any other condition, promised
 *  nothing and owes nothing beyond correctness, exactly as one whose
 *  \f$\varepsilon_k\f$ is infinite. Infinite bounds are not compared,
 *  there being nothing to compare.
 *
 *  @p verdict_out receives the token ("OK(f)"/"OK(e)"/"OK(u)"/"OK"/"KO",
 *  where OK(f) means at least one reading pinned the optimum from both
 *  sides); @p diff_out receives |z* - ref| when both are defined (z* being
 *  the value of the first such reading), else NaN. */

bool cross_check( const std::vector< SolverReading > & rd ,
                  const std::vector< bool > & has_solution ,
                  const std::vector< int > & status ,
                  double ref , double tol ,
                  std::string & verdict_out , double & diff_out );

/*--------------------------------------------------------------------------*/
/// run EVERY Solver registered on @p block and cross-check the results
/** For each instance it computes every Solver (timing each), maps each
 *  result to a SolverReading via @p classify, hands the readings to
 *  cross_check() (see its comment for the agreement criterion), prints the
 *  uniform per-instance line via print_instance_line() showing ALL the
 *  Solver values (and @p ref, if given), and returns the pass/fail verdict.
 *
 *  @p tol is the numerical tolerance of the comparisons, i.e. how much of a
 *  difference is attributed to floating-point noise rather than to a
 *  disagreement; how much of a gap each Solver may leave is instead declared
 *  per Solver by -E (@ref solver_eps).
 *
 *  Out-params, if non-null, are populated from the FIRST Solver (value,
 *  has-solution flag, elapsed time, elapsed iterations). */

bool SolveAll( Block * block ,
               const SolverClassifier & classify ,
               double ref = std::numeric_limits< double >::quiet_NaN() ,
               double tol = 1e-5 ,
               double * out_fo1 = nullptr ,
               bool   * out_hs1 = nullptr ,
               double * out_time1 = nullptr ,
               long   * out_it1 = nullptr );

/*--------------------------------------------------------------------------*/
/// SolveAll() reading every Solver with read_bounds(), the usual case

bool SolveAll( Block * block ,
               double ref = std::numeric_limits< double >::quiet_NaN() ,
               double tol = 1e-5 ,
               double * out_fo1 = nullptr ,
               bool   * out_hs1 = nullptr ,
               double * out_time1 = nullptr ,
               long   * out_it1 = nullptr );

/*--------------------------------------------------------------------------*/
/// SolveAll() of the one or two Solver registered to @p block
/** The Solver are read via @p g1 and @p g2 rather than as the interval of
 *  their bounds, which is what the tests whose Solver only publish a value
 *  need. With @p one_sided_le false (the default) both readings claim an
 *  optimum and must agree; with it true the first is only a lower bound and
 *  the second only an upper bound, so the verdict becomes LB <= UB, and each
 *  of the two is held to what -E declares for it, if anything.
 *
 *  Out-params, if non-null, are populated from the first Solver: value,
 *  has-solution flag, elapsed time, elapsed iterations. */

bool SolveBoth( Block * block ,
                ObjGetter g1 = ObjGetter::LowerBound ,
                ObjGetter g2 = ObjGetter::LowerBound ,
                bool one_sided_le = false ,
                double tol = 1e-5 ,
                double * out_fo1 = nullptr ,
                bool   * out_hs1 = nullptr ,
                double * out_time1 = nullptr ,
                long   * out_it1 = nullptr );

/*--------------------------------------------------------------------------*/
/// print "fo ~ Ref = ref (|diff| = ..., OK/KO)" and return whether OK
/** Tolerance: |fo - ref| ≤ rel_tol × max(1, |fo|, |ref|).
 *  @p time1 and @p iters are pre-pended for output symmetry with SolveBoth. */

bool CheckRefValue( double fo , double ref ,
                    double rel_tol = 1e-5 ,
                    double time1 = 0.0 , long iters = 0 );

/*--------------------------------------------------------------------------*/
/// run the (only) Solver of @p block and compare obj against @p ref
/** Bundle solve + ref-check into one call for the single-Solver case. */

bool SolveAndCheckRef( Block * block , double ref ,
                       ObjGetter g = ObjGetter::LowerBound ,
                       double rel_tol = 1e-5 );

/*--------------------------------------------------------------------------*/
/*------------------------ CLI baseline (opt-in) ---------------------------*/
/*--------------------------------------------------------------------------*/
/** Helpers to give tests a getopt-style command line with a small shared
 *  baseline (-h help, -B BlockConfig, -S SolverConfig, -p Block prefix,
 *  -c Config prefix, -D dryrun, -v verbose). Tests that need extra options
 *  override @ref short_opts / @ref long_opts / @ref help and handle the
 *  extra switches via their own dispatcher (calling process_standard_arg()
 *  to deal with the baseline first). */

/// normalize a directory prefix in a portable way (adds trailing separator)

inline std::string normalize_prefix( const std::string & prefix )
{
 if( prefix.empty() )
  return( prefix );

 std::filesystem::path p( prefix );
 p = p.lexically_normal();

 auto s = p.string();
 if( s.empty() )
  return( s );

 if( ( s.back() != '/' ) && ( s.back() != '\\' ) )
  s += std::filesystem::path::preferred_separator;

 return( s );
 }

/*--------------------------------------------------------------------------*/
/// resolve @p name against @p prefix in a portable way
/** If @p name is absolute, returns it normalized; otherwise prepends @p
 *  prefix (if non-empty) and normalizes. Used to apply the --prefix
 *  (block_prefix) and --configdir (conf_prefix) options. */

inline std::string resolve_with_prefix( const std::string & prefix ,
                                        const std::string & name )
{
 if( name.empty() )
  return( name );

 std::filesystem::path p( name );
 if( p.is_absolute() )
  return( p.lexically_normal().string() );

 if( prefix.empty() )
  return( p.lexically_normal().string() );

 return( ( std::filesystem::path( prefix ) / p ).lexically_normal().string() );
 }

/*--------------------------------------------------------------------------*/
/// extract the file basename from a (possibly empty) full path

inline std::string get_filename( const std::string & fullpath )
{
 if( fullpath.empty() )
  return( fullpath );
 return( std::filesystem::path( fullpath ).filename().string() );
 }

/*--------------------------------------------------------------------------*/
/// open an SMS++ nc4 file, return its file type (eProbFile or eBlockFile)

int read_open_netCDF( netCDF::NcFile & f , std::string fn );

/*--------------------------------------------------------------------------*/
/// print the test description and the standard usage to stdout

void docopt( void );

/*--------------------------------------------------------------------------*/
/// processes one of the default command-line arguments
/** Returns true if @p opt was recognised as a standard option, false if it
 *  was unknown (so the caller can dispatch it as a test-specific option or
 *  emit a usage error). Tests that extend short_opts/long_opts should call
 *  process_standard_arg() first and handle the test-specific switches in
 *  their own dispatcher.  */

bool process_standard_arg( int opt );

/*--------------------------------------------------------------------------*/
/// processes all command-line arguments via getopt_long()
/** Drives the parsing loop, sets @p filename from the last positional arg,
 *  and exits 1 with a usage hint on error. */

void process_args( int argc , char ** argv );

/*--------------------------------------------------------------------------*/
/// processes all command-line arguments, with test-specific options
/** Same as the two-argument overload, but @p custom_arg is called first for
 *  each option so that a test can handle its OWN command-line options (the
 *  ones it appends to short_opts / long_opts) on top of the standard ones
 *  (the instance positional and -B / -S / -c / -p / -D / -v, handled
 *  centrally by process_standard_arg()). This is the exact same machinery
 *  the tools use: the standard parameters live here, every test only adds
 *  its specific ones. @p custom_arg returns true if it consumed the option.
 *
 *  If @p filename_optional is true (set by a test that generates its own
 *  instance rather than reading one, e.g. the seed-driven testers) a
 *  missing positional instance is not an error. */

void process_args( int argc , char ** argv , bool ( *custom_arg )( int opt ) );

/*--------------------------------------------------------------------------*/
/// true if the instance positional argument is optional (generator tests)
extern bool filename_optional;

/*--------------------------------------------------------------------------*/
/// require that a BlockSolverConfig was provided (-S); throw otherwise
void require_solver_config( void );

/// require that a BlockConfig was provided (-B); throw otherwise
void require_block_config( void );

/*--------------------------------------------------------------------------*/

#endif  /* __TESTS_COMMON_UTILS */

/*--------------------------------------------------------------------------*/
/*-------------------------- End common_utils.h ----------------------------*/
/*--------------------------------------------------------------------------*/
