/*--------------------------------------------------------------------------*/
/*-------------------------- File test.cpp ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Main for testing the infeasibility certificate of the :MILPSolver
 *
 * When a Block is infeasible a Solver may provide a Farkas certificate, the
 * dual ray proving that no feasible point exists, which is what a Benders
 * scheme turns into a feasibility cut. Each :MILPSolver reads it from its own
 * solver, whose sign convention is its own: Gurobi hands out FARKASDUAL with
 * the sign opposite to that of its optimal duals, SCIP likewise, CPLEX and
 * HiGHS are meant to already agree. Whoever consumes the certificate,
 * however, reads it off the Constraint with the very same code that reads the
 * optimal duals, and therefore needs the two to agree: a certificate that
 * arrives with the wrong sign is a cut in the wrong direction, which is worse
 * than no cut at all.
 *
 * This is what is checked here, on a linear program small enough that its
 * certificate is unique up to a positive factor. The same rows are given
 * twice, once with a right-hand side that makes the program feasible and once
 * with one that makes it infeasible; every registered :MILPSolver is asked
 * for the optimal duals of the former and for the certificate of the latter,
 * and the two must have the same sign on the same row. The check thus needs
 * to know no solver's convention, only that a solver keeps one.
 *
 * The program is
 *
 *     min  x + y  s.t.  2x +  y >= 3 ,  x + 2y >= 3 ,  x + y <= u ,  x , y >= 0
 *
 * with u = 3 in the feasible instance, whose optimum is x = y = 1, and u = 7/5
 * in the infeasible one. It is written this way on purpose: a certificate is
 * a ray of the dual simplex, and so it exists only where the infeasibility is
 * proved by the simplex. Were the two sides of a single variable to conflict,
 * every solver would see the empty domain by bound propagation alone and stop
 * before the first LP iteration, with no dual basis to build the ray from.
 * Here the activity bounds conclude nothing, u bounding each variable by 7/5
 * and the largest activity of the first row being 21/5, above its 3; the
 * infeasibility only shows as a combination, a third of the first row plus a
 * third of the second giving x + y >= 2 against x + y <= 7/5.
 *
 * The solvers that state they have no certificate to give are reported and
 * skipped, that being a legitimate answer: the infeasibility may have been
 * proved by the presolve, with no dual basis to build a ray from.
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

#include <iomanip>
#include <iostream>
#include <vector>

#include "AbstractBlock.h"

#include "FRealObjective.h"

#include "FRowConstraint.h"

#include "LinearFunction.h"

#include "MILPSolver.h"

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*----------------------------- CONSTANTS ----------------------------------*/
/*--------------------------------------------------------------------------*/

/// the :MILPSolver to try, each skipped if it is not in the Solver factory

static const std::vector< std::string > SolverNames =
 { "CPXMILPSolver" , "GRBMILPSolver" , "SCIPMILPSolver" , "HiGHSMILPSolver" };

/// a dual value below this in absolute terms is taken to be zero

static constexpr double Eps = 1e-9;

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

/// builds the program of the file comment with the given right-hand side
/** Every Farkas certificate of the infeasible instance is a positive multiple
 * of ( 1/3 , 1/3 , 1 ): whichever way a solver scales it, the sign of each
 * component is determined, and so the comparison against the optimal duals of
 * the feasible instance is well posed. */

static AbstractBlock * build( double u ,
                              std::vector< FRowConstraint > * & rows )
{
 auto ab = new AbstractBlock();

 auto x = new std::vector< ColVariable >( 2 );
 for( auto & v : *x ) {
  v.set_type( ColVariable::kContinuous );
  v.is_positive( true );
  }
 ab->add_static_variable( *x , "x" );

 rows = new std::vector< FRowConstraint >( 3 );

 // 2x + y >= 3
 (*rows)[ 0 ].set_function( new LinearFunction(
  { std::make_pair( &(*x)[ 0 ] , 2.0 ) ,
    std::make_pair( &(*x)[ 1 ] , 1.0 ) } ) );
 (*rows)[ 0 ].set_lhs( 3 );
 (*rows)[ 0 ].set_rhs( Inf< double >() );

 // x + 2y >= 3
 (*rows)[ 1 ].set_function( new LinearFunction(
  { std::make_pair( &(*x)[ 0 ] , 1.0 ) ,
    std::make_pair( &(*x)[ 1 ] , 2.0 ) } ) );
 (*rows)[ 1 ].set_lhs( 3 );
 (*rows)[ 1 ].set_rhs( Inf< double >() );

 // x + y <= u
 (*rows)[ 2 ].set_function( new LinearFunction(
  { std::make_pair( &(*x)[ 0 ] , 1.0 ) ,
    std::make_pair( &(*x)[ 1 ] , 1.0 ) } ) );
 (*rows)[ 2 ].set_lhs( -Inf< double >() );
 (*rows)[ 2 ].set_rhs( u );

 for( auto & c : *rows )
  c.set_Block( ab );

 ab->add_static_constraint( *rows , "rows" );

 auto obj = new FRealObjective( ab , new LinearFunction(
  { std::make_pair( &(*x)[ 0 ] , 1.0 ) ,
    std::make_pair( &(*x)[ 1 ] , 1.0 ) } ) );
 obj->set_sense( Objective::eMin );
 ab->set_objective( obj );

 ab->generate_abstract_variables();
 ab->generate_abstract_constraints();
 ab->generate_objective();

 return( ab );
 }

/*--------------------------------------------------------------------------*/

/// the sign of \p v, zero when it is below the threshold

static int sgn( double v )
{
 if( v >  Eps ) return(  1 );
 if( v < -Eps ) return( -1 );
 return( 0 );
 }

