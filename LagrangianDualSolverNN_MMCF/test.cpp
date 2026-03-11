/*--------------------------------------------------------------------------*/
/*-------------------------- File test.cpp ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Main for testing LagrangianDualSolver with MMCFBlock, using a
 * machine-learning heuristic to predict the bundle regularization parameter t.
 *
 * Overview
 * --------
 * An MMCFBlock instance (Min-Cost Multi-Commodity Flow problem) is loaded
 * from a text file. Two Solvers are registered to it:
 *
 *   1. A reference solver (e.g. a MILP solver) used as ground truth.
 *   2. A LagrangianDualSolver whose inner bundle method (BundleSolverML)
 *      replaces the classical rule-based step-size heuristic with a small
 *      neural network (Net, 20→16→1) that predicts the proximal
 *      regularization parameter t at each bundle iteration.
 *
 * ML training loop
 * ----------------
 * After each call to Slvr2->compute(), BML->Backward() is invoked to
 * accumulate gradients over all recorded bundle iterations and update
 * the network weights. The solve is repeated three times (SolveBoth calls
 * in main) to allow online learning across successive solves of the same
 * instance.
 *
 * The predicted t controls the trade-off between the quadratic proximal
 * term and the cutting-plane model in the master QP:
 *   min  f_tilde(d)  +  (1/2t) ||d||^2
 * A larger t gives a more aggressive (longer) step; a smaller t is more
 * conservative. The network learns to predict a good t from a 20-dimensional
 * feature vector built from the current solver state (norms, counters,
 * function values, etc.).
 *
 * Comparison
 * ----------
 * After both solvers terminate, their objective values are compared with a
 * relative tolerance of 2e-7. The test is considered passed if both agree
 * on feasibility/infeasibility/unboundedness and, when feasible, on the
 * objective value.
 *
 * Future extensions
 * -----------------
 * The tester has scaffolding for repeatedly and randomly modifying the
 * MMCFBlock and re-solving it (the main loop body is currently commented
 * out with !!...!!), which would allow testing Modification handling and
 * could also serve as additional training data for the ML model.
 *
 * \author Francesca Demelas \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \author Antonio Frangioni \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * Copyright &copy by Antonio Frangioni
 */
/*--------------------------------------------------------------------------*/
/*-------------------------------- MACROS ----------------------------------*/
/*--------------------------------------------------------------------------*/

/// Verbosity level for console output.
/// 0 = only pass/fail summary
/// 1 = result of each test (timing, objective value)
/// 2 = + solver log redirected to file (or cout if LOG_ON_COUT != 0)
/// 3 = + save LP file of the master problem
#define LOG_LEVEL 1

#if( LOG_LEVEL >= 1 )
 #define LOG1( x ) cout << x
 #define CLOG1( y , x ) if( y ) cout << x

 #if( LOG_LEVEL >= 2 )
  /// If nonzero, the 2nd Solver (LagrangianDualSolver) log goes to cout
  /// instead of to a log file. Useful for interactive debugging.
  #define LOG_ON_COUT 0
 #endif
#else
 #define LOG1( x )
 #define CLOG1( y , x )
#endif

/*--------------------------------------------------------------------------*/

/// If nonzero, the 1st Solver is unregistered and re-registered at every
/// round of changes, exercising the attach/detach machinery.
#define DETACH_1ST 1

/// If nonzero, the 2nd Solver (LagrangianDualSolver with BundleSolverML)
/// is unregistered and re-registered at every round, similarly to DETACH_1ST.
#define DETACH_2ND 1

/*--------------------------------------------------------------------------*/

/// If nonzero, the two Solvers are not called at every round of changes but
/// only every SKIP_BEAT + 1 rounds. This lets modifications accumulate and
/// stresses the Solver's Modification handling. The total number of Block
/// solutions is unchanged (the number of rounds is scaled accordingly).
#define SKIP_BEAT 0

/*--------------------------------------------------------------------------*/

/// Enable ANSI color codes in terminal output (green = pass, red = fail).
#define USECOLORS 1
#if( USECOLORS )
 #define RED( x )   "\x1B[31m" #x "\033[0m"
 #define GREEN( x ) "\x1B[32m" #x "\033[0m"
#else
 #define RED( x )   #x
 #define GREEN( x ) #x
#endif

