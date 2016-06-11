/************************************************************************************
 * configs/sama5d3-xplained/src/sam_ethernet.c
 *
 *   Copyright (C) 2014 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ************************************************************************************/

/************************************************************************************
 * Included Files
 ************************************************************************************/

#include <nuttx/config.h>

/* Force verbose debug on in this file only to support unit-level testing. */

#ifdef CONFIG_NETDEV_PHY_DEBUG
#  undef  CONFIG_DEBUG_INFO
#  define CONFIG_DEBUG_INFO 1
#  undef  CONFIG_DEBUG_NET
#  define CONFIG_DEBUG_NET 1
#endif

#include <string.h>
#include <assert.h>
#include <debug.h>

#include <nuttx/irq.h>
#include <nuttx/arch.h>

#include "sam_pio.h"
#include "sam_ethernet.h"

#include "sama5d3-xplained.h"

#ifdef HAVE_NETWORK

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/

#ifndef CONFIG_SAMA5_EMACA
#  undef CONFIG_SAMA5_EMAC_ISETH0
#endif

#ifdef CONFIG_SAMA5_EMAC_ISETH0
#  define SAMA5_EMAC_DEVNAME "eth0"
#  define SAMA5_GMAC_DEVNAME "eth1"
#else
#  define SAMA5_GMAC_DEVNAME "eth0"
#  define SAMA5_EMAC_DEVNAME "eth1"
#endif

/* Debug ********************************************************************/
/* Extra, in-depth debug output that is only available if
 * CONFIG_NETDEV_PHY_DEBUG us defined.
 */

#ifdef CONFIG_NETDEV_PHY_DEBUG
#  define phydbg    dbg
#  define phyllerr  llerr
#else
#  define phydbg(x...)
#  define phyllerr(x...)
#endif

/************************************************************************************
 * Private Data
 ************************************************************************************/

#ifdef CONFIG_SAMA5_PIOE_IRQ
#ifdef CONFIG_SAMA5_EMACA
static xcpt_t g_emac_handler;
#endif
#ifdef CONFIG_SAMA5_GMAC
static xcpt_t g_gmac_handler;
#endif
#endif

/************************************************************************************
 * Private Functions
 ************************************************************************************/

/************************************************************************************
 * Name: sam_emac_phy_enable and sam_gmac_enable
 ************************************************************************************/

#ifdef CONFIG_SAMA5_PIOE_IRQ
#ifdef CONFIG_SAMA5_EMACA
static void sam_emac_phy_enable(bool enable)
{
  phydbg("IRQ%d: enable=%d\n", IRQ_INT_ETH1, enable);
  if (enable)
    {
      sam_pioirqenable(IRQ_INT_ETH1);
    }
  else
    {
      sam_pioirqdisable(IRQ_INT_ETH1);
    }
}

#endif

#ifdef CONFIG_SAMA5_GMAC
static void sam_gmac_phy_enable(bool enable)
{
  phydbg("IRQ%d: enable=%d\n", IRQ_INT_ETH0, enable);
  if (enable)
    {
      sam_pioirqenable(IRQ_INT_ETH0);
    }
  else
    {
      sam_pioirqdisable(IRQ_INT_ETH0);
    }
}
#endif
#endif

/************************************************************************************
 * Public Functions
 ************************************************************************************/

/************************************************************************************
 * Name: sam_netinitialize
 *
 * Description:
 *   Configure board resources to support networking.
 *
 ************************************************************************************/

void weak_function sam_netinitialize(void)
{
#ifdef CONFIG_SAMA5_EMACA
  /* Ethernet 10/100 (EMAC A) Port
   *
   * The main board contains a MICREL PHY device (KSZ8051) operating at 10/100 Mbps.
   * The board supports MII and RMII interface modes.
   *
   * The two independent PHY devices embedded on CM and MB boards are connected to
   * independent RJ-45 connectors with built-in magnetic and status LEDs.
   *
   * At the De-Assertion of Reset:
   *   PHY ADD[2:0]:001
   *   CONFIG[2:0]:001,Mode:RMII
   *   Duplex Mode:Half Duplex
   *   Isolate Mode:Disable
   *   Speed Mode:100Mbps
   *   Nway Auto-Negotiation:Enable
   *
   * The KSZ8051 PHY interrupt is available on PE30 INT_ETH1
  */

  phydbg("Configuring %08x\n", PIO_INT_ETH1);
  sam_configpio(PIO_INT_ETH1);
#endif

#ifdef CONFIG_SAMA5_GMAC
  /* Tri-Speed Ethernet PHY
   *
   * The SAMA5D3 series-CM board is equipped with a MICREL PHY devices (MICREL
   * KSZ9021/31) operating at 10/100/1000 Mbps. The board supports RGMII interface
   * mode. The Ethernet interface consists of 4 pairs of low voltage differential
   * pair signals designated from GRX� and GTx� plus control signals for link
   * activity indicators. These signals can be used to connect to a 10/100/1000
   * BaseT RJ45 connector integrated on the main board.
   *
   * The KSZ9021/31 interrupt is available on PB35 INT_GETH0
   */

  phydbg("Configuring %08x\n", PIO_INT_ETH0);
  sam_configpio(PIO_INT_ETH0);
#endif
}

