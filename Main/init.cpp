// init.c
////////////////////////////////////////////////////////////////////////////////

#include "stm32f4xx_hal.h"




// Function prototypes
void SystemClock_Config(void);
extern "C" void __libc_init_array();


// Main initialization
//
void Main_Init(void)
{
	// Initialize global objects (solving a bug of the building process)
	//__libc_init_array(); 	(solved)

	// Reset of all peripherals, Initializes the Flash interface and the Systick
	HAL_Init();

	HAL_NVIC_SetPriorityGrouping( NVIC_PRIORITYGROUP_4 );		// tells that we are using only priority and not subpriority

	// Configure the system clock
	SystemClock_Config();

	// Init all GPIO clocks
	__GPIOA_CLK_ENABLE();
	__GPIOB_CLK_ENABLE();
	__GPIOC_CLK_ENABLE();
	__GPIOD_CLK_ENABLE();
	__GPIOE_CLK_ENABLE();
	__GPIOH_CLK_ENABLE();
}



// System Clock Configuration
//
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_ClkInitTypeDef RCC_ClkInitStruct;

  __PWR_CLK_ENABLE();

  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = 16;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  HAL_RCC_OscConfig(&RCC_OscInitStruct);

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1
                              |RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);

  HAL_SYSTICK_Config( HAL_RCC_GetHCLKFreq() / 1000 );

  HAL_SYSTICK_CLKSourceConfig( SYSTICK_CLKSOURCE_HCLK ); 	// SYSTICK_CLKSOURCE_HCLK  SYSTICK_CLKSOURCE_HCLK_DIV8

  /* SysTick_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}






//////////////////////////////////////////////////////////////
// System functions

extern "C"
{

extern void _exit( int status )
{
    printf( "Exiting with status %d.\n", status ) ;
    for ( ; ; ) ;
}

int _getpid(void) { return 1; }

void _kill(int pid)
{
	while(1) ;
}


}

