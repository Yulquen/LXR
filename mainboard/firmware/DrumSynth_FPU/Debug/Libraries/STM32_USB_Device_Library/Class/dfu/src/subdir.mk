################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Libraries/STM32_USB_Device_Library/Class/dfu/src/usbd_dfu_core.c \
../Libraries/STM32_USB_Device_Library/Class/dfu/src/usbd_dfu_mal.c \
../Libraries/STM32_USB_Device_Library/Class/dfu/src/usbd_flash_if.c \
../Libraries/STM32_USB_Device_Library/Class/dfu/src/usbd_mem_if_template.c \
../Libraries/STM32_USB_Device_Library/Class/dfu/src/usbd_otp_if.c 

OBJS += \
./Libraries/STM32_USB_Device_Library/Class/dfu/src/usbd_dfu_core.o \
./Libraries/STM32_USB_Device_Library/Class/dfu/src/usbd_dfu_mal.o \
./Libraries/STM32_USB_Device_Library/Class/dfu/src/usbd_flash_if.o \
./Libraries/STM32_USB_Device_Library/Class/dfu/src/usbd_mem_if_template.o \
./Libraries/STM32_USB_Device_Library/Class/dfu/src/usbd_otp_if.o 

C_DEPS += \
./Libraries/STM32_USB_Device_Library/Class/dfu/src/usbd_dfu_core.d \
./Libraries/STM32_USB_Device_Library/Class/dfu/src/usbd_dfu_mal.d \
./Libraries/STM32_USB_Device_Library/Class/dfu/src/usbd_flash_if.d \
./Libraries/STM32_USB_Device_Library/Class/dfu/src/usbd_mem_if_template.d \
./Libraries/STM32_USB_Device_Library/Class/dfu/src/usbd_otp_if.d 


# Each subdirectory must supply rules for building sources it contributes
Libraries/STM32_USB_Device_Library/Class/dfu/src/%.o: ../Libraries/STM32_USB_Device_Library/Class/dfu/src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: ARM Windows GCC C Compiler'
	arm-none-eabi-gcc -DSTM32F4XX -DUSE_USB_OTG_FS -DNDEBUG -DHSE_VALUE=8000000 -DUSE_STDPERIPH_DRIVER -DUSE_STM32F4_DISCOVERY -I"C:\stm32DevTools\ArmGcc\lib\gcc\arm-none-eabi\4.6.2\include" -I"C:\stm32DevTools\ArmGcc\lib\gcc\arm-none-eabi\4.6.2\include-fixed" -I"C:\stm32DevTools\ArmGcc\arm-none-eabi\include" -I"E:\Eclipse_Drum_Synth_STM32\DrumSynth_Workspace\DrumSynth_FPU\Libraries\CMSIS\Include" -I"E:\Eclipse_Drum_Synth_STM32\DrumSynth_Workspace\DrumSynth_FPU\Libraries\Device\STM32F4xx\Include" -I"E:\Eclipse_Drum_Synth_STM32\DrumSynth_Workspace\DrumSynth_FPU\Libraries\STM32F4-Discovery" -I"E:\Eclipse_Drum_Synth_STM32\DrumSynth_Workspace\DrumSynth_FPU\Libraries\STM32F4xx_StdPeriph_Driver\inc" -I"E:\Eclipse_Drum_Synth_STM32\DrumSynth_Workspace\DrumSynth_FPU\src" -I"E:\Eclipse_Drum_Synth_STM32\DrumSynth_Workspace\DrumSynth_FPU\src\AudioCodecManager" -I"E:\Eclipse_Drum_Synth_STM32\DrumSynth_Workspace\DrumSynth_FPU\src\MIDI" -I"E:\Eclipse_Drum_Synth_STM32\DrumSynth_Workspace\DrumSynth_FPU\src\DSPAudio" -I"E:\Eclipse_Drum_Synth_STM32\DrumSynth_Workspace\DrumSynth_FPU\src\Sequencer" -I"E:\Eclipse_Drum_Synth_STM32\DrumSynth_Workspace\DrumSynth_FPU\src\Hardware" -I"E:\Eclipse_Drum_Synth_STM32\DrumSynth_Workspace\DrumSynth_FPU\src\Hardware\SD_FAT" -I"E:\Eclipse_Drum_Synth_STM32\DrumSynth_Workspace\DrumSynth_FPU\src\Hardware\USB" -I"E:\Eclipse_Drum_Synth_STM32\DrumSynth_Workspace\DrumSynth_FPU\Libraries\STM32_USB_OTG_Driver" -I"E:\Eclipse_Drum_Synth_STM32\DrumSynth_Workspace\DrumSynth_FPU\Libraries\STM32_USB_OTG_Driver\inc" -I"E:\Eclipse_Drum_Synth_STM32\DrumSynth_Workspace\DrumSynth_FPU\Libraries\STM32_USB_Device_Library" -I"E:\Eclipse_Drum_Synth_STM32\DrumSynth_Workspace\DrumSynth_FPU\Libraries\STM32_USB_Device_Library\Core\inc" -O2 -ffunction-sections -fdata-sections -ffast-math -freciprocal-math -fsingle-precision-constant -Wall -Wa,-adhlns="$@.lst" -c -fmessage-length=0 -mfloat-abi=hard -mfpu=fpv4-sp-d16 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -mcpu=cortex-m4 -mthumb -mthumb-interwork -g3 -gdwarf-2 -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


