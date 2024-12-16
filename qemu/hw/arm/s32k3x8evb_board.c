//System Includes
#include "qemu/osdep.h"
#include "hw/core/split-irq.h"
#include "hw/sysbus.h"
#include "hw/sd/sd.h"
#include "hw/ssi/ssi.h"
#include "hw/arm/boot.h"
#include "qemu/timer.h"
#include "exec/memory.h"
#include "hw/i2c/i2c.h"
#include "hw/boards.h"
#include "qemu/log.h"
#include "exec/address-spaces.h"
#include "hw/timer/cmsdk-apb-timer.h"
#include "sysemu/sysemu.h"
#include "hw/arm/armv7m.h"
#include "hw/char/pl011.h"
#include "hw/irq.h"
#include "migration/vmstate.h"
#include "hw/misc/unimp.h"
#include "hw/qdev-clock.h"
#include "qom/object.h"
#include "qapi/qmp/qlist.h"
#include "ui/input.h"
#include "qemu/typedefs.h"

/*------------------------------------------------------------------------------*/
// Define some functions useful in the code.
int load_image_targphys(const char *filename, hwaddr addr, hwaddr size);
// Function to load the firmware
void s32k3x8_load_firmware(ARMCPU *cpu, MachineState *ms, MemoryRegion *flash, const char *firmware_filename);
// Function to initialize the memory regions
void s32k3x8_initialize_memory_regions(MemoryRegion *system_memory);



/*------------------------------------------------------------------------------*/
// Define constants for memory regions
#define FLASH_BASE_ADDR  0x00000000        // Flash memory base address
#define FLASH_SIZE       0x00C00000        // 12 MB program flash

#define SRAM_BASE_ADDR   0x20400000        // SRAM base address
#define SRAM_SIZE        0x00240000        // 2.25 MB SRAM

#define UART_BASE_ADDR   0x4006A000        // UART base address

#define PIT_TIMER1_BASE_ADDR    0x40037000  // PIT base address

#define PIT_TIMER2_BASE_ADDR    0x40038000  // PIT base address



/*------------------------------------------------------------------------------*/
// Define the machine state

#define TYPE_S32K3X8EVB_SYS "s32k3x8evb-sys"
OBJECT_DECLARE_SIMPLE_TYPE(ssys_state, S32K3X8EVB_SYS)
typedef struct S32K3X8MachineClass S32K3X8MachineClass;



/*------------------------------------------------------------------------------*/
// Creation of the syss_state struct that represents the state of the system
struct ssys_state {
    SysBusDevice parent_obj;
    MemoryRegion iomem;
    uint32_t rcc;
    uint32_t rcc2;
    qemu_irq irq;
    Clock *sysclk;
    Clock *refclk;
// Define the machine state
};



/*------------------------------------------------------------------------------*/
// Creation of the S32K3X8MachineState struct that represents the state of the machine
struct S32K3X8MachineState {
    MachineState *parent_obj;
    ssys_state sys;
    ARMv7MState nvic;
};
typedef struct S32K3X8MachineState S32K3X8MachineState;



/*------------------------------------------------------------------------------*/
// Creation of the names of the board seen by QEMU.
#define TYPE_S32K3X8_BASE_MACHINE MACHINE_TYPE_NAME("s32k3x8")
#define TYPE_S32K3X8_MACHINE MACHINE_TYPE_NAME("s32k3x8-board")

DECLARE_INSTANCE_CHECKER(S32K3X8MachineState, S32K3X8_MACHINE, TYPE_S32K3X8_MACHINE)



/*------------------------------------------------------------------------------*/
// Implementation of the function to initialize the memory regions

void s32k3x8_initialize_memory_regions(MemoryRegion *system_memory) {

	fprintf(stdout, "\n\n--------------- Initialization of the memory regions ---------------\n");
    MemoryRegion *flash = g_new(MemoryRegion, 1);
    MemoryRegion *sram = g_new(MemoryRegion, 1);

	// Flash memory initialization (Read-Only)
	fprintf(stdout, "\nInitializing flash memory...\n\n");
	memory_region_init_rom(flash, NULL, "s32k3x8.flash", FLASH_SIZE, &error_fatal);
	memory_region_add_subregion(system_memory, FLASH_BASE_ADDR, flash);

	// SRAM memory initialization (RAM - Read-Write)
	fprintf(stdout, "Initializing SRAM memory...\n\n");
	memory_region_init_ram(sram, NULL, "s32k3x8.sram", SRAM_SIZE, &error_fatal);
	memory_region_add_subregion_overlap(system_memory, SRAM_BASE_ADDR, sram, 0);

	fprintf(stdout, "Memory regions initialized successfully.\n\n");
}


