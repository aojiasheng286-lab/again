#include "my_adc.h"

extern ADC_HandleTypeDef hadc1; 

float adc_get_voltage(void)
{
    uint32_t adc_value = 0;
    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)
    {
        adc_value = HAL_ADC_GetValue(&hadc1);
    }
    HAL_ADC_Stop(&hadc1);
    
    return (float)adc_value * 3.3f / 4095.0f;
}  