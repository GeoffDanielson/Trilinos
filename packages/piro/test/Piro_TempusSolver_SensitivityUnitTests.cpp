/*
// @HEADER
// ************************************************************************
//
//        Piro: Strategy package for embedded analysis capabilitites
//                  Copyright (2010) Sandia Corporation
//
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact Andy Salinger (agsalin@sandia.gov), Sandia
// National Laboratories.
//
// ************************************************************************
// @HEADER
*/

#include "Piro_ConfigDefs.hpp"

#ifdef HAVE_PIRO_TEMPUS
#include "Piro_TempusSolver.hpp"
#include "Tempus_StepperFactory.hpp"
//#include "Tempus_StepperBackwardEuler.hpp"
#include "Piro_ObserverToTempusIntegrationObserverAdapter.hpp"

#ifdef HAVE_PIRO_NOX
#include "Piro_NOXSolver.hpp"
#endif /* HAVE_PIRO_NOX */

#include "Piro_Test_ThyraSupport.hpp"
#include "Piro_Test_WeakenedModelEvaluator.hpp"
#include "Piro_Test_MockObserver.hpp"
#include "Piro_TempusIntegrator.hpp"

#include "MockModelEval_A.hpp"

#include "Thyra_EpetraModelEvaluator.hpp"
#include "Thyra_ModelEvaluatorHelpers.hpp"

#include "Thyra_DefaultNominalBoundsOverrideModelEvaluator.hpp"

#include "Thyra_AmesosLinearOpWithSolveFactory.hpp"

#include "Teuchos_UnitTestHarness.hpp"

#include "Teuchos_Ptr.hpp"
#include "Teuchos_Array.hpp"
#include "Teuchos_Tuple.hpp"

#include <stdexcept>
#include<mpi.h>

using namespace Teuchos;
using namespace Piro;
using namespace Piro::Test;


namespace Thyra {
  typedef ModelEvaluatorBase MEB;
} // namespace Thyra

// Setup support

const RCP<EpetraExt::ModelEvaluator> epetraModelNew()
{
#ifdef HAVE_MPI
  const MPI_Comm comm = MPI_COMM_WORLD;
#else /*HAVE_MPI*/
  const int comm = 0;
#endif /*HAVE_MPI*/
  return rcp(new MockModelEval_A(comm));
}

const RCP<Thyra::ModelEvaluatorDefaultBase<double> > thyraModelNew(const RCP<EpetraExt::ModelEvaluator> &epetraModel)
{
  const RCP<Thyra::LinearOpWithSolveFactoryBase<double> > lowsFactory(new Thyra::AmesosLinearOpWithSolveFactory);
  return epetraModelEvaluator(epetraModel, lowsFactory);
}

RCP<Thyra::ModelEvaluatorDefaultBase<double> > defaultModelNew()
{
  return thyraModelNew(epetraModelNew());
}

