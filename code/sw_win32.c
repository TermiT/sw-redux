#include <windows.h>
#include <stdio.h>

#include <SDL/SDL.h>
#include <SDL/SDL_syswm.h>

#include "sw_system.h"
//#define WIN32_LEAN_AND_MEAN

static double PCFreq = 0.0;
static __int64 CounterStart = 0;

static LARGE_INTEGER m_high_perf_timer_freq;

void Win32_InitQPC() {
	if(!QueryPerformanceFrequency(&m_high_perf_timer_freq)) {
		printf("QueryPerformanceFrequency failed!\n");
	}

    PCFreq = ((double)(m_high_perf_timer_freq.QuadPart))/1000.0;

    QueryPerformanceCounter(&m_high_perf_timer_freq);
    CounterStart = m_high_perf_timer_freq.QuadPart;
}

double Win32_GetQPC() {
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return ((double)(li.QuadPart-CounterStart))/PCFreq;
}

extern SDL_Surface *sdl_surface;

int Sys_Init(int argc, char *argv[]) {
	return 0;
}

const char* Sys_GetResourceDir(void) {
	return ".";
}

void Sys_ShowAlert(const char * text) {
	MessageBoxA(NULL, text, "Shadow Warrior", MB_OK);
}

void Sys_StoreData(const char *filepath, const char *data) {
	FILE *f;
	if ((f = fopen(filepath, "wb")) != NULL) {
		fputs(data, f);
		fclose(f);
	}
}

void Sys_InitTimer() {
	timeBeginPeriod(1);
	Win32_InitQPC();
	Sys_OutputDebugString("Sys_InitTimer\n");
}

void Sys_UninitTimer() {
	timeEndPeriod(1);
}

double Sys_GetTicks() {
	//char buf[40];
	//sprintf(buf, "%d\n", Win32_GetQPC()*1000);
	//Sys_OutputDebugString(buf);
	//return Win32_GetQPC();
	return Win32_GetQPC();
}

void Sys_ThrottleFPS(int max_fps) {
	static double end_of_prev_frame = 0.0;
	double frame_time, current_time, time_to_wait;
	int i;

	if (end_of_prev_frame != 0) {
		frame_time = 1000.0/max_fps;

		while (1) {
			current_time = Win32_GetQPC();
			time_to_wait = frame_time - (current_time - end_of_prev_frame);
			if (time_to_wait < 0.00001) {
				break;
			}
			if (time_to_wait > 2) {
				Sleep(1);
			} else {
				for (i = 0; i < 10; i++) {
					Sleep(0);
				}
			}
		}

		end_of_prev_frame = current_time;
	} else {
		end_of_prev_frame = Win32_GetQPC();
	}
}

void Sys_GetScreenSize(int *width, int *height) {
	HMONITOR monitor = MonitorFromWindow(win_gethwnd(), MONITOR_DEFAULTTONEAREST);
	MONITORINFO info;
	info.cbSize = sizeof(MONITORINFO);
	GetMonitorInfo(monitor, &info);
	*width = info.rcMonitor.right - info.rcMonitor.left;
	*height = info.rcMonitor.bottom - info.rcMonitor.top;
}

void Sys_CenterWindow(int w, int h) {
	HWND hwnd = win_gethwnd();
	HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
	MONITORINFO info;
	RECT rc;
	int x, y;
	info.cbSize = sizeof(MONITORINFO);
	GetMonitorInfo(monitor, &info);
	GetWindowRect(hwnd, &rc);
	x = info.rcWork.left + (info.rcWork.right-info.rcWork.left-w)/2;
	y = info.rcWork.top + (info.rcWork.bottom-info.rcWork.top-h)/2;
	MoveWindow(hwnd, x, y, w, h, FALSE);
}

void* win_gethwnd() {
	SDL_SysWMinfo wmi;
    SDL_VERSION(&wmi.version);
	if (!SDL_GetWMInfo(&wmi)) {
		return NULL;
	}
    return wmi.window;
}

void Sys_OutputDebugString(const char *string) {
	OutputDebugStringA(string);
}
