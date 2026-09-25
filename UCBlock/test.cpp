/*--------------------------------------------------------------------------*/
/*-------------------------- File test.cpp ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Main for testing LagrangianDualSolver with UCBlock.
 *
 * An UCBlock instance is loaded from netCDF file, all the Solver listed in the
 * given BlockSolverConfig are registered to it, the UCBlock is solved by each
 * of them and the results are cross-checked against each other (and against a
 * reference objective value, where one is known). Each Solver enters the
 * cross-check as its [ get_lb() , get_ub() ] interval, valid by the base
 * Solver contract, and is measured against the best bounds all of them
 * provide: correctness always, and the tolerance it declares when it says
 * it delivered it. That tolerance is the dblRelAcc of its ComputeConfig
 * unless -E overrides it. Nothing here is tied to a particular Solver, so
 * bringing a new one into the comparison is a matter of listing it in the
 * BlockSolverConfig.
 *
 * Although the tester does not even include BundleSolver, some
 * BundleSolver-specific steps are done if a macro is set.
 *
 * The tester has some parts for the future extension when the UCBlock is
 * repeatedly randomly modified and re-solved several times, but this is not
 * done yet.
 *
 * Called as UCBlock_test --pollutant, with no other argument, the tester
 * instead checks the pollutant budget constraints of UCBlock.
 *
 * Called as UCBlock_test --scale, with no other argument, it instead checks
 * the scale factor of a unit, i.e., the number of copies of it the UCBlock
 * holds. The Objective of a scaled unit is the scale factor times the cost
 * of one copy, while its Variable stay those of one copy, which the rows of
 * the UCBlock that use them multiply by the factor [see UnitBlock::scale()]:
 * a unit scaled after the model is built must therefore give the model of
 * one scaled before it, and the data of the unit must not move. What a
 * dualizing Solver writes into the Objective, being scaled, is divided back
 * into the cost of one copy, which is what the (physical) DP Solvers read,
 * and they in turn answer for all the copies. This is checked on a thermal
 * unit in each of the seven formulations, with and without the perspective
 * cuts, and on a nuclear one, both of them carrying a cost of every kind
 * the Objective can hold, the two reserves and the reactive power.
 *
 * The PyPSA instances of batch-pypsa check the pollutant budget
 * constraints against the objective value of PyPSA, which however writes a
 * limit with a single zone spanning all the nodes, and knows nothing of what
 * is peculiar to UCBlock: several zones per pollutant, a node belonging to no
 * zone, the scale of a unit, the Modification that change a budget, the
 * Solution and the netCDF form of all this. These are checked here, on
 * instances small enough that every optimum is known.
 *
 * All the instances share the same data: two time instants with a demand of
 * 80 and 60 at node 2 of three nodes on a path, whose lines are never binding;
 * three IntermittentUnitBlock U0, U1 and U2 at nodes 0, 1 and 2, with a
 * capacity of 100 and a cost of 10, 30 and 60; a SlackUnitBlock at node 2
 * with a cost of 1000; in some instances a BatteryUnitBlock at node 2, empty
 * at the start, with a capacity of 100 in either direction and a maximum
 * level of 100. They differ in the pollutants:
 *
 * - N: none, whose optimum is 1400;
 *
 * - A: CO2 with two zones, { 0 } with budget 100 and { 1 , 2 } with budget
 *   1000, rates 1 and 0.5 for U0 and U1; NOx with one zone { 0 , 1 }, node 2
 *   belonging to none, budget 1000, rates 0.2, 0.1 and 5 for U0, U1 and U2
 *   (the rate of U2 must not count). Optimum 2200, dual of CO2 in zone 0
 *   equal to 20;
 *
 * - A2: A with a NOx budget of 20, optimum 3000 with dual 200; with U1
 *   scaled by 0.25, 3150 with dual 250;
 *
 * - B: CO2 alone, with neither NumberPollutantZones nor PollutantZones (one
 *   zone of all the nodes), budget 50 and rates depending on time (1 and 0.5
 *   at time 0, 1.5 and 0.5 at time 1), optimum 5400 with dual 60;
 *
 * - M1: CO2 with rates 1 and 2 for U0 and U1, no upper bound and a lower
 *   bound of 180 (PollutantMinBudget), optimum 2200 with dual 20;
 *
 * - M2: as M1 with both bounds equal to 150, optimum 1600 (1400 once the
 *   lower bound is removed);
 *
 * - S0 and S: the battery and CO2 with rates 1 and 0.5 and budget 100, the
 *   level of the battery at the end of the horizon counting -2 in S
 *   (PollutantStorageRho), which lowers the optimum from 3000 to 1800; with
 *   the battery scaled by 0.1 the optimum of S is 2700.
 *
 * The optima have been computed on a linear program written independently
 * of UCBlock. On each instance it is checked that the value is the expected
 * one, that the coefficients of every row are the scale of the unit times the
 * factor of the data, that the dual has the expected absolute value (not on
 * S0 and S, whose battery makes the problem a MILP), that UCBlock::is_feasible()
 * holds at the optimum while a solution exceeding a budget violates the
 * rows, and that the instance written back by UCBlock::serialize() has the
 * same optimum. On A the duals also go through a UCBlockSolution and its
 * netCDF form; on A2 and M2 the setters of the budget and of its lower bound,
 * by range and by subset, change the rows of the attached Solver; on A2
 * scaling a unit with the Solver attached gives the optimum of the scaled
 * instance read from scratch, also when the unit is held by a LagBFunction
 * as a LagrangianDualSolver does, and with a LagrangianDualSolver attached
 * (whose ComputeConfig is LDCfg-tight.txt, the LDCfg.txt of the batches
 * with a tighter threshold) the Lagrangian dual gives that same optimum, whether the unit is
 * scaled before the Solver is attached or after; on S scaling the battery
 * gives the expected optimum. Finally, an instance with inconsistent data
 * must be refused by UCBlock::deserialize() in five ways: two zones and no
 * PollutantZones, a PollutantRho of the wrong size, a
 * TotalNumberPollutantZones that is not the sum of NumberPollutantZones, a
 * NumberStorages that is not the number of storages of the units, and a
 * PollutantStorageRho of the wrong size.
 *
 * The instances are written in a temporary directory, and solved by the first
 * :MILPSolver in the Solver factory.
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
#include <filesystem>
#include <random>

#include <unistd.h>

#include "common_utils.h"

#include "PolyhedralFunctionBlock.h"

#include "UCBlock.h"

#include "ThermalUnitBlock.h"

#include "HydroSystemUnitBlock.h"

#include "ECNetworkBlock.h"

#include "BatteryUnitBlock.h"

#include "BlockSolverConfig.h"

#include "CDASolver.h"

#include "LinearFunction.h"

#include "DQuadFunction.h"

#include "LagBFunction.h"

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
                           // (used by batch-pypsa to iterate over all wf)

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
//   -V / --viol       : how much the solution a relaxation reconstructs may
//                       violate the rows it has dualised

static double RelaxationViol = 1e-1;

static bool process_specific_arg( int opt )
{
 switch( opt ) {
  case( 'r' ): Str2Sthg( optarg , RefObjective );   return( true );
  case( 'f' ): Str2Sthg( optarg , wf );             return( true );
  case( 'V' ): Str2Sthg( optarg , RelaxationViol ); return( true );
  default:                                         return( false );
  }
 }

/*--------------------------------------------------------------------------*/
namespace pollutant {

/*--------------------------------------------------------------------------*/
/*--------------- CONSTANTS OF THE POLLUTANT BUDGET CHECKS -----------------*/
/*--------------------------------------------------------------------------*/

/// the :MILPSolver to try, the first in the Solver factory being used

static const std::vector< std::string > SolverNames =
 { "CPXMILPSolver" , "GRBMILPSolver" , "HiGHSMILPSolver" , "SCIPMILPSolver" };

/// the relative tolerance of every comparison

static constexpr double Eps = 1e-6;

/*--------------------------------------------------------------------------*/
/*----------------- TYPES OF THE POLLUTANT BUDGET CHECKS -------------------*/
/*--------------------------------------------------------------------------*/

/// the data of one pollutant [see the file comment]

struct Pollutant {
 Index nz;                                ///< number of zones
 std::vector< Index > zones;              ///< zone of each node
 std::vector< double > ub;                ///< PollutantBudget
 std::vector< double > lb;                ///< PollutantMinBudget, if any
 std::vector< std::vector< double > > rho;  ///< [ t or 0 ][ generator ]
 std::vector< double > sigma;             ///< [ t ] on the battery level
 };

/// an instance [see the file comment]

struct Instance {
 std::vector< Pollutant > pollutants;
 bool battery = false;
 bool zones = true;            ///< write NumberPollutantZones, PollutantZones
 // the defects of the instances that must be refused
 bool drop_zones = false;      ///< no PollutantZones although nz > 1
 bool bad_rho = false;         ///< PollutantRho over one generator less
 Index bad_tnpz = 0;           ///< added to TotalNumberPollutantZones
 bool bad_storages = false;    ///< NumberStorages one more than the units'
 bool bad_sigma = false;       ///< PollutantStorageRho over two storages
 };

/*--------------------------------------------------------------------------*/
/*--------------- GLOBALS OF THE POLLUTANT BUDGET CHECKS -------------------*/
/*--------------------------------------------------------------------------*/

static bool all_passed = true;  ///< false as soon as a check fails

static std::string solver_name;

static std::filesystem::path dir;

/*--------------------------------------------------------------------------*/
/*-------------- FUNCTIONS OF THE POLLUTANT BUDGET CHECKS ------------------*/
/*--------------------------------------------------------------------------*/

static void check( bool ok , const std::string & what )
{
 std::cout << "  " << what << ( ok ? " -> OK" : " -> Error" ) << std::endl;
 if( ! ok )
  all_passed = false;
 }

/*--------------------------------------------------------------------------*/

static bool near( double a , double b )
{
 return( std::abs( a - b ) <= Eps * std::max( 1.0 , std::abs( b ) ) );
 }

/*--------------------------------------------------------------------------*/
/// writes the instance in the file with the given name, returning its path

static std::string write( const Instance & in , const std::string & name )
{
 const Index T = 2;
 const Index N = 3;
 const Index G = in.battery ? 5 : 4;
 const Index P = in.pollutants.size();

 auto path = ( dir / ( name + ".nc4" ) ).string();
 netCDF::NcFile f( path , netCDF::NcFile::replace );
 f.putAtt( "SMS++_file_type" , netCDF::NcInt() , 1 );

 auto g = f.addGroup( "Block_0" );
 g.putAtt( "type" , "UCBlock" );
 auto dT = g.addDim( "TimeHorizon" , T );
 g.addDim( "NumberUnits" , G );
 auto dG = g.addDim( "NumberElectricalGenerators" , G );
 auto dN = g.addDim( "NumberNodes" , N );
 auto dL = g.addDim( "NumberLines" , 2 );

 const std::vector< double > demand = { 0 , 0 , 0 , 0 , 80 , 60 };
 g.addVar( "ActivePowerDemand" , netCDF::NcDouble() ,
           { dN , dT } ).putVar( demand.data() );
 std::vector< unsigned > gen_node = { 0 , 1 , 2 , 2 , 2 };
 g.addVar( "GeneratorNode" , netCDF::NcUint() , dG ).putVar( gen_node.data() );
 const std::vector< unsigned > start = { 0 , 1 } , end = { 1 , 2 };
 g.addVar( "StartLine" , netCDF::NcUint() , dL ).putVar( start.data() );
 g.addVar( "EndLine" , netCDF::NcUint() , dL ).putVar( end.data() );
 const std::vector< double > maxf = { 1000 , 1000 } , minf = { -1000 , -1000 };
 g.addVar( "MaxPowerFlow" , netCDF::NcDouble() , dL ).putVar( maxf.data() );
 g.addVar( "MinPowerFlow" , netCDF::NcDouble() , dL ).putVar( minf.data() );

 if( P ) {
  auto dP = g.addDim( "NumberPollutants" , P );
  Index tnpz = 0;
  std::vector< unsigned > npz , pz;
  std::vector< double > ub , lb;
  bool any_lb = false;
  for( const auto & p : in.pollutants ) {
   tnpz += p.nz;
   npz.push_back( p.nz );
   pz.insert( pz.end() , p.zones.begin() , p.zones.end() );
   ub.insert( ub.end() , p.ub.begin() , p.ub.end() );
   if( p.lb.empty() )
    lb.insert( lb.end() , p.nz , -INF );
   else {
    lb.insert( lb.end() , p.lb.begin() , p.lb.end() );
    any_lb = true;
    }
   }

  auto dZ = g.addDim( "TotalNumberPollutantZones" , tnpz + in.bad_tnpz );
  if( in.zones ) {
   g.addVar( "NumberPollutantZones" , netCDF::NcUint() ,
             dP ).putVar( npz.data() );
   if( ! in.drop_zones )
    g.addVar( "PollutantZones" , netCDF::NcUint() ,
              { dP , dN } ).putVar( pz.data() );
   }
  ub.resize( tnpz + in.bad_tnpz , ub.back() );
  lb.resize( tnpz + in.bad_tnpz , lb.back() );
  g.addVar( "PollutantBudget" , netCDF::NcDouble() , dZ ).putVar( ub.data() );
  if( any_lb )
   g.addVar( "PollutantMinBudget" , netCDF::NcDouble() ,
             dZ ).putVar( lb.data() );

  // the rates, over one time instant if they all are constant
  const Index RT = in.pollutants[ 0 ].rho.size();
  const Index RG = in.bad_rho ? G - 1 : G;
  std::vector< double > rho( RT * P * RG , 0 );
  for( Index t = 0 ; t < RT ; ++t )
   for( Index p = 0 ; p < P ; ++p )
    for( Index h = 0 ; h < std::min( RG , Index( 4 ) ) ; ++h )
     rho[ ( t * P + p ) * RG + h ] = in.pollutants[ p ].rho[ t ][ h ];
  auto dRT = g.addDim( "PollutantRhoTime" , RT );
  auto dRG = in.bad_rho ? g.addDim( "OneGeneratorLess" , RG ) : dG;
  g.addVar( "PollutantRho" , netCDF::NcDouble() ,
            { dRT , dP , dRG } ).putVar( rho.data() );

  // the factors of the storages, over the one battery: over two with a
  // NumberStorages that says so if bad_storages, over two with no
  // NumberStorages at all if bad_sigma
  if( ! in.pollutants[ 0 ].sigma.empty() ) {
   const Index S = ( in.bad_storages || in.bad_sigma ) ? 2 : 1;
   auto dS = g.addDim( in.bad_sigma ? "TwoStorages" : "NumberStorages" , S );
   std::vector< double > sigma( T * P * S , 0 );
   for( Index t = 0 ; t < T ; ++t )
    for( Index p = 0 ; p < P ; ++p )
     if( ! in.pollutants[ p ].sigma.empty() )
      sigma[ ( t * P + p ) * S ] = in.pollutants[ p ].sigma[ t ];
   g.addVar( "PollutantStorageRho" , netCDF::NcDouble() ,
             { dT , dP , dS } ).putVar( sigma.data() );
   }
  }

 auto unit = [ & ]( Index u , const char * type , double cost , double max ) {
  auto ug = g.addGroup( "UnitBlock_" + std::to_string( u ) );
  ug.putAtt( "type" , type );
  ug.addVar( "MaxPower" , netCDF::NcDouble() ).putVar( & max );
  ug.addVar( "ActivePowerCost" , netCDF::NcDouble() ).putVar( & cost );
  if( std::string( type ) == "IntermittentUnitBlock" ) {
   const double zero = 0;
   ug.addVar( "MinPower" , netCDF::NcDouble() ).putVar( & zero );
   }
  };
 unit( 0 , "IntermittentUnitBlock" , 10 , 100 );
 unit( 1 , "IntermittentUnitBlock" , 30 , 100 );
 unit( 2 , "IntermittentUnitBlock" , 60 , 100 );
 unit( 3 , "SlackUnitBlock" , 1000 , 1000 );

 if( in.battery ) {
  auto bg = g.addGroup( "UnitBlock_4" );
  bg.putAtt( "type" , "BatteryUnitBlock" );
  auto scalar = [ & bg ]( const char * var , double value ) {
   bg.addVar( var , netCDF::NcDouble() ).putVar( & value );
   };
  scalar( "MaxPower" , 100 );
  scalar( "MinPower" , -100 );
  scalar( "ExtractingBatteryRho" , 1 );
  scalar( "StoringBatteryRho" , 1 );
  scalar( "MinStorage" , 0 );
  scalar( "MaxStorage" , 100 );
  scalar( "InitialStorage" , 0 );
  scalar( "Cost" , 0 );
  }

 return( path );
 }

/*--------------------------------------------------------------------------*/

static UCBlock * load( const std::string & path )
{
 auto uc = dynamic_cast< UCBlock * >( Block::deserialize( path ) );
 if( ! uc )
  throw( std::logic_error( path + " is not a UCBlock" ) );
 return( uc );
 }

/*--------------------------------------------------------------------------*/
/// attaches the :MILPSolver to the UCBlock (or detaches it if clear)

static void attach( UCBlock * uc , bool clear = false )
{
 BlockSolverConfig bsc( 1 );
 bsc.add_ComputeConfig( std::string( solver_name ) , nullptr );
 if( clear )
  bsc.clear();
 bsc.apply( uc );
 if( ! clear )
  if( auto s = uc->get_registered_solvers().front() ) {
   const auto par = s->int_par_str2idx( "intLogVerb" );
   if( par < Inf< Solver::idx_type >() )
    s->set_par( par , 0 );
   }
 }

/*--------------------------------------------------------------------------*/
/// solves the UCBlock, writing the solution in it, and returns the value

static double solve( UCBlock * uc )
{
 auto solver = static_cast< CDASolver * >(
                                    uc->get_registered_solvers().front() );
 if( solver->compute( false ) != Solver::kOK )
  return( std::numeric_limits< double >::quiet_NaN() );
 solver->get_var_solution();
 if( solver->has_dual_solution() )
  solver->get_dual_solution();
 return( solver->get_var_value() );
 }

/*--------------------------------------------------------------------------*/
/// the index of the row of zone 0 of pollutant p

static Index first_zone( const UCBlock * uc , Index p )
{
 Index k = 0;
 for( Index q = 0 ; q < p ; ++q )
  k += uc->get_number_pollutant_zones()[ q ];
 return( k );
 }

/*--------------------------------------------------------------------------*/
/// true if every row has exactly the terms of the data
/** Each row must have, for each generator of a unit at a node of its zone
 * and each time with a nonzero rate, the active power with coefficient the
 * scale of the unit times the rate, and for each storage the level with
 * coefficient the scale times its factor; and nothing else. */

static bool rows_match_data( UCBlock * uc )
{
 const Index T = uc->get_time_horizon();
 for( Index p = 0 ; p < uc->get_number_pollutants() ; ++p )
  for( Index z = 0 ; z < uc->get_number_pollutant_zones()[ p ] ; ++z ) {
   const Index k = first_zone( uc , p ) + z;
   const auto & row = uc->get_const_pollutant_constraints()[ p ][ z ];
   if( ( row.get_rhs() != uc->get_pollutant_budget()[ k ] ) ||
       ( row.get_lhs() != uc->get_pollutant_min_budget()[ k ] ) )
    return( false );

   // the terms expected from the data
   std::vector< std::pair< const ColVariable * , double > > expected;
   Index eg = 0 , st = 0;
   for( Index u = 0 ; u < uc->get_number_units() ; ++u ) {
    auto ub = uc->get_unit_block( u );
    const Index first_eg = eg;
    for( Index h = 0 ; h < ub->get_number_generators() ; ++h , ++eg ) {
     const Index node = uc->get_generator_node()[ eg ];
     const Index zone = uc->get_pollutant_zone().empty() ? 0 :
                        uc->get_pollutant_zone()[ p ][ node ];
     if( zone != z )
      continue;
     auto ap = ub->get_active_power( h );
     for( Index t = 0 ; ap && ( t < T ) ; ++t )
      if( uc->get_pollutant_rho( t , p , eg ) != 0 )
       expected.emplace_back( & ap[ t ] ,
                              ub->get_scale() *
                              uc->get_pollutant_rho( t , p , eg ) );
     }
    if( ! uc->get_pollutant_storage_rho().empty() ) {
     const Index node = ub->get_number_generators() ?
                        uc->get_generator_node()[ first_eg ] : 0;
     const Index zone = uc->get_pollutant_zone().empty() ? 0 :
                        uc->get_pollutant_zone()[ p ][ node ];
     for( Index s = 0 ; s < ub->get_number_storages() ; ++s ) {
      auto level = ub->get_storage_level( s );
      for( Index t = 0 ; level && ( zone == z ) && ( t < T ) ; ++t )
       if( uc->get_pollutant_storage_rho( t , p , st + s ) != 0 )
        expected.emplace_back( & level[ t ] , ub->get_scale() *
                               uc->get_pollutant_storage_rho( t , p ,
                                                              st + s ) );
      }
     st += ub->get_number_storages();
     }
    }

   auto lf = static_cast< LinearFunction * >( row.get_function() );
   if( lf->get_v_var().size() != expected.size() )
    return( false );
   for( const auto & [ var , coeff ] : expected ) {
    const auto i = lf->is_active( var );
    if( ( i >= lf->get_num_active_var() ) ||
        ( ! near( lf->get_coefficient( i ) , coeff ) ) )
     return( false );
    }
   }
 return( true );
 }

/*--------------------------------------------------------------------------*/
/// checks value, rows, dual, is_feasible() and the netCDF round trip

static UCBlock * run( const std::string & name , const Instance & in ,
                      double value , double dual = -1 , Index dual_p = 0 ,
                      Index dual_z = 0 )
{
 std::cout << name << std::endl;
 auto uc = load( write( in , name ) );
 attach( uc );
 const double v = solve( uc );
 check( near( v , value ) , "optimum " + std::to_string( v ) + " == " +
                            std::to_string( value ) );
 check( rows_match_data( uc ) , "rows are scale times the data" );
 if( dual >= 0 )
  check( near( std::abs( uc->get_const_pollutant_constraints()[ dual_p ]
                         [ dual_z ].get_dual() ) , dual ) ,
         "dual of pollutant " + std::to_string( dual_p ) + " zone " +
         std::to_string( dual_z ) + " == " + std::to_string( dual ) );

 SimpleConfiguration< double > tol( 1e-6 );
 check( uc->is_feasible( false , & tol ) , "is_feasible() at the optimum" );

 // the instance written by UCBlock has the same optimum
 auto copy = ( dir / ( name + "-copy.nc4" ) ).string();
 static_cast< Block * >( uc )->serialize( copy , eBlockFile );
 auto rt = load( copy );
 attach( rt );
 check( near( solve( rt ) , value ) , "serialize() keeps the optimum" );
 attach( rt , true );
 delete rt;

 return( uc );
 }

/*--------------------------------------------------------------------------*/

static void expect_refused( const std::string & name , const Instance & in ,
                            const std::string & what )
{
 bool refused = false;
 try {
  delete load( write( in , name ) );
  }
 catch( std::exception & e ) {
  refused = true;
  }
 check( refused , what + " is refused" );
 }

/*--------------------------------------------------------------------------*/

static void release( UCBlock * uc )
{
 attach( uc , true );
 delete uc;
 }

/*--------------------------------------------------------------------------*/
/// the checks of the pollutant budget constraints [see the file comment]

static int test( void )
{
 for( const auto & name : SolverNames )
  if( Solver::has_Solver( name ) ) {
   solver_name = name;
   break;
   }

 if( solver_name.empty() ) {
  std::cout << "no :MILPSolver in this build, nothing to check" << std::endl;
  return( 0 );
  }
 std::cout << "solving with " << solver_name << std::endl;

 dir = std::filesystem::temp_directory_path() /
       ( "UCBlock_pollutant_test_" + std::to_string( getpid() ) );
 std::filesystem::create_directories( dir );

 const std::vector< std::vector< double > > co2_rate = { { 1 , 0.5 , 0 , 0 } };

 // N- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 {
  auto uc = run( "N" , Instance() , 1400 );
  check( uc->get_const_pollutant_constraints().empty() , "no rows" );
  release( uc );
  }

