// VentilatorControl.h
///////////////////////////////////////////////////////////////

#include "main.h"
#include "VentilatorControl.h"
#include "Communication.h"

#include "arm_math.h"


// Counter timer (TIM6, base clock is 90 MHz)  (the timer will be reset at each output wave start phase)
#define		VC_CounterTimer_Prescaler 	1500		// we need not less than about one reload of the timer (16 bit) per second
#define		VC_CounterTimer_BaseFreq 	84000000	// set by initialization

// Interrupt timer (TIM7, base clock is 90 MHz)
#define		VC_InterruptTimer_Freq 		4000		// interrupt freq in microseconds
#define		VC_InterruptTimer_BaseFreq 	90000000	// set by initialization

// PWM timers
#define 	VC_PWMTimersPeriod			8400		// PWM period in timer clocks, assuming the freq should be 20 kHz (base timers clock is 168000000 MHz)
#define 	VC_PWMTimer1ChannelA		TIM_CHANNEL_2
#define 	VC_PWMTimer1ChannelB		TIM_CHANNEL_3
#define 	VC_PWMTimer2ChannelA		TIM_CHANNEL_1
#define 	VC_PWMTimer2ChannelB		TIM_CHANNEL_3

//
const int 		VC_WavesPhaseShiftPercent = 26;		// phase angle difference between output sinuses
const float32_t VC_PhaseVoltageCompensation = 0.08; // 0.15;//0.09;


int 		VC_CurWavePeriod = 		0;		// current period of output sinus, in milliseconds;  = 0 if wave is off
int 		VC_CurWavePeriod_Actual = 0;	// period actually used for calculations and updated by VC_CurWavePeriod value at start of phase moment

const int 	VC_WavePeriodMin = 		14;		// period of maximal allowed frequency (max freq. may be bigger than the nominal one to overspeed the motor)
const int 	VC_WavePeriodNominal = 	20;		// (!!!) period of 100% frequency (motor nominal speed)
const int 	VC_WavePeriodMax = 		500;	// period of min frequency


TIM_HandleTypeDef 	VC_CounterTimerHandle = {0};		// used in current wave phase calculation
TIMObjectType 		VC_InterruptTimerHandle ( VentilatorControl_InterruptTimerHandle );
TIM_HandleTypeDef 	VC_PWMTimer1Handle = {0};
TIM_HandleTypeDef 	VC_PWMTimer2Handle = {0};


// Private functions
static void PWMTimersInit();
void InterruptTimerCallback( TIMObjectType *pObject, int nCallbackID );


/*
typedef struct { uint sys; uint tim; } str;
#define size 1000
str buff[size];
/**/



void VentilatorControl_Init()
{

	// *** Init Enable motor HW driver pin first and shut down it while initialization
	{
		VentilatorControl_MotorDriverEnableOutGPIO.Init( GPIO_MODE_OUTPUT_PP, GPIO_PULLDOWN );

		VentilatorControl_MotorDriverEnableOutGPIO.SetPin( false );	// motor driver is off
	}


	// *** Setup PWM timers

	PWMTimersInit();


	// *** Setup counter timer
	{
		VentilatorControl_CounterTimerEnableFunc();		// turn on the timer clock

		VC_CounterTimerHandle.Instance = VentilatorControl_CounterTimerHandle;
		VC_CounterTimerHandle.Init.Prescaler = VC_CounterTimer_Prescaler;
		VC_CounterTimerHandle.Init.CounterMode = TIM_COUNTERMODE_UP;
		VC_CounterTimerHandle.Init.Period = 0xFFFF;		// use max since the timer will be reloaded by soft
		HAL_TIM_Base_Init( &VC_CounterTimerHandle );

		TIM_MasterConfigTypeDef sMasterConfig;
		sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
		sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
		HAL_TIMEx_MasterConfigSynchronization( &VC_CounterTimerHandle, &sMasterConfig );

		__HAL_TIM_SET_COUNTER( &VC_CounterTimerHandle, 0 );
		HAL_TIM_Base_Start( &VC_CounterTimerHandle );
	}

	// *** Setup interrupt timer
	{
		VentilatorControl_InterruptTimerEnableFunc();	// turn on the timer clock

		uint InterruptTimerPrescaler = (VC_InterruptTimer_BaseFreq / 0xFFFF) / VC_InterruptTimer_Freq;		// get min prescaler for goal freq
		uint InterruptTimerPeriod =  ( VC_InterruptTimer_BaseFreq / (InterruptTimerPrescaler + 1) ) / VC_InterruptTimer_Freq;

		//VC_InterruptTimerHandle.Instance = VentilatorControl_InterruptTimerHandle;
		VC_InterruptTimerHandle.Init.Prescaler = InterruptTimerPrescaler;
		VC_InterruptTimerHandle.Init.CounterMode = TIM_COUNTERMODE_UP;
		VC_InterruptTimerHandle.Init.Period = InterruptTimerPeriod;
		HAL_TIM_Base_Init( &VC_InterruptTimerHandle );

		VC_InterruptTimerHandle.SetCallback( InterruptTimerCallback );

		TIM_MasterConfigTypeDef sMasterConfig;
		sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
		sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
		HAL_TIMEx_MasterConfigSynchronization( &VC_InterruptTimerHandle, &sMasterConfig );

		HAL_NVIC_SetPriority( VentilatorControl_InterruptTimerIRQn, VentilatorControl_InterruptTimerPriority, 0 );
		HAL_NVIC_EnableIRQ( VentilatorControl_InterruptTimerIRQn );

		__HAL_TIM_SET_COUNTER( &VC_InterruptTimerHandle, 0 );
		HAL_TIM_Base_Start_IT( &VC_InterruptTimerHandle );
	}

}



