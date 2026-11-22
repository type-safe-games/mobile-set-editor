# Third-party sources

Everything the Android build compiles comes from a pinned submodule here, so a checkout with its
submodules builds with no network access (F-Droid; ADR 0011, #43). The desktop build uses the
system's Boost and Hunspell unless `-DMSE_VENDORED_DEPS=ON`; Android always uses these.

| Path | What | Pinned to | Licence |
| --- | --- | --- | --- |
| `minizip-ng` | zip packages | 4.2.2 | Zlib |
| `skia`, `skia-deps/*`, `gn` | the Skia backends | see [`tools/skia/README.md`](../tools/skia/README.md) | |
| `boost/<library>` | Boost, header-only | the commits Boost 1.92.0 pins | BSL-1.0 |
| `hunspell` | spell checking | v1.7.5 | MPL-1.1 / GPL-2.0-or-later / LGPL-2.1-or-later; used under the GPL |

zlib is the NDK's (`libz`, a stable Android platform library), as minizip-ng and the desktop use
the system's.

## Boost

Only the 26 Boost libraries the core includes are vendored, each its own submodule from
`github.com/boostorg/<library>`, at the commit the Boost superproject's `boost-1.92.0` tag pins
(1.92.0 is also the desktop's system Boost). CMake puts every `third_party/boost/*/include` on the
include path. Boost.Regex is header-only, and Boost.JSON's sources are compiled into the core by
`src/util/boost_json.cpp`, so no Boost library is built.

The list is the transitive closure of the core's Boost includes: config, assert, static_assert,
throw_exception, core, predef, preprocessor, type_traits, mp11, mpl, move, intrusive, container,
container_hash, describe, endian, system, variant2, compat, json, integer, iterator,
smart_ptr, pool, logic and regex.

To upgrade, or after including a new Boost header:

1. Take the pins from the new release's superproject:
   `git fetch --depth 1 https://github.com/boostorg/boost refs/tags/boost-1.NN.0 && git ls-tree FETCH_HEAD libs/`.
2. Move each submodule to its pin (`git -C third_party/boost/<lib> fetch --depth 1 origin <sha> && git -C
   third_party/boost/<lib> checkout <sha>`).
3. Check that the include closure is complete: preprocess a file with the core's Boost includes
   (`grep -rho '#include <boost/[^>]*>' src`) with only the vendored include directories, e.g.
   `g++ -M`, and make sure no header comes from `/usr/include/boost`. Add a submodule for each
   library that is missing.

## Hunspell

Upstream builds with autotools only, so CMake compiles libhunspell's sources itself (target
`mse_hunspell`, static, `HUNSPELL_STATIC`). Its generated headers (`hunversion.h`, `hunvisapi.h`)
are tracked upstream. On an upgrade, compare `src/hunspell/Makefile.am`'s
`libhunspell_*_la_SOURCES` with the list in `CMakeLists.txt`.