 // A- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 Instance A;
 A.pollutants = {
  { 2 , { 0 , 1 , 1 } , { 100 , 1000 } , {} , co2_rate , {} } ,
  { 1 , { 0 , 0 , 1 } , { 1000 } , {} , { { 0.2 , 0.1 , 5 , 0 } } , {} } };
 {
  auto uc = run( "A" , A , 2200 , 20 , 0 , 0 );

  // the duals through a UCBlockSolution and its netCDF form
  SimpleConfiguration< int > what( 128 );
  auto sol = uc->get_Solution( & what , false );
  auto sol_path = ( dir / "A-solution.nc4" ).string();
  {
   netCDF::NcFile f( sol_path , netCDF::NcFile::replace );
   auto g = f.addGroup( "Solution_0" );
   sol->serialize( g );
   }
  delete sol;
  for( auto & zones : uc->get_pollutant_constraints() )
   for( auto & row : zones )
    row.set_dual( 0 );
  {
   netCDF::NcFile f( sol_path , netCDF::NcFile::read );
   UCBlockSolution read;
   read.deserialize( f.getGroup( "Solution_0" ) );
   read.write( uc );
   }
  check( near( std::abs( uc->get_const_pollutant_constraints()[ 0 ][ 0 ]
                         .get_dual() ) , 20 ) ,
         "the dual goes through the Solution" );

  // a solution beyond the budget of zone 0 of CO2 violates the rows
  auto u0 = uc->get_unit_block( 0 )->get_active_power( 0 );
  u0[ 0 ].set_value( u0[ 0 ].get_value() + 50 );
  check( ! RowConstraint::is_feasible( uc->get_pollutant_constraints() ,
                                       1e-6 ) ,
         "exceeding a budget violates the rows" );
  release( uc );
  }

