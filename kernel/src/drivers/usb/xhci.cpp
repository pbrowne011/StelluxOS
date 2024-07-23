#include "xhci.h"
#include <paging/page.h>
#include <paging/phys_addr_translation.h>
#include <paging/tlb.h>
#include <memory/kmemory.h>
#include <time/ktime.h>
#include <arch/x86/ioapic.h>
#include <interrupts/interrupts.h>
#include <kprint.h>

namespace drivers {
    XhciDriver g_globalXhciInstance;

    void* _allocXhciMemory(size_t size, size_t alignment = 64, size_t boundary = PAGE_SIZE) {
        // Allocate extra memory to ensure we can align the block within the boundary
        size_t totalSize = size + boundary - 1;
        void* memblock = kmallocAligned(totalSize, alignment);

        if (!memblock) {
            kuPrint("[XHCI] ======= MEMORY ALLOCATION PROBLEM =======\n");
            while (true);
        }

        // Align the memory block to the specified boundary
        size_t alignedAddress = ((size_t)memblock + boundary - 1) & ~(boundary - 1);
        void* aligned = (void*)alignedAddress;

        // Mark the aligned memory block as uncacheable
        paging::markPageUncacheable(aligned);

        return aligned;
    }

    const char* extendedCapabilityToString(XhciExtendedCapabilityCode capid) {
        uint8_t id = static_cast<uint8_t>(capid);

        switch (capid) {
        case XhciExtendedCapabilityCode::Reserved: return "Reserved";
        case XhciExtendedCapabilityCode::UsbLegacySupport: return "USB Legacy Support";
        case XhciExtendedCapabilityCode::SupportedProtocol: return "Supported Protocol";
        case XhciExtendedCapabilityCode::ExtendedPowerManagement: return "Extended Power Management";
        case XhciExtendedCapabilityCode::IOVirtualizationSupport: return "I/O Virtualization Support";
        case XhciExtendedCapabilityCode::LocalMemorySupport: return "Local Memory Support";
        case XhciExtendedCapabilityCode::UsbDebugCapabilitySupport: return "USB Debug Capability Support";
        case XhciExtendedCapabilityCode::ExtendedMessageInterruptSupport: return "Extended Message Interrupt Support";
        default: break;
        }

        if (id >= 7 && id <= 9) {
            return "Reserved";
        }

        if (id >= 11 && id <= 16) {
            return "Reserved";
        }

        if (id >= 18 && id <= 191) {
            return "Reserved";
        }

        return "Vendor Specific";
    }

    XhciExtendedCapability::XhciExtendedCapability(volatile uint32_t* capPtr)
    : m_base(capPtr) {
        m_entry.raw = *m_base;
        _readNextExtCaps();
    }

    void XhciExtendedCapability::_readNextExtCaps() {
        if (m_entry.next) {
            auto nextCapPtr = XHCI_NEXT_EXT_CAP_PTR(m_base, m_entry.next);
            m_next = kstl::SharedPtr<XhciExtendedCapability>(
                new XhciExtendedCapability(nextCapPtr)
            );
        }
    }

    void XhciPortRegisterSet::readPortscReg(XhciPortscRegister& reg) const {
        uint64_t portscAddress = m_base + m_portscOffset;
        reg.raw = *reinterpret_cast<volatile uint32_t*>(portscAddress);
    }

    void XhciPortRegisterSet::writePortscReg(XhciPortscRegister& reg) const {
        uint64_t portscAddress = m_base + m_portscOffset;
        *reinterpret_cast<volatile uint32_t*>(portscAddress) = reg.raw;
    }

    void XhciPortRegisterSet::readPortpmscRegUsb2(XhciPortpmscRegisterUsb2& reg) const {
        uint64_t portpmscAddress = m_base + m_portpmscOffset;
        reg.raw = *reinterpret_cast<volatile uint32_t*>(portpmscAddress);
    }

    void XhciPortRegisterSet::writePortpmscRegUsb2(XhciPortpmscRegisterUsb2& reg) const {
        uint64_t portpmscAddress = m_base + m_portpmscOffset;
        *reinterpret_cast<volatile uint32_t*>(portpmscAddress) = reg.raw;
    }

    void XhciPortRegisterSet::readPortpmscRegUsb3(XhciPortpmscRegisterUsb3& reg) const {
        uint64_t portpmscAddress = m_base + m_portpmscOffset;
        reg.raw = *reinterpret_cast<volatile uint32_t*>(portpmscAddress);
    }

    void XhciPortRegisterSet::writePortpmscRegUsb3(XhciPortpmscRegisterUsb3& reg) const {
        uint64_t portpmscAddress = m_base + m_portpmscOffset;
        *reinterpret_cast<volatile uint32_t*>(portpmscAddress) = reg.raw;
    }

