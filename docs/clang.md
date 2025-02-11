# Clang

Chromium ships a prebuilt [clang](http://clang.llvm.org) binary.
It's just upstream clang built at a known-good revision that we
bump every two weeks or so.

This is the only supported compiler for building Chromium.

[TOC]

## Using gcc on Linux

`is_clang = false` will make the build use system gcc on Linux. There are no
bots that test this and there is no guarantee it will work, but we accept
patches for this configuration.

## Mailing List

https://groups.google.com/a/chromium.org/group/clang/topics

## Using plugins

The
[chromium style plugin](https://dev.chromium.org/developers/coding-style/chromium-style-checker-errors)
is used by default when clang is used.

If you're working on the plugin, you can build it locally like so:

1.  Run `./tools/clang/scripts/build.py --without-android`
    to build the plugin.
1.  Run `ninja -C third_party/llvm-build/Release+Asserts/` to build incrementally.
1.  Build with clang like described above, but, if you use goma, disable it.

To test the FindBadConstructs plugin, run:

    (cd tools/clang/plugins/tests && \
     ./test.py ../../../../third_party/llvm-build/Release+Asserts/bin/clang \
               ../../../../third_party/llvm-build/Release+Asserts/lib/libFindBadConstructs.so)

Since the plugin is rolled with clang changes, behavior changes to the plugin
should be guarded by flags to make it easy to roll clang. A general outline:
1.  Implement new plugin behavior behind a flag.
1.  Wait for a compiler roll to bring in the flag.
1.  Start passing the new flag in `GN` and verify the new behavior.
1.  Enable the new plugin behavior unconditionally and update the plugin to
    ignore the flag.
1.  Wait for another compiler roll.
1.  Stop passing the flag from `GN`.
1.  Remove the flag completely.

## Using the clang static analyzer

See [clang_static_analyzer.md](clang_static_analyzer.md).

## Windows

clang is the default compiler on Windows. It uses MSVC's SDK, so you still need
to have Visual Studio with C++ support installed.

## Using a custom clang binary

Set `clang_base_path` in your args.gn to the llvm build directory containing
`bin/clang` (i.e. the directory you ran cmake). This must be an absolute
path. You also need to disable chromium's clang plugin.

Here's an example that also disables debug info and enables the component build
(both not strictly necessary, but they will speed up your build):

```
clang_base_path = getenv("HOME") + "/src/llvm-build"
clang_use_chrome_plugins = false
is_debug = false
symbol_level = 1
is_component_build = true
```

You can then run `head out/gn/toolchain.ninja` and check that the first to
lines set `cc` and `cxx` to your clang binary. If things look good, run `ninja
-C out/gn` to build.

Chromium tries to be buildable with its currently pinned clang, and with clang
trunk. Set `llvm_force_head_revision = true` in your args.gn if the clang you're
trying to build with is closer to clang trunk than to Chromium's pinned clang
(which `tools/clang/scripts/update.py --print-revision` prints).