const RCP<TempusSolver<double> > solverNew(
    const RCP<Thyra::ModelEvaluatorDefaultBase<double> > &thyraModel,
    double finalTime, 
    const std::string sens_method)
{
  const RCP<ParameterList> tempusPL(new ParameterList("Tempus"));
  tempusPL->set("Integrator Name", "Demo Integrator");
  tempusPL->sublist("Demo Integrator").set("Integrator Type", "Integrator Basic");
  tempusPL->sublist("Demo Integrator").set("Stepper Name", "Demo Stepper");
  tempusPL->sublist("Demo Integrator").sublist("Solution History").set("Storage Type", "Unlimited");
  tempusPL->sublist("Demo Integrator").sublist("Solution History").set("Storage Limit", 20);
  tempusPL->sublist("Demo Integrator").sublist("Time Step Control").set("Initial Time", 0.0);
  tempusPL->sublist("Demo Integrator").sublist("Time Step Control").set("Final Time", finalTime);
  tempusPL->sublist("Demo Stepper").set("Stepper Type", "Backward Euler");
  tempusPL->sublist("Demo Stepper").set("Zero Initial Guess", false);
  tempusPL->sublist("Demo Stepper").set("Solver Name", "Demo Solver");
  tempusPL->sublist("Demo Stepper").sublist("Demo Solver").sublist("NOX").sublist("Direction").set("Method","Newton");
  int sens_method_int = 0; 
  if (sens_method == "None") sens_method_int = 0; 
  else if (sens_method == "Forward") sens_method_int = 1; 
  else if (sens_method == "Adjoint") sens_method_int = 2; 
  Teuchos::RCP<Piro::TempusIntegrator<double> > integrator 
      = Teuchos::rcp(new Piro::TempusIntegrator<double>(tempusPL, thyraModel, sens_method_int));
  auto x0 =  thyraModel->getNominalValues().get_x(); 
  const int num_param = thyraModel->get_p_space(0)->dim();
  RCP<Thyra::MultiVectorBase<double> > DxDp0 =
      Thyra::createMembers(thyraModel->get_x_space(), num_param);
  DxDp0->assign(0.0); 
  integrator->initializeSolutionHistory(0.0, x0, Teuchos::null, Teuchos::null,
                                          DxDp0, Teuchos::null, Teuchos::null);
  const RCP<Thyra::NonlinearSolverBase<double> > stepSolver = Teuchos::null;

  RCP<ParameterList> stepperPL = Teuchos::rcp(&(tempusPL->sublist("Demo Stepper")), false);

  RCP<Tempus::StepperFactory<double> > sf = Teuchos::rcp(new Tempus::StepperFactory<double>());
  const RCP<Tempus::Stepper<double> > stepper = sf->createStepper(stepperPL, thyraModel);
  //const RCP<Tempus::Stepper<double> > stepper = rcp(new Tempus::StepperBackwardEuler<double>(thyraModel, stepperPL));
  return rcp(new TempusSolver<double>(integrator, stepper, stepSolver, thyraModel, finalTime, sens_method));
}

const RCP<TempusSolver<double> > solverNew(
    const RCP<Thyra::ModelEvaluatorDefaultBase<double> > &thyraModel,
    double initialTime,
    double finalTime,
    const RCP<Piro::ObserverBase<double> > &observer,
    const std::string sens_method)
{
  const RCP<ParameterList> tempusPL(new ParameterList("Tempus"));
  tempusPL->set("Integrator Name", "Demo Integrator");
  tempusPL->sublist("Demo Integrator").set("Integrator Type", "Integrator Basic");
  tempusPL->sublist("Demo Integrator").set("Stepper Name", "Demo Stepper");
  tempusPL->sublist("Demo Integrator").sublist("Solution History").set("Storage Type", "Unlimited");
  tempusPL->sublist("Demo Integrator").sublist("Solution History").set("Storage Limit", 20);
  tempusPL->sublist("Demo Integrator").sublist("Time Step Control").set("Initial Time", initialTime);
  tempusPL->sublist("Demo Integrator").sublist("Time Step Control").set("Final Time", finalTime);
  tempusPL->sublist("Demo Stepper").set("Stepper Type", "Backward Euler");
  tempusPL->sublist("Demo Stepper").set("Zero Initial Guess", false);
  tempusPL->sublist("Demo Stepper").set("Solver Name", "Demo Solver");
  tempusPL->sublist("Demo Stepper").sublist("Demo Solver").sublist("NOX").sublist("Direction").set("Method","Newton");
  int sens_method_int = 0; 
  if (sens_method == "None") sens_method_int = 0; 
  else if (sens_method == "Forward") sens_method_int = 1; 
  else if (sens_method == "Adjoint") sens_method_int = 2; 
  Teuchos::RCP<Piro::TempusIntegrator<double> > integrator 
      = Teuchos::rcp(new Piro::TempusIntegrator<double>(tempusPL, thyraModel, sens_method_int));
  auto x0 =  thyraModel->getNominalValues().get_x(); 
  const int num_param = thyraModel->get_p_space(0)->dim();
  RCP<Thyra::MultiVectorBase<double> > DxDp0 =
      Thyra::createMembers(thyraModel->get_x_space(), num_param);
  DxDp0->assign(0.0); 
  integrator->initializeSolutionHistory(0.0, x0, Teuchos::null, Teuchos::null,
                                          DxDp0, Teuchos::null, Teuchos::null);
  const RCP<const Tempus::SolutionHistory<double> > solutionHistory = integrator->getSolutionHistory();
  const RCP<const Tempus::TimeStepControl<double> > timeStepControl = integrator->getTimeStepControl();

  const Teuchos::RCP<Tempus::IntegratorObserver<double> > tempusObserver = Teuchos::rcp(new ObserverToTempusIntegrationObserverAdapter<double>(solutionHistory, timeStepControl, observer));
  integrator->setObserver(tempusObserver);
  const RCP<Thyra::NonlinearSolverBase<double> > stepSolver = Teuchos::null;
  RCP<ParameterList> stepperPL = Teuchos::rcp(&(tempusPL->sublist("Demo Stepper")), false);
  RCP<Tempus::StepperFactory<double> > sf = Teuchos::rcp(new Tempus::StepperFactory<double>());
  const RCP<Tempus::Stepper<double> > stepper = sf->createStepper(stepperPL, thyraModel);
  //const RCP<Tempus::Stepper<double> > stepper = rcp(new Tempus::StepperBackwardEuler<double>(thyraModel, stepperPL));

  return rcp(new TempusSolver<double>(integrator, stepper, stepSolver, thyraModel, initialTime, finalTime, sens_method));
}

