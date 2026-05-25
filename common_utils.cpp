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
 constexpr double INF = std::numeric_limits< double >::has_infinity
                        ? std::numeric_limits< double >::infinity()
                        : std::numeric_limits< double >::max();

 try {
  // solve with the 1st Solver- - - - - - - - - - - - - - - - - - - - - - - -
  auto start = std::chrono::system_clock::now();
  Solver * Slvr1 = block->get_registered_solvers().front();
  int rtrn1st = Slvr1->compute( false );
  auto end = std::chrono::system_clock::now();
  std::chrono::duration< double > elapsed = end - start;
  auto time1 = elapsed.count();

  bool hs1st = ( ( ( rtrn1st >= Solver::kOK ) && ( rtrn1st < Solver::kError )
                   && ( rtrn1st != Solver::kUnbounded )
                   && ( rtrn1st != Solver::kInfeasible ) )
                 || ( rtrn1st == Solver::kLowPrecision ) );

  double fo1st = hs1st ? get_obj_value( Slvr1 , g1 ) : -INF;

  if( out_fo1 )   *out_fo1   = fo1st;
  if( out_hs1 )   *out_hs1   = hs1st;
  if( out_time1 ) *out_time1 = time1;
  if( out_it1 )   *out_it1   = Slvr1->get_elapsed_iterations();

  if( block->get_registered_solvers().size() == 1 ) {
   std::cout << fixd << time1 << "\t" << Slvr1->get_elapsed_iterations()
             << "\t";
   PrintResults( hs1st , rtrn1st , fo1st );
   std::cout << std::endl;
   return( hs1st );
   }

  // solve with the 2nd Solver- - - - - - - - - - - - - - - - - - - - - - - -
  start = std::chrono::system_clock::now();
  Solver * Slvr2 = block->get_registered_solvers().back();
  int rtrn2nd = Slvr2->compute( false );
  end = std::chrono::system_clock::now();
  elapsed = end - start;
  auto time2 = elapsed.count();
  std::cout << fixd << time1 << " - " << time2 << " - ";

  bool hs2nd = ( ( ( rtrn2nd >= Solver::kOK ) && ( rtrn2nd < Solver::kError )
                   && ( rtrn2nd != Solver::kUnbounded )
                   && ( rtrn2nd != Solver::kInfeasible ) )
                 || ( rtrn2nd == Solver::kLowPrecision ) );
  double fo2nd = -INF;

  if( hs1st && hs2nd ) {
   fo2nd = get_obj_value( Slvr2 , g2 );
   bool OK;
   if( one_sided_le )
    // ProxHeur-style: fo2nd >= fo1st (within absolute tol)
    OK = ( fo1st - fo2nd <= tol );
   else
    OK = ( std::abs( fo1st - fo2nd ) <=
           tol * std::max( double( 1 ) , std::max( std::abs( fo1st ) ,
                                                   std::abs( fo2nd ) ) ) );

   if( OK ) {
    std::cout << "OK(f)" << std::endl;
    return( true );
    }
   }

  if( ( rtrn1st == Solver::kInfeasible ) &&
      ( rtrn2nd == Solver::kInfeasible ) ) {
   std::cout << "OK(e)" << std::endl;
   return( true );
   }

  if( ( rtrn1st == Solver::kUnbounded ) &&
      ( rtrn2nd == Solver::kUnbounded ) ) {
   std::cout << "OK(u)" << std::endl;
   return( true );
   }

  std::cout << "Solver1 = ";
  PrintResults( hs1st , rtrn1st , fo1st );
  std::cout << " ~ Solver2 = ";
  PrintResults( hs2nd , rtrn2nd , fo2nd );
  std::cout << std::endl;

  return( false );
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
 constexpr double INF = std::numeric_limits< double >::has_infinity
                        ? std::numeric_limits< double >::infinity()
                        : std::numeric_limits< double >::max();

 try {
  auto start = std::chrono::system_clock::now();
  Solver * Slvr1 = block->get_registered_solvers().front();
  int rtrn1st = Slvr1->compute( false );
  auto end = std::chrono::system_clock::now();
  std::chrono::duration< double > elapsed = end - start;
  auto time1 = elapsed.count();

  bool hs1st = ( ( ( rtrn1st >= Solver::kOK ) && ( rtrn1st < Solver::kError )
                   && ( rtrn1st != Solver::kUnbounded )
                   && ( rtrn1st != Solver::kInfeasible ) )
                 || ( rtrn1st == Solver::kLowPrecision ) );

  if( ! hs1st ) {
   std::cout << "Solver returned code " << rtrn1st << std::endl;
   return( false );
   }

  double fo1st = get_obj_value( Slvr1 , g );

  return( CheckRefValue( fo1st , ref , rel_tol ,
                         time1 , Slvr1->get_elapsed_iterations() ) );
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
std::string short_opts = "B:S:p:c:Dv:h";

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
  case 'c': conf_prefix = normalize_prefix( std::string( optarg ) ); break;
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

void process_args( int argc , char ** argv )
{
 exe = get_filename( argv[ 0 ] );
 if( argc < 2 ) {
  std::cout << exe << ": no input file" << std::endl
            << "Try '" << exe << " --help' for more information" << std::endl;
  exit( 1 );
  }

 while( true ) {  // options
  const auto opt = getopt_long( argc , argv , short_opts.data() ,
                                long_opts.data() , nullptr );
  if( opt == -1 ) break;

  if( ! process_standard_arg( opt ) ) {
   std::cout << "Try '" << exe << " --help' for more information"
             << std::endl;
   exit( 1 );
   }
  }

 if( optind < argc )  // last argument == [Block] filename
  filename = std::string( argv[ optind ] );
 else {
  std::cout << exe << ": no input file" << std::endl
            << "Try '" << exe << " --help' for more information" << std::endl;
  exit( 1 );
  }
 }

/*--------------------------------------------------------------------------*/
/*------------------------ End common_utils.cpp ----------------------------*/
/*--------------------------------------------------------------------------*/
