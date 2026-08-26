cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME Electronic-Lock)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here

    # Application
    App/Core/Src/App_Core.c
    App/Config/Src/App_Config.c
    App/Config/Src/App_ConfigHalCallbacks.c
    App/Core/Src/App_Executor.c

    # Platform
    Platforms/Src/GPIO_Platform_Interface.c
    Platforms/Src/I2C_Platform_Interface.c
    Platforms/Src/PWM_Platform_Interface.c
    Platforms/Src/Time_Platform_Interface.c
    Platforms/Src/FLASH_Platform_Interface.c

    # Components
    Libs/Components/Buzzer/Src/Buzzer_Driver.c
    Libs/Components/DoorSensor/Src/DoorSensor_Driver.c
    Libs/Components/ExitButton/Src/ExitButton_Driver.c
    Libs/Components/LCD/Src/LCD_Driver.c
    Libs/Components/LCD/Src/LCD_PCF8574_BusAdapter.c
    Libs/Components/LCD/Src/LCD_PWM_BacklightAdapter.c
    Libs/Components/Led/Src/Led_Driver.c
    Libs/Components/LockActuator/Src/LockActuator_Driver.c
    Libs/Components/MatrixKeyboard/Src/MatrixKeyboard_Driver.c
    Libs/Components/MatrixKeyboard/Src/MatrixKeyboard_GPIO_ScanAdapter.c
    Libs/Components/PCF8574/Src/PCF8574_Driver.c

    # Services
    Libs/Services/Authentication/Src/Authentication_Service.c
    Libs/Services/Credential_Entry/Src/Credential_Entry_Service.c
    Libs/Services/Credential_Register/Src/Credential_Register_Service.c
    Libs/Services/Credential_Storage/Src/Credential_Storage_Service.c
    Libs/Services/Display_Render/Src/Display_Render_Service.c
    Libs/Services/Lock_Control/Src/Lock_Control_Service.c
    Libs/Services/Sound_Generator/Src/Sound_Generator_Service.c
    Libs/Services/Status_Indication/Src/Status_Indication_Service.c
    Libs/Services/Timeout_Validation/Src/Timeout_Validation_Service.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths

    # Application
    App/Core/Inc
    App/Config/Inc

    # Platform
    Platforms/Inc

    # Components
    Libs/Components/Buzzer/Inc
    Libs/Components/DoorSensor/Inc
    Libs/Components/ExitButton/Inc
    Libs/Components/LCD/Inc
    Libs/Components/Led/Inc
    Libs/Components/LockActuator/Inc
    Libs/Components/MatrixKeyboard/Inc
    Libs/Components/PCF8574/Inc

    # Services
    Libs/Services/Authentication/Inc
    Libs/Services/Credential_Entry/Inc
    Libs/Services/Credential_Register/Inc
    Libs/Services/Credential_Storage/Inc
    Libs/Services/Display_Render/Inc
    Libs/Services/Lock_Control/Inc
    Libs/Services/Sound_Generator/Inc
    Libs/Services/Status_Indication/Inc
    Libs/Services/Timeout_Validation/Inc
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