const RCP<TempusSolver<double> > solverNew(
    const RCP<Thyra::ModelEvaluatorDefaultBase<double> > &thyraModel,
    double initialTime,
    double finalTime,
    double fixedTimeStep,
    const RCP<Piro::ObserverBase<double> > &observer,
    const std::string sens_method)
{
  const RCP<ParameterList> tempusPL(new ParameterList("Tempus"));
  tempusPL->set("Integrator Name", "Demo Integrator");
  tempusPL->sublist("Demo Integrator").set("Integrator Type", "Integrator Basic");
  tempusPL->sublist("Demo Integrator").set("Stepper Name", "Demo Stepper");
  tempusPL->sublist("Demo Integrator").sublist("Solution History").set("Storage Type", "Unlimited");
  tempusPL->sublist("Demo Integrator").sublist("Solution History").set("Storage Limit", 20);
  tempusPL->sublist("Demo Integrator").sublist("Time Step Control").set("Initial Time", initialTime);
  tempusPL->sublist("Demo Integrator").sublist("Time Step Control").set("Final Time", finalTime);
  tempusPL->sublist("Demo Integrator").sublist("Time Step Control").set("Minimum Time Step", fixedTimeStep);
  tempusPL->sublist("Demo Integrator").sublist("Time Step Control").set("Initial Time Step", fixedTimeStep);
  tempusPL->sublist("Demo Integrator").sublist("Time Step Control").set("Maximum Time Step", fixedTimeStep);
  tempusPL->sublist("Demo Stepper").set("Stepper Type", "Backward Euler");
  tempusPL->sublist("Demo Stepper").set("Zero Initial Guess", false);
  tempusPL->sublist("Demo Stepper").set("Solver Name", "Demo Solver");
  tempusPL->sublist("Demo Stepper").sublist("Demo Solver").sublist("NOX").sublist("Direction").set("Method","Newton");
  int sens_method_int = 0; 
  if (sens_method == "None") sens_method_int = 0; 
  else if (sens_method == "Forward") sens_method_int = 1; 
  else if (sens_method == "Adjoint") sens_method_int = 2; 
  Teuchos::RCP<Piro::TempusIntegrator<double> > integrator 
      = Teuchos::rcp(new Piro::TempusIntegrator<double>(tempusPL, thyraModel, sens_method_int));
  const RCP<const Tempus::SolutionHistory<double> > solutionHistory = integrator->getSolutionHistory();
  const RCP<const Tempus::TimeStepControl<double> > timeStepControl = integrator->getTimeStepControl();

  const Teuchos::RCP<Tempus::IntegratorObserver<double> > tempusObserver = Teuchos::rcp(new ObserverToTempusIntegrationObserverAdapter<double>(solutionHistory, timeStepControl, observer));
  integrator->setObserver(tempusObserver);
  const RCP<Thyra::NonlinearSolverBase<double> > stepSolver = Teuchos::null;
  RCP<ParameterList> stepperPL = Teuchos::rcp(&(tempusPL->sublist("Demo Stepper")), false);
  RCP<Tempus::StepperFactory<double> > sf = Teuchos::rcp(new Tempus::StepperFactory<double>());
  const RCP<Tempus::Stepper<double> > stepper = sf->createStepper(stepperPL, thyraModel);
  //const RCP<Tempus::Stepper<double> > stepper = rcp(new Tempus::StepperBackwardEuler<double>(thyraModel, stepperPL));

  return rcp(new TempusSolver<double>(integrator, stepper, stepSolver, thyraModel, initialTime, finalTime, sens_method));
}


