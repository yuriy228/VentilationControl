
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <stdarg.h>

#include "stm32f4xx.h"
#include "stm32f4xx_hal.h"

#include "Diagnostics.h"
#include "types.h"
#include "array.h"
#include "tools.h"



// Common resources

#define VentilatorControl_CounterTimerEnableFunc		__TIM6_CLK_ENABLE

#define VentilatorControl_InterruptTimerPriority		1
#define VentilatorControl_InterruptTimerEnableFunc		__TIM7_CLK_ENABLE
#define VentilatorControl_InterruptTimerIRQn			TIM7_IRQn

// TIMs
#define VentilatorControl_CounterTimerHandle			TIM6
#define VentilatorControl_InterruptTimerHandle			TIM7
#define VentilatorControl_PWMTimer1Handle				TIM1
#define VentilatorControl_PWMTimer2Handle				TIM8

#define Logic_SreamSpeedSensor_TimerHandle				TIM3
#define Logic_SreamSpeedSensorAux_TimerHandle			TIM9

#define Heater_PWMTimerHandle							TIM14

// ADCs
#define Sensors_AnalogSensors_ADCHandle			ADC1

// DMA
#define Sensors_AnalogSensors_DMAHandle			DMA2_Stream0

#define Sensors_AnalogSensors_DMAChannel		DMA_CHANNEL_0

// UARTS
#define Comm_UARTHandle							UART5

#define Comm_UARTEnableFunc 					__UART5_CLK_ENABLE

// Priorities
#define Logic_ErrorTriggerInterruptPriority				0
#define Comm_UARTInterruptPriority						10
#define Sensors_AnalogSensors_DMAInterruptPriority		11
#define Logic_SreamSpeedSensor_InterruptPriority		12
#define Logic_SreamSpeedSensorAux_InterruptPriority		13

// IRQs
#define Comm_UART5_IRQn							UART5_IRQn
#define Sensors_AnalogSensors_DMAIRQn			DMA2_Stream0_IRQn


// GPIOs
const GPIOPort Comm_UARTPortTX( GPIO_C, GPIO_PIN_12 );
const GPIOPort Comm_UARTPortRX( GPIO_D, GPIO_PIN_2 );

const GPIOPort VentilatorControl_MotorDriverEnableOutGPIO( GPIO_E, GPIO_PIN_15 );
const GPIOPort VentilatorControl_MotorDriverFaultInputGPIO( GPIO_D, GPIO_PIN_9, 0, GPIO_NOPULL );

const GPIOPort Heater_EnableOutputGPIO( GPIO_B, GPIO_PIN_7 );
const GPIOPort Heater_PWMOutputGPIO( GPIO_A, GPIO_PIN_7 );

const GPIOPort Logic_SreamSpeedSensor_StartGPIO( GPIO_D, GPIO_PIN_7 );
const GPIOPort Logic_SreamSpeedSensor_TimerChannel1GPIO( GPIO_B, GPIO_PIN_4 );
const GPIOPort Logic_SreamSpeedSensor_TimerChannel2GPIO( GPIO_B, GPIO_PIN_5 );

const GPIOPort Logic_SreamSpeedSensorAux_GPIO( GPIO_E, GPIO_PIN_5, GPIO_MODE_AF_PP, GPIO_PULLUP );
const GPIOPort Logic_HeaterTempSensorGPIO( GPIO_A, GPIO_PIN_0 );
const GPIOPort Logic_StreamTempSensorGPIO( GPIO_C, GPIO_PIN_0 );
const GPIOPort Logic_VentilatorTempSensorGPIO( GPIO_A, GPIO_PIN_1 );

const GPIOPort Main_Led3GPIO( GPIO_D, GPIO_PIN_13, GPIO_MODE_OUTPUT_PP, GPIO_PULLUP );
const GPIOPort Main_Led4GPIO( GPIO_D, GPIO_PIN_12, GPIO_MODE_OUTPUT_PP, GPIO_PULLUP );
const GPIOPort Main_Led5GPIO( GPIO_D, GPIO_PIN_14, GPIO_MODE_OUTPUT_PP, GPIO_PULLUP );
const GPIOPort Main_Led6GPIO( GPIO_D, GPIO_PIN_15, GPIO_MODE_OUTPUT_PP, GPIO_PULLUP );



// Other GPIOs
//
// PB0  	TIM1_CH2N
// PB1  	TIM1_CH3N
// PE11  	TIM1_CH2
// PE13  	TIM1_CH3
//
// PA5     	TIM8_CH1N
// PB15     TIM8_CH3N
// PC6     	TIM8_CH1
// PC8     	TIM8_CH3
//
