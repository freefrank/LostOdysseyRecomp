# Separately installed Android fork builds

The Android runtime accepts optional Gradle properties for a fork's installed
package identity and launcher label. Defaults remain
`io.github.freefrank.lostodyssey` and `Lost Odyssey Recomp`.

After generating PPC code and building/staging the native runtime with the
normal Android build tools, package a separately installed fork with:

```sh
export LO_ANDROID_DEBUG_KEYSTORE=/private/path/fork-debug.keystore
./gradlew :runtime:assembleRelease \
  -PloVersionName=v1.1.0-Steprant \
  -PloApplicationId=io.github.freefrank.lostodyssey.steprant \
  '-PloAppLabel=Lost Odyssey Recomp Steprant'
```

Run Gradle from `packaging/android`. Keep the signing key private and reuse it
for future updates of that same fork package.

The Java namespace remains `io.github.freefrank.lostodyssey`, preserving the
existing Java/JNI symbols. Only the installed application ID, manifest
authorities and launcher label change. An independent package can coexist
with the official app without its private signing key.

App-private settings and saves belong to their package. To transfer saves,
export them from the existing app and import them into the fork. For game data,
choose an accessible shared extracted-game folder or import the same discs
into the fork. Android can restrict access to another app's `Android/data`
directory. Installing the separate package does not migrate private data.

The runtime APK also includes the project's license and the existing SDL,
Plume and DXC dependency notices, alongside the existing audio notices.
