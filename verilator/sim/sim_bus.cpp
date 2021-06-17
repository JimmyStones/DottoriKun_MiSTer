#include <iostream>
#include <queue>
#include <string>

#include "sim_bus.h"
#include "sim_console.h"
#include "verilated_heavy.h"

#ifndef _MSC_VER
#else
#define WIN32
#endif


static DebugConsole console;

FILE* ioctl_file = NULL;
int ioctl_next_addr = -1;
int ioctl_last_index = -1;

IData* ioctl_addr = NULL;
CData* ioctl_index = NULL;
CData* ioctl_wait = NULL;
CData* ioctl_download = NULL;
CData* ioctl_upload = NULL;
CData* ioctl_wr = NULL;
CData* ioctl_dout = NULL;
CData* ioctl_din = NULL;

std::queue<SimBus_DownloadChunk> downloadQueue;

void SimBus::QueueDownload(std::string file, int index) {
	SimBus_DownloadChunk chunk = SimBus_DownloadChunk(file, index);
	downloadQueue.push(chunk);
}

int nextchar = 0;
void SimBus::BeforeEval()
{
	// If no file is open and there is a download queued
	if (!ioctl_file && downloadQueue.size() > 0) {

		// Get chunk from queue
		currentDownload = downloadQueue.front();
		downloadQueue.pop();

		// If last index differs from this one then reset the addresses
		if (currentDownload.index != *ioctl_index) { ioctl_next_addr = -1; }

		// Set address and index
		*ioctl_addr = ioctl_next_addr;
		*ioctl_index = currentDownload.index;

		// Open file
		ioctl_file = fopen(currentDownload.file.c_str(), "rb");
		if (!ioctl_file) {
			console.AddLog("Cannot open file for download %s\n", currentDownload.file.c_str());
		}
		else {
			console.AddLog("Starting download: %s %d", currentDownload.file.c_str(), ioctl_next_addr, ioctl_next_addr);
		}
	}

	if (ioctl_file) {
		//console.AddLog("ioctl_download addr %x  ioctl_wait %x  ioctl_wr %x  ioctl_din %x", *ioctl_addr, *ioctl_wait, *ioctl_wr, *ioctl_din);
		if (*ioctl_wait == 0) {
			*ioctl_download = 1;
			*ioctl_wr = 1;
			if (feof(ioctl_file)) {
				fclose(ioctl_file);
				ioctl_file = NULL;
				*ioctl_download = 0;
				*ioctl_wr = 0;
				console.AddLog("ioctl_download complete %d", ioctl_next_addr);
			}
			if (ioctl_file) {
				int curchar = fgetc(ioctl_file);
				if (feof(ioctl_file) == 0) {
					nextchar = curchar;
			//		console.AddLog("ioctl_download: dout %x \n", *ioctl_dout);
					ioctl_next_addr++;
				}
			}
		}
	}
	else {
		ioctl_download = 0;
		ioctl_wr = 0;
	}
}

void SimBus::AfterEval()
{
	*ioctl_addr = ioctl_next_addr;
	*ioctl_dout = (unsigned char)nextchar;
	if (ioctl_file) {
		//console.AddLog("ioctl_download %x wr %x dl %x do\n", *ioctl_addr, *ioctl_wr, *ioctl_download, *ioctl_dout);
	}
}


SimBus::SimBus(DebugConsole c) {
	console = c;
	ioctl_addr = NULL;
	ioctl_index = NULL;
	ioctl_wait = NULL;
	ioctl_download = NULL;
	ioctl_upload = NULL;
	ioctl_wr = NULL;
	ioctl_dout = NULL;
	ioctl_din = NULL;
}

SimBus::~SimBus() {

}