/*--------------------------------------------------------------------------*/
/*------------------------------ File test.cpp -----------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Main for testing SVMBlock, SVCBlock, SVRBlock and SMOSolver.
 *
 * A data set is generated out of a seed, a SVCBlock or a SVRBlock is loaded
 * with it and its abstract representation is generated in one of the
 * formulations of the training problem, which the bit-wise value \p wf
 * selects together with the loss and with how the bias is dealt with. All the
 * :Solver of the given BlockSolverConfig are then registered to the Block and
 * the results they obtain are cross-checked against each other, which is the
 * test: the ad hoc SMOSolver, a general-purpose :MILPSolver and, on the
 * decomposed formulation, a LagrangianDualSolver all have to agree on the
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

/// bits 0-1 of wf: which formulation of the training problem
static constexpr Index FormMsk = 3;

/// bit 2 of wf: the training errors are penalised quadratically
static constexpr Index SqrLoss = 4;

/// bit 3 of wf: the bias is regularised together with the weights
static constexpr Index RegBias = 8;

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
Index nchunk = 4;           ///< chunks of the decomposed formulation
Index wf = 0;               ///< what formulation, coded bit-wise
int kernel = SVMBlock::kLinear;   ///< which kernel
bool regression = false;    ///< if the model is a regression one
double parC = 1;            ///< the trade-off parameter C
double parE = 0.1;          ///< the half-width of the insensitivity tube
Index n_repeat = 10;        ///< number of rounds
double tol = 1e-5;          ///< relative tolerance of the cross-check

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
/// runs one round: builds the SVMBlock, solves it with every Solver, checks

static bool run_round( unsigned sd )
{
 auto svm = construct( sd );

 // which formulation the abstract representation encodes is a Configuration
 // matter, and so is the number of chunks the decomposed one is split into
 SimpleConfiguration< std::pair< int , int > >
  fcfg( { int( wf & FormMsk ) , int( nchunk ) } );

 svm->generate_abstract_variables( & fcfg );
 svm->generate_abstract_constraints();
 svm->generate_objective();

 // attach the Solver by reading a BlockSolverConfig from file and apply()-ing
 // it to the SVMBlock; the BlockSolverConfig is clear()-ed and kept to do the
 // cleanup at the end. It may be a plain BlockSolverConfig or a meta-config
 // SimpleConfiguration< std::map< std::string , Configuration * > >
 auto bsc = Configuration::deserialize( sconf_file );
 s_config_Block( svm , bsc , sconf_file );

 if( svm->get_registered_solvers().empty() ) {
  std::cerr << "Error: the BlockSolverConfig registered no Solver"
            << std::endl;
  return( false );
  }

 /* Every Solver is read as an exact optimum up to the test tolerance, taken
  * from the finite one of the bounds it returns: this covers a :MILPSolver
  * and SMOSolver, which close the gap, as well as a LagrangianDualSolver,
  * whose lower bound is the optimal value itself since the training problem
  * is convex and the decomposed formulation is an exact reformulation of it. */
 const bool ok = SolveAll( svm , eps_getter( {} ) , RefObjective , tol );

 s_config_Block( svm , bsc );  // remove the Solver by re-apply()-ing the
 delete bsc;                   // clear()-ed BlockSolverConfig
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
 short_opts += "e:N:M:s:f:K:C:E:n:t:r:g";
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
   { "regress"  , no_argument       , nullptr , 'g' } };
 long_opts.insert( std::prev( long_opts.end() ) ,
                   my_opts.begin() , my_opts.end() );
 help += "  -e, --seed <n>                  pseudo-random generator seed [1]\n"
         "  -N, --nsample <n>               number of samples [60]\n"
         "  -M, --nfeature <n>              number of features [4]\n"
         "  -s, --nchunk <n>                chunks of the decomposed "
         "formulation [4]\n"
         "  -f, --wf <bits>                 what formulation, bit-wise [0]:\n"
         "                                    0-1 = 0 Wolfe dual, 1 primal,\n"
         "                                          2 decomposed\n"
         "                                    +4  = squared loss\n"
         "                                    +8  = regularised bias\n"
         "  -K, --kernel <n>                kernel: 0 linear, 1 poly,\n"
         "                                  2 gaussian, 3 laplacian, "
         "4 sigmoid [0]\n"
         "  -C, --parC <x>                  trade-off parameter C [1]\n"
         "  -E, --epsilon <x>               half-width of the insensitivity "
         "tube [0.1]\n"
         "  -g, --regress                   regression instead of "
         "classification\n"
         "  -n, --rounds <n>                how many rounds [10]\n"
         "  -t, --tol <x>                   relative tolerance of the "
         "cross-check [1e-5]\n"
         "  -r, --ref <x>                   reference objective value\n";

 process_args( argc , argv , process_specific_arg );

 // the BlockSolverConfig (-S) must be provided explicitly: the test never
 // falls back to a hardcoded default Configuration
 require_solver_config();

 if( ( wf & FormMsk ) > SVMBlock::kDecomposed ) {
  std::cerr << "Error: unknown formulation " << ( wf & FormMsk ) << std::endl;
  return( 1 );
  }

 if( ( ( wf & FormMsk ) != SVMBlock::kWolfeDual ) &&
     ( kernel != SVMBlock::kLinear ) ) {
  std::cerr << "Error: the primal formulations require the linear kernel"
            << std::endl;
  return( 1 );
  }

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
