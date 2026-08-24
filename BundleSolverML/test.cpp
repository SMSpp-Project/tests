/*--------------------------------------------------------------------------*/
/*------------------------------ File test.cpp -----------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Training and benchmarking harness for BundleSolverML.
 *
 * TWO MODES
 *
 *   train    Solve the instances of a split one after another, keeping one
 *            solver throughout so its network accumulates the updates, and
 *            write the weights out at the end.
 *
 *   compare  Run each instance of a split under two solver configurations
 *            and report how they relate, optionally giving the second one
 *            the weights produced by a training run.
 *
 * WHY THE SOLVER MOVES AND NOT THE NETWORK
 *   BundleSolverML offers set_shared_net(), which hands a network to the
 *   solver from outside; the documentation calls it the pattern to use for
 *   training. On Windows it crashes: a shared_ptr< Net > held by the
 *   executable while the solver runs a long solve inside the DLL gives an
 *   access violation partway through compute(). The same code is clean on
 *   Linux under ASan, so it is a boundary problem rather than a logic one,
 *   but it makes that route unusable here.
 *
 *   What works is the opposite arrangement: build one solver, and move it
 *   from Block to Block with set_Block(). The network never leaves the DLL,
 *   the same object is carried across every instance, and the weights
 *   accumulate exactly as intended. Weights come out through SaveModel(),
 *   which takes a filename, so nothing crosses the boundary there either.
 *
 *   One consequence: SaveModel() writes the parameters and not the shape of
 *   the network, so a training run and the evaluation that uses its weights
 *   have to be configured with the same architecture.
 *
 * WHY THE RATIO AND NOT THE TIME
 *   UCBlock and MMCF instances differ enough in size that absolute times
 *   say more about the instances than about the solver, and the network is
 *   evaluated at every iteration, which costs the same whatever the
 *   sub-problems cost. Iterations are the property of the algorithm; that
 *   is what is reported.
 *
 * USAGE
 *   test train   <split> <data-dir> <block-cfg> <ml-cfg> -o <weights>
 *                [-e <epochs>] [-t <type>] [-c <config-dir>]
 *
 *   test compare <split> <data-dir> <block-cfg> <cfg-A> <cfg-B>
 *                [-r <weights>] [-o <results.csv>] [-t <type>]
 *                [-c <config-dir>]
 */
/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include "Block.h"
#include "BlockSolverConfig.h"
#include "Solver.h"

#include "MMCFBlock.h"

#include "BundleSolverML.h"
#include "LagrangianDualSolver.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <random>
#include <string>
#include <vector>

/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*-------------------------------- TYPES -----------------------------------*/
/*--------------------------------------------------------------------------*/

struct RunResult {
 bool        solved  = false;
 double      seconds = 0.0;
 long        iters   = -1;
 double      lower   = 0.0;
 double      upper   = 0.0;
 int         status  = -1;
 std::string error;
 };

/*--------------------------------------------------------------------------*/

struct InstanceResult {
 std::string name;
 std::string family;
 RunResult   a;
 RunResult   b;
 };

/*--------------------------------------------------------------------------*/

struct Args {
 std::string mode;
 std::string split_file;
 std::string data_dir;
 std::string block_cfg;
 std::string cfg_a;
 std::string cfg_b;
 std::string config_dir;
 std::string out;         // weights file in train, csv in compare
 std::string weights;     // -r: weights to load in compare
 int         epochs   = 1;
 char        filetype = 0;
 };

/*--------------------------------------------------------------------------*/
/*------------------------------- HELPERS ----------------------------------*/
/*--------------------------------------------------------------------------*/

static std::string join( const std::string & dir , const std::string & name )
{
 if( dir.empty() )
  return( name );

 const char last = dir.back();
 if( ( last == '/' ) || ( last == '\\' ) )
  return( dir + name );

 return( dir + "/" + name );
 }

/*--------------------------------------------------------------------------*/

static std::string family_of( const std::string & entry )
{
 const auto pos = entry.find_last_of( "/\\" );
 return( pos == std::string::npos ? std::string( "-" )
                                  : entry.substr( 0 , pos ) );
 }

/*--------------------------------------------------------------------------*/

static std::vector< std::string > read_split( const std::string & path )
{
 std::vector< std::string > names;
 std::ifstream in( path );

 if( ! in )
  return( names );

 for( std::string line ; std::getline( in , line ) ; ) {
  while( ( ! line.empty() ) &&
         ( ( line.back() == '\r' ) || ( line.back() == ' ' ) ) )
   line.pop_back();

  if( ! line.empty() )
   names.push_back( line );
  }

 return( names );
 }

