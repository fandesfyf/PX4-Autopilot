/* ---------------------------------------------------------------------- 
* Copyright (C) 2010 ARM Limited. All rights reserved.   
*  
* $Date:        29. November 2010  
* $Revision: 	V1.0.3
*  
* Project: 	    CMSIS DSP Library  
* Title:	    arm_sin_cos_example_f32.c		  
*  
* Description:	Example code demonstrating sin and cos calculation of input signal. 
* 
* Target Processor: Cortex-M4/Cortex-M3  
*
*
* Version 1.0.3 2010/11/29 
*    Re-organized the CMSIS folders and updated documentation. 
* 
* Version 1.0.1 2010/10/05 KK 
*    Production release and review comments incorporated.  
*
* Version 1.0.0 2010/09/20 KK
*    Production release and review comments incorporated.
* ------------------------------------------------------------------- */ 
 
/** 
 * @ingroup groupExamples 
 */ 
 
/**    
 * @defgroup SinCosExample SineCosine Example    
 * 
 * \par Description:
 * \par
 * Demonstrates the Pythagorean trignometric identity with the use of Cosine, Sine, Vector
 * Multiplication, and Vector Addition functions.
 *
 * \par Algorithm:
 * \par
 * Mathematically, the Pythagorean trignometric identity is defined by the following equation:
 *  <pre>sin(x) * sin(x) + cos(x) * cos(x) = 1</pre> 
 * where \c x is the angle in radians. 
 *
 * \par Block Diagram:
 * \par
 * \image html sinCos.gif 
 * 
 * \par Variables Description:
 * \par
 * \li \c testInput_f32 array of input angle in radians
 * \li \c testOutput stores sum of the squares of sine and cosine values of input angle
 *
 * \par CMSIS DSP Software Library Functions Used:
 * \par
 * - arm_cos_f32()
 * - arm_sin_f32()
 * - arm_mult_f32()
 * - arm_add_f32()
 * 
 * <b> Refer  </b> 
 * \link arm_sin_cos_example_f32.c \endlink
 * 
 */ 
 
 
/** \example arm_sin_cos_example_f32.c 
  */  
 
#include <math.h>     
#include "arm_math.h" 
 
/* ---------------------------------------------------------------------- 
* Defines each of the tests performed 
* ------------------------------------------------------------------- */ 
#define MAX_BLOCKSIZE	32 
#define DELTA           (0.000001f) 
 
 
/* ---------------------------------------------------------------------- 
* Test input data for Floating point sin_cos example for 32-blockSize 
* Generated by the MATLAB randn() function 
* ------------------------------------------------------------------- */ 
 
const float32_t testInput_f32[MAX_BLOCKSIZE] =  
{    
	-1.244916875853235400,	-4.793533929171324800,	0.360705030233248850,	0.827929644170887320,	-3.299532218312426900,	3.427441903227623800,	3.422401784294607700,	-0.108308165334010680,	 
	0.941943896490312180,	0.502609575000365850,	-0.537345278736373500,	2.088817392965764500,	-1.693168684143455700,	6.283185307179590700,	-0.392545884746175080,	0.327893095115825040,	 
	3.070147440456292300,	0.170611405884662230,	-0.275275082396073010,	-2.395492805446796300,	0.847311163536506600,	-3.845517018083148800,	2.055818378415868300,	4.672594161978930800,	 
	-1.990923030266425800,	2.469305197656249500,	3.609002606064021000,	-4.586736582331667500,	-4.147080139136136300,	1.643756718868359500,	-1.150866392366494800,	1.985805026477433800 
 
 
};  
 
const float32_t testRefOutput_f32 = 1.000000000; 
 
/* ---------------------------------------------------------------------- 
* Declare Global variables  
* ------------------------------------------------------------------- */ 
uint32_t blockSize = 32; 
float32_t  testOutput;  
float32_t  cosOutput;  
float32_t  sinOutput;  
float32_t  cosSquareOutput;  
float32_t  sinSquareOutput; 
 
/* ---------------------------------------------------------------------- 
* Max magnitude FFT Bin test 
* ------------------------------------------------------------------- */ 

arm_status status; 
 
int32_t main(void) 
{ 
	float32_t diff; 
	uint32_t i; 
 
	for(i=0; i< blockSize; i++) 
    { 
        cosOutput = arm_cos_f32(testInput_f32[i]); 
		sinOutput = arm_sin_f32(testInput_f32[i]); 
 
		arm_mult_f32(&cosOutput, &cosOutput, &cosSquareOutput, 1); 
		arm_mult_f32(&sinOutput, &sinOutput, &sinSquareOutput, 1); 
 
		arm_add_f32(&cosSquareOutput, &sinSquareOutput, &testOutput, 1);
 
		/* absolute value of difference between ref and test */ 
	    diff = fabsf(testRefOutput_f32 - testOutput); 
	 
	    /* Comparison of sin_cos value with reference */ 
	    if(diff > DELTA) 
	    { 
		   status = ARM_MATH_TEST_FAILURE; 
	    } 
		 
	    if( status == ARM_MATH_TEST_FAILURE) 
	    { 
	       while(1); 
	    } 
 
    } 

    while(1);                             /* main function does not return */
} 
 
 /** \endlink */ 
 
