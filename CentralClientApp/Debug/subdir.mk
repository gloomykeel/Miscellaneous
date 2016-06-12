################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../UdpSender.cpp \
../main.cpp \
../tinyxml.cpp \
../tinyxmlerror.cpp \
../tinyxmlparser.cpp 

OBJS += \
./UdpSender.o \
./main.o \
./tinyxml.o \
./tinyxmlerror.o \
./tinyxmlparser.o 

CPP_DEPS += \
./UdpSender.d \
./main.d \
./tinyxml.d \
./tinyxmlerror.d \
./tinyxmlparser.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


