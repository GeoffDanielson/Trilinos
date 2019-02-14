// @HEADER
//
// ***********************************************************************
//
//        MueLu: A package for multigrid based preconditioning
//                  Copyright 2012 Sandia Corporation
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
// Questions? Contact
//                    Jonathan Hu       (jhu@sandia.gov)
//                    Andrey Prokopenko (aprokop@sandia.gov)
//                    Ray Tuminaro      (rstumin@sandia.gov)
//
// ***********************************************************************
//
// @HEADER
#ifndef MUELU_COORDINATESTRANSFER_FACTORY_DEF_HPP
#define MUELU_COORDINATESTRANSFER_FACTORY_DEF_HPP

#include "Xpetra_ImportFactory.hpp"
#include "Xpetra_MultiVectorFactory.hpp"
#include "Xpetra_MapFactory.hpp"
#include "Xpetra_IO.hpp"

#include "MueLu_CoarseMapFactory.hpp"
#include "MueLu_Aggregates.hpp"
#include "MueLu_CoordinatesTransferFactory_decl.hpp"
//#include "MueLu_Utilities.hpp"

#include "MueLu_Level.hpp"
#include "MueLu_Monitor.hpp"

namespace MueLu {

  template <class Scalar, class LocalOrdinal, class GlobalOrdinal, class Node>
  RCP<const ParameterList> CoordinatesTransferFactory<Scalar, LocalOrdinal, GlobalOrdinal, Node>::GetValidParameterList() const {
    RCP<ParameterList> validParamList = rcp(new ParameterList());

    validParamList->set<RCP<const FactoryBase> >("Coordinates",                  Teuchos::null, "Factory for coordinates generation");
    validParamList->set<RCP<const FactoryBase> >("Material Coordinates",         Teuchos::null, "Factory for material coordinates generation");
    validParamList->set<RCP<const FactoryBase> >("Aggregates",                   Teuchos::null, "Factory for coordinates generation");
    validParamList->set<RCP<const FactoryBase> >("CoarseMap",                    Teuchos::null, "Generating factory of the coarse map");
    validParamList->set<bool>                   ("structured aggregation",       false, "Flag specifying that the geometric data is transferred for StructuredAggregationFactory");
    validParamList->set<bool>                   ("aggregation coupled",          false, "Flag specifying if the aggregation algorithm was used in coupled mode.");
    validParamList->set<bool>                   ("Geometric",                    false, "Flag specifying that the coordinates are transferred for GeneralGeometricPFactory");
    validParamList->set<RCP<const FactoryBase> >("coarseCoordinates",            Teuchos::null, "Factory for coarse coordinates generation");
    validParamList->set<RCP<const FactoryBase> >("gCoarseNodesPerDim",           Teuchos::null, "Factory providing the global number of nodes per spatial dimensions of the mesh");
    validParamList->set<RCP<const FactoryBase> >("lCoarseNodesPerDim",           Teuchos::null, "Factory providing the local number of nodes per spatial dimensions of the mesh");
    validParamList->set<RCP<const FactoryBase> >("numDimensions"     ,           Teuchos::null, "Factory providing the number of spatial dimensions of the mesh");
    validParamList->set<int>                    ("write start",                  -1, "first level at which coordinates should be written to file");
    validParamList->set<int>                    ("write end",                    -1, "last level at which coordinates should be written to file");
    validParamList->set<RCP<const FactoryBase> >("aggregationRegionTypeCoarse",  Teuchos::null, "Factory indicating what aggregation type is to be used on the coarse level of the region");
    validParamList->set<bool>                   ("hybrid aggregation",           false, "Flag specifying that hybrid aggregation data is transfered for HybridAggregationFactory");


    return validParamList;
  }

  template <class Scalar, class LocalOrdinal, class GlobalOrdinal, class Node>
  void CoordinatesTransferFactory<Scalar, LocalOrdinal, GlobalOrdinal, Node>::DeclareInput(Level& fineLevel, Level& coarseLevel) const {
    static bool isAvailableCoords = false;

    const ParameterList& pL = GetParameterList();
    if(pL.get<bool>("structured aggregation") == true) {
      if(pL.get<bool>("aggregation coupled") == true) {
        Input(fineLevel, "gCoarseNodesPerDim");
      }
      Input(fineLevel, "lCoarseNodesPerDim");
      Input(fineLevel, "numDimensions");
    } else if(pL.get<bool>("Geometric") == true) {
      Input(coarseLevel, "coarseCoordinates");
      Input(coarseLevel, "gCoarseNodesPerDim");
      Input(coarseLevel, "lCoarseNodesPerDim");
    } else {
      if (coarseLevel.GetRequestMode() == Level::REQUEST)
        isAvailableCoords = coarseLevel.IsAvailable("Coordinates", this);

      if (isAvailableCoords == false) {
        Input(fineLevel, "Coordinates");
        Input(fineLevel, "Aggregates");
        Input(fineLevel, "CoarseMap");
      }
    }
    if(pL.get<bool>("hybrid aggregation") == true) {
      Input(fineLevel,"aggregationRegionTypeCoarse");
      Input(fineLevel, "lCoarseNodesPerDim");
    }
  }

