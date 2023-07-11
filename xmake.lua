local project_name = "Project"
local target_dir = "xmake-build"

set_project(project_name)
set_version("v0.2")

add_rules("mode.debug", "mode.release", "mode.releasedbg", "mode.minsizerel")
set_defaultmode("releasedbg")

toolchain("arm-none-eabi")
    set_kind("standalone")

    set_toolset("cc", "arm-none-eabi-gcc")
    set_toolset("cxx", "arm-none-eabi-g++")
    set_toolset("as", "arm-none-eabi-gcc")
    set_toolset("ld", "arm-none-eabi-gcc")
toolchain_end()

-- basic board info
target(project_name)
    local CPU = "-mcpu=cortex-m4"
    local FPU = "-mfpu=fpv4-sp-d16"
    local FLOAT_ABI = "-mfloat-abi=hard"
    local LDSCRIPT = "hw/bsp/rm-c/ld/LinkerScripts.ld"

    add_defines("USE_HAL_DRIVER", "STM32F407xx")
    add_cflags(CPU, "-mthumb", FPU, FLOAT_ABI, "-fdata-sections", "-ffunction-sections", {force = true})
    add_cxflags(CPU, "-mthumb", FPU, FLOAT_ABI, "-fdata-sections", "-ffunction-sections", {force = true})
    add_asflags(CPU, "-mthumb", FPU, FLOAT_ABI, "-fdata-sections", "-ffunction-sections", {force = true})
    add_ldflags(CPU, "-mthumb", FPU, FLOAT_ABI, "-specs=nano.specs", "-T"..LDSCRIPT, "-lm -lc -lnosys", "-Wl,-Map=" .. target_dir .. "/" .. project_name .. ".map,--cref -Wl,--gc-sections", {force = true})
    add_syslinks("m", "c", "nosys")

target_end()

-- add files
target(project_name)
    add_files(
    "hw/bsp/rm-c/main.cpp",
    "hw/bsp/rm-c/drivers/hal/Core/Src/*.c",
    "hw/bsp/rm-c/drivers/rtos/*.c",
    "hw/bsp/rm-c/drivers/flash/*.c",
    "hw/bsp/rm-c/drivers/usb/*.c",
    "hw/bsp/rm-c/drivers/*.c",
    "hw/mcu/st/stm32f4xx_hal_driver/Src/*.c",
    "lib/freertos/*.c",
    "lib/freertos/portable/MemMang/heap_4.c",
    "lib/freertos/portable/GCC/ARM_CM4F/port.c",
    "lib/tinyusb/src/tusb.c",
    "lib/tinyusb/src/common/tusb_fifo.c",
    "lib/tinyusb/src/device/usbd.c",
    "lib/tinyusb/src/device/usbd_control.c",
    "lib/tinyusb/src/class/cdc/cdc_device.c",
    "lib/tinyusb/src/class/dfu/dfu_rt_device.c",
    "lib/tinyusb/src/class/hid/hid_device.c",
    "lib/tinyusb/src/class/midi/midi_device.c",
    "lib/tinyusb/src/class/msc/msc_device.c",
    "lib/tinyusb/src/class/net/net_device.c",
    "lib/tinyusb/src/class/usbtmc/usbtmc_device.c",
    "lib/tinyusb/src/class/vendor/vendor_device.c",
    "lib/tinyusb/src/portable/st/synopsys/dcd_synopsys.c",
    "lib/easyflash/*.c",
    "hw/mcu/st/cmsis_device_f4/Source/Templates/system_stm32f4xx.c",
    "hw/mcu/st/cmsis_device_f4/Source/Templates/gcc/startup_stm32f407xx.s"
    )

    remove_files(
    "*stream_buffer.c",
    "*template.c"
    )

    add_includedirs(
    "hw/bsp/rm-c/drivers/hal/Core/Inc",
    "hw/mcu/st/stm32f4xx_hal_driver/Inc",
    "hw/mcu/default",
    "hw/mcu/st/stm32f4xx_hal_driver/Inc/Legacy",
    "hw/bsp/rm-c/drivers/rtos",
    "hw/bsp/rm-c/drivers/flash",
    "hw/bsp/rm-c/drivers/usb",
    "hw/mcu/st/cmsis_device_f4/Include",
    "lib/cmsis_5/CMSIS/Core/Include",
    "lib/tinyusb/src",
    "lib/easyflash",
    "hw/bsp/rm-c/drivers",
    "lib/freertos/include",
    "lib/freertos/portable/GCC/ARM_CM4F"
    )

target_end()

-- other config
target(project_name)
    set_targetdir(target_dir)
    set_objectdir(target_dir .. "/obj")
    set_dependir(target_dir .. "/dep")
    set_kind("binary")
    set_extension(".elf")

    add_toolchains("arm-none-eabi")
    set_warnings("all")
    set_languages("c11", "cxx17")

    if is_mode("debug") then
        set_symbols("debug")
        add_cxflags("-Og", "-gdwarf-2", {force = true})
        add_asflags("-Og", "-gdwarf-2", {force = true})
    elseif is_mode("release") then
        set_symbols("hidden")
        set_optimize("fastest")
        set_strip("all")
    elseif is_mode("releasedbg") then
        set_optimize("fastes")
        set_symbols("debug")
        set_strip("all")
    elseif is_mode() then
        set_symbols("hidden")
        set_optimize("smallest")
        set_strip("all")
    end

target_end()

after_build(function(target)
    import("core.project.task")
    cprint("${bright black onwhite}********************储存空间占用情况*****************************")
    os.exec(string.format("arm-none-eabi-objcopy -O ihex %s.elf %s.hex", target_dir .. '/' .. project_name, target_dir .. '/' .. project_name))
    os.exec(string.format("arm-none-eabi-objcopy -O binary %s.elf %s.bin", target_dir .. '/' .. project_name, target_dir .. '/' .. project_name))
    os.exec(string.format("arm-none-eabi-size -Ax %s.elf", target_dir .. '/' .. project_name))
    os.exec(string.format("arm-none-eabi-size -Bd %s.elf", target_dir .. '/' .. project_name))
    cprint("${bright black onwhite}heap-堆、stack-栈、.data-已初始化的变量全局/静态变量，bss-未初始化的data、.text-代码和常量")
end)

on_run(function(target)
    os.exec("openocd -f %s -c 'program ./%s/%s.elf verify reset exit'", download_cfg, target_dir, project_name)
end)
