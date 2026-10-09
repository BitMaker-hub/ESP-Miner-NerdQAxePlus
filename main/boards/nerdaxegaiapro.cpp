#include "nerdaxegaiapro.h"

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
