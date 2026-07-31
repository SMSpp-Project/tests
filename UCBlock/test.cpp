/*--------------------------------------------------------------------------*/
/*-------------------------- File test.cpp ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Main for testing LagrangianDualSolver with UCBlock.
 *
 * An UCBlock instance is loaded from netCDF file, all the Solver listed in the
 * given BlockSolverConfig are registered to it, the UCBlock is solved by each
 * of them and the results are cross-checked against each other (and against a
 * reference objective value, where one is known). How each Solver enters the
 * cross-check follows from the per-Solver optimality tolerances declared to
 * eps_getter(): a Solver with a finite eps is exact up to it and must agree
 * with the other exact ones on the optimum, while one with an infinite eps
 * only has to contain the optimum in its [ get_lb() , get_ub() ] interval,
 * which is valid by the base Solver contract. Nothing here is tied to a
 * particular Solver, so bringing a new one into the comparison is a matter
 * of listing it in the BlockSolverConfig.
 *
 * Although the tester does not even include BundleSolver, some
 * BundleSolver-specific steps are done if a macro is set.
 *
 * The tester has some parts for the future extension when the UCBlock is
 * repeatedly randomly modified and re-solved several times, but this is not
 * done yet.
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
/*-------------------------------- MACROS ----------------------------------*/
/*--------------------------------------------------------------------------*/

#define LOG_LEVEL 2
// -1 = no log at all, not even pass/fail
// 0 = only pass/fail
// 1 = result of each test
// 2 = + solver log
// 3 = + save LP file
// 4 = + print data

#if( LOG_LEVEL >= 1 )
 #define LOG1( x ) std::cout << x
 #define CLOG1( y , x ) if( y ) std::cout << x

 #if( LOG_LEVEL >= 2 )
  #define LOG_ON_COUT 1
  // if nonzero, the 2nd Solver (LagrangianDualSolver) log is sent on std::cout
  // rather than on a file
 #endif
#else
 #define LOG1( x )
 #define CLOG1( y , x )
#endif

/*--------------------------------------------------------------------------*/
// if nonzero, the 2nd Solver attached to the UCBlock is assumed to be a
// LagrangianDualSolver (or PrimalProximalHeur) using [Parallel]BundleSolver
// as the "inner" solver; parameters from the BlockSolverConfig are read and
// set so that, if "easy components" are used, all UnitBlock that are
// ThermalUnitBlock or HydroSystemUnitBlock are attached an appropriate
// Solver, whereas all other inner Block are treated as "easy components"

#define USE_BundleSolver 1

/*--------------------------------------------------------------------------*/
// if nonzero, the 1st Solver attached to the UCBlock is detached
// and re-attached to it at all iterations

#define DETACH_1ST 0

// if nonzero, the 2nd Solver attached to the UCBlock is detached and
// re-attached to it at all iterations

#define DETACH_2ND 0

/*--------------------------------------------------------------------------*/
// if nonzero, the two Block are not solved at every round of changes, but
// only every SKIP_BEAT + 1 rounds. this allows changes to accumulate, and
// therefore puts more pressure on the Modification handling of the Solver
// (in case this tries to do "smart" things rather than dumbly processing
// each one in turn)
//
// note that the number of rounds of changes is them multiplied by
// SKIP_BEAT + 1, so that the input parameter still dictates the number of
// Block solutions

#define SKIP_BEAT 0

/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <cmath>
#include <cstdlib>
#include <random>

#include "common_utils.h"

#include "PolyhedralFunctionBlock.h"

#include "UCBlock.h"

#include "ThermalUnitBlock.h"

#include "HydroSystemUnitBlock.h"

#include "ECNetworkBlock.h"

#include "BatteryUnitBlock.h"

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*-------------------------------- TYPES -----------------------------------*/
/*--------------------------------------------------------------------------*/

using Subset = Block::Subset;

using FunctionValue = Function::FunctionValue;

/*--------------------------------------------------------------------------*/
/*------------------------------- CONSTANTS --------------------------------*/
/*--------------------------------------------------------------------------*/

const double scale = 10;
const char * const logF = "log.txt";

const FunctionValue INF = SMSpp_di_unipi_it::Inf< FunctionValue >();

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/

Block * TestBlock;         // the [UC]Block that is solved

