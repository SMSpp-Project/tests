/*--------------------------------------------------------------------------*/
/*------------------------ File stochastic_test.cpp ------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * The main of the testers of the stochastic Block [see stochastic_test.h].
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
// 0 = only pass/fail
// 1 = + "First call: " before the first solution
// 2 = + the log of the last Solver of each copy on the standard output

/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <cmath>
#include <iostream>

#include "common_utils.h"

#include "stochastic_test.h"

/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/

namespace {

StochasticTester * tester = nullptr;  ///< the tester being run

bool ProxHeur = false;  ///< the second Solver read by its upper bound

const double RefTolerance = 1e-5;  ///< of the comparisons

/*--------------------------------------------------------------------------*/
// -w and -r, then the options of the tester

bool process_specific_arg( int opt )
{
 switch( opt ) {
  case( 'w' ): Str2Sthg( optarg , ProxHeur );    return( true );
  case( 'r' ): Str2Sthg( optarg , RefObjective ); return( true );
  default:
   return( tester->handle ? tester->handle( opt ) : false );
  }
 }

/*--------------------------------------------------------------------------*/
// the Solver of the one copy, as the testers of a stochastic Block have
// always compared them

bool solve_one( Block * block )
{
 // the second Solver, if any, read by its lower bound, or by its upper
 // bound if it is a PrimalProximalHeur
 const auto getter1 = ObjGetter::LowerBound;
 const auto getter2 = ProxHeur ? ObjGetter::UpperBound : ObjGetter::LowerBound;

 if( ! solver_relaxation.empty() )
  /* -R says which Solver solve a relaxation, e.g., a Lagrangian dual of a
   * stochastic Block whose scenarios are unit commitments, whose bound is
   * below the optimum by the duality gap: every Solver then enters the
   * cross-check as the interval of its bounds, as in the other testers,
   * the relaxations with the one bound that holds for this problem, and
   * there may be any number of them. */
  return( SolveAll( block , RefObjective , RefTolerance ) );

 if( block->get_registered_solvers().size() > 1 ) {
  double fo1st = - Inf< double >();
  bool hs1st = false;
  double time1 = 0.0;
  long it1 = 0;
  bool ok = SolveBoth( block , getter1 , getter2 , ProxHeur , RefTolerance ,
                       & fo1st , & hs1st , & time1 , & it1 );

  // optional additional check against reference objective
  if( ! std::isnan( RefObjective ) ) {
   if( hs1st )
    ok &= CheckRefValue( fo1st , RefObjective , RefTolerance , time1 , it1 );
   else
    ok = false;
   }
  return( ok );
  }

 // single-solver case
 if( std::isnan( RefObjective ) )
  return( SolveBoth( block , getter1 ) );

 return( SolveAndCheckRef( block , RefObjective , getter1 , RefTolerance ) );
 }

}  // end( anonymous namespace )

/*--------------------------------------------------------------------------*/
/*--------------------------------- MAIN -----------------------------------*/
/*--------------------------------------------------------------------------*/