/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <fstream>
#include <sstream>
#include <iomanip>
#include <random>

#include "BlockSolverConfig.h"
#include "CDASolver.h"
#include <cmath>
#include "MMCFBlock.h"
#include "BundleSolverML.h"       // ML-based bundle solver (step-size via NN)
#include "LagrangianDualSolver.h"

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace std;
using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*-------------------------------- TYPES -----------------------------------*/
/*--------------------------------------------------------------------------*/

using Index      = Block::Index;
using c_Index    = Block::c_Index;
using Range      = Block::Range;
using c_Range    = Block::c_Range;
using Subset     = Block::Subset;
using c_Subset   = Block::c_Subset;
using FunctionValue = Function::FunctionValue;

/*--------------------------------------------------------------------------*/
/*------------------------------- CONSTANTS --------------------------------*/
/*--------------------------------------------------------------------------*/

/// Name of the solver log file (used when LOG_LEVEL >= 2 and LOG_ON_COUT == 0)
const char *const logF = "log.txt";

/// Floating-point infinity, used as a sentinel for infeasible objective values
const FunctionValue INF = SMSpp_di_unipi_it::Inf<FunctionValue>();

/*--------------------------------------------------------------------------*/
/*------------------------------- GLOBALS ----------------------------------*/
/*--------------------------------------------------------------------------*/

/// The Multi-Commodity Flow block that is loaded and solved
MMCFBlock *TestBlock;

/// Copy of main's argv for use inside helper functions (e.g. to derive filenames)
char **globalArgv;

/// Bitmask controlling what is written to output files:
///   bit 0 (1): dual solution (reduced costs / potentials)
///   bit 1 (2): primal solution (flows)
///   bit 2 (4): timing, lower bound, iteration count, objective value
int wprnt = 0;

/// If true, Lagrangian multipliers are initialised from a file (argv[4])
/// before the 2nd Solver is re-registered, allowing warm-starting.
bool init;

/// Path to the file containing the initial Lagrangian multipliers (potentials)
char *initPi;

/// Mersenne-Twister random generator (seed fixed at construction)
std::mt19937 rg;

/// Uniform distribution over [0, 1] used for random perturbations
std::uniform_real_distribution<> dis(0.0, 1.0);

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

/// Converts a C-string to any type T via istringstream.
template<class T>
static void Str2Sthg(const char *const str, T &sthg) {
  istringstream(str) >> sthg;
}

/*----------------------------------------------------------------------------
 * The following two helpers are currently unused (commented out):
 *
 *   rndfctr()       – returns a random factor in [0, 2] biased toward < 1,
 *                     intended for scaling arc costs / capacities.
 *   GenerateRand()  – returns a sorted random k-subset of {0, …, m-1},
 *                     intended for selecting arcs/nodes to modify.
 *
 * They will be re-enabled when the random-modification loop is activated.
 ----------------------------------------------------------------------------*/

/**
 * @brief Prints the objective value (or a status string) of one Solver.
 * @param hs     Whether the Solver found a feasible solution.
 * @param rtrn   Return code from Solver::compute().
 * @param fo     Objective value (meaningful only when hs == true).
 */
static void PrintResults(bool hs, int rtrn, double fo) {
  if (hs)
    cout << fo;
  else if (rtrn == Solver::kInfeasible)
    cout << "    Unfeas";
  else if (rtrn == Solver::kUnbounded)
    cout << "      Unbounded";
  else
    cout << "      Error!";
}

/*--------------------------------------------------------------------------*/

/**
 * @brief Writes solver output (duals, primals, timing) to files.
 *
 * Controlled by the bitmask wprnt:
 *   - bit 0: write dual variables (potentials) to redCosts/<name>Sol.dat
 *   - bit 1: write primal variables (flows) to primals/<name>.dat
 *   - bit 2: write timing / lb / iterations / objective to times/<name>Sol.dat
 *
 * The output filename is derived from the input instance path (argv[1])
 * with a "_1" or "_2" suffix depending on which Solver produced the result.
 *
 * @param slvr               The solver whose solution is extracted.
 * @param first              True for the 1st Solver, false for the 2nd.
 * @param time               Wall-clock time taken by the solve (seconds).
 * @param lb                 Best lower bound on the optimal value.
 * @param objFunc            Primal objective value (if feasible).
 * @param iters              Number of iterations performed.
 * @param str_results_folder Root folder for output files (default "./").
 */
