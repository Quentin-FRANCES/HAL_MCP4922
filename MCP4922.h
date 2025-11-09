#ifndef INC_MCP4922_H_
#define INC_MCP4922_H_

#define DAC_CHANNEL_A 0
#define DAC_CHANNEL_B 1
#define NUM_SAMPLES 256  // Nombre d'échantillons pour nos vagues
#define PI 3.1415926535f
#define TIMER_CLOCK_FREQ 64000000.0f // 1MHz (64MHz / (Prescaler+1))

typedef enum {
    WAVE_NONE,
    WAVE_SINE,
    WAVE_TRIANGLE,
    WAVE_SAWTOOTH
} Waveform_t;


/* --- Prototypes des Fonctions --- */

/**
 * @brief Initialise le driver du DAC.
 * @note Doit être appelée une fois au démarrage.
 * @param hspi: Pointeur vers le handle SPI (ex: &hspi1)
 * @param htim: Pointeur vers le handle Timer (ex: &htim6)
 */
void MCP4922_Init(SPI_HandleTypeDef* hspi, TIM_HandleTypeDef* htim);

/**
 * @brief Envoie une valeur 12 bits brute à un canal.
 * @param channel: Le canal à utiliser (DAC_CHANNEL_A ou DAC_CHANNEL_B)
 * @param data: La valeur 12 bits à envoyer (0-4095)
 */
void MCP4922_Write(uint8_t channel, uint16_t data);

/**
 * @brief Configure le générateur pour une onde Sinusoïdale.
 * @param frequency: Fréquence du signal en Hz
 * @param amplitude: Amplitude (0-4095)
 */
void Set_Sine_Wave(float frequency, uint16_t amplitude);

/**
 * @brief Configure le générateur pour une onde Triangulaire.
 * @param frequency: Fréquence du signal en Hz
 * @param amplitude: Amplitude (0-4095)
 */
void Set_Triangle_Wave(float frequency, uint16_t amplitude);

/**
 * @brief Configure le générateur pour une onde en Dents de Scie.
 * @param frequency: Fréquence du signal en Hz
 * @param amplitude: Amplitude (0-4095)
 */
void Set_Sawtooth_Wave(float frequency, uint16_t amplitude);

/**
 * @brief Fonction de mise à jour, à appeler depuis l'ISR du timer.
 * @note C'est le cœur du générateur, il calcule et envoie 1 échantillon.
 */
void MCP4922_TIM_Callback(void);

#endif /* INC_MCP4922_H_ */
