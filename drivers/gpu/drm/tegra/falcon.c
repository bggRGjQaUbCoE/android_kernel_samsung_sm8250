/*
 * Copyright (c) 2015, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/firmware.h>
#include <linux/pci_ids.h>
#include <linux/iopoll.h>

#include "falcon.h"
#include "drm.h"

enum falcon_memory {
	FALCON_MEMORY_IMEM,
	FALCON_MEMORY_DATA,
};

static void falcon_writel(struct falcon *falcon, u32 value, u32 offset)
{
	writel(value, falcon->regs + offset);
}

int falcon_wait_idle(struct falcon *falcon)
{
	u32 value;

	return readl_poll_timeout(falcon->regs + FALCON_IDLESTATE, value,
				  (value == 0), 10, 100000);
}

static int falcon_dma_wait_idle(struct falcon *falcon)
{
	u32 value;

	return readl_poll_timeout(falcon->regs + FALCON_DMATRFCMD, value,
				  (value & FALCON_DMATRFCMD_IDLE), 10, 100000);
}

static int falcon_copy_chunk(struct falcon *falcon,
			     phys_addr_t base,
			     unsigned long offset,
			     enum falcon_memory target)
{
	u32 cmd = FALCON_DMATRFCMD_SIZE_256B;

	if (target == FALCON_MEMORY_IMEM)
		cmd |= FALCON_DMATRFCMD_IMEM;

	falcon_writel(falcon, offset, FALCON_DMATRFMOFFS);
	falcon_writel(falcon, base, FALCON_DMATRFFBOFFS);
	falcon_writel(falcon, cmd, FALCON_DMATRFCMD);

	return falcon_dma_wait_idle(falcon);
}

static void falcon_copy_firmware_image(struct falcon *falcon,
				       const struct firmware *firmware)
{
	u32 *firmware_vaddr = falcon->firmware.vaddr;
	dma_addr_t daddr;
	size_t i;
	int err;

	/* copy the whole thing taking into account endianness */
	for (i = 0; i < firmware->size / sizeof(u32); i++)
		firmware_vaddr[i] = le32_to_cpu(((u32 *)firmware->data)[i]);

	/* ensure that caches are flushed and falcon can see the firmware */
	daddr = dma_map_single(falcon->dev, firmware_vaddr,
			       falcon->firmware.size, DMA_TO_DEVICE);
	err = dma_mapping_error(falcon->dev, daddr);
	if (err) {
		dev_err(falcon->dev, "failed to map firmware: %d\n", err);
		return;
	}
	dma_sync_single_for_device(falcon->dev, daddr,
				   falcon->firmware.size, DMA_TO_DEVICE);
	dma_unmap_single(falcon->dev, daddr, falcon->firmware.size,
			 DMA_TO_DEVICE);
}

static int falcon_parse_firmware_image(struct falcon *falcon)
{
	struct falcon_fw_bin_header_v1 *bin = (void *)falcon->firmware.vaddr;
	struct falcon_fw_os_header_v1 *os;

	/* endian problems would show up right here */
	if (bin->magic != PCI_VENDOR_ID_NVIDIA) {
		dev_err(falcon->dev, "incorrect firmware magic\n");
		return -EINVAL;
	}

	/* currently only version 1 is supported */
	if (bin->version != 1) {
		dev_err(falcon->dev, "unsupported firmware version\n");
		return -EINVAL;
	}

	/* check that the firmware size is consistent */
	if (bin->size > falcon->firmware.size) {
		dev_err(falcon->dev, "firmware image size inconsistency\n");
		return -EINVAL;
	}

	os = falcon->firmware.vaddr + bin->os_header_offset;

	falcon->firmware.bin_data.size = bin->os_size;
	falcon->firmware.bin_data.offset = bin->os_data_offset;
	falcon->firmware.code.offset = os->code_offset;
	falcon->firmware.code.size = os->code_size;
	falcon->firmware.data.offset = os->data_offset;
	falcon->firmware.data.size = os->data_size;

	return 0;
}

int falcon_read_firmware(struct falcon *falcon, const char *name)
{
	int err;

	/* request_firmware prints error if it fails */
	err = request_firmware(&falcon->firmware.firmware, name, falcon->dev);
	if (err < 0)
		return err;

	return 0;
}

