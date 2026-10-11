#ifndef PCI_H
#define PCI_H

#include "../utils/utils.h"

/* ============================================
   PCI-шина (поиск устройств)
   ============================================ */

#define PCI_CONFIG_ADDR  0xCF8
#define PCI_CONFIG_DATA  0xCFC

/* Классы устройств */
#define PCI_CLASS_NETWORK  0x02
#define PCI_SUBCLASS_ETHER 0x00

/* Производители */
#define PCI_VENDOR_REALTEK  0x10EC
#define PCI_VENDOR_INTEL    0x8086

typedef struct {
    uint8_t  bus;
    uint8_t  slot;
    uint8_t  func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  class_code;
    uint8_t  subclass;
    uint8_t  prog_if;
    uint8_t  header_type;
    uint32_t bar0;   /* Базовый адрес регистра 0 */
    uint32_t bar1;
} PciDevice;

/* Глобальные переменные — найденная сетевая карта */
extern PciDevice g_net_device;
extern int       g_net_found;

/* Инициализация — ищет Ethernet-контроллер и сохраняет в g_net_device */
void pci_init(void);

/* Получить найденную сетевую карту (или 0, если не найдена) */
PciDevice* pci_get_net_device(void);

/* Прочитать 32 бита из конфигурации */
uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);

/* Записать 32 бита */
void pci_write32(uint8_t bus, uint8_t slot, uint8_t func,
                 uint8_t offset, uint32_t value);

/* Найти первое устройство по классу/подклассу */
int pci_find_device(uint8_t class_code, uint8_t subclass, PciDevice* out);

/* Найти первое устройство по vendor/device */
int pci_find_by_id(uint16_t vendor, uint16_t device, PciDevice* out);

/* Вывести все устройства (отладка) */
void pci_dump(void);

#endif