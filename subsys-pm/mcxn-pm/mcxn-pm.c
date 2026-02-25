#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/irq.h>
#include <zephyr/device.h>
#include <cmsis_core.h>
#include "mcxn-pm.h"

// BIT MASK MACROS + HELPERS
#define BIT(n) (1U << (n))
#define REG_SET_BIT(reg, n) ((reg) |= BIT(n))
#define REG_CLR_BIT(reg, n) ((reg) &= ~ BIT(n))

void set_bits(volatile uint32_t* val, uint8_t start_bit, uint8_t bit_num, uint32_t new_val) {
    if (bit_num == 0 || bit_num > 32 || start_bit > 31 || (start_bit + bit_num) > 32) {
        return;
    }
    uint32_t field_mask = (bit_num == 32) ? 0xFFFFFFFFu : ((1u << bit_num) - 1u);
    uint32_t mask = field_mask << start_bit;
    *val = (*val & ~mask) | ((new_val & field_mask) << start_bit);
}

#define REG_2BIT_SHIFT(idx) ((idx) * 2U)
#define REG_4BIT_SHIFT(idx) ((idx) * 4U)
#define REG_SET_2BIT(regptr, idx, v) set_bits(regptr, REG_2BIT_SHIFT(idx), 2U, (uint32_t)(v))
#define REG_SET_4BIT(regptr, idx, v) set_bits(regptr, REG_4BIT_SHIFT(idx), 4u, (uint32_t)(v))

static void mk_pin_interrupt(struct pin_interrupt* interrupt, uint8_t enabled, void* user_data, wuu_cb_t cb) {
  interrupt->enabled = enabled;
  interrupt->user_data = user_data;
  interrupt->cb = cb;
}

// Interrupt table contains info about currently enabled external interrupt pins
static struct pin_interrupt pin_interrupt_table[WUU_EXTERNAL_PIN_AMT];

static void wuu_external_pin_cb_handler(const void* arg) {
  (void)arg;
  volatile uint32_t* pf = (volatile uint32_t*)(WUU_PIN_FLAG);
  int pin = -1;
  // loop through all pin flags, find the one that triggered irq
  for (uint8_t i = 0; i < WUU_EXTERNAL_PIN_AMT; i++) {
    uint8_t pf_i = (*pf >> i) & 1;
    if (pf_i) {
      pin = (int)i;
      *pf = (1u << i); // reset pin flag
      break;
    }
  } 
  // if this happened something went very wrong
  if (pin == -1) {
    return;
  }
  struct pin_interrupt last_interrupt = pin_interrupt_table[pin];
  if (last_interrupt.cb != NULL) {
    last_interrupt.cb(last_interrupt.user_data);
  }
}

static void init_mcxn_pm(const struct device* dev) {
  (void)dev;
  for (uint8_t i = 0; i < WUU_EXTERNAL_PIN_AMT; i++) {
    mk_pin_interrupt(&pin_interrupt_table[i], 0, NULL, NULL);
  }
  IRQ_CONNECT(WUU_INTERRUPT_IRQ, 0, wuu_external_pin_cb_handler, NULL, 0);
}

void wuu_external_pin_attach_cb(uint8_t pin, wuu_cb_t cb, void* user_data) {
  mk_pin_interrupt(&pin_interrupt_table[pin], 1, user_data, cb);
}

void wuu_module_attach_cb(wuu_interrupt_irq interrupt, wuu_cb_t cb, void* user_data) {
  irq_connect_dynamic(interrupt, 0, cb, user_data, 0);
}

void wuu_external_pin_enable_interrupt(uint8_t enable) {
  if (!enable) {
    irq_disable(WUU_INTERRUPT_IRQ);
    return;
  }
  NVIC_EnableIRQ(WUU_INTERRUPT_IRQ);
  NVIC_ClearPendingIRQ(WUU_INTERRUPT_IRQ);
}

void wuu_module_enable_interrupt(wuu_interrupt_irq interrupt, uint8_t enable) {
  if (!enable) {
    irq_disable(interrupt);
  }
  NVIC_EnableIRQ(interrupt);
  NVIC_ClearPendingIRQ(interrupt);
}

int wuu_cfg_external_pin(uint8_t pin, struct external_pin_cfg* cfg) {
  if (pin > WUU_EXTERNAL_PIN_AMT) {
    return -1;
  }
  volatile uint32_t* pmc = (volatile uint32_t*)(WUU_PIN_PM);
  volatile uint32_t* pf = (volatile uint32_t*)(WUU_PIN_FLAG);
  volatile uint32_t* pe1 = (volatile uint32_t*)(WUU_PIN_ENABLE1);
  volatile uint32_t* pe2 = (volatile uint32_t*)(WUU_PIN_ENABLE2);
  volatile uint32_t* pdc1 = (volatile uint32_t*)(WUU_PIN_DMATRIG1);
  volatile uint32_t* pdc2 = (volatile uint32_t*)(WUU_PIN_DMATRIG2);

  // if setting to all power modes, bit must first be set to zero before other fields
  REG_CLR_BIT(*pmc, pin);
  *pf = BIT(pin);
  if (pin > 15) {
    REG_SET_2BIT(pdc2, pin - 16, cfg->event);
    REG_SET_2BIT(pe2, pin - 16, cfg->edge);
  } else {
    REG_SET_2BIT(pdc1, pin, cfg->event);
    REG_SET_2BIT(pe1, pin, cfg->edge);
  }
  if (cfg->pm == EXTERNAL_PIN_ALL_POWER_MODES) {
    REG_SET_BIT(*pmc, pin);
  } else {
    REG_CLR_BIT(*pmc, pin);
  }
  return 0;
}

