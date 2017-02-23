// tools.h
////////////////////////////////////////////////////////////////////////////////

#include <malloc.h>
#include <algorithm>
#include <deque>
#include <vector>

//#define DEBUG


// Disable and enable interruptions (affects on all int. and configurable exceptions)
//
#define DISABLE_IRQ()		__ASM volatile ("cpsid i" : : : "memory")
#define ENABLE_IRQ()		__ASM volatile ("cpsie i" : : : "memory")

// Optimization prediction
#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

// Memory management functions
//
#if defined DEBUG
	#define free( p )		{				\
		/*memset( p, 0xEE, strlen((char*)p));*/	\
		free( p );	}
#else
	#define free( p ) free(p);
#endif


////////////////////////////////////////////////////////////////////////////////
// GPIO port object
//
class GPIOPort
{
public:
	GPIO_TypeDef* hGPIO;
	uint 		nPin;
	uint 		nMode;
	uint 		nPull;
	uint 		nAlternate;

	GPIOPort( const GPIOPort &port )  	{ *this = port; }
	GPIOPort( GPIO_TypeDef *_hGPIO, uint _nPin, uint _nMode = 0, uint _nPull = 0, uint _nAlternate = 0 ) 	: hGPIO( _hGPIO ), nPin( _nPin ), nMode( _nMode ), nPull( _nPull ), nAlternate( _nAlternate ) {}

	void Init() const																						{ GPIO_InitTypeDef is = { nPin, nMode, nPull, GPIO_SPEED_HIGH, nAlternate };
																											  HAL_GPIO_Init( hGPIO, &is ); }
	void Init( uint _nMode, uint _nPull, uint _nAlternate = 0 ) const 										{ GPIO_InitTypeDef is = { nPin, _nMode, _nPull, GPIO_SPEED_LOW, _nAlternate };
																											  HAL_GPIO_Init( hGPIO, &is ); }
	void SetPin( bool bOn ) const																			{ HAL_GPIO_WritePin( hGPIO, nPin, bOn ? GPIO_PIN_SET : GPIO_PIN_RESET ); }
	bool GetPin( ) const																					{ return HAL_GPIO_ReadPin( hGPIO, nPin ) == GPIO_PIN_SET; }
};




////////////////////////////////////////////////////////////////////////////////
// ObservedHALObjects classes

// Callback listener type
//
class ObservedHALObjectListener
{public:
	virtual void OnCallback( uint nCallbackID ) = 0;
};


// ObservedHALObjectsList class for HAL objects
//
template < class ObjectType, class HALObjectType >
class ObservedHALObjectsList
{public:
// Attributes
	static const int ObjectsListMaxSize = 10;
	static ObjectType* m_arrObjects[ObjectsListMaxSize];
	static int m_nObjectsListSize;

	ObjectType *m_pObject;

	typedef void (* CallbackType)( ObjectType* pObject, int nCallbackID );
	CallbackType m_fncCallback;
	ObservedHALObjectListener *m_pListener;

// Functions
	ObservedHALObjectsList( ObjectType* pObject ) 				: m_pObject( pObject ), m_fncCallback( NULL ), m_pListener( NULL )
															{ if ( m_nObjectsListSize >= ObjectsListMaxSize ) return;  m_arrObjects[m_nObjectsListSize++] = pObject; }

	void SetCallback( CallbackType fncCallback )			{ m_fncCallback = fncCallback; }
	void SetListener( ObservedHALObjectListener *pListener )	{ m_pListener = pListener; }

	void Callback( uint nCallbackID ) 						{ if ( m_fncCallback ) m_fncCallback( m_pObject, nCallbackID );  if ( m_pListener ) m_pListener->OnCallback( nCallbackID ); };

	static bool FindObject( void *pHandlerPtr, int &nIndex );
	//static void CallHALHandler( void *pHandlerPtr, void (* fncHALHandler)(ObjectType *) );
	static void CallHALHandler( void *pHandlerPtr, void (* fncHALHandler)(HALObjectType*) );
};

template < class ObjectType, class HALObjectType >
ObjectType* ObservedHALObjectsList< ObjectType, HALObjectType >::m_arrObjects[ ObjectsListMaxSize ];
template < class ObjectType, class HALObjectType >
int ObservedHALObjectsList< ObjectType, HALObjectType >::m_nObjectsListSize = 0;



// Timers
//
class TIMObjectType : public TIM_HandleTypeDef, public ObservedHALObjectsList< TIMObjectType, TIM_HandleTypeDef >
{public:
// Attributes

// Construction
	TIMObjectType( TIM_TypeDef *TIMx ) 			: ObservedHALObjectsList< TIMObjectType, TIM_HandleTypeDef >( this ) 	{ Instance = TIMx; }

// Operations
	static void EnableIRQs();					// enables all IRQs for all TIMs

	void CLKEnable();							// enables clock for this TIM
	void SetIRQPriority( uint nPriority );		// sets priority for all IRQs of TIM

	uint GetFrequency();						// returns base frequency for TIM
	uint GetGPIOAltFuncID();					// returns alternate function ID for GPIOs for TIM
};

enum TIMCallbackType
{
	TCT_PeriodElapsedCallback,
	TCT_OC_DelayElapsedCallback,
	TCT_IC_CaptureCallback,
	TCT_PWM_PulseFinishedCallback,
	TCT_TriggerCallback,
	TCT_ErrorCallback
};



// ADC  (not complete)
//
class ADCObjectType : public ADC_HandleTypeDef, public ObservedHALObjectsList< ADCObjectType, ADC_HandleTypeDef >
{public:
// Construction
	ADCObjectType( ADC_TypeDef *TIMx ) 			: ObservedHALObjectsList< ADCObjectType, ADC_HandleTypeDef >( this ) 	{ Instance = TIMx; }

// Operations
//	static void EnableIRQs();					// enables all IRQs for all ADCs

	void CLKEnable();							// enables clock for ADC
//	uint SetIRQPriority( uint nPriority );		// sets priority for all IRQs of ADC
};





////////////////////////////////////////////////////////////////////////////////
//

template < class ValueType >
class MedianFilter
{
public:
	MedianFilter( uint nResolution = 3 ) 	{ m_nResolution = nResolution; }
	void SetResolution( uint nResolution )	{ m_nResolution = nResolution; }

	void AddValue( ValueType value )		{ m_ValuesQueue.push_back( value ); if (m_ValuesQueue.size() > m_nResolution) m_ValuesQueue.pop_front(); }

	ValueType GetValue()					{ std::vector<ValueType> arr( m_ValuesQueue.size() );
											  std::partial_sort_copy( m_ValuesQueue.begin(), m_ValuesQueue.end(), arr.begin(), arr.end() );
											  return (arr.size() == 0)? 0 : ( arr[arr.size() / 2] );  }
	int GetValuesAmount()					{ return m_ValuesQueue.size();  }
	ValueType GetLastValue()				{ return (m_ValuesQueue.size() == 0)? 0 : m_ValuesQueue.back(); }

// Attributes
private:
	uint m_nResolution;	// filter resolution

	std::deque<ValueType> m_ValuesQueue;
};


#define RUN_EACH_MILLISECONDS( nPeriodMilliseconds, fnc ) { static uint nLastTime = HAL_GetTick(); if ( HAL_GetTick() - nLastTime >= nPeriodMilliseconds ) { nLastTime = HAL_GetTick(); fnc(); } }




////////////////////////////////////////////////////////////////////////////////
// Global functions

IRQn_Type GetExtiIrqByGPIOPin( uint nPin );