 // A2 - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 Instance A2 = A;
 A2.pollutants[ 1 ].ub = { 20 };
 {
  auto uc = run( "A2" , A2 , 3000 , 200 , 1 , 0 );

  std::vector< double > budget = { 1000 };
  uc->set_pollutant_budget( budget.begin() , Block::Range( 2 , 3 ) ,
                            eModBlck , eModBlck );
  check( near( solve( uc ) , 2200 ) , "set_pollutant_budget( range )" );
  budget = { 20 };
  uc->set_pollutant_budget( budget.begin() , Block::Subset( { 2 } ) , true ,
                            eModBlck , eModBlck );
  check( near( solve( uc ) , 3000 ) , "set_pollutant_budget( subset )" );

  uc->get_unit_block( 1 )->scale( 0.25 , eModBlck , eModBlck );
  check( rows_match_data( uc ) , "rows follow the scale of a unit" );
  check( near( solve( uc ) , 3150 ) , "scaled unit with the Solver attached" );
  release( uc );

  auto fresh = load( ( dir / "A2.nc4" ).string() );
  fresh->get_unit_block( 1 )->scale( 0.25 , eNoMod , eNoMod );
  attach( fresh );
  check( near( solve( fresh ) , 3150 ) , "scaled unit read from scratch" );
  check( near( std::abs( fresh->get_const_pollutant_constraints()[ 1 ][ 0 ]
                         .get_dual() ) , 250 ) , "dual of the scaled unit" );
  release( fresh );
  }

