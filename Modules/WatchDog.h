// WatchDog.h
/////////////////////////////////////////////////////////////////////

const uint LSIFreq = 32000;		// Hz

const uint WatchDog_Timeout = 1000;		// the main reset timeout for watchdog; milliseconds

IWDG_HandleTypeDef WatchDog_hiwdg = {0};


void WatchDog_Init()
{
	__HAL_DBGMCU_FREEZE_IWDG();		// freeze watchdog in debug mode

	WatchDog_hiwdg.Instance = IWDG;
	WatchDog_hiwdg.Init.Prescaler = IWDG_PRESCALER_256;
	WatchDog_hiwdg.Init.Reload = WatchDog_Timeout * (LSIFreq / 256) / 1000;

	HAL_IWDG_Init( &WatchDog_hiwdg );
	HAL_IWDG_Start( &WatchDog_hiwdg );
	HAL_IWDG_Refresh( &WatchDog_hiwdg );
}


void WatchDog_Refresh()
{
	HAL_IWDG_Refresh( &WatchDog_hiwdg );
}
