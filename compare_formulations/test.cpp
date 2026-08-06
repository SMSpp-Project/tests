/*--------------------------------------------------------------------------*/
/*----------------------------- File test.cpp ------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Main for testing different formulations of some problem.
 *
 * This main loads a Block twice. Then it Block-Config-ure each copy with a
 * different BlockConfig taken by two different files, assumed to produce
 * two different formulations of the same problem. Then it attaches two
 * Solver to the two copies of the Block (ideally identical, but this should
 * be irrelevant), solve both and compare the results.
 *
 * Also, a special "meta-configuration" mode is supported for both the
 * BlockConfig and BlockSolverConfig: if the specified Configuration is not
 * really a BlockConfig / BlockSolverConfig but rather a
 *
 *   SimpleConfiguration< std::map< std::string , Configuration * > >
 *
 * then this is interpreted as "the BlockConfig / BlockSolverConfig that
 * are to be apply()-ed to the Block / all its sub-Block that have that
 * specific classname()". That is, if the SimpleConfiguration< ... >
 * contains, say,
 *
 *    { { "UCBlock" , < pointer to BC1 > } ,
 *      { "DCNetworkBlock" , < pointer to BC2 > } }
 *
 * then the Block is scanned, and all its sub-Block (possibly, itself) that
 * are UCBlock are BlockConfig-ured with (a clone() to) BC1 while all the its
 * sub-Block (...) that are DCNetworkBlock are BlockConfig-ured with (...)
 * BC2; analogously for the BlockSolverConfig (except there is no need for
 * clone()-ing).
 *
 * \author Antonio Frangioni \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Antonio Frangioni
 */
/*--------------------------------------------------------------------------*/
/*----------------------------- INCLUDES -----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <chrono>
#include <cmath>
#include <limits>

#include "common_utils.h"

#include "RBlockConfig.h"

/*--------------------------------------------------------------------------*/
/*------------------------------- TYPES ------------------------------------*/
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/*----------------------------- CONSTANTS ----------------------------------*/
/*--------------------------------------------------------------------------*/

static constexpr double INF = SMSpp_di_unipi_it::Inf< double >();

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/

Block * Block1;
Block * Block2;

// RefObjective is defined in common_utils.cpp (extern in common_utils.h).
// If not-NaN, the objective value of the 1st Solver is compared against
// the reference value passed on the command line (argv[6]).

/*--------------------------------------------------------------------------*/
/*----------------------------- FUNCTIONS ----------------------------------*/
/*--------------------------------------------------------------------------*/

