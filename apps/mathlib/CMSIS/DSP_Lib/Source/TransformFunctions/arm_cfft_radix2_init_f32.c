/* ----------------------------------------------------------------------   
* Copyright (C) 2010 ARM Limited. All rights reserved.   
*   
* $Date:        15. February 2012  
* $Revision: 	V1.1.0  
*   
* Project: 	    CMSIS DSP Library   
* Title:	    arm_cfft_radix4_init_f32.c   
*   
* Description:	Radix-4 Decimation in Frequency Floating-point CFFT & CIFFT Initialization function   
*   
* Target Processor: Cortex-M4/Cortex-M3/Cortex-M0
*  
* Version 1.1.0 2012/02/15 
*    Updated with more optimizations, bug fixes and minor API changes.  
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
* Version 0.0.5  2010/04/26    
* 	 incorporated review comments and updated with latest CMSIS layer   
*   
* Version 0.0.3  2010/03/10    
*    Initial version   
* -------------------------------------------------------------------- */


#include "arm_math.h"
#include "arm_common_tables.h"

/**   
 * @ingroup groupTransforms   
 */

/**   
 * @addtogroup Radix2_CFFT_CIFFT   
 * @{   
 */

/**   
* @brief  Initialization function for the floating-point CFFT/CIFFT.  
* @param[in,out] *S             points to an instance of the floating-point CFFT/CIFFT structure.  
* @param[in]     fftLen         length of the FFT.  
* @param[in]     ifftFlag       flag that selects forward (ifftFlag=0) or inverse (ifftFlag=1) transform.  
* @param[in]     bitReverseFlag flag that enables (bitReverseFlag=1) or disables (bitReverseFlag=0) bit reversal of output.  
* @return        The function returns ARM_MATH_SUCCESS if initialization is successful or ARM_MATH_ARGUMENT_ERROR if <code>fftLen</code> is not a supported value.  
*   
* \par Description:  
* \par   
* The parameter <code>ifftFlag</code> controls whether a forward or inverse transform is computed.   
* Set(=1) ifftFlag for calculation of CIFFT otherwise  CFFT is calculated  
* \par   
* The parameter <code>bitReverseFlag</code> controls whether output is in normal order or bit reversed order.   
* Set(=1) bitReverseFlag for output to be in normal order otherwise output is in bit reversed order.   
* \par   
* The parameter <code>fftLen</code>	Specifies length of CFFT/CIFFT process. Supported FFT Lengths are 16, 64, 256, 1024.   
* \par   
* This Function also initializes Twiddle factor table pointer and Bit reversal table pointer.   
*/
arm_status arm_cfft_radix2_init_f32(
  arm_cfft_radix2_instance_f32 * S,
  uint16_t fftLen,
  uint8_t ifftFlag,
  uint8_t bitReverseFlag)
{
  /*  Initialise the default arm status */
  arm_status status = ARM_MATH_SUCCESS;

  /*  Initialise the FFT length */
  S->fftLen = fftLen;

  /*  Initialise the Twiddle coefficient pointer */
  S->pTwiddle = (float32_t *) twiddleCoef;

  /*  Initialise the Flag for selection of CFFT or CIFFT */
  S->ifftFlag = ifftFlag;

  /*  Initialise the Flag for calculation Bit reversal or not */
  S->bitReverseFlag = bitReverseFlag;

  /*  Initializations of structure parameters depending on the FFT length */
  switch (S->fftLen)
  {

  case 4096u:
    /*  Initializations of structure parameters for 4096 point FFT */

    /*  Initialise the twiddle coef modifier value */
    S->twidCoefModifier = 1u;
    /*  Initialise the bit reversal table modifier */
    S->bitRevFactor = 1u;
    /*  Initialise the bit reversal table pointer */
    S->pBitRevTable = (uint16_t *) armBitRevTable;
    /*  Initialise the 1/fftLen Value */
    S->onebyfftLen = 0.000244140625;
    break;

  case 2048u:
    /*  Initializations of structure parameters for 2048 point FFT */

    /*  Initialise the twiddle coef modifier value */
    S->twidCoefModifier = 2u;
    /*  Initialise the bit reversal table modifier */
    S->bitRevFactor = 2u;
    /*  Initialise the bit reversal table pointer */
    S->pBitRevTable = (uint16_t *) & armBitRevTable[1];
    /*  Initialise the 1/fftLen Value */
    S->onebyfftLen = 0.00048828125;
    break;

  case 1024u:
    /*  Initializations of structure parameters for 1024 point FFT */

    /*  Initialise the twiddle coef modifier value */
    S->twidCoefModifier = 4u;
    /*  Initialise the bit reversal table modifier */
    S->bitRevFactor = 4u;
    /*  Initialise the bit reversal table pointer */
    S->pBitRevTable = (uint16_t *) & armBitRevTable[3];
    /*  Initialise the 1/fftLen Value */
    S->onebyfftLen = 0.0009765625f;
    break;

  case 512u:
    /*  Initializations of structure parameters for 512 point FFT */

    /*  Initialise the twiddle coef modifier value */
    S->twidCoefModifier = 8u;
    /*  Initialise the bit reversal table modifier */
    S->bitRevFactor = 8u;
    /*  Initialise the bit reversal table pointer */
    S->pBitRevTable = (uint16_t *) & armBitRevTable[7];
    /*  Initialise the 1/fftLen Value */
    S->onebyfftLen = 0.001953125;
    break;

  case 256u:
    /*  Initializations of structure parameters for 256 point FFT */
    S->twidCoefModifier = 16u;
    S->bitRevFactor = 16u;
    S->pBitRevTable = (uint16_t *) & armBitRevTable[15];
    S->onebyfftLen = 0.00390625f;
    break;

  case 128u:
    /*  Initializations of structure parameters for 128 point FFT */
    S->twidCoefModifier = 32u;
    S->bitRevFactor = 32u;
    S->pBitRevTable = (uint16_t *) & armBitRevTable[31];
    S->onebyfftLen = 0.0078125;
    break;

  case 64u:
    /*  Initializations of structure parameters for 64 point FFT */
    S->twidCoefModifier = 64u;
    S->bitRevFactor = 64u;
    S->pBitRevTable = (uint16_t *) & armBitRevTable[63];
    S->onebyfftLen = 0.015625f;
    break;

  case 32u:
    /*  Initializations of structure parameters for 64 point FFT */
    S->twidCoefModifier = 128u;
    S->bitRevFactor = 128u;
    S->pBitRevTable = (uint16_t *) & armBitRevTable[127];
    S->onebyfftLen = 0.03125;
    break;

  case 16u:
    /*  Initializations of structure parameters for 16 point FFT */
    S->twidCoefModifier = 256u;
    S->bitRevFactor = 256u;
    S->pBitRevTable = (uint16_t *) & armBitRevTable[255];
    S->onebyfftLen = 0.0625f;
    break;


  default:
    /*  Reporting argument error if fftSize is not valid value */
    status = ARM_MATH_ARGUMENT_ERROR;
    break;
  }

  return (status);
}

/**   
 * @} end of Radix2_CFFT_CIFFT group   
 */
