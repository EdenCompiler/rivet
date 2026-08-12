/* RIVET — pci.h : legacy PCI configuration space access.
 *
 * Uses the type-1 mechanism via I/O ports 0xCF8 (address) and 0xCFC
 * (data). Sufficient for early enumeration before paging+MMCONFIG. */
#ifndef RIVET_PCI_H
#define RIVET_PCI_H

#include "core.h"
#include "io.h"

#if RIVET_ARCH_X86 || RIVET_ARCH_X86_64

#define RIV_PCI_CONFIG_ADDR  0xCF8
#define RIV_PCI_CONFIG_DATA  0xCFC

/* Identifies one PCI function: bus(8) | device(5) | function(3). */
typedef struct { riv_u8 bus, dev, func; } riv_pci_loc;

/* Standard config-space offsets. */
#define RIV_PCI_VENDOR     0x00
#define RIV_PCI_DEVICE     0x02
#define RIV_PCI_COMMAND    0x04
#define RIV_PCI_STATUS     0x06
#define RIV_PCI_REV_CLASS  0x08
#define RIV_PCI_HDR_TYPE   0x0E
#define RIV_PCI_BAR0       0x10

static inline riv_u32 _riv_pci_addr(riv_pci_loc l, riv_u8 off) {
    return  (1u << 31)
          | ((riv_u32)l.bus  << 16)
          | ((riv_u32)l.dev  << 11)
          | ((riv_u32)l.func << 8)
          | ((riv_u32)off & 0xFC);
}

RIV_ALWAYS riv_u32 riv_pci_read32(riv_pci_loc l, riv_u8 off) {
    riv_outl(RIV_PCI_CONFIG_ADDR, _riv_pci_addr(l, off));
    return riv_inl(RIV_PCI_CONFIG_DATA);
}
RIV_ALWAYS void riv_pci_write32(riv_pci_loc l, riv_u8 off, riv_u32 v) {
    riv_outl(RIV_PCI_CONFIG_ADDR, _riv_pci_addr(l, off));
    riv_outl(RIV_PCI_CONFIG_DATA, v);
}

RIV_ALWAYS riv_u16 riv_pci_read16(riv_pci_loc l, riv_u8 off) {
    riv_u32 v = riv_pci_read32(l, (riv_u8)(off & 0xFC));
    return (riv_u16)((v >> ((off & 2) * 8)) & 0xFFFF);
}
RIV_ALWAYS riv_u8 riv_pci_read8(riv_pci_loc l, riv_u8 off) {
    riv_u32 v = riv_pci_read32(l, (riv_u8)(off & 0xFC));
    return (riv_u8)((v >> ((off & 3) * 8)) & 0xFF);
}

RIV_ALWAYS int riv_pci_present(riv_pci_loc l) {
    return riv_pci_read16(l, RIV_PCI_VENDOR) != 0xFFFF;
}

/* Walk every bus/device/function, invoking `cb` for present ones.
 * Single-function devices skip f=1..7. */
typedef void (*riv_pci_visit_fn)(riv_pci_loc loc, riv_u16 vendor,
                                  riv_u16 device, void *ctx);

RIV_ALWAYS void riv_pci_enumerate(riv_pci_visit_fn cb, void *ctx) {
    for (int bus = 0; bus < 256; ++bus) {
        for (int dev = 0; dev < 32; ++dev) {
            riv_pci_loc l = { (riv_u8)bus, (riv_u8)dev, 0 };
            if (!riv_pci_present(l)) continue;
            riv_u16 v = riv_pci_read16(l, RIV_PCI_VENDOR);
            riv_u16 d = riv_pci_read16(l, RIV_PCI_DEVICE);
            cb(l, v, d, ctx);
            riv_u8 hdr = riv_pci_read8(l, RIV_PCI_HDR_TYPE);
            if (!(hdr & 0x80)) continue;
            for (int f = 1; f < 8; ++f) {
                riv_pci_loc lf = { (riv_u8)bus, (riv_u8)dev, (riv_u8)f };
                if (!riv_pci_present(lf)) continue;
                cb(lf,
                   riv_pci_read16(lf, RIV_PCI_VENDOR),
                   riv_pci_read16(lf, RIV_PCI_DEVICE), ctx);
            }
        }
    }
}

#endif /* x86 */

#endif /* RIVET_PCI_H */