/*--------------------------------------------------------------------------*/
 /// load an instance and build its abstract representation
 /** For the text formats the BlockConfig has to be applied before the
  * abstract representation is generated: with MMCF it is what chooses Flow
  * or Knapsack, and that decides which Variables and Constraints are the
  * right ones to create. */

static Block * load_instance( const std::string & path ,
                              const std::string & block_cfg ,
                              char filetype )
{
 Block * block = nullptr;

 if( filetype ) {
  auto mmcf = new MMCFBlock;
  block = mmcf;
  mmcf->load( path , filetype );
  mmcf->PreProcess();
  }
 else {
  block = Block::deserialize( path );
  if( ! block )
   throw( std::runtime_error( "could not deserialise " + path ) );
  }

 auto c  = Configuration::deserialize( block_cfg );
 auto bc = dynamic_cast< BlockConfig * >( c );

 if( ! bc ) {
  delete c;
  delete block;
  throw( std::runtime_error( block_cfg + " is not a BlockConfig" ) );
  }

 bc->apply( block );
 delete bc;

 if( filetype ) {
  auto mmcf = static_cast< MMCFBlock * >( block );
  mmcf->generate_abstract_variables();
  mmcf->generate_abstract_constraints();
  mmcf->generate_objective();
  }

 return( block );
 }

/*--------------------------------------------------------------------------*/
 /// the Solver that should be asked to solve
 /** A Block may carry more than one: MMCF attaches a MILPSolver alongside
  * the LagrangianDualSolver, and front() would run the MILP one, which
  * never enters the bundle loop. The configurations list the Lagrangian
  * solver last, as tests/MMCFBlock also assumes. */

static Solver * outer_solver( Block * block )
{
 auto reg = block->get_registered_solvers();
 return( reg.empty() ? nullptr : reg.back() );
 }

/*--------------------------------------------------------------------------*/
 /// the BundleSolverML doing the bundle iterations, if there is one

static BundleSolverML * ml_inside( Solver * solver )
{
 if( ! solver )
  return( nullptr );

 if( auto lds = dynamic_cast< LagrangianDualSolver * >( solver ) )
  return( dynamic_cast< BundleSolverML * >( lds->get_inner_Solver() ) );

 return( dynamic_cast< BundleSolverML * >( solver ) );
 }

/*--------------------------------------------------------------------------*/
 /// iterations of the Solver that actually ran

static long iterations_of( Solver * solver )
{
 if( ! solver )
  return( -1 );

 if( auto ml = ml_inside( solver ) ) {
  const long n = ml->get_elapsed_iterations();
  if( n > 0 )
   return( n );
  }

 return( solver->get_elapsed_iterations() );
 }

/*--------------------------------------------------------------------------*/
/*------------------------------- TRAINING ---------------------------------*/
/*--------------------------------------------------------------------------*/
 /// solve every instance of the split with one solver
 /** The solver is configured once, on the first instance, and then carried
  * to each of the others with set_Block(). Its network goes with it, so the
  * updates BundleSolverML makes during compute() accumulate over the whole
  * split rather than being discarded with each Block.
  *
  * The instances are reshuffled every epoch so the network does not come to
  * depend on the order they happen to sit in the split file; the seed is
  * fixed so a run can be repeated. */

