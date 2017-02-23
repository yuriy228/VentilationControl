// tools.cpp
////////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include <cassert>

#if not defined(STM32F407xx)
	#error file implemented for STM32F407xx only
#endif


////////////////////////////////////////////////////////////////////////////////
// Global objects



////////////////////////////////////////////////////////////////////////////////
// Common functions


template < class ObjectType, class HALObjectType >
bool ObservedHALObjectsList< ObjectType, HALObjectType >::FindObject( void *pHandlerPtr, int &nIndex )		// static function
{
	for ( nIndex = 0; nIndex < m_nObjectsListSize; nIndex++ )
	{
		if ( m_arrObjects[nIndex]->Instance == pHandlerPtr )
			return true;
	}

	return false;
}



template < class ObjectType, class HALObjectType >
void ObservedHALObjectsList< ObjectType, HALObjectType >::CallHALHandler( void *pHandlerPtr, void (* fncHALHandler)(HALObjectType*) )
{
	int nInd = 0;
	if ( FindObject( pHandlerPtr, nInd ) )
		fncHALHandler( m_arrObjects[nInd] );
}





////////////////////////////////////////////////////////////////////////////////
// Timers functions


void TIMObjectType::EnableIRQs()
{
	// Enable all IRQs (since we defined all handlers below)
	NVIC_EnableIRQ( TIM1_BRK_TIM9_IRQn );
	NVIC_EnableIRQ( TIM1_UP_TIM10_IRQn );
	NVIC_EnableIRQ( TIM1_TRG_COM_TIM11_IRQn );
	NVIC_EnableIRQ( TIM1_CC_IRQn );
	NVIC_EnableIRQ( TIM2_IRQn );
	NVIC_EnableIRQ( TIM3_IRQn );
	NVIC_EnableIRQ( TIM4_IRQn );
	NVIC_EnableIRQ( TIM5_IRQn );
	NVIC_EnableIRQ( TIM6_DAC_IRQn );
	NVIC_EnableIRQ( TIM7_IRQn );
	NVIC_EnableIRQ( TIM8_BRK_TIM12_IRQn );
	NVIC_EnableIRQ( TIM8_UP_TIM13_IRQn );
	NVIC_EnableIRQ( TIM8_TRG_COM_TIM14_IRQn );
	NVIC_EnableIRQ( TIM8_CC_IRQn );
}


// Sets priority for all IRQs of TIM
//
void TIMObjectType::SetIRQPriority( uint nPriority )
{
	#define SET( TIMx, IRQn )	if ( Instance == TIMx ) HAL_NVIC_SetPriority( IRQn, nPriority, 0 );

	SET( TIM1, TIM1_BRK_TIM9_IRQn );
	SET( TIM1, TIM1_UP_TIM10_IRQn );
	SET( TIM1, TIM1_TRG_COM_TIM11_IRQn );
	SET( TIM1, TIM1_CC_IRQn );
	SET( TIM2, TIM2_IRQn );
	SET( TIM3, TIM3_IRQn );
	SET( TIM4, TIM4_IRQn );
	SET( TIM5, TIM5_IRQn );
	SET( TIM6, TIM6_DAC_IRQn );
	SET( TIM7, TIM7_IRQn );
	SET( TIM8, TIM8_BRK_TIM12_IRQn );
	SET( TIM8, TIM8_UP_TIM13_IRQn );
	SET( TIM8, TIM8_TRG_COM_TIM14_IRQn );
	SET( TIM8, TIM8_CC_IRQn );

	SET( TIM9, TIM1_BRK_TIM9_IRQn );
	SET( TIM10, TIM1_UP_TIM10_IRQn );
	SET( TIM11, TIM1_TRG_COM_TIM11_IRQn );
	SET( TIM12, TIM8_BRK_TIM12_IRQn );
	SET( TIM13, TIM8_UP_TIM13_IRQn );
	SET( TIM14, TIM8_TRG_COM_TIM14_IRQn );
}



// Enables clock for TIM
//
void TIMObjectType::CLKEnable()
{
	#define ENABLE( TIMx, Enabler )		if ( Instance == TIMx )  { Enabler(); return; }

	ENABLE( TIM1, __TIM1_CLK_ENABLE );
	ENABLE( TIM2, __TIM2_CLK_ENABLE );
	ENABLE( TIM3, __TIM3_CLK_ENABLE );
	ENABLE( TIM4, __TIM4_CLK_ENABLE );
	ENABLE( TIM5, __TIM5_CLK_ENABLE );
	ENABLE( TIM6, __TIM6_CLK_ENABLE );
	ENABLE( TIM7, __TIM7_CLK_ENABLE );
	ENABLE( TIM8, __TIM8_CLK_ENABLE );
	ENABLE( TIM9, __TIM9_CLK_ENABLE );
	ENABLE( TIM10, __TIM10_CLK_ENABLE );
	ENABLE( TIM11, __TIM11_CLK_ENABLE );
	ENABLE( TIM12, __TIM12_CLK_ENABLE );
	ENABLE( TIM13, __TIM13_CLK_ENABLE );
	ENABLE( TIM14, __TIM14_CLK_ENABLE );
}



