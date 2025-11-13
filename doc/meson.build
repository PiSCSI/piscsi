man_pages = [
  'piscsi.1',
]

if to_build.contains('scsictl')
    man_pages += 'scsictl.1'
endif

if to_build.contains('scsimon')
    man_pages += 'scsimon.1'
endif

if to_build.contains('scsidump')
    man_pages += 'scsidump.1'
endif

if to_build.contains('scsiloop')
    man_pages += 'scsiloop.1'
endif

foreach man : man_pages
  install_data(man, install_dir : get_option('prefix') / get_option('mandir') / 'man1')
  # Also generate a plain text version and install it into the doc/ directory in the source tree
  txt = man.split('.')[0] + '_man_page.txt'
  custom_target(
    txt,
    input : man,
    output : txt,
    command : [
      'sh', '-c',
      'echo "!!   ------ THIS FILE IS AUTO_GENERATED! DO NOT MANUALLY UPDATE!!!" && ' +
      'echo "!!   ------ The source file is $(basename @INPUT@)" && ' +
      'echo "" && ' +
      'man -l @INPUT@ | col -bx'
    ],
    capture: true,
    install : true,
    install_dir : meson.project_source_root() / 'doc',
  )
endforeach
