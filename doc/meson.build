man_pages = [
  'piscsi.1',
]

if target.contains('scsictl')
    man_pages += 'scsictl.1'
endif

if target.contains('scsimon')
    man_pages += 'scsimon.1'
endif

if target.contains('scsidump')
    man_pages += 'scsidump.1'
endif

if target.contains('sasidump')
    man_pages += 'sasidump.1'
endif

if target.contains('scsiloop')
    man_pages += 'scsiloop.1'
endif

foreach man : man_pages
  install_data(man, install_dir : get_option('prefix') / get_option('mandir') / 'man1')
endforeach

install_data('piscsi-network-profile.8', install_dir : get_option('prefix') / get_option('mandir') / 'man8')
