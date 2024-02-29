/*--------------------------------------------------------------------------*/
/*-------------------------- File test.cpp ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Main for testing PolyhedralFunction
 *
 * A "random" PolyhedralFunction is constructed and put as the only Objective
 * of an otherwise "empty" Block. The same PolyhedralFunction is represented
 * in terms of linear inequalities for another otherwise "empty" Block. The
 * two Block are solved by a NDO Solver and a LP Solver, respectively, and
 * the results are compared. The two Block are then repeatedly randomly
 * modified "in the same way", and re-solved several times.
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
//
// note: to always save the LP file with the same name it would be enough to
//       directly set strOutputFile in the configuration file, but the
//       tester rather saves the LP file of each iteration i in a different
//       LPBlock-<i>.lp file, which cannot be done with just the config file

#if( LOG_LEVEL >= 1 )
 #define LOG1( x ) cout << x
 #define CLOG1( y , x ) if( y ) cout << x

 #if( LOG_LEVEL >= 2 )
  #define LOG_ON_COUT 1
  // if nonzero, the NDO Solver log is sent on cout rather than on a file
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

#define HAVE_CONSTRAINTS 2

/*--------------------------------------------------------------------------*/

// if nonzero, we are considering only variables with finite bound.
// This is because some *MILPSolver (e.g. SCIPMILPSolver) could have 
// some problems with interior point method in the case of unbounded variables.
// NOTE: At the moment, it can be nonzero only with HAVE_CONSTRAINT = 2
#define BOUND_FINITE 0

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

/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <fstream>
#include <sstream>
#include <iomanip>

#include <random>

#include "AbstractBlock.h"

#include "BlockSolverConfig.h"

#include "FRealObjective.h"

#include "FRowConstraint.h"

#if( LOG_LEVEL >= 3 )
 #include "MILPSolver.h"
#endif

#include "LinearFunction.h"

#include "OneVarConstraint.h"

#include "PolyhedralFunction.h"

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace std;

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*-------------------------------- TYPES -----------------------------------*/
/*--------------------------------------------------------------------------*/

using Index = Block::Index;
using c_Index = Block::c_Index;

using Range = Block::Range;
using c_Range = Block::c_Range;

using Subset = Block::Subset;
using c_Subset = Block::c_Subset;

using FunctionValue = Function::FunctionValue;
using c_FunctionValue = Function::c_FunctionValue;

using MultiVector = PolyhedralFunction::MultiVector;
using RealVector = PolyhedralFunction::RealVector;

using p_LF = LinearFunction *;
using p_PF = PolyhedralFunction *;

/*--------------------------------------------------------------------------*/
/*------------------------------- CONSTANTS --------------------------------*/
/*--------------------------------------------------------------------------*/

const double scale = 10;
const char *const logF = "log.bn";

const FunctionValue INF = SMSpp_di_unipi_it::Inf< FunctionValue >();

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/

AbstractBlock * LPBlock;   // the problem expressed as an LP

AbstractBlock * NDOBlock;  // the problem expressed via PolyhedralFunction

bool convex = true;        // true if the PolyhedralFunction is convex

double bound = 1000;       // a tentative bound to detect unbounded instances

FunctionValue BND;         // the bound in the PolyhedralFunction (if any)

Index nvar = 10;           // number of variables
#if DYNAMIC_VARS > 0
 Index nsvar;              // number of static variables
 Index ndvar;              // number of dynamic variables
#else
 #define nsvar nvar        // all variables are static
#endif

Index m;                   // number of rows

std::mt19937 rg;           // base random generator
std::uniform_real_distribution<> dis( 0.0 , 1.0 );

MultiVector A;
RealVector b;

ColVariable * vLP;                 // pointer to v LP variable

std::vector< ColVariable > * xLP;  // pointer to (static) x LP variables
#if DYNAMIC_VARS > 0
 std::list< ColVariable > * xLPd;  // pointer to (dynamic) x LP variables
#endif

#if HAVE_CONSTRAINTS == 2
 std::list< BoxConstraint > * LPbnd;   // BoxConstraint for LPBlock
 std::list< BoxConstraint > * NDObnd;  // BoxConstraint for NDOBlock
#endif
#if HAVE_CONSTRAINTS == 3
 std::list< FRowConstraint > * LPbnd;   // FRowConstrait for LPBlock
 std::list< BoxConstraint > * NDObnd;  // BoxConstraint for NDOBlock
#endif

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
 // return a random number between 0.5 and 2, with 50% probability of being
 // < 1
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

