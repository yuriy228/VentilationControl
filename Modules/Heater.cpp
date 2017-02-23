// Heater.h
/////////////////////////////////////////////////////////////////////////////////


#include "main.h"
#include "Heater.h"
#include "Communication.h"


#define 	Heater_PWMTimerFrequency				20000
#define 	Heater_PWMTimerPrescaler				10
#define 	Heater_PowerPercentBoundsThreshold		3		// defines width of unwanted power percent top and bottom range

TIMObjectType Heater_PWMTimerObject( Heater_PWMTimerHandle );




void Heater_Init()
{
	// *** Init GPIO

	Heater_PWMOutputGPIO.Init( GPIO_MODE_AF_PP, GPIO_NOPULL, Heater_PWMTimerObject.GetGPIOAltFuncID() );

	Heater_EnableOutputGPIO.Init( GPIO_MODE_OUTPUT_PP, GPIO_NOPULL );


	// *** Init timer

	uint nTimerPeriod = Heater_PWMTimerObject.GetFrequency() / Heater_PWMTimerPrescaler / Heater_PWMTimerFrequency;

	Heater_PWMTimerObject.CLKEnable();

	// Init TIM
	Heater_PWMTimerObject.Init.Prescaler = Heater_PWMTimerPrescaler;
	Heater_PWMTimerObject.Init.CounterMode = TIM_COUNTERMODE_UP;
	Heater_PWMTimerObject.Init.Period = nTimerPeriod;
	Heater_PWMTimerObject.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	Heater_PWMTimerObject.Init.RepetitionCounter = 0;
	HAL_TIM_PWM_Init( &Heater_PWMTimerObject );

	// Clock config
	TIM_ClockConfigTypeDef sClockSourceConfig = {0};
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	HAL_TIM_ConfigClockSource( &Heater_PWMTimerObject, &sClockSourceConfig );

	// Master config
	TIM_MasterConfigTypeDef sMasterConfig;
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	HAL_TIMEx_MasterConfigSynchronization( &Heater_PWMTimerObject, &sMasterConfig );

	// Channels config
	TIM_OC_InitTypeDef sConfigOC;
	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	sConfigOC.Pulse = 0;			// min value to turn off the output at the start
	sConfigOC.OCPolarity = TIM_OCPOLARITY_LOW;
	sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
	sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
	HAL_TIM_PWM_ConfigChannel( &Heater_PWMTimerObject, &sConfigOC, TIM_CHANNEL_1 );


	// *** Start timer

	HAL_TIM_PWM_Start( &Heater_PWMTimerObject, TIM_CHANNEL_1 );
}



void Heater_SetPower( int nPercents )
{
	// Update Heater enable pin
	if ( nPercents > 0 && Heater_EnableOutputGPIO.GetPin() == false )
	{
		Heater_EnableOutputGPIO.SetPin( true );
		HAL_Delay( 50 );	// wait until the relay is ON
	}

	// Check for threshold
	int nPercentsFitted = nPercents;

	if ( nPercentsFitted < Heater_PowerPercentBoundsThreshold )
		nPercentsFitted = 0;

	if ( nPercentsFitted > 100 - Heater_PowerPercentBoundsThreshold )
	{
		if ( nPercentsFitted < 100 - Heater_PowerPercentBoundsThreshold / 2 )
			nPercentsFitted = 100 - Heater_PowerPercentBoundsThreshold;
		else
			nPercentsFitted = 100;
	}

	// Set timer ON period for PWM
	uint nComparePeriod = (Heater_PWMTimerObject.Init.Period + 1) * nPercentsFitted / 100;

	__HAL_TIM_SET_COMPARE( &Heater_PWMTimerObject, TIM_CHANNEL_1, nComparePeriod );

	// Update Heater enable pin
	if ( nPercents == 0 && Heater_EnableOutputGPIO.GetPin() )
	{
		HAL_Delay( 50 );	// wait until the heater current is OFF
		Heater_EnableOutputGPIO.SetPin( false );
	}

//Comm_SendDebugString( "InitPeriod: %d  CurPeriod:  %d", Heater_PWMTimerObject.Init.Period, nComparePeriod );

}