    void XhciPortRegisterSet::readPortliReg(XhciPortliRegister& reg) const {
        uint64_t portliAddress = m_base + m_portliOffset;
        reg.raw = *reinterpret_cast<volatile uint32_t*>(portliAddress);
    }

    void XhciPortRegisterSet::writePortliReg(XhciPortliRegister& reg) const {
        uint64_t portliAddress = m_base + m_portliOffset;
        *reinterpret_cast<volatile uint32_t*>(portliAddress) = reg.raw;
    }

    void XhciPortRegisterSet::readPorthlpmcRegUsb2(XhciPorthlpmcRegisterUsb2& reg) const {
        uint64_t porthlpmAddress = m_base + m_porthlpmcOffset;
        reg.raw = *reinterpret_cast<volatile uint32_t*>(porthlpmAddress);
    }

    void XhciPortRegisterSet::writePorthlpmcRegUsb2(XhciPorthlpmcRegisterUsb2& reg) const {
        uint64_t porthlpmAddress = m_base + m_porthlpmcOffset;
        *reinterpret_cast<volatile uint32_t*>(porthlpmAddress) = reg.raw;
    }

    void XhciPortRegisterSet::readPorthlpmcRegUsb3(XhciPorthlpmcRegisterUsb3& reg) const {
        uint64_t porthlpmAddress = m_base + m_porthlpmcOffset;
        reg.raw = *reinterpret_cast<volatile uint32_t*>(porthlpmAddress);
    }

    void XhciPortRegisterSet::writePorthlpmcRegUsb3(XhciPorthlpmcRegisterUsb3& reg) const {
        uint64_t porthlpmAddress = m_base + m_porthlpmcOffset;
        *reinterpret_cast<volatile uint32_t*>(porthlpmAddress) = reg.raw;
    }

    XhciDriver& XhciDriver::get() {
        return g_globalXhciInstance;
    }

    bool XhciDriver::init(PciDeviceInfo& deviceInfo) {
        _mapDeviceMmio(deviceInfo.barAddress);

        _parseCapabilityRegisters();
        _logCapabilityRegisters();

        _parseExtendedCapabilityRegisters();

        _configureOperationalRegisters();

        if (!_resetHostController()) {
            return false;
        }

        _startHostController();

        // Reset the ports
        for (uint8_t i = 0; i < m_maxPorts; i++) {
            if (_resetPort(i)) {
                kprintInfo("[*] Successfully reset %s port %i\n", _isUSB3Port(i) ? "USB3" : "USB2", i);
            } else {
                kprintWarn("[*] Failed to reset %s port %i\n", _isUSB3Port(i) ? "USB3" : "USB2", i);
            }
        }
        kprint("\n");

        // msleep(600);
        // while (true) {
        //     for (uint8_t i = 0; i < m_maxPorts; i++) {
        //         XhciPortRegisterSet regset = _getPortRegisterSet(i);
        //         XhciPortscRegister portsc;
        //         regset.readPortscReg(portsc);

        //         bool isUsb3Port = _isUSB3Port(i);

        //         if (portsc.ccs) {
        //             kprint("%s device connected on port %i with speed ", isUsb3Port ? "USB3" : "USB2", i);

        //             switch (portsc.portSpeed) {
        //             case XHCI_USB_SPEED_FULL_SPEED: kprint("Full Speed (12 MB/s - USB2.0)\n"); break;
        //             case XHCI_USB_SPEED_LOW_SPEED: kprint("Low Speed (1.5 Mb/s - USB 2.0)\n"); break;
        //             case XHCI_USB_SPEED_HIGH_SPEED: kprint("High Speed (480 Mb/s - USB 2.0)\n"); break;
        //             case XHCI_USB_SPEED_SUPER_SPEED: kprint("Super Speed (5 Gb/s - USB3.0)\n"); break;
        //             case XHCI_USB_SPEED_SUPER_SPEED_PLUS: kprint("Super Speed Plus (10 Gb/s - USB 3.1)\n"); break;
        //             default: kprint("Undefined\n"); break;
        //             }
        //         }
        //     }

        //     sleep(2);
        //     kprint("\n");
        // }

        kprint("\n");
        return true;
    }

