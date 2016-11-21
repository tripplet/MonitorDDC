// MonitorDDC.cpp : Defines the entry point for the console application.
//

#include <stdio.h>
#include <tchar.h>

#include <string>
#include <iostream>
#include <fstream>
#include <list>
#include <memory>
#include <algorithm>
#include <ctime>
#include <locale>
#include <sstream>
#include <iomanip>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <HighLevelMonitorConfigurationAPI.h>

#include "LimitSingleInstance.h"

#include "yocto_api.h"
#include "yocto_lightsensor.h"

CLimitSingleInstance *singleInstance;

#define PROGRAMM_GUID L"{B720223C-220C-49EC-82D0-89A77970BF0B}"

BOOL CALLBACK MyInfoEnumProc(_In_ HMONITOR hMonitor, _In_ HDC hdcMonitor, _In_ LPRECT lprcMonitor, _In_ LPARAM dwData)
{
	DWORD cPhysicalMonitors = 0;

	if (GetNumberOfPhysicalMonitorsFromHMONITOR(hMonitor, &cPhysicalMonitors))
	{
		auto pPhysicalMonitors = std::make_unique<PHYSICAL_MONITOR[]>(cPhysicalMonitors);

		if (GetPhysicalMonitorsFromHMONITOR(hMonitor, cPhysicalMonitors, pPhysicalMonitors.get()))
		{
			auto list = reinterpret_cast<std::list<HANDLE>*>(dwData);

			for (size_t idx = 0; idx < cPhysicalMonitors; idx++)
			{
				list->push_back(pPhysicalMonitors[idx].hPhysicalMonitor);
			}
		}
	}

	return true;
}

void PrintLighSensorValue(_In_ YLightSensor * sensor)
{
	if (sensor != NULL && sensor->isOnline())
	{
		std::cout << "Current ambient light: " << sensor->get_currentValue() << " lx (" << std::fixed << std::setprecision(1) << (sensor->get_module()->get_upTime() / 60000.0) << " min up)" << std::endl;
	}
}

int PrintCurrentMonitorBrightness(std::list<HANDLE> &monitors) {
	std::cout << "Monitor brightness: ";
	int firstMonitorBrightness;

	std::for_each(monitors.begin(), monitors.end(), [&monitors,&firstMonitorBrightness](HANDLE monitor)
	{
		DWORD min, current, max;
		GetMonitorBrightness(monitor, &min, &current, &max);

		if (monitor != monitors.front())
		{
			std::cout << ", ";
		} 
		else 
		{
			firstMonitorBrightness = current;
		}

		std::cout << current << "%";
	});

	std::cout << std::endl;
	return firstMonitorBrightness;
}

// See: http://stackoverflow.com/questions/15220861/how-can-i-set-the-comma-to-be-a-decimal-point
class punct_facet : public std::numpunct<char>
{
	protected: char do_decimal_point() const { return '.'; }
};

int main()
{
	singleInstance = new CLimitSingleInstance(PROGRAMM_GUID);
	if (singleInstance->IsAnotherInstanceRunning())
	{
		return -1;
	}

	// Set decimal separator to '.'
	std::cout.imbue(std::locale(std::cout.getloc(), new punct_facet()));

	std::list<HANDLE> monitors;

	string lightSensorErrorMsg;
	YLightSensor *sensor;
	int desiredMonitorBrightness;

	// Get local monitors
	EnumDisplayMonitors(NULL, NULL, MyInfoEnumProc, reinterpret_cast<LPARAM>(&monitors));

	// Setup the API to use local USB devices
	if (yRegisterHub("usb", lightSensorErrorMsg) != YAPI_SUCCESS)
	{
		std::cerr << "RegisterHub (light sensor) error: " << lightSensorErrorMsg << std::endl;
	}

	sensor = YLightSensor::FirstLightSensor();
	if (sensor == NULL)
	{
		std::cout << "No light sensor found" << std::endl;
	}
	else
	{
		sensor->get_module()->setLuminosity(0);
	}

	PrintLighSensorValue(sensor);
	desiredMonitorBrightness = PrintCurrentMonitorBrightness(monitors);

	char buffer[100];
	string input;
	std::cout << std::endl << "Set monitor brightness by typing percent value (0-100)" << std::endl
		<< "g = just get the ambient light" << std::endl
		<< "s = save ambient light to monitor brightness pair" << std::endl
		<< "m = get current monitor brightness" << std::endl
		<< "q = quit" << std::endl << std::endl;

	do
	{
		std::cout << "> ";
		std::cin >> input;

		if (input == "s")
		{
			//  Current time
			const time_t tStart = time(0);
			struct tm tmStart;
			localtime_s(&tmStart, &tStart);

			snprintf(buffer, 100, "%04d-%02d-%02d %02d:%02d:%02d    %.1f lx = %d %% (%.f min up)",
				(1900 + tmStart.tm_year), tmStart.tm_mon, tmStart.tm_mday, tmStart.tm_hour, tmStart.tm_min, tmStart.tm_sec,
				sensor->get_currentValue(), desiredMonitorBrightness, (sensor->get_module()->get_upTime() / 60000.0));

			std::ofstream outfile;
			outfile.open("monitor.log", std::ios_base::app);
			outfile << std::endl << buffer;
			outfile.close();
			std::cout << buffer << std::endl;
		}
		else if (input == "q")
		{
			break;
		}
		else if (input == "m")
		{
			PrintCurrentMonitorBrightness(monitors);
		}
		else if (input == "g")
		{
			PrintLighSensorValue(sensor);
		}
		else
		{
			try
			{
				desiredMonitorBrightness = stoi(input);

				PrintLighSensorValue(sensor);

				if (desiredMonitorBrightness >= 0 && desiredMonitorBrightness <= 100)
				{
					std::for_each(monitors.begin(), monitors.end(), [&desiredMonitorBrightness](HANDLE monitor) { SetMonitorBrightness(monitor, desiredMonitorBrightness); });
				}
				else
				{
					std::cout << "Invalid brightness value" << std::endl;
				}
			}
			catch (const std::exception&)
			{
				std::cout << "Unknown command" << std::endl;
			}			
		}
	}
	while (true);

	// Cleanup
	for (auto monitor : monitors)
	{
		DestroyPhysicalMonitor(monitor);
	}

	yFreeAPI();
	delete singleInstance;

	return 0;
}
