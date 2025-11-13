project(
    'piscsi',
    'cpp',
    version : '24.4.1',
    default_options : ['cpp_std=c++20', 'warning_level=2']
)

common_flags = [
  '-D_FILE_OFFSET_BITS=64',
  '-DFMT_HEADER_ONLY',
  '-DSPDLOG_FMT_EXTERNAL',
]

if get_option('buildtype') == 'debug'
  common_flags += '-DDEBUG'
else
  common_flags += '-DNDEBUG'
endif

if host_machine.system() == 'linux'
  common_flags += '-Wno-psabi'
endif

to_build = get_option('to_build')
connect_type = get_option('connect_type')
common_flags += '-DCONNECT_TYPE_' + connect_type
add_project_arguments(common_flags, language : 'cpp')

# Dependencies
protobuf_dep = dependency('protobuf', required : true)
protoc = find_program('protoc', required : true)
pthread_dep = dependency('threads', required : true)
pcap_dep = dependency('pcap', required : true)
gtest_dep = dependency('gtest', required : false)
gmock_dep = dependency('gmock', required : false)

subdir('cpp')
subdir('doc')
subdir('os_integration')
