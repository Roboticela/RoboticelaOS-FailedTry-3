/* robobootloader - RoboBootInfo construction. */
#include "bootinfo.h"
#include "memory/memory.h"
#include "lib.h"
#include "debug.h"

RoboBootInfo *bootinfo_alloc(void)
{
    RoboBootInfo *bi = mem_alloc_zero(sizeof(RoboBootInfo));
    if (!bi)
        return NULL;
    bi->version = ROBOBOOTINFO_VERSION;
    bi->framebuffer_format = ROBO_FB_FORMAT_UNKNOWN;
    return bi;
}

void bootinfo_set_framebuffer(RoboBootInfo *bi, uint64_t addr, uint64_t size,
                              uint32_t w, uint32_t h, uint32_t pitch, uint32_t format)
{
    if (!bi)
        return;
    bi->framebuffer_address = addr;
    bi->framebuffer_size = size;
    bi->framebuffer_width = w;
    bi->framebuffer_height = h;
    bi->framebuffer_pitch = pitch;
    bi->framebuffer_format = format;
}

void bootinfo_set_kernel(RoboBootInfo *bi, uint64_t base, uint64_t size, uint64_t entry)
{
    if (!bi)
        return;
    bi->kernel_base = base;
    bi->kernel_size = size;
    bi->kernel_entry = entry;
}

void bootinfo_set_initfs(RoboBootInfo *bi, uint64_t addr, uint64_t size)
{
    if (!bi)
        return;
    bi->initfs_address = addr;
    bi->initfs_size = size;
}

void bootinfo_set_memory_map(RoboBootInfo *bi, uint64_t map, uint64_t size,
                             uint64_t desc_size, uint32_t desc_version)
{
    if (!bi)
        return;
    bi->memory_map = map;
    bi->memory_map_size = size;
    bi->memory_map_descriptor_size = desc_size;
    bi->memory_map_descriptor_version = desc_version;
}

void bootinfo_set_acpi_rsdp(RoboBootInfo *bi, uint64_t rsdp)
{
    if (bi)
        bi->acpi_rsdp = rsdp;
}

void bootinfo_debug_dump(const RoboBootInfo *bi)
{
    if (!bi)
        return;
    debugf("RoboBootInfo @ %p", (void *)bi);
    debugf("  version=%llu", bi->version);
    debugf("  fb addr=0x%llx size=0x%llx %ux%u pitch=%u fmt=%u",
           bi->framebuffer_address, bi->framebuffer_size,
           bi->framebuffer_width, bi->framebuffer_height,
           bi->framebuffer_pitch, bi->framebuffer_format);
    debugf("  acpi rsdp=0x%llx", bi->acpi_rsdp);
    debugf("  kernel base=0x%llx size=0x%llx entry=0x%llx",
           bi->kernel_base, bi->kernel_size, bi->kernel_entry);
    debugf("  initfs addr=0x%llx size=0x%llx", bi->initfs_address, bi->initfs_size);
    debugf("  memmap=0x%llx size=0x%llx dsize=%llu", bi->memory_map,
           bi->memory_map_size, bi->memory_map_descriptor_size);
    debugf("  boot_device=%u cmdline=0x%llx", bi->boot_device, bi->command_line);
}
