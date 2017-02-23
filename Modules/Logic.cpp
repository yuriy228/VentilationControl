// Logic.cpp
///////////////////////////////////////////////////////////////////////////////////////////////


#include "main.h"
#include "Logic.h"
#include "Communication.h"
#include "Sensors.h"
#include "VentilatorControl.h"
#include "Heater.h"


// *** Sensors

AirSpeedSensorType Logic_StreamSpeedSensor( Logic_SreamSpeedSensor_StartGPIO, Logic_SreamSpeedSensor_TimerChannel1GPIO, Logic_SreamSpeedSensor_TimerChannel2GPIO,
											Logic_SreamSpeedSensor_TimerHandle, Logic_SreamSpeedSensor_InterruptPriority,
											0.875,		// fDistanceBetweenSensors, meters
											20, 		// nMeasurmentPeriod, millesec
											50,			// nMedianFilterResolution
											2 );		// fAveragingCoeff
const float Logic_SreamSpeedSensor_SectionalArea = 0.02205;		// area of cut of tube; square meters; (73 mm x 302 mm)
const int Logic_SreamSpeedSensor_FlowMinValue = 10;				// minimal significant flow speed; m^3/h
const int Logic_SreamSpeedSensor_MaxTimeout	= 2000;				// max timeout for last sensor value; if timeout occurs, the flow value is counted as zero; milliseconds


PulseSensorType Logic_SreamSpeedSensorAux( Logic_SreamSpeedSensorAux_GPIO, Logic_SreamSpeedSensorAux_TimerHandle, Logic_SreamSpeedSensorAux_InterruptPriority, 1000, 1 );
const int Logic_SreamSpeedSensorAux_NormalFlow = 1100;			// flow speed for 100% motor speed; M^3/hours
const int Logic_SreamSpeedSensorAux_NormalPeriod = 6900;		// sensor period for 100% motor speed; microseconds
const int Logic_SreamSpeedSensorAux_MaximalPeriod = 2000000;	// minimal flow speed significant period of the sensor; microseconds	(400000 allows to sense of 18 m3/h minimal flow)
const int Logic_SreamSpeedSensorAux_MaxTimeout	= 3000;			// max timeout for last sensor value; if timeout occurs, the flow value is counted as zero; milliseconds
// CurFlow = NormalFlow / (CurPeriod / NormalPeriod ) = NormalFlow * NormalPeriod / CurPeriod

AnalogSensorType Logic_HeaterTempSensor( Logic_HeaterTempSensorGPIO, ADC_CHANNEL_0, 0.01, 100 );	// TMP35GT9Z; temperature sensor just after the heater
const int Logic_HeaterTempSensor_MaxTemp = 80;
const int Logic_HeaterTempSensor_MaxFaultTemp = 90; // (!!!)

AnalogSensorType Logic_StreamTempSensor( Logic_StreamTempSensorGPIO, ADC_CHANNEL_10, 2.272, -0.9, 2.597, 51.5 );		// lm135z; temperature of the flow
const int Logic_StreamTempSensor_MaxFaultTemp = 60;

AnalogSensorType Logic_VentilatorTempSensor( Logic_VentilatorTempSensorGPIO, ADC_CHANNEL_1, 2.078, 4.5, 2.306, 33.7 );  // lm335z; temperature sensor inside the ventilator motor
const int Logic_VentilatorTempSensor_MaxFaultTemp = 90;

TriggerSensorType Logic_VentilatorMotorDriverFaultState( VentilatorControl_MotorDriverFaultInputGPIO, Logic_ErrorTriggerInterruptPriority );		// = 0 on error; motor overcurrent and motor temperature fuse state


// *** PID regulators

PIDRegulator Logic_VentilatorSpeedRegulator(	// input: flow speed in m3/h, output: percents of ventilator speed
									0,		// proportional coefficient (zero, since we need slowly speed change to prevent actuation of motor protection)
									0.1,	// integral coefficient
									0,		// differential coefficient
									-50,	// input value min
									+50,	// input value max
									20,		// output value min
									140 );	// output value max; maximum allowed speed for ventilator motor (with overspeeding)