 /* A2 with the unit scaled while a LagBFunction holds it, as it does while a
  * LagrangianDualSolver is attached: the LagBFunction is then the father of
  * the unit, and the unit is its only sub-Block, while the scale must still
  * rewrite the rows of unit 1 and not those of unit 0. */
 {
  auto uc = load( ( dir / "A2.nc4" ).string() );
  attach( uc );
  auto unit = uc->get_unit_block( 1 );
  auto lbf = new LagBFunction( unit );
  lbf->set_f_Block( uc );
  unit->scale( 0.25 , eModBlck , eModBlck );
  check( rows_match_data( uc ) , "rows follow the scale of a unit under a "
                                 "LagBFunction" );
  lbf->set_inner_block( nullptr , false );
  lbf->set_f_Block( nullptr );
  unit->set_f_Block( uc );
  delete lbf;
  check( near( solve( uc ) , 3150 ) , "scaled under a LagBFunction" );
  release( uc );
  }

 /* A2 with a LagrangianDualSolver attached, which by default gives each
  * LagBFunction only the dual pairs of the relaxed constraints its sub-Block
  * appears in [see intSparseLagPairs]: scaling a unit rewrites coefficients
  * of relaxed rows, and each change has to reach the Lagrangian term of the
  * right LagBFunction. The instance being continuous, the Lagrangian dual is
  * its optimum, 3150 with the unit scaled, whether it is scaled before the
  * Solver is attached or after. The ComputeConfig of the LagrangianDualSolver
  * LDCfg-tight.txt, which is the LDCfg.txt of the batches, next to which the
  * test is run [see CMakeLists.txt], with a tighter threshold on the
  * residual: the one of the batches stops the Bundle some 6% away from the
  * optimum, at a value that does not change with the scale, and the check
  * would not see it. */
 {
  // the value of the Lagrangian dual of A2 with unit 1 scaled by 0.25,
  // before the LagrangianDualSolver is attached or after
  const auto lagrangian = [ & ]( bool after ) {
   auto cc = dynamic_cast< ComputeConfig * >(
			       Configuration::deserialize( "LDCfg-tight.txt" ) );
   if( ! cc )
    return( std::numeric_limits< double >::quiet_NaN() );
   auto uc = load( ( dir / "A2.nc4" ).string() );
   if( ! after )
    uc->get_unit_block( 1 )->scale( 0.25 , eNoMod , eNoMod );
   BlockSolverConfig bsc( 1 );
   bsc.add_ComputeConfig( "LagrangianDualSolver" , cc );
   bsc.apply( uc );
   if( after )
    uc->get_unit_block( 1 )->scale( 0.25 , eModBlck , eModBlck );
   auto solver = static_cast< CDASolver * >(
                                    uc->get_registered_solvers().front() );
   const auto status = solver->compute( false );
   const auto v = ( ( status == Solver::kOK ) ||
                    ( status == Solver::kLowPrecision ) ) ?
                  solver->get_var_value() :
                  std::numeric_limits< double >::quiet_NaN();
   bsc.clear();
   bsc.apply( uc );
   delete uc;
   return( v );
   };

  const auto before = lagrangian( false );
  const auto after = lagrangian( true );
  check( ( std::abs( before - 3150 ) <= 1e-5 * 3150 ) &&
         ( std::abs( after - 3150 ) <= 1e-5 * 3150 ) ,
         "scaled unit with a LagrangianDualSolver attached: " +
         std::to_string( after ) + " and, scaled before, " +
         std::to_string( before ) + " == 3150" );
  }

