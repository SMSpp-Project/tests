/*--------------------------------------------------------------------------*/
/*-------------------------- File test.cpp ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Main for testing PolyhedralFunctionBlock
 *
 * Given the parameter nf, abs( nf ) "random" PolyhedralFunction are
 * constructed, each inside a PolyhedralFunctionBlock, then R3-Block-ed each
 * to another PolyhedralFunctionBlock. If abs( nf ) > 1, both sets of
 * PolyhedralFunctionBlock are bunched each as sons of two separate
 * AbstractBlock. If nf < 0, the two AbstractBlock are also given two
 * identical linear Objective (a FRealObjective with a LinearFunction inside).
 * Then, the first is configured to use the "linearized" representation, and
 * has an appropriate LP Solver registered; also, UpdateSolver are registered
 * to all its sons (PolyhedralFunctionBlock) that maps all the Modification
 * to the corresponding son of the second. The latter is configured to use the
 * "natural" representation and has an appropriate NDO Solver attached. At
 * each round a "linearized" PolyhedralFunctionBlock is randomly modified,
 * with the Modification automatically transmitted to the corresponding
 * "natural" PolyhedralFunctionBlock to keep them in synch. Then both are
 * solved and the results compared.
 *
 * \author Antonio Frangioni \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Antonio Frangioni
 */
/*--------------------------------------------------------------------------*/
/*-------------------------------- MACROS ----------------------------------*/
/*--------------------------------------------------------------------------*/

#define LOG_LEVEL 0
// 0 = only pass/fail
// 1 = result of each test
// 2 = + solver log
// 3 = + save LP file
// 4 = + print data

#if( LOG_LEVEL >= 1 )
 #define LOG1( x ) cout << x
 #define CLOG1( y , x ) if( y ) cout << x

 #if( LOG_LEVEL >= 2 )
  #define LOG_ON_COUT 0
  // if nonzero, the BundleSolver log is sent on cout rather than on a file
 #endif
#else
 #define LOG1( x )
 #define CLOG1( y , x )
#endif

/*--------------------------------------------------------------------------*/
// if HAVE_CONSTRAINTS == 1, then about 50% of the variables will have a
// non-negativity constraint implemented via ColVariable::is_positive()
// if HAVE_CONSTRAINTS == 2, then about 50% of the variables will have
// bound constraints; of these, 33% will only have 0 lower bound, 33% will
// only have random upper bound, and the rest will have both. of the
// remaining 50% of the variables, another 50%  will have a
// non-negativity constraint implemented via ColVariable::is_positive()
// if HAVE_CONSTRAINT == 3, then the same situation described in the case 2 
// will be reproduced, but while in the NDOBlock the bound constraint are 
// realized by BoxContstraint, in the LPBlock they are FRowConstraint.

#define HAVE_CONSTRAINTS 1

/*--------------------------------------------------------------------------*/
// if BOUND_ALWAYS_RANGED == 0, then the global bound could be turned off and
// the static constraint "global bound" could be treated as a non ranged one.
// if BOUND_ALWAYS_RANGED == 1, then the global bound is always set and the
// the static constraint "global bound" is always represented as a ranged one.
// WARNING: using GRBMILPSolver as *MILPSolver in the LPBlock and with this 
// option set to 0 could generate error.

#define BOUND_ALWAYS_RANGED 0
/*--------------------------------------------------------------------------*/
// if nonzero, the Solver attached to the NDOBlock is detached and re-attached
// to it at all iterations

#define DETACH_NDO 0

// if nonzero, the Solver attached to the LPBlock is detached and re-attached
// to it at all iterations

#define DETACH_LP 0

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

#define SKIP_BEAT 3

/*--------------------------------------------------------------------------*/

#define PANICMSG { cout << endl << "something very bad happened!" << endl; \
		   exit( 1 ); \
                   }

#define PANIC( x ) if( ! ( x ) ) PANICMSG

#define USECOLORS 1
#if( USECOLORS )
 #define RED( x ) "\x1B[31m" #x "\033[0m"
 #define GREEN( x ) "\x1B[32m" #x "\033[0m"
#else
 #define RED( x ) #x
 #define GREEN( x ) #x
#endif

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

#define DYNAMIC_VARS 0
// if 1, half of the variables are dynamic
// WARNING: THE CODE HERE IS LIFTER STRAIGHT FROM PolthedralFunction/test.cpp
// BUT IT DOES NOT WORK DUE TO NOT-YET-HANDLED COMPLICATIONS IN BundleSolver
// (ALL C05Function MUST HAVE THE SAME ColVariable, AND THEREFORE ADDING AND
// REMOVING THEM MUST ALWAYS BE DONE AT THE SAME TIME)

/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <fstream>
#include <sstream>
#include <iomanip>

#include <random>

#include "BlockSolverConfig.h"

#if( LOG_LEVEL >= 3 )
 #include "MILPSolver.h"
#endif

#include "PolyhedralFunctionBlock.h"

#include "UpdateSolver.h"

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace std;

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*-------------------------------- TYPES -----------------------------------*/
/*--------------------------------------------------------------------------*/

using Index = Block::Index;

using Range = Block::Range;

using Subset = Block::Subset;

using FunctionValue = Function::FunctionValue;
using c_FunctionValue = Function::c_FunctionValue;

using MultiVector = PolyhedralFunction::MultiVector;
using RealVector = PolyhedralFunction::RealVector;

using p_LF = LinearFunction *;
using p_PF = PolyhedralFunction *;
using p_PFB = PolyhedralFunctionBlock *;

/*--------------------------------------------------------------------------*/
/*------------------------------- CONSTANTS --------------------------------*/
/*--------------------------------------------------------------------------*/

const double scale = 10;
const char * const logF = "log.bn";

c_FunctionValue INF = SMSpp_di_unipi_it::Inf< FunctionValue >();

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/

AbstractBlock * LPBlock;   // the "linearized" representaion

AbstractBlock * NDOBlock;  // the "natural" representation

bool convex = true;        // true if the PolyhedralFunction is convex

double bound = 3000;       // the global *conditional* bound 

double lbound = INF;       // the global lower bound

int nf = 0;                // number of sub-Block
Index nvar = 10;           // number of variables
#if DYNAMIC_VARS > 0
 Index nsvar;              // number of static variables
 Index ndvar;              // number of dynamic variables
#else
 #define nsvar nvar        // all variables are static
#endif

std::mt19937 rg;           // base random generator
std::uniform_real_distribution<> dis( 0.0 , 1.0 );

MultiVector A;
RealVector b;

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/
// convex ==> minimize ==> negative numbers

static double rs( double x ) { return( convex ? -x : x ); }

/*--------------------------------------------------------------------------*/

template< class T >
static void Str2Sthg( const char* const str , T &sthg )
{
 istringstream( str ) >> sthg;
 }

/*--------------------------------------------------------------------------*/

static double rndfctr( void )
{
 // a random number between 0.5 and 2, with 50% probability of being < 1
 double fctr = dis( rg ) - 0.5;
 return( fctr < 0 ? - fctr : fctr * 4 );
 }

/*--------------------------------------------------------------------------*/

static void GenerateA( Index nr , Index nc )
{
 A.resize( nr );

 for( auto & Ai : A ) {
  Ai.resize( nc );
  for( auto & aij : Ai )
   aij = scale * ( 2 * dis( rg ) - 1 );
  }
 }

/*--------------------------------------------------------------------------*/

