#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <memory.h>
#include <linux/input.h>
#include <string>      
#include <iostream> 
#include <sstream>
#include <stdint.h>
#include <stddef.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <algorithm>
#include <signal.h>
#include <tuple>
#include <libgen.h>
#include <sys/time.h>
#include "Globals.h"
#include "ReportFilter.h"
#include "A1644.h"
#include "A1314.h"
#include "WebApiServer.h"
#include "bluetooth/bt.h"
#include "Logger.h"
#include "GenericReportFilter.h"
#include "Main.h"

using namespace std;

WebApiServer g_WebApiServer;
static bool g_ExitRequested = false;

int main(int argc, char** argv)
{
	InfoMsg("RaspiKey " VERSION ".");

	// Get RaspiKey binary "working" dir
	readlink("/proc/self/exe", Globals::g_szModuleDir, sizeof(Globals::g_szModuleDir));
	dirname(Globals::g_szModuleDir); //strip last component from path

	InfoMsg(Globals::FormatString("System uptime: %lu", Globals::GetUptime()).c_str());

	if (!StartServices())
	{
		ErrorMsg("StartServices() failed.");
		return -1;
	}

	PollDevicesLoop();

	return 0;
}

void SignalHandler(int signo)
{
	if (signo == SIGINT || signo == SIGTERM)
	{
		InfoMsg("Termination request received");		

		StopAllServices();
	}
	else if (signo == SIGALRM) // Timer
	{
		InfoMsg("Timer elapsed signal received");
	}
}

bool StartServices()
{
	// Start the Web Api server
	g_WebApiServer.Start();

	// Start the bluetooth
	try
	{
		bluetooth::Start();
	}
	catch (exception& ex)
	{
		ErrorMsg("Fatal error: could not initialize the bluetooth device: %s", ex.what());
		return false;
	}

	// Set signal handlers
	signal(SIGINT, SignalHandler);
	signal(SIGTERM, SignalHandler);
	signal(SIGALRM, SignalHandler);

	return true;
}

void PollDevicesLoop()
{
	while (!g_ExitRequested)
	{
		DevFileDescriptors fds;
		memset(&fds, 0, sizeof(fds));

		if (!OpenDevices(fds))
		{
			sleep(1);
			continue;
		}

		IReportFilter* prp;
		if (fds.appleKbInputEventDevName == A1644_DEV_NAME)
		{
			InfoMsg("Using A1644 report processor");
			prp = new A1644();
		}
		else if (fds.appleKbInputEventDevName == A1314_DEV_NAME)
		{
			InfoMsg("Using A1314 report processor");
			prp = new A1314();
		}
		else
		{
			//ErrorMsg("Unexpected device %s", fds.appleKbInputEventDevName.c_str());
			//sleep(1);
			//continue;
			InfoMsg("Using GENERIC report processor");
			prp = new GenericReportFilter();
		}

		// Start forwarding loop with the established devices
		ForwardingLoop(prp, fds.appleKbHidrawFd, fds.hidgKbFd);
		if (g_ExitRequested)
		{
			// This is to prevent the frozen "C" key situation if the user presses Ctrl-C in the raspikey process console.
			DbgMsg("Sending break scancode before exiting");
			uint8_t buf[16] = { 0 };
			buf[0] = 1;
			write(fds.hidgKbFd, buf, 9);
		}

		CloseDevices(fds);
		delete prp;
		prp = nullptr;

		sleep(1);
	}
}

void StopAllServices()
{	
	g_ExitRequested = true;

	bluetooth::Stop();
	g_WebApiServer.Stop();	
}

void CloseDevices(DevFileDescriptors& fds)
{
	close(fds.appleKbInputEventFd);
	close(fds.appleKbHidrawFd);
	close(fds.hidgKbFd);
}

bool OpenDevices(DevFileDescriptors& fds)
{
	int res = OpenAppleKbDevice(fds.appleKbInputEventDevName, fds.appleKbInputEventFd, fds.appleKbHidrawFd);
	if (res < 0)
		return false;

	res = OpenHidgKbDevice(fds.hidgKbFd);

	return res >= 0;
}

