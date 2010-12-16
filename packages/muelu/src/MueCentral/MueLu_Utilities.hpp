#ifndef MUELU_UTILITIES_HPP
#define MUELU_UTILITIES_HPP

#include <Cthulhu_Map.hpp>
#include <Cthulhu_CrsMatrix.hpp>
#include <Cthulhu_EpetraCrsMatrix.hpp>
#include <Cthulhu_CrsOperator.hpp>
#include <Cthulhu.hpp>

#include "MueLu_MatrixFactory.hpp"

#include "MueLu_UseShortNames.hpp"

namespace MueLu {

  using Teuchos::RCP;
  using Teuchos::rcp;
  using Teuchos::rcp_dynamic_cast;
  using Cthulhu::EpetraCrsMatrix;

  //! @brief Helper utility to pull out the underlying Epetra_CrsMatrix from an Cthulhu::Operator.
  RCP<const Epetra_CrsMatrix> Op2EpetraCrs(RCP<Operator> Op) {
    RCP<const Epetra_CrsMatrix> A;
    // Get the underlying Epetra Mtx
    RCP<const CrsOperator> crsOp = rcp_dynamic_cast<const CrsOperator>(Op);
    if (crsOp == Teuchos::null)
      throw(std::logic_error("Cast from Cthulhu::Operator to Cthulhu::CrsOperator failed"));
    RCP<const CrsMatrix> tmp_CrsMtx = crsOp->getCrsMatrix();
    const RCP<const EpetraCrsMatrix> &tmp_ECrsMtx = rcp_dynamic_cast<const EpetraCrsMatrix>(tmp_CrsMtx);
    if (tmp_ECrsMtx == Teuchos::null)
      throw(std::logic_error("Cast from Cthulhu::CrsMatrix to Cthulhu::EpetraCrsMatrix failed"));
    A = tmp_ECrsMtx->getEpetra_CrsMatrix();
    return A;
  } //Op2EpetraCrs

  //! @brief Helper utility to pull out the underlying Epetra_CrsMatrix from an Cthulhu::Operator.
  RCP<Epetra_CrsMatrix> Op2NonConstEpetraCrs(RCP<Operator> Op) {
    RCP<Epetra_CrsMatrix> A;
    // Get the underlying Epetra Mtx
    RCP<const CrsOperator> crsOp = rcp_dynamic_cast<const CrsOperator>(Op);
    if (crsOp == Teuchos::null)
      throw(std::logic_error("Cast from Cthulhu::Operator to Cthulhu::CrsOperator failed"));
    RCP<const CrsMatrix> tmp_CrsMtx = crsOp->getCrsMatrix();
    const RCP<const EpetraCrsMatrix> &tmp_ECrsMtx = rcp_dynamic_cast<const EpetraCrsMatrix>(tmp_CrsMtx);
    if (tmp_ECrsMtx == Teuchos::null)
      throw(std::logic_error("Cast from Cthulhu::CrsMatrix to Cthulhu::EpetraCrsMatrix failed"));
    A = tmp_ECrsMtx->getEpetra_CrsMatrixNonConst();
    return A;
  } //Op2NonConstEpetraCrs

  /*! @brief Helper function to do matrix-matrix multiply "in-place"

      Returns RCP to non-constant Cthulhu::Operator.
  */
  RCP<Operator> TwoMatrixMultiply(RCP<Operator> const &A, RCP<Operator> const &B)
  {
    RCP<const Epetra_CrsMatrix> epA = Op2EpetraCrs(A);
    RCP<const Epetra_CrsMatrix> epB = Op2EpetraCrs(B);
    //FIXME 30 is likely a big overestimate
    RCP< Operator > C = rcp( new CrsOperator(A->getRowMap(), 30) );
    RCP<Epetra_CrsMatrix> epC = Op2NonConstEpetraCrs(C);
    int i = EpetraExt::MatrixMatrix::Multiply(*epA,false,*epB,false,*epC);
    if (i != 0) {
      std::ostringstream buf;
      buf << i;
      std::string msg = "EpetraExt::MatrixMatrix::Multiply return value of " + buf.str();
      throw(std::logic_error(msg));
    }
    return C;
  } //TwoMatrixMultiply()