 // B- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 {
  Instance B;
  B.zones = false;
  B.pollutants = { { 1 , { 0 , 0 , 0 } , { 50 } , {} ,
                     { { 1 , 0.5 , 0 , 0 } , { 1.5 , 0.5 , 0 , 0 } } , {} } };
  auto uc = run( "B" , B , 5400 , 60 , 0 , 0 );
  check( ( uc->get_number_pollutant_zones().size() == 1 ) &&
         ( uc->get_number_pollutant_zones()[ 0 ] == 1 ) &&
         uc->get_pollutant_zone().empty() , "one zone of all the nodes" );
  release( uc );
  }

 // M1 and M2- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 const std::vector< std::vector< double > > dirty = { { 1 , 2 , 0 , 0 } };
 {
  Instance M1;
  M1.pollutants = { { 1 , { 0 , 0 , 0 } , { INF } , { 180 } , dirty , {} } };
  release( run( "M1" , M1 , 2200 , 20 , 0 , 0 ) );
  }
 {
  Instance M2;
  M2.pollutants = { { 1 , { 0 , 0 , 0 } , { 150 } , { 150 } , dirty , {} } };
  auto uc = run( "M2" , M2 , 1600 , 20 , 0 , 0 );

  std::vector< double > floor = { -INF };
  uc->set_pollutant_min_budget( floor.begin() , Block::Range( 0 , 1 ) ,
                                eModBlck , eModBlck );
  check( near( solve( uc ) , 1400 ) , "set_pollutant_min_budget( range )" );
  floor = { 150 };
  uc->set_pollutant_min_budget( floor.begin() , Block::Subset( { 0 } ) ,
                                true , eModBlck , eModBlck );
  check( near( solve( uc ) , 1600 ) , "set_pollutant_min_budget( subset )" );
  release( uc );
  }

 // S0 and S - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 Instance S0;
 S0.battery = true;
 S0.pollutants = { { 1 , { 0 , 0 , 0 } , { 100 } , {} , co2_rate , {} } };
 release( run( "S0" , S0 , 3000 ) );

 Instance S = S0;
 S.pollutants[ 0 ].sigma = { 0 , -2 };
 {
  auto uc = run( "S" , S , 1800 );
  check( uc->get_number_storages() == 1 , "one storage, the battery" );
  uc->get_unit_block( 4 )->scale( 0.1 , eModBlck , eModBlck );
  check( rows_match_data( uc ) , "rows follow the scale of the battery" );
  check( near( solve( uc ) , 2700 ) , "scaled battery" );
  release( uc );
  }

 // refused instances- - - - - - - - - - - - - - - - - - - - - - - - - - - -
 std::cout << "refused instances" << std::endl;
 {
  Instance E = A;
  E.drop_zones = true;
  expect_refused( "E1" , E , "two zones and no PollutantZones" );
  }
 {
  Instance E = A;
  E.bad_rho = true;
  expect_refused( "E2" , E , "a PollutantRho of the wrong size" );
  }
 {
  Instance E = A;
  E.bad_tnpz = 1;
  expect_refused( "E3" , E , "a wrong TotalNumberPollutantZones" );
  }
 {
  Instance E = S;
  E.bad_storages = true;
  expect_refused( "E4" , E , "a wrong NumberStorages" );
  }
 {
  Instance E = S;
  E.bad_sigma = true;
  expect_refused( "E5" , E , "a PollutantStorageRho of the wrong size" );
  }

 std::filesystem::remove_all( dir );

 if( all_passed )
  std::cout << "All tests passed!!" << std::endl;
 else
  std::cout << "Shit happened!!" << std::endl;

 return( all_passed ? 0 : 1 );
 }

}  // end( namespace pollutant )

