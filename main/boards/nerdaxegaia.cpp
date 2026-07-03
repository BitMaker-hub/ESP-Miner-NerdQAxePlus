#include <math.h>
#include "serial.h"
#include "board.h"
#include "nvs_config.h"
#include "nerdaxegaia.h"

#include "drivers/nerdaxe/DS4432U.h"
#include "drivers/nerdaxe/EMC2101.h"
#include "drivers/nerdaxe/INA260.h"
#include "drivers/nerdaxe/adc.h"
#include "drivers/nerdaxe/TPS546.h"

#define BM1373_RST_PIN GPIO_NUM_1
#define LDO_EN_PIN GPIO_NUM_12   // LDO enable (active-high) — new Gaia board
#define GAIA_POWER_OFFSET 5

bool tempinit_gaia = false;

static const char* TAG="nerdaxeGaia";

#define MAX(a,b) ((a)>(b)?(a):(b))

NerdaxeGaia::NerdaxeGaia() : NerdAxe() {
    m_deviceModel = "NerdAxeGaia";
    m_miningAgent = "NerdAxe";
    m_asicModel = "BM1373";
    m_version = 200;
    m_asicCount = 1;

    m_asicJobIntervalMs = 1500;
    m_asicFrequencies = {300, 350, 400, 425, 450, 475, 500};
    m_asicVoltages = {900, 950, 1000, 1050, 1100, 1150, 1200};
    m_defaultAsicFrequency = m_asicFrequency = 400;
    m_defaultAsicVoltageMillis = m_asicVoltageMillis = 1000;
    // m_absMaxAsicFrequency = 750;
    // m_absMaxAsicVoltageMillis = 1300;
    m_initVoltageMillis = 1000;
    m_fanInvertPolarity = false;
    m_fanPerc = 100;
    m_flipScreen = true;
    m_vr_maxTemp = TPS546_THROTTLE_TEMP; //Set max voltage regulator temp

    m_pidSettings[0].targetTemp = 60;
    m_pidSettings[0].p =  600; // 6.00
    m_pidSettings[0].i =   10; // 0.1
    m_pidSettings[0].d = 1000; // 10.00

    m_maxPin = 25.0;
    m_minPin = 5.0;
    m_maxVin = 5.5;
    m_minVin = 4.5;
    m_minCurrentA = 0.0f;
    m_maxCurrentA = 6.0f;

    m_asicMaxDifficulty = 2048;
    m_asicMinDifficulty = 512;
    m_asicMinDifficultyDualPool = 256;

#ifdef NERDAXEGAIA
    m_theme = new ThemeNerdaxegaia();
#endif

    m_swarmColorName = "#e7cf00"; // yellow

    m_asics = new BM1373();
    m_hasHashCounter = true;
    m_vrFrequency = m_defaultVrFrequency = m_asics->getDefaultVrFrequency();
}