// Returns frequency for TIM
//
uint TIMObjectType::GetFrequency()
{
	if ( Instance >= (void *)APB1PERIPH_BASE && Instance < (void *)APB2PERIPH_BASE )
		return HAL_RCC_GetPCLK1Freq() * 2;
	else
		return HAL_RCC_GetPCLK2Freq() * 2;
}



// Returns alternate function ID for GPIOs for TIM
//
uint TIMObjectType::GetGPIOAltFuncID()
{
	if ( Instance == TIM1 || Instance == TIM2 )  											return GPIO_AF1_TIM1;	// the same as GPIO_AF1_TIM2
	if ( Instance == TIM3 || Instance == TIM4 || Instance == TIM5 )  						return GPIO_AF2_TIM3;
	if ( Instance == TIM8 || Instance == TIM9 || Instance == TIM10 || Instance == TIM11 )  	return GPIO_AF3_TIM8;
	if ( Instance == TIM12 || Instance == TIM13 || Instance == TIM14 )  					return GPIO_AF9_TIM12;
	return 0;
}



// IRQ handlers
//
extern "C"
{
#define CallHALHandler_TIM( TIMx )			TIMObjectType::CallHALHandler( TIMx, HAL_TIM_IRQHandler );
#define CallHALHandler_TIM2( TIMx1, TIMx2 )	TIMObjectType::CallHALHandler( TIMx1, HAL_TIM_IRQHandler ); TIMObjectType::CallHALHandler( TIMx2, HAL_TIM_IRQHandler );

void TIM1_BRK_TIM9_IRQHandler()			{ CallHALHandler_TIM2( 	TIM1, TIM9 ); }
void TIM1_UP_TIM10_IRQHandler()			{ CallHALHandler_TIM2( 	TIM1, TIM10 ); }
void TIM1_TRG_COM_TIM11_IRQHandler()	{ CallHALHandler_TIM2( 	TIM1, TIM11 ); }
void TIM1_CC_IRQHandler()				{ CallHALHandler_TIM( 	TIM1 ); }
void TIM2_IRQHandler()					{ CallHALHandler_TIM( 	TIM2 ); }
void TIM3_IRQHandler()					{ CallHALHandler_TIM( 	TIM3 ); }
void TIM4_IRQHandler()					{ CallHALHandler_TIM( 	TIM4 ); }
void TIM5_IRQHandler()					{ CallHALHandler_TIM( 	TIM5 ); }
void TIM6_DAC_IRQHandler()				{ CallHALHandler_TIM( 	TIM6 ); }
void TIM7_IRQHandler()					{ CallHALHandler_TIM( 	TIM7 ); }
void TIM8_BRK_TIM12_IRQHandler()		{ CallHALHandler_TIM2( 	TIM8, TIM12 ); }
void TIM8_UP_TIM13_IRQHandler()			{ CallHALHandler_TIM2( 	TIM8, TIM13 ); }
void TIM8_TRG_COM_TIM14_IRQHandler()	{ CallHALHandler_TIM2( 	TIM8, TIM14 ); }
void TIM8_CC_IRQHandler()				{ CallHALHandler_TIM( 	TIM8 ); }
}


// HAL callbacks
//
#define DefineTIMCallback( CallbackID )		void HAL_TIM_##CallbackID( TIM_HandleTypeDef *htim ) 	\
{																									\
	int nInd = 0;																					\
	if ( TIMObjectType::FindObject( htim->Instance, nInd ) )										\
		TIMObjectType::m_arrObjects[nInd]->Callback( TCT_##CallbackID );							\
}

DefineTIMCallback( PeriodElapsedCallback );
DefineTIMCallback( OC_DelayElapsedCallback );
DefineTIMCallback( IC_CaptureCallback );
DefineTIMCallback( PWM_PulseFinishedCallback );
DefineTIMCallback( TriggerCallback );
DefineTIMCallback( ErrorCallback );





////////////////////////////////////////////////////////////////////////////////
// ADC functions


// Enables clock for TIM
//
void ADCObjectType::CLKEnable()
{
	#define ENABLE( TIMx, Enabler )		if ( Instance == TIMx )  { Enabler(); return; }

	ENABLE( ADC1, __ADC1_CLK_ENABLE );
	ENABLE( ADC2, __ADC2_CLK_ENABLE );
	ENABLE( ADC3, __ADC3_CLK_ENABLE );
}





////////////////////////////////////////////////////////////////////////////////
// Global functions


IRQn_Type GetExtiIrqByGPIOPin( uint nPin )
{
	switch ( nPin )
	{
	case GPIO_PIN_0:	return EXTI0_IRQn;
	case GPIO_PIN_1:	return EXTI1_IRQn;
	case GPIO_PIN_2:	return EXTI2_IRQn;
	case GPIO_PIN_3:	return EXTI3_IRQn;
	case GPIO_PIN_4:	return EXTI4_IRQn;

	case GPIO_PIN_5:
	case GPIO_PIN_6:
	case GPIO_PIN_7:
	case GPIO_PIN_8:
	case GPIO_PIN_9:	return EXTI9_5_IRQn;

	case GPIO_PIN_10:
	case GPIO_PIN_11:
	case GPIO_PIN_12:
	case GPIO_PIN_13:
	case GPIO_PIN_14:
	case GPIO_PIN_15:	return EXTI15_10_IRQn;

	default:
		assert(false);
	}
}







