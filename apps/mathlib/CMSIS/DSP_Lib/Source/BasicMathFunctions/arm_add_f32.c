/* ----------------------------------------------------------------------    
* Copyright (C) 2010 ARM Limited. All rights reserved.    
*    
* $Date:        15. February 2012  
* $Revision: 	V1.1.0  
*    
* Project: 	    CMSIS DSP Library    
* Title:		arm_add_f32.c    
*    
* Description:	Floating-point vector addition.    
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
*    
* Version 0.0.7  2010/06/10     
*    Misra-C changes done    
* ---------------------------------------------------------------------------- */

#include "arm_math.h"

/**        
 * @ingroup groupMath        
 */

/**        
 * @defgroup BasicAdd Vector Addition        
 *        
 * Element-by-element addition of two vectors.        
 *        
 * <pre>        
 *     pDst[n] = pSrcA[n] + pSrcB[n],   0 <= n < blockSize.        
 * </pre>        
 *        
 * There are separate functions for floating-point, Q7, Q15, and Q31 data types.        
 */

/**        
 * @addtogroup BasicAdd        
 * @{        
 */

/**        
 * @brief Floating-point vector addition.        
 * @param[in]       *pSrcA points to the first input vector        
 * @param[in]       *pSrcB points to the second input vector        
 * @param[out]      *pDst points to the output vector        
 * @param[in]       blockSize number of samples in each vector        
 * @return none.        
 */

void arm_add_f32(
  float32_t * pSrcA,
  float32_t * pSrcB,
  float32_t * pDst,
  uint32_t blockSize)
{
  uint32_t blkCnt;                               /* loop counter */

#ifndef ARM_MATH_CM0

/* Run the below code for Cortex-M4 and Cortex-M3 */
  float32_t inA1, inA2, inA3, inA4;              /* temporary input variabels */
  float32_t inB1, inB2, inB3, inB4;              /* temporary input variables */

  /*loop Unrolling */
  blkCnt = blockSize >> 2u;

  /* First part of the processing with loop unrolling.  Compute 4 outputs at a time.        
   ** a second loop below computes the remaining 1 to 3 samples. */
  while(blkCnt > 0u)
  {
    /* C = A + B */
    /* Add and then store the results in the destination buffer. */

    /* read four inputs from sourceA and four inputs from sourceB */
    inA1 = *pSrcA;
    inB1 = *pSrcB;
    inA2 = *(pSrcA + 1);
    inB2 = *(pSrcB + 1);
    inA3 = *(pSrcA + 2);
    inB3 = *(pSrcB + 2);
    inA4 = *(pSrcA + 3);
    inB4 = *(pSrcB + 3);

    /* C = A + B */
    /* add and store result to destination */
    *pDst = inA1 + inB1;
    *(pDst + 1) = inA2 + inB2;
    *(pDst + 2) = inA3 + inB3;
    *(pDst + 3) = inA4 + inB4;

    /* update pointers to process next samples */
    pSrcA += 4u;
    pSrcB += 4u;
    pDst += 4u;


    /* Decrement the loop counter */
    blkCnt--;
  }

  /* If the blockSize is not a multiple of 4, compute any remaining output samples here.        
   ** No loop unrolling is used. */
  blkCnt = blockSize % 0x4u;

#else

  /* Run the below code for Cortex-M0 */

  /* Initialize blkCnt with number of samples */
  blkCnt = blockSize;

#endif /* #ifndef ARM_MATH_CM0 */

  while(blkCnt > 0u)
  {
    /* C = A + B */
    /* Add and then store the results in the destination buffer. */
    *pDst++ = (*pSrcA++) + (*pSrcB++);

    /* Decrement the loop counter */
    blkCnt--;
  }
}

/**        
 * @} end of BasicAdd group        
 */
