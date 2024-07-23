#include "pch.h"
#include "Driver.h"

PFLT_FILTER filter;
PFLT_PORT port;
PFLT_PORT client_port;

extern "C"
NTSTATUS DriverEntry(PDRIVER_OBJECT driver_obj, PUNICODE_STRING registry_path) {

	KdPrint(("Currently in the DriverEntry section!\n"));

	NTSTATUS status;

	do {

		// initiating minifilter
		status = InitMinifilter(driver_obj, registry_path);

		// preparing security descriptor and creating communication port
		UNICODE_STRING name = RTL_CONSTANT_STRING(L"\\SecuredFileSystemPort");
		PSECURITY_DESCRIPTOR sd;

		status = FltBuildDefaultSecurityDescriptor(&sd, FLT_PORT_ALL_ACCESS);

		if (!NT_SUCCESS(status)) {
			KdPrint(("Failed building default security descriptor, error: (0x%08X)\n", status));
			break;
		}

		OBJECT_ATTRIBUTES port_attr;
		InitializeObjectAttributes(&port_attr, &name, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, nullptr, sd);

		status = FltCreateCommunicationPort(filter, &port, &port_attr, nullptr, PortConnectNotify,
			PortDisconnectNotify, PortMessageNotify, 1);

		if (!NT_SUCCESS(status)) {

			KdPrint(("Failed creating communication port, error: (0x%08X)\n", status));
			break;
		}

		FltFreeSecurityDescriptor(sd);
		
		// start filtering
		status = FltStartFiltering(filter);

		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "Failed starting filtering, status: (0x%08X)\n", status));
			break;
		}

	} while (false);

	if (!NT_SUCCESS(status)) {
		
		KdPrint((DRIVER_PREFIX "Failed initializing driver, status: (0x%08X)\n", status));

		if (filter)
			FltUnregisterFilter(filter);

		if (port)
			FltCloseCommunicationPort(port);

		if (client_port)
			FltCloseCommunicationPort(client_port);

		return status;

	}

	KdPrint((DRIVER_PREFIX "Initialized driver successfully!\n"));

	return status;

}

