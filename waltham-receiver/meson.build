project('waltham-receiver',
  'c',
  version: '0.0.1',
  default_options: [
    'warning_level=3',
    'c_std=gnu99',
  ],
  meson_version: '>= 0.50',
  license: 'MIT/Expat',
)

pkgconfig = import('pkgconfig')
cc = meson.get_compiler('c')

add_project_arguments(
  cc.get_supported_arguments([
    '-Wno-unused-parameter',
    '-Wno-pedantic',
    '-Wextra',
    '-Werror'
  ]),
  language: 'c'
)

add_project_arguments([
    '-DPACKAGE_STRING="waltham-receiver @0@"'.format(meson.project_version()),
    '-D_GNU_SOURCE',
    '-D_ALL_SOURCE',
  ],
  language: 'c'
)

optional_libc_funcs = [ 'memfd_create', 'strchrnul' ]
foreach func: optional_libc_funcs
    if cc.has_function(func)
        add_project_arguments('-DHAVE_@0@=1'.format(func.to_upper()), language: 'c')
    endif
endforeach

env_modmap = ''

libwayland_dep = dependency('wayland-client')
libwayland_cursor_dep = dependency('wayland-cursor')
libwaltham_dep = dependency('waltham')

dep_scanner = dependency('wayland-scanner', native: true)
prog_scanner = find_program(dep_scanner.get_pkgconfig_variable('wayland_scanner'))
dep_wp = dependency('wayland-protocols', version: '>= 1.18')
dir_wp_base = dep_wp.get_pkgconfig_variable('pkgdatadir')
xdg_shell_xml = join_paths(dir_wp_base, 'stable', 'xdg-shell', 'xdg-shell.xml')

protocols = [
  { 'name': 'xdg-shell', 'source': 'wp-stable' },
]

foreach proto: protocols
  proto_name = proto['name']
  if proto['source'] == 'internal'
    base_file = proto_name
    xml_path = join_paths('protocol', '@0@.xml'.format(base_file))
  elif proto['source'] == 'wp-stable'
    base_file = proto_name
    xml_path = join_paths(dir_wp_base, 'stable', proto_name, '@0@.xml'.format(base_file))
  else
    base_file = '@0@-unstable-@1@'.format(proto_name, proto['version'])
    xml_path = join_paths(dir_wp_base, 'unstable', proto_name, '@0@.xml'.format(base_file))
  endif

  foreach output_type: [ 'client-header', 'server-header', 'private-code' ]
    if output_type == 'client-header'
      output_file = '@0@-client-protocol.h'.format(base_file)
    elif output_type == 'server-header'
      output_file = '@0@-server-protocol.h'.format(base_file)
    else
      output_file = '@0@-protocol.c'.format(base_file)
    if dep_scanner.version().version_compare('< 1.14.91')
      output_type = 'code'
    endif
  endif

    var_name = output_file.underscorify()
      target = custom_target(
	  '@0@ @1@'.format(base_file, output_type),
	  command: [ prog_scanner, output_type, '@INPUT@', '@OUTPUT@' ],
	  input: xml_path,
	  output: output_file,
	  )

    set_variable(var_name, target)
  endforeach
endforeach


dep_gstreamer = dependency('gstreamer-1.0')
dep_gstreamer_plugins_base = dependency('gstreamer-plugins-base-1.0')
dep_gstreamer_plugins_bad = dependency('gstreamer-plugins-bad-1.0')
dep_gstreamer_app = dependency('gstreamer-app-1.0')
dep_gstreamer_video = dependency('gstreamer-video-1.0')
dep_gstreamer_alloc = dependency('gstreamer-allocators-1.0')
dep_egl = dependency('egl')
dep_gles = dependency('glesv2')
dep_wayland_egl = dependency('wayland-egl')

prefix_path = get_option('prefix')
binplugin_dir = join_paths(prefix_path, get_option('bindir'))
common_inc = include_directories('include')

deps_waltham_receiver = [
    libwayland_dep, libwayland_cursor_dep,
    libwaltham_dep,
    dep_gstreamer, dep_gstreamer_app, dep_gstreamer_plugins_base,
    dep_gstreamer_plugins_bad,
    dep_gstreamer_video, dep_gstreamer_alloc,
    dep_egl, dep_gles, dep_wayland_egl,
    # gstwayland-1.0 is special here because gstreamer-plugins-bad-1.0 does not
    # provide it
    cc.find_library('pthread'), cc.find_library('gstwayland-1.0')
]

buf_type = get_option('buffer-type')
buf_type_src = []

if buf_type == 'egl'
    buf_type_src += 'src/wth-receiver-gst-egl.c'
elif buf_type == 'shm'
    buf_type_src += 'src/wth-receiver-gst-shm.c'
endif

srcs_wth_receiver = [
    'src/bitmap.c',
    'src/os-compatibility.c',
    'src/wth-receiver-comm.c',
    'src/wth-receiver-buffer.c',
    'src/wth-receiver-surface.c',
    'src/wth-receiver-seat.c',
    'src/wth-receiver-main.c',
    buf_type_src,
    xdg_shell_client_protocol_h,
    xdg_shell_protocol_c,
]

exe_wth_receiver = executable(
    'waltham-receiver',
    srcs_wth_receiver,
    include_directories: common_inc,
    dependencies: deps_waltham_receiver,
    install_rpath: binplugin_dir,
    install: true
)
