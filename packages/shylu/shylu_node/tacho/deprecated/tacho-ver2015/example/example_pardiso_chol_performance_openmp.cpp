#include <Kokkos_Core.hpp>

#include <Kokkos_OpenMP.hpp>

#include "Teuchos_CommandLineProcessor.hpp"

using namespace std;

typedef double value_type;
typedef int    ordinal_type;
typedef int    size_type;

typedef Kokkos::OpenMP exec_space;

#include "example_pardiso_chol_performance.hpp"

using namespace Tacho;

int main (int argc, char *argv[]) {

  Teuchos::CommandLineProcessor clp;
  clp.setDocString("This example program measure the performance of Pardiso Chol algorithms on Kokkos::OpenMP execution space.\n");

  int nthreads = 0;
  clp.setOption("nthreads", &nthreads, "Number of threads");

  int numa = 0;
  clp.setOption("numa", &numa, "Number of numa node");

  int core_per_numa = 0;
  clp.setOption("core-per-numa", &core_per_numa, "Number of cores per numa node");

  bool verbose = false;
  clp.setOption("enable-verbose", "disable-verbose", &verbose, "Flag for verbose printing");

  string file_input = "test.mtx";
  clp.setOption("file-input", &file_input, "Input file (MatrixMarket SPD matrix)");

  int nrhs = 1;
  clp.setOption("nrhs", &nrhs, "Number of RHS vectors");

  clp.recogniseAllOptions(true);
  clp.throwExceptions(false);

  Teuchos::CommandLineProcessor::EParseCommandLineReturn r_parse= clp.parse( argc, argv );

  if (r_parse == Teuchos::CommandLineProcessor::PARSE_HELP_PRINTED) return 0;
  if (r_parse != Teuchos::CommandLineProcessor::PARSE_SUCCESSFUL  ) return -1;

  int r_val = 0;
  {
    exec_space::initialize(nthreads, numa, core_per_numa);
    exec_space::print_configuration(cout, true);

#ifdef HAVE_SHYLU_NODETACHO_MKL
    const bool skip_factorize = false;
    const bool skip_solve     = false;
    r_val = examplePardisoCholPerformance
      <value_type,ordinal_type,size_type,exec_space,void>
      (file_input,
       nrhs, 
       nthreads,
       skip_factorize,
       skip_solve,
       verbose);
#else
    r_val = -1;
    cout << "MKL is NOT configured in Trilinos" << endl;
#endif

    exec_space::finalize();
  }

  return r_val;
}
