/*--------------------------------------------------------------------------*/
/*----------------------------- File test.cpp ------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Main for testing different formulations of some problem.
 *
 * This main loads a Block once per formulation of its BlockConfig, given as
 * the list of -F [see read_formulations()], and BlockConfig-ures each copy
 * with its own, assumed to produce a formulation of the same problem. Then it
 * attaches to each copy the Solver of the BlockSolverConfig of its
 * formulation, or of -S if the formulation has none, solves all of them and
 * compares the results [see SolveFormulations()]. Given -B rather than -F,
 * there is one copy and its Solver are compared with each other.
 *
 * Both the BlockConfig and the BlockSolverConfig can be "meta", i.e., a
 *
 *   SimpleConfiguration< std::map< std::string , Configuration * > >
 *
 * which is interpreted as "the BlockConfig / BlockSolverConfig that are to be
 * apply()-ed to the Block / all its sub-Block that have that specific
 * classname()". That is, if the SimpleConfiguration< ... > contains, say,
 *
 *    { { "UCBlock" , < pointer to BC1 > } ,
 *      { "DCNetworkBlock" , < pointer to BC2 > } }
 *
 * then the Block is scanned, and all its sub-Block (possibly, itself) that
 * are UCBlock are BlockConfig-ured with (a clone() to) BC1 while all the its
 * sub-Block (...) that are DCNetworkBlock are BlockConfig-ured with (...)
 * BC2; analogously for the BlockSolverConfig (except there is no need for
 * clone()-ing).
 *
 * \author Antonio Frangioni \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Antonio Frangioni
 */
/*--------------------------------------------------------------------------*/
/*----------------------------- INCLUDES -----------------------------------*/
/*--------------------------------------------------------------------------*/

#include "common_utils.h"

#include "BlockSolverConfig.h"

/*--------------------------------------------------------------------------*/

// test-specific command-line knobs, set by process_specific_arg(); the
// formulations (-F or -B) and the BlockSolverConfig (-S) are those of
// common_utils

static bool process_specific_arg( int opt )
{
 switch( opt ) {
  case( 'r' ): Str2Sthg( optarg , RefObjective ); return( true );
  default:                                        return( false );
  }
 }

/*--------------------------------------------------------------------------*/

int main( int argc , char **argv )
{
 // override the default terminate handler to print the exception message
 std::set_terminate( smspp_terminate );

 // read command line parameters- - - - - - - - - - - - - - - - - - - - - - -

 docopt_desc = "SMS++ compare-formulations test.\n";
 short_opts += "r:";
 const std::vector< option > my_opts = {
   { "ref"        , required_argument , nullptr , 'r' } };
 long_opts.insert( std::prev( long_opts.end() ) ,
                   my_opts.begin() , my_opts.end() );
 help += "  -r, --ref <value>               reference objective to compare "
         "against [none]\n";

 process_args( argc , argv , process_specific_arg );

 // the formulations of the BlockConfig, with their BlockSolverConfig - - - -

 auto forms = get_formulations();

 std::vector< Block * > blocks;
 std::vector< Configuration * > bscs;
 std::vector< std::vector< bool > > relax;
 for( std::size_t k = 0 ; k < forms.configs.size() ; ++k ) {
  auto b = Block::deserialize( filename );
  if( ! b ) {
   std::cerr << "error: cannot load Block from " << filename << std::endl;
   return( 1 );
   }

  b_config_Block( b , forms.configs[ k ] , forms.file );

  auto bsc = solver_config_of( forms , k );
  relax.push_back( attach_formulation_Solvers( b , bsc , forms , k ) );
  if( b->get_registered_solvers().empty() ) {
   std::cerr << "Error: no Solver registered to formulation "
             << forms.names[ k ] << std::endl;
   exit( 1 );
   }

  blocks.push_back( b );
  bscs.push_back( bsc );
  }

 delete( forms.owner );

 // solve, each Solver claiming an optimum, which must agree (2e-7) - - - - -

 bool ok = SolveFormulations( blocks , forms.names ,
                              exact_getter( ObjGetter::VarValue ) ,
                              RefObjective , 2e-7 , {} , relax );

 if( ok )
  std::cout << GREEN( Test passed!! ) << std::endl;
 else
  std::cout << RED( Shit happened!! ) << std::endl;

 // clean - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 for( std::size_t k = 0 ; k < blocks.size() ; ++k ) {
  s_config_Block( blocks[ k ] , bscs[ k ] );  // clear()-ed before
  delete( blocks[ k ] );
  delete( bscs[ k ] );
  }

 return( ok ? 0 : 1 );
 }

/*--------------------------------------------------------------------------*/
/*------------------------- End File test.cpp ------------------------------*/
/*--------------------------------------------------------------------------*/
