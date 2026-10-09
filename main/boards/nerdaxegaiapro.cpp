#include "nerdaxegaiapro.h"
#include "drivers/nerdaxe/TPS546.h"

NerdaxeGaiaPro::NerdaxeGaiaPro() : NerdaxeGaia()
{
    m_deviceModel = "NerdAxeGaiaPro";

    // 2-phase TPS546D24A stack. Run at the strapped 650 kHz so the strap-set loop
    // compensation matches (the 1-phase profile forces 400 kHz, which detunes it),
    // and lift the output over-current to the hardware strap limits (40 A warn /
    // 52 A fault, master+slave) instead of the 1-phase 28/33 A cap.
    m_tpsSwitchKHz = 650;
    m_tpsOcWarnA   = 40.0f;
    m_tpsOcFaultA  = 52.0f;

    // 2-phase VR gives more current headroom: extend the ASIC frequency scale up to
    // 650 MHz for testing, keep a safe default, and offer more core voltage to pair
    // with the high frequencies.
    m_asicFrequencies = {300, 350, 400, 425, 450, 475, 500, 525, 550, 575, 600, 625, 650};
    m_defaultAsicFrequency = m_asicFrequency = 400;
    m_absMaxAsicFrequency  = 650;   // hard ceiling for manual input (test headroom)
    m_asicVoltages = {900, 940, 960, 980, 1000, 1020, 1040, 1060, 1080, 1100, 1150, 1200};
    m_defaultAsicVoltageMillis = m_asicVoltageMillis = 960;

    // Gauge ceilings for the doubled VR capacity (display only, not a runtime cutoff).
    m_maxPin      = 60.0;
    m_maxCurrentA = 10.0f;

    // Reuse the Gaia on-device artwork (the Gaia ctor only sets it under NERDAXEGAIA).
    m_theme = new ThemeNerdaxegaia();
}

// The base getPin() (Vout*Iout + 5W) only models the VR input. Calibrated against two
// bench points (inline ammeter on the 12V): real board input = VR output over ~92%
// efficiency plus ~9W of housekeeping (ESP, display, fan) that the TPS546 cannot see.
//   650MHz: 43.1/0.92 + 9 = 55.9 W (measured 56.1)
//   600MHz: 36.5/0.92 + 9 = 48.7 W (measured 48.9)
// The ~9W is mostly the fan at full load; at low load/temperature it reads a little high.
float NerdaxeGaiaPro::getPin()
{
    return (TPS546_get_vout() * TPS546_get_iout()) / 0.92f + 9.0f;
}
