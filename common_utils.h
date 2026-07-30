/*--------------------------------------------------------------------------*/
/*--------------------------- common_utils.h -------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Common utilities shared by the SMS++ test programs (compare_formulations,
 * LagrangianDualSolver_UC, TwoStageStochasticBlock, InvestmentBlock, ...).
 *
 * Pure helpers that are bit-fedeli copies of code previously duplicated across
 * the four test.cpp files: ostream manipulators (def, fixd), string->T parser
 * (Str2Sthg), pretty-printing of Solver return codes (PrintResults), the
 * std::terminate handler that dumps the active exception (smspp_terminate),
 * and the meta-configuration BFS helpers (b_config_Block, s_config_Block)
 * which apply a BlockConfig / BlockSolverConfig to a Block or, if the
 * Configuration is a
 *
 *   SimpleConfiguration< std::map< std::string , Configuration * > >
 *
 * dispatch to every nested sub-Block matching the classname() key.
 *
 * Solver-comparison and reference-objective helpers cover Pattern B (one
 * Block with an arbitrary number of Solver registered): SolveAll() is the
 * general engine, it runs every registered Solver, prints the uniform
 * per-instance line (timings + every Solver value, and the reference if any)
 * via print_instance_line(), and returns the cross-check verdict; SolveBoth()
 * is the 1-or-2-Solver wrapper over it, CheckRefValue() prints the comparison
 * against a reference objective, and SolveAndCheckRef() is the single-Solver
 * convenience that bundles solve + ref-check in one call. The classifier
 * (SolverClassifier / SolverReading) lets a test say how each Solver's result
 * enters the cross-check (exact, lower/upper bound, or relaxation bracket)
 * without pulling any solver-specific dependency into common_utils. Pattern A
 * (two separate Block each with one Solver, as in compare_formulations) is
 * structurally different and still lives in that test.cpp.
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
/** The objective getters (ObjGetter) only say WHERE a Solver stores its
 *  answer; SolverReading::Kind says HOW that answer is compared against the
 *  others when more than one Solver is registered:
 *
 *  - Exact:      an exact optimum; all Exact readings must agree on z*;
 *  - LowerBound: a valid lower bound (LB <= z*);
 *  - UpperBound: a valid upper bound (UB >= z*);
 *  - Bracket:    a relaxation that brackets z* in [ lb , ub ]. */

struct SolverReading {
 enum class Kind { Exact , LowerBound , UpperBound , Bracket };

 Kind   kind  = Kind::Exact;
 double value = std::numeric_limits< double >::quiet_NaN();   ///< Exact/LB/UB
 double lb    = - std::numeric_limits< double >::infinity();  ///< Bracket only
 double ub    =   std::numeric_limits< double >::infinity();  ///< Bracket only
 };

/*--------------------------------------------------------------------------*/
/// classify+read a Solver: maps (Solver*, index) to a SolverReading
/** Called by SolveAll() once per feasible Solver, after it has compute()d, to
 *  decide how to read and cross-check its result. The default classifiers
 *  below cover the common cases (every Solver an Exact optimum, or a relaxation
 *  bracketing z* recognised by its classname()); tests with bespoke needs
 *  (e.g. one-sided bounds, or extra per-Solver bookkeeping) wrap these or pass
 *  their own. */

using SolverClassifier =
 std::function< SolverReading ( Solver * , std::size_t ) >;

/*--------------------------------------------------------------------------*/
/// default classifier: read every Solver via @p g as an Exact optimum

SolverClassifier exact_getter( ObjGetter g = ObjGetter::VarValue );

/// per-Solver classifier: read Solver k via getters[k] (Exact), else @p dflt

SolverClassifier exact_getters( std::vector< ObjGetter > getters ,
                                ObjGetter dflt = ObjGetter::VarValue );

/// classifier: read a Solver as a [ get_lb() , get_ub() ] Bracket when its
/// classname() contains "Relaxation", else as an Exact get_var_value() optimum

/** Encodes the :RelaxationSolver naming convention so that any relaxation
 *  Solver is cross-checked uniformly (it only brackets z*) without the test
 *  depending on its concrete type. */

SolverClassifier relaxation_aware_getter( void );

/*--------------------------------------------------------------------------*/
/// print the uniform per-instance log line
/** Prints "<t0> - <t1> - ... | S0 = <tok0>  S1 = <tok1>  ... [ ~ Ref = <r>
 *  (|diff| = <d>) ]  -> <verdict>". @p times and @p value_tokens must have the
 *  same length (one entry per Solver); a Ref is printed only if @p ref is not
 *  NaN, and the "(|diff| = ...)" detail only if @p diff is also not NaN. Used
 *  both by SolveAll() and by the tests that keep their own solve loop (so every
 *  test prints the same line). */

void print_instance_line( const std::vector< double > & times ,
                          const std::vector< std::string > & value_tokens ,
                          double ref ,
                          const std::string & verdict ,
                          double diff =
                           std::numeric_limits< double >::quiet_NaN() );

/*--------------------------------------------------------------------------*/
/// format a SolverReading as a value token ("v" or "[ lb , ub ]")

std::string reading_token( const SolverReading & r );

