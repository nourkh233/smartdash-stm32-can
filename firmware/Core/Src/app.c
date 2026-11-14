#include "app.h"
#include "main.h"
#include "can_codec.h"
#include <stdio.h>
#include <stdbool.h>

extern ADC_HandleTypeDef  hadc1;
extern CAN_HandleTypeDef  hcan1;
extern TIM_HandleTypeDef  htim3;
extern UART_HandleTypeDef huart2;

/* ===================== Réglages ===================== */
#define CAN_LOOPBACK_TEST 0   /* 1 = test sur une seule carte (CubeMX en Loopback) ; 0 = Normal */
#define SENSOR_UART_ECHO  1   /* carte capteurs : affiche aussi ses trames sur son port COM */
#define SIM_SPEED   0         /* 1 = valeur simulée à la place du capteur */
#define SIM_DIST    0
#define SIM_CURRENT 0
#define SIM_TEMP    0

#define TRIG_PORT GPIOA
#define TRIG_PIN  GPIO_PIN_3  /* si conflit avec la ST-LINK : GPIO_PIN_1 */

#define ACS_DIV_FACTOR   1.5f     /* diviseur 10k/20k */
#define ACS_SENS_V_PER_A 0.185f   /* ACS712-5A */
#define SPEED_MAX_KMH    120.0f
#define DIST_MAX_MM      2000.0f

/* ===================== Outils communs ===================== */
typedef struct { uint32_t period_ms, last; void (*run)(void); } task_t;
static void sched_run(task_t *t, int n) {
    const uint32_t now = HAL_GetTick();
    for (int i = 0; i < n; i++) {
        if ((uint32_t)(now - t[i].last) >= t[i].period_ms) {
            t[i].last += t[i].period_ms;
            t[i].run();
        }
    }
}

static void uart_send_frame(const sd_frame_t *f) {     /* une trame = une ligne "ID#DONNEES" */
    char s[40];
    int n = snprintf(s, sizeof s, "%03X#", (unsigned)f->id);
    for (int i = 0; i < f->dlc; i++) n += snprintf(s + n, sizeof s - n, "%02X", f->data[i]);
    s[n++] = '\n';
    HAL_UART_Transmit(&huart2, (uint8_t *)s, n, 10);
}

static void print_can_errors(void) {
    uint32_t esr = hcan1.Instance->ESR;
    char s[40];
    int n = snprintf(s, sizeof s, "ERR tec=%lu rec=%lu\n",
                     (unsigned long)((esr >> 16) & 0xFF), (unsigned long)((esr >> 24) & 0xFF));
    HAL_UART_Transmit(&huart2, (uint8_t *)s, n, 10);
}

/* ===================== CAN ===================== */
static void can_start(void) {
    CAN_FilterTypeDef f = {0};
    f.FilterBank = 0;
    f.FilterMode = CAN_FILTERMODE_IDMASK;
    f.FilterScale = CAN_FILTERSCALE_32BIT;
    f.FilterIdHigh = 0; f.FilterIdLow = 0;
    f.FilterMaskIdHigh = 0; f.FilterMaskIdLow = 0;    /* tout accepter */
    f.FilterFIFOAssignment = CAN_RX_FIFO0;
    f.FilterActivation = ENABLE;
    f.SlaveStartFilterBank = 14;
    HAL_CAN_ConfigFilter(&hcan1, &f);
    HAL_CAN_Start(&hcan1);
    HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);
}

/* Tampon circulaire (interruption CAN -> boucle principale) */
#define RING_N 64
static sd_frame_t ring[RING_N];
static volatile uint16_t head = 0, tail = 0;

