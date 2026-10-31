# SPDX-License-Identifier: GPL-2.0-or-later
# The upstream FindSDL2 module imports the archive without its Vita stubs.
set_property(TARGET SDL2::SDL2 APPEND PROPERTY INTERFACE_LINK_LIBRARIES
	SceGxm_stub SceDisplay_stub SceCtrl_stub SceAppMgr_stub SceAppUtil_stub
	SceAudio_stub SceAudioIn_stub SceSysmodule_stub SceIofilemgr_stub
	SceCommonDialog_stub SceTouch_stub SceHid_stub SceMotion_stub ScePower_stub
	SceProcessmgr_stub SceSysmem_stub SceNet_stub SceNetCtl_stub m)
