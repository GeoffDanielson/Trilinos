
#
# A) Define the package
#

TRIBITS_PACKAGE(Amesos2)

SET(${PACKAGE_NAME_UC}_VERSION "0.3d")
SET(${PACKAGE_NAME_UC}_RELEASE_DATE "07/28/2011")

#
# B) Set up package-specific options
#

IF (${PACKAGE_NAME}_ENABLE_MUMPS AND NOT Tpetra_INST_INT_INT)
   MESSAGE(WARNING "*****Amesos2 will not provide MUMPS Support with Tpetra_INST_INT_INT OFF.*****")
ENDIF ()

IF (Amesos2_ENABLE_UMFPACK)
  IF (Trilinos_ENABLE_COMPLEX_FLOAT AND NOT Trilinos_ENABLE_COMPLEX_DOUBLE)
    MESSAGE(WARNING "*****Amesos2 is building Umfpack without a complex<double> type. Umfpack natively supports only complex<double> types, not complex<float> types, and this will lead to type conversions. *****")
  ENDIF()
  # This may not happen anyways… right now it seems we can’t compile this package with Trilinos_ENABLE_FLOAT and TPetra_INST_DOUBLE turned off - need to resolve.
  IF (NOT Tpetra_INST_DOUBLE)
    MESSAGE(WARNING "*****Amesos2 is building Umfpack without a double type. Umfpack natively supports only double types, not float types, and this will lead to type conversions. *****")
  ENDIF()
ENDIF()


# if using SuperLU_5.0, must use the proper API
IF(${PACKAGE_NAME}_ENABLE_SuperLU)
  IF(NOT ${PROJECT_NAME}_ENABLE_SuperLU5_API)

   SET(HAVE_AMESOS2_SUPERLU5_API OFF)

  ELSE()

   SET(HAVE_AMESOS2_SUPERLU5_API ON)

  ENDIF()
ENDIF()


TRIBITS_ADD_OPTION_AND_DEFINE(${PACKAGE_NAME}_ENABLE_VERBOSE_DEBUG
  HAVE_${PACKAGE_NAME_UC}_VERBOSE_DEBUG
  "Enable verbose output during debug-mode"
  OFF )

TRIBITS_ADD_OPTION_AND_DEFINE(${PACKAGE_NAME}_ENABLE_TIMERS
  HAVE_${PACKAGE_NAME_UC}_TIMERS
  "Enable performance timers"
  OFF )

#
# TPL Solver interfaces are enabled automatically based on Trilinos
# TPL configuration.
#
# Config options such as Amesos2_ENABLE_XXXXXX are automatically
# generated in case a user would wish to explicitely disable Amesos2
# support for an otherwise enabled third-part solver library
#


IF(${PACKAGE_NAME}_ENABLE_Cholmod)
  SET(HAVE_AMESOS2_CHOLMOD ON )
ENDIF()


TRIBITS_ADD_OPTION_AND_DEFINE(${PACKAGE_NAME}_ENABLE_LAPACK
  HAVE_${PACKAGE_NAME_UC}_LAPACK
  "Enable the dense serial solver interface"
  OFF )

TRIBITS_ADD_OPTION_AND_DEFINE(${PACKAGE_NAME}_ENABLE_KLU2
  HAVE_AMESOS2_KLU2
  "Enable KLU2 in Amesos2"
  ON
)

IF (Trilinos_ENABLE_ShyLU_NodeTacho AND NOT Kokkos_ENABLE_CUDA)
  SET(Amesos2_HAVE_TACHO ON)
ELSE()
  SET(Amesos2_HAVE_TACHO OFF)
ENDIF()

TRIBITS_ADD_OPTION_AND_DEFINE(${PACKAGE_NAME}_ENABLE_ShyLU_NodeTacho
  HAVE_AMESOS2_TACHO
  "Enable Tacho in Amesos2"
  ${Amesos2_HAVE_TACHO}
)

TRIBITS_ADD_OPTION_AND_DEFINE(${PACKAGE_NAME}_ENABLE_Basker
   HAVE_AMESOS2_BASKER
   "Enable Basker in Amesos2"
   OFF
)

IF (Trilinos_ENABLE_ShyLU_NodeBasker AND Kokkos_ENABLE_OPENMP AND NOT Kokkos_ENABLE_CUDA)
  SET(Amesos2_HAVE_SHYLUBASKER ON)
