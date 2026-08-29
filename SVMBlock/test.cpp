/*--------------------------------------------------------------------------*/
/*------------------------------ File test.cpp -----------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Main for testing SVMBlock, SVCBlock, SVRBlock and SMOSolver.
 *
 * A data set is generated out of a seed, a SVCBlock or a SVRBlock is loaded
 * with it and its abstract representation is generated for the problem that
 * the BlockConfig asks for, the Wolfe dual or the training problem itself,
 * with the loss and the treatment of the bias that the bit-wise value \p wf
 * selects; with more than one chunk the training problem is rather rewritten
 * as the consensus reformulation set_structure() gives it, which is what a
 * Lagrangian Solver attacks.
 *
 * All the :Solver of the given BlockSolverConfig are then registered and
 * the results they obtain are cross-checked against each other, which is the
 * test: the ad hoc SMOSolver, a general-purpose :MILPSolver and, on the
 * consensus rewriting, a LagrangianDualSolver all have to agree on the
 * optimal value of the same training problem, whichever formulation of it the
 * abstract representation encodes.
 *
 * The whole thing is repeated for a given number of rounds, each one with a
 * different data set drawn from the same distribution. Alternatively, the
 * SVMBlock can be read from a netCDF file given as the positional argument.
 *
 * \author Donato Meoli \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Donato Meoli
 */
/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include "SVCBlock.h"

#include "SVRBlock.h"

#include "SMOSolver.h"

#include "common_utils.h"

#include <random>

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

using Index = Block::Index;
using doubleVec = SVMBlock::doubleVec;

/*--------------------------------------------------------------------------*/
/*----------------------------- CONSTANTS ----------------------------------*/
/*--------------------------------------------------------------------------*/

/// bit 0 of wf: the training errors are penalised quadratically
static constexpr Index SqrLoss = 1;

/// bit 1 of wf: the bias is regularised together with the weights
static constexpr Index RegBias = 2;

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/

// test-specific command-line knobs, set by process_specific_arg(); the
// standard parameters (-S BlockSolverConfig, -v, ...) are handled centrally
// by common_utils. This tester GENERATES its own data set out of the seed,
// so the instance positional is optional (filename_optional = true).

long int seed = 1;          ///< seed of the pseudo-random generator
Index nsample = 60;         ///< number of samples of the generated data set
Index nfeature = 4;         ///< number of features of each sample
Index nchunk = 1;           ///< chunks of the consensus rewriting, 1 = none
Index wf = 0;               ///< what formulation, coded bit-wise
int kernel = SVMBlock::kLinear;   ///< which kernel
bool regression = false;    ///< if the model is a regression one
double parC = 1;            ///< the trade-off parameter C
double parE = 0.1;          ///< the half-width of the insensitivity tube
Index n_repeat = 10;        ///< number of rounds
double tol = 1e-5;          ///< relative tolerance of the cross-check
bool reopt = false;         ///< re-solve after changing the training problem

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/
/// generates a data set of \p n samples of \p m features out of \p sd
/** Generates a data set that the model can fit well, so that the training
 * problem is neither trivial nor degenerate: two gaussian clouds separated
 * along the first feature for the classification case, an affine function of
 * the features plus a small noise for the regression one. */

static void generate( Index n , Index m , doubleVec & X , doubleVec & y ,
                      unsigned sd )
{
 std::mt19937 rng( sd );
 std::normal_distribution< double > gauss( 0 , 1 );
 std::normal_distribution< double > noise( 0 , 0.05 );

 X.resize( std::size_t( n ) * m );
 y.resize( n );

 if( ! regression ) {
  for( Index i = 0 ; i < n ; ++i ) {
   const double lbl = ( i % 2 ) ? 1 : -1;
   y[ i ] = lbl;
   for( Index j = 0 ; j < m ; ++j )
    X[ std::size_t( i ) * m + j ] = gauss( rng ) + ( j ? 0 : 1.5 * lbl );
   }
  return;
  }

 doubleVec w( m );
 for( Index j = 0 ; j < m ; ++j )
  w[ j ] = gauss( rng );

 for( Index i = 0 ; i < n ; ++i ) {
  double v = 0.5;
  for( Index j = 0 ; j < m ; ++j ) {
   const double xij = gauss( rng );
   X[ std::size_t( i ) * m + j ] = xij;
   v += w[ j ] * xij;
   }
  y[ i ] = v + noise( rng );
  }

 }  // end( generate )

/*--------------------------------------------------------------------------*/
/// constructs the SVMBlock of the round, either generated or read from file

static SVMBlock * construct( unsigned sd )
{
 if( ! filename.empty() ) {
  auto block = dynamic_cast< SVMBlock * >( Block::deserialize( filename ) );
  if( ! block )
   throw( std::invalid_argument( filename + " does not contain a SVMBlock" ) );
  return( block );
  }

 auto svm = dynamic_cast< SVMBlock * >(
  Block::new_Block( regression ? "SVRBlock" : "SVCBlock" ) );

 svm->set_kernel( kernel );
 svm->set_C( parC );
 svm->set_squared_loss( wf & SqrLoss );
 svm->set_reg_bias( wf & RegBias );

 if( auto svr = dynamic_cast< SVRBlock * >( svm ) )
  svr->set_epsilon( parE );

 doubleVec X , y;
 generate( nsample , nfeature , X , y , sd );
 svm->load( nsample , nfeature , std::move( X ) , std::move( y ) );

 return( svm );

 }  // end( construct )

