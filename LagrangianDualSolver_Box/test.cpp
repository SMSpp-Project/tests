/*--------------------------------------------------------------------------*/
/*-------------------------- File test.cpp ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Main for testing LagrangianDualSolver with BoxSolver
 *
 * A "very simple structured" AbstractBlock is constructed, formed of k
 * sub-AbstractBlock with n variables each, only box constraints and
 * separable Objective (FRealObjective with a LinearFunction or DQuadFunction
 * inside). m linking constraints are constructed in the father, which has no
 * Variable and no Objective of its own. Two different Solver are registered
 * to the AbstractBlock via a BlockSolverConfig, one of which is assumed to be
 * a LagrangianDualSolver using BoxSolver to solve the sub-AbstractBlock. The
 * AbstractBlock is solved by the Solver(s) and the results are compared.
 * The AbstractBlock is repeatedly randomly modified and re-solved several
 * times.
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

#if( LOG_LEVEL >= 1 )
 #define LOG1( x ) cout << x
 #define CLOG1( y , x ) if( y ) cout << x

 #if( LOG_LEVEL >= 2 )
  #define LOG_ON_COUT 1
  // if nonzero, the 2nd Solver (LagrangianDualSolver) log is sent on cout
  // rather than on a file
 #endif
#else
 #define LOG1( x )
 #define CLOG1( y , x )
#endif

/*--------------------------------------------------------------------------*/
// if nonzero, the 1st Solver attached to the AbstractBlock is detached
// and re-attached to it at all iterations

#define DETACH_1ST 0

// if nonzero, the 2nd Solver attached to the AbstractBlock is detached and
// re-attached to it at all iterations

#define DETACH_2ND 0

/*--------------------------------------------------------------------------*/
// if nonzero, the AbstractBlock is not solved at every round of changes, but
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

#define USECOLORS 1
#if( USECOLORS )
 #define RED( x ) "\x1B[31m" #x "\033[0m"
 #define GREEN( x ) "\x1B[32m" #x "\033[0m"
#else
 #define RED( x ) #x
 #define GREEN( x ) #x
#endif

/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <sstream>

#include <random>

#include "AbstractBlock.h"

#include "BlockSolverConfig.h"

#include "FRealObjective.h"

#include "FRowConstraint.h"

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

const char *const logF = "log.txt";

static constexpr FunctionValue INF = Inf< RHSValue >();

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/

AbstractBlock * TestBlock;  // the AbstractBlock that is solved
Index nvar = 10;            // number of variables

bool minobj;                // whether min or max
bool isquad;                // whether lin or quad

std::mt19937 rg;            // base random generator
std::uniform_real_distribution<> dis( 0.0 , 1.0 );

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

