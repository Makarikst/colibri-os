#include "pci.h"
#include "../vga/vga.h"

/* ============================================
   PCI-шина (поиск устройств)
   ============================================ */

PciDevice g_net_device;
int       g_net_found = 0;

/* ------------------------------------------------------------
   Чтение 32 бит из конфигурационного пространства PCI
   ------------------------------------------------------------ */
uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address =
        ((uint32_t)bus      << 16) |
        ((uint32_t)slot     << 11) |
        ((uint32_t)func     <<  8) |
        ((uint32_t)offset & 0xFC)  |
        0x80000000;

    outl(PCI_CONFIG_ADDR, address);
    return inl(PCI_CONFIG_DATA);
}

void pci_write32(uint8_t bus, uint8_t slot, uint8_t func,
                 uint8_t offset, uint32_t value) {
    uint32_t address =
        ((uint32_t)bus      << 16) |
        ((uint32_t)slot     << 11) |
        ((uint32_t)func     <<  8) |
        ((uint32_t)offset & 0xFC)  |
        0x80000000;

    outl(PCI_CONFIG_ADDR, address);
    outl(PCI_CONFIG_DATA, value);
}

/* ------------------------------------------------------------
   Заполнить структуру PciDevice
   ------------------------------------------------------------ */
static void pci_fill(uint8_t bus, uint8_t slot, uint8_t func, PciDevice* d) {
    uint32_t reg0 = pci_read32(bus, slot, func, 0x00);
    uint32_t reg2 = pci_read32(bus, slot, func, 0x08);
    uint32_t reg3 = pci_read32(bus, slot, func, 0x0C);

    d->bus         = bus;
    d->slot        = slot;
    d->func        = func;
    d->vendor_id   = (uint16_t)(reg0 & 0xFFFF);
    d->device_id   = (uint16_t)((reg0 >> 16) & 0xFFFF);
    d->class_code  = (uint8_t)((reg2 >> 24) & 0xFF);
    d->subclass    = (uint8_t)((reg2 >> 16) & 0xFF);
    d->prog_if     = (uint8_t)((reg2 >>  8) & 0xFF);
    d->header_type = (uint8_t)((reg3 >> 16) & 0xFF);
    d->bar0        = pci_read32(bus, slot, func, 0x10);
    d->bar1        = pci_read32(bus, slot, func, 0x14);
}

/* ------------------------------------------------------------
   Поиск устройства по классу и подклассу
   Возвращает 1, если найдено
   ------------------------------------------------------------ */
int pci_find_device(uint8_t class_code, uint8_t subclass, PciDevice* out) {
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint32_t reg0 = pci_read32((uint8_t)bus, slot, 0, 0x00);
            uint16_t vendor = (uint16_t)(reg0 & 0xFFFF);
            if (vendor == 0xFFFF) continue;

            uint32_t reg2 = pci_read32((uint8_t)bus, slot, 0, 0x08);
            uint8_t  cc   = (uint8_t)((reg2 >> 24) & 0xFF);
            uint8_t  sc   = (uint8_t)((reg2 >> 16) & 0xFF);

            if (cc == class_code && sc == subclass) {
                pci_fill((uint8_t)bus, slot, 0, out);
                return 1;
            }

            /* Проверяем многофункциональные устройства */
            uint32_t reg3 = pci_read32((uint8_t)bus, slot, 0, 0x0C);
            uint8_t  ht   = (uint8_t)((reg3 >> 16) & 0xFF);
            if (ht & 0x80) {
                for (uint8_t func = 1; func < 8; func++) {
                    uint32_t r0 = pci_read32((uint8_t)bus, slot, func, 0x00);
                    if ((uint16_t)(r0 & 0xFFFF) == 0xFFFF) continue;
                    uint32_t r2 = pci_read32((uint8_t)bus, slot, func, 0x08);
                    uint8_t  c  = (uint8_t)((r2 >> 24) & 0xFF);
                    uint8_t  s  = (uint8_t)((r2 >> 16) & 0xFF);
                    if (c == class_code && s == subclass) {
                        pci_fill((uint8_t)bus, slot, func, out);
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}

/* ------------------------------------------------------------
   Поиск устройства по vendor/device ID
   ------------------------------------------------------------ */
int pci_find_by_id(uint16_t vendor, uint16_t device, PciDevice* out) {
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint32_t reg0 = pci_read32((uint8_t)bus, slot, 0, 0x00);
            uint16_t v = (uint16_t)(reg0 & 0xFFFF);
            uint16_t d = (uint16_t)((reg0 >> 16) & 0xFFFF);
            if (v == 0xFFFF) continue;
            if (v == vendor && d == device) {
                pci_fill((uint8_t)bus, slot, 0, out);
                return 1;
            }

            uint32_t reg3 = pci_read32((uint8_t)bus, slot, 0, 0x0C);
            uint8_t  ht   = (uint8_t)((reg3 >> 16) & 0xFF);
            if (ht & 0x80) {
                for (uint8_t func = 1; func < 8; func++) {
                    uint32_t r0 = pci_read32((uint8_t)bus, slot, func, 0x00);
                    uint16_t v2 = (uint16_t)(r0 & 0xFFFF);
                    uint16_t d2 = (uint16_t)((r0 >> 16) & 0xFFFF);
                    if (v2 == 0xFFFF) continue;
                    if (v2 == vendor && d2 == device) {
                        pci_fill((uint8_t)bus, slot, func, out);
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}

/* ------------------------------------------------------------
   Инициализация PCI — ищет сетевую карту навсегда
   ------------------------------------------------------------ */
void pci_init(void) {
    g_net_found = 0;

    if (pci_find_device(PCI_CLASS_NETWORK, PCI_SUBCLASS_ETHER, &g_net_device)) {
        g_net_found = 1;
        vga_print_color("PCI: Ethernet found: vendor=0x", VGA_LIGHT_GREEN);
        vga_print_hex32(g_net_device.vendor_id);
        vga_print(", device=0x");
        vga_print_hex32(g_net_device.device_id);
        vga_print("\n");
    } else {
        vga_print_color("PCI: Ethernet NOT found\n", VGA_LIGHT_RED);
    }
}

PciDevice* pci_get_net_device(void) {
    return g_net_found ? &g_net_device : 0;
}

/* ------------------------------------------------------------
   Вывести все устройства (отладка)
   ------------------------------------------------------------ */
void pci_dump(void) {
    vga_print_color("=== PCI devices ===\n", VGA_LIGHT_CYAN);

    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint32_t reg0 = pci_read32((uint8_t)bus, slot, 0, 0x00);
            uint16_t vendor = (uint16_t)(reg0 & 0xFFFF);
            if (vendor == 0xFFFF) continue;

            uint32_t reg2 = pci_read32((uint8_t)bus, slot, 0, 0x08);
            uint8_t  cc   = (uint8_t)((reg2 >> 24) & 0xFF);
            uint8_t  sc   = (uint8_t)((reg2 >> 16) & 0xFF);

            vga_print("bus=");
            vga_print_dec(bus);
            vga_print(" slot=");
            vga_print_dec(slot);
            vga_print(" vendor=0x");
            vga_print_hex32(vendor);
            vga_print(" class=0x");
            vga_print_hex32(cc);
            vga_print(" subclass=0x");
            vga_print_hex32(sc);
            vga_print("\n");
        }
    }
}