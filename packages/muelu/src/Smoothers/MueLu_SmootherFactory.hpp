#ifndef MUELU_SMOOTHERFACTORY_HPP
#define MUELU_SMOOTHERFACTORY_HPP

#include <iostream>

#include "MueLu_BaseFactory.hpp"
#include "MueLu_Smoother.hpp"
#include "MueLu_Level.hpp"

/*!
  @class Smoother factory base class.
  @brief Base class for smoother factories.
*/

namespace MueLu {

template <class Scalar,class LO,class GO,class Node>
class SmootherFactory : public BaseFactory {

  typedef MueLu::Smoother<Scalar,LO,GO,Node> Smoother;

  private:
    int outputLevel_;
    int priorOutputLevel_;

  public:
    //@{ Constructors/Destructors.
    SmootherFactory() {*(this->out_) << "Instantiating a new SmootherFactory" << std::endl;}

    virtual ~SmootherFactory() {}
    //@}

    //@{
    //! @name Build methods.

    //! Build pre-smoother and/or post-smoother
    bool Build(Level<Scalar,LO,GO,Node> &level /*,Teuchos::ParameterList Specs*/) {
      *(this->out_) << "Building pre-smoother and/or post-smoother" << std::endl;
      return true;
    }

    //@}

/*
//FIXME The needs mechanism hasn't been decided upon.
    void AddNeeds(Teuchos::ParameterList const &newNeeds) {
      CrossFactory.MergeNeeds(newNeeds,Needs_);
    }

    Teuchos::ParameterList& GetNeeds(Teuchos::ParameterList &newNeeds) {
      return Needs_;
    }
*/

    void SetOutputLevel(int outputLevel) {
      outputLevel_ = outputLevel;
    }

    int GetOutputLevel() {
      return outputLevel_;
    }

}; //class SmootherFactory

} //namespace MueLu

#endif //ifndef MUELU_SMOOTHERFACTORY_HPP
