#include "app_tick.h"
#include "main.h"

static volatile uint32_t s_tick;

uint32_t SYS_GetCurrentTimeMs(void)
{
    return s_tick;
}

void HAL_LPTIM_AutoReloadMatchCallback(LPTIM_HandleTypeDef *hlptim)
{
    if (hlptim->Instance == LPTIM1)
    {
        s_tick++;
    }
}