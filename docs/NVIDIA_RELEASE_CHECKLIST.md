# NVIDIA release checklist

The repository can enforce packaging and notice requirements, but it cannot
obtain permissions from NVIDIA or patent pools automatically. Complete and
retain evidence for the applicable items before a public or commercial release.

- [ ] Review the exact `NVIDIA_RTX_Video_SDK_License.pdf` shipped with the SDK version used to build the release.
- [ ] Confirm only permitted object-code/runtime materials are included; never publish the SDK as a standalone package.
- [ ] Include the NVIDIA license PDF and the package-level attribution in every installer and portable archive.
- [ ] Determine whether NVIDIA requires advance product notification for the SDK features used, and submit it when applicable.
- [ ] Obtain any written NVIDIA approval required for product-name, logo, or other trademark use before release.
- [ ] Preserve the approval/notification correspondence with the release records.
- [ ] Evaluate H.264/AVC, H.265/HEVC, AV1, and other codec patent licensing for the target territories and distribution model.
- [ ] Re-run `verify-release-compliance.ps1` against the final publish directory and release artifacts.

Official starting points:

- NVIDIA RTX Video SDK: <https://developer.nvidia.com/rtx-video-sdk>
- NVIDIA developer legal information: <https://developer.nvidia.com/legal-info>

This checklist records release gates; it is not legal advice and does not state
that NVIDIA or any patent licensor has approved a release.