static void Generateb( Index nr )
{
 b.resize( nr );

 for( auto & bj : b )
  bj = scale * nvar * ( 2 * dis( rg ) - 1 ) / 4;
 }

/*--------------------------------------------------------------------------*/

static void GenerateAb( Index nr , Index nc )
{
 // rationale: the solution x^* will be more or less the solution of some
 // square sub-system A_B x = b_B. We want x^* to be "well scaled", i.e.,
 // the entries to be ~= 1 (in absolute value). The average of each row A_i
 // is 0, the maximum (and minimum) expected value is something like
 // scale * nvar / 2. So we take each b_j in +- scale * nvar / 4

 GenerateA( nr , nc );
 Generateb( nr );
 }

/*--------------------------------------------------------------------------*/

static double GenerateBND( void )
{
 // rationale: we expect the solution x^* to have entries ~= 1 (in absolute
 // value, and the coefficients of A are <= scale (in absolute value), so
 // the LHS should be at most around - scale * nvar; the RHS can add it
 // a further - scale * nvar / 4, so we expect - (5/4) * scale * nvar to
 // be a "natural" LB. We therefore set the LB to a mean of 1/2 of that
 // (tight) 33% of the time, a mean of 2 times that (loose) 33% of the time,
 // and -INF the rest

 double BND = INF;          // no bound
 if( dis( rg ) <= 0.333 )   // "tight" bound
  BND = dis( rg ) * 5 * scale * nvar / 4;
 else{
  #if BOUND_ALWAYS_RANGED == 0
    if( dis( rg ) <= 0.333 )  // "loose" bound
    BND = dis( rg ) * 5 * scale * nvar;
  #endif
  #if BOUND_ALWAYS_RANGED == 1 // global bound needs to be always set
    BND = dis( rg ) * 5 * scale * nvar; // "loose" bound
  #endif
 }

 if( convex )
  BND = - BND;

 return( BND );
 }

/*--------------------------------------------------------------------------*/

static void set_global_bound( void )
{
 auto bnd = GenerateBND();
 if( bnd == lbound )
  return;
 
 lbound = bnd;

 auto lbc = LPBlock->get_static_constraint< FRowConstraint >(
							    "globalbound" );
 if( ! lbc ) {
  cout << "something very bad happened!" << endl;
  exit( 1 );
  }

 if( convex ) {
  lbc->set_lhs( bnd );          // set it in LPBlock
  if( std::abs( bnd ) == INF )  // set it in NDOBlock
   NDOBlock->set_valid_lower_bound( - bound , true );
  else
   NDOBlock->set_valid_lower_bound( bnd , false );
  }
 else {
  lbc->set_rhs( bnd );          // set it in LPBlock
  if( std::abs( bnd ) == INF )  // set it in NDOBlock
   NDOBlock->set_valid_upper_bound( bound , true );
  else
   NDOBlock->set_valid_upper_bound( bnd , false );
  }
 }

/*--------------------------------------------------------------------------*/

static Subset GenerateSubset( Index m , Index k )
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

static void printAb( const MultiVector & tA , const RealVector & tb ,
		     double bnd )
{
 PANIC( tA.size() == tb.size() )
 for( auto & tai : tA )
  PANIC( tai.size() == nvar );

 cout << "n = " << nvar << ", m = " << tA.size();
 if( std::abs( bnd ) == INF )
  cout << " (no bound)" << endl;
 else
  cout << ", bound = " << bnd << endl;

 for( Index i = 0 ; i < tA.size() ; ++i ) {
  cout << "A[ " << i << " ] = [ ";
  for( Index j = 0 ; j < nvar ; ++j )
   cout << tA[ i ][ j ] << " ";
   cout << "], b[ " << i << " ] = " << tb[ i ] << endl;
  }
 }

/*--------------------------------------------------------------------------*/

static void ConstructObj( AbstractBlock * AB )
{
 // in the AbstractBlock x is the 0-th group of static Variable, and this
 // is only called if nf < 0

 auto x = AB->get_static_variable_v< ColVariable >( 0 );
 #if DYNAMIC_VARS > 0
  auto xd = AB->get_dynamic_variable< ColVariable >( 0 );
 #endif

 LinearFunction::v_coeff_pair cp( nvar );
 Index i = 0;
 // static x
 for( ; i < nsvar ; ++i )
  cp[ i ] = std::make_pair( &((*x)[ i ] ) , A[ 0 ][ i ] );

 #if DYNAMIC_VARS > 0
  // dynamic x
  auto xdit = xd->begin();
  for( ; i < nvar ; ++i , ++xdit )
   cp[ i ] = std::make_pair( &(*xdit) , A[ 0 ][ i ] );
 #endif

 auto obj = new FRealObjective( AB , new LinearFunction( std::move( cp ) ) );
 obj->set_sense( convex ? Objective::eMin : Objective::eMax , eNoMod );
 AB->set_objective( obj , eNoMod );
 }

/*--------------------------------------------------------------------------*/

static void ChangeLPConstraint( Index i , FRowConstraint & ci , ModParam iAM )
{
 // change the constant == LHS or RHS of the constraint (depending on convex)
 if( convex )
  ci.set_lhs( b[ i ] , iAM );
 else
  ci.set_rhs( b[ i ] , iAM );

 // now change the coefficients, except that of v that is always 1
 LinearFunction::Vec_FunctionValue coeffs( nvar );

 for( Index j = 0 ; j < nvar ; ++j )
  coeffs[ j ] = - A[ i ][ j ];

 auto f = static_cast< p_LF >( ci.get_function() );
 f->modify_coefficients( std::move( coeffs ) , Range( 1 , nvar + 1 ) , iAM );
 }

/*--------------------------------------------------------------------------*/

