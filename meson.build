project('frida-cycript', 'c', 'cpp',
  version: '1.0.0',
  default_options: ['cpp_std=c++11'],
)

host_os_family = host_machine.system()
host_os = host_os_family
host_arch = host_machine.cpu_family()

languages = ['c', 'cpp']
if host_os_family == 'darwin'
  languages += ['objc', 'objcpp']
  add_languages('objc', 'objcpp')
endif

cc = meson.get_compiler('c')

target_conditionals_prefix = '#include <TargetConditionals.h>'

is_macos_src = target_conditionals_prefix + '''
#if !TARGET_OS_OSX
# error Not macOS
#endif
'''
if cc.compiles(is_macos_src, name: 'compiling for macOS')
  host_os = 'macos'
endif

is_ios_src = target_conditionals_prefix + '''
#if !TARGET_OS_IOS
# error Not iOS
#endif
'''
if cc.compiles(is_ios_src, name: 'compiling for iOS')
  host_os = 'ios'
endif

if cc.has_header('android/api-level.h')
  host_os = 'android'
endif

flex = find_program('flex')
bison = find_program(files('ext/toolchain/usr/bin/bison.py'))
npm = find_program('npm')

cdata = configuration_data()

host_os_id = '1'
cdata.set_quoted('CY_SYSTEM', host_os_id)

if get_option('enable_engine')
  cdata.set('CY_EXECUTE', 1)
endif

dep_includes = []
dep_libs = []

if not cc.has_header('sqlite3.h')
  error('sqlite3 is required')
endif
dep_libs += ['-lsqlite3']

if get_option('enable_console')
  readline = subproject('readline')
  readline_dep = readline.get_variable('readline_static_dep')

  if cc.has_function('cur_term', args: ['-ltermcap'])
    termcap_libs = ['-ltermcap']
  elif cc.has_function('cur_term', args: ['-lncurses'])
    termcap_libs = ['-lncurses']
  else
    error('termcap or ncurses is required')
  endif
endif

enable_java = true
cdata.set10('CY_JAVA', enable_java)

enable_ruby = true
cdata.set10('CY_RUBY', enable_ruby)

python = get_option('with-python')
if python != 'no'
  enable_python = true

  result = run_command(python, '-c',
    'from distutils import sysconfig; import sys; sys.stdout.write(sysconfig.get_python_inc())')
  if result.returncode() != 0
    error('Unable to detect Python include directory: ' + result.stdout() + result.stderr())
  endif
  python_incdir = result.stdout()

  dep_includes += '-I' + python_incdir
else
  enable_python = false
endif
cdata.set10('CY_PYTHON', enable_python)

enable_objc = true
cdata.set10('CY_OBJECTIVEC', enable_objc)

if build_machine.system() == 'darwin'
  result = run_command('scripts/detect-xcode-clang-libdir')
  clang_libdir = result.stdout().strip()
  clang_dep = cc.find_library('clang', dirs: clang_libdir)
else
  clang_libdir = ''
  clang_dep = cc.find_library('clang')
endif

configure_file(input: 'config.h.in',
  output: 'config.h',
  configuration: cdata)

add_project_arguments('-include', 'config.h', language: languages)

if cc.get_id() == 'gcc' or cc.get_id() == 'clang'
  add_project_arguments('-fvisibility=hidden', language: languages)

  candidate_flags = [
    '-Werror',
    '-Wno-non-virtual-dtor',
    ## XXX: this one is used to support GNU Objective-C struct objc_method
    '-Wno-deprecated-declarations',
    '-Wno-dangling-else',
    '-Wno-empty-body',
    '-Wno-overloaded-virtual',
    '-Wno-parentheses',
    '-Wno-tautological-undefined-compare',
    '-Wno-unneeded-internal-declaration',
    '-Wno-inconsistent-missing-override',
  ]
else
  candidate_flags = []
endif

extra_args = []
foreach flag : candidate_flags
  if cc.has_argument(flag)
    extra_args += [flag]
  endif
endforeach
add_project_arguments(extra_args, language: languages)

subdir('src')
