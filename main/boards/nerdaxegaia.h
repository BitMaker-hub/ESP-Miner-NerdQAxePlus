#pragma once

#include "asic.h"
#include "bm1373.h"
#include "board.h"
#include "nerdaxe.h"

class NerdaxeGaia : public NerdAxe {
  protected:
    int m_initVoltageMillis;

    // W5500 ethernet interposer present — auto-detected in the constructor by
    // reading the W5500 VERSIONR over SPI (see eth-interposer docs/FIRMWARE-GAIA.md).
    bool m_hasEth = false;

    // Checks over SPI whether the W5500 interposer is present on the board.
    bool isEthConnected();

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

    // Ethernet via W5500 interposer on the display header (see FIRMWARE-GAIA.md).
    virtual bool hasEthernet() override { return m_hasEth; }
    virtual const EthPins *getEthPins() override;
};
