#pragma once

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_eth.h"
#include "esp_netif.h"

/* Simple hook signatures (no fancy C++) */
typedef void (*W5500HookFn)();

class W5500 {
  public:
    W5500();

    /* Call once after esp_netif_init() + esp_event_loop_create_default() */
    esp_err_t init();

    void setHookLinkUp(W5500HookFn fn)
    {
        m_hookLinkUp = fn;
    }
    void setHookLinkDown(W5500HookFn fn)
    {
        m_hookLinkDown = fn;
    }
    void setHookGotIp(W5500HookFn fn)
    {
        m_hookGotIp = fn;
    }

    esp_netif_t *getNetif() const
    {
        return m_ethNetif;
    }
    bool isLinkUp() const
    {
        return m_linkUp;
    }
    bool hasIp() const
    {
        return m_hasIp;
    }
    const char *getIpStr() const
    {
        return m_ipAddr;
    }

    esp_err_t earlySpiInit();

    /* Override the default pins before earlySpiInit(). Pass -1 to disable a pin:
       rst=-1 => no GPIO reset (RC on the board), irq=-1 => polling. */
    void setPins(int sclk, int mosi, int miso, int cs, int rst, int irq);

  private:
    static void makeEthMacFromEfuse(uint8_t out_mac[6]);
    static void setEthMac(esp_eth_handle_t eth_handle, const char *tag);
    static void hwResetGpio(gpio_num_t rst);

    /* Software reset over SPI (write MR RST bit). Needed because the interposer's
       reset is an RC that only fires on power-up, so a software reboot of the ESP
       would otherwise leave the W5500 in its previous state. */
    void swReset();

    void onLinkUp();
    void onLinkDown();
    void onGotIp();

    static void ethEventHandlerTrampoline(void *arg, esp_event_base_t base, int32_t id, void *data);
    static void ipEventHandlerTrampoline(void *arg, esp_event_base_t base, int32_t id, void *data);

    void handleEthEvent(int32_t id);
    void handleIpEvent(int32_t id, void *data);

  private:
    /* Pins (int, so -1 means "not used": rst=-1 => RC reset, no GPIO;
       int=-1 => polling instead of an interrupt line). Defaults are the q1373's. */
    int m_pinMosi = 12;
    int m_pinMiso = 16;
    int m_pinSclk = 2;
    int m_pinCs = 21;
    int m_pinRst = 13;
    int m_pinInt = 11;

    /* State */
    bool m_inited = false;
    bool m_linkUp = false;
    bool m_hasIp = false;
    char m_ipAddr[20] = "0.0.0.0";

    esp_netif_t *m_ethNetif = nullptr;
    esp_eth_handle_t m_ethHandle = nullptr;

    /* Hooks */
    W5500HookFn m_hookLinkUp = nullptr;
    W5500HookFn m_hookLinkDown = nullptr;
    W5500HookFn m_hookGotIp = nullptr;

    const spi_host_device_t spi_host = SPI2_HOST;
};
