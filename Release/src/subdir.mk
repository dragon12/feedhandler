################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/feedhandler.cpp \
../src/feedhandler_main.cpp \
../src/orderbook.cpp 

OBJS += \
./src/feedhandler.o \
./src/feedhandler_main.o \
./src/orderbook.o 

CPP_DEPS += \
./src/feedhandler.d \
./src/feedhandler_main.d \
./src/orderbook.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -std=c++0x -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


