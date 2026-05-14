/*--------------------------------------------------------------------------*/
/*-------------------------- common_utils.cpp ------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Implementations of the utility functions declared in common_utils.h.
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

#include "common_utils.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <limits>
#include <list>
#include <map>
#include <typeinfo>

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

void PrintResults( bool hs , int rtrn , double fo )
{
 if( hs ) {
  std::cout.setf( std::ios::scientific , std::ios::floatfield );
  std::cout << def << fo;
  }
 else
  if( rtrn == Solver::kInfeasible )
   std::cout << "    Unfeas";
  else
   if( rtrn == Solver::kUnbounded )
    std::cout << "      Unbounded";
   else
    std::cout << "      Error!";
 }

/*--------------------------------------------------------------------------*/

void smspp_terminate( void )
{
 std::cerr << "Uncaught exception in executing SMS++:\n";
 try {
  std::rethrow_exception( std::current_exception() );
  }
 catch( const std::exception & e ) {
  std::cerr << "\tException type: " << typeid( e ).name() << "\n";
  std::cerr << "\tException message: " << e.what() << "\n";
  }
 catch( ... ) {
  std::cerr << "\tUnknown exception" << std::endl;
  }
 std::abort();  // or exit( 1 )
 }

/*--------------------------------------------------------------------------*/

void b_config_Block( Block * block , Configuration * b_config ,
                     const std::string & fn )
{
 // std::list rather than std::vector since it's built by push_back and
 // only trasversed head-to-tail
 std::list< Block * > BFS;

 // handle the special case of a "meta" BlockConfig
 if( auto * mb =
     dynamic_cast< SimpleConfiguration< std::map< std::string ,
                                                  Configuration * > >
                                        * >( b_config ) ) {

  // construct the list of all Block inside block
  BFS.push_back( block );
  for( auto bit = BFS.begin() ; bit != BFS.end() ; ++bit )
   for( auto el : ( *bit )->get_nested_Blocks() )
    BFS.push_back( el );

  auto & map = mb->f_value;

  // now BlockConfig-ure all Block whose classname() matches
  for( auto b : BFS )
   if( auto bcit = map.find( b->classname() ); bcit != map.end() ) {
    if( auto bc = dynamic_cast< BlockConfig * >( bcit->second ) ) {
     auto cbc = bc->clone();
     cbc->apply( b );
     delete cbc;
     }
    else {
     std::cerr << "Error: meta-Configuration for :Block " << bcit->first
               << " in file " << fn << " is not a BlockConfig" << std::endl;
     exit( 1 );
     }
    }

  return;  // all done
  }

 if( auto * bc = dynamic_cast< BlockConfig * >( b_config ) ) {
  bc->apply( block );  // just apply() it
  return;              // all done
  }

 std::cerr << "Error: " << fn
           << " does not contain a valid [meta]BlockConfig" << std::endl;
 exit( 1 );

 }  // end( b_config_Block )

/*--------------------------------------------------------------------------*/

void s_config_Block( Block * block , Configuration * s_config ,
                     const std::string & fn )
{
 // std::list rather than std::vector since it's built by push_back and
 // only trasversed head-to-tail
 std::list< Block * > BFS;

 // handle the special case of a "meta" BlockSolverConfig
 if( auto * mb =
     dynamic_cast< SimpleConfiguration< std::map< std::string ,
                                                  Configuration * > >
                                        * >( s_config ) ) {

  // construct the list of all Block inside block
  BFS.push_back( block );
  for( auto bit = BFS.begin() ; bit != BFS.end() ; ++bit )
   for( auto el : ( *bit )->get_nested_Blocks() )
    BFS.push_back( el );

  auto & map = mb->f_value;

  // now BlockSolverConfig-ure all Block whose classname() matches
  for( auto b : BFS )
   if( auto bcit = map.find( b->classname() ); bcit != map.end() ) {
    if( auto bsc = dynamic_cast< BlockSolverConfig * >( bcit->second ) )
     bsc->apply( b );
    else {
     std::cerr << "Error: meta-Configuration for :Block " << bcit->first
               << " in file " << fn << " is not a BlockSolverConfig"
               << std::endl;
     exit( 1 );
     }
    }

  // finally, clear() all the BlockSolverConfig for final cleanup
  for( auto & el : map )
   (el.second)->clear();

  return;  // all done
  }

 if( auto * bsc = dynamic_cast< BlockSolverConfig * >( s_config ) ) {
  bsc->apply( block );  // just apply() it
  bsc->clear();         // clear() it for final cleanup
  return;               // all done
  }

 std::cerr << "Error: " << fn
           << " does not contain a valid [meta]BlockSolverConfig"
           << std::endl;
 exit( 1 );

 }  // end( s_config_Block )

