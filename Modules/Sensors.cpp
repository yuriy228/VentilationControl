// Sensors.cpp
///////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "Sensors.h"
#include "Communication.h"

#include "cmath"



///////////////////////////////////////////////////////////////////////////////
// TriggerSensorType class implementation

TriggerSensorType::TriggerSensorType( const GPIOPort &port, int nIRQPriority )
	: SensorsTmpl( this ), m_GPIOPort( port ), m_nIRQPriority( nIRQPriority )
{
}



void TriggerSensorType::init()		// static function
{
	for ( int n = 0; n < m_nSensorsListSize; n++ )
	{
		TriggerSensorType &sensor = *m_arrSensors[n];

		sensor.m_GPIOPort.nMode = GPIO_MODE_IT_RISING_FALLING;
		sensor.m_GPIOPort.Init();

		IRQn_Type nIRQ = GetExtiIrqByGPIOPin( sensor.m_GPIOPort.nPin );

	    HAL_NVIC_SetPriority( nIRQ, sensor.m_nIRQPriority, 0 );
	    HAL_NVIC_EnableIRQ( nIRQ );
	}
}



void TriggerSensorType::IRQHandler()		// static function
{
	for ( int n = 0; n < m_nSensorsListSize; n++ )
	{
		TriggerSensorType &sensor = *m_arrSensors[n];

		// Check for port's interrupt request
		if( __HAL_GPIO_EXTI_GET_IT( sensor.m_GPIOPort.nPin ) == RESET )
			continue;

		// Reset interrupt request flag
		__HAL_GPIO_EXTI_CLEAR_IT( sensor.m_GPIOPort.nPin );

		// Get current port value
		float fCurVal = HAL_GPIO_ReadPin( sensor.m_GPIOPort.hGPIO, sensor.m_GPIOPort.nPin );

		// Update value
		sensor.SetSensorValue( fCurVal );
	}
}


// EXTI IRQ handlers
//
extern "C" void EXTI0_IRQHandler() 		{ TriggerSensorType::IRQHandler(); }
extern "C" void EXTI1_IRQHandler() 		{ TriggerSensorType::IRQHandler(); }
extern "C" void EXTI2_IRQHandler() 		{ TriggerSensorType::IRQHandler(); }
extern "C" void EXTI3_IRQHandler() 		{ TriggerSensorType::IRQHandler(); }
extern "C" void EXTI4_IRQHandler() 		{ TriggerSensorType::IRQHandler(); }
extern "C" void EXTI9_5_IRQHandler()	{ TriggerSensorType::IRQHandler(); }
extern "C" void EXTI15_10_IRQHandler()	{ TriggerSensorType::IRQHandler(); }





///////////////////////////////////////////////////////////////////////////////
// PulseSensorType class implementation


PulseSensorType::PulseSensorType( const GPIOPort &port, TIM_TypeDef* TIMx, int nIRQPriority, int nMaxPeriodMilliseconds, float fAveragingCoeff /* = 1 */ )
	: SensorsTmpl( this ), m_GPIOPort( port ), m_TIMObject( TIMx ), m_nIRQPriority( nIRQPriority ), m_nMaxPeriodMilliseconds( nMaxPeriodMilliseconds ), m_fAveragingCoeff( fAveragingCoeff )
{
}