Thyra::ModelEvaluatorBase::InArgs<double> getStaticNominalValues(const Thyra::ModelEvaluator<double> &model)
{
  Thyra::ModelEvaluatorBase::InArgs<double> result = model.getNominalValues();
  if (result.supports(Thyra::ModelEvaluatorBase::IN_ARG_x_dot)) {
    result.set_x_dot(Teuchos::null);
  }
  return result;
}


// Floating point tolerance
const double tol = 1.0e-8;

TEUCHOS_UNIT_TEST(Piro_TempusSolver, TimeZero_DefaultSolutionSensitivity)
{
  Teuchos::RCP<Teuchos::FancyOStream> fos =
      Teuchos::VerboseObjectBase::getDefaultOStream();

  const RCP<Thyra::ModelEvaluatorDefaultBase<double> > model = defaultModelNew();
  const double finalTime = 0.0;

  const RCP<TempusSolver<double> > solver = solverNew(model, finalTime, "Forward");

  const Thyra::MEB::InArgs<double> inArgs = solver->getNominalValues();

  Thyra::MEB::OutArgs<double> outArgs = solver->createOutArgs();
  const int solutionResponseIndex = solver->Ng() - 1;
  const int parameterIndex = 0;
  const Thyra::MEB::Derivative<double> dxdp_deriv =
    Thyra::create_DgDp_mv(*solver, solutionResponseIndex, parameterIndex, Thyra::MEB::DERIV_MV_JACOBIAN_FORM);
  const RCP<Thyra::MultiVectorBase<double> > dxdp = dxdp_deriv.getMultiVector();
  outArgs.set_DgDp(solutionResponseIndex, parameterIndex, dxdp_deriv);

  solver->evalModel(inArgs, outArgs);
  
  // Test if at 'Final Time'
  double time = solver->getPiroTempusIntegrator()->getTime();
  TEST_FLOATING_EQUALITY(time, 0.0, 1.0e-14);

  const Array<Array<double> > expected = tuple(
      Array<double>(tuple(0.0, 0.0, 0.0, 0.0)),
      Array<double>(tuple(0.0, 0.0, 0.0, 0.0)));
  RCP<const Thyra::MultiVectorBase<double> > DxDp = solver->getPiroTempusIntegrator()->getDxDp();
  //IKT, 5/11/2020: question for Eric: why is dxdp giving the wrong value here??  Need to call and 
  //query getDxDp() to get right values of sensitivities.  Otherwise, it appears we get uninitialized values.
  TEST_EQUALITY(DxDp->domain()->dim(), expected.size());

  for (int i = 0; i < expected.size(); ++i) {
    TEST_EQUALITY(DxDp->range()->dim(), expected[i].size());
    const Array<double> actual = arrayFromVector(*DxDp->col(i));
    /**fos << "IKT i = " << i << "\n";  
    for (int j=0; j<actual.size(); j++) {
      *fos << "  IKT j, actual, expected = " << j << ", " << actual[j] << ", " << expected[i][j] << "\n"; 
    }*/
    TEST_COMPARE_FLOATING_ARRAYS(actual, expected[i], tol);
  }
}

