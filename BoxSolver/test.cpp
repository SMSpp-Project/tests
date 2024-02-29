/*--------------------------------------------------------------------------*/
/*-------------------------- File test.cpp ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Main for testing BoxSolver
 *
 * A random "box-only" AbstractBlock with separable Objective (a
 * FRealObjective with either a LinearFunction or a DQuadFunction inside) is
 * constructed and solved with a BoxSolver. According to the value of the
 * macro DIRECTION_TEST, we either compare the get_opposite_value() with the
 * get_var_value() obtained by reversing the sign of the Objective (that must
 * be equal), or we compare with the results of an appropriate Solver (e.g.,
 * a :MILPSolver). The Block is then repeatedly randomly modified and
 * re-solved several times, the results are compared. 
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

#if( LOG_LEVEL >= 1 )
 #define LOG1( x ) cout << x
 #define CLOG1( y , x ) if( y ) cout << x
#else
 #define LOG1( x )
 #define CLOG1( y , x )
#endif

/*--------------------------------------------------------------------------*/
/* the AbstractBlock for testing is made of NUMBER_LEVELS levels, and each
 * Block save those in the last level (the leaves) has NUMBER_SONS sub-Block.
 */

#define NUMBER_LEVELS 2

#define NUMBER_SONS 5

/*--------------------------------------------------------------------------*/
/* if nonzero, quadratic terms are always >= 0 when minimizing and <= 0 when
 * maximising, since many other Solver (but not BoxSolver) may not be able to
 * solve the Block otherwise. */

#define ENSURE_CONVEX 1

/*--------------------------------------------------------------------------*/
/* if nonzero, lower bounds on binary variables are guaranteed to be <= 0 and
 * upper bound >= 1, i.e., compatible with the "binarity status" of the
 * variable. This is because some other Solver (but not BoxSolver) may take
 * issue with incompatible bounds rather than just doing the right thing and
 * either fixing the variable or declaring the model empty outright. */

#define ENSURE_BIN_FEASIBLE 0

/*--------------------------------------------------------------------------*/
// if nonzero, we compare the get_opposite_value() with the get_var_value()

#define DIRECTION_TEST 0

/*--------------------------------------------------------------------------*/
// if nonzero, the BoxSolver is detached and re-attached at all iterations

#define DETACH_BOX 0

// if nonzero, the MILPSolver is detached and re-attached at all iterations

#define DETACH_LP 0

/*--------------------------------------------------------------------------*/
// if nonzero, the Block is not solved at every round of changes, but only
// every SKIP_BEAT + 1 rounds. this allows changes to accumulate, and
// therefore puts more pressure on the Modification handling of the Solver
// (in case this tries to do "smart" things rather than dumbly processing
// each one in turn)
//
// note that the number of rounds of changes is them multiplied by
// SKIP_BEAT + 1, so that the input parameter still dictates the number of
// Block solutions

#define SKIP_BEAT 3

/*--------------------------------------------------------------------------*/

#define USECOLORS 1
#if( USECOLORS )
 #define RED( x ) "\x1B[31m" #x "\033[0m"
 #define GREEN( x ) "\x1B[32m" #x "\033[0m"
#else
 #define RED( x ) #x
 #define GREEN( x ) #x
#endif

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

#define DYNAMIC_VARS 1
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

// if SMSpp_ensure_load() need not be used, BoxSolver.h need not be included
// unless DIRECTION_TEST > 0
#if DIRECTION_TEST
 #include "BoxSolver.h"
#endif

#include "FRealObjective.h"

#include "DQuadFunction.h"

#include "LinearFunction.h"

#include "OneVarConstraint.h"

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
using Vec_FunctionValue = LinearFunction::Vec_FunctionValue;

using RHSValue = RowConstraint::RHSValue;

using coeff_pair = LinearFunction::coeff_pair;
using v_coeff_pair = LinearFunction::v_coeff_pair;

using coeff_triple = DQuadFunction::coeff_triple;
using v_coeff_triple = DQuadFunction::v_coeff_triple;

/*--------------------------------------------------------------------------*/
/*------------------------------- CONSTANTS --------------------------------*/
/*--------------------------------------------------------------------------*/

static constexpr FunctionValue INF = Inf< RHSValue >();

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/

AbstractBlock * BoxBlock;  // the "box" Block

Index nvar = 10;           // number of variables
#if DYNAMIC_VARS > 0
 Index nsvar;              // number of static variables
 Index ndvar;              // number of dynamic variables