void PulseSensorType::Init()		// static functions
{
	for ( int n = 0; n < m_nSensorsListSize; n++ )
	{
		PulseSensorType &sensor = *m_arrSensors[n];

		// Init TIM
		sensor.m_TIMObject.SetListener( &sensor );

		sensor.m_TIMObject.CLKEnable();

		sensor.m_TIMObject.Init.Prescaler = sensor.m_nMaxPeriodMilliseconds * (sensor.m_TIMObject.GetFrequency() >> 16) / 1000 * 2;		// MaxPeriodMilliseconds / 1000 / (1 / freq * 0xFFFF) * 2

		sensor.m_TIMObject.Init.CounterMode = TIM_COUNTERMODE_UP;
		sensor.m_TIMObject.Init.Period = 0xFFFF;
		sensor.m_TIMObject.Init.ClockDivision = TIM_CLOCKDIVISION_DIV4;	// divider for input filters (DTS period)
		HAL_TIM_Base_Init( &sensor.m_TIMObject );

		TIM_ClockConfigTypeDef sClockSourceConfig;
		sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
		HAL_TIM_ConfigClockSource( &sensor.m_TIMObject, &sClockSourceConfig );

		HAL_TIM_IC_Init( &sensor.m_TIMObject );

		TIM_SlaveConfigTypeDef sSlaveConfig;
		sSlaveConfig.SlaveMode = TIM_SLAVEMODE_RESET; //TIM_SLAVEMODE_TRIGGER
		sSlaveConfig.InputTrigger = TIM_TS_TI1FP1;
		sSlaveConfig.TriggerPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
		sSlaveConfig.TriggerFilter = 15;		// see TIMx_CCMR1 -> IC1F bits description
		HAL_TIM_SlaveConfigSynchronization( &sensor.m_TIMObject, &sSlaveConfig );

		TIM_IC_InitTypeDef sConfigIC;
		sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
		sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
		sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
		sConfigIC.ICFilter = 15;
		HAL_TIM_IC_ConfigChannel( &sensor.m_TIMObject, &sConfigIC, TIM_CHANNEL_1 );

		sensor.m_TIMObject.SetIRQPriority( sensor.m_nIRQPriority );

		// Init GPIO
		sensor.m_GPIOPort.Init( GPIO_MODE_AF_PP, sensor.m_GPIOPort.nPull, sensor.m_TIMObject.GetGPIOAltFuncID() );

		// Calc sensor value multiplier
		sensor.m_fValueMultiplier = (float)sensor.m_TIMObject.Init.Prescaler / (sensor.m_TIMObject.GetFrequency() / 1000000);

		// Start TIM
		HAL_TIM_IC_Start_IT( &sensor.m_TIMObject, TIM_CHANNEL_1 );
	}
}



// Callback from TIM object
//
void PulseSensorType::OnCallback( uint nCallbackID )
{
	if ( nCallbackID != TCT_IC_CaptureCallback )
		return;

	// Get value
	uint nValInClock = __HAL_TIM_GET_COMPARE( &m_TIMObject, TIM_CHANNEL_1 );

	// Convert to microseconds
	float fVal = nValInClock * m_fValueMultiplier;	// = nValInClock * (1 / (sensor.m_TIMObject.CLKEnable() / sensor.m_TIMObject.Init.Prescaler)) * 1000000

	// Make averaging
	fVal = m_fSensorValue * (1 - m_fAveragingCoeff) + fVal * m_fAveragingCoeff;

	// Update
	SetSensorValue( fVal );
}








///////////////////////////////////////////////////////////////////////////////
// AirSpeedSensorType class implementation


AirSpeedSensorType::AirSpeedSensorType( const GPIOPort& portStartMeasure, const GPIOPort& portTIM_CH1, const GPIOPort& portTIM_CH2, TIM_TypeDef* TIMx, int nIRQPriority,
		float fDistanceBetweenSensors, uint nMeasurmentPeriod, int nMedianFilterResolution, float fAveragingCoeff )
	: SensorsTmpl( this ),
	m_GPIOPort_StartMeasure( portStartMeasure ),
	m_GPIOPort_TIM_CH1( portTIM_CH1 ),
	m_GPIOPort_TIM_CH2( portTIM_CH2 ),
	m_TIMObject( TIMx )
{
	m_nIRQPriority = nIRQPriority;

	m_fDistanceBetweenSensors = fDistanceBetweenSensors;
	m_nMeasurmentPeriod = nMeasurmentPeriod;

	m_ResultValueFilter.SetResolution( nMedianFilterResolution );
	m_fAveragingCoeff = fAveragingCoeff;

	m_nLastStartTicks = 0;
}



