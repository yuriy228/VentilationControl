// Sensors.h
///////////////////////////////////////////////////////////////////////////////

const float Sensors_BaseADCVoltage = 3.27;


// Sensors base template class
//
template < class SensorTypeClass >
class SensorsTmpl
{
public:
	// Definition of sensors list values
	static const int SensorsListMaxSize = 10;
	static SensorTypeClass* m_arrSensors[SensorsListMaxSize];
	static int m_nSensorsListSize;

	// Sensor's data
	SensorTypeClass *m_pSensor;

	float m_fSensorValue;		// universal type value for all sensors
	uint m_nLastUpdateTime;		// ticks of sensor value last update

	void (* m_fncValueChangeCallback)();

	// Construction
	SensorsTmpl( SensorTypeClass *pSensor )				: m_pSensor( pSensor ), m_fSensorValue( 0 ), m_nLastUpdateTime( 0 ), m_fncValueChangeCallback( NULL )
														{  if ( m_nSensorsListSize >= SensorsListMaxSize ) return;  m_arrSensors[m_nSensorsListSize++] = pSensor; }
	// Operations
	void SetSensorValue( float fSensorValue )			{ float fOldVal = m_fSensorValue;  m_fSensorValue = fSensorValue;  m_nLastUpdateTime = HAL_GetTick();  if ( m_fncValueChangeCallback && fOldVal != fSensorValue ) m_fncValueChangeCallback(); };
	float GetSensorValue( uint *pElapsedTicks = NULL ) 	{ if ( pElapsedTicks ) { volatile uint nTimePrefetch = m_nLastUpdateTime; *pElapsedTicks = HAL_GetTick() - nTimePrefetch; }  return m_fSensorValue; };

	uint GetLastUpdateTime()							{ return m_nLastUpdateTime; }

	void SetCallback( void (* fncCallback)() )			{ m_fncValueChangeCallback = fncCallback; };
};

template < class SensorTypeClass >
SensorTypeClass* SensorsTmpl< SensorTypeClass >::m_arrSensors[ SensorsListMaxSize ];
template < class SensorTypeClass >
int SensorsTmpl< SensorTypeClass >::m_nSensorsListSize = 0;



// Trigger sensor class
//
// Value: LOW: 0, HI: 1
//
class TriggerSensorType : public SensorsTmpl< TriggerSensorType >
{
public:
	TriggerSensorType( const GPIOPort &port, int nIRQPriority );

	static void init();		// initialization of all sensors of this type

	static void IRQHandler();

// Attributes
	GPIOPort m_GPIOPort;
	int m_nIRQPriority;
};




// Pulse sensor class
//
// Value: period in microseconds
//
class PulseSensorType : public SensorsTmpl< PulseSensorType >, ObservedHALObjectListener
{
public:
// Attributes
	GPIOPort 		m_GPIOPort;
	TIMObjectType 	m_TIMObject;
	int 			m_nIRQPriority;
	int 			m_nMaxPeriodMilliseconds;
	float 			m_fAveragingCoeff;

// Functions
	PulseSensorType( const GPIOPort &port, TIM_TypeDef* TIMx, int nIRQPriority, int nMaxPeriodMilliseconds, float fAveragingCoeff = 1 );

	static void Init();		// initialization of all sensors of this type

	virtual void OnCallback( uint nCallbackID );		// a callback from TIM object

private:
	float m_fValueMultiplier;		// converts timer's period value to microseconds
};



// Air speed sensor class
//
// For HC-SR04 based sensor
// Value: linear air speed in m/s
//
class AirSpeedSensorType : public SensorsTmpl< AirSpeedSensorType >, ObservedHALObjectListener
{
public:
// Class interface
	AirSpeedSensorType( const GPIOPort& portStartMeasure, const GPIOPort& portTIM_CH1, const GPIOPort& portTIM_CH2, TIM_TypeDef* TIMx, int nIRQPriority,
						float fDistanceBetweenSensors, uint nMeasurmentPeriod, int nMedianFilterResolution, float fAveragingCoeff );

	static void Init();		// initialization of all sensors of this type

	static void LoopStep();	// starts measurement in specified period

private:
// Constants
	static const int AverageSoundSpeed = 345;		// in m/s, 25 celsius

// Attributes
	// Sensor parameters
	GPIOPort 		m_GPIOPort_StartMeasure,
					m_GPIOPort_TIM_CH1,
					m_GPIOPort_TIM_CH2;
	TIMObjectType 	m_TIMObject;
	int 			m_nIRQPriority;
	float			m_fDistanceBetweenSensors;		// in meters
	uint 			m_nMeasurmentPeriod;			// in milliseconds
	float 			m_fAveragingCoeff;

	// Measure mode values
	uint m_nLastStartTicks;							// ticks of last measurement start
	bool m_bIsMeasuring;							// is true after start siglan is sent and until measurement complete or error occured (timeout)

	enum { CAPTURED_NON, CAPTURED_RISE, CAPTURED_FALL }
		m_enTIMCapturedChannel1, m_enTIMCapturedChannel2;		// states of captured sensor's PWM signals during current measurement

	int m_nChannel1ValueRise, m_nChannel1ValueFall,				// captured raw values for each channel
		m_nChannel2ValueRise, m_nChannel2ValueFall;
	MedianFilter<float> m_ResultValueFilter;					// median filter for result values

// Internal functions
	virtual void OnCallback( uint nCallbackID );	// a callback from TIM object
};





// Analog sensor class
//
// Value:
// Scans all sensors in loop
//
class AnalogSensorType : public SensorsTmpl< AnalogSensorType >
{
public:
// Attributes
	GPIOPort 	m_GPIOPort;
	uint 		m_ADCChannel;

	float m_fValueShift;
	float m_fValueMultiplier;

// Functions
	AnalogSensorType( const GPIOPort &port, uint ADCChannel, float fValueShift, float fValueMultiplier );
	AnalogSensorType( const GPIOPort &port, uint ADCChannel, float fV1, float fT1, float fV2, float fT2 );	// T1, T2 - temperatures, V1, V2 - apropriate voltages

	static void Init();		// initialization of all sensors of this type

// Implementation
	static const uint ADCResolution = ADC_RESOLUTION12b;
	static const uint ADCMaxInputValue = (0x1 << 12);			// should consider ADCResolution

	static const uint ApproximationConversionsCount = 1000;		// amount of ADC conversions for each sensor at each reading step; used to approximate the result values

	static ADC_HandleTypeDef m_hADC;
	static DMA_HandleTypeDef m_hDMA;

	static efcArray< uint16 > m_arrRoughValuesBuffer;			// used to store values by DMA in ADC conversions

};





