#ifndef MCXN947_PM_H
#define MCXN947_PM_H

#include <zephyr/kernel.h>

// WAKEUP UNIT (WUU) PARAMS

// WUU INTERRUPTS

typedef enum wuu_interrupt_irq_t {
  WUU_INTERRUPT_IRQ       = 147, // WUU interrupt number (must be enabled for external pins)
  SPC0_INTERRUPT_IRQ      = 146, // system power controller interrupt
  VBAT0_INTERRUPT_IRQ     = 99,  // VBAT or digital tamper interrupt
  RTC0_INTERRUPT_IRQ      = 52,  // RTC subsystem interrupt
  LPTMR0_INTERRUPT_IRQ    = 143, // Low power timer zero
  LPTMR1_INTERRUPT_IRQ    = 144, // Low power timer one
  GPIO5_0_INTERRUPT_IRQ   = 27,  // GPIO 5 Interrupt zero 
  GPIO5_1_INTERRUPT_IRQ   = 28,  // GPIO 5 Interrupt one
  CMP0_INTERRUPT_IRQ      = 109, // Comparator zero interrupt
  CMP1_INTERRUPT_IRQ      = 110, // Comparator one interrupt
} wuu_interrupt_irq;

#define WUU_DMAMUX_NUM 17

#define WUU_EXTERNAL_PIN_AMT 30
#define WUU_INTERNAL_MODULE_AMT 10

// All info pertaining to the WUU can be found beginning in the reference manual on page 831
// page numbers provided below point to the starting page of the information on configuration 
// of each param for the WUU
#define WUU_BASE          (0x40046000)        // Base address             page: 837
#define WUU_PIN_ENABLE1   (WUU_BASE + 0x8)    // Pin Enable       (PE1)   page: 839
#define WUU_PIN_ENABLE2   (WUU_BASE + 0xC)    // Pin Enable       (PE2)   page: 845
#define WUU_PIN_FLAG      (WUU_BASE + 0x20)   // Pin Flag         (PF)    page: 857
#define WUU_PIN_PM        (WUU_BASE + 0x50)   // Pin Mode Cfg     (PMC)   page: 873
#define WUU_PIN_DMATRIG1  (WUU_BASE + 0x38)   // Pin DMA/Trig Cfg (PDC1)  page: 864
#define WUU_PIN_DMATRIG2  (WUU_BASE + 0x3C)   // Pin DMA/Trig Cfg (PDC2)  page: 868

// PDC
typedef enum external_pin_wakeup_event_t {
	EXTERNAL_PIN_WAKEUP_INTERRUPT     = 0x00,   // WUU triggers interrupt on wakeup pin activation
	EXTERNAL_PIN_WAKEUP_DMA_REQUEST   = 0x1U,   // WUU triggers DMA Request upon pin activation
	EXTERNAL_PIN_WAKEUP_TRIGGER_EVENT = 0x2U,   // WUU triggers a trigger request upon pin activation
} external_pin_wakeup_event;

// PE
typedef enum external_pin_edge_detection_t {
  EXTERNAL_PIN_DISABLED             = 0x0U,   // External pin is disabled
	EXTERNAL_PIN_EDGE_RISING          = 0x1U,   // External pin triggers event on rising level
	EXTERNAL_PIN_EDGE_FALLING         = 0x2U,   // External pin triggers event on falling level
	EXTERNAL_PIN_EDGE_ANY             = 0x3U,   // External pin triggers event on any level change
} external_pin_edge_detection;

// PMC
typedef enum external_pin_pm_t {
  EXTERNAL_PIN_LOW_LEAKAGE_MODE     = 0x00U,  // External pin can only fire when in a deep sleep power down, or deep power down 
  EXTERNAL_PIN_ALL_POWER_MODES      = 0x01U,  // External pin can fire at any time (any sleep state)
} external_pin_pm;

// user set callback function - WUU interrupt handler
typedef void (*wuu_cb_t)(void* user_data);

struct pin_interrupt {
  uint8_t enabled;
  void* user_data;
  wuu_cb_t cb;
};

// Configuration for WUU external pin
struct external_pin_cfg {
  enum external_pin_wakeup_event_t event;
  enum external_pin_edge_detection_t edge;
  enum external_pin_pm_t pm;
};

// TODO --> add configurations for modules in the WUU

// CORE MODE CONTROLLER (CMC) PARAMS

#define CMC_BASE      (0x40048000)              // Base address               page: 1437
#define CMC_CKCTRL    (CMC_BASE + 0x10)         // Clock Control   (CKCTRL)   page: 1439 
#define CMC_GPMCTRL   (CMC_BASE + 0x1C)         // Global Pwr Mode (GPMCTRL)  page: 1443
#define CMC_PMPROT    (CMC_BASE + 0x18)         // Pwr Mode Prtct  (PMPROT)   page: 1442
#define CMC_SRAM_RET  (CMC_BASE + 0xD0)         // SRAM Retention  (SRAMRET0) page: 1463
#define CMC_DBG_CTRL  (CMC_BASE + 0x120)        // Debug Control   (DBGCTL)   page: 1468
#define CMC_CKSTAT    (CMC_BASE + 0x14)         // Clock Status    (CKSTAT)   page: 1440

