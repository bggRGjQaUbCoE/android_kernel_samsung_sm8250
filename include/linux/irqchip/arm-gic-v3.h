/*
 * Copyright (C) 2013, 2014 ARM Limited, All Rights Reserved.
 * Author: Marc Zyngier <marc.zyngier@arm.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __LINUX_IRQCHIP_ARM_GIC_V3_H
#define __LINUX_IRQCHIP_ARM_GIC_V3_H

/*
 * Distributor registers. We assume we're running non-secure, with ARE
 * being set. Secure-only and non-ARE registers are not described.
 */
#define GICD_CTLR			0x0000
#define GICD_TYPER			0x0004
#define GICD_IIDR			0x0008
#define GICD_STATUSR			0x0010
#define GICD_SETSPI_NSR			0x0040
#define GICD_CLRSPI_NSR			0x0048
#define GICD_SETSPI_SR			0x0050
#define GICD_CLRSPI_SR			0x0058
#define GICD_SEIR			0x0068
#define GICD_IGROUPR			0x0080
#define GICD_ISENABLER			0x0100
#define GICD_ICENABLER			0x0180
#define GICD_ISPENDR			0x0200
#define GICD_ICPENDR			0x0280
#define GICD_ISACTIVER			0x0300
#define GICD_ICACTIVER			0x0380
#define GICD_IPRIORITYR			0x0400
#define GICD_ICFGR			0x0C00
#define GICD_IGRPMODR			0x0D00
#define GICD_NSACR			0x0E00
#define GICD_IROUTER			0x6000
#define GICD_IDREGS			0xFFD0
#define GICD_PIDR2			0xFFE8

/*
 * Those registers are actually from GICv2, but the spec demands that they
 * are implemented as RES0 if ARE is 1 (which we do in KVM's emulated GICv3).
 */
#define GICD_ITARGETSR			0x0800
#define GICD_SGIR			0x0F00
#define GICD_CPENDSGIR			0x0F10
#define GICD_SPENDSGIR			0x0F20

#define GICD_CTLR_RWP			(1U << 31)
#define GICD_CTLR_DS			(1U << 6)
#define GICD_CTLR_ARE_NS		(1U << 4)
#define GICD_CTLR_ENABLE_G1A		(1U << 1)
#define GICD_CTLR_ENABLE_G1		(1U << 0)

#define GICD_IIDR_IMPLEMENTER_SHIFT	0
#define GICD_IIDR_IMPLEMENTER_MASK	(0xfff << GICD_IIDR_IMPLEMENTER_SHIFT)
#define GICD_IIDR_REVISION_SHIFT	12
#define GICD_IIDR_REVISION_MASK		(0xf << GICD_IIDR_REVISION_SHIFT)
#define GICD_IIDR_VARIANT_SHIFT		16
#define GICD_IIDR_VARIANT_MASK		(0xf << GICD_IIDR_VARIANT_SHIFT)
#define GICD_IIDR_PRODUCT_ID_SHIFT	24
#define GICD_IIDR_PRODUCT_ID_MASK	(0xff << GICD_IIDR_PRODUCT_ID_SHIFT)


/*
 * In systems with a single security state (what we emulate in KVM)
 * the meaning of the interrupt group enable bits is slightly different
 */
#define GICD_CTLR_ENABLE_SS_G1		(1U << 1)
#define GICD_CTLR_ENABLE_SS_G0		(1U << 0)

#define GICD_TYPER_RSS			(1U << 26)
#define GICD_TYPER_LPIS			(1U << 17)
#define GICD_TYPER_MBIS			(1U << 16)

#define GICD_TYPER_ID_BITS(typer)	((((typer) >> 19) & 0x1f) + 1)
#define GICD_TYPER_NUM_LPIS(typer)	((((typer) >> 11) & 0x1f) + 1)
#define GICD_TYPER_IRQS(typer)		((((typer) & 0x1f) + 1) * 32)

#define GICD_IROUTER_SPI_MODE_ONE	(0U << 31)
#define GICD_IROUTER_SPI_MODE_ANY	(1U << 31)

#define GIC_PIDR2_ARCH_MASK		0xf0
#define GIC_PIDR2_ARCH_GICv3		0x30
#define GIC_PIDR2_ARCH_GICv4		0x40

#define GIC_V3_DIST_SIZE		0x10000

/*
 * Re-Distributor registers, offsets from RD_base
 */
#define GICR_CTLR			GICD_CTLR
#define GICR_IIDR			0x0004
#define GICR_TYPER			0x0008
#define GICR_STATUSR			GICD_STATUSR
#define GICR_WAKER			0x0014
#define GICR_SETLPIR			0x0040
#define GICR_CLRLPIR			0x0048
#define GICR_SEIR			GICD_SEIR
#define GICR_PROPBASER			0x0070
#define GICR_PENDBASER			0x0078
#define GICR_INVLPIR			0x00A0
#define GICR_INVALLR			0x00B0
#define GICR_SYNCR			0x00C0
#define GICR_MOVLPIR			0x0100
#define GICR_MOVALLR			0x0110
#define GICR_IDREGS			GICD_IDREGS
#define GICR_PIDR2			GICD_PIDR2

