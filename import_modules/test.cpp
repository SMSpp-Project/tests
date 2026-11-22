/** @file
 * Imports the C++20 module of each SMS++ module that is in the build (the
 * macro HAVE_<module> says which, see CMakeLists.txt), and for each one of
 * them checks that its main classes are exported and that the factory still
 * gives a Block or a Solver of them: a name missing from a module stops the
 * build, a lost factory registration stops the run.
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

#include <iostream>
#include <memory>
#include <string>
#include <type_traits>

#undef NDEBUG
#include <cassert>

import smspp;
#if defined( HAVE_BendersDecompositionSolver )
import smspp.bendersdecompositionsolver;
#endif
#if defined( HAVE_BinaryKnapsackBlock )
import smspp.binaryknapsackblock;
#endif
#if defined( HAVE_BranchAndXSolver )
import smspp.branchandxsolver;
#endif
#if defined( HAVE_BundleSolver )
import smspp.bundlesolver;
#endif
#if defined( HAVE_CapacitatedFacilityLocationBlock )
import smspp.capacitatedfacilitylocationblock;
#endif
#if defined( HAVE_FrankWolfeSolver )
import smspp.frankwolfesolver;
#endif
#if defined( HAVE_InvestmentBlock )
import smspp.investmentblock;
#endif
#if defined( HAVE_LagrangianDualSolver )
import smspp.lagrangiandualsolver;
#endif
#if defined( HAVE_LukFiBlock )
import smspp.lukfiblock;
#endif
#if defined( HAVE_MCFBlock )
import smspp.mcfblock;
#endif
#if defined( HAVE_MCFClassSolver )
import smspp.mcfclasssolver;
#endif
#if defined( HAVE_MCFLemonSolver )
import smspp.mcflemonsolver;
#endif
#if defined( HAVE_MILPSolver )
import smspp.milpsolver;
#endif
#if defined( HAVE_MMCFBlock )
import smspp.mmcfblock;
#endif
#if defined( HAVE_MultiKnapsackAssignBlock )
import smspp.multiknapsackassignblock;
#endif
#if defined( HAVE_MultiStageStochasticBlock )
import smspp.multistagestochasticblock;
#endif
#if defined( HAVE_SATBlock )
import smspp.satblock;
#endif
#if defined( HAVE_SDDPBlock )
import smspp.sddpblock;
#endif
#if defined( HAVE_SVMBlock )
import smspp.svmblock;
#endif
#if defined( HAVE_SatellitesBlock )
import smspp.satellitesblock;
#endif
#if defined( HAVE_ScenarioReductionSolver )
import smspp.scenarioreductionsolver;
#endif
#if defined( HAVE_SingleFlowDCRBlock )
import smspp.singleflowdcrblock;
#endif
#if defined( HAVE_StochasticBlock )
import smspp.stochasticblock;
#endif
#if defined( HAVE_SubGradSolver )
import smspp.subgradsolver;
#endif
#if defined( HAVE_TwoStageStochasticBlock )
import smspp.twostagestochasticblock;
#endif
#if defined( HAVE_UCBlock )
import smspp.ucblock;
#endif

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

/// names a class template of one type, whose instances are not exported
/// (their template arguments come from the libraries the module wraps)

template< template< typename > class T >
struct names_template {};

/// the factory gives a Block of the class T under the name classname

template< class T >
static void check_Block( const std::string & classname )
{
 static_assert( std::is_base_of_v< Block , T > );
 std::unique_ptr< Block > b( Block::new_Block( classname ) );
 assert( dynamic_cast< T * >( b.get() ) );
 std::cout << classname << " ";
 }

/// the factory gives a Solver of the class T under the name classname

template< class T >
static void check_Solver( const std::string & classname )
{
 static_assert( std::is_base_of_v< Solver , T > );
 std::unique_ptr< Solver > s( Solver::new_Solver( classname ) );
 assert( dynamic_cast< T * >( s.get() ) );
 std::cout << classname << " ";
 }

/*--------------------------------------------------------------------------*/
/*-------------------------------- MAIN ------------------------------------*/
/*--------------------------------------------------------------------------*/

