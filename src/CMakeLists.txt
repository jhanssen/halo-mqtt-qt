set(SOURCES
    main.cpp
    Crypto.cpp
    HaloBluetooth.cpp
    HaloMqtt.cpp
    Locations.cpp
)

find_package(Qt6 REQUIRED COMPONENTS Bluetooth Core Network)

get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)

add_executable(halo-qt ${SOURCES})
target_link_libraries(halo-qt PRIVATE Qt6::Bluetooth Qt6::Core Qt6::Network Qt6::Mqtt QtAES::QtAES)
target_compile_features(halo-qt PRIVATE cxx_std_20)
set_property(TARGET halo-qt PROPERTY COMPILE_WARNING_AS_ERROR ON)
set_property(TARGET halo-qt PROPERTY AUTOMOC ON)

set_target_properties(halo-qt PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

target_link_options(halo-qt PRIVATE LINKER:-sectcreate,__TEXT,__info_plist,${CMAKE_CURRENT_SOURCE_DIR}/Info.plist)

# this wasn't obvious to add
target_link_libraries(halo-qt PRIVATE Qt6::QDarwinBluetoothPermissionPlugin)