PIDRegulator Logic_StreamTempRegulator(	// input: stream temperature, output: percents of heater power
									0.007,	// proportional coefficient; ~= 0.34 * 100 (%) / 6000 (wt); will be multiplied by flow speed as additional proportion coefficient
									0.06,	// integral coefficient
									6,		// differential coefficient
									-20,	// input value min
									+20,	// input value max
									0,		// output value min
									100  );	// output value max

PIDRegulator Logic_HeaterLimitTempRegulator(	// input: heater temperature and limit goal temperature, output: negative percents of heater power; used to decrease percent after the main regulator if heater temperature crosses limit level
									3,		// proportional coefficient; ~= 0.34 * 100 (%) / 6000 (wt); will be multiplied by flow speed as additional proportion coefficient
									0.2,	// integral coefficient
									20,		// differential coefficient
									-40,	// input value min
									+20,	// input value max
									-100,	// output value min
									0 );	// output value max


bool 		Logic_GeneralError = FALSE;
uint 		Logic_VentilatorStopRequestTime = 0;	// after general fault keeps ticks when the ventilator should be stopped; is zero if no need to stop
const char* Logic_GeneralErrorString = NULL;

int INT_NOT_SET = 0xFFFFFFFF;

int Logic_StreamSpeedGoal = 20;		// m3/h
int Logic_StreamTempGoal = 32;		// celsius

int Logic_ManualMode_HeaterPowerPercent = INT_NOT_SET;
int Logic_ManualMode_VentilatorSpeedPercent = INT_NOT_SET;



// Skip protection defines
#define ENABLE_PROTECTION_HeaterFaultTemp			1
#define ENABLE_PROTECTION_VentilatorTempFaultTemp	1
#define ENABLE_PROTECTION_StreemSpeedSensorFault	1
#define ENABLE_PROTECTION_FlowZerroSpeedFault		1
#define ENABLE_PROTECTION_StreemFaultTemp			1
#define ENABLE_PROTECTION_VetilatorFaultState		1

#define ENABLE_DEBUG_Log	1


ADC_HandleTypeDef g_TmpADC = {0};
void Tmp_InitADC();


// Predeclarations
void TriggerHandler_VentilatorMotorDriverFaultState();




////////////////////////////////////////////////////////////////////////////////////////////
// Functions


void Logic_Init()
{
	Tmp_InitADC();

	Logic_VentilatorMotorDriverFaultState.SetCallback( TriggerHandler_VentilatorMotorDriverFaultState );
}




