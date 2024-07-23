#include "pch.h"
#include "Driver.h"
#include <ntstrsafe.h>

// defining functions which will be implemented

FLT_PREOP_CALLBACK_STATUS SecuredFileSystemPreCreate(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID*);

NTSTATUS SecuredFileSystemUnload(FLT_FILTER_UNLOAD_FLAGS flags);

NTSTATUS SecuredFileSystemInstanceSetup(
	PCFLT_RELATED_OBJECTS flt_objects,
	FLT_INSTANCE_SETUP_FLAGS flags,
	DEVICE_TYPE volume_device_type,
	FLT_FILESYSTEM_TYPE	volume_filesystem_type
);

NTSTATUS SecuredFileSystemInstanceQueryTeardown(
	PCFLT_RELATED_OBJECTS flt_objects,
	FLT_INSTANCE_QUERY_TEARDOWN_FLAGS flags
);

VOID SecuredFileSystemInstanceTeardownStart(
	PCFLT_RELATED_OBJECTS flt_objects,
	FLT_INSTANCE_TEARDOWN_FLAGS flags
);

VOID SecuredFileSystemInstanceTeardownComplete(
	PCFLT_RELATED_OBJECTS flt_objects,
	FLT_INSTANCE_TEARDOWN_FLAGS flags
);

bool IsUserAttempt();
bool IntervalPassed();
UNICODE_STRING GetFileName(PFLT_CALLBACK_DATA data);
NTSTATUS GetAccessState();
bool IsPasswordNeeded(PFLT_CALLBACK_DATA data, UNICODE_STRING file_name);
NTSTATUS CommunicateWithUserMode();
NTSTATUS HandleRequest(PFLT_CALLBACK_DATA data, UNICODE_STRING file_name);

#define PASSWORD_PROMPT_INTERVAL (15 * 1000 * 10000) // 15 seconds in 100-nanosecond interval

// global variable to store the last time the password was prompted, and a mutex to prevent race condition. access variable
// to determine open approval or prohibition
LARGE_INTEGER last_prompt_time;
KMUTEX time_mutex;
bool access = true;

extern "C"
NTSTATUS InitMinifilter(PDRIVER_OBJECT driver_obj, PUNICODE_STRING registry_path) {

	KdPrint(("Currently in the InitMiniFilter function!\n"));

	// initiate Registry keys so that the driver would be recognized as a minifilter in the system

	HANDLE hKey = nullptr, hSubKey = nullptr;
	NTSTATUS status;

	do {

		OBJECT_ATTRIBUTES keyAttr = RTL_CONSTANT_OBJECT_ATTRIBUTES(registry_path, OBJ_KERNEL_HANDLE);
		status = ZwOpenKey(&hKey, KEY_WRITE, &keyAttr);

		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "Failed opening hKey, status: (0x%08X)\n", status));
			break;
		}

		UNICODE_STRING subKey = RTL_CONSTANT_STRING(L"Instances");
		OBJECT_ATTRIBUTES subKeyAttr;
		InitializeObjectAttributes(&subKeyAttr, &subKey, OBJ_KERNEL_HANDLE, hKey, nullptr);

		status = ZwCreateKey(&hSubKey, KEY_WRITE, &subKeyAttr, 0, nullptr, 0, nullptr);

		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "Failed creating subKey Instances, status: (0x%08X)\n", status));
			break;
		}

		UNICODE_STRING value_name = RTL_CONSTANT_STRING(L"DefaultInstance");
		WCHAR name[] = L"SecuredFileSystemDefaultInstance";

		status = ZwSetValueKey(hSubKey, &value_name, 0, REG_SZ, name, sizeof(name));

		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "Failed setting DefaultInstance's value key, status: (0x%08X)\n", status));
			break;
		}

		UNICODE_STRING instKeyName;
		RtlInitUnicodeString(&instKeyName, name);
		HANDLE hInstKey;
		InitializeObjectAttributes(&subKeyAttr, &instKeyName, OBJ_KERNEL_HANDLE, hSubKey, nullptr);

		status = ZwCreateKey(&hInstKey, KEY_WRITE, &subKeyAttr, 0, nullptr, 0, nullptr);

		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "Failed creating instance key under Instances, status: (0x%08X)\n", status));
			break;
		}

		WCHAR altitude[] = L"425342";
		UNICODE_STRING altitude_name = RTL_CONSTANT_STRING(L"Altitude");

		status = ZwSetValueKey(hInstKey, &altitude_name, 0, REG_SZ, altitude, sizeof(altitude));

		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "Failed setting altitude value key, status: (0x%08X)\n", status));
			break;
		}

		UNICODE_STRING flags_name = RTL_CONSTANT_STRING(L"Flags");
		ULONG flags = 0;

		status = ZwSetValueKey(hInstKey, &flags_name, 0, REG_DWORD, &flags, sizeof(flags));

		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "Failed setting flags value key, status: (0x%08X)\n", status));
			break;
		}

		// initiate the registry key which will be used to communicate with the user-mode

		UNICODE_STRING access_status = RTL_CONSTANT_STRING(L"Access");
		DWORD access_value = 0;

		status = ZwSetValueKey(hInstKey, &access_status, 0, REG_DWORD, &access_value, sizeof(access_value));

		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "Failed setting access value key, status: (0x%08X)\n", status));
			break;
		}

		KdPrint(("Finished initiating registry keys!\n"));

		ZwClose(hInstKey);

		// register the minifilter as PreCreate interceptor

		FLT_OPERATION_REGISTRATION const callbacks[] = {
			{IRP_MJ_CREATE, 0, SecuredFileSystemPreCreate, nullptr},
			{IRP_MJ_OPERATION_END}
		};

		FLT_REGISTRATION const reg = {

			sizeof(FLT_REGISTRATION),
			FLT_REGISTRATION_VERSION,
			0,
			nullptr,
			callbacks,
			SecuredFileSystemUnload,
			SecuredFileSystemInstanceSetup,
			SecuredFileSystemInstanceQueryTeardown,
			SecuredFileSystemInstanceTeardownStart,
			SecuredFileSystemInstanceTeardownComplete

		};

		status = FltRegisterFilter(driver_obj, &reg, &filter);

	} while (false);

	if (hSubKey) {

		if (!NT_SUCCESS(status))
			ZwDeleteKey(hSubKey);

		ZwClose(hSubKey);

	}

	if (hKey)
		ZwClose(hKey);

	// initialize the time and mutex

	KeInitializeMutex(&time_mutex, 0);
	KeQuerySystemTime(&last_prompt_time);

	// subtract the interval to ensure the first access requires a password
	last_prompt_time.QuadPart -= PASSWORD_PROMPT_INTERVAL;

	return status;

}