static void GenerateBND( void )
{
 // rationale: we expect the solution x^* to have entries ~= 1 (in absolute
 // value, and the coefficients of A are <= scale (in absolute value), so
 // the LHS should be at most around - scale * nvar; the RHS can add it
 // a further - scale * nvar / 4, so we expect - (5/4) * scale * nvar to
 // be a "natural" LB. We therefore set the LB to a mean of 1/2 of that
 // (tight) 33% of the time, a mean of 2 times that (loose) 33% of the time,
 // and -INF the rest

 if( dis( rg ) <= 0.333 ) {   // "tight" bound
  BND = rs( dis( rg ) * 5 * scale * nvar / 4 );
  return;
  }

 if( dis( rg ) <= 0.333 ) {  // "loose" bound
  BND = rs( dis( rg ) * 5 * scale * nvar );
  return;
  }

 BND = INF;
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

static void ConstructLPConstraint( Index i , FRowConstraint & ci ,
				   bool setblock = true )
{
 // construct constraint ci out of A[ i ] and b[ i ]:
 //
 // in the convex case, the constraint is
 //
 //          b[ i ] <= vLP - \sum_j Ai[ j ] * xLP[ j ] <= INF
 //
 // in the concave case, the constraint is
 //
 //          -INF <= vLP - \sum_j Ai[ j ] * xLP[ j ] <= b[ i ]
 //
 // note: constraints are constructed dense (elements == 0, which are
 //       anyway quite unlikely, are ignored) to make things simpler
 //
 // note: variable x[ i ] is given index i + 1, variable v has index 0

 if( convex ) {
  ci.set_lhs( b[ i ] );
  ci.set_rhs( INF );
  }
 else {
  ci.set_lhs( -INF );
  ci.set_rhs( b[ i ] );
  }
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

 ci.set_function( new LinearFunction( std::move( vars ) ) );
 if( setblock )
  ci.set_Block( LPBlock );
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

static void SetGlobalBound( void )
{
 if( BND == INF )
  if( convex )
   NDOBlock->set_valid_lower_bound( -bound , true );
  else
   NDOBlock->set_valid_upper_bound( bound , true );
 else
  if( convex )
   NDOBlock->set_valid_lower_bound( -BND );
  else
   NDOBlock->set_valid_upper_bound( BND );
 }

/*--------------------------------------------------------------------------*/

#if HAVE_CONSTRAINTS > 0

static inline void SetNN( ColVariable & LPxi , ColVariable & NDOxi )
{ 
 if( dis( rg ) < 0.5 ) {
  LPxi.is_positive( true , eNoMod );
  NDOxi.is_positive( true , eNoMod );
  }
 }

/*--------------------------------------------------------------------------*/

#if DYNAMIC_VARS > 0

static void RemoveBox( AbstractBlock & AB , Range rng )
{
 // the dynamic variable from the "xd" group in the Range are removed: if
 // anything is "active" in those is a BoxConstraint from the "xbnd" group
 // that has to be removed as well

 auto xd = AB.get_dynamic_variable< ColVariable >( "xd" );
 auto & box = *(AB.get_dynamic_constraint< BoxConstraint >( "xbnd" ));
 std::vector< typename std::list< BoxConstraint >::iterator > rmvd;
 auto it = std::next( xd->begin() , rng.first );
 for( Index i = rng.first ; i < rng.second ; ++i , ++it ) {
  if( ! it->get_num_active() )
   continue;
  if( it->get_num_active() != 1 ) {
   cout << "Too much stuff active in to-be-deleted Variable" << endl;
   exit( 1 );
   }
  auto bc = dynamic_cast< BoxConstraint * >( it->get_active( 0 ) );
  if( ! bc ) {
   cout << "Unexpected stuff active in to-be-deleted Variable" << endl;
   exit( 1 );
   }
  auto it = std::find_if( box.begin() , box.end() ,
			  [ bc ]( BoxConstraint & x ) {
			   return( & x == bc );
			   } );
  if( it == box.end() ) {
   cout << "BoxConstraint not found" << endl;
   exit( 1 );
   }
  rmvd.push_back( it );
  }

 if( ! rmvd.empty() )
  AB.remove_dynamic_constraints( box , rmvd ); 
 }

/*--------------------------------------------------------------------------*/

static void RemoveBox( AbstractBlock & AB , const Subset & sbst )
{
 // the dynamic variable from the "xd" group in the (ordered) Subset are
 // removed: if anything is "active" in those is a BoxConstraint from the
 // "xbnd" group that has to be removed as well

 auto xd = AB.get_dynamic_variable< ColVariable >( "xd" );
 auto & box = *(AB.get_dynamic_constraint< BoxConstraint >( "xbnd" ));
 std::vector< typename std::list< BoxConstraint >::iterator > rmvd;
 Index prev = 0;
 auto it = xd->begin();
 for( auto ind : sbst ) {
  it = std::next( it , ind - prev );
  prev = ind;
  if( ! it->get_num_active() )
   continue;
  if( it->get_num_active() != 1 ) {
   cout << "Too much stuff active in to-be-deleted Variable" << endl;
   exit( 1 );
   }
  auto bc = dynamic_cast< BoxConstraint * >( it->get_active( 0 ) );
  if( ! bc ) {
   cout << "Unexpected stuff active in to-be-deleted Variable" << endl;
   exit( 1 );
   }
  auto it = std::find_if( box.begin() , box.end() ,
			  [ bc ]( BoxConstraint & x ) {
			   return( & x == bc );
			   } );
  if( it == box.end() ) {
   cout << "BoxConstraint not found" << endl;
   exit( 1 );
   }
  rmvd.push_back( it );
  }

 if( ! rmvd.empty() )
  AB.remove_dynamic_constraints( box , rmvd ); 
 }

/*--------------------------------------------------------------------------*/

#endif // DYNAMIC_VARS > 0

#if HAVE_CONSTRAINTS == 2

static inline void SetBox( ColVariable & LPxi , ColVariable & NDOxi )
{
 if( dis( rg ) < 0.5 || BOUND_FINITE == 1 ) {
  LPbnd->resize( LPbnd->size() + 1 );
  NDObnd->resize( NDObnd->size() + 1 );
  LPbnd->back().set_variable( & LPxi );
  NDObnd->back().set_variable( & NDOxi );
  auto p = dis( rg );
  double lhs, rhs;
  #if BOUND_FINITE == 1
    lhs = 0;
    rhs = p;
  #else
    lhs = p < 0.666 ? 0 : -INF;
    rhs = p > 0.333 ? dis( rg ) : INF;
  #endif
  LPbnd->back().set_lhs( lhs , eNoMod );
  NDObnd->back().set_lhs( lhs , eNoMod );
  LPbnd->back().set_rhs( rhs , eNoMod );
  NDObnd->back().set_rhs( rhs , eNoMod );
  }
 else
  SetNN( LPxi , NDOxi );
 }

/*--------------------------------------------------------------------------*/

#endif // HAVE CONSTRAINT == 2

#if HAVE_CONSTRAINTS == 3

static inline void SetFRow_Box( ColVariable & LPxi , ColVariable & NDOxi )
{
 if( dis( rg ) < 0.5 ) {
  LPbnd->resize( LPbnd->size() + 1 );
  NDObnd->resize( NDObnd->size() + 1 );
  LinearFunction::v_coeff_pair vars_LP( 1 );
  vars_LP[ 0 ] = std::make_pair( & LPxi , 1 );
  LPbnd->back().set_function( new LinearFunction( std::move( vars_LP ) ) );
  NDObnd->back().set_variable( & NDOxi );
  auto p = dis( rg );
  auto lhs = p < 0.666 ? 0 : -INF;
  auto rhs = p > 0.333 ? dis( rg ) : INF;
  LPbnd->back().set_lhs( lhs , eNoMod );
  NDObnd->back().set_lhs( lhs , eNoMod );
  LPbnd->back().set_rhs( rhs , eNoMod );
  NDObnd->back().set_rhs( rhs , eNoMod );
  }
 else
  SetNN( LPxi , NDOxi );
 }

/*--------------------------------------------------------------------------*/

#if DYNAMIC_VARS > 0

static void RemoveFRow( AbstractBlock & AB , Range rng )
{
 // the dynamic variable from the "xd" group in the Range are removed: if
 // anything is "active" in those is a FRowConstraint from the "xbnd" group
 // that has to be removed as well

 auto xd = AB.get_dynamic_variable< ColVariable >( "xd" );
 auto & frow = *(AB.get_dynamic_constraint< FRowConstraint >( "xbnd" ));
 std::vector< typename std::list< FRowConstraint >::iterator > rmvd;
 auto it = std::next( xd->begin() , rng.first );
 for( Index i = rng.first ; i < rng.second ; ++i , ++it ) {
  if( ! it->get_num_active() )
   continue;
  if( it->get_num_active() != 1 ) {
   cout << "Too much stuff active in to-be-deleted Variable" << endl;
   exit( 1 );
   }
  auto bc = dynamic_cast< FRowConstraint * >( it->get_active( 0 ) );
  if( ! bc ) {
   cout << "Unexpected stuff active in to-be-deleted Variable" << endl;
   exit( 1 );
   }
  auto it = std::find_if( frow.begin() , frow.end() ,
			  [ bc ]( FRowConstraint & x ) {
			   return( & x == bc );
			   } );
  if( it == frow.end() ) {
   cout << "FRowConstraint not found" << endl;
   exit( 1 );
   }
  rmvd.push_back( it );
  }

 if( ! rmvd.empty() )
  AB.remove_dynamic_constraints( frow , rmvd ); 
 }

/*--------------------------------------------------------------------------*/

static void RemoveFRow( AbstractBlock & AB , const Subset & sbst )
{
 // the dynamic variable from the "xd" group in the (ordered) Subset are
 // removed: if anything is "active" in those is a FRowConstraint from the
 // "xbnd" group that has to be removed as well

 auto xd = AB.get_dynamic_variable< ColVariable >( "xd" );
 auto & frow = *(AB.get_dynamic_constraint< FRowConstraint >( "xbnd" ));
 std::vector< typename std::list< FRowConstraint >::iterator > rmvd;
 Index prev = 0;
 auto it = xd->begin();
 for( auto ind : sbst ) {
  it = std::next( it , ind - prev );
  prev = ind;
  if( ! it->get_num_active() )
   continue;
  if( it->get_num_active() != 1 ) {
   cout << "Too much stuff active in to-be-deleted Variable" << endl;
   exit( 1 );
   }
  auto bc = dynamic_cast< FRowConstraint * >( it->get_active( 0 ) );
  if( ! bc ) {
   cout << "Unexpected stuff active in to-be-deleted Variable" << endl;
   exit( 1 );
   }
  auto it = std::find_if( frow.begin() , frow.end() ,
			  [ bc ]( FRowConstraint & x ) {
			   return( & x == bc );
			   } );
  if( it == frow.end() ) {
   cout << "FRowConstraint not found" << endl;
   exit( 1 );
   }
  rmvd.push_back( it );
  }

 if( ! rmvd.empty() )
  AB.remove_dynamic_constraints( frow , rmvd ); 
 }

/*--------------------------------------------------------------------------*/

#endif // DYNAMIC_VARS > 0

#endif // HAVE_CONSTRAINT == 3

#endif // HAVE_CONSTRAINT > 0

/*--------------------------------------------------------------------------*/

static void printAb( const MultiVector & tA , const RealVector & tb ,
		     double bound )
{
 PANIC( ( tA.size() == tb.size() ) || ( tA.size() + 1 == tb.size() ) );
 PANIC( tA.size() == m );
 for( auto & tai : tA )
  PANIC( tai.size() == nvar );

 cout << "n = " << nvar << ", m = " << m;
 if( std::abs( bound ) == INF )
  cout << " (no bound)" << endl;
 else
  cout << ", bound = " << bound << endl;

 for( Index i = 0 ; i < m ; ++i ) {
  cout << "A[ " << i << " ] = [ ";
  for( Index j = 0 ; j < nvar ; ++j )
   cout << tA[ i ][ j ] << " ";
   cout << "], b[ " << i << " ] = " << tb[ i ] << endl;
  }
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
   if( ( convex && ( foNDO <= bound * ( 1 + 1e-9 ) ) ) ||
       ( ( ! convex ) && ( foNDO >= bound * ( 1 + 1e-9 ) ) ) ) {
    LOG1( "OK(?bound?)" << endl );
    bound *= 2;
    if( convex )
     NDOBlock->set_valid_lower_bound( -bound );
    else
     NDOBlock->set_valid_upper_bound( bound );
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
 Index wchg = 127;
 double dens = 4;  
 double p_change = 0.5;
 Index n_change = 10;
 Index n_repeat = 40;

 switch( argc ) {
  case( 8 ): Str2Sthg( argv[ 7 ] , p_change );
  case( 7 ): Str2Sthg( argv[ 6 ] , n_change );
  case( 6 ): Str2Sthg( argv[ 5 ] , n_repeat );
  case( 5 ): Str2Sthg( argv[ 4 ] , dens );
  case( 4 ): Str2Sthg( argv[ 3 ] , nvar );
  case( 3 ): Str2Sthg( argv[ 2 ] , wchg );
  case( 2 ): Str2Sthg( argv[ 1 ] , seed );
             break;
  default: cerr << "Usage: " << argv[ 0 ] <<
	   " seed [wchg nvar dens #rounds #chng %chng]"
 		<< endl <<
           "       wchg: what to change, coded bit-wise [127]"
		<< endl <<
           "             0 = add rows, 1 = delete rows "
		<< endl <<
           "             2 = modify rows, 3 = modify constants"
		<< endl <<
           "             4 = change global lower/upper bound"
          #if DYNAMIC_VARS > 0
		<< endl <<
           "             5 = add variables, 6 = delete variables"
	  #endif
	        << endl <<
           "       nvar: number of variables [10]"
	        << endl <<
           "       dens: rows / variables [4]"
	        << endl <<
           "       #rounds: how many iterations [40]"
	        << endl <<
           "       #chng: number changes [10]"
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

 m = nvar * dens;
 if( m < 1 ) {
  cout << "error: dens too small";
  exit( 1 );
  }

 rg.seed( seed );  // seed the pseudo-random number generator

 // constructing the data of the problem- - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // choosing whether convex or concave: toss a(n unbiased, two-sided) coin
 convex = ( dis( rg ) < 0.5 );

 // construct the matrix m x nvar matrix A and the m-vector b

 GenerateAb( m , nvar );
 GenerateBND();

 cout.setf( ios::scientific, ios::floatfield );
 cout << setprecision( 10 );

 #if( LOG_LEVEL >= 4 )
  printAb( A , b , BND );
 #endif

 // construction and loading of the objects - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 // construct the LP- - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 {
  // ensure all original pointers go out of scope immediately after that
  // the construction has finished

  LPBlock = new AbstractBlock();

  // construct the Variable
  xLP = new std::vector< ColVariable >( nsvar );
  #if DYNAMIC_VARS > 0
   xLPd = new std::list< ColVariable >( ndvar );
  #endif

  vLP = new ColVariable;
  vLP->set_Block( LPBlock );

  // construct the m dynamic Constraint
  auto ALP = new std::list< FRowConstraint >( m );
  auto ALPit = ALP->begin();
  for( Index i = 0 ; i < m ; )
   ConstructLPConstraint( i++ , *(ALPit++) );

  // construct the static lower bound Constraint
  auto LBc = new BoxConstraint( LPBlock , vLP , -INF , INF );
  if( BND != INF ) {
   if( convex )
    LBc->set_lhs( -BND );
   else
    LBc->set_rhs( BND );
   }

  // construct the Objective
  auto objLP = new FRealObjective();
  objLP->set_function( new LinearFunction( { std::make_pair( vLP , 1 ) } ) );
  objLP->set_sense( convex ? Objective::eMin : Objective::eMax , eNoMod );
  
  // now set the Variable, Constraint and Objective in the AbstractBlock
  LPBlock->add_static_variable( *vLP , "v" );
  LPBlock->add_static_variable( *xLP , "x" );
  #if DYNAMIC_VARS > 0
   LPBlock->add_dynamic_variable( *xLPd , "xd" );
  #endif
  LPBlock->add_dynamic_constraint( *ALP , "cuts" );
  LPBlock->add_static_constraint( *LBc , "vbnd" );
  LPBlock->set_objective( objLP );
  }

 // construct the NDO problem - - - - - - - - - - - - - - - - - - - - - - - -
 {
  // ensure all original pointers go out of scope immediately after that
  // the construction has finished

  NDOBlock = new AbstractBlock();

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

  // construct the Objective
  auto PF = new PolyhedralFunction( std::move( vars ) ,	std::move( A ) ,
				    std::move( b ) );
  if( BND != INF )
   PF->modify_bound( rs( BND ) );
  PF->set_is_convex( convex , eNoMod );
  auto objNDO = new FRealObjective();
  objNDO->set_function( PF );
  objNDO->set_sense( convex ? Objective::eMin : Objective::eMax , eNoMod );

  // now set the Variable and Objective in the AbstractBlock
  NDOBlock->add_static_variable( *xNDO , "x" );
  #if DYNAMIC_VARS > 0
   NDOBlock->add_dynamic_variable( *xNDOd , "xd" );
  #endif
  NDOBlock->set_objective( objNDO );

  SetGlobalBound();  // set lower bound, be it "hard" or "conditional"
  }

 // define bound constraints- - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 #if HAVE_CONSTRAINTS == 1
 {
  auto & LPx = *(LPBlock->get_static_variable_v< ColVariable >( "x" ));
  auto & NDOx = *(NDOBlock->get_static_variable_v< ColVariable >( "x" ));
  for( Index i = 0 ; i < nsvar ; ++i )
   SetNN( LPx[ i ] , NDOx[ i ] );
  #if DYNAMIC_VARS > 0
   auto LPxd = LPBlock->get_dynamic_variable< ColVariable >( "xd" )->begin();
   auto NDOxd = NDOBlock->get_dynamic_variable< ColVariable >( "xd"
							       )->begin();
   for( Index i = 0 ; i < ndvar ; ++i )
    SetNN( *(LPxd++) , *(NDOxd++) );
  #endif
  }
 #endif
 #if HAVE_CONSTRAINTS == 2
 {
  LPbnd = new std::list< BoxConstraint >;
  NDObnd = new std::list< BoxConstraint >;
  auto & LPx = *(LPBlock->get_static_variable_v< ColVariable >( "x" ));
  auto & NDOx = *(NDOBlock->get_static_variable_v< ColVariable >( "x" ));
  for( Index i = 0 ; i < nsvar ; ++i )
   SetBox( LPx[ i ] , NDOx[ i ] );
  #if DYNAMIC_VARS > 0
   auto LPxd = LPBlock->get_dynamic_variable< ColVariable >( "xd" )->begin();
   auto NDOxd = NDOBlock->get_dynamic_variable< ColVariable >( "xd"
							       )->begin();
   for( Index i = 0 ; i < ndvar ; ++i )
    SetBox( *(LPxd++) , *(NDOxd++) );
  #endif

  // note: the list may be empty, but it is intentionally added anyway
  LPBlock->add_dynamic_constraint( *LPbnd , "xbnd" );
  NDOBlock->add_dynamic_constraint( *NDObnd , "xbnd" );
  }
 #endif
 #if HAVE_CONSTRAINTS == 3
 {
  LPbnd = new std::list< FRowConstraint >;
  NDObnd = new std::list< BoxConstraint >;
  auto & LPx = *(LPBlock->get_static_variable_v< ColVariable >( "x" ));
  auto & NDOx = *(NDOBlock->get_static_variable_v< ColVariable >( "x" ));
  for( Index i = 0 ; i < nsvar ; ++i )
   SetFRow_Box( LPx[ i ] , NDOx[ i ] );
  #if DYNAMIC_VARS > 0
   auto LPxd = LPBlock->get_dynamic_variable< ColVariable >( "xd" )->begin();
   auto NDOxd = NDOBlock->get_dynamic_variable< ColVariable >( "xd"
							       )->begin();
   for( Index i = 0 ; i < ndvar ; ++i )
    SetFRow_Box( *(LPxd++) , *(NDOxd++) );
  #endif

  // note: the list may be empty, but it is intentionally added anyway
  LPBlock->add_dynamic_constraint( *LPbnd , "xbnd" );
  NDOBlock->add_dynamic_constraint( *NDObnd , "xbnd" );
  }
 #endif
 
 // attach the Solver to the Block- - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // for both Block do this by reading an appropriate BlockSolverConfig from
 // file and apply() it to the Block; note that the BlockSolverConfig are
 // clear()-ed and kept to do the cleanup at the end

 BlockSolverConfig * lpbsc;
 {
  auto c = Configuration::deserialize( "LPPar.txt" );
  lpbsc = dynamic_cast< BlockSolverConfig * >( c );
  if( ! lpbsc ) {
   cerr << "Error: LPPar.txt does not contain a BlockSolverConfig" << endl;
   delete( c );
   exit( 1 );
   }
  }

 lpbsc->apply( LPBlock );
 lpbsc->clear();

 BlockSolverConfig * ndobsc;
 {
  auto c = Configuration::deserialize( "NDOPar.txt" );
  ndobsc = dynamic_cast< BlockSolverConfig * >( c );
  if( ! ndobsc ) {
   cerr << "Error: NDOPar.txt does not contain a BlockSolverConfig" << endl;
   delete( c );
   exit( 1 );
   }
  }

 ndobsc->apply( NDOBlock );
 ndobsc->clear();

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
 // now, for n_repeat times in p_change% of the cases
 // - up to n_change rows are added
 // - up to n_change rows are deleted
 // - up to n_change rows are modified
 // - up to n_change rows are modified
 // - the bound is modified
 // - min( 1 , rnd( nsvar / 4 ) ) variables are added
 // - up to ndvar variables are removed
 //
 // then the two problems are re-solved

 for( Index rep = 0 ; rep < n_repeat * ( SKIP_BEAT + 1 ) ; ) {
  if( ! AllPassed )
   break;

  LOG1( rep << ": ");

  // add rows - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 1 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = Index( dis( rg ) * n_change ) ) {
    LOG1( "added " << tochange << " rows - " );

    GenerateAb( tochange , nvar );

    // add them to the LP
    vLP = LPBlock->get_static_variable< ColVariable >( "v" );
    xLP = LPBlock->get_static_variable_v< ColVariable >( "x" );
    #if DYNAMIC_VARS > 0
     xLPd = LPBlock->get_dynamic_variable< ColVariable >( "xd" );
    #endif

    std::list< FRowConstraint > nc( tochange );
    auto ncit = nc.begin();
    for( Index i = 0 ; i < tochange ; )
     ConstructLPConstraint( i++ , *(ncit++) );
    auto cnst = LPBlock->get_dynamic_constraint< FRowConstraint >( "cuts" );
    LPBlock->add_dynamic_constraints( *cnst , nc );

    // add them to the NDO
    auto PF = static_cast< p_PF >(
	       NDOBlock->get_objective< FRealObjective >()->get_function() );
     
    if( tochange == 1 )
     PF->add_row( std::move( A[ 0 ] ) , b[ 0 ] );
    else
     PF->add_rows( std::move( A ) , b );

    // update m
    m += tochange;

    // sanity checks
    PANIC( m == PF->get_A().size() );
    PANIC( m == cnst->size() );
    }

  // delete rows- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 2 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = min( m - 1 , Index( dis( rg ) * n_change ) ) ) {
    LOG1( "deleted " << tochange << " rows" );

    auto cnst = LPBlock->get_dynamic_constraint< FRowConstraint >( "cuts" );
    auto PF = static_cast< p_PF >(
	       NDOBlock->get_objective< FRealObjective >()->get_function() );
    
    if( dis( rg ) <= 0.5 ) {  // in 50% of the cases do a ranged change
     LOG1( "(r) - " );

     Index strt = dis( rg ) * ( m - tochange );
     Index stp = strt + tochange;

     // remove them from the LP
     LPBlock->remove_dynamic_constraints( *cnst , Range( strt , stp ) );
 
     // remove them from the NDO
     if( tochange == 1 )
      PF->delete_row( strt );
     else
      PF->delete_rows( Range( strt , stp ) );
     }
    else {  // in the other 50% of the cases, do a sparse change
     LOG1( "(s) - " );
     Subset nms( GenerateRand( m , tochange ) );

     // remove them from the LP
     if( tochange == 1 )
      LPBlock->remove_dynamic_constraint( *cnst , std::next( cnst->begin() ,
							     nms[ 0 ] ) );
     else
      LPBlock->remove_dynamic_constraints( *cnst , Subset( nms ) , true );
    
     // remove them from the NDO
     if( tochange == 1 )
      PF->delete_row( nms[ 0 ] );
     else
      PF->delete_rows( std::move( nms ) , true );
     }

    // update m
    m -= tochange;

    // sanity checks
    PANIC( m == PF->get_A().size() );
    PANIC( m == cnst->size() );
    }

  // modify rows- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 4 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = std::min( m , Index( dis( rg ) * n_change ) ) ) {
    LOG1( "modified " << tochange << " rows" );

    auto PF = static_cast< p_PF >(
	       NDOBlock->get_objective< FRealObjective >()->get_function() );

    GenerateAb( tochange , nvar );

    vLP = LPBlock->get_static_variable< ColVariable >( "v" );
    xLP = LPBlock->get_static_variable_v< ColVariable >( "x" );
    #if DYNAMIC_VARS > 0
     xLPd = LPBlock->get_dynamic_variable< ColVariable >( "xd" );
    #endif
    auto cnst = LPBlock->get_dynamic_constraint< FRowConstraint >( "cuts" );

    if( dis( rg ) <= 0.5 ) {  // in 50% of the cases do a ranged change
     LOG1( "(r) - " );

     Index strt = dis( rg ) * ( m - tochange );
     Index stp = strt + tochange;

     // send all the Modification to the same channel
     Observer::ChnlName chnl = LPBlock->open_channel();
     const auto iAM = Observer::make_par( eModBlck , chnl );

     // modify them in the LP
     auto cit = std::next( cnst->begin() , strt );
     for( Index i = 0 ; i < tochange ; ++i )
      ChangeLPConstraint( i , *(cit++) , iAM );

     LPBlock->close_channel( chnl );  // close the channel

     // modify them in the NDO
     if( tochange == 1 )
      PF->modify_row( strt , std::move( A[ 0 ] ) , b[ 0 ] );
     else
      PF->modify_rows( std::move( A ) , b , Range( strt , stp ) );
     }
    else {  // in the other 50% of the cases, do a sparse change
     LOG1( "(s) - " );
     Subset nms( GenerateRand( m , tochange ) );

     // send all the Modification to the same channel
     Observer::ChnlName chnl = LPBlock->open_channel();
     const auto iAM = Observer::make_par( eModBlck , chnl );

     // modify them in the LP
     Index prev = 0;
     auto cit = cnst->begin();
     for( Index i = 0 ; i < tochange ; ++i ) {
      cit = std::next( cit , nms[ i ] - prev );
      prev = nms[ i ];
      ChangeLPConstraint( i , *cit , iAM );
      }

     LPBlock->close_channel( chnl );  // close the channel

     // modify them in the NDO
     if( tochange == 1 )
      PF->modify_row( nms[ 0 ] , std::move( A[ 0 ] ) , b[ 0 ] );
     else
      PF->modify_rows( std::move( A ) , b , std::move( nms ) , true );
     }
    }

  // modify constants - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 8 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = std::min( m , Index( dis( rg ) * n_change ) ) ) {
    LOG1( "modified " << tochange << " constants" );

    Generateb( tochange );

    auto PF = static_cast< p_PF >(
	       NDOBlock->get_objective< FRealObjective >()->get_function() );
     
    auto cnst = LPBlock->get_dynamic_constraint< FRowConstraint >( "cuts" );

    if( dis( rg ) <= 0.5 ) {  // in 50% of the cases do a ranged change
     LOG1( "(r) - " );

     Index strt = dis( rg ) * ( m - tochange );
     Index stp = strt + tochange;

     // change them in the LP
     auto cit = std::next( cnst->begin() , strt );
     if( convex )
      for( Index i = 0 ; i < tochange ; )
       (*(cit++)).set_lhs( b[ i++ ] );
     else
      for( Index i = 0 ; i < tochange ; )
       (*(cit++)).set_rhs( b[ i++ ] );
 
     // modify them in the NDO
     if( tochange == 1 )
      PF->modify_constant( strt , b[ 0 ] );
     else
      PF->modify_constants( b , Range( strt , stp ) );
     }
    else {  // in the other 50% of the cases, do a sparse change
     LOG1( "(s) - " );
     Subset nms( GenerateRand( m , tochange ) );

     // change them in the LP
     Index prev = 0;
     auto cit = cnst->begin();
     if( convex )
      for( Index i = 0 ; i < tochange ; ) {
       cit = std::next( cit , nms[ i ] - prev );
       prev = nms[ i ];
       (*cit).set_lhs( b[ i++ ] );
       }
     else
      for( Index i = 0 ; i < tochange ; ) {
       cit = std::next( cit , nms[ i ] - prev );
       prev = nms[ i ];
       (*cit).set_rhs( b[ i++ ] );
       }

     // modify them in the NDO
     if( tochange == 1 )
      PF->modify_constant( nms[ 0 ] , b[ 0 ] );
     else
      PF->modify_constants( b , std::move( nms ) , true );
     }
    }

  // modify bound - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 16 ) && ( dis( rg ) <= p_change ) ) {
   LOG1( "modified bound - " );

   GenerateBND();

   // change it in the LP
   auto cnst = LPBlock->get_static_constraint< BoxConstraint >( "vbnd" );
   if( convex )
    cnst->set_lhs( -BND );
   else
    cnst->set_rhs( BND );

   // modify it in the NDO
   auto PF = static_cast< p_PF >(
	       NDOBlock->get_objective< FRealObjective >()->get_function() );

   PF->modify_bound( rs( BND ) );

   SetGlobalBound();  // set lower bound, be it "hard" or "conditional"
   }

 // add variables- - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  #if DYNAMIC_VARS > 0
  if( ( wchg & 32 ) && ( dis( rg ) <= p_change ) ) {
   Index tochange = std::max( Index( 1 ) , Index( dis( rg ) * nsvar / 4 ) );
   LOG1( "added " << tochange << " variables - " );

   GenerateA( m , tochange );

   // add them in the LP
   std::list< ColVariable > nxLPd( tochange );
   std::vector< ColVariable * > nxp( tochange );
   auto nxlpit = nxLPd.begin();
   for( Index i = 0 ; i < tochange ; )
    nxp[ i++ ] = &(*(nxlpit++));

   LPBlock->add_dynamic_variables(
	   *(LPBlock->get_dynamic_variable< ColVariable >( "xd" )) , nxLPd );

   auto cnst_it =
        LPBlock->get_dynamic_constraint< FRowConstraint >( "cuts" )->begin();
   if( tochange == 1 )
    for( Index i = 0 ; i < m ; ++i ) {
     auto fi = static_cast< p_LF >( (cnst_it++)->get_function() );
     fi->add_variable( nxp[ 0 ] , - A[ i ][ 0 ] );
     }
   else
    for( Index i = 0 ; i < m ; ++i ) {
     auto fi = static_cast< p_LF >( (cnst_it++)->get_function() );
     LinearFunction::v_coeff_pair ncp( tochange );
     for( Index j = 0 ; j < ncp.size() ; ++j ) {
      ncp[ j ].first = nxp[ j ];
      ncp[ j ].second = - A[ i ][ j ];
      }
     fi->add_variables( std::move( ncp ) );
     }

   // add them in the NDO
   std::list< ColVariable > nxNDOd( tochange );
   auto nxndit = nxNDOd.begin();
   for( Index i = 0 ; i < tochange ; )
    nxp[ i++ ] = &(*(nxndit++));

   NDOBlock->add_dynamic_variables(
	 *(NDOBlock->get_dynamic_variable< ColVariable >( "xd" )) , nxNDOd );

   // generate bound constraints
   #if HAVE_CONSTRAINTS > 0
    auto & LPxd = *(LPBlock->get_dynamic_variable< ColVariable >( "xd" ));
    auto LPxd_it = LPxd.begin();
    auto NDOxd_it = NDOBlock->get_dynamic_variable< ColVariable >( "xd"
								   )->begin();
    std::next( LPxd_it , ndvar );
    std::next( NDOxd_it , ndvar );

    #if HAVE_CONSTRAINTS == 1
     for( ; LPxd_it != LPxd.end() ; )
      SetNN( *(LPxd_it++) , *(NDOxd_it++) );
    #endif
    #if HAVE_CONSTRAINTS == 2
     LPbnd = new std::list< BoxConstraint >;
     NDObnd = new std::list< BoxConstraint >;

     for( ; LPxd_it != LPxd.end() ; )
      SetBox( *(LPxd_it++) , *(NDOxd_it++) );

     if( ! LPbnd->empty() ) {
      LPBlock->add_dynamic_constraints(
	 *(LPBlock->get_dynamic_constraint< BoxConstraint >( "xbnd" )) ,
	 *LPbnd );
      NDOBlock->add_dynamic_constraints(
	 *(NDOBlock->get_dynamic_constraint< BoxConstraint >( "xbnd" )) ,
	 *NDObnd );
      }
    #endif
    #if HAVE_CONSTRAINTS == 3
     LPbnd = new std::list< FRowConstraint >;
     NDObnd = new std::list< BoxConstraint >;

     for( ; LPxd_it != LPxd.end() ; )
      SetFRow_Box( *(LPxd_it++) , *(NDOxd_it++) );

     if( ! LPbnd->empty() ) {
      LPBlock->add_dynamic_constraints(
	 *(LPBlock->get_dynamic_constraint< FRowConstraint >( "xbnd" )) ,
	 *LPbnd );
      NDOBlock->add_dynamic_constraints(
	 *(NDOBlock->get_dynamic_constraint< BoxConstraint >( "xbnd" )) ,
	 *NDObnd );
      }
    #endif
   #endif

   auto PF = static_cast< p_PF >(
	       NDOBlock->get_objective< FRealObjective >()->get_function() );

   if( tochange == 1 )
    PF->add_variable( nxp[ 0 ] , A[ 0 ] );
   else
    PF->add_variables( std::move( nxp ) , std::move( A ) );

   // update nvar and ndvar
   nvar += tochange;
   ndvar += tochange;

   // sanity checks
   PANIC( nvar == PF->get_num_active_var() );
   for( auto & ai : PF->get_A() )
    PANIC( nvar == ai.size() );
   PANIC( ndvar ==
	      LPBlock->get_dynamic_variable< ColVariable >( "xd" )->size() );
   PANIC( ndvar ==
	     NDOBlock->get_dynamic_variable< ColVariable >( "xd" )->size() );
   for( auto & ci :
 	    *(LPBlock->get_dynamic_constraint< FRowConstraint >( "cuts" )) )
    PANIC( nvar == ci.get_num_active_var() );
   }

  // remove variables - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 64 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = Index( dis( rg ) * ndvar ) ) {
    LOG1( "removed " << tochange << " variables" );

    auto PF = static_cast< p_PF >(
	       NDOBlock->get_objective< FRealObjective >()->get_function() );

    if( dis( rg ) <= 0.5 ) {  // in 50% of the cases do a ranged removal
     LOG1( "(r) - " );

     Index strt = dis( rg ) * ( ndvar - tochange );
     Index stp = strt + tochange;

     // remove them from the LP
     auto xLPd = LPBlock->get_dynamic_variable< ColVariable >( "xd" );
     auto cnst_it =
      LPBlock->get_dynamic_constraint< FRowConstraint >( "cuts" )->begin();
     if( tochange == 1 )
      for( Index i = 0 ; i < m ; ++i ) {
       auto fi = static_cast< p_LF >( (cnst_it++)->get_function() );
       fi->remove_variable( strt + 1 );
       }
     else
      for( Index i = 0 ; i < m ; ++i ) {
       auto fi = static_cast< p_LF >( (cnst_it++)->get_function() );
       fi->remove_variables( Range( strt + 1 , stp + 1 ) , true );
       }
    
     #if HAVE_CONSTRAINTS == 2
      // the variables can now only be active in the associated box
      // constraint, if any: exploit this to identify the box constraint
      // and remove it
      RemoveBox( *LPBlock , Range( strt , stp ) );
     #endif
     #if HAVE_CONSTRAINTS == 3
      // the variables can now only be active in the associated frow
      // constraint, if any: exploit this to identify the frow constraint
      // and remove it
      RemoveFRow( *LPBlock , Range( strt , stp ) );
     #endif

     LPBlock->remove_dynamic_variables( *xLPd , Range( strt , stp ) );

     // remove them from the NDO
     auto xNDOd = NDOBlock->get_dynamic_variable< ColVariable >( "xd" );
     if( tochange == 1 )
      PF->remove_variable( strt );
     else
      PF->remove_variables( Range( strt , stp ) );

     #if HAVE_CONSTRAINTS > 1
      // the variables can now only be active in the associated box
      // constraint, if any: exploit this to identify the box constraint
      // and remove it
      // NOTE: no need in calling RemoveFRow because in the NDOBlock
      // bound constraints are always treated as Box ones
      RemoveBox( *NDOBlock , Range( strt , stp ) );
     #endif

     NDOBlock->remove_dynamic_variables( *xNDOd , Range( strt , stp ) );
     }
    else {  // in the other 50% of the cases, do a sparse change
     LOG1( "(s) - " );
     Subset nms( GenerateRand( ndvar , tochange ) );

     // remove them from the LP
     auto xLPd = LPBlock->get_dynamic_variable< ColVariable >( 0 );
     auto cnst_it =
             LPBlock->get_dynamic_constraint< FRowConstraint >( 0 )->begin();
     if( tochange == 1 ) {
      for( Index i = 0 ; i < m ; ++i ) {
       auto fi = static_cast< p_LF >( (cnst_it++)->get_function() );
       fi->remove_variable( nms[ 0 ] + 1 );
       }

      #if HAVE_CONSTRAINTS == 2
       // the variables can now only be active in the associated box
       // constraint, if any: exploit this to identify the box constraint
       // and remove it
       RemoveBox( *LPBlock , Range( nms[ 0 ] , nms[ 0 ] + 1 ) );
      #endif
      #if HAVE_CONSTRAINTS == 3
       // the variables can now only be active in the associated frow
       // constraint, if any: exploit this to identify the frow constraint
       // and remove it
       RemoveFRow( *LPBlock , Range( nms[ 0 ] , nms[ 0 ] + 1 ) );
      #endif

      auto vp = std::next( xLPd->begin() , nms[ 0 ] );
      LPBlock->remove_dynamic_variable( *xLPd , vp );
      }
     else {
      for( Index i = 0 ; i < m ; ++i ) {
       auto fi = static_cast< p_LF >( (cnst_it++)->get_function() );
       Subset nms1( nms );
       for( auto & n1i : nms1 )
	++n1i;
       fi->remove_variables( std::move( nms1 ) , true );
       }

      #if HAVE_CONSTRAINTS == 2
       // the variables can now only be active in the associated box
       // constraint, if any: exploit this to identify the box constraint
       // and remove it
       RemoveBox( *LPBlock , nms );
      #endif
      #if HAVE_CONSTRAINTS == 3
       // the variables can now only be active in the associated frow
       // constraint, if any: exploit this to identify the frow constraint
       // and remove it
       RemoveFRow( *LPBlock , nms );
      #endif

      LPBlock->remove_dynamic_variables( *xLPd , Subset( nms ) );
      }

     // remove them from the NDO
     auto xNDOd = NDOBlock->get_dynamic_variable< ColVariable >( 0 );
     if( tochange == 1 ) {
      PF->remove_variable( nms[ 0 ] );

      #if HAVE_CONSTRAINTS > 1
       // the variables can now only be active in the associated box
       // constraint, if any: exploit this to identify the box constraint
       // and remove it
       // NOTE: no need in calling RemoveFRow because in the NDOBlock
       // bound constraints are always treated as Box ones
       RemoveBox( *NDOBlock , Range( nms[ 0 ] , nms[ 0 ] + 1 ) );
      #endif

      auto vp = std::next( xNDOd->begin() , nms[ 0 ] );
      NDOBlock->remove_dynamic_variable( *xNDOd , vp );
      }
     else {
      PF->remove_variables( Subset( nms ) );

      #if HAVE_CONSTRAINTS > 1
       // the variables can now only be active in the associated box
       // constraint, if any: exploit this to identify the box constraint
       // and remove it
       // NOTE: no need in calling RemoveFRow because in the NDOBlock
       // bound constraints are always treated as Box ones
       RemoveBox( *NDOBlock , nms );
      #endif

      NDOBlock->remove_dynamic_variables( *xNDOd , std::move( nms ) );
      }
     }

    // update ndvar
    ndvar -= tochange;

    // sanity checks
    PANIC( ndvar == PF->get_num_active_var() );
    for( auto & ai : PF->get_A() )
     PANIC( ndvar == ai.size() );
    PANIC( ndvar ==
	         LPBlock->get_dynamic_variable< ColVariable >( 0 )->size() );
    PANIC( ndvar ==
	        NDOBlock->get_dynamic_variable< ColVariable >( 0 )->size() );
    for( auto & ci :
	          *(LPBlock->get_dynamic_constraint< FRowConstraint >( 0 )) )
     PANIC( ndvar == ci.get_num_active_var() );
    }
  #endif

  // if verbose, print out stuff- - - - - - - - - - - - - - - - - - - - - - -

  #if( LOG_LEVEL >= 3 )
   ((LPBlock->get_registered_solvers()).front())->set_par(
		                     MILPSolver::strOutputFile , "LPBlock-" +
		                     std::to_string( rep ) + ".lp" );
   #if( LOG_LEVEL >= 4 )
    auto PF = dynamic_cast< PolyhedralFunction * >(
	       NDOBlock->get_objective< FRealObjective >()->get_function() );
    PANIC( PF );
    printAb( PF->get_A() , PF->get_b() ,
	     convex ? PF->get_global_lower_bound()
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
  cout << GREEN( All tests passed!! ) << endl;
 else
  cout << RED( Shit happened!! ) << endl;
 
 // destroy the Block - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 // apply() the clear()-ed BlockSolverConfig to cleanup Solver
 ndobsc->apply( NDOBlock );
 lpbsc->apply( LPBlock );

 // then delete the BlockSolverConfig
 delete( ndobsc );
 delete( lpbsc );

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