static void PrintSol(CDASolver *slvr, bool first,
                     double time, double lb, double objFunc, int iters,
                     string str_results_folder = "./") {
  if (!wprnt) return;

  // Derive a base filename from the instance path
  std::string name(globalArgv[1]);
  name = name.substr(name.find_last_of("/") + 1, name.length());
  name = name.substr(0, name.find("."));
  name.append(first ? "_1" : "_2");

  // Write dual solution (potentials per commodity and node)
  if (wprnt & 1) {
    slvr->get_dual_solution();
    ofstream solutionsFile(str_results_folder + "redCosts/" + name + "Sol.dat");
    for (Index k = 0; k < TestBlock->get_NComm(); ++k) {
      for (Index i = 0; i < TestBlock->get_NNodes(); ++i)
        solutionsFile << TestBlock->get_potential(k, i) << " ";
      solutionsFile << "\n";
    }
    solutionsFile.close();
  }

  // Write primal solution (flows per commodity and arc)
  if (wprnt & 2) {
    slvr->get_var_solution();
    ofstream primalFile(str_results_folder + "primals/" + name + ".dat");
    for (Index k = 0; k < TestBlock->get_NComm(); ++k) {
      for (Index i = 0; i < TestBlock->get_NArcs(); ++i)
        primalFile << TestBlock->get_flow(k, i) << " ";
      primalFile << "\n";
    }
    primalFile.close();
  }

  // Write timing and objective summary
  if (wprnt & 4) {
    ofstream timeFile(str_results_folder + "times/" + name + "Sol.dat");
    timeFile << time     << "\n";
    timeFile << lb       << "\n";
    timeFile << iters    << "\n";
    timeFile << objFunc  << "\n";
    timeFile.close();
  }
}

/*--------------------------------------------------------------------------*/

/**
 * @brief Solves the MMCFBlock with both registered Solvers and compares results.
 *
 * Workflow:
 *   1. Cast the front registered Solver to CDASolver and run compute().
 *   2. Cast the back registered Solver to LagrangianDualSolver and run compute().
 *      - Its inner BundleSolverML predicts the regularization parameter t via
 *        a neural network at each bundle iteration.
 *      - After compute() returns, BML->Backward() is called to perform one
 *        gradient-descent update of the network weights (online learning).
 *   3. Compare the two objective values; log and return pass/fail.
 *
 * The event handler registered on Slvr2 logs the lower-bound trajectory
 * (one entry per bundle iteration) to a per-instance log file, enabling
 * post-hoc analysis of the convergence curve produced by the ML step-size.
 *
 * @return true if both Solvers agree (within tolerance) on feasibility and
 *         objective value; false otherwise.
 */