TEUCHOS_UNIT_TEST(Piro_TempusSolver, TimeZero_DefaultSolutionSensitivityOp)
{
  Teuchos::RCP<Teuchos::FancyOStream> fos =
      Teuchos::VerboseObjectBase::getDefaultOStream();
  
  const RCP<Thyra::ModelEvaluatorDefaultBase<double> > model = defaultModelNew();
  const double finalTime = 0.0;

  const RCP<TempusSolver<double> > solver = solverNew(model, finalTime, "Forward");

  const Thyra::MEB::InArgs<double> inArgs = solver->getNominalValues();

  Thyra::MEB::OutArgs<double> outArgs = solver->createOutArgs();
  const int solutionResponseIndex = solver->Ng() - 1;
  const int parameterIndex = 0;
  const Thyra::MEB::Derivative<double> dxdp_deriv =
    solver->create_DgDp_op(solutionResponseIndex, parameterIndex);
  const RCP<Thyra::LinearOpBase<double> > dxdp = dxdp_deriv.getLinearOp();
  outArgs.set_DgDp(solutionResponseIndex, parameterIndex, dxdp_deriv);

  solver->evalModel(inArgs, outArgs);
  
  // Test if at 'Final Time'
  double time = solver->getPiroTempusIntegrator()->getTime();
  TEST_FLOATING_EQUALITY(time, 0.0, 1.0e-14);
  
  const Array<Array<double> > expected = tuple(
      Array<double>(tuple(0.0, 0.0, 0.0, 0.0)),
      Array<double>(tuple(0.0, 0.0, 0.0, 0.0)));
  RCP<const Thyra::MultiVectorBase<double> > DxDp = solver->getPiroTempusIntegrator()->getDxDp();
  //IKT, 5/11/2020: question for Eric: why is dxdp giving the wrong value here??  Need to call and 
  //query getDxDp() to get right values of sensitivities.  Otherwise, it appears we get uninitialized values.
  TEST_EQUALITY(DxDp->domain()->dim(), expected.size());
  for (int i = 0; i < expected.size(); ++i) {
  TEST_EQUALITY(dxdp->range()->dim(), expected[i].size());
    const Array<double> actual = arrayFromLinOp(*DxDp, i);
    /**fos << "IKT i = " << i << "\n";  
    for (int j=0; j<actual.size(); j++) {
      *fos << "  IKT j, actual, expected = " << j << ", " << actual[j] << ", " << expected[i][j] << "\n"; 
    }*/
    TEST_COMPARE_FLOATING_ARRAYS(actual, expected[i], tol);
  }
}

