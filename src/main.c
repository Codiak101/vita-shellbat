/*
 *  ShellBat plugin
 *  Copyright (c) 2017 David Rosca
 *  Copyright (c) 2020 Princess of Sleeping
 *  Copyright (c) 2021 Codiak
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 */

#include <psp2/kernel/modulemgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/ctrl.h>
#include <psp2/paf.h>
#include <psp2/power.h>
#include <psp2/rtc.h>
#include <psp2/sysmodule.h>
#include <taihen.h>
#include "taihen_extra.h"

#define TAI_NEXT(this_func, hook, ...) ({ \
  (((struct _tai_hook_user *)hook)->next) != 0 ? \
    ((__typeof__(&this_func))((struct _tai_hook_user *)((struct _tai_hook_user *)hook)->next)->func)(__VA_ARGS__) \
  : \
    ((__typeof__(&this_func))((struct _tai_hook_user *)hook)->old)(__VA_ARGS__) \
  ; \
})

#define HookImport(module_name, library_nid, func_nid, func_name) \
	taiHookFunctionImport(&func_name ## _ref, module_name, library_nid, func_nid, func_name ## _patch)

SceWChar16 *sce_paf_private_wcschr(const SceWChar16 *s, SceWChar16 c);
SceWChar16 *sce_paf_private_wcsncpy(SceWChar16 *dst, const SceWChar16 *src, SceSize n);
int sce_paf_private_swprintf(SceWChar16 *dst, SceSize len, SceWChar16 *fmt, ...);
int scePafWidgetSetFontSize(void *widget, float size, int unk0, int pos, int len);
void pulse_state(int pulse_freq, int pulse_reset);

SceWChar16 cached_strings[0x64];
SceKernelSysClock prevIdleClock[4];

int pos_percent_battery;
int prev_percent_battery;
int pos_controller_battery;
int pulse_controller_battery;
SceUInt8 prev_battery_level, ctrl_battery_level;
SceRtcTick pulse_tick0, pulse_tick1;

tai_hook_ref_t scePafWidgetSetFontSize_ref;
int scePafWidgetSetFontSize_patch(void *pWidget, float size, int unk0, int pos, SceSize len){

	uint32_t id = *(uint32_t *)(pWidget + 0x14C);

	if(id == 0x89FFAD08 && size == 16.0f)
		return 0;

	int res = TAI_NEXT(scePafWidgetSetFontSize_patch, scePafWidgetSetFontSize_ref, pWidget, size, unk0, pos, len);

	if(id == 0x89FFAD08 && size == 22.0f){

		SceWChar16 *pMStr = sce_paf_private_wcschr(cached_strings, *L"M");
		if(pMStr != NULL && pMStr != cached_strings && ((pMStr[-1] == *L"A" || pMStr[-1] == *L"P"))){
			scePafWidgetSetFontSize(pWidget, 16.0f, 1, (int)(pMStr - cached_strings) - 2, 3);
		}

		if(pos_controller_battery != 0){
			scePafWidgetSetFontSize(pWidget, 16.0f, 1, pos_controller_battery, 0);
		}

		if(pos_percent_battery != 0){
			scePafWidgetSetFontSize(pWidget, 16.0f, 1, pos_percent_battery, 0);
		}
	}

	return res;
}

tai_hook_ref_t scePafGetTimeDataStrings_ref;
int scePafGetTimeDataStrings_patch(ScePafDateTime *data, SceWChar16 *dst, SceSize len, void *a4, int a5){

	if(len != 0x64)
		return TAI_CONTINUE(int, scePafGetTimeDataStrings_ref, data, dst, len, a4, a5);

	int res = 0, out_len = 0;

	/* is_disp_clock */
	res = TAI_CONTINUE(int, scePafGetTimeDataStrings_ref, data, &dst[out_len], len, a4, a5);
	out_len += res; len -= res;

	/* is_disp_controller */
	SceCtrlPortInfo portinfo;
	sceCtrlGetControllerPortInfo(&portinfo);

	if (portinfo.port[1] == 8) {
		SceWChar16 battery_indicator[] = L" P1[   ]";
		sceCtrlGetBatteryInfo(1, &ctrl_battery_level);
		pulse_controller_battery = prev_battery_level == ctrl_battery_level ? pulse_controller_battery : 0;
		prev_battery_level = ctrl_battery_level;

		switch (ctrl_battery_level) {
		case 0xEE: // battery is charging [| | |]
			pulse_state(32, 3);
			battery_indicator[0x4] = '|';
			battery_indicator[0x5] = pulse_controller_battery > 1 ? '|' : ' ';
			battery_indicator[0x6] = pulse_controller_battery > 2 ? '|' : ' ';
			break;
		case 0xEF: // battery level is fully charged [|||]
			battery_indicator[0x4] = '|';
			battery_indicator[0x5] = '|';
			battery_indicator[0x6] = '|';
			break;
		case 0x5: // battery level is 5 [|| |]
			pulse_state(16, 2);
			battery_indicator[0x4] = '|';
			battery_indicator[0x5] = '|';
			battery_indicator[0x6] = pulse_controller_battery == 1 ? '|' : ' ';
			break;
		case 0x4: // battery level is 4 [|| ]
			battery_indicator[0x4] = '|';
			battery_indicator[0x5] = '|';
			battery_indicator[0x6] = ' ';
			break;
		case 0x3: // battery level is 3 [| | ]
			pulse_state(16, 2);
			battery_indicator[0x4] = '|';
			battery_indicator[0x5] = pulse_controller_battery == 1 ? '|' : ' ';
			battery_indicator[0x6] = ' ';
			break;
		case 0x2: // battery level is 2 [|  ]
		case 0x1: // battery level is 1 [   ]
			pulse_state(32, 2);
			battery_indicator[0x1] = pulse_controller_battery == 1 ? 'P' : ' ';
			battery_indicator[0x2] = pulse_controller_battery == 1 ? '1' : ' ';
			battery_indicator[0x3] = pulse_controller_battery == 1 ? '[' : ' ';
			battery_indicator[0x4] = ctrl_battery_level == 0x2 ? '|' : ' ';
			battery_indicator[0x4] = pulse_controller_battery == 1 ? battery_indicator[0x4] : ' ';
			battery_indicator[0x5] = ' ';
			battery_indicator[0x6] = ' ';
			battery_indicator[0x7] = pulse_controller_battery == 1 ? ']' : ' ';
			break;
		case 0x0: // battery level is 0
		default:
			break;
		}

		res = sce_paf_private_swprintf(&dst[out_len], len, battery_indicator);
		out_len += res; len -= res;
		pos_controller_battery = out_len - 1;
	}

	/* is_disp_battery */
	int percent = scePowerGetBatteryLifePercent();
	if(percent < 0)
		percent = prev_percent_battery; // for resume

	prev_percent_battery = percent;

	res = sce_paf_private_swprintf(&dst[out_len], len, L" %d%%", percent);
	out_len += res; len -= res;
	pos_percent_battery = out_len - 1;

	sce_paf_private_wcsncpy(cached_strings, dst, sizeof(cached_strings) / 2);

	return out_len;
}

tai_hook_ref_t sceSysmoduleLoadModuleInternalWithArg_ref;
int sceSysmoduleLoadModuleInternalWithArg_patch(SceSysmoduleInternalModuleId id, SceSize args, void *argp, void *unk){

	int res = TAI_CONTINUE(int, sceSysmoduleLoadModuleInternalWithArg_ref, id, args, argp, unk);

	if(res >= 0 && id == SCE_SYSMODULE_INTERNAL_PAF){
		HookImport("SceShell", 0x3D643CE8, 0xF8D1975F, scePafGetTimeDataStrings);
		HookImport("SceShell", 0x073F8C68, 0x39B15B98, scePafWidgetSetFontSize);
	}

	return res;
}

void pulse_state(int pulse_freq, int pulse_reset)
{
	sceRtcGetCurrentTick(&pulse_tick1);
	pulse_tick0.tick = pulse_tick0.tick > pulse_tick1.tick ? 0 : pulse_tick0.tick;
	pulse_controller_battery = pulse_controller_battery >= pulse_reset ? 0 : pulse_controller_battery;

	if ((pulse_tick1.tick - pulse_tick0.tick) >= (1000000 / pulse_freq))
	{
		pulse_tick0.tick = pulse_tick1.tick;
		pulse_controller_battery += 1;
	}
}

void _start() __attribute__ ((weak, alias("module_start")));
int module_start(SceSize args, void *argp){

	pulse_tick0.tick = 0;
	pulse_tick1.tick = 0;

	tai_module_info_t info;
	info.size = sizeof(info);

	if(taiGetModuleInfo("ScePaf", &info) < 0){
		HookImport("SceShell", 0x03FCF19D, 0xC3C26339, sceSysmoduleLoadModuleInternalWithArg);
	}else{
		HookImportNonEnso("SceShell", "ScePaf", "ScePafMisc", 0x3D643CE8, 0xF8D1975F, scePafGetTimeDataStrings);
		HookImportNonEnso("SceShell", "ScePaf", "ScePafWidget", 0x073F8C68, 0x39B15B98, scePafWidgetSetFontSize);
	}

	return SCE_KERNEL_START_SUCCESS;
}