  template <class Scalar, class LocalOrdinal, class GlobalOrdinal, class Node>
  void CoordinatesTransferFactory<Scalar, LocalOrdinal, GlobalOrdinal, Node>::Build(Level & fineLevel, Level &coarseLevel) const {
    FactoryMonitor m(*this, "Build", coarseLevel);

    typedef typename Teuchos::ScalarTraits<Scalar>::coordinateType coordinate_type;
    typedef Xpetra::MultiVector<coordinate_type,LO,GO,NO> CoordinateMultiVector;
    typedef Xpetra::MultiVectorFactory<coordinate_type,LO,GO,NO> CoordinateMultiVectorFactory;

    GetOStream(Runtime0) << "Transferring coordinates" << std::endl;

    int numDimensions;
    RCP<CoordinateMultiVector> coarseCoords;
    RCP<CoordinateMultiVector> fineCoords;
    Array<GO> gCoarseNodesPerDir;
    Array<LO> lCoarseNodesPerDir;

    const ParameterList& pL = GetParameterList();

    if(pL.get<bool>("hybrid aggregation") == true) {
      std::string regionType = Get<std::string>(fineLevel,"aggregationRegionTypeCoarse");
      lCoarseNodesPerDir     = Get<Array<LO> >(fineLevel, "lCoarseNodesPerDim");
      Set<std::string>(coarseLevel, "aggregationRegionType", regionType);
      Set< Array<LO> >(coarseLevel, "lNodesPerDim", lCoarseNodesPerDir);
    }

    if(pL.get<bool>("structured aggregation") == true) {
      if(pL.get<bool>("aggregation coupled") == true) {
        gCoarseNodesPerDir = Get<Array<GO> >(fineLevel, "gCoarseNodesPerDim");
        Set<Array<GO> >(coarseLevel, "gNodesPerDim", gCoarseNodesPerDir);
      }
      lCoarseNodesPerDir = Get<Array<LO> >(fineLevel, "lCoarseNodesPerDim");
      Set<Array<LO> >(coarseLevel, "lNodesPerDim", lCoarseNodesPerDir);
      numDimensions = Get<int>(fineLevel, "numDimensions");
      Set<int>(coarseLevel, "numDimensions", numDimensions);
    } else if(pL.get<bool>("Geometric") == true) {
      coarseCoords       = Get<RCP<CoordinateMultiVector> >(coarseLevel, "coarseCoordinates");
      gCoarseNodesPerDir = Get<Array<GO> >(coarseLevel, "gCoarseNodesPerDim");
      lCoarseNodesPerDir = Get<Array<LO> >(coarseLevel, "lCoarseNodesPerDim");
      Set<Array<GO> >(coarseLevel, "gNodesPerDim", gCoarseNodesPerDir);
      Set<Array<LO> >(coarseLevel, "lNodesPerDim", lCoarseNodesPerDir);

      Set<RCP<CoordinateMultiVector> >(coarseLevel, "Coordinates", coarseCoords);

    } else {
      if (coarseLevel.IsAvailable("Coordinates", this)) {
        GetOStream(Runtime0) << "Reusing coordinates" << std::endl;
        return;
      }

      fineCoords      = Get< RCP<CoordinateMultiVector> >(fineLevel, "Coordinates");
      coarseCoords    = TransferCoords<CoordinateMultiVector,CoordinateMultiVectorFactory>(fineLevel,fineCoords);
      Set<RCP<CoordinateMultiVector> >(coarseLevel, "Coordinates", coarseCoords);
      
      if(fineLevel.IsAvailable("Material Coordinates")) {
        RCP<Vector> fineMatCoords    = Get<RCP<Vector> >(fineLevel,"Material Coordinates");
        RCP<Vector> coarseMatCoords  = TransferCoords<Vector,VectorFactory>(fineLevel,fineMatCoords);
        Set<RCP<Vector> >(coarseLevel, "Material Coordinates", coarseMatCoords);
      }

    } // else

    int writeStart = pL.get<int>("write start"), writeEnd = pL.get<int>("write end");
    if (writeStart == 0 && fineLevel.GetLevelID() == 0 && writeStart <= writeEnd) {
      std::ostringstream buf;
      buf << fineLevel.GetLevelID();
      std::string fileName = "coordinates_before_rebalance_level_" + buf.str() + ".m";
      Xpetra::IO<typename Teuchos::ScalarTraits<Scalar>::coordinateType,LO,GO,NO>::Write(fileName,*fineCoords);
    }
    if (writeStart <= coarseLevel.GetLevelID() && coarseLevel.GetLevelID() <= writeEnd) {
      std::ostringstream buf;
      buf << coarseLevel.GetLevelID();
      std::string fileName = "coordinates_before_rebalance_level_" + buf.str() + ".m";
      Xpetra::IO<typename Teuchos::ScalarTraits<Scalar>::coordinateType,LO,GO,NO>::Write(fileName,*coarseCoords);
    }
  }



template <class Scalar,class LocalOrdinal, class GlobalOrdinal, class Node>
template<class CoordinatesType, class CoordinatesFactoryType>
RCP<CoordinatesType> CoordinatesTransferFactory<Scalar, LocalOrdinal, GlobalOrdinal, Node>::TransferCoords(Level& fineLevel, RCP<CoordinatesType> fineCoords) const{
  typedef typename CoordinatesType::scalar_type coordinate_type;

  RCP<Aggregates>            aggregates    = Get< RCP<Aggregates> >           (fineLevel, "Aggregates");
  RCP<const Map>             coarseMap     = Get< RCP<const Map> >            (fineLevel, "CoarseMap");

  //*** Create the coarse coordinates ***
  // First create the coarse map and coarse multivector
  ArrayView<const GO> elementAList = coarseMap->getNodeElementList();
  LO                  blkSize      = 1;
  if (rcp_dynamic_cast<const StridedMap>(coarseMap) != Teuchos::null)
    blkSize = rcp_dynamic_cast<const StridedMap>(coarseMap)->getFixedBlockSize();
  GO                  indexBase     = coarseMap->getIndexBase();
  size_t              numNodes      = elementAList.size() / blkSize;
  Array<GO>           nodeList(numNodes);
  const int           numDimensions = fineCoords->getNumVectors();
  
  for (LO i = 0; i < Teuchos::as<LO>(numNodes); i++) {
    nodeList[i] = (elementAList[i*blkSize]-indexBase)/blkSize + indexBase;
  }

  RCP<const Map> coarseCoordsMap = MapFactory::Build(fineCoords->getMap()->lib(),
                                                     Teuchos::OrdinalTraits<Xpetra::global_size_t>::invalid(),
                                                     nodeList,
                                                     indexBase,
                                                     fineCoords->getMap()->getComm());
  RCP<CoordinatesType> coarseCoords = CoordinatesFactoryType::Build(coarseCoordsMap, numDimensions);
  
  // Create overlapped fine coordinates to reduce global communication
  RCP<CoordinatesType> ghostedCoords;
  if (aggregates->AggregatesCrossProcessors()) {
    RCP<const Map>    aggMap = aggregates->GetMap();
    // FIXME: We shouldn't have to build this importer from scratch here...
    RCP<const Import> importer     = ImportFactory::Build(fineCoords->getMap(), aggMap);
    
    ghostedCoords = CoordinatesFactoryType::Build(aggMap, numDimensions);
    ghostedCoords->doImport(*fineCoords, *importer, Xpetra::INSERT);
  } else {
    ghostedCoords = fineCoords;
  }
  
  // Get some info about aggregates
  int                         myPID        = coarseCoordsMap->getComm()->getRank();
  LO                          numAggs      = aggregates->GetNumAggregates();
  ArrayRCP<LO>                aggSizes     = aggregates->ComputeAggregateSizes();
  const ArrayRCP<const LO>    vertex2AggID = aggregates->GetVertex2AggId()->getData(0);
  const ArrayRCP<const LO>    procWinner   = aggregates->GetProcWinner()->getData(0);
  
  // Fill in coarse coordinates
  for (int dim = 0; dim < numDimensions; ++dim) {
    ArrayRCP<const coordinate_type> fineCoordsData = ghostedCoords->getData(dim);
    ArrayRCP<coordinate_type>     coarseCoordsData = coarseCoords->getDataNonConst(dim);
    
    for (LO lnode = 0; lnode < Teuchos::as<LO>(numNodes); lnode++) {
      if (procWinner[lnode] == myPID &&
          lnode < vertex2AggID.size() &&
          lnode < fineCoordsData.size() &&
          vertex2AggID[lnode] < coarseCoordsData.size() &&
          Teuchos::ScalarTraits<coordinate_type>::isnaninf(fineCoordsData[lnode]) == false) {
        coarseCoordsData[vertex2AggID[lnode]] += fineCoordsData[lnode];
      }
    }
    for (LO agg = 0; agg < numAggs; agg++) {
      coarseCoordsData[agg] /= aggSizes[agg];
    }
  }

  return coarseCoords;
}

} // namespace MueLu

#endif // MUELU_COORDINATESTRANSFER_FACTORY_DEF_HPP
