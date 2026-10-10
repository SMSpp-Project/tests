/*--------------------------------------------------------------------------*/
/*------------------------- File BlockCopySolver.h -------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Header file for the class BlockCopySolver, a Solver of the testers that
 * solves a copy of its Block configured by a BlockConfig of its own, so that
 * the formulations that the BlockConfig selects can be compared, in one run,
 * as further Solver of the BlockSolverConfig of the Block.
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
/*----------------------------- DEFINITIONS --------------------------------*/
/*--------------------------------------------------------------------------*/

#ifndef __BlockCopySolver
 #define __BlockCopySolver
                      /* self-identification: #endif at the end of the file */

/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include "Solver.h"

#include <array>

#include <string>

/*--------------------------------------------------------------------------*/
/*----------------------------- NAMESPACE ----------------------------------*/
/*--------------------------------------------------------------------------*/

namespace SMSpp_di_unipi_it
{

/*--------------------------------------------------------------------------*/
/*------------------------ CLASS BlockCopySolver ---------------------------*/
/*--------------------------------------------------------------------------*/
/// a Solver that solves a copy of its Block in another formulation
/** At each compute() the BlockCopySolver writes its Block in a netCDF group
 * in memory and reads it back into a copy, which therefore has the data of
 * the Block as they are at that moment (the changes of the data issued so
 * far included); it gives the copy the reserves and the reactive power the
 * Block has, if the Block is a UnitBlock, applies to it the (meta-)
 * BlockConfig of the file in the string parameter "strBlockCfg", generates
 * its abstract representation, fixes the Variable that are fixed in the
 * Block (those of the groups of the same name and size), applies to it the
 * (meta-)BlockSolverConfig of the file in the string parameter
 * "strInnerBSC" and computes the first Solver that it registers, whose
 * status it returns. The bounds and the value are those of that Solver,
 * get_Solution() returns the Solution of the copy (which has the class of
 * the Block), and get_var_solution() writes in the Variable of the Block
 * the values of those of the copy of the same group, then lets the Block
 * derive the others if it is a ThermalUnitBlock [see
 * ThermalUnitBlock::set_solution()]. The Modification of the Block are not
 * read: the copy is written anew at the next compute(). The copy and its
 * Solver are deleted at the next compute() and by the destructor. */

class BlockCopySolver : public Solver
{
/*--------------------------------------------------------------------------*/
/*----------------------- PUBLIC PART OF THE CLASS -------------------------*/
/*--------------------------------------------------------------------------*/

 public:

/*--------------------------------------------------------------------------*/
/*--------------------- CONSTRUCTOR AND DESTRUCTOR -------------------------*/
/*--------------------------------------------------------------------------*/

 BlockCopySolver( void ) : Solver() {}

 ~BlockCopySolver() override;

/*--------------------------------------------------------------------------*/
/*--------------------- DERIVED METHODS OF BASE CLASS ----------------------*/
/*--------------------------------------------------------------------------*/

 /// solves a copy of the Block [see the class comment]
 int compute( bool changedvars = true ) override;

 /// the lower bound of the Solver of the copy
 OFValue get_lb( void ) override;

 /// the upper bound of the Solver of the copy
 OFValue get_ub( void ) override;

 /// the value of the solution of the Solver of the copy
 OFValue get_var_value( void ) override;

 /// true if the Solver of the copy has a solution
 bool has_var_solution( void ) override;

 /// writes the solution of the copy in the Variable of the Block
 void get_var_solution( Configuration * solc = nullptr ) override;

 /// the Solution of the copy, which has the class of that of the Block
 Solution * get_Solution( Configuration * solc = nullptr ) override;

 /// the Modification are not read, the copy being written anew
 void add_Modification( sp_Mod & mod ) override {}

/*--------------------------------------------------------------------------*/
/*---------------------------- THE PARAMETERS ------------------------------*/
/*--------------------------------------------------------------------------*/

 [[nodiscard]] idx_type get_num_str_par( void ) const override {
  return( Solver::get_num_str_par() + 2 );
  }

 [[nodiscard]] const std::string & get_dflt_str_par( idx_type par )
  const override;

 [[nodiscard]] idx_type str_par_str2idx( const std::string & name )
  const override;

 [[nodiscard]] const std::string & str_par_idx2str( idx_type idx )
  const override;

 [[nodiscard]] const std::string & get_str_par( idx_type par )
  const override;

 void set_par( idx_type par , std::string && value ) override;

/*--------------------------------------------------------------------------*/
/*-------------------- PRIVATE PART OF THE CLASS ---------------------------*/
/*--------------------------------------------------------------------------*/

 private:

 /// deletes the copy and its Solver
 void clear_copy( void );

 /// the files of the BlockConfig and of the BlockSolverConfig of the copy
 std::array< std::string , 2 > f_files;

 Block * f_copy = nullptr;            ///< the copy of the Block
 Configuration * f_bsc = nullptr;     ///< the BlockSolverConfig of the copy
 Solver * f_inner = nullptr;          ///< the Solver of the copy
 int f_status = kError;               ///< what compute() returned

 SMSpp_insert_in_factory_h;

 };  // end( class( BlockCopySolver ) )

/*--------------------------------------------------------------------------*/

}  // end( namespace SMSpp_di_unipi_it )

/*--------------------------------------------------------------------------*/

#endif  /* BlockCopySolver.h included */

/*--------------------------------------------------------------------------*/
/*------------------------ End File BlockCopySolver.h ----------------------*/
/*--------------------------------------------------------------------------*/