static void set_bounds( BoxConstraint & b )
{
 // the upper bound is taken in [ 0 , 2 ] and the lower bound in [ -2 , 0 ]
 // so that they do not contrast, 0 is always feasible and it never is
 // unbounded

 b.set_lhs( - 2 * dis( rg ) );
 b.set_rhs( 2 * dis( rg ) );
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

 a = 0;
 if( dis( rg ) < 0.60 ) {
  a = dis( rg ) * 0.1875 + 0.0625;
  if( ! minobj )
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

static AbstractBlock * construct_son( void )
{
 auto BoxBlock = new AbstractBlock();

 // construct the Variable
 auto x = new std::vector< ColVariable >( nvar );

 // set the Variable in the BoxBlock
 BoxBlock->add_static_variable( *x , "x" );

 // construct the OneVarConstraint
 auto box = new std::vector< BoxConstraint >( nvar );

 auto boxit = box->begin();
 for( auto & xi : *x )
  set_bounds( xi , *(boxit++) );

 // set the OneVarConstraint in the BoxBlock
 BoxBlock->add_static_constraint( *box , "box" );

 // construct the Objective
 auto obj = new FRealObjective();
 Function *f;
 if( isquad ) {  // quadratic objective
  v_coeff_triple vt( nvar );
  auto vit = vt.begin();
  for( auto & xi : *x )
   set_quad( xi , *(vit++) );

  f = new DQuadFunction( std::move( vt ) );
  }
 else {          // linear objective
  v_coeff_pair vp( nvar );
  auto vit = vp.begin();
  for( auto & xi : *x )
   set_lin( xi , *(vit++) );

  f = new LinearFunction( std::move( vp ) );
  }

 obj->set_function( f );
 obj->set_sense( minobj ? Objective::eMin : Objective::eMax , eNoMod );
  
 // set the Objective in the AbstractBlock
 BoxBlock->set_objective( obj );

 //!! check the AbstractBlock
 BoxBlock->is_correct();

 return( BoxBlock );
 }

/*--------------------------------------------------------------------------*/

static FunctionValue get_coeff( void )
{
 // linking constraints coefficients are random in [ -1 , 1 ]
 return( 2 * dis( rg ) - 1 );
 }

/*--------------------------------------------------------------------------*/

static bool SolveBoth( void ) 
{
 try {
  // solve with the 1st Solver- - - - - - - - - - - - - - - - - - - - - - - -
  Solver * Slvr1 = TestBlock->get_registered_solvers().front();
  #if DETACH_1ST
   TestBlock->unregister_Solver( Slvr1 );
   TestBlock->register_Solver( Slvr1 , true );  // push it to the front
  #endif
  int rtrn1st = Slvr1->compute( false );
  bool hs1st = ( ( ( rtrn1st >= Solver::kOK ) && ( rtrn1st < Solver::kError )
                   && ( rtrn1st != Solver::kUnbounded )
                   && ( rtrn1st != Solver::kInfeasible ) )
                 || ( rtrn1st == Solver::kLowPrecision ) );
  double fo1st = hs1st ? Slvr1->get_var_value() : -INF;

  if( TestBlock->get_registered_solvers().size() == 1 ) {
   #if( LOG_LEVEL >= 1 )
    cout << "Solver = ";
    PrintResults( hs1st , rtrn1st , fo1st );
    cout << endl;
   #endif
   return( true );
   }

  // solve with the 2nd Solver- - - - - - - - - - - - - - - - - - - - - - - -
  Solver * Slvr2 = TestBlock->get_registered_solvers().back();
  #if DETACH_2ND
   TestBlock->unregister_Solver( Slvr2 );
   TestBlock->register_Solver( Slvr2 );  // push it to the back
  #endif
  int rtrn2nd = Slvr2->compute( false );

  bool hs2nd = ( ( ( rtrn2nd >= Solver::kOK ) && ( rtrn2nd < Solver::kError )
                   && ( rtrn2nd != Solver::kUnbounded )
                   && ( rtrn2nd != Solver::kInfeasible ) )
                 || ( rtrn2nd == Solver::kLowPrecision ) );
  // double fo2nd = hs2nd ? Slvr2->get_var_value() : -INF;
  // we assume the 2nd solver to be a Lagrangian-based one, which may have
  // issues in producing accurate primal solutions but it should be able to
  // produce accurate dual ones: hence, use the dual bound as the reference
  // value (lower bound if you minimise, upper bound if you maximise)
  double fo2nd = -INF;
  if( hs2nd )
   fo2nd = minobj ? Slvr2->get_lb() : Slvr2->get_ub();

  if( hs1st && hs2nd && ( abs( fo1st - fo2nd ) <= 1e-5 *
			  max( double( 1 ) , max( abs( fo1st ) ,
						  abs( fo2nd ) ) ) ) ) {
   LOG1( "OK(f)" << endl );
   return( true );
   }

  if( ( rtrn1st == Solver::kInfeasible ) &&
      ( rtrn2nd == Solver::kInfeasible ) ) {
    LOG1( "OK(e)" << endl );
    return( true );
    }

  if( ( rtrn1st == Solver::kUnbounded ) &&
      ( rtrn2nd == Solver::kUnbounded ) ) {
   LOG1( "OK(u)" << endl );
   return( true );
   }

  #if( LOG_LEVEL >= 0 )
   cout << "Solver1 = ";
    PrintResults( hs1st , rtrn1st , fo1st );

   cout << " ~ Solver2 = ";
   PrintResults( hs2nd , rtrn2nd , fo2nd );
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
 Index wchg = 15;
 Index nson = 2;
 double dens = 0.1;
 double p_change = 0.5;
 Index n_change = 10;
 Index n_repeat = 40;

 switch( argc ) {
  case( 9 ): Str2Sthg( argv[ 8 ] , p_change );
  case( 8 ): Str2Sthg( argv[ 7 ] , n_change );
  case( 7 ): Str2Sthg( argv[ 6 ] , n_repeat );
  case( 6 ): Str2Sthg( argv[ 5 ] , dens );
  case( 5 ): Str2Sthg( argv[ 4 ] , nson );
  case( 4 ): Str2Sthg( argv[ 3 ] , nvar );
  case( 3 ): Str2Sthg( argv[ 2 ] , wchg );
  case( 2 ): Str2Sthg( argv[ 1 ] , seed );
   break;
 default: cerr << "Usage: " << argv[ 0 ] <<
	   " seed [wchg nvar nson dens #rounds #chng %chng]"
 	<< endl <<
           "       wchg: what to change, coded bit-wise [15]"
		<< endl <<
           "             0 = bounds, 1 = objective"
		<< endl <<
           "             2 = linking coefficients, 3 = linking lhs/rhs"
		<< endl <<
           "       nvar: number of variables [10]"
		<< endl <<
           "       nson: number of sub-Block [2]"
		<< endl <<
           "       dens: number of constraints, fraction of nvar * nson [0.1]"
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

 if( nson < 1 ) {
  cout << "error: nson too small";
  exit( 1 );
  }

 Index m = std::max( Index( ( nvar * nson ) * dens ) , Index( 1 ) );

 rg.seed( seed );  // seed the pseudo-random number generator

 // constructing the data of the problem- - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // choosing whether min or max: toss a(n unbiased, two-sided) coin
 minobj = ( dis( rg ) < 0.5 );
 // choosing whether lin or quad: toss a(n unbiased, two-sided) coin
 //!!isquad = false;
 isquad = ( dis( rg ) < 0.5 );

 #if( LOG_LEVEL >= 1 )
  if( minobj ) cout << "min"; else cout << "max";
  cout << " ~ ";
  if( isquad ) cout << "quad"; else cout << "lin";
  cout << " ~ ";
 #endif
 
 // construction and loading of the objects - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 {
  // ensure all original pointers go out of scope immediately after that
  // the construction has finished

  TestBlock = new AbstractBlock();

  // create the sub-Block and add them
  for( Index k = 0 ; k++ < nson ; )
   TestBlock->add_nested_Block( construct_son() );

  // create m the linking constraints
  auto link = new std::vector< FRowConstraint >( m );

   // each constraint will have at least one and at most 25% of the
   // ColVariable in each son
  Index ps = std::max( Index( 1 ) , nvar / 4 );
  
  for( Index i = 0 ; i < m ; ++i ) {
   LinearFunction::v_coeff_pair vp( ps * nson );
   auto vpit = vp.begin();

   for( Index k = 0 ; k < nson ; ++k ) {
    auto son = TestBlock->get_nested_Block( k );
    auto x = son->get_static_variable_v< ColVariable >( "x" );
    Subset nms( GenerateRand( nvar , ps ) );
    for( auto nm : nms )
     *(vpit++) = coeff_pair( & (*x)[ nm ] , get_coeff() );
    }

   (*link)[ i ].set_function( new LinearFunction( std::move( vp ) ) );

   if( dis( rg ) <= 0.33 ) {   // in 33% of the cases a <= constraint
    (*link)[ i ].set_rhs( dis( rg ) );     // ... with rhs in [ 0 , 1 ]
    (*link)[ i ].set_lhs( -INF );          // ... and lhs = -INF
     }
   else
    if( dis( rg ) <= 0.33 ) {  // in other 33% of the cases a >= constraint
     (*link)[ i ].set_lhs( - dis( rg ) );  // ... with lhs in [ -1 , 0 ]
     (*link)[ i ].set_rhs( INF );          // ... and rhs = INF
     }
    else                       // in all other cases a == 0 constraint
     (*link)[ i ].set_both( 0 );
   }

  // set the linking constraints in the TestBlock
  TestBlock->add_static_constraint( *link , "link" );

  //!! add an empty Objective; this should not be necessary, but
  //!! MILPSolver currently fails to properly set the sense if the
  //!! root Block does not have an Objective, even if empty
  auto obj = new FRealObjective();
  obj->set_function( new LinearFunction() );
  obj->set_sense( minobj ? Objective::eMin : Objective::eMax , eNoMod );
  TestBlock->set_objective( obj );
  }

 // attach the Solver(s) to the Block - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // do this by reading an appropriate BlockSolverConfig from file and
 // apply() it to the TestBlock; note that the BlockSolverConfig is
 // clear()-ed and kept to do the cleanup at the end

 BlockSolverConfig * bsc;
 {
  auto c = Configuration::deserialize( "BSPar.txt" );
  bsc = dynamic_cast< BlockSolverConfig * >( c );
  if( ! bsc ) {
   cerr << "Error: configuration file not a BlockSolverConfig" << endl;
   delete( c );
   exit( 1 );
   }

  bsc->apply( TestBlock );
  bsc->clear();

  if( TestBlock->get_registered_solvers().empty() ) {
   cout << endl << "no Solver registered to the Block!" << endl;
   exit( 1 );
   }
  }

 // open log-file - - - - - - - - - - -  - - - - - - - - - - - - - - - - - -
 //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 #if( LOG_LEVEL >= 2 )
  #if( LOG_ON_COUT )
   ((TestBlock->get_registered_solvers()).back())->set_log( &cout );
  #else
   ofstream LOGFile( logF , ofstream::out );
   if( ! LOGFile.is_open() )
    cerr << "Warning: cannot open log file """ << logF << """" << endl;
   else {
    LOGFile.setf( ios::scientific, ios::floatfield );
    LOGFile << setprecision( 10 );
    ((TestBlock->get_registered_solvers()).back())->set_log( &LOGFile );
    }
  #endif
 #endif

 // first solver call - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 LOG1( "First call: " );

 bool AllPassed = SolveBoth();
 
 // main loop - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // now, for n_repeat times:
 // - up to n_change bounds are changed
 // - up to n_change objective coefficients are changed
 // - up to n_change linking constraint are changed
 //
 // then the two Solver are called to re-solve the BoxBlock

 for( Index rep = 0 ; rep < n_repeat * ( SKIP_BEAT + 1 ) ; ) {
  // select the specific sub-Block to change
  auto BoxBlock = TestBlock->get_nested_Block( rep % nson );
  LOG1( rep << " - BB[" << rep % nson << "]: " );

  // change bounds- - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 1 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = min( nvar , Index( dis( rg ) * n_change ) ) ) {
    LOG1( "changed " << tochange << " bounds - " );

    auto box = BoxBlock->get_static_constraint_v< BoxConstraint >( "box" );
    assert( box );
    const double prob = double( tochange ) / double( nvar );

    for( auto & bi : *box )
     if( dis( rg ) <= prob ) {
      set_bounds( bi );
      if( ! --tochange )
       break;
      }
    }

  // change coefficients- - - - - - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 2 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = min( nvar , Index( dis( rg ) * n_change ) ) ) {
    LOG1( "changed " << tochange << " obj coeffs" );

    Vec_FunctionValue NC( tochange );
    for( auto & nc : NC )
     set_lin_c( nc );

    auto obj = static_cast< FRealObjective * >( BoxBlock->get_objective() );

    if( dis( rg ) <= 0.5 ) {  // in 50% of the cases do a ranged change
     LOG1( "(r) - " );

     Index strt = dis( rg ) * ( nvar - tochange );
     Index stp = strt + tochange;

     if( isquad ) {  // quadratic objective
      auto qf = static_cast< DQuadFunction * >( obj->get_function() );

      /*!!
      Vec_FunctionValue NQC( tochange );
      for( auto & nqc : NQC )
       set_quad_c( nqc );

      if( tochange == 1 )
       qf->modify_term( strt , NQC.front() , NC.front() );
      else
       qf->modify_terms( NQC.begin() , NC.begin() , Range( strt , stp ) );
       !!*/
      if( tochange == 1 )
       qf->modify_linear_coefficient( strt , NC.front() );
      else
       qf->modify_linear_coefficients( std::move( NC ) , Range( strt , stp ) );
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

      /*!!
      Vec_FunctionValue NQC( tochange );
      for( auto & nqc : NQC )
       set_quad_c( nqc );

      if( tochange == 1 )
       qf->modify_term( nms.front() , NQC.front() , NC.front() );
      else
       qf->modify_terms( NQC.begin() , NC.begin() , std::move( nms ) );
       !!*/

      if( tochange == 1 )
       qf->modify_linear_coefficient( nms.front() , NC.front() );
      else
       qf->modify_linear_coefficients( std::move( NC ) , std::move( nms ) );
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

  // change linking constraints - - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 4 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = min( m , Index( dis( rg ) * n_change ) ) ) {
    LOG1( "changed " << tochange << " constraints - " );

   auto link = TestBlock->get_static_constraint_v< FRowConstraint >( "link" );
   Subset nms( GenerateRand( m , tochange ) );
   for( auto nm : nms ) {
    auto lf = static_cast< LinearFunction * >( (*link)[ nm ].get_function() );
    Index av = lf->get_num_active_var();
    Index tcn = std::max( Index( 1 ) , Index( dis( rg ) * av ) );
    Vec_FunctionValue NC( tcn );
    for( auto & nc : NC )
     nc = get_coeff();

    if( dis( rg ) <= 0.5 ) {  // in 50% of the cases do a ranged change
     Index strt = dis( rg ) * ( av - tcn );
     Index stp = strt + tcn;

     if( tcn == 1 )
       lf->modify_coefficient( strt , NC.front() );
     else
       lf->modify_coefficients( std::move( NC ) , Range( strt , stp ) );
     }
    else {  // in the other 50% of the cases, do a sparse change
     Subset nmsn( GenerateRand( av , tcn ) );

     if( tcn == 1 )
      lf->modify_coefficient( nmsn.front() , NC.front() );
     else
      lf->modify_coefficients( std::move( NC ) , std::move( nmsn ) );
     }
    }
   }

  // change linking lhs/rhs - - - - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 8 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = min( m , Index( dis( rg ) * n_change ) ) ) {
    LOG1( "changed " << tochange << " lhs/rhs - " );

    double prob = double( tochange ) / double( m );
    auto link = TestBlock->get_static_constraint_v< FRowConstraint >( "link"
								      );
   for( auto & li : *link ) {
    if( dis( rg ) > prob )
     continue;

    auto lhs = li.get_lhs();
    auto rhs = li.get_lhs();
    if( lhs == rhs )
     continue;

    if( lhs == -INF )
     li.set_rhs( dis( rg ) );
    else
     li.set_lhs( - dis( rg ) );

    if( ! --tochange )
     break;
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
 
 // destroy the Block - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 // apply() the clear()-ed BlockSolverConfig to cleanup Solver
 bsc->apply( TestBlock );

 // then delete the BlockSolverConfig
 delete( bsc );

 // finally the AbstractBlock can be deleted
 delete( TestBlock );

 // terminate - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 return( AllPassed ? 0 : 1 );

 }  // end( main )

/*--------------------------------------------------------------------------*/
/*------------------------ End File test.cpp -------------------------------*/
/*--------------------------------------------------------------------------*/
