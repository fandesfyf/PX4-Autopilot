/* ----------------------------------------------------------------------------    
* Copyright (C) 2010 ARM Limited. All rights reserved.    
*    
* $Date:        15. February 2012  
* $Revision: 	V1.1.0  
*    
* Project: 	    CMSIS DSP Library    
* Title:		arm_q15_to_q7.c    
*    
* Description:	Converts the elements of the Q15 vector to Q7 vector.  
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
 * @ingroup groupSupport    
 */

/**    
 * @addtogroup q15_to_x    
 * @{    
 */


/**    
 * @brief Converts the elements of the Q15 vector to Q7 vector.     
 * @param[in]       *pSrc points to the Q15 input vector    
 * @param[out]      *pDst points to the Q7 output vector   
 * @param[in]       blockSize length of the input vector    
 * @return none.    
 *    
 * \par Description:    
 *    
 * The equation used for the conversion process is:    
 *   
 * <pre>    
 * 	pDst[n] = (q7_t) pSrc[n] >> 8;   0 <= n < blockSize.    
 * </pre>   
 *   
 */


void arm_q15_to_q7(
  q15_t * pSrc,
  q7_t * pDst,
  uint32_t blockSize)
{
  q15_t *pIn = pSrc;                             /* Src pointer */
  uint32_t blkCnt;                               /* loop counter */

#ifndef ARM_MATH_CM0

  /* Run the below code for Cortex-M4 and Cortex-M3 */
  q31_t in1, in2;
  q31_t out1, out2;

  /*loop Unrolling */
  blkCnt = blockSize >> 2u;

  /* First part of the processing with loop unrolling.  Compute 4 outputs at a time.    
   ** a second loop below computes the remaining 1 to 3 samples. */
  while(blkCnt > 0u)
  {
    /* C = (q7_t) A >> 8 */
    /* convert from q15 to q7 and then store the results in the destination buffer */
    in1 = *__SIMD32(pIn)++;
    in2 = *__SIMD32(pIn)++;

#ifndef ARM_MATH_BIG_ENDIAN

    out1 = __PKHTB(in2, in1, 16);
    out2 = __PKHBT(in2, in1, 16);

#else

    out1 = __PKHTB(in1, in2, 16);
    out2 = __PKHBT(in1, in2, 16);

#endif //      #ifndef ARM_MATH_BIG_ENDIAN

    /* rotate packed value by 24 */
    out2 = ((uint32_t) out2 << 8) | ((uint32_t) out2 >> 24);

    /* anding with 0xff00ff00 to get two 8 bit values */
    out1 = out1 & 0xFF00FF00;
    /* anding with 0x00ff00ff to get two 8 bit values */
    out2 = out2 & 0x00FF00FF;

    /* oring two values(contains two 8 bit values) to get four packed 8 bit values */
    out1 = out1 | out2;

    /* store 4 samples at a time to destiantion buffer */
    *__SIMD32(pDst)++ = out1;

    /* Decrement the loop counter */
    blkCnt--;
  }

  /* If the blockSize is not a multiple of 4, compute any remaining output samples here.    
   ** No loop unrolling is used. */
  blkCnt = blockSize % 0x4u;

#else

  /* Run the below code for Cortex-M0 */

  /* Loop over blockSize number of values */
  blkCnt = blockSize;

#endif /* #ifndef ARM_MATH_CM0 */

  while(blkCnt > 0u)
  {
    /* C = (q7_t) A >> 8 */
    /* convert from q15 to q7 and then store the results in the destination buffer */
    *pDst++ = (q7_t) (*pIn++ >> 8);

    /* Decrement the loop counter */
    blkCnt--;
  }

}

/**    
 * @} end of q15_to_x group    
 */
