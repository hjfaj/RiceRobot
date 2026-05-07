#include "stm32f10x.h"

/* DWT 寄存器定义（兼容旧版 CMSIS core_cm3.h） */
typedef struct {
  volatile uint32_t CTRL;
  volatile uint32_t CYCCNT;
} DWT_Type;

#define DWT_BASE (0xE0001000)
#define DWT ((DWT_Type *)DWT_BASE)
#define DWT_CTRL_CYCCNTENA_Msk (1UL << 0)

static int dwt_enabled = 0;

uint32_t GetTick(void)
{
	if (!dwt_enabled) {
		CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
		DWT->CYCCNT = 0;
		DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
		dwt_enabled = 1;
	}
	return DWT->CYCCNT / (SystemCoreClock / 1000);
}

/**
  * @brief  微秒级延时
  * @param  xus 延时时长，范围：0~233015
  * @retval 无
  */
void Delay_us(uint32_t xus)
{
	// 保存当前 SysTick 配置，避免破坏其他模块（如 RTOS）的 SysTick 设置
	uint32_t saved_load = SysTick->LOAD;
	uint32_t saved_val  = SysTick->VAL;
	uint32_t saved_ctrl = SysTick->CTRL;

	SysTick->LOAD = 72 * xus;				//设置定时器重装值
	SysTick->VAL = 0x00;					//清空当前计数值
	SysTick->CTRL = 0x00000005;				//设置时钟源为HCLK，启动定时器
	while(!(SysTick->CTRL & 0x00010000));	//等待计数到0
	SysTick->CTRL = 0x00000004;				//关闭定时器

	// 恢复原始 SysTick 配置
	SysTick->LOAD = saved_load;
	SysTick->VAL  = saved_val;
	SysTick->CTRL = saved_ctrl;
}

/**
  * @brief  毫秒级延时
  * @param  xms 延时时长，范围：0~4294967295
  * @retval 无
  */
void Delay_ms(uint32_t xms)
{
	while(xms--)
	{
		Delay_us(1000);
	}
}
 
/**
  * @brief  秒级延时
  * @param  xs 延时时长，范围：0~4294967295
  * @retval 无
  */
void Delay_s(uint32_t xs)
{
	while(xs--)
	{
		Delay_ms(1000);
	}
} 