std::mt19937 rg;           // base random generator
std::uniform_real_distribution<> dis( 0.0 , 1.0 );

// if not-NaN, the objective value of the (only) Solver attached to the Block
// is compared against a reference value passed on the command line

// RefObjective is defined in common_utils.cpp (extern in common_utils.h)

int wf = -1;               // DCNetworkBlock formulation selector
                           // 0 = PTDF, 1 = CYCLE, 2 = KIRCHHOFF
                           // < 0 (default) = use the value set in the meta-
                           // BlockConfig InnerBCfg.txt (-> DCNBCfg.txt); when
                           // passed on the command line it overrides that file
                           // (used by batch-resilient to iterate over all wf)

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

static void Configure_HSUB( HydroSystemUnitBlock * hsub ) {
 // ensure that the PolyhedralFunctionBlock in the HydroSystemUnitBlock is
 // Configured to use the "linearised" representation of the Objective

 for( auto sb : hsub->get_nested_Blocks() )
  if( auto pfb = dynamic_cast< PolyhedralFunctionBlock * >( sb ) ) {
   auto bc = new BlockConfig;
   bc->f_static_variables_Configuration = new SimpleConfiguration< int >( 1 );
   pfb->set_BlockConfig( bc );
   }
 }

/*--------------------------------------------------------------------------*/

static double rndfctr( void )
{
 // return a random number between 0.5 and 2, with 50% probability of being
 // < 1
 double fctr = dis( rg ) - 0.5;
 return( fctr < 0 ? - fctr : fctr * 4 );
 }

/*--------------------------------------------------------------------------*/

static Subset GenerateRand( Index m , Index k )
{
 // generate a sorted random k-vector of unique integers in 0 ... m - 1

 Subset rnd( m );
 std::iota( rnd.begin() , rnd.end() , 0 );
 std::shuffle( rnd.begin() , rnd.end() , rg );
 rnd.resize( k );
 sort( rnd.begin() , rnd.end() );

 return( std::move( rnd ) );
 }

/*--------------------------------------------------------------------------*/

// test-specific command-line knobs, set by process_specific_arg(); the
// standard parameters (instance positional, -B BlockConfig, -S
// BlockSolverConfig, -c/-p prefixes) are handled centrally by common_utils
//   -r / --ref        : reference objective value to compare against
//   -f / --wf         : DCNetworkBlock formulation, overrides the -B one

static bool process_specific_arg( int opt )
{
 switch( opt ) {
  case( 'r' ): Str2Sthg( optarg , RefObjective ); return( true );
  case( 'f' ): Str2Sthg( optarg , wf );           return( true );
  default:                                         return( false );
  }
 }

