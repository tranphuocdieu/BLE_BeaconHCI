#include "app_tick.h"
#include "main.h"

extern LPTIM_HandleTypeDef hlptim1;
volatile uint32_t lptim_high = 0;
uint64_t lptim_ticks(void);

uint32_t SYS_GetCurrentTimeUs(void)
{
    uint64_t ticks = lptim_ticks();

    return (uint32_t)((ticks * 1000000ULL) / 32768ULL);
}

uint32_t SYS_GetCurrentTimeMs(void)
{
    uint64_t ticks = lptim_ticks();

    return (uint32_t)((ticks * 1000ULL) / 32768ULL);
}

uint64_t lptim_ticks(void)
{
    uint32_t high;
    uint16_t low;

    do
    {
        high = lptim_high;
        low  = HAL_LPTIM_ReadCounter(&hlptim1);
        if (__HAL_LPTIM_GET_FLAG(&hlptim1, LPTIM_FLAG_ARRM) != RESET)
        {
            high++;
            low = HAL_LPTIM_ReadCounter(&hlptim1);
        }
    }
    while ((high != lptim_high) &&
            (__HAL_LPTIM_GET_FLAG(&hlptim1, LPTIM_FLAG_ARRM) == RESET));

    return ((uint64_t)high << 16) | low;
}

extern void SS_HAL_Timer_ISR( void );
void HAL_LPTIM_AutoReloadMatchCallback(LPTIM_HandleTypeDef *hlptim)
{
    if (hlptim->Instance == LPTIM1)
    {
        SS_HAL_Timer_ISR();
        lptim_high++;
    }
}