TEUCHOS_UNIT_TEST(Piro_TempusSolver, TimeZero_DefaultResponseSensitivity)
{
  const RCP<Thyra::ModelEvaluatorDefaultBase<double> > model = defaultModelNew();

  const int responseIndex = 0;
  const int parameterIndex = 0;

  const Thyra::MEB::Derivative<double> dgdp_deriv_expected =
    Thyra::create_DgDp_mv(*model, responseIndex, parameterIndex, Thyra::MEB::DERIV_MV_JACOBIAN_FORM);
  const RCP<const Thyra::MultiVectorBase<double> > dgdp_expected = dgdp_deriv_expected.getMultiVector();
  {
    const Thyra::MEB::InArgs<double> modelInArgs = getStaticNominalValues(*model);
    Thyra::MEB::OutArgs<double> modelOutArgs = model->createOutArgs();
    modelOutArgs.set_DgDp(responseIndex, parameterIndex, dgdp_deriv_expected);
    model->evalModel(modelInArgs, modelOutArgs);
  }

  const double finalTime = 0.0;
  const RCP<TempusSolver<double> > solver = solverNew(model, finalTime, "Forward");

  const Thyra::MEB::InArgs<double> inArgs = solver->getNominalValues();

  Thyra::MEB::OutArgs<double> outArgs = solver->createOutArgs();
  const Thyra::MEB::Derivative<double> dgdp_deriv =
    Thyra::create_DgDp_mv(*solver, responseIndex, parameterIndex, Thyra::MEB::DERIV_MV_JACOBIAN_FORM);
  const RCP<const Thyra::MultiVectorBase<double> > dgdp = dgdp_deriv.getMultiVector();
  outArgs.set_DgDp(responseIndex, parameterIndex, dgdp_deriv);

  solver->evalModel(inArgs, outArgs);

  TEST_EQUALITY(dgdp->domain()->dim(), dgdp_expected->domain()->dim());
  TEST_EQUALITY(dgdp->range()->dim(), dgdp_expected->range()->dim());
  for (int i = 0; i < dgdp_expected->domain()->dim(); ++i) {
    const Array<double> actual = arrayFromVector(*dgdp->col(i));
    const Array<double> expected = arrayFromVector(*dgdp_expected->col(i));
    TEST_COMPARE_FLOATING_ARRAYS(actual, expected, tol);
  }
}

TEUCHOS_UNIT_TEST(Piro_TempusSolver, TimeZero_DefaultResponseSensitivity_NoDgDxMv)
{
  Teuchos::RCP<Teuchos::FancyOStream> fos =
      Teuchos::VerboseObjectBase::getDefaultOStream();
  
  const RCP<Thyra::ModelEvaluatorDefaultBase<double> > model(
    new WeakenedModelEvaluator_NoDgDxMv(defaultModelNew()));

  const int responseIndex = 0;
  const int parameterIndex = 0;

  const Thyra::MEB::Derivative<double> dgdp_deriv_expected =
    Thyra::create_DgDp_mv(*model, responseIndex, parameterIndex, Thyra::MEB::DERIV_MV_JACOBIAN_FORM);
  const RCP<const Thyra::MultiVectorBase<double> > dgdp_expected = dgdp_deriv_expected.getMultiVector();
  {
    const Thyra::MEB::InArgs<double> modelInArgs = getStaticNominalValues(*model);
    Thyra::MEB::OutArgs<double> modelOutArgs = model->createOutArgs();
    modelOutArgs.set_DgDp(responseIndex, parameterIndex, dgdp_deriv_expected);
    model->evalModel(modelInArgs, modelOutArgs);
  }

  const double finalTime = 0.0;
  const RCP<TempusSolver<double> > solver = solverNew(model, finalTime, "Forward");

  const Thyra::MEB::InArgs<double> inArgs = solver->getNominalValues();

  Thyra::MEB::OutArgs<double> outArgs = solver->createOutArgs();
  const Thyra::MEB::Derivative<double> dgdp_deriv =
    Thyra::create_DgDp_mv(*solver, responseIndex, parameterIndex, Thyra::MEB::DERIV_MV_JACOBIAN_FORM);
  const RCP<const Thyra::MultiVectorBase<double> > dgdp = dgdp_deriv.getMultiVector();
  outArgs.set_DgDp(responseIndex, parameterIndex, dgdp_deriv);

  solver->evalModel(inArgs, outArgs);
  
  TEST_EQUALITY(dgdp->domain()->dim(), dgdp_expected->domain()->dim());
  TEST_EQUALITY(dgdp->range()->dim(), dgdp_expected->range()->dim());
  for (int i = 0; i < dgdp_expected->domain()->dim(); ++i) {
    const Array<double> actual = arrayFromVector(*dgdp->col(i));
    const Array<double> expected = arrayFromVector(*dgdp_expected->col(i));
    //*fos << "  IKT i, actual, expected = " << i << ", " << actual[i] << ", " << expected[i] << "\n"; 
    TEST_COMPARE_FLOATING_ARRAYS(actual, expected, tol);
  }
}


