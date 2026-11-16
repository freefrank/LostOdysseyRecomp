# Building an Android fork with authorized inputs

The public source tree does not contain Disc 1's `default.xex` or generated PPC sources. The official Android workflow stages the XEX privately, builds the host recompiler, generates PPC code, and only then compiles and packages the ARM64 runtime. A missing input is a blocked build, not a successful APK.

Choose one authorized input source in the fork's repository settings:

- Set the **Actions secret** `LO_BUILD_XEX_URL` to a private HTTPS download URL for your own Disc 1 `default.xex`. The existing provisioning script reads it without printing the URL or content.
- Alternatively, keep the XEX in an authorized private GitHub repository. Set the **Actions secret** `LO_BUILD_INPUT_KEY` to a read-only deploy key authorized on that repository. Set the **Actions variables** `LO_BUILD_INPUT_REPOSITORY` (`owner/name`) and `LO_BUILD_INPUT_REF` (a pinned commit) to select it.

The URL takes precedence when both secrets are configured. With neither secret the workflow stops before installing the expensive toolchain and states exactly what is missing.

When no repository variables are provided, the official private repository and pinned commit remain the defaults. Access to the public fork does not grant access to the maintainer's private inputs. Do not copy, expose or request the maintainer's credentials.

For a private input repository, store the XEX at `default.xex`. Optional `android/libdxcompiler.so` is reused only when its hash matches the official pin; otherwise Android DXC is built from source. Optional `android/debug.keystore` preserves the project's signing identity when its use is authorized. A newly generated fork signing key cannot update an installed APK signed by another key.

Keep game inputs, private URLs, keys and keystores out of public commits, build logs and artifacts. The workflow uploads the APK and test reports, not the input checkout or generated source tree. After a successful build, signature, ABI, integrity and actual-device acceptance remain separate checks.

GitHub may separately suspend Actions on a fork because of usage volume. A repository maintainer must re-enable Actions before a push or manual dispatch can run. Provisioning an XEX does not remove that platform block.