/****************************************************************************
 * Name: arch_phy_irq
 *
 * Description:
 *   This function may be called to register an interrupt handler that will
 *   be called when a PHY interrupt occurs.  This function both attaches
 *   the interrupt handler and enables the interrupt if 'handler' is non-
 *   NULL.  If handler is NULL, then the interrupt is detached and disabled
 *   instead.
 *
 *   The PHY interrupt is always disabled upon return.  The caller must
 *   call back through the enable function point to control the state of
 *   the interrupt.
 *
 *   This interrupt may or may not be available on a given platform depending
 *   on how the network hardware architecture is implemented.  In a typical
 *   case, the PHY interrupt is provided to board-level logic as a GPIO
 *   interrupt (in which case this is a board-specific interface and really
 *   should be called board_phy_irq()); In other cases, the PHY interrupt
 *   may be cause by the chip's MAC logic (in which case arch_phy_irq()) is
 *   an appropriate name.  Other other boards, there may be no PHY interrupts
 *   available at all.  If client attachable PHY interrupts are available
 *   from the board or from the chip, then CONFIG_ARCH_PHY_INTERRUPT should
 *   be defined to indicate that fact.
 *
 *   Typical usage:
 *   a. OS service logic (not application logic*) attaches to the PHY
 *      PHY interrupt and enables the PHY interrupt.
 *   b. When the PHY interrupt occurs:  (1) the interrupt should be
 *      disabled and () work should be scheduled on the worker thread (or
 *      perhaps a dedicated application thread).
 *   c. That worker thread should use the SIOCGMIIPHY, SIOCGMIIREG,
 *      and SIOCSMIIREG ioctl calls** to communicate with the PHY,
 *      determine what network event took place (Link Up/Down?), and
 *      take the appropriate actions.
 *   d. It should then interact the the PHY to clear any pending
 *      interrupts, then re-enable the PHY interrupt.
 *
 *    * This is an OS internal interface and should not be used from
 *      application space.  Rather applications should use the SIOCMIISIG
 *      ioctl to receive a signal when a PHY event occurs.
 *   ** This interrupt is really of no use if the Ethernet MAC driver
 *      does not support these ioctl calls.
 *
 * Input Parameters:
 *   intf    - Identifies the network interface.  For example "eth0".  Only
 *             useful on platforms that support multiple Ethernet interfaces
 *             and, hence, multiple PHYs and PHY interrupts.
 *   handler - The client interrupt handler to be invoked when the PHY
 *             asserts an interrupt.  Must reside in OS space, but can
 *             signal tasks in user space.  A value of NULL can be passed
 *             in order to detach and disable the PHY interrupt.
 *   enable  - A function pointer that be unsed to enable or disable the
 *             PHY interrupt.
 *
 * Returned Value:
 *   The previous PHY interrupt handler address is returned.  This allows you
 *   to temporarily replace an interrupt handler, then restore the original
 *   interrupt handler.  NULL is returned if there is was not handler in
 *   place when the call was made.
 *
 ****************************************************************************/

#ifdef CONFIG_SAMA5_PIOE_IRQ
xcpt_t arch_phy_irq(FAR const char *intf, xcpt_t handler, phy_enable_t *enable)
{
  irqstate_t flags;
  xcpt_t *phandler;
  xcpt_t oldhandler;
  pio_pinset_t pinset;
  phy_enable_t enabler;
  int irq;

  DEBUGASSERT(intf);

  ninfo("%s: handler=%p\n", intf, handler);
#ifdef CONFIG_SAMA5_EMACA
  phydbg("EMAC: devname=%s\n", SAMA5_EMAC_DEVNAME);
#endif
#ifdef CONFIG_SAMA5_GMAC
  phydbg("GMAC: devname=%s\n", SAMA5_GMAC_DEVNAME);
#endif

#ifdef CONFIG_SAMA5_EMACA
  if (strcmp(intf, SAMA5_EMAC_DEVNAME) == 0)
    {
      phydbg("Select EMAC\n");
      phandler = &g_emac_handler;
      pinset   = PIO_INT_ETH1;
      irq      = IRQ_INT_ETH1;
      enabler  = sam_emac_phy_enable;
    }
  else
#endif
#ifdef CONFIG_SAMA5_GMAC
  if (strcmp(intf, SAMA5_GMAC_DEVNAME) == 0)
    {
      phydbg("Select GMAC\n");
      phandler = &g_gmac_handler;
      pinset   = PIO_INT_ETH0;
      irq      = IRQ_INT_ETH0;
      enabler  = sam_gmac_phy_enable;
    }
  else
#endif
    {
      ndbg("Unsupported interface: %s\n", intf);
      return NULL;
    }

  /* Disable interrupts until we are done.  This guarantees that the
   * following operations are atomic.
   */

  flags = enter_critical_section();

  /* Get the old interrupt handler and save the new one */

  oldhandler = *phandler;
  *phandler = handler;

  /* Configure the interrupt */

  if (handler)
    {
      phydbg("Configure pin: %08x\n", pinset);
      sam_pioirq(pinset);

      phydbg("Attach IRQ%d\n", irq);
      (void)irq_attach(irq, handler);
    }
  else
    {
      phydbg("Detach IRQ%d\n", irq);
      (void)irq_detach(irq);
      enabler = NULL;
    }

  /* Return with the interrupt disabled in either case */

  sam_pioirqdisable(irq);

  /* Return the enabling function pointer */

  if (enable)
    {
      *enable = enabler;
    }

  /* Return the old handler (so that it can be restored) */

  leave_critical_section(flags);
  return oldhandler;
}
#endif /* CONFIG_SAMA5_PIOE_IRQ */

#endif /* HAVE_NETWORK */