FLT_PREOP_CALLBACK_STATUS SecuredFileSystemPreCreate(PFLT_CALLBACK_DATA data, PCFLT_RELATED_OBJECTS flt_objects, PVOID*) {

	UNREFERENCED_PARAMETER(flt_objects);

	// skip kernel-mode requests
	if (data->RequestorMode == KernelMode)
		return FLT_PREOP_SUCCESS_NO_CALLBACK;

	const auto& params = data->Iopb->Parameters.Create;

	if (!(params.Options & FILE_OPEN))
		return FLT_PREOP_SUCCESS_NO_CALLBACK;

	// get file name
	UNICODE_STRING file_name = GetFileName(data);

	// if the request is not by the user, skip it
	const bool user_attempt = IsUserAttempt();

	if (!user_attempt) {

		return FLT_PREOP_SUCCESS_NO_CALLBACK;

	}

	// get mutex
	KeEnterCriticalRegion();
	NTSTATUS wait_status = KeWaitForSingleObject(&time_mutex, Executive, KernelMode, FALSE, NULL);

	if (!NT_SUCCESS(wait_status)) {

		KdPrint((DRIVER_PREFIX "could not get a handle to mutex\n"));

		KeLeaveCriticalRegion();

		return FLT_PREOP_SUCCESS_NO_CALLBACK;

	}

	// check whether password is needed
	bool res = IsPasswordNeeded(data, file_name);

	if (!res) {

		KeReleaseMutex(&time_mutex, FALSE);
		KeLeaveCriticalRegion();

		if (data->IoStatus.Status == STATUS_ACCESS_DENIED)
			return FLT_PREOP_COMPLETE;

		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	// communicate with user-mode
	NTSTATUS result = CommunicateWithUserMode();

	if (!NT_SUCCESS(result)) {

		KeReleaseMutex(&time_mutex, FALSE);
		KeLeaveCriticalRegion();
		
		return FLT_PREOP_SUCCESS_NO_CALLBACK;

	}

	// read Access key value from the Registry
	result = GetAccessState();

	if (!NT_SUCCESS(result)) {

		KdPrint((DRIVER_PREFIX "Failed reaching Access key\n"));

		KeReleaseMutex(&time_mutex, FALSE);
		KeLeaveCriticalRegion();

		return FLT_PREOP_SUCCESS_NO_CALLBACK;

	}

	result = HandleRequest(data, file_name);

	// updates the last prompt time, releases mutex and leaves critical region
	KeQuerySystemTime(&last_prompt_time);
	KeReleaseMutex(&time_mutex, FALSE);
	KeLeaveCriticalRegion();

	if (data->IoStatus.Status == STATUS_ACCESS_DENIED)
		return FLT_PREOP_COMPLETE;
		
	return FLT_PREOP_SUCCESS_NO_CALLBACK;

}

NTSTATUS SecuredFileSystemUnload(FLT_FILTER_UNLOAD_FLAGS flags) {

	// this routine occurs when the driver is uninstalled

	UNREFERENCED_PARAMETER(flags);

	FltCloseCommunicationPort(port);

	FltUnregisterFilter(filter);

	return STATUS_SUCCESS;

}

NTSTATUS SecuredFileSystemInstanceSetup(PCFLT_RELATED_OBJECTS flt_objects, FLT_INSTANCE_SETUP_FLAGS flags, DEVICE_TYPE volume_device_type,
	FLT_FILESYSTEM_TYPE volume_filesystem_type) {

	UNREFERENCED_PARAMETER(flt_objects);
	UNREFERENCED_PARAMETER(flags);
	UNREFERENCED_PARAMETER(volume_device_type);

	// the driver works for the NTFS file system only

	return volume_filesystem_type == FLT_FSTYPE_NTFS ? STATUS_SUCCESS : STATUS_FLT_DO_NOT_ATTACH;

}

NTSTATUS SecuredFileSystemInstanceQueryTeardown(PCFLT_RELATED_OBJECTS flt_objects, FLT_INSTANCE_QUERY_TEARDOWN_FLAGS flags) {

	UNREFERENCED_PARAMETER(flt_objects);
	UNREFERENCED_PARAMETER(flags);

	return STATUS_SUCCESS;

}

VOID SecuredFileSystemInstanceTeardownStart(PCFLT_RELATED_OBJECTS FltObjects, FLT_INSTANCE_TEARDOWN_FLAGS Flags) {

	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);

}