static bool SolveBoth(void) {
  try {
    // -----------------------------------------------------------------------
    // Solve with the 1st Solver (reference, e.g. MILP)
    // -----------------------------------------------------------------------
    auto Slvr1 = dynamic_cast<CDASolver *>(
        TestBlock->get_registered_solvers().front());
    if (!Slvr1) {
      cout << "Error! First solver registered to TestBlock not a CDASolver";
      exit(1);
    }

#if DETACH_1ST
    // Detach and re-attach to exercise the Solver's registration machinery
    TestBlock->unregister_Solver(Slvr1);
    TestBlock->register_Solver(Slvr1, true); // push to front
#endif

#if(LOG_LEVEL >= 3)
    Slvr1->set_par(MILPSolver::strOutputFile, "LPBlock-CPXMILP.lp");
#endif

    auto start = std::chrono::system_clock::now();
    int rtrn1st = Slvr1->compute(false);

    bool hs1st = ((rtrn1st >= Solver::kOK) && (rtrn1st < Solver::kError) &&
                  (rtrn1st != Solver::kUnbounded) &&
                  (rtrn1st != Solver::kInfeasible)) ||
                 (rtrn1st == Solver::kLowPrecision);
    double fo1st = hs1st ? Slvr1->get_var_value() : -INF;

    auto end = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    PrintSol(Slvr1, true, elapsed.count(), Slvr1->get_lb(),
             fo1st, Slvr1->get_elapsed_iterations());

#if(LOG_LEVEL >= 1)
    cout.setf(ios::scientific, ios::floatfield);
    cout << setprecision(2) << elapsed.count() << " - " << flush;
#endif

    // If only one Solver is registered, print its result and return
    if (TestBlock->get_registered_solvers().size() == 1) {
#if(LOG_LEVEL >= 1)
      PrintResults(hs1st, rtrn1st, fo1st);
      cout << endl;
#endif
      return true;
    }

    // -----------------------------------------------------------------------
    // Solve with the 2nd Solver (LagrangianDualSolver + BundleSolverML)
    //
    // BundleSolverML overrides the step-size heuristic: at each bundle
    // iteration it builds a 20-dimensional feature vector from the solver
    // state, runs a forward pass through a small neural network (20→16→1),
    // and uses the output as the proximal regularization parameter t.
    //
    // After compute() finishes, Backward() propagates gradients through the
    // stored computation graph (features, search directions, Gram matrices)
    // and updates the network weights via the autograd engine.
    // -----------------------------------------------------------------------
    auto Slvr2 = dynamic_cast<LagrangianDualSolver *>(
        TestBlock->get_registered_solvers().back());

    if (!Slvr2) {
      cout << "Error! Last solver registered to TestBlock not a LagrangianDualSolver";
      exit(1);
    }

    // Optional: warm-start the Lagrangian multipliers from a file
    if (init) {
      TestBlock->unregister_Solver(Slvr2);
      cout << endl << "Initialization of lagrangian multipliers\n" << endl;
      ifstream piFile(initPi);
      float pi;
      for (Index k = 0; k < TestBlock->get_NComm(); ++k)
        for (Index i = 0; i < TestBlock->get_NNodes(); ++i) {
          piFile >> pi;
          TestBlock->set_potential(pi, k, i);
        }
      TestBlock->register_Solver(Slvr2); // push to back
    }

#if DETACH_2ND
    TestBlock->unregister_Solver(Slvr2);
    TestBlock->register_Solver(Slvr2); // push to back
#endif

    // Load the known optimal value ZKR for this instance (used to compute gap)
    double ZKR;
    std::ifstream input;
    std::string s = globalArgv[1];
    std::string token = s.substr(s.find_last_of("/") + 1, s.size());

    // Open the reference objective file (absolute path to benchmark dataset)
    input.open("/media/francesco/Kali/OBJ/MCNDBig40/1/" + token);

    // Open the lower-bound trajectory log for this instance
    std::ofstream ZKRoutput;
    ZKRoutput.open("./logs/" + token);

    cout << token << " " << ZKR << endl;
    input >> ZKR;
    ZKR = ZKR >= 0 ? ZKR : -ZKR; // ensure positive reference value
    input.close();

    float gapAcc = 0.0001; // target optimality gap (currently unused in termination)
    cout << endl << ZKR << endl << token << endl << endl;

    // Register an event handler that logs the lower bound at every iteration.
    // This records the convergence trajectory of the Lagrangian dual,
    // which can later be used to assess the quality of the ML step-size policy.
    Slvr2->set_event_handler(
        ThinComputeInterface::eEverykIteration,
        [&]() {
          auto LBF = Slvr2->get_lb();
          auto t = std::chrono::duration<double>(
                       std::chrono::system_clock::now() - start).count();
          ZKRoutput << LBF << endl << " " << t << endl;
          return ThinComputeInterface::eContinue;
        });

    // Run the Lagrangian dual solve (BundleSolverML predicts t at each iter)
    int rtrn2nd = Slvr2->compute(false);

    // Retrieve the inner BundleSolverML and perform one training step:
    // gradients are computed w.r.t. a loss that rewards serious steps and
    // penalizes null steps, then backpropagated into the neural network.
    auto BML = dynamic_cast<BundleSolverML *>(Slvr2->get_inner_Solver());
    BML->Backward();

    bool hs2nd = ((rtrn2nd >= Solver::kOK) && (rtrn2nd < Solver::kError) &&
                  (rtrn2nd != Solver::kUnbounded) &&
                  (rtrn2nd != Solver::kInfeasible)) ||
                 (rtrn2nd == Solver::kLowPrecision);
    double fo2nd = hs2nd ? Slvr2->get_var_value() : -INF;

    end = std::chrono::system_clock::now();
    elapsed = end - start;

    PrintSol(dynamic_cast<CDASolver *>(Slvr2), false, elapsed.count(),
             Slvr2->get_lb(), fo2nd, Slvr2->get_elapsed_iterations(), "./");

#if(LOG_LEVEL >= 1)
    cout.setf(ios::scientific, ios::floatfield);
    cout << setprecision(2) << elapsed.count();
#endif

    // -----------------------------------------------------------------------
    // Compare the two Solvers' results
    // -----------------------------------------------------------------------

    // Both feasible and objective values agree within relative tolerance 2e-7
    if (hs1st && hs2nd &&
        (abs(fo1st - fo2nd) <=
         2e-7 * max(double(1), max(abs(fo1st), abs(fo2nd))))) {
      LOG1(" - OK(f)" << endl);
      return true;
    }

    // Both declare infeasibility
    if ((rtrn1st == Solver::kInfeasible) && (rtrn2nd == Solver::kInfeasible)) {
      LOG1(" - OK(e)" << endl);
      return true;
    }

    // Both declare unboundedness
    if ((rtrn1st == Solver::kUnbounded) && (rtrn2nd == Solver::kUnbounded)) {
      LOG1(" - OK(u)" << endl);
      return true;
    }

    // Disagreement: print both results for diagnosis
#if(LOG_LEVEL >= 1)
    cout << " - " << setprecision(7);
    PrintResults(hs1st, rtrn1st, fo1st);
    cout << " - ";
    PrintResults(hs2nd, rtrn2nd, fo2nd);
    cout << endl;
#endif

    return false;

  } catch (exception &e) {
    cerr << e.what() << endl;
    exit(1);
  } catch (...) {
    cerr << "Error: unknown exception thrown" << endl;
    exit(1);
  }
}