static bool SolveBoth( void ) 
{
 try {
  // solve the LPBlock- - - - - - - - - - - - - - - - - - - - - - - - - - - -
  Solver * slvrLP = (LPBlock->get_registered_solvers()).front();
  #if DETACH_LP
   LPBlock->unregister_Solver( slvrLP );
   LPBlock->register_Solver( slvrLP , true );  // push it to the front
  #endif
  int rtrnLP = slvrLP->compute( false );
  bool hsLP = ( ( rtrnLP >= Solver::kOK ) && ( rtrnLP < Solver::kError ) )
              || ( rtrnLP == Solver::kLowPrecision );
  double foLP = hsLP ? ( convex ? slvrLP->get_ub() : slvrLP->get_lb() )
                     : ( convex ? INF : -INF );

  // solve the NODBlock - - - - - - - - - - - - - - - - - - - - - - - - - - -
  Solver * slvrNDO = (NDOBlock->get_registered_solvers()).front();
  #if DETACH_NDO
   NDOBlock->unregister_Solver( slvrNDO );
   NDOBlock->register_Solver( slvrNDO );
  #endif
  int rtrnNDO = slvrNDO->compute( false );
  bool hsNDO = ( ( rtrnNDO >= Solver::kOK ) && ( rtrnNDO < Solver::kError ) )
              || ( rtrnNDO == Solver::kLowPrecision );
  double foNDO = hsNDO ? ( convex ? slvrNDO->get_ub() : slvrNDO->get_lb() )
                       : ( convex ? INF : -INF );

  if( hsLP && hsNDO && ( abs( foLP - foNDO ) <= 2e-7 *
			 max( double( 1 ) , abs( max( foLP , foNDO ) ) ) ) ) {
   LOG1( "OK(f)" << endl );
   return( true );
   }

  if( hsLP && ( rtrnNDO == Solver::kUnbounded ) ) {
   /* Weird case: the LP found an optimal solution but the NDO declared the
    * problem unbounded below. This may be because the tentative lb is too
    * high, check it this actually is the case and if so declare the
    * run a success (but also decrease the lb). */
   if( ( convex && ( foNDO <= - bound * ( 1 - 1e-9 ) ) ) ||
       ( ( ! convex ) && ( foNDO >= bound * ( 1 - 1e-9 ) ) ) ) {
    LOG1( "OK(?bound?)" << endl );
    bound *= 2;
    if( convex )
     NDOBlock->set_valid_lower_bound( -bound , true );
    else
     NDOBlock->set_valid_upper_bound( bound , true );
    return( true );
    }
   }

  if( ( rtrnLP == Solver::kInfeasible ) &&
      ( rtrnNDO == Solver::kInfeasible ) ) {
    LOG1( "OK(?e?)" << endl );
    return( true );
    }

  if( ( rtrnLP == Solver::kUnbounded ) &&
      ( rtrnNDO == Solver::kUnbounded ) ) {
   LOG1( "OK(u)" << endl );
   return( true );
   }

  #if( LOG_LEVEL >= 1 )
   cout << "LPBlock = ";
   if( hsLP )
    cout << foLP;
   else
    if( rtrnLP == Solver::kInfeasible )
     cout << "    Unfeas(?)";
    else
     if( rtrnLP == Solver::kUnbounded )
      cout << "      Unbounded";
     else
      cout << "      Error!";

   cout << " ~ NDOBlock = ";
   if( hsNDO )
    cout << foNDO;
   else
    if( rtrnNDO == Solver::kInfeasible )
     cout << "    Unfeas(?)";
    else
     if( rtrnNDO == Solver::kUnbounded )
      cout << "      Unbounded";
     else
      cout << "      Error!";
   cout << endl;
  #endif

  return( false );
  }
 catch( exception &e ) {
  cerr << e.what() << endl;
  exit( 1 );
  }
 catch(...) {
  cerr << "Error: unknown exception thrown" << endl;
  exit( 1 );
  }
 }

/*--------------------------------------------------------------------------*/