/*--------------------------------------------------------------------------*/

double get_obj_value( Solver * slvr , ObjGetter g )
{
 switch( g ) {
  case ObjGetter::VarValue:   return( slvr->get_var_value() );
  case ObjGetter::LowerBound: return( slvr->get_lb() );
  case ObjGetter::UpperBound: return( slvr->get_ub() );
  }
 return( std::numeric_limits< double >::quiet_NaN() );  // unreachable
 }

/*--------------------------------------------------------------------------*/

bool SolveBoth( Block * block ,
                ObjGetter g1 ,
                ObjGetter g2 ,
                bool one_sided_le ,
                double tol ,
                double * out_fo1 ,
                bool   * out_hs1 ,
                double * out_time1 ,
                long   * out_it1 )
{
 constexpr double INF = std::numeric_limits< double >::has_infinity
                        ? std::numeric_limits< double >::infinity()
                        : std::numeric_limits< double >::max();

 try {
  // solve with the 1st Solver- - - - - - - - - - - - - - - - - - - - - - - -
  auto start = std::chrono::system_clock::now();
  Solver * Slvr1 = block->get_registered_solvers().front();
  int rtrn1st = Slvr1->compute( false );
  auto end = std::chrono::system_clock::now();
  std::chrono::duration< double > elapsed = end - start;
  auto time1 = elapsed.count();

  bool hs1st = ( ( ( rtrn1st >= Solver::kOK ) && ( rtrn1st < Solver::kError )
                   && ( rtrn1st != Solver::kUnbounded )
                   && ( rtrn1st != Solver::kInfeasible ) )
                 || ( rtrn1st == Solver::kLowPrecision ) );

  double fo1st = hs1st ? get_obj_value( Slvr1 , g1 ) : -INF;

  if( out_fo1 )   *out_fo1   = fo1st;
  if( out_hs1 )   *out_hs1   = hs1st;
  if( out_time1 ) *out_time1 = time1;
  if( out_it1 )   *out_it1   = Slvr1->get_elapsed_iterations();

  if( block->get_registered_solvers().size() == 1 ) {
   std::cout << fixd << time1 << "\t" << Slvr1->get_elapsed_iterations()
             << "\t";
   PrintResults( hs1st , rtrn1st , fo1st );
   std::cout << std::endl;
   return( hs1st );
   }

  // solve with the 2nd Solver- - - - - - - - - - - - - - - - - - - - - - - -
  start = std::chrono::system_clock::now();
  Solver * Slvr2 = block->get_registered_solvers().back();
  int rtrn2nd = Slvr2->compute( false );
  end = std::chrono::system_clock::now();
  elapsed = end - start;
  auto time2 = elapsed.count();
  std::cout << fixd << time1 << " - " << time2 << " - ";

  bool hs2nd = ( ( ( rtrn2nd >= Solver::kOK ) && ( rtrn2nd < Solver::kError )
                   && ( rtrn2nd != Solver::kUnbounded )
                   && ( rtrn2nd != Solver::kInfeasible ) )
                 || ( rtrn2nd == Solver::kLowPrecision ) );
  double fo2nd = -INF;

  if( hs1st && hs2nd ) {
   fo2nd = get_obj_value( Slvr2 , g2 );
   bool OK;
   if( one_sided_le )
    // ProxHeur-style: fo2nd >= fo1st (within absolute tol)
    OK = ( fo1st - fo2nd <= tol );
   else
    OK = ( std::abs( fo1st - fo2nd ) <=
           tol * std::max( double( 1 ) , std::max( std::abs( fo1st ) ,
                                                   std::abs( fo2nd ) ) ) );

   if( OK ) {
    std::cout << "OK(f)" << std::endl;
    return( true );
    }
   }

  if( ( rtrn1st == Solver::kInfeasible ) &&
      ( rtrn2nd == Solver::kInfeasible ) ) {
   std::cout << "OK(e)" << std::endl;
   return( true );
   }

  if( ( rtrn1st == Solver::kUnbounded ) &&
      ( rtrn2nd == Solver::kUnbounded ) ) {
   std::cout << "OK(u)" << std::endl;
   return( true );
   }

  std::cout << "Solver1 = ";
  PrintResults( hs1st , rtrn1st , fo1st );
  std::cout << " ~ Solver2 = ";
  PrintResults( hs2nd , rtrn2nd , fo2nd );
  std::cout << std::endl;

  return( false );
  }
 catch( std::exception & e ) {
  std::cerr << e.what() << std::endl;
  exit( 1 );
  }
 catch( ... ) {
  std::cerr << "Error: unknown exception thrown" << std::endl;
  exit( 1 );
  }
 }