/*--------------------------------------------------------------------------*/

/**
 * @brief Entry point.
 *
 * Command-line usage:
 *   test file_name [typ [wprnt [init]]]
 *
 *   file_name : path to the MMCF instance file
 *   typ       : instance file format ('s' = standard, 'c', 'p', 'o', 'd',
 *               'u', 'm'; upper or lower case); default 's'
 *   wprnt     : output bitmask (0 = nothing, 1 = duals, 2 = primal,
 *               4 = timing + objective); default 0
 *   init      : path to a file containing initial Lagrangian multipliers
 *               (potentials) for warm-starting the 2nd Solver; optional
 *
 * Main steps:
 *   1. Parse arguments and load the MMCFBlock from file.
 *   2. Apply block configuration (BPar.txt) and generate abstract variables.
 *   3. Attach Solvers via BlockSolverConfig (BSPar.txt).
 *      - The 2nd Solver must be a LagrangianDualSolver whose inner solver
 *        is a BundleSolverML (ML-based step-size heuristic).
 *   4. Optionally open a solver log file.
 *   5. Call SolveBoth() three times:
 *      - Each call runs both Solvers, then triggers one neural network
 *        training step (BML->Backward()), so the network progressively
 *        improves its t predictions across successive solves.
 *   6. Print pass/fail, clean up, and return 0 (pass) or 1 (fail).
 */