void VentilatorControl_SetSpeed( int nPercents )
{
	int nNewPeriod = 0;
	if ( nPercents > 0 )
	{
		nNewPeriod = (int)(100 / ( 1.0 / VC_WavePeriodMax * ( 100 - nPercents ) + 1.0 / VC_WavePeriodNominal * nPercents ) );	 //	= 1 / ( 1 / VC_WavePeriodMax + ( 1 / VC_WavePeriodNominal - 1 / VC_WavePeriodMax) * nPercents / 100 );

		if ( nNewPeriod < VC_WavePeriodMin )		nNewPeriod = VC_WavePeriodMin;
		if ( nNewPeriod > VC_WavePeriodMax )		nNewPeriod = VC_WavePeriodMax;
	}

	VC_CurWavePeriod = nNewPeriod;	// (!) load value by one operation since the code may be interrupted

	if ( VC_CurWavePeriod == 0 )
	{
		VC_CurWavePeriod_Actual = 0;	// reset actual period to stop rotation immediately; (!) only after reset of VC_CurWavePeriod
	}

	// Update Enable motor HW driver pin
	if ( nPercents > 0 )
		VentilatorControl_MotorDriverEnableOutGPIO.SetPin( true );
	else
		VentilatorControl_MotorDriverEnableOutGPIO.SetPin( false );
}


bool VentilatorControl_IsEnabled( )
{
	return VC_CurWavePeriod != 0;
}


