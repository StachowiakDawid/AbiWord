# macOS Apple Silicon build

This Cocoa build was tested on macOS 27.2 (arm64) with the macOS 27.2
SDK. It produces AbiWord 3.1.90 and requires native arm64 dependencies.

## Minimum macOS version

The app targets macOS 15 Sequoia, the oldest release Apple still ships
security updates for. `configure` passes `-mmacosx-version-min=15.0` to the
compiler and linker and writes `LSMinimumSystemVersion` into `Info.plist`.
Set `MACOSX_DEPLOYMENT_TARGET` before running `configure` to choose another
version.

Homebrew bottles are built for the macOS version they are installed on, so
a build linked against them only runs on that version or newer, whatever the
deployment target. Building the dependencies with the same deployment target
is part of release packaging.

## Dependencies

Install Xcode or its command-line tools and these Homebrew packages:

```sh
brew install autoconf autoconf-archive automake libtool pkgconf \
  fribidi glib libgsf wv libxslt enchant cairo pango goffice \
  libpng jpeg-turbo
```

The tested Homebrew prefix is `/opt/homebrew`. No Rosetta build is needed.

## Build

From the repository root:

```sh
NOCONFIGURE=1 ./autogen.sh
mkdir -p build
cd build
../configure --prefix="$PWD/install" \
  --disable-introspection --enable-clipart \
  CC=clang CXX=clang++ \
  CFLAGS='-O2 -arch arm64' CXXFLAGS='-O2 -arch arm64' \
  CPPFLAGS='-I/opt/homebrew/include' LDFLAGS='-L/opt/homebrew/lib'
make -j8
make install DESTDIR="$PWD/install"
```

Use `DESTDIR` when installing: the Cocoa build uses the legacy absolute
bundle path `/AbiWord.app`, independently of `--prefix`. Staging places
the application in `build/install/AbiWord.app` without needing
root access or writing to the macOS system volume.

## Smoke Tests

From the build directory:

```sh
file install/AbiWord.app/Contents/MacOS/AbiWord
install/AbiWord.app/Contents/MacOS/AbiWord --version
install/AbiWord.app/Contents/MacOS/AbiWord --to=txt \
  --to-name="$PWD/smoke-readme.txt" ../user/wp/readme.abw
test -s smoke-readme.txt
open install/AbiWord.app
```

Run the unit tests with `make check`.

The tested executable and bundled libabiword are both Mach-O arm64 binaries.
The version command prints `3.1.90`, and the readme conversion produces
nonempty text. GUI startup responds to an activation Apple event, and a
post-startup process sample shows the main thread waiting in the AppKit event
loop rather than spinning. A key event delivered through the AppKit input
context inserts text, and a rendered-view capture confirms the text and caret
are visible at the top of the page. Missing-graphics-context warnings are no
longer observed. Broader editing workflows and IME behavior remain untested.

## Limitations

- The default plugins (`opendocument`, `openxml`: .odt and .docx import and
  export) are installed in `Contents/PlugIns`. GObject introspection is
  disabled.
- Spell checking uses Enchant, whose providers (AppleSpell, aspell) and
  dictionaries come from Homebrew in this build. The UI, document language
  and dictionary follow the macOS preferred language unless `LANG`/`LC_ALL`
  is set to a real locale.
- The app links to libraries under `/opt/homebrew`; it is a local development
  build, not a self-contained distributable application.
- The Cocoa code is compiled with ARC (`-fobjc-arc`) and uses no deprecated
  AppKit APIs. Some dialog nibs still use the old-style `NSBox` type, which
  `ibtool` reports as deprecated. After editing a `designable.nib`, recompile
  its `keyedobjects.nib` with
  `xcrun ibtool --minimum-deployment-target 15.0 --compile out.nib in.xib`.
- The application is not packaged for distribution, signed with a Developer
  ID, or notarized. Bundling dependencies and release packaging are separate
  steps.