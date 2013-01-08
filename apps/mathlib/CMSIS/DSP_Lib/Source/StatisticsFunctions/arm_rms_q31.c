/* ----------------------------------------------------------------------    
* Copyright (C) 2010 ARM Limited. All rights reserved.    
*    
* $Date:        15. February 2012  
* $Revision: 	V1.1.0  
*    
* Project: 	    CMSIS DSP Library    
* Title:		arm_rms_q31.c    
*    
* Description:	Root Mean Square of the elements of a Q31 vector.    
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
 * @addtogroup RMS        
 * @{        
 */


/**        
 * @brief Root Mean Square of the elements of a Q31 vector.        
 * @param[in]       *pSrc points to the input vector        
 * @param[in]       blockSize length of the input vector        
 * @param[out]      *pResult rms value returned here        
 * @return none.        
 *        
 * @details        
 * <b>Scaling and Overflow Behavior:</b>        
 *        
 *\par        
 * The function is implemented using an internal 64-bit accumulator.        
 * The input is represented in 1.31 format, and intermediate multiplication        
 * yields a 2.62 format.        
 * The accumulator maintains full precision of the intermediate multiplication results,         
 * but provides only a single guard bit.        
 * There is no saturation on intermediate additions.        
 * If the accumulator overflows, it wraps around and distorts the result.         
 * In order to avoid overflows completely, the input signal must be scaled down by         
 * log2(blockSize) bits, as a total of blockSize additions are performed internally.         
 * Finally, the 2.62 accumulator is right shifted by 31 bits to yield a 1.31 format value.        
 *        
 */

void arm_rms_q31(
  q31_t * pSrc,
  uint32_t blockSize,
  q31_t * pResult)
{
  q63_t sum = 0;                                 /* accumulator */
  q31_t in;                                      /* Temporary variable to store the input */
  uint32_t blkCnt;                               /* loop counter */

#ifndef ARM_MATH_CM0

  /* Run the below code for Cortex-M4 and Cortex-M3 */

  q31_t in1, in2, in3, in4;                      /* Temporary input variables */

  /*loop Unrolling */
  blkCnt = blockSize >> 2u;

  /* First part of the processing with loop unrolling.  Compute 8 outputs at a time.        
   ** a second loop below computes the remaining 1 to 7 samples. */
  while(blkCnt > 0u)
  {
    /* C = A[0] * A[0] + A[1] * A[1] + A[2] * A[2] + ... + A[blockSize-1] * A[blockSize-1] */
    /* Compute sum of the squares and then store the result in a temporary variable, sum */
    /* read two samples from source buffer */
    in1 = pSrc[0];
    in2 = pSrc[1];

    /* calculate power and accumulate to accumulator */
    sum += (q63_t) in1 *in1;
    sum += (q63_t) in2 *in2;

    /* read two samples from source buffer */
    in3 = pSrc[2];
    in4 = pSrc[3];

    /* calculate power and accumulate to accumulator */
    sum += (q63_t) in3 *in3;
    sum += (q63_t) in4 *in4;


    /* update source buffer to process next samples */
    pSrc += 4u;

    /* Decrement the loop counter */
    blkCnt--;
  }

  /* If the blockSize is not a multiple of 8, compute any remaining output samples here.        
   ** No loop unrolling is used. */
  blkCnt = blockSize % 0x4u;

#else

  /* Run the below code for Cortex-M0 */
  blkCnt = blockSize;

#endif /* #ifndef ARM_MATH_CM0 */

  while(blkCnt > 0u)
  {
    /* C = A[0] * A[0] + A[1] * A[1] + A[2] * A[2] + ... + A[blockSize-1] * A[blockSize-1] */
    /* Compute sum of the squares and then store the results in a temporary variable, sum */
    in = *pSrc++;
    sum += (q63_t) in *in;

    /* Decrement the loop counter */
    blkCnt--;
  }

  /* Convert data in 2.62 to 1.31 by 31 right shifts and saturate */

  sum = __SSAT(sum >> 31, 31);


  /* Compute Rms and store the result in the destination vector */
  arm_sqrt_q31((q31_t) ((q31_t) sum / (int32_t) blockSize), pResult);
}

/**        
 * @} end of RMS group        
 */
