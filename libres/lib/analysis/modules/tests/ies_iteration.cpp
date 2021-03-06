#include <ert/util/test_util.hpp>
#include <ert/util/rng.h>

#include <ert/res_util/es_testdata.hpp>

#include <ert/analysis/std_enkf.hpp>
#include "ies_enkf_data.h"
#include "ies_enkf.h"


void init_stdA(const res::es_testdata& testdata, matrix_type * A2) {
  rng_type * rng = rng_alloc( MZRAN, INIT_DEFAULT );
  std_enkf_data_type * std_data = static_cast<std_enkf_data_type*>(std_enkf_data_alloc());
  std_enkf_set_truncation(std_data, 1.00);
  matrix_type * X = matrix_alloc(testdata.active_ens_size, testdata.active_ens_size);

  std_enkf_initX(std_data,
                 X,
                 nullptr,
                 testdata.S,
                 testdata.R,
                 testdata.dObs,
                 testdata.E,
                 testdata.D,
                 rng);

  matrix_inplace_matmul(A2, X);

  std_enkf_data_free(std_data);
  rng_free(rng);
}


/*
  This function will run the forward model again and update the matrices in the
  testdata structure - with A1 as prior.
*/

void forward_model(res::es_testdata& testdata, const matrix_type *A1) {
  int nrens = matrix_get_columns( A1 );
  int ndim  = matrix_get_rows( A1 );
  int nrobs = matrix_get_rows( testdata.S );

  /* Model prediction gives new S given prior S=func(A) */
  for (int iens=0; iens< nrens; iens++){
    for (int i=0; i < nrobs; i++){
      double coeffa = matrix_iget(A1,0,iens) ;
      double coeffb = matrix_iget(A1,1,iens) ;
      double coeffc = matrix_iget(A1,2,iens) ;
      double y = coeffa*i*i + coeffb*i + coeffc ;
      matrix_iset(testdata.S,i,iens,y) ;
    }
  }

  /* Updating D according to new S: D=dObs+E-S*/
  for (int i=0;i<nrens;i++)
    matrix_copy_column(testdata.D , testdata.dObs, i , 0) ;

  matrix_inplace_add(testdata.D, testdata.E);
  matrix_inplace_sub(testdata.D, testdata.S);
}


void cmp_std_ies(res::es_testdata& testdata) {
  int num_iter = 100;
  bool verbose = false;
  rng_type * rng = rng_alloc( MZRAN, INIT_DEFAULT );
  matrix_type * A1 = testdata.alloc_state("prior");
  matrix_type * A2 = testdata.alloc_state("prior");
  ies_enkf_data_type * ies_data = static_cast<ies_enkf_data_type*>(ies_enkf_data_alloc(rng));
  ies_enkf_config_type * ies_config = ies_enkf_data_get_config(ies_data);

  forward_model(testdata, A1);
  ies_enkf_config_set_truncation(ies_config, 1.0);
  ies_enkf_config_set_ies_max_steplength(ies_config, 0.6);
  ies_enkf_config_set_ies_min_steplength(ies_config, 0.6);
  ies_enkf_config_set_ies_inversion(ies_config, IES_INVERSION_EXACT);
  ies_enkf_config_set_ies_aaprojection(ies_config, false);

/* ES solution */
  
  init_stdA(testdata, A2);


  for (int iter=0; iter < num_iter; iter++) {
    forward_model(testdata, A1);

    ies_enkf_init_update(ies_data,
                         testdata.ens_mask,
                         testdata.obs_mask,
                         testdata.S,
                         testdata.R,
                         testdata.dObs,
                         testdata.E,
                         testdata.D,
                         rng);

    ies_enkf_updateA(ies_data,
                     A1,
                     testdata.S,
                     testdata.R,
                     testdata.dObs,
                     testdata.E,
                     testdata.D,
                     NULL,
                     rng);


    if (verbose) {
      fprintf(stdout,"IES iteration   = %d %d\n", iter, bool_vector_count_equal(testdata.ens_mask, true));
      matrix_pretty_fprint(A1,"Aies","%11.5f",stdout);
      matrix_pretty_fprint(A2,"Astdenkf","%11.5f",stdout);
    }
    test_assert_int_equal( ies_enkf_data_get_iteration_nr(ies_data), iter + 1);

    if ( matrix_similar(A1, A2, 1e-5)) break;
  }

  test_assert_true( matrix_similar(A1, A2, 1e-5));

  matrix_free(A1);
  matrix_free(A2);
  ies_enkf_data_free(ies_data);
  rng_free( rng );
}


