#include "WorkerThread.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

static int thread_count; 

static HANDLE * go_signal;
static HANDLE * done_signal;

static volatile LONG remaining;

struct Params {
	int thread_id;

	const Raytracer * raytracer;
	const Window    * window;

	PerformanceStats * stats;
};
static Params * parameters;

static PerformanceStats * stats;

// This is the actual function that will run on each Worker Thread
// It will wait until work becomes available, execute it and notify when the work is done
ULONG WINAPI worker_thread(LPVOID parameters) {
	Params params = *reinterpret_cast<Params *>(parameters);

	HANDLE thread = GetCurrentThread(); 

	WCHAR thread_name[32];
	wsprintfW(thread_name, L"WorkerThread_%d", params.thread_id);
	SetThreadDescription(thread, thread_name);
	
	// Set the Thread Affinity to pin it to 1 logical core
#if USE_MULTITHREADING
	DWORD_PTR thread_affinity_mask     = 1 << params.thread_id;
	DWORD_PTR thread_affinity_mask_old = SetThreadAffinityMask(thread, thread_affinity_mask);

	// Check validity of Thread Affinity
	if ((thread_affinity_mask & thread_affinity_mask_old) == 0) {
		printf("Unable to set Process Affinity Mask!\n");

		abort();
	}
#endif

	while (true) {
		// Wait until the Worker Thread is signalled by the main thread
		WaitForSingleObject(go_signal[params.thread_id], INFINITE);

		while (remaining > 0) { 
			int task = (int)InterlockedDecrement(&remaining); 
			
			if (task >= 0) { 
				int x = (task % params.window->tile_count_x) * params.window->tile_width;
				int y = (task / params.window->tile_count_x) * params.window->tile_height; 
				
				int tile_width  = x + params.window->tile_width  < params.window->width  ? params.window->tile_width  : params.window->width  - x;
				int tile_height = y + params.window->tile_height < params.window->height ? params.window->tile_height : params.window->height - y;

				params.raytracer->render_tile(*params.window, x, y, tile_width, tile_height, *params.stats);
			} 
		}
		
		// Signal the main thread that we are done
		SetEvent(done_signal[params.thread_id]);
	}
}

void WorkerThreads::init(const Raytracer & raytracer, const Window & window) {
#if USE_MULTITHREADING
	thread_count = 0;

	SYSTEM_LOGICAL_PROCESSOR_INFORMATION info[64];
	DWORD buffer_length = 64 * sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
	GetLogicalProcessorInformation(info, &buffer_length);
	
	// Count the number of Logical Cores
	for (int i = 0; i < buffer_length / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION); i++) {
		if (info[i].Relationship == LOGICAL_PROCESSOR_RELATIONSHIP::RelationProcessorCore) {
			// For each Physical Core count its Logical Cores
			for (int j = 0; j < 32; j++) {
				if (info[i].ProcessorMask >> j & 1) {
					thread_count++;
				}
			}
		}
	}
#else
	thread_count = 1;
#endif

	go_signal   = new HANDLE[thread_count];
	done_signal = new HANDLE[thread_count];

	parameters = new Params[thread_count];

	stats = new PerformanceStats[thread_count];

	// Spawn the appropriate number of Worker Threads.
	for (int i = 0; i < thread_count; i++) {
		go_signal  [i] = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		done_signal[i] = CreateEvent(nullptr, FALSE, FALSE, nullptr);

		parameters[i].thread_id = i;
		parameters[i].raytracer = &raytracer;
		parameters[i].window    = &window;
		parameters[i].stats     = stats + i;

		CreateThread(nullptr, 0, worker_thread, &parameters[i], 0, nullptr);
	}
}

void WorkerThreads::wake_up_worker_threads(int job_count) {
	remaining = job_count;
	
	// Set all performance statistics to zero
	memset(stats, 0, thread_count * sizeof(PerformanceStats));

	for (int i = 0; i < thread_count; i++) {
		SetEvent(go_signal[i]);
	}
}

void WorkerThreads::wait_on_worker_threads() {
	WaitForMultipleObjects(thread_count, done_signal, true, INFINITE);
}

PerformanceStats WorkerThreads::sum_performance_stats() {
	PerformanceStats result = { 0 };

	for (int i = 0; i < thread_count; i++) {
		result.num_primary_rays    += stats[i].num_primary_rays;
		result.num_shadow_rays     += stats[i].num_shadow_rays;
		result.num_reflection_rays += stats[i].num_reflection_rays;
		result.num_refraction_rays += stats[i].num_refraction_rays;
	}

	// Rays are traced in Packets of size SIMD_LINE_SIZE
	result.num_primary_rays    *= SIMD_LANE_SIZE;
	result.num_shadow_rays     *= SIMD_LANE_SIZE;
	result.num_reflection_rays *= SIMD_LANE_SIZE;
	result.num_refraction_rays *= SIMD_LANE_SIZE;

	return result;
}
