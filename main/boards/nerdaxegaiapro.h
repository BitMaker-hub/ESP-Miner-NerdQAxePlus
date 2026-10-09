#pragma once

#include "nerdaxegaia.h"

// NerdAxe Gaia Pro: the Gaia with a 2-phase TPS546D24A stack (master + slave) and an
// onboard W5500 on the same pins the Gaia interposer uses. The second phase is
// transparent to firmware (pin-strap + BCX; only the master answers PMBus 0x24), so
// this board just lifts the VR over-current cap, runs at the strapped switching
// frequency, and reuses the Gaia artwork. Everything else is inherited from NerdaxeGaia.
class NerdaxeGaiaPro : public NerdaxeGaia {
  public:
    NerdaxeGaiaPro();

    // Report the real 12V board input power (the base estimate only covers the VR).
    float getPin() override;
};