void cmp_std_ies_delrel(res::es_testdata& testdata) {
  int num_iter = 100;
  bool verbose = true;
  rng_type * rng = rng_alloc( MZRAN, INIT_DEFAULT );
  matrix_type * A1 = testdata.alloc_state("prior");
  matrix_type * A2 = testdata.alloc_state("prior");
  matrix_type * A1c = matrix_alloc_copy(A1);
  matrix_type * A2c = matrix_alloc_copy(A2);
  ies_enkf_data_type * ies_data = static_cast<ies_enkf_data_type*>(ies_enkf_data_alloc(rng));
  ies_enkf_config_type * ies_config = ies_enkf_data_get_config(ies_data);

  forward_model(testdata, A1);
  ies_enkf_config_set_truncation(ies_config, 1.0);
  ies_enkf_config_set_ies_min_steplength(ies_config, 0.6);
  ies_enkf_config_set_ies_max_steplength(ies_config, 0.6);
  ies_enkf_config_set_ies_inversion(ies_config, IES_INVERSION_EXACT);
  ies_enkf_config_set_ies_aaprojection(ies_config, false);
  int iens_deact = testdata.active_ens_size / 2;

  if (verbose) {
    fprintf(stdout,"ES and IES original priors\n");
    matrix_pretty_fprint(A1,"A1  ","%11.5f",stdout);
    matrix_pretty_fprint(A2,"A2  ","%11.5f",stdout);
  }

/* IES solution after with one realization is inactivated */
   for (int iter=0; iter < num_iter; iter++) {
      forward_model(testdata, A1);
    
// Removing the realization
      if (iter == 6) {
         testdata.deactivate_realization( iens_deact );
         A1c = matrix_alloc( matrix_get_rows(A1), bool_vector_count_equal(testdata.ens_mask, true));
         int iens_active = 0;
         for (int iens=0; iens < matrix_get_columns(A1); iens++) {
            if (bool_vector_iget(testdata.ens_mask, iens)) {
               matrix_copy_column(A1c, A1, iens_active, iens);
               iens_active += 1;
            }
         }
         matrix_realloc_copy(A1,A1c);
      }

      ies_enkf_init_update(ies_data,
                           testdata.ens_mask,
                           testdata.obs_mask,
                           testdata.S,
                           testdata.R,
                           testdata.dObs,
                           testdata.E,
                           testdata.D,
                           rng);

      ies_enkf_updateA(ies_data,
                       A1,
                       testdata.S,
                       testdata.R,
                       testdata.dObs,
                       testdata.E,
                       testdata.D,
                       NULL,
                       rng);


      if (verbose) {
         fprintf(stdout,"IES iteration = %d active realizations= %d\n", iter, bool_vector_count_equal(testdata.ens_mask, true));
         matrix_pretty_fprint(A1,"Aies","%11.5f",stdout);
      }
   }
   fprintf(stdout,"IES solution with %d active realizations\n", bool_vector_count_equal(testdata.ens_mask, true));
   matrix_pretty_fprint(A1,"A1  ","%11.5f",stdout);


/* ES update with one realization removed*/
   {
      A2c = matrix_alloc( matrix_get_rows(A2), bool_vector_count_equal(testdata.ens_mask, true));
      int iens_active = 0;
      for (int iens=0; iens < matrix_get_columns(A2); iens++) {
         if (bool_vector_iget(testdata.ens_mask, iens)) {
            matrix_copy_column(A2c, A2, iens_active, iens);
            iens_active += 1;
         }
      }
      matrix_realloc_copy(A2,A2c);
   }
   forward_model(testdata, A2);

   if (verbose) {
      fprintf(stdout,"\n\n\nES prior with one realization removed\n");
      matrix_pretty_fprint(A2,"A2  ","%11.5f",stdout);
   }

   init_stdA(testdata, A2);

   if (verbose) {
      fprintf(stdout,"ES solution with one realization removed\n");
      matrix_pretty_fprint(A2,"A2  ","%11.5f",stdout);
   }

   test_assert_true( matrix_similar(A1, A2, 1e-5));

   matrix_free(A1c);
   matrix_free(A2c);
   matrix_free(A1);
   matrix_free(A2);
   ies_enkf_data_free(ies_data);
   rng_free( rng );
}