    void XhciDriver::_parseCapabilityRegisters() {
        m_capRegs = reinterpret_cast<volatile XhciCapabilityRegisters*>(m_xhcBase);

        m_capabilityRegsLength = m_capRegs->caplength;

        m_maxDeviceSlots = XHCI_MAX_DEVICE_SLOTS(m_capRegs);
        m_maxInterrupters = XHCI_MAX_INTERRUPTERS(m_capRegs);
        m_maxPorts = XHCI_MAX_PORTS(m_capRegs);

        m_isochronousSchedulingThreshold = XHCI_IST(m_capRegs);
        m_erstMax = XHCI_ERST_MAX(m_capRegs);
        m_maxScratchpadBuffers = XHCI_MAX_SCRATCHPAD_BUFFERS(m_capRegs);

        m_64bitAddressingCapability = XHCI_AC64(m_capRegs);
        m_bandwidthNegotiationCapability = XHCI_BNC(m_capRegs);
        m_64ByteContextSize = XHCI_CSZ(m_capRegs);
        m_portPowerControl = XHCI_PPC(m_capRegs);
        m_portIndicators = XHCI_PIND(m_capRegs);
        m_lightResetCapability = XHCI_LHRC(m_capRegs);
        m_extendedCapabilitiesOffset = XHCI_XECP(m_capRegs) * sizeof(uint32_t);

        // Update the base pointer to operational register set
        m_opRegs = reinterpret_cast<volatile XhciOperationalRegisters*>(m_xhcBase + m_capabilityRegsLength);
    }

    void XhciDriver::_logCapabilityRegisters() {
        kprintInfo("===== Capability Registers (0x%llx) =====\n", (uint64_t)m_capRegs);
        kprintInfo("    Length                : %i\n", m_capabilityRegsLength);
        kprintInfo("    Max Device Slots      : %i\n", m_maxDeviceSlots);
        kprintInfo("    Max Interrupters      : %i\n", m_maxInterrupters);
        kprintInfo("    Max Ports             : %i\n", m_maxPorts);
        kprintInfo("    IST                   : %i\n", m_isochronousSchedulingThreshold);
        kprintInfo("    ERST Max Size         : %i\n", m_erstMax);
        kprintInfo("    Scratchpad Buffers    : %i\n", m_maxScratchpadBuffers);
        kprintInfo("    64-bit Addressing     : %i\n", m_64bitAddressingCapability);
        kprintInfo("    Bandwidth Negotiation : %i\n", m_bandwidthNegotiationCapability);
        kprintInfo("    64-byte Context Size  : %i\n", m_64ByteContextSize);
        kprintInfo("    Port Power Control    : %i\n", m_portPowerControl);
        kprintInfo("    Port Indicators       : %i\n", m_portIndicators);
        kprintInfo("    Light Reset Available : %i\n", m_lightResetCapability);
        kprint("\n");
    }

    void XhciDriver::_parseExtendedCapabilityRegisters() {
        volatile uint32_t* headCapPtr = reinterpret_cast<volatile uint32_t*>(
            m_xhcBase + m_extendedCapabilitiesOffset
        );

        m_extendedCapabilitiesHead = kstl::SharedPtr<XhciExtendedCapability>(
            new XhciExtendedCapability(headCapPtr)
        );

        auto node = m_extendedCapabilitiesHead;
        while (node.get()) {
            if (node->id() == XhciExtendedCapabilityCode::SupportedProtocol) {
                XhciUsbSupportedProtocolCapability cap(node->base());
                // Make the ports zero-based
                uint8_t firstPort = cap.compatiblePortOffset - 1;
                uint8_t lastPort = firstPort + cap.compatiblePortCount - 1;

                if (cap.majorRevisionVersion == 3) {
                    for (uint8_t port = firstPort; port <= lastPort; port++) {
                        m_usb3Ports.pushBack(port);
                    }
                }
            }
            node = node->next();
        }
    }

    void XhciDriver::_logUsbsts() {
        uint32_t status = m_opRegs->usbsts;
        kprint("===== USBSTS =====\n");
        if (status & XHCI_USBSTS_HCH) kprint("    Host Controlled Halted\n");
        if (status & XHCI_USBSTS_HSE) kprint("    Host System Error\n");
        if (status & XHCI_USBSTS_EINT) kprint("    Event Interrupt\n");
        if (status & XHCI_USBSTS_PCD) kprint("    Port Change Detect\n");
        if (status & XHCI_USBSTS_SSS) kprint("    Save State Status\n");
        if (status & XHCI_USBSTS_RSS) kprint("    Restore State Status\n");
        if (status & XHCI_USBSTS_SRE) kprint("    Save/Restore Error\n");
        if (status & XHCI_USBSTS_CNR) kprint("    Controller Not Ready\n");
        if (status & XHCI_USBSTS_HCE) kprint("    Host Controller Error\n");
        kprint("\n");
    }

    void XhciDriver::_configureOperationalRegisters() {
        // Establish host controller's supported page size in bytes
        m_hcPageSize = static_cast<uint64_t>(m_opRegs->pagesize & 0xffff) << 12;
        
        // Enable device notifications 
        m_opRegs->dnctrl = 0xffff;

        // Configure the usbconfig field
        m_opRegs->config = static_cast<uint32_t>(m_maxDeviceSlots);
    }

