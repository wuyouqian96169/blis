/*

   BLIS    
   An object-based framework for developing high-performance BLAS-like
   libraries.

   Copyright (C) 2013, The University of Texas

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:
    - Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    - Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    - Neither the name of The University of Texas nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include "blis2.h"
#include "test_libblis.h"


// Static variables.
static char*     op_str                    = "fnormm";
static char*     o_types                   = "m";  // x
static char*     p_types                   = "";   // (no parameters)
static thresh_t  thresh[BLIS_NUM_FP_TYPES] = { { 1e-04, 1e-05 },   // warn, pass for s
                                               { 1e-04, 1e-05 },   // warn, pass for c
                                               { 1e-13, 1e-14 },   // warn, pass for d
                                               { 1e-13, 1e-14 } }; // warn, pass for z

// Local prototypes.
void libblis_test_fnormm_deps( test_params_t* params,
                               test_op_t*     op );

void libblis_test_fnormm_experiment( test_params_t* params,
                                     test_op_t*     op,
                                     mt_impl_t      impl,
                                     num_t          datatype,
                                     char*          pc_str,
                                     char*          sc_str,
                                     dim_t          p_cur,
                                     double*        perf,
                                     double*        resid );

void libblis_test_fnormm_impl( mt_impl_t impl,
                               obj_t*    x,
                               obj_t*    norm );

void libblis_test_fnormm_check( obj_t*  beta,
                                obj_t*  x,
                                obj_t*  norm,
                                double* resid );



void libblis_test_fnormm_deps( test_params_t* params, test_op_t* op )
{
	libblis_test_setm( params, &(op->ops->setm) );
}



void libblis_test_fnormm( test_params_t* params, test_op_t* op )
{

	// Return early if this test has already been done.
	if ( op->test_done == TRUE ) return;

	// Call dependencies first.
	if ( TRUE ) libblis_test_fnormm_deps( params, op );

	// Execute the test driver for each implementation requested.
	if ( op->front_seq == ENABLE )
	{
		libblis_test_op_driver( params,
		                        op,
		                        BLIS_TEST_SEQ_FRONT_END,
		                        op_str,
		                        p_types,
		                        o_types,
		                        thresh,
		                        libblis_test_fnormm_experiment );
	}
}



void libblis_test_fnormm_experiment( test_params_t* params,
                                     test_op_t*     op,
                                     mt_impl_t      impl,
                                     num_t          datatype,
                                     char*          pc_str,
                                     char*          sc_str,
                                     dim_t          p_cur,
                                     double*        perf,
                                     double*        resid )
{
	unsigned int n_repeats = params->n_repeats;
	unsigned int i;

	num_t        dt_real   = bl2_datatype_proj_to_real( datatype );

	double       time_min  = 1e9;
	double       time;

	dim_t        m, n;

	obj_t        beta, norm;
	obj_t        x;


	// Map the dimension specifier to actual dimensions.
	m = libblis_test_get_dim_from_prob_size( op->dim_spec[0], p_cur );
	n = libblis_test_get_dim_from_prob_size( op->dim_spec[1], p_cur );

	// Map parameter characters to BLIS constants.


	// Create test scalars.
	bl2_obj_init_scalar( datatype, &beta );
	bl2_obj_init_scalar( dt_real,  &norm );

	// Create test operands (vectors and/or matrices).
	libblis_test_mobj_create( params, datatype, BLIS_NO_TRANSPOSE,
	                          sc_str[0], m, n, &x );

	// Initialize beta to 2 - 2i.
	bl2_setsc( 2.0, -2.0, &beta );

	// Set all elements of x to beta.
	bl2_setm( &beta, &x );

	// Repeat the experiment n_repeats times and record results. 
	for ( i = 0; i < n_repeats; ++i )
	{
		time = bl2_clock();

		libblis_test_fnormm_impl( impl, &x, &norm );

		time_min = bl2_clock_min_diff( time_min, time );
	}

	// Estimate the performance of the best experiment repeat.
	*perf = ( 2.0 * m * n ) / time_min / FLOPS_PER_UNIT_PERF;
	if ( bl2_obj_is_complex( x ) ) *perf *= 2.0;

	// Perform checks.
	libblis_test_fnormm_check( &beta, &x, &norm, resid );

	// Free the test objects.
	bl2_obj_free( &x );
}



void libblis_test_fnormm_impl( mt_impl_t impl,
                               obj_t*    x,
                               obj_t*    norm )
{
	switch ( impl )
	{
		case BLIS_TEST_SEQ_FRONT_END:
		bl2_fnormm( x, norm );
		break;

		default:
		libblis_test_printf_error( "Invalid implementation type.\n" );
	}
}



void libblis_test_fnormm_check( obj_t*  beta,
                                obj_t*  x,
                                obj_t*  norm,
                                double* resid )
{
	num_t  dt_real = bl2_obj_datatype_proj_to_real( *x );
	dim_t  m       = bl2_obj_length( *x );
	dim_t  n       = bl2_obj_width( *x );

	obj_t  m_r, n_r, temp_r;

	double junk;

	//
	// Pre-conditions:
	// - x is set to beta.
	// Note:
	// - beta should have a non-zero imaginary component in the complex
	//   cases in order to more fully exercise the implementation.
	//
	// Under these conditions, we assume that the implementation for
	//
	//   norm := fnorm( x )
	//
	// is functioning correctly if
	//
	//   norm = sqrt( absqsc( beta ) * m * n )
	//
	// where m and n are the dimensions of x.
	//

	bl2_obj_init_scalar( dt_real, &temp_r );
	bl2_obj_init_scalar( dt_real, &m_r );
	bl2_obj_init_scalar( dt_real, &n_r );

	bl2_setsc( ( double )m, 0.0, &m_r );
	bl2_setsc( ( double )n, 0.0, &n_r );

	bl2_absqsc( beta, &temp_r );
	bl2_mulsc( &m_r, &temp_r );
	bl2_mulsc( &n_r, &temp_r );
	bl2_sqrtsc( &temp_r, &temp_r );
	bl2_subsc( &temp_r, norm );

	bl2_getsc( norm, resid, &junk );
}