/*--------------------------------------------------------------------------*/
namespace scaling {

/*--------------------------------------------------------------------------*/
/*------------------- CONSTANTS OF THE SCALING CHECKS ----------------------*/
/*--------------------------------------------------------------------------*/

/// the :MILPSolver to try, the first in the Solver factory being used

static const std::vector< std::string > SolverNames =
 { "CPXMILPSolver" , "GRBMILPSolver" , "HiGHSMILPSolver" , "SCIPMILPSolver" };

/// the relative tolerance of every comparison

static constexpr double Eps = 1e-6;

/// the scale factor the unit under investment is given

static constexpr double Kappa = 3;

/// the time horizon of the instances

static constexpr Index T = 8;

/*--------------------------------------------------------------------------*/
/*------------------- GLOBALS OF THE SCALING CHECKS ------------------------*/
/*--------------------------------------------------------------------------*/

static bool all_passed = true;  ///< false as soon as a check fails

static std::string solver_name;

static std::filesystem::path dir;

/*--------------------------------------------------------------------------*/
/*------------------ FUNCTIONS OF THE SCALING CHECKS -----------------------*/
/*--------------------------------------------------------------------------*/

static void check( bool ok , const std::string & what )
{
 std::cout << "  " << what << ( ok ? " -> OK" : " -> Error" ) << std::endl;
 if( ! ok )
  all_passed = false;
 }

/*--------------------------------------------------------------------------*/

static bool near( double a , double b )
{
 return( std::abs( a - b ) <= Eps * std::max( 1.0 , std::abs( b ) ) );
 }

/*--------------------------------------------------------------------------*/
/// writes the instance in the file with the given name, returning its path
/** The instance has one node, a unit that is scaled and a SlackUnitBlock
 * that makes it feasible whatever the first one does. The scaled unit is a
 * ThermalUnitBlock with a cost of every kind the Objective can carry, i.e.,
 * start-up, shut-down, linear, quadratic and fixed, the two reserves and the
 * reactive power, or the NuclearUnitBlock that adds to them the costs of the
 * downward modulation steps and of the deep decreases. */

static std::string write( const std::string & name , bool nuclear ,
                          bool schedule = false )
{
 auto path = ( dir / ( name + ".nc4" ) ).string();
 netCDF::NcFile f( path , netCDF::NcFile::replace );
 f.putAtt( "SMS++_file_type" , netCDF::NcInt() , 1 );

 auto g = f.addGroup( "Block_0" );
 g.putAtt( "type" , "UCBlock" );
 auto dT = g.addDim( "TimeHorizon" , T );
 g.addDim( "NumberUnits" , 2 );
 auto dG = g.addDim( "NumberElectricalGenerators" , 2 );
 auto dN = g.addDim( "NumberNodes" , 1 );
 auto dP = g.addDim( "NumberPrimaryZones" , 1 );
 auto dS = g.addDim( "NumberSecondaryZones" , 1 );

 const std::vector< unsigned > gen_node = { 0 , 0 };
 g.addVar( "GeneratorNode" , netCDF::NcUint() , dG ).putVar( gen_node.data() );

 const std::vector< double > demand =
  { 260 , 300 , 280 , 320 , 300 , 260 , 240 , 280 };
 g.addVar( "ActivePowerDemand" , netCDF::NcDouble() ,
           { dN , dT } ).putVar( demand.data() );

 // the two reserves are asked for, so that their rows exist and carry the
 // scale factor of the unit; the reactive demand does the same for the
 // reactive node injection rows
 const std::vector< double > reserve( T , 1 );
 g.addVar( "PrimaryDemand" , netCDF::NcDouble() ,
           { dP , dT } ).putVar( reserve.data() );
 g.addVar( "SecondaryDemand" , netCDF::NcDouble() ,
           { dS , dT } ).putVar( reserve.data() );
 const std::vector< double > reactive( T , 5 );
 g.addVar( "ReactivePowerDemand" , netCDF::NcDouble() ,
           { dN , dT } ).putVar( reactive.data() );

 // the unit that is scaled - - - - - - - - - - - - - - - - - - - - - - - - -

 auto u = g.addGroup( "UnitBlock_0" );
 u.putAtt( "type" , nuclear ? "NuclearUnitBlock" : "ThermalUnitBlock" );

 auto put = [ & u ]( const std::string & nm , double v ) {
  u.addVar( nm , netCDF::NcDouble() ).putVar( & v );
  };
 auto put_u = [ & u ]( const std::string & nm , unsigned v ) {
  u.addVar( nm , netCDF::NcUint() ).putVar( & v );
  };
 auto put_i = [ & u ]( const std::string & nm , int v ) {
  u.addVar( nm , netCDF::NcInt() ).putVar( & v );
  };

 put( "MinPower" , 100 );
 put( "MaxPower" , 300 );
 put( "LinearTerm" , 12 );
 put( "QuadTerm" , 0.002 );
 put( "ConstTerm" , 40 );
 put( "StartUpCost" , 500 );
 put( "DeltaRampUp" , 60 );
 put( "DeltaRampDown" , 60 );
 put( "StartUpLimit" , 200 );
 put( "ShutDownLimit" , 200 );
 put_u( "MinUpTime" , 2 );
 put_u( "MinDownTime" , 2 );
 put( "InitialPower" , 150 );
 put_i( "InitUpDownTime" , 4 );
 // what the unit consumes while it is off, which the node injection rows
 // carry weighed with its scale factor
 put( "FixedConsumption" , 30 );
 put( "PrimaryRho" , 0.05 );
 put( "SecondaryRho" , 0.1 );
 put( "MaxReactivePowerOn" , 120 );
 put( "MinReactivePowerOn" , 0 );
 if( schedule ) {
  // the profile the unit is asked to follow, the deviation from which is a
  // further term of the Objective
  const std::vector< double > ref =
   { 150 , 200 , 180 , 220 , 200 , 160 , 140 , 180 };
  u.addVar( "ReferenceSchedule" , netCDF::NcDouble() ,
            g.getDim( "TimeHorizon" ) ).putVar( ref.data() );
  }

 if( ! nuclear )
  // the cost a thermal unit pays at the instant it shuts down, which the
  // nuclear one does not have
  put( "ShutDownCost" , 90 );
 else {
  // the operating rules, on the scale of a horizon of 8 instants
  auto dB = u.addDim( "NumberPowerBands" , 2 );
  put_u( "ModulationTime" , 2 );
  put_u( "InitModulation" , 2 );
  put( "ModulationDeltaRampUp" , 0 );
  put( "ModulationDeltaRampDown" , 0 );
  put_u( "MaxModulationLength" , 2 );
  put_u( "StabilityAfterStartUp" , 1 );
  const std::vector< double > bands = { 180 , 250 };
  u.addVar( "PowerBands" , netCDF::NcDouble() , dB ).putVar( bands.data() );
  put_u( "DayLength" , T );
  put_u( "ModulationsPerDay" , 2 );
  put_u( "StartUpsPerDay" , 1 );
  put( "DeepDecreaseThreshold" , 150 );
  put( "DeepDecreaseGradient" , 20 );
  put_u( "DeepDecreasesPerDay" , 1 );
  put( "DeepDecreaseCost" , 20 );
  put( "DownModulationCost" , 2 );
  }

 // the unit that makes the instance feasible - - - - - - - - - - - - - - - -

 auto s = g.addGroup( "UnitBlock_1" );
 s.putAtt( "type" , "SlackUnitBlock" );
 const std::vector< double > big( T , 1000 );
 s.addVar( "MaxPower" , netCDF::NcDouble() , dT ).putVar( big.data() );
 s.addVar( "MaxPrimaryPower" , netCDF::NcDouble() , dT ).putVar( big.data() );
 s.addVar( "MaxSecondaryPower" , netCDF::NcDouble() ,
           dT ).putVar( big.data() );
 const std::vector< double > cost( T , 1000 );
 s.addVar( "ActivePowerCost" , netCDF::NcDouble() , dT ).putVar( cost.data() );
 s.addVar( "PrimaryCost" , netCDF::NcDouble() , dT ).putVar( cost.data() );
 s.addVar( "SecondaryCost" , netCDF::NcDouble() , dT ).putVar( cost.data() );

 return( path );
 }

/*--------------------------------------------------------------------------*/

static UCBlock * load( const std::string & path )
{
 auto uc = dynamic_cast< UCBlock * >( Block::deserialize( path ) );
 if( ! uc )
  throw( std::logic_error( path + " is not a UCBlock" ) );
 return( uc );
 }

/*--------------------------------------------------------------------------*/
/// gives the unit the formulation wf and builds the abstract representation
/** A negative \p wf leaves the formulation of the unit alone. */

static void generate( UCBlock * uc , int wf )
{
 if( wf >= 0 ) {
  BlockConfig bc;
  bc.f_static_variables_Configuration = new SimpleConfiguration< int >( wf );
  bc.apply( uc->get_unit_block( 0 ) );
  }
 uc->generate_abstract_variables();
 uc->generate_abstract_constraints();
 uc->generate_objective();
 }

/*--------------------------------------------------------------------------*/
/// every number the abstract representation of the UCBlock is made of
/** The coefficients of the Objective of each unit and the two sides and the
 * coefficients of each row the UCBlock owns, in the order in which they are
 * generated: two UCBlock that hold the same model give the same vector. */

static std::vector< double > snapshot( UCBlock * uc )
{
 std::vector< double > v;

 for( Index u = 0 ; u < uc->get_number_units() ; ++u ) {
  auto obj = dynamic_cast< FRealObjective * >(
                                   uc->get_unit_block( u )->get_objective() );
  if( ! obj )
   continue;
  auto fnc = obj->get_function();
  if( auto qf = dynamic_cast< DQuadFunction * >( fnc ) )
   for( Index i = 0 ; i < qf->get_num_active_var() ; ++i ) {
    v.push_back( qf->get_linear_coefficient( i ) );
    v.push_back( qf->get_quadratic_coefficient( i ) );
    }
  else
   if( auto lf = dynamic_cast< LinearFunction * >( fnc ) )
    for( Index i = 0 ; i < lf->get_num_active_var() ; ++i )
     v.push_back( lf->get_coefficient( i ) );
  }

 auto rows = [ & v ]( const auto & group ) {
  for( auto & row : group ) {
   v.push_back( row.get_lhs() );
   v.push_back( row.get_rhs() );
   if( auto lf = dynamic_cast< LinearFunction * >( row.get_function() ) )
    for( Index i = 0 ; i < lf->get_num_active_var() ; ++i )
     v.push_back( lf->get_coefficient( i ) );
   }
  };

 auto rows_2D = [ & rows ]( auto && group ) {
  for( auto at_t : group )
   rows( at_t );
  };

 rows_2D( uc->get_node_injection_constraints() );
 rows_2D( uc->get_reactive_node_injection_constraints() );
 rows_2D( uc->get_primary_demand_constraints() );
 rows_2D( uc->get_secondary_demand_constraints() );
 rows_2D( uc->get_inertia_demand_constraints() );

 return( v );
 }

/*--------------------------------------------------------------------------*/
/// attaches the given Solver to the Block, or detaches all of them

static void attach( Block * b , const std::vector< std::string > & names )
{
 BlockSolverConfig bsc( names.size() );
 for( const auto & nm : names )
  bsc.add_ComputeConfig( std::string( nm ) , nullptr );
 if( names.empty() )
  bsc.clear();
 bsc.apply( b );
 for( auto slv : b->get_registered_solvers() ) {
  auto par = slv->int_par_str2idx( "intLogVerb" );
  if( par < Inf< Solver::idx_type >() )
   slv->set_par( par , 0 );
  }
 }

/*--------------------------------------------------------------------------*/
/// solves with the i-th Solver of the Block, returning the value it gives

static double solve( Block * b , Index i = 0 )
{
 auto slv = static_cast< CDASolver * >(
                     *std::next( b->get_registered_solvers().begin() , i ) );
 if( slv->compute( false ) != Solver::kOK )
  return( std::numeric_limits< double >::quiet_NaN() );
 return( slv->get_var_value() );
 }

/*--------------------------------------------------------------------------*/
/// the model of a unit scaled after it is built is that of one scaled before
/** The UCBlock is built and the unit is then scaled, which has to rewrite
 * every coefficient that carries the scale factor, both in the Objective of
 * the unit and in the rows of the UCBlock that use its Variable; the same
 * UCBlock with the unit scaled before anything is built is the model it must
 * give, and its optimum is the same. Scaling must leave the data of the unit,
 * which are those of one copy, where they are. */

static void check_model( const std::string & inst , int wf ,
                         const std::string & what )
{
 auto A = load( inst );
 generate( A , wf );
 // the Solver is attached before the unit is scaled, as it is when an
 // InvestmentBlock scales it: what the scaling writes into the Objective
 // then comes back to the unit, which must not take it for a change of the
 // cost of one copy
 if( ! solver_name.empty() )
  attach( A , { solver_name } );
 auto tu = static_cast< ThermalUnitBlock * >( A->get_unit_block( 0 ) );
 const auto cost = tu->get_linear_term();
 const auto start_up = tu->get_start_up_cost();
 A->get_unit_block( 0 )->scale( Kappa , eModBlck , eModBlck );
 check( tu->get_linear_term() == cost ,
        what + ": the cost of one copy is left alone" );
 check( tu->get_start_up_cost() == start_up ,
        what + ": the start-up cost of one copy is left alone" );

 auto B = load( inst );
 if( wf >= 0 ) {
  BlockConfig bc;
  bc.f_static_variables_Configuration = new SimpleConfiguration< int >( wf );
  bc.apply( B->get_unit_block( 0 ) );
  }
 B->get_unit_block( 0 )->scale( Kappa , eNoMod , eNoMod );
 generate( B , -1 );

 check( snapshot( A ) == snapshot( B ) ,
        what + ": scaled after == scaled before" );

 if( ! solver_name.empty() ) {
  attach( B , { solver_name } );
  const auto va = solve( A );
  const auto vb = solve( B );
  check( near( va , vb ) , what + ": same optimum" );
  attach( A , {} );
  attach( B , {} );
  }

 delete A;
 delete B;
 }

/*--------------------------------------------------------------------------*/
/// the DP Solver of a scaled unit answers for all its copies
/** The Objective of a scaled unit is the scale factor times the cost of one
 * copy, and the DP Solver, which reads the data of one copy, has to answer
 * for all of them. A dualizing Solver writes its multipliers into the
 * Objective, i.e., scaled: what the unit stores is that change divided back
 * into the cost of one copy, which is what the DP reads. */

static void check_dp( const std::string & inst , const std::string & dp ,
                      const std::string & what )
{
 if( solver_name.empty() )
  return;

 auto uc = load( inst );
 generate( uc , -1 );
 auto tu = static_cast< ThermalUnitBlock * >( uc->get_unit_block( 0 ) );
 attach( tu , { dp , solver_name } );

 check( near( solve( tu , 0 ) , solve( tu , 1 ) ) ,
        what + ": the DP and the MILP agree on the unit" );

 tu->scale( Kappa );
 const auto cost = tu->get_linear_term();

 // a price on the power, as a dualizing Solver puts it, i.e., scaled
 const double price = *std::max_element( cost.begin() , cost.end() ) + 1;
 auto qf = static_cast< DQuadFunction * >(
            static_cast< FRealObjective * >( tu->get_objective()
                                             )->get_function() );
 const Index first = qf->is_active( & tu->get_active_power( 0 )[ 0 ] );
 DQuadFunction::Vec_FunctionValue nc( T );
 for( Index t = 0 ; t < T ; ++t )
  nc[ t ] = qf->get_linear_coefficient( first + t ) - Kappa * price;
 tu->anyone_there( true );
 qf->modify_linear_coefficients( std::move( nc ) ,
                                 Block::Range( first , first + T ) ,
                                 eModBlck );

 bool per_copy = true;
 for( Index t = 0 ; t < T ; ++t )
  per_copy &= near( tu->get_linear_term()[ t ] , cost[ t ] - price );
 check( per_copy , what + ": the price is stored as the cost of one copy" );

 check( near( solve( tu , 0 ) , solve( tu , 1 ) ) ,
        what + ": the DP and the MILP agree on the scaled unit" );

 attach( tu , {} );
 delete uc;
 }

/*--------------------------------------------------------------------------*/
/// the checks of the scale factor of a unit [see the file comment]

static int test( void )
{
 for( const auto & name : SolverNames )
  if( auto solver = Solver::new_Solver( name ) ) {
   delete solver;
   solver_name = name;
   break;
   }

 if( solver_name.empty() )
  std::cout << "no :MILPSolver in this build, only the model is checked"
            << std::endl;
 else
  std::cout << "solving with " << solver_name << std::endl;

 dir = std::filesystem::temp_directory_path() /
       ( "UCBlock_scale_test_" + std::to_string( getpid() ) );
 std::filesystem::create_directories( dir );

 const auto thermal = write( "thermal" , false );
 const auto nuclear = write( "nuclear" , true );

 // the seven formulations of the thermal unit, with and without the
 // perspective cuts, each of which lays the Objective out its own way
 for( int form = 0 ; form <= 6 ; ++form )
  for( int pc = 0 ; pc <= 1 ; ++pc ) {
   const int wf = form + 8 * pc;
   check_model( thermal , wf ,
                "thermal, formulation " + std::to_string( form ) +
                ( pc ? " with P/C" : "" ) );
   }

 check_model( nuclear , -1 , "nuclear" );

 // a thermal unit asked to follow a reference schedule: the deviation from
 // it is weighed with the scale factor as every other term, the schedule
 // being that of one unit [see ThermalUnitBlock::generate_objective()]. The
 // DP Solvers know nothing of it, hence they are not asked about it
 const auto with_schedule = write( "schedule" , false , true );
 for( int form = 0 ; form <= 6 ; ++form )
  check_model( with_schedule , form ,
               "thermal with a schedule, formulation " +
               std::to_string( form ) );

 // the DP Solvers have no term for the deviation from a reference schedule,
 // hence they refuse a unit that has one instead of answering for a unit
 // that pays nothing to depart from its schedule
 if( ! solver_name.empty() ) {
  auto uc = load( with_schedule );
  generate( uc , -1 );
  auto tu = uc->get_unit_block( 0 );
  for( const auto & dp : { "ThermalUnitDPSolver" , "ThermalUnitExtDPSolver" } ) {
   bool refused = false;
   try {
    attach( tu , { dp } );
    solve( tu , 0 );
    }
   catch( const std::exception & e ) { refused = true; }
   check( refused , std::string( dp ) +
          " refuses a unit with a reference schedule" );
   attach( tu , {} );
   }
  delete uc;
  }

 // the rows of the UCBlock must not depend on the way they were reached:
 // writing back the demand the Block already has, which makes the setter
 // recompute the right-hand sides, has to leave the model where it is. The
 // setter skips the values that do not change, hence the demand is moved and
 // put back
 {
  auto uc = load( thermal );
  generate( uc , -1 );
  const auto before = snapshot( uc );
  const auto & d = uc->get_active_power_demand();
  const Index N = d.shape()[ 0 ];
  std::vector< double > same;
  for( Index n = 0 ; n < N ; ++n )
   for( Index t = 0 ; t < T ; ++t )
    same.push_back( d[ n ][ t ] );
  auto moved = same;
  for( auto & v : moved )
   ++v;
  uc->set_active_power_demand( moved.cbegin() , Block::Range( 0 , N * T ) ,
                               eModBlck , eModBlck );
  uc->set_active_power_demand( same.cbegin() , Block::Range( 0 , N * T ) ,
                               eModBlck , eModBlck );
  check( snapshot( uc ) == before ,
         "the demand written back leaves the rows where they are" );
  delete uc;
  }

 check_dp( thermal , "ThermalUnitDPSolver" , "thermal, standard DP" );
 check_dp( thermal , "ThermalUnitExtDPSolver" , "thermal, extended DP" );
 check_dp( nuclear , "NuclearUnitExtDPSolver" , "nuclear, extended DP" );

 std::filesystem::remove_all( dir );

 if( all_passed )
  std::cout << "All tests passed!!" << std::endl;
 else
  std::cout << "Shit happened!!" << std::endl;

 return( all_passed ? 0 : 1 );
 }

}  // end( namespace scaling )

