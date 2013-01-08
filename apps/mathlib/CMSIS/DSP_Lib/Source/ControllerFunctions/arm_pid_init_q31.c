/* ----------------------------------------------------------------------    
* Copyright (C) 2010 ARM Limited. All rights reserved.    
*    
* $Date:        15. February 2012  
* $Revision: 	V1.1.0  
*    
* Project: 	    CMSIS DSP Library    
* Title:	    arm_pid_init_q31.c    
*    
* Description:	Q31 PID Control initialization function     
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
* ------------------------------------------------------------------- */

#include "arm_math.h"

 /**    
 * @addtogroup PID    
 * @{    
 */

/**    
 * @brief  Initialization function for the Q31 PID Control.   
 * @param[in,out] *S points to an instance of the Q31 PID structure.   
 * @param[in]     resetStateFlag  flag to reset the state. 0 = no change in state 1 = reset the state.   
 * @return none.    
 * \par Description:   
 * \par    
 * The <code>resetStateFlag</code> specifies whether to set state to zero or not. \n   
 * The function computes the structure fields: <code>A0</code>, <code>A1</code> <code>A2</code>    
 * using the proportional gain( \c Kp), integral gain( \c Ki) and derivative gain( \c Kd)    
 * also sets the state variables to all zeros.    
 */

void arm_pid_init_q31(
  arm_pid_instance_q31 * S,
  int32_t resetStateFlag)
{

#ifndef ARM_MATH_CM0

  /* Run the below code for Cortex-M4 and Cortex-M3 */

  /* Derived coefficient A0 */
  S->A0 = __QADD(__QADD(S->Kp, S->Ki), S->Kd);

  /* Derived coefficient A1 */
  S->A1 = -__QADD(__QADD(S->Kd, S->Kd), S->Kp);


#else

  /* Run the below code for Cortex-M0 */

  q31_t temp;

  /* Derived coefficient A0 */
  temp = clip_q63_to_q31((q63_t) S->Kp + S->Ki);
  S->A0 = clip_q63_to_q31((q63_t) temp + S->Kd);

  /* Derived coefficient A1 */
  temp = clip_q63_to_q31((q63_t) S->Kd + S->Kd);
  S->A1 = -clip_q63_to_q31((q63_t) temp + S->Kp);

#endif /* #ifndef ARM_MATH_CM0 */

  /* Derived coefficient A2 */
  S->A2 = S->Kd;

  /* Check whether state needs reset or not */
  if(resetStateFlag)
  {
    /* Clear the state buffer.  The size will be always 3 samples */
    memset(S->state, 0, 3u * sizeof(q31_t));
  }

}

/**    
 * @} end of PID group    
 */