static void PWMTimersInit()
{
	// *** Init timers

	__TIM1_CLK_ENABLE();
	__TIM8_CLK_ENABLE();

	// Init TIM1
	VC_PWMTimer1Handle.Instance = VentilatorControl_PWMTimer1Handle;
	VC_PWMTimer1Handle.Init.Prescaler = 0;
	VC_PWMTimer1Handle.Init.CounterMode = TIM_COUNTERMODE_UP;
	VC_PWMTimer1Handle.Init.Period = VC_PWMTimersPeriod;
	VC_PWMTimer1Handle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	VC_PWMTimer1Handle.Init.RepetitionCounter = 0;
	HAL_TIM_PWM_Init( &VC_PWMTimer1Handle );

	// Init TIM8
	VC_PWMTimer2Handle.Init = VC_PWMTimer1Handle.Init;
	VC_PWMTimer2Handle.Instance = VentilatorControl_PWMTimer2Handle;
	HAL_TIM_PWM_Init( &VC_PWMTimer2Handle );

	// Clock config
	TIM_ClockConfigTypeDef sClockSourceConfig = {0};
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	HAL_TIM_ConfigClockSource( &VC_PWMTimer1Handle, &sClockSourceConfig );
	HAL_TIM_ConfigClockSource( &VC_PWMTimer2Handle, &sClockSourceConfig );

	// Master config
	TIM_MasterConfigTypeDef sMasterConfig;
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	HAL_TIMEx_MasterConfigSynchronization( &VC_PWMTimer1Handle, &sMasterConfig );
	HAL_TIMEx_MasterConfigSynchronization( &VC_PWMTimer2Handle, &sMasterConfig );

	// Break and dead config
	TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig;
	sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
	sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
	sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
	sBreakDeadTimeConfig.DeadTime = 0xC0 | (30); // 6us: 0xE0 | (30)  3us: 0xC0 | (30);  1us: 0x80 + 20;  0.6us: 100
	sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
	sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
	sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
	HAL_TIMEx_ConfigBreakDeadTime( &VC_PWMTimer1Handle, &sBreakDeadTimeConfig );
	HAL_TIMEx_ConfigBreakDeadTime( &VC_PWMTimer2Handle, &sBreakDeadTimeConfig );

	// Channels config
	TIM_OC_InitTypeDef sConfigOC;
	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	sConfigOC.Pulse = 0;			// min value to turn off the output at the start
	sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC.OCNPolarity = TIM_OCNPOLARITY_LOW;	// inverted
	sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
	sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
	sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
	HAL_TIM_PWM_ConfigChannel(&VC_PWMTimer1Handle, &sConfigOC, VC_PWMTimer1ChannelA);
	HAL_TIM_PWM_ConfigChannel(&VC_PWMTimer1Handle, &sConfigOC, VC_PWMTimer1ChannelB);
	HAL_TIM_PWM_ConfigChannel(&VC_PWMTimer2Handle, &sConfigOC, VC_PWMTimer2ChannelA);
	HAL_TIM_PWM_ConfigChannel(&VC_PWMTimer2Handle, &sConfigOC, VC_PWMTimer2ChannelB);

	// Additionally enable complementary output pins
	TIM_CCxChannelCmd( VC_PWMTimer1Handle.Instance, VC_PWMTimer1ChannelA, TIM_CCxN_ENABLE );
	TIM_CCxChannelCmd( VC_PWMTimer1Handle.Instance, VC_PWMTimer1ChannelB, TIM_CCxN_ENABLE );
	TIM_CCxChannelCmd( VC_PWMTimer2Handle.Instance, VC_PWMTimer2ChannelA, TIM_CCxN_ENABLE );
	TIM_CCxChannelCmd( VC_PWMTimer2Handle.Instance, VC_PWMTimer2ChannelB, TIM_CCxN_ENABLE );

	// *** Init GPIO

	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_LOW;

	// GPIO for TIM1
	GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1;
	GPIO_InitStruct.Alternate = GPIO_AF1_TIM1;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = GPIO_PIN_11|GPIO_PIN_13;
	GPIO_InitStruct.Alternate = GPIO_AF1_TIM1;
	HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

	// GPIO for TIM8
	GPIO_InitStruct.Pin = GPIO_PIN_5;
	GPIO_InitStruct.Alternate = GPIO_AF3_TIM8;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = GPIO_PIN_15;
	GPIO_InitStruct.Alternate = GPIO_AF3_TIM8;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_6|GPIO_PIN_8;
    GPIO_InitStruct.Alternate = GPIO_AF3_TIM8;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	// *** Start timers

	HAL_TIM_PWM_Start( &VC_PWMTimer1Handle, VC_PWMTimer1ChannelA );
	HAL_TIM_PWM_Start( &VC_PWMTimer1Handle, VC_PWMTimer1ChannelB );

	HAL_TIM_PWM_Start( &VC_PWMTimer2Handle, VC_PWMTimer2ChannelA );
	HAL_TIM_PWM_Start( &VC_PWMTimer2Handle, VC_PWMTimer2ChannelB );
}



