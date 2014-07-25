################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../test_src/orderbook_tests.cpp \
../test_src/test.cpp 

OBJS += \
./test_src/orderbook_tests.o \
./test_src/test.o 

CPP_DEPS += \
./test_src/orderbook_tests.d \
./test_src/test.d 


# Each subdirectory must supply rules for building sources it contributes
test_src/%.o: ../test_src/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -std=c++0x -O0 -g3 -Wall -Wextra -Werror -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


