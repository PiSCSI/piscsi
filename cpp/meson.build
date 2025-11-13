controllers_src = [
    'controllers/abstract_controller.cpp',
    'controllers/controller_manager.cpp',
    'controllers/phase_handler.cpp',
    'controllers/scsi_controller.cpp',
]
devices_src = [
    'devices/cd_track.cpp',
    'devices/ctapdriver.cpp',
    'devices/device_factory.cpp',
    'devices/device_logger.cpp',
    'devices/device.cpp',
    'devices/disk_cache.cpp',
    'devices/disk_track.cpp',
    'devices/disk.cpp',
    'devices/host_services.cpp',
    'devices/mode_page_device.cpp',
    'devices/primary_device.cpp',
    'devices/scsi_command_util.cpp',
    'devices/scsi_daynaport.cpp',
    'devices/scsi_printer.cpp',
    'devices/scsi_streamer.cpp',
    'devices/scsicd.cpp',
    'devices/scsihd_nec.cpp',
    'devices/scsihd.cpp',
    'devices/scsimo.cpp',
    'devices/storage_device.cpp',
]
hal_src = [
    'hal/bus.cpp',
    'hal/data_sample.cpp',
    'hal/gpiobus_factory.cpp',
    'hal/gpiobus_raspberry.cpp',
    'hal/gpiobus_virtual.cpp',
    'hal/gpiobus.cpp',
    'hal/sbc_version.cpp',
    'hal/systimer_raspberry.cpp',
    'hal/systimer.cpp',
]
piscsi_core_src = [
    'piscsi/command_context.cpp',
    'piscsi/localizer.cpp',
    'piscsi/piscsi_core.cpp',
    'piscsi/piscsi_executor.cpp',
    'piscsi/piscsi_image.cpp',
    'piscsi/piscsi_response.cpp',
    'piscsi/piscsi_service.cpp',
] + controllers_src + devices_src + hal_src
piscsi_src = [
    'piscsi/piscsi.cpp',
]
scsictl_core_src = [
    'scsictl/scsictl_commands.cpp',
    'scsictl/scsictl_core.cpp',
    'scsictl/scsictl_display.cpp',
    'scsictl/scsictl_parser.cpp',
]
scsictl_src = [
    'scsictl/scsictl.cpp',
]
scsidump_core_src = [
    'scsidump/scsidump_core.cpp',
]
scsidump_src = [
    'scsidump/scsidump.cpp',
] + scsidump_core_src + hal_src
scsiloop_src = [
    'scsiloop/scsiloop.cpp',
    'scsiloop/scsiloop_core.cpp',
    'scsiloop/scsiloop_cout.cpp',
    'scsiloop/scsiloop_gpio.cpp',
    'scsiloop/scsiloop_timer.cpp',
] + hal_src
scsimon_src = [
    'scsimon/scsimon.cpp',
    'scsimon/sm_core.cpp',
    'scsimon/sm_html_report.cpp',
    'scsimon/sm_json_report.cpp',
    'scsimon/sm_vcd_report.cpp',
] + hal_src
shared_src = [
    'shared/network_util.cpp',
    'shared/piscsi_util.cpp',
    'shared/piscsi_version.cpp',
]
protobuf_src = [
    'shared/protobuf_util.cpp',
]
test_src = [
    'test/abstract_controller_test.cpp',
    'test/bus_test.cpp',
    'test/command_context_test.cpp',
    'test/controller_manager_test.cpp',
    'test/ctapdriver_test.cpp',
    'test/device_factory_test.cpp',
    'test/device_test.cpp',
    'test/disk_test.cpp',
    'test/gpiobus_raspberry_test.cpp',
    'test/host_services_test.cpp',
    'test/linux_os_stubs.cpp',
    'test/localizer_test.cpp',
    'test/mode_page_device_test.cpp',
    'test/network_util_test.cpp',
    'test/phase_handler_test.cpp',
    'test/piscsi_exceptions_test.cpp',
    'test/piscsi_executor_test.cpp',
    'test/piscsi_image_test.cpp',
    'test/piscsi_response_test.cpp',
    'test/piscsi_service_test.cpp',
    'test/piscsi_util_test.cpp',
    'test/primary_device_test.cpp',
    'test/protobuf_util_test.cpp',
    'test/scsi_command_util_test.cpp',
    'test/scsi_controller_test.cpp',
    'test/scsi_daynaport_test.cpp',
    'test/scsi_printer_test.cpp',
    'test/scsicd_test.cpp',
    'test/scsictl_commands_test.cpp',
    'test/scsictl_display_test.cpp',
    'test/scsictl_parser_test.cpp',
    'test/scsidump_test.cpp',
    'test/scsihd_nec_test.cpp',
    'test/scsihd_test.cpp',
    'test/scsimo_test.cpp',
    'test/storage_device_test.cpp',
    'test/test_setup.cpp',
    'test/test_shared.cpp',
] + scsidump_core_src

subdir('generated')

if to_build.contains('piscsi')
    piscsi_bin = executable('piscsi',
        piscsi_core_src + piscsi_src + shared_src + protobuf_src + piscsi_interface_src[0],
        dependencies : [pthread_dep, pcap_dep, protobuf_dep],
        install : true,
    )
endif

if to_build.contains('scsictl')
    scsictl_bin = executable('scsictl',
        scsictl_core_src + scsictl_src + shared_src + protobuf_src + piscsi_interface_src[0],
        dependencies : [pthread_dep, protobuf_dep],
        install : true,
    )
endif

if to_build.contains('scsimon')
    scsimon_bin = executable('scsimon',
        scsimon_src + shared_src,
        dependencies : [pthread_dep],
        install : true,
    )
endif

if to_build.contains('scsiloop')
    scsiloop_bin = executable('scsiloop',
        scsiloop_src + shared_src,
        dependencies : [pthread_dep],
        install : true,
    )
endif

if connect_type == 'FULLSPEC' and to_build.contains('scsidump')
    scsidump_bin = executable('scsidump',
        scsidump_src + shared_src,
        dependencies : [pthread_dep],
        install : true,
    )
endif

if gtest_dep.found() and gmock_dep.found() and to_build.contains('test')
    test_bin = executable('piscsi_test',
        piscsi_core_src + scsictl_core_src + test_src + shared_src + protobuf_src + piscsi_interface_src[0],
        dependencies : [pthread_dep, pcap_dep, protobuf_dep, gtest_dep, gmock_dep],
        link_args : ['-Wl,--wrap=fopen64'],
        install : false
    )
    test('unit', test_bin)
endif