void AirSpeedSensorType::Init()		// static function
{
	for ( int n = 0; n < m_nSensorsListSize; n++ )
	{
		AirSpeedSensorType &sensor = *m_arrSensors[n];

		// Calc prescaler
		float fAveragePeriod = sensor.m_fDistanceBetweenSensors / AverageSoundSpeed;		// in seconds
		int nPrescaler = int( fAveragePeriod * (sensor.m_TIMObject.GetFrequency() >> 16) * 1.5 );

		// Init TIM
		sensor.m_TIMObject.SetListener( &sensor );

		sensor.m_TIMObject.CLKEnable();

		sensor.m_TIMObject.Init.Prescaler = nPrescaler;
		sensor.m_TIMObject.Init.CounterMode = TIM_COUNTERMODE_UP;
		sensor.m_TIMObject.Init.Period = 0xFFFF;
		sensor.m_TIMObject.Init.ClockDivision = TIM_CLOCKDIVISION_DIV4; 	// divider for input filters (DTS period)
		HAL_TIM_Base_Init( &sensor.m_TIMObject );

		TIM_ClockConfigTypeDef sClockSourceConfig;
		sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
		HAL_TIM_ConfigClockSource( &sensor.m_TIMObject, &sClockSourceConfig );

		HAL_TIM_IC_Init( &sensor.m_TIMObject );

		sensor.m_TIMObject.Instance->CR1 |= TIM_OPMODE_SINGLE;		// single mode

		TIM_SlaveConfigTypeDef sSlaveConfig;
		sSlaveConfig.SlaveMode = TIM_SLAVEMODE_TRIGGER; //TIM_SLAVEMODE_RESET; //TIM_SLAVEMODE_TRIGGER
		sSlaveConfig.InputTrigger = TIM_TS_TI1F_ED;
		sSlaveConfig.TriggerPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
		sSlaveConfig.TriggerFilter = 0;									//12;	??  // see TIMx_CCMR1->IC1F bits description
		HAL_TIM_SlaveConfigSynchronization( &sensor.m_TIMObject, &sSlaveConfig );

		sensor.m_TIMObject.SetIRQPriority( sensor.m_nIRQPriority );

		// Init TIM channels
		TIM_IC_InitTypeDef sConfigIC;
		sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING; 	// TIM_INPUTCHANNELPOLARITY_FALLING; // TIM_INPUTCHANNELPOLARITY_BOTHEDGE; //TIM_INPUTCHANNELPOLARITY_RISING;
		sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
		sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
		sConfigIC.ICFilter = 0;						//12;	// ??
		HAL_TIM_IC_ConfigChannel( &sensor.m_TIMObject, &sConfigIC, TIM_CHANNEL_1 );
		HAL_TIM_IC_ConfigChannel( &sensor.m_TIMObject, &sConfigIC, TIM_CHANNEL_2 );

		// Init TIM GPIO
		sensor.m_GPIOPort_TIM_CH1.Init( GPIO_MODE_AF_PP, sensor.m_GPIOPort_TIM_CH1.nPull, sensor.m_TIMObject.GetGPIOAltFuncID() );
		sensor.m_GPIOPort_TIM_CH2.Init( GPIO_MODE_AF_PP, sensor.m_GPIOPort_TIM_CH2.nPull, sensor.m_TIMObject.GetGPIOAltFuncID() );

		// Init measurement start GPIO
		sensor.m_GPIOPort_StartMeasure.Init( GPIO_MODE_OUTPUT_PP, sensor.m_GPIOPort_StartMeasure.nPull );
		sensor.m_GPIOPort_StartMeasure.SetPin( false );
	}
}



void AirSpeedSensorType::LoopStep()
{
	for ( int n = 0; n < m_nSensorsListSize; n++ )
	{
		AirSpeedSensorType &sensor = *m_arrSensors[n];

		// Start measurement if needed
		if ( HAL_GetTick() - sensor.m_nLastStartTicks >= sensor.m_nMeasurmentPeriod )
		{
			// Set measuring mode values
			sensor.m_bIsMeasuring = true;
			sensor.m_enTIMCapturedChannel1 = sensor.m_enTIMCapturedChannel2 = CAPTURED_NON;
			sensor.m_nLastStartTicks = HAL_GetTick();

			// Start timer channels
			HAL_TIM_IC_Start_IT( &sensor.m_TIMObject, TIM_CHANNEL_1 );	// NOTE, it starts counter
			HAL_TIM_IC_Start_IT( &sensor.m_TIMObject, TIM_CHANNEL_2 );

			// Stop and reset counter, since it should wait for rising edge
			sensor.m_TIMObject.Instance->CR1 &= ~(TIM_CR1_CEN);		// instead of __HAL_TIM_DISABLE() because of HAL bug
			__HAL_TIM_SET_COUNTER( &sensor.m_TIMObject, 0 );

			// Send start pulse (10 mkS minimum by specs)
			sensor.m_GPIOPort_StartMeasure.SetPin( true );

			int i;
			for ( i = 0; i < 40; i++ )	// 1000 steps = 75 us; 10 steps is workable minimum; 10 us is minimum by specs
				__ASM volatile ("nop");

			sensor.m_GPIOPort_StartMeasure.SetPin( false );
		}
	}
}



