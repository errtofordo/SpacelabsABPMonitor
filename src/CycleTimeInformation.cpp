#include "CycleTimeInformation.h"

#pragma warning(disable:6011)

CycleTimeInformation* CreateCycleTimeInformation
(
	int activeStateHour,
	int nextStateHour,
	int hourlyReadInterval,
	int tone
)
{
	return new CycleTimeInformation{ 0, 0, activeStateHour, nextStateHour, hourlyReadInterval, tone };
}

void InsertCycleTimeInformation
(
	CycleTimeInformation* head,
	int activeStateHour,
	int nextStateHour,
	int hourlyReadInterval,
	int tone
)
{
	CycleTimeInformation* newNode = CreateCycleTimeInformation(activeStateHour, nextStateHour, hourlyReadInterval, tone);
	if (head == nullptr)
	{
		head = newNode;
	}
	else
	{
		CycleTimeInformation* temp = head;
		while (temp->Next != nullptr)
		{
			temp = temp->Next;
		}
		temp->Next = newNode;
		newNode->Prev = temp;
	}
}

void DeleteCycleTimeInformation(CycleTimeInformation* head)
{
	CycleTimeInformation* current = head;
	CycleTimeInformation* nextNode = nullptr;

	while (!current)
	{
		nextNode = current->Next;
		delete current;
		current = nextNode;
	}

	head = nullptr;
}