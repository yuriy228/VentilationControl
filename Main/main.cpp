/**
  ******************************************************************************
  * File Name          : main.c
  * Description        : Main program body
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/

#include "main.h"
#include "VentilatorControl.h"
#include "Communication.h"
#include "WatchDog.h"
#include "Sensors.h"
#include "Heater.h"
#include "Logic.h"

#include "stm32f4xx_hal_dac.h"


void Main_Init();


int main(void)
{
	Main_Init();

	// Init modules
	TIMObjectType::EnableIRQs();

	Comm_Init();

	WatchDog_Init();	// init before ventilator and heater control

	VentilatorControl_Init();
	Heater_Init();

	TriggerSensorType::init();
	PulseSensorType::Init();
	AnalogSensorType::Init();
	AirSpeedSensorType::Init();

	Logic_Init();

	//
	Main_Led5GPIO.Init();

	// Start log
	Comm_SendString( "" );
	Comm_SendString( "------------------------------------------------------------------------------" );
	Comm_SendString( "Program starts..." );

	// Get and log the reset initiator
	if ( __HAL_RCC_GET_FLAG( RCC_FLAG_BORRST ) )		Comm_SendString( "Reset by brownout" );
	if ( __HAL_RCC_GET_FLAG( RCC_FLAG_PINRST ) )		Comm_SendString( "Reset by pin" );
	if ( __HAL_RCC_GET_FLAG( RCC_FLAG_PORRST ) )		Comm_SendString( "Reset by power on" );
	if ( __HAL_RCC_GET_FLAG( RCC_FLAG_SFTRST ) )		Comm_SendString( "Reset by software" );
	if ( __HAL_RCC_GET_FLAG( RCC_FLAG_IWDGRST ) )		Comm_SendString( "Reset by watchdog" );
	if ( __HAL_RCC_GET_FLAG( RCC_FLAG_WWDGRST ) )		Comm_SendString( "Reset by watchdog" );
	if ( __HAL_RCC_GET_FLAG( RCC_FLAG_LPWRRST ) )		Comm_SendString( "Reset by low power" );
	Comm_SendString( "" );

	__HAL_RCC_CLEAR_RESET_FLAGS();

	// The main loop
	while (1)
	{

//		int p = HAL_GPIO_ReadPin( sensor1.m_GPIOPort.hGPIO, GPIO_PIN_5 );

/*		{ // Full idle: 1012 ms
		int n;
		uint32_t nStart = HAL_GetTick();

		for ( n = 0; n < 10000000; n++ )
			a++;

		uint32_t nPeriod = HAL_GetTick() - nStart;
		Comm_SendDebugString( "Load: %d ticks (%d %%) \n", nPeriod, (nPeriod - 1012) * 100 / nPeriod );
		}
/**/

/*		{ // Full idle: 1012 ms
		int n = 0;
		uint32_t nStart = HAL_GetTick();

		while ( HAL_GetTick() < nStart + 1000 )
			n++;

		Comm_SendDebugString( "Steps: %d", n );
		}
/**/

		WatchDog_Refresh();

		Logic_MainLoopStep();

		AirSpeedSensorType::LoopStep();


		// LED blink
		Main_Led5GPIO.SetPin( (HAL_GetTick() % 1000) < 500 );

		HAL_Delay( 10 );
	}


}





/*
extern "C" void Default_Handler(void)
{
	while (1)
	{
		int a = 1;
	}
}
/**/


// Speed measurement
/*
void fnc()
{
	// CPU step: 6 ms

	// int:
	// base: 65
	// mult: 71		1 clk
	// div: 95		4 clk

	// float:
	// base: 65
	// mul 72		1 clk
	// div 114		8 clk

	// convert u32 to f32 or back: 1 clk

	__asm(
//		"ldr r2, [pc, #12] \n"
//		"ldr r3, [pc, #12] \n"

//		"mul.w r3, r3, r2 \n"
//		"udiv r3, r3, r2 \n"

		"vldr s15, [pc, #12] \n"
		"vldr s14, [pc, #12] \n"

		"vmul.f32 s15, s15, s14	\n"
		"vdiv.f32 s15, s15, s14	\n"
		           );
}
/**/