static bool SolveBoth( double * out_fo1 = nullptr ,
                       bool   * out_hs1 = nullptr )
{
 try {
  // solve with the 1st Solver- - - - - - - - - - - - - - - - - - - - - - - -
  auto Slvr1 = Block1->get_registered_solvers().front();

  auto start = std::chrono::system_clock::now();

  int rtrn1st = Slvr1->compute( false );
  bool hs1st = ( ( ( rtrn1st >= Solver::kOK ) && ( rtrn1st < Solver::kError )
                   && ( rtrn1st != Solver::kUnbounded )
                   && ( rtrn1st != Solver::kInfeasible ) )
                 || ( rtrn1st == Solver::kLowPrecision ) );
  double fo1st = hs1st ? Slvr1->get_var_value() : -INF;

  if( out_fo1 ) *out_fo1 = fo1st;
  if( out_hs1 ) *out_hs1 = hs1st;

  auto end = std::chrono::system_clock::now();
  std::chrono::duration< double > elapsed = end - start;
  double t1 = elapsed.count();

  // solve with the 2nd Solver- - - - - - - - - - - - - - - - - - - - - - - -
  auto Slvr2 = Block2->get_registered_solvers().front();

  start = std::chrono::system_clock::now();

  int rtrn2nd = Slvr2->compute( false );
  bool hs2nd = ( ( ( rtrn2nd >= Solver::kOK ) && ( rtrn2nd < Solver::kError )
                   && ( rtrn2nd != Solver::kUnbounded )
                   && ( rtrn2nd != Solver::kInfeasible ) )
                 || ( rtrn2nd == Solver::kLowPrecision ) );
  double fo2nd = hs2nd ? Slvr2->get_var_value() : -INF;

  end = std::chrono::system_clock::now();
  elapsed = end - start;

  double t2 = elapsed.count();

  // the two formulations are two separate Block with one Solver each, both
  // claiming an optimum, and the two optima must agree (2e-7); the verdict
  // and the per-instance line are those of common_utils
  std::vector< SolverReading > rd( 2 );
  std::vector< bool > hs{ hs1st , hs2nd };
  std::vector< int > status{ rtrn1st , rtrn2nd };
  if( hs1st ) rd[ 0 ] = SolverReading::exact( fo1st , eps_of( 0 , Slvr1 ) );
  if( hs2nd ) rd[ 1 ] = SolverReading::exact( fo2nd , eps_of( 1 , Slvr2 ) );

  auto tok = []( bool h , int rtrn , const SolverReading & r ) -> std::string {
   if( h )                               return( reading_token( r ) );
   if( rtrn == Solver::kInfeasible )     return( "Unfeas" );
   if( rtrn == Solver::kUnbounded )      return( "Unbounded" );
   return( "Error!" );
   };
  std::string verdict;
  double diff;
  bool ok = cross_check( rd , hs , status ,
                         std::numeric_limits< double >::quiet_NaN() ,
                         2e-7 , verdict , diff );
  print_instance_line(
   { t1 , t2 } ,
   { tok( hs1st , rtrn1st , rd[ 0 ] ) , tok( hs2nd , rtrn2nd , rd[ 1 ] ) } ,
   std::numeric_limits< double >::quiet_NaN() , verdict );
  return( ok );
  }
 catch( std::exception &e ) {
  std::cerr << e.what() << std::endl;
  exit( 1 );
  }
 catch(...) {
  std::cerr << "error: unknown exception thrown" << std::endl;
  exit( 1 );
  }
 }

/*--------------------------------------------------------------------------*/

// test-specific command-line config/knobs, set by process_specific_arg().
// This test compares two FORMULATIONS of the same instance, so it needs two
// BlockConfigs and two BlockSolverConfigs: formulation 1 uses the standard
// -B / -S (handled by common_utils), formulation 2 uses -X / -Y here.
std::string bconf2;          // BlockConfig of formulation 2 (-X)
std::string sconf2;          // BlockSolverConfig of formulation 2 (-Y)

/*--------------------------------------------------------------------------*/

static bool process_specific_arg( int opt )
{
 switch( opt ) {
  case( 'X' ): bconf2 = optarg;                   return( true );
  case( 'Y' ): sconf2 = optarg;                   return( true );
  case( 'r' ): Str2Sthg( optarg , RefObjective ); return( true );
  default:                                        return( false );
  }
 }

/*--------------------------------------------------------------------------*/