int main( void )
{
 check_Block< AbstractBlock >( "AbstractBlock" );
#if defined( HAVE_BendersDecompositionSolver )
 check_Solver< BendersDecompositionSolver >( "BendersDecompositionSolver" );
#endif
#if defined( HAVE_BinaryKnapsackBlock )
 check_Block< BinaryKnapsackBlock >( "BinaryKnapsackBlock" );
#endif
#if defined( HAVE_BranchAndXSolver )
 check_Solver< BranchAndXSolver >( "BranchAndXSolver" );
#endif
#if defined( HAVE_BundleSolver )
 check_Solver< BundleSolver >( "BundleSolver" );
#endif
#if defined( HAVE_CapacitatedFacilityLocationBlock )
 check_Block< CapacitatedFacilityLocationBlock >(
					      "CapacitatedFacilityLocationBlock" );
#endif
#if defined( HAVE_FrankWolfeSolver )
 check_Solver< FrankWolfeSolver >( "FrankWolfeSolver" );
#endif
#if defined( HAVE_InvestmentBlock )
 check_Block< InvestmentBlock >( "InvestmentBlock" );
#endif
#if defined( HAVE_LagrangianDualSolver )
 check_Solver< LagrangianDualSolver >( "LagrangianDualSolver" );
#endif
#if defined( HAVE_LukFiBlock )
 check_Block< LukFiBlock >( "LukFiBlock" );
#endif
#if defined( HAVE_MCFBlock )
 check_Block< MCFBlock >( "MCFBlock" );
#endif
#if defined( HAVE_MCFClassSolver )
 [[maybe_unused]] names_template< MCFSolver > mcf_solver;
#endif
#if defined( HAVE_MCFLemonSolver )
 static_assert( sizeof( MCFListDigraph ) > 0 );
#endif
#if defined( HAVE_MILPSolver )
 static_assert( std::is_base_of_v< Solver , MILPSolver > );
#endif
#if defined( HAVE_MMCFBlock )
 check_Block< MMCFBlock >( "MMCFBlock" );
#endif
#if defined( HAVE_MultiKnapsackAssignBlock )
 check_Block< MultiKnapsackAssignBlock >( "MultiKnapsackAssignBlock" );
#endif
#if defined( HAVE_MultiStageStochasticBlock )
 check_Block< MultiStageStochasticBlock >( "MultiStageStochasticBlock" );
#endif
#if defined( HAVE_SATBlock )
 check_Block< SATBlock >( "SATBlock" );
#endif
#if defined( HAVE_SDDPBlock )
 check_Block< SDDPBlock >( "SDDPBlock" );
 check_Solver< SDDPGreedySolver >( "SDDPGreedySolver" );
#endif
#if defined( HAVE_SVMBlock )
 check_Block< SVCBlock >( "SVCBlock" );
#endif
#if defined( HAVE_SatellitesBlock )
 check_Block< ConstellationBlock >( "ConstellationBlock" );
#endif
#if defined( HAVE_ScenarioReductionSolver )
 check_Solver< CSSCScenarioReductionSolver >(
					       "CSSCScenarioReductionSolver" );
#endif
#if defined( HAVE_SingleFlowDCRBlock )
 check_Block< SingleFlowDCRBlock >( "SingleFlowDCRBlock" );
#endif
#if defined( HAVE_StochasticBlock )
 check_Block< StochasticBlock >( "StochasticBlock" );
#endif
#if defined( HAVE_SubGradSolver )
 check_Solver< SubGradSolver >( "SubGradSolver" );
#endif
#if defined( HAVE_TwoStageStochasticBlock )
 check_Block< TwoStageStochasticBlock >( "TwoStageStochasticBlock" );
#endif
#if defined( HAVE_UCBlock )
 check_Block< UCBlock >( "UCBlock" );
 check_Block< ThermalUnitBlock >( "ThermalUnitBlock" );
#endif

 std::cout << std::endl << "import_modules: OK" << std::endl;
 return( 0 );
 }

/*--------------------------------------------------------------------------*/
/*-------------------------- End File test.cpp -----------------------------*/
/*--------------------------------------------------------------------------*/
