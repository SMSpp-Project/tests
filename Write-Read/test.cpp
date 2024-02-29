/*--------------------------------------------------------------------------*/
/*-------------------- File write-read-test.cpp ----------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Main for testing read_mps feature
 *
 * A "random" PolyhedralFunction is constructed and represented in terms
 * of linear inequalities for an otherwise "empty" AbstractBlock. The
 * Block is then solved by a CPXMILPSolver which also produce a .mps file with 
 * the data of the model. This file is then read again from another 
 * AbstractBlock and solved by a (possibly) different LP Solver. The results
 * are then compared to assess the equality between the written and read model. 
 * The first Block is then repeatedly randomly modified, and each time the 
 * same procedure is applied.
 *
 * \author Antonio Frangioni \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 * 
 * \author Enrico Calandrini \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Antonio Frangioni, Enrico Calandrini
 */
/*--------------------------------------------------------------------------*/
/*-------------------------------- MACROS ----------------------------------*/
/*--------------------------------------------------------------------------*/

#define LOG1( x ) cout << x
#define CLOG1( y , x ) if( y ) cout << x

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

#include "MILPSolver.h"

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

// Choose here the format of the output file:
// L stands for LP-file
// M stands for MPS-file
char format = 'L';

AbstractBlock * LPBlock;   // the problem expressed as an LP
AbstractBlock * secondLPBlock;   // the problem expressed as an LP

bool convex = true;        // true if the PolyhedralFunction is convex

double bound = 1000;       // a tentative bound to detect unbounded instances

FunctionValue BND;         // the bound in the PolyhedralFunction (if any)

Index nvar = 10;           // number of variables

#define nsvar nvar        // all variables are static

Index m;                   // number of rows

std::mt19937 rg;           // base random generator
std::uniform_real_distribution<> dis( 0.0 , 1.0 );

MultiVector A;
RealVector b;

ColVariable * vLP;                 // pointer to v LP variable

std::vector< ColVariable > * xLP;  // pointer to (static) x LP variables

std::list< BoxConstraint > * LPbnd;   // BoxConstraint for LPBlock

int rtrnfirstLP; // status returned by first optimization
bool hsfirstLP; // wheter or not the first optimization produced an optimal solution
double fofirstLP; // optimal solution of first optimization

int rtrnsecondLP; // status returned by second optimization
bool hssecondLP; // wheter or not the second optimization produced an optimal solution
double fosecondLP; // optimal solution of second optimization


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

static inline void SetNN( ColVariable & LPxi )
{ 
 if( dis( rg ) < 0.5 ) {
  LPxi.is_positive( true , eNoMod );
  }
 }

/*--------------------------------------------------------------------------*/

static inline void SetBox( ColVariable & LPxi )
{
 if( dis( rg ) < 0.5 ) {
  LPbnd->resize( LPbnd->size() + 1 );
  LPbnd->back().set_variable( & LPxi );
  auto p = dis( rg );
  double lhs, rhs;
  lhs = p < 0.666 ? 0 : -INF;
  rhs = p > 0.333 ? dis( rg ) : INF;
  LPbnd->back().set_lhs( lhs , eNoMod );
  LPbnd->back().set_rhs( rhs , eNoMod );
  }
 else
  SetNN( LPxi );
 }

/*--------------------------------------------------------------------------*/