/*------------------------------------------------------------------------------*/
// Function to initialize the S32K3X8 board for QEMU
static void s32k3x8_init(MachineState *ms) {


    fprintf(stdout, "\n\n===================== Initializing the System =====================\n");


    /*--------------------------------------------------------------------------------------*/
    /*------------Declaration of pointers for various QEMU device models--------------------*/
    /*--------------------------------------------------------------------------------------*/

    DeviceState *nvic; // Device models for NVIC and PIT timer
    Object *soc_container; // Container object for the System-on-Chip (SoC)
    DeviceState *syss_dev; // Device state for the system controller
    DeviceState *pit_timer1,*pit_timer2; //DeviceState for the 
    MemoryRegion *system_memory; //initialize the pointer to the system memory

    /*--------------------------------------------------------------------------------------*/
    /*------------Allocate memory and initialize the machine state structure----------------*/
    /*--------------------------------------------------------------------------------------*/

    S32K3X8MachineState *m_state = g_new0(S32K3X8MachineState, 1);

    //The machine state in qemu represents the state of the machine at runtime
    m_state->parent_obj = ms; // Link the machine state to the parent machine state object

    /*--------------------------------------------------------------------------------------*/
    /*---------------Obtain a reference to the global system memory region------------------*/
    /*--------------------------------------------------------------------------------------*/

    system_memory = get_system_memory();
    // Initialize memory regions for flash, SRAM, etc.
    s32k3x8_initialize_memory_regions(system_memory);

    /*--------------------------------------------------------------------------------------*/
    /*------------------------Create a container object for the SoC-------------------------*/
    /*--------------------------------------------------------------------------------------*/

    // This organizes all devices under the "soc" hierarchy
    soc_container = object_new("container");

    // Add the SoC container as a child of the machine state object
    object_property_add_child(OBJECT(ms), "soc", soc_container);

    // Initialize the system controller (represents core system functionality)
    object_initialize_child(OBJECT(ms), "sys", &m_state->sys, TYPE_S32K3X8EVB_SYS);
    sysbus_realize(SYS_BUS_DEVICE(&m_state->sys), &error_abort); // Realize the system controller

    // Create and add another instance of the system controller for the SoC
    syss_dev = qdev_new(TYPE_S32K3X8EVB_SYS);
    object_property_add_child(soc_container, "sys", OBJECT(syss_dev));

    // Realize and activate the system controller device model
    sysbus_realize_and_unref(SYS_BUS_DEVICE(syss_dev), &error_fatal);


    /*--------------------------------------------------------------------------------------*/
    /*------------------------ Initialize a clock for the system----------------------------*/
    /*--------------------------------------------------------------------------------------*/

    fprintf(stdout, "\n------------------ Initialization of the system Clock ------------------\n");

    m_state->sys.sysclk = clock_new(OBJECT(DEVICE(&m_state->sys)), "sysclk"); // Create clock object
    // Set the clock period to 40.69ns (equivalent to 24.5MHz frequency)
    clock_set_ns(m_state->sys.sysclk, 40.69);

    m_state->sys.refclk = clock_new(OBJECT(DEVICE(&m_state->sys)), "refclk");
    clock_set_hz(m_state->sys.refclk, 1000000);

    // Log the successful clock initialization
    fprintf(stdout, "\nClock initialized.\n\n");


    /*--------------------------------------------------------------------------------------*/
    /*-------------Initialize the Nested Vectored Interrupt Controller (NVIC)---------------*/
    /*--------------------------------------------------------------------------------------*/

    fprintf(stdout, "\n------------------ Initialization of the system NVIC ------------------\n");

    nvic = qdev_new(TYPE_ARMV7M); // Create a new NVIC device model

    // Add the NVIC to the SoC container
    object_property_add_child(soc_container, "v7m", OBJECT(nvic));

    // Configure the NVIC with the number of IRQs (128 for S32K3X8)
    qdev_prop_set_uint32(nvic, "num-irq", 32);

    // Configure the number of priority bits for the NVIC
    qdev_prop_set_uint8(nvic, "num-prio-bits", 4);

    // Connect the NVIC to the system clock
    qdev_connect_clock_in(nvic, "cpuclk", m_state->sys.sysclk);
    qdev_connect_clock_in(nvic, "refclk", m_state->sys.refclk);

    // Set the CPU type for the NVIC (retrieved from the machine state)
    //In particular we are setting the cortex-m7 cpu type
    qdev_prop_set_string(nvic, "cpu-type", m_state->parent_obj->cpu_type);

    // Enable bit-band support for the NVIC
    qdev_prop_set_bit(nvic, "enable-bitband", true);

    // Link the NVIC's memory access to the system memory
    object_property_set_link(OBJECT(nvic), "memory", OBJECT(get_system_memory()), &error_abort);

    // Realize and activate the NVIC model
    sysbus_realize_and_unref(SYS_BUS_DEVICE(nvic), &error_fatal);

    // Log the successful realization of the NVIC
    fprintf(stdout, "\nNVIC realized.\n");


    /*--------------------------------------------------------------------------------------*/
    /*--------------------------Initialize the UART device----------------------------------*/
    /*--------------------------------------------------------------------------------------*/

    fprintf(stdout, "\n---------------------- Initialization of the UART ---------------------\n");


    pl011_create(UART_BASE_ADDR, qdev_get_gpio_in(nvic, 0), serial_hd(0));

    fprintf(stdout, "\nUART initialized and connected to NVIC.\n\n");



    /*--------------------------------------------------------------------------------------*/
    /*-------------------------- Initialize the PIT timer-----------------------------------*/
    /*--------------------------------------------------------------------------------------*/

    fprintf(stdout, "\n------------------- Initialization of the Two timers ------------------\n");
    //First Timer
    pit_timer1 = qdev_new(TYPE_CMSDK_APB_TIMER);
    SysBusDevice *sbd1;
    sbd1 = SYS_BUS_DEVICE(pit_timer1);
    qdev_connect_clock_in(DEVICE(pit_timer1), "pclk", m_state->sys.sysclk);
    sysbus_realize_and_unref(sbd1, &error_fatal);
    sysbus_mmio_map(sbd1, 0, PIT_TIMER1_BASE_ADDR);
    sysbus_connect_irq(sbd1, 0, qdev_get_gpio_in(nvic, 8));

    fprintf(stdout,"\nFirst Timer Initialized Correctly\n\n");

    //Second Timer
    pit_timer2 = qdev_new(TYPE_CMSDK_APB_TIMER);
    SysBusDevice *sbd2;
    sbd2 = SYS_BUS_DEVICE(pit_timer2);
    qdev_connect_clock_in(DEVICE(pit_timer2), "pclk", m_state->sys.sysclk);
    sysbus_realize_and_unref(sbd2, &error_fatal);
    sysbus_mmio_map(sbd2, 0, PIT_TIMER2_BASE_ADDR);
    sysbus_connect_irq(sbd2, 0, qdev_get_gpio_in(nvic, 9));

    fprintf(stdout,"\nSecond Timer Initialized Correctly\n\n");



    /*--------------------------------------------------------------------------------------*/
    /*--------------------Load firmware into the emulated flash memory----------------------*/
    /*--------------------------------------------------------------------------------------*/
    fprintf(stdout, "\n--------------- Loading the Kernel into the flash memory ---------------\n");
    // The firmware file is specified in the machine state (ms->kernel_filename)
    armv7m_load_kernel(ARM_CPU(first_cpu), ms->kernel_filename, 0, FLASH_SIZE);

    // Log the successful loading of the firmware
    fprintf(stdout, "\nKernel loaded into flash memory.\n\n");

    // Log the completion of the board initialization
    fprintf(stdout, "System initialized.\n\n");

    fprintf(stdout, "Starting to run...\n");

    fprintf(stdout, "\n-----------------------------------------------------------------------\n");
}



