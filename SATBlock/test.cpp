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
 * satisfy the hard clauses, and its value, i.e., the weight of the soft
 * clauses it violates plus the costs of its true variables, has to be the
 * upper bound the Solver declares, or less.
 *
 * With -B a BlockConfig is applied to the SATBlock before anything else,
 * e.g., giving it a structure out of the groups of its variables [see
 * SATBlock::set_structure()], under which the Solver that relax it, such as
 * the LagrangianDualSolver, are declared with -R: their bound is
 * cross-checked, the solution they write is not. The rounds of -n then
 * keep the structure: a linking clause of the kRelaxation one stays hard or
 * soft, and the clauses added are on the variables of one group.
 *
 * With -n the instance is then changed n times, each time by a Modification
 * drawn at random (with the seed of -e) and followed by the cross-check
 * again, the Solver staying registered so that each of them reoptimizes as
 * it can: the costs of a fifth of the variables moved by a step, as the
 * Lagrangian term of a decomposition does, the weights of a range of
 * clauses changed (a clause possibly turning hard or soft), or a few
 * clauses added; the optimum of -r holds for the instance as read only.
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

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <random>

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
//   -r / --ref    : the optimum of the instance, to compare against
//   -n / --rounds : the number of Modification after the first solution
//   -e / --seed   : the seed of the Modification

static unsigned int n_rounds = 0;
static long int seed = 1;

static bool process_specific_arg( int opt )
{
 switch( opt ) {
  case( 'r' ): Str2Sthg( optarg , RefObjective ); return( true );
  case( 'n' ): Str2Sthg( optarg , n_rounds );     return( true );
  case( 'e' ): Str2Sthg( optarg , seed );         return( true );
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
 short_opts += "r:n:e:";
 const std::vector< option > my_opts = {
   { "ref" , required_argument , nullptr , 'r' } ,
   { "rounds" , required_argument , nullptr , 'n' } ,
   { "seed" , required_argument , nullptr , 'e' } };
 long_opts.insert( std::prev( long_opts.end() ) ,
		   my_opts.begin() , my_opts.end() );
 help += "  -r, --ref <value>               the optimum of the instance "
	 "[none]\n"
	 "  -n, --rounds <n>                Modification after the first "
	 "solution [0]\n"
	 "  -e, --seed <n>                  seed of the Modification [1]\n";
 process_args( argc , argv , process_specific_arg );
 require_solver_config();

 auto sat = read_SATBlock();

 if( ! bconf_file.empty() ) {
  Configuration * bc = Configuration::deserialize( bconf_file );
  if( ! bc ) {
   std::cerr << "Error: cannot load " << bconf_file << std::endl;
   delete sat;
   exit( 1 );
   }
  b_config_Block( sat , bc , bconf_file );
  delete bc;
  }

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
  if( s->has_var_solution() && ! is_relaxation( k ) ) {
   s->get_var_solution();
   const double ub = s->get_ub();
   const double w = sat->get_objective_value();
   if( ! sat->is_feasible() ) {
    std::cout << "Solver " << k << ": its solution violates a hard clause"
	      << std::endl;
    solutions_ok = false;
    }
   else
    if( w > ub + tol * std::max( 1.0 , std::abs( ub ) ) ) {
     std::cout << "Solver " << k << ": its solution costs " << w
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

 // the rounds of Modification: the steps of the costs and the new weights
 // are integer and scaled on the median soft weight
 std::mt19937_64 rg( seed );
 auto rnd = [ & ]( unsigned int k ) { return( unsigned( rg() % k ) ); };
 const unsigned int n = sat->get_number_variables();
 double scale = 1;
 {
  std::vector< double > sw;
  for( unsigned int i = 0 ; i < sat->get_number_clauses() ; ++i )
   if( ! sat->is_hard( i ) )
    sw.push_back( sat->get_weights()[ i ] );
  if( ! sw.empty() ) {
   std::nth_element( sw.begin() , sw.begin() + sw.size() / 2 , sw.end() );
   scale = std::max( 1.0 , std::round( sw[ sw.size() / 2 ] ) );
   }
  }
 auto new_weight = [ & ]( void ) {
  return( rnd( 10 ) == 0 ? Inf< double >()
			 : double( 1 + rnd( unsigned( 2 * scale ) ) ) );
  };
 std::vector< double > costs( sat->get_costs() );
 costs.resize( n , 0 );
 const double step = std::max( 1.0 , std::round( scale / 10 ) );

 for( unsigned int r = 1 ; r <= n_rounds ; ++r ) {
  RefObjective = std::numeric_limits< double >::quiet_NaN();
  const unsigned int what = rnd( 5 );
  if( what < 3 ) {
   // the costs of a fifth of the variables moved by a step
   for( unsigned int i = 0 ; i < n ; ++i )
    if( rnd( 5 ) == 0 )
     costs[ i ] += step * ( double( rnd( 3 ) ) - 1 );
   sat->chg_costs( costs , Block::Range( 0 , n ) );
   std::cout << "round " << r << ": costs" << std::endl;
   }
  else
   if( what == 3 ) {
    // the weights of up to 5 consecutive clauses
    const unsigned int m = sat->get_number_clauses();
    const unsigned int first = rnd( m );
    const unsigned int k = 1 + rnd( std::min( 5u , m - first ) );
    std::vector< double > w( k );
    for( unsigned int j = 0 ; j < k ; ++j ) {
     w[ j ] = new_weight();
     // with the kRelaxation structure, a linking clause stays as hard or
     // as soft as it is [see SATBlock::chg_weights()]
     if( sat->is_linking( first + j ) &&
	 ( sat->get_structure_type() == SATBlock::kRelaxation ) &&
	 ( sat->is_hard( first + j ) != ( w[ j ] == Inf< double >() ) ) )
      w[ j ] = sat->is_hard( first + j ) ? Inf< double >()
				       : double( 1 + rnd( unsigned( 2 * scale ) ) );
     }
    sat->chg_weights( w , Block::Range( first , first + k ) );
    std::cout << "round " << r << ": weights of " << k << " clauses"
	      << std::endl;
    }
   else {
    // up to 3 clauses of 1 to 3 literals; with a structure, on the
    // variables of one group [see SATBlock::add_clauses()]
    const auto & grp = sat->get_variable_groups();
    const bool one_group =
     sat->get_structure_type() != SATBlock::kNoStructure;
    SATBlock::v_Clause nc( 1 + rnd( 3 ) );
    SATBlock::v_Weight nw( nc.size() );
    for( unsigned int c = 0 ; c < nc.size() ; ++c ) {
     const unsigned int v0 = rnd( n );
     nc[ c ].push_back( int( 1 + v0 ) * ( rnd( 2 ) ? 1 : -1 ) );
     for( unsigned int l = rnd( 3 ) ; l-- ; ) {
      unsigned int v = rnd( n );
      if( one_group && ( grp[ v ] != grp[ v0 ] ) )
       v = v0;  // a literal repeated is kept once
      nc[ c ].push_back( int( 1 + v ) * ( rnd( 2 ) ? 1 : -1 ) );
      }
     nw[ c ] = new_weight();
     }
    sat->add_clauses( std::move( nc ) , std::move( nw ) );
    std::cout << "round " << r << ": " << nw.size() << " clauses added"
	      << std::endl;
    }

  const bool rok = SolveAll( sat , classify , RefObjective , tol , nullptr ,
			     nullptr , nullptr , nullptr ,
			     dynamic_cast< BlockSolverConfig * >( bsc ) );
  ok = ok && rok && solutions_ok;
  }

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