static int train( const Args & args ,
                  const std::string & block_cfg ,
                  const std::string & ml_cfg )
{
 auto names = read_split( args.split_file );

 if( names.empty() ) {
  std::cerr << "no instances read from " << args.split_file << std::endl;
  return( 1 );
  }

 std::cout << names.size() << " instances, " << args.epochs
           << ( args.epochs == 1 ? " epoch" : " epochs" ) << std::endl
           << "  config: " << ml_cfg << std::endl << std::endl;

 std::mt19937 rng( 42 );

 // these outlive the loop: one solver, one configuration, one network
 Block *             held   = nullptr;   // the Block the solver sits on
 BlockSolverConfig * bsc    = nullptr;
 Solver *            solver = nullptr;
 BundleSolverML *    ml     = nullptr;

 int total_solved = 0 , total_failed = 0;

 for( int epoch = 1 ; epoch <= args.epochs ; ++epoch ) {

  std::shuffle( names.begin() , names.end() , rng );

  int    solved = 0 , failed = 0;
  double seconds = 0.0;
  long   iters   = 0;

  std::cout << "epoch " << epoch << "/" << args.epochs << std::endl;

  for( std::size_t i = 0 ; i < names.size() ; ++i ) {

   std::cout << "  [" << ( i + 1 ) << "/" << names.size() << "] "
             << std::left << std::setw( 30 ) << names[ i ] << std::right
             << std::flush;

   Block * block = nullptr;

   try {
    block = load_instance( join( args.data_dir , names[ i ] ) ,
                           block_cfg , args.filetype );

    if( ! solver ) {
     // first instance: build the solver here and keep it from now on
     auto s = Configuration::deserialize( ml_cfg );
     bsc = dynamic_cast< BlockSolverConfig * >( s );

     if( ! bsc ) {
      delete s;
      throw( std::runtime_error( ml_cfg + " is not a BlockSolverConfig" ) );
      }

     bsc->apply( block );
     solver = outer_solver( block );

     if( ! solver )
      throw( std::runtime_error( "no Solver registered" ) );

     ml = ml_inside( solver );

     if( ! ml )
      throw( std::runtime_error(
       "no BundleSolverML found; does " + ml_cfg +
       " set str_LDSlv_ISName to BundleSolverML?" ) );
     }
    else {
     // every other instance: move the solver across, and only then let go
     // of the Block it was on
     solver->set_Block( nullptr );
     delete held;
     held = nullptr;
     solver->set_Block( block );
     }

    held = block;
    block = nullptr;          // ownership is with `held` from here

    const auto t0 = std::chrono::steady_clock::now();
    const int status = solver->compute();
    const auto t1 = std::chrono::steady_clock::now();

    const double s = std::chrono::duration< double >( t1 - t0 ).count();
    const long   n = iterations_of( solver );

    if( status == Solver::kOK ) {
     ++solved;
     seconds += s;
     if( n > 0 )
      iters += n;

     std::cout << std::fixed << std::setprecision( 3 )
               << std::setw( 9 ) << s << "s"
               << std::setw( 8 ) << n << " it"
               << std::scientific << std::setprecision( 6 )
               << std::setw( 16 ) << solver->get_lb() << std::endl;
     }
    else {
     ++failed;
     std::cout << "   status " << status << std::endl;
     }
    }
   catch( const std::exception & e ) {
    ++failed;
    std::cout << "   failed: " << e.what() << std::endl;
    delete block;
    }
   catch( ... ) {
    ++failed;
    std::cout << "   failed: unknown exception" << std::endl;
    delete block;
    }
   }

  std::cout << "  solved " << solved << "/" << names.size();
  if( solved ) {
   std::cout << std::fixed << std::setprecision( 1 )
             << ", " << seconds << "s total";
   if( iters )
    std::cout << ", " << iters << " iterations ("
              << std::setprecision( 0 ) << ( double( iters ) / solved )
              << " per instance)";
   }
  std::cout << std::endl;

  total_solved += solved;
  total_failed += failed;

  // written every epoch, so an interrupted run still leaves something
  if( ml && ( ! args.out.empty() ) && solved ) {
   ml->SaveModel( args.out );
   std::cout << "  weights -> " << args.out << std::endl;
   }

  std::cout << std::endl;
  }

 // ---- tear down what was held across the loop -------------------------
 try {
  if( solver )
   solver->set_Block( nullptr );
  if( bsc ) {
   delete bsc;
   }
  delete held;
  }
 catch( ... ) {}

 if( total_solved == 0 ) {
  std::cerr << "nothing was solved; no weights were produced" << std::endl;
  return( 1 );
  }

 std::cout << "Trained on " << total_solved << " successful solves";
 if( total_failed )
  std::cout << " (" << total_failed << " failed)";
 std::cout << "." << std::endl;

 return( 0 );
 }

/*--------------------------------------------------------------------------*/
/*------------------------------ COMPARING ---------------------------------*/
/*--------------------------------------------------------------------------*/
 /// one instance under one configuration, torn down afterwards
 /** If @p weights is non-empty and the configuration produced a
  * BundleSolverML, the weights are loaded into it before the solve. The
  * file is read on the far side of the boundary, so nothing has to be
  * passed in. */

