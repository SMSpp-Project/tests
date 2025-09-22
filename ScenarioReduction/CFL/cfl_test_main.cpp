/*--------------------------------------------------------------------------*/
/*-------------------------- File cfl_test_main.cpp -----------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Main entry point for CFL scenario reduction tests.
 *
 * \author Benoît Tran \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Benoît Tran
 */
/*--------------------------------------------------------------------------*/

#include "CFLScenarioReductionTest.h"

using namespace ScenarioReductionTesting;

/*--------------------------------------------------------------------------*/
/*--------------------------------- MAIN -----------------------------------*/
/*--------------------------------------------------------------------------*/

int main(int argc, char* argv[]) {
    // Create CFL-specific test instance
    CFLScenarioReductionTest test;

    // Run the test using the base class workflow
    return test.run(argc, argv);
}

/*--------------------------------------------------------------------------*/
/*------------------------ End File cfl_test_main.cpp ---------------------*/
/*--------------------------------------------------------------------------*/