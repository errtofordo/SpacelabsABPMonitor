#include <iostream>
#include <map>

#include <windows.h>

#include "cryptography.h"
#include "CycleTimeInformation.h"

std::vector<uint8_t> CreateCmdData(const char* cmd)
{
	std::vector<uint8_t> result;

	size_t cmdLength = strlen(cmd);
	result.resize(cmdLength);
	memcpy(result.data(), cmd, cmdLength);
	result.push_back('\r');

	unsigned short crc = Crc16(result.data(), result.size());
	char buf[5];
	sprintf_s(buf, "%02X", crc);
	result.insert(result.begin(), 0x01);
	for (int i = 0; i < 4; i++) 
	{
		result.push_back(static_cast<uint8_t>(buf[i]));
	}
	return result;
}

std::vector<std::wstring> GetCOMPorts()
{
	std::vector<std::wstring> ports;
	wchar_t portName[20];

	for (int i = 1; i <= 256; i++) 
	{
		wsprintf(portName, L"\\\\.\\COM%d", i);
		HANDLE hPort = CreateFile(portName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
		if (hPort != INVALID_HANDLE_VALUE) 
		{
			ports.push_back(portName);
			CloseHandle(hPort);
		}
	}
	return ports;
}

HANDLE ConnectToDevice()
{
	auto ports = GetCOMPorts();
	if (ports.size() == 0) 
	{ 
		std::cerr << "No COM ports available." << std::endl;
		return INVALID_HANDLE_VALUE;
	}

	HANDLE hComm = CreateFile(ports[0].c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

	if (hComm == INVALID_HANDLE_VALUE) 
	{
		std::cerr << "Error opening COM port: " << GetLastError() << std::endl;
		return INVALID_HANDLE_VALUE;
	}

	DCB dcb = { 0 };
	dcb.DCBlength = sizeof(dcb);
	if (!GetCommState(hComm, &dcb)) 
	{
		std::cerr << "Error getting COM state: " << GetLastError() << std::endl;
		CloseHandle(hComm);
		return INVALID_HANDLE_VALUE;
	}

	dcb.BaudRate = CBR_9600;
	dcb.ByteSize = 8;
	dcb.StopBits = ONESTOPBIT;
	dcb.Parity = NOPARITY;

	if (!SetCommState(hComm, &dcb)) 
	{
		std::cerr << "Error setting COM state: " << GetLastError() << std::endl;
		CloseHandle(hComm);
		return INVALID_HANDLE_VALUE;
	}

	COMMTIMEOUTS timeouts = { 0 };
	timeouts.ReadIntervalTimeout = 50;
	timeouts.ReadTotalTimeoutConstant = 50;
	timeouts.ReadTotalTimeoutMultiplier = 10;
	timeouts.WriteTotalTimeoutConstant = 50;
	timeouts.WriteTotalTimeoutMultiplier = 10;

	if (!SetCommTimeouts(hComm, &timeouts)) 
	{
		std::cerr << "Error setting timeouts: " << GetLastError() << std::endl;
		CloseHandle(hComm);
		return INVALID_HANDLE_VALUE;
	}

	return hComm;
}

void PrintData(std::vector<uint8_t> data)
{
	for (uint8_t x : data)
	{
		printf("%02X ", x);
	}
	printf("\n");
}

std::vector<uint8_t> SendCmd(HANDLE hDev, const char* cmd, int nNumberOfBytesToRead)
{
	if (hDev == INVALID_HANDLE_VALUE)
	{
		return std::vector<uint8_t>();
	}

	printf("Sending cmd: %s\n", cmd);

	std::vector<uint8_t> data(nNumberOfBytesToRead);

	DWORD bytesRead = 0;
	while (bytesRead == 0)
	{
		auto cmdData = CreateCmdData(cmd);
		if (!WriteFile(hDev, cmdData.data(), static_cast<DWORD>(cmdData.size()), NULL, NULL))
		{
			std::cerr << "Error writing to COM port: " << GetLastError() << std::endl;
			return std::vector<uint8_t>();
		}

		Sleep(1000);

		if (!ReadFile(hDev, data.data(), nNumberOfBytesToRead, &bytesRead, NULL))
		{
			std::cerr << "Error reading from COM port: " << GetLastError() << std::endl;
			return std::vector<uint8_t>();
		}
	}
	
	printf("Raw data: \n");
	PrintData(data);
		
	return data;
}

std::vector<uint8_t> SendDataCmd(HANDLE hDev, const char* cmd, int numberOfBytesToRead)
{
	auto encryptedData = SendCmd(hDev, cmd, numberOfBytesToRead);

	auto cmdData = CreateCmdData(cmd);
	cmdData.push_back('\0');
	auto hexData = std::vector<uint8_t>(cmdData.begin() + 2, cmdData.end());

	std::vector<uint8_t> key = ConvertHexStringToKey((char*)hexData.data());
	printf("Key: %02X %02X\n", key[0], key[1]);

	std::vector<uint8_t> decipheredData = DecryptHexString((char*)encryptedData.data() + 2, encryptedData.size(), &key);
	printf("Data: ");
	PrintData(decipheredData);

	unsigned short crc = Crc16(decipheredData.data(), decipheredData.size());
	printf("CRC: %02X\n", crc);

	char* strCrc = strstr((char*)encryptedData.data(), (char*)"\r");
	if (strCrc)
	{
		if (strtoul(strCrc + 1, 0, 16) == crc)
		{
			printf("Decrypt OK\n");
			return decipheredData;
		}
	}

	printf("Decrypt fail\n");
	return std::vector<uint8_t>();
}

/*
 * Sets monitor control flags:
 * @param hDev - monitor device handle
 * @param showResults - Show results of reading (true: show, false: hide)
 * @param is24h - Monitor display time (false: 12-hour, true: 24-hour)
 * @param cuffPressure - Display cuff pressure (true: show, false: hide)
 * @param clinical - Clinical verification setup (true: enable, false: dissable)
 * @return 0 - success; 1 - fail
 */
int SendMonitorControlFlags(HANDLE hDev, bool showResults, bool is24h, bool cuffPresure, bool clinical)
{
	uint8_t flags = showResults == 0;

	if (!is24h)
		flags |= 8;

	if (cuffPresure)
		flags |= 16;

	if (clinical)
		flags |= 64;

	char buff[16];
	snprintf(buff, sizeof(buff), "W811702%02X%02X", 4, flags);

	auto data = SendCmd(hDev, buff, 8);
	if (!data.empty())
	{
		return 0;
	}

	return 1;
}

enum DateComponentType 
{
	YearComponent = 0,
	MonthComponent,
	DayComponent,
	HourComponent,
	MinuteComponent
};

int FixDateComponent(uint8_t dateComponent, DateComponentType dateComponentType)
{
	switch (dateComponent)
	{
	case YearComponent:
		if (dateComponent > 99u) return 99;
		break;

	case MonthComponent:
		if ((dateComponent > 12u) || !dateComponent) return 12;
		break;

	case DayComponent:
		if ((dateComponent > 31u) || !dateComponent) return 31;
		break;

	case HourComponent:
		if (dateComponent > 23u) return 23;
		break;

	case MinuteComponent:
		if (dateComponent > 59u) return 59;
		break;
	}

	return dateComponent;
}

int ReadData(int argc, char* argv[])
{
	HANDLE hDev = ConnectToDevice();
	if (hDev == INVALID_HANDLE_VALUE)
	{
		std::cerr << "Error connecting to device" << std::endl;
		return 1;
	}

	printf("Sending password: \n");
	auto data = SendCmd(hDev, "UGENERIC       ", 8); //Password
	if (!data.empty())
	{
		printf("OK\n");
	}

	printf("Reading initialization date and time\n");
	auto initData = SendDataCmd(hDev, "D81D306", 19); // Initialization date and time
	if (initData.empty())
	{
		CloseHandle(hDev);
		return 1;
	}

	std::time_t t = std::time(0);
	std::tm now;
	localtime_s(&now, &t);
	int currentCenturyStart = ((now.tm_year + 1900) / 100) * 100;

	int initMouth = FixDateComponent(initData[0], MonthComponent);
	int initDay = FixDateComponent(initData[1], DayComponent);
	int initYear = FixDateComponent(initData[2], YearComponent) + currentCenturyStart;
	int initHour = FixDateComponent(initData[3], HourComponent);
	int initMinute = FixDateComponent(initData[4], MinuteComponent);

	printf("Initialization date: %d-%d-%d %02d:%02d\n", initYear, initMouth, initDay, initHour, initMinute);

	auto readingsData = SendDataCmd(hDev, "D810B01", 9); // Number of readings
	if (readingsData.empty())
	{
		CloseHandle(hDev);
		return 1;
	}
	printf("Number of readings: %d\n", readingsData[0]);

	printf("Systolic reading\n");
	auto systolicData = SendDataCmd(hDev, "D820078", 247); // Systolic reading
	if (systolicData.empty())
	{
		CloseHandle(hDev);
		return 1;
	}

	printf("Diastolic reading\n");
	auto diastolicData = SendDataCmd(hDev, "D830078", 247); // Diastolic reading
	if (diastolicData.empty())
	{
		CloseHandle(hDev);
		return 1;
	}

	for (int i = 0; i < 120; i++)
	{
		if (!diastolicData[i])
		{
			if (systolicData[i] == 35 || systolicData[i] == 94)
			{
				CloseHandle(hDev);
				return 1;
			}
		}
		else
		{
			systolicData[i] += 35;
		}
	}

	printf("MAP reading\n");
	auto MAPData = SendDataCmd(hDev, "D840078", 247); // MAP reading
	if (MAPData.empty())
	{
		CloseHandle(hDev);
		return 1;
	}

	printf("Heart Rate reading\n");
	auto heartRateData = SendDataCmd(hDev, "D850078", 247); // Heart Rate reading
	if (heartRateData.empty())
	{
		CloseHandle(hDev);
		return 1;
	}

	printf("Hour reading times\n");
	auto hourData = SendDataCmd(hDev, "D860078", 247); // Hour reading times
	if (hourData.empty())
	{
		CloseHandle(hDev);
		return 1;
	}

	printf("Minute reading times\n");
	auto minuteData = SendDataCmd(hDev, "D870078", 247); // Minute reading times
	if (minuteData.empty())
	{
		CloseHandle(hDev);
		return 1;
	}

	printf("Reading Day of Month information\n");
	auto dayData = SendDataCmd(hDev, "DCA0078", 247); // Reading Day of Month information
	if (dayData.empty())
	{
		CloseHandle(hDev);
		return 1;
	}

	printf("Reading Month information\n");
	auto monthData = SendDataCmd(hDev, "DCB0078", 247); // Reading Month information
	if (monthData.empty())
	{
		CloseHandle(hDev);
		return 1;
	}

	FILE* csvFile;
	if (fopen_s(&csvFile, "measurements.csv", "w"))
	{
		printf("Error opening file!\n");
		return 1;
	}

	fprintf(csvFile, "sep=,\n");
	fprintf(csvFile, "Date,Sys,Dia,MAP,Pulse Pressure,Heart Rate,Event Code\n");
	for (int i = 0; i < readingsData[0]; i++)
	{
		monthData[i] |= monthData[i] & 4;
		monthData[i] |= monthData[i] & 3;
		monthData[i] >>= 4;

		int readingMonth = FixDateComponent(monthData[i], MonthComponent);
		int readingDay = FixDateComponent(dayData[i], DayComponent);
		int readingHour = FixDateComponent(hourData[i], HourComponent);
		int readingMinute = FixDateComponent(minuteData[i], MinuteComponent);

		uint8_t pp = systolicData[i] - diastolicData[i];
		fprintf(csvFile, "%d-%02d-%02d %02d:%02d,", initYear, readingMonth, readingDay, readingHour, readingMinute);

		printf("\n");
		printf("Date of measurement: %d-%02d-%02d %02d:%02d\n", initYear, readingMonth, readingDay, readingHour, readingMinute);
		if (diastolicData[i])
		{
			printf("Sys: %d, Dia: %d\n", systolicData[i], diastolicData[i]);
			printf("MAP: %d\n", MAPData[i]);
			printf("Pulse Pressure: %d\n", pp);
			printf("Heart Rate: %d\n", heartRateData[i]);

			fprintf(csvFile, "%d,%d,%d,%d,%d\n", systolicData[i], diastolicData[i], MAPData[i], pp, heartRateData[i]);
		}
		else
		{
			printf("Event Code: %d\n", systolicData[i]);

			fprintf(csvFile, ",,,,,%02d\n", systolicData[i]);
		}
	}

	fclose(csvFile);
	CloseHandle(hDev);
	return 0;
}

int SendCycleTimeInfo(HANDLE hDev, CycleTimeInformation* cycles) 
{
	BYTE Timers[24] = { 128 };
	char Command[128] = { 0 };

	while (cycles) 
	{
		int ActiveStateHour = cycles->ActiveStateHour;
		int NextStateHour = cycles->NextStateHour;
		BYTE timerValue = 0;
		__int16 HourlyReadInterval = cycles->HourlyReadInterval;
		DWORD Tone = cycles->Tone;

		if ((BYTE)HourlyReadInterval && (unsigned __int8)HourlyReadInterval < 60u) 
		{
			timerValue = 60 / (unsigned __int8)HourlyReadInterval;
		}
		else if ((BYTE)HourlyReadInterval == 100) 
		{
			timerValue = 120;
		}

		for (int hourIndex = 0; hourIndex < 24; hourIndex++) 
		{
			if ((hourIndex >= ActiveStateHour || hourIndex < NextStateHour) &&
				(ActiveStateHour != NextStateHour || hourIndex == ActiveStateHour)) 
			{

				Timers[hourIndex] = timerValue;
				if (!Tone) {
					Timers[hourIndex] += 128;
				}
			}
		}

		cycles = cycles->Next;
	}

	if (!cycles) 
	{
		strcpy_s(Command, 9, "W814018");
		char* CommandData = Command + 7;

		for (int hourIndex = 0; hourIndex < 24; hourIndex++)
		{
			sprintf_s(CommandData, 3, "%02X", Timers[hourIndex]);
			CommandData += 2;
		}

		auto data = SendCmd(hDev, Command, 8);
		if (data.empty()) 
		{
			return 1;
		}
	}

	return 0;
}

int SendInitDate(HANDLE hDev) 
{
	time_t t = time(0);
	tm time;

	if (localtime_s(&time, &t) != 0) 
	{
		return 1; 
	}

	char cmd[32];
	sprintf_s(cmd, sizeof(cmd), "W81D306%02X%02X%02X%02X%02X%02X",
		time.tm_mon + 1, time.tm_mday, time.tm_year % 100,
		time.tm_hour, time.tm_min, 0);

	auto data = SendCmd(hDev, cmd, 8);
	return data.empty() ? 1 : 0; 
}

int SendMonitorClock(HANDLE hDev)
{
	time_t t = time(0);
	tm time;

	if (localtime_s(&time, &t) != 0)
	{
		return 1;
	}

	char cmd[32];
	sprintf_s(cmd, sizeof(cmd), "T%2.2d%2.2d%2.2d%2.2d",
		time.tm_hour, time.tm_min, time.tm_mday, time.tm_mon + 1);

	auto data = SendCmd(hDev, cmd, 8);
	return data.empty() ? 1 : 0;
}

int SendBioData(HANDLE hDev)
{
	auto data = SendCmd(hDev, "W81600100", 8);
	if (data.empty())
	{
		return 1;
	}
	else
	{
		return 0;
	}

	data = SendCmd(hDev, "W818202200", 8);
	if (data.empty())
	{
		return 1;
	}
	else
	{
		return 0;
	}

	data = SendCmd(hDev, "W81920100", 8);
	if (data.empty())
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

int Init(int argc, char* argv[])
{
	std::map<std::string, int> argMap
	{
		{"-d", 3},
		{"-n", 2},
	};

	if (argc >= 4 && argc <= 6) 
	{
		for (int i = 2; i < argc; i += 2) 
		{
			try 
			{
				int value = atoi(argv[i + 1]);

				if (value <= 0 || value > 3) 
				{
					std::cerr << "Error: Invalid value for argument: " << argv[i]
						<< ". Expected 1 to 3." << std::endl;
						return 1;
				}

				argMap.at(argv[i]) = value;
			}
			catch (const std::exception&) 
			{
				std::cerr << "Error: Invalid argument: " << argv[i]
					<< ". Expected -d <number> or -n <number>." << std::endl;
					return 1;
			}
		}
	}

	HANDLE hDev = ConnectToDevice();
	if (hDev == INVALID_HANDLE_VALUE)
	{
		std::cerr << "Error connecting to device" << std::endl;
		return 1;
	}

	printf("Sending password: \n");
	std::vector<uint8_t> data = SendCmd(hDev, "UGENERIC       ", 8); 
	if (!data.empty())
	{
		printf("OK\n");
	}
	else
	{
		printf("Error\n");
		CloseHandle(hDev);
		return 1;
	}

	printf("Sending reset command: \n");
	data = SendCmd(hDev, "R", 8);
	if (!data.empty())
	{
		printf("OK\n");
	}
	else
	{
		printf("Error\n");
		CloseHandle(hDev);
		return 1;
	}

	printf("Sending controll flags: \n");
	if (SendMonitorControlFlags(hDev, 0, 1, 0, 0)) 
	{
		printf("Error\n");
		CloseHandle(hDev);
		return 1;
	} 
	else
	{
		printf("OK\n");
	}

	int d = argMap["-d"];
	int n = argMap["-n"];

	CycleTimeInformation* info = CreateCycleTimeInformation(6, 22, d, 1);
	InsertCycleTimeInformation(info, 22, 6, n, 0);

	printf("Sending cycle time information: \n");
	if (SendCycleTimeInfo(hDev, info)) 
	{
		printf("Error\n");
		CloseHandle(hDev);
		return 1;
	}
	else
	{
		printf("OK\n");
	}

	printf("Sending initialization time to monitor: \n");
	if (SendInitDate(hDev))
	{
		printf("Error\n");
		CloseHandle(hDev);
		return 1;
	}
	else
	{
		printf("OK\n");
	}

	printf("Sending monitor clock: \n");
	if (SendMonitorClock(hDev)) 
	{
		printf("Error\n");
		CloseHandle(hDev);
		return 1;
	}
	else
	{
		printf("OK\n");
	}

	printf("Sending biographical data: \n");
	if (SendBioData(hDev)) 
	{
		printf("Error\n");
		CloseHandle(hDev);
		return 1;
	}
	else
	{
		printf("OK\n");
	}

	printf("Setting processed readings flag to zero: \n");
	data = SendCmd(hDev, "W81DA0100", 9);
	if (!data.empty()) 
	{
		printf("OK\n");
	}
	else 
	{
		printf("Error\n");
		CloseHandle(hDev);
		return 1;
	}

	printf("Monitor successfully initialized\n");
	CloseHandle(hDev);
	return 0;
}

int Help(int argc, char* argv[])
{
	printf("Usage:\n");
	printf("%s retrieve - read the monitor data\n", argv[0]);
	printf("%s init - initializate the monitor and reset data\n", argv[0]);
	return 0;
}

std::map<std::string, int(*)(int argc, char* argv[])> cmdMap
{ 
	{"retrieve", ReadData},
	{"init", Init},
	{"help", Help}
};

int main(int argc, char* argv[])
{
	if (argc > 1) 
	{
		try
		{
			return cmdMap.at(argv[1])(argc, argv);
		}
		catch (const std::exception&)
		{
			return Help(argc, argv);
		}		
	}	
	return Help(argc, argv);
}