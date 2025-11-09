#include "main.h"
#include "MCP4922.h"
#include <math.h>

static SPI_HandleTypeDef* g_hspi;
static TIM_HandleTypeDef* g_htim;

// Table de consultation (Lookup Table) pour la sinusoïde (256 points, 0-4095)
static const uint16_t sine_lut[NUM_SAMPLES] = {
		2048, 2098, 2148, 2198, 2248, 2298, 2348, 2398,
		2447, 2496, 2545, 2594, 2642, 2690, 2737, 2784,
		2831, 2877, 2923, 2968, 3013, 3057, 3100, 3143,
		3185, 3226, 3267, 3307, 3346, 3385, 3423, 3459,
		3495, 3530, 3565, 3598, 3630, 3662, 3692, 3722,
		3750, 3777, 3804, 3829, 3853, 3876, 3898, 3919,
		3939, 3958, 3975, 3992, 4007, 4021, 4034, 4045,
		4056, 4065, 4073, 4080, 4085, 4089, 4093, 4094,
		4095, 4094, 4093, 4089, 4085, 4080, 4073, 4065,
		4056, 4045, 4034, 4021, 4007, 3992, 3975, 3958,
		3939, 3919, 3898, 3876, 3853, 3829, 3804, 3777,
		3750, 3722, 3692, 3662, 3630, 3598, 3565, 3530,
		3495, 3459, 3423, 3385, 3346, 3307, 3267, 3226,
		3185, 3143, 3100, 3057, 3013, 2968, 2923, 2877,
		2831, 2784, 2737, 2690, 2642, 2594, 2545, 2496,
		2447, 2398, 2348, 2298, 2248, 2198, 2148, 2098,
		2048, 1997, 1947, 1897, 1847, 1797, 1747, 1697,
		1648, 1599, 1550, 1501, 1453, 1405, 1358, 1311,
		1264, 1218, 1172, 1127, 1082, 1038, 995, 952,
		910, 869, 828, 788, 749, 710, 672, 636,
		600, 565, 530, 497, 465, 433, 403, 373,
		345, 318, 291, 266, 242, 219, 197, 176,
		156, 137, 120, 103, 88, 74, 61, 50,
		39, 30, 22, 15, 10, 6, 2, 1,
		0, 1, 2, 6, 10, 15, 22, 30,
		39, 50, 61, 74, 88, 103, 120, 137,
		156, 176, 197, 219, 242, 266, 291, 318,
		345, 373, 403, 433, 465, 497, 530, 565,
		600, 636, 672, 710, 749, 788, 828, 869,
		910, 952, 995, 1038, 1082, 1127, 1172, 1218,
		1264, 1311, 1358, 1405, 1453, 1501, 1550, 1599,
		1648, 1697, 1747, 1797, 1847, 1897, 1947, 1997,
};

// Variables d'état globales (privées)
static volatile Waveform_t g_current_wave = WAVE_NONE;
static volatile uint32_t g_sample_index = 0;
static volatile float g_current_value = 0.0f;
static volatile float g_step = 0.0f;
static volatile uint16_t g_amplitude = 4095;


void MCP4922_Init(SPI_HandleTypeDef* hspi, TIM_HandleTypeDef* htim)
{
    g_hspi = hspi; // Stocke le pointeur SPI
    g_htim = htim; // Stocke le pointeur Timer
}