  /*! @brief Helper function to calculate alpha*A + beta*B.

      @param A (required) left matrix operand
      @param B (required) right matrix operand
      @param alpha (optional) scalar multiplier for A, defaults to 1.0
      @param beta  (optional) scalar multiplier for B, defaults to 1.0

      @return Teuchos::RCP to non-constant Cthulhu::Operator.
  */
  RCP<Operator> TwoMatrixAdd(RCP<Operator> const &A, RCP<Operator> const &B,
                             SC alpha=1.0, SC beta=1.0)
  {
    RCP<const Epetra_CrsMatrix> epA = Op2EpetraCrs(A);
    RCP<const Epetra_CrsMatrix> epB = Op2EpetraCrs(B);
    //FIXME 30 is a complete guess as to the #nonzeros per row
    RCP< Operator > C = rcp( new CrsOperator(A->getRowMap(), 30) );
    RCP<Epetra_CrsMatrix> epC = Op2NonConstEpetraCrs(C);
    //int i = EpetraExt::MatrixMatrix::Add(*epA,false,(double)alpha,*epB,false,(double)beta,&(*epC));
    Epetra_CrsMatrix* ref2epC = &*epC; //to avoid a compiler error...
    int i = EpetraExt::MatrixMatrix::Add(*epA,false,(double)alpha,*epB,false,(double)beta,ref2epC);
    if (i != 0) {
      std::ostringstream buf;
      buf << i;
      std::string msg = "EpetraExt::MatrixMatrix::Add return value of " + buf.str();
      throw(std::logic_error(msg));
    }
    return C;
  } //TwoMatrixAdd()

  void MatrixPrint(RCP<Operator> Op) {
    RCP<const Epetra_CrsMatrix> epOp = Op2EpetraCrs(Op);
    std::cout << *epOp << std::endl;
  }

  /*! @brief Get Operator Diagonal
  */
  RCP<Operator> BuildMatrixDiagonal(RCP<Operator> const &A)
  {
     const RCP<const Map> rowmap = A->getRowMap();
     //std::vector<SC> diag(A->getNodeNumRows());
     std::vector<SC> diag(rowmap->getNodeNumElements());
     Teuchos::ArrayView<const LO> cols;
     Teuchos::ArrayView<const SC> vals;
     //for (size_t i=0; i<A->getNodeNumRows(); ++i) {
     for (size_t i=0; i<rowmap->getNodeNumElements(); ++i) {
       A->getLocalRowView(i,cols,vals);
       for (Teuchos::ArrayView<const LO>::size_type j=0; j<cols.size(); j++) {
         //TODO this will break down if diagonal entry is not present
         //if (!(cols[j] > i)) {  //JG says this will work ... maybe
         if (cols[j] == i) {
           diag[i] = vals[j];
           break;
         }
       }
     }

     Teuchos::RCP< Operator > D = Teuchos::rcp( new CrsOperator(rowmap, 1) );
     std::vector<LO> diagInd(1);
     Teuchos::ArrayView<GO> iv(&diagInd[0],1);
     //for (size_t i=0; i< A->getNodeNumRows(); ++i) {
     for (size_t i=0; i< rowmap->getNodeNumElements(); ++i) {
       Teuchos::ArrayView<SC> av(&diag[i],1);
       diagInd[0] = rowmap->getGlobalElement(i);
       D->insertGlobalValues(i,iv,av);
     }
     D->fillComplete();
     //MatrixPrint(D);

     return D;

  } //BuildMatrixDiagonal()

  /*! @brief Get reciprocal of Operator diagonal
  */
  RCP<Operator> BuildMatrixInverseDiagonal(RCP<Operator> const &A)
  {
     const RCP<const Map> rowmap = A->getRowMap();
     //std::vector<SC> diag(A->getNodeNumRows());
     std::vector<SC> diag(rowmap->getNodeNumElements());
     Teuchos::ArrayView<const LO> cols;
     Teuchos::ArrayView<const SC> vals;
     //for (size_t i=0; i<A->getNodeNumRows(); ++i) {
     for (size_t i=0; i<rowmap->getNodeNumElements(); ++i) {
       A->getLocalRowView(i,cols,vals);
       for (size_t j=0; j<cols.size(); j++) {
         //TODO this will break down if diagonal entry is not present
         if (cols[j] == i) {
           diag[i] = 1 / vals[j];
           break;
         }
       }
     }

     Teuchos::RCP< Operator > D = Teuchos::rcp( new CrsOperator(rowmap, 1) );
     std::vector<LO> diagInd(1);
     Teuchos::ArrayView<GO> iv(&diagInd[0],1);
     //for (size_t i=0; i< A->getNodeNumRows(); ++i) {
     for (size_t i=0; i< rowmap->getNodeNumElements(); ++i) {
       Teuchos::ArrayView<SC> av(&diag[i],1);
       diagInd[0] = rowmap->getGlobalElement(i);
       D->insertGlobalValues(i,iv,av);
     }
     D->fillComplete();

     return D;

  } //BuildMatrixInverseDiagonal()


} //namespace MueLu

#define MUELU_UTILITIES_SHORT

#endif //ifndef MUELU_UTILITIES_HPP