/*------------------------------------------------------------------------------*/

// Implementation of the class init function

static void s32k3x8_class_init(ObjectClass *oc, void *data) {
    MachineClass *mc = MACHINE_CLASS(oc);
    mc->name = g_strdup("s32k3x8evb");
    mc->desc = "NXP S32K3X8 EVB (Cortex-M7)";
    mc->init = s32k3x8_init;
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("cortex-m7");
    mc->default_cpus = 1;
    mc->min_cpus = mc->default_cpus;
    mc->max_cpus = mc->default_cpus;
    mc->no_floppy = 1;
    mc->no_cdrom = 1;
    mc->no_parallel = 1;
}



/*------------------------------------------------------------------------------*/

// Section of code to define the infos of the board, wehere we do specify the functions to run for the init and class init

static const TypeInfo s32k3x8_machine_types = {
    .name           = TYPE_S32K3X8_MACHINE,
    .parent         = TYPE_MACHINE,
    .class_init     = s32k3x8_class_init,
};

static const TypeInfo s32k3x8evb_sys_info = {
    .name          = TYPE_S32K3X8EVB_SYS,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(ssys_state),
};

static void s32k3x8evb_machine_init(void) {
    type_register_static(&s32k3x8_machine_types);
    type_register_static(&s32k3x8evb_sys_info);
}

type_init(s32k3x8evb_machine_init);

/*------------------------------------------------------------------------------*/