/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 // override the default terminate handler to print the exception message
 std::set_terminate( smspp_terminate );

 // reading command line parameters - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // standard params (instance positional + -B + -S) are parsed by
 // common_utils; the test only appends its own knobs

 assert( SKIP_BEAT >= 0 );

 docopt_desc = "SMS++ LagrangianDualSolver-on-UCBlock test.\n";
 short_opts += "r:f:";
 const std::vector< option > my_opts = {
   { "ref"        , required_argument , nullptr , 'r' } ,
   { "wf"         , required_argument , nullptr , 'f' } };
 long_opts.insert( std::prev( long_opts.end() ) ,
                   my_opts.begin() , my_opts.end() );
 help += "  -r, --ref <value>               reference objective to compare "
         "against [none]\n"
         "  -f, --wf <0|1|2>                DCNetworkBlock formulation, "
         "overrides the -B one [file]\n";

 process_args( argc , argv , process_specific_arg );

 // both the BlockConfig (-B, the inner formulation) and the
 // BlockSolverConfig (-S) must be provided explicitly: the test never falls
 // back to a hardcoded default Configuration
 require_block_config();
 require_solver_config();

 // read the Block- - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 TestBlock = Block::deserialize( filename );
 if( ! TestBlock ) {
  std::cout << std::endl << "Block::deserialize() failed!" << std::endl;
  exit( 1 );
  }

 // attach the Solver(s) to the Block - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // do this by reading an appropriate BlockSolverConfig from file and
 // apply() it to the TestBlock; note that the BlockSolverConfig is
 // clear()-ed and kept to do the cleanup at the end

 BlockSolverConfig * bsc;
 {
  auto c = Configuration::deserialize( sconf_file );
  bsc = dynamic_cast< BlockSolverConfig * >( c );
  if( ! bsc ) {
   std::cerr << "Error: configuration file not a BlockSolverConfig"
             << std::endl;
   delete( c );
   exit( 1 );
   }

  // load the inner (meta-)BlockConfig that drives the *formulation* of the
  // sub-Blocks: a SimpleConfiguration< map< classname, Configuration* >>
  // mapping a Block classname to the BlockConfig to apply to every sub-Block of
  // that class, dispatched by b_config_Block (see tests/compare_formulations):
  //   InnerBCfg.txt -> ThermalUnitBlock formulation (TUBCfg.txt) and
  //                    DCNetworkBlock formulation (DCNBCfg.txt)
  // How the sub-Blocks are *solved* inside the Lagrangian Dual is NOT set here:
  // it descends entirely from the main BSC stack via the LagrangianDualSolver
  // str_LagBF_BSCfg parameter (BSPar -> LDCfg -> InnerBSCfg.txt), so the inner
  // Solver of the LagBFunction is the single source of truth (e.g. BSPar-2S-EC
  // -> LDCfg-EC -> InnerBSCfg-DP.txt to solve the thermal units with the
  // efficient ThermalUnitExtDPSolver). A HydroSystemUnitBlock is a "hard" component
  // iff that str_LagBF_BSCfg meta configures it; computed below once cc is found.
  auto ibc = Configuration::deserialize( bconf_file );
  if( ! ibc ) {
   std::cerr << "Error: cannot load BlockConfig from " << bconf_file
             << std::endl;
   delete( c );
   exit( 1 );
   }
  bool hydro_hard = false;

  // optional command-line override of the DCNetworkBlock formulation: when wf
  // is passed (>= 0) it replaces the static-variables Configuration of the
  // DCNetworkBlock entry of the meta-BlockConfig, overriding DCNBCfg.txt (used
  // by batch-resilient to iterate over all formulations)
  if( wf >= 0 )
   if( auto m = dynamic_cast< SimpleConfiguration<
        std::map< std::string , Configuration * > > * >( ibc ) ) {
    auto it = m->f_value.find( "DCNetworkBlock" );
    if( it != m->f_value.end() )
     if( auto dcbc = dynamic_cast< BlockConfig * >( it->second ) ) {
      delete dcbc->f_static_variables_Configuration;
      dcbc->f_static_variables_Configuration =
       new SimpleConfiguration< int >( wf );
      }
    }

  #if USE_BundleSolver
   auto nbsc = bsc->num_ComputeConfig();
   if( ! nbsc ) {
    std::cerr << "Error: no ComputeConfig in the BlockSolverConfig"
              << std::endl;
    delete( c );
    exit( 1 );
    }

   // check if any of the Solver is a LagrangianDualSolver
   bool DoEasy = false;
   bool is_LDS = true;
   ComputeConfig * cc = nullptr;
   for( auto h = 0 ; h < nbsc ; ++h ) {
    if( bsc->get_SolverName( h ) != "LagrangianDualSolver" ) {  // if not
     is_LDS = false;
     continue;                                                  // do nothing
     }

    cc = bsc->get_SolverConfig( h );
    if( ! cc ) {
     std::cerr << "Error: empty ComputeConfig in the BlockSolverConfig"
               << std::endl;
     delete( c );
     exit( 1 );
     }

    // find the inner Solver
    auto sit = std::find_if( cc->str_pars.begin() , cc->str_pars.end() ,
			     []( auto & pair ) {
			      return( pair.first == "str_LDSlv_ISName" );
			      } );
    if( sit == cc->str_pars.end() )  // if it's not there
     continue;                       // do nothing

    // check if it is a [Parallel]BundleSolver
    if( ( sit->second.find( "BundleSolver" ) == std::string::npos ) &&
        ( sit->second.find( "ParallelBundleSolver" ) == std::string::npos ) )
     continue;  // if not, do nothing

    // check if the BundleSolver uses "easy" components
    // find if the ComputeConfig contains "intDoEasy"
    auto it = std::find_if( cc->int_pars.begin() , cc->int_pars.end() ,
			    []( auto & pair ) {
			     return( pair.first == "intDoEasy" );
			     } );
    if( it != cc->int_pars.end() )     // if so
     DoEasy = ( it->second & 1 ) > 0;  // read it
    else                               // otherwise
     DoEasy = true;                    // assume it is true (default)

    // the inner Solver of each LagBFunction descends from str_LagBF_BSCfg; a
    // HydroSystemUnitBlock is a "hard" component iff that (meta-)BSC configures
    // it. Peek at the file to decide (no-op when it is a plain BSC or absent)
    auto bit = std::find_if( cc->str_pars.begin() , cc->str_pars.end() ,
			     []( auto & pair ) {
			      return( pair.first == "str_LagBF_BSCfg" );
			      } );
    if( bit != cc->str_pars.end() )
     if( auto lbc = Configuration::deserialize( bit->second ) ) {
      if( auto m = dynamic_cast< SimpleConfiguration<
           std::map< std::string , Configuration * > > * >( lbc ) )
       hydro_hard = m->f_value.count( "HydroSystemUnitBlock" ) > 0;
      delete( lbc );
      }

    break;  // note that we assume this happens *at most* once
    }

   auto sb = TestBlock->get_nested_Blocks();

   // apply the inner (meta-)BlockConfig (formulation) by classname over the
   // sub-Blocks; b_config_Block clones each BlockConfig before applying, so ibc
   // keeps ownership. The inner Solvers are NOT attached here: they descend from
   // the LagrangianDualSolver str_LagBF_BSCfg when bsc is applied below. The
   // OUBSCfg catch-all stays code-driven (DoEasy=false branch).
   b_config_Block( TestBlock , ibc , "InnerBCfg.txt" );

   // Configure_HSUB the linearised PolyhedralFunctionBlock inside every
   // HydroSystemUnitBlock; runtime block-mutation, not config-driven
   for( auto sb_i : sb )
    if( auto hsub = dynamic_cast< HydroSystemUnitBlock * >( sb_i ) )
     Configure_HSUB( hsub );

   // if "easy" components are used
   if( DoEasy ) {
    // define the vector of components to be excluded from being "easy",
    // i.e., all ThermalUnitBlock and possibly the HydroSystemUnitBlock,
    // plus the BatteryUnitBlock whose commitment variables are binary
    std::vector< int > NoEasy;
    for( auto i = 0 ; i < sb.size() ; ++i ) {
     if( dynamic_cast< ThermalUnitBlock * >( sb[ i ] ) )
      NoEasy.push_back( i );
     else if( auto bub = dynamic_cast< BatteryUnitBlock * >( sb[ i ] ) ) {
      if( ! bub->get_intake_outtake_binary_variables().empty() )
       NoEasy.push_back( i );
      }
     else if( dynamic_cast< HydroSystemUnitBlock * >( sb[ i ] ) ) {
      if( hydro_hard )
       NoEasy.push_back( i );
      }
     }

    // if no "hard" components were given in Configuration file...
    auto it_cc = std::find_if( cc->vint_pars.begin() , cc->vint_pars.end() ,
                               []( const auto & pair ) {
                                return( pair.first == "vintNoEasy" );
                               } );
    if( ( cc->vint_pars.empty() ||              // no pairs present
          ( ( it_cc != cc->vint_pars.end() ) && // or vintNoEasy exists
            it_cc->second.empty() ) ) ) {       // but is empty
     // ... and no "hard" components were selected...
     if( NoEasy.empty() ) {
      // ... but there is at least one ECNetworkBlock
      if( std::any_of( sb.begin() , sb.end() , []( Block * b ) {
       return( dynamic_cast< ECNetworkBlock * >( b ) );
      } ) ) {
       // then indicate the first non-ECNetworkBlock as "hard" component,
       // otherwise the BundleSolver will fail because all Block are easy
       auto it = std::find_if_not( sb.begin() , sb.end() , []( Block * b ) {
        return( dynamic_cast< ECNetworkBlock * >( b ) );
       } );
       if( it != sb.end() )
        NoEasy.push_back( ( int ) std::distance( sb.begin() , it ) );
       else
        throw( std::logic_error(
         "There is no non-ECNetworkBlock candidate block to set as a `hard` "
         "component, so set intDoEasy == 0 in the Configuration file since "
         "BundleSolver cannot deal with the problem if all its components are "
         "`easy`." ) );
       }
      }
     } // ... else if "hard" components were given in the Configuration file...
    else
     for( auto i : it_cc->second )
      // ... but some of there is an ECNetworkBlock...
      if( dynamic_cast< ECNetworkBlock * >( sb[ i ] ) )
       // ... then raise error since we cannot treat is as "hard" component
       throw( std::logic_error(
        "ECNetworkBlock cannot treat as `hard` component, so remove it "
        "from `vintNoEasy` parameter." ) );
      else if( ! ( std::find( NoEasy.begin() ,
                              NoEasy.end() , i ) != NoEasy.end() ) )
       // ... otherwise add it to NoEasy if it is not already contained
       NoEasy.push_back( i );

    // now add the vintNoEasy parameter to the BundleSolver ComputeConfig
    // we are assuming it's not there already: if it is, the new copy is
    // seen after the old one and therefore overrides it
    std::sort( NoEasy.begin() , NoEasy.end() );
    cc->vint_pars.push_back( std::make_pair( "vintNoEasy" ,
                                             std::move( NoEasy ) ) );
    }  // end( if( DoEasy ) )
   else
    {
    if( is_LDS )
     // if there is at least one ECNetworkBlock...
     if( std::any_of( sb.begin() , sb.end() , []( Block * b ) {
      return( dynamic_cast< ECNetworkBlock * >( b ) );
     } ) )
      // ... then raise error since we cannot treat is as "hard" component
      throw( std::logic_error(
       "ECNetworkBlock(s) cannot treat as `hard` components, so set "
       "intDoEasy == 0 in the Configuration file and, optionally, specify "
       "which non-ECNetworkBlocks(s) to treat as `hard` components through "
       "`vintNoEasy` parameter." ) );
    // load the BlockSolverConfig for all the other :UnitBlock; note that
    // this can be "empty", and indeed even not there.
    // When the main BSC contains a LagrangianDualSolver (cc != nullptr,
    // independently of whether it is the first or a later Solver) we
    // *skip* applying this catch-all altogether: LagrangianDualSolver will
    // configure each sub-Block's inner Solver itself, via the
    // str_LagBF_BSCfg parameter (typically LPBSCfg.txt). Pre-attaching an
    // MILPSolver here would just stack a second, never-used Solver on top
    // of each sub-Block — on large instances this dominates the setup time.
    if( ! cc ) {
     auto co = Configuration::deserialize( "OUBSCfg.txt" );
     auto obsc = dynamic_cast< BlockSolverConfig * >( co );
     if( ( ! obsc ) || ( ! obsc->num_ComputeConfig() ) ) {
      delete( co );
      obsc = nullptr;
      }

     // apply obsc as catch-all to every sub-Block that is not Thermal or
     // HSUB (those have already been configured via the meta-config above)
     if( obsc )
      for( auto ub : sb )
       if( ! dynamic_cast< ThermalUnitBlock * >( ub ) &&
           ! dynamic_cast< HydroSystemUnitBlock * >( ub ) )
        obsc->apply( ub );

     delete( obsc );
     }
    }
  #endif

  // cleanup the inner (meta-)BlockConfig (its destructor deletes the contained
  // per-classname BlockConfig)
  delete( ibc );

  // bsc may be a plain BlockSolverConfig or a meta-config; s_config_Block
  // dispatches on the runtime type, applies, and clear()s for cleanup
  s_config_Block( TestBlock , bsc , sconf_file );

  if( TestBlock->get_registered_solvers().empty() ) {
   std::cout << std::endl << "no Solver registered to the Block!" << std::endl;
   exit( 1 );
   }
  }

 // open log-file- - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 #if( LOG_LEVEL >= 2 )
  #if( LOG_ON_COUT )
   ( ( TestBlock->get_registered_solvers() ).back() )->set_log( &std::cout );
  #else
   std::ofstream LOGFile( logF , std::ofstream::out );
   if( ! LOGFile.is_open() )
    std::cerr << "Warning: cannot open log file """ << logF << """"
              << std::endl;
   else {
    LOGFile.setf( std::ios::scientific, std::ios::floatfield );
    LOGFile << std::setprecision( 10 );
    ( ( TestBlock->get_registered_solvers() ).back() )->set_log( &LOGFile );
    }
  #endif
 #endif

 // first solver call - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 LOG1( "First call: " );

 // cross-check EVERY registered Solver against the others (and against the
 // reference objective value, where one is known). Each Solver enters the
 // check as its [ get_lb() , get_ub() ] interval, valid by the base Solver
 // contract; the per-Solver eps below declares, positionally with respect to
 // the Solver order in the BSPar*.txt files, how tight each one claims to
 // be: the :MILPSolver and the LagrangianDualSolver (which closes the gap on
 // these instances) are exact up to the test tolerance, while nothing beyond
 // plain correctness is claimed for the PrimalProximalHeur (its bounds only
 // have to contain the optimum). Further exact Solver appended to the
 // BlockSolverConfig are covered by the eps_getter() default, so bringing
 // one into the comparison is only a matter of listing it there
 const bool AllPassed = SolveAll( TestBlock ,
                                  eps_getter( { 1e-5 , 1e-5 , INF } ) ,
                                  RefObjective , 1e-5 );

 // main loop - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // now, for n_repeat times:
 // - up to n_change ... are ...
 // - up to n_change ... are ...
 // - up to n_change ... are ...
 // - up to n_change ... are ...
 //
 // then the TestBlock is re-solved with both Solver

 /*!!
 for( Index rep = 0 ; rep < n_repeat * ( SKIP_BEAT + 1 ) ; ) {
  LOG1( rep << ": ");

  // do stuff 1 - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 1 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = Index( dis( rg ) * n_change ) ) {
    LOG1( "... " << tochange << " ... - " );

    }

  // do stuff 2 - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 2 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = min( m - 1 , Index( dis( rg ) * n_change ) ) ) {
    LOG1( "... " << tochange << " ..." );

    
    if( dis( rg ) <= 0.5 ) {  // in 50% of the cases do a ranged change
     LOG1( "(r) - " );

     }
    else {  // in the other 50% of the cases, do a sparse change
     LOG1( "(s) - " );
     Subset nms( GenerateRand( m , tochange ) );

     }

    }

  // ...


  // if verbose, print out stuff- - - - - - - - - - - - - - - - - - - - - - -

  #if( LOG_LEVEL >= 3 )
   ( ( LPBlock->get_registered_solvers() ).front() )->set_par(
		                     MILPSolver::strOutputFile , "LPBlock-" +
		                     std::to_string( rep ) + ".lp" );
  #endif

  // finally, re-solve the problems- - - - - - - - - - - - - - - - - - - - -
  // ... every SKIP_BEAT + 1 rounds

  if( ! ( ++rep % ( SKIP_BEAT + 1 ) ) )
   AllPassed &= SolveBoth();
  #if( LOG_LEVEL >= 1 )
  else
   std::cout << std::endl;
  #endif

  }  // end( main loop )- - - - - - - - - - - - - - - - - - - - - - - - - - -
     // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     !!*/

 #if( LOG_LEVEL >= 0 )
  if( ! std::isnan( RefObjective ) ||
      ( TestBlock->get_registered_solvers().size() > 1 ) ) {
   // tests only make sense if more than one Solver is attached, unless
   // a reference objective value is provided
   if( AllPassed )
    std::cout << GREEN( All tests passed!! ) << std::endl;
   else
    std::cout << RED( Shit happened!! ) << std::endl;
   }
 #endif

 // destroy the Block - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 // apply() the clear()-ed BlockSolverConfig to cleanup Solver
 //!bsc->apply( TestBlock );

 // then delete the BlockSolverConfig
 delete( bsc );

 #if USE_BundleSolver
  // since some Solver have been attached "by hand" to some sub-Block,
  // unregister "by hand" any remaining Solver attached to them
  for( auto sb : TestBlock->get_nested_Blocks() )
   sb->unregister_Solvers();
 #endif

 // finally the AbstractBlock can be deleted
 delete( TestBlock );

 // terminate - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 return( AllPassed ? 0 : 1 );

 }  // end( main )

/*--------------------------------------------------------------------------*/
/*--------------------------- End File test.cpp ----------------------------*/
/*--------------------------------------------------------------------------*/