static RunResult run_one( const std::string & instance_path ,
                          const std::string & block_cfg ,
                          const std::string & solver_cfg ,
                          const std::string & weights ,
                          char filetype )
{
 RunResult r;

 Block * block = nullptr;
 BlockSolverConfig * bsc = nullptr;

 try {
  block = load_instance( instance_path , block_cfg , filetype );

  auto s = Configuration::deserialize( solver_cfg );
  bsc = dynamic_cast< BlockSolverConfig * >( s );

  if( ! bsc ) {
   delete s;
   throw( std::runtime_error( solver_cfg + " is not a BlockSolverConfig" ) );
   }

  bsc->apply( block );

  auto solver = outer_solver( block );
  if( ! solver )
   throw( std::runtime_error( "no Solver registered" ) );

  if( ! weights.empty() )
   if( auto ml = ml_inside( solver ) )
    ml->LoadModel( weights );

  const auto t0 = std::chrono::steady_clock::now();
  r.status = solver->compute();
  const auto t1 = std::chrono::steady_clock::now();

  r.seconds = std::chrono::duration< double >( t1 - t0 ).count();
  r.iters   = iterations_of( solver );
  r.lower   = solver->get_lb();
  r.upper   = solver->get_ub();
  r.solved  = ( r.status == Solver::kOK );
  }
 catch( const std::exception & e ) {
  r.error  = e.what();
  r.solved = false;
  }
 catch( ... ) {
  r.error  = "unknown exception";
  r.solved = false;
  }

 try {
  if( bsc ) { bsc->clear(); bsc->apply( block ); delete bsc; }
  delete block;
  }
 catch( ... ) {}

 return( r );
 }

/*--------------------------------------------------------------------------*/
/*------------------------------ REPORTING ---------------------------------*/
/*--------------------------------------------------------------------------*/

static void print_table( const std::vector< InstanceResult > & results )
{
 std::cout << std::endl
           << std::left  << std::setw( 30 ) << "instance"
           << std::right << std::setw( 10 ) << "A (s)"
           << std::setw( 10 ) << "B (s)"
           << std::setw( 9 )  << "A it"
           << std::setw( 9 )  << "B it"
           << std::setw( 10 ) << "it ratio" << std::endl
           << std::string( 78 , '-' ) << std::endl
           << std::fixed;

 for( const auto & r : results ) {
  std::cout << std::left << std::setw( 30 ) << r.name << std::right;

  if( r.a.solved ) std::cout << std::setw( 10 ) << std::setprecision( 3 )
                             << r.a.seconds;
  else             std::cout << std::setw( 10 ) << "failed";

  if( r.b.solved ) std::cout << std::setw( 10 ) << std::setprecision( 3 )
                             << r.b.seconds;
  else             std::cout << std::setw( 10 ) << "failed";

  std::cout << std::setw( 9 ) << r.a.iters << std::setw( 9 ) << r.b.iters;

  if( r.a.solved && r.b.solved && ( r.b.iters > 0 ) )
   std::cout << std::setw( 10 ) << std::setprecision( 2 )
             << ( double( r.a.iters ) / double( r.b.iters ) );
  else
   std::cout << std::setw( 10 ) << "-";

  std::cout << std::endl;
  }
 }

/*--------------------------------------------------------------------------*/
 /// bounds that differ are worth seeing: a configuration that takes fewer
 /// iterations but stops at a worse bound has not actually won

static void print_bound_differences(
 const std::vector< InstanceResult > & results )
{
 bool any = false;

 for( const auto & r : results ) {
  if( ! ( r.a.solved && r.b.solved ) )
   continue;

  const double scale = std::max( 1.0 , std::abs( r.a.lower ) );
  if( std::abs( r.a.lower - r.b.lower ) / scale < 1e-6 )
   continue;

  if( ! any ) {
   std::cout << std::endl << "Instances where the bounds differ:"
             << std::endl;
   any = true;
   }

  std::cout << "  " << std::left << std::setw( 32 ) << r.name
            << std::right << std::scientific << std::setprecision( 8 )
            << std::setw( 18 ) << r.a.lower
            << std::setw( 18 ) << r.b.lower << std::endl;
  }

 std::cout << std::fixed;
 }

/*--------------------------------------------------------------------------*/

