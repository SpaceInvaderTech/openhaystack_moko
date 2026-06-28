#include "accel.h"
#include "nrfx_twi.h"
#include "nrfx_gpiote.h"
#include "nrf_delay.h"
#include "nrf_log.h"
#include "app_error.h"

#define TWI_INSTANCE_ID     0

/* LIS2DH12 register map (relevant subset) */
#define LIS2DH12_WHO_AM_I       0x0F
#define LIS2DH12_CTRL_REG1      0x20
#define LIS2DH12_CTRL_REG2      0x21
#define LIS2DH12_CTRL_REG3      0x22
#define LIS2DH12_CTRL_REG4      0x23
#define LIS2DH12_CTRL_REG5      0x24
#define LIS2DH12_CTRL_REG6      0x25
#define LIS2DH12_INT1_CFG       0x30
#define LIS2DH12_INT1_THS       0x32
#define LIS2DH12_INT1_DURATION  0x33
#define LIS2DH12_INT1_SRC       0x31

#define LIS2DH12_WHO_AM_I_VAL   0x33

static const nrfx_twi_t m_twi = NRFX_TWI_INSTANCE(TWI_INSTANCE_ID);
static volatile bool m_twi_xfer_done;
static accel_motion_handler_t m_motion_handler;

static void twi_handler(nrfx_twi_evt_t const *p_event, void *p_context)
{
    m_twi_xfer_done = true;
}

static ret_code_t twi_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    m_twi_xfer_done = false;

    ret_code_t err = nrfx_twi_tx(&m_twi, LIS2DH12_ADDR, buf, 2, false);
    if (err != NRF_SUCCESS) return err;

    while (!m_twi_xfer_done) {}
    return NRF_SUCCESS;
}

static ret_code_t twi_read_reg(uint8_t reg, uint8_t *val)
{
    m_twi_xfer_done = false;

    ret_code_t err = nrfx_twi_tx(&m_twi, LIS2DH12_ADDR, &reg, 1, true);
    if (err != NRF_SUCCESS) return err;
    while (!m_twi_xfer_done) {}

    m_twi_xfer_done = false;
    err = nrfx_twi_rx(&m_twi, LIS2DH12_ADDR, val, 1);
    if (err != NRF_SUCCESS) return err;
    while (!m_twi_xfer_done) {}

    return NRF_SUCCESS;
}

static void int1_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    if (pin == ACCEL_INT1_PIN && m_motion_handler)
    {
        /* Read INT1_SRC to clear the interrupt latch */
        uint8_t dummy;
        twi_read_reg(LIS2DH12_INT1_SRC, &dummy);

        m_motion_handler();
    }
}

static void twi_init(void)
{
    const nrfx_twi_config_t twi_config = {
        .scl                = ACCEL_SCL_PIN,
        .sda                = ACCEL_SDA_PIN,
        .frequency          = NRF_TWI_FREQ_400K,
        .interrupt_priority = NRFX_TWI_DEFAULT_CONFIG_IRQ_PRIORITY,
        .hold_bus_uninit    = false,
    };

    ret_code_t err = nrfx_twi_init(&m_twi, &twi_config, twi_handler, NULL);
    APP_ERROR_CHECK(err);

    nrfx_twi_enable(&m_twi);
}

static void gpiote_int1_init(void)
{
    ret_code_t err;

    if (!nrfx_gpiote_is_init())
    {
        err = nrfx_gpiote_init();
        APP_ERROR_CHECK(err);
    }

    nrfx_gpiote_in_config_t in_config = NRFX_GPIOTE_CONFIG_IN_SENSE_LOTOHI(false);
    in_config.pull = NRF_GPIO_PIN_PULLDOWN;

    err = nrfx_gpiote_in_init(ACCEL_INT1_PIN, &in_config, int1_handler);
    APP_ERROR_CHECK(err);

    nrfx_gpiote_in_event_enable(ACCEL_INT1_PIN, true);
}

/**
 * Configures the LIS2DH12 for low-power motion detection:
 *   - 10 Hz ODR, low-power mode, all axes enabled
 *   - High-pass filter for interrupt generation
 *   - INT1 routed to IA1 (interrupt activity 1)
 *   - Threshold ~250 mg, duration ~100 ms (1 sample at 10 Hz)
 */
static void lis2dh12_configure_motion(void)
{
    uint8_t who;
    ret_code_t err = twi_read_reg(LIS2DH12_WHO_AM_I, &who);
    APP_ERROR_CHECK(err);

    if (who != LIS2DH12_WHO_AM_I_VAL)
    {
        NRF_LOG_ERROR("LIS2DH12 not found (WHO_AM_I=0x%02X)", who);
        return;
    }

    /* CTRL_REG1: 10 Hz ODR, low-power mode, XYZ enabled */
    twi_write_reg(LIS2DH12_CTRL_REG1, 0x2F);

    /* CTRL_REG2: High-pass filter enabled for INT1 (HP_IA1) */
    twi_write_reg(LIS2DH12_CTRL_REG2, 0x01);

    /* CTRL_REG3: IA1 interrupt on INT1 pin */
    twi_write_reg(LIS2DH12_CTRL_REG3, 0x40);

    /* CTRL_REG4: FS = ±2g, block data update */
    twi_write_reg(LIS2DH12_CTRL_REG4, 0x80);

    /* CTRL_REG5: Latch interrupt on INT1 (LIR_INT1) */
    twi_write_reg(LIS2DH12_CTRL_REG5, 0x08);

    /* CTRL_REG6: INT1 active-high (default) */
    twi_write_reg(LIS2DH12_CTRL_REG6, 0x00);

    /* INT1_THS: threshold ~250 mg (4 * 16mg per LSB at ±2g low-power) */
    twi_write_reg(LIS2DH12_INT1_THS, 0x10);

    /* INT1_DURATION: 0 (immediate, no debounce samples needed) */
    twi_write_reg(LIS2DH12_INT1_DURATION, 0x00);

    /* INT1_CFG: OR combination, detect movement on any axis (6-direction) */
    twi_write_reg(LIS2DH12_INT1_CFG, 0x2A);

    /* Dummy read to clear any pending interrupt */
    uint8_t dummy;
    twi_read_reg(LIS2DH12_INT1_SRC, &dummy);
}

void accel_init(accel_motion_handler_t handler)
{
    m_motion_handler = handler;

    twi_init();
    nrf_delay_ms(10);

    lis2dh12_configure_motion();
    gpiote_int1_init();
}

void accel_enter_low_power(void)
{
    /* Switch to 1 Hz ODR for minimal power while keeping motion detection alive */
    twi_write_reg(LIS2DH12_CTRL_REG1, 0x1F);
}