int wuu_cfg_module(uint8_t module, module_wakeup_event event) {
  volatile uint32_t* me = (volatile uint32_t*)(WUU_MODULE_ME);
  volatile uint32_t* de = (volatile uint32_t*)(WUU_MODULE_DE);
  if (module > 9) {
    return -1;
  }
  if (event == MODULE_WAKEUP_INTERRUPT) {
    REG_SET_BIT(*me, module);
  } else {
    REG_SET_BIT(*de, module);
  }
  return 0;
}

int wuu_disable_external_pin(uint8_t pin) {
  volatile uint32_t* pe1 = (volatile uint32_t*)(WUU_PIN_ENABLE1);
  volatile uint32_t* pe2 = (volatile uint32_t*)(WUU_PIN_ENABLE2);

  if (pin > 15) {
    REG_SET_2BIT(pe2, pin - 16, EXTERNAL_PIN_DISABLED);
  } else {
    REG_SET_2BIT(pe1, pin, EXTERNAL_PIN_DISABLED);
  }
}

int cmc_sram_retain(uint32_t mask) {
  volatile uint32_t* sramret = (volatile uint32_t*)(CMC_SRAM_RET);

  *sramret = mask;
  return 0;
}

int cmc_allow_dbg(uint8_t enable) {
  volatile uint32_t* dbgctl = (volatile uint32_t*)(CMC_DBG_CTRL);

  if (enable) {
    REG_SET_BIT(*dbgctl, 0);
  } else {
    REG_CLR_BIT(*dbgctl, 0);
  }
  return 0;
}

uint8_t cmc_get_last_power_state(void) {
  volatile uint32_t* ckstat = (volatile uint32_t*)(CMC_CKSTAT);

  uint8_t ckmode = *ckstat & 0xF;
  return ckmode;
}

int cmc_deep_power_down(void) {
  volatile uint32_t* ckctrl = (volatile uint32_t*)(CMC_CKCTRL); 
  volatile uint32_t* pmprot = (volatile uint32_t*)(CMC_PMPROT);
  volatile uint32_t* gpmctrl = (volatile uint32_t*)(CMC_GPMCTRL);
  REG_SET_4BIT(ckctrl, 0, CMC_CLK_DEEP); 
  REG_SET_4BIT(pmprot, 0, CMC_PMP_DEEP_POWER_DOWN);
  REG_SET_4BIT(gpmctrl, 0, CMC_GPM_DEEP_POWER_DOWN);

  // docs say last used register must be read back before WFI
  (void)*gpmctrl;
	SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
  __DSB();
  __WFI();
  __ISB();
  SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
  return 0;
}

int cmc_power_down(void) {
  volatile uint32_t* ckctrl = (volatile uint32_t*)(CMC_CKCTRL); 
  volatile uint32_t* pmprot = (volatile uint32_t*)(CMC_PMPROT);
  volatile uint32_t* gpmctrl = (volatile uint32_t*)(CMC_GPMCTRL);
  REG_SET_4BIT(ckctrl, 0, CMC_CLK_DEEP); 
  REG_SET_4BIT(pmprot, 0, CMC_PMP_POWER_DOWN);
  REG_SET_4BIT(gpmctrl, 0, CMC_GPM_POWER_DOWN);

  // docs say last use register must be read back before WFI
  (void)*gpmctrl;
	SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
  __DSB();
  __WFI();
  __ISB();
  SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
  return 0;
}

int cmc_sleep(void) {
  volatile uint32_t* ckctrl = (volatile uint32_t*)(CMC_CKCTRL); 
  REG_SET_4BIT(ckctrl, 0, CMC_CLK_SLEEP); 

  (void)*ckctrl;
	SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
  __DSB();
  __WFI();
  __ISB();
  SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
  return 0;
}

int cmc_deep_sleep(void) {
  volatile uint32_t* ckctrl = (volatile uint32_t*)(CMC_CKCTRL); 
  volatile uint32_t* pmprot = (volatile uint32_t*)(CMC_PMPROT);
  volatile uint32_t* gpmctrl = (volatile uint32_t*)(CMC_GPMCTRL);
  REG_SET_4BIT(ckctrl, 0, CMC_CLK_DEEP); 
  REG_SET_4BIT(pmprot, 0, CMC_PMP_DEEP_SLEEP);
  REG_SET_4BIT(gpmctrl, 0, CMC_GPM_DEEP_SLEEP);
  
  // docs say last use register must be read back before WFI
  (void)*gpmctrl;
	SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
  __DSB();
  __WFI();
  __ISB();
  SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
  return 0;
}

SYS_INIT(init_mcxn_pm, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