int ForwardingLoop(IReportFilter* prp, int appleKbHidrawFd, int hidgKbFd)
{
	uint8_t buf[16] = { 0 };

	while(!g_ExitRequested)
	{
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(appleKbHidrawFd, &fds);
		FD_SET(hidgKbFd, &fds);
		const int fdsmax = std::max(appleKbHidrawFd, hidgKbFd);

		struct timeval tv{}; 
		tv.tv_sec = 1;
		tv.tv_usec = 0;

		const int ret = select(fdsmax + 1, &fds, nullptr, nullptr, &tv);
		if (ret == -1)
		{
			ErrorMsg("Error select(): %s", strerror(errno));
			if (errno == EINTR) // Interrupted system call due to external signal (we should retry)
				continue;
			return -1; // Any other error, exit the loop
		}

		// Kbd -> PC
		if (FD_ISSET(appleKbHidrawFd, &fds))
		{
			ssize_t len = read(appleKbHidrawFd, buf, sizeof(buf));
			if (len < 0)
			{
				ErrorMsg("Error read(appleKbFd): %s", strerror(errno));
				return -1;
			}
					
			DbgMsg("[in] -> %s", Globals::FormatBuffer(buf, len).c_str());

			len = prp->ProcessInputReport(buf, len);
			if (len > 0)
			{
				DbgMsg("[in] ~> %s", Globals::FormatBuffer(buf, len).c_str());

				len = write(hidgKbFd, buf, len);
				if (len < 0)
				{
					ErrorMsg("Error write(hidgKbFd): %s", strerror(errno));
					return -1;
				}
			}
		}

		// PC -> Kbd
		if (FD_ISSET(hidgKbFd, &fds))
		{
			ssize_t len = read(hidgKbFd, buf, sizeof(buf));
			if (len < 0)
			{
				ErrorMsg("Error read(hidgKbFd): %s", strerror(errno));
				return -1;
			}

			DbgMsg("[out] -> %s", Globals::FormatBuffer(buf, len).c_str());

			len = prp->ProcessOutputReport(buf, len);
			if (len > 0)
			{
				DbgMsg("[out] ~> %s", Globals::FormatBuffer(buf, len).c_str());

				len = write(appleKbHidrawFd, buf, len);
				if (len < 0)
				{
					ErrorMsg("Error write(appleKbFd): %s", strerror(errno));
					return -1;
				}
			}
		}		
	}

	return 0;
}

int OpenAppleKbDevice(string& strDevName, int& appleKbInputEventFd, int& appleKbHidrawFd)
{
	appleKbInputEventFd = open(EVENT_DEV_PATH, O_RDWR);
	if (appleKbInputEventFd < 0)
	{
		InfoMsg("Failed to open() " EVENT_DEV_PATH ": %s", strerror(errno));
		
		return -1;
	}

	char szDevName[256] = "";
	ioctl(appleKbInputEventFd, EVIOCGNAME(sizeof(szDevName)), szDevName);
	strDevName = szDevName;

	int res = ioctl(appleKbInputEventFd, EVIOCGRAB, 1);
	if (res < 0)
	{
		InfoMsg("Failed to get exclusive access on " EVENT_DEV_PATH ": %s", strerror(errno));
		
		close(appleKbInputEventFd);
		appleKbInputEventFd = -1;

		return -1;
	}

	appleKbHidrawFd = open(HIDRAW_DEV_PATH, O_RDWR);
	if (appleKbHidrawFd < 0)
	{
		InfoMsg("Failed to open() " HIDRAW_DEV_PATH ": %s", strerror(errno));

		close(appleKbInputEventFd);
		appleKbInputEventFd = -1;

		return -1;
	}

	return 0;
}

int OpenHidgKbDevice(int& hidgKbFd)
{
	hidgKbFd = open(HIDG_DEV_PATH, O_RDWR);
	if (hidgKbFd == -1)
	{
		InfoMsg("Failed to open() " HIDG_DEV_PATH ": %s", strerror(errno));

		return -1;
	}

	return 0;
}