int main( int argc , char **argv )
{
 // reading command line parameters - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 assert( SKIP_BEAT >= 0 );

 long int seed = 0;
 Index wchg = 319;
 double dens = 3;
 Index n_repeat = 40;
 Index n_change = 10;
 double p_change = 0.5;

 switch( argc ) {
  case( 9 ): Str2Sthg( argv[ 8 ] , p_change );
  case( 8 ): Str2Sthg( argv[ 7 ] , n_change );
  case( 7 ): Str2Sthg( argv[ 6 ] , n_repeat );
  case( 6 ): Str2Sthg( argv[ 5 ] , nf );
  case( 5 ): Str2Sthg( argv[ 4 ] , dens );
  case( 4 ): Str2Sthg( argv[ 3 ] , nvar );
  case( 3 ): Str2Sthg( argv[ 2 ] , wchg );
  case( 2 ): Str2Sthg( argv[ 1 ] , seed );
             break;
  default: cerr << "Usage: " << argv[ 0 ] <<
	   " seed [wchg nvar dens #nf #rounds #chng %chng]"
 		<< endl <<
           "       wchg: what to change, coded bit-wise [319]"
		<< endl <<
           "             0 = add rows, 1 = delete rows "
		<< endl <<
           "             2 = modify rows, 3 = modify constants"
		<< endl <<
           "             4 = change local lower/upper bound"
		<< endl <<
           "             5 = change linear objective"
		<< endl <<
           "             6 = change global lower/upper bound"
  #if DYNAMIC_VARS > 0  
		<< endl <<
           "             7 = add variables, 8 = delete variables"
  #endif
		<< endl <<
           "             9 (+512) = do \"abstract\" changes"
	        << endl <<
           "       nvar: number of variables [10]"
	        << endl <<
           "       dens: rows / variables [3]"
	        << endl <<
           "       #nf: number of PolyhedralFunction in the sub-Block [0]"
	        << endl <<
           "       #rounds: how many iterations [40]"
	        << endl <<
           "       #chng: number of changes [10]"
	        << endl <<
           "       %chng: probability of changing [0.5]"
	        << endl;
	   return( 1 );
  }

 if( nvar < 1 ) {
  cout << "error: nvar too small";
  exit( 1 );
  }

 #if DYNAMIC_VARS > 0
  nsvar = nvar / 2;      // half of the variables are dynamic
  ndvar = nvar - nsvar;  // the other half are static
 #endif

 Index m = nvar * dens;  // number of rows
 if( m < 1 ) {
  cout << "error: dens too small";
  exit( 1 );
  }

 // adjust the bound depending on the number of components and variables
 // for each component, (5/4) * scale * nvar should be a "natural" bound,
 // so we use
 //     < # components > * 10 * scale * nvar
 // as the global conditional bound, hoping it will also account for the
 // linear term, if any
 bound = std::max( 1 , std::abs( nf ) ) * 10 * scale * nvar;

 rg.seed( seed );  // seed the pseudo-random number generator

 // constructing the data of the problem- - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // choosing whether convex or concave: toss a(n unbiased, two-sided) coin
 convex = ( dis( rg ) < 0.5 );

 cout.setf( ios::scientific, ios::floatfield );
 cout << setprecision( 10 );

 // construction and loading of the objects - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  
 // construct the "linearized" representation - - - - - - - - - - - - - - - -
 {
  // ensure all original pointers go out of scope immediately after that
  // the construction has finished

  if( nf ) {
   LPBlock = new AbstractBlock();
   for( Index i = 0 ; i < std::abs( nf ) ; ++i )
    LPBlock->add_nested_Block( new PolyhedralFunctionBlock( LPBlock ) );
   }
  else
   LPBlock = new PolyhedralFunctionBlock();

  // construct the Variable
  auto xLP = new std::vector< ColVariable >( nsvar );
  PolyhedralFunction::VarVector vars( nvar );
  auto vit = vars.begin();
  for( auto & xi : *xLP )
   *(vit++) = & xi;
  #if DYNAMIC_VARS > 0
   auto xLPd = new std::list< ColVariable >( ndvar );
   for( auto & xi : *xLPd )
    *(vit++) = & xi;
  #endif

  // now set the Variable, Constraint and Objective in the AbstractBlock
  LPBlock->add_static_variable( *xLP , "x" );
  #if DYNAMIC_VARS > 0
   LPBlock->add_dynamic_variable( *xLPd );
  #endif

  if( nf ) {
   // construct the sub-Block
   for( Index i = 0 ; i < LPBlock->get_number_nested_Blocks() ; ++i ) {
    auto bi = static_cast< p_PFB >( LPBlock->get_nested_Block( i ) );
    auto & pf = bi->get_PolyhedralFunction();
    // pass the Variable to the PolyhedralFunction (copy the vector)
    pf.set_variables( PolyhedralFunction::VarVector( vars ) );

    // construct the m x nvar matrix A, the m-vector b, and the bound
    GenerateAb( m , nvar );
    auto BND = GenerateBND();

    #if( LOG_LEVEL >= 4 )
     cout << "PF[ " << i << " ] = " << endl;
     printAb( A , b , BND );
    #endif

    // pass all the data of the PolyhedralFunction
    pf.set_PolyhedralFunction( std::move( A ) , std::move( b ) ,
			       BND , convex );

    // configure it to use the "linearised" representation
    auto bc = new BlockConfig();
    bc->f_static_variables_Configuration = new SimpleConfiguration< int >( 1 );
    bi->set_BlockConfig( bc );
    }

   // construct the objective of LPBlock
   if( nf < 0 ) {
    GenerateA( 1 , nvar );
 
    ConstructObj( LPBlock );

    #if( LOG_LEVEL >= 4 )
     cout << "L = [ ";
     for( Index j = 0 ; j < nvar ; ++j )
      cout << A[ 0 ][ j ] << " ";
     cout << "]" << endl;
    #endif
    }

   LPBlock->generate_abstract_variables();

   if( wchg & 64 ) {
    // if a finite global bound can be set, construct a static constraint
    // group containing a single FRowConstraint that can be used to set
    // the global bound. this is objective >= bound in the convex case
    // and objective <= bound in the concave one. however, in some pesky
    // MILPSolver this does not work with bound = +/-inf unless the other
    // side of the constraint is (large, negative if necessary but) finite;
    // that is, large_positive >= objective >= bound in the convex case
    // and large_negative <= objective <= bound
    auto lbc = new FRowConstraint();
    lbc->set_lhs( convex ? -INF : - 10 * bound );
    lbc->set_rhs( convex ? 10 * bound : INF );
    LinearFunction::v_coeff_pair vp;
    if( nf < 0 ) {
     auto obj = static_cast< p_LF >( static_cast< FRealObjective * >(
				LPBlock->get_objective() )->get_function() );
     vp = obj->get_v_var();
     }
    Index i = vp.size();
    vp.resize( i + std::abs( nf ) );
    for( Index h = 0 ; h < LPBlock->get_number_nested_Blocks() ; ++h ) {
     auto vh = LPBlock->get_nested_Block( h
		         )->get_static_variable< ColVariable >( "PolyF_v" );
     if( ! vh ) {
      cout << "something very bad happened!" << endl;
      exit( 1 );
      }
     vp[ i++ ] = std::make_pair( vh , 1 );
     }
    lbc->set_function( new LinearFunction( std::move( vp ) ) );
    LPBlock->add_static_constraint( *lbc , "globalbound" );
    }
   }
  else {
   auto & pf = static_cast< p_PFB >( LPBlock )->get_PolyhedralFunction();

   // pass the Variable to the PolyhedralFunction (move the vector)
   pf.set_variables( std::move( vars ) );

   // construct the m x nvar matrix A, the m-vector b, and the bound
   GenerateAb( m , nvar );
   auto BND = GenerateBND();

   #if( LOG_LEVEL >= 4 )
    printAb( A , b , BND );
   #endif

   // pass all the data of the PolyhedralFunction
   pf.set_PolyhedralFunction( std::move( A ) , std::move( b ) , BND ,
			      convex );

   // generate the abstract representation
   SimpleConfiguration< int > cfg( 1 );  // 1 = linearized representation
   LPBlock->generate_abstract_variables( &cfg );
   }

  LPBlock->generate_abstract_constraints();
  LPBlock->generate_objective();
  }

 // construct the "natural" representation- - - - - - - - - - - - - - - - - -
 {
  // ensure all original pointers go out of scope immediately after that
  // the construction has finished

  if( nf ) {
   // contruct the sub-Block (via R3 Block)
   NDOBlock = dynamic_cast< AbstractBlock * >(
		    LPBlock->get_R3_Block( nullptr , new AbstractBlock() ) );
   }
  else
   NDOBlock = dynamic_cast< AbstractBlock * >( LPBlock->get_R3_Block() );

  assert( NDOBlock );  // excess of caution (we know it is)

  // construct the Variable
  auto xNDO = new std::vector< ColVariable >( nsvar );
  PolyhedralFunction::VarVector vars( nvar );
  auto vit = vars.begin();
  for( auto & xi : *xNDO )
   *(vit++) = & xi;
  #if DYNAMIC_VARS > 0
   auto xNDOd = new std::list< ColVariable >( ndvar );
   for( auto & xi : *xNDOd )
    *(vit++) = & xi;
  #endif

  // now set the Variable, Constraint and Objective in the AbstractBlock
  NDOBlock->add_static_variable( *xNDO , "x" );
  #if DYNAMIC_VARS > 0
   NDOBlock->add_dynamic_variable( *xNDOd );
  #endif

  if( nf ) {
   for( Index i = 0 ; i < NDOBlock->get_number_nested_Blocks() ; ++i )
    // pass the Variable to the PolyhedralFunction (copy the vector)
    static_cast< p_PFB >( NDOBlock->get_nested_Block( i ) )->
     get_PolyhedralFunction().set_variables(
				    PolyhedralFunction::VarVector( vars ) );

   // construct the objective of NDOBlock
   if( nf < 0 )
    ConstructObj( NDOBlock );
   }
  else  // pass the Variable to the PolyhedralFunction (move the vector)
   static_cast< p_PFB >( NDOBlock )->get_PolyhedralFunction().set_variables(
							  std::move( vars ) );
  if( convex )
   NDOBlock->set_valid_lower_bound( -bound , true );
  else
   NDOBlock->set_valid_upper_bound( bound , true );

  // generate the abstract representation
  SimpleConfiguration< int > cfg( 0 );  // 0 = natural representation
  NDOBlock->generate_abstract_variables( &cfg );
  NDOBlock->generate_abstract_constraints();
  NDOBlock->generate_objective();
  }

 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 if( nf && ( wchg & 64 ) )  // if a finite global bound is set
  set_global_bound();       // do it now on both :Block

 // define bound constraints- - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 #if HAVE_CONSTRAINTS == 1
 {
  auto LPx = LPBlock->get_static_variable_v< ColVariable >( "x" );
  auto NDOx = NDOBlock->get_static_variable_v< ColVariable >( "x" );
  for( Index i = 0 ; i < nvar ; ++i )
   if( dis( rg ) < 0.5 ) {
    (*LPx)[ i ].is_positive( true , eNoMod );
    (*NDOx)[ i ].is_positive( true , eNoMod );
    }
  }
 #endif
 #if HAVE_CONSTRAINTS == 2
 {
  auto LPx = LPBlock->get_static_variable_v< ColVariable >( "x" );
  auto NDOx = NDOBlock->get_static_variable_v< ColVariable >( "x" );
  auto LPbnd = new std::list< BoxConstraint >;
  auto NDObnd = new std::list< BoxConstraint >;
  for( Index i = 0 ; i < nvar ; ++i )
   if( dis( rg ) < 0.5 ) {
    LPbnd->resize( LPbnd->size() + 1 );
    NDObnd->resize( NDObnd->size() + 1 );
    LPbnd->back().set_variable( & (*LPx)[ i ] );
    NDObnd->back().set_variable( & (*NDOx)[ i ] );
    auto p = dis( rg );
    auto lhs = p < 0.666 ? 0 : -INF;
    auto rhs = p > 0.333 ? dis( rg ) : INF;
    LPbnd->back().set_lhs( lhs , eNoMod );
    NDObnd->back().set_lhs( lhs , eNoMod );
    LPbnd->back().set_rhs( rhs , eNoMod );
    NDObnd->back().set_rhs( rhs , eNoMod );
    }
   else
    if( dis( rg ) < 0.5 ) {
     (*LPx)[ i ].is_positive( true , eNoMod );
     (*NDOx)[ i ].is_positive( true , eNoMod );
     }

  LPBlock->add_dynamic_constraint( *LPbnd , "box" );
  NDOBlock->add_dynamic_constraint( *NDObnd , "box" );
  }
 #endif
 #if HAVE_CONSTRAINTS == 3
 {
  auto LPx = LPBlock->get_static_variable_v< ColVariable >( "x" );
  auto NDOx = NDOBlock->get_static_variable_v< ColVariable >( "x" );
  auto LPbnd = new std::list< FRowConstraint >;
  auto NDObnd = new std::list< BoxConstraint >;
  for( Index i = 0 ; i < nsvar ; ++i )
   if( dis( rg ) < 0.5 ) {
    LinearFunction::v_coeff_pair vars_LP( 1 );
    LinearFunction::v_coeff_pair vars_NDO( 1 );
    vars_LP[ 0 ] = std::make_pair( & (*LPx)[ i ] , 1 );
    LPbnd->resize( LPbnd->size() + 1 );
    NDObnd->resize( NDObnd->size() + 1 );
    LPbnd->back().set_function( new LinearFunction( std::move( vars_LP ) ) );
    NDObnd->back().set_variable( & (*NDOx)[ i ] );
    auto p = dis( rg );
    auto lhs = p < 0.666 ? 0 : -INF;
    auto rhs = p > 0.333 ? dis( rg ) : INF;
    LPbnd->back().set_lhs( lhs , eNoMod );
    NDObnd->back().set_lhs( lhs , eNoMod );
    LPbnd->back().set_rhs( rhs , eNoMod );
    NDObnd->back().set_rhs( rhs , eNoMod );
    }
   else
    if( dis( rg ) < 0.5 ) {
     (*LPx)[ i ].is_positive( true , eNoMod );
     (*NDOx)[ i ].is_positive( true , eNoMod );
     }

  LPBlock->add_dynamic_constraint( *LPbnd , "NObox" );
  NDOBlock->add_dynamic_constraint( *NDObnd , "box" );
  }
 #endif

 // final checks- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 /*!!
 LPBlock->is_correct();
 NDOBlock->is_correct();
 !!*/

 // attach the Solver to the Block- - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // do this by reading appropriate BlockSolverConfig from files and apply()
 // them to the ::Block
 
 auto msc = new BlockSolverConfig;
 {
  ifstream LPParFile( "LPPar.txt" );
  if( ! LPParFile.is_open() ) {
   cerr << "Error: cannot open file LPPar.txt" << endl;
   return( 1 );
   }

  LPParFile >> *( msc );
  LPParFile.close();

  msc->apply( LPBlock );
  msc->clear();

  // for LPBlock, in addition  "manually" attach an UpdateSolver to (each
  // PolyhedralFunctionBlock in) LPBlock
  if( nf )
   for( int i = 0 ; i < LPBlock->get_number_nested_Blocks() ; ++i )
    LPBlock->get_nested_Block( i )->register_Solver(
		       new UpdateSolver( NDOBlock->get_nested_Block( i ) ) );
  else
   LPBlock->register_Solver( new UpdateSolver( NDOBlock ) );
  }
 
 auto bsc = new BlockSolverConfig;
 {
  // for NDOBlock do this by reading appropriate BlockSolverConfig from
  // files and apply() it to the NDOBlock
  ifstream NDOParFile( "NDOPar.txt" );
  if( ! NDOParFile.is_open() ) {
   cerr << "Error: cannot open file NDOPar.txt" << endl;
   return( 1 );
   }

  NDOParFile >> *( bsc );
  NDOParFile.close();

  bsc->apply( NDOBlock );
  bsc->clear();
  }

 // open log-file - - - - - - - - - - -  - - - - - - - - - - - - - - - - - -
 //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 #if( LOG_LEVEL >= 2 )
  #if( LOG_ON_COUT )
   ((NDOBlock->get_registered_solvers()).front())->set_log( &cout );
  #else
   ofstream LOGFile( logF , ofstream::out );
   if( ! LOGFile.is_open() )
    cerr << "Warning: cannot open log file """ << logF << """" << endl;
   else {
    LOGFile.setf( ios::scientific, ios::floatfield );
    LOGFile << setprecision( 10 );
    ((NDOBlock->get_registered_solvers()).front())->set_log( &LOGFile );
    }
  #endif

  #if( LOG_LEVEL >= 3 )
   ((LPBlock->get_registered_solvers()).front())->set_par(
	                          MILPSolver::strOutputFile , "LPBlock.lp" );
  #endif
 #endif

 // first solver call - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 LOG1( "First call: " );

 bool AllPassed = SolveBoth();
 
 // main loop - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // now, for n_repeat times:
 // - up to n_change rows are added
 // - up to n_change rows are deleted
 // - up to n_change rows are modified
 // - up to n_change constants are modified
 // - up to n_change coefficient of linear obj (if any) are modified
 //
 // then the two problems are re-solved
 //
 // IMPORTANT NOTE: only LPBlock is changed, because UpdateSolver takes
 //                 care of intercepting all (physical) Modification and
 //                 map_forward them to NDOBlock
 //
 // if there are multiple PolyhedralFunctionBlock inside LPBlock and
 // NDOBlock, at each iteration only one of them is changed; however, by
 // playing with SKIP_BEAT one can solve the Block after having changed an
 // arbitrary number of them

 for( Index rep = 0 ; rep < n_repeat * ( SKIP_BEAT + 1 ) ; ) {
  if( ! AllPassed )
   break;

  p_PFB LPBr;
  if( nf ) {
   LPBr = static_cast< p_PFB >( LPBlock->get_nested_Blocks()[
						    rep % std::abs( nf ) ] );
   LOG1( rep << " [" << rep % std::abs( nf ) << "]: ");
   }
  else {
   LPBr = static_cast< p_PFB >( LPBlock );
   LOG1( rep << ": ");
   }

  m = LPBr->get_PolyhedralFunction().get_nrows();

  // add rows - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 1 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = Index( dis( rg ) * n_change ) ) {
    LOG1( "added " << tochange << " rows" );

    GenerateAb( tochange , nvar );

    auto cnst = LPBr->get_dynamic_constraint< FRowConstraint >( 0 );

    if( ( wchg & 512 ) && ( dis( rg ) <= p_change ) ) {
     // in 50% of the cases do an "abstract" change
     LOG1( "(a)" );

     ColVariable * vLP;                 // pointer to v LP variable
     std::vector< ColVariable > * xLP;  // pointer to (static) x LP variables
     if( nf ) {
      vLP = LPBr->get_static_variable< ColVariable >( 0 );
      xLP = LPBlock->get_static_variable_v< ColVariable >( 0 );
      }
     else {
      vLP = LPBlock->get_static_variable< ColVariable >( 0 );
      xLP = LPBlock->get_static_variable_v< ColVariable >( 1 );
      }
     #if DYNAMIC_VARS > 0
      auto xLPd = LPBlock->get_dynamic_variable< ColVariable >( 0 );
     #endif

     std::list< FRowConstraint > nc( tochange );
     auto ncit = nc.begin();
     for( Index i = 0 ; i < tochange ; ++i , ++ncit ) {
      // construct constraint ci out of A[ i ] and b[ i ]:
      // the constraint is b[ i ] <= vLP - \sum_j Ai[ j ] * xLP[ j ] <= INF
      //
      // note: constraints are constructed dense (elements == 0, which are
      //       anyway quite unlikely, are ignored) to make things simpler
      //
      // note: variable x[ i ] is given index i + 1, variable v has index 0

      ncit->set_lhs( convex ? b[ i ] : -INF );
      ncit->set_rhs( convex ? INF : b[ i ] );
      LinearFunction::v_coeff_pair vars( nvar + 1 );
      Index j = 0;

      // first, v
      vars[ j ] = std::make_pair( vLP , 1 );

      // then, static x
      for( ; j < nsvar ; ++j )
       vars[ j + 1 ] = std::make_pair( &((*xLP)[ j ] ) , - A[ i ][ j ] );

      #if DYNAMIC_VARS > 0
       // finally, dynamic x
       auto xLPdit = xLPd->begin();
       for( ; j < nvar ; ++j , ++xLPdit )
	vars[ j + 1 ] = std::make_pair( &(*xLPdit) , - A[ i ][ j ] );
      #endif

      ncit->set_function( new LinearFunction( std::move( vars ) ) );
      }

     LPBr->add_dynamic_constraints( *cnst , nc );
     }
    else  // directly change the PolyhedralFunction in LPBlock
     if( tochange == 1 )
      LPBr->get_PolyhedralFunction().add_row( std::move( A[ 0 ] ) , b[ 0 ] );
     else
      LPBr->get_PolyhedralFunction().add_rows( std::move( A ) , b );

    LOG1( " - " );

    // update m
    m += tochange;

    // sanity checks
    PANIC( m == LPBr->get_PolyhedralFunction().get_nrows() );
    PANIC( m == cnst->size() );
    }

  // delete rows- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 2 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = min( m - 1 , Index( dis( rg ) * n_change ) ) ) {
    LOG1( "deleted " << tochange << " rows" );

    auto cnst = LPBr->get_dynamic_constraint< FRowConstraint >( 0 );

    if( dis( rg ) <= 0.5 ) {  // in 50% of the cases do a ranged change
    
     Index strt = dis( rg ) * ( m - tochange );
     Index stp = strt + tochange;

     if( ( wchg & 512 ) && ( dis( rg ) <= p_change ) ) {
      // in 50% of the cases do an "abstract" change
      LOG1( "(r,a) - " );
      if( tochange == 1 )
       LPBr->remove_dynamic_constraint( *cnst , std::next( cnst->begin() ,
							   strt ) );
      else
       LPBr->remove_dynamic_constraints( *cnst , Range( strt , stp ) );
      }
     else {  // directly change the PolyhedralFunction
      LOG1( "(r) - " );
      if( tochange == 1 )
       LPBr->get_PolyhedralFunction().delete_row( strt );
      else
       LPBr->get_PolyhedralFunction().delete_rows( Range( strt , stp ) );
      }
     }
    else {  // in the other 50% of the cases, do a sparse change
     Subset nms = GenerateSubset( m , tochange );

     // remove them from the LP
     if( ( wchg & 512 ) && ( dis( rg ) <= p_change ) ) {
      // in 50% of the cases do an "abstract" change
      LOG1( "(s,a) - " );
      if( tochange == 1 )
       LPBr->remove_dynamic_constraint( *cnst , std::next( cnst->begin() ,
							   nms[ 0 ] ) );
      else
       LPBr->remove_dynamic_constraints( *cnst , std::move( nms ) , true );
      }
     else {  // directly change the PolyhedralFunction
      LOG1( "(s) - " );
      if( tochange == 1 )
       LPBr->get_PolyhedralFunction().delete_row( nms[ 0 ] );
      else
       LPBr->get_PolyhedralFunction().delete_rows( std::move( nms ) );
      }
     }

    // update m
    m -= tochange;

    // sanity checks
    PANIC( m == LPBr->get_PolyhedralFunction().get_nrows() );
    PANIC( m == cnst->size() );
    }

  // modify rows- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 4 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = std::min( m , Index( dis( rg ) * n_change ) ) ) {
    LOG1( "modified " << tochange << " rows" );

    GenerateAb( tochange , nvar );

    if( dis( rg ) <= 0.5 ) {  // in 50% of the cases do a ranged change
     Index strt = dis( rg ) * ( m - tochange );
     Index stp = strt + tochange;

     if( ( wchg & 512 ) && ( dis( rg ) <= p_change ) ) {
      // in 50% of the cases do an "abstract" change
      LOG1( "(r,a) - " );

      // modify them in the LP
      ColVariable * vLP;                 // pointer to v LP variable
      std::vector< ColVariable > * xLP;  // pointer to (static) x LP variables
      if( nf ) {
       vLP = LPBr->get_static_variable< ColVariable >( 0 );
       xLP = LPBlock->get_static_variable_v< ColVariable >( 0 );
       }
      else {
       vLP = LPBlock->get_static_variable< ColVariable >( 0 );
       xLP = LPBlock->get_static_variable_v< ColVariable >( 1 );
       }
      #if DYNAMIC_VARS > 0
       auto xLPd = LPBlock->get_dynamic_variable< ColVariable >( 0 );
      #endif
      auto cnst = LPBr->get_dynamic_constraint< FRowConstraint >( 0 );

      // send all the Modification to the same channel
      auto chnl = LPBr->open_channel();
      auto iAM = Observer::make_par( eModBlck , chnl );

      auto cit = std::next( cnst->begin() , strt );
      for( Index i = 0 ; i < tochange ; ++i )
       ChangeLPConstraint( i++ , *(cit++) , iAM );

      LPBr->close_channel( chnl );
      }
     else {  // directly change the PolyhedralFunction
      LOG1( "(r) - " );
      if( tochange == 1 )
       LPBr->get_PolyhedralFunction().modify_row( strt ,
						  std::move( A[ 0 ] ) ,
						  b[ 0 ] );
      else
       LPBr->get_PolyhedralFunction().modify_rows( std::move( A ) , b ,
						   Range( strt , stp ) );
      }
     }
    else {  // in the other 50% of the cases, do a sparse change
     Subset nms = GenerateSubset( m , tochange );

     if( ( wchg & 512 ) && ( dis( rg ) <= p_change ) ) {
      // in 50% of the cases do an "abstract" change
      LOG1( "(s,a) - " );

      // modify them in the LP
      ColVariable * vLP;                 // pointer to v LP variable
      std::vector< ColVariable > * xLP;  // pointer to (static) x LP variables
      if( nf ) {
       vLP = LPBr->get_static_variable< ColVariable >( 0 );
       xLP = LPBlock->get_static_variable_v< ColVariable >( 0 );
       }
      else {
       vLP = LPBlock->get_static_variable< ColVariable >( 0 );
       xLP = LPBlock->get_static_variable_v< ColVariable >( 1 );
       }
      #if DYNAMIC_VARS > 0
       auto xLPd = LPBlock->get_dynamic_variable< ColVariable >( 0 );
      #endif
      auto cnst = LPBr->get_dynamic_constraint< FRowConstraint >( 0 );

      // send all the Modification to the same channel
      auto chnl = LPBr->open_channel();
      auto iAM = Observer::make_par( eModBlck , chnl );

      Index prev = 0;
      auto cit = cnst->begin();
      for( Index i = 0 ; i < tochange ; ++i ) {
       cit = std::next( cit , nms[ i ] - prev );
       prev = nms[ i ];
       ChangeLPConstraint( i , *cit , iAM );
       }

      LPBr->close_channel( chnl );
      }
     else {  // directly change the PolyhedralFunction
      LOG1( "(s) - " );
      if( tochange == 1 )
       LPBr->get_PolyhedralFunction().modify_row( nms[ 0 ] ,
						  std::move( A[ 0 ] ) ,
						  b[ 0 ] );
      else
       LPBr->get_PolyhedralFunction().modify_rows( std::move( A ) , b ,
						   std::move( nms ) , true );
      }
     }
    }

  // modify constants - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 8 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = std::min( m , Index( dis( rg ) * n_change ) ) ) {
    LOG1( "modified " << tochange << " constants" );

    Generateb( tochange );

    if( dis( rg ) <= 0.5 ) {  // in 50% of the cases do a ranged change
     Index strt = dis( rg ) * ( m - tochange );
     Index stp = strt + tochange;

     if( ( wchg & 512 ) && ( dis( rg ) <= p_change ) ) {
      // in 50% of the cases do an "abstract" change
      LOG1( "(r,a) - " );

      auto cnst = LPBr->get_dynamic_constraint< FRowConstraint >( 0 );

      // send all the Modification to the same channel
      auto chnl = LPBr->open_channel();
      auto iAM = Observer::make_par( eModBlck , chnl );

      auto cit = std::next( cnst->begin() , strt );
      if( convex )
       for( Index i = 0 ; i < tochange ; )
	(cit++)->set_lhs( b[ i++ ] , iAM );
      else
       for( Index i = 0 ; i < tochange ; )
	(cit++)->set_rhs( b[ i++ ] , iAM );

      LPBr->close_channel( chnl );
      }
     else {  // directly change the PolyhedralFunction
      LOG1( "(r) - " );
      if( tochange == 1 )
       LPBr->get_PolyhedralFunction().modify_constant( strt , b[ 0 ] );
      else
       LPBr->get_PolyhedralFunction().modify_constants( b ,
							Range( strt , stp ) );
      }
     }
    else {  // in the other 50% of the cases, do a sparse change
     Subset nms = GenerateSubset( m , tochange );

     if( ( wchg & 512 ) && ( dis( rg ) <= p_change ) ) {
      // in 50% of the cases do an "abstract" change
      LOG1( "(s,a) - " );

      auto cnst = LPBr->get_dynamic_constraint< FRowConstraint >( 0 );

      // send all the Modification to the same channel
      auto chnl = LPBr->open_channel();
      auto iAM = Observer::make_par( eModBlck , chnl );

      Index prev = 0;
      auto cit = cnst->begin();
      if( convex )
       for( Index i = 0 ; i < tochange ; ) {
	cit = std::next( cit , nms[ i ] - prev );
	prev = nms[ i ];
	cit->set_lhs( b[ i++ ] , iAM );
        }
      else
       for( Index i = 0 ; i < tochange ; ) {
	cit = std::next( cit , nms[ i ] - prev );
	prev = nms[ i ];
	cit->set_rhs( b[ i++ ] , iAM );
        }

      LPBr->close_channel( chnl );
      }
     else {  // directly change the PolyhedralFunction
      LOG1( "(s) - " );
      if( tochange == 1 )
       LPBr->get_PolyhedralFunction().modify_constant( nms[ 0 ] , b[ 0 ] );
      else
       LPBr->get_PolyhedralFunction().modify_constants( b , std::move( nms ) ,
							true );
      }
     }
    }

  // modify local bounds- - - - - - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 16 ) && ( dis( rg ) <= p_change ) ) {
   LOG1( "modified bound" );

   auto BND = GenerateBND();

   if( ( wchg & 512 ) && ( dis( rg ) <= p_change ) ) {
    // in 50% of the cases do an "abstract" change
    LOG1( "(a)" );

    if( convex )
     LPBr->get_static_constraint< BoxConstraint >( 0 )->set_lhs( BND );
    else
     LPBr->get_static_constraint< BoxConstraint >( 0 )->set_rhs( BND );
    }
   else  // directly change the PolyhedralFunction
    LPBr->get_PolyhedralFunction().modify_bound( BND );

   LOG1( " - " );
   }

  // modify linear objective- - - - - - - - - - - - - - - - - - - - - - - - -
  // ... if there is any, of course

  if( ( nf < 0 ) && ( wchg & 32 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = Index( dis( rg ) * std::min( nvar , n_change ) ) ) {
    LOG1( "changed " << tochange << " objective coeff." );

    GenerateA( 1 , tochange );

    p_LF lf = nullptr;
    if( wchg & 64 ) {
     // if the constraint "-INF <= objective <= INF" is there, it must be
     // changed accordingly, too
     if( auto lbc = LPBlock->get_static_constraint< FRowConstraint >(
							    "globalbound" ) )
      lf = static_cast< p_LF >( lbc->get_function() );
     else {
      cout << "something very bad happened!" << endl;
      exit( 1 );
      }
     }

    auto LPLF = static_cast< p_LF >(
	    ( LPBlock->get_objective< FRealObjective >() )->get_function() );
    auto NDOLF = static_cast< p_LF >(
	   ( NDOBlock->get_objective< FRealObjective >() )->get_function() );

    if( dis( rg ) <= 0.5 ) {  // in 50% of the cases do a ranged change
     Index strt = dis( rg ) * ( nvar - tochange );
     Index stp = strt + tochange;

     if( tochange == 1 ) {
      if( lf )
       lf->modify_coefficient( strt , A[ 0 ][ 0 ] );
      LPLF->modify_coefficient( strt , A[ 0 ][ 0 ] );
      NDOLF->modify_coefficient( strt , A[ 0 ][ 0 ] );
      }
     else {
      if( lf )
       lf->modify_coefficients( RealVector( A[ 0 ] ) , Range( strt , stp ) );
      LPLF->modify_coefficients( RealVector( A[ 0 ] ) , Range( strt , stp ) );
      NDOLF->modify_coefficients( std::move( A[ 0 ] ) , Range( strt , stp ) );
      }
      
     LOG1( "(r) - " );
     }
    else {  // in the other 50% of the cases, do a sparse change
     Subset nms = GenerateSubset( nvar , tochange );

     if( tochange == 1 ) {
      if( lf )
       lf->modify_coefficient( nms.front() , A[ 0 ][ 0 ] );
      LPLF->modify_coefficient( nms.front() , A[ 0 ][ 0 ] );
      NDOLF->modify_coefficient( nms.front() , A[ 0 ][ 0 ] );
      }
     else {
      if( lf )
       lf->modify_coefficients( RealVector( A[ 0 ] ) , Subset( nms ) ,
				true );
      LPLF->modify_coefficients( RealVector( A[ 0 ] ) , Subset( nms ) ,
				 true );
      NDOLF->modify_coefficients( std::move( A[ 0 ] ) , std::move( nms ) ,
				  true );
      }

     LOG1( "(s) - " );
     }
    }
  
  // modify global bound- - - - - - - - - - - - - - - - - - - - - - - - - - -

  if( nf && ( wchg & 64 ) && ( dis( rg ) <= p_change ) ) {
   LOG1( "changed global bound - " );

   set_global_bound();
   }

  // add variables- - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  #if DYNAMIC_VARS > 0
  if( ( wchg & 128 ) && ( dis( rg ) <= p_change ) ) {
   Index tochange = Index( dis( rg ) * n_change );
   if( tochange ) {
    LOG1( "added " << tochange << " variables - " );

    throw( std::logic_error( "adding variables not implemented yet" ) );

    GenerateA( m , tochange );

    // add them in the LP, *copying* the data
    std::list< ColVariable > nxLPd( tochange );
    std::vector< Variable * > nxpLP( tochange );
    auto nxit = nxLPd.begin();
    for( Index i = 0 ; i < tochange ; )
     nxpLP[ i++ ] = &(*(nxit++));

    LPBlock->add_dynamic_variables(
	      *(LPBlock->get_dynamic_variable< ColVariable >( 0 )) , nxLPd );

    if( tochange == 1 )
     LPBlock->get_PolyhedralFunction().add_variable( nxpLP[ 0 ] , A[ 0 ] );
    else
     LPBlock->get_PolyhedralFunction().add_variables( std::move( nxpLP ) ,
						      MultiVector( A ) );

    // add them in the NDO, letting the data go
    std::list< ColVariable > nxNDOd( tochange );
    std::vector< Variable * > nxpNDO( tochange );
     auto nxit = nxNDOd.begin();
    for( Index i = 0 ; i < tochange ; )
     nxpNDO[ i++ ] = &(*(nxit++));

    NDOBlock->add_dynamic_variables(
	    *(NDOBlock->get_dynamic_variable< ColVariable >( 0 )) , nxNDOd );

    if( tochange == 1 )
     NDOBlock->get_PolyhedralFunction().add_variable( nxpNDO[ 0 ] , A[ 0 ] );
    else
     NDOBlock->get_PolyhedralFunction().add_variables( std::move( nxpNDO ) ,
						       std::move( A ) );

    // update ndvar
    ndvar += tochange;
    }
   }

  // remove variables - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 512 ) && ( dis( rg ) <= p_change ) ) {
   Index tochange = min( ndvar , Index( dis( rg ) * n_change ) );
   if( tochange ) {
    LOG1( "removed " << tochange << " variables" );

    throw( std::logic_error( "removing variables not implemented yet" ) );

    // in 50% of the cases do a ranged change, in the others a sparse change
    if( dis( rg ) <= 0.5 ) {
     LOG1( "(r) - " );

     Index strt = dis( rg ) * ( ndvar - tochange );
     Index stp = strt + tochange;

     // remove them from the LP
     auto xLPd = NDOBlock->get_dynamic_variable< ColVariable >( 0 );
     if( tochange == 1 )
      LPBlock->get_PolyhedralFunction().remove_variable( strt );
     else
      LPBlock->get_PolyhedralFunction().remove_variables( Range( strt ,
								 stp ) );

     LPBlock->remove_dynamic_variables( *xLPd , Range( strt , stp ) );

     // remove them from the NDO
     auto xNDOd = NDOBlock->get_dynamic_variable< ColVariable >( 0 );
     if( tochange == 1 )
      NDOBlock->get_PolyhedralFunction().remove_variable( strt );
     else
      NDOBlock->get_PolyhedralFunction().remove_variables( Range( strt ,
								  stp ) );

     NDOBlock->remove_dynamic_variables( *xNDOd , Range( strt , stp ) );
     }
    else {
     LOG1( "(s) - " );
     Subset nms = GenerateSubset( ndvar , tochange );

     // remove them from the LP, *copying* names
     auto xLPd = LPBlock->get_dynamic_variable< ColVariable >( 0 );
     if( tochange == 1 ) {
      LPBlock->get_PolyhedralFunction().remove_variable( nms[ 0 ] );
      auto vp = &(*std::next( xLPd->begin() , nms[ 0 ] ));
      LPBlock->remove_dynamic_variable( *xLPd , vp );
      }
     else {
      LPBlock->get_PolyhedralFunction().remove_variables( Subset( nms ) );
      LPBlock->remove_dynamic_variables( *xLPd , Subset( nms ) );
      }

     // remove them from the NDO, finally letting names go
     auto xNDOd = NDOBlock->get_dynamic_variable< ColVariable >( 0 );
     if( tochange == 1 ) {
      NDOBlock->get_PolyhedralFunction().remove_variable( nms[ 0 ] );
      auto vp = &(*std::next( xNDOd->begin() , nms[ 0 ] ));
      NDOBlock->remove_dynamic_variable( *xNDOd , vp );
      }
     else {
      NDOBlock->get_PolyhedralFunction().remove_variables( Subset( nms ) );
      NDOBlock->remove_dynamic_variables( *xNDOd , std::move( nms ) );
      }
     }

    // update ndvar
    ndvar -= tochange;
    }
   }

  #endif  // DYNAMIC_VARS > 0

  // if verbose, print out stuff- - - - - - - - - - - - - - - - - - - - - - -

  #if( LOG_LEVEL >= 3 )
   ((LPBlock->get_registered_solvers()).front())->set_par(
		                     MILPSolver::strOutputFile , "LPBlock-" +
		                     std::to_string( rep ) + ".lp" );
   #if( LOG_LEVEL >= 4 )
    cout << endl << "LPBlock-PF: ";
    auto PF = & LPBr->get_PolyhedralFunction();
    printAb( PF->get_A() , PF->get_b() , convex
	     ? PF->get_global_lower_bound()
	     : PF->get_global_upper_bound() );
    p_PFB NDOBr;
    if( nf )
     NDOBr = static_cast< p_PFB >( NDOBlock->get_nested_Blocks()[
						    rep % std::abs( nf ) ] );
    else
     NDOBr = static_cast< p_PFB >( NDOBlock );
    cout << "NDOBlock-PF: ";
    PF = & NDOBr->get_PolyhedralFunction();
    printAb( PF->get_A() , PF->get_b() , convex
	     ? PF->get_global_lower_bound()
	     : PF->get_global_upper_bound() );
   #endif
  #endif

  // finally, re-solve the problems- - - - - - - - - - - - - - - - - - - - -
  // ... every SKIP_BEAT + 1 rounds

  if( ! ( ++rep % ( SKIP_BEAT + 1 ) ) )
   AllPassed &= SolveBoth();
  #if( LOG_LEVEL >= 1 )
  else
   cout << endl;
  #endif

  }  // end( main loop )- - - - - - - - - - - - - - - - - - - - - - - - - - -
     // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 if( AllPassed )
  cout << GREEN( All test passed!! ) << endl;
 else
  cout << RED( Shit happened!! ) << endl;
 
 // cleanup - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // unregister (and delete) all Solvers attached to the Blocks: do this by
 // apply()-ing the clear()-ed BlockSolverConfig, then delete them

 bsc->apply( NDOBlock );
 delete( bsc );

 // for LPBlock, before  "manually" un-register (and delete) the
 // UpdateSolver from (each PolyhedralFunctionBlock in) LPBlock
 if( nf )
  for( int i = 0 ; i < LPBlock->get_number_nested_Blocks() ; ++i ) {
   auto bi = LPBlock->get_nested_Block( i );
   bi->unregister_Solver( bi->get_registered_solvers().back() , true );
   }
 else
  LPBlock->unregister_Solver( LPBlock->get_registered_solvers().back() ,
			      true );
 msc->apply( LPBlock );
 delete( msc );

 // delete the Blocks
 delete( NDOBlock );
 delete( LPBlock );

 // terminate - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 return( AllPassed ? 0 : 1 );

 }  // end( main )

/*--------------------------------------------------------------------------*/
/*------------------------ End File test.cpp -------------------------------*/
/*--------------------------------------------------------------------------*/