int SMSpp_di_unipi_it::stochastic_test( int argc , char ** argv ,
                                        StochasticTester & t )
{
 std::set_terminate( smspp_terminate );
 tester = & t;

 // reading command line parameters - - - - - - - - - - - - - - - - - - - - -
 // the standard parameters (instance positional, -B BlockConfig or -F the
 // list of them, -S BlockSolverConfig, -c/-p prefixes, -R) are parsed by
 // common_utils; -w, -r and those of the tester are appended

 docopt_desc = "SMS++ " + t.classname + " test.\n";
 short_opts += t.short_opts + "w:r:";
 std::vector< option > my_opts = t.long_opts;
 my_opts.push_back( { "warm-start" , required_argument , nullptr , 'w' } );
 my_opts.push_back( { "ref"        , required_argument , nullptr , 'r' } );
 long_opts.insert( std::prev( long_opts.end() ) ,
                   my_opts.begin() , my_opts.end() );
 help += t.help +
         "  -w, --warm-start <0|1>          0 = LagrangianDualSolver, "
         "1 = PrimalProximalHeur [0]\n"
         "  -r, --ref <value>               reference objective to compare "
         "against [none]\n";

 process_args( argc , argv , process_specific_arg );

 // the BlockSolverConfig (-S) is mandatory, the BlockConfig (-B, the inner
 // formulation) or the list of them (-F) is optional
 auto forms = get_formulations( true );

 // read the Block once per formulation, and attach the Solver to it- - - - - -

 std::vector< Block * > blocks;
 std::vector< Configuration * > bscs;
 std::vector< std::vector< bool > > relax;
 auto give_up = [ & ]( void ) {
  for( std::size_t k = 0 ; k < blocks.size() ; ++k ) {
   delete bscs[ k ];
   delete blocks[ k ];
   }
  delete forms.owner;
  exit( 1 );
  };

 for( std::size_t k = 0 ; k < forms.configs.size() ; ++k ) {
  auto block = Block::deserialize( filename );
  if( ! block ) {
   std::cout << std::endl << "Block::deserialize() failed!" << std::endl;
   give_up();
   }

  if( ! t.is_one( block ) ) {
   std::cout << std::endl << "Error: deserialized Block is not a "
             << t.classname << std::endl;
   delete block;
   give_up();
   }

  // the inner (meta-)BlockConfig (formulation), applied by classname
  // recursively over the whole Block tree: the scenarios contain UCBlocks
  // whose ThermalUnitBlock (and DCNetworkBlock) sit arbitrarily deep, and
  // b_config_Block reaches them all. The inner Solvers are NOT attached
  // here: they descend from the BlockSolverConfig below
  if( forms.configs[ k ] )
   b_config_Block( block , forms.configs[ k ] , forms.file );

  // the Block the Solver are attached to, if not the one read
  if( t.transform ) {
   auto other = t.transform( block );
   if( ! other ) {
    delete block;
    give_up();
    }
   block = other;
   }

  // BSC may be a plain BlockSolverConfig or a meta-config; s_config_Block()
  // dispatches on the runtime type and clears the config(s) for cleanup
  auto bsc = solver_config_of( forms , k );
  blocks.push_back( block );
  bscs.push_back( bsc );
  relax.push_back( attach_formulation_Solvers( block , bsc , forms , k ) );

  if( block->get_registered_solvers().empty() ) {
   std::cout << std::endl << "no Solver registered to the Block!"
             << std::endl;
   give_up();
   }

  #if( LOG_LEVEL >= 2 )
   // the log of the last Solver on the standard output
   block->get_registered_solvers().back()->set_log( & std::cout );
  #endif
  }

 delete forms.owner;

 // solve - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 #if( LOG_LEVEL >= 1 )
  std::cout << "First call: ";
 #endif

 bool AllPassed;
 if( blocks.size() == 1 ) {
  const auto global = solver_relaxation;
  solver_relaxation = relax.front();
  AllPassed = solve_one( blocks.front() );
  solver_relaxation = global;
  }
 else {
  // each Solver read as with one copy: by its lower bound, the second by
  // its upper bound with -w 1, or by the interval of its bounds with -R
  SolverClassifier classify = []( Solver * s , std::size_t k ) {
   if( ProxHeur && ( k == 1 ) )
    return( SolverReading::upper_bound( s->get_ub() , eps_of( k , s ) ) );
   return( SolverReading::exact( s->get_lb() , eps_of( k , s ) ) );
   };
  bool any_relax = false;
  for( const auto & r : relax )
   for( auto b : r )
    any_relax |= b;
  if( any_relax || ( ! solver_relaxation.empty() ) )
   classify = read_bounds;
  AllPassed = SolveFormulations( blocks , forms.names , classify ,
                                 RefObjective , RefTolerance , {} , relax );
  }

 if( t.check )
  for( auto b : blocks )
   if( ! t.check( b ) )
    AllPassed = false;

 if( ( ! std::isnan( RefObjective ) ) || ( blocks.size() > 1 ) ||
     ( blocks.front()->get_registered_solvers().size() > 1 ) ) {
  if( AllPassed )
   std::cout << GREEN( All tests passed!! ) << std::endl;
  else
   std::cout << RED( Shit happened!! ) << std::endl;
  }

 // destroy the Block - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 for( std::size_t k = 0 ; k < blocks.size() ; ++k ) {
  blocks[ k ]->unregister_Solvers( true );
  delete bscs[ k ];
  delete blocks[ k ];
  }

 return( AllPassed ? 0 : 1 );
 }

/*--------------------------------------------------------------------------*/
/*---------------------- End File stochastic_test.cpp ----------------------*/
/*--------------------------------------------------------------------------*/
