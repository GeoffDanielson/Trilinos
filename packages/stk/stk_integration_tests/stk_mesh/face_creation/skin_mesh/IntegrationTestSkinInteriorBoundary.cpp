/*--------------------------------------------------------------------*/
/*    Copyright 2009 Sandia Corporation.                              */
/*    Under the terms of Contract DE-AC04-94AL85000, there is a       */
/*    non-exclusive license for use of this work by or on behalf      */
/*    of the U.S. Government.  Export of this program may require     */
/*    a license from the United States Government.                    */
/*--------------------------------------------------------------------*/

#include <gtest/gtest.h>                // for AssertHelper, EXPECT_EQ, etc
#include <Ioss_IOFactory.h>             // for IOFactory
#include <Ioss_Region.h>                // for Region
#include <init/Ionit_Initializer.h>     // for Initializer
#include <stddef.h>                     // for size_t, nullptr
#include <stk_io/StkMeshIoBroker.hpp>   // for StkMeshIoBroker
#include <string>                       // for string
#include "mpi.h"                        // for MPI_COMM_WORLD
#include "stk_io/DatabasePurpose.hpp"
#include <stk_mesh/base/GetEntities.hpp>
#include <stk_mesh/base/Comm.hpp>
#include <stk_mesh/base/SkinBoundary.hpp>
#include <stk_unit_test_utils/ioUtils.hpp>
#include <stk_util/parallel/ParallelReduce.hpp>
#include <stk_unit_test_utils/MeshFixture.hpp>  // for MeshTestFixture
#include "../FaceCreationTestUtils.hpp"

namespace
{

const SideTestUtil::TestCaseData interiorBlockBoundaryTestCases =
{
  /* filename, max#procs, #side,   sideset */
    {"AB.e",      2,        1,    {{1, 5}, {2, 4}}},
    {"Ae.e",      2,        1,    {{1, 5}, {2, 1}}},
    {"Aef.e",     3,        1,    {{1, 5}, {2, 1}, {3, 1}}},
    {"AeB.e",     3,        2,    {{1, 5}, {2, 0}, {2, 1}, {3, 4}}},
    {"AefB.e",    4,        2,    {{1, 5}, {3, 0}, {3, 1}, {4, 0}, {4, 1}, {2, 4}}},
    {"ef.e",      2,        0,    {}},

    {"AB_doubleKissing.e", 2, 2,  {{1, 1}, {1, 2}, {2, 0}, {2, 3}}},

    {"Tg.e",      2,        0,    {}},
    {"ZY.e",      2,        1,    {{1, 5}, {2, 4}}}
};

class InteriorBlockBoundaryTester : public SideTestUtil::SideCreationTester
{
public:
    InteriorBlockBoundaryTester() : SideTestUtil::SideCreationTester(MPI_COMM_WORLD) {}
protected:
    virtual void test_side_creation(stk::mesh::BulkData& bulkData,
                                    const SideTestUtil::TestCase& testCase)
    {
        stk::mesh::Part& skinnedPart = bulkData.mesh_meta_data().declare_part("interior");
        stk::mesh::create_interior_block_boundary_sides(bulkData, bulkData.mesh_meta_data().universal_part(), skinnedPart);
        expect_interior_sides_connected_as_specified_in_test_case(bulkData, testCase, skinnedPart);
    }

    void expect_interior_sides_connected_as_specified_in_test_case(const stk::mesh::BulkData& bulkData,
                                                                   const SideTestUtil::TestCase& testCase,
                                                                   const stk::mesh::Part &skinnedPart)
    {
        SideTestUtil::expect_global_num_sides_in_part(bulkData, testCase, skinnedPart);
        SideTestUtil::expect_all_sides_exist_for_elem_side(bulkData, testCase.filename, testCase.sideSet);
    }
};

TEST(InteriorBlockBoundaryTest, DISABLED_run_all_test_cases_aura)
{
    InteriorBlockBoundaryTester().run_all_test_cases(interiorBlockBoundaryTestCases, stk::mesh::BulkData::AUTO_AURA);
}

TEST(InteriorBlockBoundaryTest, DISABLED_run_all_test_cases_no_aura)
{
    InteriorBlockBoundaryTester().run_all_test_cases(interiorBlockBoundaryTestCases, stk::mesh::BulkData::NO_AUTO_AURA);
}

}
