#pragma once

struct CycleTimeInformation
{
	CycleTimeInformation* Next;
	CycleTimeInformation* Prev;
	int ActiveStateHour;
	int NextStateHour;
	int HourlyReadInterval;
	int Tone;
};

CycleTimeInformation* CreateCycleTimeInformation
(
	int activeStateHour,
	int nextStateHour,
	int hourlyReadInterval,
	int tone
);

void InsertCycleTimeInformation
(
	CycleTimeInformation* head,
	int activeStateHour,
	int nextStateHour,
	int hourlyReadInterval,
	int tone
);

void DeleteCycleTimeInformation(CycleTimeInformation* head);