void MCP4922_Write(uint8_t channel, uint16_t data)
{
    uint16_t command_word;
    uint16_t config_bits = (channel == DAC_CHANNEL_A) ? 0x7000 : 0xF000;

    command_word = config_bits | (data & 0x0FFF);

    HAL_GPIO_WritePin(CS_DAC_GPIO_Port, CS_DAC_Pin, GPIO_PIN_RESET);
    // Utilise le pointeur SPI stocké
    HAL_SPI_Transmit(g_hspi, (uint8_t*)&command_word, 1, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(CS_DAC_GPIO_Port, CS_DAC_Pin, GPIO_PIN_SET);

    HAL_GPIO_WritePin(LDAC_DAC_GPIO_Port, LDAC_DAC_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LDAC_DAC_GPIO_Port, LDAC_DAC_Pin, GPIO_PIN_SET);
}

static void Set_Sample_Rate(float sample_rate_hz)
{
    HAL_TIM_Base_Stop_IT(g_htim); // Utilise le pointeur Timer stocké

    uint32_t arr_value = (uint32_t)(TIMER_CLOCK_FREQ / sample_rate_hz) - 1;

    __HAL_TIM_SET_AUTORELOAD(g_htim, arr_value);
    __HAL_TIM_SET_COUNTER(g_htim, 0);

    HAL_TIM_Base_Start_IT(g_htim); // Redémarre le timer
}

void Set_Sine_Wave(float frequency, uint16_t amplitude)
{
    g_amplitude = (amplitude > 4095) ? 4095 : amplitude;
    g_sample_index = 0;
    g_current_wave = WAVE_SINE;

    float sample_rate = frequency * NUM_SAMPLES;
    Set_Sample_Rate(sample_rate);
}

void Set_Triangle_Wave(float frequency, uint16_t amplitude)
{
    g_amplitude = (amplitude > 4095) ? 4095 : amplitude;
    g_current_wave = WAVE_TRIANGLE;
    g_step = (float)g_amplitude / (NUM_SAMPLES / 2.0f);
    g_current_value = 0.0f;

    float sample_rate = frequency * NUM_SAMPLES;
    Set_Sample_Rate(sample_rate);
}

void Set_Sawtooth_Wave(float frequency, uint16_t amplitude)
{
    g_amplitude = (amplitude > 4095) ? 4095 : amplitude;
    g_current_wave = WAVE_SAWTOOTH;
    g_step = (float)g_amplitude / (float)NUM_SAMPLES;
    g_current_value = 0.0f;

    float sample_rate = frequency * NUM_SAMPLES;
    Set_Sample_Rate(sample_rate);
}

void MCP4922_TIM_Callback(void)
{
    uint16_t dac_value = 0;

    switch (g_current_wave)
    {
        case WAVE_SINE:
        {
            uint16_t lut_value = sine_lut[g_sample_index];
            if (g_amplitude == 4095)
                        {
                            dac_value = lut_value;
                        }
                        else
                        {

                            dac_value = (uint32_t)(lut_value * g_amplitude) / 4095;
                        }
            g_sample_index = (g_sample_index + 1) & 0xFF;
            break;
        }

        case WAVE_TRIANGLE:
        {
            g_current_value += g_step;
            if (g_current_value >= g_amplitude) {
                g_current_value = g_amplitude;
                g_step = -g_step;
            } else if (g_current_value <= 0) {
                g_current_value = 0;
                g_step = -g_step;
            }
            dac_value = (uint16_t)g_current_value;
            break;
        }

        case WAVE_SAWTOOTH:
        {
            g_current_value += g_step;
            if (g_current_value >= g_amplitude) {
                g_current_value = 0;
            }
            dac_value = (uint16_t)g_current_value;
            break;
        }

        case WAVE_NONE:
        default:
            dac_value = 0;
            break;
    }

    // Envoyer la valeur finale au DAC (Canal A)
    // Note: cette fonction doit être rapide, nous utilisons une version optimisée

    // 1. Construire la trame
    uint16_t command_word = 0x7000 | (dac_value & 0x0FFF);

    // 2. Envoyer (sans pulsar LDAC pour l'instant, on le fera à la fin)
    HAL_GPIO_WritePin(CS_DAC_GPIO_Port, CS_DAC_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(g_hspi, (uint8_t*)&command_word, 1, 1);
    HAL_GPIO_WritePin(CS_DAC_GPIO_Port, CS_DAC_Pin, GPIO_PIN_SET);

    // 3. Pulsar LDAC pour mettre à jour la sortie
    HAL_GPIO_WritePin(LDAC_DAC_GPIO_Port, LDAC_DAC_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LDAC_DAC_GPIO_Port, LDAC_DAC_Pin, GPIO_PIN_SET);
}
