/* ============================================
   Colibri OS — UEFI Bootloader (без ошибок)
   Загружает kernel.efi и вызывает его.
   ============================================ */

#include <efi.h>
#include <efilib.h>

#define KERNEL_LOAD_ADDR 0x1000000
#define BOOTINFO_ADDR    0x7000

typedef struct {
    UINT32* framebuffer;
    UINT32  width;
    UINT32  height;
    UINT32  pitch;
    UINT32  bpp;
} BootInfo;

/* Точка входа ядра (EFI-функция) */
typedef void (*kernel_entry_t)(BootInfo*);

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable) {
    EFI_STATUS status;
    EFI_FILE_HANDLE root = NULL;
    EFI_FILE_HANDLE kernel_file = NULL;
    BootInfo* bootinfo = (BootInfo*) BOOTINFO_ADDR;
    EFI_PHYSICAL_ADDRESS kernel_addr = KERNEL_LOAD_ADDR;
    UINTN kernel_size = 0;

    InitializeLib(ImageHandle, SystemTable);
    Print(L"Colibri OS UEFI bootloader\n");

    /* ---- GOP ---- */
    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = NULL;
    status = uefi_call_wrapper(BS->LocateProtocol, 3, &gop_guid, NULL, (void**)&gop);
    if (EFI_ERROR(status)) {
        Print(L"GOP not found\n");
        return status;
    }

    bootinfo->framebuffer = (UINT32*) gop->Mode->FrameBufferBase;
    bootinfo->width       = gop->Mode->Info->HorizontalResolution;
    bootinfo->height      = gop->Mode->Info->VerticalResolution;
    bootinfo->pitch       = gop->Mode->Info->PixelsPerScanLine * 4;
    bootinfo->bpp         = 32;

    Print(L"Framebuffer: 0x%x, %dx%d\n",
          gop->Mode->FrameBufferBase,
          bootinfo->width, bootinfo->height);

    /* ---- Файловая система ---- */
    EFI_GUID fs_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fs = NULL;
    status = uefi_call_wrapper(BS->LocateProtocol, 3, &fs_guid, NULL, (void**)&fs);
    if (EFI_ERROR(status)) {
        Print(L"FS not found\n");
        return status;
    }

    status = uefi_call_wrapper(fs->OpenVolume, 2, fs, &root);
    if (EFI_ERROR(status)) {
        Print(L"OpenVolume failed\n");
        return status;
    }

    /* ---- Открываем kernel.efi ---- */
    status = uefi_call_wrapper(root->Open, 5, root, &kernel_file,
                               L"kernel.efi", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) {
        Print(L"kernel.efi not found\n");
        return status;
    }

    EFI_FILE_INFO* fi = LibFileInfo(kernel_file);
    kernel_size = fi->FileSize;
    FreePool(fi);

    Print(L"Kernel size: %d bytes\n", kernel_size);

    /* ---- Выделяем память по фиксированному адресу ---- */
    status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAddress,
                               EfiLoaderData,
                               (kernel_size + 4095) / 4096,
                               &kernel_addr);
    if (EFI_ERROR(status)) {
        Print(L"AllocatePages failed\n");
        return status;
    }

    /* ---- Читаем ядро ---- */
    status = uefi_call_wrapper(kernel_file->Read, 3, kernel_file,
                               &kernel_size, (void*)kernel_addr);
    if (EFI_ERROR(status)) {
        Print(L"Read failed\n");
        return status;
    }

    Print(L"Kernel loaded at 0x%x\n", kernel_addr);

    uefi_call_wrapper(kernel_file->Close, 1, kernel_file);

    /* ---- Вызываем ядро как EFI-функцию ---- */
    kernel_entry_t kernel_entry = (kernel_entry_t) kernel_addr;
    kernel_entry(bootinfo);

    /* Если ядро вернулось — зависаем */
    while (1) {
        uefi_call_wrapper(BS->Stall, 1, 1000000);
    }

    return EFI_SUCCESS;
}