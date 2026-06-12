/*--------------------------------------------------------------------------*/
/*-------------------------- File test.cpp ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Main for testing BundleSolverML
 *
 * A "random" convex PolyhedralFunction is constructed and put as the only
 * Objective of an otherwise "empty" AbstractBlock. The Block is first
 * solved by a standard BundleSolver, whose optimal value is taken as the
 * reference, and then repeatedly solved by a BundleSolverML, whose network
 * is trained online via Backward() between successive solves; the optimal
 * values are compared at each epoch. The model save / load round-trip and
 * the shared-network mechanism are tested as well.
 *
 * \author Francesca Demelas \n
 *         Laboratoire d'Informatique de Paris Nord \n
 *         Universite' Sorbonne Paris Nord \n
 *
 * \author Donato Meoli \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Francesca Demelas, Donato Meoli
 */
/*--------------------------------------------------------------------------*/
/*-------------------------------- MACROS ----------------------------------*/
/*--------------------------------------------------------------------------*/

#define LOG_LEVEL 0
// 0 = only pass/fail
// 1 = result of each epoch
// 2 = + solver log

#if( LOG_LEVEL >= 1 )
 #define LOG1( x ) cout << x
#else
 #define LOG1( x )
#endif

/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <fstream>
#include <iomanip>
#include <random>

#include "AbstractBlock.h"

#include "ColVariable.h"

#include "FRealObjective.h"

#include "PolyhedralFunction.h"

#include "BundleSolverML.h"

#include "common_utils.h"

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace std;

using namespace SMSpp_di_unipi_it;

using Index = Block::Index;

using FunctionValue = Function::FunctionValue;

using MultiVector = PolyhedralFunction::MultiVector;
using RealVector = PolyhedralFunction::RealVector;

/*--------------------------------------------------------------------------*/
/*------------------------------- CONSTANTS --------------------------------*/
/*--------------------------------------------------------------------------*/

const double BND = 1e+6;  ///< the global lower bound on the function

const char * const ModelFile = "model.pt";  ///< file for the model weights

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/

AbstractBlock * NDOBlock;  // the problem expressed via PolyhedralFunction

std::mt19937 rg;           // base random generator
std::uniform_real_distribution<> dis( 0.0 , 1.0 );

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

static double rndfctr( void )
{
 // return a random number between -1 and 1, with 100 different values
 return( double( ( rand() % 200 ) - 100 ) / double( 100 ) );
 }

/*--------------------------------------------------------------------------*/

static void reset_x( void )
{
 // reset the Variable of NDOBlock to the all-0 starting point, so that
 // each epoch starts from the same initial iterate
 auto x = NDOBlock->get_static_variable_v< ColVariable >( "x" );
 for( auto & xi : *x )
  xi.set_value( 0 );
 }

/*--------------------------------------------------------------------------*/

static void wipe_global_pool( void )
{
 /* Wipe the linearizations accumulated by the previous solves in the
  * global pool of the PolyhedralFunction: on larger instances they would
  * otherwise fill it completely, leaving no space for those produced by
  * the next solve, since BundleSolver "plays nice" and never evicts
  * linearizations it does not own [see intBPar7], thereby stalling with
  * an empty bundle. */
 auto PF = static_cast< PolyhedralFunction * >(
	      NDOBlock->get_objective< FRealObjective >()->get_function() );
 PF->delete_linearizations( {} , true , eNoMod );
 }

/*--------------------------------------------------------------------------*/

static double net_norm( Net * nn )
{
 // return the L2 norm of all the parameters of the network
 double nrm = 0;
 for( auto & p : nn->parameters() )
  nrm += p.norm().item< double >() * p.norm().item< double >();
 return( std::sqrt( nrm ) );
 }

