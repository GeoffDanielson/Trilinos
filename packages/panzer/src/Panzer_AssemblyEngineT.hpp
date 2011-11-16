#include "Epetra_Vector.h"
#include "Epetra_MultiVector.h"
#include "Epetra_CrsMatrix.h"
#include "Phalanx_FieldManager.hpp"
#include "Panzer_FieldManagerBuilder.hpp"
#include "Panzer_AssemblyEngine_InArgs.hpp"

//===========================================================================
//===========================================================================
template <typename EvalT,typename LO,typename GO>
panzer::AssemblyEngine<EvalT,LO,GO>::
AssemblyEngine(const Teuchos::RCP<panzer::FieldManagerBuilder<LO,GO> >& fmb,
               const Teuchos::RCP<const panzer::LinearObjFactory<panzer::Traits> > & lof)
  : m_field_manager_builder(fmb), m_lin_obj_factory(lof)
{ 

}

//===========================================================================
//===========================================================================
template <typename EvalT,typename LO,typename GO>
void panzer::AssemblyEngine<EvalT,LO,GO>::
evaluate(const panzer::AssemblyEngineInArgs& in)
{
  typedef LinearObjContainer LOC;

  // Push solution, x and dxdt into ghosted domain
  m_lin_obj_factory->globalToGhostContainer(*in.container_,*in.ghostedContainer_,LOC::X | LOC::DxDt);

  // *********************
  // Volumetric fill
  // *********************
  this->evaluateVolume(in);

  // *********************
  // BC fill
  // *********************
  // NOTE: We have to split neumann and dirichlet bcs since dirichlet
  // bcs overwrite equations where neumann sum into equations.  Make
  // sure all neumann are done before dirichlet.

  this->evaluateNeumannBCs(in);

  // Dirchlet conditions require a global matrix
  this->evaluateDirichletBCs(in);

  m_lin_obj_factory->ghostToGlobalContainer(*in.ghostedContainer_,*in.container_,LOC::F | LOC::Mat);

  return;
}

//===========================================================================
//===========================================================================
template <typename EvalT,typename LO,typename GO>
void panzer::AssemblyEngine<EvalT,LO,GO>::
evaluateVolume(const panzer::AssemblyEngineInArgs& in)
{
  const std::vector< Teuchos::RCP<std::vector<panzer::Workset> > >& 
    worksets = m_field_manager_builder->getWorksets();

  // Loop over element blocks
  for (std::size_t block = 0; block < worksets.size(); ++block) {

    std::vector<panzer::Workset>& w = *worksets[block]; 

    Teuchos::RCP< PHX::FieldManager<panzer::Traits> > fm = 
	m_field_manager_builder->getVolumeFieldManagers()[block];

    Traits::PED preEvalData;

    fm->template preEvaluate<EvalT>(preEvalData);

    // Loop over worksets in this element block
    for (std::size_t i = 0; i < w.size(); ++i) {
	panzer::Workset& workset = w[i];

      workset.ghostedLinContainer = in.ghostedContainer_;
      workset.linContainer = in.container_;
      workset.alpha = in.alpha;
      workset.beta = in.beta;
      workset.time = in.time;
      workset.evaluate_transient_terms = in.evaluate_transient_terms;

	fm->template evaluateFields<EvalT>(workset);
    }

    fm->template postEvaluate<EvalT>(NULL);
  }
}

//===========================================================================
//===========================================================================
template <typename EvalT,typename LO,typename GO>
void panzer::AssemblyEngine<EvalT,LO,GO>::
evaluateNeumannBCs(const panzer::AssemblyEngineInArgs& in)
{
  this->evaluateBCs(panzer::BCT_Neumann, in);
}

//===========================================================================
//===========================================================================
template <typename EvalT,typename LO,typename GO>
void panzer::AssemblyEngine<EvalT,LO,GO>::
evaluateDirichletBCs(const panzer::AssemblyEngineInArgs& in)
{
  typedef LinearObjContainer LOC;

  // allocate a counter to keep track of where this processor set dirichlet boundary conditions
  Teuchos::RCP<LinearObjContainer> localCounter = m_lin_obj_factory->buildPrimitiveGhostedLinearObjContainer();
  m_lin_obj_factory->initializeGhostedContainer(LinearObjContainer::X,*localCounter); // store counter in X
  localCounter->initialize();
     // this has only an X vector. The evaluate BCs will add a one to each row
     // that has been set as a dirichlet condition on this processor

  // apply dirichlet conditions, make sure to keep track of the local counter
  this->evaluateBCs(panzer::BCT_Dirichlet, in,localCounter);

  Teuchos::RCP<LinearObjContainer> summedGhostedCounter = m_lin_obj_factory->buildPrimitiveGhostedLinearObjContainer();
  m_lin_obj_factory->initializeGhostedContainer(LinearObjContainer::X,*summedGhostedCounter); // store counter in X
  summedGhostedCounter->initialize();

  // do communication to build summed ghosted counter for dirichlet conditions
  {
     Teuchos::RCP<LinearObjContainer> globalCounter = m_lin_obj_factory->buildPrimitiveLinearObjContainer();
     m_lin_obj_factory->initializeContainer(LinearObjContainer::X,*globalCounter); // store counter in X
     globalCounter->initialize();
     m_lin_obj_factory->ghostToGlobalContainer(*localCounter,*globalCounter,LOC::X);
        // Here we do the reduction across all processors so that the number of times
        // a dirichlet condition is applied is summed into the global counter

     m_lin_obj_factory->globalToGhostContainer(*globalCounter,*summedGhostedCounter,LOC::X);
        // finally we move the summed global vector into a local ghosted vector
        // so that the dirichlet conditions can be applied to both the ghosted
        // right hand side and the ghosted matrix
  }

  // adjust ghosted system for boundary conditions
  m_lin_obj_factory->adjustForDirichletConditions(*localCounter,*summedGhostedCounter,*in.ghostedContainer_);
}

