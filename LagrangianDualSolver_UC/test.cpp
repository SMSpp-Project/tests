/*--------------------------------------------------------------------------*/
/*-------------------------- File test.cpp ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Main for testing LagrangianDualSolver with UCBlock.
 *
 * An UCBlock instance is loaded from netCDF file, two different Solver are
 * registered to the UCBlock, the second of which is assumed to be a
 * LagrangianDualSolver, the UCBlock is solved by the Solver and the results
 * are compared.
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

bool ProxHeur = false;     // false = LagrangianDualSolver
                           // true  = PrimalProximalHeur

int wf = 0;                // DCNetworkBlock formulation selector
                           // 0 = PTDF (default), 1 = CYCLE, 2 = KIRCHHOFF

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

int main( int argc , char ** argv )
{
 // override the default terminate handler to print the exception message
 std::set_terminate( smspp_terminate );

 // reading command line parameters - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 assert( SKIP_BEAT >= 0 );

 /*!!
 long int seed = 0;
 Index wchg = 127;
 double dens = 4;  
 double p_change = 0.5;
 Index n_change = 10;
 Index n_repeat = 40;
 !!*/

 switch( argc ) {
  /*!!
  case( 8 ): Str2Sthg( argv[ 7 ] , p_change );
  case( 7 ): Str2Sthg( argv[ 6 ] , n_change );
  case( 6 ): Str2Sthg( argv[ 5 ] , n_repeat );
  case( 3 ): Str2Sthg( argv[ 2 ] , wchg );
  case( 2 ): Str2Sthg( argv[ 1 ] , seed );
             break;
	     !!*/
  case( 6 ): Str2Sthg( argv[ 5 ] , wf );
  case( 5 ): Str2Sthg( argv[ 4 ] , RefObjective );
  case( 4 ): Str2Sthg( argv[ 3 ] , ProxHeur );
  case( 3 ):
  case( 2 ): break;
  default: std::cerr << "Usage: " << argv[ 0 ]
		     << " UC-file [BSC-file ws ref wf]"
		     << std::endl <<
	   "       BSC-file: BlockSolverConfig description [BSPar.txt]"
		     << std::endl <<
           "       ws: 0 = LagrangianDualSolver, 1 = PrimalProximalHeur [0]"
		     << std::endl <<
           "       ref: reference objective value to compare against [none]"
		     << std::endl <<
           "       wf: DCNetworkBlock formulation [0]"
		     << std::endl <<
           "             0 = PTDF, 1 = CYCLE, 2 = KIRCHHOFF"
	        << std::endl;
    /*!!
	   " UC file [BSC file seed wchg #rounds #chng %chng]"
 		<< std::endl <<
	   "       seed: random seed generator [0]"
 		<< std::endl <<
           "       wchg: what to change, coded bit-wise [127]"
		<< std::endl <<
           "             0 = ..., 1 = ...s "
		<< std::endl <<
           "             2 = ..., 3 = ..."
	        << std::endl <<
           "       #rounds: how many iterations [40]"
	        << std::endl <<
           "       #chng: number changes [10]"
	        << std::endl <<
           "       %chng: probability of changing [0.5]"
		!!*/
	   return( 1 );
  }

 // read the Block- - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 TestBlock = Block::deserialize( argv[ 1 ] );
 if( ! TestBlock ) {
  std::cout << std::endl << "Block::deserialize() failed!" << std::endl;
  exit( 1 );
  }

 // attach the Solver(s) to the Block - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // do this by reading an appropriate BlockSolverConfig from file and
 // apply() it to the TestBlock; note that the BlockSolverConfig is
 // clear()-ed and kept to do the cleanup at the end

 std::string bsc_fn = argc >= 3 ? argv[ 2 ] : "BSPar.txt";
 BlockSolverConfig * bsc;
 {
  auto c = Configuration::deserialize( bsc_fn );
  bsc = dynamic_cast< BlockSolverConfig * >( c );
  if( ! bsc ) {
   std::cerr << "Error: configuration file not a BlockSolverConfig"
             << std::endl;
   delete( c );
   exit( 1 );
   }

  // load the BlockConfig for ThermalUnitBlock
  auto tc = Configuration::deserialize( "TUBCfg.txt" );
  auto tbc = dynamic_cast< BlockConfig * >( tc );
  if( ! tbc ) {
   std::cerr << "Error: TUBCfg.txt does not contain a BlockSolverConfig"
             << std::endl;
   delete( c );
   delete( tc );
   exit( 1 );
   }

  // load the BlockSolverConfig for ThermalUnitBlock
  auto ct = Configuration::deserialize( "TUBSCfg-CLP.txt" );
  auto tbsc = dynamic_cast< BlockSolverConfig * >( ct );
  if( ! tbsc ) {
   std::cerr << "Error: TUBSCfg-CLP.txt does not contain a BlockSolverConfig"
             << std::endl;
   delete( c );
   delete( tc );
   delete( ct );
   exit( 1 );
   }

  // load the BlockSolverConfig for HydroSystemUnitBlock; note that
  // this can be "empty", and indeed even not there, in which case
  // the HydroSystemUnitBlock will be treated as "easy"
  auto ch = Configuration::deserialize( "HSUBSCfg.txt" );
  auto hbsc = dynamic_cast< BlockSolverConfig * >( ch );
  if( ( ! hbsc ) || ( ! hbsc->num_ComputeConfig() ) ) {
   delete( ch );
   hbsc = nullptr;
   }

  // per-classname BlockConfig that drives the DCNetworkBlock formulation
  // selector (read by DCNetworkBlock::generate_abstract_variables): 0=PTDF,
  // 1=CYCLE, 2=KIRCHHOFF. A no-op on instances without a DCNetworkBlock.
  // Dispatched below via the meta_bc map (same pattern used for tbc).
  auto dcbc = new BlockConfig;
  dcbc->f_static_variables_Configuration = new SimpleConfiguration< int >( wf );

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

    break;  // note that we assume this happens *at most* once
    }

   auto sb = TestBlock->get_nested_Blocks();

   // dispatch the per-classname configurations loaded above to all the
   // matching sub-Blocks via a (meta) BlockConfig / BlockSolverConfig that
   // b_config_Block / s_config_Block resolve by classname:
   //   tbc  -> every ThermalUnitBlock           (BlockConfig)
   //   tbsc -> every ThermalUnitBlock           (BlockSolverConfig)
   //   hbsc -> every HydroSystemUnitBlock       (BlockSolverConfig, optional)
   // obsc remains code-driven (catch-all on non-Thermal/non-HSUB) and is
   // applied below only in the DoEasy=false branch.
   {
    // NB: SimpleConfiguration<map<string,Configuration*>>::guts_of_destructor
    // delete()s every value in the map; we f_value.clear() before scope-exit
    // so tbc / tbsc / hbsc / dcbc are NOT double-deleted (they survive for
    // the explicit delete() at end of this configuration block).
    SimpleConfiguration< std::map< std::string , Configuration * > > meta_bc;
    meta_bc.f_value[ "ThermalUnitBlock" ] = tbc;
    meta_bc.f_value[ "DCNetworkBlock" ] = dcbc;
    b_config_Block( TestBlock , &meta_bc , "" );
    meta_bc.f_value.clear();

    SimpleConfiguration< std::map< std::string , Configuration * > > meta_bsc;
    meta_bsc.f_value[ "ThermalUnitBlock" ] = tbsc;
    if( hbsc )
     meta_bsc.f_value[ "HydroSystemUnitBlock" ] = hbsc;
    s_config_Block( TestBlock , &meta_bsc , "" , false );  // deferred clear
    meta_bsc.f_value.clear();
    }

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
      if( hbsc )
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

  // cleanup
  delete( tbc );
  delete( tbsc );
  delete( hbsc );
  delete( dcbc );

  // bsc may be a plain BlockSolverConfig or a meta-config; s_config_Block
  // dispatches on the runtime type, applies, and clear()s for cleanup
  s_config_Block( TestBlock , bsc , bsc_fn );

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

 bool AllPassed;

 // Solver1 is always treated as a lower-bound producer; Solver2 too unless
 // ProxHeur=true, in which case it's a PrimalProximalHeur (upper bound) and
 // the agreement criterion becomes one-sided "fo1st - fo2nd <= 1e-5".
 const auto getter1 = ObjGetter::LowerBound;
 const auto getter2 = ProxHeur ? ObjGetter::UpperBound : ObjGetter::LowerBound;

 if( TestBlock->get_registered_solvers().size() > 1 ) {
  double fo1st = -INF;
  bool hs1st = false;
  double time1 = 0.0;
  long it1 = 0;
  AllPassed = SolveBoth( TestBlock , getter1 , getter2 , ProxHeur , 1e-5 ,
                         &fo1st , &hs1st , &time1 , &it1 );

  // optional additional check against reference objective
  if( ! std::isnan( RefObjective ) ) {
   if( hs1st )
    AllPassed &= CheckRefValue( fo1st , RefObjective , 1e-5 , time1 , it1 );
   else
    AllPassed = false;
   }
  }
 else {
  // single-solver case: keep old behavior
  if( std::isnan( RefObjective ) )
   AllPassed = SolveBoth( TestBlock , getter1 );  // just solve the only solver
  else
   AllPassed = SolveAndCheckRef( TestBlock , RefObjective , getter1 , 1e-5 );
 }

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