static void print_summary( const std::vector< InstanceResult > & results )
{
 std::map< std::string , std::vector< const InstanceResult * > > by_family;

 int both = 0 , failed = 0;

 for( const auto & r : results )
  if( r.a.solved && r.b.solved ) {
   by_family[ r.family ].push_back( &r );
   ++both;
   }
  else
   ++failed;

 std::cout << std::endl << std::string( 78 , '=' ) << std::endl
           << "Solved by both configurations: " << both
           << " of " << results.size();

 if( failed )
  std::cout << "   (" << failed << " left out)";

 std::cout << std::endl << std::endl;

 if( both == 0 ) {
  std::cout << "Nothing to compare." << std::endl;
  return;
  }

 std::cout << std::left  << std::setw( 22 ) << "family"
           << std::right << std::setw( 7 )  << "count"
           << std::setw( 10 ) << "A (s)"
           << std::setw( 10 ) << "B (s)"
           << std::setw( 9 )  << "A it"
           << std::setw( 9 )  << "B it"
           << std::setw( 12 ) << "it ratio" << std::endl
           << std::string( 79 , '-' ) << std::endl;

 for( const auto & entry : by_family ) {
  double sum_a = 0.0 , sum_b = 0.0;
  long   it_a  = 0   , it_b  = 0;

  for( auto r : entry.second ) {
   sum_a += r->a.seconds;
   sum_b += r->b.seconds;
   if( r->a.iters > 0 ) it_a += r->a.iters;
   if( r->b.iters > 0 ) it_b += r->b.iters;
   }

  const auto n = double( entry.second.size() );

  std::cout << std::left << std::setw( 22 ) << entry.first
            << std::right << std::setw( 7 ) << entry.second.size()
            << std::setw( 10 ) << std::setprecision( 3 ) << ( sum_a / n )
            << std::setw( 10 ) << std::setprecision( 3 ) << ( sum_b / n )
            << std::setw( 9 )  << std::setprecision( 0 ) << ( it_a / n )
            << std::setw( 9 )  << std::setprecision( 0 ) << ( it_b / n );

  if( it_b > 0 )
   std::cout << std::setw( 11 ) << std::setprecision( 2 )
             << ( double( it_a ) / double( it_b ) ) << "x";
  else
   std::cout << std::setw( 12 ) << "-";

  std::cout << std::endl;
  }

 std::cout << std::endl
           << "Ratios above 1 mean B took fewer iterations." << std::endl;
 }

/*--------------------------------------------------------------------------*/

static void write_csv( const std::string & path ,
                       const std::vector< InstanceResult > & results )
{
 std::ofstream out( path );
 if( ! out ) {
  std::cerr << "could not write " << path << std::endl;
  return;
  }

 out << "instance,family,"
        "a_solved,a_seconds,a_iters,a_lower,a_upper,a_status,"
        "b_solved,b_seconds,b_iters,b_lower,b_upper,b_status" << std::endl
     << std::setprecision( 10 );

 for( const auto & r : results )
  out << r.name << ',' << r.family << ','
      << ( r.a.solved ? 1 : 0 ) << ',' << r.a.seconds << ','
      << r.a.iters << ','
      << r.a.lower << ',' << r.a.upper << ',' << r.a.status << ','
      << ( r.b.solved ? 1 : 0 ) << ',' << r.b.seconds << ','
      << r.b.iters << ','
      << r.b.lower << ',' << r.b.upper << ',' << r.b.status << std::endl;

 std::cout << std::endl << "Wrote " << results.size() << " rows to "
           << path << std::endl;
 }

/*--------------------------------------------------------------------------*/

static int compare( const Args & args ,
                    const std::string & block_cfg ,
                    const std::string & cfg_a ,
                    const std::string & cfg_b )
{
 const auto names = read_split( args.split_file );

 if( names.empty() ) {
  std::cerr << "no instances read from " << args.split_file << std::endl;
  return( 1 );
  }

 std::cout << names.size() << " instances from " << args.split_file
           << std::endl
           << "  A: " << cfg_a << std::endl
           << "  B: " << cfg_b;

 if( args.weights.empty() )
  std::cout << "   (untrained: no -r given)";
 else
  std::cout << "   with weights from " << args.weights;

 std::cout << std::endl;

 std::vector< InstanceResult > results;
 results.reserve( names.size() );

 int done = 0;

 for( const auto & name : names ) {
  const std::string path = join( args.data_dir , name );

  std::cout << "[" << ++done << "/" << names.size() << "] " << name
            << std::flush;

  InstanceResult r;
  r.name   = name;
  r.family = family_of( name );

  // A is the baseline and never gets the weights
  r.a = run_one( path , block_cfg , cfg_a , "" , args.filetype );
  r.b = run_one( path , block_cfg , cfg_b , args.weights , args.filetype );

  if( r.a.solved && r.b.solved )
   std::cout << "   " << std::fixed << std::setprecision( 3 )
             << r.a.seconds << "s / " << r.b.seconds << "s"
             << "   " << r.a.iters << " / " << r.b.iters << " it"
             << std::endl;
  else {
   std::cout << "   failed" << std::endl;
   if( ! r.a.error.empty() )
    std::cout << "      A: " << r.a.error << std::endl;
   if( ! r.b.error.empty() )
    std::cout << "      B: " << r.b.error << std::endl;
   }

  results.push_back( r );
  }

 print_table( results );
 print_bound_differences( results );
 print_summary( results );

 if( ! args.out.empty() )
  write_csv( args.out , results );

 const bool any = std::any_of( results.begin() , results.end() ,
                               []( const InstanceResult & r )
                                { return( r.a.solved && r.b.solved ); } );

 return( any ? 0 : 1 );
 }