static bool SolveFirst( void ) 
{
 try {
  // solve the LPBlock- - - - - - - - - - - - - - - - - - - - - - - - - - - -
  Solver * slvrLP = (LPBlock->get_registered_solvers()).front();
  rtrnfirstLP = slvrLP->compute( false );
  hsfirstLP = ( ( rtrnfirstLP >= Solver::kOK ) && ( rtrnfirstLP < Solver::kError ) )
              || ( rtrnfirstLP == Solver::kLowPrecision );
  fofirstLP = hsfirstLP ? ( convex ? slvrLP->get_ub() : slvrLP->get_lb() )
                     : ( convex ? INF : -INF );

  if( hsfirstLP ) {
   LOG1( "OK(f) - " );
   LOG1( "First optimization produced an optimal solution : " << fofirstLP << endl );
   return( true );
   }

  if( rtrnfirstLP == Solver::kInfeasible ) {
    LOG1( "OK(?e?) - " );
    LOG1( "First optimization produced an unfeasible model" << endl );
    return( true );
    }

  if( rtrnfirstLP == Solver::kUnbounded ) {
   LOG1( "OK(u) - " );
   LOG1( "First optimization produced an unbounded model" << endl );
   return( true );
   }

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

static bool SolveSecond( void ) 
{
 try {
  // solve the LPBlock- - - - - - - - - - - - - - - - - - - - - - - - - - - -
  Solver * slvrLP = (secondLPBlock->get_registered_solvers()).front();
  rtrnsecondLP = slvrLP->compute( false );
  hssecondLP = ( ( rtrnsecondLP >= Solver::kOK ) && ( rtrnsecondLP < Solver::kError ) )
              || ( rtrnsecondLP == Solver::kLowPrecision );

  /* NOTE:
  ** When a model is saved in a .mps format, it is assumed to be a minimization
  ** problem. Thus, if the original problem was a maximization one, the 
  ** inverse of objective coefficients are evaluated and then printed. 
  ** However, to have a complete equivalence between the printed model and 
  ** the read one we need to take the inverse of the objective value 
  ** obtained. For this reason, if the starting model is not convex, we 
  ** consider as objective value of the read model: - slvrLP->get_lb() */
  if( format == 'M' )
    fosecondLP = hssecondLP ? ( convex ? slvrLP->get_ub() : - slvrLP->get_lb() )
                     : ( convex ? INF : -INF );
  else if( format == 'L' )
    fosecondLP = hssecondLP ? ( convex ? slvrLP->get_ub() : slvrLP->get_lb() )
                     : ( convex ? INF : -INF );

  if( hssecondLP ) {
   LOG1( "OK(f) - " );
   LOG1( "Second optimization produced an optimal solution : " << fosecondLP << endl );
   }

  if( rtrnsecondLP == Solver::kInfeasible ) {
    LOG1( "OK(?e?) - " );
    LOG1( "Second optimization produced an unfeasible model" << endl );
    }

  if( rtrnsecondLP == Solver::kUnbounded ) {
   LOG1( "OK(u) - " );
   LOG1( "Second optimization produced an unbounded model" << endl );
   }

   if( hsfirstLP && hssecondLP && ( abs( fofirstLP - fosecondLP ) <= 2e-7 *
			 max( double( 1 ) , abs( max( fofirstLP , fosecondLP ) ) ) ) )
    return( true );

  if( ( rtrnfirstLP == Solver::kInfeasible ) &&
      ( rtrnsecondLP == Solver::kInfeasible ) )
    return( true );

  if( ( rtrnfirstLP == Solver::kUnbounded ) &&
      ( rtrnsecondLP == Solver::kUnbounded ) )
   return( true );

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

 long int seed = 0;
 Index wchg = 31;
 double dens = 4;
 double p_change = 0.5;
 Index n_change = 10;
 Index n_repeat = 10;

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
           "       wchg: what to change, coded bit-wise [31]"
		<< endl <<
           "             0 = add rows, 1 = delete rows "
		<< endl <<
           "             2 = modify rows, 3 = modify constants"
		<< endl <<
           "             4 = change global lower/upper bound"
        << endl <<
        "       nvar: number of variables [10]"
        << endl <<
        "       dens: rows / variables [4]"
        << endl <<
        "       #rounds: how many iterations [10]"
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

 // construction and loading of the objects - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 // construct the LP- - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 {
  // ensure all original pointers go out of scope immediately after that
  // the construction has finished
  // ensure all original pointers go out of scope immediately after that
  // the construction has finished

  LPBlock = new AbstractBlock();

  // construct the Variable
  xLP = new std::vector< ColVariable >( nsvar );

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
  LPBlock->add_dynamic_constraint( *ALP , "cuts" );
  LPBlock->add_static_constraint( *LBc , "vbnd" );
  LPBlock->set_objective( objLP );
  }

 // define bound constraints- - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 {
  LPbnd = new std::list< BoxConstraint >;
  auto & LPx = *(LPBlock->get_static_variable_v< ColVariable >( "x" ));
  for( Index i = 0 ; i < nsvar ; ++i )
   SetBox( LPx[ i ] );
  }

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

 // open log-file - - - - - - - - - - -  - - - - - - - - - - - - - - - - - -
 //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // write the .mps file which will be then read again from another AbstractBlock
 // and resolved
 
 std::string output_name = "LPBlock";
 if( format == 'L')
  output_name = output_name + ".lp";
 else if( format == 'M' )
  output_name = output_name + ".mps";

 ((LPBlock->get_registered_solvers()).front())->set_par(
	                         MILPSolver::strOutputFile , output_name );

 // first solver call - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 LOG1( "First call: " );

 SolveFirst();

// construct the second LP by simply reading the previous written model - - -
 {
  secondLPBlock = new AbstractBlock();
  
  std::ifstream file;
  file.open(output_name);
  secondLPBlock->load( file , format );
 }

 // attach the Solver to the Block- - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // for both Block do this by reading an appropriate BlockSolverConfig from
 // file and apply() it to the Block; note that the BlockSolverConfig are
 // clear()-ed and kept to do the cleanup at the end

 BlockSolverConfig * secondlpbsc;
 {
  auto c = Configuration::deserialize( "SecondLPPar.txt" );
  secondlpbsc = dynamic_cast< BlockSolverConfig * >( c );
  if( ! secondlpbsc ) {
   cerr << "Error: SecondLPPar.txt does not contain a BlockSolverConfig" << endl;
   delete( c );
   exit( 1 );
   }
  }

 secondlpbsc->apply( secondLPBlock );

 // open log-file - - - - - - - - - - -  - - - - - - - - - - - - - - - - - -
 //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // write the .mps file which will be then compared with the previous one
 ((secondLPBlock->get_registered_solvers()).front())->set_par(
	                         MILPSolver::strOutputFile , "SecondLPBlock.lp" );

 // second solver call - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 bool AllPassed = SolveSecond();

 for( Index rep = 0 ; rep < n_repeat ; ) {
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

    std::list< FRowConstraint > nc( tochange );
    auto ncit = nc.begin();
    for( Index i = 0 ; i < tochange ; )
     ConstructLPConstraint( i++ , *(ncit++) );
    auto cnst = LPBlock->get_dynamic_constraint< FRowConstraint >( "cuts" );
    LPBlock->add_dynamic_constraints( *cnst , nc );

    // update m
    m += tochange;

    // sanity checks
    PANIC( m == cnst->size() );
    }

  // delete rows- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 2 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = std::min( m - 1 , Index( dis( rg ) * n_change ) ) ) {
    LOG1( "deleted " << tochange << " rows" );

    auto cnst = LPBlock->get_dynamic_constraint< FRowConstraint >( "cuts" );
    
    if( dis( rg ) <= 0.5 ) {  // in 50% of the cases do a ranged change
     LOG1( "(r) - " );

     Index strt = dis( rg ) * ( m - tochange );
     Index stp = strt + tochange;

     // remove them from the LP
     LPBlock->remove_dynamic_constraints( *cnst , Range( strt , stp ) );
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
     }

    // update m
    m -= tochange;

    // sanity checks
    PANIC( m == cnst->size() );
    }

  // modify rows- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 4 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = std::min( m , Index( dis( rg ) * n_change ) ) ) {
    LOG1( "modified " << tochange << " rows" );

    GenerateAb( tochange , nvar );

    vLP = LPBlock->get_static_variable< ColVariable >( "v" );
    xLP = LPBlock->get_static_variable_v< ColVariable >( "x" );
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

     }
    }

  // modify constants - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  if( ( wchg & 8 ) && ( dis( rg ) <= p_change ) )
   if( Index tochange = std::min( m , Index( dis( rg ) * n_change ) ) ) {
    LOG1( "modified " << tochange << " constants" );

    Generateb( tochange );
     
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
   }

    // open log-file - - - - - - - - - - -  - - - - - - - - - - - - - - - - - -
    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    // write the .mps file which will be then read again from another AbstractBlock
    // and resolved

    std::string rep_name = "LPBlock-" + std::to_string( rep );
    if( format == 'L')
     rep_name = rep_name + ".lp";
    else if( format == 'M' )
     rep_name = rep_name + ".mps";

    ((LPBlock->get_registered_solvers()).front())->set_par(
                                MILPSolver::strOutputFile , rep_name );

    // finally, re-solve the problems with the first solver - - - - - - - - - - -
    // ... every SKIP_BEAT + 1 rounds
    SolveFirst();

    // construct the second LP by simply reading the previous written model - - -
    {
    secondLPBlock = new AbstractBlock();

    std::ifstream file;
    file.open(rep_name);
    secondLPBlock->load( file , format );
    }

    // attach the Solver to the Block- - - - - - - - - - - - - - - - - - - - - -
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    // for both Block do this by reading an appropriate BlockSolverConfig from
    // file and apply() it to the Block; note that the BlockSolverConfig are
    // clear()-ed and kept to do the cleanup at the end
    secondlpbsc->apply( secondLPBlock );

    // open log-file - - - - - - - - - - -  - - - - - - - - - - - - - - - - - -
    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    // write the .mps file which will be then compared with the previous one
    ((secondLPBlock->get_registered_solvers()).front())->set_par(
                                MILPSolver::strOutputFile , "Second" + rep_name );

    // second solver call - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    bool AllPassed = SolveSecond();

    ++rep;

    }  // end( main loop )- - - - - - - - - - - - - - - - - - - - - - - - - - -
        // - - -

 if( AllPassed )
  cout << GREEN( All tests passed!! ) << endl;
 else
  cout << RED( Shit happened!! ) << endl;
 
 // destroy the Block - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 secondlpbsc->clear();

 // apply() the clear()-ed BlockSolverConfig to cleanup Solver
 lpbsc->apply( LPBlock );
 secondlpbsc->apply( secondLPBlock );

 // then delete the BlockSolverConfig
 delete( lpbsc );
 delete( secondlpbsc );

 // delete the Blocks
 delete( LPBlock );
 delete( secondLPBlock );

 // terminate - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 return( AllPassed ? 0 : 1 );

 }  // end( main )

/*--------------------------------------------------------------------------*/
/*------------------------ End File test.cpp -------------------------------*/
/*--------------------------------------------------------------------------*/