bool NerdaxeGaia::initBoard()
{
    Board::initBoard();

    ADC_init();
    SERIAL_init();

    // Init I2C
    if (i2c_master_init() != ESP_OK) {
        ESP_LOGE(TAG, "I2C initializing failed");
        return false;
    }

    EMC2101_init(m_fanInvertPolarity);
    EMC2101_set_ideality_factor(EMC2101_IDEALITY_1_0319);
    EMC2101_set_beta_compensation(EMC2101_BETA_11);
    setFanSpeed(m_fanPerc);

    //Init voltage controller
    if (TPS546_init() != ESP_OK) {
        ESP_LOGE(TAG, "TPS546 init failed!");
        return ESP_FAIL;
    }
    TPS546_set_frequency(400);
    setVoltage(0.0);

    gpio_pad_select_gpio(BM1373_RST_PIN);
    gpio_set_direction(BM1373_RST_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(BM1373_RST_PIN, 0);

    // LDO enable line: configured as output and kept OFF until the ASIC
    // power-up sequence in initAsics() brings it up.
    gpio_pad_select_gpio(LDO_EN_PIN);
    gpio_set_direction(LDO_EN_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LDO_EN_PIN, 0);

    return true;
}

void NerdaxeGaia::shutdown() {
    setVoltage(0.0);

    // let the core rail collapse before cutting the LDO
    vTaskDelay(pdMS_TO_TICKS(500));

    LDO_disable();

    vTaskDelay(pdMS_TO_TICKS(500));

    Board::shutdown();
}

void NerdaxeGaia::LDO_enable() {
    ESP_LOGI(TAG, "Enable LDO");
    gpio_set_level(LDO_EN_PIN, 1);
}

void NerdaxeGaia::LDO_disable() {
    ESP_LOGI(TAG, "Disable LDO");
    gpio_set_level(LDO_EN_PIN, 0);
}

bool NerdaxeGaia::initAsics() {

    // core buck off + LDO off for a clean power-up state.
    // NOTE: selfTest() (inherited from NerdAxe) powers the chip through this
    // same initAsics() path, so the LDO is enabled during the chip test too.
    setVoltage(0.0);
    LDO_disable();

    // wait 500ms
    vTaskDelay(pdMS_TO_TICKS(500));

    // set reset low
    gpio_set_level(BM1373_RST_PIN, 0);

    // wait 250ms
    vTaskDelay(pdMS_TO_TICKS(250));

    // enable the LDO before ramping the core voltage
    LDO_enable();

    // wait 100ms
    vTaskDelay(pdMS_TO_TICKS(100));

    // set the init voltage
    // use the higher voltage for initialization
    setVoltage((float) MAX(m_initVoltageMillis, m_asicVoltageMillis) / 1000.0f);

    // wait 500ms
    vTaskDelay(pdMS_TO_TICKS(500));

    m_isBuckInitialized = true;

    // release reset pin
    gpio_set_level(BM1373_RST_PIN, 1);

    // delay for 250ms
    vTaskDelay(pdMS_TO_TICKS(250));

    SERIAL_clear_buffer();
    m_chipsDetected = m_asics->init(m_asicFrequency, m_asicCount, m_asicMaxDifficulty, m_vrFrequency);
    if (!m_chipsDetected) {
        ESP_LOGE(TAG, "error initializing asics!");
        return false;
    }
    int maxBaud = m_asics->setMaxBaud();
    // no idea why a delay is needed here starting with esp-idf 5.4 🙈
    vTaskDelay(pdMS_TO_TICKS(500));
    SERIAL_set_baud(maxBaud);
    SERIAL_clear_buffer();

    vTaskDelay(pdMS_TO_TICKS(500));

    m_isInitialized = true;
    return true;
}

bool NerdaxeGaia::setVoltage(float core_voltage)
{
    if (!validateVoltage(core_voltage)) {
        return false;
    }

    ESP_LOGI(TAG, "Set ASIC voltage = %.3fV", core_voltage);
    return TPS546_set_vout(core_voltage);
}

float NerdaxeGaia::getTemperature(int index) {

    if (!m_isInitialized) {
        return EMC2101_get_internal_temp() + 5;
    }

    if (index > 0) {
        return 0.0f;
    }

    //Reading ASIC temp
    float asic_temp = EMC2101_get_external_temp();
    ESP_LOGI(TAG, "Read ASIC temp = %.3fºC", asic_temp);
    return asic_temp; //External board Temp
}

float NerdaxeGaia::getVRTemp() {
    //Reading voltage regulator temp
    float vr_temp = TPS546_get_temperature();
    ESP_LOGI(TAG, "Read vr temp = %.3fºC", vr_temp);
    return vr_temp; //- vr_temp (voltage regulator temp)
}

float NerdaxeGaia::getVin() {
    return TPS546_get_vin();
}

float NerdaxeGaia::getIin() {
    float vin = getVin();
    if (!vin) {
        return 0.0f;
    }

    return getPin() / vin;
}

float NerdaxeGaia::getPin() {
    return (TPS546_get_vout() * TPS546_get_iout()) + GAIA_POWER_OFFSET;
}

float NerdaxeGaia::getVout() {
    return ADC_get_vcore() / 1000.0;
}

float NerdaxeGaia::getIout() {
    return TPS546_get_iout();
}

float NerdaxeGaia::getPout() {
    return getPin();
}