VOID SecuredFileSystemInstanceTeardownComplete(PCFLT_RELATED_OBJECTS FltObjects, FLT_INSTANCE_TEARDOWN_FLAGS Flags) {

	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);

}

NTSTATUS PortConnectNotify(PFLT_PORT ClientPort, PVOID ServerPortCookie, PVOID ConnectionContext, ULONG SizeOfContext,
	PVOID* ConnectionPortCookie) {

	UNREFERENCED_PARAMETER(ServerPortCookie);
	UNREFERENCED_PARAMETER(ConnectionContext);
	UNREFERENCED_PARAMETER(SizeOfContext);
	UNREFERENCED_PARAMETER(ConnectionPortCookie);

	client_port = ClientPort;

	return STATUS_SUCCESS;
}

void PortDisconnectNotify(PVOID connection_cookie) {

	UNREFERENCED_PARAMETER(connection_cookie);

	// when client disconnects, close client's port 

	FltCloseClientPort(filter, &client_port);

	client_port = nullptr;

}

NTSTATUS PortMessageNotify(PVOID PortCookie, PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, PULONG ReturnOutputBufferLength) {
	UNREFERENCED_PARAMETER(PortCookie);
	UNREFERENCED_PARAMETER(InputBuffer);
	UNREFERENCED_PARAMETER(InputBufferLength);
	UNREFERENCED_PARAMETER(OutputBuffer);
	UNREFERENCED_PARAMETER(OutputBufferLength);
	UNREFERENCED_PARAMETER(ReturnOutputBufferLength);

	return STATUS_SUCCESS;
}

bool IsUserAttempt() {

	// asks user-mode for true if the process is explorer.exe, false if not 
	// return true if "explorer.exe" is the process, else false

	PEPROCESS process = PsGetCurrentProcess();

	CHAR image_name[15];

	// offset for windows 22H2 is 0x5a8 in explorer.exe (I checked it via WinDbg)
	RtlCopyMemory(image_name, (PVOID)((uintptr_t)process + 0x5a8), sizeof(image_name));

	// KdPrint(("%s attempts to open a file\n", image_name));

	if (strcmp(image_name, "explorer.exe") == 0) {

		return true;

	}

	return false;

}

bool IntervalPassed() {

	// this function checks whether the 15 seconds interval had passed

	LARGE_INTEGER current_time;
	KeQuerySystemTime(&current_time);

	// calculating the difference in time

	LARGE_INTEGER time_difference;
	time_difference.QuadPart = current_time.QuadPart - last_prompt_time.QuadPart;

	// check if the interval has passed

	if (time_difference.QuadPart >= PASSWORD_PROMPT_INTERVAL)
	{
		return true;
	}
	return false;

}