void InterruptTimerCallback( TIMObjectType *pObject, int nCallbackID )
{
	if( __HAL_TIM_GET_IT_SOURCE( &VC_InterruptTimerHandle, TIM_IT_UPDATE ) !=RESET )		// check for interrupt request activity
	{
		uint nT1ChannelACurValue = 0;
		uint nT1ChannelBCurValue = 0;
		uint nT2ChannelACurValue = 0;
		uint nT2ChannelBCurValue = 0;

		if ( VC_CurWavePeriod > 0 )		// if the wave is not off
		{
			// Check if actual period was zero and reset phase then
			if ( VC_CurWavePeriod_Actual == 0 )
			{
				VC_CurWavePeriod_Actual = VC_CurWavePeriod;
				__HAL_TIM_SET_COUNTER( &VC_CounterTimerHandle, 0 );
			}

			// Get counter timer values
			uint nFullPhaseTicksCount = VC_CurWavePeriod_Actual * (VC_CounterTimer_BaseFreq / VC_CounterTimer_Prescaler) / 1000;	// amount of ticks for whole period of output wave
			uint nCurCounterValue = __HAL_TIM_GET_COUNTER( &VC_CounterTimerHandle );

			// Check for phase's end
			if ( nCurCounterValue >= nFullPhaseTicksCount )
			{
				// Shift the counter timer for one phase period back
				nCurCounterValue %= nFullPhaseTicksCount;
				__HAL_TIM_SET_COUNTER( &VC_CounterTimerHandle, nCurCounterValue );

				VC_CurWavePeriod_Actual = VC_CurWavePeriod;	// update used period at phase start

				nFullPhaseTicksCount = VC_CurWavePeriod_Actual * (VC_CounterTimer_BaseFreq / VC_CounterTimer_Prescaler) / 1000;	// amount of ticks for whole period of output wave
			}

			// Calc current wave voltage value
			float32_t fCurPhase = 2 * PI * nCurCounterValue / nFullPhaseTicksCount;

			float32_t fSinValue1 = arm_sin_f32( fCurPhase ) * (1 + VC_PhaseVoltageCompensation);
			float32_t fSinValue2 = arm_sin_f32( fCurPhase - 2 * PI * VC_WavesPhaseShiftPercent / 100 ) * (1 - VC_PhaseVoltageCompensation);

			if ( fSinValue1 > 1 )	fSinValue1 = 1;
			if ( fSinValue1 < -1 )	fSinValue1 = -1;
			if ( fSinValue2 > 1 )	fSinValue2 = 1;
			if ( fSinValue2 < -11 )	fSinValue2 = -1;

			nT1ChannelACurValue = (fSinValue1 > 0) ? (uint)(VC_PWMTimersPeriod * fSinValue1) : 0;
			nT1ChannelBCurValue = (fSinValue1 < 0) ? (uint)(VC_PWMTimersPeriod * (-fSinValue1)) : 0;

			nT2ChannelACurValue = (fSinValue2 > 0) ? (uint)(VC_PWMTimersPeriod * fSinValue2) : 0;
			nT2ChannelBCurValue = (fSinValue2 < 0) ? (uint)(VC_PWMTimersPeriod * (-fSinValue2)) : 0;

			// Implement V/F logic
			nT1ChannelACurValue = nT1ChannelACurValue * VC_WavePeriodNominal / VC_CurWavePeriod_Actual;
			nT1ChannelBCurValue = nT1ChannelBCurValue * VC_WavePeriodNominal / VC_CurWavePeriod_Actual;

			nT2ChannelACurValue = nT2ChannelACurValue * VC_WavePeriodNominal / VC_CurWavePeriod_Actual;
			nT2ChannelBCurValue = nT2ChannelBCurValue * VC_WavePeriodNominal / VC_CurWavePeriod_Actual;

			// Avoid not-significant values
			const uint MinSignificantVal = VC_PWMTimersPeriod * 4 / 100;
			if ( nT1ChannelACurValue < MinSignificantVal )	nT1ChannelACurValue = 0;
			if ( nT1ChannelBCurValue < MinSignificantVal )	nT1ChannelBCurValue = 0;
			if ( nT2ChannelACurValue < MinSignificantVal )	nT2ChannelACurValue = 0;
			if ( nT2ChannelBCurValue < MinSignificantVal )	nT2ChannelBCurValue = 0;
		}
		else
		{
			VC_CurWavePeriod_Actual = 0;
		}

		// Update PWM timers
		__HAL_TIM_SET_COMPARE( &VC_PWMTimer1Handle, VC_PWMTimer1ChannelA, nT1ChannelACurValue );
		__HAL_TIM_SET_COMPARE( &VC_PWMTimer1Handle, VC_PWMTimer1ChannelB, nT1ChannelBCurValue );

		__HAL_TIM_SET_COMPARE( &VC_PWMTimer2Handle, VC_PWMTimer2ChannelA, nT2ChannelACurValue );
		__HAL_TIM_SET_COMPARE( &VC_PWMTimer2Handle, VC_PWMTimer2ChannelB, nT2ChannelBCurValue );

		// Clear interrupt request flag to enable following interrupt requests
		__HAL_TIM_CLEAR_IT( &VC_InterruptTimerHandle, TIM_IT_UPDATE );
	}
}