void Logic_MainLoopStep()
{
	// *** Ventilator regulation
/*
	uint nStreamSpeedSensorTime;
	float fFlowSpeed = 0;
	float fStreamSpeedSensorValue = Logic_StreamSpeedSensor.GetSensorValue( &nStreamSpeedSensorTime );	// in m/s
	if ( nStreamSpeedSensorTime < Logic_SreamSpeedSensor_MaxTimeout )
	{
		fFlowSpeed = fStreamSpeedSensorValue * Logic_SreamSpeedSensor_SectionalArea * 3600;		// in m^3/h

		if ( fFlowSpeed < Logic_SreamSpeedSensor_FlowMinValue )
			fFlowSpeed = 0;
	}
	/**/

	uint nStreamSpeedSensorAuxTime;
	float fFlowSpeedAux = 0;
	float fStreamSpeedSensorAuxValue = Logic_SreamSpeedSensorAux.GetSensorValue( &nStreamSpeedSensorAuxTime );
	if ( nStreamSpeedSensorAuxTime < Logic_SreamSpeedSensorAux_MaxTimeout
		&& fStreamSpeedSensorAuxValue > 0 && fStreamSpeedSensorAuxValue < Logic_SreamSpeedSensorAux_MaximalPeriod )
	{
		fFlowSpeedAux = Logic_SreamSpeedSensorAux_NormalFlow * Logic_SreamSpeedSensorAux_NormalPeriod / fStreamSpeedSensorAuxValue;
	}

	float fFlowSpeed = fFlowSpeedAux;	// DEBUG

	int nVentiltorSpeedPercent = (int)Logic_VentilatorSpeedRegulator.Calculate( fFlowSpeed, Logic_StreamSpeedGoal );

	// Use manual mode if set
	if ( Logic_ManualMode_VentilatorSpeedPercent != INT_NOT_SET )
		nVentiltorSpeedPercent = Logic_ManualMode_VentilatorSpeedPercent;


	// *** Stream temperature regulation

	float fStreamTempReal = Logic_StreamTempSensor.GetSensorValue();
	float fHeaterTempReal = Logic_HeaterTempSensor.GetSensorValue();

	Logic_StreamTempRegulator.SetAdditionalProportionCoeff( fFlowSpeed );	// set flow speed as an additional proportion value, since needed heater power depends on the flow speed

	// Main regulator
	int nHeaterPowerPercent = Logic_StreamTempRegulator.Calculate( fStreamTempReal, Logic_StreamTempGoal );

	// Compensation by heater limit regulator
	nHeaterPowerPercent += Logic_HeaterLimitTempRegulator.Calculate( fHeaterTempReal, Logic_HeaterTempSensor_MaxTemp );

	// Use manual mode if set
	if ( Logic_ManualMode_HeaterPowerPercent != INT_NOT_SET )
		nHeaterPowerPercent = Logic_ManualMode_HeaterPowerPercent;

	nHeaterPowerPercent = MIN( nHeaterPowerPercent, Logic_StreamTempRegulator.m_fResultMax );
	nHeaterPowerPercent = MAX( nHeaterPowerPercent, Logic_StreamTempRegulator.m_fResultMin );

	// Limit by flow zero speed and heater max temperature
	if ( ( fFlowSpeed == 0 || fFlowSpeedAux == 0 )
//			&& Logic_ManualMode_HeaterPowerPercent == INT_NOT_SET		// do not limit in manual mode
			)
	{
		// Reset heater power to zero
		Logic_StreamTempRegulator.SetIntegralPartValue( 0 );
		nHeaterPowerPercent = 0;
	}


	// *** General error processing

	if ( Logic_GeneralError )
	{
		// Check for ventilator stop request
		if ( Logic_VentilatorStopRequestTime != 0
			&& HAL_GetTick() - Logic_VentilatorStopRequestTime > 60*1000 )
		{
			// Stop the ventilator
			VentilatorControl_SetSpeed( 0 );

			Logic_VentilatorStopRequestTime = 0;
		}

		// Send log
		RUN_EACH_MILLISECONDS( 500, []
		{
			Comm_SendString( "General fault", COMM_POSTMSG_ERR );

			if ( Logic_GeneralErrorString )
				Comm_SendString( Logic_GeneralErrorString, COMM_POSTMSG_ERR );
		} );

		// Reset values
		if ( Logic_VentilatorStopRequestTime == 0 )
			nVentiltorSpeedPercent = 0;			// reset only if we do not have stop request period set
		nHeaterPowerPercent = 0;
	}


	// *** Set values to modules

	VentilatorControl_SetSpeed( nVentiltorSpeedPercent );
	Heater_SetPower( nHeaterPowerPercent );


	// *** Protection

	if ( !Logic_GeneralError )
	{
		// Heater fault temperature
		if ( ENABLE_PROTECTION_HeaterFaultTemp
			&& Logic_HeaterTempSensor.GetSensorValue() >= Logic_HeaterTempSensor_MaxFaultTemp )
		{
			Logic_GeneralError = TRUE;

			Heater_SetPower( 0 );

			Logic_VentilatorStopRequestTime = HAL_GetTick();	// turn off the ventilator after some timeout

			Logic_GeneralErrorString = "Fault heater temp reached";

			return;
		}

		// Ventilator motor fault temp
		if ( ENABLE_PROTECTION_VentilatorTempFaultTemp
			&& Logic_VentilatorTempSensor.GetSensorValue() >= Logic_VentilatorTempSensor_MaxFaultTemp )
		{
			Logic_GeneralError = TRUE;

			Heater_SetPower( 0 );
			VentilatorControl_SetSpeed( 0 );

			Logic_GeneralErrorString = "Fault ventilator motor temp reached";

			return;
		}

		// Stream speed sensor
		const uint Logic_StreamSpeedSensorTimeout = 60 * 1000;
		if ( ENABLE_PROTECTION_StreemSpeedSensorFault
			&& Logic_StreamSpeedGoal != 0
			&& Logic_ManualMode_VentilatorSpeedPercent != 0
			&& ( /* nStreamSpeedSensorTime > Logic_StreamSpeedSensorTimeout || */ nStreamSpeedSensorAuxTime > Logic_StreamSpeedSensorTimeout ) )
		{
			Logic_GeneralError = TRUE;

			Heater_SetPower( 0 );
			VentilatorControl_SetSpeed( 0 );

			Logic_GeneralErrorString = "Stream speed sensor fault";

			return;
		}

		// Zerro flow speed check
		const uint Logic_ZeroFlowSpeedTimeout = 60 * 1000;
		static uint nLastNotZeroFlowSpeedTicks = HAL_GetTick();

		if ( Logic_StreamSpeedGoal == 0
			|| Logic_ManualMode_VentilatorSpeedPercent == 0
			|| ( fFlowSpeed > 0 && fFlowSpeedAux > 0 ) )
		{
			nLastNotZeroFlowSpeedTicks = HAL_GetTick();
		}

		if ( ENABLE_PROTECTION_FlowZerroSpeedFault
			&& HAL_GetTick() - nLastNotZeroFlowSpeedTicks > Logic_ZeroFlowSpeedTimeout )
		{
			Logic_GeneralError = TRUE;

			Heater_SetPower( 0 );
			VentilatorControl_SetSpeed( 0 );

			Logic_GeneralErrorString = "Zero flow speed fault";

			return;
		}

		// Stream temperature sensor
		if ( ENABLE_PROTECTION_StreemFaultTemp
			&& Logic_StreamTempSensor.GetSensorValue() >= Logic_StreamTempSensor_MaxFaultTemp )
		{
			Logic_GeneralError = TRUE;

			Heater_SetPower( 0 );

			Logic_VentilatorStopRequestTime = HAL_GetTick();	// turn off the ventilator after some timeout

			Logic_GeneralErrorString = "Stream temp sensor fault";

			return;
		}
	}

#if ENABLE_DEBUG_Log
	// Debug output
	RUN_EACH_MILLISECONDS( 500, [&]
	{
		Comm_SendDebugString( "StreamSpd: %d (%d)  Motor%%: %d  StreamT: %d (%d)  HeatPwr: %d (%d %d)  HeaterT: %d  MotorT: %d",
			(int)fFlowSpeed, Logic_StreamSpeedGoal, nVentiltorSpeedPercent,
			(int)(fStreamTempReal * 100), Logic_StreamTempGoal, nHeaterPowerPercent, (int)Logic_StreamTempRegulator.Calculate( fStreamTempReal, Logic_StreamTempGoal ), (int)Logic_HeaterLimitTempRegulator.Calculate( fHeaterTempReal, Logic_HeaterTempSensor_MaxTemp ),
			(int)(Logic_HeaterTempSensor.GetSensorValue() * 100),
			(int)(Logic_VentilatorTempSensor.GetSensorValue() * 100) );
	} );
#endif

/*
{
	int nPercent = 200 * hADC.Instance->DR / (0x1 << 12);

	static float fAverage = 0;
	float fCoeff = 0.6;//0.07;
	if ( nPercent < fAverage - 5 ) nPercent = fAverage - 5;
	if ( nPercent > fAverage + 5 ) nPercent = fAverage + 5;
	fAverage = fAverage * (1 - fCoeff) + nPercent * fCoeff;
	nPercent = fAverage;

	//VentilatorControl_SetSpeed( nPercent  );
	//Heater_SetPower( nPercent );

	static uint t = HAL_GetTick();
	if (  HAL_GetTick() > t + 500 )
	{
	t = HAL_GetTick();

	//Comm_SendDebugString( "%d %%", nPercent );
	}
}
/**/


}