ELSE()
  SET(Amesos2_HAVE_SHYLUBASKER OFF)
ENDIF()

TRIBITS_ADD_OPTION_AND_DEFINE(${PACKAGE_NAME}_ENABLE_ShyLU_NodeBasker
  HAVE_AMESOS2_SHYLUBASKER
  "Enable ShyLUBasker in Amesos2"
  ${Amesos2_HAVE_SHYLUBASKER}
)

TRIBITS_ADD_OPTION_AND_DEFINE(${PACKAGE_NAME}_ENABLE_MUMPS
  HAVE_AMESOS2_MUMPS
  "Enable MUMPS in Amesos2"
  OFF
)

TRIBITS_ADD_OPTION_AND_DEFINE(${PACKAGE_NAME}_ENABLE_TRIANGULAR_SOLVES
  HAVE_AMESOS2_TRIANGULAR_SOLVES
  "Enable Triangular Solves in Amesos2"
  OFF
)

# If Triangular Solves is on then we must have proper settings for KokkosKernels
ASSERT_DEFINED(${PACKAGE_NAME}_ENABLE_TRIANGULAR_SOLVES)
IF(${PACKAGE_NAME}_ENABLE_TRIANGULAR_SOLVES)
  ASSERT_DEFINED(TPL_ENABLE_Cholmod)
  ASSERT_DEFINED(${PACKAGE_NAME}_ENABLE_Cholmod)
  IF(TPL_ENABLE_Cholmod AND ${PACKAGE_NAME}_ENABLE_Cholmod)
    ASSERT_DEFINED(KokkosKernels_ENABLE_SUPERNODAL_SPTRSV)
    ASSERT_DEFINED(KokkosKernels_ENABLE_TPL_CHOLMOD)
    IF(NOT KokkosKernels_ENABLE_SUPERNODAL_SPTRSV OR NOT KokkosKernels_ENABLE_TPL_CHOLMOD)
      MESSAGE(FATAL_ERROR "You must enable -DKokkosKernels_ENABLE_SUPERNODAL_SPTRSV=ON and -DKokkosKernels_ENABLE_TPL_CHOLMOD=ON for Triangular Solves with Cholmod")
    ENDIF()
  ENDIF()
ENDIF()

ASSERT_DEFINED(TPL_ENABLE_METIS)
TRIBITS_ADD_OPTION_AND_DEFINE(${PACKAGE_NAME}_ENABLE_METIS
  HAVE_AMESOS2_METIS
  "Enable METIS in Amesos2"
  "${TPL_ENABLE_METIS}"
  )
ASSERT_DEFINED(${PACKAGE_NAME}_ENABLE_METIS)

ASSERT_DEFINED(TPL_ENABLE_CUSPARSE)
TRIBITS_ADD_OPTION_AND_DEFINE(${PACKAGE_NAME}_ENABLE_CUSPARSE
  HAVE_AMESOS2_CUSPARSE
  "Enable cuSPARSE in Amesos2"
  "${TPL_ENABLE_CUSPARSE}"
  )
ASSERT_DEFINED(${PACKAGE_NAME}_ENABLE_CUSPARSE)

ASSERT_DEFINED(TPL_ENABLE_CUSOLVER)
TRIBITS_ADD_OPTION_AND_DEFINE(${PACKAGE_NAME}_ENABLE_CUSOLVER
  HAVE_AMESOS2_CUSOLVER
  "Enable cuSOLVER in Amesos2"
  "${TPL_ENABLE_CUSOLVER}"
  )
ASSERT_DEFINED(${PACKAGE_NAME}_ENABLE_CUSOLVER)

TRIBITS_ADD_OPTION_AND_DEFINE(${PACKAGE_NAME}_ENABLE_Experimental
  HAVE_AMESOS2_EXPERIMENTAL
  "Enable building and installation of experimental Amesos2 features."
  NO 
)

TRIBITS_ADD_DEBUG_OPTION()

TRIBITS_ADD_EXPLICIT_INSTANTIATION_OPTION()

#
# C) Add the libraries, tests, and examples
#

ADD_SUBDIRECTORY(src)

TRIBITS_ADD_TEST_DIRECTORIES(test)

TRIBITS_ADD_EXAMPLE_DIRECTORIES(example)

#
# D) Do standard postprocessing
#

#leaving in place for future excludes
#TRIBITS_EXCLUDE_FILES(
#  )

TRIBITS_PACKAGE_POSTPROCESS()