/*--------------------------------------------------------------------------*/
/*-------------------------------- MAIN ------------------------------------*/
/*--------------------------------------------------------------------------*/

int main( void )
{
 bool all_passed = true;
 bool any_solver = false;

 // the sign the first row's multiplier has in the certificate, as agreed on
 // by the solvers seen so far; Inf marks "nobody has spoken yet"
 int agreed_sign = 0;
 std::string agreed_by;

 for( const auto & name : SolverNames ) {

  auto solver = Solver::new_Solver( name );
  if( ! solver )
   continue;                 // this one is not in the build, nothing to say
  any_solver = true;

  // silence the solver's own log, and ask for the duals: SCIP does not
  // compute them unless told to, and refuses to hand them out otherwise
  solver->set_par( MILPSolver::intLogVerb , 0 );

  // a certificate is a ray of the dual simplex, so it exists only if the
  // infeasibility is proved by the simplex: on a program this small every
  // presolve gets there first, and then there is no dual basis to build it
  // from. Each solver spells the switch its own way, and the ones it does
  // not know are skipped
  auto try_int = [ &solver ]( const char * name , int value ) {
   const auto par = solver->int_par_str2idx( name );
   if( par < Inf< Solver::idx_type >() )
    solver->set_par( par , value );
   };
  auto try_str = [ &solver ]( const char * name , const char * value ) {
   const auto par = solver->str_par_str2idx( name );
   if( par < Inf< Solver::idx_type >() )
    solver->set_par( par , std::string( value ) );
   };

  try_int( "intComputeDuals" , 1 );                     // SCIP
  try_int( "Presolve" , 0 );                            // Gurobi
  try_int( "InfUnbdInfo" , 1 );                         // Gurobi
  try_int( "CPXPARAM_Preprocessing_Presolve" , 0 );     // CPLEX
  try_int( "presolving/maxrounds" , 0 );                // SCIP
  try_str( "presolve" , "off" );                        // HiGHS
  try_str( "solver" , "simplex" );                      // HiGHS

  std::cout << std::left << std::setw( 16 ) << name;

  // the feasible instance, whose optimum is x = y = 1 - - - - - - - - - - -

  std::vector< FRowConstraint > * frows;
  auto feasible = build( 3 , frows );
  feasible->register_Solver( solver );

  auto status = solver->compute();
  int solution_sign = 0;

  if( status != Solver::kOK )
   std::cout << " the feasible instance is not solved (status "
             << status << ")";
  else {
   auto cda = dynamic_cast< CDASolver * >( solver );
   try {
    if( cda && cda->has_dual_solution() ) {
     cda->get_dual_solution();
     solution_sign = sgn( (*frows)[ 0 ].get_dual() );
     }
    }
   catch( const std::exception & e ) {
    std::cout << " the optimal duals are refused: " << e.what();
    }
   }

  feasible->unregister_Solver( solver );
  delete feasible;

  if( ! solution_sign ) {
   std::cout << " no optimal dual on the binding row, nothing to compare"
             << std::endl;
   all_passed = false;
   delete solver;
   continue;
   }

  // the infeasible instance - - - - - - - - - - - - - - - - - - - - - - - -

  std::vector< FRowConstraint > * irows;
  auto infeasible = build( 1.4 , irows );
  infeasible->register_Solver( solver );

  status = solver->compute();

  if( status != Solver::kInfeasible ) {
   std::cout << " does not report the instance as infeasible (status "
             << status << ")" << std::endl;
   all_passed = false;
   }
  else {
   auto cda = dynamic_cast< CDASolver * >( solver );

   bool has = false;
   try { has = cda && cda->has_dual_direction(); }
   catch( const std::exception & e ) {
    std::cout << " has_dual_direction() throws: " << e.what() << std::endl;
    all_passed = false;
    }

   if( ! has )
    // a legitimate answer: without a dual basis there is no ray to give
    std::cout << " has no certificate to give" << std::endl;
   else {
    cda->get_dual_direction();

    const auto d0 = (*irows)[ 0 ].get_dual();
    const auto d1 = (*irows)[ 1 ].get_dual();
    const auto d2 = (*irows)[ 2 ].get_dual();

    if( ( ! sgn( d0 ) ) && ( ! sgn( d1 ) ) && ( ! sgn( d2 ) ) ) {
     std::cout << " gives an all-zero certificate" << std::endl;
     all_passed = false;
     }
    else {
     std::cout << " certificate ( " << std::showpos << std::setw( 9 ) << d0
               << " , " << std::setw( 9 ) << d1
               << " , " << std::setw( 9 ) << d2 << std::noshowpos << " )";

     // the certificate must have, on the same row, the sign the optimal dual
     // has: whoever reads it does so with the same code
     if( sgn( d0 ) != solution_sign ) {
      std::cout << " -> KO, the sign is not that of the optimal dual";
      all_passed = false;
      }
     else if( agreed_sign && ( sgn( d0 ) != agreed_sign ) ) {
      std::cout << " -> KO, it disagrees with " << agreed_by;
      all_passed = false;
      }
     else {
      if( ! agreed_sign ) { agreed_sign = sgn( d0 ); agreed_by = name; }
      std::cout << " -> OK";
      }

     std::cout << std::endl;
     }
    }
   }

  infeasible->unregister_Solver( solver );
  delete infeasible;
  delete solver;
  }

 if( ! any_solver ) {
  std::cout << "no :MILPSolver in this build, nothing to check" << std::endl;
  return( 0 );
  }

 if( all_passed )
  std::cout << "All tests passed!!" << std::endl;
 else
  std::cout << "Shit happened!!" << std::endl;

 return( all_passed ? 0 : 1 );
 }

/*--------------------------------------------------------------------------*/
/*--------------------------- End File test.cpp ----------------------------*/
/*--------------------------------------------------------------------------*/
