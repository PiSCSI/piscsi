piscsi_interface_src = custom_target(
    'piscsi_interface_proto',
    input : meson.project_source_root() / 'cpp' / 'piscsi_interface.proto',
    output : ['piscsi_interface.pb.cc', 'piscsi_interface.pb.h'],
    command : [
        protoc, 
        '--cpp_out=@BUILD_ROOT@/cpp/generated', 
        '--proto_path=' + meson.project_source_root() / 'cpp',
        '@INPUT@'
    ],
    build_by_default : true,
    install : false
)