TEUCHOS_UNIT_TEST(Piro_TempusSolver, TimeZero_ResponseAndDefaultSensitivities)
{
  Teuchos::RCP<Teuchos::FancyOStream> fos =
      Teuchos::VerboseObjectBase::getDefaultOStream();
  
  const RCP<Thyra::ModelEvaluatorDefaultBase<double> > model = defaultModelNew();

  const int responseIndex = 0;
  const int parameterIndex = 0;

  const RCP<Thyra::VectorBase<double> > expectedResponse =
    Thyra::createMember(model->get_g_space(responseIndex));
  {
    const Thyra::MEB::InArgs<double> modelInArgs = getStaticNominalValues(*model);
    Thyra::MEB::OutArgs<double> modelOutArgs = model->createOutArgs();
    modelOutArgs.set_g(responseIndex, expectedResponse);
    model->evalModel(modelInArgs, modelOutArgs);
  }

  const Thyra::MEB::Derivative<double> dgdp_deriv_expected =
    Thyra::create_DgDp_mv(*model, responseIndex, parameterIndex, Thyra::MEB::DERIV_MV_JACOBIAN_FORM);
  const RCP<const Thyra::MultiVectorBase<double> > dgdp_expected = dgdp_deriv_expected.getMultiVector();
  {
    const Thyra::MEB::InArgs<double> modelInArgs = getStaticNominalValues(*model);
    Thyra::MEB::OutArgs<double> modelOutArgs = model->createOutArgs();
    modelOutArgs.set_DgDp(responseIndex, parameterIndex, dgdp_deriv_expected);
    model->evalModel(modelInArgs, modelOutArgs);
  }

  const double finalTime = 0.0;
  const RCP<TempusSolver<double> > solver = solverNew(model, finalTime, "Forward");

  const Thyra::MEB::InArgs<double> inArgs = solver->getNominalValues();

  Thyra::MEB::OutArgs<double> outArgs = solver->createOutArgs();

  // Requesting response
  const RCP<Thyra::VectorBase<double> > response =
    Thyra::createMember(solver->get_g_space(responseIndex));
  outArgs.set_g(responseIndex, response);

  // Requesting response sensitivity
  const Thyra::MEB::Derivative<double> dgdp_deriv =
    Thyra::create_DgDp_mv(*solver, responseIndex, parameterIndex, Thyra::MEB::DERIV_MV_JACOBIAN_FORM);
  const RCP<const Thyra::MultiVectorBase<double> > dgdp = dgdp_deriv.getMultiVector();
  outArgs.set_DgDp(responseIndex, parameterIndex, dgdp_deriv);

  // Run solver
  solver->evalModel(inArgs, outArgs);

  // Checking response
  {
    const Array<double> expected = arrayFromVector(*expectedResponse);
    const Array<double> actual = arrayFromVector(*response);
    TEST_COMPARE_FLOATING_ARRAYS(actual, expected, tol);
  }

  // Checking sensitivity
  {
    TEST_EQUALITY(dgdp->domain()->dim(), dgdp_expected->domain()->dim());
    TEST_EQUALITY(dgdp->range()->dim(), dgdp_expected->range()->dim());
    for (int i = 0; i < dgdp_expected->domain()->dim(); ++i) {
      const Array<double> actual = arrayFromVector(*dgdp->col(i));
      const Array<double> expected = arrayFromVector(*dgdp_expected->col(i));
      //*fos << "  IKT i, actual, expected = " << i << ", " << actual[i] << ", " << expected[i] << "\n"; 
      TEST_COMPARE_FLOATING_ARRAYS(actual, expected, tol);
    }
  }
}
#endif /* HAVE_PIRO_TEMPUS */