//===========================================================================
//===========================================================================
template <typename EvalT,typename LO,typename GO>
void panzer::AssemblyEngine<EvalT,LO,GO>::
evaluateBCs(const panzer::BCType bc_type,
	    const panzer::AssemblyEngineInArgs& in,
            const Teuchos::RCP<LinearObjContainer> preEval_loc)
{

  {
    const std::map<panzer::BC, 
      std::map<unsigned,PHX::FieldManager<panzer::Traits> >,
      panzer::LessBC>& bc_field_managers = 
      m_field_manager_builder->getBCFieldManagers();
  
    const std::map<panzer::BC,
      Teuchos::RCP<std::map<unsigned,panzer::Workset> >,
      panzer::LessBC>& bc_worksets = 
      m_field_manager_builder->getBCWorksets();

    // Must do all neumann before all dirichlet so we need a double loop
    // here over all bcs
    typedef typename std::map<panzer::BC, 
      std::map<unsigned,PHX::FieldManager<panzer::Traits> >,
      panzer::LessBC>::const_iterator bcfm_it_type;

    typedef typename std::map<panzer::BC,
      Teuchos::RCP<std::map<unsigned,panzer::Workset> >,
      panzer::LessBC>::const_iterator bcwkst_it_type;

    // loop over bcs
    for (bcfm_it_type bcfm_it = bc_field_managers.begin(); 
	 bcfm_it != bc_field_managers.end(); ++bcfm_it) {
      
      const panzer::BC& bc = bcfm_it->first;
      const std::map<unsigned,PHX::FieldManager<panzer::Traits> > bc_fm = 
	bcfm_it->second;

      bcwkst_it_type bc_wkst_it = bc_worksets.find(bc);

      TEUCHOS_TEST_FOR_EXCEPTION(bc_wkst_it == bc_worksets.end(), std::logic_error,
			 "Failed to find corresponding bc workset!");

      const std::map<unsigned,panzer::Workset>& bc_wkst = 
	*(bc_wkst_it->second);  

      // Only process bcs of the appropriate type (neumann or dirichlet)
      if (bc.bcType() == bc_type) {

	// Loop over local faces
	for (std::map<unsigned,PHX::FieldManager<panzer::Traits> >::const_iterator side = bc_fm.begin(); side != bc_fm.end(); ++side) {

	  // extract field manager for this side  
	  unsigned local_side_index = side->first;
	  PHX::FieldManager<panzer::Traits>& local_side_fm = 
	    const_cast<PHX::FieldManager<panzer::Traits>& >(side->second);
	  
          // extract workset for this side: only one workset per face
	  std::map<unsigned,panzer::Workset>::const_iterator wkst_it = 
	    bc_wkst.find(local_side_index);
	  
	  TEUCHOS_TEST_FOR_EXCEPTION(wkst_it == bc_wkst.end(), std::logic_error,
			     "Failed to find corresponding bc workset side!");
	  
	  panzer::Workset& workset = 
	    const_cast<panzer::Workset&>(wkst_it->second); 

          // run prevaluate
          Traits::PED preEvalData;
          preEvalData.dirichletData.ghostedCounter = preEval_loc;

          local_side_fm.template preEvaluate<EvalT>(preEvalData);

          // build and evaluate fields for the workset: only one workset per face
          workset.ghostedLinContainer = in.ghostedContainer_;
          workset.linContainer = in.container_;
	  workset.alpha = in.alpha;
	  workset.beta = in.beta;
	  workset.time = in.time;
	  
	  local_side_fm.template evaluateFields<EvalT>(workset);

          // run postevaluate for consistency
	  local_side_fm.template postEvaluate<EvalT>(NULL);
	  
	}
      }
    } 
  }

}
