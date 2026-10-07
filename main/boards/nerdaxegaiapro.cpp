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

    // 2-phase VR gives more current headroom: extend the ASIC frequency scale up,
    // bump the default a couple of steps and pair it with a little more core voltage.
    m_asicFrequencies = {300, 320, 340, 350, 360, 380, 400, 420, 440, 460, 480, 500};
    m_defaultAsicFrequency = m_asicFrequency = 400;
    m_absMaxAsicFrequency  = 550;   // hard ceiling for manual input
    m_asicVoltages = {900, 920, 940, 960, 980, 1000, 1020, 1040, 1060, 1080};
    m_defaultAsicVoltageMillis = m_asicVoltageMillis = 960;

    // Gauge ceilings for the doubled VR capacity (display only, not a runtime cutoff).
    m_maxPin      = 60.0;
    m_maxCurrentA = 10.0f;

    // Reuse the Gaia on-device artwork (the Gaia ctor only sets it under NERDAXEGAIA).
    m_theme = new ThemeNerdaxegaia();
}