matrix_type * swap_matrix(matrix_type * old_matrix, matrix_type * new_matrix) {
  matrix_free( old_matrix );
  return new_matrix;
}

/*
  This test verifies that the update iteration do not crash hard when
  realizations and observations are deactived between iterations.
  The function is testing reactivation as well. It is a bit tricky since there is no
  reactivation function. What is done is to start with identical copies, testdata and
  testdata2. In the first iteration, one observation is removed in testdata2 and before
  computing the update. In the subsequent iterations, testdata is used which includes
  the datapoint initially removed from testdata2. 
*/

void test_deactivate_observations_and_realizations(const char * testdata_file) {
  res::es_testdata testdata(testdata_file);
  res::es_testdata testdata2(testdata_file);
  int num_iter = 10;
  rng_type * rng = rng_alloc( MZRAN, INIT_DEFAULT );

  ies_enkf_data_type * ies_data = static_cast<ies_enkf_data_type*>(ies_enkf_data_alloc(rng));
  ies_enkf_config_type * ies_config = ies_enkf_data_get_config(ies_data);

  matrix_type * A0 = testdata.alloc_state("prior");
  matrix_type * A = matrix_alloc_copy(A0);


  ies_enkf_config_set_truncation(ies_config, 1.00);
  ies_enkf_config_set_ies_max_steplength(ies_config, 0.50);
  ies_enkf_config_set_ies_min_steplength(ies_config, 0.50);
  ies_enkf_config_set_ies_inversion(ies_config, IES_INVERSION_SUBSPACE_EXACT_R);
  ies_enkf_config_set_ies_aaprojection(ies_config, false);

  for (int iter=0; iter < 1; iter++) {
    printf("test_deactivate_observations_and_realizations: iter= %d\n",iter);

    // deactivate an observation initially to test reactivation in the following iteration
    testdata2.deactivate_obs( 2 );

    ies_enkf_init_update(ies_data,
                         testdata2.ens_mask,
                         testdata2.obs_mask,
                         testdata2.S,
                         testdata2.R,
                         testdata2.dObs,
                         testdata2.E,
                         testdata2.D,
                         rng);

    ies_enkf_updateA(ies_data,
                     A,
                     testdata2.S,
                     testdata2.R,
                     testdata2.dObs,
                     testdata2.E,
                     testdata2.D,
                     NULL,
                     rng);
  }
  
  for (int iter=1; iter < num_iter; iter++) {
    printf("test_deactivate_observations_and_realizations: iter= %d\n",iter);

    // Deactivate a realization
    if (iter == 3) {
      int iens = testdata.active_ens_size / 2;
      testdata.deactivate_realization( iens );
      A = matrix_alloc( matrix_get_rows(A0), bool_vector_count_equal(testdata.ens_mask, true));
      int iens_active = 0;
      for (int iens=0; iens < matrix_get_columns(A0); iens++) {
        if (bool_vector_iget(testdata.ens_mask, iens)) {
          matrix_copy_column(A, A0, iens_active, iens);
          iens_active += 1;
        }
      }
    }

    // Now deactivate a previously active observation
    if (iter == 7)
      testdata.deactivate_obs( testdata.active_obs_size / 2 );

    ies_enkf_init_update(ies_data,
                         testdata.ens_mask,
                         testdata.obs_mask,
                         testdata.S,
                         testdata.R,
                         testdata.dObs,
                         testdata.E,
                         testdata.D,
                         rng);

    ies_enkf_updateA(ies_data,
                     A,
                     testdata.S,
                     testdata.R,
                     testdata.dObs,
                     testdata.E,
                     testdata.D,
                     NULL,
                     rng);
  }


  matrix_free(A);
  matrix_free(A0);

  ies_enkf_data_free(ies_data);
  rng_free( rng );
}


int main(int argc, char ** argv) {
  res::es_testdata testdata(argv[1]);
  cmp_std_ies(testdata);
  cmp_std_ies_delrel(testdata);
  test_deactivate_observations_and_realizations(argv[1]);
}
