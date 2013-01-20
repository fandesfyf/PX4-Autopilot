/* ----------------------------------------------------------------------    
* Copyright (C) 2010 ARM Limited. All rights reserved.    
*    
* $Date:        15. February 2012  
* $Revision: 	V1.1.0  
*    
* Project: 	    CMSIS DSP Library    
* Title:		arm_cmplx_conj_f32.c    
*    
* Description:	Floating-point complex conjugate.    
*    
* Target Processor: Cortex-M4/Cortex-M3/Cortex-M0
*  
* Version 1.1.0 2012/02/15 
*    Updated with more optimizations, bug fixes and minor API changes.  
*   
* Version 1.0.10 2011/7/15  
*    Big Endian support added and Merged M0 and M3/M4 Source code.   
*    
* Version 1.0.3 2010/11/29   
*    Re-organized the CMSIS folders and updated documentation.    
*     
* Version 1.0.2 2010/11/11    
*    Documentation updated.     
*    
* Version 1.0.1 2010/10/05     
*    Production release and review comments incorporated.    
*    
* Version 1.0.0 2010/09/20     
*    Production release and review comments incorporated.    
* ---------------------------------------------------------------------------- */
#include "arm_math.h"

/**        
 * @ingroup groupCmplxMath        
 */

/**        
 * @defgroup cmplx_conj Complex Conjugate        
 *        
 * Conjugates the elements of a complex data vector.        
 *       
 * The <code>pSrc</code> points to the source data and        
 * <code>pDst</code> points to the where the result should be written.        
 * <code>numSamples</code> specifies the number of complex samples        
 * and the data in each array is stored in an interleaved fashion        
 * (real, imag, real, imag, ...).        
 * Each array has a total of <code>2*numSamples</code> values.        
 * The underlying algorithm is used:        
 *        
 * <pre>        
 * for(n=0; n<numSamples; n++) {        
 *     pDst[(2*n)+0)] = pSrc[(2*n)+0];     // real part        
 *     pDst[(2*n)+1)] = -pSrc[(2*n)+1];    // imag part        
 * }        
 * </pre>        
 *        
 * There are separate functions for floating-point, Q15, and Q31 data types.        
 */

/**        
 * @addtogroup cmplx_conj        
 * @{        
 */

/**        
 * @brief  Floating-point complex conjugate.        
 * @param  *pSrc points to the input vector        
 * @param  *pDst points to the output vector        
 * @param  numSamples number of complex samples in each vector        
 * @return none.        
 */

void arm_cmplx_conj_f32(
  float32_t * pSrc,
  float32_t * pDst,
  uint32_t numSamples)
{
  uint32_t blkCnt;                               /* loop counter */

#ifndef ARM_MATH_CM0

  /* Run the below code for Cortex-M4 and Cortex-M3 */
  float32_t inR1, inR2, inR3, inR4;
  float32_t inI1, inI2, inI3, inI4;

  /*loop Unrolling */
  blkCnt = numSamples >> 2u;

  /* First part of the processing with loop unrolling.  Compute 4 outputs at a time.        
   ** a second loop below computes the remaining 1 to 3 samples. */
  while(blkCnt > 0u)
  {
    /* C[0]+jC[1] = A[0]+ j (-1) A[1] */
    /* Calculate Complex Conjugate and then store the results in the destination buffer. */
    /* read real input samples */
    inR1 = pSrc[0];
    /* store real samples to destination */
    pDst[0] = inR1;
    inR2 = pSrc[2];
    pDst[2] = inR2;
    inR3 = pSrc[4];
    pDst[4] = inR3;
    inR4 = pSrc[6];
    pDst[6] = inR4;

    /* read imaginary input samples */
    inI1 = pSrc[1];
    inI2 = pSrc[3];

    /* conjugate input */
    inI1 = -inI1;

    /* read imaginary input samples */
    inI3 = pSrc[5];

    /* conjugate input */
    inI2 = -inI2;

    /* read imaginary input samples */
    inI4 = pSrc[7];

    /* conjugate input */
    inI3 = -inI3;

    /* store imaginary samples to destination */
    pDst[1] = inI1;
    pDst[3] = inI2;

    /* conjugate input */
    inI4 = -inI4;

    /* store imaginary samples to destination */
    pDst[5] = inI3;

    /* increment source pointer by 8 to process next sampels */
    pSrc += 8u;

    /* store imaginary sample to destination */
    pDst[7] = inI4;

    /* increment destination pointer by 8 to store next samples */
    pDst += 8u;

    /* Decrement the loop counter */
    blkCnt--;
  }

  /* If the numSamples is not a multiple of 4, compute any remaining output samples here.        
   ** No loop unrolling is used. */
  blkCnt = numSamples % 0x4u;

#else

  /* Run the below code for Cortex-M0 */
  blkCnt = numSamples;

#endif /* #ifndef ARM_MATH_CM0 */

  while(blkCnt > 0u)
  {
    /* realOut + j (imagOut) = realIn + j (-1) imagIn */
    /* Calculate Complex Conjugate and then store the results in the destination buffer. */
    *pDst++ = *pSrc++;
    *pDst++ = -*pSrc++;

    /* Decrement the loop counter */
    blkCnt--;
  }
}

/**        
 * @} end of cmplx_conj group        
 */