/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 // override the default terminate handler to print the exception message
 std::set_terminate( smspp_terminate );

 // the checks of the pollutant budget constraints need no instance: they
 // write their own [see the file comment], and only read the ComputeConfig
 // of the LagrangianDualSolver from the directory they are run in
 if( ( argc == 2 ) && ( std::string( argv[ 1 ] ) == "--pollutant" ) )
  return( pollutant::test() );

 // the same goes for the checks of the scale factor of a unit
 if( ( argc == 2 ) && ( std::string( argv[ 1 ] ) == "--scale" ) )
  return( scaling::test() );

 // reading command line parameters - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // standard params (instance positional + -B + -S) are parsed by
 // common_utils; the test only appends its own knobs

 assert( SKIP_BEAT >= 0 );

 docopt_desc = "SMS++ LagrangianDualSolver-on-UCBlock test.\n";
 short_opts += "r:f:V:";
 const std::vector< option > my_opts = {
   { "ref"        , required_argument , nullptr , 'r' } ,
   { "wf"         , required_argument , nullptr , 'f' } ,
   { "viol"       , required_argument , nullptr , 'V' } };
 long_opts.insert( std::prev( long_opts.end() ) ,
                   my_opts.begin() , my_opts.end() );
 help += "  -r, --ref <value>               reference objective to compare "
         "against [none]\n"
         "  -f, --wf <0|1|2>                DCNetworkBlock formulation, "
         "overrides the -B one [file]\n"
         "  -V, --viol <value>              how much the solution a "
         "relaxation reconstructs may violate the rows it dualised [1e-1]\n";

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
  // by batch-pypsa to iterate over all formulations)
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
 // contract, and is held to the gap declared for it, which is the dblRelAcc
 // of its ComputeConfig unless -E overrides it: the batches do so for the
 // PrimalProximalHeur, whose dblRelAcc is what the inner Solver is asked
 // and not what its primal solution is worth
 bool AllPassed = SolveAll( TestBlock , RefObjective , 1e-5 );

 // each Solver that has a solution must have one that is worth what the
 // Solver says: the cross-check compares the values the Solver report with
 // each other, this compares each of them with the solution it comes with.
 // It is the only place where the solution the LagrangianDualSolver
 // RECONSTRUCTS is read back, that being written from the convex
 // combination the important linearization of each LagBFunction describes
 AllPassed &= check_var_solutions( TestBlock );

 // a Solver that solves a relaxation is not covered by the check above, the
 // value it reports not being that of what it writes: what its reconstructed
 // solution can be held to is the rows this Block couples, i.e. the ones the
 // relaxation has dualised, which it satisfies where the relaxation is exact
 AllPassed &= check_relaxation_solutions( TestBlock , RelaxationViol ,
                                          RefObjective );

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