void Logic_ProcessControlCommand( const char* strCommand, const char* strValue /* = NULL */ )
{
	// SetFlowSpeed command
	if ( strcmp( strCommand, Logic_ControlCommand_SetFlowSpeed) == 0 )
	{
		if ( strValue == NULL )
			return;

		Logic_StreamSpeedGoal = atoi( strValue );
		Logic_ManualMode_VentilatorSpeedPercent = INT_NOT_SET;

		Comm_SendDebugString( "Flow speed is set to %d m3/h", Logic_StreamSpeedGoal );

		return;
	}

	// SetFlowTemp command
	if ( strcmp( strCommand, Logic_ControlCommand_SetFlowTemp) == 0 )
	{
		if ( strValue == NULL )
			return;

		Logic_StreamTempGoal = atoi( strValue );
		Logic_ManualMode_HeaterPowerPercent = INT_NOT_SET;

		Comm_SendDebugString( "Flow temperature is set to %d cels.", Logic_StreamTempGoal );

		return;
	}

	// SetVentilatorSpeedPercent command
	if ( strcmp( strCommand, Logic_ControlCommand_ManualMode_SetVentilatorSpeedPercent ) == 0 )
	{
		if ( strValue == NULL )
			return;

		int nPercent = atoi( strValue );

		Logic_ManualMode_VentilatorSpeedPercent = nPercent;
		VentilatorControl_SetSpeed( nPercent );

		Comm_SendDebugString( "Ventilator speed percent is set to %d %%. (MANUAL MODE)", nPercent );

		return;
	}

	// SetHeaterPowerPercent command
	if ( strcmp( strCommand, Logic_ControlCommand_ManualMode_SetHeaterPowerPercent ) == 0 )
	{
		if ( strValue == NULL )
			return;

		int nPercent = atoi( strValue );

		Logic_ManualMode_HeaterPowerPercent = nPercent;
		Heater_SetPower( nPercent );

		//Logic_StreamTempRegulator.SetIntegralPartValue( nPercent );

		Comm_SendDebugString( "Heater power percent is set to %d %%. (MANUAL MODE)", nPercent );

		return;
	}

	// Reset command
	if ( strcmp( strCommand, Logic_ControlCommand_Reset ) == 0 )
	{
		HAL_NVIC_SystemReset();
	}
}