/*--------------------------------------------------------------------------*/
/// changes the training problem under the Solver, re-solving after each change
/** Subjects the SVMBlock to a sequence of changes of its data, re-solving it
 * after each one with all the Solver that are attached to it: since they keep
 * having to agree with each other, this is a check that each of them makes
 * the right sense of the Modification, be it a Solver reading the physical
 * representation, such as SMOSolver, which can then re-optimize starting from
 * the previous solution, or one working on the abstract representation, which
 * the SVMBlock has to keep up to date. */

static bool run_changes( SVMBlock * svm , Block * block )
{
 bool ok = true;

 auto step = [ & ]( const std::string & what ) {
  std::cout << "  after " << what << ": ";
  ok &= SolveAll( block , std::numeric_limits< double >::quiet_NaN() , tol );
  };

 svm->set_C( parC * 8 );
 step( "C increased" );

 svm->set_C( parC / 8 );
 step( "C decreased" );

 svm->set_squared_loss( ! ( wf & SqrLoss ) );
 step( "the loss changed" );

 svm->set_C( parC );
 step( "C changed back" );

 svm->set_squared_loss( wf & SqrLoss );
 step( "the loss changed back" );

 if( auto svr = dynamic_cast< SVRBlock * >( svm ) ) {
  svr->set_epsilon( parE * 4 );
  step( "epsilon increased" );

  svm->chg_target( svm->get_y()[ 0 ] + 1 , 0 );
  step( "a target changed" );
  }
 else {
  svm->chg_target( - svm->get_y()[ 0 ] , 0 );
  step( "a target flipped" );
  }

 // whatever changes the Hessian of the dual as a whole makes the SVMBlock
 // rebuild its abstract representation and issue a NBModification
 svm->set_reg_bias( ! ( wf & RegBias ) );
 step( "the bias regularised or not" );

 if( svm->get_generated_problem() != SVMBlock::kPrimal ) {
  svm->set_kernel( kernel == SVMBlock::kLinear ? SVMBlock::kGaussian
                                               : SVMBlock::kLinear );
  step( "the kernel changed" );
  }

 // and so does a whole new data set, of a different size
 doubleVec X , y;
 generate( ( nsample * 2 ) / 3 + 1 , nfeature , X , y , seed + 1000 );
 svm->load( ( nsample * 2 ) / 3 + 1 , nfeature , std::move( X ) ,
            std::move( y ) );
 step( "a new data set" );

 return( ok );

 }  // end( run_changes )

/*--------------------------------------------------------------------------*/
/// runs one round: builds the SVMBlock, solves it with every Solver, checks

static bool run_round( unsigned sd )
{
 auto svm = construct( sd );

 /* Whether the training problem is one Block or the chunks tied by the
  * consensus constraints is a *structure* of the SVMBlock, chosen by
  * set_structure(); which problem the abstract representation encodes is
  * instead a Configuration of the Variable. Both come out of the BlockConfig
  * when one is given, the number of chunks otherwise being said by -s. */
 Block * block = svm;

 if( ! bconf_file.empty() ) {
  auto bc = Configuration::deserialize( bconf_file );
  b_config_Block( svm , bc , bconf_file );
  }

 if( nchunk > 1 ) {
  SimpleConfiguration< int > chunks( nchunk );
  svm->set_structure( & chunks );
  }

 svm->generate_abstract_variables();
 svm->generate_abstract_constraints();
 svm->generate_objective();

 // attach the Solver by reading a BlockSolverConfig from file and apply()-ing
 // it to the SVMBlock; the BlockSolverConfig is clear()-ed and kept to do the
 // cleanup at the end. It may be a plain BlockSolverConfig or a meta-config
 // SimpleConfiguration< std::map< std::string , Configuration * > >
 auto bsc = Configuration::deserialize( sconf_file );
 s_config_Block( block , bsc , sconf_file );

 if( block->get_registered_solvers().empty() ) {
  std::cerr << "Error: the BlockSolverConfig registered no Solver"
            << std::endl;
  return( false );
  }

 /* Every Solver is read as an exact optimum up to the test tolerance, taken
  * from the finite one of the bounds it returns: this covers a :MILPSolver
  * and SMOSolver, which close the gap, as well as a LagrangianDualSolver,
  * whose lower bound is the optimal value itself since the training problem
  * is convex and the consensus rewriting is an exact one. */
 bool ok = SolveAll( block , RefObjective , tol );

 /* The training problem can also be changed under the Solver, which then have
  * to keep agreeing with each other: the consensus rewriting is left out,
  * since there the Solver are attached to the assembled Block and not to the
  * SVMBlock the changes would be made to. */
 if( reopt && ( nchunk <= 1 ) )
  ok &= run_changes( svm , block );

 s_config_Block( block , bsc );  // remove the Solver by re-apply()-ing the
 delete bsc;                     // clear()-ed BlockSolverConfig

 if( block != svm )
  delete block;
 delete svm;

 return( ok );

 }  // end( run_round )