#define GICR_CTLR_ENABLE_LPIS		(1UL << 0)
#define GICR_CTLR_RWP			(1UL << 3)

#define GICR_TYPER_CPU_NUMBER(r)	(((r) >> 8) & 0xffff)

#define GICR_WAKER_ProcessorSleep	(1U << 1)
#define GICR_WAKER_ChildrenAsleep	(1U << 2)

#define GIC_BASER_CACHE_nCnB		0ULL
#define GIC_BASER_CACHE_SameAsInner	0ULL
#define GIC_BASER_CACHE_nC		1ULL
#define GIC_BASER_CACHE_RaWt		2ULL
#define GIC_BASER_CACHE_RaWb		3ULL
#define GIC_BASER_CACHE_WaWt		4ULL
#define GIC_BASER_CACHE_WaWb		5ULL
#define GIC_BASER_CACHE_RaWaWt		6ULL
#define GIC_BASER_CACHE_RaWaWb		7ULL
#define GIC_BASER_CACHE_MASK		7ULL
#define GIC_BASER_NonShareable		0ULL
#define GIC_BASER_InnerShareable	1ULL
#define GIC_BASER_OuterShareable	2ULL
#define GIC_BASER_SHAREABILITY_MASK	3ULL

#define GIC_BASER_CACHEABILITY(reg, inner_outer, type)			\
	(GIC_BASER_CACHE_##type << reg##_##inner_outer##_CACHEABILITY_SHIFT)

#define GIC_BASER_SHAREABILITY(reg, type)				\
	(GIC_BASER_##type << reg##_SHAREABILITY_SHIFT)

/* encode a size field of width @w containing @n - 1 units */
#define GIC_ENCODE_SZ(n, w) (((unsigned long)(n) - 1) & GENMASK_ULL(((w) - 1), 0))

#define GICR_PROPBASER_SHAREABILITY_SHIFT		(10)
#define GICR_PROPBASER_INNER_CACHEABILITY_SHIFT		(7)
#define GICR_PROPBASER_OUTER_CACHEABILITY_SHIFT		(56)
#define GICR_PROPBASER_SHAREABILITY_MASK				\
	GIC_BASER_SHAREABILITY(GICR_PROPBASER, SHAREABILITY_MASK)
#define GICR_PROPBASER_INNER_CACHEABILITY_MASK				\
	GIC_BASER_CACHEABILITY(GICR_PROPBASER, INNER, MASK)
#define GICR_PROPBASER_OUTER_CACHEABILITY_MASK				\
	GIC_BASER_CACHEABILITY(GICR_PROPBASER, OUTER, MASK)
#define GICR_PROPBASER_CACHEABILITY_MASK GICR_PROPBASER_INNER_CACHEABILITY_MASK

#define GICR_PROPBASER_InnerShareable					\
	GIC_BASER_SHAREABILITY(GICR_PROPBASER, InnerShareable)

#define GICR_PROPBASER_nCnB	GIC_BASER_CACHEABILITY(GICR_PROPBASER, INNER, nCnB)
#define GICR_PROPBASER_nC 	GIC_BASER_CACHEABILITY(GICR_PROPBASER, INNER, nC)
#define GICR_PROPBASER_RaWt	GIC_BASER_CACHEABILITY(GICR_PROPBASER, INNER, RaWt)
#define GICR_PROPBASER_RaWb	GIC_BASER_CACHEABILITY(GICR_PROPBASER, INNER, RaWb)
#define GICR_PROPBASER_WaWt	GIC_BASER_CACHEABILITY(GICR_PROPBASER, INNER, WaWt)
#define GICR_PROPBASER_WaWb	GIC_BASER_CACHEABILITY(GICR_PROPBASER, INNER, WaWb)
#define GICR_PROPBASER_RaWaWt	GIC_BASER_CACHEABILITY(GICR_PROPBASER, INNER, RaWaWt)
#define GICR_PROPBASER_RaWaWb	GIC_BASER_CACHEABILITY(GICR_PROPBASER, INNER, RaWaWb)

#define GICR_PROPBASER_IDBITS_MASK			(0x1f)
#define GICR_PROPBASER_ADDRESS(x)	((x) & GENMASK_ULL(51, 12))
#define GICR_PENDBASER_ADDRESS(x)	((x) & GENMASK_ULL(51, 16))

#define GICR_PENDBASER_SHAREABILITY_SHIFT		(10)
#define GICR_PENDBASER_INNER_CACHEABILITY_SHIFT		(7)
#define GICR_PENDBASER_OUTER_CACHEABILITY_SHIFT		(56)
#define GICR_PENDBASER_SHAREABILITY_MASK				\
	GIC_BASER_SHAREABILITY(GICR_PENDBASER, SHAREABILITY_MASK)
#define GICR_PENDBASER_INNER_CACHEABILITY_MASK				\
	GIC_BASER_CACHEABILITY(GICR_PENDBASER, INNER, MASK)
#define GICR_PENDBASER_OUTER_CACHEABILITY_MASK				\
	GIC_BASER_CACHEABILITY(GICR_PENDBASER, OUTER, MASK)
#define GICR_PENDBASER_CACHEABILITY_MASK GICR_PENDBASER_INNER_CACHEABILITY_MASK

#define GICR_PENDBASER_InnerShareable					\
	GIC_BASER_SHAREABILITY(GICR_PENDBASER, InnerShareable)

#define GICR_PENDBASER_nCnB	GIC_BASER_CACHEABILITY(GICR_PENDBASER, INNER, nCnB)
#define GICR_PENDBASER_nC 	GIC_BASER_CACHEABILITY(GICR_PENDBASER, INNER, nC)
#define GICR_PENDBASER_RaWt	GIC_BASER_CACHEABILITY(GICR_PENDBASER, INNER, RaWt)
#define GICR_PENDBASER_RaWb	GIC_BASER_CACHEABILITY(GICR_PENDBASER, INNER, RaWb)
#define GICR_PENDBASER_WaWt	GIC_BASER_CACHEABILITY(GICR_PENDBASER, INNER, WaWt)
#define GICR_PENDBASER_WaWb	GIC_BASER_CACHEABILITY(GICR_PENDBASER, INNER, WaWb)
#define GICR_PENDBASER_RaWaWt	GIC_BASER_CACHEABILITY(GICR_PENDBASER, INNER, RaWaWt)
#define GICR_PENDBASER_RaWaWb	GIC_BASER_CACHEABILITY(GICR_PENDBASER, INNER, RaWaWb)

#define GICR_PENDBASER_PTZ				BIT_ULL(62)

/*
 * Re-Distributor registers, offsets from SGI_base
 */
#define GICR_IGROUPR0			GICD_IGROUPR
#define GICR_ISENABLER0			GICD_ISENABLER
#define GICR_ICENABLER0			GICD_ICENABLER
#define GICR_ISPENDR0			GICD_ISPENDR
#define GICR_ICPENDR0			GICD_ICPENDR
#define GICR_ISACTIVER0			GICD_ISACTIVER
#define GICR_ICACTIVER0			GICD_ICACTIVER
#define GICR_IPRIORITYR0		GICD_IPRIORITYR
#define GICR_ICFGR0			GICD_ICFGR
#define GICR_IGRPMODR0			GICD_IGRPMODR
#define GICR_NSACR			GICD_NSACR

#define GICR_TYPER_PLPIS		(1U << 0)
#define GICR_TYPER_VLPIS		(1U << 1)
#define GICR_TYPER_DirectLPIS		(1U << 3)
#define GICR_TYPER_LAST			(1U << 4)

#define GIC_V3_REDIST_SIZE		0x20000

#define LPI_PROP_GROUP1			(1 << 1)
#define LPI_PROP_ENABLED		(1 << 0)

/*
 * Re-Distributor registers, offsets from VLPI_base
 */
#define GICR_VPROPBASER			0x0070

#define GICR_VPROPBASER_IDBITS_MASK	0x1f

#define GICR_VPROPBASER_SHAREABILITY_SHIFT		(10)
#define GICR_VPROPBASER_INNER_CACHEABILITY_SHIFT	(7)
#define GICR_VPROPBASER_OUTER_CACHEABILITY_SHIFT	(56)

#define GICR_VPROPBASER_SHAREABILITY_MASK				\
	GIC_BASER_SHAREABILITY(GICR_VPROPBASER, SHAREABILITY_MASK)
#define GICR_VPROPBASER_INNER_CACHEABILITY_MASK				\
	GIC_BASER_CACHEABILITY(GICR_VPROPBASER, INNER, MASK)
#define GICR_VPROPBASER_OUTER_CACHEABILITY_MASK				\
	GIC_BASER_CACHEABILITY(GICR_VPROPBASER, OUTER, MASK)
#define GICR_VPROPBASER_CACHEABILITY_MASK				\
	GICR_VPROPBASER_INNER_CACHEABILITY_MASK

#define GICR_VPROPBASER_InnerShareable					\
	GIC_BASER_SHAREABILITY(GICR_VPROPBASER, InnerShareable)

#define GICR_VPROPBASER_nCnB	GIC_BASER_CACHEABILITY(GICR_VPROPBASER, INNER, nCnB)
#define GICR_VPROPBASER_nC 	GIC_BASER_CACHEABILITY(GICR_VPROPBASER, INNER, nC)
#define GICR_VPROPBASER_RaWt	GIC_BASER_CACHEABILITY(GICR_VPROPBASER, INNER, RaWt)
#define GICR_VPROPBASER_RaWb	GIC_BASER_CACHEABILITY(GICR_VPROPBASER, INNER, RaWb)
#define GICR_VPROPBASER_WaWt	GIC_BASER_CACHEABILITY(GICR_VPROPBASER, INNER, WaWt)
#define GICR_VPROPBASER_WaWb	GIC_BASER_CACHEABILITY(GICR_VPROPBASER, INNER, WaWb)
#define GICR_VPROPBASER_RaWaWt	GIC_BASER_CACHEABILITY(GICR_VPROPBASER, INNER, RaWaWt)
#define GICR_VPROPBASER_RaWaWb	GIC_BASER_CACHEABILITY(GICR_VPROPBASER, INNER, RaWaWb)

#define GICR_VPENDBASER			0x0078

#define GICR_VPENDBASER_SHAREABILITY_SHIFT		(10)
#define GICR_VPENDBASER_INNER_CACHEABILITY_SHIFT	(7)
#define GICR_VPENDBASER_OUTER_CACHEABILITY_SHIFT	(56)
#define GICR_VPENDBASER_SHAREABILITY_MASK				\
	GIC_BASER_SHAREABILITY(GICR_VPENDBASER, SHAREABILITY_MASK)
#define GICR_VPENDBASER_INNER_CACHEABILITY_MASK				\
	GIC_BASER_CACHEABILITY(GICR_VPENDBASER, INNER, MASK)
#define GICR_VPENDBASER_OUTER_CACHEABILITY_MASK				\
	GIC_BASER_CACHEABILITY(GICR_VPENDBASER, OUTER, MASK)
#define GICR_VPENDBASER_CACHEABILITY_MASK				\
	GICR_VPENDBASER_INNER_CACHEABILITY_MASK

#define GICR_VPENDBASER_NonShareable					\
	GIC_BASER_SHAREABILITY(GICR_VPENDBASER, NonShareable)

#define GICR_VPENDBASER_nCnB	GIC_BASER_CACHEABILITY(GICR_VPENDBASER, INNER, nCnB)
#define GICR_VPENDBASER_nC 	GIC_BASER_CACHEABILITY(GICR_VPENDBASER, INNER, nC)
#define GICR_VPENDBASER_RaWt	GIC_BASER_CACHEABILITY(GICR_VPENDBASER, INNER, RaWt)
#define GICR_VPENDBASER_RaWb	GIC_BASER_CACHEABILITY(GICR_VPENDBASER, INNER, RaWb)
#define GICR_VPENDBASER_WaWt	GIC_BASER_CACHEABILITY(GICR_VPENDBASER, INNER, WaWt)
#define GICR_VPENDBASER_WaWb	GIC_BASER_CACHEABILITY(GICR_VPENDBASER, INNER, WaWb)
#define GICR_VPENDBASER_RaWaWt	GIC_BASER_CACHEABILITY(GICR_VPENDBASER, INNER, RaWaWt)
#define GICR_VPENDBASER_RaWaWb	GIC_BASER_CACHEABILITY(GICR_VPENDBASER, INNER, RaWaWb)

#define GICR_VPENDBASER_Dirty		(1ULL << 60)
#define GICR_VPENDBASER_PendingLast	(1ULL << 61)
#define GICR_VPENDBASER_IDAI		(1ULL << 62)
#define GICR_VPENDBASER_Valid		(1ULL << 63)

/*
 * ITS registers, offsets from ITS_base
 */
#define GITS_CTLR			0x0000
#define GITS_IIDR			0x0004
#define GITS_TYPER			0x0008
#define GITS_CBASER			0x0080
#define GITS_CWRITER			0x0088
#define GITS_CREADR			0x0090
#define GITS_BASER			0x0100
#define GITS_IDREGS_BASE		0xffd0
#define GITS_PIDR0			0xffe0
#define GITS_PIDR1			0xffe4
#define GITS_PIDR2			GICR_PIDR2
#define GITS_PIDR4			0xffd0
#define GITS_CIDR0			0xfff0
#define GITS_CIDR1			0xfff4
#define GITS_CIDR2			0xfff8
#define GITS_CIDR3			0xfffc

#define GITS_TRANSLATER			0x10040

#define GITS_CTLR_ENABLE		(1U << 0)
#define GITS_CTLR_ImDe			(1U << 1)
#define	GITS_CTLR_ITS_NUMBER_SHIFT	4
#define	GITS_CTLR_ITS_NUMBER		(0xFU << GITS_CTLR_ITS_NUMBER_SHIFT)
#define GITS_CTLR_QUIESCENT		(1U << 31)

#define GITS_TYPER_PLPIS		(1UL << 0)
#define GITS_TYPER_VLPIS		(1UL << 1)
#define GITS_TYPER_ITT_ENTRY_SIZE_SHIFT	4
#define GITS_TYPER_ITT_ENTRY_SIZE(r)	((((r) >> GITS_TYPER_ITT_ENTRY_SIZE_SHIFT) & 0xf) + 1)
#define GITS_TYPER_IDBITS_SHIFT		8
#define GITS_TYPER_DEVBITS_SHIFT	13
#define GITS_TYPER_DEVBITS(r)		((((r) >> GITS_TYPER_DEVBITS_SHIFT) & 0x1f) + 1)
#define GITS_TYPER_PTA			(1UL << 19)
#define GITS_TYPER_HCC_SHIFT		24
#define GITS_TYPER_HCC(r)		(((r) >> GITS_TYPER_HCC_SHIFT) & 0xff)
#define GITS_TYPER_VMOVP		(1ULL << 37)

#define GITS_IIDR_REV_SHIFT		12
#define GITS_IIDR_REV_MASK		(0xf << GITS_IIDR_REV_SHIFT)
#define GITS_IIDR_REV(r)		(((r) >> GITS_IIDR_REV_SHIFT) & 0xf)
#define GITS_IIDR_PRODUCTID_SHIFT	24

#define GITS_CBASER_VALID			(1ULL << 63)
#define GITS_CBASER_SHAREABILITY_SHIFT		(10)
#define GITS_CBASER_INNER_CACHEABILITY_SHIFT	(59)
#define GITS_CBASER_OUTER_CACHEABILITY_SHIFT	(53)
#define GITS_CBASER_SHAREABILITY_MASK					\
	GIC_BASER_SHAREABILITY(GITS_CBASER, SHAREABILITY_MASK)
#define GITS_CBASER_INNER_CACHEABILITY_MASK				\
	GIC_BASER_CACHEABILITY(GITS_CBASER, INNER, MASK)
#define GITS_CBASER_OUTER_CACHEABILITY_MASK				\
	GIC_BASER_CACHEABILITY(GITS_CBASER, OUTER, MASK)
#define GITS_CBASER_CACHEABILITY_MASK GITS_CBASER_INNER_CACHEABILITY_MASK

#define GITS_CBASER_InnerShareable					\
	GIC_BASER_SHAREABILITY(GITS_CBASER, InnerShareable)

#define GITS_CBASER_nCnB	GIC_BASER_CACHEABILITY(GITS_CBASER, INNER, nCnB)
#define GITS_CBASER_nC		GIC_BASER_CACHEABILITY(GITS_CBASER, INNER, nC)
#define GITS_CBASER_RaWt	GIC_BASER_CACHEABILITY(GITS_CBASER, INNER, RaWt)
#define GITS_CBASER_RaWb	GIC_BASER_CACHEABILITY(GITS_CBASER, INNER, RaWb)
#define GITS_CBASER_WaWt	GIC_BASER_CACHEABILITY(GITS_CBASER, INNER, WaWt)
#define GITS_CBASER_WaWb	GIC_BASER_CACHEABILITY(GITS_CBASER, INNER, WaWb)
#define GITS_CBASER_RaWaWt	GIC_BASER_CACHEABILITY(GITS_CBASER, INNER, RaWaWt)
#define GITS_CBASER_RaWaWb	GIC_BASER_CACHEABILITY(GITS_CBASER, INNER, RaWaWb)

#define GITS_BASER_NR_REGS		8

#define GITS_BASER_VALID			(1ULL << 63)
#define GITS_BASER_INDIRECT			(1ULL << 62)

#define GITS_BASER_INNER_CACHEABILITY_SHIFT	(59)
#define GITS_BASER_OUTER_CACHEABILITY_SHIFT	(53)
#define GITS_BASER_INNER_CACHEABILITY_MASK				\
	GIC_BASER_CACHEABILITY(GITS_BASER, INNER, MASK)
#define GITS_BASER_CACHEABILITY_MASK		GITS_BASER_INNER_CACHEABILITY_MASK
#define GITS_BASER_OUTER_CACHEABILITY_MASK				\
	GIC_BASER_CACHEABILITY(GITS_BASER, OUTER, MASK)
#define GITS_BASER_SHAREABILITY_MASK					\
	GIC_BASER_SHAREABILITY(GITS_BASER, SHAREABILITY_MASK)

#define GITS_BASER_nCnB		GIC_BASER_CACHEABILITY(GITS_BASER, INNER, nCnB)
#define GITS_BASER_nC		GIC_BASER_CACHEABILITY(GITS_BASER, INNER, nC)
#define GITS_BASER_RaWt		GIC_BASER_CACHEABILITY(GITS_BASER, INNER, RaWt)
#define GITS_BASER_RaWb		GIC_BASER_CACHEABILITY(GITS_BASER, INNER, RaWb)
#define GITS_BASER_WaWt		GIC_BASER_CACHEABILITY(GITS_BASER, INNER, WaWt)
#define GITS_BASER_WaWb		GIC_BASER_CACHEABILITY(GITS_BASER, INNER, WaWb)
#define GITS_BASER_RaWaWt	GIC_BASER_CACHEABILITY(GITS_BASER, INNER, RaWaWt)
#define GITS_BASER_RaWaWb	GIC_BASER_CACHEABILITY(GITS_BASER, INNER, RaWaWb)

#define GITS_BASER_TYPE_SHIFT			(56)
#define GITS_BASER_TYPE(r)		(((r) >> GITS_BASER_TYPE_SHIFT) & 7)
#define GITS_BASER_ENTRY_SIZE_SHIFT		(48)
#define GITS_BASER_ENTRY_SIZE(r)	((((r) >> GITS_BASER_ENTRY_SIZE_SHIFT) & 0x1f) + 1)
#define GITS_BASER_ENTRY_SIZE_MASK	GENMASK_ULL(52, 48)
#define GITS_BASER_PHYS_52_to_48(phys)					\
	(((phys) & GENMASK_ULL(47, 16)) | (((phys) >> 48) & 0xf) << 12)
#define GITS_BASER_SHAREABILITY_SHIFT	(10)
#define GITS_BASER_InnerShareable					\
	GIC_BASER_SHAREABILITY(GITS_BASER, InnerShareable)
#define GITS_BASER_PAGE_SIZE_SHIFT	(8)
#define GITS_BASER_PAGE_SIZE_4K		(0ULL << GITS_BASER_PAGE_SIZE_SHIFT)
#define GITS_BASER_PAGE_SIZE_16K	(1ULL << GITS_BASER_PAGE_SIZE_SHIFT)
#define GITS_BASER_PAGE_SIZE_64K	(2ULL << GITS_BASER_PAGE_SIZE_SHIFT)
#define GITS_BASER_PAGE_SIZE_MASK	(3ULL << GITS_BASER_PAGE_SIZE_SHIFT)
#define GITS_BASER_PAGES_MAX		256
#define GITS_BASER_PAGES_SHIFT		(0)
#define GITS_BASER_NR_PAGES(r)		(((r) & 0xff) + 1)

#define GITS_BASER_TYPE_NONE		0
#define GITS_BASER_TYPE_DEVICE		1
#define GITS_BASER_TYPE_VCPU		2
#define GITS_BASER_TYPE_RESERVED3	3
#define GITS_BASER_TYPE_COLLECTION	4
#define GITS_BASER_TYPE_RESERVED5	5
#define GITS_BASER_TYPE_RESERVED6	6
#define GITS_BASER_TYPE_RESERVED7	7

#define GITS_LVL1_ENTRY_SIZE           (8UL)

/*
 * ITS commands
 */
#define GITS_CMD_MAPD			0x08
#define GITS_CMD_MAPC			0x09
#define GITS_CMD_MAPTI			0x0a
#define GITS_CMD_MAPI			0x0b
#define GITS_CMD_MOVI			0x01
#define GITS_CMD_DISCARD		0x0f
#define GITS_CMD_INV			0x0c
#define GITS_CMD_MOVALL			0x0e
#define GITS_CMD_INVALL			0x0d
#define GITS_CMD_INT			0x03
#define GITS_CMD_CLEAR			0x04
#define GITS_CMD_SYNC			0x05

/*
 * GICv4 ITS specific commands
 */
#define GITS_CMD_GICv4(x)		((x) | 0x20)
#define GITS_CMD_VINVALL		GITS_CMD_GICv4(GITS_CMD_INVALL)
#define GITS_CMD_VMAPP			GITS_CMD_GICv4(GITS_CMD_MAPC)
#define GITS_CMD_VMAPTI			GITS_CMD_GICv4(GITS_CMD_MAPTI)
#define GITS_CMD_VMOVI			GITS_CMD_GICv4(GITS_CMD_MOVI)
#define GITS_CMD_VSYNC			GITS_CMD_GICv4(GITS_CMD_SYNC)
/* VMOVP is the odd one, as it doesn't have a physical counterpart */
#define GITS_CMD_VMOVP			GITS_CMD_GICv4(2)

/*
 * ITS error numbers
 */
#define E_ITS_MOVI_UNMAPPED_INTERRUPT		0x010107
#define E_ITS_MOVI_UNMAPPED_COLLECTION		0x010109
#define E_ITS_INT_UNMAPPED_INTERRUPT		0x010307
#define E_ITS_CLEAR_UNMAPPED_INTERRUPT		0x010507
#define E_ITS_MAPD_DEVICE_OOR			0x010801
#define E_ITS_MAPD_ITTSIZE_OOR			0x010802
#define E_ITS_MAPC_PROCNUM_OOR			0x010902
#define E_ITS_MAPC_COLLECTION_OOR		0x010903
#define E_ITS_MAPTI_UNMAPPED_DEVICE		0x010a04
#define E_ITS_MAPTI_ID_OOR			0x010a05
#define E_ITS_MAPTI_PHYSICALID_OOR		0x010a06
#define E_ITS_INV_UNMAPPED_INTERRUPT		0x010c07
#define E_ITS_INVALL_UNMAPPED_COLLECTION	0x010d09
#define E_ITS_MOVALL_PROCNUM_OOR		0x010e01
#define E_ITS_DISCARD_UNMAPPED_INTERRUPT	0x010f07

/*
 * CPU interface registers
 */
#define ICC_CTLR_EL1_EOImode_SHIFT	(1)
#define ICC_CTLR_EL1_EOImode_drop_dir	(0U << ICC_CTLR_EL1_EOImode_SHIFT)
#define ICC_CTLR_EL1_EOImode_drop	(1U << ICC_CTLR_EL1_EOImode_SHIFT)
#define ICC_CTLR_EL1_EOImode_MASK	(1 << ICC_CTLR_EL1_EOImode_SHIFT)
#define ICC_CTLR_EL1_CBPR_SHIFT		0
#define ICC_CTLR_EL1_CBPR_MASK		(1 << ICC_CTLR_EL1_CBPR_SHIFT)
#define ICC_CTLR_EL1_PRI_BITS_SHIFT	8
#define ICC_CTLR_EL1_PRI_BITS_MASK	(0x7 << ICC_CTLR_EL1_PRI_BITS_SHIFT)
#define ICC_CTLR_EL1_ID_BITS_SHIFT	11
#define ICC_CTLR_EL1_ID_BITS_MASK	(0x7 << ICC_CTLR_EL1_ID_BITS_SHIFT)
#define ICC_CTLR_EL1_SEIS_SHIFT		14
#define ICC_CTLR_EL1_SEIS_MASK		(0x1 << ICC_CTLR_EL1_SEIS_SHIFT)
#define ICC_CTLR_EL1_A3V_SHIFT		15
#define ICC_CTLR_EL1_A3V_MASK		(0x1 << ICC_CTLR_EL1_A3V_SHIFT)
#define ICC_CTLR_EL1_RSS		(0x1 << 18)
#define ICC_PMR_EL1_SHIFT		0
#define ICC_PMR_EL1_MASK		(0xff << ICC_PMR_EL1_SHIFT)
#define ICC_BPR0_EL1_SHIFT		0
#define ICC_BPR0_EL1_MASK		(0x7 << ICC_BPR0_EL1_SHIFT)
#define ICC_BPR1_EL1_SHIFT		0
#define ICC_BPR1_EL1_MASK		(0x7 << ICC_BPR1_EL1_SHIFT)
#define ICC_IGRPEN0_EL1_SHIFT		0
#define ICC_IGRPEN0_EL1_MASK		(1 << ICC_IGRPEN0_EL1_SHIFT)
#define ICC_IGRPEN1_EL1_SHIFT		0
#define ICC_IGRPEN1_EL1_MASK		(1 << ICC_IGRPEN1_EL1_SHIFT)
#define ICC_SRE_EL1_DIB			(1U << 2)
#define ICC_SRE_EL1_DFB			(1U << 1)
#define ICC_SRE_EL1_SRE			(1U << 0)

/*
 * Hypervisor interface registers (SRE only)
 */
#define ICH_LR_VIRTUAL_ID_MASK		((1ULL << 32) - 1)

#define ICH_LR_EOI			(1ULL << 41)
#define ICH_LR_GROUP			(1ULL << 60)
#define ICH_LR_HW			(1ULL << 61)
#define ICH_LR_STATE			(3ULL << 62)
#define ICH_LR_PENDING_BIT		(1ULL << 62)
#define ICH_LR_ACTIVE_BIT		(1ULL << 63)
#define ICH_LR_PHYS_ID_SHIFT		32
#define ICH_LR_PHYS_ID_MASK		(0x3ffULL << ICH_LR_PHYS_ID_SHIFT)
#define ICH_LR_PRIORITY_SHIFT		48
#define ICH_LR_PRIORITY_MASK		(0xffULL << ICH_LR_PRIORITY_SHIFT)

/* These are for GICv2 emulation only */
#define GICH_LR_VIRTUALID		(0x3ffUL << 0)
#define GICH_LR_PHYSID_CPUID_SHIFT	(10)
#define GICH_LR_PHYSID_CPUID		(7UL << GICH_LR_PHYSID_CPUID_SHIFT)

#define ICH_MISR_EOI			(1 << 0)
#define ICH_MISR_U			(1 << 1)

#define ICH_HCR_EN			(1 << 0)
#define ICH_HCR_UIE			(1 << 1)
#define ICH_HCR_NPIE			(1 << 3)
#define ICH_HCR_TC			(1 << 10)
#define ICH_HCR_TALL0			(1 << 11)
#define ICH_HCR_TALL1			(1 << 12)
#define ICH_HCR_EOIcount_SHIFT		27
#define ICH_HCR_EOIcount_MASK		(0x1f << ICH_HCR_EOIcount_SHIFT)

#define ICH_VMCR_ACK_CTL_SHIFT		2
#define ICH_VMCR_ACK_CTL_MASK		(1 << ICH_VMCR_ACK_CTL_SHIFT)
#define ICH_VMCR_FIQ_EN_SHIFT		3
#define ICH_VMCR_FIQ_EN_MASK		(1 << ICH_VMCR_FIQ_EN_SHIFT)
#define ICH_VMCR_CBPR_SHIFT		4
#define ICH_VMCR_CBPR_MASK		(1 << ICH_VMCR_CBPR_SHIFT)
#define ICH_VMCR_EOIM_SHIFT		9
#define ICH_VMCR_EOIM_MASK		(1 << ICH_VMCR_EOIM_SHIFT)
#define ICH_VMCR_BPR1_SHIFT		18
#define ICH_VMCR_BPR1_MASK		(7 << ICH_VMCR_BPR1_SHIFT)
#define ICH_VMCR_BPR0_SHIFT		21
#define ICH_VMCR_BPR0_MASK		(7 << ICH_VMCR_BPR0_SHIFT)
#define ICH_VMCR_PMR_SHIFT		24
#define ICH_VMCR_PMR_MASK		(0xffUL << ICH_VMCR_PMR_SHIFT)
#define ICH_VMCR_ENG0_SHIFT		0
#define ICH_VMCR_ENG0_MASK		(1 << ICH_VMCR_ENG0_SHIFT)
#define ICH_VMCR_ENG1_SHIFT		1
#define ICH_VMCR_ENG1_MASK		(1 << ICH_VMCR_ENG1_SHIFT)

#define ICH_VTR_PRI_BITS_SHIFT		29
#define ICH_VTR_PRI_BITS_MASK		(7 << ICH_VTR_PRI_BITS_SHIFT)
#define ICH_VTR_ID_BITS_SHIFT		23
#define ICH_VTR_ID_BITS_MASK		(7 << ICH_VTR_ID_BITS_SHIFT)
#define ICH_VTR_SEIS_SHIFT		22
#define ICH_VTR_SEIS_MASK		(1 << ICH_VTR_SEIS_SHIFT)
#define ICH_VTR_A3V_SHIFT		21
#define ICH_VTR_A3V_MASK		(1 << ICH_VTR_A3V_SHIFT)

#define ICC_IAR1_EL1_SPURIOUS		0x3ff

#define ICC_SRE_EL2_SRE			(1 << 0)
#define ICC_SRE_EL2_ENABLE		(1 << 3)

#define ICC_SGI1R_TARGET_LIST_SHIFT	0
#define ICC_SGI1R_TARGET_LIST_MASK	(0xffff << ICC_SGI1R_TARGET_LIST_SHIFT)
#define ICC_SGI1R_AFFINITY_1_SHIFT	16
#define ICC_SGI1R_AFFINITY_1_MASK	(0xff << ICC_SGI1R_AFFINITY_1_SHIFT)
#define ICC_SGI1R_SGI_ID_SHIFT		24
#define ICC_SGI1R_SGI_ID_MASK		(0xfULL << ICC_SGI1R_SGI_ID_SHIFT)
#define ICC_SGI1R_AFFINITY_2_SHIFT	32
#define ICC_SGI1R_AFFINITY_2_MASK	(0xffULL << ICC_SGI1R_AFFINITY_2_SHIFT)
#define ICC_SGI1R_IRQ_ROUTING_MODE_BIT	40
#define ICC_SGI1R_RS_SHIFT		44
#define ICC_SGI1R_RS_MASK		(0xfULL << ICC_SGI1R_RS_SHIFT)
#define ICC_SGI1R_AFFINITY_3_SHIFT	48
#define ICC_SGI1R_AFFINITY_3_MASK	(0xffULL << ICC_SGI1R_AFFINITY_3_SHIFT)

#include <asm/arch_gicv3.h>

#ifndef __ASSEMBLY__

/*
 * We need a value to serve as a irq-type for LPIs. Choose one that will
 * hopefully pique the interest of the reviewer.
 */
#define GIC_IRQ_TYPE_LPI		0xa110c8ed

struct rdists {
	struct {
		void __iomem	*rd_base;
		struct page	*pend_page;
		phys_addr_t	phys_base;
	} __percpu		*rdist;
	struct page		*prop_page;
	u64			flags;
	u32			gicd_typer;
	bool			has_vlpis;
	bool			has_direct_lpi;
};

struct irq_domain;
struct fwnode_handle;
int its_cpu_init(void);
int its_init(struct fwnode_handle *handle, struct rdists *rdists,
	     struct irq_domain *domain);
int mbi_init(struct fwnode_handle *fwnode, struct irq_domain *parent);

static inline bool gic_enable_sre(void)
{
	u32 val;

	val = gic_read_sre();
	if (val & ICC_SRE_EL1_SRE)
		return true;

	val |= ICC_SRE_EL1_SRE;
	gic_write_sre(val);
	val = gic_read_sre();

	return !!(val & ICC_SRE_EL1_SRE);
}

#endif

#endif