/*--------------------------------------------------------------------------*/

bool CheckRefValue( double fo , double ref ,
                    double rel_tol ,
                    double time1 , long iters )
{
 double maxv = std::max( double( 1 ) ,
                         std::max( std::abs( fo ) , std::abs( ref ) ) );
 double diff = std::abs( fo - ref );
 double tol = rel_tol * maxv;

 bool OK = ( diff <= tol );

 std::cout << fixd << time1 << "\t" << iters << "\t"
           << def << fo
           << " ~ Ref = " << def << ref
           << " (|diff| = " << def << diff
           << ( OK ? ", OK" : ", KO" ) << ")" << std::endl;

 return( OK );
 }

/*--------------------------------------------------------------------------*/

bool SolveAndCheckRef( Block * block , double ref ,
                       ObjGetter g ,
                       double rel_tol )
{
 constexpr double INF = std::numeric_limits< double >::has_infinity
                        ? std::numeric_limits< double >::infinity()
                        : std::numeric_limits< double >::max();

 try {
  auto start = std::chrono::system_clock::now();
  Solver * Slvr1 = block->get_registered_solvers().front();
  int rtrn1st = Slvr1->compute( false );
  auto end = std::chrono::system_clock::now();
  std::chrono::duration< double > elapsed = end - start;
  auto time1 = elapsed.count();

  bool hs1st = ( ( ( rtrn1st >= Solver::kOK ) && ( rtrn1st < Solver::kError )
                   && ( rtrn1st != Solver::kUnbounded )
                   && ( rtrn1st != Solver::kInfeasible ) )
                 || ( rtrn1st == Solver::kLowPrecision ) );

  if( ! hs1st ) {
   std::cout << "Solver returned code " << rtrn1st << std::endl;
   return( false );
   }

  double fo1st = get_obj_value( Slvr1 , g );

  return( CheckRefValue( fo1st , ref , rel_tol ,
                         time1 , Slvr1->get_elapsed_iterations() ) );
  }
 catch( std::exception & e ) {
  std::cerr << e.what() << std::endl;
  exit( 1 );
  }
 catch( ... ) {
  std::cerr << "Error: unknown exception thrown" << std::endl;
  exit( 1 );
  }
 }

/*--------------------------------------------------------------------------*/
/*------------------------ End common_utils.cpp ----------------------------*/
/*--------------------------------------------------------------------------*/
