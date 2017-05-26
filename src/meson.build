cycript_sources = [
  'ConvertUTF.c',
  'Decode.cpp',
  'Driver.cpp',
  'Error.cpp',
  'Highlight.cpp',
  'Library.cpp',
  'Output.cpp',
  'Replace.cpp',
  'Syntax.cpp',
  'sig/parse.cpp',
  'sig/copy.cpp',
  'ObjectiveC/Output.cpp',
  'ObjectiveC/Replace.cpp',
]

cycript_features = ['C', 'Java', 'Ruby', 'Python', 'ObjectiveC']
cycript_link_args = []

filter = files('filter.py')

cycript_scanner_grammar = custom_target('cycript-scanner-grammar',
  input: 'Scanner.lpp.in',
  output: 'Scanner.lpp',
  command: [
    filter,
    '@INPUT@',
    '@OUTPUT@',
  ] + cycript_features,
)
cycript_scanner = custom_target('cycript-scanner',
  input: cycript_scanner_grammar,
  output: ['Scanner.cpp'],
  command: [
    flex,
    '--full',
    '--ecs',
    '-o', '@OUTPUT@',
    '@INPUT@',
  ],
)
cycript_sources += cycript_scanner

cycript_parser_grammar = custom_target('cycript-parser-grammar',
  input: 'Parser.ypp.in',
  output: 'Parser.ypp',
  command: [
    filter,
    '@INPUT@',
    '@OUTPUT@',
  ] + cycript_features,
)
cycript_parser = custom_target('cycript-parser',
  input: cycript_parser_grammar,
  output: ['Parser.hpp', 'Parser.cpp'],
  command: [
    files('generate-parser.py'),
    bison,
    '@INPUT@',
    '@OUTPUT@',
  ],
)
cycript_sources += cycript_parser

if get_option('enable_console')
  cycript_sources += ['Complete.cpp']
endif

if get_option('enable_engine')
  frida_core = custom_target('frida-core',
    output: ['frida-core.h'],
    command: [
      files('fetch-frida-devkit.py'),
      '11.0.6',
      host_os,
      host_arch,
      '@OUTDIR@',
    ],
  )
  cycript_sources += [
    'Execute.cpp',
    frida_core,
  ]
  cycript_link_args += [
    '-L' + meson.current_build_dir(),
    '-lfrida-core',
  ]
  if host_os == 'macos'
    cycript_link_args += [
      '-lbsm',
      '-lresolv',
      '-Wl,-framework,Foundation,-framework,AppKit',
      '-Wl,-no_compact_unwind',
    ]
  endif

  analyze_sources = [
    'Analyze.cpp',
    'Error.cpp',
    'Output.cpp',
    'Replace.cpp',
    'Syntax.cpp',
  ]
  analyze = executable('Analyze', analyze_sources,
    include_directories: include_directories('extra'),
    dependencies: clang_dep,
    build_rpath: clang_libdir,
    install_rpath: clang_libdir,
  )

  cycript_bridge_definitions = custom_target('cycript-bridge-definitions',
    input: [
      analyze,
      'Analysis.cpp',
      'Bridge.old',
    ],
    output: 'Bridge.def',
    command: [
      files('generate-bridge-definitions.py'),
      host_os,
      host_arch,
      '@INPUT@',
      '@OUTPUT@',
      '-DCY_JAVA=' + enable_java.to_string('1', '0'),
      '-DCY_PYTHON=' + enable_python.to_string('1', '0'),
      '-DCY_OBJECTIVEC=' + enable_objc.to_string('1', '0'),
    ] + dep_includes,
  )

  cycript_database = custom_target('cycript-database',
    input: cycript_bridge_definitions,
    output: 'libcycript.db',
    command: [
      files('generate-database.py'),
      host_os_id,
      '@OUTPUT@',
      '@INPUT@',
      join_paths(meson.source_root(), 'ext', 'node'),
    ],
    install: true,
    install_dir: get_option('datadir'),
  )

  node_modules = custom_target('node-modules',
    input: 'package.json',
    output: 'node_modules',
    command: [
      files('fetch-node-modules.py'),
      npm,
      '@INPUT@',
      '@OUTDIR@',
    ],
  )

  custom_target('cycript-runtime',
    input: [
      'Agent/index.js',
      node_modules,
    ],
    output: 'libcycript.js',
    command: [
      files('generate-runtime.py'),
      '@INPUT@',
      '@OUTPUT@',
    ],
    install: true,
    install_dir: get_option('datadir'),
  )
endif

extra_link_args = []
if host_os_family == 'darwin'
  extra_link_args += ['-Wl,-exported_symbols_list,' + join_paths(meson.current_source_dir(), 'libcycript-core.symbols')]
  if get_option('enable_engine')
    extra_link_args += ['-Wl,-exported_symbols_list,' + join_paths(meson.current_source_dir(), 'libcycript-engine.symbols')]
  endif
endif

cycript = library('cycript', cycript_sources,
  cpp_args: ['-DYYDEBUG=1'],
  link_args: cycript_link_args + dep_libs + extra_link_args,
  install: true,
)

if get_option('enable_console')
  executable('cycript', 'Console.cpp',
    link_args: termcap_libs,
    link_with: cycript,
    dependencies: [readline_dep],
  )
endif