static void ring_push(const sd_frame_t *f) {
    uint16_t n = (head + 1) & (RING_N - 1);
    if (n == tail) return;
    ring[head] = *f;
    __DMB();
    head = n;
}
static bool ring_pop(sd_frame_t *f) {
    if (tail == head) return false;
    *f = ring[tail];
    __DMB();
    tail = (tail + 1) & (RING_N - 1);
    return true;
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *h) {
    CAN_RxHeaderTypeDef rx; sd_frame_t f;
    while (HAL_CAN_GetRxFifoFillLevel(h, CAN_RX_FIFO0) > 0) {
        if (HAL_CAN_GetRxMessage(h, CAN_RX_FIFO0, &rx, f.data) == HAL_OK) {
            f.id = (uint16_t)rx.StdId; f.dlc = (uint8_t)rx.DLC;
            ring_push(&f);
        }
    }
}

#if NODE_A
/* =====================================================================
 *  CARTE CAPTEURS : mesure et émet sur le CAN
 * ===================================================================== */

static void dwt_init(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}
static void delay_us(uint32_t us) {
    const uint32_t start = DWT->CYCCNT, ticks = us * (SystemCoreClock / 1000000u);
    while ((DWT->CYCCNT - start) < ticks) { }
}
static float tri(uint32_t t, uint32_t period, float lo, float hi) {
    float x = (float)(t % period) / (float)period;
    x = (x < 0.5f) ? x * 2.0f : (1.0f - x) * 2.0f;
    return lo + (hi - lo) * x;
}

static void publish(const sd_frame_t *f) {
    if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) == 0)  /* boîtes pleines (pas de passerelle ?) : on purge */
        HAL_CAN_AbortTxRequest(&hcan1, CAN_TX_MAILBOX0 | CAN_TX_MAILBOX1 | CAN_TX_MAILBOX2);
    CAN_TxHeaderTypeDef h = { .StdId = f->id, .ExtId = 0, .IDE = CAN_ID_STD,
                              .RTR = CAN_RTR_DATA, .DLC = f->dlc, .TransmitGlobalTime = DISABLE };
    uint32_t mb;
    HAL_CAN_AddTxMessage(&hcan1, &h, (uint8_t *)f->data, &mb);
#if SENSOR_UART_ECHO && !CAN_LOOPBACK_TEST
    uart_send_frame(f);
#endif
}

/* ---- ADC + DMA : PA0 (vitesse), PA4 (ACS712), PA5 (LM35) ---- */
#define ADC_SCANS 16
static uint16_t adc_buf[ADC_SCANS][3];

static float adc_avg(int ch) {
    uint32_t sum = 0;
    for (int i = 0; i < ADC_SCANS; i++) sum += adc_buf[i][ch];
    return (float)sum / ADC_SCANS;
}
static float counts_to_volts(float c) { return c * 3.3f / 4095.0f; }

static float acs_offset_v = 2.5f;
static void acs_calibrate(void) {                     /* courant NUL obligatoire au démarrage */
    HAL_Delay(300);
    float sum = 0;
    for (int i = 0; i < 200; i++) { sum += counts_to_volts(adc_avg(1)) * ACS_DIV_FACTOR; HAL_Delay(2); }
    acs_offset_v = sum / 200.0f;
}
static float read_speed(void)   { return adc_avg(0) / 4095.0f * SPEED_MAX_KMH; }
static float read_current(void) {
    float v = counts_to_volts(adc_avg(1)) * ACS_DIV_FACTOR;
    float a = (v - acs_offset_v) / ACS_SENS_V_PER_A;
    return (a > -0.03f && a < 0.03f) ? 0.0f : a;
}
static float read_temp(void)    { return counts_to_volts(adc_avg(2)) * 100.0f; }   /* LM35 : 10 mV/°C */

/* ---- HC-SR04 : TRIG PA3 (GPIO), ECHO PA6 (TIM3 PWM input) ---- */
static volatile uint32_t echo_us = 0;
static volatile bool echo_new = false;

