/*

   BLIS    
   An object-based framework for developing high-performance BLAS-like
   libraries.

   Copyright (C) 2014, The University of Texas at Austin

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:
    - Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    - Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    - Neither the name of The University of Texas at Austin nor the names
      of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include "blis.h"

#undef  GENTFUNCCO
#define GENTFUNCCO( ctype, ctype_r, ch, chr, varname, gemmkerid ) \
\
void PASTEMAC(ch,varname) \
     ( \
       dim_t               k, \
       ctype*     restrict alpha, \
       ctype*     restrict a, \
       ctype*     restrict b, \
       ctype*     restrict beta, \
       ctype*     restrict c, inc_t rs_c, inc_t cs_c, \
       auxinfo_t* restrict data, \
       cntx_t*    restrict cntx  \
     ) \
{ \
	const num_t       dt        = PASTEMAC(ch,type); \
	const num_t       dt_r      = PASTEMAC(chr,type); \
\
	PASTECH(chr,gemm_ukr_ft) \
	                  rgemm_ukr = bli_cntx_get_l3_nat_ukr_dt( dt_r, gemmkerid, cntx ); \
	const bool_t      col_pref  = bli_cntx_l3_ukr_prefers_cols_dt( dt, BLIS_GEMM_UKR, cntx ); \
	const bool_t      row_pref  = !col_pref; \
\
	const dim_t       mr        = bli_cntx_get_blksz_def_dt( dt, BLIS_MR, cntx ); \
	const dim_t       nr        = bli_cntx_get_blksz_def_dt( dt, BLIS_NR, cntx ); \
\
	const dim_t       k2        = 2 * k; \
\
	ctype             ct[ BLIS_STACK_BUF_MAX_SIZE \
	                      / sizeof( ctype_r ) ] \
	                      __attribute__((aligned(BLIS_STACK_BUF_ALIGN_SIZE))); \
	inc_t             rs_ct; \
	inc_t             cs_ct; \
\
	ctype_r* restrict a_r       = ( ctype_r* )a; \
\
	ctype_r* restrict b_r       = ( ctype_r* )b; \
\
	ctype_r* restrict zero_r    = PASTEMAC(chr,0); \
\
	ctype_r* restrict alpha_r   = &PASTEMAC(ch,real)( *alpha ); \
	ctype_r* restrict alpha_i   = &PASTEMAC(ch,imag)( *alpha ); \
\
	const ctype_r     beta_r    = PASTEMAC(ch,real)( *beta ); \
	const ctype_r     beta_i    = PASTEMAC(ch,imag)( *beta ); \
\
	ctype_r           beta_use; \
\
	ctype_r*          c_use; \
	inc_t             rs_c_use; \
	inc_t             cs_c_use; \
\
	bool_t            using_ct; \
\
\
	/* SAFETY CHECK: The higher level implementation should never
	   allow an alpha with non-zero imaginary component to be passed
	   in, because it can't be applied properly using the 1m method.
	   If alpha is not real, then something is very wrong. */ \
	if ( !PASTEMAC(chr,eq0)( *alpha_i ) ) \
		bli_check_error_code( BLIS_NOT_YET_IMPLEMENTED ); \
\
\
	/* If beta has a non-zero imaginary component OR if c is stored with
	   general stride OR if for some reason the storage of c is not the
	   preferred storage of the micro-kernel, then we compute the
	   alpha*a*b product into temporary storage and then accumulate that
	   result into c afterwards. */ \
	if      ( !PASTEMAC(chr,eq0)( beta_i ) )                using_ct = TRUE; \
	else if ( bli_is_col_stored( rs_c, cs_c ) && row_pref ) using_ct = TRUE; \
	else if ( bli_is_row_stored( rs_c, cs_c ) && col_pref ) using_ct = TRUE; \
	else if ( bli_is_gen_stored( rs_c, cs_c ) )             using_ct = TRUE; \
	else                                                    using_ct = FALSE; \
\
\
	if ( using_ct ) \
	{ \
		/* Set the strides of ct based on the preference of the underlying
		   native real domain gemm micro-kernel. Note that we set the ct
		   strides in units of complex elements. */ \
		if ( col_pref ) { rs_ct = 1;  cs_ct = mr; } \
		else            { rs_ct = nr; cs_ct = 1; } \
\
		beta_use = *zero_r; \
		c_use    = ( ctype_r* )ct; \
		rs_c_use = rs_ct; \
		cs_c_use = cs_ct; \
	} \
	else \
	{ \
		/* In a typical case, we use the real part of beta and accumulate
		   directly into the output matrix c. */ \
		beta_use = beta_r; \
		c_use    = ( ctype_r* )c; \
		rs_c_use = rs_c; \
		cs_c_use = cs_c; \
	} \
\
\
	/* Convert the strides from being in units of complex elements to
	   be in units of real elements. Note that we don't need to check for
	   general storage here because that case corresponds to the scenario
	   where we are using the ct buffer and its rs_ct/cs_ct strides. */ \
	if ( bli_is_col_stored( rs_c_use, cs_c_use ) ) cs_c_use *= 2; \
	else                                           rs_c_use *= 2; \
\
\
	/* The following gemm micro-kernel call implements the 1m method,
	   which induces a complex matrix multiplication by calling the
	   real matrix micro-kernel on micro-panels that have been packed
	   according to the 1e and 1r formats. */ \
\
	/* c = beta * c + alpha_r * a * b; */ \
	rgemm_ukr \
	( \
	  k2, \
	  alpha_r, \
	  a_r, \
	  b_r, \
	  &beta_use, \
	  c_use, rs_c_use, cs_c_use, \
	  data, \
	  cntx  \
	); \
\
\
	/* If necessary, accumulate the final result in ct back to c. */ \
	if ( using_ct ) \
	{ \
		dim_t i, j; \
\
		for ( j = 0; j < nr; ++j ) \
		for ( i = 0; i < mr; ++i ) \
		{ \
			PASTEMAC(ch,xpbys)( *(ct + i*rs_ct + j*cs_ct), \
			                    *beta, \
			                    *(c  + i*rs_c  + j*cs_c ) ); \
		} \
	} \
}

INSERT_GENTFUNCCO_BASIC( gemm1m_ukr_ref, BLIS_GEMM_UKR )
