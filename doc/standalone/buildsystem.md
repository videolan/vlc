# Buildsystem documentation {#buildsystem}

## Autotools

VLC uses the Autotools build system tools, that is composed of various tools:

- m4  
  Used for macro expansions in the configure.ac file.
- Autoconf  
  Generates the `configure` shell script from the `configure.ac` file
- Automake  
  Generates the Makefiles from the `Makefile.am` files
- libtool  
  Wrapper tool to abstract linker differences across different platforms


## Meson

In addition to the "traditional" autotools build system, recently there has
been an ongoing effort to migrate the VLC build system to meson. Currently
the meson buildsystem is in an **experimental state** and should not be used for
release builds of VLC!

However we encourage developers to try out the meson buildsystem and report
any issues with it or help add missing module builds to meson.

### Organization

All meson build definitions can be found in `meson.build` files. All
options are defined in the top-level directory `meson_options.txt` file,
which has the same syntax as the meson build files but only accepts the
`option()` function.


### Code style and conventions

- Indentation must be used with 4 spaces, no tabs should be used.
- Keyword arguments (named arguments) should have the `:` right next
  to them followed by a space:
  ```meson
  # Right
  add_project_arguments('-D_EXAMPLE', language: ['c', 'cpp', 'objc'])

  # Wrong
  add_project_arguments('-D_EXAMPLE', language : ['c', 'cpp', 'objc'])
  ```
- For dictionaries, key and value are separated by `:` and spaces should be
  used before and after:
  ```meson
  # Right
  { 'key' : 'value' }

  # Wrong
  { 'key': 'value' }
  ```
- Strings in meson must always use 'single' quotes, never use "double" quotes.
- Do not use arrays where not necessary, especially for the `files()` function:
  ```meson
  # Right
  files('hello.c', 'world.c')

  # Wrong
  files(['hello.c', 'world.c'])
  ```
- Dependencies found with the `dependency()` function **must** use a variable named after the
  library followed by the `_dep` suffix, those found with `find_library()` must use the `_lib`
  suffix:
  ```meson
  # Right
  foundation_dep = dependency('Foundation')
  iconv_lib = cc.find_library('iconv')

  # Wrong
  foundation_lib = dependency('Foundation')
  iconv_dep = cc.find_library('iconv')
  ```

### Adding a module

VLC modules are meson dicts added to a special array named `vlc_modules`. To add a new module, simply
append to that variable:

```meson
vlc_modules += {
    'name' : 'file_logger',
    'sources' : files('file.c')
}
```

@warning 	Make sure to not accidentally overwrite the `vlc_modules` variable by using
		 	and `=` instead of `+=` when appending the dictionary.

Currently the modules dict accepts the following keys:

@param 	name
		The name of the VLC plugin (used for the `MODULE_STRING` define). **Required**

@param 	sources
		The source files for the module, use [`files()`][mref_files] to specify them. **Required**

@param  dependencies
		The dependencies needed by the module. Only list external dependencies
		here, not libraries that are built as part of the VLC build, for these
		use `link_with` instead.

@param 	include_directories
		Additional include directories that should be used when compiling the module.
		These should be specified using the [`include_directories()`][mref_include_directories]
		function.

@param  c_args
		Additional flags to pass to the C compiler.

@param 	cpp_args
		Additional flags to pass to the C++ compiler.

@param 	objc_args
		Additional flags to pass to the Objective-C compiler.

@param  link_args
		Additional flags to pass to the dynamic linker. Do _not_ use this to specify
		additional libraries to link with, use the `dependencies` instead.

@param 	link_language
		Force the linker to be for the specified language. This is not needed in most
		cases but can be useful for example for a C plugin that depends on a C++ library
		therefore needing the C++ standard library linked.

### Checking the host system
It is frequently necessary to check the host we are building for. To do that, use the
`host_system` variable. For possible values, refer to the [reference table][mref_hostsys] in
the documentation.

For now, `host_system` is equivalent to `host_machine.system()`, however this might not
always be the case, hence the special variable for that.

### Finding dependencies
As most VLC modules likely require an external dependency too, let's have a look how to find a
dependency and add an option for it.

To find dependencies, Meson has essentially two options:

- [`compiler.find_library()`][mref_compiler_find_library]
- [`dependency()`][mref_dependency]

Generally the latter should be preferred in most cases. It primarily uses `pkg-config` to find
the dependency, however even for very common dependencies that do not provide a `.pc` file, it
can have handling to find it by other means. For a full list of the special cases, consult the
[documentation][mdoc_dpendency_custom]. It is roughly equivalent to `PKG_CHECK_MODULES` in autoconf.

However there might be a case where you need to find a library that has no `.pc` file, no custom
`*-config` tool or handling by meson. In these cases you can use the compiler object's
[`find_library`][mref_compiler_find_library] function. It basically does a linker check for the
specified library name and returns a dependency object for it. It is roughtly equivalent to
`AC_CHECK_LIB` in autoconf.

#### Options

To add an option for the dependency, add the respective option entry in `meson_options.txt`:

```meson
option('dav1d',
    type : 'feature',
    value : 'auto',
    description : 'libdav1d AV1 decoder support')
```

This adds a new [feature option][mdoc_feature] with the name `dav1d`. A feature option is a special tri-state
option that can be `enabled`, `disabled` or `auto`.

Now we can just look up the option and use that for the `required` argument of the `dependency()` function:

```meson
# dav1d AV1 decoder
dav1d_dep = dependency('dav1d', version: '>= 0.5.0', required: get_option('dav1d'))
if dav1d_dep.found()
    vlc_modules += {
        'name' : 'dav1d',
        'sources' : files('dav1d.c'),
        'dependencies' : [dav1d_dep]
    }
endif
```

As the option defaults to `auto`, meson will look it up, and if its is found, add the module.
If the option is manually set to `disabled`, meson will never look it up and just always return a not-found
dependency object. If it's set to `enabled`, meson will error out if it's not found.

The feature option object provides a few more [useful functions][mref_feature], for more complex
cases of conditional dependencies. For example suppose we want an option to be disabled in some
cases when it is set to `auto`:

```meson
if (get_option('x11')
    .disable_auto_if(host_system in ['darwin', 'windows'])
    .allowed())
    # Do something here if the X11 option is enabled
endif
```

This will disable the `x11` option if it is set to auto, when on `darwin` or `windows`.

\note
Options, like most objects in meson, are immutable. So if you were to instead write
```meson
x11_opt = get_option('x11')
x11_opt.disable_auto_if(host_system in ['darwin', 'windows']) # Wrong, don't do this!
if (x11_opt.allowed())
	# This is not disabled on Darwin or Windows!
endif
```
it would not do what you might expect, as `disable_auto_if` returns a new option and does not mutate the
existing one. The returned option object is never assigned to any variable, so it is lost.


[mref_files]: https://mesonbuild.com/Reference-manual_functions.html#files
[mref_include_directories]: https://mesonbuild.com/Reference-manual_functions.html#include_directories
[mref_compiler_find_library]: https://mesonbuild.com/Reference-manual_returned_compiler.html#compilerfind_library
[mref_dependency]: https://mesonbuild.com/Reference-manual_functions.html#dependency
[mdoc_dpendency_custom]: https://mesonbuild.com/Dependencies.html#dependencies-with-custom-lookup-functionality
[mdoc_feature]: https://mesonbuild.com/Build-options.html#features
[mref_feature]: https://mesonbuild.com/Reference-manual_returned_feature.html#feature-option-object-feature
[mref_hostsys]: https://mesonbuild.com/Reference-tables.html#operating-system-names