// Callback from TIM object
//
void AirSpeedSensorType::OnCallback( uint nCallbackID )
{
	if ( unlikely( !m_bIsMeasuring ) )
		return;

	// CAPTURE event
	if ( likely( nCallbackID == TCT_IC_CaptureCallback ) )
	{
		// Process channel 1
		if ( likely( m_TIMObject.Channel & HAL_TIM_ACTIVE_CHANNEL_1 ) )
		{
			auto nValInClock = __HAL_TIM_GET_COMPARE( &m_TIMObject, TIM_CHANNEL_1 );

			if ( likely( m_enTIMCapturedChannel1 == CAPTURED_RISE ) )		// should be first for optimisation
			{
				m_nChannel1ValueFall = nValInClock;
				m_enTIMCapturedChannel1 = CAPTURED_FALL;
			}
			else if ( m_enTIMCapturedChannel1 == CAPTURED_NON )
			{
				m_nChannel1ValueRise = nValInClock;
				m_enTIMCapturedChannel1 = CAPTURED_RISE;
			}
		}

		// Process channel 2
		if ( likely( m_TIMObject.Channel & HAL_TIM_ACTIVE_CHANNEL_2 ) )
		{
			auto nValInClock = __HAL_TIM_GET_COMPARE( &m_TIMObject, TIM_CHANNEL_2 );

			if ( likely( m_enTIMCapturedChannel2 == CAPTURED_RISE ) )		// should be first for optimisation
			{
				m_nChannel2ValueFall = nValInClock;
				m_enTIMCapturedChannel2 = CAPTURED_FALL;
			}
			else if ( m_enTIMCapturedChannel2 == CAPTURED_NON )
			{
				m_nChannel2ValueRise = nValInClock;
				m_enTIMCapturedChannel2 = CAPTURED_RISE;
			}
		}
	}

	// Check for measurement process complete
	if ( unlikely( m_enTIMCapturedChannel1 == CAPTURED_FALL && m_enTIMCapturedChannel2 == CAPTURED_FALL ) )
	{
		// Stop timer channels
		HAL_TIM_IC_Stop_IT( &m_TIMObject, TIM_CHANNEL_1 );
		HAL_TIM_IC_Stop_IT( &m_TIMObject, TIM_CHANNEL_2 );

		// Stop timer counter
		m_TIMObject.Instance->CR1 &= ~(TIM_CR1_CEN);	// instead of __HAL_TIM_DISABLE() because of HAL bug

		// Stop measuring mode
		m_bIsMeasuring = false;

		// Check for timeout
		uint nTimeout = ceil( m_fDistanceBetweenSensors / AverageSoundSpeed * 1000 * 1.5);		// (( increase by 1 because the period value is too small (about 4 ms) ))
		if ( nCallbackID == TCT_PeriodElapsedCallback || HAL_GetTick() - m_nLastStartTicks > nTimeout )
		{
			return;
		}

		// Consider PWM pulses start shift
		float fChannel1Value = m_nChannel1ValueFall - m_nChannel2ValueRise * 0.6;	// sensor 1 gets wave from sensor 2, so we consider wave start sift from sensor 2 here
		float fChannel2Value = m_nChannel2ValueFall - m_nChannel1ValueRise * 0.6;

		// Calculate sensor value
		float fOneTickPeriod = (float)m_TIMObject.Init.Prescaler / m_TIMObject.GetFrequency();	 	// period of one timer tick in seconds

		float fValue = m_fDistanceBetweenSensors * (1.0 / (fChannel2Value * fOneTickPeriod) - 1.0 / (fChannel1Value * fOneTickPeriod)) / 2;

		// Do meidan filterring
		m_ResultValueFilter.AddValue( fValue );
		fValue = m_ResultValueFilter.GetValue();

		// Do averaging
		float fCoeff = m_fAveragingCoeff * (HAL_GetTick() - GetLastUpdateTime()) / 1000;
		fCoeff = MIN( 1, fCoeff );
		fValue = GetSensorValue() * (1 - fCoeff) + fValue * fCoeff;

		// Update sensor value
		SetSensorValue( fValue );
	}
}