#else
 #define nsvar nvar        // all variables are static
#endif

bool minobj;               // whether min or max
bool isint;                // whether integer-constrained
bool isquad;               // whether lin or quad
bool isfeas;               // whether always feasible
bool isbndd;               // whether always bounded

std::mt19937 rg;           // base random generator
std::uniform_real_distribution<> dis( 0.0 , 1.0 );
std::uniform_int_distribution<> idis( 0 , NUMBER_SONS );

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

template< class T >
static void Str2Sthg( const char* const str , T &sthg )
{
 istringstream( str ) >> sthg;
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

static void PrintResults( bool hs , int rtrn , double fo )
{
 if( hs )
  cout << fo;
 else
  if( rtrn == Solver::kInfeasible )
   cout << "    Unfeas";
  else
   if( rtrn == Solver::kUnbounded )
    cout << "      Unbounded";
   else
    cout << "      Error!";
 }

/*--------------------------------------------------------------------------*/

static void set_bounds( ColVariable & x )
{
 if( dis( rg ) < 0.25 )  // in 25% of the cases it is in [ -1 , 1 ]
  x.is_unitary( true , eNoMod );
 if( dis( rg ) < 0.15 )  // in 15% of the cases it is positive
  x.is_positive( true , eNoMod );
 if( dis( rg ) < 0.15 )  // in 15% of the cases it is negative
  x.is_negative( true , eNoMod );
 if( isint && ( dis( rg ) < 0.50 ) )  // in 50% of the cases it is integer
  x.is_integer( true , eNoMod );
 }

/*--------------------------------------------------------------------------*/

static void set_bounds( BoxConstraint & b )
{
 RHSValue l = -INF;
 RHSValue u = INF;
 // if feasibility has to be guaranteed the upper bound is taken in
 // [ 0 , 2 ] and the lower bound in [ -2 , 0 ] so that they do not
 // contrast (and they are 50% of the times active against the "inherent"
 // upper and lower bound of 1 and -1, respectively, if any); otherwise
 // they are taken in [ - 0.2 , 2 ] and [ -2 , 0.2 ] so that they do have
 // a small chance to overlap, either betweeb themselves or against the
 // "inherent" sign constraints
 RHSValue le = isfeas ? 0 : 0.2;
 RHSValue re = isfeas ? 2 : 2.2;
 auto x = static_cast< ColVariable * >( b.get_active_var( 0 ) );
 if( isbndd ) {  // boundedness is required 
  // if there is no "inherent" lower bound, and anyway in 60% of the cases
  if( ( x->get_lb() == -INF ) || ( dis( rg ) < 0.60 ) )
   l = - re * dis( rg ) + le;  // generate a LB
  // if there is no "inherent" upper bound, and anyway in 60% of the cases
  if( ( x->get_ub() == INF ) || ( dis( rg ) < 0.60 ) )
   u = re * dis( rg ) - le;    // generate an UB
  }
 else {
  if( dis( rg ) < 0.60 )       // in 60% of the cases,
   l = - re * dis( rg ) + le;  // generate a LB
  if( dis( rg ) < 0.60 )       // in 60% of the cases,
   u = re * dis( rg ) - le;    // generate an UB
  }
 #if ENSURE_BIN_FEASIBLE
  if( x->is_unitary() && x->is_positive() && x->is_integer() ) {
   if( l > 0 ) l = 0;
   if( u < 1 ) u = 1;
   }
 #endif
 if( l > -INF )                // if a LB is generated
  b.set_lhs( l );              // set it
 if( u < INF )                 // if an UB is generated
  b.set_rhs( u );              // set it
 }

/*--------------------------------------------------------------------------*/

static void set_bounds( ColVariable & x , BoxConstraint & b )
{
 b.set_variable( & x );
 set_bounds( b );
 }

/*--------------------------------------------------------------------------*/

static void set_lin_c( FunctionValue & b )
{
 b = 2 * dis( rg ) - 1;
 }

/*--------------------------------------------------------------------------*/

static void set_quad_c( FunctionValue & a )
{
 // rationale: if bounded at all, the variable are in [ -2 , 2 ] with a
 // b taken in [ -1 , 1 ], i.e., abs( b ) = 0.5, and the stationary point
 // has the form - b / ( 2 * a ); with a = 1/8 one gets 2 in average,
 // which means that the stationary point is surely outside of the
 // interval, thus a is taken between 1/4 and 1/16 (with the right sign,
 // and if nonzero which is 60% of the times)
 // the sign is randomly chosen with equal probability, unless ENSURE_CONVEX
 // in which case it is fixed according to the optimization sign
 a = 0;
 if( dis( rg ) < 0.60 ) {
  a = dis( rg ) * 0.1875 + 0.0625;
  #if ENSURE_CONVEX > 0
   if( ! minobj )
  #else
   if( dis( rg ) < 0.50 )
  #endif
    a = -a;
  }
 }
 
/*--------------------------------------------------------------------------*/

static void set_quad( ColVariable & x , coeff_triple & t )
{
 std::get< 0 >( t ) = & x;
 set_lin_c( std::get< 1 >( t ) );
 set_quad_c( std::get< 2 >( t ) );
 }
 
/*--------------------------------------------------------------------------*/

static void set_lin( ColVariable & x , coeff_pair & p )
{
 p.first = & x;
 set_lin_c( p.second );
 }

/*--------------------------------------------------------------------------*/

static AbstractBlock * new_AB( Index lev = 0 )
{
 auto AB = new AbstractBlock();

 // construct the Variable
 auto x = new std::vector< ColVariable >( nsvar );
 #if DYNAMIC_VARS > 0
  auto xd = new std::list< ColVariable >( ndvar );
 #endif

 // set the "inherent" bounds on the Variable
 for( auto & xi : *x )
  set_bounds( xi );
	   
 #if DYNAMIC_VARS > 0
  for( auto & xi : *xd )
   set_bounds( xi );
 #endif

 // set the Variable in the BoxBlock
 AB->add_static_variable( *x , "x" );
 #if DYNAMIC_VARS > 0
  AB->add_dynamic_variable( *xd , "xd" );
 #endif

 // construct the OneVarConstraint
 auto box = new std::vector< BoxConstraint >( nsvar );
 #if DYNAMIC_VARS > 0
  auto boxd = new std::list< BoxConstraint >( ndvar );
 #endif

 auto boxit = box->begin();
 for( auto & xi : *x )
  set_bounds( xi , *(boxit++) );

 #if DYNAMIC_VARS > 0
  auto boxdit = boxd->begin();
  for( auto & xi : *xd )
   set_bounds( xi , *(boxdit++) );
 #endif

 // set the OneVarConstraint in the AbstractBlock
 AB->add_static_constraint( *box , "box" );
 #if DYNAMIC_VARS > 0
  AB->add_dynamic_constraint( *boxd , "boxd" );
 #endif

 // construct the Objective
 auto obj = new FRealObjective();
 Function *f;
 if( isquad ) {  // quadratic objective
  v_coeff_triple vt( nvar );
  auto vit = vt.begin();
  for( auto & xi : *x )
   set_quad( xi , *(vit++) );
  #if DYNAMIC_VARS > 0
   for( auto & xi : *xd )
    set_quad( xi , *(vit++) );
  #endif

  f = new DQuadFunction( std::move( vt ) );
  }
 else {          // linear objective
  v_coeff_pair vp( nvar );
  auto vit = vp.begin();
  for( auto & xi : *x )
   set_lin( xi , *(vit++) );
  #if DYNAMIC_VARS > 0
   for( auto & xi : *xd )
    set_lin( xi , *(vit++) );
  #endif

  f = new LinearFunction( std::move( vp ) );
  }

 obj->set_function( f );
 obj->set_sense( minobj ? Objective::eMin : Objective::eMax , eNoMod );

 // set the Objective in the AbstractBlock
 AB->set_objective( obj );

 // now iterate on the sub-Block
 if( lev < NUMBER_LEVELS )
 for( Index i = 0 ; i < NUMBER_SONS ; ++i )
  AB->add_nested_Block( new_AB( lev + 1 ) );

 return( AB );
 }

/*--------------------------------------------------------------------------*/

static AbstractBlock * choose_Block( AbstractBlock * ab )
{
 if( ! ab->get_number_nested_Blocks() ) {
  LOG1( "0 ]" );
  return( ab );
  }

 if( auto wb = idis( rg ) ) {
  --wb;
  LOG1( wb << ", " );
  return( choose_Block( static_cast< AbstractBlock * >(
					    ab->get_nested_Block( wb ) ) ) );
  }
 else {
  LOG1( "0 ]" );
  return( ab );
  }
 }

/*--------------------------------------------------------------------------*/

static void set_sense( AbstractBlock * ab , int news )
{
 ab->get_objective()->set_sense( news );
 for( auto bk : ab->get_nested_Blocks() )
  set_sense( static_cast< AbstractBlock * >( bk ) , news );
 }

/*--------------------------------------------------------------------------*/

static bool SolveBoth( void ) 
{
 try {
  // solve with the Box Solver- - - - - - - - - - - - - - - - - - - - - - - -
  Solver * Slvr1 = BoxBlock->get_registered_solvers().front();
  #if DETACH_MILP
   BoxBlock->unregister_Solver( Slvr1 );
   BoxBlock->register_Solver( Slvr1 , true );  // push it to the front
  #endif
  int rtrn1st = Slvr1->compute( false );
  bool hs1st = ( ( ( rtrn1st >= Solver::kOK ) && ( rtrn1st < Solver::kError )
                   && ( rtrn1st != Solver::kUnbounded )
                   && ( rtrn1st != Solver::kInfeasible ) )
                 || ( rtrn1st == Solver::kLowPrecision ) );
  double fo1st = Slvr1->get_var_value();

  #if DIRECTION_TEST
   auto BSlvr1 = dynamic_cast< BoxSolver * >( Slvr1 );
   if( ! BSlvr1 ) {
    cerr << "Error: Solver1 not a BoxSolver and DIRECTION_TEST" << endl;
    exit( 1 );
    }
   double oppfo = BSlvr1->get_opposite_value();
   // invert the verse of the Objective
   set_sense( BoxBlock , minobj ? Objective::eMax : Objective::eMin );
   int invrtrn = Slvr1->compute( false );
   double invfo = Slvr1->get_var_value();

   // restore the verse of the Objective
   set_sense( BoxBlock , minobj ? Objective::eMin : Objective::eMax );
   if( minobj ) {
    if( ( oppfo == -INF ) && ( invfo == -INF ) ) {
     LOG1( "OK(u)" << endl );
     return( true );
     }
    if( ( oppfo == INF ) && ( invfo == INF ) ) {
     LOG1( "OK(e)" << endl );
     return( true );
     }
    }
   else {
    if( ( oppfo == INF ) && ( invfo == INF ) ) {
     LOG1( "OK(u)" << endl );
     return( true );
     }
    if( ( oppfo == -INF ) && ( invfo == -INF ) ) {
     LOG1( "OK(e)" << endl );
     return( true );
     }
    }

   if( ( oppfo > -INF ) && ( oppfo < INF ) &&
       ( invfo > -INF ) && ( invfo < INF ) && 
       ( abs( oppfo - invfo ) <= 2e-7 *
	 max( double( 1 ) , max( abs( oppfo ) , abs( invfo ) ) ) ) ) {
    LOG1( "OK(f)" << endl );
    return( true );
    }

   #if( LOG_LEVEL >= 1 )
    cout << "opp = ";
    if( oppfo == -INF )
     cout << "-INF";
    else
     if( oppfo == INF )
      cout << "INF";
     else
      cout << oppfo;

    cout << " - inv = ";
    if( invfo == -INF )
     cout << "-INF";
    else
     if( invfo == INF )
      cout << "INF";
     else
      cout << invfo;
    cout << endl;
   #endif
  #else
   // solve with the :MILP Solver- - - - - - - - - - - - - - - - - - - - - - -
   Solver * Slvr2 = BoxBlock->get_registered_solvers().back();
   #if DETACH_BOX
    BoxBlock->unregister_Solver( Slvr2 );
    BoxBlock->register_Solver( Slvr2 );  // push it to the back
   #endif
   int rtrn2nd = Slvr2->compute( false );

   bool hs2nd = ( ( ( rtrn2nd >= Solver::kOK ) && ( rtrn2nd < Solver::kError )
                   && ( rtrn2nd != Solver::kUnbounded )
                   && ( rtrn2nd != Solver::kInfeasible ) )
                 || ( rtrn2nd == Solver::kLowPrecision ) );
   double fo2nd = hs2nd ? Slvr2->get_var_value() : -INF;

   if( hs1st && hs2nd && ( abs( fo1st - fo2nd ) <= 2e-7 *
			   max( double( 1 ) , max( abs( fo1st ) ,
						   abs( fo2nd ) ) ) ) ) {
    LOG1( "OK(f)" << endl );
    return( true );
    }

   // note: for a problem that is both potentially unbounded (say, it has
   // at least one/ variable with positive linear cost, 0 quadratic cost,
   // and +INF upper bound) and unfeasible (upper bound < lower bound),
   // BoxSolver correctly returns kInfeasible. however, some :MILPSolver
   // detects this in the preprocessor and may "get confused" returning
   // and "unbounded OR unfeasible" return status that can be translated
   // into kUnbounded. this is the reason of this apparently weird test
   // where we consider kUnbounded and kInfeasible as "equivalent". while
   // this may lead to bona fide errors to be ignored, it seems to be the
   // only reasonable way out
   if( ( rtrn1st == Solver::kInfeasible ) &&
       ( ( rtrn2nd == Solver::kInfeasible ) ||
	 ( rtrn2nd == Solver::kUnbounded ) ) ) {
    LOG1( "OK(e)" << endl );
    return( true );
    }

   if( ( rtrn1st == Solver::kUnbounded ) &&
       ( rtrn2nd == Solver::kUnbounded ) ) {
    LOG1( "OK(u)" << endl );
    return( true );
    }

   #if( LOG_LEVEL >= 1 )
    cout << "BOX = ";
    PrintResults( hs1st , rtrn1st , fo1st );

    cout << " ~ MILP = ";
    PrintResults( hs2nd , rtrn2nd , fo2nd );
    cout << endl;
   #endif
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
 Index wchg = 3;
 double p_change = 0.6;
 Index n_change = 10;
 Index n_repeat = 100;

 switch( argc ) {
  case( 7 ): Str2Sthg( argv[ 6 ] , p_change );
  case( 6 ): Str2Sthg( argv[ 5 ] , n_change );
  case( 5 ): Str2Sthg( argv[ 4 ] , n_repeat );
  case( 4 ): Str2Sthg( argv[ 3 ] , nvar );
  case( 3 ): Str2Sthg( argv[ 2 ] , wchg );
  case( 2 ): Str2Sthg( argv[ 1 ] , seed );
             break;
  default: cerr << "Usage: " << argv[ 0 ] <<
	   " seed [wchg nvar #rounds #chng %chng]"
 		<< endl <<
           "       wchg: what to change, coded bit-wise [3]"
		<< endl <<
           "             0 = bounds, 1 = objective "
		<< endl <<
           "       nvar: number of variables [10]"
	        << endl <<
           "       #rounds: how many iterations [100]"
	        << endl <<
           "       #chng: number changes [10]"
	        << endl <<
           "       %chng: probability of changing [0.6]"
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

 rg.seed( seed );  // seed the pseudo-random number generator

 // constructing the data of the problem- - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // choosing whether min or max: toss a(n unbiased, two-sided) coin
 minobj = ( dis( rg ) < 0.5 );
 // choosing whether lin or quad: toss a(n unbiased, two-sided) coin
 isquad = ( dis( rg ) < 0.5 );
 // choosing whether integer or not: toss a(n unbiased, two-sided) coin
 isint = ( dis( rg ) < 0.5 );
 // choosing whether always feasible or not: toss a(...) coin
 isfeas = ( dis( rg ) < 0.5 );
 // choosing whether always bounded or not: toss a(...) coin
 isbndd = ( dis( rg ) < 0.5 );
 
 #if( LOG_LEVEL >= 1 )
  if( minobj ) cout << "min"; else cout << "max";
  cout << " ~ ";
  if( isquad ) cout << "quad"; else cout << "lin";
  cout << " ~ ";
  if( isint ) cout << "int"; else cout << "cont";
  cout << " ~ ";
  if( isfeas ) cout << "feas"; else cout << "unfeas";
  cout << " ~ ";
  if( isbndd ) cout << "bndd"; else cout << "unbndd";
  cout << endl;
 #endif
 
 // construction and loading of the objects - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 BoxBlock = new_AB();

 //!! check the AbstractBlock
 BoxBlock->is_correct();

 // attach the Solver(s) to the Block - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // do this by reading an appropriate BlockSolverConfig from file and
 // apply() it to the BoxBlock; note that the BlockSolverConfig is
 // clear()-ed and kept to do the cleanup at the end

 BlockSolverConfig * bsc;
 {
  auto c = Configuration::deserialize( "BSCfg.txt" );
  bsc = dynamic_cast< BlockSolverConfig * >( c );
  if( ! bsc ) {
   cerr << "Error: configuration file not a BlockSolverConfig" << endl;
   delete( c );
   exit( 1 );
   }

  bsc->apply( BoxBlock );
  bsc->clear();

  if( BoxBlock->get_registered_solvers().empty() ) {
   cout << endl << "no Solver registered to the Block!" << endl;
   exit( 1 );
   }
  }

 // first solver call - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 LOG1( "First call: " );

 bool AllPassed = SolveBoth();
 
 // main loop - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // now, for n_repeat times:
 // - up to n_change bounds are changed
 // - up to n_change objective coefficients are changed
 //
 // then the two Solver are called to re-solve the BoxBlock

 for( Index rep = 0 ; rep < n_repeat * ( SKIP_BEAT + 1 ) ; ) {
  LOG1( rep << ": ");

  // change bounds- - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 1 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = min( nvar , Index( dis( rg ) * n_change ) ) ) {
    LOG1( "changed " << tochange << " bounds[ " );
    auto bk = choose_Block( BoxBlock );
    LOG1( " - " );
    auto box = bk->get_static_constraint_v< BoxConstraint >( "box" );
    assert( box );
    const double prob = double( tochange ) / double( nvar );

    for( auto & bi : *box )
     if( dis( rg ) <= prob ) {
      set_bounds( bi );
      if( ! --tochange )
       break;
      }

    #if DYNAMIC_VARS > 0
     if( tochange ) {
      auto boxd = bk->get_dynamic_constraint< BoxConstraint >( "boxd" );
      assert( boxd );

      for( auto & bi : *boxd )
       if( dis( rg ) <= prob ) {
        set_bounds( bi );
        if( ! --tochange )
	 break;
        }
      }
    #endif    
    }

  // change coefficients- - - - - - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 2 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = min( nvar , Index( dis( rg ) * n_change ) ) ) {
    LOG1( "changed " << tochange << " obj coeffs[ " );
    auto bk = choose_Block( BoxBlock );

    Vec_FunctionValue NC( tochange );
    for( auto & nc : NC )
     set_lin_c( nc );

    auto obj = static_cast< FRealObjective * >( bk->get_objective() );

    if( dis( rg ) <= 0.5 ) {  // in 50% of the cases do a ranged change
     LOG1( "(r) - " );

     Index strt = dis( rg ) * ( nvar - tochange );
     Index stp = strt + tochange;

     if( isquad ) {  // quadratic objective
      auto qf = static_cast< DQuadFunction * >( obj->get_function() );

      Vec_FunctionValue NQC( tochange );
      for( auto & nqc : NQC )
       set_quad_c( nqc );

      if( tochange == 1 )
       qf->modify_term( strt , NC.front() , NQC.front() );
      else
       qf->modify_terms( NQC.begin() , NC.begin() , Range( strt , stp ) );
      }
     else {          // linear objective
      auto lf = static_cast< LinearFunction * >( obj->get_function() );

      if( tochange == 1 )
       lf->modify_coefficient( strt , NC.front() );
      else
       lf->modify_coefficients( std::move( NC ) , Range( strt , stp ) );
      }
     }
    else {  // in the other 50% of the cases, do a sparse change
     LOG1( "(s) - " );
     Subset nms( GenerateRand( nvar , tochange ) );

     if( isquad ) {  // quadratic objective
      auto qf = static_cast< DQuadFunction * >( obj->get_function() );

      Vec_FunctionValue NQC( tochange );
      for( auto & nqc : NQC )
       set_quad_c( nqc );

      if( tochange == 1 )
       qf->modify_term( nms.front() , NC.front() , NQC.front() );
      else
       qf->modify_terms( NQC.begin() , NC.begin() , std::move( nms ) );
      }
     else {          // linear objective
      auto lf = static_cast< LinearFunction * >( obj->get_function() );

      if( tochange == 1 )
       lf->modify_coefficient( nms.front() , NC.front() );
      else
       lf->modify_coefficients( std::move( NC ) , std::move( nms ) );
      }
     }
    }

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
 
 // destroy objects and vectors - - - - - - - - - - - - - - - - - - - - - - - 
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 // apply() the clear()-ed BlockSolverConfig to cleanup Solver
 bsc->apply( BoxBlock );

 // then delete the BlockSolverConfig
 delete( bsc );

 // delete the Block
 delete( BoxBlock );

 // terminate - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 return( AllPassed ? 0 : 1 );

 }  // end( main )

/*--------------------------------------------------------------------------*/
/*------------------------ End File test.cpp -------------------------------*/
/*--------------------------------------------------------------------------*/