/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 // override the default terminate handler to print the exception message
 std::set_terminate( smspp_terminate );

 // reading command line parameters - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 long int seed = 0;
 Index nvar = 10;
 double dens = 4;
 Index n_epochs = 5;

 switch( argc ) {
  case( 5 ): Str2Sthg( argv[ 4 ] , n_epochs );
  case( 4 ): Str2Sthg( argv[ 3 ] , dens );
  case( 3 ): Str2Sthg( argv[ 2 ] , nvar );
  case( 2 ): Str2Sthg( argv[ 1 ] , seed );
             break;
  default: cerr << "Usage: " << argv[ 0 ] << " seed [nvar dens #epochs]"
		<< endl <<
	   "       nvar: number of variables [10]"
		<< endl <<
	   "       dens: rows / variables [4]"
		<< endl <<
	   "       #epochs: training epochs [5]"
		<< endl;
	   return( 1 );
  }

 if( nvar < 1 ) {
  cout << "error: nvar too small";
  exit( 1 );
  }

 Index m = nvar * dens;
 if( m < 1 ) {
  cout << "error: dens too small";
  exit( 1 );
  }

 rg.seed( seed );
 srand( seed );
 torch::manual_seed( seed );  // make the network initialization reproducible

 cout.setf( ios::scientific , ios::floatfield );
 cout << setprecision( 10 );

 // construction and loading of the objects - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 {
  NDOBlock = new AbstractBlock();

  // construct the Variable
  auto x = new std::vector< ColVariable >( nvar );
  PolyhedralFunction::VarVector vars( nvar );
  auto vit = vars.begin();
  for( auto & xi : *x )
   *(vit++) = & xi;

  // construct the random m x nvar matrix A and the m-vector b
  MultiVector A( m );
  for( auto & Ai : A ) {
   Ai.resize( nvar );
   for( auto & aij : Ai )
    aij = 10 * rndfctr();
   }

  RealVector b( m );
  for( auto & bi : b )
   bi = 10 * rndfctr();

  // construct the Objective: a convex PolyhedralFunction with a finite
  // global lower bound, so that the problem is surely bounded below
  auto PF = new PolyhedralFunction( std::move( vars ) , std::move( A ) ,
				    std::move( b ) , - BND );
  auto objNDO = new FRealObjective();
  objNDO->set_function( PF );
  objNDO->set_sense( Objective::eMin , eNoMod );

  // now set the Variable and Objective in the AbstractBlock
  NDOBlock->add_static_variable( *x , "x" );
  NDOBlock->set_objective( objNDO );
  }

 bool AllPassed = true;

 // reference solve with the standard BundleSolver- - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 double fi_ref;
 {
  std::string bsc_fn = "BSPar.txt";
  Configuration * bsc = Configuration::deserialize( bsc_fn );
  if( ! bsc ) {
   cerr << "Error: cannot load BSC from " << bsc_fn << endl;
   exit( 1 );
   }
  s_config_Block( NDOBlock , bsc , bsc_fn );

  Solver * slvr = ( NDOBlock->get_registered_solvers() ).front();
  #if( LOG_LEVEL >= 2 )
   slvr->set_log( & cout );
  #endif

  reset_x();
  int rtrn = slvr->compute( false );
  bool hs = ( ( rtrn >= Solver::kOK ) && ( rtrn < Solver::kError ) ) ||
            ( rtrn == Solver::kLowPrecision );
  if( ! hs ) {
   cout << "BundleSolver: failure (return code " << rtrn << ")" << endl;
   cout << RED( Shit happened!! ) << endl;
   exit( 1 );
   }
  fi_ref = slvr->get_ub();

  LOG1( "BundleSolver: Fi* = " << fi_ref << endl );

  // unregister the reference Solver
  s_config_Block( NDOBlock , bsc );
  delete( bsc );
  }

 // training solves with BundleSolverML - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 std::string mlbsc_fn = "BSPar-ML.txt";
 Configuration * mlbsc = Configuration::deserialize( mlbsc_fn );
 if( ! mlbsc ) {
  cerr << "Error: cannot load BSC from " << mlbsc_fn << endl;
  exit( 1 );
  }
 s_config_Block( NDOBlock , mlbsc , mlbsc_fn );

 auto bml = dynamic_cast< BundleSolverML * >(
		         ( NDOBlock->get_registered_solvers() ).front() );
 if( ! bml ) {
  cerr << "Error: the registered Solver is not a BundleSolverML" << endl;
  exit( 1 );
  }

 #if( LOG_LEVEL >= 2 )
  bml->set_log( & cout );
 #endif

 // the relative accuracy required to BundleSolverML in BSPar-ML.txt is
 // 1e-6, but an untrained network may stop the algorithm slightly short
 // of the required precision, hence the looser comparison tolerance
 const double tol = 1e-4;

 for( Index epoch = 0 ; epoch < n_epochs ; ++epoch ) {
  // detach and re-attach the Solver so that each epoch performs a full
  // solve from scratch: otherwise the bundle accumulated in the previous
  // epochs makes the Solver terminate immediately with no new iteration
  reset_x();
  wipe_global_pool();
  NDOBlock->unregister_Solver( bml );
  NDOBlock->register_Solver( bml );

  int rtrn = bml->compute( false );
  bool hs = ( ( rtrn >= Solver::kOK ) && ( rtrn < Solver::kError ) ) ||
            ( rtrn == Solver::kLowPrecision );
  double fi_ml = hs ? bml->get_ub() : Inf< double >();

  bool ok = hs && ( std::abs( fi_ml - fi_ref ) <=
		    tol * std::max( 1.0 , std::abs( fi_ref ) ) );
  AllPassed &= ok;

  LOG1( "BundleSolverML[ " << epoch << " ]: Fi* = " << fi_ml << " ("
	<< bml->phi_vecs.size() << " recorded iterations)" << endl );
  if( ! ok )
   cout << "epoch " << epoch << ": Fi* = " << fi_ml << " vs reference "
	<< fi_ref << " <-- ERROR" << endl;

  // the network must have actually been used, i.e., Heuristic() must have
  // recorded at least one iteration, for otherwise the "ML" part of the
  // solver is not exercised at all
  if( bml->phi_vecs.empty() ) {
   cout << "epoch " << epoch << ": Heuristic() recorded no iteration"
	<< " <-- ERROR" << endl;
   AllPassed = false;
   }

  // online training step: the parameters of the network must change
  double nrm_before = net_norm( bml->nn );
  bml->Backward();
  double nrm_after = net_norm( bml->nn );

  if( ( ! bml->phi_vecs.empty() ) && ( nrm_before == nrm_after ) ) {
   cout << "epoch " << epoch << ": Backward() did not change the network"
	<< " parameters <-- ERROR" << endl;
   AllPassed = false;
   }

  bml->ClearBuffers();
  }

 // test the model save / load round-trip - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 {
  auto probe = torch::ones( { 20 } );
  double out_saved = bml->nn->forward( probe ).item< double >();
  bml->SaveModel( ModelFile );

  // one more training epoch to perturb the weights
  reset_x();
  wipe_global_pool();
  NDOBlock->unregister_Solver( bml );
  NDOBlock->register_Solver( bml );
  bml->compute( false );
  bml->Backward();
  bml->ClearBuffers();

  bml->LoadModel( ModelFile );
  double out_loaded = bml->nn->forward( probe ).item< double >();

  if( std::abs( out_loaded - out_saved ) > 1e-12 ) {
   cout << "save / load round-trip mismatch: " << out_saved << " vs "
	<< out_loaded << " <-- ERROR" << endl;
   AllPassed = false;
   }

  remove( ModelFile );
  }

 // test the shared-network mechanism - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 {
  auto net = bml->get_shared_net();

  auto other = dynamic_cast< BundleSolverML * >(
				 Solver::new_Solver( "BundleSolverML" ) );
  if( ! other ) {
   cerr << "Error: cannot construct a BundleSolverML from the factory"
	<< endl;
   exit( 1 );
   }

  // before the injection the two solvers have different networks ...
  if( other->nn == bml->nn )
   AllPassed = false;

  // ... after the injection they share the same one
  other->set_shared_net( net );
  if( ( other->nn != bml->nn ) ||
      ( other->get_shared_net().get() != bml->nn ) )
   AllPassed = false;

  // and after clearing they differ again
  other->clear_shared_net();
  if( other->nn == bml->nn )
   AllPassed = false;

  delete( other );
  }

 if( AllPassed )
  cout << GREEN( All tests passed!! ) << endl;
 else
  cout << RED( Shit happened!! ) << endl;

 // destroy the Block - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 // apply() the clear()-ed BlockSolverConfig to cleanup the Solver
 s_config_Block( NDOBlock , mlbsc );

 // then delete the BlockSolverConfig
 delete( mlbsc );

 // delete the Block
 delete( NDOBlock );

 // terminate - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 return( AllPassed ? 0 : 1 );

 }  // end( main )

/*--------------------------------------------------------------------------*/
/*------------------------ End File test.cpp -------------------------------*/
/*--------------------------------------------------------------------------*/