int Logic_GetTmpADCValue()
{
	return g_TmpADC.Instance->DR;
}




void TriggerHandler_VentilatorMotorDriverFaultState()
{
#if ENABLE_PROTECTION_VetilatorFaultState

	float fTrigValue = Logic_VentilatorMotorDriverFaultState.GetSensorValue();

	if ( fTrigValue == 0 && VentilatorControl_IsEnabled() )
	{
		Logic_VentilatorSpeedRegulator.SetIntegralPartValue( 0 );
		VentilatorControl_SetSpeed( 0 );

		Comm_SendDebugString( "Ventilator fault state triggered (%u)", HAL_GetTick()  );
	}

#endif
}





////////////////////////////////////////////////////////////////////////////////////////////
// PIDRegulator class

PIDRegulator::PIDRegulator( float fCoeffProport, float fCoeffIntegr, float fCoeffDiffer, float fInputOffsetMin, float fInputOffsetMax, float fResultMin, float fResultMax )
	: m_fCoeffProport( fCoeffProport ), m_fCoeffIntegr( fCoeffIntegr ), m_fCoeffDiffer( fCoeffDiffer ),
	  m_fInputOffsetMin( fInputOffsetMin ), m_fInputOffsetMax( fInputOffsetMax ),
	  m_fResultMin( fResultMin ), m_fResultMax( fResultMax )
{
	m_bFirstCalculation = TRUE;
	m_fLastResultValue = fResultMin;	// starting from minimal value
	m_fLastIntegralPartValue = fResultMin;
	m_nLastResultTicks = 0;
	m_fLastInputValue = 0;
	m_fLastDiffValue = 0;

	m_fAdditionalCoeffProport = 1;
}