int main(int argc, char **argv) {

  // -------------------------------------------------------------------------
  // Parse command-line arguments
  // -------------------------------------------------------------------------
  globalArgv = argv;
  assert(SKIP_BEAT >= 0);

  char filetype = 's'; // default: standard MMCF file format

  switch (argc) {
    case(5): initPi = &argv[4][0]; init = true; // fall through
    case(4): wprnt  = argv[3][0];               // fall through
    case(3): filetype = argv[2][0];             // fall through
    case(2): break;
    default:
      cerr << "Usage: " << argv[0] << " file_name [typ wprnt init]" << endl
           << "        typ = s*, c, p, o, d, u, m (lower or uppercase)"   << endl
           << "        wprnt: what print into a file, coded bit-wise [0]" << endl
           << "         0 = nothing, 1 = duals,"                          << endl
           << "         2 = primal,  4 = time & objective value"           << endl;
      exit(1);
  }

  // -------------------------------------------------------------------------
  // Load the MMCFBlock from file and apply block configuration
  // -------------------------------------------------------------------------
  TestBlock = new MMCFBlock;
  TestBlock->load(argv[1], filetype);
  TestBlock->PreProcess();

  // Apply block-level configuration from BPar.txt (e.g. flow bounds, costs)
  auto cfg = Configuration::deserialize("BPar.txt");
  if (BlockConfig *bc = dynamic_cast<BlockConfig *>(cfg))
    bc->apply(TestBlock);
  else {
    cerr << "Error: BPar.txt does not contain a BlockConfig" << endl;
    delete cfg;
    exit(1);
  }
  delete cfg;

  TestBlock->generate_abstract_variables();

  // -------------------------------------------------------------------------
  // Attach Solvers via BlockSolverConfig (BSPar.txt)
  //
  // BSPar.txt must configure at least one (and typically two) Solvers:
  //   - Solver 1: a reference solver (e.g. a MILP solver)
  //   - Solver 2: a LagrangianDualSolver with BundleSolverML as inner solver
  //
  // BundleSolverML inherits from BundleSolver and overrides Heuristic() to
  // predict the proximal regularization parameter t via a neural network.
  // -------------------------------------------------------------------------
  BlockSolverConfig *bsc;
  {
    auto c = Configuration::deserialize("BSPar.txt");
    bsc = dynamic_cast<BlockSolverConfig *>(c);
    if (!bsc) {
      cerr << "Error: BSPar.txt does not contain a BlockSolverConfig" << endl;
      delete c;
      exit(1);
    }
    bsc->apply(TestBlock);
    bsc->clear();

    if (TestBlock->get_registered_solvers().empty()) {
      cout << endl << "no Solver registered to the Block!" << endl;
      exit(1);
    }
  }

  // -------------------------------------------------------------------------
  // Open solver log file (only when LOG_LEVEL >= 2)
  // -------------------------------------------------------------------------
#if(LOG_LEVEL >= 2)
 #if(LOG_ON_COUT)
  ((TestBlock->get_registered_solvers()).back())->set_log(&cout);
 #else
  ofstream LOGFile(logF, ofstream::out);
  if (!LOGFile.is_open())
    cerr << "Warning: cannot open log file \"" << logF << "\"" << endl;
  else {
    LOGFile.setf(ios::scientific, ios::floatfield);
    LOGFile << setprecision(10);
    ((TestBlock->get_registered_solvers()).back())->set_log(&LOGFile);
  }
 #endif
#endif

  // -------------------------------------------------------------------------
  // Main solve loop: call SolveBoth three times.
  //
  // Each iteration:
  //   1. The reference solver produces a ground-truth objective value.
  //   2. The LagrangianDualSolver runs the bundle method; BundleSolverML
  //      predicts t at each inner iteration via the neural network.
  //   3. After convergence, BML->Backward() performs one gradient update,
  //      improving the network's t-prediction policy for the next solve.
  //
  // Repeating on the same instance acts as simple online training: the
  // network sees the same problem with updated weights each round.
  // -------------------------------------------------------------------------
  bool AllPassed = SolveBoth();
  AllPassed = SolveBoth(); // 2nd training step on the same instance
  AllPassed = SolveBoth(); // 3rd training step on the same instance

  // -------------------------------------------------------------------------
  // Main modification loop (currently disabled, reserved for future use)
  //
  // When enabled, this loop would:
  //   - Randomly perturb arc costs, capacities, or demands of the MMCFBlock
  //   - Re-solve with both Solvers every SKIP_BEAT + 1 rounds
  //   - Provide additional training data for the ML step-size model
  // -------------------------------------------------------------------------
  /*!!
  for( Index rep = 0 ; rep < n_repeat * ( SKIP_BEAT + 1 ) ; ) {
    ...
  }
  !!*/

  // -------------------------------------------------------------------------
  // Report overall result
  // -------------------------------------------------------------------------
  if (AllPassed)
    cout << GREEN(All tests passed!!) << endl;
  else
    cout << RED(Shit happened!!) << endl;

  // -------------------------------------------------------------------------
  // Cleanup: detach Solvers, release memory
  // -------------------------------------------------------------------------
  bsc->apply(TestBlock); // re-applying a clear()-ed BSC detaches all Solvers
  delete bsc;
  delete TestBlock;

  return AllPassed ? 0 : 1;

} // end( main )

/*--------------------------------------------------------------------------*/
/*------------------------ End File test.cpp -------------------------------*/
/*--------------------------------------------------------------------------*/