///////////////////////////////////////////////////////////////////////////////
// AnalogSensorType class implementation


// Static objects
ADC_HandleTypeDef AnalogSensorType::m_hADC;
DMA_HandleTypeDef AnalogSensorType::m_hDMA;
efcArray< uint16 > AnalogSensorType::m_arrRoughValuesBuffer;


AnalogSensorType::AnalogSensorType( const GPIOPort &port, uint ADCChannel, float fValueShift, float fValueMultiplier )
	: SensorsTmpl( this ), m_GPIOPort( port ), m_ADCChannel( ADCChannel )
{
	m_fValueShift = fValueShift;
	m_fValueMultiplier = fValueMultiplier;
}


AnalogSensorType::AnalogSensorType( const GPIOPort &port, uint ADCChannel, float fV1, float fT1, float fV2, float fT2 )
	: SensorsTmpl( this ), m_GPIOPort( port ), m_ADCChannel( ADCChannel )
{
	float fCoeff = (fT1 - fT2) / (fV1 - fV2);

	// T = (V  + T1 / fCoeff - V1) * fCoeff
	m_fValueShift = fT1 / fCoeff - fV1;
	m_fValueMultiplier = fCoeff;
}



void AnalogSensorType::Init()		// static functions
{
	// *** Init DMA interrupt
	__DMA2_CLK_ENABLE();

	HAL_NVIC_SetPriority( Sensors_AnalogSensors_DMAIRQn, Sensors_AnalogSensors_DMAInterruptPriority, 0 );
	HAL_NVIC_EnableIRQ( Sensors_AnalogSensors_DMAIRQn );

	// DMA init
	m_hDMA.Instance = Sensors_AnalogSensors_DMAHandle;
	m_hDMA.Init.Channel = Sensors_AnalogSensors_DMAChannel;

	m_hDMA.Init.Direction = DMA_PERIPH_TO_MEMORY;
	m_hDMA.Init.PeriphInc = DMA_PINC_DISABLE;
	m_hDMA.Init.MemInc = DMA_MINC_ENABLE;
	m_hDMA.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
	m_hDMA.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
	m_hDMA.Init.Mode = DMA_NORMAL;
	m_hDMA.Init.Priority = DMA_PRIORITY_HIGH;
	m_hDMA.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
	HAL_DMA_Init( &m_hDMA );

	__HAL_LINKDMA( &m_hADC, DMA_Handle, m_hDMA );

	// ADC GPIO Configuration
	for ( int n = 0; n < m_nSensorsListSize; n++ )
	{
		AnalogSensorType &sensor = *m_arrSensors[n];

		sensor.m_GPIOPort.Init( GPIO_MODE_ANALOG, sensor.m_GPIOPort.nPull );
	}

	//
	ADCObjectType tmpADC( Sensors_AnalogSensors_ADCHandle );	// (the class is not complete)
	tmpADC.CLKEnable();

	m_hADC.Instance = Sensors_AnalogSensors_ADCHandle;
	m_hADC.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV8;		// base clock is 84 MHz
	m_hADC.Init.Resolution = ADCResolution;
	m_hADC.Init.ScanConvMode = ENABLE;
	m_hADC.Init.ContinuousConvMode = ENABLE;
	m_hADC.Init.NbrOfConversion = m_nSensorsListSize;
	m_hADC.Init.DMAContinuousRequests = ENABLE;

	m_hADC.Init.DiscontinuousConvMode = DISABLE;
	m_hADC.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
	m_hADC.Init.DataAlign = ADC_DATAALIGN_RIGHT;
	m_hADC.Init.EOCSelection = ADC_EOC_SEQ_CONV; //EOC_SINGLE_CONV;
	HAL_ADC_Init( &m_hADC );

	for ( int n = 0; n < m_nSensorsListSize; n++ )
	{
		ADC_ChannelConfTypeDef sConfig;
		sConfig.Channel = m_arrSensors[n]->m_ADCChannel;
		sConfig.Rank = n + 1;
		sConfig.SamplingTime = ADC_SAMPLETIME_144CYCLES;
		HAL_ADC_ConfigChannel( &m_hADC, &sConfig );
	}

	// Prepare rough values buffer
	m_arrRoughValuesBuffer.SetSize( ApproximationConversionsCount * m_nSensorsListSize );

	// Start ADC reading process
	HAL_ADC_Start_DMA( &m_hADC, (uint32_t*)m_arrRoughValuesBuffer.GetData(), m_arrRoughValuesBuffer.GetSize() );

}



