# pthread-embedded: licensing and static linking

The supplied snapshot is `11d2e5722d98c86f33c908fc47b2cf6e55205db5` of the Vita pthread-embedded port. Its upstream README describes the library as LGPLv2-or-later. Its original `COPYING` instead specifies GNU LGPL version 2.1. The original files are retained unchanged. This distribution follows the LGPL 2.1 conditions without depending on the broader README statement. Vita-specific files retain their MIT notices where applicable; individual source headers remain relevant.

The full LGPL 2.1 text is supplied in `LICENSES/pthread-LGPL-2.1.txt`; the original COPYING statement is also preserved with the sources. Do not describe a missing upstream `COPYING.LIB` file as if it were present at that source path.

## Chosen source and relinking route

The VPK contains statically linked pthread implementation code. The source-delivery approach is **LGPL 2.1 section 6, option 6(a)**: provide the complete library source, including relevant changes, plus the application source and compilation/installation material needed to rebuild and relink with a modified library. Section 6 allows terms of choice for the linked work subject to its conditions, including modification and reverse-engineering permissions for debugging changes to the library. The application uses GPL-3.0-or-later; no contrary restriction is added here.

This is a linking/source-delivery approach. We do **not** claim to have converted pthread source notices to GPL under LGPL section 3. Original library notices and LGPL text remain supplied. The FSF's [license compatibility table](https://www.gnu.org/licenses/gpl-faq.html#AllCompatibility) lists linking an LGPL 2.1 library with a GPLv3 program as permissible; that does not remove the source and relinking requirements. Read the [actual LGPL 2.1 text](https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html), particularly sections 5 and 6.

The delivery consists of:

- `third_party/pthread-embedded/`: the pthread source snapshot and Vita implementation.
- `src/vcmi/` and the other `third_party/` components: application and dependency sources.
- `dependency-recipes/`, `scripts/build.ps1`, `BUILDING.md` and `SOURCE_ACCESS.md`: supplied build/install material and externally obtained toolchain prerequisites.
- Original notices, the LGPL text, MIT Vita notice and combined-application GPL text in the sources and `LICENSES/`.

Source-form application code is used for the relinking route; this is not a claim that proprietary application object files are supplied or that a shared-library replacement mechanism exists. No signature-enforcement mechanism is added by this project to prevent installing a rebuilt VPK. This does not grant rights to third-party console firmware or game data.

## Verification evidence

The RC includes a clean exported-source build and a separate modified-pthread rebuild/relink procedure. See docs/compliance/BUILD_VERIFICATION.json for actual outcomes and BUILDING.md for reproduction. The probe is a link-level check, not execution on a console; the original library and release executable are restored before packaging. Do not infer a completed device test from this build check.
