project(
    'piscsi',
    'cpp',
    version : '24.4.1',
    default_options : ['cpp_std=c++20', 'warning_level=2']
)

common_flags = [
  '-D_FILE_OFFSET_BITS=64',
]

if get_option('buildtype') == 'debug'
  common_flags += '-DDEBUG'
else
  common_flags += '-DNDEBUG'
endif

if host_machine.system() == 'linux'
  common_flags += '-Wno-psabi'
endif

target = get_option('target')
add_project_arguments(common_flags, language : 'cpp')

# Dependencies
protobuf_dep = dependency('protobuf', required : true)
protoc = find_program('protoc', required : true)
pthread_dep = dependency('threads', required : true)
pcap_dep = dependency('pcap', required : true)
spdlog_dep = dependency('spdlog', required : true)
fmt_dep = dependency('fmt', required : true)
gtest_dep = dependency('gtest', required : false)
gmock_dep = dependency('gmock', required : false)

common_deps = [pthread_dep, spdlog_dep, fmt_dep]

subdir('cpp')
subdir('doc')
subdir('os_integration')

summary(
  {
    'Build type': get_option('buildtype'),
    'Targets': ', '.join(target),
    'Prefix': get_option('prefix'),
  },
  section : 'Configuration'
)