int main( int argc , char **argv )
{
 // override the default terminate handler to print the exception message
 std::set_terminate( smspp_terminate );

 // read command line parameters- - - - - - - - - - - - - - - - - - - - - - -
 // the instance positional and the formulation-1 configs (-B / -S) are
 // parsed by common_utils; the test appends the formulation-2 configs
 // (-X / -Y) and the optional reference objective (-r)

 docopt_desc = "SMS++ compare-two-formulations test.\n";
 short_opts += "X:Y:r:";
 const std::vector< option > my_opts = {
   { "blockcfg2"  , required_argument , nullptr , 'X' } ,
   { "solvercfg2" , required_argument , nullptr , 'Y' } ,
   { "ref"        , required_argument , nullptr , 'r' } };
 long_opts.insert( std::prev( long_opts.end() ) ,
                   my_opts.begin() , my_opts.end() );
 help += "  -X, --blockcfg2 <file>          BlockConfig of formulation 2 "
         "(mandatory)\n"
         "  -Y, --solvercfg2 <file>         BlockSolverConfig of formulation "
         "2 [same as -S]\n"
         "  -r, --ref <value>               reference objective to compare "
         "against [none]\n";

 process_args( argc , argv , process_specific_arg );

 // all the configs must be explicit: -B / -X (the two formulations) and -S
 // (the solver); -Y defaults to -S when only one solver config is given
 require_block_config();    // -B (formulation 1)
 require_solver_config();    // -S
 if( bconf2.empty() )
  throw( std::invalid_argument(
   "the BlockConfig of formulation 2 must be provided (forgot -X?)" ) );
 if( sconf2.empty() )
  sconf2 = sconf_file;

 // load both Block out of the same netCDF file- - - - - - - - - - - - - - - -

 Block1 = Block::deserialize( filename );
 if( ! Block1 ) {
  std::cerr << "error: cannot load Block from " << filename << std::endl;
  return( 1 );
  }

 Block2 = Block::deserialize( filename );
 // this reasonably should not fail ...

 // load two BlockConfig from file- - - - - - - - - - - - - - - - - - - - - -

 std::string fn1 = bconf_file;   // -B, formulation 1
 auto cfg1 = Configuration::deserialize( fn1 );
 if( ! cfg1 ) {
  std::cerr << "error: cannot load BlockConfig " << fn1 << std::endl;
  return( 1 );
  }

 b_config_Block( Block1 , cfg1 , fn1 );
 delete( cfg1 );

 std::string fn2 = bconf2;       // -X, formulation 2
 auto cfg2 = Configuration::deserialize( fn2 );
 if( ! cfg2 ) {
  std::cerr << "error: cannot load BlockConfig " << fn2 << std::endl;
  return( 1 );
  }

 b_config_Block( Block2 , cfg2 , fn2 );
 delete( cfg2 );

 // load two BlockSolverConfig from file- - - - - - - - - - - - - - - - - - -

 fn1 = sconf_file;   // -S, solver of formulation 1
 if( ! ( cfg1 = Configuration::deserialize( fn1 ) ) ) {
  std::cerr << "error: cannot load BlockSolverConfig " << fn1 << std::endl;
  return( 1 );
  }

 s_config_Block( Block1 , cfg1 , fn1 );
 if( Block1->get_registered_solvers().empty() ) {
  std::cerr << "Error: no Solver registered to Block1" << std::endl;
  exit( 1 );
  }

 fn2 = sconf2;       // -Y (defaults to -S), solver of formulation 2
 if( ! ( cfg2 = Configuration::deserialize( fn2 ) ) ) {
  std::cerr << "error: cannot load BlockSolverConfig " << fn2  << std::endl;
  return( 1 );
  }

 s_config_Block( Block2 , cfg2 , fn2 );
 if( Block2->get_registered_solvers().empty() ) {
  std::cerr << "Error: no Solver registered to Block2" << std::endl;
  exit( 1 );
  }

 // solve- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 double fo1 = -INF;
 bool   hs1 = false;
 auto ok = SolveBoth( &fo1 , &hs1 );

 // optional reference-objective check- - - - - - - - - - - - - - - - - - - -

 if( ok && ! std::isnan( RefObjective ) ) {
  if( ! hs1 ) {
   std::cout << "Cannot check Ref: Solver1 returned no solution"
             << std::endl;
   ok = false;
   }
  else {
   double maxv = std::max( double( 1 ) ,
                           std::max( std::abs( fo1 ) ,
                                     std::abs( RefObjective ) ) );
   double diff = std::abs( fo1 - RefObjective );
   double tol = 1e-5 * maxv;
   bool ref_ok = ( diff <= tol );

   std::cout << def << fo1
             << " ~ Ref = " << def << RefObjective
             << " (|diff| = " << def << diff
             << ( ref_ok ? ", OK" : ", KO" ) << ")" << std::endl;

   if( ! ref_ok ) ok = false;
   }
  }

 if( ok )
  std::cout << GREEN( Test passed!! ) << std::endl;
 else
  std::cout << RED( Shit happened!! ) << std::endl;

 // clean - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

 s_config_Block( Block1 , cfg1 );  // cfg1 has been clear()-ed before
 delete( Block1 );
 delete( cfg1 );

 s_config_Block( Block2 , cfg2 );  // cfg2 has been clear()-ed before
 delete( Block2 );
 delete( cfg2 );

 return( ok ? 0 : 1 );
 }

/*--------------------------------------------------------------------------*/
/*------------------------- End File test.cpp ------------------------------*/
/*--------------------------------------------------------------------------*/