float PIDRegulator::Calculate( float fInputValue, float fGoalValue )
{
	// Process first call
	if ( m_bFirstCalculation )
	{
		m_bFirstCalculation = FALSE;
		m_nLastResultTicks = HAL_GetTick();
		m_fLastInputValue = fInputValue;

		return m_fLastResultValue;
	}

	// Get offset value
	float fInputOffsetValue = fGoalValue - fInputValue;
	fInputOffsetValue = MAX( fInputOffsetValue, m_fInputOffsetMin );
	fInputOffsetValue = MIN( fInputOffsetValue, m_fInputOffsetMax );

	// Get time difference since last calculation
	uint nCurTicks = HAL_GetTick();
	float fTimeDiff = (float)(nCurTicks - m_nLastResultTicks) / 1000;		// in seconds
	fTimeDiff = MIN( fTimeDiff, 1000.0 );		// limit by one second
	if ( fTimeDiff == 0 )
		return m_fLastResultValue;

	// Get current value difference with approximation
	const float fDiffApproxCoeff = 0.3;
	m_fLastDiffValue = m_fLastDiffValue * (1 - fDiffApproxCoeff) + (fInputValue - m_fLastInputValue) / fTimeDiff * fDiffApproxCoeff;

	// Calculate integral part
	m_fLastIntegralPartValue += (m_fCoeffIntegr * fInputOffsetValue - m_fCoeffDiffer * m_fLastDiffValue) * fTimeDiff;

	m_fLastIntegralPartValue = MAX( m_fLastIntegralPartValue, m_fResultMin );
	m_fLastIntegralPartValue = MIN( m_fLastIntegralPartValue, m_fResultMax );

	// Calculate result value
	m_fLastResultValue = m_fCoeffProport * m_fAdditionalCoeffProport * fInputOffsetValue + m_fLastIntegralPartValue;

	m_fLastResultValue = MAX( m_fLastResultValue, m_fResultMin );
	m_fLastResultValue = MIN( m_fLastResultValue, m_fResultMax );

	// Save history values
	m_nLastResultTicks = nCurTicks;
	m_fLastInputValue = fInputValue;

	return m_fLastResultValue;
}






////////////////////////////////////////////////////////////////////////////////////////////
// DEBUG

void Tmp_InitADC()
{
	// *** ADC GPIO Configuration


	GPIO_InitTypeDef GPIO_InitStruct;
	GPIO_InitStruct.Pin = GPIO_PIN_1;
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init( GPIOC, &GPIO_InitStruct );


	// *** ADC Configuration

	__ADC2_CLK_ENABLE();

	g_TmpADC.Instance = ADC2;
	g_TmpADC.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV8;
	g_TmpADC.Init.Resolution = ADC_RESOLUTION12b;
	g_TmpADC.Init.ScanConvMode = DISABLE; //ENABLE
	g_TmpADC.Init.ContinuousConvMode = ENABLE;
	g_TmpADC.Init.NbrOfConversion = 1;
	g_TmpADC.Init.DMAContinuousRequests = DISABLE; // ENABLE;

	g_TmpADC.Init.DiscontinuousConvMode = DISABLE;
	g_TmpADC.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
	g_TmpADC.Init.DataAlign = ADC_DATAALIGN_RIGHT;
	g_TmpADC.Init.EOCSelection = DISABLE; //EOC_SINGLE_CONV;
	HAL_ADC_Init( &g_TmpADC );

	ADC_ChannelConfTypeDef sConfig;
	sConfig.Channel = ADC_CHANNEL_11;
	sConfig.Rank = 1;
	sConfig.SamplingTime = ADC_SAMPLETIME_144CYCLES;
	HAL_ADC_ConfigChannel(&g_TmpADC, &sConfig);

	// Start ADC reading process
	HAL_ADC_Start( &g_TmpADC );
}