/*--------------------------------------------------------------------------*/
/*------------------------------ ARGUMENTS ---------------------------------*/
/*--------------------------------------------------------------------------*/

static void usage( const char * prog )
{
 std::cerr
  << "usage:" << std::endl
  << "  " << prog << " train   <split> <data-dir> <block-cfg> <ml-cfg>"
  << " -o <weights> [-e <epochs>]" << std::endl
  << "  " << prog << " compare <split> <data-dir> <block-cfg> <cfg-A>"
  << " <cfg-B> [-r <weights>] [-o <results.csv>]" << std::endl << std::endl
  << "  -c <dir>   prefix for the configuration files" << std::endl
  << "  -t <c>     text instance format, as -t in tests/MMCFBlock"
  << std::endl
  << "             ('m' for Mnetgen, 'p' for JLF); omit for netCDF"
  << std::endl
  << "  -r <file>  in compare: weights for the B side, written by a"
  << std::endl
  << "             training run; without it B runs untrained" << std::endl;
 }

/*--------------------------------------------------------------------------*/

static bool parse_args( int argc , char ** argv , Args & args )
{
 if( argc < 2 )
  return( false );

 args.mode = argv[ 1 ];
 if( ( args.mode != "train" ) && ( args.mode != "compare" ) )
  return( false );

 std::vector< std::string > positional;

 for( int i = 2 ; i < argc ; ++i ) {
  const std::string a = argv[ i ];

  if( a == "-c" ) {
   if( ++i >= argc ) return( false );
   args.config_dir = argv[ i ];
   }
  else if( a == "-o" ) {
   if( ++i >= argc ) return( false );
   args.out = argv[ i ];
   }
  else if( a == "-r" ) {
   if( ++i >= argc ) return( false );
   args.weights = argv[ i ];
   }
  else if( a == "-t" ) {
   if( ++i >= argc ) return( false );
   args.filetype = argv[ i ][ 0 ];
   }
  else if( a == "-e" ) {
   if( ++i >= argc ) return( false );
   args.epochs = std::atoi( argv[ i ] );
   if( args.epochs < 1 ) return( false );
   }
  else if( ( a == "-h" ) || ( a == "--help" ) )
   return( false );
  else
   positional.push_back( a );
  }

 const std::size_t wanted = ( args.mode == "train" ) ? 4 : 5;

 if( positional.size() != wanted )
  return( false );

 args.split_file = positional[ 0 ];
 args.data_dir   = positional[ 1 ];
 args.block_cfg  = positional[ 2 ];
 args.cfg_a      = positional[ 3 ];

 if( args.mode == "compare" )
  args.cfg_b = positional[ 4 ];

 return( true );
 }

/*--------------------------------------------------------------------------*/
/*--------------------------------- MAIN -----------------------------------*/
/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 Args args;

 if( ! parse_args( argc , argv , args ) ) {
  usage( argv[ 0 ] );
  return( 1 );
  }

 const std::string block_cfg = join( args.config_dir , args.block_cfg );
 const std::string cfg_a     = join( args.config_dir , args.cfg_a );

 if( args.mode == "train" ) {

  if( args.out.empty() ) {
   std::cerr << "train needs -o <weights>: without it the training would"
             << " be thrown away at the end of the run" << std::endl;
   return( 1 );
   }

  if( ! args.weights.empty() )
   std::cerr << "note: -r is ignored in train mode" << std::endl;

  return( train( args , block_cfg , cfg_a ) );
  }

 return( compare( args , block_cfg , cfg_a ,
                  join( args.config_dir , args.cfg_b ) ) );

 }  // end( main )

/*--------------------------------------------------------------------------*/
/*--------------------------- End File test.cpp ----------------------------*/
/*--------------------------------------------------------------------------*/