extern "C" void DMA2_Stream0_IRQHandler(void)
{
	HAL_DMA_IRQHandler( &AnalogSensorType::m_hDMA );
}



void HAL_ADC_ConvCpltCallback( ADC_HandleTypeDef* hadc )
{
	if ( hadc->Instance != AnalogSensorType::m_hADC.Instance )
		return;

	// Get approximate conversion values
	const int nSensorsCount = AnalogSensorType::m_nSensorsListSize;
	efcArray< float > arr( nSensorsCount );

	for ( int nSensorInd = 0; nSensorInd < nSensorsCount; nSensorInd++ )
	{
		// Get preliminary average value
		for ( int n = nSensorInd; n < AnalogSensorType::m_arrRoughValuesBuffer.GetSize(); n += nSensorsCount )
		{
			arr[nSensorInd] += AnalogSensorType::m_arrRoughValuesBuffer[n];
		}

		int nValuesCount = AnalogSensorType::ApproximationConversionsCount;
		int nRoughAverage = (int)(arr[nSensorInd] / nValuesCount);

		// Exclude values that much differs from average
		const int MaxDiff = (1 << 12) / 100;
		const int MinVal = nRoughAverage - MaxDiff;
		const int MaxVal = nRoughAverage + MaxDiff;

		for ( int n = nSensorInd; n < AnalogSensorType::m_arrRoughValuesBuffer.GetSize(); n += nSensorsCount )
		{
			int nVal = AnalogSensorType::m_arrRoughValuesBuffer[n];
			if ( nVal < MinVal || nVal > MaxVal )
			{
				arr[nSensorInd] -= nVal;
				nValuesCount--;
			}
		}

		// Get final average
		arr[nSensorInd] /= nValuesCount;
	}

	// Calc and update values
	for ( int nSensorInd = 0; nSensorInd < nSensorsCount; nSensorInd++ )
	{
		AnalogSensorType &sensor = *AnalogSensorType::m_arrSensors[nSensorInd];

		const float fOneStepVoltage = Sensors_BaseADCVoltage / (1 << 12);

		float fValue = ( arr[nSensorInd] * fOneStepVoltage + sensor.m_fValueShift) * sensor.m_fValueMultiplier;

		// Update value
		sensor.SetSensorValue( fValue );


static uint t = HAL_GetTick();
if (  HAL_GetTick() > t + 1000 )
{
if ( nSensorInd == nSensorsCount - 1 )
	t = HAL_GetTick();

//if ( nSensorInd == 0 )
//Comm_SendDebugArray( AnalogSensorType::m_arrRoughValuesBuffer.GetData(), AnalogSensorType::m_arrRoughValuesBuffer.GetSize(), 2, 2 * nSensorsCount );

//Comm_SendDebugString( "Sensor: %d  ROUGH: %d  VOLTAGE: %d	RESULT: %d", nSensorInd, (int)arr[nSensorInd], (int)(arr[nSensorInd] * fOneStepVoltage * 1000), (int)(fValue * 100) );
}
/**/

	}

	// Restart conversion
	HAL_ADC_Stop_DMA( &AnalogSensorType::m_hADC );
	HAL_ADC_Start_DMA( &AnalogSensorType::m_hADC, (uint32_t*)AnalogSensorType::m_arrRoughValuesBuffer.GetData(), AnalogSensorType::m_arrRoughValuesBuffer.GetSize() );
}



void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc)
{
	if ( hadc->Instance != AnalogSensorType::m_hADC.Instance )
		return;

	Comm_SendString( "Error callback: HAL_ADC_ErrorCallback", COMM_POSTMSG_ERR );

	// Restart conversion
	HAL_ADC_Stop_DMA( &AnalogSensorType::m_hADC );
	HAL_ADC_Start_DMA( &AnalogSensorType::m_hADC, (uint32_t*)AnalogSensorType::m_arrRoughValuesBuffer.GetData(), AnalogSensorType::m_arrRoughValuesBuffer.GetSize() );
}