UNICODE_STRING GetFileName(PFLT_CALLBACK_DATA data) {

	// this function returns the file name

	UNICODE_STRING file_name = RTL_CONSTANT_STRING(L"");
	PFLT_FILE_NAME_INFORMATION file_name_info;

	NTSTATUS status = FltGetFileNameInformation(data, FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT, &file_name_info);

	if (!NT_SUCCESS(status)) {

		KdPrint((DRIVER_PREFIX "could not get file name information, error: (0x%08X)\n", status));
		return file_name;

	}

	status = FltParseFileNameInformation(file_name_info);

	if (!NT_SUCCESS(status)) {

		KdPrint((DRIVER_PREFIX "could not parse file name information, error: (0x%08X)\n", status));
		FltReleaseFileNameInformation(file_name_info);
		return file_name;

	}

	file_name = file_name_info->Name;
	FltReleaseFileNameInformation(file_name_info);

	return file_name;


}

NTSTATUS GetAccessState() {

	// this function reads the Access key and changes the global variable access accordingly

	UNICODE_STRING registryPath;
	OBJECT_ATTRIBUTES objectAttributes;
	HANDLE hKey;
	NTSTATUS status;
	ULONG resultLength;
	PKEY_VALUE_PARTIAL_INFORMATION pKeyValueInfo;
	UCHAR keyValueBuffer[sizeof(KEY_VALUE_PARTIAL_INFORMATION) + sizeof(DWORD)];

	// initialize the Unicode string with the registry path
	RtlInitUnicodeString(&registryPath, L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Services\\SecuredFileSystem\\Instances\\SecuredFileSystemDefaultInstance");

	// initialize the OBJECT_ATTRIBUTES structure
	InitializeObjectAttributes(&objectAttributes, &registryPath, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);

	// open the registry key
	status = ZwOpenKey(&hKey, KEY_READ, &objectAttributes);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Failed to open registry key, status: 0x%X\n", status));
		return status;
	}

	// query the registry value
	UNICODE_STRING valueName;
	RtlInitUnicodeString(&valueName, L"Access");

	pKeyValueInfo = (PKEY_VALUE_PARTIAL_INFORMATION)keyValueBuffer;
	status = ZwQueryValueKey(hKey, &valueName, KeyValuePartialInformation, pKeyValueInfo, sizeof(keyValueBuffer), &resultLength);

	if (!NT_SUCCESS(status)) {

		KdPrint((DRIVER_PREFIX "could not query value key, error: (0x%08X)\n", status));
		return status;

	}

	if (pKeyValueInfo->Type != REG_DWORD || pKeyValueInfo->DataLength != sizeof(DWORD)) {

		KdPrint((DRIVER_PREFIX "not the correct form\n"));
		return STATUS_UNSUCCESSFUL;

	}

	DWORD accessValue = *(DWORD*)(pKeyValueInfo->Data);

	if (accessValue == 0)
		access = true;
	else
		access = false;

	ZwClose(hKey);

	return STATUS_SUCCESS;

}

bool IsPasswordNeeded(PFLT_CALLBACK_DATA data, UNICODE_STRING file_name) {

	// returns true if password is needed, false otherwise. Operates accordingly

	if (IntervalPassed())
		return true;

	KdPrint((DRIVER_PREFIX "Password Not Needed\n"));

	if (!access) {

		data->IoStatus.Status = STATUS_ACCESS_DENIED;
		data->IoStatus.Information = 0;
		KdPrint(("denied access to file %wZ\n", file_name));

	}

	KdPrint(("approved access to file %wZ\n", file_name));
	
	return false;

}

NTSTATUS CommunicateWithUserMode() {

	// this function communicates with the user mode

	struct
	{
		FILTER_REPLY_HEADER reply_header;
		bool is_access_allowed;
	} reply;

	NTSTATUS msg_status;
	WCHAR message[] = L"Enter password to access the file:";

	ULONG reply_length = sizeof(reply);

	msg_status = FltSendMessage(filter, &client_port, message, sizeof(message), &reply, &reply_length, NULL);

	if (!NT_SUCCESS(msg_status)) {

		KdPrint(("Error sending msg to user, error: (0x%08X)\n", msg_status));

		return msg_status;

	}

	if (&reply == nullptr) {

		KdPrint(("return value is nullptr\n"));

		return msg_status;
	}

	return STATUS_SUCCESS;

}

NTSTATUS HandleRequest(PFLT_CALLBACK_DATA data, UNICODE_STRING file_name) {

	// this function proccesses the global variable access value and acts accordingly

	if (access) {

		KdPrint((DRIVER_PREFIX "(Pre Create) Approved access to %wZ\n", file_name));
		return STATUS_SUCCESS;

	}

	data->IoStatus.Status = STATUS_ACCESS_DENIED;
	data->IoStatus.Information = 0;
	KdPrint((DRIVER_PREFIX "(Pre Create) Prevented access to %wZ\n", file_name));
	
	return STATUS_SUCCESS;

}