/*--------------------------------------------------------------------------*/
/// processes the test-specific command-line options

static bool process_specific_arg( int opt )
{
 switch( opt ) {
  case( 'e' ): Str2Sthg( optarg , seed );      return( true );
  case( 'N' ): Str2Sthg( optarg , nsample );   return( true );
  case( 'M' ): Str2Sthg( optarg , nfeature );  return( true );
  case( 's' ): Str2Sthg( optarg , nchunk );    return( true );
  case( 'f' ): Str2Sthg( optarg , wf );        return( true );
  case( 'K' ): Str2Sthg( optarg , kernel );    return( true );
  case( 'C' ): Str2Sthg( optarg , parC );      return( true );
  case( 'E' ): Str2Sthg( optarg , parE );      return( true );
  case( 'n' ): Str2Sthg( optarg , n_repeat );  return( true );
  case( 't' ): Str2Sthg( optarg , tol );       return( true );
  case( 'g' ): regression = true;              return( true );
  case( 'R' ): reopt = true;                   return( true );
  case( 'r' ): Str2Sthg( optarg , RefObjective ); return( true );
  }

 return( false );

 }  // end( process_specific_arg )

/*--------------------------------------------------------------------------*/
/*-------------------------------- main() ----------------------------------*/
/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 std::set_terminate( smspp_terminate );

 docopt_desc = "SMS++ SVMBlock test.\n";
 filename_optional = true;
 short_opts += "e:N:M:s:f:K:C:E:n:t:r:gR";
 const std::vector< option > my_opts = {
   { "seed"     , required_argument , nullptr , 'e' } ,
   { "nsample"  , required_argument , nullptr , 'N' } ,
   { "nfeature" , required_argument , nullptr , 'M' } ,
   { "nchunk"   , required_argument , nullptr , 's' } ,
   { "wf"       , required_argument , nullptr , 'f' } ,
   { "kernel"   , required_argument , nullptr , 'K' } ,
   { "parC"     , required_argument , nullptr , 'C' } ,
   { "epsilon"  , required_argument , nullptr , 'E' } ,
   { "rounds"   , required_argument , nullptr , 'n' } ,
   { "tol"      , required_argument , nullptr , 't' } ,
   { "ref"      , required_argument , nullptr , 'r' } ,
   { "regress"  , no_argument       , nullptr , 'g' } ,
   { "reopt"    , no_argument       , nullptr , 'R' } };
 long_opts.insert( std::prev( long_opts.end() ) ,
                   my_opts.begin() , my_opts.end() );
 help += "  -e, --seed <n>                  pseudo-random generator seed [1]\n"
         "  -N, --nsample <n>               number of samples [60]\n"
         "  -M, --nfeature <n>              number of features [4]\n"
         "  -s, --nchunk <n>                rewrite the training problem as "
         "n chunks tied\n"
         "                                  by consensus constraints [1]\n"
         "  -f, --wf <bits>                 the loss and the bias, bit-wise "
         "[0]:\n"
         "                                    1 = squared loss\n"
         "                                    2 = regularised bias\n"
         "  -K, --kernel <n>                kernel: 0 linear, 1 poly,\n"
         "                                  2 gaussian, 3 laplacian, "
         "4 sigmoid [0]\n"
         "  -C, --parC <x>                  trade-off parameter C [1]\n"
         "  -E, --epsilon <x>               half-width of the insensitivity "
         "tube [0.1]\n"
         "  -g, --regress                   regression instead of "
         "classification\n"
         "  -R, --reopt                     also change the training problem "
         "under the\n"
         "                                  Solver, re-solving after each "
         "change\n"
         "  -n, --rounds <n>                how many rounds [10]\n"
         "  -t, --tol <x>                   relative tolerance of the "
         "cross-check [1e-5]\n"
         "  -r, --ref <x>                   reference objective value\n";

 process_args( argc , argv , process_specific_arg );

 // the BlockSolverConfig (-S) must be provided explicitly: the test never
 // falls back to a hardcoded default Configuration
 require_solver_config();


 bool AllPassed = true;

 // a file is one instance, a seed is a family of them
 const Index rounds = filename.empty() ? n_repeat : 1;

 for( Index r = 0 ; r < rounds ; ++r )
  AllPassed &= run_round( seed + r );

 if( AllPassed )
  std::cout << GREEN( All tests passed!! ) << std::endl;
 else
  std::cout << RED( Shit happened!! ) << std::endl;

 return( AllPassed ? 0 : 1 );

 }  // end( main )

/*--------------------------------------------------------------------------*/
/*---------------------------- End File test.cpp ---------------------------*/
/*--------------------------------------------------------------------------*/