    bool XhciDriver::_isUSB3Port(uint8_t portNum) {
        for (size_t i = 0; i < m_usb3Ports.size(); ++i) {
            if (m_usb3Ports[i] == portNum) {
                return true;
            }
        }

        return false;
    }

    XhciPortRegisterSet XhciDriver::_getPortRegisterSet(uint8_t portNum) {
        uint64_t base = reinterpret_cast<uint64_t>(m_opRegs) + (0x400 + (0x10 * portNum));
        return XhciPortRegisterSet(base);
    }

    void XhciDriver::_mapDeviceMmio(uint64_t pciBarAddress) {
        // Map a conservatively large space for xHCI registers
        for (size_t offset = 0; offset < 0x20000; offset += PAGE_SIZE) {
            void* mmioPage = (void*)(pciBarAddress + offset);
            paging::mapPage(mmioPage, mmioPage, KERNEL_PAGE, PAGE_ATTRIB_CACHE_DISABLED, paging::g_kernelRootPageTable);
        }

        paging::flushTlbAll();

        m_xhcBase = pciBarAddress;
    }

    bool XhciDriver::_resetHostController() {
        // Make sure we clear the Run/Stop bit
        uint32_t usbcmd = m_opRegs->usbcmd;
        usbcmd &= ~XHCI_USBCMD_RUN_STOP;
        m_opRegs->usbcmd = usbcmd;

        // Wait for the HCHalted bit to be set
        uint32_t timeout = 20;
        while (!(m_opRegs->usbsts & XHCI_USBSTS_HCH)) {
            if (--timeout == 0) {
                kprint("XHCI HC did not halt within %ims\n", timeout);
                return false;
            }

            msleep(1);
        }

        // Set the HC Reset bit
        usbcmd = m_opRegs->usbcmd;
        usbcmd |= XHCI_USBCMD_HCRESET;
        m_opRegs->usbcmd = usbcmd;

        // Wait for this bit and CNR bit to clear
        timeout = 100;
        while (
            m_opRegs->usbcmd & XHCI_USBCMD_HCRESET ||
            m_opRegs->usbsts & XHCI_USBSTS_CNR
        ) {
            if (--timeout == 0) {
                kprint("XHCI HC did not reset within %ims\n", timeout);
                return false;
            }

            msleep(1);
        }

        msleep(50);

        // Check the defaults of the operational registers
        if (m_opRegs->usbcmd != 0)
            return false;

        if (m_opRegs->dnctrl != 0)
            return false;

        if (m_opRegs->crcr != 0)
            return false;

        if (m_opRegs->dcbaap != 0)
            return false;

        if (m_opRegs->config != 0)
            return false;

        return true;
    }

    void XhciDriver::_startHostController() {
        uint32_t usbcmd = m_opRegs->usbcmd;
        usbcmd |= XHCI_USBCMD_RUN_STOP;
        usbcmd |= XHCI_USBCMD_INTERRUPTER_ENABLE;
        usbcmd |= XHCI_USBCMD_HOSTSYS_ERROR_ENABLE;

        m_opRegs->usbcmd = usbcmd;

        // Make sure the controller's HCH flag is cleared
        while (m_opRegs->usbsts & XHCI_USBSTS_HCH) {
            msleep(16);
        }
    }

    bool XhciDriver::_resetPort(uint8_t portNum) {
        XhciPortRegisterSet regset = _getPortRegisterSet(portNum);
        XhciPortscRegister portsc;
        regset.readPortscReg(portsc);

        bool isUsb3Port = _isUSB3Port(portNum);

        if (portsc.pp == 0) {
            portsc.pp = 1;
            regset.writePortscReg(portsc);
            msleep(20);
            regset.readPortscReg(portsc);

            if (portsc.pp == 0) {
                kprintWarn("Port %i: Bad Reset\n", portNum);
                return false;
            }
        }

        // Clear connect status change bit by writing a '1' to it
        portsc.csc = 1;
        regset.writePortscReg(portsc);

        // Write to the appropriate reset bit
        if (isUsb3Port) {
            portsc.wpr = 1;
        } else {
            portsc.pr = 1;
        }
        portsc.ped = 0;
        regset.writePortscReg(portsc);

        int timeout = 500;
        while (timeout) {
            regset.readPortscReg(portsc);

            // Detect port reset change bit to be set
            if (isUsb3Port && portsc.wrc) {
                break;
            } else if (!isUsb3Port && portsc.prc) {
                break;
            }

            timeout--;
            msleep(1);
        }

        if (timeout > 0) {
            msleep(3);
            regset.readPortscReg(portsc);

            // Check for the port enable/disable bit
            // to be set and indicate 'enabled' state.
            if (portsc.ped) {
                // Clear connect status change bit by writing a '1' to it
                portsc.csc = 1;
                regset.writePortscReg(portsc);
                return true;
            }
        }

        return false; 
    }
} // namespace drivers