static void hcsr04_init(void) {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef g = {0};
    g.Pin = TRIG_PIN; g.Mode = GPIO_MODE_OUTPUT_PP; g.Pull = GPIO_NOPULL; g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(TRIG_PORT, &g);
    HAL_GPIO_WritePin(TRIG_PORT, TRIG_PIN, GPIO_PIN_RESET);
    HAL_TIM_IC_Start(&htim3, TIM_CHANNEL_1);          /* front montant : remet le compteur à 0 */
    HAL_TIM_IC_Start_IT(&htim3, TIM_CHANNEL_2);       /* front descendant : largeur de l'écho */
}
static void hcsr04_trigger(void) {
    HAL_GPIO_WritePin(TRIG_PORT, TRIG_PIN, GPIO_PIN_SET);
    delay_us(10);
    HAL_GPIO_WritePin(TRIG_PORT, TRIG_PIN, GPIO_PIN_RESET);
}
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM3 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2) {
        echo_us = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);   /* largeur en µs */
        echo_new = true;
    }
}

/* ---- Tâches ---- */
static uint8_t hb_counter = 0;
static float dist_mm = DIST_MAX_MM;
static uint8_t miss = 0;

static void tx_hb(void) { sd_frame_t f = codec_hb(ID_HB_A, hb_counter++, 0); publish(&f); }
static void tx_speed(void) {
    float v = SIM_SPEED ? tri(HAL_GetTick(), 20000, 0.0f, SPEED_MAX_KMH) : read_speed();
    sd_frame_t f = codec_speed(v); publish(&f);
}
static void tx_current(void) {
    float a = SIM_CURRENT ? (float)((HAL_GetTick() / 5000) % 3) * 0.8f : read_current();
    sd_frame_t f = codec_current(a); publish(&f);
}
static void tx_temp(void) {
    float t = SIM_TEMP ? tri(HAL_GetTick(), 60000, 22.0f, 30.0f) : read_temp();
    sd_frame_t f = codec_temp(t); publish(&f);
}
static void task_dist(void) {
    if (SIM_DIST) {
        dist_mm = tri(HAL_GetTick(), 15000, 50.0f, DIST_MAX_MM);
    } else {
        if (echo_new) {
            echo_new = false; miss = 0;
            uint32_t us = echo_us;
            if (us >= 100) { float mm = (float)us * 0.1715f; dist_mm = (mm > DIST_MAX_MM) ? DIST_MAX_MM : mm; }
        } else if (++miss >= 3) { dist_mm = DIST_MAX_MM; miss = 3; }   /* pas d'écho : hors portée */
        hcsr04_trigger();
    }
    sd_frame_t f = codec_dist(dist_mm); publish(&f);
}

static task_t tasks[] = {
    {   50, 0, tx_speed },
    {  100, 0, task_dist },
    {   50, 0, tx_current },
    { 1000, 0, tx_temp },
    {  500, 0, tx_hb },
    { 2000, 0, print_can_errors },
};

void app_init(void) {
    dwt_init();
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_buf, ADC_SCANS * 3);
    can_start();
    acs_calibrate();
    hcsr04_init();
}

void app_loop(void) {
    sched_run(tasks, sizeof tasks / sizeof tasks[0]);
#if CAN_LOOPBACK_TEST
    sd_frame_t f;
    while (ring_pop(&f)) uart_send_frame(&f);          /* loopback : on revoit ses propres trames */
#endif
}

#else
/* =====================================================================
 *  CARTE PASSERELLE : CAN -> PC (aucun capteur)
 * ===================================================================== */

static uint8_t hb_counter = 0;
static void tx_hb(void) {
    sd_frame_t f = codec_hb(ID_HB_B, hb_counter++, 0);
    uart_send_frame(&f);                               /* heartbeat vers le PC uniquement */
}
static task_t tasks[] = {
    {  500, 0, tx_hb },
    { 2000, 0, print_can_errors },
};

void app_init(void) {
    can_start();
}

void app_loop(void) {
    sched_run(tasks, sizeof tasks / sizeof tasks[0]);
    sd_frame_t f;
    while (ring_pop(&f)) uart_send_frame(&f);          /* tout le bus -> PC */
}
#endif
