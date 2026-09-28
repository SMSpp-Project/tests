/*--------------------------------------------------------------------------*/
/*-------------------------- File test.cpp ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Main for testing SATBlock.
 *
 * A SATBlock is read from a file, in the DIMACS CNF or WCNF format or as a
 * netCDF one, a BlockSolverConfig is applied that registers to it the
 * Solver to be compared, and SolveAll() cross-checks them against one
 * another and, if given with -r, against the optimum of the instance. The
 * Solver of a SATBlock are of two kinds, those that read its physical
 * representation, such as the SATSolver solving the weighted MaxSAT by OLL,
 * and those that read its abstract one, i.e., its MILP formulation, such as
 * the :MILPSolver: the cross-check is then also a check that the two
 * representations are the same problem. On top of that, the solution each
 * Solver writes is checked on the physical representation: it has to
 * satisfy the hard clauses, and the weight of the soft clauses it violates
 * has to be the upper bound the Solver declares, or less.
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

#include <cmath>
#include <fstream>
#include <iostream>

#include "common_utils.h"
#include "SATBlock.h"

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/
// the options of this test, on top of those common_utils handles (the
// instance positional and -S / -c / -p / -D / -v / -E):
//   -r / --ref : the optimum of the instance, to compare against

static bool process_specific_arg( int opt )
{
 switch( opt ) {
  case( 'r' ): Str2Sthg( optarg , RefObjective ); return( true );
  default:                                        return( false );
  }
 }

/*--------------------------------------------------------------------------*/
/// reads the SATBlock out of filename: netCDF if it ends in .nc4, the DIMACS
/// CNF or WCNF format otherwise

static SATBlock * read_SATBlock( void )
{
 if( ( filename.size() > 4 ) &&
     ( filename.compare( filename.size() - 4 , 4 , ".nc4" ) == 0 ) ) {
  auto block = Block::deserialize( filename );
  auto sat = dynamic_cast< SATBlock * >( block );
  if( ! sat ) {
   std::cerr << "Error: " << filename << " does not hold a SATBlock"
	     << std::endl;
   delete block;
   exit( 1 );
   }
  return( sat );
  }

 std::ifstream in( filename );
 if( ! in ) {
  std::cerr << "Error: cannot open " << filename << std::endl;
  exit( 1 );
  }
 auto sat = new SATBlock();
 sat->load( in );
 return( sat );
 }

/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 std::set_terminate( smspp_terminate );

 docopt_desc = "SMS++ SATBlock test.\n";
 short_opts += "r:";
 const std::vector< option > my_opts = {
   { "ref" , required_argument , nullptr , 'r' } };
 long_opts.insert( std::prev( long_opts.end() ) ,
		   my_opts.begin() , my_opts.end() );
 help += "  -r, --ref <value>               the optimum of the instance "
	 "[none]\n";
 process_args( argc , argv , process_specific_arg );
 require_solver_config();

 auto sat = read_SATBlock();

 // the Solver of the abstract representation need it, the others ignore it
 sat->generate_abstract_variables();
 sat->generate_abstract_constraints();
 sat->generate_objective();

 Configuration * bsc = Configuration::deserialize( sconf_file );
 if( ! bsc ) {
  std::cerr << "Error: cannot load " << sconf_file << std::endl;
  delete sat;
  exit( 1 );
  }
 s_config_Block( sat , bsc , sconf_file , false );
 if( sat->get_registered_solvers().empty() ) {
  // none of the Solver it names is in this build: nothing to do
  delete bsc;
  delete sat;
  return( 77 );
  }

 // each Solver is read by its bounds, and the solution it writes is checked
 // on the physical representation
 constexpr double tol = 1e-6;
 bool solutions_ok = true;
 auto classify = [ & ]( Solver * s , std::size_t k ) {
  auto reading = read_bounds( s , k );
  if( s->has_var_solution() ) {
   s->get_var_solution();
   const double ub = s->get_ub();
   const double w = sat->get_violated_weight();
   if( ! sat->is_feasible() ) {
    std::cout << "Solver " << k << ": its solution violates a hard clause"
	      << std::endl;
    solutions_ok = false;
    }
   else
    if( w > ub + tol * std::max( 1.0 , std::abs( ub ) ) ) {
     std::cout << "Solver " << k << ": its solution weighs " << w
	       << ", more than its upper bound " << ub << std::endl;
     solutions_ok = false;
     }
   }
  return( reading );
  };

 bool ok = SolveAll( sat , classify , RefObjective , tol , nullptr , nullptr ,
		     nullptr , nullptr ,
		     dynamic_cast< BlockSolverConfig * >( bsc ) );
 ok = ok && solutions_ok;

 // detach the Solver by applying the cleared BlockSolverConfig, then free
 bsc->clear();
 s_config_Block( sat , bsc , sconf_file );
 delete bsc;
 delete sat;

 return( ok ? 0 : 1 );
 }

/*--------------------------------------------------------------------------*/
/*-------------------------- End File test.cpp -----------------------------*/
/*--------------------------------------------------------------------------*/
