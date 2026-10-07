#pragma once

#include "asic.h"
#include "bm1373.h"
#include "board.h"
#include "nerdaxe.h"

class NerdaxeGaia : public NerdAxe {
  protected:
    int m_initVoltageMillis;

    // W5500 interposer present (auto-detected in the ctor).
    bool m_hasEth = false;

    // TPS546 output over-current limits and switching frequency applied in initBoard().
    // Defaults are for the 1-phase Gaia; the 2-phase Gaia Pro overrides them in its ctor.
    float m_tpsOcWarnA  = 28.0f;
    float m_tpsOcFaultA = 33.0f;
    int   m_tpsSwitchKHz = 400;

    // LDO enable line (GPIO12) — power sequencing helpers
    void LDO_enable();
    void LDO_disable();

  public:
    NerdaxeGaia();

    virtual bool initBoard();
    virtual bool initAsics();

    virtual void shutdown();

    virtual bool setVoltage(float core_voltage);

    virtual float getTemperature(int index);
    virtual float getVRTemp();
    virtual bool isPIDAvailable() { return true; }

    virtual float getVin();
    virtual float getIin();
    virtual float getPin();
    virtual float getVout();
    virtual float getIout();
    virtual float getPout();

    virtual bool hasEthernet() override { return m_hasEth; }
    virtual const EthPins *getEthPins() override;
};