// CKCTRL
typedef enum cmc_clock_control_t {
  CMC_CLK_SLEEP                   = 0x01U,    // Clock cfg for sleep mode
  CMC_CLK_DEEP                    = 0x0FU,    // Clock cfg for any deep sleep mode (deep pwr down, pwr, down, deep sleep)
} cmc_clock_control;

// PMPROT
typedef enum cmc_power_mode_protect_t {
  CMC_PMP_ALLOW_ALL               = 0xFU,
  CMC_PMP_DEEP_POWER_DOWN         = 0x8U,
  CMC_PMP_POWER_DOWN              = 0x2U,
  CMC_PMP_DEEP_SLEEP              = 0x1U,
} cmc_power_mode_protect;

// GPMCTRL
typedef enum cmc_global_power_mode_t {
  CMC_GPM_DEEP_POWER_DOWN         = 0xFU,
  CMC_GPM_POWER_DOWN              = 0x7U,
  CMC_GPM_DEEP_SLEEP              = 0x3U,
  CMC_GPM_SLEEP                   = 0x1U,
} cmc_global_power_mode;

// MCXN947 API Functions

// Enables or disabled the interrupt for external pins.
// @param enable - 0 to disable the interrupt, any other val to enable
void wuu_external_pin_enable_interrupt(uint8_t enable);

// Enables or disables the interrupt for modules with interrupt support.
// @param interrupt - the particular interrupt to enable or disable
// @param enable - 0 to disable the interrupt, any other value to enable the interrupt
void wuu_module_enable_interrupt(wuu_interrupt_irq interrupt, uint8_t enable);

// attaches a user defined callback function to the particular pin.
// The users function will fire when the interrupt is triggered
// @param pin - the pin the set the callback for
// @param cb - the callback function to use 
// @param user_data - user data passed to the callback function
void wuu_external_pin_attach_cb(uint8_t pin, wuu_cb_t cb, void* user_data);

// attaches a user defined callback function to a particular interrupt generated by a module.
// @param interrupt - the particular module interrupt to attach this callback function to
// @param cb - the particular user defined callback function to attach this interrupt to
// @param user_data - the user data passed to the callback function
void wuu_module_attach_cb(wuu_interrupt_irq interrupt, wuu_cb_t cb, void* user_data);

// configures an external pin as a wake up source from a sleep state.
// @param pin - the external pin which will act as the wake up source (0 - WUU_EXTERNAL_PIN_AMT)
// @param cfg - the configuration for the pin and a user defined callback function upon wake (can be NULL if unneeded)
// @return - returns 0 upon success or -1 if the pin could not be configured
int wuu_cfg_external_pin(uint8_t pin, struct external_pin_cfg* cfg); 

// disables the external pin as a wake up source, and removes the callback function
// @param pin - the pin to disable (0 - WUU_EXTERNAL_PIN_AMT)
// @return - returns 0 upon success or -1 if the pin could not be configured
int wuu_disable_external_pin(uint8_t pin);

// checks the last power state that the core went into
// @return - CKMODE 0000b (never entered a lower power mode), 0001b went into sleep mode, 1111b went into a low power mode
uint8_t cmc_get_last_power_state(void);

// configures which partitions of the SRAM will be retained in the power-down state.
// IMPORTANT NOTE: setting a bit to zero means the SRAM will be retained for that partition, if one it will not be retained
// @param mask - the mask defining which SRAM partitions will be retained.
// @return - 0 if the operation was a success, -1 otherwise
int cmc_sram_retain(uint32_t mask);

// allows debugging to occur while the core is sleeping. This should only be enabled for debugging purposes, as it does directly
// effect the sleeping state by keeping the core clock running.
// @param enable - 0 if dbg should be not be enabled, any other value otherwise
// @return - 0 if the operation was a success, -1 otherwise
int cmc_allow_dbg(uint8_t enable);

// puts the MCU into a deep power down state. All core, peripherals and SRAM will no be retained upon wake.
// In deep power down mode wake will cause the entire system to reset, only the WUU can wake the MCU.
// @return - 0 upon successfully waking up, or -1 if a module or pin has not been configured for wake yet
int cmc_deep_power_down(void);

// puts the MCU into power down mode. Both the core and peripherals will be shut down but there state will be retained.
// SRAM is optionally retained via the cmc_sram_retain function. Only the WUU can wake the MCU.
// @return - 0 upon successfully waking up, or -1 if a module or pin has not been configured for wake yet
int cmc_power_down(void);

// puts the MCU into sleep mode. In sleep mode both the core and peripherals remain powered until an interrupt or event occurs.
// Both the WUU (if the pin configured for all power modes), or an interrupt/event can wake the MCU from this mode. All SRAM retained.
// @return - 0 upon successfully waking up, or -1 if a module or pin has not been configured for wake yet
int cmc_sleep(void);

// puts the MCU into deep sleep mode. In deep sleep the core is stopped and some peripherals may be stopped but states will be retained.
// Only the WUU and NVIC can wake the MCU from this mode. All SRAM retained.
// @return - 0 upon successfully waking up, or -1 if a module or pin has not been configured for wake yet
int cmc_deep_sleep(void);

#endif
