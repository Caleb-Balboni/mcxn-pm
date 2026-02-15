#include <zephyr/kernel.h>
#include <zephyr/init.h>
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

// Interrupt table contains info about currently enabled external interrupt pins
static struct pin_interrupt pin_interrupt_table[32];
static uint8_t pin_reg_amt = 0;

// FUNCTION IMPL

static void init_mcxn_pm(void) {
  // TODO
}

int wuu_cfg_external_pin(uint8_t pin, struct external_pin_cfg* cfg) {
  if (pin_interrupt_table[pin].enabled) {
    return -1;
  }
  pin_interrupt_table[pin].enabled = 1;
  pin_interrupt_table[pin].cb = cfg->cb;
  pin_interrupt_table[pin].user_data = cfg->user_data;
  pin_reg_amt++;
  volatile uint32_t* pmc = (volatile uint32_t*)(WUU_PIN_PM);
  volatile uint32_t* pf = (volatile uint32_t*)(WUU_PIN_FLAG);
  volatile uint32_t* pe1 = (volatile uint32_t*)(WUU_PIN_ENABLE1);
  volatile uint32_t* pe2 = (volatile uint32_t*)(WUU_PIN_ENABLE2);
  volatile uint32_t* pdc1 = (volatile uint32_t*)(WUU_PIN_DMATRIG1);
  volatile uint32_t* pdc2 = (volatile uint32_t*)(WUU_PINDMATRIG2);

  // if setting to all power modes, bit must first be set to zero before other fields
  if (cfg->pm == EXTERNAL_ALL_POWER_MODES) {
    REG_CLR_BIT(*pmc, pin);
  }
  *pf = BIN(pin);
  if (pin > 15) {
    REG_SET_2BIT(pdc2, pin - 16, cfg->event);
    REG_SET_2BIT(pe2, pin - 16, cfg->edge);
  } else {
    REG_SET_2BIT(pdc1, pin, cfg->event);
    REG_SET_2BIT(pe1, pin, cfg->edge);
  }
  REG_SET_BIT(*pmc, pin);
  return 0;
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

int cmc_deep_power_down(void) {
  if (pin_reg_amt >= 0) {
    return -1;
  }
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
  if (pin_reg_amt >= 0) {
    return -1;
  }
  volatile uint32_t* ckctrl = (volatile uint32_t*)(CMC_CKCTRL); 
  volatile uint32_t* pmprot = (volatile uint32_t*)(CMC_PMPROT);
  volatile uint32_t* gpmctrl = (volatile uint32_t*)(CMC_GPMCTRL);
  REG_SET_4BIT(ckctrl, 0, CMC_CLK_DEEP); 
  REG_SET_4BIT(pmprot, 0, CMC_PMP_POWER_DOWN);
  REG_SET_4BIT(gpmctrl, 0, CMC_GPM_POWER_DOWN);

  // docs say last use register must be read back before WFI
  (void*)gpmctrl;
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

  (void*)ckctrl;
	SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
  __DSB();
  __WFI();
  __ISB();
  SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
  return 0;
}

int cmc_deep_sleep(void) {
  if (pin_reg_amt >= 0) {
    return -1;
  }
  volatile uint32_t* ckctrl = (volatile uint32_t*)(CMC_CKCTRL); 
  volatile uint32_t* pmprot = (volatile uint32_t*)(CMC_PMPROT);
  volatile uint32_t* gpmctrl = (volatile uint32_t*)(CMC_GPMCTRL);
  REG_SET_4BIT(ckctrl, 0, CMC_CLK_DEEP); 
  REG_SET_4BIT(pmprot, 0, CMC_PMP_DEEP_SLEEP);
  REG_SET_4BIT(gpmctrl, 0, CMC_GPM_DEPP_SLEEP);

  // docs say last use register must be read back before WFI
  (void*)gpmctrl;
	SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
  __DSB();
  __WFI();
  __ISB();
  SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
  return 0;
}

SYS_INIT(init_mcxn_pm, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