/*--------------------------------------------------------------------------*/
/// cross-check per-Solver readings against each other and an optional reference
/** Pure verdict logic shared by SolveAll() and by tests that run their own
 *  solve loop (so the cross-check is implemented once). For each Solver k:
 *  @p has_solution[k] says whether it found a solution, @p status[k] is its
 *  return code (for infeasible/unbounded parity), and @p rd[k] is its reading
 *  (consulted only when has_solution[k]).
 *
 *  Verdict (relative tolerance @p tol, a ~ b iff |a-b| <= tol*max(1,|a|,|b|)):
 *  - 1 Solver, no @p ref: passes iff it found a solution;
 *  - all-infeasible / all-unbounded and no @p ref: OK(e) / OK(u);
 *  - otherwise every Exact reading must agree on z*, every LowerBound <= z*,
 *    every UpperBound >= z*, every Bracket must contain z*, and (if given)
 *    @p ref must match z* (z* is taken from the Exact readings, else from
 *    @p ref, else from the bounds' mutual consistency).
 *
 *  @p verdict_out receives the token ("OK(f)"/"OK(e)"/"OK(u)"/"OK"/"KO");
 *  @p diff_out receives |z* - ref| when both are defined, else NaN. */

bool cross_check( const std::vector< SolverReading > & rd ,
                  const std::vector< bool > & has_solution ,
                  const std::vector< int > & status ,
                  double ref , double tol ,
                  std::string & verdict_out , double & diff_out );

/*--------------------------------------------------------------------------*/
/// run EVERY Solver registered on @p block and cross-check the results
/** Generalizes SolveBoth() to an arbitrary number M >= 1 of registered
 *  Solver. For each instance it computes every Solver (timing each), prints
 *  the uniform per-instance line via print_instance_line() showing ALL the
 *  Solver values (and @p ref, if given), and returns the pass/fail verdict:
 *
 *  - M == 1, no @p ref: passes iff the Solver found a solution (the plain
 *    "solve and show the value" case);
 *  - M == 1, with @p ref: the value must match @p ref within @p tol;
 *  - M >= 2: feasibility must be unanimous (all-infeasible -> OK(e),
 *    all-unbounded -> OK(u), mixed -> KO); among the feasible Solver every
 *    Exact reading must agree on z*, every LowerBound must be <= z*, every
 *    UpperBound >= z*, every Bracket must contain z*, and (if given) @p ref
 *    must match z*. When no Exact reading exists, z* is taken from @p ref if
 *    provided, otherwise the bounds must be mutually consistent
 *    (max LB <= min UB).
 *
 *  Tolerances are relative: a ~ b iff |a - b| <= tol * max(1,|a|,|b|).
 *  Out-params, if non-null, are populated from the FIRST Solver (value,
 *  has-solution flag, elapsed time, elapsed iterations).
 *
 *  If @p bsc is given, the Solver that are run are those that @p bsc has
 *  registered to @p block [see BlockSolverConfig::get_Solvers()], in that
 *  order, rather than ALL the Solver registered to the Block: this keeps
 *  the cross-check to the Solver the test itself attached even when
 *  somebody else (say, an enumerative Solver under test) has registered
 *  further Solver of its own to the same Block. */

bool SolveAll( Block * block ,
               const SolverClassifier & classify ,
               double ref = std::numeric_limits< double >::quiet_NaN() ,
               double tol = 1e-5 ,
               double * out_fo1 = nullptr ,
               bool   * out_hs1 = nullptr ,
               double * out_time1 = nullptr ,
               long   * out_it1 = nullptr ,
               BlockSolverConfig * bsc = nullptr );

/*--------------------------------------------------------------------------*/
/// convenience SolveAll(): read every Solver via @p getters as Exact optima
/** @p getters is matched positionally to the registered Solver; if empty,
 *  every Solver is read via get_var_value(); if shorter than the number of
 *  Solver, the missing entries default to get_var_value(). The optional
 *  @p bsc restricts the run to the Solver it registered [see the main
 *  SolveAll()]. */

bool SolveAll( Block * block ,
               double ref = std::numeric_limits< double >::quiet_NaN() ,
               const std::vector< ObjGetter > & getters = {} ,
               double tol = 1e-5 ,
               double * out_fo1 = nullptr ,
               bool   * out_hs1 = nullptr ,
               double * out_time1 = nullptr ,
               long   * out_it1 = nullptr ,
               BlockSolverConfig * bsc = nullptr );

/*--------------------------------------------------------------------------*/
/// Pattern B: run the Solver(s) registered on a single Block
/** If @p block has just one registered Solver, this calls compute() on it,
 *  prints "<time>\\t<iters>\\t<fo>\\n" and returns the has-solution flag.
 *
 *  If @p block has two registered Solver, this calls compute() on both
 *  (Slvr1 = front(), Slvr2 = back()), prints "<time1> - <time2> - <verdict>"
 *  with verdict ∈ { OK(f) , OK(e) , OK(u) , Solver1 = ... ~ Solver2 = ... }
 *  and returns true iff the two results agree.
 *
 *  Agreement criterion:
 *  - if @p one_sided_le is false (default), |fo1 - fo2| ≤ tol × max(1,|fo1|,|fo2|);
 *  - if true (ProxHeur-style), fo1 - fo2 ≤ tol (absolute, one-sided);
 *  in either case kInfeasible / kUnbounded parity is also accepted.
 *
 *  @p g1 / @p g2 select which getter to use on each Solver.
 *  Out-params, if non-null, are populated from Slvr1: fo1, has-solution flag,
 *  elapsed time, elapsed iterations. */

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