int falcon_load_firmware(struct falcon *falcon)
{
	const struct firmware *firmware = falcon->firmware.firmware;
	int err;

	falcon->firmware.size = firmware->size;

	/* allocate iova space for the firmware */
	falcon->firmware.vaddr = falcon->ops->alloc(falcon, firmware->size,
						    &falcon->firmware.paddr);
	if (!falcon->firmware.vaddr) {
		dev_err(falcon->dev, "dma memory mapping failed\n");
		return -ENOMEM;
	}

	/* copy firmware image into local area. this also ensures endianness */
	falcon_copy_firmware_image(falcon, firmware);

	/* parse the image data */
	err = falcon_parse_firmware_image(falcon);
	if (err < 0) {
		dev_err(falcon->dev, "failed to parse firmware image\n");
		goto err_setup_firmware_image;
	}

	release_firmware(firmware);
	falcon->firmware.firmware = NULL;

	return 0;

err_setup_firmware_image:
	falcon->ops->free(falcon, falcon->firmware.size,
			  falcon->firmware.paddr, falcon->firmware.vaddr);

	return err;
}

int falcon_init(struct falcon *falcon)
{
	/* check mandatory ops */
	if (!falcon->ops || !falcon->ops->alloc || !falcon->ops->free)
		return -EINVAL;

	falcon->firmware.vaddr = NULL;

	return 0;
}

void falcon_exit(struct falcon *falcon)
{
	if (falcon->firmware.firmware) {
		release_firmware(falcon->firmware.firmware);
		falcon->firmware.firmware = NULL;
	}

	if (falcon->firmware.vaddr) {
		falcon->ops->free(falcon, falcon->firmware.size,
				  falcon->firmware.paddr,
				  falcon->firmware.vaddr);
		falcon->firmware.vaddr = NULL;
	}
}

int falcon_boot(struct falcon *falcon)
{
	unsigned long offset;
	int err;

	if (!falcon->firmware.vaddr)
		return -EINVAL;

	falcon_writel(falcon, 0, FALCON_DMACTL);

	/* setup the address of the binary data so Falcon can access it later */
	falcon_writel(falcon, (falcon->firmware.paddr +
			       falcon->firmware.bin_data.offset) >> 8,
		      FALCON_DMATRFBASE);

	/* copy the data segment into Falcon internal memory */
	for (offset = 0; offset < falcon->firmware.data.size; offset += 256)
		falcon_copy_chunk(falcon,
				  falcon->firmware.data.offset + offset,
				  offset, FALCON_MEMORY_DATA);

	/* copy the first code segment into Falcon internal memory */
	falcon_copy_chunk(falcon, falcon->firmware.code.offset,
			  0, FALCON_MEMORY_IMEM);

	/* setup falcon interrupts */
	falcon_writel(falcon, FALCON_IRQMSET_EXT(0xff) |
			      FALCON_IRQMSET_SWGEN1 |
			      FALCON_IRQMSET_SWGEN0 |
			      FALCON_IRQMSET_EXTERR |
			      FALCON_IRQMSET_HALT |
			      FALCON_IRQMSET_WDTMR,
		      FALCON_IRQMSET);
	falcon_writel(falcon, FALCON_IRQDEST_EXT(0xff) |
			      FALCON_IRQDEST_SWGEN1 |
			      FALCON_IRQDEST_SWGEN0 |
			      FALCON_IRQDEST_EXTERR |
			      FALCON_IRQDEST_HALT,
		      FALCON_IRQDEST);

	/* enable interface */
	falcon_writel(falcon, FALCON_ITFEN_MTHDEN |
			      FALCON_ITFEN_CTXEN,
		      FALCON_ITFEN);

	/* boot falcon */
	falcon_writel(falcon, 0x00000000, FALCON_BOOTVEC);
	falcon_writel(falcon, FALCON_CPUCTL_STARTCPU, FALCON_CPUCTL);

	err = falcon_wait_idle(falcon);
	if (err < 0) {
		dev_err(falcon->dev, "Falcon boot failed due to timeout\n");
		return err;
	}

	return 0;
}

void falcon_execute_method(struct falcon *falcon, u32 method, u32 data)
{
	falcon_writel(falcon, method >> 2, FALCON_UCLASS_METHOD_OFFSET);
	falcon_writel(falcon, data, FALCON_UCLASS_METHOD_DATA);
}
